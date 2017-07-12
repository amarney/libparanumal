#include "ellipticHex3D.h"


void ellipticOperator3D(solver_t *solver, dfloat lambda,
      occa::memory &o_q, occa::memory &o_Aq, const char *options){

  mesh_t *mesh = solver->mesh;

  occaTimerTic(mesh->device,"AxKernel");
  
  dfloat *sendBuffer = solver->sendBuffer;
  dfloat *recvBuffer = solver->recvBuffer;

  // compute local element operations and store result in o_Aq
  if(strstr(options, "CONTINUOUS")){
    //    mesh->AxKernel(mesh->Nelements, mesh->o_ggeo, mesh->o_D, lambda, o_q, o_Aq);
#if 0
    solver->AxKernel(mesh->Nelements, solver->o_gggeo, solver->o_gD, solver->o_gI, lambda, o_q, o_Aq);

    ellipticParallelGatherScatter(mesh, solver->ogs, o_Aq, o_Aq, dfloatString, "add");
#else
    
    // switch to data stream
    mesh->device.setStream(solver->dataStream);
    if(solver->NglobalGatherElements)
      solver->partialAxKernel(solver->NglobalGatherElements, solver->o_globalGatherElementList,
			      solver->o_gggeo, solver->o_gD, solver->o_gI, lambda, o_q, o_Aq);

    mesh->gatherKernel(solver->halo->Ngather, solver->halo->o_gatherOffsets, solver->halo->o_gatherLocalIds, o_v, solver->halo->o_gatherTmp);

    // copy partially gathered solver->halo data from DEVICE to HOST
    solver->halo->o_gatherTmp.asyncCopyTo(solver->halo->gatherTmp);

    mesh->device.setStream(solver->defaultStream);

    //ellipticHaloGatherScatterStart(solver, solver->halo, o_Aq, dfloatString, "add");    

    mesh->device.setStream(solver->defaultStream);
    if(solver->NnotGlobalGatherElements)
      solver->partialAxKernel(solver->NnotGlobalGatherElements, solver->o_notGlobalGatherElementList,
			      solver->o_gggeo, solver->o_gD, solver->o_gI, lambda, o_q, o_Aq);

    ellipticNonHaloGatherScatter(solver, solver->nonHalo, o_Aq, dfloatString, "add");

    ellipticHaloGatherScatterEnd(solver, solver->halo, o_Aq, dfloatString, "add");
    mesh->device.finish();
    
    //mesh->device.setStream(solver->defaultStream);

#endif

  }
  else{
    // should not be hard coded
    dfloat tau = 2.f*(mesh->Nq)*(mesh->Nq+2)/3.;

    iint offset = 0;
    
    ellipticStartHaloExchange3D(solver, o_q, sendBuffer, recvBuffer);

    solver->partialGradientKernel(mesh->Nelements, offset, mesh->o_vgeo, mesh->o_D, o_q, solver->o_grad);

    ellipticInterimHaloExchange3D(solver, o_q, sendBuffer, recvBuffer);

    if(mesh->NinternalElements)
      solver->partialIpdgKernel(mesh->NinternalElements,
				mesh->o_internalElementIds,
				mesh->o_vmapM,
				mesh->o_vmapP,
				lambda,
				tau,
				mesh->o_vgeo,
				mesh->o_sgeo,
				mesh->o_D,
				solver->o_grad,
				o_Aq);
    
    ellipticEndHaloExchange3D(solver, o_q, recvBuffer);

    if(mesh->totalHaloPairs){
      offset = mesh->Nelements;
      solver->partialGradientKernel(mesh->totalHaloPairs, offset, mesh->o_vgeo, mesh->o_D, o_q, solver->o_grad);
    }

#if 1
    if(mesh->NnotInternalElements)
      solver->partialIpdgKernel(mesh->NnotInternalElements,
				mesh->o_notInternalElementIds,
				mesh->o_vmapM,
				mesh->o_vmapP,
				lambda,
				tau,
				mesh->o_vgeo,
				mesh->o_sgeo,
				mesh->o_D,
				solver->o_grad,
				o_Aq);
#else
    solver->ipdgKernel(mesh->Nelements,
		       mesh->o_vmapM,
		       mesh->o_vmapP,
		       lambda,
		       tau,
		       mesh->o_vgeo,
		       mesh->o_sgeo,
		       mesh->o_D,
		       solver->o_grad,
		       o_Aq);
#endif
    
    
  }

  occaTimerToc(mesh->device,"AxKernel");
}


