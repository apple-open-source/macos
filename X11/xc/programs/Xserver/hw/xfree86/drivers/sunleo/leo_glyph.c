/*
 * Acceleration for the Leo (ZX) framebuffer - Glyph rops.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunleo/leo_glyph.c,v 1.1 2000/05/18 23:21:40 dawes Exp $ */

#define PSZ 32

#include "leo.h"

#include "pixmapstr.h"
#include "scrnintstr.h"
#include "fontstruct.h"
#include "dixfontstr.h"

#include "cfb.h"
#include "mi.h"

void
LeoPolyGlyphBlt (DrawablePtr pDrawable, GCPtr pGC, int x, int y,
		     unsigned int nglyph, CharInfoPtr *ppci, pointer pGlyphBase)
{
	LeoPtr pLeo = LeoGetScreenPrivate (pGC->pScreen);
	LeoCommand0 *lc0 = pLeo->lc0;
	LeoDraw *ld0 = pLeo->ld0;
	RegionPtr clip;
	CharInfoPtr pci;
	int w, h, x0, y0, i;
	unsigned int *bits;
	BoxRec box;
	int curw = -1;
	unsigned int *fbf;
	unsigned char *fb;
	int height, width;

	clip = cfbGetCompositeClip(pGC);
	/* compute an approximate (but covering) bounding box */
	box.x1 = 0;
	if (ppci[0]->metrics.leftSideBearing < 0)
		box.x1 = ppci[0]->metrics.leftSideBearing;
	h = nglyph - 1;
	w = ppci[h]->metrics.rightSideBearing;
	while (--h >= 0)
		w += ppci[h]->metrics.characterWidth;
	box.x2 = w;
	box.y1 = -FONTMAXBOUNDS(pGC->font,ascent);
	box.y2 = FONTMAXBOUNDS(pGC->font,descent);
		
	box.x1 += pDrawable->x + x;
	box.x2 += pDrawable->x + x;
	box.y1 += pDrawable->y + y;
	box.y2 += pDrawable->y + y;
	
	switch (RECT_IN_REGION(pGC->pScreen, clip, &box)) {
	case rgnPART:
		if (REGION_NUM_RECTS(clip) == 1) {
			ld0->vclipmin = (clip->extents.y1 << 16) | clip->extents.x1;
			ld0->vclipmax = ((clip->extents.y2 - 1) << 16) | (clip->extents.x2 - 1);
			break;
		}
		cfbPolyGlyphBlt8 (pDrawable, pGC, x, y, nglyph, ppci, pGlyphBase);
	case rgnOUT:
		return;
	default:
		clip = NULL;
		break;
	}
	
	x += pDrawable->x;
	y += pDrawable->y;
	
	lc0->fontt = 1;
	lc0->addrspace = LEO_ADDRSPC_FONT_OBGR;
	ld0->fg = pGC->fgPixel;
	if (pGC->alu != GXcopy)
		ld0->rop = leoRopTable[pGC->alu];
	if (pGC->planemask != 0xffffff)
		ld0->planemask = pGC->planemask;
	height = pLeo->height;
	width = pLeo->width;
	
	fb = (unsigned char *)pLeo->fb;

	while (nglyph--) {
		pci = *ppci++;

		w = GLYPHWIDTHPIXELS (pci);
		h = GLYPHHEIGHTPIXELS (pci);
		if (!w || !h)
			goto next_glyph;

		x0 = x + pci->metrics.leftSideBearing;
		y0 = y - pci->metrics.ascent;

		/* We're off the screen to the left, making our way
		 * back onto the screen.
		 */
		if((x0 >> 31) == -1)
			goto next_glyph;

		/* We walked off the screen (to the right or downwards)
		 * or we started there, we're never going to work our
		 * way back so stop now.
		 */
		if(x0 >= width || y0 >= height)
			break;
		
		bits = (unsigned int *) pci->bits;

		if (w != curw) {
			curw = w;
			if (w)
				lc0->fontmsk = 0xffffffff << (32 - w);
			else
				lc0->fontmsk = 0;
		}

		fbf = (unsigned *)(fb + (y0 << 13) + (x0 << 2));
		if (y0 + h <= height)
			for (i = 0; i < h; i++) {
				*fbf = *bits++;
				fbf += 2048;
			}
		else
			for (i = 0; i < h && y0 + i < height; i++) {
				*fbf = *bits++;
				fbf += 2048;
			}
	next_glyph:
		x += pci->metrics.characterWidth;
	}
	
	lc0->addrspace = LEO_ADDRSPC_OBGR;
	if (pGC->alu != GXcopy)
		ld0->rop = LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW;
	if (pGC->planemask != 0xffffff)
		ld0->planemask = 0xffffff;
	if (clip) {
		ld0->vclipmin = 0;
		ld0->vclipmax = pLeo->vclipmax;
	}
}

