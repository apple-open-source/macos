/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/r128_dri.c,v 1.28 2003/02/07 20:41:14 martin Exp $ */
/*
 * Copyright 1999, 2000 ATI Technologies Inc., Markham, Ontario,
 *                      Precision Insight, Inc., Cedar Park, Texas, and
 *                      VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, PRECISION INSIGHT, VA LINUX
 * SYSTEMS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Kevin E. Martin <martin@valinux.com>
 *   Rickard E. Faith <faith@valinux.com>
 *   Daryll Strauss <daryll@valinux.com>
 *   Gareth Hughes <gareth@valinux.com>
 *
 */

				/* Driver data structures */
#include "r128.h"
#include "r128_dri.h"
#include "r128_reg.h"
#include "r128_sarea.h"
#include "r128_version.h"

				/* X and server generic header files */
#include "xf86.h"
#include "windowstr.h"
#include "xf86PciInfo.h"

				/* GLX/DRI/DRM definitions */
#define _XF86DRI_SERVER_
#include "GL/glxtokens.h"
#include "sarea.h"

/* ?? HACK - for now, put this here... */
/* ?? Alpha - this may need to be a variable to handle UP1x00 vs TITAN */
#if defined(__alpha__)
# define DRM_PAGE_SIZE 8192
#elif defined(__ia64__)
# define DRM_PAGE_SIZE getpagesize()
#else
# define DRM_PAGE_SIZE 4096
#endif

/* Initialize the visual configs that are supported by the hardware.
   These are combined with the visual configs that the indirect
   rendering core supports, and the intersection is exported to the
   client. */
static Bool R128InitVisualConfigs(ScreenPtr pScreen)
{
    ScrnInfoPtr       pScrn            = xf86Screens[pScreen->myNum];
    R128InfoPtr       info             = R128PTR(pScrn);
    int               numConfigs       = 0;
    __GLXvisualConfig *pConfigs        = 0;
    R128ConfigPrivPtr pR128Configs     = 0;
    R128ConfigPrivPtr *pR128ConfigPtrs = 0;
    int               i, accum, stencil, db;

    switch (info->CurrentLayout.pixel_code) {
    case 8:  /* 8bpp mode is not support */
    case 15: /* FIXME */
    case 24: /* FIXME */
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[dri] R128DRIScreenInit failed (depth %d not supported).  "
		   "Disabling DRI.\n", info->CurrentLayout.pixel_code);
	return FALSE;

#define R128_USE_ACCUM   1
#define R128_USE_STENCIL 1
#define R128_USE_DB      1

    case 16:
	numConfigs = 1;
	if (R128_USE_ACCUM)   numConfigs *= 2;
	if (R128_USE_STENCIL) numConfigs *= 2;
	if (R128_USE_DB)      numConfigs *= 2;

	if (!(pConfigs
	      = (__GLXvisualConfig*)xcalloc(sizeof(__GLXvisualConfig),
					      numConfigs))) {
	    return FALSE;
	}
	if (!(pR128Configs
	      = (R128ConfigPrivPtr)xcalloc(sizeof(R128ConfigPrivRec),
					     numConfigs))) {
	    xfree(pConfigs);
	    return FALSE;
	}
	if (!(pR128ConfigPtrs
	      = (R128ConfigPrivPtr*)xcalloc(sizeof(R128ConfigPrivPtr),
					      numConfigs))) {
	    xfree(pConfigs);
	    xfree(pR128Configs);
	    return FALSE;
	}

	i = 0;
	for (db = 0; db <= R128_USE_DB; db++) {
	  for (accum = 0; accum <= R128_USE_ACCUM; accum++) {
	    for (stencil = 0; stencil <= R128_USE_STENCIL; stencil++) {
		pR128ConfigPtrs[i] = &pR128Configs[i];

		pConfigs[i].vid                = (VisualID)(-1);
		pConfigs[i].class              = -1;
		pConfigs[i].rgba               = TRUE;
		pConfigs[i].redSize            = 5;
		pConfigs[i].greenSize          = 6;
		pConfigs[i].blueSize           = 5;
		pConfigs[i].alphaSize          = 0;
		pConfigs[i].redMask            = 0x0000F800;
		pConfigs[i].greenMask          = 0x000007E0;
		pConfigs[i].blueMask           = 0x0000001F;
		pConfigs[i].alphaMask          = 0x00000000;
		if (accum) { /* Simulated in software */
		    pConfigs[i].accumRedSize   = 16;
		    pConfigs[i].accumGreenSize = 16;
		    pConfigs[i].accumBlueSize  = 16;
		    pConfigs[i].accumAlphaSize = 0;
		} else {
		    pConfigs[i].accumRedSize   = 0;
		    pConfigs[i].accumGreenSize = 0;
		    pConfigs[i].accumBlueSize  = 0;
		    pConfigs[i].accumAlphaSize = 0;
		}
		if (db) 
		    pConfigs[i].doubleBuffer       = TRUE;
		else
		    pConfigs[i].doubleBuffer       = FALSE;
		pConfigs[i].stereo             = FALSE;
		pConfigs[i].bufferSize         = 16;
		pConfigs[i].depthSize          = 16;
		if (stencil)
		    pConfigs[i].stencilSize    = 8; /* Simulated in software */
		else
		    pConfigs[i].stencilSize    = 0;
		pConfigs[i].auxBuffers         = 0;
		pConfigs[i].level              = 0;
		if (accum || stencil) {
		   pConfigs[i].visualRating    = GLX_SLOW_VISUAL_EXT;
		} else {
		   pConfigs[i].visualRating    = GLX_NONE_EXT;
		}
		pConfigs[i].transparentPixel   = GLX_NONE;
		pConfigs[i].transparentRed     = 0;
		pConfigs[i].transparentGreen   = 0;
		pConfigs[i].transparentBlue    = 0;
		pConfigs[i].transparentAlpha   = 0;
		pConfigs[i].transparentIndex   = 0;
		i++;
	    }
	  }
	}
	break;

    case 32:
	numConfigs = 1;
	if (R128_USE_ACCUM)   numConfigs *= 2;
	if (R128_USE_STENCIL) numConfigs *= 2;
	if (R128_USE_DB)      numConfigs *= 2;

	if (!(pConfigs
	      = (__GLXvisualConfig*)xcalloc(sizeof(__GLXvisualConfig),
					      numConfigs))) {
	    return FALSE;
	}
	if (!(pR128Configs
	      = (R128ConfigPrivPtr)xcalloc(sizeof(R128ConfigPrivRec),
					     numConfigs))) {
	    xfree(pConfigs);
	    return FALSE;
	}
	if (!(pR128ConfigPtrs
	      = (R128ConfigPrivPtr*)xcalloc(sizeof(R128ConfigPrivPtr),
					      numConfigs))) {
	    xfree(pConfigs);
	    xfree(pR128Configs);
	    return FALSE;
	}

	i = 0;
	for (db = 0; db <= R128_USE_DB; db++) {
	  for (accum = 0; accum <= R128_USE_ACCUM; accum++) {
	    for (stencil = 0; stencil <= R128_USE_STENCIL; stencil++) {
		pR128ConfigPtrs[i] = &pR128Configs[i];

		pConfigs[i].vid                = (VisualID)(-1);
		pConfigs[i].class              = -1;
		pConfigs[i].rgba               = TRUE;
		pConfigs[i].redSize            = 8;
		pConfigs[i].greenSize          = 8;
		pConfigs[i].blueSize           = 8;
		pConfigs[i].alphaSize          = 0;
		pConfigs[i].redMask            = 0x00FF0000;
		pConfigs[i].greenMask          = 0x0000FF00;
		pConfigs[i].blueMask           = 0x000000FF;
		pConfigs[i].alphaMask          = 0x00000000;
		if (accum) { /* Simulated in software */
		    pConfigs[i].accumRedSize   = 16;
		    pConfigs[i].accumGreenSize = 16;
		    pConfigs[i].accumBlueSize  = 16;
		    pConfigs[i].accumAlphaSize = 0;
		} else {
		    pConfigs[i].accumRedSize   = 0;
		    pConfigs[i].accumGreenSize = 0;
		    pConfigs[i].accumBlueSize  = 0;
		    pConfigs[i].accumAlphaSize = 0;
		}
		if (db)
		    pConfigs[i].doubleBuffer       = TRUE;
		else
		    pConfigs[i].doubleBuffer       = FALSE;
		pConfigs[i].stereo             = FALSE;
		pConfigs[i].bufferSize         = 24;
		if (stencil) {
		    pConfigs[i].depthSize      = 24;
		    pConfigs[i].stencilSize    = 8;
		} else {
		    pConfigs[i].depthSize      = 24;
		    pConfigs[i].stencilSize    = 0;
		}
		pConfigs[i].auxBuffers         = 0;
		pConfigs[i].level              = 0;
		if (accum || stencil) {
		   pConfigs[i].visualRating    = GLX_SLOW_VISUAL_EXT;
		} else {
		   pConfigs[i].visualRating    = GLX_NONE_EXT;
		}
		pConfigs[i].transparentPixel   = GLX_NONE;
		pConfigs[i].transparentRed     = 0;
		pConfigs[i].transparentGreen   = 0;
		pConfigs[i].transparentBlue    = 0;
		pConfigs[i].transparentAlpha   = 0;
		pConfigs[i].transparentIndex   = 0;
		i++;
	    }
	  }
	}
	break;
    }

    info->numVisualConfigs   = numConfigs;
    info->pVisualConfigs     = pConfigs;
    info->pVisualConfigsPriv = pR128Configs;
    GlxSetVisualConfigs(numConfigs, pConfigs, (void**)pR128ConfigPtrs);
    return TRUE;
}

