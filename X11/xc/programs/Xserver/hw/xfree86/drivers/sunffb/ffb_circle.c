/*
 * Acceleration for the Creator and Creator3D framebuffer - Circle rops.
 *
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
 * Copyright (C) 1999 Jakub Jelinek (jakub@redhat.com)
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
 * JAKUB JELINEK OR DAVID MILLER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_circle.c,v 1.2 2000/05/23 04:47:44 dawes Exp $ */

#include "ffb.h"
#include "ffb_regs.h"
#include "ffb_rcache.h"
#include "ffb_fifo.h"
#include "ffb_stip.h"

#include "pixmapstr.h"
#include "scrnintstr.h"

#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb32.h"
#include "cfb8_32wid.h"

#include "mi.h"
#include "mifillarc.h"

/* Wheee, circles... */
static void
CreatorFillEllipseSolid(DrawablePtr pDrawable, GCPtr pGC, xArc *arc)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pDrawable->pScreen);
	CreatorPrivGCPtr gcPriv = CreatorGetGCPrivate (pGC);
	ffb_fbcPtr ffb = pFfb->regs;
	miFillArcRec info;
	int x, y, e, yk, xk, ym, xm, dx, dy, xorg, yorg, slw;

	/* Get the RP ready. */
	if(gcPriv->stipple == NULL) {
		FFB_ATTR_GC(pFfb, pGC, pWin,
			    FFB_PPC_APE_DISABLE | FFB_PPC_CS_CONST,
			    FFB_DRAWOP_RECTANGLE);
	} else {
		unsigned int fbc;

		FFBSetStipple(pFfb, ffb, gcPriv->stipple,
			      FFB_PPC_CS_CONST, FFB_PPC_CS_MASK);
		FFB_WRITE_PMASK(pFfb, ffb, pGC->planemask);
		FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_RECTANGLE);
		fbc = FFB_FBC_WIN(pWin);
		fbc = (fbc & ~FFB_FBC_XE_MASK) | FFB_FBC_XE_OFF;
		FFB_WRITE_FBC(pFfb, ffb, fbc);
	}

	/* Start computing the rects. */
	miFillArcSetup(arc, &info);
	MIFILLARCSETUP();
	if(pGC->miTranslate) {
		xorg += pDrawable->x;
		yorg += pDrawable->y;
	}
	while(y > 0) {
		MIFILLARCSTEP(slw);
		if(slw > 0) {
			/* Render. */
			FFBFifo(pFfb, 4);
			FFB_WRITE64(&ffb->by, yorg - y, xorg - x);
			FFB_WRITE64_2(&ffb->bh, 1, slw);
			if(miFillArcLower(slw)) {
				FFBFifo(pFfb, 4);
				FFB_WRITE64(&ffb->by, yorg + y + dy, xorg - x);
				FFB_WRITE64_2(&ffb->bh, 1, slw);
			}
		}
	}
	pFfb->rp_active = 1;
	FFBSync(pFfb, ffb);
}

#define ADDSPAN(l,r) \
    if (r >= l) { \
	FFBFifo(pFfb, 4); \
	FFB_WRITE64(&ffb->by, ya, l); \
	FFB_WRITE64_2(&ffb->bh, 1, r - l + 1); \
    }

#define ADDSLICESPANS(flip) \
    if (!flip) \
    { \
	ADDSPAN(xl, xr); \
    } \
    else \
    { \
	xc = xorg - x; \
	ADDSPAN(xc, xr); \
	xc += slw - 1; \
	ADDSPAN(xl, xc); \
    }

static void
CreatorFillArcSliceSolid(DrawablePtr pDrawable, GCPtr pGC, xArc *arc)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pDrawable->pScreen);
	CreatorPrivGCPtr gcPriv = CreatorGetGCPrivate (pGC);
	ffb_fbcPtr ffb = pFfb->regs;
	miFillArcRec info;
	miArcSliceRec slice;
	int x, y, e, yk, xk, ym, xm, dx, dy, xorg, yorg, slw;
	int ya, xl, xr, xc;

	/* Get the RP ready. */
	if(gcPriv->stipple == NULL) {
		FFB_ATTR_GC(pFfb, pGC, pWin,
			    FFB_PPC_APE_DISABLE | FFB_PPC_CS_CONST,
			    FFB_DRAWOP_RECTANGLE);
	} else {
		unsigned int fbc;

		FFBSetStipple(pFfb, ffb, gcPriv->stipple,
			      FFB_PPC_CS_CONST, FFB_PPC_CS_MASK);
		FFB_WRITE_PMASK(pFfb, ffb, pGC->planemask);
		FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_RECTANGLE);
		fbc = FFB_FBC_WIN(pWin);
		fbc = (fbc & ~FFB_FBC_XE_MASK) | FFB_FBC_XE_OFF;
		FFB_WRITE_FBC(pFfb, ffb, fbc);
		FFB_WRITE_FBC(pFfb, ffb, FFB_FBC_WIN(pWin));
	}
	miFillArcSetup(arc, &info);
	miFillArcSliceSetup(arc, &slice, pGC);
	MIFILLARCSETUP();
	slw = arc->height;
	if (slice.flip_top || slice.flip_bot)
		slw += (arc->height >> 1) + 1;
	if (pGC->miTranslate) {
		xorg += pDrawable->x;
		yorg += pDrawable->y;
		slice.edge1.x += pDrawable->x;
		slice.edge2.x += pDrawable->x;
	}
	while (y > 0) {
		MIFILLARCSTEP(slw);
		MIARCSLICESTEP(slice.edge1);
		MIARCSLICESTEP(slice.edge2);
		if (miFillSliceUpper(slice)) {
			ya = yorg - y;
			MIARCSLICEUPPER(xl, xr, slice, slw);
			ADDSLICESPANS(slice.flip_top);
		}
		if (miFillSliceLower(slice)) {
			ya = yorg + y + dy;
			MIARCSLICELOWER(xl, xr, slice, slw);
			ADDSLICESPANS(slice.flip_bot);
		}
	}
	pFfb->rp_active = 1;
	FFBSync(pFfb, ffb);
}

void
CreatorPolyFillArcSolid (DrawablePtr pDrawable, GCPtr pGC, int narcs, xArc *parcs)
{
	RegionPtr cclip;
	xArc *arc;
	BoxRec box;
	int i, x2, y2;

	FFBLOG(("CreatorPolyFillArcSolid: narcs(%d)\n", narcs));
	cclip = cfbGetCompositeClip(pGC);
	for(arc = parcs, i = narcs; --i >= 0; arc++) {
		if(miFillArcEmpty(arc))
			continue;
		if(miCanFillArc(arc)) {
			box.x1 = arc->x + pDrawable->x;
			box.y1 = arc->y + pDrawable->y;
			box.x2 = x2 = box.x1 + (int)arc->width + 1;
			box.y2 = y2 = box.y1 + (int)arc->height + 1;
			if((x2 & ~0x7ff) == 0 &&
			   (y2 & ~0x7ff) == 0 &&
			   (RECT_IN_REGION(pDrawable->pScreen, cclip, &box) == rgnIN)) {
				if(arc->angle2 >= FULLCIRCLE ||
				   arc->angle2 <= -FULLCIRCLE)
					CreatorFillEllipseSolid(pDrawable, pGC, arc);
				else
					CreatorFillArcSliceSolid(pDrawable, pGC, arc);
				continue;
			}
		}
		/* Use slow mi code if we can't handle it simply. */
		miPolyFillArc(pDrawable, pGC, 1, arc);
	}
}