void ellipticMatrixFreeAx(void **args, occa::memory o_q, occa::memory o_Aq, const char* options) {

  solver_t *solver = (solver_t *) args[0];
  dfloat  *lambda  = (dfloat *)  args[1];

  mesh_t *mesh = solver->mesh;
  dfloat *sendBuffer = solver->sendBuffer;
  dfloat *recvBuffer = solver->recvBuffer;

  // compute local element operations and store result in o_Aq
  if(strstr(options, "CONTINUOUS")){
    //    mesh->AxKernel(mesh->Nelements, mesh->o_ggeo, mesh->o_D, lambda, o_q, o_Aq);
    solver->AxKernel(mesh->Nelements, solver->o_gggeo, solver->o_gD, solver->o_gI, lambda, o_q, o_Aq);
  }
  else{
    // tau should not be hard coded
    dfloat tau = 2.f*(mesh->Nq)*(mesh->Nq+2)/3.;
    
    iint offset = 0;
    
    ellipticStartHaloExchange3D(solver, o_q, sendBuffer, recvBuffer);

    solver->partialGradientKernel(mesh->Nelements, offset, mesh->o_vgeo, mesh->o_D, o_q, solver->o_grad);

    ellipticInterimHaloExchange3D(solver, o_q, sendBuffer, recvBuffer);

    if(mesh->NinternalElements)
      solver->partialIpdgKernel(mesh->NinternalElements,
				mesh->o_internalElementIds,
				mesh->o_vmapM,
				mesh->o_vmapP,
				lambda,
				tau,
				mesh->o_vgeo,
				mesh->o_sgeo,
				mesh->o_D,
				solver->o_grad,
				o_Aq);
   
    ellipticEndHaloExchange3D(solver, o_q, recvBuffer);

    if(mesh->totalHaloPairs){
      offset = mesh->Nelements;      
      solver->partialGradientKernel(mesh->totalHaloPairs, offset, mesh->o_vgeo, mesh->o_D, o_q, solver->o_grad);
    }

#if 1
    if(mesh->NnotInternalElements)
      solver->partialIpdgKernel(mesh->NnotInternalElements,
				mesh->o_notInternalElementIds,
				mesh->o_vmapM,
				mesh->o_vmapP,
				lambda,
				tau,
				mesh->o_vgeo,
				mesh->o_sgeo,
				mesh->o_D,
				solver->o_grad,
				o_Aq);
#else
    solver->ipdgKernel(mesh->Nelements,
		       mesh->o_vmapM,
		       mesh->o_vmapP,
		       lambda,
		       tau,
		       mesh->o_vgeo,
		       mesh->o_sgeo,
		       mesh->o_D,
		       solver->o_grad,
		       o_Aq);
#endif
  }
}


dfloat ellipticScaledAdd(solver_t *solver, dfloat alpha, occa::memory &o_a, dfloat beta, occa::memory &o_b){

  mesh_t *mesh = solver->mesh;

  iint Ntotal = mesh->Nelements*mesh->Np;

  occaTimerTic(mesh->device,"scaledAddKernel");
  
  // b[n] = alpha*a[n] + beta*b[n] n\in [0,Ntotal)
  mesh->scaledAddKernel(Ntotal, alpha, o_a, beta, o_b);

  occaTimerToc(mesh->device,"scaledAddKernel");
  
}

dfloat ellipticWeightedInnerProduct(solver_t *solver,
            occa::memory &o_w,
            occa::memory &o_a,
            occa::memory &o_b,
            const char *options){

  mesh_t *mesh = solver->mesh;
  dfloat *tmp = solver->tmp;
  iint Nblock = solver->Nblock;
  iint Ntotal = mesh->Nelements*mesh->Np;

  occa::memory &o_tmp = solver->o_tmp;

  occaTimerTic(mesh->device,"weighted inner product2");

  if(strstr(options,"CONTINUOUS")||strstr(options, "PROJECT"))
    mesh->weightedInnerProduct2Kernel(Ntotal, o_w, o_a, o_b, o_tmp);
  else
    mesh->innerProductKernel(Ntotal, o_a, o_b, o_tmp);

  occaTimerToc(mesh->device,"weighted inner product2");
  
  o_tmp.copyTo(tmp);

  dfloat wab = 0;
  for(iint n=0;n<Nblock;++n){
    wab += tmp[n];
  }
      
  dfloat globalwab = 0;
  MPI_Allreduce(&wab, &globalwab, 1, MPI_DFLOAT, MPI_SUM, MPI_COMM_WORLD);

  return globalwab;
}


void ellipticPreconditioner3D(solver_t *solver,
			      occa::memory &o_r,
			      occa::memory &o_zP,
			      occa::memory &o_z,
			      const char *options){

  mesh_t *mesh = solver->mesh;
  precon_t *precon = solver->precon;
  ogs_t    *ogs = solver->ogs; // C0 Gather ScatterTri info
  
  dfloat *sendBuffer = solver->sendBuffer;
  dfloat *recvBuffer = solver->recvBuffer;

  if(strstr(options, "JACOBI")){

    iint Ntotal = mesh->Np*mesh->Nelements;
    // Jacobi preconditioner
    occaTimerTic(mesh->device,"dotDivideKernel");   
    mesh->dotDivideKernel(Ntotal, o_r, precon->o_diagA, o_z);
    occaTimerToc(mesh->device,"dotDivideKernel");   
  }
  else // turn off preconditioner
    o_z.copyFrom(o_r);
  
}

