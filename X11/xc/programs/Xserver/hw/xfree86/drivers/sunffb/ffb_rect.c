/*
 * Acceleration for the Creator and Creator3D framebuffer - Non-filled rects.
 *
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
 * Copyright (C) 1999 Jakub Jelinek   (jakub@redhat.com)
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
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_rect.c,v 1.2 2000/05/23 04:47:45 dawes Exp $ */

#define PSZ 32

#include "ffb.h"
#include "ffb_regs.h"
#include "ffb_rcache.h"
#include "ffb_fifo.h"
#include "ffb_loops.h"
#include "ffb_clip.h"

#include "pixmapstr.h"
#include "scrnintstr.h"

#include "cfb.h"

#error If we start using this again, need to fixup FFB_WRITE_ATTRIBUTES for wids -DaveM

/* Heavily derived from mipolyrect.c code, see there for authors. */

/* This about it, capping makes not a single difference ever,
 * always the upper left corner coordinate will be pixelated.
 */
void
CreatorPolyRectangle(DrawablePtr pDrawable, GCPtr pGC,
		     int nrects, xRectangle *pRects)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	CreatorPrivGCPtr gcPriv = CreatorGetGCPrivate (pGC);
	ffb_fbcPtr ffb = pFfb->regs;
	xRectangle *pR = pRects;
	RegionPtr clip;
	unsigned int ppc, line_drawop;
	int bound_tmp, i, numRects;

	if(nrects <= 0)
		return;
	clip = ((cfbPrivGC *)(pGC->devPrivates[cfbGCPrivateIndex].ptr))->pCompositeClip;
	numRects = REGION_NUM_RECTS(clip);
	if (!numRects)
		return;
	if(!(ppc = FFBSetClip(pFfb, ffb, clip, numRects))) {
		miPolyRectangle(pDrawable, pGC, nrects, pRects);
		return;
	}
	if(pGC->lineStyle == LineDoubleDash)
		line_drawop = FFB_DRAWOP_DDLINE;
	else
		line_drawop = FFB_DRAWOP_BRLINECAP;
	if(gcPriv->stipple == NULL) {
		FFB_WRITE_ATTRIBUTES(pFfb,
				     ppc|FFB_PPC_APE_DISABLE|FFB_PPC_TBE_OPAQUE|
				     FFB_PPC_XS_CONST|FFB_PPC_YS_CONST|FFB_PPC_ZS_CONST|FFB_PPC_CS_CONST,
				     FFB_PPC_VCE_MASK|FFB_PPC_ACE_MASK|FFB_PPC_APE_MASK|FFB_PPC_TBE_MASK|
				     FFB_PPC_XS_MASK|FFB_PPC_YS_MASK|FFB_PPC_ZS_MASK|FFB_PPC_CS_MASK,
				     pGC->planemask,
				     FFB_ROP_EDIT_BIT|pGC->alu,
				     -1, pGC->fgPixel,
				     FFB_FBC_DEFAULT);
		if(pGC->lineStyle == LineDoubleDash)
			FFB_WRITE_BG(pFfb, ffb, pGC->bgPixel);
	} else {
		FFBSetStipple(pFfb, ffb, gcPriv->stipple,
			      ppc|
			      FFB_PPC_XS_CONST|FFB_PPC_YS_CONST|FFB_PPC_ZS_CONST|FFB_PPC_CS_CONST,
			      FFB_PPC_VCE_MASK|FFB_PPC_ACE_MASK|
			      FFB_PPC_XS_MASK|FFB_PPC_YS_MASK|FFB_PPC_ZS_MASK|FFB_PPC_CS_MASK);
		FFB_WRITE_PMASK(pFfb, ffb, pGC->planemask);
		FFB_WRITE_FBC(pFfb, ffb, FFB_FBC_DEFAULT);
	}

#define MINBOUND(dst,eqn)	bound_tmp = eqn; \
				if (bound_tmp < -32768) \
				    bound_tmp = -32768; \
				dst = bound_tmp;

#define MAXBOUND(dst,eqn)	bound_tmp = eqn; \
				if (bound_tmp > 32767) \
				    bound_tmp = 32767; \
				dst = bound_tmp;

