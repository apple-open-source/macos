/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_dri.c,v 1.41 2004/01/04 18:08:00 twini Exp $ */
/*
 * DRI wrapper for 300 and 315 series
 *
 * Copyright (C) 2001-2004 by Thomas Winischhofer, Vienna, Austria
 *
 * Preliminary 315/330 support by Thomas Winischhofer
 * Portions of Mesa 4/5 changes by Eric Anholt
 *
 * Licensed under the following terms:
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appears in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * and that the name of the copyright holder not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission. The copyright holder makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without expressed or implied warranty.
 *
 * THE COPYRIGHT HOLDER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO
 * EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Previously taken and modified from tdfx_dri.c, mga_dri.c
 *
 * Authors:	Can-Ru Yeou, SiS Inc.
 *		Alan Hourihane, Wigan, England,
 *		Thomas Winischhofer <thomas@winischhofer.net>
 *		others.
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Priv.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "fb.h"

#include "GL/glxtokens.h"

#include "sis.h"
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,2,99,0,0)
#include "xf86drmCompat.h"
#endif

#ifdef SISNEWDRI
#include "sis_common.h"
#endif

/* Idle function for 300 series */
#define BR(x)   (0x8200 | (x) << 2)
#define SiSIdle \
  while((MMIO_IN16(pSiS->IOBase, BR(16)+2) & 0xE000) != 0xE000){}; \
  while((MMIO_IN16(pSiS->IOBase, BR(16)+2) & 0xE000) != 0xE000){}; \
  MMIO_IN16(pSiS->IOBase, 0x8240);

/* Idle function for 315/330 series */
#define Q_STATUS 0x85CC
#define SiS315Idle \
  { \
  while( (MMIO_IN16(pSiS->IOBase, Q_STATUS+2) & 0x8000) != 0x8000){}; \
  while( (MMIO_IN16(pSiS->IOBase, Q_STATUS+2) & 0x8000) != 0x8000){}; \
  }


extern void GlxSetVisualConfigs(
    int nconfigs,
    __GLXvisualConfig *configs,
    void **configprivs
);

static char SISKernelDriverName[] = "sis";
static char SISClientDriverName[] = "sis";

static Bool SISInitVisualConfigs(ScreenPtr pScreen);
static Bool SISCreateContext(ScreenPtr pScreen, VisualPtr visual, 
                   drmContext hwContext, void *pVisualConfigPriv,
                   DRIContextType contextStore);
static void SISDestroyContext(ScreenPtr pScreen, drmContext hwContext,
                   DRIContextType contextStore);
static void SISDRISwapContext(ScreenPtr pScreen, DRISyncType syncType, 
                   DRIContextType readContextType, 
                   void *readContextStore,
                   DRIContextType writeContextType, 
                   void *writeContextStore);
static void SISDRIInitBuffers(WindowPtr pWin, RegionPtr prgn, CARD32 index);
static void SISDRIMoveBuffers(WindowPtr pParent, DDXPointRec ptOldOrg, 
                   RegionPtr prgnSrc, CARD32 index);

#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
extern Bool drmSiSAgpInit(int driSubFD, int offset, int size);
#endif

