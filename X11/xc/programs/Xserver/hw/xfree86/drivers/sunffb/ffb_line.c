/*
 * Acceleration for the Creator and Creator3D framebuffer - Line rops.
 *
 * Copyright (C) 1998,1999 Jakub Jelinek (jakub@redhat.com)
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_line.c,v 1.2 2000/05/23 04:47:45 dawes Exp $ */

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

#include "miline.h"

/* The scheme here is similar as for segments, except that
 * if there are any out of range coordinate, we fully punt
 * and do all of the pieces in software.  This is as to
 * avoid complexity in final line capping, sloped lines, line
 * patterns etc.
 */
#define IN_RANGE(_extent, _x, _y) \
	((_x) >= (_extent)->x1	&&	(_x) <  (_extent)->x2	&& \
	 (_y) >= (_extent)->y1	&&	(_y) <  (_extent)->y2)


void
CreatorPolylines (DrawablePtr pDrawable, GCPtr pGC, int mode, int nptInit, DDXPointPtr pptInit)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	CreatorPrivGCPtr gcPriv = CreatorGetGCPrivate (pGC);
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pGC->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
	DDXPointPtr ppt;
	BoxPtr extent;
	int x, y, xorg, yorg, npt, capLast;

	npt = nptInit;
	if (npt <= 1)
		return;
	ppt = pptInit;
	xorg = pDrawable->x;
	yorg = pDrawable->y;
	x = ppt->x + xorg;
	y = ppt->y + yorg;
	extent = REGION_RECTS(cfbGetCompositeClip(pGC));
	if (!IN_RANGE(extent, x, y))
		goto punt_rest;
	ppt++;
	while(npt--) {
		if (mode == CoordModeOrigin) {
			x = ppt->x + xorg;
			y = ppt->y + yorg;
		} else {
			x += ppt->x;
			y += ppt->y;
		}
		if (!IN_RANGE(extent, x, y))
			goto punt_rest;
		ppt++;
	}
	FFBLOG(("CreatorPolylines: npt(%d) lpat(%08x) alu(%x) pmsk(%08x)\n",
		npt, gcPriv->linepat, pGC->alu, pGC->planemask));
	if(gcPriv->stipple == NULL) {
		FFB_ATTR_GC(pFfb, pGC, pWin,
			    FFB_PPC_APE_DISABLE | FFB_PPC_CS_CONST,
			    FFB_DRAWOP_BRLINEOPEN);
	} else {
		unsigned int fbc;

		FFBSetStipple(pFfb, ffb, gcPriv->stipple,
			      FFB_PPC_CS_CONST, FFB_PPC_CS_MASK);
		FFB_WRITE_PMASK(pFfb, ffb, pGC->planemask);
		FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_BRLINEOPEN);
		fbc = FFB_FBC_WIN(pWin);
		fbc = (fbc & ~FFB_FBC_XE_MASK) | FFB_FBC_XE_OFF;
		FFB_WRITE_FBC(pFfb, ffb, fbc);
	}
	pFfb->rp_active = 1;

	ppt = pptInit;
	npt = nptInit;
	x = ppt->x + xorg;
	y = ppt->y + yorg;

	FFBFifo(pFfb, 3);
	ffb->lpat = gcPriv->linepat;
	ffb->by = y;
	ffb->bx = x;
	ppt++;
	npt--;
	capLast = pGC->capStyle != CapNotLast;
	if (mode == CoordModeOrigin) {
		if (capLast)
			npt--;
		if (pFfb->has_brline_bug) {
			while (npt--) {
				x = ppt->x + xorg;
				y = ppt->y + yorg;
				ppt++;
				FFBFifo(pFfb, 3);
				ffb->ppc = 0;
				FFB_WRITE64(&ffb->bh, y, x);
			}
		} else {
			while (npt--) {
				x = ppt->x + xorg;
				y = ppt->y + yorg;
				ppt++;
				FFBFifo(pFfb, 2);
				FFB_WRITE64(&ffb->bh, y, x);
			}
		}
		if (capLast) {
			register int x2, y2;

			x2 = ppt->x + xorg;
			y2 = ppt->y + yorg;

			FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_BRLINECAP);
			FFBFifo(pFfb, 5);
			ffb->ppc = 0;
			FFB_WRITE64(&ffb->by, y, x);
			FFB_WRITE64_2(&ffb->bh, y2, x2);
		}
	} else {
		if (capLast)
			npt--;
		if (pFfb->has_brline_bug) {
			while (npt--) {
				x += ppt->x;
				y += ppt->y;
				ppt++;

				FFBFifo(pFfb, 3);
				ffb->ppc = 0;
				FFB_WRITE64(&ffb->bh, y, x);
			}
		} else {
			while (npt--) {
				x += ppt->x;
				y += ppt->y;
				ppt++;

				FFBFifo(pFfb, 2);
				FFB_WRITE64(&ffb->bh, y, x);
			}
		}
		if (capLast) {
			register int x2, y2;

			x2 = x + ppt->x;
			y2 = y + ppt->y;

			FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_BRLINECAP);
			FFBFifo(pFfb, 5);
			ffb->ppc = 0;
			FFB_WRITE64(&ffb->by, y, x);
			FFB_WRITE64_2(&ffb->bh, y2, x2);
		}
	}
	FFBSync(pFfb, ffb);
	return;

punt_rest:
	gcPriv->Polylines(pDrawable, pGC, mode, nptInit, pptInit);
	return;
}
