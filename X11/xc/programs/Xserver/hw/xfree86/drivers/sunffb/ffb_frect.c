/*
 * Acceleration for the Creator and Creator3D framebuffer - Rectangle filling.
 *
 * Copyright (C) 1998,1999 Jakub Jelinek (jakub@redhat.com)
 * Copyright (C) 1998 Michal Rehacek (majkl@iname.com)
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
 * JAKUB JELINEK, MICHAL REHACEK, OR DAVID MILLER BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_frect.c,v 1.3 2001/04/05 17:42:33 dawes Exp $ */

#include "ffb.h"
#include "ffb_regs.h"
#include "ffb_rcache.h"
#include "ffb_fifo.h"
#include "ffb_stip.h"
#include "ffb_loops.h"

#include "pixmapstr.h"
#include "scrnintstr.h"

#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb32.h"

#define PAGEFILL_DISABLED(pFfb) ((pFfb)->disable_pagefill != 0)
#define FASTFILL_AP_DISABLED(pFfb) ((pFfb)->disable_fastfill_ap != 0)

void
CreatorFillBoxStipple (DrawablePtr pDrawable, int nBox, BoxPtr pBox, CreatorStipplePtr stipple)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	WindowPtr pWin = (WindowPtr) pDrawable;
	ffb_fbcPtr ffb = pFfb->regs;
	unsigned int bits[32];
	unsigned int newalign;

	FFBLOG(("CreatorFillBoxStipple: nbox(%d)\n", nBox));
	newalign = ((pDrawable->y & 31) << 16) | (pDrawable->x & 31);
	if (stipple->patalign != newalign) {
		int x, y, i;
		
		x = (pDrawable->x - (stipple->patalign & 0xffff)) & 31;
		y = (pDrawable->y - (stipple->patalign >> 16)) & 31;
		if (x | y) {
			memcpy(bits, stipple->bits, sizeof(bits));
			for (i = 0; i < 32; i++)
				stipple->bits[(i + y) & 31] =
					(bits[i] >> x) | (bits[i] << (32 - x));
			stipple->inhw = 0;
		}
		stipple->patalign = newalign;
	}

	FFBSetStipple(pFfb, ffb, stipple,
		      FFB_PPC_APE_DISABLE|FFB_PPC_CS_CONST|FFB_PPC_XS_WID,
		      FFB_PPC_APE_MASK|FFB_PPC_CS_MASK|FFB_PPC_XS_MASK);
	FFB_WRITE_PMASK(pFfb, ffb, ~0);
	FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_RECTANGLE);
	FFB_WRITE_FBC(pFfb, ffb, FFB_FBC_WIN(pWin));
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

enum ffb_fillrect_method { fillrect_page,
			   fillrect_fast, fillrect_fast_opaque,
			   fillrect_normal };

#define BOX_AREA(__w, __h)	((int)(__w) * (int)(__h))

/* Compute the page aligned box for a page mode fast fill.
 * In 'ework' this returns greater than zero if there are some odd
 * edges to take care of which are outside of the page aligned area.
 * It will place less than zero there if the box is too small,
 * indicating that a different method must be used to fill it.
 */
#define CreatorPageFillParms(pFfb, ffp, x, y, w, h, px, py, pw, ph, ework) \
do {	int xdiff, ydiff; \
	int pf_bh = ffp->pagefill_height; \
	int pf_bw = ffp->pagefill_width; \
	py = ((y + (pf_bh - 1)) & ~(pf_bh - 1)); \
	ydiff = py - y; \
	px = pFfb->Pf_AlignTab[x + (pf_bw - 1)]; \
	xdiff = px - x; \
	ph = ((h - ydiff) & ~(pf_bh - 1)); \
	if(ph <= 0) \
		ework = -1; \
	else { \
		pw = pFfb->Pf_AlignTab[w - xdiff]; \
		if(pw <= 0) { \
			ework = -1; \
		} else { \
			ework = (((xdiff > 0)		|| \
				  (ydiff > 0)		|| \
				  ((w - pw) > 0)	|| \
				  ((h - ph) > 0))) ? 1 : 0; \
		} \
	} \
} while(0);

/* Compute fixups of non-page aligned areas after a page fill.
 * Return the number of fixups needed.
 */