static Bool
SISInitVisualConfigs(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  SISPtr pSIS = SISPTR(pScrn);
  int numConfigs = 0;
  __GLXvisualConfig *pConfigs = 0;
  SISConfigPrivPtr pSISConfigs = 0;
  SISConfigPrivPtr *pSISConfigPtrs = 0;
  int i, db, z_stencil, accum;
  Bool useZ16 = FALSE;

  if(getenv("SIS_FORCE_Z16")) {
     useZ16 = TRUE;
  }

  switch (pScrn->bitsPerPixel) {
  case 8:
  case 24:
    break;
  case 16:
  case 32:
    numConfigs = (useZ16) ? 8 : 16;

    if(!(pConfigs = (__GLXvisualConfig*)xcalloc(sizeof(__GLXvisualConfig),
						   numConfigs))) {
       return FALSE;
    }
    if(!(pSISConfigs = (SISConfigPrivPtr)xcalloc(sizeof(SISConfigPrivRec),
						    numConfigs))) {
       xfree(pConfigs);
       return FALSE;
    }
    if(!(pSISConfigPtrs = (SISConfigPrivPtr*)xcalloc(sizeof(SISConfigPrivPtr),
							  numConfigs))) {
       xfree(pConfigs);
       xfree(pSISConfigs);
       return FALSE;
    }
    for(i=0; i<numConfigs; i++)  pSISConfigPtrs[i] = &pSISConfigs[i];

    i = 0;
    for(accum = 0; accum <= 1; accum++) {
      for(z_stencil=0; z_stencil<(useZ16?2:4); z_stencil++) {
        for(db = 0; db <= 1; db++) {
          pConfigs[i].vid = -1;
          pConfigs[i].class = -1;
          pConfigs[i].rgba = TRUE;
          pConfigs[i].redSize = -1;
          pConfigs[i].greenSize = -1;
          pConfigs[i].blueSize = -1;
          pConfigs[i].redMask = -1;
          pConfigs[i].greenMask = -1;
          pConfigs[i].blueMask = -1;
          pConfigs[i].alphaMask = 0;
          if(accum) {
            pConfigs[i].accumRedSize = 16;
            pConfigs[i].accumGreenSize = 16;
            pConfigs[i].accumBlueSize = 16;
            pConfigs[i].accumAlphaSize = 16;
          } else {
            pConfigs[i].accumRedSize = 0;
            pConfigs[i].accumGreenSize = 0;
            pConfigs[i].accumBlueSize = 0;
            pConfigs[i].accumAlphaSize = 0;
          }
          if(db) pConfigs[i].doubleBuffer = TRUE;
          else   pConfigs[i].doubleBuffer = FALSE;
          pConfigs[i].stereo = FALSE;
          pConfigs[i].bufferSize = -1;
          switch(z_stencil) {
            case 0:
              pConfigs[i].depthSize = 0;
              pConfigs[i].stencilSize = 0;
              break;
            case 1:
              pConfigs[i].depthSize = 16;
              pConfigs[i].stencilSize = 0;
              break;
            case 2:
              pConfigs[i].depthSize = 32;
              pConfigs[i].stencilSize = 0;
              break;
            case 3:
              pConfigs[i].depthSize = 24;
              pConfigs[i].stencilSize = 8;
              break;
          }
          pConfigs[i].auxBuffers = 0;
          pConfigs[i].level = 0;
          pConfigs[i].visualRating = GLX_NONE_EXT;
          pConfigs[i].transparentPixel = 0;
          pConfigs[i].transparentRed = 0;
          pConfigs[i].transparentGreen = 0;
          pConfigs[i].transparentBlue = 0;
          pConfigs[i].transparentAlpha = 0;
          pConfigs[i].transparentIndex = 0;
          i++;
        }
      }
    }
    if(i != numConfigs) {
       xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                 "[dri] Incorrect initialization of visuals. Disabling DRI.\n");
       return FALSE;
    }
    break;
  }
 
  pSIS->numVisualConfigs = numConfigs;
  pSIS->pVisualConfigs = pConfigs;
  pSIS->pVisualConfigsPriv = pSISConfigs;
  GlxSetVisualConfigs(numConfigs, pConfigs, (void**)pSISConfigPtrs);

  return TRUE;
}

