/*
 * Acceleration for the Creator and Creator3D framebuffer - Unaccelerated stuff.
 *
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
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
 * DAVID MILLER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER 
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_stubs.c,v 1.2 2000/05/23 04:47:45 dawes Exp $ */

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

/* CFB is just too clever for it's own good.  There are paths
 * in cfb for 8bpp that just arbitrarily change the ROP of the
 * GC and assume it just works even without revalidating that
 * GC.  So for now we turn this stuff off.  -DaveM
 */
#undef USE_SFB_TRICKS

#ifdef USE_SFB_TRICKS
/* Sorry, have to expose some CFB internals to get the stubs right. -DaveM */
struct cfb_gcstate {
	unsigned long pmask;
	int alu, rop, and, xor;
};

#define CFB_STATE_SAVE(__statep, __gcp, __privp) \
do {	(__statep)->pmask = (__gcp)->planemask; \
	(__statep)->alu = (__gcp)->alu; \
	(__statep)->rop = (__privp)->rop; \
	(__statep)->and = (__privp)->and; \
	(__statep)->xor = (__privp)->xor; \
} while(0)

#define CFB_STATE_RESTORE(__statep, __gcp, __privp) \
do {	(__gcp)->planemask = (__statep)->pmask; \
	(__gcp)->alu = (__statep)->alu; \
	(__privp)->rop = (__statep)->rop; \
	(__privp)->and = (__statep)->and; \
	(__privp)->xor = (__statep)->xor; \
} while(0)

#define CFB_STATE_SET_SFB(__gcp, __privp) \
do {	(__gcp)->alu = GXcopy; \
	(__gcp)->planemask = (((__gcp)->depth==8)?0xff:0xffffff); \
	(__privp)->rop = GXcopy; \
	(__privp)->and = 0; \
	(__privp)->xor = (__gcp)->fgPixel; \
} while(0)
#endif

/* Stubs so we can wait for the raster processor to
 * unbusy itself before we let the cfb code write
 * directly to the framebuffer.
 */
void
CreatorSolidSpansGeneralStub (DrawablePtr pDrawable, GCPtr pGC,
			      int nInit, DDXPointPtr pptInit,
			      int *pwidthInit, int fSorted)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;

	FFBLOG(("STUB(%s:%d)\n", __FILE__, __LINE__));
	FFB_ATTR_SFB_VAR_WIN(pFfb, 0xffffffff, GXcopy, pWin);
	FFBWait(pFfb, ffb);
	if (pGC->depth == 8)
		cfbSolidSpansGeneral(pDrawable, pGC, nInit, pptInit,
				     pwidthInit, fSorted);
	else
		cfb32SolidSpansGeneral(pDrawable, pGC, nInit, pptInit,
				       pwidthInit, fSorted);
}

void
CreatorSegmentSSStub (DrawablePtr pDrawable, GCPtr pGC, int nseg, xSegment *pSeg)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
	cfbPrivGCPtr devPriv = cfbGetGCPrivate(pGC);
#ifdef USE_SFB_TRICKS
	struct cfb_gcstate cfb_state;
#endif

	FFBLOG(("STUB(%s:%d)\n", __FILE__, __LINE__));
#ifndef USE_SFB_TRICKS
	FFB_ATTR_SFB_VAR_WIN(pFfb, 0xffffffff, GXcopy, pWin);
	FFBWait(pFfb, ffb);
	if (pGC->depth == 8) {
		if (devPriv->oneRect &&
		    ((pDrawable->x >= pGC->pScreen->width - 32768) &&
		     (pDrawable->y >= pGC->pScreen->height - 32768)))
			cfb8SegmentSS1Rect(pDrawable, pGC, nseg, pSeg);
		else
			cfbSegmentSS(pDrawable, pGC, nseg, pSeg);
	} else
		cfb32SegmentSS(pDrawable, pGC, nseg, pSeg);
#else
	CFB_STATE_SAVE(&cfb_state, pGC, devPriv);
	FFB_ATTR_SFB_VAR_WIN(pFfb, cfb_state.pmask, cfb_state.alu, pWin);
	FFBWait(pFfb, ffb);
	CFB_STATE_SET_SFB(pGC, devPriv);
	if (pGC->depth == 8) {
		if (devPriv->oneRect &&
		    ((pDrawable->x >= pGC->pScreen->width - 32768) &&
		     (pDrawable->y >= pGC->pScreen->height - 32768)))
			cfb8SegmentSS1Rect(pDrawable, pGC, nseg, pSeg);
		else
			cfbSegmentSS(pDrawable, pGC, nseg, pSeg);
	} else
		cfb32SegmentSS(pDrawable, pGC, nseg, pSeg);
	CFB_STATE_RESTORE(&cfb_state, pGC, devPriv);
