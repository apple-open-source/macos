/*
 * Acceleration for the Creator and Creator3D framebuffer - Dbe Acceleration.
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
 * DAVID MILLER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_dbe.c,v 1.2 2003/02/11 03:19:02 dawes Exp $ */

#define NEED_REPLIES
#define NEED_EVENTS
#include "X.h"
#include "Xproto.h"
#include "misc.h"
#include "os.h"
#include "windowstr.h"
#include "scrnintstr.h"
#include "pixmapstr.h"
#include "extnsionst.h"
#include "dixstruct.h"
#include "resource.h"
#include "opaque.h"
#include "dbestruct.h"
#include "regionstr.h"
#include "gcstruct.h"
#include "inputstr.h"

#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb32.h"

#include "xf86_ansic.h"
#include "xf86.h"

#include "ffb.h"
#include "ffb_fifo.h"
#include "ffb_rcache.h"

static int	FFBDbePrivPrivGeneration  =  0;
static int	FFBDbeWindowPrivPrivIndex = -1;
static RESTYPE	dbeDrawableResType;
static RESTYPE	dbeWindowPrivResType;
static int	dbeScreenPrivIndex = -1;
static int	dbeWindowPrivIndex = -1;

#define FFB_DBE_WINDOW_PRIV_PRIV(pDbeWindowPriv) \
    (((FFBDbeWindowPrivPrivIndex < 0) || (!pDbeWindowPriv)) ? \
    NULL : \
    ((FFBDbeWindowPrivPrivPtr) \
     ((pDbeWindowPriv)->devPrivates[FFBDbeWindowPrivPrivIndex].ptr)))

#define FFB_DBE_WINDOW_PRIV_PRIV_FROM_WINDOW(pWin) \
    FFB_DBE_WINDOW_PRIV_PRIV(DBE_WINDOW_PRIV(pWin))

typedef struct _FFBDbeWindowPrivPrivRec {
	int HwAccelerated;
	int HwCurrentBuf;	/* 0 = buffer A, 1 = buffer B */

	/* We need also what midbe would use in case we must
	 * revert to unaccelerated dbe.
	 */
	PixmapPtr		pBackBuffer;
	PixmapPtr		pFrontBuffer;

	/* Back pointer to generic DBE layer window private. */
	DbeWindowPrivPtr	pDbeWindowPriv;
} FFBDbeWindowPrivPrivRec, *FFBDbeWindowPrivPrivPtr;

static Bool
FFBDbeGetVisualInfo(ScreenPtr pScreen, XdbeScreenVisualInfo *pScrVisInfo)
{
	XdbeVisualInfo *visInfo;
	DepthPtr pDepth;
	int i, j, k, count;

	/* XXX Should check for double-buffer presence.  But even if
	 * XXX the double-buffer is not present we can still play
	 * XXX tricks with GetWindowPixmap in 8bpp mode, ie. double
	 * XXX buffer between the R and G planes of buffer A. -DaveM
	 */

	/* Determine number of visuals for this screen. */
	for (i = 0, count = 0; i < pScreen->numDepths; i++)
		count += pScreen->allowedDepths[i].numVids;

	/* Allocate an array of XdbeVisualInfo items. */
	if (!(visInfo = (XdbeVisualInfo *)xalloc(count * sizeof(XdbeVisualInfo))))
		return FALSE;

	for (i = 0, k = 0; i < pScreen->numDepths; i++) {
		/* For each depth of this screen, get visual information. */
		pDepth = &pScreen->allowedDepths[i];
		for (j = 0; j < pDepth->numVids; j++) {
			/* For each visual for this depth of this screen, get visual ID
			 * and visual depth.  For now, we will always return
			 * the same performance level for all visuals (0).  A higher
			 * performance level value indicates higher performance.
			 */
			visInfo[k].visual    = pDepth->vids[j];
			visInfo[k].depth     = pDepth->depth;

			/* XXX We should set this appropriately... -DaveM */
			visInfo[k].perflevel = 0;
			k++;
		}
	}

	/* Record the number of visuals and point visual_depth to
	 * the array of visual info.
	 */
	pScrVisInfo->count   = count;
	pScrVisInfo->visinfo = visInfo;

	return TRUE;
}