static __inline__ int
CreatorComputePageFillFixups(xRectangle *fixups,
			     int x, int y, int w, int h,
			     int paligned_x, int paligned_y,
			     int paligned_w, int paligned_h)
{
	int nfixups = 0;

	/* FastFill Left */
	if(paligned_x != x) {
		fixups[nfixups].x = x;
		fixups[nfixups].y = paligned_y;
		fixups[nfixups].width = paligned_x - x;
		fixups[nfixups].height = paligned_h;
		nfixups++;
	}
	/* FastFill Top */
	if(paligned_y != y) {
		fixups[nfixups].x = x;
		fixups[nfixups].y = y;
		fixups[nfixups].width = w;
		fixups[nfixups].height = paligned_y - y;
		nfixups++;
	}
	/* FastFill Right */
	if((x+w) != (paligned_x+paligned_w)) {
		fixups[nfixups].x = (paligned_x+paligned_w);
		fixups[nfixups].y = paligned_y;
		fixups[nfixups].width = (x+w) - fixups[nfixups].x;
		fixups[nfixups].height = paligned_h;
		nfixups++;
	}
	/* FastFill Bottom */
	if((y+h) != (paligned_y+paligned_h)) {
		fixups[nfixups].x = x;
		fixups[nfixups].y = (paligned_y+paligned_h);
		fixups[nfixups].width = w;
		fixups[nfixups].height = (y+h) - fixups[nfixups].y;
		nfixups++;
	}
	return nfixups;
}

/* Fill a set of boxes, pagefill and fastfill not allowed. */
static void
CreatorBoxFillNormal(FFBPtr pFfb,
		     int nbox, BoxPtr pbox)
{
	ffb_fbcPtr ffb = pFfb->regs;

	FFBLOG(("BFNormal: "));
	if(nbox)
		FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_RECTANGLE);
	while(nbox--) {
		register int x, y, w, h;

		x = pbox->x1;
		y = pbox->y1;
		w = (pbox->x2 - x);
		h = (pbox->y2 - y);
		pbox++;
		FFBLOG(("[%08x:%08x:%08x:%08x] ", x, y, w, h));
		FFBFifo(pFfb, 4);
		FFB_WRITE64(&ffb->by, y, x);
		FFB_WRITE64_2(&ffb->bh, h, w);
	}
	FFBLOG(("\n"));
}

/* Fill a set of boxes, only non-pagemode fastfill is allowed. */
static void
CreatorBoxFillFast(FFBPtr pFfb,
		   int nbox, BoxPtr pbox)
{
	ffb_fbcPtr ffb = pFfb->regs;

	FFBLOG(("BFFast: "));
	while(nbox--) {
		struct fastfill_parms *ffp = &FFB_FFPARMS(pFfb);
		register int x, y, w, h;

		x = pbox->x1;
		y = pbox->y1;
		w = (pbox->x2 - x);
		h = (pbox->y2 - y);
		pbox++;
		if(BOX_AREA(w, h) < ffp->fastfill_small_area) {
			/* Too small for fastfill to be useful. */
			FFBLOG(("NRM(%08x:%08x:%08x:%08x) ",
				x, y, w, h));
			FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_RECTANGLE);
			FFBFifo(pFfb, 4);
			FFB_WRITE64(&ffb->by, y, x);
			FFB_WRITE64_2(&ffb->bh, h, w);
		} else {
			FFBLOG(("FST(%08x:%08x:%08x:%08x:[%08x:%08x]) ",
				x, y, w, h,
				(w + (x & (ffp->fastfill_width - 1))),
				(h + (y & (ffp->fastfill_height - 1)))));
			if (pFfb->ffb_res == ffb_res_high &&
			    ((x & 7) != 0 || (w & 7) != 0)) {
				FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_RECTANGLE);
				if ((x & 7) != 0) {
					register int nx = x;
					register int nw;

					nw = 8 - (nx & 7);
					if (nw > w)
						nw = w;
					FFBFifo(pFfb, 4);
					FFB_WRITE64(&ffb->by, y, nx);
					FFB_WRITE64_2(&ffb->bh, h, nw);
					x += nw;
					w -= nw;
				}
				if ((w & 7) != 0) {
					register int nx, nw;

					nw = (w & 7);
					nx = x + (w - nw);
					FFBFifo(pFfb, 4);
					FFB_WRITE64(&ffb->by, y, nx);
					FFB_WRITE64_2(&ffb->bh, h, nw);
					w -= nw;
				}
				if (w <= 0)
					goto next_rect;
			}
			FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_FASTFILL);
			FFBFifo(pFfb, 10);
			ffb->by = FFB_FASTFILL_COLOR_BLK;
			FFB_WRITE64(&ffb->dy, 0, 0);
			FFB_WRITE64_2(&ffb->bh,
				      ffp->fastfill_height,
				      (ffp->fastfill_width * 4));
			FFB_WRITE64_3(&ffb->dy, y, x);
			ffb->bh = (h + (y & (ffp->fastfill_height - 1)));
			FFB_WRITE64(&ffb->by, FFB_FASTFILL_BLOCK,
				    (w + (x & (ffp->fastfill_width - 1))));
		}
	next_rect:
		;
	}
	FFBLOG(("\n"));
}