#define MAXUBOUND(dst,eqn)	bound_tmp = eqn; \
				if (bound_tmp > 65535) \
				    bound_tmp = 65535; \
				dst = bound_tmp;

	if (pGC->lineStyle == LineSolid &&
	    pGC->joinStyle == JoinMiter &&
	    pGC->lineWidth != 0) {
		int ntmp;
		int offset1, offset2, offset3;
		int x, y, width, height;
		int tx, ty, tw, th;

		ntmp = (nrects << 2);
		offset2 = pGC->lineWidth;
		offset1 = offset2 >> 1;
		offset3 = offset2 - offset1;
		for (i = 0; i < nrects; i++) {
			x = pR->x;
			y = pR->y;
			width = pR->width;
			height = pR->height;
			pR++;
			if (width == 0 && height == 0) {
				FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_DOT);
				FFBFifo(pFfb, 2);
				FFB_WRITE64(&ffb->bh, y + pDrawable->y, x + pDrawable->x);
				continue;
			}
			FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_RECTANGLE);
			if (height < offset2 || width < offset1) {
				if (height == 0) {
					tx = x;
					tw = width;
				} else {
					MINBOUND (tx, x - offset1);
					MAXUBOUND (tw, width + offset2);
				}
				if (width == 0) {
					ty = y;
					th = height;
				} else {
					MINBOUND (ty, y - offset1);
					MAXUBOUND (th, height + offset2);
				}
				FFBFifo(pFfb, 4);
				FFB_WRITE64(&ffb->by, ty + pDrawable->y, tx + pDrawable->x);
				FFB_WRITE64_2(&ffb->bh, th, tw);
			} else {
				MINBOUND(tx, x - offset1);
				MINBOUND(ty, y - offset1);
				MAXUBOUND(tw, width + offset2);
				th = offset2;
				FFBFifo(pFfb, 13);
				FFB_WRITE64(&ffb->by, ty + pDrawable->y, tx + pDrawable->x);
				FFB_WRITE64_2(&ffb->bh, th, tw);
				MAXBOUND(ty, y + offset3);
				tw = offset2;
				th = height - offset2;
				ffb->by = ty + pDrawable->y;
				FFB_WRITE64_2(&ffb->bh, th, tw);
				MAXBOUND(tx, x + width - offset1);
				ffb->bx = tx + pDrawable->x;
				ffb->bw = tw;				
				MINBOUND(tx, x - offset1);
				MAXBOUND(ty, y + height - offset1);
				MAXUBOUND(tw, width + offset2);
				th = offset2;
				FFB_WRITE64(&ffb->by, ty + pDrawable->y, tx + pDrawable->x);
				FFB_WRITE64_2(&ffb->bh, th, tw);
			}
		}
	} else {
		int xOrg = pDrawable->x;
		int yOrg = pDrawable->y;
		unsigned int linepat = gcPriv->linepat;

		FFB_WRITE_DRAWOP(pFfb, ffb, line_drawop);
		if(linepat == 0) {
			FFBFifo(pFfb, 1);
			ffb->lpat = 0;
		}
		for (i=0; i < nrects; i++) {
			register int x0, xCoord, y0, yCoord;

			x0 = xCoord = pR->x;
			y0 = yCoord = pR->y;
			FFBFifo(pFfb, 11);
			if(linepat)
				ffb->lpat = linepat;
			else
				ffb->ppc = 0;
			FFB_WRITE64(&ffb->by, (yCoord + yOrg), (xCoord + xOrg));
			MAXBOUND(xCoord, pR->x + (int) pR->width);
			FFB_WRITE64(&ffb->bh, (yCoord + yOrg), (xCoord + xOrg));
			MAXBOUND(yCoord, pR->y + (int) pR->height);
			FFB_WRITE64_2(&ffb->bh, (yCoord + yOrg), (xCoord + xOrg));
			FFB_WRITE64_3(&ffb->bh, (yCoord + yOrg), (x0 + xOrg));
			FFB_WRITE64(&ffb->bh, (y0 + yOrg), (x0 + xOrg));
			pR++;
		}
	}
}
