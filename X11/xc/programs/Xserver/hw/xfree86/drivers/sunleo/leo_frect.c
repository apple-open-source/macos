/*
 * Acceleration for the Leo (ZX) framebuffer - Rectangle filling.
 *
 * Copyright (C) 1999, 2000 Jakub Jelinek (jakub@redhat.com)
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
 * JAKUB JELINEK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunleo/leo_frect.c,v 1.2 2000/05/23 04:47:47 dawes Exp $ */

#define PSZ 32

#include "leo.h"
#include "leo_regs.h"

#include "pixmapstr.h"
#include "scrnintstr.h"

#include "cfb.h"
#include "cfbmskbits.h"

void
LeoPolyFillRect(DrawablePtr pDrawable, GCPtr pGC, int nrectFill, xRectangle *prectInit)
{
	LeoPtr pLeo = LeoGetScreenPrivate (pDrawable->pScreen);
	LeoCommand0 	*lc0 = pLeo->lc0;
	LeoDraw		*ld0 = pLeo->ld0;
	xRectangle	*prect;
	RegionPtr	prgnClip;
	register BoxPtr	pbox;
	BoxPtr		pextent;
	int		n;
	int		xorg, yorg;
    
	/* No garbage please. */
	if(nrectFill <= 0)
		return;

	prgnClip = cfbGetCompositeClip(pGC);

	prect = prectInit;
	xorg = pDrawable->x;
	yorg = pDrawable->y;
	if (xorg || yorg) {
		prect = prectInit;
		n = nrectFill;
		while(n--) {
			prect->x += xorg;
			prect->y += yorg;
			prect++;
		}
	}

	prect = prectInit;
	
	if (pGC->alu != GXcopy)
		ld0->rop = leoRopTable[pGC->alu];
	if (pGC->planemask != 0xffffff)
		ld0->planemask = pGC->planemask;
	ld0->fg = pGC->fgPixel;

	if (REGION_NUM_RECTS(prgnClip) == 1) {
		int x1, y1, x2, y2;
		int x, y, xx, yy;

		pextent = REGION_RECTS(prgnClip);
		x1 = pextent->x1;
		y1 = pextent->y1;
		x2 = pextent->x2;
		y2 = pextent->y2;
		while (nrectFill--) {
			x = prect->x;
			y = prect->y;
			xx = x + prect->width;
			yy = y + prect->height;
			if (x < x1)
				x = x1;
			if (y < y1)
				y = y1;
			prect++;
			if (xx > x2) xx = x2;
			if (yy > y2) yy = y2;
			if (x >= xx) continue;
			if (y >= yy) continue;

			lc0->extent = (xx - x - 1) | ((yy - y - 1) << 11);
			lc0->fill = x | (y << 11);
			while (lc0->csr & LEO_CSR_BLT_BUSY);
		}
	} else {
		int x1, y1, x2, y2, bx1, by1, bx2, by2;
		int x, y, w, h;

		pextent = REGION_EXTENTS(pGC->pScreen, prgnClip);
		x1 = pextent->x1;
		y1 = pextent->y1;
		x2 = pextent->x2;
		y2 = pextent->y2;
		while (nrectFill--) {
			if ((bx1 = prect->x) < x1)
				bx1 = x1;
    
			if ((by1 = prect->y) < y1)
				by1 = y1;
    
			bx2 = (int) prect->x + (int) prect->width;
			if (bx2 > x2)
				bx2 = x2;
    
			by2 = (int) prect->y + (int) prect->height;
			if (by2 > y2)
				by2 = y2;

			prect++;
    
			if (bx1 >= bx2 || by1 >= by2)
				continue;
    
			n = REGION_NUM_RECTS (prgnClip);
			pbox = REGION_RECTS(prgnClip);
    
			/* clip the rectangle to each box in the clip region
			   this is logically equivalent to calling Intersect()
			 */
			while(n--) {
				x = max(bx1, pbox->x1);
				y = max(by1, pbox->y1);
				w = min(bx2, pbox->x2) - x;
				h = min(by2, pbox->y2) - y;
				pbox++;

				/* see if clipping left anything */
				if (w > 0 && h > 0) {
					lc0->extent = (w - 1) | ((h - 1) << 11);
					lc0->fill = x | (y << 11);
			
					while (lc0->csr & LEO_CSR_BLT_BUSY);
				}
			}
		}
	}
	
	if (pGC->alu != GXcopy)
		ld0->rop = LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW;
	if (pGC->planemask != 0xffffff)
		ld0->planemask = 0xffffff;
}

void
LeoPolyFillRect1Rect(DrawablePtr pDrawable, GCPtr pGC, int nrectFill, xRectangle *prectInit)
{
	LeoPtr pLeo = LeoGetScreenPrivate (pDrawable->pScreen);
	LeoCommand0 	*lc0 = pLeo->lc0;
	LeoDraw		*ld0 = pLeo->ld0;
	xRectangle	*prect;
	RegionPtr	prgnClip;
	BoxPtr		pextent;
	int		n;
	int		xorg, yorg;
	int		x1, y1, x2, y2;
	int		x, y, xx, yy;
    
	/* No garbage please. */
	if(nrectFill <= 0)
		return;

	prgnClip = cfbGetCompositeClip(pGC);

	prect = prectInit;
	xorg = pDrawable->x;
	yorg = pDrawable->y;
	if (xorg || yorg) {
		prect = prectInit;
		n = nrectFill;
		while(n--) {
			prect->x += xorg;
			prect->y += yorg;
			prect++;
		}
	}

	prect = prectInit;
	
	if (pGC->alu != GXcopy)
		ld0->rop = leoRopTable[pGC->alu];
	if (pGC->planemask != 0xffffff)
		ld0->planemask = pGC->planemask;
	ld0->fg = pGC->fgPixel;

	pextent = REGION_RECTS(prgnClip);
	x1 = pextent->x1;
	y1 = pextent->y1;
	x2 = pextent->x2;
	y2 = pextent->y2;
	while (nrectFill--) {
		x = prect->x;
		y = prect->y;
		xx = x + prect->width;
		yy = y + prect->height;
		if (x < x1)
			x = x1;
		if (y < y1)
			y = y1;
		prect++;
		if (xx > x2) xx = x2;
		if (x >= xx) continue;
		if (yy > y2) yy = y2;
		if (y >= yy) continue;

		lc0->extent = (xx - x - 1) | ((yy - y - 1) << 11);
		lc0->fill = x | (y << 11);
		while (lc0->csr & LEO_CSR_BLT_BUSY);
	}
	
	if (pGC->alu != GXcopy)
		ld0->rop = LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW;
	if (pGC->planemask != 0xffffff)
		ld0->planemask = 0xffffff;
}
