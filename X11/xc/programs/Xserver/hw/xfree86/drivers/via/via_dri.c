/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_dri.c,v 1.9 2004/02/08 17:57:10 tsi Exp $ */
/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Priv.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#define _XF86DRI_SERVER_
#include "GL/glxtokens.h"
#include "sarea.h"

#include "via_driver.h"
#include "via_dri.h"
#include "via_common.h"
#include "xf86drm.h"

extern void GlxSetVisualConfigs(
    int nconfigs,
    __GLXvisualConfig *configs,
    void **configprivs
);

#define VIDEO	0 
#define AGP		1
#define AGP_PAGE_SIZE 4096
#define AGP_PAGES 8192
#define AGP_SIZE (AGP_PAGE_SIZE * AGP_PAGES)
#define AGP_CMDBUF_PAGES 256
#define AGP_CMDBUF_SIZE (AGP_PAGE_SIZE * AGP_CMDBUF_PAGES)

static char VIAKernelDriverName[] = "via";
static char VIAClientDriverName[] = "via";
int test_alloc_FB(ScreenPtr pScreen, VIAPtr pVia, int Size);
int test_alloc_AGP(ScreenPtr pScreen, VIAPtr pVia, int Size);
static Bool VIAInitVisualConfigs(ScreenPtr pScreen);
static Bool VIADRIAgpInit(ScreenPtr pScreen, VIAPtr pVia);
static Bool VIADRIPciInit(ScreenPtr pScreen, VIAPtr pVia);
static Bool VIADRIFBInit(ScreenPtr pScreen, VIAPtr pVia);
static Bool VIADRIKernelInit(ScreenPtr pScreen, VIAPtr pVia);
static Bool VIADRIMapInit(ScreenPtr pScreen, VIAPtr pVia);

static Bool VIACreateContext(ScreenPtr pScreen, VisualPtr visual, 
                   drmContext hwContext, void *pVisualConfigPriv,
                   DRIContextType contextStore);
static void VIADestroyContext(ScreenPtr pScreen, drmContext hwContext,
                   DRIContextType contextStore);
static void VIADRISwapContext(ScreenPtr pScreen, DRISyncType syncType, 
                   DRIContextType readContextType, 
                   void *readContextStore,
                   DRIContextType writeContextType, 
                   void *writeContextStore);
static void VIADRIInitBuffers(WindowPtr pWin, RegionPtr prgn, CARD32 index);
static void VIADRIMoveBuffers(WindowPtr pParent, DDXPointRec ptOldOrg, 
                   RegionPtr prgnSrc, CARD32 index);


