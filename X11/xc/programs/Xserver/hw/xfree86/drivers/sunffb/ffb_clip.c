/*
 * Acceleration for the Creator and Creator3D framebuffer - clip setting.
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
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_clip.c,v 1.1 2000/05/18 23:21:36 dawes Exp $ */

#include "ffb.h"
#include "ffb_regs.h"
#include "ffb_rcache.h"
#include "ffb_fifo.h"
#include "ffb_clip.h"

unsigned int
FFBSetClip(FFBPtr pFfb, ffb_fbcPtr ffb, RegionPtr pClip, int numRects)
{
	unsigned int xy1, xy2;
		
	if (numRects == 1) {
		xy1 = (pClip->extents.y1 << 16) | pClip->extents.x1;
		xy2 = ((pClip->extents.y2 - 1) << 16) | (pClip->extents.x2 - 1);
		if (pFfb->clips[0] == xy1 && pFfb->clips[1] == xy2)
			return FFB_PPC_VCE_2D|FFB_PPC_ACE_DISABLE;
		FFBFifo(pFfb, 2);
		FFB_WRITE64(&ffb->vclipmin, xy1, xy2);
		pFfb->clips[0] = xy1;
		pFfb->clips[1] = xy2;
		return FFB_PPC_VCE_2D|FFB_PPC_ACE_DISABLE;
	} else if (numRects <= 5) {
		ffb_auxclipPtr auxclip;
		BoxPtr pBox = REGION_RECTS(pClip);
		int i, j;
		unsigned int xy3, xy4;

		xy1 = (pBox->y1 << 16) | pBox->x1;
		xy2 = ((pBox->y2 - 1) << 16) | (pBox->x2 - 1);
		if (pFfb->clips[0] == xy1 && pFfb->clips[1] == xy2) {
			j = 0;
			for (i = 1; i < numRects; i++) {
				xy3 = (pBox[i].y1 << 16) | pBox[i].x1;
				xy4 = ((pBox[i].y2 - 1) << 16) | (pBox[i].x2 - 1);
				if (j || xy3 != pFfb->clips[2*i] || xy4 != pFfb->clips[2*i+1]) {
					j = 1;
					pFfb->clips[2*i] = xy3;
					pFfb->clips[2*i+1] = xy4;
				}
			}
			for (; i < 5; i++) {
				if (j || pFfb->clips[2*i] != 1 || pFfb->clips[2*i+1]) {
					j = 1;
					pFfb->clips[2*i] = 1;
					pFfb->clips[2*i+1] = 0;
				}
			}
			if (!j)
				return FFB_PPC_VCE_2D|FFB_PPC_ACE_AUX_ADD;
			FFBFifo(pFfb, 8);
			auxclip = ffb->auxclip;
			for (i = 1; i < 5; i++, auxclip++)
				FFB_WRITE64P(&auxclip->min, &pFfb->clips[2*i]);
			return FFB_PPC_VCE_2D|FFB_PPC_ACE_AUX_ADD;
		}
		FFBFifo(pFfb, 10);
		FFB_WRITE64(&ffb->vclipmin, xy1, xy2);
		pFfb->clips[0] = xy1;
		pFfb->clips[1] = xy2;
		auxclip = ffb->auxclip;
		pBox++;
		for (i = 1; i < numRects; i++, auxclip++, pBox++) {
			xy3 = (pBox->y1 << 16) | pBox->x1;
			xy4 = ((pBox->y2 - 1) << 16) | (pBox->x2 - 1);
			FFB_WRITE64(&auxclip->min, xy3, xy4);
			pFfb->clips[2*i] = xy3;
			pFfb->clips[2*i+1] = xy4;
		}
		for (; i < 5; i++, auxclip++) {
			FFB_WRITE64(&auxclip->min, 1, 0);
			pFfb->clips[2*i] = 1;
			pFfb->clips[2*i+1] = 0;
		}
		return FFB_PPC_VCE_2D|FFB_PPC_ACE_AUX_ADD;
	}
	return 0;
}