/* Create the Rage 128-specific context information */
static Bool R128CreateContext(ScreenPtr pScreen, VisualPtr visual,
			      drmContext hwContext, void *pVisualConfigPriv,
			      DRIContextType contextStore)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    R128InfoPtr info = R128PTR(pScrn);

    info->drmCtx = hwContext;
    return TRUE;
}

/* Destroy the Rage 128-specific context information */
static void R128DestroyContext(ScreenPtr pScreen, drmContext hwContext,
			       DRIContextType contextStore)
{
    /* Nothing yet */
}

/* Called when the X server is woken up to allow the last client's
   context to be saved and the X server's context to be loaded.  This is
   not necessary for the Rage 128 since the client detects when it's
   context is not currently loaded and then load's it itself.  Since the
   registers to start and stop the CCE are privileged, only the X server
   can start/stop the engine. */
static void R128EnterServer(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    R128InfoPtr info = R128PTR(pScrn);

    if (info->accel) info->accel->NeedToSync = TRUE;
}

/* Called when the X server goes to sleep to allow the X server's
   context to be saved and the last client's context to be loaded.  This
   is not necessary for the Rage 128 since the client detects when it's
   context is not currently loaded and then load's it itself.  Since the
   registers to start and stop the CCE are privileged, only the X server
   can start/stop the engine. */
static void R128LeaveServer(ScreenPtr pScreen)
{
    ScrnInfoPtr   pScrn     = xf86Screens[pScreen->myNum];
    R128InfoPtr   info      = R128PTR(pScrn);
    unsigned char *R128MMIO = info->MMIO;

    if (!info->directRenderingEnabled) {
	/* Save all hardware scissors */
	info->sc_left     = INREG(R128_SC_LEFT);
	info->sc_right    = INREG(R128_SC_RIGHT);
	info->sc_top      = INREG(R128_SC_TOP);
	info->sc_bottom   = INREG(R128_SC_BOTTOM);
	info->aux_sc_cntl = INREG(R128_SC_BOTTOM);
    } else if (info->CCEInUse) {
	R128CCEReleaseIndirect(pScrn);

	info->CCEInUse = FALSE;
    }
}

/* Contexts can be swapped by the X server if necessary.  This callback
   is currently only used to perform any functions necessary when
   entering or leaving the X server, and in the future might not be
   necessary. */
static void R128DRISwapContext(ScreenPtr pScreen, DRISyncType syncType,
			       DRIContextType oldContextType, void *oldContext,
			       DRIContextType newContextType, void *newContext)
{
    if ((syncType==DRI_3D_SYNC) && (oldContextType==DRI_2D_CONTEXT) &&
	(newContextType==DRI_2D_CONTEXT)) { /* Entering from Wakeup */
	R128EnterServer(pScreen);
    }
    if ((syncType==DRI_2D_SYNC) && (oldContextType==DRI_NO_CONTEXT) &&
	(newContextType==DRI_2D_CONTEXT)) { /* Exiting from Block Handler */
	R128LeaveServer(pScreen);
    }
}