static Bool VIADRIAgpInit(ScreenPtr pScreen, VIAPtr pVia)
{
    unsigned long  agp_phys;
    drmAddress agpaddr;
    VIADRIPtr pVIADRI;
    DRIInfoPtr pDRIInfo;
    pDRIInfo = pVia->pDRIInfo;
    pVIADRI = pDRIInfo->devPrivate;
    pVia->agpSize = 0;

    if (drmAgpAcquire(pVia->drmFD) < 0) {
        xf86DrvMsg(pScreen->myNum, X_ERROR, "[drm] drmAgpAcquire failed %d\n", errno);
        return FALSE;
    }

    if (drmAgpEnable(pVia->drmFD, drmAgpGetMode(pVia->drmFD)&~0x0) < 0) {
         xf86DrvMsg(pScreen->myNum, X_ERROR, "[drm] drmAgpEnable failed\n");
        return FALSE;
    }
    
    xf86DrvMsg(pScreen->myNum, X_INFO, "[drm] drmAgpEnabled succeeded\n");

    if (drmAgpAlloc(pVia->drmFD, AGP_SIZE, 0, &agp_phys, &pVia->agpHandle) < 0) {
        xf86DrvMsg(pScreen->myNum, X_ERROR,
                 "[drm] drmAgpAlloc failed\n");
        drmAgpRelease(pVia->drmFD);
        return FALSE;
    }
   
    if (drmAgpBind(pVia->drmFD, pVia->agpHandle, 0) < 0) {
        xf86DrvMsg(pScreen->myNum, X_ERROR,
                 "[drm] drmAgpBind failed\n");
        drmAgpFree(pVia->drmFD, pVia->agpHandle);
        drmAgpRelease(pVia->drmFD);

        return FALSE;
    }

    pVia->agpSize = AGP_SIZE;
    pVia->agpAddr = drmAgpBase(pVia->drmFD);
    xf86DrvMsg(pScreen->myNum, X_INFO,
                 "[drm] agpAddr = 0x%08lx\n",pVia->agpAddr);
		 
    pVIADRI->agp.size = pVia->agpSize;
    if (drmAddMap(pVia->drmFD, (drmHandle)0,
                 pVIADRI->agp.size, DRM_AGP, 0, 
                 &pVIADRI->agp.handle) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
	    "[drm] Failed to map public agp area\n");
        pVIADRI->agp.size = 0;
	drmAgpUnbind(pVia->drmFD, pVia->agpHandle);
	drmAgpFree(pVia->drmFD, pVia->agpHandle);
	drmAgpRelease(pVia->drmFD);
	return FALSE;
    }  
    /* Map AGP from kernel to Xserver - Not really needed */
    drmMap(pVia->drmFD, pVIADRI->agp.handle, pVIADRI->agp.size, &agpaddr);
    pVia->agpMappedAddr = agpaddr;

    xf86DrvMsg(pScreen->myNum, X_INFO, 
                "[drm] agpBase = %p\n", pVia->agpBase);
    xf86DrvMsg(pScreen->myNum, X_INFO, 
                "[drm] agpAddr = 0x%08lx\n", pVia->agpAddr);
    xf86DrvMsg(pScreen->myNum, X_INFO, 
                "[drm] agpSize = 0x%08x\n", pVia->agpSize);
    xf86DrvMsg(pScreen->myNum, X_INFO, 
                "[drm] agp physical addr = 0x%08lx\n", agp_phys);
    
    {
	drmViaAgp agp;
	agp.offset = 0;
	agp.size = AGP_SIZE;
	if (drmCommandWrite(pVia->drmFD, DRM_VIA_AGP_INIT, &agp,
			    sizeof(drmViaAgp)) < 0) {
	    drmUnmap(agpaddr,pVia->agpSize);
	    drmRmMap(pVia->drmFD,pVIADRI->agp.handle);
	    drmAgpUnbind(pVia->drmFD, pVia->agpHandle);
	    drmAgpFree(pVia->drmFD, pVia->agpHandle);
	    drmAgpRelease(pVia->drmFD);
	    return FALSE;
	}
    }
	
    return TRUE;
  
}
static Bool VIADRIFBInit(ScreenPtr pScreen, VIAPtr pVia)
{   
    int FBSize = pVia->FBFreeEnd-pVia->FBFreeStart;
    int FBOffset = pVia->FBFreeStart; 
    VIADRIPtr pVIADRI = pVia->pDRIInfo->devPrivate;
    pVIADRI->fbOffset = FBOffset;
    pVIADRI->fbSize = pVia->videoRambytes;
    
    {
	drmViaFb fb;
	fb.offset = FBOffset;
	fb.size = FBSize;
	
	if (drmCommandWrite(pVia->drmFD, DRM_VIA_FB_INIT, &fb,
			    sizeof(drmViaFb)) < 0) {
	    xf86DrvMsg(pScreen->myNum, X_ERROR,
		       "[drm] failed to init frame buffer area\n");
	    return FALSE;
	} else {
	    xf86DrvMsg(pScreen->myNum, X_INFO,
		       "[drm] FBFreeStart= 0x%08x FBFreeEnd= 0x%08x "
		       "FBSize= 0x%08x\n",
		       pVia->FBFreeStart, pVia->FBFreeEnd, FBSize);
	    return TRUE;	
	}   
    }
}

static Bool VIADRIPciInit(ScreenPtr pScreen, VIAPtr pVia)
{
    return TRUE;	
}

