#include "acoustics2D.h"


void acousticsRun2Dbbdg(mesh2D *mesh){

  // MPI send buffer
  iint haloBytes = mesh->totalHaloPairs*mesh->Nfp*mesh->Nfields*mesh->Nfaces*sizeof(dfloat);
  dfloat *sendBuffer = (dfloat*) malloc(haloBytes);
  dfloat *recvBuffer = (dfloat*) malloc(haloBytes);

  //populate the trace buffer fQ
  for (iint l=0;l<mesh->MRABNlevels;l++)
    acousticsMRABUpdate2D(mesh, 0., 0., 0., l, 0., 0.);


  for(iint tstep=0;tstep<mesh->NtimeSteps;++tstep){
    for (iint Ntick=0; Ntick < pow(2,mesh->MRABNlevels-1);Ntick++) {

      // intermediate stage time
      dfloat t = mesh->dt*(tstep*pow(2,mesh->MRABNlevels-1) + Ntick);

      iint lev;
      for (lev=0;lev<mesh->MRABNlevels;lev++)
        if (Ntick % (1<<lev) != 0) break; //find the max lev to compute rhs

      if(mesh->totalHaloPairs>0){
        // extract halo node data
        meshHaloExtract(mesh, mesh->Nfields*mesh->Nfp*mesh->Nfaces*sizeof(dfloat),
              mesh->fQP, sendBuffer);

        // start halo exchange
        meshHaloExchangeStart(mesh,
              mesh->Nfields*mesh->Nfp*mesh->Nfaces*sizeof(dfloat),
              sendBuffer,
              recvBuffer);
      }

      // compute volume contribution to DG acoustics RHS
      for (iint l=0;l<lev;l++) {
        acousticsVolume2Dbbdg(mesh,l);
        acousticsPmlVolume2D(mesh,l);
      }

      if(mesh->totalHaloPairs>0){
        // wait for halo data to arrive
        meshHaloExchangeFinish(mesh);

        // copy data to halo zone of q
        memcpy(mesh->fQP+mesh->Nfp*mesh->Nfields*mesh->Nelements*mesh->Nfaces, recvBuffer, haloBytes);
      }

      // compute surface contribution to DG acoustics RHS
      for (iint l=0;l<lev;l++) {
        acousticsSurface2Dbbdg(mesh,l,t);
        acousticsPmlSurface2Dbbdg(mesh,l,t);
      }

      dfloat a1, a2, a3;
      dfloat b1, b2, b3;
      if (tstep==0) {
        a1 = 1.0;
        a2 = 0.0;
        a3 = 0.0;
        b1 = 0.5;
        b2 = 0.0;
        b3 = 0.0;
      } else if (tstep ==1) {
        a1 =  3.0/2.0;
        a2 = -1.0/2.0;
        a3 =  0.0;
        b1 =  5.0/8.0;
        b2 = -1.0/8.0;
        b3 =  0.0;
      } else {
        a1 =  23./12.;
        a2 = -16./12.;
        a3 =   5./12.;
        b1 =  17./24.;
        b2 =  -7./24.;
        b3 =   2./24.;
      }

      for (lev=0;lev<mesh->MRABNlevels;lev++)
        if ((Ntick+1) % (1<<lev) !=0) break; //find the max lev to update

      #if WADG
        for (iint l=0; l<lev; l++) {
          acousticsMRABpmlUpdate2D(mesh, a1, a2, a3, l, mesh->dt*pow(2,l));
          acousticsMRABUpdate2D_wadg(mesh, a1, a2, a3, l, t, mesh->dt*pow(2,l));
        }
        if (lev<mesh->MRABNlevels) {
          acousticsMRABUpdateTrace2D_wadg(mesh, b1, b2, b3, lev, t, mesh->dt*pow(2,lev-1));
        }
      #else
        for (iint l=0; l<lev; l++) {
          acousticsMRABpmlUpdate2D(mesh, a1, a2, a3, l, mesh->dt*pow(2,l));
          acousticsMRABUpdate2D(mesh, a1, a2, a3, l, t, mesh->dt*pow(2,l));
        }
        if (lev<mesh->MRABNlevels) {
          acousticsMRABUpdateTrace2D(mesh, b1, b2, b3, lev, t, mesh->dt*pow(2,lev-1));
        }
      #endif
    }

    // estimate maximum error
    if((tstep%mesh->errorStep)==0){
      //Transform to nodal basis
      dfloat qtmp[mesh->Nfields*mesh->Np];
      for (iint e =0;e<mesh->Nelements;e++){
        iint id = e*mesh->Np*mesh->Nfields;

        for (iint n=0; n<mesh->Np; n++){
          qtmp[n*mesh->Nfields + 0] = mesh->q[id+n*mesh->Nfields+0];
          qtmp[n*mesh->Nfields + 1] = mesh->q[id+n*mesh->Nfields+1];
          qtmp[n*mesh->Nfields + 2] = mesh->q[id+n*mesh->Nfields+2];
          mesh->q[id+n*mesh->Nfields+0] = 0.0;
          mesh->q[id+n*mesh->Nfields+1] = 0.0;
          mesh->q[id+n*mesh->Nfields+2] = 0.0;
        }
        for (iint n=0;n<mesh->Np;n++){
          for (iint m=0; m<mesh->Np; m++){
            mesh->q[id+n*mesh->Nfields + 0] += mesh->VB[n*mesh->Np+m]*qtmp[m*mesh->Nfields+0];
            mesh->q[id+n*mesh->Nfields + 1] += mesh->VB[n*mesh->Np+m]*qtmp[m*mesh->Nfields+1];
            mesh->q[id+n*mesh->Nfields + 2] += mesh->VB[n*mesh->Np+m]*qtmp[m*mesh->Nfields+2];
          }
        }
      }

      // do error stuff on host
      acousticsError2D(mesh, (mesh->dt)*(tstep+1)*pow(2,mesh->MRABNlevels-1));

      // output field files
      iint fld = 2;
      meshPlotVTU2D(mesh, "foo", fld);

      //Transform to back to modal basis
      for (iint e =0;e<mesh->Nelements;e++){
        iint id = e*mesh->Np*mesh->Nfields;

        for (iint n=0; n<mesh->Np; n++){
          qtmp[n*mesh->Nfields + 0] = mesh->q[id+n*mesh->Nfields+0];
          qtmp[n*mesh->Nfields + 1] = mesh->q[id+n*mesh->Nfields+1];
          qtmp[n*mesh->Nfields + 2] = mesh->q[id+n*mesh->Nfields+2];
          mesh->q[id+n*mesh->Nfields+0] = 0.0;
          mesh->q[id+n*mesh->Nfields+1] = 0.0;
          mesh->q[id+n*mesh->Nfields+2] = 0.0;
        }
        for (iint n=0;n<mesh->Np;n++){
          for (iint m=0; m<mesh->Np; m++){
            mesh->q[id+n*mesh->Nfields + 0] += mesh->invVB[n*mesh->Np+m]*qtmp[m*mesh->Nfields+0];
            mesh->q[id+n*mesh->Nfields + 1] += mesh->invVB[n*mesh->Np+m]*qtmp[m*mesh->Nfields+1];
            mesh->q[id+n*mesh->Nfields + 2] += mesh->invVB[n*mesh->Np+m]*qtmp[m*mesh->Nfields+2];
          }
        }
      }
    }
  }

  free(recvBuffer);
  free(sendBuffer);
}