Bool SISDRIScreenInit(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  SISPtr pSIS = SISPTR(pScrn);
  DRIInfoPtr pDRIInfo;
  SISDRIPtr pSISDRI;
#ifdef SISNEWDRI
  drmVersionPtr version;
#endif

   pSIS->cmdQueueLenPtrBackup = NULL;
#ifdef SIS315DRI
   pSIS->cmdQ_SharedWritePortBackup = NULL;
#endif

   /* Check that the GLX, DRI, and DRM modules have been loaded by testing
    * for canonical symbols in each module.
    */
   if(!xf86LoaderCheckSymbol("GlxSetVisualConfigs")) return FALSE;
   if(!xf86LoaderCheckSymbol("DRIScreenInit"))       return FALSE;
   if(!xf86LoaderCheckSymbol("drmAvailable"))        return FALSE;
   if(!xf86LoaderCheckSymbol("DRIQueryVersion")) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
                "[dri] SISDRIScreenInit failed (libdri.a too old)\n");
      return FALSE;
   }
     
   /* Check the DRI version */
   {
      int major, minor, patch;
      DRIQueryVersion(&major, &minor, &patch);
      if(major != 4 || minor < 0) {
         xf86DrvMsg(pScreen->myNum, X_ERROR,
                    "[dri] SISDRIScreenInit failed because of a version mismatch.\n"
                    "\t[dri] libDRI version is %d.%d.%d but version 4.0.x is needed.\n"
                    "\t[dri] Disabling DRI.\n",
                    major, minor, patch);
         return FALSE;
      }
   }

  pDRIInfo = DRICreateInfoRec();
  if (!pDRIInfo) return FALSE;
  pSIS->pDRIInfo = pDRIInfo;

  pDRIInfo->drmDriverName = SISKernelDriverName;
  pDRIInfo->clientDriverName = SISClientDriverName;
#ifdef SISNEWDRI2
  if(xf86LoaderCheckSymbol("DRICreatePCIBusID")) {
     pDRIInfo->busIdString = DRICreatePCIBusID(pSiS->PciInfo);
  } else {
#endif
     pDRIInfo->busIdString = xalloc(64);
     sprintf(pDRIInfo->busIdString, "PCI:%d:%d:%d",
             ((pciConfigPtr)pSIS->PciInfo->thisCard)->busnum,
      	     ((pciConfigPtr)pSIS->PciInfo->thisCard)->devnum,
      	     ((pciConfigPtr)pSIS->PciInfo->thisCard)->funcnum);
#ifdef SISNEWDRI2
  }
#endif

  /* Hack to keep old DRI working -- checked for major==1 and
   * minor==1.
   */
#ifdef SISNEWDRI
  pDRIInfo->ddxDriverMajorVersion = SIS_MAJOR_VERSION;
  pDRIInfo->ddxDriverMinorVersion = SIS_MINOR_VERSION;
  pDRIInfo->ddxDriverPatchVersion = SIS_PATCHLEVEL;
#else
  pDRIInfo->ddxDriverMajorVersion = 0;
  pDRIInfo->ddxDriverMinorVersion = 1;
  pDRIInfo->ddxDriverPatchVersion = 0;
#endif

  pDRIInfo->frameBufferPhysicalAddress = pSIS->FbAddress;

  /* TW: This was FbMapSize which is wrong as we must not
   *     ever overwrite HWCursor and TQ area. On the other
   *     hand, using availMem here causes MTRR allocation
   *     to fail ("base is not aligned to size"). Since
   *     DRI memory management is done via framebuffer
   *     device, I assume that the size given here
   *     is NOT used for eventual memory management.
   */
  pDRIInfo->frameBufferSize = pSIS->FbMapSize;   /* availMem; */
  
  /* TW: scrnOffset is being calulated in sis_vga.c */
  pDRIInfo->frameBufferStride = pSIS->scrnOffset;
  
  pDRIInfo->ddxDrawableTableEntry = SIS_MAX_DRAWABLES;

  if(SAREA_MAX_DRAWABLES < SIS_MAX_DRAWABLES)
     pDRIInfo->maxDrawableTableEntry = SAREA_MAX_DRAWABLES;
  else
     pDRIInfo->maxDrawableTableEntry = SIS_MAX_DRAWABLES;

