/*
 * Acceleration for the Leo (ZX) framebuffer - Bit-blit copies.
 *
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
 * JAKUB JELINEK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunleo/leo_bcopy.c,v 1.1 2000/05/18 23:21:39 dawes Exp $ */

#define PSZ 32

#include "leo.h"
#include "leo_regs.h"

#include "pixmapstr.h"
#include "scrnintstr.h"

#include "cfb.h"

void
LeoDoBitblt(DrawablePtr pSrc, DrawablePtr pDst, int alu, RegionPtr prgnDst,
		DDXPointPtr pptSrc, unsigned long planemask)
{
	LeoPtr pLeo = LeoGetScreenPrivate (pDst->pScreen);
	LeoCommand0 *lc0 = pLeo->lc0;
	LeoDraw *ld0 = pLeo->ld0;
	BoxPtr pboxTmp;
	DDXPointPtr pptTmp;
	int nbox;
	BoxPtr pboxNext, pboxBase, pbox;

	pbox = REGION_RECTS(prgnDst);
	nbox = REGION_NUM_RECTS(prgnDst);

	pptTmp = pptSrc;
	pboxTmp = pbox;

	ld0->rop = leoRopTable[alu];

	if (pptSrc->y < pbox->y1) {
		if (pptSrc->x < pbox->x1) {
			/* reverse order of bands and rects in each band */
			pboxTmp=pbox+nbox;
                        pptTmp=pptSrc+nbox;

			while (nbox--){
				pboxTmp--;
				pptTmp--;
				if (pptTmp->y <= pboxTmp->y2) {
					lc0->extent = 0x80000000 | (pboxTmp->x2 - pboxTmp->x1 - 1) |
						      ((pboxTmp->y2 - pboxTmp->y1 - 1) << 11);
					lc0->src = (pptTmp->x + pboxTmp->x2 - pboxTmp->x1 - 1) |
						   ((pptTmp->y + pboxTmp->y2 - pboxTmp->y1 - 1) << 11);
					lc0->copy = (pboxTmp->x2 - 1) | ((pboxTmp->y2 - 1) << 11);
				} else {
					lc0->extent = (pboxTmp->x2 - pboxTmp->x1 - 1) |
						      ((pboxTmp->y2 - pboxTmp->y1 - 1) << 11);
					lc0->src = pptTmp->x | (pptTmp->y << 11);
					lc0->copy = pboxTmp->x1 | (pboxTmp->y1 << 11);
				}
				while (lc0->csr & LEO_CSR_BLT_BUSY);
			}
		} else {
			/* keep ordering in each band, reverse order of bands */
			pboxBase = pboxNext = pbox+nbox-1;

			while (pboxBase >= pbox) {			/* for each band */

				/* find first box in band */
				while (pboxNext >= pbox &&
				       pboxBase->y1 == pboxNext->y1)
					pboxNext--;
		
				pboxTmp = pboxNext+1;			/* first box in band */
				pptTmp = pptSrc + (pboxTmp - pbox);	/* first point in band */
		
				while (pboxTmp <= pboxBase) {		/* for each box in band */
					if (pptTmp->y <= pboxTmp->y2) {
						lc0->extent = 0x80000000 | (pboxTmp->x2 - pboxTmp->x1 - 1) |
							      ((pboxTmp->y2 - pboxTmp->y1 - 1) << 11);
						lc0->src = (pptTmp->x + pboxTmp->x2 - pboxTmp->x1 - 1) |
							   ((pptTmp->y + pboxTmp->y2 - pboxTmp->y1 - 1) << 11);
						lc0->copy = (pboxTmp->x2 - 1) | ((pboxTmp->y2 - 1) << 11);
					} else {
						lc0->extent = (pboxTmp->x2 - pboxTmp->x1 - 1) |
							      ((pboxTmp->y2 - pboxTmp->y1 - 1) << 11);
						lc0->src = pptTmp->x | (pptTmp->y << 11);
						lc0->copy = pboxTmp->x1 | (pboxTmp->y1 << 11);
					}
					while (lc0->csr & LEO_CSR_BLT_BUSY);
					++pboxTmp;
					++pptTmp;	
				}
				pboxBase = pboxNext;
			
			}
		}
	} else {
		if (pptSrc->x < pbox->x1) {
			/* reverse order of rects in each band */

			pboxBase = pboxNext = pbox;

			while (pboxBase < pbox+nbox) { /* for each band */

				/* find last box in band */
				while (pboxNext < pbox+nbox &&
				       pboxNext->y1 == pboxBase->y1)
					pboxNext++;

				pboxTmp = pboxNext;			/* last box in band */
				pptTmp = pptSrc + (pboxTmp - pbox);	/* last point in band */

				if (pptSrc->y == pbox->y1) {
					while (pboxTmp != pboxBase) {		/* for each box in band */
						--pboxTmp;
						--pptTmp;
						lc0->extent = 0x80000000 | (pboxTmp->x2 - pboxTmp->x1 - 1) |
							      ((pboxTmp->y2 - pboxTmp->y1 - 1) << 11);
						lc0->src = (pptTmp->x + pboxTmp->x2 - pboxTmp->x1 - 1) |
							   ((pptTmp->y + pboxTmp->y2 - pboxTmp->y1 - 1) << 11);
						lc0->copy = (pboxTmp->x2 - 1) | ((pboxTmp->y2 - 1) << 11);
						while (lc0->csr & LEO_CSR_BLT_BUSY);
					}
				} else {
					while (pboxTmp != pboxBase) {		/* for each box in band */
						--pboxTmp;
						--pptTmp;
						lc0->extent = (pboxTmp->x2 - pboxTmp->x1 - 1) |
							      ((pboxTmp->y2 - pboxTmp->y1 - 1) << 11);
						lc0->src = pptTmp->x | (pptTmp->y << 11);
						lc0->copy = pboxTmp->x1 | (pboxTmp->y1 << 11);
						while (lc0->csr & LEO_CSR_BLT_BUSY);
					}
				}
				pboxBase = pboxNext;
			}
		} else {
			while (nbox--) {
				lc0->extent = (pboxTmp->x2 - pboxTmp->x1 - 1) |
					      ((pboxTmp->y2 - pboxTmp->y1 - 1) << 11);
				lc0->src = pptTmp->x | (pptTmp->y << 11);
				lc0->copy = pboxTmp->x1 | (pboxTmp->y1 << 11);
				while (lc0->csr & LEO_CSR_BLT_BUSY);
				pboxTmp++;
				pptTmp++;
			}
		}
	}

	ld0->rop = LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW;
}

RegionPtr
LeoCopyArea(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable,
		GCPtr pGC, int srcx, int srcy, int width, int height, int dstx, int dsty)
{
	if (pSrcDrawable->type != DRAWABLE_WINDOW)
		return cfbCopyArea (pSrcDrawable, pDstDrawable,
				    pGC, srcx, srcy, width, height, dstx, dsty);
	return cfbBitBlt (pSrcDrawable, pDstDrawable,
			  pGC, srcx, srcy, width, height, dstx, dsty, (void (*)())LeoDoBitblt, 0);
}
