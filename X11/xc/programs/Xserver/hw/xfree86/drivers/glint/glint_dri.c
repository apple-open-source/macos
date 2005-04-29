/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/glint/glint_dri.c,v 1.38 2003/11/12 17:56:35 tsi Exp $ */
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
All Rights Reserved.

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
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,

TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Jens Owen <jens@tungstengraphics.com>
 *   Alan Hourihane <alanh@fairlite.demon.co.uk>
 *
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Priv.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "fb.h"

#include "miline.h"

#include "GL/glxtokens.h"

#include "glint_regs.h"
#include "glint.h"
#include "glint_dri.h"

static char GLINTKernelDriverName[] = "gamma";
static char GLINTClientDriverName[] = "gamma";

static void GLINTDestroyContext(ScreenPtr pScreen, drmContext hwContext,
                                DRIContextType contextStore);


static unsigned int mylog2( unsigned int n )
{
   unsigned int log2 = 1;
   while ( n > 1 ) n >>= 1, log2++;
   return log2;
}

static int 
GLINTDRIControlInit(int drmSubFD, int irq)
{
    int retcode;

    if ((retcode = drmCtlInstHandler(drmSubFD, irq))) return 1;
    return 0;
}

static Bool
GLINTInitVisualConfigs(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   GLINTPtr pGlint = GLINTPTR(pScrn);
   int numConfigs = 0;
   __GLXvisualConfig *pConfigs = NULL;
   GLINTConfigPrivPtr pGlintConfigs = NULL;
   GLINTConfigPrivPtr *pGlintConfigPtrs = NULL;
   int db, depth, stencil, accum;
   int i;

   switch ( pScrn->depth ) {
   case 15:
      numConfigs = 8;

      pConfigs = (__GLXvisualConfig*)xnfcalloc( sizeof(__GLXvisualConfig),
						numConfigs );
      if ( !pConfigs ) {
	 return FALSE;
      }

      pGlintConfigs = (GLINTConfigPrivPtr)xnfcalloc( sizeof(GLINTConfigPrivRec),
						 numConfigs );
      if ( !pGlintConfigs ) {
	 xfree( pConfigs );
	 return FALSE;
      }

      pGlintConfigPtrs = (GLINTConfigPrivPtr*)xnfcalloc( sizeof(GLINTConfigPrivPtr),
						     numConfigs );
      if ( !pGlintConfigPtrs ) {
	 xfree( pConfigs );
	 xfree( pGlintConfigs );
	 return FALSE;
      }

      for ( i = 0 ; i < numConfigs ; i++ ) {
	 pGlintConfigPtrs[i] = &pGlintConfigs[i];
      }

      i = 0;
      depth = 1;
      for ( accum = 0 ; accum <= 1 ; accum++ ) {
         for ( stencil = 0 ; stencil <= 1 ; stencil++ ) {
            for ( db = 1 ; db >= 0 ; db-- ) {
               pConfigs[i].vid			= -1;
               pConfigs[i].class		= -1;
               pConfigs[i].rgba			= TRUE;
               pConfigs[i].redSize		= 5;
               pConfigs[i].greenSize		= 5;
               pConfigs[i].blueSize		= 5;
               pConfigs[i].alphaSize		= 1;
               pConfigs[i].redMask		= 0x00007C00;
               pConfigs[i].greenMask		= 0x000003E0;
               pConfigs[i].blueMask		= 0x0000001F;
               pConfigs[i].alphaMask		= 0x00008000;
               if ( accum ) {
                  pConfigs[i].accumRedSize	= 16;
                  pConfigs[i].accumGreenSize	= 16;
                  pConfigs[i].accumBlueSize	= 16;
                  pConfigs[i].accumAlphaSize	= 0;
               } else {
                  pConfigs[i].accumRedSize	= 0;
                  pConfigs[i].accumGreenSize	= 0;
                  pConfigs[i].accumBlueSize	= 0;
                  pConfigs[i].accumAlphaSize	= 0;
               }
               if ( db ) {
                  pConfigs[i].doubleBuffer	= TRUE;
               } else {
                  pConfigs[i].doubleBuffer	= FALSE;
	       }
               pConfigs[i].stereo		= FALSE;
               pConfigs[i].bufferSize		= 20;
               if ( depth ) {
                  pConfigs[i].depthSize		= 16;
               } else {
                  pConfigs[i].depthSize		= 0;
	       }
               if ( stencil ) {
                  pConfigs[i].stencilSize	= 8;
               } else {
                  pConfigs[i].stencilSize	= 0;
	       }
               pConfigs[i].auxBuffers		= 0;
               pConfigs[i].level		= 0;
               if ( accum || stencil ) {
                  pConfigs[i].visualRating	= GLX_SLOW_VISUAL_EXT;
               } else {
                  pConfigs[i].visualRating	= GLX_NONE_EXT;
	       }
               pConfigs[i].transparentPixel	= 0;
               pConfigs[i].transparentRed	= 0;
               pConfigs[i].transparentGreen	= 0;
               pConfigs[i].transparentBlue	= 0;
               pConfigs[i].transparentAlpha	= 0;
               pConfigs[i].transparentIndex	= 0;
               i++;
            }
         }
      }
      if ( i != numConfigs ) {
         xf86DrvMsg( pScrn->scrnIndex, X_ERROR,
		     "[drm] Incorrect initialization of visuals\n" );
         return FALSE;
      }
      break;

   case 24:
      numConfigs = 8;

      pConfigs = (__GLXvisualConfig*)xnfcalloc( sizeof(__GLXvisualConfig),
						numConfigs );
      if ( !pConfigs ) {
	 return FALSE;
      }

      pGlintConfigs = (GLINTConfigPrivPtr)xnfcalloc( sizeof(GLINTConfigPrivRec),
						 numConfigs );
      if ( !pGlintConfigs ) {
	 xfree( pConfigs );
	 return FALSE;
      }

      pGlintConfigPtrs = (GLINTConfigPrivPtr*)xnfcalloc( sizeof(GLINTConfigPrivPtr),
						     numConfigs );
      if ( !pGlintConfigPtrs ) {
	 xfree( pConfigs );
	 xfree( pGlintConfigs );
	 return FALSE;
      }

      for ( i = 0 ; i < numConfigs ; i++ ) {
	 pGlintConfigPtrs[i] = &pGlintConfigs[i];
      }

      i = 0;
      for ( accum = 0 ; accum <= 1 ; accum++ ) {
         for ( depth = 0 ; depth <= 1 ; depth++ ) { /* and stencil */
            for ( db = 1 ; db >= 0 ; db-- ) {
               pConfigs[i].vid			= -1;
               pConfigs[i].class		= -1;
               pConfigs[i].rgba			= TRUE;
               pConfigs[i].redSize		= 8;
               pConfigs[i].greenSize		= 8;
               pConfigs[i].blueSize		= 8;
               pConfigs[i].alphaSize		= 0;
               pConfigs[i].redMask		= 0x00FF0000;
               pConfigs[i].greenMask		= 0x0000FF00;
               pConfigs[i].blueMask		= 0x000000FF;
               pConfigs[i].alphaMask		= 0;
               if ( accum ) {
                  pConfigs[i].accumRedSize	= 16;
                  pConfigs[i].accumGreenSize	= 16;
                  pConfigs[i].accumBlueSize	= 16;
                  pConfigs[i].accumAlphaSize	= 0;
               } else {
                  pConfigs[i].accumRedSize	= 0;
                  pConfigs[i].accumGreenSize	= 0;
                  pConfigs[i].accumBlueSize	= 0;
                  pConfigs[i].accumAlphaSize	= 0;
               }
               if ( db ) {
                  pConfigs[i].doubleBuffer	= TRUE;
               } else {
                  pConfigs[i].doubleBuffer	= FALSE;
	       }
               pConfigs[i].stereo		= FALSE;
               pConfigs[i].bufferSize		= 24;
               if ( depth ) {
		     pConfigs[i].depthSize	= 16;
                     pConfigs[i].stencilSize	= 8;
               }
               else {
                     pConfigs[i].depthSize	= 0;
                     pConfigs[i].stencilSize	= 0;
               }
               pConfigs[i].auxBuffers		= 0;
               pConfigs[i].level		= 0;
               if ( accum ) {
                  pConfigs[i].visualRating	= GLX_SLOW_VISUAL_EXT;
               } else {
                  pConfigs[i].visualRating	= GLX_NONE_EXT;
	       }
               pConfigs[i].transparentPixel	= 0;
               pConfigs[i].transparentRed	= 0;
               pConfigs[i].transparentGreen	= 0;
               pConfigs[i].transparentBlue	= 0;
               pConfigs[i].transparentAlpha	= 0;
               pConfigs[i].transparentIndex	= 0;
               i++;
            }
         }
      }
      if ( i != numConfigs ) {
         xf86DrvMsg( pScrn->scrnIndex, X_ERROR,
		     "[drm] Incorrect initialization of visuals\n" );
         return FALSE;
      }
      break;

   default: /* Can't do depth 8 or 16, just 15 or 24 */
      return FALSE;
      break;
   }

   pGlint->numVisualConfigs = numConfigs;
   pGlint->pVisualConfigs = pConfigs;
   pGlint->pVisualConfigsPriv = pGlintConfigs;
   GlxSetVisualConfigs(numConfigs, pConfigs, (void **)pGlintConfigPtrs);

   return TRUE;
}

static Bool GLINTDRIAgpInit(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   GLINTPtr pGlint = GLINTPTR(pScrn);
   int ret, count;
   CARD32 mode;

   /* FIXME: Make these configurable...
    */
   pGlint->agp.size = 2 * 1024 * 1024;
   pGlint->buffers.offset = 0;
   pGlint->buffers.size = GLINT_DRI_BUF_COUNT * GLINT_DRI_BUF_SIZE;

   if ( drmAgpAcquire( pGlint->drmSubFD ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR, "[agp] AGP not available\n" );
      return FALSE;
   }

   /* Read AGP Capabilities */
   mode = drmAgpGetMode(pGlint->drmSubFD) & ~0x03; /* Mask Host capabilities */

   mode |= 0x01; /* Gamma only supports AGP 1x */

   /* Now enable AGP only on the specified BusID */
   if ( drmAgpEnable( pGlint->drmSubFD, mode ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR, "[agp] AGP not enabled\n" );
      drmAgpRelease( pGlint->drmSubFD );
      return FALSE;
   }

   ret = drmAgpAlloc( pGlint->drmSubFD, pGlint->agp.size, 0, NULL, 
		      &pGlint->agp.handle);

   if ( ret < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR, "[agp] Out of memory (%d)\n", ret );
      drmAgpRelease( pGlint->drmSubFD );
      return FALSE;
   }
   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[agp] %d kB allocated with handle 0x%08lx\n",
	       pGlint->agp.size/1024, pGlint->agp.handle );

   if ( drmAgpBind( pGlint->drmSubFD, pGlint->agp.handle, 0 ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR, "[agp] Could not bind memory\n" );
      drmAgpFree( pGlint->drmSubFD, pGlint->agp.handle );
      drmAgpRelease( pGlint->drmSubFD );
      return FALSE;
   }

   /* DMA buffers
    */
   if ( drmAddMap( pGlint->drmSubFD, pGlint->buffers.offset, 
		   pGlint->buffers.size, DRM_AGP, 0,
		   &pGlint->buffers.handle ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR,
		  "[agp] Could not add DMA buffers mapping\n" );
      return FALSE;
   }
   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[agp] DMA buffers handle = 0x%08lx\n",
	       pGlint->buffers.handle );

   if ( drmMap( pGlint->drmSubFD, pGlint->buffers.handle, 
		pGlint->buffers.size, &pGlint->buffers.map ) < 0 ) {
      xf86DrvMsg( pScreen->myNum, X_ERROR,
		  "[agp] Could not map DMA buffers\n" );
      return FALSE;
   }
   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[agp] DMA buffers mapped at %p\n", pGlint->buffers.map);

   count = drmAddBufs( pGlint->drmSubFD,
		       GLINT_DRI_BUF_COUNT, GLINT_DRI_BUF_SIZE,
		       DRM_AGP_BUFFER, pGlint->buffers.offset );
   if ( count <= 0 ) {
      xf86DrvMsg( pScrn->scrnIndex, X_INFO,
		  "[drm] failure adding %d %d byte DMA buffers\n",
		  GLINT_DRI_BUF_COUNT, GLINT_DRI_BUF_SIZE );
      return FALSE;
   }
   xf86DrvMsg( pScreen->myNum, X_INFO,
	       "[drm] Added %d %d byte DMA buffers\n",
	       count, GLINT_DRI_BUF_SIZE );

    {
	int bufs;
	
	if ((bufs = drmAddBufs(pGlint->drmSubFD,
			       1,
			       8192, /* 8K = 8MB physical memory */
			       0,
			       DRM_RESTRICTED /* flags */)) <= 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "[drm] 0x%x failure adding page table buffer\n",bufs);
	    DRICloseScreen(pScreen);
	    return FALSE;
	}
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "[drm] added 1 page table buffer\n");
    }

    pGlint->PCIMode = FALSE;

    return TRUE;
}

