/*
 * Acceleration for the Leo (ZX) framebuffer - Tiled/stippled rectangle filling.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunleo/leo_frectsp.c,v 1.2 2000/05/23 04:47:47 dawes Exp $ */

#define PSZ 32

#include "leo.h"
#include "leo_regs.h"

#include "pixmapstr.h"
#include "scrnintstr.h"

#include "cfb.h"
#include "cfbmskbits.h"

void
LeoPolyFillStippledRect(DrawablePtr pDrawable, GCPtr pGC, int nrectFill, xRectangle *prectInit)
{
	LeoPtr pLeo = LeoGetScreenPrivate (pDrawable->pScreen);
	LeoPrivGCPtr	gcPriv = LeoGetGCPrivate (pGC);
	LeoCommand0 	*lc0 = pLeo->lc0;
	LeoDraw		*ld0 = pLeo->ld0;
	xRectangle	*prect;
	RegionPtr	prgnClip;
	register BoxPtr	pbox;
	BoxPtr		pextent;
	int		n;
	int		xorg, yorg;
	unsigned char	*fb;
	unsigned int	*dst, *dline, *src, *srcstart, *srcend;
    
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
	
	if ((gcPriv->stipple->alu & 0xf) != GXcopy)
		ld0->rop = leoRopTable[gcPriv->stipple->alu & 0xf];
	if (pGC->planemask != 0xffffff)
		ld0->planemask = pGC->planemask;
	ld0->fg = gcPriv->stipple->fg;
	lc0->addrspace = LEO_ADDRSPC_FONT_OBGR;
	if (gcPriv->stipple->alu & 0x80) {
		lc0->fontt = 1;
	} else {
		lc0->fontt = 0;
		ld0->bg = gcPriv->stipple->bg;
	}
	fb = (unsigned char *)pLeo->fb;
	srcstart = &gcPriv->stipple->bits[0];
	srcend = &gcPriv->stipple->bits[32];

	if (REGION_NUM_RECTS(prgnClip) == 1) {
		int x1, y1, x2, y2;
		int x, y, xx, yy, w, h;
		int i, j, sw, sm, ew, em, s;

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
			if (x < x1) x = x1;
			if (y < y1) y = y1;
			if (xx > x2) xx = x2;
			if (yy > y2) yy = y2;
			prect++;
			if (x >= xx) continue;
			if (y >= yy) continue;
			prect++;
			w = xx - x;
			h = yy - y;
			if (x & 31) {
				sw = 32 - (x & 31);
				sm = 0xffffffff >> (x & 31);
			} else {
				sw = 0;
				sm = 0xffffffff;
			}
			dline = (unsigned int *)(fb + (y << 13) + ((x & ~31) << 2));
			src = srcstart + (y & 31);
			w -= sw;
			if (w <= 0) {
				if (w)
					sm &= 0xffffffff << (32 - (w & 31));
				lc0->fontmsk = sm;
				
				for (i = 0; i < h; i++) {
					s = *src++;
					*dline = s;
					if (src == srcend)
						src = srcstart;
					dline += 2048;
				}
			} else {
				ew = w & 31;
				em = 0xffffffff << (32 - ew);
				w &= ~31;
				
				if (!sw && !ew)
					lc0->fontmsk = 0xffffffff;
				for (i = 0; i < h; i++) {
					s = *src++;
					dst = dline;
					if (sw) {
						lc0->fontmsk = sm;
						*dst = s;
						dst += 32;
						lc0->fontmsk = 0xffffffff;
					} else if (ew)
						lc0->fontmsk = 0xffffffff;
					for (j = 0; j < w; j += 32) {
						*dst = s;
						dst += 32;
					}
					if (ew) {
						lc0->fontmsk = em;
						*dst = s;
					}
					if (src == srcend)
						src = srcstart;
					dline += 2048;
				}
			}
		}
	} else {
		int x1, y1, x2, y2, bx1, by1, bx2, by2;
		int x, y, w, h;
		int i, j, sw, sm, ew, em, s;

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
					if (x & 31) {
						sw = 32 - (x & 31);
						sm = 0xffffffff >> (x & 31);
					} else {
						sw = 0;
						sm = 0xffffffff;
					}
					dline = (unsigned int *)(fb + (y << 13) + ((x & ~31) << 2));
					src = srcstart + (y & 31);
					w -= sw;
					if (w <= 0) {
						w += 32;
						if (w != 32)
							sm &= 0xffffffff << (32 - (w & 31));
						lc0->fontmsk = sm;
				
						for (i = 0; i < h; i++) {
							s = *src++;
							*dline = s;
							if (src == srcend)
								src = srcstart;
							dline += 2048;
						}
					} else {
						ew = w & 31;
						em = 0xffffffff << (32 - ew);
						w &= ~31;
				
						if (!sw && !ew)
							lc0->fontmsk = 0xffffffff;
		
						for (i = 0; i < h; i++) {
							s = *src++;
							dst = dline;
							if (sw) {
								lc0->fontmsk = sm;
								*dst = s;
								dst += 32;
								lc0->fontmsk = 0xffffffff;
							} else if (ew)
								lc0->fontmsk = 0xffffffff;
							for (j = 0; j < w; j += 32) {
								*dst = s;
								dst += 32;
							}
							if (ew) {
								lc0->fontmsk = em;
								*dst = s;
							}
							if (src == srcend)
								src = srcstart;
							dline += 2048;
						}
					}
				}
			}
		}
	}
	
	if (pGC->alu != GXcopy)
		ld0->rop = LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW;
	if (pGC->planemask != 0xffffff)
		ld0->planemask = 0xffffff;
	lc0->addrspace = LEO_ADDRSPC_OBGR;
}