static void
FFBDbeUpdateWidPlane(WindowPtr pWin, GCPtr pGC)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pWin->drawable.pScreen);
	CreatorPrivWinPtr pFfbPrivWin = CreatorGetWindowPrivate(pWin);
	RegionPtr prgnClip;
	BoxPtr pboxClipped, pboxClippedBase;
	unsigned int fbc;
	int numRects;
	int x, y, w, h;

	x = pWin->drawable.x;
	y = pWin->drawable.y;
	w = pWin->drawable.width;
	h = pWin->drawable.height;

	fbc = pFfbPrivWin->fbc_base;
	fbc = (fbc & ~FFB_FBC_WB_MASK) | FFB_FBC_WB_AB;
	fbc = (fbc & ~FFB_FBC_XE_MASK) | FFB_FBC_XE_ON;
	fbc = (fbc & ~FFB_FBC_RGBE_MASK) | FFB_FBC_RGBE_OFF;

	prgnClip = cfbGetCompositeClip(pGC);
	numRects = REGION_NUM_RECTS (prgnClip);
	pboxClippedBase = (BoxPtr) ALLOCATE_LOCAL(numRects * sizeof(BoxRec));
	if (!pboxClippedBase)
		return;

	pboxClipped = pboxClippedBase;
	{
		int x1, y1, x2, y2, bx2, by2;
		BoxRec box;
		BoxPtr pextent;

		pextent = REGION_EXTENTS(pGC->pScreen, prgnClip);
		x1 = pextent->x1;
		y1 = pextent->y1;
		x2 = pextent->x2;
		y2 = pextent->y2;

		if ((box.x1 = x) < x1)
			box.x1 = x1;

		if ((box.y1 = y) < y1)
			box.y1 = y1;

		bx2 = (int) x + (int) w;
		if (bx2 > x2)
			bx2 = x2;
		box.x2 = bx2;

		by2 = (int) y + (int) h;
		if (by2 > y2)
			by2 = y2;
		box.y2 = by2;

		if ((box.x1 < box.x2) && (box.y1 < box.y2)) {
			int n = REGION_NUM_RECTS (prgnClip);
			BoxPtr pbox = REGION_RECTS(prgnClip);

			/* Clip the rectangle to each box in the clip region
			 * this is logically equivalent to calling Intersect()
			 */
			while(n--) {
				pboxClipped->x1 = max(box.x1, pbox->x1);
				pboxClipped->y1 = max(box.y1, pbox->y1);
				pboxClipped->x2 = min(box.x2, pbox->x2);
				pboxClipped->y2 = min(box.y2, pbox->y2);
				pbox++;

				/* see if clipping left anything */
				if(pboxClipped->x1 < pboxClipped->x2 &&
				   pboxClipped->y1 < pboxClipped->y2)
					pboxClipped++;
			}
		}
	}

	if (pboxClipped != pboxClippedBase) {
		ffb_fbcPtr ffb = pFfb->regs;
		int num = (pboxClipped - pboxClippedBase);

		FFB_ATTR_RAW(pFfb,
			     FFB_PPC_APE_DISABLE | FFB_PPC_CS_CONST | FFB_PPC_XS_WID,
			     FFB_PPC_APE_MASK | FFB_PPC_CS_MASK | FFB_PPC_XS_MASK,
			     pGC->planemask,
			     ((FFB_ROP_EDIT_BIT | pGC->alu) | (FFB_ROP_NEW << 8)),
			     FFB_DRAWOP_RECTANGLE,
			     pGC->fgPixel,
			     fbc, pFfbPrivWin->wid);

		pboxClipped = pboxClippedBase;
		while (num--) {
			int xx, yy, ww, hh;

			xx = pboxClipped->x1;
			yy = pboxClipped->y1;
			ww = (pboxClipped->x2 - xx);
			hh = (pboxClipped->y2 - yy);
			FFBFifo(pFfb, 4);
			FFB_WRITE64(&ffb->by, yy, xx);
			FFB_WRITE64_2(&ffb->bh, hh, ww);
		}
	}

	DEALLOCATE_LOCAL (pboxClippedBase);
}