static Bool GLINTDRIKernelInit( ScreenPtr pScreen )
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   GLINTPtr pGlint = GLINTPTR(pScrn);
   DRIInfoPtr pDRIInfo = pGlint->pDRIInfo;
   GLINTDRIPtr pGlintDRI = pDRIInfo->devPrivate;
   drmGAMMAInit init;
   int ret;

   memset( &init, 0, sizeof(drmGAMMAInit) );

   init.func = GAMMA_INIT_DMA;
   init.sarea_priv_offset = sizeof(XF86DRISAREARec);

   init.mmio0 = pGlintDRI->registers0.handle;
   init.mmio1 = pGlintDRI->registers1.handle;
   init.mmio2 = pGlintDRI->registers2.handle;
   init.mmio3 = pGlintDRI->registers3.handle;
   init.num_rast = pGlint->numMultiDevices;

   if (!pGlint->PCIMode) {
       init.pcimode = 0;
       init.buffers_offset = pGlint->buffers.handle;
   } else {
       init.pcimode = 1;
   }

   ret = drmCommandWrite( pGlint->drmSubFD, DRM_GAMMA_INIT, 
                          &init, sizeof(drmGAMMAInit) );

   if ( ret < 0 ) {
      xf86DrvMsg( pScrn->scrnIndex, X_ERROR,
		  "[drm] Failed to initialize DMA! (%d)\n", ret );
      return FALSE;
   }

   return TRUE;
}

Bool
GLINTDRIScreenInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    GLINTPtr pGlint = GLINTPTR(pScrn);
    DRIInfoPtr pDRIInfo;
    GLINTDRIPtr pGlintDRI;
    int dmabufs = 0;

    /* Check that the GLX, DRI, and DRM modules have been loaded by testing
       for canonical symbols in each module. */
    if (!xf86LoaderCheckSymbol("GlxSetVisualConfigs")) return FALSE;
    if (!xf86LoaderCheckSymbol("DRIScreenInit"))       return FALSE;
    if (!xf86LoaderCheckSymbol("drmAvailable"))        return FALSE;
    if (!xf86LoaderCheckSymbol("DRIQueryVersion")) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
                 "[dri] GLINTDRIScreenInit failed (libdri.a too old)\n");
      return FALSE;
    }

    /* Check the DRI version */
    {
       int major, minor, patch;
       DRIQueryVersion(&major, &minor, &patch);
       if (major != 4 || minor < 0) {
          xf86DrvMsg(pScreen->myNum, X_ERROR,
                     "[dri] GLINTDRIScreenInit failed because of a version mismatch.\n"
                     "[dri] libDRI version is %d.%d.%d but version 4.0.x is needed.\n"
                     "[dri] Disabling DRI.\n",
                     major, minor, patch);
          return FALSE;
       }
    }

    if (pGlint->Chipset != PCI_VENDOR_3DLABS_CHIP_GAMMA) return FALSE;

    if (pGlint->numMultiDevices > 2) return FALSE;

    if (pGlint->MultiChip != PCI_CHIP_MX) return FALSE;

    pDRIInfo = DRICreateInfoRec();
    if(pDRIInfo == NULL)
	return FALSE;
    
    pGlint->pDRIInfo = pDRIInfo;

    /* setup device info */
    pDRIInfo->drmDriverName = GLINTKernelDriverName;
    pDRIInfo->clientDriverName = GLINTClientDriverName;
    pDRIInfo->busIdString = xalloc(64); /* Freed in DRIDestroyInfoRec */
    sprintf(pDRIInfo->busIdString, "PCI:%d:%d:%d",
	    ((pciConfigPtr)pGlint->PciInfo->thisCard)->busnum,
	    ((pciConfigPtr)pGlint->PciInfo->thisCard)->devnum,
	    ((pciConfigPtr)pGlint->PciInfo->thisCard)->funcnum);
    pDRIInfo->ddxDriverMajorVersion = GLINT_MAJOR_VERSION;
    pDRIInfo->ddxDriverMinorVersion = GLINT_MINOR_VERSION;
    pDRIInfo->ddxDriverPatchVersion = GLINT_PATCHLEVEL;
    pDRIInfo->frameBufferPhysicalAddress = pGlint->FbAddress;
    pDRIInfo->frameBufferSize = pGlint->FbMapSize;
    pDRIInfo->frameBufferStride = 
	    pScrn->displayWidth * (pScrn->bitsPerPixel >> 3);
    pDRIInfo->ddxDrawableTableEntry = GLINT_MAX_DRAWABLES;

    /* MAX_DRAWABLES set to number of GID's minus one for DDX */
    if (SAREA_MAX_DRAWABLES < GLINT_MAX_DRAWABLES) {
	pDRIInfo->maxDrawableTableEntry = SAREA_MAX_DRAWABLES;
    }
    else {
	pDRIInfo->maxDrawableTableEntry = GLINT_MAX_DRAWABLES;
    }
    
#ifdef NOT_DONE
    /* FIXME need to extend DRI protocol to pass this size back to client 
     * for SAREA mapping that includes a device private record
     */
    pDRIInfo->SAREASize = 
	((sizeof(XF86DRISAREARec) + 0xfff) & 0x1000); /* round to page */
	/* + shared memory device private rec */
#else
    /* For now the mapping works by using a fixed size defined
     * in the SAREA header
     */
    pDRIInfo->SAREASize = SAREA_MAX;
#endif

    if (!(pGlintDRI = (GLINTDRIPtr)xcalloc(sizeof(GLINTDRIRec),1))) {
	DRIDestroyInfoRec(pGlint->pDRIInfo);
	return FALSE;
    }

    /* setup visual configurations */
    if (!(GLINTInitVisualConfigs(pScreen))) {
	DRICloseScreen(pScreen);
	return FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "visual configs initialized\n" );

    pDRIInfo->devPrivate     = pGlintDRI;
    pDRIInfo->devPrivateSize = sizeof(GLINTDRIRec);
    pDRIInfo->contextSize    = sizeof(GLINTDRIContextRec);

    /* setup call backs */
    pDRIInfo->CreateContext  = GLINTCreateContext;
    pDRIInfo->DestroyContext = GLINTDestroyContext;
    pDRIInfo->SwapContext    = GLINTDRISwapContext;
    pDRIInfo->InitBuffers    = GLINTDRIInitBuffers;
    pDRIInfo->MoveBuffers    = GLINTDRIMoveBuffers;
    pDRIInfo->bufferRequests = DRI_ALL_WINDOWS;

    pDRIInfo->createDummyCtx     = TRUE;
    pDRIInfo->createDummyCtxPriv = FALSE;

    /* So DRICloseScreen does the right thing if we abort */
    pGlint->buffers.map = 0;
    pGlint->agp.handle = DRM_AGP_NO_HANDLE;

    if (!DRIScreenInit(pScreen, pDRIInfo, &(pGlint->drmSubFD))) {
	DRIDestroyInfoRec(pGlint->pDRIInfo);
	xfree(pGlintDRI);
	return FALSE;
    }

    /* Check the DRM versioning */
    {
        drmVersionPtr version;

	/* Check the DRM lib version.
	   drmGetLibVersion was not supported in version 1.0, so check for
	   symbol first to avoid possible crash or hang.
	 */
	if (xf86LoaderCheckSymbol("drmGetLibVersion")) {
	    version = drmGetLibVersion(pGlint->drmSubFD);
	}
	else {
	    /* drmlib version 1.0.0 didn't have the drmGetLibVersion
	       entry point.  Fake it by allocating a version record
	       via drmGetVersion and changing it to version 1.0.0
	     */
	    version = drmGetVersion(pGlint->drmSubFD);
	    version->version_major      = 1;
	    version->version_minor      = 0;
	    version->version_patchlevel = 0;
	}

	if (version) {
	    if (version->version_major != 1 ||
		version->version_minor < 1) {
		/* incompatible drm library version */
		xf86DrvMsg(pScreen->myNum, X_ERROR,
		    "[dri] GLINTDRIScreenInit failed because of a version mismatch.\n"
		    "[dri] libdrm.a module version is %d.%d.%d but version 1.1.x is needed.\n"
		    "[dri] Disabling DRI.\n",
		    version->version_major,
		    version->version_minor,
		    version->version_patchlevel);
		drmFreeVersion(version);
		GLINTDRICloseScreen(pScreen);
		return FALSE;
	    }
	    drmFreeVersion(version);
	}

        /* Check the GLINT DRM version */
        version = drmGetVersion(pGlint->drmSubFD);
        if (version) {
            if (version->version_major != 2 ||
                version->version_minor < 0) {
                /* incompatible drm version */
                xf86DrvMsg(pScreen->myNum, X_ERROR,
                    "[dri] GLINTDRIScreenInit failed because of a version mismatch.\n"
                    "[dri] gamma.o kernel module version is %d.%d.%d but version 2.0.x is needed.\n"
                    "[dri] Disabling DRI.\n",
                    version->version_major,
                    version->version_minor,
                    version->version_patchlevel);
                GLINTDRICloseScreen(pScreen);
                drmFreeVersion(version);
                return FALSE;
            }
            drmFreeVersion(version);
        }
    }

    /* Tell the client driver how many MX's we have */
    pGlintDRI->numMultiDevices = pGlint->numMultiDevices;
    /* Tell the client about our screen size setup */
    pGlintDRI->pprod = pGlint->pprod;

    pGlintDRI->cpp = pScrn->bitsPerPixel / 8;
    pGlintDRI->frontPitch = pScrn->displayWidth;
    pGlintDRI->frontOffset = 0;

    pGlintDRI->textureSize = 32 * 1024 * 1024;
    pGlintDRI->logTextureGranularity = 
		mylog2( pGlintDRI->textureSize / GAMMA_NR_TEX_REGIONS );
   
    /* setup device specific direct rendering memory maps */

    /* pci region 0: control regs, first 4k page, priveledged writes */
    pGlintDRI->registers0.size = 0x1000;
    if (drmAddMap( pGlint->drmSubFD,
		   (drmHandle)pGlint->IOAddress,
		   pGlintDRI->registers0.size,
		   DRM_REGISTERS, DRM_READ_ONLY,
		   &pGlintDRI->registers0.handle) < 0)
    {
	DRICloseScreen(pScreen);
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO, 
	       "[drm] Register handle 0 = 0x%08lx\n",
	       pGlintDRI->registers0.handle);

    /* pci region 0: control regs, following region, client access */
    pGlintDRI->registers1.size = 0xf000;
    if (drmAddMap( pGlint->drmSubFD,
		   (drmHandle)(pGlint->IOAddress + 0x1000),
		   pGlintDRI->registers1.size,
		   DRM_REGISTERS, 0,
		   &pGlintDRI->registers1.handle) < 0)
    {
	DRICloseScreen(pScreen);
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO, 
	       "[drm] Register handle 1 = 0x%08lx\n",
	       pGlintDRI->registers1.handle);

    /* pci region 0: control regs, second MX, first 4k page */
    pGlintDRI->registers2.size = 0x1000;
    if (drmAddMap( pGlint->drmSubFD,
		   (drmHandle)(pGlint->IOAddress + 0x10000),
		   pGlintDRI->registers2.size,
		   DRM_REGISTERS, DRM_READ_ONLY,
		   &pGlintDRI->registers2.handle) < 0)
    {
	DRICloseScreen(pScreen);
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO, 
	       "[drm] Register handle 2 = 0x%08lx\n",
	       pGlintDRI->registers2.handle);

    /* pci region 0: control regs, second MX, following region */
    pGlintDRI->registers3.size = 0xf000;
    if (drmAddMap( pGlint->drmSubFD,
		   (drmHandle)(pGlint->IOAddress + 0x11000),
		   pGlintDRI->registers3.size,
		   DRM_REGISTERS, 0,
		   &pGlintDRI->registers3.handle) < 0)
    {
	DRICloseScreen(pScreen);
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO, 
	       "[drm] Register handle 3 = 0x%08lx\n",
	       pGlintDRI->registers3.handle);

    /* setup DMA buffers */

    /* TRY AGP */
    if ( !GLINTDRIAgpInit( pScreen ) ) {
        /* OUCH, NO AGP, TRY PCI */
        pGlint->PCIMode = TRUE;
        if (xf86ConfigDRI.bufs_count) {
	    int i;
	    int bufs;
	
	    for (i = 0; i < xf86ConfigDRI.bufs_count; i++) {
	    	if ((bufs = drmAddBufs(pGlint->drmSubFD,
				   xf86ConfigDRI.bufs[i].count,
				   xf86ConfigDRI.bufs[i].size,
				   0,
				   0 /* flags */)) <= 0) {
		    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
			   "[drm] failure adding %d %d byte DMA buffers\n",
			   xf86ConfigDRI.bufs[i].count,
			   xf86ConfigDRI.bufs[i].size);
	    	} else {
		    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
			   "[drm] added %d %d byte DMA buffers\n",
			   bufs, xf86ConfigDRI.bufs[i].size);
		    dmabufs += bufs;
	    	}
	    }
    	}

    	if (dmabufs <= 0) {
	    int bufs;
	
	    if ((bufs = drmAddBufs(pGlint->drmSubFD,
			       GLINT_DRI_BUF_COUNT,
			       GLINT_DRI_BUF_SIZE,
			       0,
			       0 /* flags */)) <= 0) {
	    	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "[drm] failure adding %d %d byte DMA buffers\n",
		       GLINT_DRI_BUF_COUNT,
		       GLINT_DRI_BUF_SIZE);
	    	DRICloseScreen(pScreen);
	    	return FALSE;
	    }
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "[drm] added %d %d byte DMA buffers\n",
		   bufs, GLINT_DRI_BUF_SIZE);
	
	    if ((bufs = drmAddBufs(pGlint->drmSubFD,
			       1,
			       8192, /* 8K = 8MB physical memory */
			       0,
			       DRM_RESTRICTED /* flags */)) <= 0) {
	    	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "[drm] 0x%x failure adding page table buffer\n",bufs);
	    	DRICloseScreen(pScreen);
	    	return FALSE;
	    }
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "[drm] added 1 page table buffer\n");
    	}

    	if (!(pGlint->drmBufs = drmMapBufs(pGlint->drmSubFD))) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "[drm] failure mapping DMA buffers\n");
	    DRICloseScreen(pScreen);
	    return FALSE;
    	}
    	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[drm] buffers mapped with %p\n",
	       (void *)pGlint->drmBufs);
    	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[drm] %d DMA buffers mapped\n",
	       pGlint->drmBufs->count);
    } /* PCIMODE */

    if (pGlint->irq <= 0) {
	pGlint->irq = drmGetInterruptFromBusID(pGlint->drmSubFD,
					       ((pciConfigPtr)pGlint->PciInfo
						->thisCard)->busnum,
					       ((pciConfigPtr)pGlint->PciInfo
						->thisCard)->devnum,
					       ((pciConfigPtr)pGlint->PciInfo
						->thisCard)->funcnum);
    }
    
    return TRUE;
}