/* Initialize the state of the back and depth buffers. */
static void R128DRIInitBuffers(WindowPtr pWin, RegionPtr prgn, CARD32 indx)
{
    /* FIXME: This routine needs to have acceleration turned on */
    ScreenPtr   pScreen = pWin->drawable.pScreen;
    ScrnInfoPtr pScrn   = xf86Screens[pScreen->myNum];
    R128InfoPtr info    = R128PTR(pScrn);
    BoxPtr      pbox, pboxSave;
    int         nbox, nboxSave;
    int         depth;

    /* FIXME: Use accel when CCE 2D code is written */
    if (info->directRenderingEnabled)
	return;

    /* FIXME: This should be based on the __GLXvisualConfig info */
    switch (pScrn->bitsPerPixel) {
    case  8: depth = 0x000000ff; break;
    case 16: depth = 0x0000ffff; break;
    case 24: depth = 0x00ffffff; break;
    case 32: depth = 0xffffffff; break;
    default: depth = 0x00000000; break;
    }

    /* FIXME: Copy XAAPaintWindow() and use REGION_TRANSLATE() */
    /* FIXME: Only initialize the back and depth buffers for contexts
       that request them */

    pboxSave = pbox = REGION_RECTS(prgn);
    nboxSave = nbox = REGION_NUM_RECTS(prgn);

    (*info->accel->SetupForSolidFill)(pScrn, 0, GXcopy, (CARD32)(-1));
    for (; nbox; nbox--, pbox++) {
	(*info->accel->SubsequentSolidFillRect)(pScrn,
						pbox->x1 + info->fbX,
						pbox->y1 + info->fbY,
						pbox->x2 - pbox->x1,
						pbox->y2 - pbox->y1);
	(*info->accel->SubsequentSolidFillRect)(pScrn,
						pbox->x1 + info->backX,
						pbox->y1 + info->backY,
						pbox->x2 - pbox->x1,
						pbox->y2 - pbox->y1);
    }

    pbox = pboxSave;
    nbox = nboxSave;

    /* FIXME: this needs to consider depth tiling. */
    (*info->accel->SetupForSolidFill)(pScrn, depth, GXcopy, (CARD32)(-1));
    for (; nbox; nbox--, pbox++)
	(*info->accel->SubsequentSolidFillRect)(pScrn,
						pbox->x1 + info->depthX,
						pbox->y1 + info->depthY,
						pbox->x2 - pbox->x1,
						pbox->y2 - pbox->y1);

    info->accel->NeedToSync = TRUE;
}

/* Copy the back and depth buffers when the X server moves a window. */
static void R128DRIMoveBuffers(WindowPtr pWin, DDXPointRec ptOldOrg,
			       RegionPtr prgnSrc, CARD32 indx)
{
    ScreenPtr   pScreen = pWin->drawable.pScreen;
    ScrnInfoPtr pScrn   = xf86Screens[pScreen->myNum];
    R128InfoPtr info   = R128PTR(pScrn);

    /* FIXME: This routine needs to have acceleration turned on */
    /* FIXME: Copy XAACopyWindow() and use REGION_TRANSLATE() */
    /* FIXME: Only initialize the back and depth buffers for contexts
       that request them */

    /* FIXME: Use accel when CCE 2D code is written */
    if (info->directRenderingEnabled)
	return;
}

/* Initialize the AGP state.  Request memory for use in AGP space, and
   initialize the Rage 128 registers to point to that memory. */