static int
FFBDbeAllocBackBufferName(WindowPtr pWin, XID bufId, int swapAction)
{
	ScreenPtr pScreen;
	FFBPtr pFfb;
	DbeWindowPrivPtr pDbeWindowPriv;
	FFBDbeWindowPrivPrivPtr pDbeWindowPrivPriv; 
	DbeScreenPrivPtr pDbeScreenPriv;
	GCPtr pGC;
	xRectangle clearRect;

	pScreen = pWin->drawable.pScreen;
	pDbeWindowPriv = DBE_WINDOW_PRIV(pWin);
	pFfb = GET_FFB_FROM_SCREEN(pScreen);

	pDbeWindowPrivPriv = FFB_DBE_WINDOW_PRIV_PRIV(pDbeWindowPriv);
	if (pDbeWindowPriv->nBufferIDs == 0) {
		/* There is no buffer associated with the window.
		 * We have to create the window priv priv.  Remember, the window
		 * priv was created at the DIX level, so all we need to do is
		 * create the priv priv and attach it to the priv.
		 */
		pDbeScreenPriv = DBE_SCREEN_PRIV(pScreen);

		/* Setup the window priv priv. */
		pDbeWindowPrivPriv->pDbeWindowPriv = pDbeWindowPriv;

		if (!pFfb->NoAccel && pFfb->has_double_buffer) {
			CreatorPrivWinPtr pFfbPrivWin;
			unsigned int wid, fbc;

			/* We just render directly into the hardware, all
			 * that is needed is to swap the rendering attributes
			 * and the WID settings during a swap.
			 */
			if (!AddResource(bufId, dbeDrawableResType,
					 (pointer)&pWin->drawable)) {
				/* Free the buffer and the drawable resource. */
				FreeResource(bufId, RT_NONE);
				return BadAlloc;
			}

			pFfbPrivWin = CreatorGetWindowPrivate(pWin);
			wid = FFBWidUnshare(pFfb, pFfbPrivWin->wid);
			if (wid == (unsigned int)-1)
				return BadAlloc;

			pFfbPrivWin->wid = wid;

			/* Attach the priv priv to the priv. */
			pDbeWindowPriv->devPrivates[FFBDbeWindowPrivPrivIndex].ptr =
				(pointer)pDbeWindowPrivPriv;

			/* Indicate we are doing hw acceleration. */
			pDbeWindowPrivPriv->HwAccelerated = 1;

			/* Start rendering into buffer B. */
			pDbeWindowPrivPriv->HwCurrentBuf = 1;

			/* No back/front temporary pixmaps. */
			pDbeWindowPrivPriv->pFrontBuffer = NULL;
			pDbeWindowPrivPriv->pBackBuffer = NULL;

			/* Switch to writing into buffer B. */
			fbc = pFfbPrivWin->fbc_base;
			fbc &= ~(FFB_FBC_WB_MASK | FFB_FBC_RB_MASK);
			fbc |= (FFB_FBC_WB_B | FFB_FBC_RB_B);
			pFfbPrivWin->fbc_base = fbc;

			pGC = GetScratchGC(pWin->drawable.depth, pWin->drawable.pScreen);

			/* Fill X plane of front and back buffers. */
			if ((*pDbeScreenPriv->SetupBackgroundPainter)(pWin, pGC)) {
				ValidateGC(&pWin->drawable, pGC);
				FFBDbeUpdateWidPlane(pWin, pGC);
			}

			/* Clear out buffer B. */
			clearRect.x = clearRect.y = 0;
			clearRect.width  = pWin->drawable.width;
			clearRect.height = pWin->drawable.height;
			if ((*pDbeScreenPriv->SetupBackgroundPainter)(pWin, pGC)) {
				ValidateGC(&pWin->drawable, pGC);
				(*pGC->ops->PolyFillRect)(&pWin->drawable, pGC, 1, &clearRect);
			}

			FreeScratchGC(pGC);
		} else {
			/* Get a front pixmap. */
			if (!(pDbeWindowPrivPriv->pFrontBuffer =
			      (*pScreen->CreatePixmap)(pScreen, pDbeWindowPriv->width,
						       pDbeWindowPriv->height,
						       pWin->drawable.depth)))
				return BadAlloc;

			/* Get a back pixmap. */
			if (!(pDbeWindowPrivPriv->pBackBuffer =
			      (*pScreen->CreatePixmap)(pScreen, pDbeWindowPriv->width,
						       pDbeWindowPriv->height,
						       pWin->drawable.depth))) {
				(*pScreen->DestroyPixmap)(pDbeWindowPrivPriv->pFrontBuffer); 
				return BadAlloc;
			}


			/* Make the back pixmap a DBE drawable resource. */
			if (!AddResource(bufId, dbeDrawableResType,
					 (pointer)pDbeWindowPrivPriv->pBackBuffer)) {
				/* Free the buffer and the drawable resource. */
				FreeResource(bufId, RT_NONE);
				return(BadAlloc);
			}

			/* Attach the priv priv to the priv. */
			pDbeWindowPriv->devPrivates[FFBDbeWindowPrivPrivIndex].ptr =
				(pointer)pDbeWindowPrivPriv;

			/* Indicate we are doing this non-accelerated. */
			pDbeWindowPrivPriv->HwAccelerated = 0;

			/* Clear the back buffer. */
			pGC = GetScratchGC(pWin->drawable.depth, pWin->drawable.pScreen);
			if ((*pDbeScreenPriv->SetupBackgroundPainter)(pWin, pGC)) {
				ValidateGC((DrawablePtr)pDbeWindowPrivPriv->pBackBuffer, pGC);
				clearRect.x = clearRect.y = 0;
				clearRect.width  = pDbeWindowPrivPriv->pBackBuffer->drawable.width;
				clearRect.height = pDbeWindowPrivPriv->pBackBuffer->drawable.height;
				(*pGC->ops->PolyFillRect)(
					(DrawablePtr)pDbeWindowPrivPriv->pBackBuffer, pGC, 1,
					&clearRect);
			}
			FreeScratchGC(pGC);
		}
	} else {
		pointer cookie;

		/* A buffer is already associated with the window.
		 * Place the new buffer ID information at the head of the ID list.
		 */
		if (pDbeWindowPrivPriv->HwAccelerated != 0)
			cookie = (pointer)&pWin->drawable;
		else
			cookie = (pointer)pDbeWindowPrivPriv->pBackBuffer;

		/* Associate the new ID with an existing pixmap. */
		if (!AddResource(bufId, dbeDrawableResType, cookie))
			return BadAlloc;
	}

	return Success;
}