void acousticsOccaRun2Dbbdg(mesh2D *mesh){

  // MPI send buffer
  iint haloBytes = mesh->totalHaloPairs*mesh->Nfp*mesh->Nfields*mesh->Nfaces*sizeof(dfloat);
  dfloat *sendBuffer = (dfloat*) malloc(haloBytes);
  dfloat *recvBuffer = (dfloat*) malloc(haloBytes);
  int Nframe=0;

  //populate the trace buffer fQ
  dfloat zero = 0.0;
  for (iint l=0; l<mesh->MRABNlevels; l++)
    #if WADG
    if (mesh->MRABNelements[l])
      mesh->updateKernel(mesh->MRABNelements[l],
            mesh->o_MRABelementIds[l],
            zero,
            zero,zero,zero,
            mesh->o_cubInterpT,
            mesh->o_cubProjectT,
            mesh->o_c2,
            zero,
            mesh->o_x,
            mesh->o_y,
            mesh->o_invVB1DT,
            mesh->o_EToB,
            mesh->o_vmapM,
            mesh->o_rhsq,
            mesh->o_q,
            mesh->o_fQM,
            mesh->o_fQP,
            mesh->MRABshiftIndex[l]);
    #else
    if (mesh->MRABNelements[l])
      mesh->updateKernel(mesh->MRABNelements[l],
            mesh->o_MRABelementIds[l],
            zero,
            zero,zero,zero,
            zero,
            mesh->o_x,
            mesh->o_y,
            mesh->o_invVB1DT,
            mesh->o_EToB,
            mesh->o_vmapM,
            mesh->o_rhsq,
            mesh->o_q,
            mesh->o_fQM,
            mesh->o_fQP,
            mesh->MRABshiftIndex[l]);
    #endif

  for(iint tstep=0;tstep<mesh->NtimeSteps;++tstep){
    for (iint Ntick=0; Ntick < pow(2,mesh->MRABNlevels-1);Ntick++) {

      // intermediate stage time
      dfloat t = mesh->dt*(tstep*pow(2,mesh->MRABNlevels-1) + Ntick);

      iint lev;
      for (lev=0;lev<mesh->MRABNlevels;lev++)
        if (Ntick % (1<<lev) != 0) break; //find the max lev to compute rhs

      if(mesh->totalHaloPairs>0){
        // extract halo on DEVICE
        iint Nentries = mesh->Nfp*mesh->Nfields*mesh->Nfaces;

        mesh->haloExtractKernel(mesh->totalHaloPairs,
                    Nentries,
                    mesh->o_haloElementList,
                    mesh->o_fQP,
                    mesh->o_haloBuffer);

        // copy extracted halo to HOST
        mesh->o_haloBuffer.copyTo(sendBuffer);

        // start halo exchange
        meshHaloExchangeStart(mesh,
              mesh->Nfields*mesh->Nfp*mesh->Nfaces*sizeof(dfloat),
              sendBuffer,
              recvBuffer);
      }

      // compute volume contribution to DG acoustics RHS
      for (iint l=0;l<lev;l++) {
        if (mesh->MRABNelements[l])
          mesh->volumeKernel(mesh->MRABNelements[l],
              mesh->o_MRABelementIds[l],
              mesh->o_vgeo,
              mesh->o_D1ids,
              mesh->o_D2ids,
              mesh->o_D3ids,
              mesh->o_Dvals,
              mesh->o_q,
              mesh->o_rhsq,
              mesh->MRABshiftIndex[l]);

        if (mesh->MRABpmlNelements[l])
          #if WADG
            mesh->pmlKernel(mesh->MRABpmlNelements[l],
                            mesh->o_MRABpmlElementIds[l],
                            mesh->o_MRABpmlIds[l],
                            mesh->o_cubInterpT,
                            mesh->o_cubProjectT,
                            mesh->o_c2,
                            mesh->o_pmlSigmaX,
                            mesh->o_pmlSigmaY,
                            mesh->o_q,
                            mesh->o_pmlq,
                            mesh->o_rhsq,
                            mesh->o_pmlrhsq,
                            mesh->MRABshiftIndex[l]);
          #else
            mesh->pmlKernel(mesh->MRABpmlNelements[l],
                            mesh->o_MRABpmlElementIds[l],
                            mesh->o_MRABpmlIds[l],
                            mesh->o_cubInterpT,
                            mesh->o_cubProjectT,
                            mesh->o_pmlSigmaX,
                            mesh->o_pmlSigmaY,
                            mesh->o_q,
                            mesh->o_pmlq,
                            mesh->o_rhsq,
                            mesh->o_pmlrhsq,
                            mesh->MRABshiftIndex[l]);
          #endif
      }

      if(mesh->totalHaloPairs>0){
        // wait for halo data to arrive
        meshHaloExchangeFinish(mesh);

        // copy halo data to DEVICE
        size_t offset = mesh->Nfaces*mesh->Nfp*mesh->Nfields*mesh->Nelements*sizeof(dfloat); // offset for halo data
        mesh->o_fQP.copyFrom(recvBuffer, haloBytes, offset);
      }

      // compute surface contribution to DG acoustics RHS
      for (iint l=0;l<lev;l++)
        if (mesh->MRABNelements[l])
          mesh->surfaceKernel(mesh->MRABNelements[l],
                  mesh->o_MRABelementIds[l],
                  mesh->o_sgeo,
                  mesh->o_L0vals,
                  mesh->o_ELids,
                  mesh->o_ELvals,
                  mesh->o_vmapM,
                  mesh->o_mapP,
                  mesh->o_EToB,
                  t,
                  mesh->o_x,
                  mesh->o_y,
                  mesh->o_q,
                  mesh->o_fQM,
                  mesh->o_fQP,
                  mesh->o_rhsq,
                  mesh->MRABshiftIndex[l]);

      dfloat a1, a2, a3;
      dfloat b1, b2, b3;
      if (tstep==0) {
        a1 = 1.0;
        a2 = 0.0;
        a3 = 0.0;
        b1 = 0.5;
        b2 = 0.0;
        b3 = 0.0;
      } else if (tstep ==1) {
        a1 =  3.0/2.0;
        a2 = -1.0/2.0;
        a3 =  0.0;
        b1 =  5.0/8.0;
        b2 = -1.0/8.0;
        b3 =  0.0;
      } else {
        a1 =  23./12.;
        a2 = -16./12.;
        a3 =   5./12.;
        b1 =  17./24.;
        b2 =  -7./24.;
        b3 =   2./24.;
      }

      for (lev=0;lev<mesh->MRABNlevels;lev++)
        if ((Ntick+1) % (1<<lev) !=0) break; //find the max lev to update

      #if WADG
        for (iint l=0; l<lev; l++) {
          if (mesh->MRABNelements[l])
            mesh->updateKernel(mesh->MRABNelements[l],
                mesh->o_MRABelementIds[l],
                mesh->dt*pow(2,l),
                a1,a2,a3,
                mesh->o_cubInterpT,
                mesh->o_cubProjectT,
                mesh->o_c2,
                t,
                mesh->o_x,
                mesh->o_y,
                mesh->o_invVB1DT,
                mesh->o_EToB,
                mesh->o_vmapM,
                mesh->o_rhsq,
                mesh->o_q,
                mesh->o_fQM,
                mesh->o_fQP,
                mesh->MRABshiftIndex[l]);

          if (mesh->MRABpmlNelements[l])
            mesh->pmlUpdateKernel(mesh->MRABpmlNelements[l],
                mesh->o_MRABpmlIds[l],
                mesh->dt*pow(2,l),
                a1,a2,a3,
                mesh->o_pmlrhsq,
                mesh->o_pmlq,
                mesh->MRABshiftIndex[l]);

          //rotate index
          mesh->MRABshiftIndex[l] = (mesh->MRABshiftIndex[l]+1)%3;
        }
        if (lev<mesh->MRABNlevels)
          if (mesh->MRABNhaloElements[lev])
            mesh->traceUpdateKernel(mesh->MRABNhaloElements[lev],
                mesh->o_MRABhaloIds[lev],
                mesh->dt*pow(2,lev-1),
                b1,b2,b3,
                mesh->o_cubInterpT,
                mesh->o_cubProjectT,
                mesh->o_c2,
                t,
                mesh->o_x,
                mesh->o_y,
                mesh->o_invVB1DT,
                mesh->o_EToB,
                mesh->o_vmapM,
                mesh->o_rhsq,
                mesh->o_q,
                mesh->o_fQM,
                mesh->o_fQP,
                mesh->MRABshiftIndex[lev]);
      #else
        for (iint l=0; l<lev; l++) {
          if (mesh->MRABNelements[l])
            mesh->updateKernel(mesh->MRABNelements[l],
                mesh->o_MRABelementIds[l],
                mesh->dt*pow(2,l),
                a1,a2,a3,
                t,
                mesh->o_x,
                mesh->o_y,
                mesh->o_invVB1DT,
                mesh->o_EToB,
                mesh->o_vmapM,
                mesh->o_rhsq,
                mesh->o_q,
                mesh->o_fQM,
                mesh->o_fQP,
                mesh->MRABshiftIndex[l]);

          if (mesh->MRABpmlNelements[l])
            mesh->pmlUpdateKernel(mesh->MRABpmlNelements[l],
                mesh->o_MRABpmlIds[l],
                mesh->dt*pow(2,l),
                a1,a2,a3,
                mesh->o_pmlrhsq,
                mesh->o_pmlq,
                mesh->MRABshiftIndex[l]);

          //rotate index
          mesh->MRABshiftIndex[l] = (mesh->MRABshiftIndex[l]+1)%3;
        }

        if (lev<mesh->MRABNlevels)
          if (mesh->MRABNhaloElements[lev])
            mesh->traceUpdateKernel(mesh->MRABNhaloElements[lev],
                mesh->o_MRABhaloIds[lev],
                mesh->dt*pow(2,lev-1),
                b1,b2,b3,
                t,
                mesh->o_x,
                mesh->o_y,
                mesh->o_invVB1DT,
                mesh->o_EToB,
                mesh->o_vmapM,
                mesh->o_rhsq,
                mesh->o_q,
                mesh->o_fQM,
                mesh->o_fQP,
                mesh->MRABshiftIndex[lev]);
      #endif

    }
    // estimate maximum error
    if((tstep%mesh->errorStep)==0){

      // copy data back to host
      mesh->o_q.copyTo(mesh->q);

      dfloat qtmp[mesh->Nfields*mesh->Np];
      for (iint e =0;e<mesh->Nelements;e++){
        iint id = e*mesh->Np*mesh->Nfields;

        for (iint n=0; n<mesh->Np; n++){
          qtmp[n*mesh->Nfields + 0] = mesh->q[id+n*mesh->Nfields+0];
          qtmp[n*mesh->Nfields + 1] = mesh->q[id+n*mesh->Nfields+1];
          qtmp[n*mesh->Nfields + 2] = mesh->q[id+n*mesh->Nfields+2];
          mesh->q[id+n*mesh->Nfields+0] = 0.0;
          mesh->q[id+n*mesh->Nfields+1] = 0.0;
          mesh->q[id+n*mesh->Nfields+2] = 0.0;
        }
        for (iint n=0;n<mesh->Np;n++){
          for (iint m=0; m<mesh->Np; m++){
            mesh->q[id+n*mesh->Nfields + 0] += mesh->VB[n*mesh->Np+m]*qtmp[m*mesh->Nfields+0];
            mesh->q[id+n*mesh->Nfields + 1] += mesh->VB[n*mesh->Np+m]*qtmp[m*mesh->Nfields+1];
            mesh->q[id+n*mesh->Nfields + 2] += mesh->VB[n*mesh->Np+m]*qtmp[m*mesh->Nfields+2];
          }
        }
      }

      void addSourceField(mesh2D *mesh, dfloat *q, dfloat t);
      addSourceField(mesh, mesh->q,mesh->dt*(tstep+1)*pow(2,mesh->MRABNlevels-1));

      // do error stuff on host
      acousticsError2D(mesh, mesh->dt*(tstep+1)*pow(2,mesh->MRABNlevels-1));

      // output field files
      iint fld = 2;
      char fileName[BUFSIZ];
      
      int rank;
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
      
      sprintf(fileName, "foo_%04d_%04d.vtu", rank, Nframe++);
      meshPlotVTU2D(mesh, fileName, fld);
    }
  }

  free(recvBuffer);
  free(sendBuffer);
}

//Ricker pulse
void acousticsRickerPulse2D(dfloat x, dfloat y, dfloat t, dfloat f, dfloat c, 
                           dfloat *u, dfloat *v, dfloat *p);

void addSourceField(mesh2D *mesh, dfloat *q, dfloat t) {

  for (iint m=0;m<mesh->sourceNelements;m++) {
    iint e = mesh->sourceElements[m];

    for (iint n=0;n<mesh->Np;n++) {
      iint id = n + e*mesh->Np;
      
      dfloat x = mesh->x[id];
      dfloat y = mesh->y[id];

      dfloat x0 = mesh->sourceX0;
      dfloat y0 = mesh->sourceY0;
      dfloat t0 = mesh->sourceT0;
      dfloat freq = mesh->sourceFreq;
    
      dfloat c = sqrt(mesh->sourceC2);

      dfloat u, v, p;
      acousticsRickerPulse2D(x-x0, y-y0, t+t0, freq,c, &u, &v, &p);

      iint qid = mesh->Nfields*id;
      q[qid+0] += u;
      q[qid+1] += v;
      q[qid+2] += p;
    }
  }
}


