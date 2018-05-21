
ifndef OCCA_DIR
ERROR:
	@echo "Error, environment variable [OCCA_DIR] is not set"
endif

CXXFLAGS = 

include ${OCCA_DIR}/scripts/makefile

# define variables
HDRDIR  = ../../include
GSDIR  = ../../3rdParty/gslib.github/src
ALMONDDIR = ../parALMOND
ELLIPTICDIR = ../elliptic

# set options for this machine
# specify which compilers to use for c, fortran and linking
cc	= mpicc.openmpi
CC	= mpic++.openmpi
LD	= mpic++.openmpi

# compiler flags to be used (set to compile with debugging on)
CFLAGS = -I. $(compilerFlags) $(flags) -I$(HDRDIR) -I$(GSDIR) -I$(ELLIPTICDIR) -g  -D DHOLMES='"${CURDIR}/../.."' -D DINS='"${CURDIR}"'

# link flags to be used 
LDFLAGS	= $(compilerFlags) $(flags) -g -L../../3rdParty/gslib.github  -lgs \
			-L$(ELLIPTICDIR) -lelliptic -L$(ALMONDDIR) -lparALMOND 

# libraries to be linked in
LIBS	=  $(links) -L../../3rdParty/BlasLapack -lBlasLapack -lgfortran

INCLUDES = ins.h
DEPS = $(INCLUDES) \
$(HDRDIR)/mesh.h \
$(HDRDIR)/mesh2D.h \
$(HDRDIR)/mesh3D.h \
$(HDRDIR)/ogs_t.h \
$(ALMONDDIR)/parALMOND.h \
$(ELLIPTICDIR)/elliptic.h \
$(ELLIPTICDIR)/ellipticPrecon.h  

# types of files we are going to construct rules for
.SUFFIXES: .c 

# rule for .c files
.c.o: $(DEPS)
	$(CC) $(CFLAGS) -o $*.o -c $*.c $(paths)

# list of objects to be compiled
AOBJS    = \
./src/insSetup.o \
./src/insPlotVTU.o \
./src/insError.o \
./src/insReport.o \
./src/insRunARK.o \
./src/insRunEXTBDF.o \
./src/insAdvection.o \
./src/insDiffusion.o \
./src/insGradient.o \
./src/insDivergence.o \
./src/insSubCycle.o \
./src/insVelocityRhs.o \
./src/insVelocitySolve.o \
./src/insVelocityUpdate.o \
./src/insPressureRhs.o \
./src/insPressureSolve.o \
./src/insPressureUpdate.o 

# library objects
LOBJS = \
../../src/meshConnect.o \
../../src/meshConnectBoundary.o \
../../src/meshConnectFaceNodes2D.o \
../../src/meshConnectFaceNodes3D.o \
../../src/meshGeometricFactorsTet3D.o \
../../src/meshGeometricFactorsHex3D.o \
../../src/meshGeometricFactorsTri2D.o \
../../src/meshGeometricFactorsQuad2D.o \
../../src/meshGeometricPartition2D.o \
../../src/meshGeometricPartition3D.o \
../../src/meshHaloExchange.o \
../../src/meshHaloExtract.o \
../../src/meshHaloSetup.o \
../../src/meshLoadReferenceNodesTri2D.o \
../../src/meshLoadReferenceNodesQuad2D.o \
../../src/meshLoadReferenceNodesTet3D.o \
../../src/meshLoadReferenceNodesHex3D.o \
../../src/meshOccaSetup2D.o \
../../src/meshOccaSetup3D.o \
../../src/meshParallelConnectNodes.o \
../../src/meshParallelConnectOpt.o \
../../src/meshParallelConsecutiveGlobalNumbering.o\
../../src/meshParallelGatherScatter.o \
../../src/meshParallelGatherScatterSetup.o \
../../src/meshParallelReaderTri2D.o \
../../src/meshParallelReaderQuad2D.o \
../../src/meshParallelReaderTet3D.o \
../../src/meshParallelReaderHex3D.o \
../../src/meshPartitionStatistics.o \
../../src/meshPhysicalNodesTri2D.o \
../../src/meshPhysicalNodesQuad2D.o \
../../src/meshPhysicalNodesTet3D.o \
../../src/meshPhysicalNodesHex3D.o \
../../src/meshPlotVTU2D.o \
../../src/meshPlotVTU3D.o \
../../src/meshPrint2D.o \
../../src/meshPrint3D.o \
../../src/meshSetupTri2D.o \
../../src/meshSetupQuad2D.o \
../../src/meshSetupTet3D.o \
../../src/meshSetupHex3D.o \
../../src/meshSurfaceGeometricFactorsTri2D.o \
../../src/meshSurfaceGeometricFactorsQuad2D.o \
../../src/meshSurfaceGeometricFactorsTet3D.o \
../../src/meshSurfaceGeometricFactorsHex3D.o \
../../src/meshVTU2D.o \
../../src/meshVTU3D.o \
../../src/matrixInverse.o \
../../src/matrixConditionNumber.o \
../../src/mysort.o \
../../src/parallelSort.o\
../../src/hash.o\
../../src/setupAide.o \
../../src/timer.o\
../../src/readArray.o

COBJS = \
../../src/gsParallelGatherScatter.o\
../../src/gsParallelGatherScatterSetup.o\
../../src/xxtCoarseSolve.o

insMain:$(AOBJS) $(LOBJS) ./src/insMain.o gslibInterface
	cd ../../3rdParty/BlasLapack; make -j lib; cd ../../solvers/ins
	cd ../parALMOND; make -j lib; cd ../ins
	cd ../elliptic; make -j lib; cd ../ins
	$(LD)  $(LDFLAGS)  -o insMain ./src/insMain.o $(COBJS) $(AOBJS) $(LOBJS) $(paths) $(LIBS)

lib:$(AOBJS)
	ar -cr libins.a $(AOBJS)

gslibInterface:
	cd ../../3rdParty/gslib.github; make -j libgs.a; cd ../../solvers/ins
	$(cc) $(CFLAGS) -c -o ../../src/gsParallelGatherScatter.o ../../src/gsParallelGatherScatter.c $(paths)
	$(cc) $(CFLAGS) -c -o ../../src/gsParallelGatherScatterSetup.o ../../src/gsParallelGatherScatterSetup.c $(paths) 
	$(cc) $(CFLAGS) -c -o ../../src/xxtCoarseSolve.o ../../src/xxtCoarseSolve.c $(paths)

all: lib insMain

# what to do if user types "make clean"
clean:
	cd ../parALMOND; make clean; cd ../ins
	cd ../elliptic; make clean; cd ../ins
	cd ../../src; rm *.o; cd ../solvers/ins
	rm *.o insMain libins.a

realclean:
	cd ../../3rdParty/gslib.github; make clean; cd ../../solvers/ins
	cd ../../3rdParty/BlasLapack; make clean; cd ../../solvers/ins
	cd ../parALMOND; make clean; cd ../ins
	cd ../elliptic; make clean; cd ../ins
	cd ../../src; rm *.o; cd ../solvers/ins
	rm *.o insMain libins.a