static void
FFBDbeAliasBuffers(DbeWindowPrivPtr pDbeWindowPriv)
{
	FFBDbeWindowPrivPrivPtr	pDbeWindowPrivPriv =
		FFB_DBE_WINDOW_PRIV_PRIV(pDbeWindowPriv);
	pointer cookie;
	int i;

	if (pDbeWindowPrivPriv->HwAccelerated != 0)
		cookie = (pointer) &pDbeWindowPriv->pWindow->drawable;
	else
		cookie = (pointer) pDbeWindowPrivPriv->pBackBuffer;

	for (i = 0; i < pDbeWindowPriv->nBufferIDs; i++)
		ChangeResourceValue(pDbeWindowPriv->IDs[i], dbeDrawableResType, cookie);
}

static int
FFBDbeSwapBuffers(ClientPtr client, int *pNumWindows, DbeSwapInfoPtr swapInfo)
{
	FFBDbeWindowPrivPrivPtr pDbeWindowPrivPriv; 
	DbeScreenPrivPtr pDbeScreenPriv;
	PixmapPtr pTmpBuffer;
	xRectangle clearRect;
	WindowPtr pWin;
	GCPtr pGC;

	pWin               = swapInfo[0].pWindow;
	pDbeScreenPriv     = DBE_SCREEN_PRIV_FROM_WINDOW(pWin);
	pDbeWindowPrivPriv = FFB_DBE_WINDOW_PRIV_PRIV_FROM_WINDOW(pWin);
	pGC = GetScratchGC(pWin->drawable.depth, pWin->drawable.pScreen);

	if (pDbeWindowPrivPriv->HwAccelerated != 0) {
		FFBPtr pFfb = GET_FFB_FROM_SCREEN(pWin->drawable.pScreen);
		CreatorPrivWinPtr pFfbPrivWin = CreatorGetWindowPrivate(pWin);
		unsigned int fbc;
		int visible;

		/* Unfortunately, this is necessary for correctness. */
		FFBWait(pFfb, pFfb->regs);

		/* Flip front/back in the WID. */
		visible = 0;
		if (pWin->viewable &&
		    pWin->visibility != VisibilityFullyObscured)
			visible = 1;
		FFBWidChangeBuffer(pFfb, pFfbPrivWin->wid, visible);

		/* Indicate where we are rendering now. */
		pDbeWindowPrivPriv->HwCurrentBuf ^= 1;

		/* Update framebuffer controls. */
		fbc = pFfbPrivWin->fbc_base;
		fbc &= ~(FFB_FBC_WB_MASK | FFB_FBC_RB_MASK);
		if (pDbeWindowPrivPriv->HwCurrentBuf == 0)
			fbc |= FFB_FBC_WB_A | FFB_FBC_RB_A;
		else
			fbc |= FFB_FBC_WB_B | FFB_FBC_RB_B;

		/* For XdbeUndefined we do not have to do anything.
		 * This is true for XdbeUntouched as well because we
		 * do in fact retain the unobscured contents of the
		 * front buffer while it is being displayed, thus now
		 * when it has become the back buffer it is still holding
		 * those contents.
		 *
		 * The XdbeUntouched case is important because most apps
		 * using dbe use this type of swap.
		 */

		if (swapInfo[0].swapAction == XdbeCopied) {
			unsigned int fbc_front_to_back;

			/* Do a GCOPY, front to back. */
			fbc_front_to_back = fbc & ~FFB_FBC_RB_MASK;
			if (pDbeWindowPrivPriv->HwCurrentBuf == 0)
				fbc_front_to_back |= FFB_FBC_RB_B;
			else
				fbc_front_to_back |= FFB_FBC_RB_A;

			pFfbPrivWin->fbc_base = fbc_front_to_back;
			ValidateGC(&pWin->drawable, pGC);
			(*pGC->ops->CopyArea)(&pWin->drawable,
					      &pWin->drawable,
					      pGC,
					      0, 0,
					      pWin->drawable.width,
					      pWin->drawable.height,
					      0, 0);
		} else if (swapInfo[0].swapAction == XdbeBackground) {
			if ((*pDbeScreenPriv->SetupBackgroundPainter)(pWin, pGC)) {
				ValidateGC(&pWin->drawable, pGC);
				clearRect.x = 0;
				clearRect.y = 0;
				clearRect.width = pWin->drawable.width;
				clearRect.height = pWin->drawable.height;
				(*pGC->ops->PolyFillRect)(&pWin->drawable, pGC,
							  1, &clearRect);
			}
		}

		/* Ok, now render with these fb controls. */
		pFfbPrivWin->fbc_base = fbc;
	} else {
		if (swapInfo[0].swapAction == XdbeUntouched) {
			ValidateGC((DrawablePtr)pDbeWindowPrivPriv->pFrontBuffer, pGC);
			(*pGC->ops->CopyArea)((DrawablePtr)pWin,
					      (DrawablePtr)pDbeWindowPrivPriv->pFrontBuffer,
					      pGC, 0, 0, pWin->drawable.width,
					      pWin->drawable.height, 0, 0);
		}

		ValidateGC((DrawablePtr)pWin, pGC);
		(*pGC->ops->CopyArea)((DrawablePtr)pDbeWindowPrivPriv->pBackBuffer,
				      (DrawablePtr)pWin, pGC, 0, 0,
				      pWin->drawable.width, pWin->drawable.height,
				      0, 0);

		if (swapInfo[0].swapAction == XdbeBackground) {
			if ((*pDbeScreenPriv->SetupBackgroundPainter)(pWin, pGC)) {
				ValidateGC((DrawablePtr)pDbeWindowPrivPriv->pBackBuffer,
					   pGC);
				clearRect.x = 0;
				clearRect.y = 0;
				clearRect.width =
					pDbeWindowPrivPriv->pBackBuffer->drawable.width;
				clearRect.height =
					pDbeWindowPrivPriv->pBackBuffer->drawable.height;
				(*pGC->ops->PolyFillRect)(
					(DrawablePtr)pDbeWindowPrivPriv->pBackBuffer,
					pGC, 1, &clearRect);
			}
		} else if (swapInfo[0].swapAction == XdbeUntouched) {
			/* Swap pixmap pointers. */
			pTmpBuffer = pDbeWindowPrivPriv->pBackBuffer;
			pDbeWindowPrivPriv->pBackBuffer =
				pDbeWindowPrivPriv->pFrontBuffer;
			pDbeWindowPrivPriv->pFrontBuffer = pTmpBuffer;
			FFBDbeAliasBuffers(pDbeWindowPrivPriv->pDbeWindowPriv);
		}
	}

	/* Remove the swapped window from the swap information array and decrement
	 * pNumWindows to indicate to the DIX level how many windows were actually
	 * swapped.
	 */
	if (*pNumWindows > 1) {
		/* We were told to swap more than one window, but we only swapped the
		 * first one.  Remove the first window in the list by moving the last
		 * window to the beginning.
		 */
		swapInfo[0].pWindow    = swapInfo[*pNumWindows - 1].pWindow;
		swapInfo[0].swapAction = swapInfo[*pNumWindows - 1].swapAction;

		/* Clear the last window information just to be safe. */
		swapInfo[*pNumWindows - 1].pWindow    = (WindowPtr)NULL;
		swapInfo[*pNumWindows - 1].swapAction = 0;
	} else {
		/* Clear the window information just to be safe. */
		swapInfo[0].pWindow    = (WindowPtr)NULL;
		swapInfo[0].swapAction = 0;
	}

	(*pNumWindows)--;

	FreeScratchGC(pGC);

	return Success;
}

