/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/radeon_dri.c,v 1.32 2003/02/19 09:17:30 alanh Exp $ */
/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario,
 *                VA Linux Systems Inc., Fremont, California.
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
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Kevin E. Martin <martin@xfree86.org>
 *   Rickard E. Faith <faith@valinux.com>
 *   Gareth Hughes <gareth@valinux.com>
 *
 */


				/* Driver data structures */
#include "radeon.h"
#include "radeon_macros.h"
#include "radeon_dri.h"
#include "radeon_reg.h"
#include "radeon_version.h"

				/* X and server generic header files */
#include "xf86.h"
#include "windowstr.h"
#include "xf86PciInfo.h"


#include "shadowfb.h"
				/* GLX/DRI/DRM definitions */
#define _XF86DRI_SERVER_
#include "GL/glxtokens.h"
#include "sarea.h"
#include "radeon_sarea.h"

/* HACK - for now, put this here... */
/* Alpha - this may need to be a variable to handle UP1x00 vs TITAN */
#if defined(__alpha__)
# define DRM_PAGE_SIZE 8192
#elif defined(__ia64__)
# define DRM_PAGE_SIZE getpagesize()
#else
# define DRM_PAGE_SIZE 4096
#endif


static Bool RADEONDRICloseFullScreen(ScreenPtr pScreen);
static Bool RADEONDRIOpenFullScreen(ScreenPtr pScreen);
static void RADEONDRITransitionTo2d(ScreenPtr pScreen);
static void RADEONDRITransitionTo3d(ScreenPtr pScreen);
static void RADEONDRITransitionMultiToSingle3d(ScreenPtr pScreen);
static void RADEONDRITransitionSingleToMulti3d(ScreenPtr pScreen);

static void RADEONDRIRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);

/* Initialize the visual configs that are supported by the hardware.
 * These are combined with the visual configs that the indirect
 * rendering core supports, and the intersection is exported to the
 * client.
 */
