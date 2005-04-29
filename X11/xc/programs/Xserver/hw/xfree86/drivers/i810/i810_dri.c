/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i810_dri.c,v 1.42 2003/09/28 20:15:58 alanh Exp $ */
/*
 * Reformatted with GNU indent (2.2.8), using the following options:
 *
 *    -bad -bap -c41 -cd0 -ncdb -ci6 -cli0 -cp0 -ncs -d0 -di3 -i3 -ip3 -l78
 *    -lp -npcs -psl -sob -ss -br -ce -sc -hnl
 *
 * This provides a good match with the original i810 code and preferred
 * XFree86 formatting conventions.
 *
 * When editing this driver, please follow the existing formatting, and edit
 * with <TAB> characters expanded at 8-column intervals.
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Priv.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "windowstr.h"
#include "shadow.h"
#include "shadowfb.h"

#include "GL/glxtokens.h"

#include "i810.h"
#include "i810_dri.h"

static char I810KernelDriverName[] = "i810";
static char I810ClientDriverName[] = "i810";

static Bool I810InitVisualConfigs(ScreenPtr pScreen);
static Bool I810CreateContext(ScreenPtr pScreen, VisualPtr visual,
			      drmContext hwContext, void *pVisualConfigPriv,
			      DRIContextType contextStore);
static void I810DestroyContext(ScreenPtr pScreen, drmContext hwContext,
			       DRIContextType contextStore);
static void I810DRISwapContext(ScreenPtr pScreen, DRISyncType syncType,
			       DRIContextType readContextType,
			       void *readContextStore,
			       DRIContextType writeContextType,
			       void *writeContextStore);
static void I810DRIInitBuffers(WindowPtr pWin, RegionPtr prgn, CARD32 index);
static void I810DRIMoveBuffers(WindowPtr pParent, DDXPointRec ptOldOrg,
			       RegionPtr prgnSrc, CARD32 index);


static Bool I810DRIOpenFullScreen(ScreenPtr pScreen);
static Bool I810DRICloseFullScreen(ScreenPtr pScreen);
static void I810EnablePageFlip(ScreenPtr pScreen);
static void I810DisablePageFlip(ScreenPtr pScreen);
static void I810DRITransitionSingleToMulti3d(ScreenPtr pScreen);
static void I810DRITransitionMultiToSingle3d(ScreenPtr pScreen);
static void I810DRITransitionTo3d(ScreenPtr pScreen);
static void I810DRITransitionTo2d(ScreenPtr pScreen);

static void I810DRIRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);

extern void GlxSetVisualConfigs(int nconfigs,
				__GLXvisualConfig * configs,
				void **configprivs);

static int i810_pitches[] = {
   512,
   1024,
   2048,
   4096,
   0
};

static int i810_pitch_flags[] = {
   0x0,
   0x1,
   0x2,
   0x3,
   0
};

static unsigned int i810_drm_version = 0;

Bool
I810CleanupDma(ScrnInfoPtr pScrn)
{
   I810Ptr pI810 = I810PTR(pScrn);
   drmI810Init info;

   memset(&info, 0, sizeof(drmI810Init));
   info.func = I810_CLEANUP_DMA;

   if (drmCommandWrite(pI810->drmSubFD, DRM_I810_INIT,
		       &info, sizeof(drmI810Init))) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "[dri] I810 Dma Cleanup Failed\n");
      return FALSE;
   }

   return TRUE;
}

Bool
I810InitDma(ScrnInfoPtr pScrn)
{
   I810Ptr pI810 = I810PTR(pScrn);
   I810RingBuffer *ring = &(pI810->LpRing);
   I810DRIPtr pI810DRI = (I810DRIPtr) pI810->pDRIInfo->devPrivate;
   drmI810Init info;

   memset(&info, 0, sizeof(drmI810Init));

   info.ring_start = ring->mem.Start;
   info.ring_end = ring->mem.End;
   info.ring_size = ring->mem.Size;
   info.mmio_offset = (unsigned int)pI810DRI->regs;
   info.buffers_offset = (unsigned int)pI810->buffer_map;
   info.sarea_priv_offset = sizeof(XF86DRISAREARec);

   info.front_offset = 0;
   info.back_offset = pI810->BackBuffer.Start;
   info.depth_offset = pI810->DepthBuffer.Start;
   info.overlay_offset = pI810->OverlayStart;
   info.overlay_physical = pI810->OverlayPhysical;
   info.w = pScrn->virtualX;
   info.h = pScrn->virtualY;
   info.pitch = pI810->auxPitch;
   info.pitch_bits = pI810->auxPitchBits;

   /* We require DRM v1.2 or greater. Since DRM v1.2 broke compatibility
    * we created a new v1.4 that supports a new init function. Eventually the
    * old init function will go away. If you change the drm interface, make a
    * new init type too so that we can detect the new client.
    */
   switch(i810_drm_version) {
   case ((1<<16) | 0):
   case ((1<<16) | 1):
   case ((1<<16) | 2):
   case ((1<<16) | 3):
      /* Use OLD drm < 1.4 init */
      info.func = I810_INIT_DMA;
      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[drm] Init PRE v1.4 interface.\n");
      break;
   default:
   case ((1<<16) | 4):
      /*  DRM version 1.3 or greater init */
      info.func = I810_INIT_DMA_1_4;
      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[drm] Init v1.4 interface.\n");
      break;
   }

   if (drmCommandWrite(pI810->drmSubFD, DRM_I810_INIT,
		       &info, sizeof(drmI810Init))) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "[drm] I810 Dma Initialization failed.\n");
      return FALSE;
   }

   return TRUE;
}

static Bool
I810InitVisualConfigs(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I810Ptr pI810 = I810PTR(pScrn);
   int numConfigs = 0;
   __GLXvisualConfig *pConfigs = 0;
   I810ConfigPrivPtr pI810Configs = 0;
   I810ConfigPrivPtr *pI810ConfigPtrs = 0;
   int accum, stencil, db, depth;
   int i;

   switch (pScrn->bitsPerPixel) {
   case 8:
   case 24:
   case 32:
      break;
   case 16:
      numConfigs = 8;

      pConfigs =
	    (__GLXvisualConfig *) xcalloc(sizeof(__GLXvisualConfig),
					  numConfigs);
      if (!pConfigs)
	 return FALSE;

      pI810Configs =
	    (I810ConfigPrivPtr) xcalloc(sizeof(I810ConfigPrivRec),
					numConfigs);
      if (!pI810Configs) {
	 xfree(pConfigs);
	 return FALSE;
      }

      pI810ConfigPtrs =
	    (I810ConfigPrivPtr *) xcalloc(sizeof(I810ConfigPrivPtr),
					  numConfigs);
      if (!pI810ConfigPtrs) {
	 xfree(pConfigs);
	 xfree(pI810Configs);
	 return FALSE;
      }

      for (i = 0; i < numConfigs; i++)
	 pI810ConfigPtrs[i] = &pI810Configs[i];

      i = 0;
      depth = 1;
      for (accum = 0; accum <= 1; accum++) {
	 for (stencil = 0; stencil <= 1; stencil++) {
	    for (db = 1; db >= 0; db--) {
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
	       pConfigs[i].doubleBuffer = db ? TRUE : FALSE;
	       pConfigs[i].stereo = FALSE;
	       pConfigs[i].bufferSize = 16;
	       if (depth)
		  pConfigs[i].depthSize = 16;
	       else
		  pConfigs[i].depthSize = 0;
	       if (stencil)
		  pConfigs[i].stencilSize = 8;
	       else
		  pConfigs[i].stencilSize = 0;
	       pConfigs[i].auxBuffers = 0;
	       pConfigs[i].level = 0;
	       if (stencil || accum)
		  pConfigs[i].visualRating = GLX_SLOW_CONFIG;
	       else
		  pConfigs[i].visualRating = GLX_NONE;
	       pConfigs[i].transparentPixel = GLX_NONE;
	       pConfigs[i].transparentRed = 0;
	       pConfigs[i].transparentGreen = 0;
	       pConfigs[i].transparentBlue = 0;
	       pConfigs[i].transparentAlpha = 0;
	       pConfigs[i].transparentIndex = 0;
	       i++;
	    }
	 }
      }
      assert(i == numConfigs);
      break;
   }
   pI810->numVisualConfigs = numConfigs;
   pI810->pVisualConfigs = pConfigs;
   pI810->pVisualConfigsPriv = pI810Configs;
   GlxSetVisualConfigs(numConfigs, pConfigs, (void **)pI810ConfigPtrs);
   return TRUE;
}