void
GLINTDRICloseScreen(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    GLINTPtr pGlint = GLINTPTR(pScrn);

    if (pGlint->buffers.map) {
	drmUnmap( pGlint->buffers.map, pGlint->buffers.size);
	pGlint->buffers.map = NULL;
    }

    if (pGlint->agp.handle != DRM_AGP_NO_HANDLE) {
	drmAgpUnbind( pGlint->drmSubFD, pGlint->agp.handle );
	drmAgpFree( pGlint->drmSubFD, pGlint->agp.handle );
	pGlint->agp.handle = 0;
	drmAgpRelease( pGlint->drmSubFD );
    }

    if (pGlint->drmBufs) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "[drm] unmapping %d buffers\n",
		   pGlint->drmBufs->count);
        if (drmUnmapBufs(pGlint->drmBufs)) {
	    xf86DrvMsg(pScreen->myNum, X_INFO, 
		       "[drm] unable to unmap DMA buffers\n");
	}

    }

    DRICloseScreen(pScreen);
	    
    if (pGlint->pDRIInfo) {
	if (pGlint->pDRIInfo->devPrivate) {
	    xfree(pGlint->pDRIInfo->devPrivate);
	}
	DRIDestroyInfoRec(pGlint->pDRIInfo);
    }

    if (pGlint->pVisualConfigs) xfree(pGlint->pVisualConfigs);
    if (pGlint->pVisualConfigsPriv) xfree(pGlint->pVisualConfigsPriv);
}

Bool
GLINTCreateContext(ScreenPtr pScreen,
                   VisualPtr visual,
                   drmContext hwContext,
                   void *pVisualConfigPriv,
		   DRIContextType contextStore)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    GLINTPtr pGlint = GLINTPTR(pScrn);
    GLINTConfigPrivPtr pGlintConfig = (GLINTConfigPrivPtr)pVisualConfigPriv;
    
    /* These are really assertions rather than necessary logic,
       just using this to exercise device private region until really needed */

    if (!pGlintConfig) 
	return TRUE; /* no GLX driver private support, yet */

#if 0
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "[GLINTCreateContext] context priv index = %d\n",
	       pGlintConfig->index);
#endif

    if (pGlintConfig->index >= pGlint->numVisualConfigs)
	return FALSE;

    if (pGlint->pVisualConfigs[pGlintConfig->index].redMask != visual->redMask)
	return FALSE;

    return TRUE;
}

static void
GLINTDestroyContext(ScreenPtr pScreen,
                    drmContext hwContext,
                    DRIContextType contextStore)
{
}

Bool
GLINTDRIFinishScreenInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    GLINTPtr pGlint = GLINTPTR(pScrn);
    DRIInfoPtr pDRIInfo = pGlint->pDRIInfo;
    GLINTDRIPtr pGlintDRI = pDRIInfo->devPrivate;
    FBAreaPtr fbarea;

    /* 
     * Setup one of 4 types of context swap handling methods
     *
     * Option A: HIDE X CONTEXT SWAPS.  X contexts will be flagged as
     * preserved by the server.  The kernel will never swap a preserved
     * context.  The kernel will call back into the server for swapping
     * all other contexts.
     *
     *  pGlint->pDRIInfo->driverSwapMethod = DRI_HIDE_X_CONTEXT;
     *
     *
     * Option B: SERVER SIDE CONTEXT SWAPS.  X contexts will be flagged
     * as 2D contexts, but the flag will be ignored by the kernel.  The
     * preserved flag will not be used, so the kernel will call back into
     * the server for swapping all contexts.  The 2D flag will be used by
     * the server to optimize for 2D/3D switching between a single 3D 
     * context and the servers 2D context.
     *
     *  pGlint->pDRIInfo->driverSwapMethod = DRI_SERVER_SWAP;
     *
     *
     * Option C: KERNEL SIDE GENERIC SWAPS.  X server will provide generic
     * kernel driver with byte codes for performing swap.  X contexts will
     * be flagged as 2D contexts, so 2D/3D switching optimizations can still
     * be done.  This is not supported, yet.  Additional work is required
     * to support generic kernel driver and provide byte code examples.
     *
     *  pGlint->pDRIInfo->driverSwapMethod = DRI_KERNEL_SWAP;
     * 
     * add byte codes for context swap here:
     *  drmCtlAddCommand(drmSubFD, ...
     * 
     * 
     * Option D: KERNEL SIDE DEVICE SPECIFIC SWAPS.  DrmSubdriver will
     * have device specific code for handling swaps.  X context will be
     * flagged as 2D contexts, so 2D/3D switching optimizations can still
     * be done.  This is not supported by gamma driver, yet; however, the 
     * framework is in place to use this option.
     * 
     *  pGlint->pDRIInfo->driverSwapMethod = DRI_KERNEL_SWAP;
     */ 

    pGlint->pDRIInfo->driverSwapMethod = DRI_HIDE_X_CONTEXT;

				/* Allocate the shared back buffer */
    if ((fbarea = xf86AllocateOffscreenArea(pScreen,
						pScrn->virtualX,
						pScrn->virtualY,
						32, NULL, NULL, NULL))) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Reserved back buffer from (%d,%d) to (%d,%d)\n",
		       fbarea->box.x1, fbarea->box.y1,
		       fbarea->box.x2, fbarea->box.y2);

    	pGlintDRI->backPitch = pScrn->displayWidth;
        pGlintDRI->backOffset = (fbarea->box.y1 * pScrn->displayWidth * 
				pScrn->bitsPerPixel / 8) +
				(fbarea->box.x1 * pScrn->bitsPerPixel / 8);
	pGlintDRI->backX = fbarea->box.x1;
	pGlintDRI->backY = fbarea->box.y1;
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Unable to reserve back buffer\n");
    	pGlintDRI->backPitch = -1;
        pGlintDRI->backOffset = -1;
    }

    if (!DRIFinishScreenInit(pScreen)) {
	DRICloseScreen(pScreen);
	return FALSE;
    }

    if (!GLINTDRIKernelInit(pScreen)) {
	DRICloseScreen(pScreen);
	return FALSE;
    }

   if ( (pGlint->irq <= 0) || 
	      GLINTDRIControlInit(pGlint->drmSubFD, pGlint->irq) ) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	   "[drm] cannot initialize dma with IRQ %d\n",
	   pGlint->irq);
	DRICloseScreen(pScreen);
	return FALSE;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "[drm] dma control initialized, using IRQ %d\n",
	       pGlint->irq);

    if (!pGlint->PCIMode) {
    	if (!(pGlint->drmBufs = drmMapBufs(pGlint->drmSubFD))) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   	"[drm] failure mapping DMA buffers\n");
		DRICloseScreen(pScreen);
		return FALSE;
    	}
    	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[agp] buffers mapped with %p\n",
	       (void *)pGlint->drmBufs);
    	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[agp] %d DMA buffers mapped\n",
	       pGlint->drmBufs->count);
    }

#if 0
    /* Get the X server's context */
    pGlint->DRIctx = DRIGetContext(pScreen);
    pGlint->buf2D  = pGlint->drmBufs->list[GLINT_DRI_BUF_COUNT + 1].address;