static Bool RADEONInitVisualConfigs(ScreenPtr pScreen)
{
    ScrnInfoPtr          pScrn             = xf86Screens[pScreen->myNum];
    RADEONInfoPtr        info              = RADEONPTR(pScrn);
    int                  numConfigs        = 0;
    __GLXvisualConfig   *pConfigs          = 0;
    RADEONConfigPrivPtr  pRADEONConfigs    = 0;
    RADEONConfigPrivPtr *pRADEONConfigPtrs = 0;
    int                  i, accum, stencil, db, use_db;

    use_db = !info->noBackBuffer ? 1 : 0;

    switch (info->CurrentLayout.pixel_code) {
    case 8:  /* 8bpp mode is not support */
    case 15: /* FIXME */
    case 24: /* FIXME */
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[dri] RADEONInitVisualConfigs failed "
		   "(depth %d not supported).  "
		   "Disabling DRI.\n", info->CurrentLayout.pixel_code);
	return FALSE;

#define RADEON_USE_ACCUM   1
#define RADEON_USE_STENCIL 1

    case 16:
	numConfigs = 1;
	if (RADEON_USE_ACCUM)   numConfigs *= 2;
	if (RADEON_USE_STENCIL) numConfigs *= 2;
	if (use_db)             numConfigs *= 2;

	if (!(pConfigs
	      = (__GLXvisualConfig *)xcalloc(sizeof(__GLXvisualConfig),
					     numConfigs))) {
	    return FALSE;
	}
	if (!(pRADEONConfigs
	      = (RADEONConfigPrivPtr)xcalloc(sizeof(RADEONConfigPrivRec),
					     numConfigs))) {
	    xfree(pConfigs);
	    return FALSE;
	}
	if (!(pRADEONConfigPtrs
	      = (RADEONConfigPrivPtr *)xcalloc(sizeof(RADEONConfigPrivPtr),
					       numConfigs))) {
	    xfree(pConfigs);
	    xfree(pRADEONConfigs);
	    return FALSE;
	}

	i = 0;
	for (db = 0; db <= use_db; db++) {
	  for (accum = 0; accum <= RADEON_USE_ACCUM; accum++) {
	    for (stencil = 0; stencil <= RADEON_USE_STENCIL; stencil++) {
		pRADEONConfigPtrs[i] = &pRADEONConfigs[i];

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
		    pConfigs[i].doubleBuffer   = TRUE;
		else
		    pConfigs[i].doubleBuffer   = FALSE;
		pConfigs[i].stereo             = FALSE;
		pConfigs[i].bufferSize         = 16;
		pConfigs[i].depthSize          = 16;
		if (stencil)
		    pConfigs[i].stencilSize    = 8;
		else
		    pConfigs[i].stencilSize    = 0;
		pConfigs[i].auxBuffers         = 0;
		pConfigs[i].level              = 0;
		if (accum) {
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
	if (RADEON_USE_ACCUM)   numConfigs *= 2;
	if (RADEON_USE_STENCIL) numConfigs *= 2;
	if (use_db)             numConfigs *= 2;

	if (!(pConfigs
	      = (__GLXvisualConfig *)xcalloc(sizeof(__GLXvisualConfig),
					     numConfigs))) {
	    return FALSE;
	}
	if (!(pRADEONConfigs
	      = (RADEONConfigPrivPtr)xcalloc(sizeof(RADEONConfigPrivRec),
					     numConfigs))) {
	    xfree(pConfigs);
	    return FALSE;
	}
	if (!(pRADEONConfigPtrs
	      = (RADEONConfigPrivPtr *)xcalloc(sizeof(RADEONConfigPrivPtr),
					       numConfigs))) {
	    xfree(pConfigs);
	    xfree(pRADEONConfigs);
	    return FALSE;
	}

	i = 0;
	for (db = 0; db <= use_db; db++) {
	  for (accum = 0; accum <= RADEON_USE_ACCUM; accum++) {
	    for (stencil = 0; stencil <= RADEON_USE_STENCIL; stencil++) {
		pRADEONConfigPtrs[i] = &pRADEONConfigs[i];

		pConfigs[i].vid                = (VisualID)(-1);
		pConfigs[i].class              = -1;
		pConfigs[i].rgba               = TRUE;
		pConfigs[i].redSize            = 8;
		pConfigs[i].greenSize          = 8;
		pConfigs[i].blueSize           = 8;
		pConfigs[i].alphaSize          = 8;
		pConfigs[i].redMask            = 0x00FF0000;
		pConfigs[i].greenMask          = 0x0000FF00;
		pConfigs[i].blueMask           = 0x000000FF;
		pConfigs[i].alphaMask          = 0xFF000000;
		if (accum) { /* Simulated in software */
		    pConfigs[i].accumRedSize   = 16;
		    pConfigs[i].accumGreenSize = 16;
		    pConfigs[i].accumBlueSize  = 16;
		    pConfigs[i].accumAlphaSize = 16;
		} else {
		    pConfigs[i].accumRedSize   = 0;
		    pConfigs[i].accumGreenSize = 0;
		    pConfigs[i].accumBlueSize  = 0;
		    pConfigs[i].accumAlphaSize = 0;
		}
		if (db)
		    pConfigs[i].doubleBuffer   = TRUE;
		else
		    pConfigs[i].doubleBuffer   = FALSE;
		pConfigs[i].stereo             = FALSE;
		pConfigs[i].bufferSize         = 32;
		if (stencil) {
		    pConfigs[i].depthSize      = 24;
		    pConfigs[i].stencilSize    = 8;
		} else {
		    pConfigs[i].depthSize      = 24;
		    pConfigs[i].stencilSize    = 0;
		}
		pConfigs[i].auxBuffers         = 0;
		pConfigs[i].level              = 0;
		if (accum) {
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
    info->pVisualConfigsPriv = pRADEONConfigs;
    GlxSetVisualConfigs(numConfigs, pConfigs, (void**)pRADEONConfigPtrs);
    return TRUE;
}

/* Create the Radeon-specific context information */
static Bool RADEONCreateContext(ScreenPtr pScreen, VisualPtr visual,
				drmContext hwContext, void *pVisualConfigPriv,
				DRIContextType contextStore)
{
#ifdef PER_CONTEXT_SAREA
    ScrnInfoPtr          pScrn = xf86Screens[pScreen->myNum];
    RADEONInfoPtr        info  = RADEONPTR(pScrn);
    RADEONDRIContextPtr  ctx_info;

    ctx_info = (RADEONDRIContextPtr)contextStore;
    if (!ctx_info) return FALSE;

    if (drmAddMap(info->drmFD, 0,
		  info->perctx_sarea_size,
		  DRM_SHM,
		  DRM_REMOVABLE,
		  &ctx_info->sarea_handle) < 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "[dri] could not create private sarea for ctx id (%d)\n",
		   (int)hwContext);
	return FALSE;
    }

    if (drmAddContextPrivateMapping(info->drmFD, hwContext,
				    ctx_info->sarea_handle) < 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "[dri] could not associate private sarea to ctx id (%d)\n",
		   (int)hwContext);
	drmRmMap(info->drmFD, ctx_info->sarea_handle);
	return FALSE;
    }

    ctx_info->ctx_id = hwContext;
#endif
    return TRUE;
}

/* Destroy the Radeon-specific context information */
static void RADEONDestroyContext(ScreenPtr pScreen, drmContext hwContext,
				 DRIContextType contextStore)
{
#ifdef PER_CONTEXT_SAREA
    ScrnInfoPtr          pScrn = xf86Screens[pScreen->myNum];
    RADEONInfoPtr        info = RADEONPTR(pScrn);
    RADEONDRIContextPtr  ctx_info;

    ctx_info = (RADEONDRIContextPtr)contextStore;
    if (!ctx_info) return;

    if (drmRmMap(info->drmFD, ctx_info->sarea_handle) < 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "[dri] could not remove private sarea for ctx id (%d)\n",
		   (int)hwContext);
    }
#endif
}

/* Called when the X server is woken up to allow the last client's
 * context to be saved and the X server's context to be loaded.  This is
 * not necessary for the Radeon since the client detects when it's
 * context is not currently loaded and then load's it itself.  Since the
 * registers to start and stop the CP are privileged, only the X server
 * can start/stop the engine.
 */
static void RADEONEnterServer(ScreenPtr pScreen)
{
    ScrnInfoPtr    pScrn = xf86Screens[pScreen->myNum];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);

    if (info->accel) info->accel->NeedToSync = TRUE;
}

/* Called when the X server goes to sleep to allow the X server's
 * context to be saved and the last client's context to be loaded.  This
 * is not necessary for the Radeon since the client detects when it's
 * context is not currently loaded and then load's it itself.  Since the
 * registers to start and stop the CP are privileged, only the X server
 * can start/stop the engine.
 */
static void RADEONLeaveServer(ScreenPtr pScreen)
{
    ScrnInfoPtr    pScrn = xf86Screens[pScreen->myNum];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);
    RING_LOCALS;

    /* The CP is always running, but if we've generated any CP commands
     * we must flush them to the kernel module now.
     */
    if (info->CPInUse) {
	RADEON_FLUSH_CACHE();
	RADEON_WAIT_UNTIL_IDLE();
	RADEONCPReleaseIndirect(pScrn);

	info->CPInUse = FALSE;
    }
}

/* Contexts can be swapped by the X server if necessary.  This callback
 * is currently only used to perform any functions necessary when
 * entering or leaving the X server, and in the future might not be
 * necessary.
 */
static void RADEONDRISwapContext(ScreenPtr pScreen, DRISyncType syncType,
				 DRIContextType oldContextType,
				 void *oldContext,
				 DRIContextType newContextType,
				 void *newContext)
{
    if ((syncType==DRI_3D_SYNC) && (oldContextType==DRI_2D_CONTEXT) &&
	(newContextType==DRI_2D_CONTEXT)) { /* Entering from Wakeup */
	RADEONEnterServer(pScreen);
    }

    if ((syncType==DRI_2D_SYNC) && (oldContextType==DRI_NO_CONTEXT) &&
	(newContextType==DRI_2D_CONTEXT)) { /* Exiting from Block Handler */
	RADEONLeaveServer(pScreen);
    }
}

/* The Radeon has depth tiling on all the time, so we have to convert
 * the x,y coordinates into the memory bus address (mba) in the same
 * manner as the engine.  In each case, the linear block address (ba)
 * is calculated, and then wired with x and y to produce the final
 * memory address.
 */
static CARD32 radeon_mba_z16(RADEONInfoPtr info, int x, int y)
{
    CARD32  pitch   = info->frontPitch;
    CARD32  address = 0;			/* a[0]    = 0           */
    CARD32  ba;

    ba = (y / 16) * (pitch / 32) + (x / 32);

    address |= (x & 0x7) << 1;			/* a[1..3] = x[0..2]     */
    address |= (y & 0x7) << 4;			/* a[4..6] = y[0..2]     */
    address |= (x & 0x8) << 4;			/* a[7]    = x[3]        */
    address |= (ba & 0x3) << 8;			/* a[8..9] = ba[0..1]    */
    address |= (y & 0x8) << 7;			/* a[10]   = y[3]        */
    address |= ((x & 0x10) ^ (y & 0x10)) << 7;	/* a[11]   = x[4] ^ y[4] */
    address |= (ba & ~0x3u) << 10;		/* a[12..] = ba[2..]     */

    return address;
}

static CARD32 radeon_mba_z32(RADEONInfoPtr info, int x, int y)
{
    CARD32  pitch   = info->frontPitch;
    CARD32  address = 0;			/* a[0..1] = 0           */
    CARD32  ba;

    ba = (y / 16) * (pitch / 16) + (x / 16);

    address |= (x & 0x7) << 2;			/* a[2..4] = x[0..2]     */
    address |= (y & 0x3) << 5;			/* a[5..6] = y[0..1]     */
    address |=
	(((x & 0x10) >> 2) ^ (y & 0x4)) << 5;	/* a[7]    = x[4] ^ y[2] */
    address |= (ba & 0x3) << 8;			/* a[8..9] = ba[0..1]    */

    address |= (y & 0x8) << 7;			/* a[10]   = y[3]        */
    address |=
	(((x & 0x8) << 1) ^ (y & 0x10)) << 7;	/* a[11]   = x[3] ^ y[4] */
    address |= (ba & ~0x3u) << 10;		/* a[12..] = ba[2..]     */

    return address;
}

/* 16-bit depth buffer functions */
#define WRITE_DEPTH16(_x, _y, d)					\
    *(CARD16 *)(pointer)(buf + radeon_mba_z16(info, (_x), (_y))) = (d)

#define READ_DEPTH16(d, _x, _y)						\
    (d) = *(CARD16 *)(pointer)(buf + radeon_mba_z16(info, (_x), (_y)))

/* 24 bit depth, 8 bit stencil depthbuffer functions */
#define WRITE_DEPTH32(_x, _y, d)					\
do {									\
    CARD32 tmp =							\
	*(CARD32 *)(pointer)(buf + radeon_mba_z32(info, (_x), (_y)));	\
    tmp &= 0xff000000;							\
    tmp |= ((d) & 0x00ffffff);						\
    *(CARD32 *)(pointer)(buf + radeon_mba_z32(info, (_x), (_y))) = tmp;	\
} while (0)

#define READ_DEPTH32(d, _x, _y)						\
    d = (*(CARD32 *)(pointer)(buf + radeon_mba_z32(info, (_x), (_y)))	\
	 & 0x00ffffff)

/* Screen to screen copy of data in the depth buffer */
static void RADEONScreenToScreenCopyDepth(ScrnInfoPtr pScrn,
					  int xa, int ya,
					  int xb, int yb,
					  int w, int h)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    unsigned char *buf  = info->FB + info->depthOffset;
    int            xstart, xend, xdir;
    int            ystart, yend, ydir;
    int            x, y, d;

    if (xa < xb) xdir = -1, xstart = w-1, xend = 0;
    else         xdir =  1, xstart = 0,   xend = w-1;

    if (ya < yb) ydir = -1, ystart = h-1, yend = 0;
    else         ydir =  1, ystart = 0,   yend = h-1;

    switch (pScrn->bitsPerPixel) {
    case 16:
	for (x = xstart; x != xend; x += xdir) {
	    for (y = ystart; y != yend; y += ydir) {
		READ_DEPTH16(d, xa+x, ya+y);
		WRITE_DEPTH16(xb+x, yb+y, d);
	    }
	}
	break;

    case 32:
	for (x = xstart; x != xend; x += xdir) {
	    for (y = ystart; y != yend; y += ydir) {
		READ_DEPTH32(d, xa+x, ya+y);
		WRITE_DEPTH32(xb+x, yb+y, d);
	    }
	}
	break;

    default:
	break;
    }
}

/* Initialize the state of the back and depth buffers */
static void RADEONDRIInitBuffers(WindowPtr pWin, RegionPtr prgn, CARD32 indx)
{
   /* NOOP.  There's no need for the 2d driver to be clearing buffers
    * for the 3d client.  It knows how to do that on its own. 
    */
}

/* Copy the back and depth buffers when the X server moves a window.
 *
 * This routine is a modified form of XAADoBitBlt with the calls to
 * ScreenToScreenBitBlt built in. My routine has the prgnSrc as source
 * instead of destination. My origin is upside down so the ydir cases
 * are reversed.
 */
static void RADEONDRIMoveBuffers(WindowPtr pParent, DDXPointRec ptOldOrg,
				 RegionPtr prgnSrc, CARD32 indx)
{
    ScreenPtr      pScreen  = pParent->drawable.pScreen;
    ScrnInfoPtr    pScrn    = xf86Screens[pScreen->myNum];
    RADEONInfoPtr  info     = RADEONPTR(pScrn);

    BoxPtr         pboxTmp, pboxNext, pboxBase;
    DDXPointPtr    pptTmp;
    int            xdir, ydir;

    int            screenwidth = pScrn->virtualX;
    int            screenheight = pScrn->virtualY;

    BoxPtr         pbox     = REGION_RECTS(prgnSrc);
    int            nbox     = REGION_NUM_RECTS(prgnSrc);

    BoxPtr         pboxNew1 = NULL;
    BoxPtr         pboxNew2 = NULL;
    DDXPointPtr    pptNew1  = NULL;
    DDXPointPtr    pptNew2  = NULL;
    DDXPointPtr    pptSrc   = &ptOldOrg;

    int            dx       = pParent->drawable.x - ptOldOrg.x;
    int            dy       = pParent->drawable.y - ptOldOrg.y;

    /* If the copy will overlap in Y, reverse the order */
    if (dy > 0) {
	ydir = -1;

	if (nbox > 1) {
	    /* Keep ordering in each band, reverse order of bands */
	    pboxNew1 = (BoxPtr)ALLOCATE_LOCAL(sizeof(BoxRec)*nbox);
	    if (!pboxNew1) return;

	    pptNew1 = (DDXPointPtr)ALLOCATE_LOCAL(sizeof(DDXPointRec)*nbox);
	    if (!pptNew1) {
		DEALLOCATE_LOCAL(pboxNew1);
		return;
	    }

	    pboxBase = pboxNext = pbox+nbox-1;

	    while (pboxBase >= pbox) {
		while ((pboxNext >= pbox) && (pboxBase->y1 == pboxNext->y1))
		    pboxNext--;

		pboxTmp = pboxNext+1;
		pptTmp  = pptSrc + (pboxTmp - pbox);

		while (pboxTmp <= pboxBase) {
		    *pboxNew1++ = *pboxTmp++;
		    *pptNew1++  = *pptTmp++;
		}

		pboxBase = pboxNext;
	    }

	    pboxNew1 -= nbox;
	    pbox      = pboxNew1;
	    pptNew1  -= nbox;
	    pptSrc    = pptNew1;
	}
    } else {
	/* No changes required */
	ydir = 1;
    }

    /* If the regions will overlap in X, reverse the order */
    if (dx > 0) {
	xdir = -1;

	if (nbox > 1) {
	    /* reverse order of rects in each band */
	    pboxNew2 = (BoxPtr)ALLOCATE_LOCAL(sizeof(BoxRec)*nbox);
	    pptNew2  = (DDXPointPtr)ALLOCATE_LOCAL(sizeof(DDXPointRec)*nbox);

	    if (!pboxNew2 || !pptNew2) {
		DEALLOCATE_LOCAL(pptNew2);
		DEALLOCATE_LOCAL(pboxNew2);
		DEALLOCATE_LOCAL(pptNew1);
		DEALLOCATE_LOCAL(pboxNew1);
		return;
	    }

	    pboxBase = pboxNext = pbox;

	    while (pboxBase < pbox+nbox) {
		while ((pboxNext < pbox+nbox)
		       && (pboxNext->y1 == pboxBase->y1))
		    pboxNext++;

		pboxTmp = pboxNext;
		pptTmp  = pptSrc + (pboxTmp - pbox);

		while (pboxTmp != pboxBase) {
		    *pboxNew2++ = *--pboxTmp;
		    *pptNew2++  = *--pptTmp;
		}

		pboxBase = pboxNext;
	    }

	    pboxNew2 -= nbox;
	    pbox      = pboxNew2;
	    pptNew2  -= nbox;
	    pptSrc    = pptNew2;
	}
    } else {
	/* No changes are needed */
	xdir = 1;
    }

    (*info->accel->SetupForScreenToScreenCopy)(pScrn, xdir, ydir, GXcopy,
					       (CARD32)(-1), -1);

    for (; nbox-- ; pbox++) {
	int  xa    = pbox->x1;
	int  ya    = pbox->y1;
	int  destx = xa + dx;
	int  desty = ya + dy;
	int  w     = pbox->x2 - xa + 1;
	int  h     = pbox->y2 - ya + 1;

	if (destx < 0)                xa -= destx, w += destx, destx = 0;
	if (desty < 0)                ya -= desty, h += desty, desty = 0;
	if (destx + w > screenwidth)  w = screenwidth  - destx;
	if (desty + h > screenheight) h = screenheight - desty;

	if (w <= 0) continue;
	if (h <= 0) continue;

	RADEONSelectBuffer(pScrn, RADEON_BACK);
	(*info->accel->SubsequentScreenToScreenCopy)(pScrn,
						     xa, ya,
						     destx, desty,
						     w, h);

	if (info->depthMoves) {
	    RADEONSelectBuffer(pScrn, RADEON_DEPTH);
	    RADEONScreenToScreenCopyDepth(pScrn,
					  xa, ya,
					  destx, desty,
					  w, h);
	}
    }

    RADEONSelectBuffer(pScrn, RADEON_FRONT);

    DEALLOCATE_LOCAL(pptNew2);
    DEALLOCATE_LOCAL(pboxNew2);
    DEALLOCATE_LOCAL(pptNew1);
    DEALLOCATE_LOCAL(pboxNew1);

    info->accel->NeedToSync = TRUE;
}

/* Initialize the AGP state.  Request memory for use in AGP space, and
 * initialize the Radeon registers to point to that memory.
 */
static Bool RADEONDRIAgpInit(RADEONInfoPtr info, ScreenPtr pScreen)
{
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned long  mode;
    unsigned int   vendor, device;
    unsigned long agpBase;
    int            ret;
    int            s, l;

    if (drmAgpAcquire(info->drmFD) < 0) {
	xf86DrvMsg(pScreen->myNum, X_WARNING, "[agp] AGP not available\n");
	return FALSE;
    }

    /* Workaround for some hardware bugs */
    if (info->ChipFamily < CHIP_FAMILY_R200)
	OUTREG(RADEON_AGP_CNTL, INREG(RADEON_AGP_CNTL) | 0x000e0000);

				/* Modify the mode if the default mode
				 * is not appropriate for this
				 * particular combination of graphics
				 * card and AGP chipset.
				 */

    mode   = drmAgpGetMode(info->drmFD);	/* Default mode */
    vendor = drmAgpVendorId(info->drmFD);
    device = drmAgpDeviceId(info->drmFD);

    mode &= ~RADEON_AGP_MODE_MASK;
    switch (info->agpMode) {
    case 4:          mode |= RADEON_AGP_4X_MODE;
    case 2:          mode |= RADEON_AGP_2X_MODE;
    case 1: default: mode |= RADEON_AGP_1X_MODE;
    }

    if (info->agpFastWrite) mode |= RADEON_AGP_FW_MODE;
    
    if ((vendor == PCI_VENDOR_AMD) &&
	(device == PCI_CHIP_AMD761)) {
	/* The combination of 761 with MOBILITY chips will lockup the
	 * system; however, currently there is no such a product on the
	 * market, so this is not yet a problem.
	 */
	if ((info->ChipFamily == CHIP_FAMILY_M6) ||
	    (info->ChipFamily == CHIP_FAMILY_M7))
	    return FALSE;

	/* Disable fast write for AMD 761 chipset, since they cause
	 * lockups when enabled.
	 */
	mode &= ~0x10; /* FIXME: Magic number */
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

    if (drmAgpBind(info->drmFD,
		   info->agpMemHandle, info->agpOffset) < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR, "[agp] Could not bind\n");
	drmAgpFree(info->drmFD, info->agpMemHandle);
	drmAgpRelease(info->drmFD);
	return FALSE;
    }

				/* Initialize the CP ring buffer data */
    info->ringStart       = info->agpOffset;
    info->ringMapSize     = info->ringSize*1024*1024 + DRM_PAGE_SIZE;
    info->ringSizeLog2QW  = RADEONMinBits(info->ringSize*1024*1024/8)-1;

    info->ringReadOffset  = info->ringStart + info->ringMapSize;
    info->ringReadMapSize = DRM_PAGE_SIZE;

				/* Reserve space for vertex/indirect buffers */
    info->bufStart        = info->ringReadOffset + info->ringReadMapSize;
    info->bufMapSize      = info->bufSize*1024*1024;

				/* Reserve the rest for AGP textures */
    info->agpTexStart     = info->bufStart + info->bufMapSize;
    s = (info->agpSize*1024*1024 - info->agpTexStart);
    l = RADEONMinBits((s-1) / RADEON_NR_TEX_REGIONS);
    if (l < RADEON_LOG_TEX_GRANULARITY) l = RADEON_LOG_TEX_GRANULARITY;
    info->agpTexMapSize   = (s >> l) << l;
    info->log2AGPTexGran  = l;

    if (drmAddMap(info->drmFD, info->ringStart, info->ringMapSize,
		  DRM_AGP, DRM_READ_ONLY, &info->ringHandle) < 0) {
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
		  DRM_AGP, DRM_READ_ONLY, &info->ringReadPtrHandle) < 0) {
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

				/* Initialize Radeon's AGP registers */

    agpBase = drmAgpBase(info->drmFD);
    OUTREG(RADEON_AGP_BASE, agpBase);

    return TRUE;
}

/* Initialize the PCIGART state.  Request memory for use in PCI space,
 * and initialize the Radeon registers to point to that memory.
 */
static Bool RADEONDRIPciInit(RADEONInfoPtr info, ScreenPtr pScreen)
{
    int  ret;
    int  flags;

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
    info->ringSizeLog2QW  = RADEONMinBits(info->ringSize*1024*1024/8)-1;

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

    return TRUE;
}

/* Add a map for the MMIO registers that will be accessed by any
 * DRI-based clients.
 */
static Bool RADEONDRIMapInit(RADEONInfoPtr info, ScreenPtr pScreen)
{
				/* Map registers */
    info->registerSize = RADEON_MMIOSIZE;
    if (drmAddMap(info->drmFD, info->MMIOAddr, info->registerSize,
		  DRM_REGISTERS, DRM_READ_ONLY, &info->registerHandle) < 0) {
	return FALSE;
    }
    xf86DrvMsg(pScreen->myNum, X_INFO,
	       "[drm] register handle = 0x%08lx\n", info->registerHandle);

    return TRUE;
}

/* Initialize the kernel data structures */
static int RADEONDRIKernelInit(RADEONInfoPtr info, ScreenPtr pScreen)
{
    ScrnInfoPtr    pScrn = xf86Screens[pScreen->myNum];
    int            cpp   = info->CurrentLayout.pixel_bytes;
    drmRadeonInit  drmInfo;

    memset(&drmInfo, 0, sizeof(drmRadeonInit));

    if ( (info->ChipFamily == CHIP_FAMILY_R200) ||
		 (info->ChipFamily == CHIP_FAMILY_RV250) ||
		 (info->ChipFamily == CHIP_FAMILY_M9) )
       drmInfo.func             = DRM_RADEON_INIT_R200_CP;
    else
       drmInfo.func             = DRM_RADEON_INIT_CP;

    drmInfo.sarea_priv_offset   = sizeof(XF86DRISAREARec);
    drmInfo.is_pci              = info->IsPCI;
    drmInfo.cp_mode             = info->CPMode;
    drmInfo.agp_size            = info->agpSize*1024*1024;
    drmInfo.ring_size           = info->ringSize*1024*1024;
    drmInfo.usec_timeout        = info->CPusecTimeout;

    drmInfo.fb_bpp              = info->CurrentLayout.pixel_code;
    drmInfo.depth_bpp           = info->CurrentLayout.pixel_code;

    drmInfo.front_offset        = info->frontOffset;
    drmInfo.front_pitch         = info->frontPitch * cpp;
    drmInfo.back_offset         = info->backOffset;
    drmInfo.back_pitch          = info->backPitch * cpp;
    drmInfo.depth_offset        = info->depthOffset;
    drmInfo.depth_pitch         = info->depthPitch * cpp;

    drmInfo.fb_offset           = info->fbHandle;
    drmInfo.mmio_offset         = info->registerHandle;
    drmInfo.ring_offset         = info->ringHandle;
    drmInfo.ring_rptr_offset    = info->ringReadPtrHandle;
    drmInfo.buffers_offset      = info->bufHandle;
    drmInfo.agp_textures_offset = info->agpTexHandle;

    if (drmCommandWrite(info->drmFD, DRM_RADEON_CP_INIT,
			&drmInfo, sizeof(drmRadeonInit)) < 0)
	return FALSE;

    /* DRM_RADEON_CP_INIT does an engine reset, which resets some engine
     * registers back to their default values, so we need to restore
     * those engine register here.
     */
    RADEONEngineRestore(pScrn);

    return TRUE;
}

static void RADEONDRIAgpHeapInit(RADEONInfoPtr info, ScreenPtr pScreen)
{
    drmRadeonMemInitHeap drmHeap;

    /* Start up the simple memory manager for agp space */
    if (info->drmMinor >= 6) {
	drmHeap.region = RADEON_MEM_REGION_AGP;
	drmHeap.start  = 0;
	drmHeap.size   = info->agpTexMapSize;
    
	if (drmCommandWrite(info->drmFD, DRM_RADEON_INIT_HEAP,
			    &drmHeap, sizeof(drmHeap))) {
	    xf86DrvMsg(pScreen->myNum, X_ERROR,
		       "[drm] Failed to initialized agp heap manager\n");
	} else {
	    xf86DrvMsg(pScreen->myNum, X_INFO,
		       "[drm] Initialized kernel agp heap manager, %d\n",
		       info->agpTexMapSize);
	}
    } else {
	xf86DrvMsg(pScreen->myNum, X_INFO,
		   "[drm] Kernel module too old (1.%d) for agp heap manager\n",
		   info->drmMinor);
    }
}

/* Add a map for the vertex buffers that will be accessed by any
 * DRI-based clients.
 */
static Bool RADEONDRIBufInit(RADEONInfoPtr info, ScreenPtr pScreen)
{
				/* Initialize vertex buffers */
    if (info->IsPCI) {
	info->bufNumBufs = drmAddBufs(info->drmFD,
				      info->bufMapSize / RADEON_BUFFER_SIZE,
				      RADEON_BUFFER_SIZE,
				      DRM_SG_BUFFER,
				      info->bufStart);
    } else {
	info->bufNumBufs = drmAddBufs(info->drmFD,
				      info->bufMapSize / RADEON_BUFFER_SIZE,
				      RADEON_BUFFER_SIZE,
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
	       info->bufNumBufs, RADEON_BUFFER_SIZE);

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

static void RADEONDRIIrqInit(RADEONInfoPtr info, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

    if (!info->irq) {
	info->irq = drmGetInterruptFromBusID(
	    info->drmFD,
	    ((pciConfigPtr)info->PciInfo->thisCard)->busnum,
	    ((pciConfigPtr)info->PciInfo->thisCard)->devnum,
	    ((pciConfigPtr)info->PciInfo->thisCard)->funcnum);

	if ((drmCtlInstHandler(info->drmFD, info->irq)) != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "[drm] failure adding irq handler, "
		       "there is a device already using that irq\n"
		       "[drm] falling back to irq-free operation\n");
	    info->irq = 0;
	} else {
	    unsigned char *RADEONMMIO = info->MMIO;
	    info->ModeReg.gen_int_cntl = INREG( RADEON_GEN_INT_CNTL );
	}
    }

    if (info->irq)
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "[drm] dma control initialized, using IRQ %d\n",
		   info->irq);
}


/* Initialize the CP state, and start the CP (if used by the X server) */
static void RADEONDRICPInit(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

				/* Turn on bus mastering */
    info->BusCntl &= ~RADEON_BUS_MASTER_DIS;

				/* Make sure the CP is on for the X server */
    RADEONCP_START(pScrn, info);
    RADEONSelectBuffer(pScrn, RADEON_FRONT);
}


/* Initialize the screen-specific data structures for the DRI and the
 * Radeon.  This is the main entry point to the device-specific
 * initialization code.  It calls device-independent DRI functions to
 * create the DRI data structures and initialize the DRI state.
 */
Bool RADEONDRIScreenInit(ScreenPtr pScreen)
{
    ScrnInfoPtr    pScrn   = xf86Screens[pScreen->myNum];
    RADEONInfoPtr  info    = RADEONPTR(pScrn);
    DRIInfoPtr     pDRIInfo;
    RADEONDRIPtr   pRADEONDRI;
    int            major, minor, patch;
    drmVersionPtr  version;

    /* Check that the GLX, DRI, and DRM modules have been loaded by testing
     * for known symbols in each module.
     */
    if (!xf86LoaderCheckSymbol("GlxSetVisualConfigs")) return FALSE;
    if (!xf86LoaderCheckSymbol("DRIScreenInit"))       return FALSE;
    if (!xf86LoaderCheckSymbol("drmAvailable"))        return FALSE;
    if (!xf86LoaderCheckSymbol("DRIQueryVersion")) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[dri] RADEONDRIScreenInit failed (libdri.a too old)\n");
      return FALSE;
    }

    /* Check the DRI version */
    DRIQueryVersion(&major, &minor, &patch);
    if (major != 4 || minor < 0) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[dri] RADEONDRIScreenInit failed because of a version "
		   "mismatch.\n"
		   "[dri] libDRI version is %d.%d.%d but version 4.0.x is "
		   "needed.\n"
		   "[dri] Disabling DRI.\n",
		   major, minor, patch);
	return FALSE;
    }

    switch (info->CurrentLayout.pixel_code) {
    case 8:
    case 15:
    case 24:
	/* These modes are not supported (yet). */
	xf86DrvMsg(pScreen->myNum, X_ERROR,
		   "[dri] RADEONInitVisualConfigs failed "
		   "(depth %d not supported).  "
		   "Disabling DRI.\n", info->CurrentLayout.pixel_code);
	return FALSE;

	/* Only 16 and 32 color depths are supports currently. */
    case 16:
    case 32:
	break;
    }

    /* Create the DRI data structure, and fill it in before calling the
     * DRIScreenInit().
     */
    if (!(pDRIInfo = DRICreateInfoRec())) return FALSE;

    info->pDRIInfo                       = pDRIInfo;
    pDRIInfo->drmDriverName              = RADEON_DRIVER_NAME;

    if (info->ChipFamily == CHIP_FAMILY_R200)
       pDRIInfo->clientDriverName        = R200_DRIVER_NAME;
	else if ((info->ChipFamily == CHIP_FAMILY_RV250) ||
			 (info->ChipFamily == CHIP_FAMILY_M9))
       pDRIInfo->clientDriverName        = RV250_DRIVER_NAME;
    else 
       pDRIInfo->clientDriverName        = RADEON_DRIVER_NAME;

    pDRIInfo->busIdString                = xalloc(64);
    sprintf(pDRIInfo->busIdString,
	    "PCI:%d:%d:%d",
	    info->PciInfo->bus,
	    info->PciInfo->device,
	    info->PciInfo->func);
    pDRIInfo->ddxDriverMajorVersion      = RADEON_VERSION_MAJOR;
    pDRIInfo->ddxDriverMinorVersion      = RADEON_VERSION_MINOR;
    pDRIInfo->ddxDriverPatchVersion      = RADEON_VERSION_PATCH;
    pDRIInfo->frameBufferPhysicalAddress = info->LinearAddr;
    pDRIInfo->frameBufferSize            = info->FbMapSize;
    pDRIInfo->frameBufferStride          = (pScrn->displayWidth *
					    info->CurrentLayout.pixel_bytes);
    pDRIInfo->ddxDrawableTableEntry      = RADEON_MAX_DRAWABLES;
    pDRIInfo->maxDrawableTableEntry      = (SAREA_MAX_DRAWABLES
					    < RADEON_MAX_DRAWABLES
					    ? SAREA_MAX_DRAWABLES
					    : RADEON_MAX_DRAWABLES);

