/***************************************************************************

Copyright 2000 Intel Corporation.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL INTEL, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * i810_hwmc.c: i810 HWMC Driver
 *
 * Authors:
 *      Matt Sottek <matthew.j.sottek@intel.com>
 *
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i810_hwmc.c,v 1.4 2002/09/11 00:29:32 dawes Exp $ */


#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86fbman.h"
#include "regionstr.h"

#include "i810.h"
#include "i810_dri.h"

#include "xf86xv.h"
#include "xf86xvmc.h"
#include "Xv.h"
#include "XvMC.h"
#include "xaa.h"
#include "xaalocal.h"
#include "dixstruct.h"
#include "fourcc.h"

int I810XvMCCreateContext (ScrnInfoPtr pScrn, XvMCContextPtr pContext,
                           int *num_priv, long **priv );
void I810XvMCDestroyContext (ScrnInfoPtr pScrn, XvMCContextPtr pContext);

int I810XvMCCreateSurface (ScrnInfoPtr pScrn, XvMCSurfacePtr pSurf,
                           int *num_priv, long **priv );
void I810XvMCDestroySurface (ScrnInfoPtr pScrn, XvMCSurfacePtr pSurf);

int I810XvMCCreateSubpicture (ScrnInfoPtr pScrn, XvMCSubpicturePtr pSurf,
                               int *num_priv, long **priv );
void I810XvMCDestroySubpicture (ScrnInfoPtr pScrn, XvMCSubpicturePtr pSurf);


typedef struct {
  drmContext drmcontext;
  unsigned int fbBase;
  unsigned int OverlayOffset;
  unsigned int OverlaySize;
  unsigned int SurfacesOffset;
  unsigned int SurfacesSize;
  char busIdString[10];
  char pad[2];
} I810XvMCCreateContextRec;


static int yv12_subpicture_index_list[2] = 
{
  FOURCC_IA44,
  FOURCC_AI44
};

static XF86MCImageIDList yv12_subpicture_list =
{
  2,
  yv12_subpicture_index_list
};
 
static XF86MCSurfaceInfoRec i810_YV12_mpg2_surface =
{
    FOURCC_YV12,  
    XVMC_CHROMA_FORMAT_420,
    0,
    720,
    576,
    720,
    576,
    XVMC_MPEG_2,
    XVMC_OVERLAID_SURFACE | XVMC_SUBPICTURE_INDEPENDENT_SCALING |
    XVMC_INTRA_UNSIGNED,
    &yv12_subpicture_list
};

static XF86MCSurfaceInfoRec i810_YV12_mpg1_surface =
{
    FOURCC_YV12,  
    XVMC_CHROMA_FORMAT_420,
    0,
    720,
    576,
    720,
    576,
    XVMC_MPEG_1,
    XVMC_OVERLAID_SURFACE | XVMC_SUBPICTURE_INDEPENDENT_SCALING |
    XVMC_INTRA_UNSIGNED,
    &yv12_subpicture_list
};

static XF86MCSurfaceInfoPtr ppSI[2] = 
{
    (XF86MCSurfaceInfoPtr)&i810_YV12_mpg2_surface,
    (XF86MCSurfaceInfoPtr)&i810_YV12_mpg1_surface
};

/* List of subpicture types that we support */
static XF86ImageRec ia44_subpicture = XVIMAGE_IA44;
static XF86ImageRec ai44_subpicture = XVIMAGE_AI44;

static XF86ImagePtr i810_subpicture_list[2] =
{
  (XF86ImagePtr)&ia44_subpicture,
  (XF86ImagePtr)&ai44_subpicture
};

/* Fill in the device dependent adaptor record. 
 * This is named "I810 Video Overlay" because this code falls under the
 * XV extenstion, the name must match or it won't be used.
 *
 * Surface and Subpicture - see above
 * Function pointers to functions below
 */
static XF86MCAdaptorRec pAdapt = 
{
  "I810 Video Overlay",		/* name */
  2,				/* num_surfaces */
  ppSI,				/* surfaces */
  2,				/* num_subpictures */
  i810_subpicture_list,		/* subpictures */
  (xf86XvMCCreateContextProcPtr)I810XvMCCreateContext,
  (xf86XvMCDestroyContextProcPtr)I810XvMCDestroyContext,
  (xf86XvMCCreateSurfaceProcPtr)I810XvMCCreateSurface,
  (xf86XvMCDestroySurfaceProcPtr)I810XvMCDestroySurface,
  (xf86XvMCCreateSubpictureProcPtr)I810XvMCCreateSubpicture,
  (xf86XvMCDestroySubpictureProcPtr)I810XvMCDestroySubpicture
};

