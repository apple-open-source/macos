/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tdfx/tdfx_dri.c,v 1.25 2003/02/08 21:26:59 dawes Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Priv.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "fb.h"
#include "miline.h"
#include "GL/glxtokens.h"
#include "tdfx.h"
#include "tdfx_dri.h"
#include "tdfx_dripriv.h"

static char TDFXKernelDriverName[] = "tdfx";
static char TDFXClientDriverName[] = "tdfx";

static Bool TDFXCreateContext(ScreenPtr pScreen, VisualPtr visual,
			      drmContext hwContext, void *pVisualConfigPriv,
			      DRIContextType contextStore);
static void TDFXDestroyContext(ScreenPtr pScreen, drmContext hwContext,
			       DRIContextType contextStore);
static void TDFXDRISwapContext(ScreenPtr pScreen, DRISyncType syncType,
			       DRIContextType readContextType,
			       void *readContextStore,
			       DRIContextType writeContextType,
			       void *writeContextStore);
static Bool TDFXDRIOpenFullScreen(ScreenPtr pScreen);
static Bool TDFXDRICloseFullScreen(ScreenPtr pScreen);
static void TDFXDRIInitBuffers(WindowPtr pWin, RegionPtr prgn, CARD32 index);
static void TDFXDRIMoveBuffers(WindowPtr pParent, DDXPointRec ptOldOrg,
			       RegionPtr prgnSrc, CARD32 index);
static void TDFXDRITransitionTo2d(ScreenPtr pScreen);
static void TDFXDRITransitionTo3d(ScreenPtr pScreen);