static void
FFBDbeWinPrivDelete(DbeWindowPrivPtr pDbeWindowPriv, XID bufId)
{
	WindowPtr pWin = pDbeWindowPriv->pWindow;
	FFBDbeWindowPrivPrivPtr pDbeWindowPrivPriv;

	if (pDbeWindowPriv->nBufferIDs != 0) {
		/* We still have at least one more buffer ID associated with this
		 * window.
		 */
		return;
	}

	/* We have no more buffer IDs associated with this window.  We need to
	 * free some stuff.
	 */
	pDbeWindowPrivPriv = FFB_DBE_WINDOW_PRIV_PRIV(pDbeWindowPriv);

	/* If we were accelerating we need to restore the framebuffer
	 * attributes.  We need to also free up the Dbe WID and go
	 * back to using the shared one.
	 */
	if (pDbeWindowPrivPriv->HwAccelerated != 0) {
		FFBPtr pFfb = GET_FFB_FROM_SCREEN(pWin->drawable.pScreen);
		CreatorPrivWinPtr pFfbPrivWin = CreatorGetWindowPrivate(pWin);
		xRectangle clearRect;
		unsigned int fbc;
		GCPtr pGC;

		pFfbPrivWin->wid = FFBWidReshare(pFfb, pFfbPrivWin->wid);

		/* Go back to using buffer A. */
		fbc = pFfbPrivWin->fbc_base;
		fbc &= ~(FFB_FBC_WB_MASK | FFB_FBC_RB_MASK);
		fbc |= FFB_FBC_WB_A | FFB_FBC_RB_A;

		/* Now fixup the WID channel. */
		pFfbPrivWin->fbc_base =
			(fbc & ~FFB_FBC_RGBE_MASK) | FFB_FBC_RGBE_OFF;

		pGC = GetScratchGC(pWin->drawable.depth, pWin->drawable.pScreen);
		clearRect.x = clearRect.y = 0;
		clearRect.width  = pWin->drawable.width;
		clearRect.height = pWin->drawable.height;
		ValidateGC(&pWin->drawable, pGC);
		FFBDbeUpdateWidPlane(pWin, pGC);
		(*pGC->ops->PolyFillRect)(&pWin->drawable, pGC, 1, &clearRect);
		FreeScratchGC(pGC);

		pFfbPrivWin->fbc_base = fbc;
	} else {
		/* Destroy the front and back pixmaps. */
		if (pDbeWindowPrivPriv->pFrontBuffer)
			(*pDbeWindowPriv->pWindow->drawable.pScreen->DestroyPixmap)(
				pDbeWindowPrivPriv->pFrontBuffer);
		if (pDbeWindowPrivPriv->pBackBuffer)
			(*pDbeWindowPriv->pWindow->drawable.pScreen->DestroyPixmap)(
				pDbeWindowPrivPriv->pBackBuffer);
	}
}

