/*
 * Acceleration for the Creator and Creator3D framebuffer - Polygon rops.
 *
 * Copyright (C) 1999 Jakub Jelinek (jakub@redhat.com)
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
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_plygon.c,v 1.2 2000/05/23 04:47:45 dawes Exp $ */

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

void
CreatorFillPolygon (DrawablePtr pDrawable, GCPtr pGC, int shape, int mode, int count, DDXPointPtr ppt)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	CreatorPrivGCPtr gcPriv = CreatorGetGCPrivate (pGC);
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pGC->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
	BoxRec box;
	int lx, rx, ty, by;
	int t, b, i, j, k, l, tt;
	int xy[12] FFB_ALIGN64;
	int xOrg, yOrg;

	FFBLOG(("CreatorFillPolygon: ALU(%x) PMSK(%08x) shape(%d) mode(%d) count(%d)\n",
		pGC->alu, pGC->planemask, shape, mode, count));
	if (count < 3)
		return;
	
	if (shape != Convex && count > 3) {
		miFillPolygon (pDrawable, pGC, shape, mode, count, ppt);
		return;
	}
	
	xOrg = pDrawable->x;
	yOrg = pDrawable->y;

	ppt->x += xOrg;
	ppt->y += yOrg;	
	lx = ppt->x;
	rx = ppt->x;
	ty = ppt->y;
	by = ppt->y;
	t = b = 0;
	tt = 1;
	for (i = 1; i < count; i++) {
		if (mode == CoordModeOrigin) {
			ppt[i].x += xOrg;
			ppt[i].y += yOrg;
		} else {
			ppt[i].x += ppt[i-1].x;
			ppt[i].y += ppt[i-1].y;
		}
		if (ppt[i].x < lx)
			lx = ppt[i].x;
		if (ppt[i].x > rx)
			rx = ppt[i].x;
		if (ppt[i].y < ty) {
			ty = ppt[i].y;
			t = i;
			tt = 1;
		} else if (ppt[i].y == ty)
			tt++;
		if (ppt[i].y > by) {
			by = ppt[i].y;
			b = i;
		}
	}
	if (tt > 2) {
		miFillConvexPoly(pDrawable, pGC, count, ppt);
		return;
	} else if (tt == 2) {
		i = t - 1;
		if (i < 0)
			i = count - 1;
		if (ppt[i].y == ppt[t].y)
			t = i;
	}
	box.x1 = lx;
	box.x2 = rx + 1;
	box.y1 = ty;
	box.y2 = by + 1;
	
	switch (RECT_IN_REGION(pGC->pScreen, cfbGetCompositeClip(pGC), &box)) {
	case rgnPART:
		miFillConvexPoly(pDrawable, pGC, count, ppt);
	case rgnOUT:
		return;
	}
	if(gcPriv->stipple == NULL) {
		FFB_ATTR_GC(pFfb, pGC, pWin,
			    FFB_PPC_APE_DISABLE | FFB_PPC_CS_CONST,
			    FFB_DRAWOP_POLYGON);
	} else {
		unsigned int fbc;

		FFBSetStipple(pFfb, ffb, gcPriv->stipple,
			      FFB_PPC_CS_CONST, FFB_PPC_CS_MASK);
		FFB_WRITE_PMASK(pFfb, ffb, pGC->planemask);
		FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_POLYGON);
		fbc = FFB_FBC_WIN(pWin);
		fbc = (fbc & ~FFB_FBC_XE_MASK) | FFB_FBC_XE_OFF;
		FFB_WRITE_FBC(pFfb, ffb, fbc);
	}	
	xy[0] = ppt[t].y;
	xy[1] = ppt[t].x;
	j = t + 1;
	if (j == count) j = 0;
	xy[2] = ppt[j].y;
	xy[3] = ppt[j].x;
	j = t + 2;
	if (j >= count)
		j -= count;
	for (i = 2 * count - 4; i; i -= k) {
		b = 2;
		for (k = 0; k < i && k < 8; k+=2) {
			xy[4 + k] = ppt[j].y;
			xy[4 + k + 1] = ppt[j].x;
			if (xy[4 + k] > xy[b])
				b = 4 + k;
			j++; if (j == count) j = 0;
		}
		FFBFifo(pFfb, 4 + k);
		for (l = 0; l < b - 2; l+=2)
			FFB_WRITE64P(&ffb->by, &xy[l]);
		FFB_WRITE64P(&ffb->bh, &xy[l]);
		for (l+=2; l <= k; l+=2)
			FFB_WRITE64P(&ffb->by, &xy[l]);
		FFB_WRITE64P(&ffb->ebyi, &xy[l]);
		xy[2] = xy[l];
		xy[3] = xy[l+1];
	}
	pFfb->rp_active = 1;
	FFBSync(pFfb, ffb);
}