int ellipticSolveHex3D(solver_t *solver, dfloat lambda, occa::memory &o_r, occa::memory &o_x, const iint maxIterations, const char *options){

  mesh_t *mesh = solver->mesh;
  
  iint rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  // convergence tolerance (currently absolute)
  const dfloat tol = 1e-6;

  occa::memory &o_p  = solver->o_p;
  occa::memory &o_z  = solver->o_z;
  occa::memory &o_zP = solver->o_zP;
  occa::memory &o_Ap = solver->o_Ap;
  occa::memory &o_Ax = solver->o_Ax;

  occa::streamTag startTag = mesh->device.tagStream();
  
  occaTimerTic(mesh->device,"PCG");

  // gather-scatter
  if(strstr(options,"CONTINUOUS")||strstr(options, "PROJECT"))
    ellipticParallelGatherScatter(mesh, solver->ogs, o_r, o_r, dfloatString, "add");

  // compute A*x
  ellipticOperator3D(solver, lambda, o_x, o_Ax, options);
  
  // subtract r = b - A*x
  ellipticScaledAdd(solver, -1.f, o_Ax, 1.f, o_r);
  
  occaTimerTic(mesh->device,"Preconditioner");
  if(strstr(options,"PCG")){
    // Precon^{-1} (b-A*x)
    ellipticPreconditioner3D(solver, o_r, o_zP, o_z, options); // r => rP => zP => z

    // p = z
    o_p.copyFrom(o_z); // PCG
  }
  else{
    // p = r
    o_p.copyFrom(o_r); // CG
  }
  occaTimerToc(mesh->device,"Preconditioner");

  // dot(r,r)
  dfloat rdotr0 = ellipticWeightedInnerProduct(solver, solver->o_invDegree, o_r, o_r, options);
  dfloat rdotz0 = ellipticWeightedInnerProduct(solver, solver->o_invDegree, o_r, o_z, options);
  dfloat rdotr1 = 0;
  dfloat rdotz1 = 0;
  iint Niter = 0;
  dfloat alpha, beta, pAp;
    
  //  if(rank==0)
  //    printf("rdotr0 = %g, rdotz0 = %g\n", rdotr0, rdotz0);

  while(rdotr0>(tol*tol) && Niter<maxIterations){
    // A*p
    ellipticOperator3D(solver, lambda, o_p, o_Ap, options); 

    // dot(p,A*p)
    pAp = ellipticWeightedInnerProduct(solver, solver->o_invDegree, o_p, o_Ap, options);

    if(strstr(options,"PCG"))
      // alpha = dot(r,z)/dot(p,A*p)
      alpha = rdotz0/pAp;
    else
      // alpha = dot(r,r)/dot(p,A*p)
      alpha = rdotr0/pAp;

    // x <= x + alpha*p
    ellipticScaledAdd(solver,  alpha, o_p,  1.f, o_x);

    // r <= r - alpha*A*p
    ellipticScaledAdd(solver, -alpha, o_Ap, 1.f, o_r);

    // dot(r,r)
    rdotr1 = ellipticWeightedInnerProduct(solver, solver->o_invDegree, o_r, o_r, options);
    
    if(rdotr1 < tol*tol) break;

    occaTimerTic(mesh->device,"Preconditioner");
    if(strstr(options,"PCG")){

      // z = Precon^{-1} r
      ellipticPreconditioner3D(solver, o_r, o_zP, o_z, options);
      
      // dot(r,z)
      rdotz1 = ellipticWeightedInnerProduct(solver, solver->o_invDegree, o_r, o_z, options);
      
      // flexible pcg beta = (z.(-alpha*Ap))/zdotz0
      if(strstr(options,"FLEXIBLE")){
        dfloat zdotAp = ellipticWeightedInnerProduct(solver, solver->o_invDegree, o_z, o_Ap, options);
        beta = -alpha*zdotAp/rdotz0;
      }
      else{
        beta = rdotz1/rdotz0;
      }

      // p = z + beta*p
      ellipticScaledAdd(solver, 1.f, o_z, beta, o_p);

      // switch rdotz0 <= rdotz1
      rdotz0 = rdotz1;
    }
    else{
      beta = rdotr1/rdotr0;

      // p = r + beta*p
      ellipticScaledAdd(solver, 1.f, o_r, beta, o_p);
    }
    occaTimerToc(mesh->device,"Preconditioner");

    // switch rdotr0 <= rdotr1
    rdotr0 = rdotr1;

    //    if(rank==0)
    //      printf("iter=%05d pAp = %g norm(r) = %g\n", Niter, pAp, sqrt(rdotr0));
    
    ++Niter;
  };

  occaTimerToc(mesh->device,"PCG");

  occa::streamTag stopTag = mesh->device.tagStream();

  double elapsed = mesh->device.timeBetween(startTag, stopTag);
  double gElapsed;
  MPI_Allreduce(&elapsed, &gElapsed, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
  
  //  if(rank==0)
  //    printf("elapsed = %g iter=%05d pAp = %g norm(r) = %g\n",
  //	   gElapsed, Niter, pAp, sqrt(rdotr0));

  occa::printTimer();

  //  printf("total number of nodes: %d\n", mesh->Np*mesh->Nelements);

  return Niter;
}