static Bool
FFBDbePositionWindow(WindowPtr pWin, int x, int y)
{
	ScreenPtr		pScreen;
	DbeScreenPrivPtr	pDbeScreenPriv;
	DbeWindowPrivPtr	pDbeWindowPriv;
	FFBDbeWindowPrivPrivPtr	pDbeWindowPrivPriv;
	int			width, height;
	int			dx, dy, dw, dh;
	int			sourcex, sourcey;
	int			destx, desty;
	int			savewidth, saveheight;
	PixmapPtr		pFrontBuffer;
	PixmapPtr		pBackBuffer;
	Bool			clear;
	GCPtr			pGC;
	xRectangle		clearRect;
	Bool			ret;

	/* 1. Unwrap the member routine. */
	pScreen                 = pWin->drawable.pScreen;
	pDbeScreenPriv          = DBE_SCREEN_PRIV(pScreen);
	pScreen->PositionWindow = pDbeScreenPriv->PositionWindow;

	/* 2. Do any work necessary before the member routine is called.
	 *
	 *    In this case we do not need to do anything.
	 */
     
	/* 3. Call the member routine, saving its result if necessary. */
	ret = (*pScreen->PositionWindow)(pWin, x, y);

	/* 4. Rewrap the member routine, restoring the wrapper value first in case
	 *    the wrapper (or something that it wrapped) change this value.
	 */
	pDbeScreenPriv->PositionWindow = pScreen->PositionWindow;
	pScreen->PositionWindow = FFBDbePositionWindow;

	/* 5. Do any work necessary after the member routine has been called. */
	if (!(pDbeWindowPriv = DBE_WINDOW_PRIV(pWin)))
		return ret;

	if (pDbeWindowPriv->width  == pWin->drawable.width &&
	    pDbeWindowPriv->height == pWin->drawable.height)
		return ret;

	width  = pWin->drawable.width;
	height = pWin->drawable.height;

	dx = pWin->drawable.x - pDbeWindowPriv->x;
	dy = pWin->drawable.y - pDbeWindowPriv->y;
	dw = width  - pDbeWindowPriv->width;
	dh = height - pDbeWindowPriv->height;

	GravityTranslate (0, 0, -dx, -dy, dw, dh, pWin->bitGravity, &destx, &desty);

	clear = ((pDbeWindowPriv->width  < (unsigned short)width ) ||
		 (pDbeWindowPriv->height < (unsigned short)height) ||
		 (pWin->bitGravity == ForgetGravity));

	sourcex = 0;
	sourcey = 0;
	savewidth  = pDbeWindowPriv->width;
	saveheight = pDbeWindowPriv->height;

	/* Clip rectangle to source and destination. */
	if (destx < 0) {
		savewidth += destx;
		sourcex   -= destx;
		destx      = 0;
	}

	if (destx + savewidth > width)
		savewidth = width - destx;

	if (desty < 0) {
		saveheight += desty;
		sourcey    -= desty;
		desty       = 0;
	}

	if (desty + saveheight > height)
		saveheight = height - desty;

	pDbeWindowPriv->width  = width;
	pDbeWindowPriv->height = height;
	pDbeWindowPriv->x = pWin->drawable.x;
	pDbeWindowPriv->y = pWin->drawable.y;

	pGC = GetScratchGC (pWin->drawable.depth, pScreen);

	if (clear) {
		if ((*pDbeScreenPriv->SetupBackgroundPainter)(pWin, pGC)) {
			clearRect.x = 0;
			clearRect.y = 0;
			clearRect.width  = width;
			clearRect.height = height;
		} else {
			clear = FALSE;
		}
	}

	pDbeWindowPrivPriv = FFB_DBE_WINDOW_PRIV_PRIV(pDbeWindowPriv);
	if (pDbeWindowPrivPriv->HwAccelerated != 0) {
		/* If we're hw accelerating, things are much easier. */
		ValidateGC(&pWin->drawable, pGC);
		FFBDbeUpdateWidPlane(pWin, pGC);
		if (clear) {
			CreatorPrivWinPtr pFfbPrivWin = CreatorGetWindowPrivate(pWin);
			unsigned int fbc, orig_fbc;

			ValidateGC(&pWin->drawable, pGC);
			(*pGC->ops->PolyFillRect)(&pWin->drawable, pGC,
						  1, &clearRect);

			orig_fbc = fbc = pFfbPrivWin->fbc_base;
			fbc &= ~(FFB_FBC_WB_MASK);
			if (pDbeWindowPrivPriv->HwCurrentBuf == 0)
				fbc |= FFB_FBC_WB_B;
			else
				fbc |= FFB_FBC_WB_A;

			pFfbPrivWin->fbc_base = fbc;

			if ((*pDbeScreenPriv->SetupBackgroundPainter)(pWin, pGC)) {
				ValidateGC(&pWin->drawable, pGC);
				clearRect.x = 0;
				clearRect.y = 0;
				clearRect.width  = width;
				clearRect.height = height;
				(*pGC->ops->PolyFillRect)(&pWin->drawable, pGC,
							  1, &clearRect);
			}

			pFfbPrivWin->fbc_base = orig_fbc;
		}

		FreeScratchGC(pGC);
	} else {
		/* Create DBE buffer pixmaps equal to size of resized window. */
		pFrontBuffer = (*pScreen->CreatePixmap)(pScreen, width, height,
							pWin->drawable.depth);

		pBackBuffer = (*pScreen->CreatePixmap)(pScreen, width, height,
						       pWin->drawable.depth);

		if (!pFrontBuffer || !pBackBuffer) {
			/* We failed at creating 1 or 2 of the pixmaps. */
			if (pFrontBuffer)
				(*pScreen->DestroyPixmap)(pFrontBuffer);
			if (pBackBuffer)
				(*pScreen->DestroyPixmap)(pBackBuffer);

			/* Destroy all buffers for this window. */
			while (pDbeWindowPriv) {
				/* DbeWindowPrivDelete() will free the window private
				 * if there no more buffer IDs associated with this
				 * window.
				 */
				FreeResource(pDbeWindowPriv->IDs[0], RT_NONE);
				pDbeWindowPriv = DBE_WINDOW_PRIV(pWin);
			}
			FreeScratchGC(pGC);
			return FALSE;
		} else {
			/* Clear out the new DBE buffer pixmaps. */
			ValidateGC((DrawablePtr)pFrontBuffer, pGC);

			/* I suppose this could avoid quite a bit of work if
			 * it computed the minimal area required.
			 */
			if (clear) {
				(*pGC->ops->PolyFillRect)((DrawablePtr)pFrontBuffer, pGC, 1,
							  &clearRect);
				(*pGC->ops->PolyFillRect)((DrawablePtr)pBackBuffer , pGC, 1,
							  &clearRect);
			}

			/* Copy the contents of the old DBE pixmaps to the new pixmaps. */
			if (pWin->bitGravity != ForgetGravity) {
				(*pGC->ops->CopyArea)((DrawablePtr)pDbeWindowPrivPriv->pFrontBuffer,
						      (DrawablePtr)pFrontBuffer, pGC,
						      sourcex, sourcey,
						      savewidth, saveheight,
						      destx, desty);
				(*pGC->ops->CopyArea)((DrawablePtr)pDbeWindowPrivPriv->pBackBuffer,
						      (DrawablePtr)pBackBuffer, pGC,
						      sourcex, sourcey,
						      savewidth, saveheight, destx, desty);
			}

			/* Destroy the old pixmaps, and point the DBE window priv to the new
			 * pixmaps.
			 */
			(*pScreen->DestroyPixmap)(pDbeWindowPrivPriv->pFrontBuffer);
			(*pScreen->DestroyPixmap)(pDbeWindowPrivPriv->pBackBuffer);

			pDbeWindowPrivPriv->pFrontBuffer = pFrontBuffer;
			pDbeWindowPrivPriv->pBackBuffer  = pBackBuffer;

			/* Make sure all XID are associated with the new back pixmap. */
			FFBDbeAliasBuffers(pDbeWindowPriv);

			FreeScratchGC(pGC);
		}
	}

	return ret;
}