static unsigned int
mylog2(unsigned int n)
{
   unsigned int log2 = 1;

   while (n > 1)
      n >>= 1, log2++;
   return log2;
}

Bool
I810DRIScreenInit(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I810Ptr pI810 = I810PTR(pScrn);
   DRIInfoPtr pDRIInfo;
   I810DRIPtr pI810DRI;
   unsigned long tom;
   unsigned long agpHandle;
   unsigned long dcacheHandle;
   int sysmem_size = 0;
   int back_size = 0;
   int pitch_idx = 0;
   int bufs;
   int width = pScrn->displayWidth * pI810->cpp;
   int i;

   /* Hardware 3D rendering only implemented for 16bpp */
   /* And it only works for 5:6:5 (Mark) */
   if (pScrn->depth != 16)
      return FALSE;

   /* Check that the GLX, DRI, and DRM modules have been loaded by testing
    * for known symbols in each module. */
   if (!xf86LoaderCheckSymbol("GlxSetVisualConfigs"))
      return FALSE;
   if (!xf86LoaderCheckSymbol("DRIScreenInit"))
      return FALSE;
   if (!xf86LoaderCheckSymbol("drmAvailable"))
      return FALSE;
   if (!xf86LoaderCheckSymbol("DRIQueryVersion")) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[dri] I810DRIScreenInit failed (libdri.a too old)\n");
      return FALSE;
   }

   /* Check the DRI version */
   {
      int major, minor, patch;

      DRIQueryVersion(&major, &minor, &patch);
      if (major != 4 || minor < 0) {
	 xf86DrvMsg(pScreen->myNum, X_ERROR,
		    "[dri] I810DRIScreenInit failed because of a version mismatch.\n"
		    "[dri] libDRI version is %d.%d.%d bug version 4.0.x is needed.\n"
		    "[dri] Disabling DRI.\n", major, minor, patch);
	 return FALSE;
      }
   }

   pDRIInfo = DRICreateInfoRec();
   if (!pDRIInfo) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[dri] DRICreateInfoRec failed.  Disabling DRI.\n");
      return FALSE;
   }