static Bool
TDFXInitVisualConfigs(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  TDFXPtr pTDFX = TDFXPTR(pScrn);
  int numConfigs = 0;
  __GLXvisualConfig *pConfigs = 0;
  TDFXConfigPrivPtr pTDFXConfigs = 0;
  TDFXConfigPrivPtr *pTDFXConfigPtrs = 0;
  int i, db, stencil, accum, depth;

  switch (pScrn->bitsPerPixel) {
  case 8:
  case 16:
    numConfigs = 16;

    if (!(pConfigs = (__GLXvisualConfig*)xcalloc(sizeof(__GLXvisualConfig),
						   numConfigs))) {
      return FALSE;
    }
    if (!(pTDFXConfigs = (TDFXConfigPrivPtr)xcalloc(sizeof(TDFXConfigPrivRec),
						     numConfigs))) {
      xfree(pConfigs);
      return FALSE;
    }
    if (!(pTDFXConfigPtrs = (TDFXConfigPrivPtr*)xcalloc(sizeof(TDFXConfigPrivPtr),
							 numConfigs))) {
      xfree(pConfigs);
      xfree(pTDFXConfigs);
      return FALSE;
    }
    for (i=0; i<numConfigs; i++)
      pTDFXConfigPtrs[i] = &pTDFXConfigs[i];

    i=0;
    depth=1;
    for (db = 0; db <=1; db++) {
      for (depth = 0; depth<=1; depth++) {
	for (accum = 0; accum <= 1; accum++) {
	  for (stencil = 0; stencil <= 1; stencil++) {
	    pConfigs[i].vid = -1;
	    pConfigs[i].class = -1;
	    pConfigs[i].rgba = TRUE;
	    pConfigs[i].redSize = 5;
	    pConfigs[i].greenSize = 6;
	    pConfigs[i].blueSize = 5;
	    pConfigs[i].redMask = 0x0000F800;
	    pConfigs[i].greenMask = 0x000007E0;
	    pConfigs[i].blueMask = 0x0000001F;
	    pConfigs[i].alphaMask = 0;
	    if (accum) {
	      pConfigs[i].accumRedSize = 16;
	      pConfigs[i].accumGreenSize = 16;
	      pConfigs[i].accumBlueSize = 16;
	      pConfigs[i].accumAlphaSize = 0;
	    } else {
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
	    pConfigs[i].bufferSize = 16;
	    if (depth) {
	      if (pTDFX->cpp>2)
		pConfigs[i].depthSize = 24;
	      else
		pConfigs[i].depthSize = 16;
	    } else {
	      pConfigs[i].depthSize = 0;
	    }
	    if (stencil)
	      pConfigs[i].stencilSize = 8;
	    else
	      pConfigs[i].stencilSize = 0;
	    pConfigs[i].auxBuffers = 0;
	    pConfigs[i].level = 0;
	    if (stencil || accum)
	      pConfigs[i].visualRating = GLX_SLOW_VISUAL_EXT;
	    else
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
    }
    if (i!=numConfigs) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
                 "[dri] TDFXInitVisualConfigs: wrong number of visuals\n");
      return FALSE;
    }
    break; /* 16bpp */

  case 24:
  case 32:
    numConfigs = 8;

    pConfigs = (__GLXvisualConfig*) xcalloc(sizeof(__GLXvisualConfig), numConfigs);
    if (!pConfigs)
      return FALSE;

    pTDFXConfigs = (TDFXConfigPrivPtr) xcalloc(sizeof(TDFXConfigPrivRec), numConfigs);
    if (!pTDFXConfigs) {
      xfree(pConfigs);
      return FALSE;
    }

    pTDFXConfigPtrs = (TDFXConfigPrivPtr *) xcalloc(sizeof(TDFXConfigPrivPtr), numConfigs);
    if (!pTDFXConfigPtrs) {
      xfree(pConfigs);
      xfree(pTDFXConfigs);
      return FALSE;
    }

    for (i = 0; i < numConfigs; i++)
      pTDFXConfigPtrs[i] = &pTDFXConfigs[i];

    i=0;
    for (db = 0; db <=1; db++) {
      for (depth = 0; depth<=1; depth++) {
         /*stencil = depth;*/  /* Z and stencil share the same memory */
	for (accum = 0; accum <= 1; accum++) {
           /*for (stencil = 0; stencil <=1; stencil++) {*/
           stencil = depth;
	    pConfigs[i].vid = -1;
	    pConfigs[i].class = -1;
	    pConfigs[i].rgba = TRUE;
	    pConfigs[i].redSize = 8;
	    pConfigs[i].greenSize = 8;
	    pConfigs[i].blueSize = 8;
	    pConfigs[i].alphaSize = (pScrn->bitsPerPixel==32) ? 8 : 0;
	    pConfigs[i].redMask   = 0x00ff0000;
	    pConfigs[i].greenMask = 0x0000ff00;
	    pConfigs[i].blueMask  = 0x000000ff;
	    pConfigs[i].alphaMask = (pScrn->bitsPerPixel==32) ? 0xff000000 : 0;
	    if (accum) {
	      pConfigs[i].accumRedSize = 16;
	      pConfigs[i].accumGreenSize = 16;
	      pConfigs[i].accumBlueSize = 16;
	      pConfigs[i].accumAlphaSize = (pScrn->bitsPerPixel==32) ? 16 : 0;
	    } else {
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
	    pConfigs[i].bufferSize = (pScrn->bitsPerPixel==32) ? 32 : 24;
	    if (depth) {
	      if (pTDFX->cpp > 2)
		pConfigs[i].depthSize = 24;
	      else
		pConfigs[i].depthSize = 16;
	    } else {
	      pConfigs[i].depthSize = 0;
	    }
	    if (stencil)
	      pConfigs[i].stencilSize = 8;
	    else
	      pConfigs[i].stencilSize = 0;
	    pConfigs[i].auxBuffers = 0;
	    pConfigs[i].level = 0;
	    if (accum)
	      pConfigs[i].visualRating = GLX_SLOW_VISUAL_EXT;
	    else
	      pConfigs[i].visualRating = GLX_NONE_EXT;
	    pConfigs[i].transparentPixel = 0;
	    pConfigs[i].transparentRed = 0;
	    pConfigs[i].transparentGreen = 0;
	    pConfigs[i].transparentBlue = 0;
	    pConfigs[i].transparentAlpha = 0;
	    pConfigs[i].transparentIndex = 0;
	    i++;
         /*}*/
	}
      }
    }
    if (i!=numConfigs) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
                 "[dri] TDFXInitVisualConfigs: wrong number of visuals\n");
      return FALSE;
    }
    break;
  }
  pTDFX->numVisualConfigs = numConfigs;
  pTDFX->pVisualConfigs = pConfigs;
  pTDFX->pVisualConfigsPriv = pTDFXConfigs;
  GlxSetVisualConfigs(numConfigs, pConfigs, (void**)pTDFXConfigPtrs);
  return TRUE;
}

static void
TDFXDoWakeupHandler(int screenNum, pointer wakeupData, unsigned long result,
		    pointer pReadmask)
{
  ScreenPtr pScreen = screenInfo.screens[screenNum];
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

  TDFXNeedSync(pScrn);
}

static void
TDFXDoBlockHandler(int screenNum, pointer blockData, pointer pTimeout,
		  pointer pReadmask)
{
  ScreenPtr pScreen = screenInfo.screens[screenNum];
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

  TDFXCheckSync(pScrn);
}

Bool TDFXDRIScreenInit(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  TDFXPtr pTDFX = TDFXPTR(pScrn);
  DRIInfoPtr pDRIInfo;
  TDFXDRIPtr pTDFXDRI;
  Bool bppOk = FALSE;

  switch (pScrn->bitsPerPixel) {
  case 16:
    bppOk = TRUE;
    break;
  case 32:
    if (pTDFX->ChipType > PCI_CHIP_VOODOO3) {
      bppOk = TRUE;
    }
    break;
  }
  if (!bppOk) {
    xf86DrvMsg(pScreen->myNum, X_ERROR,
            "[dri] tdfx DRI not supported in %d bpp mode, disabling DRI.\n",
            (pScrn->bitsPerPixel));
    if (pTDFX->ChipType <= PCI_CHIP_VOODOO3) {
      xf86DrvMsg(pScreen->myNum, X_INFO,
              "[dri] To use DRI, invoke the server using 16 bpp\n"
	      "\t(-depth 15 or -depth 16).\n");
    } else {
      xf86DrvMsg(pScreen->myNum, X_INFO,
              "[dri] To use DRI, invoke the server using 16 bpp\n"
	      "\t(-depth 15 or -depth 16) or 32 bpp (-depth 24 -fbbpp 32).\n");
    }
    return FALSE;
  }

    /* Check that the GLX, DRI, and DRM modules have been loaded by testing
       for canonical symbols in each module. */
    if (!xf86LoaderCheckSymbol("GlxSetVisualConfigs")) return FALSE;
    if (!xf86LoaderCheckSymbol("DRIScreenInit"))       return FALSE;
    if (!xf86LoaderCheckSymbol("drmAvailable"))        return FALSE;
    if (!xf86LoaderCheckSymbol("DRIQueryVersion")) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
                 "TDFXDRIScreenInit failed (libdri.a too old)\n");
      return FALSE;
    }

  /* Check the DRI version */
  {
    int major, minor, patch;
    DRIQueryVersion(&major, &minor, &patch);
    if (major != 4 || minor < 0) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
                 "[dri] TDFXDRIScreenInit failed because of a version mismatch.\n"
                 "[dri] libDRI version is %d.%d.%d but version 4.0.x is needed.\n"
                 "[dri] Disabling the DRI.\n",
                 major, minor, patch);
      return FALSE;
    }
  }

  pDRIInfo = DRICreateInfoRec();
  if (!pDRIInfo) {
    xf86DrvMsg(pScreen->myNum, X_ERROR,
               "[dri] DRICreateInfoRect() failed, disabling DRI.\n");
    return FALSE;
  }

  pTDFX->pDRIInfo = pDRIInfo;

  pDRIInfo->drmDriverName = TDFXKernelDriverName;
  pDRIInfo->clientDriverName = TDFXClientDriverName;
  pDRIInfo->busIdString = xalloc(64);
  sprintf(pDRIInfo->busIdString, "PCI:%d:%d:%d",
	  ((pciConfigPtr)pTDFX->PciInfo->thisCard)->busnum,
	  ((pciConfigPtr)pTDFX->PciInfo->thisCard)->devnum,
	  ((pciConfigPtr)pTDFX->PciInfo->thisCard)->funcnum);
  pDRIInfo->ddxDriverMajorVersion = TDFX_MAJOR_VERSION;
  pDRIInfo->ddxDriverMinorVersion = TDFX_MINOR_VERSION;
  pDRIInfo->ddxDriverPatchVersion = TDFX_PATCHLEVEL;
  pDRIInfo->frameBufferPhysicalAddress = pTDFX->LinearAddr[0];
  pDRIInfo->frameBufferSize = pTDFX->FbMapSize;
  pDRIInfo->frameBufferStride = pTDFX->stride;
  pDRIInfo->ddxDrawableTableEntry = TDFX_MAX_DRAWABLES;

  pDRIInfo->wrap.ValidateTree = 0;
  pDRIInfo->wrap.PostValidateTree = 0;
  pDRIInfo->wrap.BlockHandler = TDFXDoBlockHandler;
  pDRIInfo->wrap.WakeupHandler = TDFXDoWakeupHandler;

  if (SAREA_MAX_DRAWABLES < TDFX_MAX_DRAWABLES)
    pDRIInfo->maxDrawableTableEntry = SAREA_MAX_DRAWABLES;
  else
    pDRIInfo->maxDrawableTableEntry = TDFX_MAX_DRAWABLES;

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
  if (sizeof(XF86DRISAREARec)+sizeof(TDFXSAREAPriv)>SAREA_MAX) {
    xf86DrvMsg(pScreen->myNum, X_ERROR, "Data does not fit in SAREA\n");
    return FALSE;
  }
  pDRIInfo->SAREASize = SAREA_MAX;