static XF86MCAdaptorPtr ppAdapt[1] = 
{
	(XF86MCAdaptorPtr)&pAdapt
};

/**************************************************************************
 *
 *  I810InitMC
 *
 *  Initialize the hardware motion compenstation extention for this 
 *  hardware. The initialization routines want the address of the pointers
 *  to the structures, not the address of the structures. This means we
 *  allocate (or create static?) the pointer memory and pass that 
 *  address. This seems a little convoluted.
 *
 *  We need to allocate memory for the device depended adaptor record. 
 *  This is what holds the pointers to all our device functions.
 *
 *  We need to map the overlay registers into the drm.
 *
 *  We need to map the surfaces into the drm.
 *
 *  Inputs:
 *    Screen pointer
 *
 *  Outputs:
 *    None, this calls the device independent screen initialization 
 *    function.
 *
 *  Revisions:
 *  
 **************************************************************************/
void I810InitMC(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  I810Ptr pI810 = I810PTR(pScrn);
  int i;

  /* Clear the Surface Allocation */
  for(i=0; i<I810_MAX_SURFACES; i++) {
	pI810->surfaceAllocation[i] = 0;
  }  

  /* Cursor is at a page boundary, Overlay regs are not, don't forget */
  if (drmAddMap(pI810->drmSubFD, (drmHandle)pI810->CursorStart,
                4096, DRM_AGP, 0, &pI810->overlay_map) < 0) {
    xf86DrvMsg(pScreen->myNum, X_ERROR, "drmAddMap(overlay) failed\n");
    return;
  }
  if (drmAddMap(pI810->drmSubFD, (drmHandle)pI810->MC.Start,
                pI810->MC.Size, DRM_AGP, 0, &pI810->mc_map) < 0) {
    xf86DrvMsg(pScreen->myNum, X_ERROR, "drmAddMap(MC) failed\n");
    return;
  }
  xf86XvMCScreenInit(pScreen, 1, ppAdapt);
}

/**************************************************************************
 *
 *  I810XvMCCreateContext
 *
 *  Some info about the private data:
 *
 *  Set *num_priv to the number of 32bit words that make up the size of
 *  of the data that priv will point to.
 *
 *  *priv = (long *) xcalloc (elements, sizeof(element))
 *  *num_priv = (elements * sizeof(element)) >> 2;
 *
 **************************************************************************/

int I810XvMCCreateContext (ScrnInfoPtr pScrn, XvMCContextPtr pContext,
                            int *num_priv, long **priv )
{
  I810Ptr pI810 = I810PTR(pScrn);
  DRIInfoPtr pDRIInfo = pI810->pDRIInfo;
  I810XvMCCreateContextRec *contextRec;


  if(!pI810->directRenderingEnabled) {
    xf86DrvMsg(X_ERROR, pScrn->scrnIndex,
        "I810XvMCCreateContext: Cannot use XvMC without DRI!\n");
    return BadAlloc;
  }

  /* Context Already in use! */
  if(pI810->xvmcContext) {
    xf86DrvMsg(X_WARNING, pScrn->scrnIndex,
        "I810XvMCCreateContext: 2 XvMC Contexts Attempted, not supported.\n");
    return BadAlloc;
  }

  *priv = xcalloc(1,sizeof(I810XvMCCreateContextRec));
  contextRec = (I810XvMCCreateContextRec *)*priv;

  if(!*priv) {
    *num_priv = 0;
    return BadAlloc;
  }

  *num_priv = sizeof(I810XvMCCreateContextRec) >> 2;
  if(drmCreateContext(pI810->drmSubFD, &(contextRec->drmcontext) ) < 0) {
    xf86DrvMsg(X_ERROR, pScrn->scrnIndex,
        "I810XvMCCreateContext: Unable to create DRMContext!\n");
    xfree(*priv);
    return BadAlloc;
  }

  drmAuthMagic(pI810->drmSubFD, pContext->flags);

  pI810->xvmcContext = contextRec->drmcontext;
  contextRec->fbBase = pScrn->memPhysBase;

  /* Overlay Regs are at 1024 offset into the Cursor Space */
  contextRec->OverlayOffset = pI810->CursorStart;
  contextRec->OverlaySize = 4096;

  contextRec->SurfacesOffset = pI810->MC.Start;
  contextRec->SurfacesSize = pI810->MC.Size;
  strncpy (contextRec->busIdString, pDRIInfo->busIdString, 9);

  return Success;
}