#ifdef NOT_DONE
  /* FIXME need to extend DRI protocol to pass this size back to client
   * for SAREA mapping that includes a device private record
   */
  pDRIInfo->SAREASize =
    ((sizeof(XF86DRISAREARec) + getpagesize() - 1) & getpagesize()); /* round to page */
    /* ((sizeof(XF86DRISAREARec) + 0xfff) & 0x1000); */ /* round to page */
  /* + shared memory device private rec */
#else
  /* For now the mapping works by using a fixed size defined
   * in the SAREA header
   */
  if(sizeof(XF86DRISAREARec)+sizeof(SISSAREAPriv) > SAREA_MAX) {
     xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"Data does not fit in SAREA\n");
     return FALSE;
  }
  pDRIInfo->SAREASize = SAREA_MAX;
#endif

  if(!(pSISDRI = (SISDRIPtr)xcalloc(sizeof(SISDRIRec),1))) {
     DRIDestroyInfoRec(pSIS->pDRIInfo);
     pSIS->pDRIInfo = 0;
     return FALSE;
  }
  pDRIInfo->devPrivate = pSISDRI;
  pDRIInfo->devPrivateSize = sizeof(SISDRIRec);
  pDRIInfo->contextSize = sizeof(SISDRIContextRec);

  pDRIInfo->CreateContext = SISCreateContext;
  pDRIInfo->DestroyContext = SISDestroyContext;
  pDRIInfo->SwapContext = SISDRISwapContext;
  pDRIInfo->InitBuffers = SISDRIInitBuffers;
  pDRIInfo->MoveBuffers = SISDRIMoveBuffers;
  pDRIInfo->bufferRequests = DRI_ALL_WINDOWS;

  if(!DRIScreenInit(pScreen, pDRIInfo, &pSIS->drmSubFD)) {
     xf86DrvMsg(pScreen->myNum, X_ERROR,
               "[dri] DRIScreenInit failed. Disabling DRI.\n");
     xfree(pDRIInfo->devPrivate);
     pDRIInfo->devPrivate = 0;
     DRIDestroyInfoRec(pSIS->pDRIInfo);
     pSIS->pDRIInfo = 0;
     pSIS->drmSubFD = -1;
     return FALSE;
  }

#ifdef SISNEWDRI
  /* Check DRM kernel version */
  version = drmGetVersion(pSIS->drmSubFD);
  if(version) {
    if(version->version_major != 1 ||
       version->version_minor < 0) {
      /* incompatible drm version */
      xf86DrvMsg(pScreen->myNum, X_ERROR,
                 "[dri] SISDRIScreenInit failed because of a version mismatch.\n"
                 "\t[dri] sis.o kernel module version is %d.%d.%d but version 1.0.x is needed.\n"
                 "\t[dri] Disabling the DRI.\n",
                 version->version_major,
                 version->version_minor,
                 version->version_patchlevel);
      drmFreeVersion(version);
      SISDRICloseScreen(pScreen);
      return FALSE;
    }
    if(version->version_minor >= 1) {
      /* Includes support for framebuffer memory allocation without sisfb */
      drm_sis_fb_t fb;
      fb.offset = pSIS->DRIheapstart;
      fb.size = pSIS->DRIheapend - pSIS->DRIheapstart;
      drmCommandWrite(pSIS->drmSubFD, DRM_SIS_FB_INIT, &fb, sizeof(fb));
      xf86DrvMsg(pScreen->myNum, X_INFO,
                 "[dri] DRI video RAM memory heap: 0x%lx to 0x%lx (%dKB)\n",
                 pSIS->DRIheapstart, pSIS->DRIheapend,
                 (int)((pSIS->DRIheapend - pSIS->DRIheapstart) >> 10));
    }
    drmFreeVersion(version);
  }