#endif

  if (!(pTDFXDRI = (TDFXDRIPtr)xcalloc(sizeof(TDFXDRIRec),1))) {
    xf86DrvMsg(pScreen->myNum, X_ERROR,
               "[dri] DRI memory allocation failed, disabling DRI.\n");
    DRIDestroyInfoRec(pTDFX->pDRIInfo);
    pTDFX->pDRIInfo=0;
    return FALSE;
  }
  pDRIInfo->devPrivate = pTDFXDRI;
  pDRIInfo->devPrivateSize = sizeof(TDFXDRIRec);
  pDRIInfo->contextSize = sizeof(TDFXDRIContextRec);

  pDRIInfo->CreateContext = TDFXCreateContext;
  pDRIInfo->DestroyContext = TDFXDestroyContext;
  pDRIInfo->SwapContext = TDFXDRISwapContext;
  pDRIInfo->InitBuffers = TDFXDRIInitBuffers;
  pDRIInfo->MoveBuffers = TDFXDRIMoveBuffers;
  pDRIInfo->OpenFullScreen = TDFXDRIOpenFullScreen;
  pDRIInfo->CloseFullScreen = TDFXDRICloseFullScreen;
  pDRIInfo->TransitionTo2d = TDFXDRITransitionTo2d;
  pDRIInfo->TransitionTo3d = TDFXDRITransitionTo3d;
  pDRIInfo->bufferRequests = DRI_ALL_WINDOWS;

  pDRIInfo->createDummyCtx = FALSE;
  pDRIInfo->createDummyCtxPriv = FALSE;

  if (!DRIScreenInit(pScreen, pDRIInfo, &pTDFX->drmSubFD)) {
    xfree(pDRIInfo->devPrivate);
    pDRIInfo->devPrivate=0;
    DRIDestroyInfoRec(pTDFX->pDRIInfo);
    pTDFX->pDRIInfo=0;
    xf86DrvMsg(pScreen->myNum, X_ERROR,
               "[dri] DRIScreenInit failed, disabling DRI.\n");

    return FALSE;
  }

  /* Check the TDFX DRM version */
  {
     drmVersionPtr version = drmGetVersion(pTDFX->drmSubFD);
     if (version) {
        if (version->version_major != 1 ||
            version->version_minor < 0) {
           /* incompatible drm version */
           xf86DrvMsg(pScreen->myNum, X_ERROR,
                      "[dri] TDFXDRIScreenInit failed because of a version mismatch.\n"
                      "[dri] tdfx.o kernel module version is %d.%d.%d but version 1.0.x is needed.\n"
                      "[dri] Disabling the DRI.\n",
                      version->version_major,
                      version->version_minor,
                      version->version_patchlevel);
           TDFXDRICloseScreen(pScreen);
           drmFreeVersion(version);
           return FALSE;
        }
        drmFreeVersion(version);
     }
  }

  pTDFXDRI->regsSize=TDFXIOMAPSIZE;
  if (drmAddMap(pTDFX->drmSubFD, (drmHandle)pTDFX->MMIOAddr[0],
		pTDFXDRI->regsSize, DRM_REGISTERS, 0, &pTDFXDRI->regs)<0) {
    TDFXDRICloseScreen(pScreen);
    xf86DrvMsg(pScreen->myNum, X_ERROR, "drmAddMap failed, disabling DRI.\n");
    return FALSE;
  }
  xf86DrvMsg(pScreen->myNum, X_INFO, "[drm] Registers = 0x%08lx\n",
	       pTDFXDRI->regs);

  if (!(TDFXInitVisualConfigs(pScreen))) {
    TDFXDRICloseScreen(pScreen);
    xf86DrvMsg(pScreen->myNum, X_ERROR, "TDFXInitVisualConfigs failed, disabling DRI.\n");
    return FALSE;
  }
  xf86DrvMsg(pScrn->scrnIndex, X_INFO, "visual configs initialized\n" );

  return TRUE;
}