static Bool R128DRIAgpInit(R128InfoPtr info, ScreenPtr pScreen)
{
    unsigned char *R128MMIO = info->MMIO;
    unsigned long mode;
    unsigned int  vendor, device;
    int           ret;
    unsigned long cntl, chunk;
    int           s, l;
    int           flags;
    unsigned long agpBase;

    if (drmAgpAcquire(info->drmFD) < 0) {
	xf86DrvMsg(pScreen->myNum, X_WARNING, "[agp] AGP not available\n");
	return FALSE;
    }

				/* Modify the mode if the default mode is
				   not appropriate for this particular
				   combination of graphics card and AGP
				   chipset. */

    mode   = drmAgpGetMode(info->drmFD);        /* Default mode */
    vendor = drmAgpVendorId(info->drmFD);
    device = drmAgpDeviceId(info->drmFD);

    mode &= ~R128_AGP_MODE_MASK;
    switch (info->agpMode) {
    case 4:          mode |= R128_AGP_4X_MODE;
    case 2:          mode |= R128_AGP_2X_MODE;
    case 1: default: mode |= R128_AGP_1X_MODE;
    }

    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[agp] Mode 0x%08lx [AGP 0x%04x/0x%04x; Card 0x%04x/0x%04x]\n",
	       mode, vendor, device,
	       info->PciInfo->vendor,
	       info->PciInfo->chipType);

    if (drmAgpEnable(info->drmFD, mode) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR, "[agp] AGP not enabled\n");
	drmAgpRelease(info->drmFD);
	return FALSE;
    }

    info->agpOffset = 0;

    if ((ret = drmAgpAlloc(info->drmFD, info->agpSize*1024*1024, 0, NULL,
			   &info->agpMemHandle)) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR, "[agp] Out of memory (%d)\n", ret);
	drmAgpRelease(info->drmFD);
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[agp] %d kB allocated with handle 0x%08x\n",
	       info->agpSize*1024, info->agpMemHandle);

    if (drmAgpBind(info->drmFD, info->agpMemHandle, info->agpOffset) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR, "[agp] Could not bind\n");
	drmAgpFree(info->drmFD, info->agpMemHandle);
	drmAgpRelease(info->drmFD);
	return FALSE;
    }

				/* Initialize the CCE ring buffer data */
    info->ringStart       = info->agpOffset;
    info->ringMapSize     = info->ringSize*1024*1024 + DRM_PAGE_SIZE;
    info->ringSizeLog2QW  = R128MinBits(info->ringSize*1024*1024/8) - 1;

    info->ringReadOffset  = info->ringStart + info->ringMapSize;
    info->ringReadMapSize = DRM_PAGE_SIZE;

				/* Reserve space for vertex/indirect buffers */
    info->bufStart        = info->ringReadOffset + info->ringReadMapSize;
    info->bufMapSize      = info->bufSize*1024*1024;

				/* Reserve the rest for AGP textures */
    info->agpTexStart     = info->bufStart + info->bufMapSize;
    s = (info->agpSize*1024*1024 - info->agpTexStart);
    l = R128MinBits((s-1) / R128_NR_TEX_REGIONS);
    if (l < R128_LOG_TEX_GRANULARITY) l = R128_LOG_TEX_GRANULARITY;
    info->agpTexMapSize   = (s >> l) << l;
    info->log2AGPTexGran  = l;

    if (info->CCESecure) flags = DRM_READ_ONLY;
    else                  flags = 0;

    if (drmAddMap(info->drmFD, info->ringStart, info->ringMapSize,
		  DRM_AGP, flags, &info->ringHandle) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[agp] Could not add ring mapping\n");
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[agp] ring handle = 0x%08lx\n", info->ringHandle);

    if (drmMap(info->drmFD, info->ringHandle, info->ringMapSize,
	       (drmAddressPtr)&info->ring) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR, "[agp] Could not map ring\n");
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[agp] Ring mapped at 0x%08lx\n",
	       (unsigned long)info->ring);

    if (drmAddMap(info->drmFD, info->ringReadOffset, info->ringReadMapSize,
		  DRM_AGP, flags, &info->ringReadPtrHandle) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[agp] Could not add ring read ptr mapping\n");
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[agp] ring read ptr handle = 0x%08lx\n",
	       info->ringReadPtrHandle);

    if (drmMap(info->drmFD, info->ringReadPtrHandle, info->ringReadMapSize,
	       (drmAddressPtr)&info->ringReadPtr) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[agp] Could not map ring read ptr\n");
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[agp] Ring read ptr mapped at 0x%08lx\n",
	       (unsigned long)info->ringReadPtr);

    if (drmAddMap(info->drmFD, info->bufStart, info->bufMapSize,
		  DRM_AGP, 0, &info->bufHandle) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[agp] Could not add vertex/indirect buffers mapping\n");
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[agp] vertex/indirect buffers handle = 0x%08lx\n",
	       info->bufHandle);

    if (drmMap(info->drmFD, info->bufHandle, info->bufMapSize,
	       (drmAddressPtr)&info->buf) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[agp] Could not map vertex/indirect buffers\n");
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[agp] Vertex/indirect buffers mapped at 0x%08lx\n",
	       (unsigned long)info->buf);

    if (drmAddMap(info->drmFD, info->agpTexStart, info->agpTexMapSize,
		  DRM_AGP, 0, &info->agpTexHandle) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[agp] Could not add AGP texture map mapping\n");
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[agp] AGP texture map handle = 0x%08lx\n",
	       info->agpTexHandle);

    if (drmMap(info->drmFD, info->agpTexHandle, info->agpTexMapSize,
	       (drmAddressPtr)&info->agpTex) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[agp] Could not map AGP texture map\n");
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[agp] AGP Texture map mapped at 0x%08lx\n",
	       (unsigned long)info->agpTex);

				/* Initialize Rage 128's AGP registers */
    cntl  = INREG(R128_AGP_CNTL);
    cntl &= ~R128_AGP_APER_SIZE_MASK;
    switch (info->agpSize) {
    case 256: cntl |= R128_AGP_APER_SIZE_256MB; break;
    case 128: cntl |= R128_AGP_APER_SIZE_128MB; break;
    case  64: cntl |= R128_AGP_APER_SIZE_64MB;  break;
    case  32: cntl |= R128_AGP_APER_SIZE_32MB;  break;
    case  16: cntl |= R128_AGP_APER_SIZE_16MB;  break;
    case   8: cntl |= R128_AGP_APER_SIZE_8MB;   break;
    case   4: cntl |= R128_AGP_APER_SIZE_4MB;   break;
    default:
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[agp] Illegal aperture size %d kB\n",
		   info->agpSize*1024);
	return FALSE;
    }
    agpBase = drmAgpBase(info->drmFD);
    OUTREG(R128_AGP_BASE, agpBase); 
    OUTREG(R128_AGP_CNTL, cntl);

				/* Disable Rage 128's PCIGART registers */
    chunk = INREG(R128_BM_CHUNK_0_VAL);
    chunk &= ~(R128_BM_PTR_FORCE_TO_PCI |
	       R128_BM_PM4_RD_FORCE_TO_PCI |
	       R128_BM_GLOBAL_FORCE_TO_PCI);
    OUTREG(R128_BM_CHUNK_0_VAL, chunk);

    OUTREG(R128_PCI_GART_PAGE, 1); /* Ensure AGP GART is used (for now) */

    return TRUE;
}