#endif
}

void
CreatorLineSSStub (DrawablePtr pDrawable, GCPtr pGC,
		   int mode, int npt, DDXPointPtr ppt)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
	cfbPrivGCPtr devPriv = cfbGetGCPrivate(pGC);
#ifdef USE_SFB_TRICKS
	struct cfb_gcstate cfb_state;
#endif

	FFBLOG(("STUB(%s:%d)\n", __FILE__, __LINE__));
#ifndef USE_SFB_TRICKS
	FFB_ATTR_SFB_VAR_WIN(pFfb, 0xffffffff, GXcopy, pWin);
	FFBWait(pFfb, ffb);
	if (pGC->depth == 8) {
		if (devPriv->oneRect &&
		    ((pDrawable->x >= pGC->pScreen->width - 32768) &&
		     (pDrawable->y >= pGC->pScreen->height - 32768)))
			cfb8LineSS1Rect(pDrawable, pGC, mode, npt, ppt);
		else
			cfbLineSS(pDrawable, pGC, mode, npt, ppt);
	} else
		cfb32LineSS(pDrawable, pGC, mode, npt, ppt);
#else
	CFB_STATE_SAVE(&cfb_state, pGC, devPriv);
	FFB_ATTR_SFB_VAR_WIN(pFfb, cfb_state.pmask, cfb_state.alu, pWin);
	FFBWait(pFfb, ffb);
	CFB_STATE_SET_SFB(pGC, devPriv);
	if (pGC->depth == 8) {
		if (devPriv->oneRect &&
		    ((pDrawable->x >= pGC->pScreen->width - 32768) &&
		     (pDrawable->y >= pGC->pScreen->height - 32768)))
			cfb8LineSS1Rect(pDrawable, pGC, mode, npt, ppt);
		else
			cfbLineSS(pDrawable, pGC, mode, npt, ppt);
	} else
		cfb32LineSS(pDrawable, pGC, mode, npt, ppt);
	CFB_STATE_RESTORE(&cfb_state, pGC, devPriv);
#endif
}

void
CreatorSegmentSDStub (DrawablePtr pDrawable, GCPtr pGC, int nseg, xSegment *pSeg)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
#ifdef USE_SFB_TRICKS
	struct cfb_gcstate cfb_state;
	cfbPrivGCPtr devPriv = cfbGetGCPrivate(pGC);
#endif

	FFBLOG(("STUB(%s:%d)\n", __FILE__, __LINE__));
#ifndef USE_SFB_TRICKS
	FFB_ATTR_SFB_VAR_WIN(pFfb, 0xffffffff, GXcopy, pWin);
	FFBWait(pFfb, ffb);
	if (pGC->depth == 8)
		cfbSegmentSD(pDrawable, pGC, nseg, pSeg);
	else
		cfb32SegmentSD(pDrawable, pGC, nseg, pSeg);
#else
	CFB_STATE_SAVE(&cfb_state, pGC, devPriv);
	FFB_ATTR_SFB_VAR_WIN(pFfb, cfb_state.pmask, cfb_state.alu, pWin);
	FFBWait(pFfb, ffb);
	CFB_STATE_SET_SFB(pGC, devPriv);
	if (pGC->depth == 8)
		cfbSegmentSD(pDrawable, pGC, nseg, pSeg);
	else
		cfb32SegmentSD(pDrawable, pGC, nseg, pSeg);
	CFB_STATE_RESTORE(&cfb_state, pGC, devPriv);
#endif
}

void
CreatorLineSDStub (DrawablePtr pDrawable, GCPtr pGC,
		   int mode, int npt, DDXPointPtr ppt)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
#ifdef USE_SFB_TRICKS
	struct cfb_gcstate cfb_state;
	cfbPrivGCPtr devPriv = cfbGetGCPrivate(pGC);
#endif

	FFBLOG(("STUB(%s:%d)\n", __FILE__, __LINE__));
#ifndef USE_SFB_TRICKS
	FFB_ATTR_SFB_VAR_WIN(pFfb, 0xffffffff, GXcopy, pWin);
	FFBWait(pFfb, ffb);
	if (pGC->depth == 8)
		cfbLineSD(pDrawable, pGC, mode, npt, ppt);
	else
		cfb32LineSD(pDrawable, pGC, mode, npt, ppt);
