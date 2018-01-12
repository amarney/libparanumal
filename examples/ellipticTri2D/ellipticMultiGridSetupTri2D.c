#include "ellipticTri2D.h"

void ellipticOperator2D(solver_t *solver, dfloat lambda, occa::memory &o_q, occa::memory &o_Aq, const char *options);
dfloat ellipticScaledAdd(solver_t *solver, dfloat alpha, occa::memory &o_a, dfloat beta, occa::memory &o_b);

void AxTri2D(void **args, occa::memory &o_x, occa::memory &o_Ax) {

  solver_t *solver = (solver_t *) args[0];
  dfloat *lambda = (dfloat *) args[1];
  char *options = (char *) args[2];

  ellipticOperator2D(solver,*lambda,o_x,o_Ax,options);
}

void coarsenTri2D(void **args, occa::memory &o_x, occa::memory &o_Rx) {

  solver_t *solver = (solver_t *) args[0];
  occa::memory *o_V = (occa::memory *) args[1];

  mesh_t *mesh = solver->mesh;
  precon_t *precon = solver->precon;

  precon->coarsenKernel(mesh->Nelements, *o_V, o_x, o_Rx);
}

void prolongateTri2D(void **args, occa::memory &o_x, occa::memory &o_Px) {

  solver_t *solver = (solver_t *) args[0];
  occa::memory *o_V = (occa::memory *) args[1];

  mesh_t *mesh = solver->mesh;
  precon_t *precon = solver->precon;

  precon->prolongateKernel(mesh->Nelements, *o_V, o_x, o_Px);
}

void ellipticGather(void **args, occa::memory &o_x, occa::memory &o_Gx) {

  solver_t *solver = (solver_t *) args[0];
  hgs_t *hgs       = (hgs_t *) args[1];
  char *options    = (char *) args[2];
  mesh_t *mesh     = solver->mesh;

  if (strstr(options,"SPARSE")) solver->dotMultiplyKernel(mesh->Np*mesh->Nelements, o_x, mesh->o_mapSgn, o_x);
  meshParallelGather(mesh, hgs, o_x, o_Gx);
  solver->dotMultiplyKernel(hgs->Ngather, hgs->o_invDegree, o_Gx, o_Gx);
}

void ellipticScatter(void **args, occa::memory &o_x, occa::memory &o_Sx) {

  solver_t *solver = (solver_t *) args[0];
  hgs_t *hgs       = (hgs_t *) args[1];
  char *options    = (char *) args[2];
  mesh_t *mesh     = solver->mesh;

  meshParallelScatter(mesh, hgs, o_x, o_Sx);
  if (strstr(options,"SPARSE")) solver->dotMultiplyKernel(mesh->Np*mesh->Nelements, o_Sx, mesh->o_mapSgn, o_Sx);
}

dfloat *buildCoarsenerTri2D(mesh2D** meshLevels, int N, int Nc, const char* options) {

  //TODO We can build the coarsen matrix either from the interRaise or interpLower matrices. Need to check which is better

  //use the Raise for now (essentally an L2 projection)

  int NpFine   = meshLevels[N]->Np;
  int NpCoarse = meshLevels[Nc]->Np;

  dfloat *P    = (dfloat *) calloc(NpFine*NpCoarse,sizeof(dfloat));
  dfloat *Ptmp = (dfloat *) calloc(NpFine*NpCoarse,sizeof(dfloat));

  //initialize P as identity
  for (int i=0;i<NpCoarse;i++) P[i*NpCoarse+i] = 1.0;

  for (int n=Nc;n<N;n++) {

    int Npp1 = meshLevels[n+1]->Np;
    int Np   = meshLevels[n]->Np;

    //copy P
    for (int i=0;i<Np*NpCoarse;i++) Ptmp[i] = P[i];

    //Multiply by the raise op
    for (int i=0;i<Npp1;i++) {
      for (int j=0;j<NpCoarse;j++) {
        P[i*NpCoarse + j] = 0.;
        for (int k=0;k<Np;k++) {
          P[i*NpCoarse + j] += meshLevels[n]->interpRaise[i*Np+k]*Ptmp[k*NpCoarse + j];
        }
      }
    }
  }

  if (strstr(options,"BERN")) {
    dfloat* BBP = (dfloat *) calloc(NpFine*NpCoarse,sizeof(dfloat));
    for (iint j=0;j<NpFine;j++) {
      for (iint i=0;i<NpCoarse;i++) {
        for (iint k=0;k<NpCoarse;k++) {
          for (iint l=0;l<NpFine;l++) {
            BBP[i+j*NpCoarse] += meshLevels[N]->invVB[l+j*NpFine]*P[k+l*NpCoarse]*meshLevels[Nc]->VB[i+k*NpCoarse];
          }
        }
      }
    }
    for (iint j=0;j<NpFine;j++) {
      for (iint i=0;i<NpCoarse;i++) {
        P[i+j*NpCoarse] = BBP[i+j*NpCoarse];
      }
    }
    free(BBP);
  }

  //the coarsen matrix is P^T
  dfloat *R    = (dfloat *) calloc(NpFine*NpCoarse,sizeof(dfloat));
  for (int i=0;i<NpCoarse;i++) {
    for (int j=0;j<NpFine;j++) {
      R[i*NpFine+j] = P[j*NpCoarse+i];
    }
  }

  free(P); free(Ptmp);

  return R;
}