/*     pDRIInfo->wrap.ValidateTree = 0;    */
/*     pDRIInfo->wrap.PostValidateTree = 0;    */

   pI810->pDRIInfo = pDRIInfo;
   pI810->LockHeld = 0;

   pDRIInfo->drmDriverName = I810KernelDriverName;
   pDRIInfo->clientDriverName = I810ClientDriverName;
   pDRIInfo->busIdString = xalloc(64);

   sprintf(pDRIInfo->busIdString, "PCI:%d:%d:%d",
	   ((pciConfigPtr) pI810->PciInfo->thisCard)->busnum,
	   ((pciConfigPtr) pI810->PciInfo->thisCard)->devnum,
	   ((pciConfigPtr) pI810->PciInfo->thisCard)->funcnum);
   pDRIInfo->ddxDriverMajorVersion = I810_MAJOR_VERSION;
   pDRIInfo->ddxDriverMinorVersion = I810_MINOR_VERSION;
   pDRIInfo->ddxDriverPatchVersion = I810_PATCHLEVEL;
   pDRIInfo->frameBufferPhysicalAddress = pI810->LinearAddr;
   pDRIInfo->frameBufferSize = (((pScrn->displayWidth *
				  pScrn->virtualY * pI810->cpp) +
				 4096 - 1) / 4096) * 4096;

   pDRIInfo->frameBufferStride = pScrn->displayWidth * pI810->cpp;
   pDRIInfo->ddxDrawableTableEntry = I810_MAX_DRAWABLES;

   if (SAREA_MAX_DRAWABLES < I810_MAX_DRAWABLES)
      pDRIInfo->maxDrawableTableEntry = SAREA_MAX_DRAWABLES;
   else
      pDRIInfo->maxDrawableTableEntry = I810_MAX_DRAWABLES;

   /* For now the mapping works by using a fixed size defined
    * in the SAREA header
    */
   if (sizeof(XF86DRISAREARec) + sizeof(I810SAREARec) > SAREA_MAX) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[dri] Data does not fit in SAREA\n");
      return FALSE;
   }
   pDRIInfo->SAREASize = SAREA_MAX;

   if (!(pI810DRI = (I810DRIPtr) xcalloc(sizeof(I810DRIRec), 1))) {
      DRIDestroyInfoRec(pI810->pDRIInfo);
      pI810->pDRIInfo = 0;
      return FALSE;
   }
   pDRIInfo->devPrivate = pI810DRI;
   pDRIInfo->devPrivateSize = sizeof(I810DRIRec);
   pDRIInfo->contextSize = sizeof(I810DRIContextRec);

   pDRIInfo->CreateContext = I810CreateContext;
   pDRIInfo->DestroyContext = I810DestroyContext;
   pDRIInfo->SwapContext = I810DRISwapContext;
   pDRIInfo->InitBuffers = I810DRIInitBuffers;
   pDRIInfo->MoveBuffers = I810DRIMoveBuffers;
   pDRIInfo->bufferRequests = DRI_ALL_WINDOWS;
   pDRIInfo->OpenFullScreen = I810DRIOpenFullScreen;
   pDRIInfo->CloseFullScreen = I810DRICloseFullScreen;
   pDRIInfo->TransitionTo2d = I810DRITransitionTo2d;
   pDRIInfo->TransitionTo3d = I810DRITransitionTo3d;
   pDRIInfo->TransitionSingleToMulti3D = I810DRITransitionSingleToMulti3d;
   pDRIInfo->TransitionMultiToSingle3D = I810DRITransitionMultiToSingle3d;

   pDRIInfo->createDummyCtx = TRUE;
   pDRIInfo->createDummyCtxPriv = FALSE;

   /* This adds the framebuffer as a drm map *before* we have asked agp
    * to allocate it.  Scary stuff, hold on...
    */
   if (!DRIScreenInit(pScreen, pDRIInfo, &pI810->drmSubFD)) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[dri] DRIScreenInit failed.  Disabling DRI.\n");
      xfree(pDRIInfo->devPrivate);
      pDRIInfo->devPrivate = 0;
      DRIDestroyInfoRec(pI810->pDRIInfo);
      pI810->pDRIInfo = 0;
      return FALSE;
   }

   /* Check the i810 DRM versioning */
   {
      drmVersionPtr version;

      /* Check the DRM lib version.
       * drmGetLibVersion was not supported in version 1.0, so check for
       * symbol first to avoid possible crash or hang.
       */
      if (xf86LoaderCheckSymbol("drmGetLibVersion")) {
	 version = drmGetLibVersion(pI810->drmSubFD);
      } else
      {
	 /* drmlib version 1.0.0 didn't have the drmGetLibVersion
	  * entry point.  Fake it by allocating a version record
	  * via drmGetVersion and changing it to version 1.0.0
	  */
	 version = drmGetVersion(pI810->drmSubFD);
	 version->version_major = 1;
	 version->version_minor = 0;
	 version->version_patchlevel = 0;
      }

#define REQ_MAJ 1
#define REQ_MIN 1
      if (version) {
	 if (version->version_major != REQ_MAJ ||
	     version->version_minor < REQ_MIN) {
	    /* incompatible drm library version */
	    xf86DrvMsg(pScreen->myNum, X_ERROR,
		       "[dri] I810DRIScreenInit failed because of a version mismatch.\n"
		       "[dri] libdrm.a module version is %d.%d.%d but version %d.%d.x is needed.\n"
		       "[dri] Disabling DRI.\n",
		       version->version_major,
		       version->version_minor, version->version_patchlevel,
		       REQ_MAJ, REQ_MIN);
	    drmFreeVersion(version);
	    I810DRICloseScreen(pScreen);
	    return FALSE;
	 }
	 drmFreeVersion(version);
      }

      /* Check the i810 DRM version */
      version = drmGetVersion(pI810->drmSubFD);
      if (version) {
	i810_drm_version = (version->version_major<<16) |
	                    version->version_minor;
	 if (version->version_major != 1 || version->version_minor < 2) {
	    /* incompatible drm version */
	    xf86DrvMsg(pScreen->myNum, X_ERROR,
		       "[dri] I810DRIScreenInit failed because of a version mismatch.\n"
		       "[dri] i810.o kernel module version is %d.%d.%d but version 1.2.0 or greater is needed.\n"
		       "[dri] Disabling DRI.\n",
		       version->version_major,
		       version->version_minor, version->version_patchlevel);
	    I810DRICloseScreen(pScreen);
	    drmFreeVersion(version);
	    return FALSE;
	 }
         pI810->drmMinor = version->version_minor;
	 drmFreeVersion(version);
      }
   }

   pI810DRI->regsSize = I810_REG_SIZE;
   if (drmAddMap(pI810->drmSubFD, (drmHandle) pI810->MMIOAddr,
		 pI810DRI->regsSize, DRM_REGISTERS, 0, &pI810DRI->regs) < 0) {
      xf86DrvMsg(pScreen->myNum, X_ERROR, "[drm] drmAddMap(regs) failed\n");
      DRICloseScreen(pScreen);
      return FALSE;
   }
   xf86DrvMsg(pScreen->myNum, X_INFO, "[drm] Registers = 0x%08lx\n",
	      pI810DRI->regs);

   pI810->backHandle = DRM_AGP_NO_HANDLE;
   pI810->zHandle = DRM_AGP_NO_HANDLE;
   pI810->cursorHandle = DRM_AGP_NO_HANDLE;
   pI810->xvmcHandle = DRM_AGP_NO_HANDLE;
   pI810->sysmemHandle = DRM_AGP_NO_HANDLE;
   pI810->agpAcquired = FALSE;
   pI810->dcacheHandle = DRM_AGP_NO_HANDLE;

   /* Agp Support - Need this just to get the framebuffer.
    */
   if (drmAgpAcquire(pI810->drmSubFD) < 0) {
      xf86DrvMsg(pScreen->myNum, X_ERROR, "[agp] drmAgpAquire failed\n");
      DRICloseScreen(pScreen);
      return FALSE;
   }
   pI810->agpAcquired = TRUE;

   if (drmAgpEnable(pI810->drmSubFD, 0) < 0) {
      xf86DrvMsg(pScreen->myNum, X_ERROR, "[agp] drmAgpEnable failed\n");
      DRICloseScreen(pScreen);
      return FALSE;
   }

   memset(&pI810->DcacheMem, 0, sizeof(I810MemRange));
   memset(&pI810->BackBuffer, 0, sizeof(I810MemRange));
   memset(&pI810->DepthBuffer, 0, sizeof(I810MemRange));
   pI810->CursorPhysical = 0;

   /* Dcache - half the speed of normal ram, but has use as a Z buffer
    * under the DRI.
    */

   drmAgpAlloc(pI810->drmSubFD, 4096 * 1024, 1, NULL, &dcacheHandle);
   pI810->dcacheHandle = dcacheHandle;

   xf86DrvMsg(pScreen->myNum, X_INFO, "[agp] dcacheHandle : 0x%lx\n",
	      dcacheHandle);