/* Fill a set of boxes, any fastfill method is allowed. */
static void
CreatorBoxFillPage(FFBPtr pFfb,
		   int nbox, BoxPtr pbox)
{
	ffb_fbcPtr ffb = pFfb->regs;

	FFBLOG(("BFPage: "));
	while(nbox--) {
		struct fastfill_parms *ffp = &FFB_FFPARMS(pFfb);
		register int x, y, w, h;

		x = pbox->x1;
		y = pbox->y1;
		w = (pbox->x2 - x);
		h = (pbox->y2 - y);
		pbox++;
		if(BOX_AREA(w, h) < ffp->fastfill_small_area) {
			/* Too small for fastfill or page fill to be useful. */
			FFBLOG(("NRM(%08x:%08x:%08x:%08x) ",
				x, y, w, h));
			FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_RECTANGLE);
			FFBFifo(pFfb, 4);
			FFB_WRITE64(&ffb->by, y, x);
			FFB_WRITE64_2(&ffb->bh, h, w);
		} else {
			int paligned_y, paligned_x;
			int paligned_h, paligned_w = 0;
			int extra_work;

			if (pFfb->ffb_res == ffb_res_high &&
			    ((x & 7) != 0 || (w & 7) != 0)) {
				FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_RECTANGLE);
				if ((x & 7) != 0) {
					register int nx = x;
					register int nw;

					nw = 8 - (nx & 7);
					if (nw > w)
						nw = w;
					FFBFifo(pFfb, 4);
					FFB_WRITE64(&ffb->by, y, nx);
					FFB_WRITE64_2(&ffb->bh, h, nw);
					x += nw;
					w -= nw;
				}
				if ((w & 7) != 0) {
					register int nx, nw;

					nw = (w & 7);
					nx = x + (w - nw);
					FFBFifo(pFfb, 4);
					FFB_WRITE64(&ffb->by, y, nx);
					FFB_WRITE64_2(&ffb->bh, h, nw);
					w -= nw;
				}
				if (w <= 0)
					goto next_rect;
			}

			FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_FASTFILL);
			if((w < (ffp->pagefill_width<<1)) ||
			   (h < (ffp->pagefill_height<<1)))
				goto do_fastfill;

			CreatorPageFillParms(pFfb, ffp,
					     x, y, w, h,
					     paligned_x, paligned_y,
					     paligned_w, paligned_h, extra_work);

			/* See if the aligned area is large enough for
			 * page fill to be worthwhile.
			 */
			if(extra_work < 0 ||
			   BOX_AREA(paligned_w, paligned_h) < ffp->pagefill_small_area) {
		   do_fastfill:
				FFBLOG(("FST(%08x:%08x:%08x:%08x:[%08x:%08x]) ",
					x, y, w, h,
					(w + (x & (ffp->fastfill_width - 1))),
					(h + (y & (ffp->fastfill_height - 1)))));
				FFBFifo(pFfb, 10);
				ffb->by = FFB_FASTFILL_COLOR_BLK;
				FFB_WRITE64(&ffb->dy, 0, 0);
				FFB_WRITE64_2(&ffb->bh,
					      ffp->fastfill_height,
					      (ffp->fastfill_width * 4));
				FFB_WRITE64_3(&ffb->dy, y, x);
				ffb->bh = (h + (y & (ffp->fastfill_height - 1)));
				FFB_WRITE64(&ffb->by, FFB_FASTFILL_BLOCK,
					    (w + (x & (ffp->fastfill_width - 1))));
			} else {
				/* Ok, page fill is worth it, let it rip. */
				FFBLOG(("PAG(%08x:%08x:%08x:%08x) ",
					paligned_x, paligned_y, paligned_w, paligned_h));
				FFBFifo(pFfb, 15);
				ffb->by = FFB_FASTFILL_COLOR_BLK;
				FFB_WRITE64(&ffb->dy, 0, 0);
				FFB_WRITE64_2(&ffb->bh, ffp->fastfill_height, (ffp->fastfill_width * 4));
				ffb->by = FFB_FASTFILL_BLOCK_X;
				FFB_WRITE64(&ffb->dy, 0, 0);
				FFB_WRITE64_2(&ffb->bh, ffp->pagefill_height, (ffp->pagefill_width * 4));
				FFB_WRITE64_3(&ffb->dy, paligned_y, paligned_x);
				ffb->bh = paligned_h;
				FFB_WRITE64(&ffb->by, FFB_FASTFILL_PAGE, paligned_w);

				if(extra_work) {
					register int nfixups;

					/* Ok, we're going to do at least one fixup. */
					nfixups = CreatorComputePageFillFixups(pFfb->Pf_Fixups,
									       x, y, w, h,
									       paligned_x, paligned_y,
									       paligned_w, paligned_h);

					/* NOTE: For the highres case we have already
					 *       aligned the outermost X and W coordinates.
					 *	 Therefore we can be assured that the fixup
					 *	 X and W coordinates below will be 8 pixel
					 *	 aligned as well.  Do the math, it works. -DaveM
					 */

					FFBFifo(pFfb, 5 + (nfixups * 5));
					ffb->by = FFB_FASTFILL_COLOR_BLK;
					FFB_WRITE64(&ffb->dy, 0, 0);
					FFB_WRITE64_2(&ffb->bh, ffp->fastfill_height, (ffp->fastfill_width * 4));

					while(--nfixups >= 0) {
						register int xx, yy, ww, hh;

						xx = pFfb->Pf_Fixups[nfixups].x;
						yy = pFfb->Pf_Fixups[nfixups].y;
						FFB_WRITE64(&ffb->dy, yy, xx);
						ww = (pFfb->Pf_Fixups[nfixups].width +
						      (xx & (ffp->fastfill_width - 1)));
						hh = (pFfb->Pf_Fixups[nfixups].height +
						      (yy & (ffp->fastfill_height - 1)));
						FFBLOG(("FIXUP(%08x:%08x:%08x:%08x) ",
							xx, yy, ww, hh));
						if(nfixups != 0) {
							ffb->by = FFB_FASTFILL_BLOCK;
							FFB_WRITE64_2(&ffb->bh, hh, ww);
						} else {
							ffb->bh = hh;
							FFB_WRITE64(&ffb->by, FFB_FASTFILL_BLOCK, ww);
						}
					}
				}
			}
		}
	next_rect:
		;
	}
	FFBLOG(("\n"));
}

