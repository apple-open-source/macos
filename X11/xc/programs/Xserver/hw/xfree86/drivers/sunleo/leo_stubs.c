/*
 * Acceleration for the Leo (ZX) framebuffer - Unaccelerated stuff.
 *
 * Copyright (C) 1999 Jakub Jelinek   (jakub@redhat.com)
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
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunleo/leo_stubs.c,v 1.1 2000/05/18 23:21:40 dawes Exp $ */

#define PSZ 32

#include "leo.h"

#include "pixmapstr.h"
#include "scrnintstr.h"

#include "cfb.h"

void
LeoFillPoly1RectGeneral(DrawablePtr pDrawable, GCPtr pGC, int shape, 
			int mode, int count, DDXPointPtr ptsIn)
{
        LeoPtr pLeo = LeoGetScreenPrivate (pDrawable->pScreen);
	LeoDraw		*ld0 = pLeo->ld0;

	if (pGC->alu != GXcopy)
		ld0->rop = leoRopTable[pGC->alu];
	if (pGC->planemask != 0xffffff)
		ld0->planemask = pGC->planemask;
	cfbFillPoly1RectCopy(pDrawable, pGC, shape, mode, count, ptsIn);
	if (pGC->alu != GXcopy)
		ld0->rop = LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW;
	if (pGC->planemask != 0xffffff)
		ld0->planemask = 0xffffff;
}

void
LeoZeroPolyArcSS8General(DrawablePtr pDrawable, GCPtr pGC, int narcs, xArc *parcs)
{
        LeoPtr pLeo = LeoGetScreenPrivate (pDrawable->pScreen);
	LeoDraw		*ld0 = pLeo->ld0;

	if (pGC->alu != GXcopy)
		ld0->rop = leoRopTable[pGC->alu];
	if (pGC->planemask != 0xffffff)
		ld0->planemask = pGC->planemask;
	cfbZeroPolyArcSS8Copy(pDrawable, pGC, narcs, parcs);
	if (pGC->alu != GXcopy)
		ld0->rop = LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW;
	if (pGC->planemask != 0xffffff)
		ld0->planemask = 0xffffff;
}

void
LeoTile32FSGeneral(DrawablePtr pDrawable, GCPtr pGC, int nInit,
		   DDXPointPtr pptInit, int *pwidthInit, int fSorted)
{
        LeoPtr pLeo = LeoGetScreenPrivate (pDrawable->pScreen);
	LeoDraw		*ld0 = pLeo->ld0;

	if (pGC->alu != GXcopy)
		ld0->rop = leoRopTable[pGC->alu];
	if (pGC->planemask != 0xffffff)
		ld0->planemask = pGC->planemask;
	cfbTile32FSCopy(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
	if (pGC->alu != GXcopy)
		ld0->rop = LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW;
	if (pGC->planemask != 0xffffff)
		ld0->planemask = 0xffffff;
}

void
LeoPolyFillArcSolidGeneral(DrawablePtr pDrawable, GCPtr pGC,
			   int narcs, xArc *parcs)
{
        LeoPtr pLeo = LeoGetScreenPrivate (pDrawable->pScreen);
	LeoDraw		*ld0 = pLeo->ld0;

	if (pGC->alu != GXcopy)
		ld0->rop = leoRopTable[pGC->alu];
	if (pGC->planemask != 0xffffff)
		ld0->planemask = pGC->planemask;
	cfbPolyFillArcSolidCopy(pDrawable, pGC, narcs, parcs);
	if (pGC->alu != GXcopy)
		ld0->rop = LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW;
	if (pGC->planemask != 0xffffff)
		ld0->planemask = 0xffffff;
}