static Bool R128DRIPciInit(R128InfoPtr info, ScreenPtr pScreen)
{
    unsigned char *R128MMIO = info->MMIO;
    CARD32 chunk;
    int ret;
    int flags;

    info->agpOffset = 0;

    ret = drmScatterGatherAlloc(info->drmFD, info->agpSize*1024*1024,
				&info->pciMemHandle);
    if (ret < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR, "[pci] Out of memory (%d)\n", ret);
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[pci] %d kB allocated with handle 0x%08x\n",
	       info->agpSize*1024, info->pciMemHandle);

				/* Initialize the CCE ring buffer data */
    info->ringStart       = info->agpOffset;
    info->ringMapSize     = info->ringSize*1024*1024 + DRM_PAGE_SIZE;
    info->ringSizeLog2QW  = R128MinBits(info->ringSize*1024*1024/8) - 1;

    info->ringReadOffset  = info->ringStart + info->ringMapSize;
    info->ringReadMapSize = DRM_PAGE_SIZE;

				/* Reserve space for vertex/indirect buffers */
    info->bufStart        = info->ringReadOffset + info->ringReadMapSize;
    info->bufMapSize      = info->bufSize*1024*1024;

    flags = DRM_READ_ONLY | DRM_LOCKED | DRM_KERNEL;

    if (drmAddMap(info->drmFD, info->ringStart, info->ringMapSize,
		  DRM_SCATTER_GATHER, flags, &info->ringHandle) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[pci] Could not add ring mapping\n");
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[pci] ring handle = 0x%08lx\n", info->ringHandle);

    if (drmMap(info->drmFD, info->ringHandle, info->ringMapSize,
	       (drmAddressPtr)&info->ring) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR, "[pci] Could not map ring\n");
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[pci] Ring mapped at 0x%08lx\n",
	       (unsigned long)info->ring);
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[pci] Ring contents 0x%08lx\n",
	       *(unsigned long *)(pointer)info->ring);

    if (drmAddMap(info->drmFD, info->ringReadOffset, info->ringReadMapSize,
		  DRM_SCATTER_GATHER, flags, &info->ringReadPtrHandle) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[pci] Could not add ring read ptr mapping\n");
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[pci] ring read ptr handle = 0x%08lx\n",
	       info->ringReadPtrHandle);

    if (drmMap(info->drmFD, info->ringReadPtrHandle, info->ringReadMapSize,
	       (drmAddressPtr)&info->ringReadPtr) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[pci] Could not map ring read ptr\n");
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[pci] Ring read ptr mapped at 0x%08lx\n",
	       (unsigned long)info->ringReadPtr);
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[pci] Ring read ptr contents 0x%08lx\n",
	       *(unsigned long *)(pointer)info->ringReadPtr);

    if (drmAddMap(info->drmFD, info->bufStart, info->bufMapSize,
		  DRM_SCATTER_GATHER, 0, &info->bufHandle) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[pci] Could not add vertex/indirect buffers mapping\n");
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[pci] vertex/indirect buffers handle = 0x%08lx\n",
	       info->bufHandle);

    if (drmMap(info->drmFD, info->bufHandle, info->bufMapSize,
	       (drmAddressPtr)&info->buf) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[pci] Could not map vertex/indirect buffers\n");
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[pci] Vertex/indirect buffers mapped at 0x%08lx\n",
	       (unsigned long)info->buf);
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[pci] Vertex/indirect buffers contents 0x%08lx\n",
	       *(unsigned long *)(pointer)info->buf);

    switch (info->Chipset) {
    case PCI_CHIP_RAGE128LE:
    case PCI_CHIP_RAGE128RE:
    case PCI_CHIP_RAGE128RK:
    case PCI_CHIP_RAGE128PD:
    case PCI_CHIP_RAGE128PP:
    case PCI_CHIP_RAGE128PR:
	/* This is a PCI card, do nothing */
	break;

    case PCI_CHIP_RAGE128LF:
    case PCI_CHIP_RAGE128MF:
    case PCI_CHIP_RAGE128ML:
    case PCI_CHIP_RAGE128RF:
    case PCI_CHIP_RAGE128RG:
    case PCI_CHIP_RAGE128RL:
    case PCI_CHIP_RAGE128SM:
    case PCI_CHIP_RAGE128PF:
    case PCI_CHIP_RAGE128TF:
    case PCI_CHIP_RAGE128TL:
    case PCI_CHIP_RAGE128TR:
    /* FIXME: ATI documentation does not specify if the following chips are
     * AGP or PCI, it just mentions their PCI IDs.  I'm assuming they're AGP
     * until I get more correct information. <mharris@redhat.com>
     */
    case PCI_CHIP_RAGE128PA:
    case PCI_CHIP_RAGE128PB:
    case PCI_CHIP_RAGE128PC:
    case PCI_CHIP_RAGE128PE:
    case PCI_CHIP_RAGE128PG:
    case PCI_CHIP_RAGE128PH:
    case PCI_CHIP_RAGE128PI:
    case PCI_CHIP_RAGE128PJ:
    case PCI_CHIP_RAGE128PK:
    case PCI_CHIP_RAGE128PL:
    case PCI_CHIP_RAGE128PM:
    case PCI_CHIP_RAGE128PN:
    case PCI_CHIP_RAGE128PO:
    case PCI_CHIP_RAGE128PQ:
    case PCI_CHIP_RAGE128PS:
    case PCI_CHIP_RAGE128PT:
    case PCI_CHIP_RAGE128PU:
    case PCI_CHIP_RAGE128PV:
    case PCI_CHIP_RAGE128PW:
    case PCI_CHIP_RAGE128PX:
    case PCI_CHIP_RAGE128SE:
    case PCI_CHIP_RAGE128SF:
    case PCI_CHIP_RAGE128SG:
    case PCI_CHIP_RAGE128SH:
    case PCI_CHIP_RAGE128SK:
    case PCI_CHIP_RAGE128SL:
    case PCI_CHIP_RAGE128SN:
    case PCI_CHIP_RAGE128TS:
    case PCI_CHIP_RAGE128TT:
    case PCI_CHIP_RAGE128TU:
    default:
	/* This is really an AGP card, force PCI GART mode */
        chunk = INREG(R128_BM_CHUNK_0_VAL);
        chunk |= (R128_BM_PTR_FORCE_TO_PCI |
		  R128_BM_PM4_RD_FORCE_TO_PCI |
		  R128_BM_GLOBAL_FORCE_TO_PCI);
        OUTREG(R128_BM_CHUNK_0_VAL, chunk);
        OUTREG(R128_PCI_GART_PAGE, 0); /* Ensure PCI GART is used */
        break;
    }

    return TRUE;
}

/* Add a map for the MMIO registers that will be accessed by any
   DRI-based clients. */