#ifdef PER_CONTEXT_SAREA
    /* This is only here for testing per-context SAREAs.  When used, the
       magic number below would be properly defined in a header file. */
    info->perctx_sarea_size = 64 * 1024;
#endif

#ifdef NOT_DONE
    /* FIXME: Need to extend DRI protocol to pass this size back to
     * client for SAREA mapping that includes a device private record
     */
    pDRIInfo->SAREASize = ((sizeof(XF86DRISAREARec) + 0xfff)
			   & 0x1000); /* round to page */
    /* + shared memory device private rec */
#else
    /* For now the mapping works by using a fixed size defined
     * in the SAREA header
     */
    if (sizeof(XF86DRISAREARec)+sizeof(RADEONSAREAPriv) > SAREA_MAX) {
	ErrorF("Data does not fit in SAREA\n");
	return FALSE;
    }
    pDRIInfo->SAREASize = SAREA_MAX;
#endif

    if (!(pRADEONDRI = (RADEONDRIPtr)xcalloc(sizeof(RADEONDRIRec),1))) {
	DRIDestroyInfoRec(info->pDRIInfo);
	info->pDRIInfo = NULL;
	return FALSE;
    }
    pDRIInfo->devPrivate     = pRADEONDRI;
    pDRIInfo->devPrivateSize = sizeof(RADEONDRIRec);
    pDRIInfo->contextSize    = sizeof(RADEONDRIContextRec);

    pDRIInfo->CreateContext  = RADEONCreateContext;
    pDRIInfo->DestroyContext = RADEONDestroyContext;
    pDRIInfo->SwapContext    = RADEONDRISwapContext;
    pDRIInfo->InitBuffers    = RADEONDRIInitBuffers;
    pDRIInfo->MoveBuffers    = RADEONDRIMoveBuffers;
    pDRIInfo->bufferRequests = DRI_ALL_WINDOWS;
    pDRIInfo->OpenFullScreen = RADEONDRIOpenFullScreen;
    pDRIInfo->CloseFullScreen = RADEONDRICloseFullScreen;
    pDRIInfo->TransitionTo2d = RADEONDRITransitionTo2d;
    pDRIInfo->TransitionTo3d = RADEONDRITransitionTo3d;
    pDRIInfo->TransitionSingleToMulti3D = RADEONDRITransitionSingleToMulti3d;
    pDRIInfo->TransitionMultiToSingle3D = RADEONDRITransitionMultiToSingle3d;

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
     * drmGetLibVersion was not supported in version 1.0, so check for
     * symbol first to avoid possible crash or hang.
     */
    if (xf86LoaderCheckSymbol("drmGetLibVersion")) {
	version = drmGetLibVersion(info->drmFD);
    } else {
	/* drmlib version 1.0.0 didn't have the drmGetLibVersion
	 * entry point.  Fake it by allocating a version record
	 * via drmGetVersion and changing it to version 1.0.0.
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
		       "[dri] RADEONDRIScreenInit failed because of a "
		       "version mismatch.\n"
		       "[dri] libdrm.a module version is %d.%d.%d but "
		       "version 1.1.x is needed.\n"
		       "[dri] Disabling DRI.\n",
		       version->version_major,
		       version->version_minor,
		       version->version_patchlevel);
	    drmFreeVersion(version);
	    RADEONDRICloseScreen(pScreen);
	    return FALSE;
	}
	drmFreeVersion(version);
    }

    /* Check the radeon DRM version */
    version = drmGetVersion(info->drmFD);
    if (version) {
	int req_minor, req_patch;

   	if ((info->ChipFamily == CHIP_FAMILY_R200) ||
		(info->ChipFamily == CHIP_FAMILY_RV250) ||
		(info->ChipFamily == CHIP_FAMILY_M9)) {
	    req_minor = 5;
	    req_patch = 0;	
	} else {
#if X_BYTE_ORDER == X_LITTLE_ENDIAN
	    req_minor = 1;
	    req_patch = 0;
#else
	    req_minor = 2;
	    req_patch = 1;	     
#endif
	}

	if (version->version_major != 1 ||
	    version->version_minor < req_minor ||
	    (version->version_minor == req_minor && 
	     version->version_patchlevel < req_patch)) {
	    /* Incompatible drm version */
	    xf86DrvMsg(pScreen->myNum, X_ERROR,
		       "[dri] RADEONDRIScreenInit failed because of a version "
		       "mismatch.\n"
		       "[dri] radeon.o kernel module version is %d.%d.%d "
		       "but version 1.%d.%d or newer is needed.\n"
		       "[dri] Disabling DRI.\n",
		       version->version_major,
		       version->version_minor,
		       version->version_patchlevel,
		       req_minor,
		       req_patch);
	    drmFreeVersion(version);
	    RADEONDRICloseScreen(pScreen);
	    return FALSE;
	}

	if (version->version_minor < 3) {
	    xf86DrvMsg(pScreen->myNum, X_WARNING,
		       "[dri] Some DRI features disabled because of version "
		       "mismatch.\n"
		       "[dri] radeon.o kernel module version is %d.%d.%d but "
		       "1.3.1 or later is preferred.\n",
		       version->version_major,
		       version->version_minor,
		       version->version_patchlevel);
	}
	info->drmMinor = version->version_minor;
	drmFreeVersion(version);
    }

				/* Initialize AGP */
    if (!info->IsPCI && !RADEONDRIAgpInit(info, pScreen)) {
#if defined(__alpha__) || defined(__powerpc__)
	info->IsPCI = TRUE;
	xf86DrvMsg(pScreen->myNum, X_WARNING,
		   "[agp] AGP failed to initialize "
		   "-- falling back to PCI mode.\n");
	xf86DrvMsg(pScreen->myNum, X_WARNING,
		   "[agp] If this is an AGP card, you may want to make sure "
		   "the agpgart\nkernel module is loaded before the radeon "
		   "kernel module.\n");
#else
	RADEONDRICloseScreen(pScreen);
	return FALSE;
#endif
    }

				/* Initialize PCI */
    if (info->IsPCI && !RADEONDRIPciInit(info, pScreen)) {
	RADEONDRICloseScreen(pScreen);
	return FALSE;
    }

				/* DRIScreenInit doesn't add all the
				 * common mappings.  Add additional
				 * mappings here.
				 */
    if (!RADEONDRIMapInit(info, pScreen)) {
	RADEONDRICloseScreen(pScreen);
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

    if (!RADEONInitVisualConfigs(pScreen)) {
	RADEONDRICloseScreen(pScreen);
	return FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[dri] Visual configs initialized\n");

    return TRUE;
}

/* Finish initializing the device-dependent DRI state, and call
 * DRIFinishScreenInit() to complete the device-independent DRI
 * initialization.
 */
Bool RADEONDRIFinishScreenInit(ScreenPtr pScreen)
{
    ScrnInfoPtr         pScrn = xf86Screens[pScreen->myNum];
    RADEONInfoPtr       info  = RADEONPTR(pScrn);
    RADEONSAREAPrivPtr  pSAREAPriv;
    RADEONDRIPtr        pRADEONDRI;

    info->pDRIInfo->driverSwapMethod = DRI_HIDE_X_CONTEXT;
    /* info->pDRIInfo->driverSwapMethod = DRI_SERVER_SWAP; */

    /* NOTE: DRIFinishScreenInit must be called before *DRIKernelInit
     * because *DRIKernelInit requires that the hardware lock is held by
     * the X server, and the first time the hardware lock is grabbed is
     * in DRIFinishScreenInit.
     */
    if (!DRIFinishScreenInit(pScreen)) {
	RADEONDRICloseScreen(pScreen);
	return FALSE;
    }

    /* Initialize the kernel data structures */
    if (!RADEONDRIKernelInit(info, pScreen)) {
	RADEONDRICloseScreen(pScreen);
	return FALSE;
    }

    /* Initialize the vertex buffers list */
    if (!RADEONDRIBufInit(info, pScreen)) {
	RADEONDRICloseScreen(pScreen);
	return FALSE;
    }

    /* Initialize IRQ */
    RADEONDRIIrqInit(info, pScreen);

    /* Initialize kernel agp memory manager */
    RADEONDRIAgpHeapInit(info, pScreen);

    /* Initialize and start the CP if required */
    RADEONDRICPInit(pScrn);

    /* Initialize the SAREA private data structure */
    pSAREAPriv = (RADEONSAREAPrivPtr)DRIGetSAREAPrivate(pScreen);
    memset(pSAREAPriv, 0, sizeof(*pSAREAPriv));

    pRADEONDRI                    = (RADEONDRIPtr)info->pDRIInfo->devPrivate;

    pRADEONDRI->deviceID          = info->Chipset;
    pRADEONDRI->width             = pScrn->virtualX;
    pRADEONDRI->height            = pScrn->virtualY;
    pRADEONDRI->depth             = pScrn->depth;
    pRADEONDRI->bpp               = pScrn->bitsPerPixel;

    pRADEONDRI->IsPCI             = info->IsPCI;
    pRADEONDRI->AGPMode           = info->agpMode;

    pRADEONDRI->frontOffset       = info->frontOffset;
    pRADEONDRI->frontPitch        = info->frontPitch;
    pRADEONDRI->backOffset        = info->backOffset;
    pRADEONDRI->backPitch         = info->backPitch;
    pRADEONDRI->depthOffset       = info->depthOffset;
    pRADEONDRI->depthPitch        = info->depthPitch;
    pRADEONDRI->textureOffset     = info->textureOffset;
    pRADEONDRI->textureSize       = info->textureSize;
    pRADEONDRI->log2TexGran       = info->log2TexGran;

    pRADEONDRI->registerHandle    = info->registerHandle;
    pRADEONDRI->registerSize      = info->registerSize;

    pRADEONDRI->statusHandle      = info->ringReadPtrHandle;
    pRADEONDRI->statusSize        = info->ringReadMapSize;

    pRADEONDRI->agpTexHandle      = info->agpTexHandle;
    pRADEONDRI->agpTexMapSize     = info->agpTexMapSize;
    pRADEONDRI->log2AGPTexGran    = info->log2AGPTexGran;
    pRADEONDRI->agpTexOffset      = info->agpTexStart;

    pRADEONDRI->sarea_priv_offset = sizeof(XF86DRISAREARec);

#ifdef PER_CONTEXT_SAREA
    /* Set per-context SAREA size */
    pRADEONDRI->perctx_sarea_size = info->perctx_sarea_size;
#endif

    /* Have shadowfb run only while there is 3d active. */
    if (info->allowPageFlip /* && info->drmMinor >= 3 */) {
	ShadowFBInit( pScreen, RADEONDRIRefreshArea );
    } else {
       info->allowPageFlip = 0;
    }

    return TRUE;
}

/* The screen is being closed, so clean up any state and free any
 * resources used by the DRI.
 */
void RADEONDRICloseScreen(ScreenPtr pScreen)
{
    ScrnInfoPtr    pScrn = xf86Screens[pScreen->myNum];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);
    drmRadeonInit  drmInfo;
    RING_LOCALS;

				/* Stop the CP */
    if (info->directRenderingEnabled) {
	/* If we've generated any CP commands, we must flush them to the
	 * kernel module now.
	 */
	if (info->CPInUse) {
	    RADEON_FLUSH_CACHE();
	    RADEON_WAIT_UNTIL_IDLE();
	    RADEONCPReleaseIndirect(pScrn);

	    info->CPInUse = FALSE;
	}
	RADEONCP_STOP(pScrn, info);
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
    memset(&drmInfo, 0, sizeof(drmRadeonInit));
    drmInfo.func = DRM_RADEON_CLEANUP_CP;
    drmCommandWrite(info->drmFD, DRM_RADEON_CP_INIT,
		    &drmInfo, sizeof(drmRadeonInit));

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



/* Fullscreen hooks.  The DRI fullscreen mode can probably be removed as
 * it adds little or nothing above the mechanism below (and isn't widely
 * used).
 */
static Bool RADEONDRIOpenFullScreen(ScreenPtr pScreen)
{
    return TRUE;
}

static Bool RADEONDRICloseFullScreen(ScreenPtr pScreen)
{
    return TRUE;
}



/* Use callbacks from dri.c to support pageflipping mode for a single
 * 3d context without need for any specific full-screen extension.
 *
 * Also use these callbacks to allocate and free 3d-specific memory on
 * demand.
 */


/* Use the shadowfb module to maintain a list of dirty rectangles.
 * These are blitted to the back buffer to keep both buffers clean
 * during page-flipping when the 3d application isn't fullscreen.
 *
 * Unlike most use of the shadowfb code, both buffers are in video memory.
 *
 * An alternative to this would be to organize for all on-screen drawing
 * operations to be duplicated for the two buffers.  That might be
 * faster, but seems like a lot more work...
 */


static void RADEONDRIRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    RADEONInfoPtr       info       = RADEONPTR(pScrn);
    int                 i;
    RADEONSAREAPrivPtr  pSAREAPriv = DRIGetSAREAPrivate(pScrn->pScreen);

    /* Don't want to do this when no 3d is active and pages are
     * right-way-round
     */
    if (!pSAREAPriv->pfAllowPageFlip && pSAREAPriv->pfCurrentPage == 0)
	return;

    (*info->accel->SetupForScreenToScreenCopy)(pScrn,
					       1, 1, GXcopy,
					       (CARD32)(-1), -1);

    for (i = 0 ; i < num ; i++, pbox++) {
	int xa = max(pbox->x1, 0), xb = min(pbox->x2, pScrn->virtualX-1);
	int ya = max(pbox->y1, 0), yb = min(pbox->y2, pScrn->virtualY-1);

	if (xa <= xb && ya <= yb) {
	    (*info->accel->SubsequentScreenToScreenCopy)(pScrn, xa, ya,
							 xa + info->backX,
							 ya + info->backY,
							 xb - xa + 1,
							 yb - ya + 1);
	}
    }
}

static void RADEONEnablePageFlip(ScreenPtr pScreen)
{
    ScrnInfoPtr         pScrn      = xf86Screens[pScreen->myNum];
    RADEONInfoPtr       info       = RADEONPTR(pScrn);
    RADEONSAREAPrivPtr  pSAREAPriv = DRIGetSAREAPrivate(pScreen);

    if (info->allowPageFlip) {
	/* Duplicate the frontbuffer to the backbuffer */
	(*info->accel->SetupForScreenToScreenCopy)(pScrn,
						   1, 1, GXcopy,
						   (CARD32)(-1), -1);

	(*info->accel->SubsequentScreenToScreenCopy)(pScrn,
						     0,
						     0,
						     info->backX,
						     info->backY,
						     pScrn->virtualX,
						     pScrn->virtualY);

	pSAREAPriv->pfAllowPageFlip = 1;
    }
}

static void RADEONDisablePageFlip(ScreenPtr pScreen)
{
    /* Tell the clients not to pageflip.  How?
     *   -- Field in sarea, plus bumping the window counters.
     *   -- DRM needs to cope with Front-to-Back swapbuffers.
     */
    RADEONSAREAPrivPtr  pSAREAPriv = DRIGetSAREAPrivate(pScreen);

    pSAREAPriv->pfAllowPageFlip = 0;
}

static void RADEONDRITransitionSingleToMulti3d(ScreenPtr pScreen)
{
    RADEONDisablePageFlip(pScreen);
}

static void RADEONDRITransitionMultiToSingle3d(ScreenPtr pScreen)
{
    /* Let the remaining 3d app start page flipping again */
    RADEONEnablePageFlip(pScreen);
}

static void RADEONDRITransitionTo3d(ScreenPtr pScreen)
{
    ScrnInfoPtr    pScrn = xf86Screens[pScreen->myNum];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);
    FBAreaPtr      fbarea;
    int            width, height;

    /* reserve offscreen area for back and depth buffers and textures */

    /* If we still have an area for the back buffer reserved, free it
     * first so we always start with all free offscreen memory, except
     * maybe for Xv
     */
    if (info->backArea) {
	xf86FreeOffscreenArea(info->backArea);
	info->backArea = NULL;
    }

    xf86PurgeUnlockedOffscreenAreas(pScreen);

    xf86QueryLargestOffscreenArea(pScreen, &width, &height, 0, 0, 0);

    /* Free Xv linear offscreen memory if necessary */
    if (height < (info->depthTexLines + info->backLines)) {
	xf86FreeOffscreenLinear(info->videoLinear);
	info->videoLinear = NULL;
	xf86QueryLargestOffscreenArea(pScreen, &width, &height, 0, 0, 0);
    }

    /* Reserve placeholder area so the other areas will match the
     * pre-calculated offsets
     */
    fbarea = xf86AllocateOffscreenArea(pScreen, pScrn->displayWidth,
				       height
				       - info->depthTexLines
				       - info->backLines,
				       pScrn->displayWidth,
				       NULL, NULL, NULL);
    if (!fbarea)
	xf86DrvMsg(pScreen->myNum, X_ERROR, "Unable to reserve placeholder "
		   "offscreen area, you might experience screen corruption\n");

    info->backArea = xf86AllocateOffscreenArea(pScreen, pScrn->displayWidth,
					       info->backLines,
					       pScrn->displayWidth,
					       NULL, NULL, NULL);
    if (!info->backArea)
	xf86DrvMsg(pScreen->myNum, X_ERROR, "Unable to reserve offscreen "
		   "area for back buffer, you might experience screen "
		   "corruption\n");

    info->depthTexArea = xf86AllocateOffscreenArea(pScreen,
						   pScrn->displayWidth,
						   info->depthTexLines,
						   pScrn->displayWidth,
						   NULL, NULL, NULL);
    if (!info->depthTexArea)
	xf86DrvMsg(pScreen->myNum, X_ERROR, "Unable to reserve offscreen "
		   "area for depth buffer and textures, you might "
		   "experience screen corruption\n");

    xf86FreeOffscreenArea(fbarea);

    RADEONEnablePageFlip(pScreen);

    info->have3DWindows = 1;

    if (info->cursor_start)
	xf86ForceHWCursor (pScreen, TRUE);
}

static void RADEONDRITransitionTo2d(ScreenPtr pScreen)
{
    ScrnInfoPtr         pScrn      = xf86Screens[pScreen->myNum];
    RADEONInfoPtr       info       = RADEONPTR(pScrn);
    RADEONSAREAPrivPtr  pSAREAPriv = DRIGetSAREAPrivate(pScreen);

    /* Shut down shadowing if we've made it back to the front page */
    if (pSAREAPriv->pfCurrentPage == 0) {
	RADEONDisablePageFlip(pScreen);
	xf86FreeOffscreenArea(info->backArea);
	info->backArea = NULL;
    } else {
	xf86DrvMsg(pScreen->myNum, X_WARNING,
		   "[dri] RADEONDRITransitionTo2d: "
		   "kernel failed to unflip buffers.\n"); 
    }

    xf86FreeOffscreenArea(info->depthTexArea); 

    info->have3DWindows = 0;

    if (info->cursor_start)
	    xf86ForceHWCursor (pScreen, FALSE);
}
