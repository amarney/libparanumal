
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <map>
#include <vector>
#include <occa.hpp>
#include <almondHeaders.hpp>
#include "mpi.h"

#pragma message("WARNING : HARD CODED TO FLOAT/INT\n")

#define dfloat double
#define iint int

typedef struct {
  almond::csr<dfloat> *A;
  almond::agmg<dfloat> M;
  std::vector<dfloat>  rhs;
  std::vector<dfloat>  x;
  std::vector<dfloat>  nullA;

  uint numLocalRows;
  uint Nnum;
  uint nnz;

  int *globalSortId, *compressId;

  double *xUnassembled;
  double *rhsUnassembled;

  double *xSort;
  double *rhsSort;

  char* iintType;
  char* dfloatType;

} almond_t;

void * almondSetup(occa::device device,
       uint  Nnum,
       uint  numLocalRows, 
		   void* rowIds,
		   uint  nnz, 
		   void* Ai,
		   void* Aj,
		   void* Avals,
       int  *globalSortId, 
       int  *compressId,
		   int   nullSpace,
		   const char* iintType, 
		   const char* dfloatType) {

  int n;
  almond_t *almond = (almond_t*) calloc(1, sizeof(almond_t));

  std::vector<int>    vAj(nnz);
  std::vector<dfloat> vAvals(nnz);
  std::vector<int>    vRowStarts(numLocalRows+1);

  int *iAi = (int*) Ai;
  int *iAj = (int*) Aj;
  dfloat *dAvals = (dfloat*) Avals;

  // assumes presorted
  int cnt = 0;
  for(n=0;n<nnz;++n){
    if(iAi[n]>=numLocalRows || iAj[n]>=numLocalRows)
      printf("errant nonzero %d,%d,%g\n", iAi[n], iAj[n], dAvals[n]);
    if(n==0 || (iAi[n]!=iAi[n-1])){
      //      printf("*\n");
      vRowStarts[cnt] = n;
      ++cnt;
    }else{
      //      printf("\n");
    }
    vAj[n] = iAj[n];
    vAvals[n] = dAvals[n];
  }
  vRowStarts[cnt] = n;
  printf("cnt=%d, numLocalRows=%d\n", cnt, numLocalRows);

  almond->Nnum = Nnum;
  almond->numLocalRows = numLocalRows;

  almond->globalSortId = (int*) calloc(Nnum,sizeof(int));
  almond->compressId  = (int*) calloc(numLocalRows+1,sizeof(int));

  for (n=0;n<Nnum;n++) almond->globalSortId[n] = globalSortId[n];
  for (n=0;n<numLocalRows+1;n++) almond->compressId[n] = compressId[n];
  
  almond->xUnassembled = (dfloat*) calloc(Nnum,sizeof(dfloat));
  almond->rhsUnassembled = (dfloat*) calloc(Nnum,sizeof(dfloat));
  almond->xSort = (dfloat*) calloc(Nnum,sizeof(dfloat));
  almond->rhsSort = (dfloat*) calloc(Nnum,sizeof(dfloat));

  almond->rhs.resize(numLocalRows);
  almond->x.resize(numLocalRows);
  
  almond->A = new almond::csr<dfloat>(vRowStarts, vAj, vAvals);

  almond->nullA.resize(numLocalRows);
  for (int i=0;i<numLocalRows;i++)almond->nullA[i] = 1;
  
  almond->M.setup(*(almond->A), almond->nullA);
  almond->M.report();
  almond->M.ktype = almond::PCG;
  
  almond->iintType = strdup(iintType);
  almond->dfloatType = strdup(dfloatType);

  return (void *) almond;
}

int almondSolve(void* x,
		void* A,
		void* rhs) {
  
  almond_t *almond = (almond_t*) A;

  almond->rhsUnassembled = (dfloat*) rhs;
  dfloat *dx = (dfloat*) x;

  //sort by globalid 
  for (iint n=0;n<almond->Nnum;n++) 
    almond->rhsSort[n] = almond->rhsUnassembled[almond->globalSortId[n]];

  //gather
  for (iint n=0;n<almond->numLocalRows;++n){
    almond->rhs[n] = 0.;
    almond->x[n] = 0.;
    for (iint id=almond->compressId[n];id<almond->compressId[n+1];id++) 
      almond->rhs[n] += almond->rhsSort[id];
  }

  if(1){
    almond->M.solve(almond->rhs, almond->x);
  }
  else{
    int maxIt = 40;
    dfloat tol = 1e-1;
    almond::pcg<dfloat>(almond->A[0],
			almond->rhs,
			almond->x,
			almond->M,
			maxIt,
			tol);
  }

  //scatter
  for (iint n=0;n<almond->numLocalRows;++n){
    for (iint id = almond->compressId[n];id<almond->compressId[n+1];id++) 
      almond->xSort[id] = almond->x[n]; 
  }

  //sort by to original numbering
  for (iint n=0;n<almond->Nnum;n++) 
    almond->xUnassembled[almond->globalSortId[n]] = almond->xSort[n];

  for(iint i=0;i<almond->Nnum;++i) dx[i] = almond->xUnassembled[i];
  
  return 0;
}

int almondFree(void* A) {
#if 0
  almond_t *almond = (almond_t *) A;

  crs_free(almond->A);

  if (!strcmp(almond->dfloatType,"float")) { 
    free(almond->Avals);
    free(almond->x);  
    free(almond->rhs);
  }

  if (!strcmp(almond->iintType,"int")) { 
    free(almond->rowIds);
  }
#endif
  return 0;
}