static Bool R128DRIMapInit(R128InfoPtr info, ScreenPtr pScreen)
{
    int flags;

    if (info->CCESecure) flags = DRM_READ_ONLY;
    else                 flags = 0;

				/* Map registers */
    info->registerSize = R128_MMIOSIZE;
    if (drmAddMap(info->drmFD, info->MMIOAddr, info->registerSize,
		  DRM_REGISTERS, flags, &info->registerHandle) < 0) {
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[drm] register handle = 0x%08lx\n", info->registerHandle);

    return TRUE;
}

/* Initialize the kernel data structures. */
static int R128DRIKernelInit(R128InfoPtr info, ScreenPtr pScreen)
{
    drmR128Init drmInfo;

    memset( &drmInfo, 0, sizeof(drmR128Init) );

    drmInfo.func                = DRM_R128_INIT_CCE;
    drmInfo.sarea_priv_offset   = sizeof(XF86DRISAREARec);
    drmInfo.is_pci              = info->IsPCI;
    drmInfo.cce_mode            = info->CCEMode;
    drmInfo.cce_secure          = info->CCESecure;
    drmInfo.ring_size           = info->ringSize*1024*1024;
    drmInfo.usec_timeout        = info->CCEusecTimeout;

    drmInfo.fb_bpp              = info->CurrentLayout.pixel_code;
    drmInfo.depth_bpp           = info->CurrentLayout.pixel_code;

    drmInfo.front_offset        = info->frontOffset;
    drmInfo.front_pitch         = info->frontPitch;

    drmInfo.back_offset         = info->backOffset;
    drmInfo.back_pitch          = info->backPitch;

    drmInfo.depth_offset        = info->depthOffset;
    drmInfo.depth_pitch         = info->depthPitch;
    drmInfo.span_offset         = info->spanOffset;

    drmInfo.fb_offset           = info->fbHandle;
    drmInfo.mmio_offset         = info->registerHandle;
    drmInfo.ring_offset         = info->ringHandle;
    drmInfo.ring_rptr_offset    = info->ringReadPtrHandle;
    drmInfo.buffers_offset      = info->bufHandle;
    drmInfo.agp_textures_offset = info->agpTexHandle;

    if (drmCommandWrite(info->drmFD, DRM_R128_INIT,
                        &drmInfo, sizeof(drmR128Init)) < 0)
        return FALSE;

    return TRUE;
}

/* Add a map for the vertex buffers that will be accessed by any
   DRI-based clients. */
static Bool R128DRIBufInit(R128InfoPtr info, ScreenPtr pScreen)
{
				/* Initialize vertex buffers */
    if (info->IsPCI) {
	info->bufNumBufs = drmAddBufs(info->drmFD,
				      info->bufMapSize / R128_BUFFER_SIZE,
				      R128_BUFFER_SIZE,
				      DRM_SG_BUFFER,
				      info->bufStart);
    } else {
	info->bufNumBufs = drmAddBufs(info->drmFD,
				      info->bufMapSize / R128_BUFFER_SIZE,
				      R128_BUFFER_SIZE,
				      DRM_AGP_BUFFER,
				      info->bufStart);
    }
    if (info->bufNumBufs <= 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[drm] Could not create vertex/indirect buffers list\n");
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[drm] Added %d %d byte vertex/indirect buffers\n",
	       info->bufNumBufs, R128_BUFFER_SIZE);

    if (!(info->buffers = drmMapBufs(info->drmFD))) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[drm] Failed to map vertex/indirect buffers list\n");
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[drm] Mapped %d vertex/indirect buffers\n",
	       info->buffers->count);

    return TRUE;
}

static void R128DRIIrqInit(R128InfoPtr info, ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

   if (!info->irq) {
      info->irq = drmGetInterruptFromBusID(
	 info->drmFD,
	 ((pciConfigPtr)info->PciInfo->thisCard)->busnum,
	 ((pciConfigPtr)info->PciInfo->thisCard)->devnum,
	 ((pciConfigPtr)info->PciInfo->thisCard)->funcnum);

      if((drmCtlInstHandler(info->drmFD, info->irq)) != 0) {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "[drm] failure adding irq handler, "
		    "there is a device already using that irq\n"
		    "[drm] falling back to irq-free operation\n");
	 info->irq = 0;
      } else {
          unsigned char *R128MMIO = info->MMIO;
          info->gen_int_cntl = INREG( R128_GEN_INT_CNTL );
      }
   }

   if (info->irq)
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "[drm] dma control initialized, using IRQ %d\n",
		 info->irq);
}

/* Initialize the CCE state, and start the CCE (if used by the X server) */
static void R128DRICCEInit(ScrnInfoPtr pScrn)
{
    R128InfoPtr   info      = R128PTR(pScrn);

				/* Turn on bus mastering */
    info->BusCntl &= ~R128_BUS_MASTER_DIS;

				/* CCEMode is initialized in r128_driver.c */
    switch (info->CCEMode) {
    case R128_PM4_NONPM4:                 info->CCEFifoSize = 0;   break;
    case R128_PM4_192PIO:                 info->CCEFifoSize = 192; break;
    case R128_PM4_192BM:                  info->CCEFifoSize = 192; break;
    case R128_PM4_128PIO_64INDBM:         info->CCEFifoSize = 128; break;
    case R128_PM4_128BM_64INDBM:          info->CCEFifoSize = 128; break;
    case R128_PM4_64PIO_128INDBM:         info->CCEFifoSize = 64;  break;
    case R128_PM4_64BM_128INDBM:          info->CCEFifoSize = 64;  break;
    case R128_PM4_64PIO_64VCBM_64INDBM:   info->CCEFifoSize = 64;  break;
    case R128_PM4_64BM_64VCBM_64INDBM:    info->CCEFifoSize = 64;  break;
    case R128_PM4_64PIO_64VCPIO_64INDPIO: info->CCEFifoSize = 64;  break;
    }

    if (info->directRenderingEnabled) {
				/* Make sure the CCE is on for the X server */
	R128CCE_START(pScrn, info);
    } else {
				/* Make sure the CCE is off for the X server */
	R128CCE_STOP(pScrn, info);
    }
}

/* Initialize the screen-specific data structures for the DRI and the
   Rage 128.  This is the main entry point to the device-specific
   initialization code.  It calls device-independent DRI functions to
   create the DRI data structures and initialize the DRI state. */
Bool R128DRIScreenInit(ScreenPtr pScreen)
{
    ScrnInfoPtr   pScrn = xf86Screens[pScreen->myNum];
    R128InfoPtr   info = R128PTR(pScrn);
    DRIInfoPtr    pDRIInfo;
    R128DRIPtr    pR128DRI;
    int           major, minor, patch;
    drmVersionPtr version;

    /* Check that the GLX, DRI, and DRM modules have been loaded by testing
     * for known symbols in each module. */
    if (!xf86LoaderCheckSymbol("GlxSetVisualConfigs")) return FALSE;
    if (!xf86LoaderCheckSymbol("DRIScreenInit"))       return FALSE;
    if (!xf86LoaderCheckSymbol("drmAvailable"))        return FALSE;
    if (!xf86LoaderCheckSymbol("DRIQueryVersion")) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[dri] R128DRIScreenInit failed (libdri.a too old)\n");
      return FALSE;
    }

    /* Check the DRI version */
    DRIQueryVersion(&major, &minor, &patch);
    if (major != 4 || minor < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		"[dri] R128DRIScreenInit failed because of a version mismatch.\n"
		"[dri] libDRI version is %d.%d.%d but version 4.0.x is needed.\n"
		"[dri] Disabling the DRI.\n",
		major, minor, patch);
	return FALSE;
    }

    switch (info->CurrentLayout.pixel_code) {
    case 8:
	/* These modes are not supported (yet). */
    case 15:
    case 24:
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[dri] R128DRIScreenInit failed (depth %d not supported).  "
		   "[dri] Disabling DRI.\n", info->CurrentLayout.pixel_code);
	return FALSE;

	/* Only 16 and 32 color depths are supports currently. */
    case 16:
    case 32:
	break;
    }

    /* Create the DRI data structure, and fill it in before calling the
       DRIScreenInit(). */
    if (!(pDRIInfo = DRICreateInfoRec())) return FALSE;

    info->pDRIInfo                       = pDRIInfo;
    pDRIInfo->drmDriverName              = R128_DRIVER_NAME;
    pDRIInfo->clientDriverName           = R128_DRIVER_NAME;
    pDRIInfo->busIdString                = xalloc(64);
    sprintf(pDRIInfo->busIdString,
	    "PCI:%d:%d:%d",
	    info->PciInfo->bus,
	    info->PciInfo->device,
	    info->PciInfo->func);
    pDRIInfo->ddxDriverMajorVersion      = R128_VERSION_MAJOR;
    pDRIInfo->ddxDriverMinorVersion      = R128_VERSION_MINOR;
    pDRIInfo->ddxDriverPatchVersion      = R128_VERSION_PATCH;
    pDRIInfo->frameBufferPhysicalAddress = info->LinearAddr;
    pDRIInfo->frameBufferSize            = info->FbMapSize;
    pDRIInfo->frameBufferStride          = (pScrn->displayWidth *
					    info->CurrentLayout.pixel_bytes);
    pDRIInfo->ddxDrawableTableEntry      = R128_MAX_DRAWABLES;
    pDRIInfo->maxDrawableTableEntry      = (SAREA_MAX_DRAWABLES
					    < R128_MAX_DRAWABLES
					    ? SAREA_MAX_DRAWABLES
					    : R128_MAX_DRAWABLES);