#endif

  pSISDRI->regs.size = SISIOMAPSIZE;
  pSISDRI->regs.map = 0;
  if(drmAddMap(pSIS->drmSubFD, (drmHandle)pSIS->IOAddress,
        	pSISDRI->regs.size, DRM_REGISTERS, 0,
        	&pSISDRI->regs.handle) < 0) {
     SISDRICloseScreen(pScreen);
     return FALSE;
  }

  xf86DrvMsg(pScreen->myNum, X_INFO, "[drm] Registers = 0x%08lx\n",
           pSISDRI->regs.handle);

  /* AGP */
  do {
    pSIS->agpWantedSize = pSIS->agpWantedPages * AGP_PAGE_SIZE;
    pSIS->agpSize = 0;
    pSIS->agpCmdBufSize = 0;
    pSISDRI->AGPCmdBufSize = 0;

    if(!pSIS->IsAGPCard)
       break;

    if(drmAgpAcquire(pSIS->drmSubFD) < 0) {
       xf86DrvMsg(pScreen->myNum, X_ERROR, "[drm] Failed to acquire AGP, AGP disabled\n");
       break;
    }

    if(pSIS->VGAEngine == SIS_315_VGA) {
#ifdef SIS315DRI
       /* Default to 1X agp mode in SIS315 */
       if(drmAgpEnable(pSIS->drmSubFD, drmAgpGetMode(pSIS->drmSubFD) & ~0x00000002) < 0) {
          xf86DrvMsg(pScreen->myNum, X_ERROR, "[drm] Failed to enable AGP, AGP disabled\n");
          break;
       }
#endif
    } else {
       /* TODO: default value is 2x? */
       if(drmAgpEnable(pSIS->drmSubFD, drmAgpGetMode(pSIS->drmSubFD) & ~0x0) < 0) {
          xf86DrvMsg(pScreen->myNum, X_ERROR, "[drm] Failed to enable AGP, AGP disabled\n");
          break;
       }
    }

    xf86DrvMsg(pScreen->myNum, X_INFO, "[drm] AGP enabled\n");

#define AGP_DEFAULT_SIZE_MB 8
#define AGP_DEFAULT_SIZE    (AGP_DEFAULT_SIZE_MB * 1024 * 1024)

    if(drmAgpAlloc(pSIS->drmSubFD, pSIS->agpWantedSize, 0, NULL, &pSIS->agpHandle) < 0) {
       xf86DrvMsg(pScreen->myNum, X_ERROR, "[drm] Failed to allocate %dMB AGP memory\n",
                  (int)(pSIS->agpWantedSize / (1024 * 1024)));
       if(pSIS->agpWantedSize > AGP_DEFAULT_SIZE) {
          xf86DrvMsg(pScreen->myNum, X_INFO, "[drm] Retrying with %dMB\n", AGP_DEFAULT_SIZE_MB);
          pSIS->agpWantedSize = AGP_DEFAULT_SIZE;
          if(drmAgpAlloc(pSIS->drmSubFD, pSIS->agpWantedSize, 0, NULL, &pSIS->agpHandle) < 0) {
             xf86DrvMsg(pScreen->myNum, X_ERROR, "[drm] Failed to allocate %dMB AGP memory, AGP disabled\n",
	  	        AGP_DEFAULT_SIZE_MB);
             drmAgpRelease(pSIS->drmSubFD);
             break;
	  }
       } else {
          drmAgpRelease(pSIS->drmSubFD);
          break;
       }
    }

    xf86DrvMsg(pScreen->myNum, X_INFO, "[drm] Allocated %dMB AGP memory\n",
    		(int)(pSIS->agpWantedSize / (1024 * 1024)));

    if(drmAgpBind(pSIS->drmSubFD, pSIS->agpHandle, 0) < 0) {
       xf86DrvMsg(pScreen->myNum, X_ERROR, "[drm] Failed to bind AGP memory\n");
       drmAgpFree(pSIS->drmSubFD, pSIS->agpHandle);
       if(pSIS->agpWantedSize > AGP_DEFAULT_SIZE) {
          xf86DrvMsg(pScreen->myNum, X_INFO, "[drm] Retrying with %dMB\n", AGP_DEFAULT_SIZE_MB);
          pSIS->agpWantedSize = AGP_DEFAULT_SIZE;
          if(drmAgpAlloc(pSIS->drmSubFD, pSIS->agpWantedSize, 0, NULL, &pSIS->agpHandle) < 0) {
	     xf86DrvMsg(pScreen->myNum, X_ERROR, "[drm] Failed to re-allocate AGP memory, AGP disabled\n");
	     drmAgpRelease(pSIS->drmSubFD);
             break;
	  } else if(drmAgpBind(pSIS->drmSubFD, pSIS->agpHandle, 0) < 0) {
	     xf86DrvMsg(pScreen->myNum, X_ERROR, "[drm] Failed to bind AGP memory again, AGP disabled\n");
             drmAgpFree(pSIS->drmSubFD, pSIS->agpHandle);
	     drmAgpRelease(pSIS->drmSubFD);
             break;
	  }
       } else {
          drmAgpRelease(pSIS->drmSubFD);
          break;
       }
    }

    xf86DrvMsg(pScreen->myNum, X_INFO, "[drm] Bound %dMB AGP memory\n",
	       (int)(pSIS->agpWantedSize / (1024 * 1024)));

    pSIS->agpSize = pSIS->agpWantedSize;
    pSIS->agpAddr = drmAgpBase(pSIS->drmSubFD);
    /* pSIS->agpBase = */

    pSISDRI->agp.size = pSIS->agpSize;
    if(drmAddMap(pSIS->drmSubFD, (drmHandle)0, pSISDRI->agp.size, DRM_AGP, 0, &pSISDRI->agp.handle) < 0) {
       xf86DrvMsg(pScreen->myNum, X_ERROR, "[drm] Failed to map public AGP area, AGP disabled\n");
       pSISDRI->agp.size = 0;
       break;
    }

    if(pSIS->VGAEngine == SIS_315_VGA) {
#ifdef SIS315DRI
       pSIS->agpVtxBufSize = AGP_VTXBUF_SIZE; /* = 2MB */
       pSIS->agpVtxBufAddr = pSIS->agpAddr;
       pSIS->agpVtxBufBase = pSIS->agpVtxBufAddr - pSIS->agpAddr + pSIS->agpBase;
       pSIS->agpVtxBufFree = 0;

       pSISDRI->AGPVtxBufOffset = pSIS->agpVtxBufAddr - pSIS->agpAddr;
       pSISDRI->AGPVtxBufSize = pSIS->agpVtxBufSize;

       drmSiSAgpInit(pSIS->drmSubFD, AGP_VTXBUF_SIZE,(pSIS->agpSize - AGP_VTXBUF_SIZE));
#endif
    } else {

       pSIS->agpCmdBufSize = AGP_CMDBUF_SIZE;
       pSIS->agpCmdBufAddr = pSIS->agpAddr;
       pSIS->agpCmdBufBase = pSIS->agpCmdBufAddr - pSIS->agpAddr + pSIS->agpBase;
       pSIS->agpCmdBufFree = 0;
         
       pSISDRI->AGPCmdBufOffset = pSIS->agpCmdBufAddr - pSIS->agpAddr;
       pSISDRI->AGPCmdBufSize = pSIS->agpCmdBufSize;

       drmSiSAgpInit(pSIS->drmSubFD, AGP_CMDBUF_SIZE,(pSIS->agpSize - AGP_CMDBUF_SIZE));
    }
  }
  while(0);
    
  /* enable IRQ */
  pSIS->irq = drmGetInterruptFromBusID(pSIS->drmSubFD,
           ((pciConfigPtr)pSIS->PciInfo->thisCard)->busnum,
           ((pciConfigPtr)pSIS->PciInfo->thisCard)->devnum,
           ((pciConfigPtr)pSIS->PciInfo->thisCard)->funcnum);

  if((drmCtlInstHandler(pSIS->drmSubFD, pSIS->irq)) != 0) {
     xf86DrvMsg(pScrn->scrnIndex, X_INFO,
         "[drm] Failed to add IRQ %d handler, stereo disabled\n",
         pSIS->irq);
     pSIS->irqEnabled = FALSE;
  } else {
     pSIS->irqEnabled = TRUE;
  }
  
  pSISDRI->irqEnabled = pSIS->irqEnabled;
  
  if(!(SISInitVisualConfigs(pScreen))) {
     SISDRICloseScreen(pScreen);
     return FALSE;
  }

  xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[dri] Visual configs initialized.\n" );

  return TRUE;
}

