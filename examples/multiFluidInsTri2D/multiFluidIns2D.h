#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mpi.h"
#include "mesh2D.h"
#include "ellipticTri2D.h"

#define UXID 0
#define UYID 1
#define PRID 2

typedef struct {

  mesh_t *mesh;
  solver_t *vSolver;
  solver_t *pSolver;

  char *pSolverOptions, *vSolverOptions; 	
  precon_t *precon;

  // INS SOLVER OCCA VARIABLES
  dfloat rho, nu;
  int NVfields, NTfields, Nfields;
  int NtotalDofs, NDofs, NtotalElements; // Total DOFs for Velocity i.e. Nelements + Nelements_halo
  int ExplicitOrder; 

  dfloat dt;          // time step
  dfloat lambda;      // helmhotz solver -lap(u) + lamda u
  dfloat finalTime;   // final time to run acoustics to
  int   NtimeSteps;  // number of time steps 
  int   Nstages;     // Number of history states to store
  int   index;       // Index of current state
  int   errorStep; 
  
  int NiterU, NiterV, NiterP;

//solver tolerances
  dfloat presTOL, velTOL;

  dfloat a0, a1, a2, b0, b1, b2, c0, c1, c2, g0, tau; 
  dfloat idt, ig0, inu; // hold some inverses
  
  dfloat *U, *V, *P, *Phi, *NU, *NV;   
  dfloat *rhsU, *rhsV, *rhsP, *rhsPhi;   
  dfloat *PI,*Px,*Py;
  


  // dfloat *Ut, *Vt, *Pt, *WN; // For stiffly stable scheme
  // dfloat dtfactor;

  

  dfloat g[2];      // gravitational Acceleration

  int Nsubsteps;  
  dfloat *Ud, *Vd, *Ue, *Ve, *resU, *resV, sdt;
  occa::memory o_Ud, o_Vd, o_Ue, o_Ve, o_resU, o_resV;

  // occa::kernel subCycleVolumeKernel,  subCycleCubatureVolumeKernel ;
  // occa::kernel subCycleSurfaceKernel, subCycleCubatureSurfaceKernel;;
  // occa::kernel subCycleRKUpdateKernel;
  // occa::kernel subCycleExtKernel;

 
  occa::memory o_U, o_V, o_P, o_Phi, o_NU, o_NV;
  occa::memory o_rhsU, o_rhsV, o_rhsP, o_rhsPhi, o_resPhi; 

  occa::memory o_Px, o_Py;

  occa::memory o_UH, o_VH;
  occa::memory o_PI, o_PIx, o_PIy;

  occa::memory o_Ut, o_Vt, o_Pt, o_WN; 


  // multiple RHS pressure projection variables
  int maxPresHistory, NpresHistory;
  int Nblock;

  dfloat *presAlpha, *presLocalAlpha; 
  occa::memory o_presAlpha;

  dfloat *presHistory;
  occa::memory o_PIbar, o_APIbar, o_presHistory;

  dfloat *blockReduction;
  occa::memory o_blockReduction;

  occa::kernel multiWeightedInnerProductKernel;
  occa::kernel multiInnerProductKernel;
  occa::kernel multiScaledAddKernel;



  occa::memory o_vHaloBuffer, o_pHaloBuffer, o_phiHaloBuffer, o_tHaloBuffer; 

  occa::kernel scaledAddKernel;

  occa::kernel totalHaloExtractKernel;
  occa::kernel totalHaloScatterKernel;

  occa::kernel velocityHaloExtractKernel;
  occa::kernel velocityHaloScatterKernel;
  occa::kernel pressureHaloExtractKernel;
  occa::kernel pressureHaloScatterKernel;
  occa::kernel levelSetHaloExtractKernel;
  occa::kernel levelSetHaloScatterKernel;

  occa::kernel advectionVolumeKernel;
  occa::kernel advectionSurfaceKernel;

  occa::kernel levelSetTransportVolumeKernel;
  occa::kernel levelSetTransportSurfaceKernel;
  occa::kernel levelSetTransportUpdateKernel;
  
  
  occa::kernel poissonUpdateKernel; //SS
  occa::kernel poissonRhsCurlKernel; // SS
  occa::kernel poissonRhsNeumannKernel; // SS
  occa::kernel poissonRhsForcingKernel;
  occa::kernel poissonRhsIpdgBCKernel;
  occa::kernel poissonRhsBCKernel;
  occa::kernel poissonAddBCKernel;
  occa::kernel poissonPenaltyKernel;

  occa::kernel advectionCubatureVolumeKernel;
  occa::kernel advectionCubatureSurfaceKernel;
  //
  occa::kernel gradientVolumeKernel;
  occa::kernel gradientSurfaceKernel;

  occa::kernel divergenceVolumeKernel;
  occa::kernel divergenceSurfaceKernel;
  //
  occa::kernel helmholtzRhsForcingKernel;
  occa::kernel helmholtzRhsIpdgBCKernel;
  
  occa::kernel updateUpdateKernel;

}multiFluidIns_t;