void
TDFXDRICloseScreen(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  TDFXPtr pTDFX = TDFXPTR(pScrn);

  DRICloseScreen(pScreen);

  if (pTDFX->pDRIInfo) {
    if (pTDFX->pDRIInfo->devPrivate) {
      xfree(pTDFX->pDRIInfo->devPrivate);
      pTDFX->pDRIInfo->devPrivate=0;
    }
    DRIDestroyInfoRec(pTDFX->pDRIInfo);
    pTDFX->pDRIInfo=0;
  }
  if (pTDFX->pVisualConfigs) xfree(pTDFX->pVisualConfigs);
  if (pTDFX->pVisualConfigsPriv) xfree(pTDFX->pVisualConfigsPriv);
}

static Bool
TDFXCreateContext(ScreenPtr pScreen, VisualPtr visual,
		  drmContext hwContext, void *pVisualConfigPriv,
		  DRIContextType contextStore)
{
  return TRUE;
}

static void
TDFXDestroyContext(ScreenPtr pScreen, drmContext hwContext,
		   DRIContextType contextStore)
{
}

Bool
TDFXDRIFinishScreenInit(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  TDFXPtr pTDFX = TDFXPTR(pScrn);
  TDFXDRIPtr pTDFXDRI;

  pTDFX->pDRIInfo->driverSwapMethod = DRI_HIDE_X_CONTEXT;

  pTDFXDRI=(TDFXDRIPtr)pTDFX->pDRIInfo->devPrivate;
  pTDFXDRI->deviceID=pTDFX->PciInfo->chipType;
  pTDFXDRI->width=pScrn->virtualX;
  pTDFXDRI->height=pScrn->virtualY;
  pTDFXDRI->mem=pScrn->videoRam*1024;
  pTDFXDRI->cpp=pTDFX->cpp;
  pTDFXDRI->stride=pTDFX->stride;
  pTDFXDRI->fifoOffset=pTDFX->fifoOffset;
  pTDFXDRI->fifoSize=pTDFX->fifoSize;
  pTDFXDRI->textureOffset=pTDFX->texOffset;
  pTDFXDRI->textureSize=pTDFX->texSize;
  pTDFXDRI->fbOffset=pTDFX->fbOffset;
  pTDFXDRI->backOffset=pTDFX->backOffset;
  pTDFXDRI->depthOffset=pTDFX->depthOffset;
  pTDFXDRI->sarea_priv_offset = sizeof(XF86DRISAREARec);
  return DRIFinishScreenInit(pScreen);
}

