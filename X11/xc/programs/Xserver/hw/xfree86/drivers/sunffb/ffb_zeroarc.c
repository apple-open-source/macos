/*
 * Acceleration for the Creator and Creator3D framebuffer - Zero arc
 * rops.
 *
 * Copyright (C) 1999 Jakub Jelinek (jakub@redhat.com)
 *
 * Derived from mi/mizerarc.c, see there for authors.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_zeroarc.c,v 1.2 2000/05/23 04:47:45 dawes Exp $ */

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

#include "mi.h"
#include "mizerarc.h"

#define FULLCIRCLE (360 * 64)
#define OCTANT (45 * 64)
#define QUADRANT (90 * 64)
#define HALFCIRCLE (180 * 64)
#define QUADRANT3 (270 * 64)

typedef struct {
	int skipStart;
	int haveStart;
	DDXPointRec startPt;
	int haveLast;
	int skipLast;
	DDXPointRec endPt;
	int dashIndex;
	int dashOffset;
	int dashIndexInit;
	int dashOffsetInit;
} DashInfo;

#define Pixelate(xval,yval,ext) 				\
if (((xval)+xoff) >= (ext)->x1 &&				\
    ((xval)+xoff) < (ext)->x2 &&				\
    ((yval)+yoff) >= (ext)->y1 &&				\
    ((yval)+yoff) < (ext)->y2) {				\
	FFBFifo(pFfb, 2);					\
	FFB_WRITE64(&ffb->bh, ((yval)+yoff), ((xval)+xoff));	\
}

#define Pixelate1(xval,yval,ext)				\
if (((xval)+xoff) >= (ext)->x1 &&				\
    ((xval)+xoff) < (ext)->x2 &&				\
    ((yval)+yoff) >= (ext)->y1 &&				\
    ((yval)+yoff) < (ext)->y2) {				\
	FFBFifo(pFfb, 2);					\
	FFB_WRITE64(&ffb->bh, ((yval)+yoff), ((xval)+xoff));	\
}

#define DoPix(idx,xval,yval,ext) if (mask & (1 << idx)) Pixelate(xval, yval,ext);

