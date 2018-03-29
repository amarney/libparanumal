#include <stdlib.h>
#include "mesh2D.h"

typedef struct {
  iint localId;
  iint globalId;
}gatherNode_t;

int compareGatherNodes(const void *a, const void *b){
  const gatherNode_t *nodea = (gatherNode_t*) a;
  const gatherNode_t *nodeb = (gatherNode_t*) b;

  if(nodea->globalId <nodeb->globalId) return -1;
  if(nodea->globalId >nodeb->globalId) return +1;
  return 0;
}

// serial -
// in parallel - add a isend/irecv step before doing this then finish up after wait
void meshGatherScatterSetupQuad2D(mesh2D *mesh){

  iint NnodesL = mesh->Np*mesh->Nelements;

  // set up local-global number pairs
  gatherNode_t *gatherNodes = (gatherNode_t*) calloc(NnodesL, sizeof(gatherNode_t));
  int lnode = 0;
  for(iint e=0;e<mesh->Nelements;++e){
    for(iint n=0;n<mesh->Np;++n){
      gatherNodes[lnode].localId  = lnode;
      gatherNodes[lnode].globalId = mesh->globalNumbering[lnode];
      ++lnode;
    }
  }
  
  // sort based on gatherNode index
  qsort(gatherNodes, NnodesL, sizeof(gatherNode_t), compareGatherNodes);

  // extract: ordered list of local node indices for gather
  mesh->gatherIds = (iint*) calloc(NnodesL, sizeof(iint));
  for(iint n=0;n<NnodesL;++n)   
    mesh->gatherIds[n] = gatherNodes[n].localId;

  // count unique global nodes
  mesh->NgatherNodes = 1;
  for(iint n=1;n<NnodesL;++n)
    if(gatherNodes[n].globalId!=gatherNodes[n-1].globalId)
      ++(mesh->NgatherNodes);
  
  // count multiplicity of gather nodes and find offsets
  mesh->gatherCounts  = (iint*) calloc(mesh->NgatherNodes, sizeof(iint)); 
  mesh->gatherOffsets = (iint*) calloc(mesh->NgatherNodes, sizeof(iint)); 
  iint gnode = 0;
  for(iint n=1;n<NnodesL;++n){ 
    if(gatherNodes[n].globalId!=gatherNodes[n-1].globalId){
      ++gnode;
      mesh->gatherOffsets[gnode] = n;
      mesh->gatherCounts[gnode-1] = n-mesh->gatherOffsets[gnode-1]; 
    }
  }

  mesh->gatherCounts[gnode] = NnodesL-mesh->gatherOffsets[gnode]; // ??

#if 0
  printf("gnode = %d, NgatherNodes = %d,offsets = %d, gatherCounts=%d\n",
	 gnode, mesh->NgatherNodes, mesh->gatherOffsets[gnode], mesh->gatherCounts[gnode]);

  iint test = 0;
  for(iint n=0;n<mesh->NgatherNodes;++n){
    test += mesh->gatherCounts[n];
  }
  printf("test=%d, NnodesL=%d\n", test, NnodesL);
  exit(-1);

  for(iint e=0;e<mesh->Nelements;++e){
    for(iint j=0;j<mesh->Nq;++j){
      for(iint i=0;i<mesh->Nq;++i){
	printf("%d ", mesh->globalNumbering[i + j*mesh->Nq + e*mesh->Np]);
      }
      printf("\n");
    }
    printf("\n");
  }
  printf("----");
  
  for(int n=0;n<mesh->NgatherNodes;++n){
    printf("%d: ", n);
    for(int m=0;m<mesh->gatherCounts[n];++m){
      printf("%d ", mesh->gatherIds[m + mesh->gatherOffsets[n]]);
    }
    printf("\n");
  }
#endif
  
  free(gatherNodes);
}