static void
TDFXDRISwapContext(ScreenPtr pScreen, DRISyncType syncType,
		   DRIContextType oldContextType, void *oldContext,
		   DRIContextType newContextType, void *newContext)
{
}

static void
TDFXDRIInitBuffers(WindowPtr pWin, RegionPtr prgn, CARD32 index)
{
  ScreenPtr pScreen = pWin->drawable.pScreen;
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  TDFXPtr pTDFX = TDFXPTR(pScrn);
  BoxPtr pbox;
  int nbox;

  /* It looks nicer if these start out black */
  pbox = REGION_RECTS(prgn);
  nbox = REGION_NUM_RECTS(prgn);

  TDFXSetupForSolidFill(pScrn, 0, GXcopy, -1);
  while (nbox--) {
    TDFXSelectBuffer(pTDFX, TDFX_BACK);
    TDFXSubsequentSolidFillRect(pScrn, pbox->x1, pbox->y1,
				pbox->x2-pbox->x1, pbox->y2-pbox->y1);
    TDFXSelectBuffer(pTDFX, TDFX_DEPTH);
    TDFXSubsequentSolidFillRect(pScrn, pbox->x1, pbox->y1,
				pbox->x2-pbox->x1, pbox->y2-pbox->y1);
    pbox++;
  }
  TDFXSelectBuffer(pTDFX, TDFX_FRONT);

  pTDFX->AccelInfoRec->NeedToSync = TRUE;
}