void
LeoTEGlyphBlt (DrawablePtr pDrawable, GCPtr pGC, int x, int y,
		   unsigned int nglyph, CharInfoPtr *ppci, pointer pGlyphBase)
{
	LeoPtr pLeo = LeoGetScreenPrivate (pGC->pScreen);
	LeoCommand0 *lc0 = pLeo->lc0;
	LeoDraw *ld0 = pLeo->ld0;
	RegionPtr clip;
	int h, hTmp;
	int widthGlyph, widthGlyphs;
	BoxRec bbox;
	FontPtr pfont = pGC->font;
	int curw = -1;
	unsigned int *fbf;
	unsigned char *fb;
	int height, width;

	widthGlyph = FONTMAXBOUNDS(pfont,characterWidth);
	h = FONTASCENT(pfont) + FONTDESCENT(pfont);
	clip = cfbGetCompositeClip(pGC);
	bbox.x1 = x + pDrawable->x;
	bbox.x2 = bbox.x1 + (widthGlyph * nglyph);
	bbox.y1 = y + pDrawable->y - FONTASCENT(pfont);
	bbox.y2 = bbox.y1 + h;

	/* If fully out of range, and we have no chance of getting back
	 * in range, no work to do.
	 */
	y = y + pDrawable->y - FONTASCENT(pfont);
	x += pDrawable->x;
	height = pLeo->height;
	width = pLeo->width;
	
	if (x >= width)
	   	return;

	switch (RECT_IN_REGION(pGC->pScreen, clip, &bbox)) {
	case rgnPART: 
		if (REGION_NUM_RECTS(clip) == 1) {
			ld0->vclipmin = (clip->extents.y1 << 16) | clip->extents.x1;
			ld0->vclipmax = ((clip->extents.y2 - 1) << 16) | (clip->extents.x2 - 1);
			break;
		}
		x -= pDrawable->x;
		y = y - pDrawable->y + FONTASCENT(pfont);
		if (pGlyphBase)
			cfbPolyGlyphBlt8 (pDrawable, pGC, x, y, nglyph, ppci, NULL);
		else
			miImageGlyphBlt(pDrawable, pGC, x, y, nglyph, ppci, pGlyphBase);
	case rgnOUT:
		return;
	default:
		clip = NULL;
		break;
	}
	
	lc0->addrspace = LEO_ADDRSPC_FONT_OBGR;
	ld0->fg = pGC->fgPixel;
	if (pGC->alu != GXcopy)
		ld0->rop = leoRopTable[pGC->alu];
	if (pGC->planemask != 0xffffff)
		ld0->planemask = pGC->planemask;
		
	fb = (unsigned char *)pLeo->fb;

	if(pGlyphBase)
		lc0->fontt = 1;
	else {
		lc0->fontt = 0;
		ld0->bg = pGC->bgPixel;
	}

#define LoopIt(count, w, loadup, fetch) \
	if (w != curw) { \
		curw = w; \
		lc0->fontmsk = 0xffffffff << (32 - w); \
	} \
	while (nglyph >= count) { \
		loadup \
		nglyph -= count; \
		fbf = (unsigned *)(fb + (y << 13) + (x << 2)); \
		hTmp = h; \
		if (y + h <= height) \
			while (hTmp--) { \
				*fbf = fetch; \
				fbf += 2048; \
			} \
		else \
			for (hTmp = 0; hTmp < h && y + hTmp < height; hTmp++) { \
				*fbf = fetch; \
				fbf += 2048; \
			} \
		x += w; \
		if(x >= width) \
			goto out; \
	}

	if (widthGlyph <= 8) {
		widthGlyphs = widthGlyph << 2;
		LoopIt(4, widthGlyphs,
		       unsigned int *char1 = (unsigned int *) (*ppci++)->bits;
		       unsigned int *char2 = (unsigned int *) (*ppci++)->bits;
		       unsigned int *char3 = (unsigned int *) (*ppci++)->bits;
		       unsigned int *char4 = (unsigned int *) (*ppci++)->bits;,
		       (*char1++ | ((*char2++ | ((*char3++ | (*char4++
							      >> widthGlyph))
						 >> widthGlyph))
				    >> widthGlyph)))
	} else if (widthGlyph <= 10) {
		widthGlyphs = (widthGlyph << 1) + widthGlyph;
		LoopIt(3, widthGlyphs,
		       unsigned int *char1 = (unsigned int *) (*ppci++)->bits;
		       unsigned int *char2 = (unsigned int *) (*ppci++)->bits;
		       unsigned int *char3 = (unsigned int *) (*ppci++)->bits;,
		       (*char1++ | ((*char2++ | (*char3++ >> widthGlyph)) >> widthGlyph)))
	} else if (widthGlyph <= 16) {
		widthGlyphs = widthGlyph << 1;
		LoopIt(2, widthGlyphs,
		       unsigned int *char1 = (unsigned int *) (*ppci++)->bits;
		       unsigned int *char2 = (unsigned int *) (*ppci++)->bits;,
		       (*char1++ | (*char2++ >> widthGlyph)))
	}
	if(nglyph != 0) {
		if (widthGlyph != curw) {
			curw = widthGlyph;
			lc0->fontmsk = 0xffffffff << (32 - widthGlyph);
		}
		while (nglyph--) {
			unsigned int *char1 = (unsigned int *) (*ppci++)->bits;
			fbf = (unsigned *)(fb + (y << 13) + (x << 2));
			hTmp = h;
			if (y + h <= height)
				while (hTmp--) {
					*fbf = *char1++;
					fbf += 2048;
				}
			else
				for (hTmp = 0; hTmp < h && y + hTmp < height; hTmp++) {
					*fbf = *char1++;
					fbf += 2048;
				}
			x += widthGlyph;
			if (x >= width)
				goto out;
		}
	}
	
out:	lc0->addrspace = LEO_ADDRSPC_OBGR;
	if (pGC->alu != GXcopy)
		ld0->rop = LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW;
	if (pGC->planemask != 0xffffff)
		ld0->planemask = 0xffffff;
	if (clip) {
		ld0->vclipmin = 0;
		ld0->vclipmax = pLeo->vclipmax;
	}
}

void
LeoPolyTEGlyphBlt (DrawablePtr pDrawable, GCPtr pGC, int x, int y,
		       unsigned int nglyph, CharInfoPtr *ppci, pointer pGlyphBase)
{
	LeoTEGlyphBlt (pDrawable, pGC, x, y, nglyph, ppci, (char *) 1);
}