#else
	CFB_STATE_SAVE(&cfb_state, pGC, devPriv);
	FFB_ATTR_SFB_VAR_WIN(pFfb, cfb_state.pmask, cfb_state.alu, pWin);
	FFBWait(pFfb, ffb);
	CFB_STATE_SET_SFB(pGC, devPriv);
	if (pGC->depth == 8)
		cfbLineSD(pDrawable, pGC, mode, npt, ppt);
	else
		cfb32LineSD(pDrawable, pGC, mode, npt, ppt);
	CFB_STATE_RESTORE(&cfb_state, pGC, devPriv);
#endif
}

void
CreatorPolyGlyphBlt8Stub (DrawablePtr pDrawable, GCPtr pGC,
			  int x, int y, unsigned int nglyph, CharInfoPtr *ppci,
			  pointer pglyphBase)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
#ifdef USE_SFB_TRICKS
	struct cfb_gcstate cfb_state;
	cfbPrivGCPtr devPriv = cfbGetGCPrivate(pGC);
#endif

	FFBLOG(("STUB(%s:%d)\n", __FILE__, __LINE__));
#ifndef USE_SFB_TRICKS
	FFB_ATTR_SFB_VAR_WIN(pFfb, 0xffffffff, GXcopy, pWin);
	FFBWait(pFfb, ffb);
	if (pGC->depth == 8)
		cfbPolyGlyphBlt8(pDrawable, pGC, x, y, nglyph, ppci, pglyphBase);
	else
		cfb32PolyGlyphBlt8(pDrawable, pGC, x, y, nglyph, ppci, pglyphBase);
#else
	CFB_STATE_SAVE(&cfb_state, pGC, devPriv);
	FFB_ATTR_SFB_VAR_WIN(pFfb, cfb_state.pmask, cfb_state.alu, pWin);
	FFBWait(pFfb, ffb);
	CFB_STATE_SET_SFB(pGC, devPriv);
	if (pGC->depth == 8)
		cfbPolyGlyphBlt8(pDrawable, pGC, x, y, nglyph, ppci, pglyphBase);
	else
		cfb32PolyGlyphBlt8(pDrawable, pGC, x, y, nglyph, ppci, pglyphBase);
	CFB_STATE_RESTORE(&cfb_state, pGC, devPriv);
#endif
}

void
CreatorImageGlyphBlt8Stub (DrawablePtr pDrawable, GCPtr pGC,
			   int x, int y, unsigned int nglyph,
			   CharInfoPtr *ppci, pointer pglyphBase)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
#ifdef USE_SFB_TRICKS
	struct cfb_gcstate cfb_state;
	cfbPrivGCPtr devPriv = cfbGetGCPrivate(pGC);
#endif

	FFBLOG(("STUB(%s:%d)\n", __FILE__, __LINE__));
#ifndef USE_SFB_TRICKS
	FFB_ATTR_SFB_VAR_WIN(pFfb, 0xffffffff, GXcopy, pWin);
	FFBWait(pFfb, ffb);
	if (pGC->depth == 8)
		cfbImageGlyphBlt8(pDrawable, pGC, x, y, nglyph, ppci, pglyphBase);
	else
		cfb32ImageGlyphBlt8(pDrawable, pGC, x, y, nglyph, ppci, pglyphBase);
#else
	CFB_STATE_SAVE(&cfb_state, pGC, devPriv);
	FFB_ATTR_SFB_VAR_WIN(pFfb, cfb_state.pmask, cfb_state.alu, pWin);
	FFBWait(pFfb, ffb);
	CFB_STATE_SET_SFB(pGC, devPriv);
	if (pGC->depth == 8)
		cfbImageGlyphBlt8(pDrawable, pGC, x, y, nglyph, ppci, pglyphBase);
	else
		cfb32ImageGlyphBlt8(pDrawable, pGC, x, y, nglyph, ppci, pglyphBase);
	CFB_STATE_RESTORE(&cfb_state, pGC, devPriv);
#endif
}

void
CreatorTile32FSCopyStub(DrawablePtr pDrawable, GCPtr pGC,
			int nInit, DDXPointPtr pptInit,
			int *pwidthInit, int fSorted)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
#ifdef USE_SFB_TRICKS
	struct cfb_gcstate cfb_state;
	cfbPrivGCPtr devPriv = cfbGetGCPrivate(pGC);