multiFluidIns_t *multiFluidInsLevelSetSetup2D(mesh2D* mesh, char *options);

void multiFluidInsLevelSetRun2D(multiFluidIns_t *levelSet, char *options);

void multiFluidInsLevelSetTransportLSERKStep2D(multiFluidIns_t *solver, int tstep, int haloBytes,
                       dfloat * sendBuffer, dfloat *recvBuffer, char * options);


void multiFluidInsPlotVTU2D(multiFluidIns_t *solver, char *fileNameBase);
void multiFluidInsReport2D(multiFluidIns_t *solver, int tstep, char *options);
void multiFluidInsError2D(multiFluidIns_t *solver, dfloat time, char *options);

// ins_t *insSetup2D(mesh2D *mesh, int i, char *options, 
//                   char *velSolverOptions, char *velParAlmondOptions,
//                   char *prSolverOptions,  char *prParAlmondOptions,
//                   char *bdryHeaderFileName);

// void insMakePeriodic2D(mesh2D *mesh, dfloat xper, dfloat yper);

// void insRun2D(ins_t *solver, char *options);
// void insPlotVTU2D(ins_t *solver, char *fileNameBase);
// void insReport2D(ins_t *solver, int tstep, char *options);
// void insError2D(ins_t *solver, dfloat time, char *options);
// void insErrorNorms2D(ins_t *solver, dfloat time, char *options);


// void insRunTimer2D(mesh2D *mesh, char *options, char *bdryHeaderFileName);


// void insRun2D(ins_t *solver, char *options);


// void insAdvectionStep2D(ins_t *solver, int tstep, int haloBytes,
//                      dfloat * sendBuffer, dfloat *recvBuffer, char * options);

// void insAdvectionStepSS2D(ins_t *solver, int tstep, int haloBytes,
//                      dfloat * sendBuffer, dfloat *recvBuffer, char * options);


// void insHelmholtzStep2D(ins_t *solver, int tstep, int haloBytes,
//                      dfloat * sendBuffer, dfloat *recvBuffer, char * options);

// void insHelmholtzStepSS2D(ins_t *solver, int tstep, int haloBytes,
//                      dfloat * sendBuffer, dfloat *recvBuffer, char * options);

// void insPoissonStep2D(ins_t *solver, int tstep, int haloBytes,
//                      dfloat * sendBuffer, dfloat *recvBuffer, char * options);

// void insPoissonStepSS2D(ins_t *solver, int tstep, int haloBytes,
//                      dfloat * sendBuffer, dfloat *recvBuffer, char * options);


// void insUpdateStep2D(ins_t *solver, int tstep, int haloBytes,
//                      dfloat * sendBuffer, dfloat *recvBuffer, char * options);



// void insAdvectionSubCycleStep2D(ins_t *solver, int tstep,
//                      dfloat * tsendBuffer, dfloat *trecvBuffer, 
//                      dfloat * sendBuffer, dfloat *recvBuffer,char * options);