#endif

    return TRUE;
}

#define ContextDump_tag 	0x1b8
#define ContextRestore_tag 	0x1b9
#define ContextData_tag 	0x1ba

void
GLINTDRISwapContext(
    ScreenPtr pScreen,
    DRISyncType syncType,
    DRIContextType readContextType,
    void *readContextStore,
    DRIContextType writeContextType,
    void *writeContextStore)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    GLINTPtr pGlint = GLINTPTR(pScrn);
    GLINTDRIContextPtr pRC = (GLINTDRIContextPtr)readContextStore; 
    GLINTDRIContextPtr pWC = (GLINTDRIContextPtr)writeContextStore; 
    int	dumpIndex;
    CARD32 readValue;

    /* Sync here covers both read and write context of contexts */
    if (pGlint->AccelInfoRec->Sync) {
	(*pGlint->AccelInfoRec->Sync)(pGlint->AccelInfoRec->pScrn);
    }
    pGlint->AccelInfoRec->NeedToSync = FALSE;

    if (readContextType != DRI_NO_CONTEXT) {

	/* save the 2D portion of the old context */
	pRC->MX1.CStartXDom               = GLINT_READ_REG(StartXDom);
	pRC->MX1.CdXDom                   = GLINT_READ_REG(dXDom);
	pRC->MX1.CStartXSub               = GLINT_READ_REG(StartXSub);
	pRC->MX1.CdXSub                   = GLINT_READ_REG(dXSub);
	pRC->MX1.CStartY                  = GLINT_READ_REG(StartY);
	pRC->MX1.CdY                      = GLINT_READ_REG(dY);
	pRC->MX1.CGLINTCount              = GLINT_READ_REG(GLINTCount);
	pRC->MX1.CPointTable0             = GLINT_READ_REG(PointTable0);
	pRC->MX1.CPointTable1             = GLINT_READ_REG(PointTable1);
	pRC->MX1.CPointTable2             = GLINT_READ_REG(PointTable2);
	pRC->MX1.CPointTable3             = GLINT_READ_REG(PointTable3);
	pRC->MX1.CRasterizerMode          = GLINT_READ_REG(RasterizerMode);
	pRC->MX1.CYLimits                 = GLINT_READ_REG(YLimits);
	pRC->MX1.CScanLineOwnership       = GLINT_READ_REG(ScanLineOwnership);
	pRC->MX1.CPixelSize               = GLINT_READ_REG(PixelSize);
	pRC->MX1.CScissorMode             = GLINT_READ_REG(ScissorMode);
	pRC->MX1.CScissorMinXY            = GLINT_READ_REG(ScissorMinXY);
	pRC->MX1.CScissorMaxXY            = GLINT_READ_REG(ScissorMaxXY);
	pRC->MX1.CScreenSize              = GLINT_READ_REG(ScreenSize);
	pRC->MX1.CAreaStippleMode         = GLINT_READ_REG(AreaStippleMode);
	pRC->MX1.CLineStippleMode         = GLINT_READ_REG(LineStippleMode);
	pRC->MX1.CLoadLineStippleCounters = GLINT_READ_REG(LoadLineStippleCounters);
	pRC->MX1.CWindowOrigin            = GLINT_READ_REG(WindowOrigin);
	pRC->MX1.CRouterMode              = GLINT_READ_REG(RouterMode);
	pRC->MX1.CTextureAddressMode      = GLINT_READ_REG(TextureAddressMode);
	pRC->MX1.CTextureReadMode         = GLINT_READ_REG(TextureReadMode);
	pRC->MX1.CTextureColorMode        = GLINT_READ_REG(TextureColorMode);
	pRC->MX1.CFogMode                 = GLINT_READ_REG(FogMode);
	pRC->MX1.CColorDDAMode            = GLINT_READ_REG(ColorDDAMode);
	pRC->MX1.CGLINTColor              = GLINT_READ_REG(GLINTColor);
	pRC->MX1.CAlphaTestMode           = GLINT_READ_REG(AlphaTestMode);
	pRC->MX1.CAntialiasMode           = GLINT_READ_REG(AntialiasMode);
	pRC->MX1.CAlphaBlendMode          = GLINT_READ_REG(AlphaBlendMode);
	pRC->MX1.CDitherMode              = GLINT_READ_REG(DitherMode);
	pRC->MX1.CFBSoftwareWriteMask     = GLINT_READ_REG(FBSoftwareWriteMask);
	pRC->MX1.CLogicalOpMode           = GLINT_READ_REG(LogicalOpMode);
	pRC->MX1.CFBWriteData             = GLINT_READ_REG(FBWriteData);
	pRC->MX1.CLBReadMode              = GLINT_READ_REG(LBReadMode);
	pRC->MX1.CLBSourceOffset          = GLINT_READ_REG(LBSourceOffset);
	pRC->MX1.CLBWindowBase            = GLINT_READ_REG(LBWindowBase);
	pRC->MX1.CLBWriteMode             = GLINT_READ_REG(LBWriteMode);
	pRC->MX1.CTextureDownloadOffset   = GLINT_READ_REG(TextureDownloadOffset);
	pRC->MX1.CLBWindowOffset          = GLINT_READ_REG(LBWindowOffset);
	pRC->MX1.CGLINTWindow             = GLINT_READ_REG(GLINTWindow);
	pRC->MX1.CStencilMode             = GLINT_READ_REG(StencilMode);
	pRC->MX1.CDepthMode               = GLINT_READ_REG(DepthMode);
	pRC->MX1.CGLINTDepth              = GLINT_READ_REG(GLINTDepth);
	pRC->MX1.CFBReadMode              = GLINT_READ_REG(FBReadMode);
	pRC->MX1.CFBSourceOffset          = GLINT_READ_REG(FBSourceOffset);
	pRC->MX1.CFBPixelOffset           = GLINT_READ_REG(FBPixelOffset);
	pRC->MX1.CFBWindowBase            = GLINT_READ_REG(FBWindowBase);
	pRC->MX1.CFBWriteMode             = GLINT_READ_REG(FBWriteMode);
	pRC->MX1.CFBHardwareWriteMask     = GLINT_READ_REG(FBHardwareWriteMask);
	pRC->MX1.CFBBlockColor            = GLINT_READ_REG(FBBlockColor);
	pRC->MX1.CPatternRamMode          = GLINT_READ_REG(PatternRamMode);
	pRC->MX1.CFBBlockColorU           = GLINT_READ_REG(FBBlockColorU);
	pRC->MX1.CFBBlockColorL           = GLINT_READ_REG(FBBlockColorL);
	pRC->MX1.CFilterMode              = GLINT_READ_REG(FilterMode);
	pRC->MX1.CStatisticMode           = GLINT_READ_REG(StatisticMode);

        if (pGlint->numMultiDevices == 2) {
	pRC->MX1.CBroadcastMask           = GLINT_READ_REG(BroadcastMask);

	ACCESSCHIP2();
	pRC->MX2.CStartXDom               = GLINT_READ_REG(StartXDom);
	pRC->MX2.CdXDom                   = GLINT_READ_REG(dXDom);
	pRC->MX2.CStartXSub               = GLINT_READ_REG(StartXSub);
	pRC->MX2.CdXSub                   = GLINT_READ_REG(dXSub);
	pRC->MX2.CStartY                  = GLINT_READ_REG(StartY);
	pRC->MX2.CdY                      = GLINT_READ_REG(dY);
	pRC->MX2.CGLINTCount              = GLINT_READ_REG(GLINTCount);
	pRC->MX2.CPointTable0             = GLINT_READ_REG(PointTable0);
	pRC->MX2.CPointTable1             = GLINT_READ_REG(PointTable1);
	pRC->MX2.CPointTable2             = GLINT_READ_REG(PointTable2);
	pRC->MX2.CPointTable3             = GLINT_READ_REG(PointTable3);
	pRC->MX2.CRasterizerMode          = GLINT_READ_REG(RasterizerMode);
	pRC->MX2.CYLimits                 = GLINT_READ_REG(YLimits);
	pRC->MX2.CScanLineOwnership       = GLINT_READ_REG(ScanLineOwnership);
	pRC->MX2.CPixelSize               = GLINT_READ_REG(PixelSize);
	pRC->MX2.CScissorMode             = GLINT_READ_REG(ScissorMode);
	pRC->MX2.CScissorMinXY            = GLINT_READ_REG(ScissorMinXY);
	pRC->MX2.CScissorMaxXY            = GLINT_READ_REG(ScissorMaxXY);
	pRC->MX2.CScreenSize              = GLINT_READ_REG(ScreenSize);
	pRC->MX2.CAreaStippleMode         = GLINT_READ_REG(AreaStippleMode);
	pRC->MX2.CLineStippleMode         = GLINT_READ_REG(LineStippleMode);
	pRC->MX2.CLoadLineStippleCounters = GLINT_READ_REG(LoadLineStippleCounters);
	pRC->MX2.CWindowOrigin            = GLINT_READ_REG(WindowOrigin);
	pRC->MX2.CRouterMode              = GLINT_READ_REG(RouterMode);
	pRC->MX2.CTextureAddressMode      = GLINT_READ_REG(TextureAddressMode);
	pRC->MX2.CTextureReadMode         = GLINT_READ_REG(TextureReadMode);
	pRC->MX2.CTextureColorMode        = GLINT_READ_REG(TextureColorMode);
	pRC->MX2.CFogMode                 = GLINT_READ_REG(FogMode);
	pRC->MX2.CColorDDAMode            = GLINT_READ_REG(ColorDDAMode);
	pRC->MX2.CGLINTColor              = GLINT_READ_REG(GLINTColor);
	pRC->MX2.CAlphaTestMode           = GLINT_READ_REG(AlphaTestMode);
	pRC->MX2.CAntialiasMode           = GLINT_READ_REG(AntialiasMode);
	pRC->MX2.CAlphaBlendMode          = GLINT_READ_REG(AlphaBlendMode);
	pRC->MX2.CDitherMode              = GLINT_READ_REG(DitherMode);
	pRC->MX2.CFBSoftwareWriteMask     = GLINT_READ_REG(FBSoftwareWriteMask);
	pRC->MX2.CLogicalOpMode           = GLINT_READ_REG(LogicalOpMode);
	pRC->MX2.CFBWriteData             = GLINT_READ_REG(FBWriteData);
	pRC->MX2.CLBReadMode              = GLINT_READ_REG(LBReadMode);
	pRC->MX2.CLBSourceOffset          = GLINT_READ_REG(LBSourceOffset);
	pRC->MX2.CLBWindowBase            = GLINT_READ_REG(LBWindowBase);
	pRC->MX2.CLBWriteMode             = GLINT_READ_REG(LBWriteMode);
	pRC->MX2.CTextureDownloadOffset   = GLINT_READ_REG(TextureDownloadOffset);
	pRC->MX2.CLBWindowOffset          = GLINT_READ_REG(LBWindowOffset);
	pRC->MX2.CGLINTWindow             = GLINT_READ_REG(GLINTWindow);
	pRC->MX2.CStencilMode             = GLINT_READ_REG(StencilMode);
	pRC->MX2.CDepthMode               = GLINT_READ_REG(DepthMode);
	pRC->MX2.CGLINTDepth              = GLINT_READ_REG(GLINTDepth);
	pRC->MX2.CFBReadMode              = GLINT_READ_REG(FBReadMode);
	pRC->MX2.CFBSourceOffset          = GLINT_READ_REG(FBSourceOffset);
	pRC->MX2.CFBPixelOffset           = GLINT_READ_REG(FBPixelOffset);
	pRC->MX2.CFBWindowBase            = GLINT_READ_REG(FBWindowBase);
	pRC->MX2.CFBWriteMode             = GLINT_READ_REG(FBWriteMode);
	pRC->MX2.CFBHardwareWriteMask     = GLINT_READ_REG(FBHardwareWriteMask);
	pRC->MX2.CFBBlockColor            = GLINT_READ_REG(FBBlockColor);
	pRC->MX2.CPatternRamMode          = GLINT_READ_REG(PatternRamMode);
	pRC->MX2.CFBBlockColorU           = GLINT_READ_REG(FBBlockColorU);
	pRC->MX2.CFBBlockColorL           = GLINT_READ_REG(FBBlockColorL);
	pRC->MX2.CFilterMode              = GLINT_READ_REG(FilterMode);
	pRC->MX2.CStatisticMode           = GLINT_READ_REG(StatisticMode);
	ACCESSCHIP1();
	}

	if (readContextType == DRI_3D_CONTEXT) {
	    /* save the 3D portion of the old context */

	    /* first the MX portions */
	    pRC->MX1.CSStart                  = GLINT_READ_REG(SStart);
	    pRC->MX1.CdSdx                    = GLINT_READ_REG(dSdx);
	    pRC->MX1.CdSdyDom                 = GLINT_READ_REG(dSdyDom);
	    pRC->MX1.CTStart                  = GLINT_READ_REG(TStart);
	    pRC->MX1.CdTdx                    = GLINT_READ_REG(dTdx);
	    pRC->MX1.CdTdyDom                 = GLINT_READ_REG(dTdyDom);
	    pRC->MX1.CQStart                  = GLINT_READ_REG(QStart);
	    pRC->MX1.CdQdx                    = GLINT_READ_REG(dQdx);
	    pRC->MX1.CdQdyDom                 = GLINT_READ_REG(dQdyDom);
	    pRC->MX1.CLOD                     = GLINT_READ_REG(LOD);
	    pRC->MX1.CdSdy                    = GLINT_READ_REG(dSdy);
	    pRC->MX1.CdTdy                    = GLINT_READ_REG(dTdy);
	    pRC->MX1.CdQdy                    = GLINT_READ_REG(dQdy);
	    pRC->MX1.CTextureFormat           = GLINT_READ_REG(TextureFormat);
	    pRC->MX1.CTextureCacheControl     = GLINT_READ_REG(TextureCacheControl);
	    pRC->MX1.CGLINTBorderColor        = GLINT_READ_REG(GLINTBorderColor);
	    pRC->MX1.CTexelLUTIndex           = GLINT_READ_REG(TexelLUTIndex);
	    pRC->MX1.CTexelLUTData            = GLINT_READ_REG(TexelLUTData);
	    pRC->MX1.CTexelLUTAddress         = GLINT_READ_REG(TexelLUTAddress);
	    pRC->MX1.CTexelLUTTransfer        = GLINT_READ_REG(TexelLUTTransfer);
	    pRC->MX1.CTextureFilterMode       = GLINT_READ_REG(TextureFilterMode);
	    pRC->MX1.CTextureChromaUpper      = GLINT_READ_REG(TextureChromaUpper);
	    pRC->MX1.CTextureChromaLower      = GLINT_READ_REG(TextureChromaLower);
	    pRC->MX1.CTxBaseAddr0             = GLINT_READ_REG(TxBaseAddr0);
	    pRC->MX1.CTxBaseAddr1             = GLINT_READ_REG(TxBaseAddr1);
	    pRC->MX1.CTxBaseAddr2             = GLINT_READ_REG(TxBaseAddr2);
	    pRC->MX1.CTxBaseAddr3             = GLINT_READ_REG(TxBaseAddr3);
	    pRC->MX1.CTxBaseAddr4             = GLINT_READ_REG(TxBaseAddr4);
	    pRC->MX1.CTxBaseAddr5             = GLINT_READ_REG(TxBaseAddr5);
	    pRC->MX1.CTxBaseAddr6             = GLINT_READ_REG(TxBaseAddr6);
	    pRC->MX1.CTxBaseAddr7             = GLINT_READ_REG(TxBaseAddr7);
	    pRC->MX1.CTxBaseAddr8             = GLINT_READ_REG(TxBaseAddr8);
	    pRC->MX1.CTxBaseAddr9             = GLINT_READ_REG(TxBaseAddr9);
	    pRC->MX1.CTxBaseAddr10            = GLINT_READ_REG(TxBaseAddr10);
	    pRC->MX1.CTxBaseAddr11            = GLINT_READ_REG(TxBaseAddr11);
	    pRC->MX1.CTexelLUT0               = GLINT_READ_REG(TexelLUT0);
	    pRC->MX1.CTexelLUT1               = GLINT_READ_REG(TexelLUT1);
	    pRC->MX1.CTexelLUT2               = GLINT_READ_REG(TexelLUT2);
	    pRC->MX1.CTexelLUT3               = GLINT_READ_REG(TexelLUT3);
	    pRC->MX1.CTexelLUT4               = GLINT_READ_REG(TexelLUT4);
	    pRC->MX1.CTexelLUT5               = GLINT_READ_REG(TexelLUT5);
	    pRC->MX1.CTexelLUT6               = GLINT_READ_REG(TexelLUT6);
	    pRC->MX1.CTexelLUT7               = GLINT_READ_REG(TexelLUT7);
	    pRC->MX1.CTexelLUT8               = GLINT_READ_REG(TexelLUT8);
	    pRC->MX1.CTexelLUT9               = GLINT_READ_REG(TexelLUT9);
	    pRC->MX1.CTexelLUT10              = GLINT_READ_REG(TexelLUT10);
	    pRC->MX1.CTexelLUT11              = GLINT_READ_REG(TexelLUT11);
	    pRC->MX1.CTexelLUT12              = GLINT_READ_REG(TexelLUT12);
	    pRC->MX1.CTexelLUT13              = GLINT_READ_REG(TexelLUT13);
	    pRC->MX1.CTexelLUT14              = GLINT_READ_REG(TexelLUT14);
	    pRC->MX1.CTexelLUT15              = GLINT_READ_REG(TexelLUT15);
	    pRC->MX1.CTexel0                  = GLINT_READ_REG(Texel0);
	    pRC->MX1.CTexel1                  = GLINT_READ_REG(Texel1);
	    pRC->MX1.CTexel2                  = GLINT_READ_REG(Texel2);
	    pRC->MX1.CTexel3                  = GLINT_READ_REG(Texel3);
	    pRC->MX1.CTexel4                  = GLINT_READ_REG(Texel4);
	    pRC->MX1.CTexel5                  = GLINT_READ_REG(Texel5);
	    pRC->MX1.CTexel6                  = GLINT_READ_REG(Texel6);
	    pRC->MX1.CTexel7                  = GLINT_READ_REG(Texel7);
	    pRC->MX1.CInterp0                 = GLINT_READ_REG(Interp0);
	    pRC->MX1.CInterp1                 = GLINT_READ_REG(Interp1);
	    pRC->MX1.CInterp2                 = GLINT_READ_REG(Interp2);
	    pRC->MX1.CInterp3                 = GLINT_READ_REG(Interp3);
	    pRC->MX1.CInterp4                 = GLINT_READ_REG(Interp4);
	    pRC->MX1.CTextureFilter           = GLINT_READ_REG(TextureFilter);
	    pRC->MX1.CTextureEnvColor         = GLINT_READ_REG(TextureEnvColor);
	    pRC->MX1.CFogColor                = GLINT_READ_REG(FogColor);
	    pRC->MX1.CFStart                  = GLINT_READ_REG(FStart);
	    pRC->MX1.CdFdx                    = GLINT_READ_REG(dFdx);
	    pRC->MX1.CdFdyDom                 = GLINT_READ_REG(dFdyDom);
	    pRC->MX1.CKsStart                 = GLINT_READ_REG(KsStart);
	    pRC->MX1.CdKsdx                   = GLINT_READ_REG(dKsdx);
	    pRC->MX1.CdKsdyDom                = GLINT_READ_REG(dKsdyDom);
	    pRC->MX1.CKdStart                 = GLINT_READ_REG(KdStart);
	    pRC->MX1.CdKdStart                = GLINT_READ_REG(dKdStart);
	    pRC->MX1.CdKddyDom                = GLINT_READ_REG(dKddyDom);
	    pRC->MX1.CRStart                  = GLINT_READ_REG(RStart);
	    pRC->MX1.CdRdx                    = GLINT_READ_REG(dRdx);
	    pRC->MX1.CdRdyDom                 = GLINT_READ_REG(dRdyDom);
	    pRC->MX1.CGStart                  = GLINT_READ_REG(GStart);
	    pRC->MX1.CdGdx                    = GLINT_READ_REG(dGdx);
	    pRC->MX1.CdGdyDom                 = GLINT_READ_REG(dGdyDom);
	    pRC->MX1.CBStart                  = GLINT_READ_REG(BStart);
	    pRC->MX1.CdBdx                    = GLINT_READ_REG(dBdx);
	    pRC->MX1.CdBdyDom                 = GLINT_READ_REG(dBdyDom);
	    pRC->MX1.CAStart                  = GLINT_READ_REG(AStart);
	    pRC->MX1.CdAdx                    = GLINT_READ_REG(dAdx);
	    pRC->MX1.CdAdyDom                 = GLINT_READ_REG(dAdyDom);
	    pRC->MX1.CConstantColor           = GLINT_READ_REG(ConstantColor);
	    pRC->MX1.CChromaUpper             = GLINT_READ_REG(ChromaUpper);
	    pRC->MX1.CChromaLower             = GLINT_READ_REG(ChromaLower);
	    pRC->MX1.CChromaTestMode          = GLINT_READ_REG(ChromaTestMode);
	    pRC->MX1.CStencilData             = GLINT_READ_REG(StencilData);
	    pRC->MX1.CGLINTStencil            = GLINT_READ_REG(GLINTStencil);
	    pRC->MX1.CZStartU                 = GLINT_READ_REG(ZStartU);
	    pRC->MX1.CZStartL                 = GLINT_READ_REG(ZStartL);
	    pRC->MX1.CdZdxU                   = GLINT_READ_REG(dZdxU);
	    pRC->MX1.CdZdxL                   = GLINT_READ_REG(dZdxL);
	    pRC->MX1.CdZdyDomU                = GLINT_READ_REG(dZdyDomU);
	    pRC->MX1.CdZdyDomL                = GLINT_READ_REG(dZdyDomL);
	    pRC->MX1.CFastClearDepth          = GLINT_READ_REG(FastClearDepth);
	    pRC->MX1.CMinRegion               = GLINT_READ_REG(MinRegion);
	    pRC->MX1.CMaxRegion               = GLINT_READ_REG(MaxRegion);
	    pRC->MX1.CKsRStart                = GLINT_READ_REG(KsRStart);
	    pRC->MX1.CdKsRdx                  = GLINT_READ_REG(dKsRdx);
	    pRC->MX1.CdKsRdyDom               = GLINT_READ_REG(dKsRdyDom);
	    pRC->MX1.CKsGStart                = GLINT_READ_REG(KsGStart);
	    pRC->MX1.CdKsGdx                  = GLINT_READ_REG(dKsGdx);
	    pRC->MX1.CdKsGdyDom               = GLINT_READ_REG(dKsGdyDom);
	    pRC->MX1.CKsBStart                = GLINT_READ_REG(KsBStart);
	    pRC->MX1.CdKsBdx                  = GLINT_READ_REG(dKsBdx);
	    pRC->MX1.CdKsBdyDom               = GLINT_READ_REG(dKsBdyDom);
	    pRC->MX1.CKdRStart                = GLINT_READ_REG(KdRStart);
	    pRC->MX1.CdKdRdx                  = GLINT_READ_REG(dKdRdx);
	    pRC->MX1.CdKdRdyDom               = GLINT_READ_REG(dKdRdyDom);
	    pRC->MX1.CKdGStart                = GLINT_READ_REG(KdGStart);
	    pRC->MX1.CdKdGdx                  = GLINT_READ_REG(dKdGdx);
	    pRC->MX1.CdKdGdyDom               = GLINT_READ_REG(dKdGdyDom);
	    pRC->MX1.CKdBStart                = GLINT_READ_REG(KdBStart);
	    pRC->MX1.CdKdBdx                  = GLINT_READ_REG(dKdBdx);
	    pRC->MX1.CdKdBdyDom               = GLINT_READ_REG(dKdBdyDom);
            if (pGlint->numMultiDevices == 2) {
	    ACCESSCHIP2();
	    pRC->MX2.CSStart                  = GLINT_READ_REG(SStart);
	    pRC->MX2.CdSdx                    = GLINT_READ_REG(dSdx);
	    pRC->MX2.CdSdyDom                 = GLINT_READ_REG(dSdyDom);
	    pRC->MX2.CTStart                  = GLINT_READ_REG(TStart);
	    pRC->MX2.CdTdx                    = GLINT_READ_REG(dTdx);
	    pRC->MX2.CdTdyDom                 = GLINT_READ_REG(dTdyDom);
	    pRC->MX2.CQStart                  = GLINT_READ_REG(QStart);
	    pRC->MX2.CdQdx                    = GLINT_READ_REG(dQdx);
	    pRC->MX2.CdQdyDom                 = GLINT_READ_REG(dQdyDom);
	    pRC->MX2.CLOD                     = GLINT_READ_REG(LOD);
	    pRC->MX2.CdSdy                    = GLINT_READ_REG(dSdy);
	    pRC->MX2.CdTdy                    = GLINT_READ_REG(dTdy);
	    pRC->MX2.CdQdy                    = GLINT_READ_REG(dQdy);
	    pRC->MX2.CTextureFormat           = GLINT_READ_REG(TextureFormat);
	    pRC->MX2.CTextureCacheControl     = GLINT_READ_REG(TextureCacheControl);
	    pRC->MX2.CGLINTBorderColor        = GLINT_READ_REG(GLINTBorderColor);
	    pRC->MX2.CTexelLUTIndex           = GLINT_READ_REG(TexelLUTIndex);
	    pRC->MX2.CTexelLUTData            = GLINT_READ_REG(TexelLUTData);
	    pRC->MX2.CTexelLUTAddress         = GLINT_READ_REG(TexelLUTAddress);
	    pRC->MX2.CTexelLUTTransfer        = GLINT_READ_REG(TexelLUTTransfer);
	    pRC->MX2.CTextureFilterMode       = GLINT_READ_REG(TextureFilterMode);
	    pRC->MX2.CTextureChromaUpper      = GLINT_READ_REG(TextureChromaUpper);
	    pRC->MX2.CTextureChromaLower      = GLINT_READ_REG(TextureChromaLower);
	    pRC->MX2.CTxBaseAddr0             = GLINT_READ_REG(TxBaseAddr0);
	    pRC->MX2.CTxBaseAddr1             = GLINT_READ_REG(TxBaseAddr1);
	    pRC->MX2.CTxBaseAddr2             = GLINT_READ_REG(TxBaseAddr2);
	    pRC->MX2.CTxBaseAddr3             = GLINT_READ_REG(TxBaseAddr3);
	    pRC->MX2.CTxBaseAddr4             = GLINT_READ_REG(TxBaseAddr4);
	    pRC->MX2.CTxBaseAddr5             = GLINT_READ_REG(TxBaseAddr5);
	    pRC->MX2.CTxBaseAddr6             = GLINT_READ_REG(TxBaseAddr6);
	    pRC->MX2.CTxBaseAddr7             = GLINT_READ_REG(TxBaseAddr7);
	    pRC->MX2.CTxBaseAddr8             = GLINT_READ_REG(TxBaseAddr8);
	    pRC->MX2.CTxBaseAddr9             = GLINT_READ_REG(TxBaseAddr9);
	    pRC->MX2.CTxBaseAddr10            = GLINT_READ_REG(TxBaseAddr10);
	    pRC->MX2.CTxBaseAddr11            = GLINT_READ_REG(TxBaseAddr11);
	    pRC->MX2.CTexelLUT0               = GLINT_READ_REG(TexelLUT0);
	    pRC->MX2.CTexelLUT1               = GLINT_READ_REG(TexelLUT1);
	    pRC->MX2.CTexelLUT2               = GLINT_READ_REG(TexelLUT2);
	    pRC->MX2.CTexelLUT3               = GLINT_READ_REG(TexelLUT3);
	    pRC->MX2.CTexelLUT4               = GLINT_READ_REG(TexelLUT4);
	    pRC->MX2.CTexelLUT5               = GLINT_READ_REG(TexelLUT5);
	    pRC->MX2.CTexelLUT6               = GLINT_READ_REG(TexelLUT6);
	    pRC->MX2.CTexelLUT7               = GLINT_READ_REG(TexelLUT7);
	    pRC->MX2.CTexelLUT8               = GLINT_READ_REG(TexelLUT8);
	    pRC->MX2.CTexelLUT9               = GLINT_READ_REG(TexelLUT9);
	    pRC->MX2.CTexelLUT10              = GLINT_READ_REG(TexelLUT10);
	    pRC->MX2.CTexelLUT11              = GLINT_READ_REG(TexelLUT11);
	    pRC->MX2.CTexelLUT12              = GLINT_READ_REG(TexelLUT12);
	    pRC->MX2.CTexelLUT13              = GLINT_READ_REG(TexelLUT13);
	    pRC->MX2.CTexelLUT14              = GLINT_READ_REG(TexelLUT14);
	    pRC->MX2.CTexelLUT15              = GLINT_READ_REG(TexelLUT15);
	    pRC->MX2.CTexel0                  = GLINT_READ_REG(Texel0);
	    pRC->MX2.CTexel1                  = GLINT_READ_REG(Texel1);
	    pRC->MX2.CTexel2                  = GLINT_READ_REG(Texel2);
	    pRC->MX2.CTexel3                  = GLINT_READ_REG(Texel3);
	    pRC->MX2.CTexel4                  = GLINT_READ_REG(Texel4);
	    pRC->MX2.CTexel5                  = GLINT_READ_REG(Texel5);
	    pRC->MX2.CTexel6                  = GLINT_READ_REG(Texel6);
	    pRC->MX2.CTexel7                  = GLINT_READ_REG(Texel7);
	    pRC->MX2.CInterp0                 = GLINT_READ_REG(Interp0);
	    pRC->MX2.CInterp1                 = GLINT_READ_REG(Interp1);
	    pRC->MX2.CInterp2                 = GLINT_READ_REG(Interp2);
	    pRC->MX2.CInterp3                 = GLINT_READ_REG(Interp3);
	    pRC->MX2.CInterp4                 = GLINT_READ_REG(Interp4);
	    pRC->MX2.CTextureFilter           = GLINT_READ_REG(TextureFilter);
	    pRC->MX2.CTextureEnvColor         = GLINT_READ_REG(TextureEnvColor);
	    pRC->MX2.CFogColor                = GLINT_READ_REG(FogColor);
	    pRC->MX2.CFStart                  = GLINT_READ_REG(FStart);
	    pRC->MX2.CdFdx                    = GLINT_READ_REG(dFdx);
	    pRC->MX2.CdFdyDom                 = GLINT_READ_REG(dFdyDom);
	    pRC->MX2.CKsStart                 = GLINT_READ_REG(KsStart);
	    pRC->MX2.CdKsdx                   = GLINT_READ_REG(dKsdx);
	    pRC->MX2.CdKsdyDom                = GLINT_READ_REG(dKsdyDom);
	    pRC->MX2.CKdStart                 = GLINT_READ_REG(KdStart);
	    pRC->MX2.CdKdStart                = GLINT_READ_REG(dKdStart);
	    pRC->MX2.CdKddyDom                = GLINT_READ_REG(dKddyDom);
	    pRC->MX2.CRStart                  = GLINT_READ_REG(RStart);
	    pRC->MX2.CdRdx                    = GLINT_READ_REG(dRdx);
	    pRC->MX2.CdRdyDom                 = GLINT_READ_REG(dRdyDom);
	    pRC->MX2.CGStart                  = GLINT_READ_REG(GStart);
	    pRC->MX2.CdGdx                    = GLINT_READ_REG(dGdx);
	    pRC->MX2.CdGdyDom                 = GLINT_READ_REG(dGdyDom);
	    pRC->MX2.CBStart                  = GLINT_READ_REG(BStart);
	    pRC->MX2.CdBdx                    = GLINT_READ_REG(dBdx);
	    pRC->MX2.CdBdyDom                 = GLINT_READ_REG(dBdyDom);
	    pRC->MX2.CAStart                  = GLINT_READ_REG(AStart);
	    pRC->MX2.CdAdx                    = GLINT_READ_REG(dAdx);
	    pRC->MX2.CdAdyDom                 = GLINT_READ_REG(dAdyDom);
	    pRC->MX2.CConstantColor           = GLINT_READ_REG(ConstantColor);
	    pRC->MX2.CChromaUpper             = GLINT_READ_REG(ChromaUpper);
	    pRC->MX2.CChromaLower             = GLINT_READ_REG(ChromaLower);
	    pRC->MX2.CChromaTestMode          = GLINT_READ_REG(ChromaTestMode);
	    pRC->MX2.CStencilData             = GLINT_READ_REG(StencilData);
	    pRC->MX2.CGLINTStencil            = GLINT_READ_REG(GLINTStencil);
	    pRC->MX2.CZStartU                 = GLINT_READ_REG(ZStartU);
	    pRC->MX2.CZStartL                 = GLINT_READ_REG(ZStartL);
	    pRC->MX2.CdZdxU                   = GLINT_READ_REG(dZdxU);
	    pRC->MX2.CdZdxL                   = GLINT_READ_REG(dZdxL);
	    pRC->MX2.CdZdyDomU                = GLINT_READ_REG(dZdyDomU);
	    pRC->MX2.CdZdyDomL                = GLINT_READ_REG(dZdyDomL);
	    pRC->MX2.CFastClearDepth          = GLINT_READ_REG(FastClearDepth);
	    pRC->MX2.CMinRegion               = GLINT_READ_REG(MinRegion);
	    pRC->MX2.CMaxRegion               = GLINT_READ_REG(MaxRegion);
	    pRC->MX2.CKsRStart                = GLINT_READ_REG(KsRStart);
	    pRC->MX2.CdKsRdx                  = GLINT_READ_REG(dKsRdx);
	    pRC->MX2.CdKsRdyDom               = GLINT_READ_REG(dKsRdyDom);
	    pRC->MX2.CKsGStart                = GLINT_READ_REG(KsGStart);
	    pRC->MX2.CdKsGdx                  = GLINT_READ_REG(dKsGdx);
	    pRC->MX2.CdKsGdyDom               = GLINT_READ_REG(dKsGdyDom);
	    pRC->MX2.CKsBStart                = GLINT_READ_REG(KsBStart);
	    pRC->MX2.CdKsBdx                  = GLINT_READ_REG(dKsBdx);
	    pRC->MX2.CdKsBdyDom               = GLINT_READ_REG(dKsBdyDom);
	    pRC->MX2.CKdRStart                = GLINT_READ_REG(KdRStart);
	    pRC->MX2.CdKdRdx                  = GLINT_READ_REG(dKdRdx);
	    pRC->MX2.CdKdRdyDom               = GLINT_READ_REG(dKdRdyDom);
	    pRC->MX2.CKdGStart                = GLINT_READ_REG(KdGStart);
	    pRC->MX2.CdKdGdx                  = GLINT_READ_REG(dKdGdx);
	    pRC->MX2.CdKdGdyDom               = GLINT_READ_REG(dKdGdyDom);
	    pRC->MX2.CKdBStart                = GLINT_READ_REG(KdBStart);
	    pRC->MX2.CdKdBdx                  = GLINT_READ_REG(dKdBdx);
	    pRC->MX2.CdKdBdyDom               = GLINT_READ_REG(dKdBdyDom);
	    ACCESSCHIP1();
	    }

	    /* send gamma the context dump command */
	    GLINT_WAIT(3);
            if (pGlint->numMultiDevices == 2)
	    	GLINT_WRITE_REG(1, BroadcastMask);
	    GLINT_WRITE_REG(3<<14, FilterMode);  /* context bits on gamma */
	    GLINT_WRITE_REG(GLINT_GAMMA_CONTEXT_MASK, ContextDump);

	    /* save context data from  output fifo */
	    dumpIndex = 0;
	    do {
		while(GLINT_READ_REG(OutFIFOWords) == 0);
		readValue = GLINT_READ_REG(OutputFIFO);
#ifdef DEBUG
xf86DrvMsg(pScreen->myNum, X_INFO, "pRC tag [%d]: %x\n",
dumpIndex,readValue);
#endif
		while(GLINT_READ_REG(OutFIFOWords) == 0);
		readValue = GLINT_READ_REG(OutputFIFO);
#ifdef DEBUG
xf86DrvMsg(pScreen->myNum, X_INFO, "pRC data [%d]: %x\n",
dumpIndex,readValue);
#endif
		pRC->Gamma[dumpIndex++] = readValue;
            } while (dumpIndex < GLINT_GAMMA_CONTEXT_SIZE);

	    /* clear contextDump tag and data out of fifo */
	    while(GLINT_READ_REG(OutFIFOWords) == 0);
	    readValue = GLINT_READ_REG(OutputFIFO);
	    if (readValue != ContextDump_tag) {
		xf86DrvMsg(pScreen->myNum, X_ERROR, "Context dump error\n");
	    }
	    while(GLINT_READ_REG(OutFIFOWords) == 0);
	    readValue = GLINT_READ_REG(OutputFIFO);

	    GLINT_SLOW_WRITE_REG(1<<10, FilterMode);
            if (pGlint->numMultiDevices == 2)
	    	GLINT_SLOW_WRITE_REG(3,BroadcastMask);
	}
    }

    if (writeContextType != DRI_NO_CONTEXT) {

	if (writeContextType == DRI_3D_CONTEXT) {
	    /* restore the 3D portion of the new context */

	    /* send context restore command */
	    GLINT_WAIT(1);
            if (pGlint->numMultiDevices == 2)
	    	GLINT_WRITE_REG(1, BroadcastMask);

	    GLINT_WAIT(3);
	    GLINT_WRITE_REG(ContextRestore_tag, OutputFIFO);
	    GLINT_WRITE_REG(GLINT_GAMMA_CONTEXT_MASK, OutputFIFO);
	    GLINT_WRITE_REG((((GLINT_GAMMA_CONTEXT_SIZE-1) << 16) | 
                              ContextData_tag), OutputFIFO);

	    /* restore context data to context data register */
	    dumpIndex = 0;
	    do {
		GLINT_WAIT(1);
#ifdef DEBUG
xf86DrvMsg(pScreen->myNum, X_INFO, "pWC data [%d]: %x\n",
dumpIndex,pWC->Gamma[dumpIndex]);
#endif
		GLINT_WRITE_REG(pWC->Gamma[dumpIndex++], OutputFIFO);
	    } while (dumpIndex < (GLINT_GAMMA_CONTEXT_SIZE));

				/* Sync after writing gamma context and
                                   before writing MX context */
	    if (pGlint->AccelInfoRec->Sync) {
		(*pGlint->AccelInfoRec->Sync)(pGlint->AccelInfoRec->pScrn);
	    }
				/* Update XAA's NeedToSync flag */
	    pGlint->AccelInfoRec->NeedToSync = TRUE;
	    
	    /* finally the MX portions */
            if (pGlint->numMultiDevices == 2)
	    	GLINT_SLOW_WRITE_REG(1, BroadcastMask);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CSStart,                  SStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdSdx,                    dSdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdSdyDom,                 dSdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTStart,                  TStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdTdx,                    dTdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdTdyDom,                 dTdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CQStart,                  QStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdQdx,                    dQdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdQdyDom,                 dQdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CLOD,                     LOD);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdSdy,                    dSdy);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdTdy,                    dTdy);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdQdy,                    dQdy);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTextureFormat,           TextureFormat);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTextureCacheControl,     TextureCacheControl);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CGLINTBorderColor,        GLINTBorderColor);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUTIndex,           TexelLUTIndex);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUTData,            TexelLUTData);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUTAddress,         TexelLUTAddress);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUTTransfer,        TexelLUTTransfer);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTextureFilterMode,       TextureFilterMode);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTextureChromaUpper,      TextureChromaUpper);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTextureChromaLower,      TextureChromaLower);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTxBaseAddr0,             TxBaseAddr0);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTxBaseAddr1,             TxBaseAddr1);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTxBaseAddr2,             TxBaseAddr2);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTxBaseAddr3,             TxBaseAddr3);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTxBaseAddr4,             TxBaseAddr4);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTxBaseAddr5,             TxBaseAddr5);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTxBaseAddr6,             TxBaseAddr6);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTxBaseAddr7,             TxBaseAddr7);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTxBaseAddr8,             TxBaseAddr8);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTxBaseAddr9,             TxBaseAddr9);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTxBaseAddr10,            TxBaseAddr10);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTxBaseAddr11,            TxBaseAddr11);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUT0,               TexelLUT0);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUT1,               TexelLUT1);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUT2,               TexelLUT2);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUT3,               TexelLUT3);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUT4,               TexelLUT4);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUT5,               TexelLUT5);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUT6,               TexelLUT6);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUT7,               TexelLUT7);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUT8,               TexelLUT8);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUT9,               TexelLUT9);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUT10,              TexelLUT10);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUT11,              TexelLUT11);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUT12,              TexelLUT12);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUT13,              TexelLUT13);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUT14,              TexelLUT14);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexelLUT15,              TexelLUT15);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexel0,                  Texel0);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexel1,                  Texel1);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexel2,                  Texel2);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexel3,                  Texel3);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexel4,                  Texel4);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexel5,                  Texel5);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexel6,                  Texel6);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTexel7,                  Texel7);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CInterp0,                 Interp0);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CInterp1,                 Interp1);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CInterp2,                 Interp2);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CInterp3,                 Interp3);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CInterp4,                 Interp4);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTextureFilter,           TextureFilter);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CTextureEnvColor,         TextureEnvColor);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CFogColor,                FogColor);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CFStart,                  FStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdFdx,                    dFdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdFdyDom,                 dFdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CKsStart,                 KsStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdKsdx,                   dKsdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdKsdyDom,                dKsdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CKdStart,                 KdStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdKdStart,                dKdStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdKddyDom,                dKddyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CRStart,                  RStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdRdx,                    dRdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdRdyDom,                 dRdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CGStart,                  GStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdGdx,                    dGdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdGdyDom,                 dGdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CBStart,                  BStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdBdx,                    dBdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdBdyDom,                 dBdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CAStart,                  AStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdAdx,                    dAdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdAdyDom,                 dAdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CConstantColor,           ConstantColor);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CChromaUpper,             ChromaUpper);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CChromaLower,             ChromaLower);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CChromaTestMode,          ChromaTestMode);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CStencilData,             StencilData);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CGLINTStencil,            GLINTStencil);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CZStartU,                 ZStartU);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CZStartL,                 ZStartL);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdZdxU,                   dZdxU);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdZdxL,                   dZdxL);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdZdyDomU,                dZdyDomU);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdZdyDomL,                dZdyDomL);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CFastClearDepth,          FastClearDepth);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CMinRegion,               MinRegion);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CMaxRegion,               MaxRegion);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CKsRStart,                KsRStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdKsRdx,                  dKsRdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdKsRdyDom,               dKsRdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CKsGStart,                KsGStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdKsGdx,                  dKsGdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdKsGdyDom,               dKsGdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CKsBStart,                KsBStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdKsBdx,                  dKsBdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdKsBdyDom,               dKsBdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CKdRStart,                KdRStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdKdRdx,                  dKdRdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdKdRdyDom,               dKdRdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CKdGStart,                KdGStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdKdGdx,                  dKdGdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdKdGdyDom,               dKdGdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CKdBStart,                KdBStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdKdBdx,                  dKdBdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX1.CdKdBdyDom,               dKdBdyDom);

            if (pGlint->numMultiDevices == 2) {
	    GLINT_SLOW_WRITE_REG(2, BroadcastMask);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CSStart,                  SStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdSdx,                    dSdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdSdyDom,                 dSdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTStart,                  TStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdTdx,                    dTdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdTdyDom,                 dTdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CQStart,                  QStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdQdx,                    dQdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdQdyDom,                 dQdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CLOD,                     LOD);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdSdy,                    dSdy);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdTdy,                    dTdy);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdQdy,                    dQdy);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTextureFormat,           TextureFormat);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTextureCacheControl,     TextureCacheControl);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CGLINTBorderColor,        GLINTBorderColor);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUTIndex,           TexelLUTIndex);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUTData,            TexelLUTData);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUTAddress,         TexelLUTAddress);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUTTransfer,        TexelLUTTransfer);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTextureFilterMode,       TextureFilterMode);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTextureChromaUpper,      TextureChromaUpper);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTextureChromaLower,      TextureChromaLower);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTxBaseAddr0,             TxBaseAddr0);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTxBaseAddr1,             TxBaseAddr1);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTxBaseAddr2,             TxBaseAddr2);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTxBaseAddr3,             TxBaseAddr3);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTxBaseAddr4,             TxBaseAddr4);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTxBaseAddr5,             TxBaseAddr5);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTxBaseAddr6,             TxBaseAddr6);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTxBaseAddr7,             TxBaseAddr7);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTxBaseAddr8,             TxBaseAddr8);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTxBaseAddr9,             TxBaseAddr9);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTxBaseAddr10,            TxBaseAddr10);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTxBaseAddr11,            TxBaseAddr11);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUT0,               TexelLUT0);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUT1,               TexelLUT1);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUT2,               TexelLUT2);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUT3,               TexelLUT3);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUT4,               TexelLUT4);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUT5,               TexelLUT5);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUT6,               TexelLUT6);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUT7,               TexelLUT7);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUT8,               TexelLUT8);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUT9,               TexelLUT9);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUT10,              TexelLUT10);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUT11,              TexelLUT11);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUT12,              TexelLUT12);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUT13,              TexelLUT13);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUT14,              TexelLUT14);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexelLUT15,              TexelLUT15);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexel0,                  Texel0);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexel1,                  Texel1);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexel2,                  Texel2);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexel3,                  Texel3);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexel4,                  Texel4);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexel5,                  Texel5);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexel6,                  Texel6);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTexel7,                  Texel7);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CInterp0,                 Interp0);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CInterp1,                 Interp1);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CInterp2,                 Interp2);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CInterp3,                 Interp3);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CInterp4,                 Interp4);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTextureFilter,           TextureFilter);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CTextureEnvColor,         TextureEnvColor);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CFogColor,                FogColor);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CFStart,                  FStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdFdx,                    dFdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdFdyDom,                 dFdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CKsStart,                 KsStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdKsdx,                   dKsdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdKsdyDom,                dKsdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CKdStart,                 KdStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdKdStart,                dKdStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdKddyDom,                dKddyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CRStart,                  RStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdRdx,                    dRdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdRdyDom,                 dRdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CGStart,                  GStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdGdx,                    dGdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdGdyDom,                 dGdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CBStart,                  BStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdBdx,                    dBdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdBdyDom,                 dBdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CAStart,                  AStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdAdx,                    dAdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdAdyDom,                 dAdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CConstantColor,           ConstantColor);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CChromaUpper,             ChromaUpper);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CChromaLower,             ChromaLower);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CChromaTestMode,          ChromaTestMode);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CStencilData,             StencilData);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CGLINTStencil,            GLINTStencil);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CZStartU,                 ZStartU);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CZStartL,                 ZStartL);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdZdxU,                   dZdxU);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdZdxL,                   dZdxL);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdZdyDomU,                dZdyDomU);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdZdyDomL,                dZdyDomL);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CFastClearDepth,          FastClearDepth);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CMinRegion,               MinRegion);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CMaxRegion,               MaxRegion);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CKsRStart,                KsRStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdKsRdx,                  dKsRdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdKsRdyDom,               dKsRdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CKsGStart,                KsGStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdKsGdx,                  dKsGdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdKsGdyDom,               dKsGdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CKsBStart,                KsBStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdKsBdx,                  dKsBdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdKsBdyDom,               dKsBdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CKdRStart,                KdRStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdKdRdx,                  dKdRdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdKdRdyDom,               dKdRdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CKdGStart,                KdGStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdKdGdx,                  dKdGdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdKdGdyDom,               dKdGdyDom);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CKdBStart,                KdBStart);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdKdBdx,                  dKdBdx);
	    GLINT_SLOW_WRITE_REG(pWC->MX2.CdKdBdyDom,               dKdBdyDom);
	    }
	}

	/* restore the 2D portion of the new context */

	/* Restore MX1's registers */
        if (pGlint->numMultiDevices == 2)
	    GLINT_SLOW_WRITE_REG(1, BroadcastMask);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CStartXDom,               StartXDom);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CdXDom,                   dXDom);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CStartXSub,               StartXSub);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CdXSub,                   dXSub);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CStartY,                  StartY);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CdY,                      dY);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CGLINTCount,              GLINTCount);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CPointTable0,             PointTable0);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CPointTable1,             PointTable1);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CPointTable2,             PointTable2);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CPointTable3,             PointTable3);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CRasterizerMode,          RasterizerMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CYLimits,                 YLimits);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CScanLineOwnership,       ScanLineOwnership);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CPixelSize,               PixelSize);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CScissorMode,             ScissorMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CScissorMinXY,            ScissorMinXY);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CScissorMaxXY,            ScissorMaxXY);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CScreenSize,              ScreenSize);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CAreaStippleMode,         AreaStippleMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CLineStippleMode,         LineStippleMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CLoadLineStippleCounters, LoadLineStippleCounters);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CWindowOrigin,            WindowOrigin);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CRouterMode,              RouterMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CTextureAddressMode,      TextureAddressMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CTextureReadMode,         TextureReadMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CTextureColorMode,        TextureColorMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CFogMode,                 FogMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CColorDDAMode,            ColorDDAMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CGLINTColor,              GLINTColor);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CAlphaTestMode,           AlphaTestMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CAntialiasMode,           AntialiasMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CAlphaBlendMode,          AlphaBlendMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CDitherMode,              DitherMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CFBSoftwareWriteMask,     FBSoftwareWriteMask);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CLogicalOpMode,           LogicalOpMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CFBWriteData,             FBWriteData);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CLBReadMode,              LBReadMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CLBSourceOffset,          LBSourceOffset);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CLBWindowBase,            LBWindowBase);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CLBWriteMode,             LBWriteMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CTextureDownloadOffset,   TextureDownloadOffset);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CLBWindowOffset,          LBWindowOffset);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CGLINTWindow,             GLINTWindow);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CStencilMode,             StencilMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CDepthMode,               DepthMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CGLINTDepth,              GLINTDepth);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CFBReadMode,              FBReadMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CFBSourceOffset,          FBSourceOffset);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CFBPixelOffset,           FBPixelOffset);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CFBWindowBase,            FBWindowBase);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CFBWriteMode,             FBWriteMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CFBHardwareWriteMask,     FBHardwareWriteMask);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CFBBlockColor,            FBBlockColor);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CPatternRamMode,          PatternRamMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CFBBlockColorU,           FBBlockColorU);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CFBBlockColorL,           FBBlockColorL);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CFilterMode,              FilterMode);
	GLINT_SLOW_WRITE_REG(pWC->MX1.CStatisticMode,           StatisticMode);

	/* Restore MX2's registers */
        if (pGlint->numMultiDevices == 2) {
	GLINT_SLOW_WRITE_REG(2, BroadcastMask);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CStartXDom,               StartXDom);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CdXDom,                   dXDom);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CStartXSub,               StartXSub);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CdXSub,                   dXSub);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CStartY,                  StartY);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CdY,                      dY);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CGLINTCount,              GLINTCount);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CPointTable0,             PointTable0);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CPointTable1,             PointTable1);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CPointTable2,             PointTable2);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CPointTable3,             PointTable3);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CRasterizerMode,          RasterizerMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CYLimits,                 YLimits);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CScanLineOwnership,       ScanLineOwnership);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CPixelSize,               PixelSize);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CScissorMode,             ScissorMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CScissorMinXY,            ScissorMinXY);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CScissorMaxXY,            ScissorMaxXY);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CScreenSize,              ScreenSize);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CAreaStippleMode,         AreaStippleMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CLineStippleMode,         LineStippleMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CLoadLineStippleCounters, LoadLineStippleCounters);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CWindowOrigin,            WindowOrigin);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CRouterMode,              RouterMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CTextureAddressMode,      TextureAddressMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CTextureReadMode,         TextureReadMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CTextureColorMode,        TextureColorMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CFogMode,                 FogMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CColorDDAMode,            ColorDDAMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CGLINTColor,              GLINTColor);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CAlphaTestMode,           AlphaTestMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CAntialiasMode,           AntialiasMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CAlphaBlendMode,          AlphaBlendMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CDitherMode,              DitherMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CFBSoftwareWriteMask,     FBSoftwareWriteMask);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CLogicalOpMode,           LogicalOpMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CFBWriteData,             FBWriteData);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CLBReadMode,              LBReadMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CLBSourceOffset,          LBSourceOffset);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CLBWindowBase,            LBWindowBase);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CLBWriteMode,             LBWriteMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CTextureDownloadOffset,   TextureDownloadOffset);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CLBWindowOffset,          LBWindowOffset);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CGLINTWindow,             GLINTWindow);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CStencilMode,             StencilMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CDepthMode,               DepthMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CGLINTDepth,              GLINTDepth);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CFBReadMode,              FBReadMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CFBSourceOffset,          FBSourceOffset);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CFBPixelOffset,           FBPixelOffset);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CFBWindowBase,            FBWindowBase);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CFBWriteMode,             FBWriteMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CFBHardwareWriteMask,     FBHardwareWriteMask);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CFBBlockColor,            FBBlockColor);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CPatternRamMode,          PatternRamMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CFBBlockColorU,           FBBlockColorU);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CFBBlockColorL,           FBBlockColorL);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CFilterMode,              FilterMode);
	GLINT_SLOW_WRITE_REG(pWC->MX2.CStatisticMode,           StatisticMode);

	/* Restore the "real" broadcast mask last */
	GLINT_SLOW_WRITE_REG(pWC->MX1.CBroadcastMask,           BroadcastMask);
	}
				/* Sync is needed here probabilistically */
	if (pGlint->AccelInfoRec->Sync) {
	    (*pGlint->AccelInfoRec->Sync)(pGlint->AccelInfoRec->pScrn);
	    pGlint->AccelInfoRec->NeedToSync = FALSE;
	}
    }
}