static Bool
VIAInitVisualConfigs(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    int numConfigs = 0;
    __GLXvisualConfig *pConfigs = 0;
    VIAConfigPrivPtr pVIAConfigs = 0;
    VIAConfigPrivPtr *pVIAConfigPtrs = 0;
    int i, db, stencil, accum;

    switch (pScrn->bitsPerPixel) {
	case 8:
	case 24:
	    break;
	case 16:
	case 32:
	    numConfigs = 8;
	    if (!(pConfigs = (__GLXvisualConfig*)xcalloc(sizeof(__GLXvisualConfig),
						   numConfigs)))
		return FALSE;
	    if (!(pVIAConfigs = (VIAConfigPrivPtr)xcalloc(sizeof(VIAConfigPrivRec),
						    numConfigs))) {
    		xfree(pConfigs);
    		return FALSE;
	    }
	    if (!(pVIAConfigPtrs = (VIAConfigPrivPtr*)xcalloc(sizeof(VIAConfigPrivPtr),
							  numConfigs))) {
    		xfree(pConfigs);
    		xfree(pVIAConfigs);
    		return FALSE;
	    }
	    for (i=0; i<numConfigs; i++) 
    		pVIAConfigPtrs[i] = &pVIAConfigs[i];

	    i = 0;
	    for (accum = 0; accum <= 1; accum++) {
    		for (stencil=0; stencil<=1; stencil++) {
    		    for (db = 0; db <= 1; db++) {
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
        		
			if (accum) {
        		    pConfigs[i].accumRedSize = 16;
        		    pConfigs[i].accumGreenSize = 16;
        		    pConfigs[i].accumBlueSize = 16;
        		    pConfigs[i].accumAlphaSize = 16;
        		}
			else {
        		    pConfigs[i].accumRedSize = 0;
        		    pConfigs[i].accumGreenSize = 0;
        		    pConfigs[i].accumBlueSize = 0;
        		    pConfigs[i].accumAlphaSize = 0;
        		}
        		if (db)
        		    pConfigs[i].doubleBuffer = TRUE;
        		else
        		    pConfigs[i].doubleBuffer = FALSE;
        		
			pConfigs[i].stereo = FALSE;
        		pConfigs[i].bufferSize = -1;
        		
			switch (stencil) {
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
	
	if (i != numConfigs) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"[dri] Incorrect initialization of visuals.  Disabling DRI.\n");
    		return FALSE;
	}
	    
	break;
    }
 
    pVia->numVisualConfigs = numConfigs;
    pVia->pVisualConfigs = pConfigs;
    pVia->pVisualConfigsPriv = pVIAConfigs;
    GlxSetVisualConfigs(numConfigs, pConfigs, (void**)pVIAConfigPtrs);

    return TRUE;
}

Bool VIADRIScreenInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    DRIInfoPtr pDRIInfo;
    VIADRIPtr pVIADRI;

    /* Check that the GLX, DRI, and DRM modules have been loaded by testing
    * for canonical symbols in each module. */
    if (!xf86LoaderCheckSymbol("GlxSetVisualConfigs")) return FALSE;
    if (!xf86LoaderCheckSymbol("DRIScreenInit"))       return FALSE;
    if (!xf86LoaderCheckSymbol("drmAvailable"))        return FALSE;
    if (!xf86LoaderCheckSymbol("DRIQueryVersion")) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
                 "[dri] VIADRIScreenInit failed (libdri.a too old)\n");
        return FALSE;
    }
    
    /* Check the DRI version */
    {
        int major, minor, patch;
        DRIQueryVersion(&major, &minor, &patch);
        if (major != 4 || minor < 0) {
            xf86DrvMsg(pScreen->myNum, X_ERROR,
                    "[dri] VIADRIScreenInit failed because of a version mismatch.\n"
                    "[dri] libDRI version is %d.%d.%d but version 4.0.x is needed.\n"
                    "[dri] Disabling DRI.\n",
                    major, minor, patch);
            return FALSE;
        }
    }

    pDRIInfo = DRICreateInfoRec();
    
    if (!pDRIInfo) return FALSE;
    
    pVia->pDRIInfo = pDRIInfo;
    pDRIInfo->drmDriverName = VIAKernelDriverName;
    pDRIInfo->clientDriverName = VIAClientDriverName;
    pDRIInfo->busIdString = xalloc(64);
    sprintf(pDRIInfo->busIdString, "PCI:%d:%d:%d",
        ((pciConfigPtr)pVia->PciInfo->thisCard)->busnum,
        ((pciConfigPtr)pVia->PciInfo->thisCard)->devnum,
        ((pciConfigPtr)pVia->PciInfo->thisCard)->funcnum);
    pDRIInfo->ddxDriverMajorVersion = VIA_VERSION_MAJOR;
    pDRIInfo->ddxDriverMinorVersion = VIA_VERSION_MINOR;
    pDRIInfo->ddxDriverPatchVersion = PATCHLEVEL;
    pDRIInfo->frameBufferPhysicalAddress = pVia->FrameBufferBase;
    pDRIInfo->frameBufferSize = pVia->videoRambytes;  
  
    pDRIInfo->frameBufferStride = (pScrn->displayWidth *
					    pScrn->bitsPerPixel / 8);
    pDRIInfo->ddxDrawableTableEntry = VIA_MAX_DRAWABLES;

    if (SAREA_MAX_DRAWABLES < VIA_MAX_DRAWABLES)
	pDRIInfo->maxDrawableTableEntry = SAREA_MAX_DRAWABLES;
    else
	pDRIInfo->maxDrawableTableEntry = VIA_MAX_DRAWABLES;

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
    if (sizeof(XF86DRISAREARec)+sizeof(VIASAREAPriv) > SAREA_MAX) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Data does not fit in SAREA\n");
	return FALSE;
    }
    pDRIInfo->SAREASize = SAREA_MAX;