void
SISDRICloseScreen(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  SISPtr pSIS = SISPTR(pScrn);

  if(pSIS->VGAEngine == SIS_315_VGA) {
#ifdef SIS315DRI
     if(pSIS->cmdQ_SharedWritePortBackup) {
        pSIS->cmdQ_SharedWritePort = pSIS->cmdQ_SharedWritePortBackup;
     }
#endif
  } else {
     if(pSIS->cmdQueueLenPtrBackup) {
        pSIS->cmdQueueLenPtr = pSIS->cmdQueueLenPtrBackup;
        *(pSIS->cmdQueueLenPtr) = 0;
     }
  }

  DRICloseScreen(pScreen);

  if(pSIS->pDRIInfo) {
     if(pSIS->pDRIInfo->devPrivate) {
        xfree(pSIS->pDRIInfo->devPrivate);
        pSIS->pDRIInfo->devPrivate=0;
     }
     DRIDestroyInfoRec(pSIS->pDRIInfo);
     pSIS->pDRIInfo=0;
  }
  if(pSIS->pVisualConfigs) xfree(pSIS->pVisualConfigs);
  if(pSIS->pVisualConfigsPriv) xfree(pSIS->pVisualConfigsPriv);

  if(pSIS->agpSize){
     xf86DrvMsg(pScreen->myNum, X_INFO, "[drm] Freeing AGP memory\n");
     drmAgpFree(pSIS->drmSubFD, pSIS->agpHandle);
     xf86DrvMsg(pScreen->myNum, X_INFO, "[drm] Releasing AGP module\n");
     drmAgpRelease(pSIS->drmSubFD);
  }
}

