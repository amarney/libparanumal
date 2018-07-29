/*

The MIT License (MIT)

Copyright (c) 2017 Tim Warburton, Noel Chalmers, Jesse Chan, Ali Karakus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "boltzmann3D.h"

// complete a time step using LSERK4
void boltzmannLsimexStep3D(mesh3D *mesh, int tstep, int haloBytes,
				   dfloat * sendBuffer, dfloat *recvBuffer, char * options){


  for(int k=0;k<mesh->Nimex;++k)
    {

      // intermediate stage time
      dfloat t = tstep*mesh->dt + mesh->dt*mesh->LsimexC[k];
    
      dfloat ramp, drampdt;
      boltzmannRampFunction3D(t, &ramp, &drampdt);

      // ramp = 1.0; 
      // drampdt = 0.0; 

      // RESIDUAL UPDATE, i.e. Y = Q+ (a(k,k-1)-b(k-1))_ex *Y + (a(k,k-1)-b(k-1))_im *Z
      mesh->device.finish();
      occa::tic("residualUpdateKernel");
      // compute volume contribution to DG boltzmann RHS
      if(mesh->pmlNelements){
	//	printf("pmlNel = %d\n", mesh->pmlNelements);
	mesh->pmlResidualUpdateKernel(mesh->pmlNelements,
				      mesh->o_pmlElementIds,
				      mesh->dt,
				      ramp,
				      mesh->LsimexABi[k],
				      mesh->LsimexABe[k],
				      mesh->o_q,
				      mesh->o_pmlq,
				      mesh->o_qZ,
				      mesh->o_qY,
				      mesh->o_pmlqY);
      }
      if(mesh->nonPmlNelements){
	//	printf("nonPmlNel = %d\n", mesh->nonPmlNelements);
	mesh->residualUpdateKernel(mesh->nonPmlNelements,
				   mesh->o_nonPmlElementIds,
				   mesh->dt,
				   mesh->LsimexABi[k],
				   mesh->LsimexABe[k],
				   mesh->o_q,
				   mesh->o_qZ,
				   mesh->o_qY);
      }
      mesh->device.finish();
      occa::toc("residualUpdateKernel");
    

      
      // Compute Implicit Part of Boltzmann, node based no communication
      if(mesh->pmlNelements){
      	//Implicit Solve Satge
      mesh->device.finish();
      occa::tic("pmlImplicitSolve");
	  mesh->pmlImplicitSolveKernel(mesh->pmlNelements,
				     mesh->o_pmlElementIds,
				     mesh->dt,
			         mesh->LsimexAd[k],
			         mesh->o_cubInterpT,
			         mesh->o_cubProjectT,
			         mesh->o_qY,
			         mesh->o_qZ); 
	// No surface term for implicit part
	mesh->pmlImplicitUpdateKernel(mesh->pmlNelements,
				      mesh->o_pmlElementIds,
				      mesh->dt,
				      ramp,
				      mesh->LsimexAd[k],
				      mesh->o_qY,
				      mesh->o_pmlqY,
				      mesh->o_qZ,
				      mesh->o_q,
				      mesh->o_pmlq,
				      mesh->o_qS,
				      mesh->o_pmlqS);
	mesh->device.finish();
      occa::toc("pmlImplicitSolve");

      }
    
      

      // compute volume contribution to DG boltzmann RHS
      
      if(mesh->nonPmlNelements){
		mesh->device.finish();
		occa::tic("implicitSolve");

		mesh->implicitSolveKernel(mesh->nonPmlNelements,
			  mesh->o_nonPmlElementIds,
			  mesh->dt,
			  mesh->LsimexAd[k],
			  mesh->o_cubInterpT,
			  mesh->o_cubProjectT,
			  mesh->o_qY,
			  mesh->o_qZ); 

		//No surface term for implicit part
		mesh->implicitUpdateKernel(mesh->nonPmlNelements,
				   mesh->o_nonPmlElementIds,
				   mesh->dt,
				   mesh->LsimexAd[k],
				   mesh->o_qZ,
				   mesh->o_qY,
				   mesh->o_q,
				   mesh->o_qS);	
			mesh->device.finish();
		occa::toc("implicitSolve");	
      }
      



      if(mesh->totalHaloPairs>0){
	// extract halo on DEVICE
	int Nentries = mesh->Np*mesh->Nfields;
      
	mesh->haloExtractKernel(mesh->totalHaloPairs,
				Nentries,
				mesh->o_haloElementList,
				mesh->o_q,
				mesh->o_haloBuffer);
      
	// copy extracted halo to HOST 
	mesh->o_haloBuffer.copyTo(sendBuffer);      
      
	// start halo exchange
	meshHaloExchangeStart(mesh,
			      mesh->Np*mesh->Nfields*sizeof(dfloat),
			      sendBuffer,
			      recvBuffer);
      }

   


      mesh->device.finish();
      occa::tic("volumeKernel");    
      //compute volume contribution to DG boltzmann RHS
      if(mesh->pmlNelements){
	mesh->pmlVolumeKernel(mesh->pmlNelements,
			      mesh->o_pmlElementIds,
			      ramp,
			      drampdt,
			      mesh->o_vgeo,
			      mesh->o_sigmax,
			      mesh->o_sigmay,
			      mesh->o_sigmaz,
			      mesh->o_DrT,
			      mesh->o_DsT,
			      mesh->o_DtT,
			      mesh->o_q,
			      mesh->o_pmlq,
			      mesh->o_qY,
			      mesh->o_pmlqY); 
      }
    
      // compute volume contribution to DG boltzmann RHS
      // added d/dt (ramp(qbar)) to RHS
      if(mesh->nonPmlNelements){
	mesh->volumeKernel(mesh->nonPmlNelements,
			   mesh->o_nonPmlElementIds,
			   ramp, 
			   drampdt,
			   mesh->o_vgeo,
			   mesh->o_DrT,
			   mesh->o_DsT,
			   mesh->o_DtT,
			   mesh->o_q,
			   mesh->o_qY);
      }
    
    
      mesh->device.finish();
      occa::toc("volumeKernel");
      



      // complete halo exchange
      if(mesh->totalHaloPairs>0){
	// wait for halo data to arrive
	meshHaloExchangeFinish(mesh);
	// copy halo data to DEVICE
	size_t offset = mesh->Np*mesh->Nfields*mesh->Nelements*sizeof(dfloat); // offset for halo data
	mesh->o_q.copyFrom(recvBuffer, haloBytes, offset);
      }
    




      mesh->device.finish();
      occa::tic("surfaceKernel");
      if (mesh->pmlNelements){  
	// compute surface contribution to DG boltzmann RHS
	mesh->pmlSurfaceKernel(mesh->pmlNelements,
			       mesh->o_pmlElementIds,
			       mesh->o_sgeo,
			       mesh->o_LIFTT,
			       mesh->o_vmapM,
			       mesh->o_vmapP,
			       mesh->o_EToB,
			       t,
			       mesh->o_x,
			       mesh->o_y,
			       mesh->o_z,
			       ramp,
			       mesh->o_q,
			       mesh->o_qY,
			       mesh->o_pmlqY);
      }
    
      if(mesh->nonPmlNelements)
	mesh->surfaceKernel(mesh->nonPmlNelements,
			    mesh->o_nonPmlElementIds,
			    mesh->o_sgeo,
			    mesh->o_LIFTT,
			    mesh->o_vmapM,
			    mesh->o_vmapP,
			    mesh->o_EToB,
			    t,
			    mesh->o_x,
			    mesh->o_y,
			    mesh->o_z,
			    ramp,
			    mesh->o_q,
			    mesh->o_qY);
    
      mesh->device.finish();
      occa::toc("surfaceKernel");
   
    



      mesh->device.finish();
      occa::tic("updateKernel");
    
      //printf("running with %d pml Nelements\n",mesh->pmlNelements);    
      if (mesh->pmlNelements){   
	mesh->pmlUpdateKernel(mesh->pmlNelements,
			      mesh->o_pmlElementIds,
			      mesh->dt,
			      mesh->LsimexB[k],
			      ramp,
			      mesh->o_qZ,
			      mesh->o_qY,
			      mesh->o_pmlqY,
			      mesh->o_qS,
			      mesh->o_pmlqS,
			      mesh->o_q,
			      mesh->o_pmlq);
      }
    
      if(mesh->nonPmlNelements){
	mesh->updateKernel(mesh->nonPmlNelements,
			   mesh->o_nonPmlElementIds,
			   mesh->dt,
			   mesh->LsimexB[k],
			   mesh->o_qZ,
			   mesh->o_qY,
			   mesh->o_qS,
			   mesh->o_q);
      }
    
      mesh->device.finish();
      occa::toc("updateKernel");      
    
    }
}