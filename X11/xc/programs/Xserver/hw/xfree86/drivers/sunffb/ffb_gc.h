/*
 * Acceleration for the Creator and Creator3D framebuffer - Accel func declarations.
 *
 * Copyright (C) 1998,1999 Jakub Jelinek (jakub@redhat.com)
 * Copyright (C) 1998 Michal Rehacek (majkl@iname.com)
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
 * JAKUB JELINEK, MICHAL REHACEK, OR DAVID MILLER BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_gc.h,v 1.2 2000/05/23 04:47:44 dawes Exp $ */

#ifndef FFBGC_H
#define FFBGC_H

extern void CreatorDoBitblt(DrawablePtr pSrc, DrawablePtr pDst, int alu, RegionPtr prgnDst,
			    DDXPointPtr pptSrc, unsigned long planemask);

extern void CreatorDoVertBitblt(DrawablePtr pSrc, DrawablePtr pDst, int alu, RegionPtr prgnDst,
				DDXPointPtr pptSrc, unsigned long planemask);

extern RegionPtr CreatorCopyArea(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable,
				 GCPtr pGC, int srcx, int srcy, int width, int height,
				 int dstx, int dsty);

extern RegionPtr CreatorCopyPlane(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable,
				  GCPtr pGC, int srcx, int srcy, int width, int height,
				  int dstx, int dsty, unsigned long bitPlane);

extern void CreatorFillBoxSolid (DrawablePtr pDrawable, int nBox,
				 BoxPtr pBox, unsigned long pixel);

extern void CreatorFillBoxStipple (DrawablePtr pDrawable,
				   int nBox, BoxPtr pBox, CreatorStipplePtr stipple);

extern void CreatorPolyFillRect(DrawablePtr pDrawable, GCPtr pGC,
				int nrectFill, xRectangle *prectInit);

extern void CreatorFillSpans (DrawablePtr pDrawable, GCPtr pGC,
			      int n, DDXPointPtr ppt,
			      int *pwidth, int fSorted);

extern void CreatorPolyPoint(DrawablePtr pDrawable, GCPtr pGC, int mode,
			     int npt, xPoint *pptInit);

extern void CreatorPolySegment (DrawablePtr pDrawable, GCPtr pGC,
				int nseg, xSegment *pSeg);

extern void CreatorFillPolygon (DrawablePtr pDrawable, GCPtr pGC,
				int shape, int mode, int count, DDXPointPtr ppt);

extern void CreatorPolylines (DrawablePtr pDrawable, GCPtr pGC,
			      int mode, int npt, DDXPointPtr ppt);

extern void CreatorPolyGlyphBlt (DrawablePtr pDrawable, GCPtr pGC, int x, int y,
				 unsigned int nglyph, CharInfoPtr *ppci, pointer pGlyphBase);

extern void CreatorTEGlyphBlt (DrawablePtr pDrawable, GCPtr pGC, int x, int y,
			       unsigned int nglyph, CharInfoPtr *ppci, pointer pGlyphBase);

extern void CreatorPolyTEGlyphBlt (DrawablePtr pDrawable, GCPtr pGC, int x, int y,
				   unsigned int nglyph, CharInfoPtr *ppci, pointer pGlyphBase);

extern void CreatorPolyFillArcSolid (DrawablePtr pDrawable, GCPtr pGC,
				    int narcs, xArc *parcs);

extern void CreatorZeroPolyArc(DrawablePtr pDrawable, GCPtr pGC,
			       int narcs, xArc *parcs);

extern int CreatorCheckTile (PixmapPtr pPixmap, CreatorStipplePtr stipple,
			     int ox, int oy, int ph);

extern int CreatorCheckStipple (PixmapPtr pPixmap, CreatorStipplePtr stipple,
				int ox, int oy, int ph);

extern int CreatorCheckLinePattern(GCPtr pGC, CreatorPrivGCPtr gcPriv);

extern int CreatorCheckFill (GCPtr pGC, DrawablePtr pDrawable);

extern void CreatorSetSpans(DrawablePtr pDrawable, GCPtr pGC, char *pcharsrc,
			    DDXPointPtr ppt, int *pwidth, int nspans, int fSorted);

/* Stuff still not accelerated fully. */
extern void CreatorSegmentSSStub (DrawablePtr pDrawable, GCPtr pGC,
				  int nseg, xSegment *pSeg);

extern void CreatorLineSSStub (DrawablePtr pDrawable, GCPtr pGC,
			       int mode, int npt, DDXPointPtr ppt);

extern void CreatorSegmentSDStub (DrawablePtr pDrawable, GCPtr pGC,
				  int nseg, xSegment *pSeg);

extern void CreatorLineSDStub (DrawablePtr pDrawable, GCPtr pGC,
			       int mode, int npt, DDXPointPtr ppt);

extern void CreatorSolidSpansGeneralStub (DrawablePtr pDrawable, GCPtr pGC,
					  int nInit, DDXPointPtr pptInit,
					  int *pwidthInit, int fSorted);

extern void CreatorPolyGlyphBlt8Stub (DrawablePtr pDrawable, GCPtr pGC,
				      int x, int y, unsigned int nglyph, CharInfoPtr *ppci,
				      pointer pglyphBase);

extern void CreatorImageGlyphBlt8Stub (DrawablePtr pDrawable, GCPtr pGC,
				       int x, int y, unsigned int nglyph,
				       CharInfoPtr *ppci, pointer pglyphBase);

extern void CreatorTile32FSCopyStub(DrawablePtr pDrawable, GCPtr pGC,
				    int nInit, DDXPointPtr pptInit,
				    int *pwidthInit, int fSorted);

extern void CreatorTile32FSGeneralStub(DrawablePtr pDrawable, GCPtr pGC,
				       int nInit, DDXPointPtr pptInit,
				       int *pwidthInit, int fSorted);

extern void CreatorUnnaturalTileFSStub(DrawablePtr pDrawable, GCPtr pGC,
				       int nInit, DDXPointPtr pptInit,
				       int *pwidthInit, int fSorted);

extern void Creator8Stipple32FSStub(DrawablePtr pDrawable, GCPtr pGC,
				    int nInit, DDXPointPtr pptInit,
				    int *pwidthInit, int fSorted);

extern void CreatorUnnaturalStippleFSStub(DrawablePtr pDrawable, GCPtr pGC,
					  int nInit, DDXPointPtr pptInit,
					  int *pwidthInit, int fSorted);

extern void Creator8OpaqueStipple32FSStub(DrawablePtr pDrawable, GCPtr pGC,
					  int nInit, DDXPointPtr pptInit,
					  int *pwidthInit, int fSorted);

extern void CreatorPolyFillRectStub(DrawablePtr pDrawable, GCPtr pGC,
				    int nrectFill, xRectangle *prectInit);

#endif /* FFBGC_H */