static void
FFBDbeResetProc(ScreenPtr pScreen)
{
	DbeScreenPrivPtr pDbeScreenPriv = DBE_SCREEN_PRIV(pScreen);

	/* Unwrap wrappers */
	pScreen->PositionWindow = pDbeScreenPriv->PositionWindow;
}

static Bool
FFBDbeInit(ScreenPtr pScreen, DbeScreenPrivPtr pDbeScreenPriv)
{
	ScrnInfoPtr pScrn;
	FFBPtr pFfb;

	pScrn = xf86Screens[pScreen->myNum];
	pFfb = GET_FFB_FROM_SCRN(pScrn);
	xf86Msg(X_INFO, "%s: Setting up double-buffer acceleration.\n",
		pFfb->psdp->device);

	/* Copy resource types created by DIX */
	dbeDrawableResType   = pDbeScreenPriv->dbeDrawableResType;
	dbeWindowPrivResType = pDbeScreenPriv->dbeWindowPrivResType;

	/* Copy private indices created by DIX */
	dbeScreenPrivIndex = pDbeScreenPriv->dbeScreenPrivIndex;
	dbeWindowPrivIndex = pDbeScreenPriv->dbeWindowPrivIndex;

	/* Reset the window priv privs if generations do not match. */
	if (FFBDbePrivPrivGeneration != serverGeneration) {
		/* Allocate the window priv priv. */
		FFBDbeWindowPrivPrivIndex = (*pDbeScreenPriv->AllocWinPrivPrivIndex)();

		if (!(*pDbeScreenPriv->AllocWinPrivPriv)(pScreen,
							 FFBDbeWindowPrivPrivIndex,
							 sizeof(FFBDbeWindowPrivPrivRec)))
			return FALSE;

		/* Make sure we only do this code once. */
		FFBDbePrivPrivGeneration = serverGeneration;
	}

	/* Wrap functions. */
	pDbeScreenPriv->PositionWindow = pScreen->PositionWindow;
	pScreen->PositionWindow        = FFBDbePositionWindow;

	/* Initialize the per-screen DBE function pointers. */
	pDbeScreenPriv->GetVisualInfo         = FFBDbeGetVisualInfo;
	pDbeScreenPriv->AllocBackBufferName   = FFBDbeAllocBackBufferName;
	pDbeScreenPriv->SwapBuffers           = FFBDbeSwapBuffers;
	pDbeScreenPriv->BeginIdiom            = 0;
	pDbeScreenPriv->EndIdiom              = 0;
	pDbeScreenPriv->ResetProc             = FFBDbeResetProc;
	pDbeScreenPriv->WinPrivDelete         = FFBDbeWinPrivDelete;

	/* The FFB implementation doesn't need buffer validation. */
	pDbeScreenPriv->ValidateBuffer	  = (void (*)())NoopDDA;

	return TRUE;
}

extern void DbeRegisterFunction(ScreenPtr pScreen, Bool (*funct)(ScreenPtr, DbeScreenPrivPtr));

Bool
FFBDbePreInit(ScreenPtr pScreen)
{
	DbeRegisterFunction(pScreen, FFBDbeInit);
	return TRUE;
}