void
GLINTDRIInitBuffers(
    WindowPtr pWin,
    RegionPtr prgn,
    CARD32 index)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    GLINTPtr pGlint = GLINTPTR(pScrn);
    BoxPtr pbox;
    int nbox;

    pbox = REGION_RECTS(prgn);
    nbox = REGION_NUM_RECTS(prgn);

    GLINT_WAIT(7);
    /* Turn off writes the FB */
    GLINT_WRITE_REG(0, FBWriteMode);
    GLINT_WRITE_REG(0, LBWindowBase);
    GLINT_WRITE_REG(1, LBWriteMode);
    if (pGlint->numMultiDevices == 2) {
    GLINT_WRITE_REG( pGlint->pprod     | 
		     LBRM_ScanlineInt2   , LBReadMode);
    } else {
    GLINT_WRITE_REG( pGlint->pprod       , LBReadMode);
    }
    GLINT_WRITE_REG(0, LBDepth);
    GLINT_WRITE_REG(0, LBStencil);
    GLINT_WRITE_REG( GWIN_UnitEnable        |
		     GWIN_ForceLBUpdate     |
		     ((index & 0xf) << 5)   |
		     GWIN_LBUpdateSourceREG |
		     GWIN_OverrideWriteFilter, GLINTWindow);

    while (nbox--) {
	GLINT_WAIT(8);
	GLINT_WRITE_REG((pbox->x2)<<16, 	StartXSub);
	GLINT_WRITE_REG((pbox->x1)<<16, 	StartXDom);
	GLINT_WRITE_REG((pbox->y1)<<16, 	StartY);
	GLINT_WRITE_REG((pbox->y2 - pbox->y1),	GLINTCount);
	GLINT_WRITE_REG(0, dXDom);
	GLINT_WRITE_REG(0x10000, dY);
	/* Must also set dXSub, since 3D tris cause it to be != 0 */
	GLINT_WRITE_REG(0,     dXSub);
	GLINT_WRITE_REG(PrimitiveTrapezoid, Render);
	pbox++;
    }

    GLINT_WAIT(3);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	LBWriteMode);
    GLINT_SLOW_WRITE_REG(UNIT_ENABLE,	FBWriteMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,  GLINTWindow);

				/* Update XAA's NeedToSync flag */
    pGlint->AccelInfoRec->NeedToSync = TRUE;
}