static void
CreatorZeroArcPts(xArc *arc, DrawablePtr pDrawable, GCPtr pGC, BoxPtr pextent)
{
	miZeroArcRec info;
	register int x, y, a, b, d, mask;
	register int k1, k3, dx, dy;
	int xoff, yoff;
	Bool do360;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;

	xoff = 0;
	yoff = 0;
	if (pGC->miTranslate) {
		xoff = pDrawable->x;
		yoff = pDrawable->y;
	}
	do360 = miZeroArcSetup(arc, &info, TRUE);
	MIARCSETUP();
	mask = info.initialMask;
	if (!(arc->width & 1)) {
		DoPix(1, info.xorgo, info.yorg, pextent);
		DoPix(3, info.xorgo, info.yorgo, pextent);
	}
	if (!info.end.x || !info.end.y) {
		mask = info.end.mask;
		info.end = info.altend;
	}
	if (do360 && (arc->width == arc->height) && !(arc->width & 1)) {
		int yorgh = info.yorg + info.h;
		int xorghp = info.xorg + info.h;
		int xorghn = info.xorg - info.h;
		int lastx = 0, lasty = 0;

		while (1) {
			if (a < 0)
				break;
			Pixelate1(info.xorg + x, info.yorg + y, pextent);
			Pixelate1(info.xorg - x, info.yorg + y, pextent);
			Pixelate1(info.xorg - x, info.yorgo - y, pextent);
			Pixelate1(info.xorg + x, info.yorgo - y, pextent);
			Pixelate1(xorghp - y, yorgh - x, pextent);
			Pixelate1(xorghn + y, yorgh - x, pextent);
			Pixelate1(xorghn + y, yorgh + x, pextent);
			lastx = xorghp - y; lasty = yorgh + x;
			Pixelate1(xorghp - y, yorgh + x, pextent);
			MIARCCIRCLESTEP(;);
		}
		if (x <= 0 || lastx != info.xorg + x || lasty != info.yorg + y) {
			Pixelate1(info.xorg + x, info.yorg + y, pextent);
			Pixelate1(info.xorg - x, info.yorg + y, pextent);
			Pixelate1(info.xorg - x, info.yorgo - y, pextent);
			Pixelate1(info.xorg + x, info.yorgo - y, pextent);
		}
		x = info.w;
		y = info.h;
	} else if (do360) {
		while (y < info.h || x < info.w) {
			MIARCOCTANTSHIFT(;);
			Pixelate1(info.xorg + x, info.yorg + y, pextent);
			Pixelate1(info.xorgo - x, info.yorg + y, pextent);
			Pixelate1(info.xorgo - x, info.yorgo - y, pextent);
			Pixelate1(info.xorg + x, info.yorgo - y, pextent);
			MIARCSTEP(;,;);
		}
	} else {
		while (y < info.h || x < info.w) {
			MIARCOCTANTSHIFT(;);
			if ((x == info.start.x) || (y == info.start.y)) {
				mask = info.start.mask;
				info.start = info.altstart;
			}
			DoPix(0, info.xorg + x, info.yorg + y, pextent);
			DoPix(1, info.xorgo - x, info.yorg + y, pextent);
			DoPix(2, info.xorgo - x, info.yorgo - y, pextent);
			DoPix(3, info.xorg + x, info.yorgo - y, pextent);
			if (x == info.end.x || y == info.end.y) {
				mask = info.end.mask;
				info.end = info.altend;
			}
			MIARCSTEP(;,;);
		}
	}
	if (x == info.start.x || y == info.start.y)
		mask = info.start.mask;
	DoPix(0, info.xorg + x, info.yorg + y, pextent);
	DoPix(2, info.xorgo - x, info.yorgo - y, pextent);
	if (arc->height & 1) {
		DoPix(1, info.xorgo - x, info.yorg + y, pextent);
		DoPix(3, info.xorg + x, info.yorgo - y, pextent);
	}
}

#undef DoPix
#define DoPix(idx,xval,yval) \
	if (mask & (1 << idx)) \
	{ \
	arcPts[idx]->x = xval; \
	arcPts[idx]->y = yval; \
	arcPts[idx]++; \
	}

