#include "boltzmannQuad3D.h"

void boltzmannRunLSERKQuad3D(solver_t *solver){

  mesh_t *mesh = solver->mesh;
    
  // MPI send buffer
  iint haloBytes = mesh->totalHaloPairs*mesh->Np*mesh->Nfields*sizeof(dfloat);
  dfloat *sendBuffer = (dfloat*) malloc(haloBytes);
  dfloat *recvBuffer = (dfloat*) malloc(haloBytes);

  dfloat * test_q = (dfloat *) calloc(mesh->Nelements*mesh->Np*mesh->Nfields*mesh->Nrhs,sizeof(dfloat));
    
  //kernel arguments
  dfloat alpha = 1./mesh->N;
  
  mesh->filterKernelq0H(mesh->Nelements,
			alpha,
			mesh->o_dualProjMatrix,
			mesh->o_cubeFaceNumber,
			mesh->o_EToE,
			mesh->o_x,
			mesh->o_y,
			mesh->o_z,
			mesh->o_qpre,
			mesh->o_qFilter);
  
  mesh->filterKernelq0V(mesh->Nelements,
			alpha,
			mesh->o_dualProjMatrix,
			mesh->o_cubeFaceNumber,
			mesh->o_EToE,
			mesh->o_x,
			mesh->o_y,
			mesh->o_z,
			mesh->o_qFilter,
			mesh->o_qpre);
  
  for(iint tstep=0;tstep<mesh->Nrhs;++tstep){
    for (iint Ntick=0; Ntick < pow(2,mesh->MRABNlevels-1);Ntick++) {
      
      	iint lev;
	for (lev=0;lev<mesh->MRABNlevels;lev++)
	  if (Ntick % (1<<lev) !=0) break; //find the max lev to add to rhsq

	iint levS;
	for (levS=0;levS<mesh->MRABNlevels;levS++)
	  if ((Ntick+1) % (1<<levS) !=0) break; //find the max lev to add to rhsq
	
      for (iint rk = 0; rk < mesh->Nrk; ++rk) {
	
	//synthesize actual stage time
	dfloat t = tstep*pow(2,mesh->MRABNlevels-1) + Ntick;
      
	if(mesh->totalHaloPairs>0){
	  // extract halo on DEVICE
	  iint Nentries = mesh->Np*mesh->Nfields;
	  
	  mesh->haloExtractKernel(mesh->totalHaloPairs,
				  Nentries,
				  mesh->o_haloElementList,
				  mesh->o_qpre,
				  mesh->o_haloBuffer);

	  // copy extracted halo to HOST 
	  mesh->o_haloBuffer.copyTo(sendBuffer);      
	
	  // start halo exchange
	  meshHaloExchangeStart(mesh,
				mesh->Np*mesh->Nfields*sizeof(dfloat),
				sendBuffer,
				recvBuffer);
	}
	// compute volume contribution to DG boltzmann RHS
	mesh->volumePreKernel(mesh->Nelements,
			      mesh->o_vgeo,
			      mesh->o_D,
			      mesh->o_x,
			      mesh->o_y,
			      mesh->o_z,
			      mesh->o_qpre,
			      mesh->o_qlserk);

	if(mesh->totalHaloPairs>0){
	  // wait for halo data to arrive
	  meshHaloExchangeFinish(mesh);
	
	  // copy halo data to DEVICE
	  size_t offset = mesh->Np*mesh->Nfields*mesh->Nelements*sizeof(dfloat); // offset for halo data
	  mesh->o_q.copyFrom(recvBuffer, haloBytes, offset);
	  mesh->device.finish();
	}
	
	mesh->surfacePreKernel(mesh->Nelements,
			       mesh->o_sgeo,
			       mesh->o_LIFTT,
			       mesh->o_vmapM,
			       mesh->o_vmapP,
			       t,
			       mesh->o_x,
			       mesh->o_y,
			       mesh->o_z,
			       mesh->o_qpre,
			       mesh->o_qlserk);

	mesh->filterKernelq0H(mesh->Nelements,
			      alpha,
			      mesh->o_dualProjMatrix,
			      mesh->o_cubeFaceNumber,
			      mesh->o_EToE,
			      mesh->o_x,
			      mesh->o_y,
			      mesh->o_z,
			      mesh->o_qlserk,
			      mesh->o_qFilter);
	
	mesh->filterKernelq0V(mesh->Nelements,
			      alpha,
			      mesh->o_dualProjMatrix,
			      mesh->o_cubeFaceNumber,
			      mesh->o_EToE,
			      mesh->o_x,
			      mesh->o_y,
			      mesh->o_z,
			      mesh->o_qFilter,
			      mesh->o_qlserk);
	
	for (iint l = 0; l < mesh->MRABNlevels; l++) {
	  iint saved = (l < lev)&&(rk == 0);
	  mesh->volumeCorrPreKernel(mesh->MRABNelements[l],
				    mesh->o_MRABelementIds[l],
				    saved,
				    mesh->MRABshiftIndex[l],
				    mesh->o_qpre,
				    mesh->o_qCorr,
				    mesh->o_qPreCorr);
				    }
	for (iint l = 0; l < mesh->MRABNlevels; l++) {
	  iint saved = (l < lev)&&(rk == 0);
	  
	  if (mesh->MRABNelements[l]) {
	    mesh->updatePreKernel(mesh->MRABNelements[l],
				  mesh->o_MRABelementIds[l],
				  saved,
				  mesh->dt,
				  mesh->rka[rk],
				  mesh->rkb[rk],
				  mesh->MRABshiftIndex[l],
				  mesh->o_qlserk,
				  mesh->o_rhsq,
				  mesh->o_qPreCorr,
				  mesh->o_resq,
				  mesh->o_qpre);
	    
	    saved = (l < levS)&&(rk == mesh->Nrk-1);
	    if (saved)
	      mesh->MRABshiftIndex[l] = (mesh->MRABshiftIndex[l]+2)%mesh->Nrhs;
	  }
	}
      }
      //runs after all rk iterations complete
      for (iint l = 0; l < levS; ++l) {
	  mesh->traceUpdatePreKernel(mesh->MRABNelements[l],
				     mesh->o_MRABelementIds[l],
				     mesh->o_vmapM,
				     mesh->o_fQM,
				     mesh->o_qpre,
				     mesh->o_q);
      }
    }

    // estimate maximum error
    /*    if(((tstep+1)%mesh->errorStep)==0){
      //	dfloat t = (tstep+1)*mesh->dt;
      dfloat t = mesh->dt*((tstep+1)*pow(2,mesh->MRABNlevels-1));
	
      printf("tstep = %d, t = %g\n", tstep, t);
      fflush(stdout);
      // copy data back to host
      mesh->o_q.copyTo(mesh->q);

      // check for nans
      for(int n=0;n<mesh->Nfields*mesh->Nelements*mesh->Np;++n){
	if(isnan(mesh->q[n])){
	  printf("found nan\n");
	  exit(-1);
	}
      }

      boltzmannPlotNorms(mesh,"norms",tstep/mesh->errorStep,mesh->q);
      }*/
    
    //    mesh->o_q.copyTo(mesh->q);
    //boltzmannPlotNorms(mesh,"start",tstep,mesh->q);
  }
  
  free(recvBuffer);
  free(sendBuffer);
}