#define Elements(x) sizeof(x)/sizeof(*x)
   for (pitch_idx = 0; pitch_idx < Elements(i810_pitches); pitch_idx++)
      if (width <= i810_pitches[pitch_idx])
	 break;

   if (pitch_idx == Elements(i810_pitches)) {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "[dri] Couldn't find depth/back buffer pitch");
      DRICloseScreen(pScreen);
      return FALSE;
   } else {
      /* for tiled memory to work, the buffer needs to have the
       * number of lines as a multiple of 16 (the tile size),
       *  - airlied */
      int lines = (pScrn->virtualY + 15) / 16 * 16;
      back_size = i810_pitches[pitch_idx] * lines;
      back_size = ((back_size + 4096 - 1) / 4096) * 4096;
   }

   sysmem_size = pScrn->videoRam * 1024;
   if (dcacheHandle != DRM_AGP_NO_HANDLE) {
      if (back_size > 4 * 1024 * 1024) {
	 xf86DrvMsg(pScreen->myNum, X_INFO,
		    "[dri] Backsize is larger then 4 meg\n");
	 sysmem_size = sysmem_size - 2 * back_size;
	 drmAgpFree(pI810->drmSubFD, dcacheHandle);
	 pI810->dcacheHandle = dcacheHandle = DRM_AGP_NO_HANDLE;
      } else {
	 sysmem_size = sysmem_size - back_size;
      }
   } else {
      sysmem_size = sysmem_size - 2 * back_size;
   }

   /* Max size is 48 without XvMC, 41 with 6 surfaces, 40 with 7 surfaces */
   if (pI810->numSurfaces && (pI810->numSurfaces == 6)) {
      if (sysmem_size > (pI810->FbMapSize - 7 * 1024 * 1024)) {
	 sysmem_size = (pI810->FbMapSize - 7 * 1024 * 1024);
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "User requested more memory then fits in the agp aperture\n"
		    "Truncating to %d bytes of memory\n", sysmem_size);
      }
   }
   if (pI810->numSurfaces && (pI810->numSurfaces == 7)) {
      if (sysmem_size > (pI810->FbMapSize - 8 * 1024 * 1024)) {
	 sysmem_size = (pI810->FbMapSize - 8 * 1024 * 1024);
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "User requested more memory then fits in the agp aperture\n"
		    "Truncating to %d bytes of memory\n", sysmem_size);
      }
   }

   if (sysmem_size > pI810->FbMapSize) {
      sysmem_size = pI810->FbMapSize;

      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "[dri] User requested more memory then fits in the agp"
		 " aperture\n\tTruncating to %d bytes of memory\n",
		 sysmem_size);
   }

   sysmem_size -= 4096;			/* remove 4k for the hw cursor */

   pI810->SysMem.Start = 0;
   pI810->SysMem.Size = sysmem_size;
   pI810->SysMem.End = sysmem_size;
   tom = sysmem_size;

   pI810->SavedSysMem = pI810->SysMem;

   if (dcacheHandle != DRM_AGP_NO_HANDLE) {
      if (drmAgpBind(pI810->drmSubFD, dcacheHandle, pI810->DepthOffset) == 0) {
	 memset(&pI810->DcacheMem, 0, sizeof(I810MemRange));
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "[agp] GART: Found 4096K Z buffer memory\n");
	 pI810->DcacheMem.Start = pI810->DepthOffset;
	 pI810->DcacheMem.Size = 1024 * 4096;
	 pI810->DcacheMem.End =
	       pI810->DcacheMem.Start + pI810->DcacheMem.Size;
	 if (!I810AllocLow
	     (&(pI810->DepthBuffer), &(pI810->DcacheMem), back_size)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "[agp] Depth buffer allocation failed\n");
	    DRICloseScreen(pScreen);
	    return FALSE;
	 }
      } else {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "[agp] GART: dcache bind failed\n");
	 drmAgpFree(pI810->drmSubFD, dcacheHandle);
	 pI810->dcacheHandle = dcacheHandle = DRM_AGP_NO_HANDLE;
      }
   } else {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "[agp] GART: no dcache memory found\n");
   }

   drmAgpAlloc(pI810->drmSubFD, back_size, 0, NULL, &agpHandle);
   pI810->backHandle = agpHandle;

   if (agpHandle != DRM_AGP_NO_HANDLE) {
      if (drmAgpBind(pI810->drmSubFD, agpHandle, pI810->BackOffset) == 0) {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "[agp] Bound backbuffer memory\n");

	 pI810->BackBuffer.Start = pI810->BackOffset;
	 pI810->BackBuffer.Size = back_size;
	 pI810->BackBuffer.End = (pI810->BackBuffer.Start +
				  pI810->BackBuffer.Size);
      } else {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "[agp] Unable to bind backbuffer.  Disabling DRI.\n");
	 DRICloseScreen(pScreen);
	 return FALSE;
      }
   } else {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "[dri] Unable to allocate backbuffer memory.  Disabling DRI.\n");
      DRICloseScreen(pScreen);
      return FALSE;
   }

   if (dcacheHandle == DRM_AGP_NO_HANDLE) {
     drmAgpAlloc(pI810->drmSubFD, back_size, 0, NULL, &agpHandle);

      pI810->zHandle = agpHandle;

      if (agpHandle != DRM_AGP_NO_HANDLE) {
	 if (drmAgpBind(pI810->drmSubFD, agpHandle, pI810->DepthOffset) == 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "[agp] Bound depthbuffer memory\n");
	    pI810->DepthBuffer.Start = pI810->DepthOffset;
	    pI810->DepthBuffer.Size = back_size;
	    pI810->DepthBuffer.End = (pI810->DepthBuffer.Start +
				      pI810->DepthBuffer.Size);
	 } else {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "[agp] Unable to bind depthbuffer.  Disabling DRI.\n");
	    DRICloseScreen(pScreen);
	    return FALSE;
	 }
      } else {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "[agp] Unable to allocate depthbuffer memory.  Disabling DRI.\n");
	 DRICloseScreen(pScreen);
	 return FALSE;
      }
   }

   /* Now allocate and bind the agp space.  This memory will include the
    * regular framebuffer as well as texture memory.
    */
   drmAgpAlloc(pI810->drmSubFD, sysmem_size, 0, NULL, &agpHandle);

   if (agpHandle == DRM_AGP_NO_HANDLE) {
      xf86DrvMsg(pScreen->myNum, X_ERROR, "[agp] drmAgpAlloc failed\n");
      DRICloseScreen(pScreen);
      return FALSE;
   }
   pI810->sysmemHandle = agpHandle;

   if (drmAgpBind(pI810->drmSubFD, agpHandle, 0) != 0) {
      xf86DrvMsg(pScreen->myNum, X_ERROR, "[agp] drmAgpBind failed\n");
      DRICloseScreen(pScreen);
      return FALSE;
   }