static void
TDFXDRIMoveBuffers(WindowPtr pParent, DDXPointRec ptOldOrg,
		   RegionPtr prgnSrc, CARD32 index)
{
  ScreenPtr pScreen = pParent->drawable.pScreen;
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  TDFXPtr pTDFX = TDFXPTR(pScrn);
  int dx, dy, xdir, ydir, i, x, y, nbox;
  BoxPtr pbox;

  dx = pParent->drawable.x - ptOldOrg.x;
  dy = pParent->drawable.y - ptOldOrg.y;

  DRIMoveBuffersHelper(pScreen, dx, dy, &xdir, &ydir, prgnSrc);

  pbox = REGION_RECTS(prgnSrc);
  nbox = REGION_NUM_RECTS(prgnSrc);

  TDFXSetupForScreenToScreenCopy(pScrn, xdir, ydir, GXcopy, ~0, -1);

  TDFXSelectBuffer(pTDFX, TDFX_BACK);
  for(i = 0; i < nbox; i++) {
     x = pbox[i].x1;
     y = pbox[i].y1;
     TDFXSubsequentScreenToScreenCopy(pScrn, x, y, x+dx, y+dy, 
                                      pbox[i].x2 - x, pbox[i].y2 - y);
  }

  TDFXSelectBuffer(pTDFX, TDFX_DEPTH);
  for(i = 0; i < nbox; i++) {
     x = pbox[i].x1;
     y = pbox[i].y1;
     TDFXSubsequentScreenToScreenCopy(pScrn, x, y, x+dx, y+dy, 
                                      pbox[i].x2 - x, pbox[i].y2 - y);
  }

  TDFXSelectBuffer(pTDFX, TDFX_FRONT);

  pTDFX->AccelInfoRec->NeedToSync = TRUE;
}

static Bool
TDFXDRIOpenFullScreen(ScreenPtr pScreen)
{
#if 0
  ScrnInfoPtr pScrn;
  TDFXPtr pTDFX;

  xf86DrvMsg(pScreen->myNum, X_INFO, "OpenFullScreen\n");
  pScrn = xf86Screens[pScreen->myNum];
  pTDFX=TDFXPTR(pScrn);
  if (pTDFX->numChips>1) {
    TDFXSetupSLI(pScrn);
  }
#endif
  return TRUE;
}

static Bool
TDFXDRICloseFullScreen(ScreenPtr pScreen)
{
#if 0
  ScrnInfoPtr pScrn;

  xf86DrvMsg(pScreen->myNum, X_INFO, "CloseFullScreen\n");
  pScrn = xf86Screens[pScreen->myNum];
  TDFXDisableSLI(pScrn);
#endif
  return TRUE;
}

static void
TDFXDRITransitionTo2d(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  TDFXPtr pTDFX = TDFXPTR(pScrn);

  xf86FreeOffscreenArea(pTDFX->reservedArea); 
}

static void
TDFXDRITransitionTo3d(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  TDFXPtr pTDFX = TDFXPTR(pScrn);
  FBAreaPtr pArea;

  if(pTDFX->overlayBuffer) {
	xf86FreeOffscreenLinear(pTDFX->overlayBuffer);
	pTDFX->overlayBuffer = NULL;
  }

  if(pTDFX->overlayBuffer2) {
	xf86FreeOffscreenLinear(pTDFX->overlayBuffer2);
	pTDFX->overlayBuffer2 = NULL;
  }

  if(pTDFX->textureBuffer) {
	xf86FreeOffscreenArea(pTDFX->textureBuffer);
	pTDFX->textureBuffer = NULL;
  }

  xf86PurgeUnlockedOffscreenAreas(pScreen);
  
  pArea = xf86AllocateOffscreenArea(pScreen, pScrn->displayWidth,
				    pTDFX->pixmapCacheLinesMin,
				    pScrn->displayWidth, NULL, NULL, NULL);
  pTDFX->reservedArea = xf86AllocateOffscreenArea(pScreen, pScrn->displayWidth,
			pTDFX->pixmapCacheLinesMax - pTDFX->pixmapCacheLinesMin,
			pScrn->displayWidth, NULL, NULL, NULL);
  xf86FreeOffscreenArea(pArea);
}