static void
CreatorZeroArcDashPts(GCPtr pGC, xArc *arc, DashInfo *dinfo, DDXPointPtr points,
                      int maxPts, DDXPointPtr *evenPts, DDXPointPtr *oddPts)
{
	miZeroArcRec info;
	register int x, y, a, b, d, mask;
	register int k1, k3, dx, dy;
	int dashRemaining;
	DDXPointPtr arcPts[4];
	DDXPointPtr startPts[5], endPts[5];
	int deltas[5];
	DDXPointPtr startPt, pt, lastPt, pts;
	int i, j, delta, ptsdelta, seg, startseg;

	for (i = 0; i < 4; i++)
		arcPts[i] = points + (i * maxPts);
	miZeroArcSetup(arc, &info, FALSE);
	MIARCSETUP();
	mask = info.initialMask;
	startseg = info.startAngle / QUADRANT;
	startPt = arcPts[startseg];
	if (!(arc->width & 1)) {
		DoPix(1, info.xorgo, info.yorg);
		DoPix(3, info.xorgo, info.yorgo);
	}
	if (!info.end.x || !info.end.y) {
		mask = info.end.mask;
		info.end = info.altend;
	}
	while (y < info.h || x < info.w) {
		MIARCOCTANTSHIFT(;);
		if ((x == info.firstx) || (y == info.firsty))
			startPt = arcPts[startseg];
		if ((x == info.start.x) || (y == info.start.y)) {
			mask = info.start.mask;
			info.start = info.altstart;
		}
		DoPix(0, info.xorg + x, info.yorg + y);
		DoPix(1, info.xorgo - x, info.yorg + y);
		DoPix(2, info.xorgo - x, info.yorgo - y);
		DoPix(3, info.xorg + x, info.yorgo - y);
		if (x == info.end.x || y == info.end.y) {
			mask = info.end.mask;
			info.end = info.altend;
		}
		MIARCSTEP(;,;);
	}
	if (x == info.firstx || y == info.firsty)
		startPt = arcPts[startseg];
	if (x == info.start.x || y == info.start.y)
		mask = info.start.mask;
	DoPix(0, info.xorg + x, info.yorg + y);
	DoPix(2, info.xorgo - x, info.yorgo - y);
	if (arc->height & 1) {
		DoPix(1, info.xorgo - x, info.yorg + y);
		DoPix(3, info.xorg + x, info.yorgo - y);
	}
	for (i = 0; i < 4; i++) {
		seg = (startseg + i) & 3;
		pt = points + (seg * maxPts);
		if (seg & 1) {
			startPts[i] = pt;
			endPts[i] = arcPts[seg];
			deltas[i] = 1;
		} else {
			startPts[i] = arcPts[seg] - 1;
			endPts[i] = pt - 1;
			deltas[i] = -1;
		}
	}
	startPts[4] = startPts[0];
	endPts[4] = startPt;
	startPts[0] = startPt;
	if (startseg & 1) {
		if (startPts[4] != endPts[4])
			endPts[4]--;
		deltas[4] = 1;
	} else {
		if (startPts[0] > startPts[4])
			startPts[0]--;
		if (startPts[4] < endPts[4])
			endPts[4]--;
		deltas[4] = -1;
	}
	if (arc->angle2 < 0) {
		DDXPointPtr tmps, tmpe;
		int tmpd;

		tmpd = deltas[0];
		tmps = startPts[0] - tmpd;
		tmpe = endPts[0] - tmpd;
		startPts[0] = endPts[4] - deltas[4];
		endPts[0] = startPts[4] - deltas[4];
		deltas[0] = -deltas[4];
		startPts[4] = tmpe;
		endPts[4] = tmps;
		deltas[4] = -tmpd;
		tmpd = deltas[1];
		tmps = startPts[1] - tmpd;
		tmpe = endPts[1] - tmpd;
		startPts[1] = endPts[3] - deltas[3];
		endPts[1] = startPts[3] - deltas[3];
		deltas[1] = -deltas[3];
		startPts[3] = tmpe;
		endPts[3] = tmps;
		deltas[3] = -tmpd;
		tmps = startPts[2] - deltas[2];
		startPts[2] = endPts[2] - deltas[2];
		endPts[2] = tmps;
		deltas[2] = -deltas[2];
	}
	for (i = 0; i < 5 && startPts[i] == endPts[i]; i++);
	if (i == 5)
		return;
	pt = startPts[i];
	for (j = 4; startPts[j] == endPts[j]; j--);
	lastPt = endPts[j] - deltas[j];
	if (dinfo->haveLast &&
	    (pt->x == dinfo->endPt.x) && (pt->y == dinfo->endPt.y)) {
		startPts[i] += deltas[i];
	} else {
		dinfo->dashIndex = dinfo->dashIndexInit;
		dinfo->dashOffset = dinfo->dashOffsetInit;
	}
	if (!dinfo->skipStart && (info.startAngle != info.endAngle)) {
		dinfo->startPt = *pt;
		dinfo->haveStart = TRUE;
	} else if (!dinfo->skipLast && dinfo->haveStart &&
		 (lastPt->x == dinfo->startPt.x) &&
		 (lastPt->y == dinfo->startPt.y) &&
		 (lastPt != startPts[i]))
		endPts[j] = lastPt;
	if (info.startAngle != info.endAngle) {
		dinfo->haveLast = TRUE;
		dinfo->endPt = *lastPt;
	}
	dashRemaining = pGC->dash[dinfo->dashIndex] - dinfo->dashOffset;
	for (i = 0; i < 5; i++) {
		pt = startPts[i];
		lastPt = endPts[i];
		delta = deltas[i];
		while (pt != lastPt) {
			if (dinfo->dashIndex & 1) {
				pts = *oddPts;
				ptsdelta = -1;
			} else {
				pts = *evenPts;
				ptsdelta = 1;
			}
			while ((pt != lastPt) && --dashRemaining >= 0) {
				*pts = *pt;
				pts += ptsdelta;
				pt += delta;
			}
			if (dinfo->dashIndex & 1)
				*oddPts = pts;
			else
				*evenPts = pts;
			if (dashRemaining <= 0) {
				if (++(dinfo->dashIndex) == pGC->numInDashList)
					dinfo->dashIndex = 0;
				dashRemaining = pGC->dash[dinfo->dashIndex];
			}
		}
	}
	dinfo->dashOffset = pGC->dash[dinfo->dashIndex] - dashRemaining;
}