/* Allocate 7 or 8MB for XvMC this is setup as follows to best use tiled
   regions and required surface pitches. (Numbers are adjusted if the
   AGP region is only 32MB
   For numSurfaces == 6
   44 - 48MB = 4MB Fence, 8 Tiles wide
   43 - 44MB = 1MB Fence, 8 Tiles wide
   42 - 43MB = 1MB Fence, 4 Tiles wide
   41 - 42MB = 1MB Fence, 4 Tiles wide
   For numSurfaces == 7
   44 - 48MB   = 4MB Fence, 8 Tiles wide
   43 - 44MB   = 1MB Fence, 8 Tiles wide
   42.5 - 43MB = 0.5MB Fence, 8 Tiles wide
   42 - 42.5MB = 0.5MB Fence, 4 Tiles wide
   40 - 42MB   = 2MB Fence, 4 Tiles wide
 */
   if (pI810->numSurfaces) {
      if (pI810->numSurfaces == 6) {
	 pI810->MC.Size = 7 * 1024 * 1024;
	 pI810->MC.Start = pI810->FbMapSize - 7 * 1024 * 1024;

      }
      if (pI810->numSurfaces == 7) {
	 pI810->MC.Size = 8 * 1024 * 1024;
	 pI810->MC.Start = pI810->FbMapSize - 8 * 1024 * 1024;
      }
      drmAgpAlloc(pI810->drmSubFD, pI810->MC.Size, 0, NULL, &agpHandle);
      
      pI810->xvmcHandle = agpHandle;

      if (agpHandle != DRM_AGP_NO_HANDLE) {
	 if (drmAgpBind(pI810->drmSubFD, agpHandle, pI810->MC.Start) == 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "GART: Allocated 7MB for HWMC\n");
	    pI810->MC.End = pI810->MC.Start + pI810->MC.Size;
	 } else {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "GART: HWMC bind failed\n");
	    pI810->MC.Start = 0;
	    pI810->MC.Size = 0;
	    pI810->MC.End = 0;
	 }
      } else {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO, "GART: HWMC alloc failed\n");
	 pI810->MC.Start = 0;
	 pI810->MC.Size = 0;
	 pI810->MC.End = 0;
      }
      pI810->xvmcContext = 0;
   }

   drmAgpAlloc(pI810->drmSubFD, 4096, 2,
	       (unsigned long *)&pI810->CursorPhysical, &agpHandle);

   pI810->cursorHandle = agpHandle;

   if (agpHandle != DRM_AGP_NO_HANDLE) {
      tom = sysmem_size;

      if (drmAgpBind(pI810->drmSubFD, agpHandle, tom) == 0) {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "[agp] GART: Allocated 4K for mouse cursor image\n");
	 pI810->CursorStart = tom;
	 tom += 4096;
      } else {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "[agp] GART: cursor bind failed\n");
	 pI810->CursorPhysical = 0;
      }
   } else {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "[agp] GART: cursor alloc failed\n");
      pI810->CursorPhysical = 0;
   }

   /* Steal some of the excess cursor space for the overlay regs.
    */
   pI810->OverlayPhysical = pI810->CursorPhysical + 1024;
   pI810->OverlayStart = pI810->CursorStart + 1024;

   I810SetTiledMemory(pScrn, 1,
		      pI810->DepthBuffer.Start,
		      i810_pitches[pitch_idx], 8 * 1024 * 1024);

   I810SetTiledMemory(pScrn, 2,
		      pI810->BackBuffer.Start,
		      i810_pitches[pitch_idx], 8 * 1024 * 1024);

   /* These are for HWMC surfaces */
   if (pI810->numSurfaces == 6) {
      I810SetTiledMemory(pScrn, 3, pI810->MC.Start, 512, 1024 * 1024);

      I810SetTiledMemory(pScrn, 4,
			 pI810->MC.Start + 1024 * 1024, 512, 1024 * 1024);

      I810SetTiledMemory(pScrn, 5,
			 pI810->MC.Start + 1024 * 1024 * 2,
			 1024, 1024 * 1024);

      I810SetTiledMemory(pScrn, 6,
			 pI810->MC.Start + 1024 * 1024 * 3,
			 1024, 4 * 1024 * 1024);
   }
   if (pI810->numSurfaces == 7) {
      I810SetTiledMemory(pScrn, 3, pI810->MC.Start, 512, 2 * 1024 * 1024);

      I810SetTiledMemory(pScrn, 4,
			 pI810->MC.Start + 2 * 1024 * 1024, 512, 512 * 1024);

      I810SetTiledMemory(pScrn, 5,
			 pI810->MC.Start + 2 * 1024 * 1024 + 512 * 1024,
			 1024, 512 * 1024);

      I810SetTiledMemory(pScrn, 6,
			 pI810->MC.Start + 3 * 1024 * 1024,
			 1024, 1 * 1024 * 1024);

      I810SetTiledMemory(pScrn, 7,
			 pI810->MC.Start + 4 * 1024 * 1024,
			 1024, 4 * 1024 * 1024);

   }

   pI810->auxPitch = i810_pitches[pitch_idx];
   pI810->auxPitchBits = i810_pitch_flags[pitch_idx];
   pI810->SavedDcacheMem = pI810->DcacheMem;
   pI810DRI->backbufferSize = pI810->BackBuffer.Size;

   if (drmAddMap(pI810->drmSubFD, (drmHandle) pI810->BackBuffer.Start,
		 pI810->BackBuffer.Size, DRM_AGP, 0,
		 &pI810DRI->backbuffer) < 0) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[drm] drmAddMap(backbuffer) failed.  Disabling DRI\n");
      DRICloseScreen(pScreen);
      return FALSE;
   }

   pI810DRI->depthbufferSize = pI810->DepthBuffer.Size;
   if (drmAddMap(pI810->drmSubFD, (drmHandle) pI810->DepthBuffer.Start,
		 pI810->DepthBuffer.Size, DRM_AGP, 0,
		 &pI810DRI->depthbuffer) < 0) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[drm] drmAddMap(depthbuffer) failed.  Disabling DRI.\n");
      DRICloseScreen(pScreen);
      return FALSE;
   }

   /* Allocate FrontBuffer etc. */
   if (!I810AllocateFront(pScrn)) {
      DRICloseScreen(pScreen);
      return FALSE;
   }

   /* Allocate buffer memory */
   I810AllocHigh(&(pI810->BufferMem), &(pI810->SysMem),
		 I810_DMA_BUF_NR * I810_DMA_BUF_SZ);

   xf86DrvMsg(pScreen->myNum, X_INFO, "[dri] Buffer map : %lx\n",
	      pI810->BufferMem.Start);

   if (pI810->BufferMem.Start == 0 ||
       pI810->BufferMem.End - pI810->BufferMem.Start >
       I810_DMA_BUF_NR * I810_DMA_BUF_SZ) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[dri] Not enough memory for dma buffers.  Disabling DRI.\n");
      DRICloseScreen(pScreen);
      return FALSE;
   }
   if (drmAddMap(pI810->drmSubFD, (drmHandle) pI810->BufferMem.Start,
		 pI810->BufferMem.Size, DRM_AGP, 0, &pI810->buffer_map) < 0) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[drm] drmAddMap(buffer_map) failed.  Disabling DRI.\n");
      DRICloseScreen(pScreen);
      return FALSE;
   }

   pI810DRI->agp_buffers = pI810->buffer_map;
   pI810DRI->agp_buf_size = pI810->BufferMem.Size;

   if (drmAddMap(pI810->drmSubFD, (drmHandle) pI810->LpRing.mem.Start,
		 pI810->LpRing.mem.Size, DRM_AGP, 0, &pI810->ring_map) < 0) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[drm] drmAddMap(ring_map) failed.  Disabling DRI.\n");
      DRICloseScreen(pScreen);
      return FALSE;
   }

   /* Use the rest of memory for textures. */
   pI810DRI->textureSize = pI810->SysMem.Size;

   i = mylog2(pI810DRI->textureSize / I810_NR_TEX_REGIONS);

   if (i < I810_LOG_MIN_TEX_REGION_SIZE)
      i = I810_LOG_MIN_TEX_REGION_SIZE;

   pI810DRI->logTextureGranularity = i;
   pI810DRI->textureSize = (pI810DRI->textureSize >> i) << i;	/* truncate */

   if (pI810DRI->textureSize < 512 * 1024) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[drm] Less then 512k memory left for textures.  Disabling DRI.\n");
      DRICloseScreen(pScreen);
      return FALSE;
   }

   I810AllocLow(&(pI810->TexMem), &(pI810->SysMem), pI810DRI->textureSize);

   if (drmAddMap(pI810->drmSubFD, (drmHandle) pI810->TexMem.Start,
		 pI810->TexMem.Size, DRM_AGP, 0, &pI810DRI->textures) < 0) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[drm] drmAddMap(textures) failed.  Disabling DRI.\n");
      DRICloseScreen(pScreen);
      return FALSE;
   }

   if ((bufs = drmAddBufs(pI810->drmSubFD,
			  I810_DMA_BUF_NR,
			  I810_DMA_BUF_SZ,
			  DRM_AGP_BUFFER, pI810->BufferMem.Start)) <= 0) {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "[drm] failure adding %d %d byte DMA buffers.  Disabling DRI.\n",
		 I810_DMA_BUF_NR, I810_DMA_BUF_SZ);
      DRICloseScreen(pScreen);
      return FALSE;
   }

   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	      "[drm] added %d %d byte DMA buffers\n", bufs, I810_DMA_BUF_SZ);

   I810InitDma(pScrn);

   /* Okay now initialize the dma engine */

   if (!pI810DRI->irq) {
      pI810DRI->irq = drmGetInterruptFromBusID(pI810->drmSubFD,
					       ((pciConfigPtr) pI810->
						PciInfo->thisCard)->busnum,
					       ((pciConfigPtr) pI810->
						PciInfo->thisCard)->devnum,
					       ((pciConfigPtr) pI810->
						PciInfo->thisCard)->funcnum);
      if ((drmCtlInstHandler(pI810->drmSubFD, pI810DRI->irq)) != 0) {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "[drm] failure adding irq handler, there is a device "
		    "already using that irq\n Consider rearranging your "
		    "PCI cards.  Disabling DRI.\n");
	 DRICloseScreen(pScreen);
	 return FALSE;
      }
   }

   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	      "[drm] dma control initialized, using IRQ %d\n", pI810DRI->irq);

   pI810DRI->deviceID = pI810->PciInfo->chipType;
   pI810DRI->width = pScrn->virtualX;
   pI810DRI->height = pScrn->virtualY;
   pI810DRI->mem = pScrn->videoRam * 1024;
   pI810DRI->cpp = pI810->cpp;

   pI810DRI->fbOffset = pI810->FrontBuffer.Start;
   pI810DRI->fbStride = pI810->auxPitch;

   pI810DRI->bitsPerPixel = pScrn->bitsPerPixel;

   pI810DRI->textureOffset = pI810->TexMem.Start;

   pI810DRI->backOffset = pI810->BackBuffer.Start;
   pI810DRI->depthOffset = pI810->DepthBuffer.Start;

   pI810DRI->ringOffset = pI810->LpRing.mem.Start;
   pI810DRI->ringSize = pI810->LpRing.mem.Size;

   pI810DRI->auxPitch = pI810->auxPitch;
   pI810DRI->auxPitchBits = pI810->auxPitchBits;
   pI810DRI->sarea_priv_offset = sizeof(XF86DRISAREARec);

   if (!(I810InitVisualConfigs(pScreen))) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[dri] I810InitVisualConfigs failed.  Disabling DRI.\n");
      DRICloseScreen(pScreen);
      return FALSE;
   }

   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	      "[dri] visual configs initialized.\n");
   pI810->pDRIInfo->driverSwapMethod = DRI_HIDE_X_CONTEXT;

   return TRUE;
}

