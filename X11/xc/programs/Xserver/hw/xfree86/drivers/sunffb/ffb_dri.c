/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_dri.c,v 1.9 2001/05/02 15:06:10 dawes Exp $
 * Acceleration for the Creator and Creator3D framebuffer - DRI/DRM support.
 *
 * Copyright (C) 2000 David S. Miller (davem@redhat.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * DAVID MILLER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Priv.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"
#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb16.h"
#include "cfb32.h"

#include "miline.h"

#include "GL/glxtokens.h"

#include "xf86drm.h"
#include "sarea.h"
#define _XF86DRI_SERVER_
#include "xf86dri.h"
#include "dri.h"
#include "dristruct.h"

#include "GL/glxint.h"

#include "ffb.h"
#include "ffb_regs.h"
#include "ffb_fifo.h"
#include "ffb_rcache.h"

static char FFBKernelDriverName[] = "ffb";
static char FFBClientDriverName[] = "ffb";

/* Forward declarations. */
static Bool FFBDRICreateContext(ScreenPtr, VisualPtr, drmContext,
				void *, DRIContextType);
static void FFBDRIDestroyContext(ScreenPtr, drmContext, DRIContextType);

static void FFBDRIInitBuffers(WindowPtr, RegionPtr, CARD32);
static void FFBDRIMoveBuffers(WindowPtr, DDXPointRec, RegionPtr, CARD32);

static void FFBDRISetDrawableIndex(WindowPtr, CARD32);

/* XXX Why isn't this in a header somewhere? XXX */
extern void GlxSetVisualConfigs(int nconfigs, __GLXvisualConfig *configs,
				void **configprivs);

static Bool
FFBDRIInitVisualConfigs(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	FFBPtr pFfb = GET_FFB_FROM_SCRN(pScrn);
	__GLXvisualConfig *pConfigs;
	FFBConfigPrivPtr pFfbConfigs;
	FFBConfigPrivPtr *pFfbConfigPtrs;

	pConfigs = (__GLXvisualConfig *)
		xcalloc(sizeof(__GLXvisualConfig), 1);
	if (!pConfigs)
		return FALSE;

	pFfbConfigs = (FFBConfigPrivPtr)
		xcalloc(sizeof(FFBConfigPrivRec), 1);
	if (!pFfbConfigs) {
		xfree(pConfigs);
		return FALSE;
	}

	pFfbConfigPtrs = (FFBConfigPrivPtr *)
		xcalloc(sizeof(FFBConfigPrivPtr), 1);
	if (!pFfbConfigPtrs) {
		xfree(pConfigs);
		xfree(pFfbConfigs);
		return FALSE;
	}

	pFfbConfigPtrs[0] = &pFfbConfigs[0];

	pConfigs->vid = -1;
	pConfigs->class = -1;
	pConfigs->rgba = TRUE;
	pConfigs->redSize = 8;
	pConfigs->greenSize = 8;
	pConfigs->blueSize = 8;
	pConfigs->alphaSize = 0;
	pConfigs->redMask = 0x000000ff;
	pConfigs->greenMask = 0x0000ff00;
	pConfigs->blueMask = 0x00ff0000;
	pConfigs->alphaMask = 0;
	pConfigs->accumRedSize = 0;
	pConfigs->accumGreenSize = 0;
	pConfigs->accumBlueSize = 0;
	pConfigs->accumAlphaSize = 0;
	pConfigs->doubleBuffer = TRUE;
	pConfigs->stereo = FALSE;
	pConfigs->bufferSize = 32;
	pConfigs->depthSize = 16;
	pConfigs->stencilSize = 0;
	pConfigs->auxBuffers = 0;
	pConfigs->level = 0;
	pConfigs->visualRating = 0;
	pConfigs->transparentPixel = 0;
	pConfigs->transparentRed = 0;
	pConfigs->transparentGreen = 0;
	pConfigs->transparentBlue = 0;
	pConfigs->transparentAlpha = 0;
	pConfigs->transparentIndex = 0;

	pFfb->numVisualConfigs = 1;
	pFfb->pVisualConfigs = pConfigs;
	pFfb->pVisualConfigsPriv = pFfbConfigs;

	GlxSetVisualConfigs(1, pConfigs, (void **)pFfbConfigPtrs);

	return TRUE;
}

static void
init_ffb_sarea(FFBPtr pFfb, ffb_dri_state_t *pFfbSarea)
{
	int i;

	pFfbSarea->flags = 0;

	switch (pFfb->ffb_type) {
	case ffb2_prototype:
	case ffb2_vertical:
	case ffb2_vertical_plus:
	case ffb2_horizontal:
	case ffb2_horizontal_plus:
		pFfbSarea->flags |= FFB_DRI_FFB2;
		break;

	default:
		break;
	};

	if (pFfb->ffb_type == ffb2_vertical_plus ||
	    pFfb->ffb_type == ffb2_horizontal_plus)
		pFfbSarea->flags |= FFB_DRI_FFB2PLUS;

	if (pFfb->dac_info.flags & FFB_DAC_PAC1)
		pFfbSarea->flags |= FFB_DRI_PAC1;

	if (pFfb->dac_info.flags & FFB_DAC_PAC2)
		pFfbSarea->flags |= FFB_DRI_PAC2;

	for (i = 0; i < FFB_DRI_NWIDS; i++)
		pFfbSarea->wid_table[i] = 0;
}

#define	FFB_DFB24_POFF		0x02000000UL
#define	FFB_DFB24_SIZE		0x01000000UL

#define	FFB_FBC_REGS_POFF	0x00600000UL
#define	FFB_FBC_REGS_SIZE	0x00002000UL

#define	FFB_DAC_POFF		0x00400000UL
#define	FFB_DAC_SIZE		0x00002000UL

#define	FFB_SFB8R_POFF		0x04000000UL
#define FFB_SFB8R_SIZE		0x00400000UL

#define	FFB_SFB32_POFF		0x05000000UL
#define FFB_SFB32_SIZE		0x01000000UL

#define	FFB_SFB64_POFF		0x06000000UL
#define FFB_SFB64_SIZE		0x02000000UL

Bool
FFBDRIScreenInit(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	FFBPtr pFfb = GET_FFB_FROM_SCRN(pScrn);
	DRIInfoPtr pDRIInfo;
	FFBDRIPtr pFfbDRI;

	/* Check that the GLX, DRI, and DRM modules have been loaded by testing
	 * for canonical symbols in each module.
	 */
	if (!xf86LoaderCheckSymbol("GlxSetVisualConfigs"))
		return FALSE;
	if (!xf86LoaderCheckSymbol("DRIScreenInit"))
		return FALSE;
	if (!xf86LoaderCheckSymbol("drmAvailable"))
		return FALSE;
        if (!xf86LoaderCheckSymbol("DRIQueryVersion")) {
      		xf86DrvMsg(pScreen->myNum, X_ERROR,
                 	"FFBDRIScreenInit failed (libdri.a too old)\n");
		return FALSE;
        }

        /* Check the DRI version */
        {
      		int major, minor, patch;
		DRIQueryVersion(&major, &minor, &patch);
		if (major != 4 || minor < 0) {
		xf86DrvMsg(pScreen->myNum, X_ERROR,
                    "[dri] FFBDRIScreenInit failed because of a version mismatch.\n"
		    "[dri] libDRI version is %d.%d.%d but version, 4.0.x is needed.\n"
		    "[dri]  Disabling DRI.\n",
                    major, minor, patch);
		return FALSE;
		}
	}

	pDRIInfo = DRICreateInfoRec();
	if (pDRIInfo == NULL)
		return FALSE;

	pFfb->pDRIInfo = pDRIInfo;

	pDRIInfo->drmDriverName = FFBKernelDriverName;
	pDRIInfo->clientDriverName = FFBClientDriverName;

	pDRIInfo->ddxDriverMajorVersion = 0;
	pDRIInfo->ddxDriverMinorVersion = 0;
	pDRIInfo->ddxDriverPatchVersion = 1;

	pDRIInfo->busIdString = xalloc(64); /* Freed in DRIDestroyInfoRec */
	sprintf(pDRIInfo->busIdString, "SBUS:%s", pFfb->psdp->device);

	/* Dumb rendering port for now... */
	pDRIInfo->frameBufferPhysicalAddress = FFB_DFB24_POFF;
	pDRIInfo->frameBufferSize = FFB_DFB24_SIZE;
	pDRIInfo->frameBufferStride = (2048 * 4);

	/* XXX */
	pDRIInfo->ddxDrawableTableEntry = 15;
	pDRIInfo->maxDrawableTableEntry = 15;
	pDRIInfo->SAREASize = (SAREA_MAX + (0x2000 - 1)) & ~(0x2000 - 1);

	pFfbDRI = (FFBDRIPtr) xcalloc(sizeof(FFBDRIRec), 1);
	if (pFfbDRI == NULL) {
		DRIDestroyInfoRec(pFfb->pDRIInfo);
		return FALSE;
	}

	pDRIInfo->devPrivate		= pFfbDRI;
	pDRIInfo->devPrivateSize	= sizeof(*pFfbDRI);
	pDRIInfo->contextSize		= 0; /* kernel does ctx swaps */

	pDRIInfo->CreateContext		= FFBDRICreateContext;
	pDRIInfo->DestroyContext	= FFBDRIDestroyContext;
	pDRIInfo->InitBuffers		= FFBDRIInitBuffers;
	pDRIInfo->MoveBuffers		= FFBDRIMoveBuffers;
	pDRIInfo->SetDrawableIndex	= FFBDRISetDrawableIndex;

	/* Our InitBuffers depends heavily on this setting. */
	pDRIInfo->bufferRequests	= DRI_3D_WINDOWS_ONLY;

	pDRIInfo->createDummyCtx	= TRUE;
	pDRIInfo->createDummyCtxPriv	= FALSE;

	if (!DRIScreenInit(pScreen, pDRIInfo, &(pFfb->drmSubFD))) {
                xf86DrvMsg(pScreen->myNum, X_ERROR,
                           "[dri] DRIScreenInit failed.  Disabling DRI.\n");
		DRIDestroyInfoRec(pFfb->pDRIInfo);
		xfree(pFfbDRI);
		return FALSE;
	}

#if 000 /* XXX this should be cleaned up and used */
        /* Check the ffb DRM version */
        version = drmGetVersion(info->drmFD);
        if (version) {
           if (version->version_major != 1 ||
               version->version_minor < 0) {
              /* incompatible drm version */
              xf86DrvMsg(pScreen->myNum, X_ERROR,
                         "[dri] FFBDRIScreenInit failed because of a version mismatch.\n"
                         "[dri] ffb.o kernel module version is %d.%d.%d but version 1.0.x is needed.\n"
                         "[dri] Disabling the DRI.\n",
                         version->version_major,
                         version->version_minor,
                         version->version_patchlevel);
              drmFreeVersion(version);
              R128DRICloseScreen(pScreen);
	    return FALSE;
           }
           drmFreeVersion(version);
        }
#endif

	pFfb->pFfbSarea = DRIGetSAREAPrivate(pScreen);
	init_ffb_sarea(pFfb, pFfb->pFfbSarea);

	/* Setup device specific direct rendering memory maps. */
	if (drmAddMap(pFfb->drmSubFD,
		      FFB_FBC_REGS_POFF, FFB_FBC_REGS_SIZE,
		      DRM_REGISTERS, 0, &pFfbDRI->hFbcRegs) < 0) {
		DRICloseScreen(pScreen);
		return FALSE;
	}
	pFfbDRI->sFbcRegs = FFB_FBC_REGS_SIZE;
	pFfbDRI->mFbcRegs = 0;

	xf86DrvMsg(pScreen->myNum, X_INFO,
		   "[drm] FBC Register handle = 0x%08x\n",
		   pFfbDRI->hFbcRegs);

	if (drmAddMap(pFfb->drmSubFD,
		      FFB_DAC_POFF, FFB_DAC_SIZE,
		      DRM_REGISTERS, 0, &pFfbDRI->hDacRegs) < 0) {
		DRICloseScreen(pScreen);
		return FALSE;
	}
	pFfbDRI->sDacRegs = FFB_DAC_SIZE;
	pFfbDRI->mDacRegs = 0;

	xf86DrvMsg(pScreen->myNum, X_INFO,
		   "[drm] DAC Register handle = 0x%08x\n",
		   pFfbDRI->hDacRegs);

	/* Now add maps for the "Smart" views of the framebuffer. */
	if (drmAddMap(pFfb->drmSubFD,
		      FFB_SFB8R_POFF, FFB_SFB8R_SIZE,
		      DRM_REGISTERS, 0, &pFfbDRI->hSfb8r) < 0) {
		DRICloseScreen(pScreen);
		return FALSE;
	}
	pFfbDRI->sSfb8r = FFB_SFB8R_SIZE;
	pFfbDRI->mSfb8r = 0;

	xf86DrvMsg(pScreen->myNum, X_INFO,
		   "[drm] SFB8R handle = 0x%08x\n",
		   pFfbDRI->hSfb8r);

	if (drmAddMap(pFfb->drmSubFD,
		      FFB_SFB32_POFF, FFB_SFB32_SIZE,
		      DRM_REGISTERS, 0, &pFfbDRI->hSfb32) < 0) {
		DRICloseScreen(pScreen);
		return FALSE;
	}
	pFfbDRI->sSfb32 = FFB_SFB32_SIZE;
	pFfbDRI->mSfb32 = 0;

	xf86DrvMsg(pScreen->myNum, X_INFO,
		   "[drm] SFB32 handle = 0x%08x\n",
		   pFfbDRI->hSfb32);

	if (drmAddMap(pFfb->drmSubFD,
		      FFB_SFB64_POFF, FFB_SFB64_SIZE,
		      DRM_REGISTERS, 0, &pFfbDRI->hSfb64) < 0) {
		DRICloseScreen(pScreen);
		return FALSE;
	}
	pFfbDRI->sSfb64 = FFB_SFB64_SIZE;
	pFfbDRI->mSfb64 = 0;

	xf86DrvMsg(pScreen->myNum, X_INFO,
		   "[drm] SFB64 handle = 0x%08x\n",
		   pFfbDRI->hSfb64);

	/* Setup visual configurations. */
	if (!FFBDRIInitVisualConfigs(pScreen)) {
		DRICloseScreen(pScreen);
		return FALSE;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "[drm] Visual configs initialized\n");

	return TRUE;
}

void
FFBDRICloseScreen(ScreenPtr pScreen)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pScreen);

	DRICloseScreen(pScreen);

	if (pFfb->pDRIInfo) {
		DRIInfoPtr pDRIInfo = pFfb->pDRIInfo;

		if (pDRIInfo->devPrivate)
			xfree(pDRIInfo->devPrivate);
		DRIDestroyInfoRec(pDRIInfo);
		pFfb->pDRIInfo = NULL;
	}

	if (pFfb->pVisualConfigs) {
		xfree(pFfb->pVisualConfigs);
		pFfb->pVisualConfigs = NULL;
	}
	if (pFfb->pVisualConfigsPriv) {
		xfree(pFfb->pVisualConfigsPriv);
		pFfb->pVisualConfigsPriv = NULL;
	}
}