void
CreatorZeroPolyArc(DrawablePtr pDrawable, GCPtr pGC, int narcs, xArc *parcs)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	int maxPts = 0;
	register int n;
	register xArc *arc;
	register int i;
	register int j, k;
	DDXPointPtr points = NULL, pts, oddPts;
	int numPts = 0;
	DashInfo dinfo;
	FFBPtr pFfb;
	ffb_fbcPtr ffb;
	RegionPtr clip;
	int numRects, ppc;
	BoxPtr pbox;
	CreatorPrivGCPtr gcPriv;
	register int off = 0, c1, c2;
	register char *addrp = NULL;
	register int *ppt, pt, pix;
	
	gcPriv = CreatorGetGCPrivate (pGC);
	pFfb = GET_FFB_FROM_SCREEN(pDrawable->pScreen);
	ffb = pFfb->regs;
	clip = cfbGetCompositeClip(pGC);
	numRects = REGION_NUM_RECTS(clip);
	if (!numRects)
		return;
	if (pGC->lineStyle == LineSolid && numRects != 1) {
		miZeroPolyArc(pDrawable, pGC, narcs, parcs);
		return;
	}
	FFBLOG(("CreatorZeroPolyArc: ALU(%x) PMSK(%08x) narcs(%d)\n",
		pGC->alu, pGC->planemask, narcs));
	if (pGC->lineStyle == LineSolid)
		for (arc = parcs, i = narcs, j = 0; --i >= 0; arc++) {
			if (!miCanZeroArc(arc))
				miPolyArc(pDrawable, pGC, 1, arc);
			else
				j++;
		}
	else
		for (arc = parcs, i = narcs, j = 0; --i >= 0; arc++) {
			if (!miCanZeroArc(arc))
				miPolyArc(pDrawable, pGC, 1, arc);
			else {
				j++;
				if (arc->width > arc->height)
					n = arc->width + (arc->height >> 1);
				else
					n = arc->height + (arc->width >> 1);
				if (n > maxPts)
					maxPts = n;
			}
		}
	if (!j)
		return;

	if (pGC->lineStyle != LineSolid) {
		numPts = maxPts << 2;
		points = (DDXPointPtr)ALLOCATE_LOCAL(sizeof(DDXPointRec) * (numPts + (maxPts << 1)));
		if (!points) return;
		dinfo.haveStart = FALSE;
		dinfo.skipStart = FALSE;
		dinfo.haveLast = FALSE;
		dinfo.dashIndexInit = 0;
		dinfo.dashOffsetInit = 0;
		miStepDash((int)pGC->dashOffset, &dinfo.dashIndexInit,
			   (unsigned char *) pGC->dash, (int)pGC->numInDashList,
			   &dinfo.dashOffsetInit);
		off = *(int *)&pDrawable->x;
		off -= (off & 0x8000) << 1;
		if (pGC->depth == 8) {
			addrp = (char *)pFfb->sfb8r +
				(pDrawable->y << 11) + (pDrawable->x << 0);
		} else {
			addrp = (char *)pFfb->sfb32 +
				(pDrawable->y << 13) + (pDrawable->x << 2);
		}
		ppc = FFB_PPC_CS_VAR;
	} else
		ppc = FFB_PPC_CS_CONST;

        if(gcPriv->stipple == NULL) {
		FFB_ATTR_GC(pFfb, pGC, pWin,
			    ppc | FFB_PPC_APE_DISABLE,
			    FFB_DRAWOP_DOT);
	} else {
		unsigned int fbc;

		FFBSetStipple(pFfb, ffb, gcPriv->stipple,
			      ppc, FFB_PPC_CS_MASK);
		FFB_WRITE_PMASK(pFfb, ffb, pGC->planemask);
		FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_DOT);
		fbc = FFB_FBC_WIN(pWin);
		fbc = (fbc & ~FFB_FBC_XE_MASK) | FFB_FBC_XE_OFF;
		FFB_WRITE_FBC(pFfb, ffb, fbc);
	}
	if((ppc & FFB_PPC_CS_MASK) == FFB_PPC_CS_VAR)
		FFBWait(pFfb, ffb);

	for (arc = parcs, i = narcs; --i >= 0; arc++) {
		if (miCanZeroArc(arc)) {
			if (pGC->lineStyle == LineSolid)
				CreatorZeroArcPts(arc, pDrawable, pGC, REGION_RECTS(clip));
			else {
				pts = points;
				oddPts = &points[(numPts >> 1) - 1];
				dinfo.skipLast = i;
				CreatorZeroArcDashPts(pGC, arc, &dinfo,
						      oddPts + 1, maxPts, &pts, &oddPts);
				dinfo.skipStart = TRUE;
				n = pts - points;
				pbox = REGION_RECTS(clip);
				j = numRects;
				pix = pGC->fgPixel;
				if (pGC->depth == 8) {
					while (j--) {
						c1 = *(int *)&pbox->x1 - off;
						c2 = *(int *)&pbox->x2 - off - 0x00010001;
						for (ppt = (int *)points, k = n; --k >= 0; ) {
							pt = *ppt++;
							if (!(((pt - c1) | (c2 - pt)) & 0x80008000))
								*(unsigned char *)(addrp + ((pt << 11) & 0x3ff800) + ((pt >> 16) & 0x07ff)) = pix;
						}
						pbox++;
					}
				} else {
					while (j--) {
						c1 = *(int *)&pbox->x1 - off;
						c2 = *(int *)&pbox->x2 - off - 0x00010001;
						for (ppt = (int *)points, k = n; --k >= 0; ) {
							pt = *ppt++;
							if (!(((pt - c1) | (c2 - pt)) & 0x80008000))
								*(unsigned int *)(addrp + ((pt << 13) & 0xffe000) + ((pt >> 14) & 0x1ffc)) = pix;
						}
						pbox++;
					}
				}
				if (pGC->lineStyle != LineDoubleDash)
					continue;
				if ((pGC->fillStyle == FillSolid) || (pGC->fillStyle == FillStippled))
					pix = pGC->bgPixel;
				pts = &points[numPts >> 1];
				oddPts++;
				n = pts - oddPts;
				pbox = REGION_RECTS(clip);
				j = numRects;
				if (pGC->depth == 8) {
					while (j--) {
						c1 = *(int *)&pbox->x1 - off;
						c2 = *(int *)&pbox->x2 - off - 0x00010001;
						for (ppt = (int *)oddPts, k = n; --k >= 0; ) {
							pt = *ppt++;
							if (!(((pt - c1) | (c2 - pt)) & 0x80008000))
								*(unsigned char *)(addrp + ((pt << 11) & 0x3ff800) + ((pt >> 16) & 0x07ff)) = pix;
						}
						pbox++;
					}
				} else {
					while (j--) {
						c1 = *(int *)&pbox->x1 - off;
						c2 = *(int *)&pbox->x2 - off - 0x00010001;
						for (ppt = (int *)oddPts, k = n; --k >= 0; ) {
							pt = *ppt++;
							if (!(((pt - c1) | (c2 - pt)) & 0x80008000))
								*(unsigned int *)(addrp + ((pt << 13) & 0xffe000) + ((pt >> 14) & 0x1ffc)) = pix;
						}
						pbox++;
					}
				}
			}
		}
	}
	if (pGC->lineStyle != LineSolid) {
		DEALLOCATE_LOCAL(points);
	} else {
		pFfb->rp_active = 1;
		FFBSync(pFfb, ffb);
	}
}