void
I810DRICloseScreen(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I810Ptr pI810 = I810PTR(pScrn);
   I810DRIPtr pI810DRI = (I810DRIPtr) pI810->pDRIInfo->devPrivate;

   if (pI810DRI->irq) {
       drmCtlUninstHandler(pI810->drmSubFD);
       pI810DRI->irq = 0;
   }

   I810CleanupDma(pScrn);

   if (pI810->dcacheHandle!=DRM_AGP_NO_HANDLE)
      drmAgpFree(pI810->drmSubFD, pI810->dcacheHandle);
   if (pI810->backHandle!=DRM_AGP_NO_HANDLE)
      drmAgpFree(pI810->drmSubFD, pI810->backHandle);
   if (pI810->zHandle!=DRM_AGP_NO_HANDLE)
      drmAgpFree(pI810->drmSubFD, pI810->zHandle);
   if (pI810->cursorHandle!=DRM_AGP_NO_HANDLE)
      drmAgpFree(pI810->drmSubFD, pI810->cursorHandle);
   if (pI810->xvmcHandle!=DRM_AGP_NO_HANDLE)
      drmAgpFree(pI810->drmSubFD, pI810->xvmcHandle);
   if (pI810->sysmemHandle!=DRM_AGP_NO_HANDLE)
      drmAgpFree(pI810->drmSubFD, pI810->sysmemHandle);

   if (pI810->agpAcquired == TRUE)
      drmAgpRelease(pI810->drmSubFD);

   pI810->backHandle = DRM_AGP_NO_HANDLE;
   pI810->zHandle = DRM_AGP_NO_HANDLE;
   pI810->cursorHandle = DRM_AGP_NO_HANDLE;
   pI810->xvmcHandle = DRM_AGP_NO_HANDLE;
   pI810->sysmemHandle = DRM_AGP_NO_HANDLE;
   pI810->agpAcquired = FALSE;
   pI810->dcacheHandle = DRM_AGP_NO_HANDLE;

   DRICloseScreen(pScreen);

   if (pI810->pDRIInfo) {
      if (pI810->pDRIInfo->devPrivate) {
	 xfree(pI810->pDRIInfo->devPrivate);
	 pI810->pDRIInfo->devPrivate = 0;
      }
      DRIDestroyInfoRec(pI810->pDRIInfo);
      pI810->pDRIInfo = 0;
   }
   if (pI810->pVisualConfigs)
      xfree(pI810->pVisualConfigs);
   if (pI810->pVisualConfigsPriv)
      xfree(pI810->pVisualConfigsPriv);
}

static Bool
I810CreateContext(ScreenPtr pScreen, VisualPtr visual,
		  drmContext hwContext, void *pVisualConfigPriv,
		  DRIContextType contextStore)
{
   return TRUE;
}

static void
I810DestroyContext(ScreenPtr pScreen, drmContext hwContext,
		   DRIContextType contextStore)
{
}

Bool
I810DRIFinishScreenInit(ScreenPtr pScreen)
{
   I810SAREARec *sPriv = (I810SAREARec *) DRIGetSAREAPrivate(pScreen);
   ScrnInfoPtr        pScrn = xf86Screens[pScreen->myNum];
   I810Ptr info  = I810PTR(pScrn);

   memset(sPriv, 0, sizeof(sPriv));

   /* Have shadow run only while there is 3d active.
    */
   if (info->allowPageFlip && info->drmMinor >= 3) {
     ShadowFBInit( pScreen, I810DRIRefreshArea );
   } 
   else
     info->allowPageFlip = 0;
   return DRIFinishScreenInit(pScreen);
}

void
I810DRISwapContext(ScreenPtr pScreen, DRISyncType syncType,
		   DRIContextType oldContextType, void *oldContext,
		   DRIContextType newContextType, void *newContext)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I810Ptr pI810 = I810PTR(pScrn);

   if (syncType == DRI_3D_SYNC &&
       oldContextType == DRI_2D_CONTEXT && newContextType == DRI_2D_CONTEXT) {
      if (I810_DEBUG & DEBUG_VERBOSE_DRI)
	 ErrorF("I810DRISwapContext (in)\n");

      if (!pScrn->vtSema)
	  return;
      pI810->LockHeld = 1;
      I810RefreshRing(pScrn);
   } else if (syncType == DRI_2D_SYNC &&
	      oldContextType == DRI_NO_CONTEXT &&
	      newContextType == DRI_2D_CONTEXT) {
      pI810->LockHeld = 0;
      if (I810_DEBUG & DEBUG_VERBOSE_DRI)
	 ErrorF("I810DRISwapContext (out)\n");
   } else if (I810_DEBUG & DEBUG_VERBOSE_DRI)
      ErrorF("I810DRISwapContext (other)\n");
}

static void
I810DRIInitBuffers(WindowPtr pWin, RegionPtr prgn, CARD32 index)
{
   ScreenPtr pScreen = pWin->drawable.pScreen;
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I810Ptr pI810 = I810PTR(pScrn);
   BoxPtr pbox = REGION_RECTS(prgn);
   int nbox = REGION_NUM_RECTS(prgn);

   if (I810_DEBUG & DEBUG_VERBOSE_DRI)
      ErrorF("I810DRIInitBuffers\n");

   I810SetupForSolidFill(pScrn, 0, GXcopy, -1);
   while (nbox--) {
      I810SelectBuffer(pScrn, I810_SELECT_BACK);
      I810SubsequentSolidFillRect(pScrn, pbox->x1, pbox->y1,
				  pbox->x2 - pbox->x1, pbox->y2 - pbox->y1);
      pbox++;
   }

   /* Clear the depth buffer - uses 0xffff rather than 0.
    */
   pbox = REGION_RECTS(prgn);
   nbox = REGION_NUM_RECTS(prgn);
   I810SelectBuffer(pScrn, I810_SELECT_DEPTH);
   I810SetupForSolidFill(pScrn, 0xffff, GXcopy, -1);
   while (nbox--) {
      I810SubsequentSolidFillRect(pScrn, pbox->x1, pbox->y1,
				  pbox->x2 - pbox->x1, pbox->y2 - pbox->y1);
      pbox++;
   }
   I810SelectBuffer(pScrn, I810_SELECT_FRONT);
   pI810->AccelInfoRec->NeedToSync = TRUE;
}