void ellipticMultiGridSetupTri2D(solver_t *solver, precon_t* precon,
                                dfloat tau, dfloat lambda, iint *BCType,
                                const char *options, const char *parAlmondOptions) {

  iint rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  mesh2D *mesh = solver->mesh;

  //read all the nodes files and load them in a dummy mesh array
  mesh2D **meshLevels = (mesh2D**) calloc(mesh->N+1,sizeof(mesh2D*));
  for (int n=1;n<mesh->N+1;n++) {
    meshLevels[n] = (mesh2D *) calloc(1,sizeof(mesh2D));
    meshLevels[n]->Nverts = mesh->Nverts;
    meshLevels[n]->Nfaces = mesh->Nfaces;
    meshLoadReferenceNodesTri2D(meshLevels[n], n);
  }

  //set the number of MG levels and their degree
  int numLevels;
  int *levelDegree;

  if (strstr(options,"ALLDEGREES")) {
    numLevels = mesh->N;
    levelDegree= (int *) calloc(numLevels,sizeof(int));
    for (int n=0;n<numLevels;n++) levelDegree[n] = mesh->N - n; //all degrees
  } else if (strstr(options,"HALFDEGREES")) {
    numLevels = floor(mesh->N/2.)+1;
    levelDegree= (int *) calloc(numLevels,sizeof(int));
    for (int n=0;n<numLevels;n++) levelDegree[n] = mesh->N - 2*n; //decrease by two
    levelDegree[numLevels-1] = 1; //ensure the last level is degree 1
  } else { //default "HALFDOFS"
    // pick the degrees so the dofs of each level halfs (roughly)
    //start by counting the number of levels neccessary
    numLevels = 1;
    int degree = mesh->N;
    int dofs = meshLevels[degree]->Np;
    while (dofs>3) {
      numLevels++;
      for (;degree>0;degree--)
        if (meshLevels[degree]->Np<=dofs/2)
          break;
      dofs = meshLevels[degree]->Np;
    }
    levelDegree= (int *) calloc(numLevels,sizeof(int));
    degree = mesh->N;
    numLevels = 1;
    levelDegree[0] = degree;
    dofs = meshLevels[degree]->Np;
    while (dofs>3) {
      for (;degree>0;degree--)
        if (meshLevels[degree]->Np<=dofs/2)
          break;
      dofs = meshLevels[degree]->Np;
      levelDegree[numLevels] = degree;
      numLevels++;
    }
  }

  //storage for lambda parameter
  dfloat *vlambda = (dfloat *) calloc(1,sizeof(dfloat));
  *vlambda = lambda;

  //storage for restriction matrices
  dfloat **R = (dfloat **) calloc(numLevels,sizeof(dfloat*));
  occa::memory *o_R = (occa::memory *) calloc(numLevels,sizeof(occa::memory));

  //maually build multigrid levels
  precon->parAlmond = parAlmondInit(mesh, parAlmondOptions);
  agmgLevel **levels = precon->parAlmond->levels;

  for (int n=0;n<numLevels;n++) {
    solver_t *solverL;

    hgs_t *coarsehgs;

    if (n==0) {
      solverL = solver;
    } else {
      //build ops for this level
      printf("=============BUIDLING MULTIGRID LEVEL OF DEGREE %d==================\n", levelDegree[n]);
      solverL = ellipticBuildMultigridLevelTri2D(solver,levelDegree,n,BCType,options);

      //set the normalization constant for the allNeumann Poisson problem on this coarse mesh
      iint totalElements = 0;
      MPI_Allreduce(&(mesh->Nelements), &totalElements, 1, MPI_IINT, MPI_SUM, MPI_COMM_WORLD);
      solverL->allNeumannScale = 1.0/sqrt(solverL->mesh->Np*totalElements);
    }

    //check if we're at the degree 1 problem
    if (n==numLevels-1) {
      // build degree 1 matrix problem
      nonZero_t *coarseA;
      iint nnzCoarseA;

      int basisNp = solverL->mesh->Np;
      dfloat *basis = NULL;

      if (strstr(options,"BERN")) basis = solverL->mesh->VB;

      iint *coarseGlobalStarts = (iint*) calloc(size+1, sizeof(iint));

      if (strstr(options,"IPDG")) {
        ellipticBuildIpdgTri2D(solverL->mesh, basisNp, basis, tau, lambda, BCType, &coarseA, &nnzCoarseA,coarseGlobalStarts, options);
      } else if (strstr(options,"BRDG")) {
        ellipticBuildBRdgTri2D(solverL->mesh, basisNp, basis, tau, lambda, BCType, &coarseA, &nnzCoarseA,coarseGlobalStarts, options);
      } else if (strstr(options,"CONTINUOUS")) {
        ellipticBuildContinuousTri2D(solverL->mesh,lambda,&coarseA,&nnzCoarseA,&coarsehgs,coarseGlobalStarts, options);
      }

      // // Build coarse grid element basis functions
      // dfloat *V1  = (dfloat*) calloc(mesh->Np*mesh->Nverts, sizeof(dfloat));
      // for(iint n=0;n<mesh->Np;++n){
      //   dfloat rn = mesh->r[n];
      //   dfloat sn = mesh->s[n];

      //   V1[0*mesh->Np+n] = -0.5*(rn+sn);
      //   V1[1*mesh->Np+n] = +0.5*(1.+rn);
      //   V1[2*mesh->Np+n] = +0.5*(1.+sn);
      // }
      // precon->o_V1  = mesh->device.malloc(mesh->Nverts*mesh->Np*sizeof(dfloat), V1);

      iint *Rows = (iint *) calloc(nnzCoarseA, sizeof(iint));
      iint *Cols = (iint *) calloc(nnzCoarseA, sizeof(iint));
      dfloat *Vals = (dfloat*) calloc(nnzCoarseA,sizeof(dfloat));

      for (iint i=0;i<nnzCoarseA;i++) {
        Rows[i] = coarseA[i].row;
        Cols[i] = coarseA[i].col;
        Vals[i] = coarseA[i].val;
      }

      // build amg starting at level 1
      parAlmondAgmgSetup(precon->parAlmond,
                         coarseGlobalStarts,
                         nnzCoarseA,
                         Rows,
                         Cols,
                         Vals,
                         solver->allNeumann,
                         solver->allNeumannPenalty);

      free(coarseA); free(Rows); free(Cols); free(Vals);

      //tell parAlmond to gather this level
      if (strstr(options,"CONTINUOUS")) {
        precon->parAlmond->levels[n]->gatherLevel = true;
        precon->parAlmond->levels[n]->Srhs = (dfloat*) calloc(solverL->mesh->Np*solverL->mesh->Nelements,sizeof(dfloat));
        precon->parAlmond->levels[n]->Sx   = (dfloat*) calloc(solverL->mesh->Np*solverL->mesh->Nelements,sizeof(dfloat));
        precon->parAlmond->levels[n]->o_Srhs = mesh->device.malloc(solverL->mesh->Np*solverL->mesh->Nelements*sizeof(dfloat),precon->parAlmond->levels[n]->Srhs);
        precon->parAlmond->levels[n]->o_Sx   = mesh->device.malloc(solverL->mesh->Np*solverL->mesh->Nelements*sizeof(dfloat),precon->parAlmond->levels[n]->Sx);

        precon->parAlmond->levels[n]->gatherArgs = (void **) calloc(3,sizeof(void*));  
        precon->parAlmond->levels[n]->gatherArgs[0] = (void *) solverL;
        precon->parAlmond->levels[n]->gatherArgs[1] = (void *) coarsehgs;
        precon->parAlmond->levels[n]->gatherArgs[2] = (void *) options;
        precon->parAlmond->levels[n]->scatterArgs = precon->parAlmond->levels[n]->gatherArgs;

        precon->parAlmond->levels[n]->device_gather  = ellipticGather;
        precon->parAlmond->levels[n]->device_scatter = ellipticScatter;        
      }

    } else {
      //build the level manually
      precon->parAlmond->numLevels++;
      levels[n] = (agmgLevel *) calloc(1,sizeof(agmgLevel));
      levels[n]->gatherLevel = false;

      //use the matrix free Ax
      levels[n]->AxArgs = (void **) calloc(3,sizeof(void*));
      levels[n]->AxArgs[0] = (void *) solverL;
      levels[n]->AxArgs[1] = (void *) vlambda;
      levels[n]->AxArgs[2] = (void *) options;
      levels[n]->device_Ax = AxTri2D;

      levels[n]->smoothArgs = (void **) calloc(2,sizeof(void*));
      levels[n]->smoothArgs[0] = (void *) solverL;
      levels[n]->smoothArgs[1] = (void *) levels[n];

      levels[n]->Nrows = mesh->Nelements*solverL->mesh->Np;
      levels[n]->Ncols = (mesh->Nelements+mesh->totalHaloPairs)*solverL->mesh->Np;

      if (strstr(options,"CHEBYSHEV")) {
        levels[n]->device_smooth = smoothChebyshevTri2D;

        levels[n]->smootherResidual = (dfloat *) calloc(levels[n]->Ncols,sizeof(dfloat));

        // extra storage for smoothing op
        levels[n]->o_smootherResidual = mesh->device.malloc(levels[n]->Ncols*sizeof(dfloat),levels[n]->smootherResidual);
        levels[n]->o_smootherResidual2 = mesh->device.malloc(levels[n]->Ncols*sizeof(dfloat),levels[n]->smootherResidual);
        levels[n]->o_smootherUpdate = mesh->device.malloc(levels[n]->Ncols*sizeof(dfloat),levels[n]->smootherResidual);
      } else {
        levels[n]->device_smooth = smoothTri2D;

        // extra storage for smoothing op
        levels[n]->o_smootherResidual = mesh->device.malloc(levels[n]->Ncols*sizeof(dfloat));
      }

      levels[n]->smootherArgs = (void **) calloc(2,sizeof(void*));
      levels[n]->smootherArgs[0] = (void *) solverL;
      levels[n]->smootherArgs[1] = (void *) options;

      dfloat rateTolerance;    // 0 - accept not approximate patches, 1 - accept all approximate patches
      if(strstr(options, "EXACT")){
        rateTolerance = 0.0;
      } else {
        rateTolerance = 1.0;
      }


      //set up the fine problem smoothing
      if(strstr(options, "OVERLAPPINGPATCH")){
        ellipticSetupSmootherOverlappingPatch(solverL, solverL->precon, levels[n], tau, lambda, BCType, options);
      } else if(strstr(options, "FULLPATCH")){
        ellipticSetupSmootherFullPatch(solverL, solverL->precon, levels[n], tau, lambda, BCType, rateTolerance, options);
      } else if(strstr(options, "FACEPATCH")){
        ellipticSetupSmootherFacePatch(solverL, solverL->precon, levels[n], tau, lambda, BCType, rateTolerance, options);
      } else if(strstr(options, "LOCALPATCH")){
        ellipticSetupSmootherLocalPatch(solverL, solverL->precon, levels[n], tau, lambda, BCType, rateTolerance, options);
      } else { //default to damped jacobi
        ellipticSetupSmootherDampedJacobi(solverL, solverL->precon, levels[n], tau, lambda, BCType, options);
      }
    }

    //reallocate vectors at N=1 since we're using the matrix free ops
    if (n==numLevels-1) {
      if (n>0) {//kcycle vectors
        if (levels[n]->Ncols) levels[n]->ckp1 = (dfloat *) realloc(levels[n]->ckp1,levels[n]->Ncols*sizeof(dfloat));
        if (levels[n]->Nrows) levels[n]->vkp1 = (dfloat *) realloc(levels[n]->vkp1,levels[n]->Nrows*sizeof(dfloat));
        if (levels[n]->Nrows) levels[n]->wkp1 = (dfloat *) realloc(levels[n]->wkp1,levels[n]->Nrows*sizeof(dfloat));

        if (levels[n]->Ncols) levels[n]->o_ckp1.free();
        if (levels[n]->Nrows) levels[n]->o_vkp1.free();
        if (levels[n]->Nrows) levels[n]->o_wkp1.free();
        if (levels[n]->Ncols) levels[n]->o_ckp1 = mesh->device.malloc(levels[n]->Ncols*sizeof(dfloat),levels[n]->ckp1);
        if (levels[n]->Nrows) levels[n]->o_vkp1 = mesh->device.malloc(levels[n]->Nrows*sizeof(dfloat),levels[n]->vkp1);
        if (levels[n]->Nrows) levels[n]->o_wkp1 = mesh->device.malloc(levels[n]->Nrows*sizeof(dfloat),levels[n]->wkp1);
      }

      if (levels[n]->Ncols) levels[n]->x    = (dfloat *) realloc(levels[n]->x  ,levels[n]->Ncols*sizeof(dfloat));
      if (levels[n]->Ncols) levels[n]->res  = (dfloat *) realloc(levels[n]->res,levels[n]->Ncols*sizeof(dfloat));
      if (levels[n]->Nrows) levels[n]->rhs  = (dfloat *) realloc(levels[n]->rhs,levels[n]->Nrows*sizeof(dfloat));

      if (levels[n]->Ncols) levels[n]->o_x.free();
      if (levels[n]->Ncols) levels[n]->o_res.free();
      if (levels[n]->Nrows) levels[n]->o_rhs.free();
      if (levels[n]->Ncols) levels[n]->o_x   = mesh->device.malloc(levels[n]->Ncols*sizeof(dfloat),levels[n]->x);
      if (levels[n]->Ncols) levels[n]->o_res = mesh->device.malloc(levels[n]->Ncols*sizeof(dfloat),levels[n]->res);
      if (levels[n]->Nrows) levels[n]->o_rhs = mesh->device.malloc(levels[n]->Nrows*sizeof(dfloat),levels[n]->rhs);
    }

    if (n==0) continue; //dont need restriction and prolongation ops at top level

    //build coarsen and prologation ops
    int N = levelDegree[n-1];
    int Nc = levelDegree[n];

    R[n] = buildCoarsenerTri2D(meshLevels, N, Nc, options);
    o_R[n] = mesh->device.malloc(meshLevels[N]->Np*meshLevels[Nc]->Np*sizeof(dfloat), R[n]);

    levels[n]->coarsenArgs = (void **) calloc(4,sizeof(void*));
    levels[n]->coarsenArgs[0] = (void *) solverL;
    levels[n]->coarsenArgs[1] = (void *) &(o_R[n]);
    levels[n]->prolongateArgs = levels[n]->coarsenArgs;
    
    levels[n]->device_coarsen = coarsenTri2D;
    levels[n]->device_prolongate = prolongateTri2D;
  }

  //report the multigrid levels
  if (strstr(options,"VERBOSE")) {
    if(rank==0) {
      printf("------------------Multigrid Report---------------------\n");
      printf("-------------------------------------------------------\n");
      printf("level|  Degree  |    dimension   |      Smoother       \n");
      printf("     |  Degree  |  (min,max,avg) |      Smoother       \n");
      printf("-------------------------------------------------------\n");
    }

    for(int lev=0; lev<numLevels; lev++){

      iint Nrows = levels[lev]->Nrows;

      iint minNrows=0, maxNrows=0, totalNrows=0;
      dfloat avgNrows;
      MPI_Allreduce(&Nrows, &maxNrows, 1, MPI_IINT, MPI_MAX, MPI_COMM_WORLD);
      MPI_Allreduce(&Nrows, &totalNrows, 1, MPI_IINT, MPI_SUM, MPI_COMM_WORLD);
      avgNrows = (dfloat) totalNrows/size;

      if (Nrows==0) Nrows=maxNrows; //set this so it's ignored for the global min
      MPI_Allreduce(&Nrows, &minNrows, 1, MPI_IINT, MPI_MIN, MPI_COMM_WORLD);

      char *smootherString;
      if(strstr(options, "OVERLAPPINGPATCH")){
        smootherString = strdup("OVERLAPPINGPATCH");
      } else if(strstr(options, "FULLPATCH")){
        smootherString = strdup("FULLPATCH");
      } else if(strstr(options, "FACEPATCH")){
        smootherString = strdup("FACEPATCH");
      } else if(strstr(options, "LOCALPATCH")){
        smootherString = strdup("LOCALPATCH");
      } else { //default to damped jacobi
        smootherString = strdup("DAMPEDJACOBI");
      }

      char *smootherOptions1 = strdup(" ");
      char *smootherOptions2 = strdup(" ");
      if (strstr(options,"EXACT")) {
        smootherOptions1 = strdup("EXACT");
      }
      if (strstr(options,"CHEBYSHEV")) {
        smootherOptions2 = strdup("CHEBYSHEV");
      }

      if (rank==0){
        printf(" %3d |   %3d    |    %10.2f  |   %s  \n",
          lev, levelDegree[lev], (dfloat)minNrows, smootherString);
        printf("     |          |    %10.2f  |   %s %s  \n", (dfloat)maxNrows, smootherOptions1, smootherOptions2);
        printf("     |          |    %10.2f  |   \n", avgNrows);
      }
    }
    if(rank==0)
      printf("-------------------------------------------------------\n");
  }

  for (int n=1;n<mesh->N+1;n++) free(meshLevels[n]);
  free(meshLevels);
}