/* TODO: xserver receives driver's swapping event and do something
 *       according the data initialized in this function
 */
static Bool
SISCreateContext(ScreenPtr pScreen, VisualPtr visual, 
          drmContext hwContext, void *pVisualConfigPriv,
          DRIContextType contextStore)
{
  return TRUE;
}

static void
SISDestroyContext(ScreenPtr pScreen, drmContext hwContext, 
           DRIContextType contextStore)
{
}

Bool
SISDRIFinishScreenInit(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  SISPtr pSiS = SISPTR(pScrn);
  SISDRIPtr pSISDRI;

  pSiS->pDRIInfo->driverSwapMethod = DRI_HIDE_X_CONTEXT;
  /* pSiS->pDRIInfo->driverSwapMethod = DRI_SERVER_SWAP; */

  pSISDRI=(SISDRIPtr)pSiS->pDRIInfo->devPrivate;
  pSISDRI->deviceID=pSiS->Chipset;  
  pSISDRI->width=pScrn->virtualX;
  pSISDRI->height=pScrn->virtualY;
  pSISDRI->mem=pScrn->videoRam*1024;
  pSISDRI->bytesPerPixel= (pScrn->bitsPerPixel+7) / 8; 
  /* TODO */
  pSISDRI->scrnX=pSISDRI->width;
  pSISDRI->scrnY=pSISDRI->height;
  
/*
  pSISDRI->textureOffset=pSiS->texOffset;
  pSISDRI->textureSize=pSiS->texSize;
  pSISDRI->fbOffset=pSiS->fbOffset;
  pSISDRI->backOffset=pSiS->backOffset;
  pSISDRI->depthOffset=pSiS->depthOffset;
*/

  /* set SAREA value */
  {
    SISSAREAPriv *saPriv;

    saPriv = (SISSAREAPriv*)DRIGetSAREAPrivate(pScreen);
    
    assert(saPriv);
          
    saPriv->CtxOwner = -1;
    saPriv->QueueLength = *(pSiS->cmdQueueLenPtr);
    pSiS->cmdQueueLenPtrBackup = pSiS->cmdQueueLenPtr;
    pSiS->cmdQueueLenPtr = &(saPriv->QueueLength);

    if(pSiS->VGAEngine == SIS_315_VGA) {
#ifdef SIS315DRI
       saPriv->AGPVtxBufNext = 0;
       saPriv->sharedWPoffset = pSiS->cmdQ_SharedWritePort_2D;
       pSiS->cmdQ_SharedWritePortBackup = pSiS->cmdQ_SharedWritePort;
       pSiS->cmdQ_SharedWritePort = &(saPriv->sharedWPoffset);
#endif
    } else {
       saPriv->AGPCmdBufNext = 0;

       /* frame control */
       saPriv->FrameCount = 0;
       *(unsigned long *)(pSiS->IOBase+0x8a2c) = 0;
       SiSIdle
    }
  }

  return DRIFinishScreenInit(pScreen);
}