/* This routine is a modified form of XAADoBitBlt with the calls to
 * ScreenToScreenBitBlt built in. My routine has the prgnSrc as source
 * instead of destination. My origin is upside down so the ydir cases
 * are reversed.
 *
 * KW: can you believe that this is called even when a 2d window moves?
 */
static void
I810DRIMoveBuffers(WindowPtr pParent, DDXPointRec ptOldOrg,
		   RegionPtr prgnSrc, CARD32 index)
{
   ScreenPtr pScreen = pParent->drawable.pScreen;
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I810Ptr pI810 = I810PTR(pScrn);
   BoxPtr pboxTmp, pboxNext, pboxBase;
   DDXPointPtr pptTmp, pptNew2;
   int xdir, ydir;

   int screenwidth = pScrn->virtualX;
   int screenheight = pScrn->virtualY;

   BoxPtr pbox = REGION_RECTS(prgnSrc);
   int nbox = REGION_NUM_RECTS(prgnSrc);

   BoxPtr pboxNew1 = 0;
   BoxPtr pboxNew2 = 0;
   DDXPointPtr pptNew1 = 0;
   DDXPointPtr pptSrc = &ptOldOrg;

   int dx = pParent->drawable.x - ptOldOrg.x;
   int dy = pParent->drawable.y - ptOldOrg.y;

   /* If the copy will overlap in Y, reverse the order */
   if (dy > 0) {
      ydir = -1;

      if (nbox > 1) {
	 /* Keep ordering in each band, reverse order of bands */
	 pboxNew1 = (BoxPtr) ALLOCATE_LOCAL(sizeof(BoxRec) * nbox);
	 if (!pboxNew1)
	    return;
	 pptNew1 = (DDXPointPtr) ALLOCATE_LOCAL(sizeof(DDXPointRec) * nbox);
	 if (!pptNew1) {
	    DEALLOCATE_LOCAL(pboxNew1);
	    return;
	 }
	 pboxBase = pboxNext = pbox + nbox - 1;
	 while (pboxBase >= pbox) {
	    while ((pboxNext >= pbox) && (pboxBase->y1 == pboxNext->y1))
	       pboxNext--;
	    pboxTmp = pboxNext + 1;
	    pptTmp = pptSrc + (pboxTmp - pbox);
	    while (pboxTmp <= pboxBase) {
	       *pboxNew1++ = *pboxTmp++;
	       *pptNew1++ = *pptTmp++;
	    }
	    pboxBase = pboxNext;
	 }
	 pboxNew1 -= nbox;
	 pbox = pboxNew1;
	 pptNew1 -= nbox;
	 pptSrc = pptNew1;
      }
   } else {
      /* No changes required */
      ydir = 1;
   }

   /* If the regions will overlap in X, reverse the order */
   if (dx > 0) {
      xdir = -1;

      if (nbox > 1) {
	 /*reverse orderof rects in each band */
	 pboxNew2 = (BoxPtr) ALLOCATE_LOCAL(sizeof(BoxRec) * nbox);
	 pptNew2 = (DDXPointPtr) ALLOCATE_LOCAL(sizeof(DDXPointRec) * nbox);
	 if (!pboxNew2 || !pptNew2) {
	    if (pptNew2)
	       DEALLOCATE_LOCAL(pptNew2);
	    if (pboxNew2)
	       DEALLOCATE_LOCAL(pboxNew2);
	    if (pboxNew1) {
	       DEALLOCATE_LOCAL(pptNew1);
	       DEALLOCATE_LOCAL(pboxNew1);
	    }
	    return;
	 }
	 pboxBase = pboxNext = pbox;
	 while (pboxBase < pbox + nbox) {
	    while ((pboxNext < pbox + nbox) && (pboxNext->y1 == pboxBase->y1))
	       pboxNext++;
	    pboxTmp = pboxNext;
	    pptTmp = pptSrc + (pboxTmp - pbox);
	    while (pboxTmp != pboxBase) {
	       *pboxNew2++ = *--pboxTmp;
	       *pptNew2++ = *--pptTmp;
	    }
	    pboxBase = pboxNext;
	 }
	 pboxNew2 -= nbox;
	 pbox = pboxNew2;
	 pptNew2 -= nbox;
	 pptSrc = pptNew2;
      }
   } else {
      /* No changes are needed */
      xdir = 1;
   }

   /* SelectBuffer isn't really a good concept for the i810.
    */
   I810EmitFlush(pScrn);
   I810SetupForScreenToScreenCopy(pScrn, xdir, ydir, GXcopy, -1, -1);
   for (; nbox--; pbox++) {

      int x1 = pbox->x1;
      int y1 = pbox->y1;
      int destx = x1 + dx;
      int desty = y1 + dy;
      int w = pbox->x2 - x1 + 1;
      int h = pbox->y2 - y1 + 1;

      if (destx < 0)
	 x1 -= destx, w += destx, destx = 0;
      if (desty < 0)
	 y1 -= desty, h += desty, desty = 0;
      if (destx + w > screenwidth)
	 w = screenwidth - destx;
      if (desty + h > screenheight)
	 h = screenheight - desty;
      if (w <= 0)
	 continue;
      if (h <= 0)
	 continue;

      if (I810_DEBUG & DEBUG_VERBOSE_DRI)
	 ErrorF("MoveBuffers %d,%d %dx%d dx: %d dy: %d\n",
		x1, y1, w, h, dx, dy);
      
      I810SelectBuffer(pScrn, I810_SELECT_BACK);
      I810SubsequentScreenToScreenCopy(pScrn, x1, y1, destx, desty, w, h);
      I810SelectBuffer(pScrn, I810_SELECT_DEPTH);
      I810SubsequentScreenToScreenCopy(pScrn, x1, y1, destx, desty, w, h);
   }
   I810SelectBuffer(pScrn, I810_SELECT_FRONT);
   I810EmitFlush(pScrn);

   if (pboxNew2) {
      DEALLOCATE_LOCAL(pptNew2);
      DEALLOCATE_LOCAL(pboxNew2);
   }
   if (pboxNew1) {
      DEALLOCATE_LOCAL(pptNew1);
      DEALLOCATE_LOCAL(pboxNew1);
   }

   pI810->AccelInfoRec->NeedToSync = TRUE;
}


/* Fullscreen hooks.  The DRI fullscreen mode can probably be removed as
 * it adds little or nothing above the mechanism below (and isn't widely
 * used).
 */
static Bool I810DRIOpenFullScreen(ScreenPtr pScreen)
{
    return TRUE;
}

static Bool I810DRICloseFullScreen(ScreenPtr pScreen)
{
    return TRUE;
}
/* Use the miext/shadow module to maintain a list of dirty rectangles.
 * These are blitted to the back buffer to keep both buffers clean
 * during page-flipping when the 3d application isn't fullscreen.
 *
 * Unlike most use of the shadow code, both buffers are in video memory.
 *
 * An alternative to this would be to organize for all on-screen drawing
 * operations to be duplicated for the two buffers.  That might be
 * faster, but seems like a lot more work...
 */


