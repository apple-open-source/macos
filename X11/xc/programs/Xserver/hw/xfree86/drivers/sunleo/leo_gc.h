/*
 * Acceleration for the Leo (ZX) framebuffer - Accel func declarations.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunleo/leo_gc.h,v 1.1 2000/05/18 23:21:40 dawes Exp $ */

#ifndef LEOGC_H
#define LEOGC_H

extern RegionPtr LeoCopyArea(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable,
				 GCPtr pGC, int srcx, int srcy, int width, int height,
				 int dstx, int dsty);

extern RegionPtr LeoCopyPlane(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable,
				  GCPtr pGC, int srcx, int srcy, int width, int height,
				  int dstx, int dsty, unsigned long bitPlane);

extern void LeoFillBoxSolid (DrawablePtr pDrawable, int nBox,
				 BoxPtr pBox, unsigned long pixel);

extern void LeoPolyFillRect(DrawablePtr pDrawable, GCPtr pGC,
				int nrectFill, xRectangle *prectInit);

extern void LeoPolyFillRect1Rect(DrawablePtr pDrawable, register GCPtr pGC,
				     int nrectFill, xRectangle *prectInit);

extern void LeoPolyFillStippledRect(DrawablePtr pDrawable, GCPtr pGC,
				    int nrectFill, xRectangle *prectInit);

extern void LeoFillSpansSolid (DrawablePtr pDrawable, GCPtr pGC,
			       int n, DDXPointPtr ppt,
			       int *pwidth, int fSorted);

extern void LeoFillSpansStippled (DrawablePtr pDrawable, GCPtr pGC,
			          int n, DDXPointPtr ppt,
			          int *pwidth, int fSorted);

extern void LeoPolyGlyphBlt (DrawablePtr pDrawable, GCPtr pGC, int x, int y,
				 unsigned int nglyph, CharInfoPtr *ppci, pointer pGlyphBase);

extern void LeoTEGlyphBlt (DrawablePtr pDrawable, GCPtr pGC, int x, int y,
			       unsigned int nglyph, CharInfoPtr *ppci, pointer pGlyphBase);

extern void LeoPolyTEGlyphBlt (DrawablePtr pDrawable, GCPtr pGC, int x, int y,
				   unsigned int nglyph, CharInfoPtr *ppci, pointer pGlyphBase);

extern void LeoFillPoly1RectGeneral(DrawablePtr pDrawable, GCPtr pGC, int shape, 
			int mode, int count, DDXPointPtr ptsIn);

extern void LeoZeroPolyArcSS8General(DrawablePtr pDraw, GCPtr pGC, int narcs, xArc *parcs);

extern void LeoTile32FSGeneral(DrawablePtr pDrawable, GCPtr pGC, int nInit,
			       DDXPointPtr pptInit, int *pwidthInit, int fSorted);

extern void LeoPolyFillArcSolidGeneral(DrawablePtr pDrawable, GCPtr pGC, 
				       int narcs, xArc *parcs);

extern int LeoCheckFill (GCPtr pGC, DrawablePtr pDrawable);

extern void LeoDoBitblt (DrawablePtr pSrc, DrawablePtr pDst, int alu, RegionPtr prgnDst,
			 DDXPointPtr pptSrc, unsigned long planemask);

#endif /* LEOGC_H */