static void
SISDRISwapContext(ScreenPtr pScreen, DRISyncType syncType, 
           DRIContextType oldContextType, void *oldContext,
           DRIContextType newContextType, void *newContext)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  SISPtr pSiS = SISPTR(pScrn);

#if 0
  if ((syncType==DRI_3D_SYNC) && (oldContextType==DRI_2D_CONTEXT) &&
      (newContextType==DRI_2D_CONTEXT)) { /* Entering from Wakeup */
    SISSwapContextPrivate(pScreen);
  }
  if ((syncType==DRI_2D_SYNC) && (oldContextType==DRI_NO_CONTEXT) &&
      (newContextType==DRI_2D_CONTEXT)) { /* Exiting from Block Handler */
    SISLostContext(pScreen);
  }
#endif
  
  /* mEndPrimitive */
  /* 
   * TODO: do this only if X-Server get lock. If kernel supports delayed
   * signal, needless to do this
   */
  if(pSiS->VGAEngine == SIS_315_VGA) {
#ifdef SIS315DRI
     /* ? */
#endif
  } else {
     *(pSiS->IOBase + 0x8B50) = 0xff;
     *(unsigned int *)(pSiS->IOBase + 0x8B60) = -1;
  }
}

static void
SISDRIInitBuffers(WindowPtr pWin, RegionPtr prgn, CARD32 index)
{
  ScreenPtr pScreen = pWin->drawable.pScreen;
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  SISPtr pSiS = SISPTR(pScrn);

  if(pSiS->VGAEngine == SIS_315_VGA) {
#ifdef SIS315DRI
     SiS315Idle		/* 315 series */
#endif
  } else {
     SiSIdle		/* 300 series */
  }
}

static void
SISDRIMoveBuffers(WindowPtr pParent, DDXPointRec ptOldOrg, 
           RegionPtr prgnSrc, CARD32 index)
{
  ScreenPtr pScreen = pParent->drawable.pScreen;
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  SISPtr pSiS = SISPTR(pScrn);

  if(pSiS->VGAEngine == SIS_315_VGA) {
#ifdef SIS315DRI
     SiS315Idle		/* 315 series */
#endif
  } else {
     SiSIdle		/* 300 series */
  }
}

#if 0
void SISLostContext(ScreenPtr pScreen) 
{
}

void SISSwapContextPrivate(ScreenPtr pScreen) 
{
}
#endif