#endif

    if (!(pVIADRI = (VIADRIPtr)xcalloc(sizeof(VIADRIRec),1))) {
	DRIDestroyInfoRec(pVia->pDRIInfo);
	pVia->pDRIInfo=0;
	return FALSE;
    }
    pDRIInfo->devPrivate = pVIADRI;
    pDRIInfo->devPrivateSize = sizeof(VIADRIRec);
    pDRIInfo->contextSize = sizeof(VIADRIContextRec);

    pDRIInfo->CreateContext = VIACreateContext;
    pDRIInfo->DestroyContext = VIADestroyContext;
    pDRIInfo->SwapContext = VIADRISwapContext;
    pDRIInfo->InitBuffers = VIADRIInitBuffers;
    pDRIInfo->MoveBuffers = VIADRIMoveBuffers;
    pDRIInfo->bufferRequests = DRI_ALL_WINDOWS;

    if (!DRIScreenInit(pScreen, pDRIInfo, &pVia->drmFD)) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
	    "[dri] DRIScreenInit failed.  Disabling DRI.\n");
	xfree(pDRIInfo->devPrivate);
	pDRIInfo->devPrivate=0;
	DRIDestroyInfoRec(pVia->pDRIInfo);
	pVia->pDRIInfo=0;
	pVia->drmFD = -1;
	return FALSE;
    }

	   
    pVia->IsPCI = !VIADRIAgpInit(pScreen, pVia);
  
    if (pVia->IsPCI) {
	VIADRIPciInit(pScreen, pVia);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[dri] use pci.\n" );
    }
    else
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[dri] use agp.\n" );

    if (!(VIADRIFBInit(pScreen, pVia))) {
	VIADRICloseScreen(pScreen);
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "[dri] frame buffer initialize fial .\n" );
	return FALSE;
    }
    
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[dri] frame buffer initialized.\n" );
  
    if (!(VIAInitVisualConfigs(pScreen))) {
	VIADRICloseScreen(pScreen);
	return FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[dri] visual configs initialized.\n" );
  
    /* DRIScreenInit doesn't add all the common mappings.  Add additional mappings here. */
    if (!VIADRIMapInit(pScreen, pVia)) {
	VIADRICloseScreen(pScreen);
	return FALSE;
    }
    pVIADRI->regs.size = VIA_MMIO_REGSIZE;
    pVIADRI->regs.map = 0;
    pVIADRI->regs.handle = pVia->registerHandle;
    xf86DrvMsg(pScreen->myNum, X_INFO, "[drm] mmio Registers = 0x%08lx\n",
	pVIADRI->regs.handle);
    
    pVIADRI->drixinerama = pVia->drixinerama;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[dri] mmio mapped.\n" );

    return TRUE;
}

void
VIADRICloseScreen(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);

    if (pVia->agpSize) {
	drmUnmap(pVia->agpMappedAddr,pVia->agpSize);
	drmRmMap(pVia->drmFD,pVia->agpHandle);
	drmAgpUnbind(pVia->drmFD, pVia->agpHandle);
	xf86DrvMsg(pScreen->myNum, X_INFO, "[drm] Freeing agp memory\n");
	drmAgpFree(pVia->drmFD, pVia->agpHandle);
	xf86DrvMsg(pScreen->myNum, X_INFO, "[drm] Releasing agp module\n");
	drmAgpRelease(pVia->drmFD);
    }

    DRICloseScreen(pScreen);
    
    if (pVia->pDRIInfo) {
	if (pVia->pDRIInfo->devPrivate) {
    	    xfree(pVia->pDRIInfo->devPrivate);
    	    pVia->pDRIInfo->devPrivate=0;
	}
	DRIDestroyInfoRec(pVia->pDRIInfo);
	pVia->pDRIInfo=0;
    }
    
    if (pVia->pVisualConfigs) {
	xfree(pVia->pVisualConfigs);
	pVia->pVisualConfigs = NULL;
    }
    if (pVia->pVisualConfigsPriv) {
	xfree(pVia->pVisualConfigsPriv);
	pVia->pVisualConfigsPriv = NULL;
    }
}

