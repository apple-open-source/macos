/*
 * Acceleration for the Leo (ZX) framebuffer - Fill spans.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunleo/leo_fspans.c,v 1.1 2000/05/18 23:21:40 dawes Exp $ */

#define PSZ 32

#include "leo.h"

#include "pixmapstr.h"
#include "scrnintstr.h"

#include "cfb.h"
#include "mi.h"
#include "mispans.h"

void
LeoFillSpansSolid (DrawablePtr pDrawable, GCPtr pGC,
		   int n, DDXPointPtr ppt,
		   int *pwidth, int fSorted)
{
	LeoPtr pLeo = LeoGetScreenPrivate (pGC->pScreen);
	LeoCommand0 *lc0 = pLeo->lc0;
	LeoDraw *ld0 = pLeo->ld0;
	int numRects, *pwidthFree;
	DDXPointPtr pptFree;
	RegionPtr clip;
	unsigned char *fb;
	int fg;
	int cx1 = 0, cy1 = 0, cx2 = 0, cy2 = 0;

	clip = cfbGetCompositeClip(pGC);
	numRects = REGION_NUM_RECTS(clip);
	
	if (!numRects)
		return;
		
	if (numRects == 1) {
		cx1 = clip->extents.x1;
		cx2 = clip->extents.x2;
		cy1 = clip->extents.y1;
		cy2 = clip->extents.y2;
	} else {
		int nTmp = n * miFindMaxBand(clip);

		pwidthFree = (int *)ALLOCATE_LOCAL(nTmp * sizeof(int));
		pptFree = (DDXPointRec *)ALLOCATE_LOCAL(nTmp * sizeof(DDXPointRec));
		if (!pptFree || !pwidthFree) {
			if (pptFree) DEALLOCATE_LOCAL(pptFree);
			if (pwidthFree) DEALLOCATE_LOCAL(pwidthFree);
			return;
		}
		n = miClipSpans(clip,
				ppt, pwidth, n,
				pptFree, pwidthFree, fSorted);
		pwidth = pwidthFree;
		ppt = pptFree;
	}
	
	if (pGC->alu != GXcopy)
		ld0->rop = leoRopTable[pGC->alu];
	if (pGC->planemask != 0xffffff)
		ld0->planemask = pGC->planemask;
	ld0->fg = fg = pGC->fgPixel;
	fb = (unsigned char *)pLeo->fb;
	
	while (n--) {
		int x, y, w;
		unsigned int *fbf;
		
		w = *pwidth++;
		x = ppt->x;
		y = ppt->y;
		ppt++;

		if (numRects == 1) {
			if (y < cy1 || y >= cy2) continue;
			if (x < cx1) {
				w -= (cx1 - x);
				if (w <= 0) continue;
				x = cx1;
			}
			if (x + w > cx2) {
				if (x >= cx2) continue;
				w = cx2 - x;
			}
		}
		
		if (w > 12) {
			lc0->extent = w - 1;
			lc0->fill = (y << 11) | x;
			while (lc0->csr & LEO_CSR_BLT_BUSY);
		} else {
			fbf = (unsigned int *)(fb + (y << 13) + (x << 2));
			while (w--)
				*fbf++ = fg;
		}
	}
	
	if (numRects != 1) {
		DEALLOCATE_LOCAL(pptFree);
		DEALLOCATE_LOCAL(pwidthFree);
	}
	if (pGC->alu != GXcopy)
		ld0->rop = LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW;
	if (pGC->planemask != 0xffffff)
		ld0->planemask = 0xffffff;
}