static Bool
FFBDRICreateContext(ScreenPtr pScreen, VisualPtr visual, drmContext hwContext,
		 void *pVisualConfigPriv, DRIContextType context)
{
	/* Nothing to do... */
	return TRUE;
}

static void
FFBDRIDestroyContext(ScreenPtr pScreen, drmContext hwContext, DRIContextType context)
{
	/* Nothing to do... */
}

Bool
FFBDRIFinishScreenInit(ScreenPtr pScreen)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pScreen);
	DRIInfoPtr pDRIInfo = pFfb->pDRIInfo;
	FFBDRIPtr pFfbDRI = (FFBDRIPtr) pDRIInfo->devPrivate;
	int i;

	/* This belongs in the kernel.  I'm sorry, the rest
	 * of the current DRI switching mechanisms just suck.
	 */
	pDRIInfo->driverSwapMethod = DRI_KERNEL_SWAP;

	/* Copy over the fast/page filling parameters now that
	 * acceleration has been fully setup.
	 */
	pFfbDRI->disable_pagefill = pFfb->disable_pagefill;
	pFfbDRI->fastfill_small_area = FFB_FFPARMS(pFfb).fastfill_small_area;
	pFfbDRI->pagefill_small_area = FFB_FFPARMS(pFfb).pagefill_small_area;
	pFfbDRI->fastfill_height = FFB_FFPARMS(pFfb).fastfill_height;
	pFfbDRI->fastfill_width = FFB_FFPARMS(pFfb).fastfill_width;
	pFfbDRI->pagefill_height = FFB_FFPARMS(pFfb).pagefill_height;
	pFfbDRI->pagefill_width = FFB_FFPARMS(pFfb).pagefill_width;
	for (i = 0; i < 0x800; i++)
		pFfbDRI->Pf_AlignTab[i] = pFfb->Pf_AlignTab[i];

	return DRIFinishScreenInit(pScreen);
}

