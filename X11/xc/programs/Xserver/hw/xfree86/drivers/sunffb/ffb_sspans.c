/*
 * Acceleration for the Creator and Creator3D framebuffer - Set spans.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_sspans.c,v 1.2 2000/05/23 04:47:45 dawes Exp $ */

#include "ffb.h"
#include "ffb_regs.h"
#include "ffb_rcache.h"
#include "ffb_fifo.h"

#include "pixmapstr.h"
#include "scrnintstr.h"

#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb32.h"

/* PPC does all of the planemask and ROP'ing on the written pixels,
 * and we know various other things are constant, so this is easy.
 */
static void
CreatorSetScanline(int y, int xOrigin, int xStart, int xEnd,
		   unsigned int *_psrc, char *sfb, int depth)
{
	if (depth == 8) {
		unsigned char *psrc = (unsigned char *)_psrc;
		unsigned char *pdst = (unsigned char *)(sfb +
							(y << 11) +
							(xStart << 0));
		int w = xEnd - xStart;

		psrc += (xStart - xOrigin);
		while(w--)
			*pdst++ = *psrc++;
	} else {
		unsigned int *psrc = (unsigned int *)_psrc;
		unsigned int *pdst = (unsigned int *)(sfb +
						      (y << 13) +
						      (xStart << 2));
		int w = xEnd - xStart;

		psrc += (xStart - xOrigin);
		while(w--)
			*pdst++ = *psrc++;
	}
}

void
CreatorSetSpans(DrawablePtr pDrawable, GCPtr pGC, char *pcharsrc,
		DDXPointPtr ppt, int *pwidth, int nspans, int fSorted)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
	unsigned int *psrc = (unsigned int *)pcharsrc;
	BoxPtr pbox, pboxLast, pboxTest;
	DDXPointPtr pptLast;
	RegionPtr prgnDst;
	char *addrp;
	int xStart, xEnd, yMax;

	if(pDrawable->type != DRAWABLE_WINDOW) {
		if (pDrawable->bitsPerPixel == 8)
			cfbSetSpans(pDrawable, pGC, pcharsrc, ppt,
				    pwidth, nspans, fSorted);
		else
			cfb32SetSpans(pDrawable, pGC, pcharsrc, ppt,
				      pwidth, nspans, fSorted);
		return;
	}
	FFBLOG(("CreatorSetSpans: ALU(%x) PMSK(%08x) nspans(%d) fsorted(%d)\n",
		pGC->alu, pGC->planemask, nspans, fSorted));
	if (pGC->alu == GXnoop)
		return;

	/* Get SFB ready. */
	FFB_ATTR_SFB_VAR_WIN(pFfb, pGC->planemask, pGC->alu, pWin);
	FFBWait(pFfb, ffb);

	if (pGC->depth == 8)
		addrp = (char *) pFfb->sfb8r;
	else
		addrp = (char *) pFfb->sfb32;

	yMax = (int) pDrawable->y + (int) pDrawable->height;
	prgnDst = cfbGetCompositeClip(pGC);
	pbox = REGION_RECTS(prgnDst);
	pboxLast = pbox + REGION_NUM_RECTS(prgnDst);
	pptLast = ppt + nspans;
	if(fSorted) {
		pboxTest = pbox;
		while(ppt < pptLast) {
			pbox = pboxTest;
			if(ppt->y >= yMax)
				break;
			while(pbox < pboxLast) {
				if(pbox->y1 > ppt->y) {
					break;
				} else if(pbox->y2 <= ppt->y) {
					pboxTest = ++pbox;
					continue;
				} else if(pbox->x1 > ppt->x + *pwidth) {
					break;
				} else if(pbox->x2 <= ppt->x) {
					pbox++;
					continue;
				}
				xStart = max(pbox->x1, ppt->x);
				xEnd = min(ppt->x + *pwidth, pbox->x2);
				CreatorSetScanline(ppt->y, ppt->x, xStart, xEnd,
						   psrc, addrp, pGC->depth);
				if(ppt->x + *pwidth <= pbox->x2)
					break;
				else
					pbox++;
			}
			ppt++;
			psrc += *pwidth++;
		}
	} else {
		while(ppt < pptLast) {
			if(ppt->y >= 0 && ppt->y < yMax) {
				for(pbox = REGION_RECTS(prgnDst); pbox < pboxLast; pbox++) {
					if(pbox->y1 > ppt->y) {
						break;
					} else if(pbox->y2 <= ppt->y) {
						pbox++;
						break;
					}
					if(pbox->x1 <= ppt->x + *pwidth &&
					   pbox->x2 > ppt->x) {
						xStart = max(pbox->x1, ppt->x);
						xEnd = min(pbox->x2, ppt->x + *pwidth);
						CreatorSetScanline(ppt->y, ppt->x,
								   xStart, xEnd,
								   psrc, addrp, pGC->depth);
					}
				}
			}
			ppt++;
			psrc += *pwidth++;
		}
	}
}