void
GLINTDRIMoveBuffers(
    WindowPtr pParent,
    DDXPointRec ptOldOrg,
    RegionPtr prgnSrc,
    CARD32 index)
{
    ScreenPtr pScreen = pParent->drawable.pScreen;
#if 0
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
#endif
    int dx, dy;
    WindowPtr pChild;
    RegionRec rgnSubWindow, rgnTranslateSrc;
    CARD32 indexSubWindow;

    /* NOT_DONE:  For now, just init the buffer.  We are not copying the depth
     * and stencil, they just get redrawn for the next frame(s).
     */

    REGION_NULL(pScreen, &rgnSubWindow);
    REGION_NULL(pScreen, &rgnTranslateSrc);
    REGION_COPY(pScreen, &rgnTranslateSrc, prgnSrc);
    dx = ptOldOrg.x - pParent->drawable.x;
    dy = ptOldOrg.y - pParent->drawable.y;
    REGION_TRANSLATE(pScreen, &rgnTranslateSrc, -dx, -dy);

    pChild = pParent;
    while (1) {
	if (pChild->viewable) {
	    REGION_INTERSECT(pScreen, &rgnSubWindow, 
		    &pChild->borderClip, &rgnTranslateSrc);
	    indexSubWindow = DRIGetDrawableIndex(pChild);
	    GLINTDRIInitBuffers( pChild, &rgnSubWindow, indexSubWindow);
	    if (pChild->firstChild) {
		pChild = pChild->firstChild;
		continue;
	    }
	}
	while (!pChild->nextSib && (pChild != pParent))
            pChild = pChild->parent;
        if (pChild == pParent)
            break;
        pChild = pChild->nextSib;
    }
}
