/*
 * Acceleration for the Creator and Creator3D framebuffer - Fill spans.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_fspans.c,v 1.2 2000/05/23 04:47:44 dawes Exp $ */

#include "ffb.h"
#include "ffb_regs.h"
#include "ffb_rcache.h"
#include "ffb_fifo.h"
#include "ffb_stip.h"
#include "ffb_loops.h"

#include "pixmapstr.h"
#include "scrnintstr.h"

#include "mi.h"
#include "mispans.h"

#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb32.h"

void
CreatorFillSpans (DrawablePtr pDrawable, GCPtr pGC,
		  int n, DDXPointPtr ppt,
		  int *pwidth, int fSorted)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	CreatorPrivGCPtr gcPriv = CreatorGetGCPrivate (pGC);
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pGC->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
	int *pwidthFree;
	DDXPointPtr pptFree;
	RegionPtr clip = cfbGetCompositeClip(pGC);
	int nTmp = n * miFindMaxBand(clip);

	FFBLOG(("CreatorFillSpans: n(%d) fsorted(%d)\n", n, fSorted));
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
	if(gcPriv->stipple == NULL) {
		FFB_ATTR_GC(pFfb, pGC, pWin,
			    FFB_PPC_APE_DISABLE | FFB_PPC_CS_CONST,
			    FFB_DRAWOP_BRLINEOPEN);
	} else {
		unsigned int fbc;

		FFBSetStipple(pFfb, ffb, gcPriv->stipple,
			      FFB_PPC_CS_CONST, FFB_PPC_CS_MASK);
		FFB_WRITE_PMASK(pFfb, ffb, pGC->planemask);
		FFB_WRITE_DRAWOP(pFfb, ffb, FFB_DRAWOP_BRLINEOPEN);
		fbc = FFB_FBC_WIN(pWin);
		fbc = (fbc & ~FFB_FBC_XE_MASK) | FFB_FBC_XE_OFF;
		FFB_WRITE_FBC(pFfb, ffb, fbc);
	}
	FFBFifo(pFfb, 1);
	ffb->lpat = 0;

	if (pFfb->has_brline_bug) {
		while(n--) {
			register int x, y, w;

			x = ppt->x;
			y = ppt->y;
			w = *pwidth++;
			FFBFifo(pFfb, 5);
			ffb->ppc = 0;
			FFB_WRITE64(&ffb->by, y, x);
			FFB_WRITE64_2(&ffb->bh, y, (x + w));
			ppt++;
		}
	} else {
		while(n--) {
			register int x, y, w;

			x = ppt->x;
			y = ppt->y;
			w = *pwidth++;
			FFBFifo(pFfb, 4);
			FFB_WRITE64(&ffb->by, y, x);
			FFB_WRITE64_2(&ffb->bh, y, (x + w));
			ppt++;
		}
	}

	DEALLOCATE_LOCAL(pptFree);
	DEALLOCATE_LOCAL(pwidthFree);
	pFfb->rp_active = 1;
	FFBSync(pFfb, ffb);
}
