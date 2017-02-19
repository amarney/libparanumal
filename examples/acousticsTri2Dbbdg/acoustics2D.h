
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mpi.h"
#include "mesh2D.h"

#define USE_BERN 1

void acousticsSetup2D(mesh2D *mesh);

void acousticsRun2D(mesh2D *mesh);

void acousticsRun2Dbbdg(mesh2D *mesh);

void acousticsOccaRun2D(mesh2D *mesh);

void acousticsOccaRun2Dbbdg(mesh2D *mesh);

void acousticsVolume2D(mesh2D *mesh);
void acousticsSurface2D(mesh2D *mesh, dfloat t);
void acousticsUpdate2D(mesh2D *mesh, dfloat rka, dfloat rkb);

void acousticsVolume2Dbbdg(mesh2D *mesh);
void acousticsSurface2Dbbdg(mesh2D *mesh, dfloat t);

void acousticsError2D(mesh2D *mesh, dfloat time);

void acousticsGaussianPulse2D(dfloat x, dfloat y, dfloat t, dfloat *u, dfloat *v, dfloat *p);