int I810XvMCCreateSurface (ScrnInfoPtr pScrn, XvMCSurfacePtr pSurf,
                           int *num_priv, long **priv )
{
  I810Ptr pI810 = I810PTR(pScrn);
  int i;

  *priv = (long *)xcalloc(2,sizeof(long));

  if(!*priv) {
    xf86DrvMsg(X_ERROR, pScrn->scrnIndex,
        "I810XvMCCreateSurface: Unable to allocate memory!\n");
    *num_priv = 0;
    return BadAlloc;
  }
  *num_priv = 2;

  /* Surface Arrangement is different based on 6 or 7 Surfaces */
  if(pI810->numSurfaces == 6) {
     for(i=0; i<pI810->numSurfaces; i++) {
       if(!pI810->surfaceAllocation[i]) {
         pI810->surfaceAllocation[i] = pSurf->surface_id;
         /* Y data starts at 2MB offset, each surface is 576k */
         (*priv)[0] = (2*1024*1024 + 576*1024 * i);
         /* UV data starts at 0 offset, each set is 288k */
         (*priv)[1] = (576*512 * i);
         return Success;
       }
     }
  }
  if(pI810->numSurfaces == 7) {
     for(i=0; i<pI810->numSurfaces; i++) {
       if(!pI810->surfaceAllocation[i]) {
         pI810->surfaceAllocation[i] = pSurf->surface_id;
         /* Y data starts at 2.5MB offset, each surface is 576k */
         (*priv)[0] = (2*1024*1024 + 512*1024 + 576*1024 * i);
         /* UV data starts at 0 offset, each set is 288k */
         (*priv)[1] = (576*512 * i);
         return Success;
       }
     }
  }
  (*priv)[0] = 0;
  (*priv)[1] = 0;
  return BadAlloc;
}

int I810XvMCCreateSubpicture (ScrnInfoPtr pScrn, XvMCSubpicturePtr pSubp,
                              int *num_priv, long **priv )
{
  I810Ptr pI810 = I810PTR(pScrn);
  int i;

  *priv = (long *)xcalloc(1,sizeof(long));

  if(!*priv) {
    xf86DrvMsg(X_ERROR, pScrn->scrnIndex,
        "I810XvMCCreateSubpicture: Unable to allocate memory!\n");
    *num_priv = 0;
    return BadAlloc;
  }
  *num_priv = 1;

  if(pI810->numSurfaces == 6) {
     for(i=6; i<8; i++) {
       if(!pI810->surfaceAllocation[i]) {
         pI810->surfaceAllocation[i] = pSubp->subpicture_id;
         /* Subpictures are after the Y surfaces in memory */ 
         (*priv)[0] = (2*1024*1024 + 576*1024 * i);
         return Success;
       }
     }
  }
  if(pI810->numSurfaces == 7) {
     for(i=7; i<9; i++) {
         if(!pI810->surfaceAllocation[i]) {
         pI810->surfaceAllocation[i] = pSubp->subpicture_id;
         /* Subpictures are after the Y surfaces in memory */
         (*priv)[0] = (2*1024*1024 + 512*1024 + 576*1024 * i);
         return Success;
       }
     }
  }

  (*priv)[0] = 0;
  return BadAlloc;
}

void I810XvMCDestroyContext (ScrnInfoPtr pScrn, XvMCContextPtr pContext)
{
  I810Ptr pI810 = I810PTR(pScrn);

  drmDestroyContext(pI810->drmSubFD,pI810->xvmcContext);
  pI810->xvmcContext = 0;
}

void I810XvMCDestroySurface (ScrnInfoPtr pScrn, XvMCSurfacePtr pSurf)
{
  I810Ptr pI810 = I810PTR(pScrn);
  int i;

  for(i=0; i<I810_MAX_SURFACES; i++) {
    if(pI810->surfaceAllocation[i] == pSurf->surface_id) {
      pI810->surfaceAllocation[i] = 0;
      return;
    }
  }
  return;
}

void I810XvMCDestroySubpicture (ScrnInfoPtr pScrn, XvMCSubpicturePtr pSubp)
{
  I810Ptr pI810 = I810PTR(pScrn);
  int i;

  for(i=pI810->numSurfaces; i<I810_MAX_SURFACES + I810_MAX_SUBPICTURES; i++) {
    if(pI810->surfaceAllocation[i] == pSubp->subpicture_id) {
      pI810->surfaceAllocation[i] = 0;
      return;
    }
  }
  return;
}