static void
FFBDRIInitBuffers(WindowPtr pWin, RegionPtr prgn, CARD32 index)
{
	ScreenPtr pScreen = pWin->drawable.pScreen;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pScreen);
	CreatorPrivWinPtr pFfbPrivWin = CreatorGetWindowPrivate(pWin);
	ffb_fbcPtr ffb = pFfb->regs;
	unsigned int fbc;
	BoxPtr pBox;
	int nBox;

	fbc = pFfbPrivWin->fbc_base;
	fbc = (fbc & ~FFB_FBC_WB_MASK) | FFB_FBC_WB_AB;
	fbc = (fbc & ~FFB_FBC_XE_MASK) | FFB_FBC_XE_ON;
	fbc = (fbc & ~FFB_FBC_RGBE_MASK) | FFB_FBC_RGBE_OFF;

	pBox = REGION_RECTS(prgn);
	nBox = (int) REGION_NUM_RECTS(prgn);
	FFB_WRITE_ROP(pFfb, ffb, (FFB_ROP_NEW | (FFB_ROP_NEW << 8)));
	FFB_WRITE_PPC(pFfb, ffb,
		      (FFB_PPC_APE_DISABLE | FFB_PPC_CS_CONST | FFB_PPC_XS_WID),
		      (FFB_PPC_APE_MASK | FFB_PPC_CS_MASK | FFB_PPC_XS_MASK));
	FFB_WRITE_PMASK(pFfb, ffb, ~0);
	FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_RECTANGLE);
	FFB_WRITE_FBC(pFfb, ffb, fbc);
	FFB_WRITE_WID(pFfb, ffb, FFB_WID_WIN(pWin));

	while(nBox--) {
		register int x, y, w, h;

		x = pBox->x1;
		y = pBox->y1;
		w = (pBox->x2 - x);
		h = (pBox->y2 - y);
		FFBFifo(pFfb, 4);
		FFB_WRITE64(&ffb->by, y, x);
		FFB_WRITE64_2(&ffb->bh, h, w);
		pBox++;
	}
	pFfb->rp_active = 1;
	FFBSync(pFfb, ffb);
}

static void
FFBDRIMoveBuffers(WindowPtr pParent, DDXPointRec ptOldOrg,
		  RegionPtr prgnSrc, CARD32 index)
{
}

static void
FFBDRISetDrawableIndex(WindowPtr pWin, CARD32 index)
{
	ScreenPtr pScreen = pWin->drawable.pScreen;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pScreen);
	CreatorPrivWinPtr pFfbPrivWin = CreatorGetWindowPrivate(pWin);
	unsigned int wid;

	if (FFBWidIsShared(pFfb, pFfbPrivWin->wid)) {
		wid = FFBWidUnshare(pFfb, pFfbPrivWin->wid);
		if (wid == (unsigned int) -1)
			return;

		ErrorF("FFB: Allocated WID %x for DRI window.\n", wid);
		pFfbPrivWin->wid = wid;

		/* Now update the SAREA. */
		pFfb->pFfbSarea->wid_table[index] = wid;
	}
}
