/*
 * Acceleration for the Creator and Creator3D framebuffer - Point rops.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_point.c,v 1.2 2000/05/23 04:47:45 dawes Exp $ */

#include "ffb.h"
#include "ffb_regs.h"
#include "ffb_rcache.h"
#include "ffb_fifo.h"
#include "ffb_loops.h"

#include "pixmapstr.h"
#include "scrnintstr.h"

#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb32.h"

void
CreatorPolyPoint(DrawablePtr pDrawable, GCPtr pGC, int mode,
		 int npt, xPoint *pptInit)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pGC->pScreen);
	WindowPtr pWin = (WindowPtr) pDrawable;
	ffb_fbcPtr ffb = pFfb->regs;
	RegionPtr clip;
	int numRects;
	register int off, c1, c2;
	register char *addrp;
	register int *ppt, pt, i;
	BoxPtr pbox;
	xPoint *pptPrev;

	FFBLOG(("CreatorPolyPoint: ALU(%x) PMSK(%08x) mode(%d) npt(%d)\n",
		pGC->alu, pGC->planemask, mode, npt));

	if (pGC->alu == GXnoop)
		return;

	clip = cfbGetCompositeClip(pGC);
	numRects = REGION_NUM_RECTS(clip);
	off = *(int *)&pDrawable->x;
	off -= (off & 0x8000) << 1;
	if (mode == CoordModePrevious && npt > 1) {
		for (pptPrev = pptInit + 1, i = npt - 1; --i >= 0; pptPrev++) {
			pptPrev->x += pptPrev[-1].x;
			pptPrev->y += pptPrev[-1].y;
		}
	}

	FFB_ATTR_GC(pFfb, pGC, pWin,
		    FFB_PPC_APE_DISABLE | FFB_PPC_CS_CONST,
		    pFfb->drawop_cache);
	FFBWait(pFfb, ffb);

	if (pGC->depth == 8) {
		addrp = (char *)pFfb->sfb8r + (pDrawable->y << 11) + (pDrawable->x << 0);
		pbox = REGION_RECTS(clip);
		while (numRects--) {
			c1 = *(int *)&pbox->x1 - off;
			c2 = *(int *)&pbox->x2 - off - 0x00010001;
			for (ppt = (int *)pptInit, i = npt; --i >= 0; ) {
				pt = *ppt++;
				if (!(((pt - c1) | (c2 - pt)) & 0x80008000))
					*(unsigned char *)(addrp + ((pt << 11) & 0x3ff800) +
							   ((pt >> 16) & 0x07ff)) = 0;
			}
			pbox++;
		}
	} else {
		addrp = (char *)pFfb->sfb32 + (pDrawable->y << 13) + (pDrawable->x << 2);
		pbox = REGION_RECTS(clip);
		while (numRects--) {
			c1 = *(int *)&pbox->x1 - off;
			c2 = *(int *)&pbox->x2 - off - 0x00010001;
			for (ppt = (int *)pptInit, i = npt; --i >= 0; ) {
				pt = *ppt++;
				if (!(((pt - c1) | (c2 - pt)) & 0x80008000))
					*(unsigned int *)(addrp + ((pt << 13) & 0xffe000) +
							  ((pt >> 14) & 0x1ffc)) = 0;
			}
			pbox++;
		}
	}
}