#endif

	FFBLOG(("STUB(%s:%d)\n", __FILE__, __LINE__));
#ifndef USE_SFB_TRICKS
	FFB_ATTR_SFB_VAR_WIN(pFfb, 0xffffffff, GXcopy, pWin);
	FFBWait(pFfb, ffb);
	if (pGC->depth == 8)
		cfbTile32FSCopy(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
	else
		cfb32Tile32FSCopy(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
#else
	CFB_STATE_SAVE(&cfb_state, pGC, devPriv);
	FFB_ATTR_SFB_VAR_WIN(pFfb, cfb_state.pmask, cfb_state.alu, pWin);
	FFBWait(pFfb, ffb);
	CFB_STATE_SET_SFB(pGC, devPriv);
	if (pGC->depth == 8)
		cfbTile32FSCopy(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
	else
		cfb32Tile32FSCopy(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
	CFB_STATE_RESTORE(&cfb_state, pGC, devPriv);
#endif
}

void
CreatorTile32FSGeneralStub(DrawablePtr pDrawable, GCPtr pGC,
			   int nInit, DDXPointPtr pptInit,
			   int *pwidthInit, int fSorted)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
#ifdef USE_SFB_TRICKS
	struct cfb_gcstate cfb_state;
	cfbPrivGCPtr devPriv = cfbGetGCPrivate(pGC);
#endif

	FFBLOG(("STUB(%s:%d)\n", __FILE__, __LINE__));
#ifndef USE_SFB_TRICKS
	FFB_ATTR_SFB_VAR_WIN(pFfb, 0xffffffff, GXcopy, pWin);
	FFBWait(pFfb, ffb);
	if (pGC->depth == 8)
		cfbTile32FSGeneral(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
	else
		cfb32Tile32FSGeneral(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
#else
	CFB_STATE_SAVE(&cfb_state, pGC, devPriv);
	FFB_ATTR_SFB_VAR_WIN(pFfb, cfb_state.pmask, cfb_state.alu, pWin);
	FFBWait(pFfb, ffb);
	CFB_STATE_SET_SFB(pGC, devPriv);
	if (pGC->depth == 8)
		cfbTile32FSGeneral(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
	else
		cfb32Tile32FSGeneral(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
	CFB_STATE_RESTORE(&cfb_state, pGC, devPriv);
#endif
}

void
CreatorUnnaturalTileFSStub(DrawablePtr pDrawable, GCPtr pGC,
			   int nInit, DDXPointPtr pptInit,
			   int *pwidthInit, int fSorted)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
#ifdef USE_SFB_TRICKS
	struct cfb_gcstate cfb_state;
	cfbPrivGCPtr devPriv = cfbGetGCPrivate(pGC);
#endif

	FFBLOG(("STUB(%s:%d)\n", __FILE__, __LINE__));
#ifndef USE_SFB_TRICKS
	FFB_ATTR_SFB_VAR_WIN(pFfb, 0xffffffff, GXcopy, pWin);
	FFBWait(pFfb, ffb);
	if (pGC->depth == 8)
		cfbUnnaturalTileFS(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
	else
		cfb32UnnaturalTileFS(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
#else
	CFB_STATE_SAVE(&cfb_state, pGC, devPriv);
	FFB_ATTR_SFB_VAR_WIN(pFfb, cfb_state.pmask, cfb_state.alu, pWin);
	FFBWait(pFfb, ffb);
	CFB_STATE_SET_SFB(pGC, devPriv);
	if (pGC->depth == 8)
		cfbUnnaturalTileFS(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
	else
		cfb32UnnaturalTileFS(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
	CFB_STATE_RESTORE(&cfb_state, pGC, devPriv);
#endif
}

void
Creator8Stipple32FSStub(DrawablePtr pDrawable, GCPtr pGC,
			int nInit, DDXPointPtr pptInit,
			int *pwidthInit, int fSorted)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
#ifdef USE_SFB_TRICKS
	struct cfb_gcstate cfb_state;
	cfbPrivGCPtr devPriv = cfbGetGCPrivate(pGC);
#endif

	FFBLOG(("STUB(%s:%d)\n", __FILE__, __LINE__));
#ifndef USE_SFB_TRICKS
	FFB_ATTR_SFB_VAR_WIN(pFfb, 0xffffffff, GXcopy, pWin);
	FFBWait(pFfb, ffb);
	cfb8Stipple32FS(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
#else
	CFB_STATE_SAVE(&cfb_state, pGC, devPriv);
	FFB_ATTR_SFB_VAR_WIN(pFfb, cfb_state.pmask, cfb_state.alu, pWin);
	FFBWait(pFfb, ffb);
	CFB_STATE_SET_SFB(pGC, devPriv);
	cfb8Stipple32FS(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
	CFB_STATE_RESTORE(&cfb_state, pGC, devPriv);
#endif
}

void
CreatorUnnaturalStippleFSStub(DrawablePtr pDrawable, GCPtr pGC,
			      int nInit, DDXPointPtr pptInit,
			      int *pwidthInit, int fSorted)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
#ifdef USE_SFB_TRICKS
	struct cfb_gcstate cfb_state;
	cfbPrivGCPtr devPriv = cfbGetGCPrivate(pGC);
#endif

	FFBLOG(("STUB(%s:%d)\n", __FILE__, __LINE__));
#ifndef USE_SFB_TRICKS
	FFB_ATTR_SFB_VAR_WIN(pFfb, 0xffffffff, GXcopy, pWin);
	FFBWait(pFfb, ffb);
	if (pGC->depth == 8)
		cfbUnnaturalStippleFS(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
	else
		cfb32UnnaturalStippleFS(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
#else
	CFB_STATE_SAVE(&cfb_state, pGC, devPriv);
	FFB_ATTR_SFB_VAR_WIN(pFfb, cfb_state.pmask, cfb_state.alu, pWin);
	FFBWait(pFfb, ffb);
	CFB_STATE_SET_SFB(pGC, devPriv);
	if (pGC->depth == 8)
		cfbUnnaturalStippleFS(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
	else
		cfb32UnnaturalStippleFS(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
	CFB_STATE_RESTORE(&cfb_state, pGC, devPriv);
#endif
}

void
Creator8OpaqueStipple32FSStub(DrawablePtr pDrawable, GCPtr pGC,
			      int nInit, DDXPointPtr pptInit,
			      int *pwidthInit, int fSorted)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
#ifdef USE_SFB_TRICKS
	struct cfb_gcstate cfb_state;
	cfbPrivGCPtr devPriv = cfbGetGCPrivate(pGC);
#endif

	FFBLOG(("STUB(%s:%d)\n", __FILE__, __LINE__));
#ifndef USE_SFB_TRICKS
	FFB_ATTR_SFB_VAR_WIN(pFfb, 0xffffffff, GXcopy, pWin);
	FFBWait(pFfb, ffb);
	cfb8OpaqueStipple32FS(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
#else
	CFB_STATE_SAVE(&cfb_state, pGC, devPriv);
	FFB_ATTR_SFB_VAR_WIN(pFfb, cfb_state.pmask, cfb_state.alu, pWin);
	FFBWait(pFfb, ffb);
	CFB_STATE_SET_SFB(pGC, devPriv);
	cfb8OpaqueStipple32FS(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
	CFB_STATE_RESTORE(&cfb_state, pGC, devPriv);
#endif
}

void
CreatorPolyFillRectStub(DrawablePtr pDrawable, GCPtr pGC,
			int nrectFill, xRectangle *prectInit)
{
	WindowPtr pWin = (WindowPtr) pDrawable;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
#ifdef USE_SFB_TRICKS
	struct cfb_gcstate cfb_state;
	cfbPrivGCPtr devPriv = cfbGetGCPrivate(pGC);
#endif

	FFBLOG(("STUB(%s:%d)\n", __FILE__, __LINE__));
#ifndef USE_SFB_TRICKS
	FFB_ATTR_SFB_VAR_WIN(pFfb, 0xffffffff, GXcopy, pWin);
	FFBWait(pFfb, ffb);
	if (pGC->depth == 8)
		cfbPolyFillRect(pDrawable, pGC, nrectFill, prectInit);
	else
		cfb32PolyFillRect(pDrawable, pGC, nrectFill, prectInit);
#else
	CFB_STATE_SAVE(&cfb_state, pGC, devPriv);
	FFB_ATTR_SFB_VAR_WIN(pFfb, cfb_state.pmask, cfb_state.alu, pWin);
	FFBWait(pFfb, ffb);
	CFB_STATE_SET_SFB(pGC, devPriv);
	if (pGC->depth == 8)
		cfbPolyFillRect(pDrawable, pGC, nrectFill, prectInit);
	else
		cfb32PolyFillRect(pDrawable, pGC, nrectFill, prectInit);
	CFB_STATE_RESTORE(&cfb_state, pGC, devPriv);
#endif
}