/* TODO: xserver receives driver's swapping event and does something
 *       according the data initialized in this function. 
 */
static Bool
VIACreateContext(ScreenPtr pScreen, VisualPtr visual,
          drmContext hwContext, void *pVisualConfigPriv,
          DRIContextType contextStore)
{
    return TRUE;
}

static void
VIADestroyContext(ScreenPtr pScreen, drmContext hwContext, 
           DRIContextType contextStore)
{
}

Bool
VIADRIFinishScreenInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    VIADRIPtr pVIADRI;

    pVia->pDRIInfo->driverSwapMethod = DRI_HIDE_X_CONTEXT;
    
    DRIFinishScreenInit(pScreen);
    
    if (!VIADRIKernelInit(pScreen, pVia)) {
	VIADRICloseScreen(pScreen);
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO, "[dri] kernel data initialized.\n");

    /* set SAREA value */
    {
	VIASAREAPriv *saPriv;

	saPriv=(VIASAREAPriv*)DRIGetSAREAPrivate(pScreen);
	assert(saPriv);
	memset(saPriv, 0, sizeof(*saPriv));
	saPriv->CtxOwner = -1;
    }
    pVIADRI=(VIADRIPtr)pVia->pDRIInfo->devPrivate;
    pVIADRI->deviceID=pVia->Chipset;  
    pVIADRI->width=pScrn->virtualX;
    pVIADRI->height=pScrn->virtualY;
    pVIADRI->mem=pScrn->videoRam*1024;
    pVIADRI->bytesPerPixel= (pScrn->bitsPerPixel+7) / 8; 
    pVIADRI->sarea_priv_offset = sizeof(XF86DRISAREARec);
    /* TODO */
    pVIADRI->scrnX=pVIADRI->width;
    pVIADRI->scrnY=pVIADRI->height;

    return TRUE;
}

static void
VIADRISwapContext(ScreenPtr pScreen, DRISyncType syncType, 
           DRIContextType oldContextType, void *oldContext,
           DRIContextType newContextType, void *newContext)
{
  /*ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  VIAPtr pVia = VIAPTR(pScrn);
  */   
  return;
}

static void
VIADRIInitBuffers(WindowPtr pWin, RegionPtr prgn, CARD32 index)
{
  /*ScreenPtr pScreen = pWin->drawable.pScreen;
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  VIAPtr pVia = VIAPTR(pScrn);
  */
  return;
}

static void
VIADRIMoveBuffers(WindowPtr pParent, DDXPointRec ptOldOrg, 
           RegionPtr prgnSrc, CARD32 index)
{
  /*ScreenPtr pScreen = pParent->drawable.pScreen;
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  VIAPtr pVia = VIAPTR(pScrn);
  */
  return;
}

/* Initialize the kernel data structures. */
static Bool VIADRIKernelInit(ScreenPtr pScreen, VIAPtr pVia)
{
    drmViaInit drmInfo;
    memset(&drmInfo, 0, sizeof(drmViaInit));
    drmInfo.func = VIA_INIT_MAP;
    drmInfo.sarea_priv_offset   = sizeof(XF86DRISAREARec);
    drmInfo.fb_offset           = pVia->FrameBufferBase;
    drmInfo.mmio_offset         = pVia->registerHandle;
    if (pVia->IsPCI)
	drmInfo.agpAddr = (CARD32)NULL;
    else
	drmInfo.agpAddr = (CARD32)pVia->agpAddr;

	if ((drmCommandWrite(pVia->drmFD, DRM_VIA_MAP_INIT,&drmInfo,
			     sizeof(drmViaInit))) < 0)
	    return FALSE;
	     

    return TRUE;
}
/* Add a map for the MMIO registers */
static Bool VIADRIMapInit(ScreenPtr pScreen, VIAPtr pVia)
{
    int flags = 0;

    if (drmAddMap(pVia->drmFD, pVia->MmioBase, VIA_MMIO_REGSIZE,
		  DRM_REGISTERS, flags, &pVia->registerHandle) < 0) {
	return FALSE;
    }
    
    xf86DrvMsg(pScreen->myNum, X_INFO,
	"[drm] register handle = 0x%08lx\n", pVia->registerHandle);

    return TRUE;
}