void
CreatorFillBoxSolid (DrawablePtr pDrawable, int nBox, BoxPtr pBox, unsigned long pixel)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	WindowPtr pWin = (WindowPtr) pDrawable;

	FFBLOG(("CreatorFillBoxSolid: nbox(%d)\n", nBox));
	FFB_ATTR_FFWIN(pFfb, pWin,
		       FFB_PPC_APE_DISABLE | FFB_PPC_CS_CONST,
		       pixel);
	if (PAGEFILL_DISABLED(pFfb))
		CreatorBoxFillNormal(pFfb, nBox, pBox);
	else
		CreatorBoxFillPage(pFfb, nBox, pBox);

	pFfb->rp_active = 1;
	FFBSync(pFfb, pFfb->regs);
}

static void
FFBSetStippleFast(FFBPtr pFfb, ffb_fbcPtr ffb,
		  CreatorStipplePtr stipple,
		  unsigned int ppc, unsigned int ppc_mask)
{
	ppc |= FFB_PPC_APE_ENABLE | FFB_PPC_TBE_TRANSPARENT | FFB_PPC_XS_WID;
	ppc_mask |= FFB_PPC_APE_MASK | FFB_PPC_TBE_MASK | FFB_PPC_XS_MASK;
	FFB_WRITE_PPC(pFfb, ffb, ppc, ppc_mask);
	FFB_WRITE_ROP(pFfb, ffb, (FFB_ROP_EDIT_BIT|stipple->alu)|(FFB_ROP_NEW<<8));
	FFB_WRITE_FG(pFfb, ffb, stipple->fg);
	FFBFifo(pFfb, 32);
	FFB_STIPPLE_LOAD(&ffb->pattern[0], &stipple->bits[0]);
}