/* This should be done *before* XAA syncs or fires its buffer.
 * Otherwise will have to fire it again???
 */
static void I810DRIRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    I810Ptr pI810 = I810PTR(pScrn);
    int i;
    I810SAREAPtr  pSAREAPriv = DRIGetSAREAPrivate(pScrn->pScreen);
    unsigned int br13;
    int cpp=2;

    
    /* Don't want to do this when no 3d is active and pages are
     * right-way-round
     */
    if (!pSAREAPriv->pf_active && pSAREAPriv->pf_current_page == 0)
      return;

    br13 = (pI810->auxPitch) | (0xcc << 16);
    
    for (i = 0 ; i < num ; i++, pbox++) {
      unsigned int w = min(pbox->y2, pScrn->virtualY-1) - max(pbox->y1, 0) + 1;
      unsigned int h = min(pbox->x2, pScrn->virtualX-1) - max(pbox->x1, 0) + 1;
      unsigned int dst = max(pbox->x1, 0)*cpp + (max(pbox->y1, 0)*pI810->auxPitch);
      
      BEGIN_LP_RING(6);

      OUT_RING(BR00_BITBLT_CLIENT | BR00_OP_SRC_COPY_BLT | 0x4);
      OUT_RING(br13);
      OUT_RING( (h<<16) | (w*cpp) );
      OUT_RING(pI810->BackBuffer.Start + dst);
      OUT_RING(br13 & 0xffff);
      OUT_RING(dst);

      ADVANCE_LP_RING();
    }

}

static void I810EnablePageFlip(ScreenPtr pScreen)
{
    ScrnInfoPtr         pScrn      = xf86Screens[pScreen->myNum];
    I810Ptr       pI810       = I810PTR(pScrn);
    I810SAREAPtr  pSAREAPriv = DRIGetSAREAPrivate(pScreen);
    int cpp=2;
    pSAREAPriv->pf_enabled = pI810->allowPageFlip;
    pSAREAPriv->pf_active = 0;
    
   if (pI810->allowPageFlip) {
      unsigned int br13 = pI810->auxPitch | (0xcc << 16);
      
      BEGIN_LP_RING(6);

      OUT_RING(BR00_BITBLT_CLIENT | BR00_OP_SRC_COPY_BLT | 0x4);
      OUT_RING(br13);
      OUT_RING((pScrn->virtualY << 16) | (pScrn->virtualX*cpp));
      OUT_RING(pI810->BackBuffer.Start);
      OUT_RING(br13 & 0xFFFF);
      OUT_RING(0);
      ADVANCE_LP_RING();

      pSAREAPriv->pf_active = 1;
   }
   
}

static void I810DisablePageFlip(ScreenPtr pScreen)
{
    I810SAREAPtr  pSAREAPriv = DRIGetSAREAPrivate(pScreen);

    pSAREAPriv->pf_active=0;
}

static void I810DRITransitionSingleToMulti3d(ScreenPtr pScreen)
{
    /* Tell the clients not to pageflip.  How?
     *   -- Field in sarea, plus bumping the window counters.
     *   -- DRM needs to cope with Front-to-Back swapbuffers.
     */
    I810DisablePageFlip(pScreen);
}

static void I810DRITransitionMultiToSingle3d(ScreenPtr pScreen)
{
    /* Let the remaining 3d app start page flipping again */
    I810EnablePageFlip(pScreen);
}

static void I810DRITransitionTo3d(ScreenPtr pScreen)
{
    ScrnInfoPtr    pScrn = xf86Screens[pScreen->myNum];
    I810Ptr  pI810  = I810PTR(pScrn);

    I810EnablePageFlip(pScreen);
    pI810->have3DWindows = 1;
}

static void I810DRITransitionTo2d(ScreenPtr pScreen)
{
    ScrnInfoPtr         pScrn      = xf86Screens[pScreen->myNum];
    I810Ptr       pI810       = I810PTR(pScrn);
    I810SAREAPtr  pSAREAPriv = DRIGetSAREAPrivate(pScreen);

    /* Shut down shadowing if we've made it back to the front page */
    if (pSAREAPriv->pf_current_page == 0) {
	I810DisablePageFlip(pScreen);
    }
    pI810->have3DWindows = 0;
}

Bool
I810DRILeave(ScrnInfoPtr pScrn)
{
   I810Ptr pI810 = I810PTR(pScrn);
    
   if (pI810->directRenderingEnabled) {
      if (pI810->dcacheHandle != 0) 
	 if (drmAgpUnbind(pI810->drmSubFD, pI810->dcacheHandle) != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,"%s\n",strerror(errno));
	    return FALSE;
	 }
      if (pI810->backHandle != 0) 
	 if (drmAgpUnbind(pI810->drmSubFD, pI810->backHandle) != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,"%s\n",strerror(errno));
 	    return FALSE;
	 }
      if (pI810->zHandle != 0)
	 if (drmAgpUnbind(pI810->drmSubFD, pI810->zHandle) != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,"%s\n",strerror(errno));
  	    return FALSE;
	 }
      if (pI810->sysmemHandle != 0)
	 if (drmAgpUnbind(pI810->drmSubFD, pI810->sysmemHandle) != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,"%s\n",strerror(errno));
  	    return FALSE;
	 }
      if (pI810->xvmcHandle != 0)
	 if (drmAgpUnbind(pI810->drmSubFD, pI810->xvmcHandle) != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,"%s\n",strerror(errno));
  	    return FALSE;
	 }
      if (pI810->cursorHandle != 0)
	 if (drmAgpUnbind(pI810->drmSubFD, pI810->cursorHandle) != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,"%s\n",strerror(errno));
	    return FALSE;
	 }
      if (pI810->agpAcquired == TRUE)
	 drmAgpRelease(pI810->drmSubFD);
      pI810->agpAcquired = FALSE;
   }
   return TRUE;
}

Bool
I810DRIEnter(ScrnInfoPtr pScrn)
{
   I810Ptr pI810 = I810PTR(pScrn);

   if (pI810->directRenderingEnabled) {

      if (pI810->agpAcquired == FALSE)
	 drmAgpAcquire(pI810->drmSubFD);
      pI810->agpAcquired = TRUE;
      if (pI810->dcacheHandle != 0)
	 if (drmAgpBind(pI810->drmSubFD, pI810->dcacheHandle,
			pI810->DepthOffset) != 0)
	    return FALSE;
      if (pI810->backHandle != 0)
	 if (drmAgpBind(pI810->drmSubFD, pI810->backHandle,
			pI810->BackOffset) != 0)
	    return FALSE;
      if (pI810->zHandle != 0)
	 if (drmAgpBind(pI810->drmSubFD, pI810->zHandle,
			pI810->DepthOffset) != 0)
	    return FALSE;
      if (pI810->sysmemHandle != 0)
	 if (drmAgpBind(pI810->drmSubFD, pI810->sysmemHandle, 0) != 0)
	    return FALSE;
      if (pI810->xvmcHandle != 0)
	 if (drmAgpBind(pI810->drmSubFD, pI810->xvmcHandle,
			pI810->MC.Start) != 0)
	    return FALSE;
      if (pI810->cursorHandle != 0)
	 if (drmAgpBind(pI810->drmSubFD, pI810->cursorHandle,
			pI810->CursorStart) != 0)
	    return FALSE;
   }
   return TRUE;
}