#ifdef NOT_DONE
    /* FIXME: Need to extend DRI protocol to pass this size back to
     * client for SAREA mapping that includes a device private record
     */
    pDRIInfo->SAREASize =
	((sizeof(XF86DRISAREARec) + 0xfff) & 0x1000); /* round to page */
    /* + shared memory device private rec */
#else
    /* For now the mapping works by using a fixed size defined
     * in the SAREA header
     */
    if (sizeof(XF86DRISAREARec)+sizeof(R128SAREAPriv)>SAREA_MAX) {
        xf86DrvMsg(pScreen->myNum, X_ERROR,
                   "[dri] Data does not fit in SAREA.  Disabling DRI.\n");
	return FALSE;
    }
    pDRIInfo->SAREASize = SAREA_MAX;
#endif

    if (!(pR128DRI = (R128DRIPtr)xcalloc(sizeof(R128DRIRec),1))) {
	DRIDestroyInfoRec(info->pDRIInfo);
	info->pDRIInfo = NULL;
	return FALSE;
    }
    pDRIInfo->devPrivate     = pR128DRI;
    pDRIInfo->devPrivateSize = sizeof(R128DRIRec);
    pDRIInfo->contextSize    = sizeof(R128DRIContextRec);

    pDRIInfo->CreateContext  = R128CreateContext;
    pDRIInfo->DestroyContext = R128DestroyContext;
    pDRIInfo->SwapContext    = R128DRISwapContext;
    pDRIInfo->InitBuffers    = R128DRIInitBuffers;
    pDRIInfo->MoveBuffers    = R128DRIMoveBuffers;
    pDRIInfo->bufferRequests = DRI_ALL_WINDOWS;

    pDRIInfo->createDummyCtx     = TRUE;
    pDRIInfo->createDummyCtxPriv = FALSE;

    if (!DRIScreenInit(pScreen, pDRIInfo, &info->drmFD)) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
                   "[dri] DRIScreenInit failed.  Disabling DRI.\n");
	xfree(pDRIInfo->devPrivate);
	pDRIInfo->devPrivate = NULL;
	DRIDestroyInfoRec(pDRIInfo);
	pDRIInfo = NULL;
	return FALSE;
    }

    /* Check the DRM lib version.
       drmGetLibVersion was not supported in version 1.0, so check for
       symbol first to avoid possible crash or hang.
     */
    if (xf86LoaderCheckSymbol("drmGetLibVersion")) {
        version = drmGetLibVersion(info->drmFD);
    }
    else {
        /* drmlib version 1.0.0 didn't have the drmGetLibVersion
           entry point.  Fake it by allocating a version record
           via drmGetVersion and changing it to version 1.0.0
         */
        version = drmGetVersion(info->drmFD);
        version->version_major      = 1;
        version->version_minor      = 0;
        version->version_patchlevel = 0;
    }

    if (version) {
	if (version->version_major != 1 ||
	    version->version_minor < 1) {
            /* incompatible drm library version */
            xf86DrvMsg(pScreen->myNum, X_ERROR,
		"[dri] R128DRIScreenInit failed because of a version mismatch.\n"
		"[dri] libdrm.a module version is %d.%d.%d but version 1.1.x is needed.\n"
		"[dri] Disabling DRI.\n",
                version->version_major,
                version->version_minor,
                version->version_patchlevel);
            drmFreeVersion(version);
	    R128DRICloseScreen(pScreen);
            return FALSE;
	}
	drmFreeVersion(version);
    }

    /* Check the r128 DRM version */
    version = drmGetVersion(info->drmFD);
    if (version) {
	if (version->version_major != 2 ||
	    version->version_minor < 2) {
	    /* incompatible drm version */
	    xf86DrvMsg(pScreen->myNum, X_ERROR,
		"[dri] R128DRIScreenInit failed because of a version mismatch.\n"
		"[dri] r128.o kernel module version is %d.%d.%d but version 2.2 or greater is needed.\n"
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

				/* Initialize AGP */
    if (!info->IsPCI && !R128DRIAgpInit(info, pScreen)) {
	info->IsPCI = TRUE;
	xf86DrvMsg(pScreen->myNum, X_WARNING,
		   "[agp] AGP failed to initialize -- falling back to PCI mode.\n");
	xf86DrvMsg(pScreen->myNum, X_WARNING,
		   "[agp] Make sure you have the agpgart kernel module loaded.\n");
    }

				/* Initialize PCIGART */
    if (info->IsPCI && !R128DRIPciInit(info, pScreen)) {
	R128DRICloseScreen(pScreen);
	return FALSE;
    }

				/* DRIScreenInit doesn't add all the
				   common mappings.  Add additional
				   mappings here. */
    if (!R128DRIMapInit(info, pScreen)) {
	R128DRICloseScreen(pScreen);
	return FALSE;
    }

				/* DRIScreenInit adds the frame buffer
				   map, but we need it as well */
    {
	void *scratch_ptr;
        int scratch_int;
	
	DRIGetDeviceInfo(pScreen, &info->fbHandle,
                         &scratch_int, &scratch_int, 
                         &scratch_int, &scratch_int,
                         &scratch_ptr);
    }

				/* FIXME: When are these mappings unmapped? */

    if (!R128InitVisualConfigs(pScreen)) {
	R128DRICloseScreen(pScreen);
	return FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[dri] Visual configs initialized\n");

    return TRUE;
}

/* Finish initializing the device-dependent DRI state, and call
   DRIFinishScreenInit() to complete the device-independent DRI
   initialization. */
Bool R128DRIFinishScreenInit(ScreenPtr pScreen)
{
    ScrnInfoPtr      pScrn = xf86Screens[pScreen->myNum];
    R128InfoPtr      info  = R128PTR(pScrn);
    R128SAREAPrivPtr pSAREAPriv;
    R128DRIPtr       pR128DRI;

    info->pDRIInfo->driverSwapMethod = DRI_HIDE_X_CONTEXT;
    /* info->pDRIInfo->driverSwapMethod = DRI_SERVER_SWAP; */

    /* NOTE: DRIFinishScreenInit must be called before *DRIKernelInit
       because *DRIKernelInit requires that the hardware lock is held by
       the X server, and the first time the hardware lock is grabbed is
       in DRIFinishScreenInit. */
    if (!DRIFinishScreenInit(pScreen)) {
	R128DRICloseScreen(pScreen);
	return FALSE;
    }

    /* Initialize the kernel data structures */
    if (!R128DRIKernelInit(info, pScreen)) {
	R128DRICloseScreen(pScreen);
	return FALSE;
    }

    /* Initialize the vertex buffers list */
    if (!R128DRIBufInit(info, pScreen)) {
	R128DRICloseScreen(pScreen);
	return FALSE;
    }

    /* Initialize IRQ */
    R128DRIIrqInit(info, pScreen);

    /* Initialize and start the CCE if required */
    R128DRICCEInit(pScrn);

    pSAREAPriv = (R128SAREAPrivPtr)DRIGetSAREAPrivate(pScreen);
    memset(pSAREAPriv, 0, sizeof(*pSAREAPriv));

    pR128DRI                    = (R128DRIPtr)info->pDRIInfo->devPrivate;

    pR128DRI->deviceID          = info->Chipset;
    pR128DRI->width             = pScrn->virtualX;
    pR128DRI->height            = pScrn->virtualY;
    pR128DRI->depth             = pScrn->depth;
    pR128DRI->bpp               = pScrn->bitsPerPixel;

    pR128DRI->IsPCI             = info->IsPCI;
    pR128DRI->AGPMode           = info->agpMode;

    pR128DRI->frontOffset       = info->frontOffset;
    pR128DRI->frontPitch        = info->frontPitch;
    pR128DRI->backOffset        = info->backOffset;
    pR128DRI->backPitch         = info->backPitch;
    pR128DRI->depthOffset       = info->depthOffset;
    pR128DRI->depthPitch        = info->depthPitch;
    pR128DRI->spanOffset        = info->spanOffset;
    pR128DRI->textureOffset     = info->textureOffset;
    pR128DRI->textureSize       = info->textureSize;
    pR128DRI->log2TexGran       = info->log2TexGran;

    pR128DRI->registerHandle    = info->registerHandle;
    pR128DRI->registerSize      = info->registerSize;

    pR128DRI->agpTexHandle      = info->agpTexHandle;
    pR128DRI->agpTexMapSize     = info->agpTexMapSize;
    pR128DRI->log2AGPTexGran    = info->log2AGPTexGran;
    pR128DRI->agpTexOffset      = info->agpTexStart;
    pR128DRI->sarea_priv_offset = sizeof(XF86DRISAREARec);

    return TRUE;
}

/* The screen is being closed, so clean up any state and free any
   resources used by the DRI. */
void R128DRICloseScreen(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    R128InfoPtr info = R128PTR(pScrn);
    drmR128Init drmInfo;

				/* Stop the CCE if it is still in use */
    if (info->directRenderingEnabled) {
	R128CCE_STOP(pScrn, info);
    }

    if (info->irq) {
	drmCtlUninstHandler(info->drmFD);
	info->irq = 0;
    }

				/* De-allocate vertex buffers */
    if (info->buffers) {
	drmUnmapBufs(info->buffers);
	info->buffers = NULL;
    }

				/* De-allocate all kernel resources */
    memset(&drmInfo, 0, sizeof(drmR128Init));
    drmInfo.func = DRM_R128_CLEANUP_CCE;
    drmCommandWrite(info->drmFD, DRM_R128_INIT,
                    &drmInfo, sizeof(drmR128Init));

				/* De-allocate all AGP resources */
    if (info->agpTex) {
	drmUnmap(info->agpTex, info->agpTexMapSize);
	info->agpTex = NULL;
    }
    if (info->buf) {
	drmUnmap(info->buf, info->bufMapSize);
	info->buf = NULL;
    }
    if (info->ringReadPtr) {
	drmUnmap(info->ringReadPtr, info->ringReadMapSize);
	info->ringReadPtr = NULL;
    }
    if (info->ring) {
	drmUnmap(info->ring, info->ringMapSize);
	info->ring = NULL;
    }
    if (info->agpMemHandle) {
	drmAgpUnbind(info->drmFD, info->agpMemHandle);
	drmAgpFree(info->drmFD, info->agpMemHandle);
	info->agpMemHandle = 0;
	drmAgpRelease(info->drmFD);
    }
    if (info->pciMemHandle) {
	drmScatterGatherFree(info->drmFD, info->pciMemHandle);
	info->pciMemHandle = 0;
    }

				/* De-allocate all DRI resources */
    DRICloseScreen(pScreen);

				/* De-allocate all DRI data structures */
    if (info->pDRIInfo) {
	if (info->pDRIInfo->devPrivate) {
	    xfree(info->pDRIInfo->devPrivate);
	    info->pDRIInfo->devPrivate = NULL;
	}
	DRIDestroyInfoRec(info->pDRIInfo);
	info->pDRIInfo = NULL;
    }
    if (info->pVisualConfigs) {
	xfree(info->pVisualConfigs);
	info->pVisualConfigs = NULL;
    }
    if (info->pVisualConfigsPriv) {
	xfree(info->pVisualConfigsPriv);
	info->pVisualConfigsPriv = NULL;
    }
}