static void
FFBSetStippleFastIdentity(FFBPtr pFfb,
			  ffb_fbcPtr ffb,
			  CreatorStipplePtr stipple)
{
	int i;

	FFB_WRITE_FG(pFfb, ffb, stipple->bg);
	FFBFifo(pFfb, 32);
	for(i = 0; i < 32; i++)
		ffb->pattern[i] = ~stipple->bits[i];
	stipple->inhw = 0;
	pFfb->laststipple = NULL;
}

void
CreatorPolyFillRect(DrawablePtr pDrawable, GCPtr pGC, int nrectFill, xRectangle *prectInit)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
	xRectangle	*prect;
	RegionPtr	prgnClip;
	register BoxPtr	pbox;
	register BoxPtr	pboxClipped;
	BoxPtr		pboxClippedBase;
	BoxPtr		pextent;
	CreatorPrivGCPtr gcPriv;
	int		numRects;
	int		n;
	int		xorg, yorg;
    
	/* No garbage please. */
	if (nrectFill <= 0)
		return;

	gcPriv = CreatorGetGCPrivate (pGC);
	FFBLOG(("CreatorPolyFillRect: nrect(%d) ALU(%x) STIP(%p) pmsk(%08x)\n",
		nrectFill, pGC->alu, gcPriv->stipple, pGC->planemask));
	prgnClip = cfbGetCompositeClip(pGC);
	prect = prectInit;
	xorg = pDrawable->x;
	yorg = pDrawable->y;
	if (xorg || yorg) {
		prect = prectInit;
		n = nrectFill;
		while (n--) {
			prect->x += xorg;
			prect->y += yorg;
			prect++;
		}
	}

	prect = prectInit;
	numRects = REGION_NUM_RECTS (prgnClip) * nrectFill;
	if (numRects > 64) {
		pboxClippedBase = (BoxPtr)ALLOCATE_LOCAL(numRects * sizeof(BoxRec));
		if (!pboxClippedBase)
			return;
	} else
		pboxClippedBase = pFfb->ClippedBoxBuf;

	pboxClipped = pboxClippedBase;
	if (REGION_NUM_RECTS(prgnClip) == 1) {
		int x1, y1, x2, y2, bx2, by2;

		pextent = REGION_RECTS(prgnClip);
		x1 = pextent->x1;
		y1 = pextent->y1;
		x2 = pextent->x2;
		y2 = pextent->y2;
		while (nrectFill--) {
			if ((pboxClipped->x1 = prect->x) < x1)
				pboxClipped->x1 = x1;

			if ((pboxClipped->y1 = prect->y) < y1)
				pboxClipped->y1 = y1;

			bx2 = (int) prect->x + (int) prect->width;
			if (bx2 > x2)
				bx2 = x2;
			pboxClipped->x2 = bx2;

			by2 = (int) prect->y + (int) prect->height;
			if (by2 > y2)
				by2 = y2;
			pboxClipped->y2 = by2;

			prect++;
			if ((pboxClipped->x1 < pboxClipped->x2) &&
			    (pboxClipped->y1 < pboxClipped->y2))
				pboxClipped++;
		}
	} else {
		int x1, y1, x2, y2, bx2, by2;

		pextent = REGION_EXTENTS(pGC->pScreen, prgnClip);
		x1 = pextent->x1;
		y1 = pextent->y1;
		x2 = pextent->x2;
		y2 = pextent->y2;
		while (nrectFill--) {
			BoxRec box;

			if ((box.x1 = prect->x) < x1)
				box.x1 = x1;

			if ((box.y1 = prect->y) < y1)
				box.y1 = y1;

			bx2 = (int) prect->x + (int) prect->width;
			if (bx2 > x2)
				bx2 = x2;
			box.x2 = bx2;

			by2 = (int) prect->y + (int) prect->height;
			if (by2 > y2)
				by2 = y2;
			box.y2 = by2;

			prect++;

			if ((box.x1 >= box.x2) || (box.y1 >= box.y2))
				continue;

			n = REGION_NUM_RECTS (prgnClip);
			pbox = REGION_RECTS(prgnClip);

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
	/* Now fill the pre-clipped boxes. */
	if(pboxClipped != pboxClippedBase) {
		enum ffb_fillrect_method how = fillrect_page;
		int num = (pboxClipped - pboxClippedBase);
		int f_w = pboxClippedBase->x2 - pboxClippedBase->x1;
		int f_h = pboxClippedBase->y2 - pboxClippedBase->y1;
		WindowPtr pWin = (WindowPtr) pDrawable;
		unsigned int fbc = FFB_FBC_WIN(pWin);
		unsigned int drawop = FFB_DRAWOP_FASTFILL;

		if (PAGEFILL_DISABLED(pFfb) ||
		    pGC->alu != GXcopy ||
		    BOX_AREA(f_w, f_h) < 128) {
			drawop = FFB_DRAWOP_RECTANGLE;
			how = fillrect_normal;
		} else if (gcPriv->stipple != NULL) {
			if (FASTFILL_AP_DISABLED(pFfb)) {
				drawop = FFB_DRAWOP_RECTANGLE;
				how = fillrect_normal;
			} else {
				if ((gcPriv->stipple->alu & FFB_ROP_EDIT_BIT) != 0)
					how = fillrect_fast;
				else
					how = fillrect_fast_opaque;
			}
		} else {
			int all_planes;

			/* Plane masks are not controllable with page fills. */
			if (pGC->depth == 8)
				all_planes = 0xff;
			else
				all_planes = 0xffffff;
			if ((pGC->planemask & all_planes) != all_planes)
				how = fillrect_fast;
		}

		if (how == fillrect_page) {
			fbc &= ~(FFB_FBC_XE_MASK | FFB_FBC_RGBE_MASK);
			fbc |= FFB_FBC_XE_ON | FFB_FBC_RGBE_ON;
		}

		/* In the high-resolution modes, the Creator3D transforms
		 * the framebuffer such that the dual-buffers present become
		 * one large single buffer.  As such you need to enable both
		 * A and B write buffers for page/fast fills to work properly
		 * under this configuration. -DaveM
		 */
		if (pFfb->ffb_res == ffb_res_high)
			fbc |= FFB_FBC_WB_B;

		/* Setup the attributes. */
		if (gcPriv->stipple == NULL) {
			FFB_ATTR_RAW(pFfb,
				     FFB_PPC_APE_DISABLE|FFB_PPC_CS_CONST|FFB_PPC_XS_WID,
				     FFB_PPC_APE_MASK|FFB_PPC_CS_MASK|FFB_PPC_XS_MASK,
				     pGC->planemask,
				     ((FFB_ROP_EDIT_BIT|pGC->alu)|(FFB_ROP_NEW<<8)),
				     drawop,
				     pGC->fgPixel,
				     fbc, FFB_WID_WIN(pWin));
		} else {
			if (how == fillrect_fast_opaque) {
				FFBSetStippleFast(pFfb, ffb, gcPriv->stipple,
						  FFB_PPC_CS_CONST|FFB_PPC_XS_WID,
						  FFB_PPC_CS_MASK|FFB_PPC_XS_MASK);
			} else {
				FFBSetStipple(pFfb, ffb, gcPriv->stipple,
					      FFB_PPC_CS_CONST|FFB_PPC_XS_WID,
					      FFB_PPC_CS_MASK|FFB_PPC_XS_MASK);
			}
			FFB_WRITE_DRAWOP(pFfb, ffb, drawop);
			FFB_WRITE_FBC(pFfb, ffb, fbc);
			FFB_WRITE_WID(pFfb, ffb, FFB_WID_WIN(pWin));
		}

		/* Now render. */
		if(how == fillrect_normal)
			CreatorBoxFillNormal(pFfb, num, pboxClippedBase);
		else if(how == fillrect_fast || how == fillrect_fast_opaque)
			CreatorBoxFillFast(pFfb, num, pboxClippedBase);
		else
			CreatorBoxFillPage(pFfb, num, pboxClippedBase);

		if(how == fillrect_fast_opaque) {
			FFBSetStippleFastIdentity(pFfb, ffb, gcPriv->stipple);
			CreatorBoxFillFast(pFfb, num, pboxClippedBase);
		}

		pFfb->rp_active = 1;
		FFBSync(pFfb, ffb);
	}
	if (pboxClippedBase != pFfb->ClippedBoxBuf)
		DEALLOCATE_LOCAL (pboxClippedBase);
}
