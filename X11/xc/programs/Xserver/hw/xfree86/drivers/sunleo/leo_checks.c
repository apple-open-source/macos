/*
 * Acceleration for the Leo (ZX) framebuffer - stipple/tile verification.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunleo/leo_checks.c,v 1.1 2000/05/18 23:21:39 dawes Exp $ */

#define PSZ 32

#include "leo.h"

#include "pixmapstr.h"
#include "scrnintstr.h"

#include "cfb.h"

int
LeoCheckTile (PixmapPtr pPixmap, LeoStipplePtr stipple, int ox, int oy)
{
	unsigned int *sbits;
	unsigned int fg = 0, bg = 0;
	int fgset = 0, bgset = 0;
	unsigned int *tilebitsLine, *tilebits, tilebit;
	unsigned int sbit, mask;
	int nbwidth;
	int h, w;
	int x, y;
	int s_y, s_x;

	h = pPixmap->drawable.height;
	if (h > 32 || (h & (h - 1)))
		return FALSE;
	w = pPixmap->drawable.width;
	if (w > 32 || (w & (w - 1)))
		return FALSE;
	stipple->patalign = (oy << 16) | ox;
	sbits = stipple->bits;
	tilebitsLine = (unsigned int *) pPixmap->devPrivate.ptr;
	nbwidth = pPixmap->devKind / sizeof(unsigned int);

	for (y = 0; y < h; y++) {
		tilebits = tilebitsLine;
		tilebitsLine += nbwidth;
		sbit = 0;
		mask = 1 << 31;
		for (x = 0; x < w; x++) {
			tilebit = *tilebits++;
			if (fgset && tilebit == fg)
				sbit |=  mask;
			else if (!bgset || tilebit != bg) {
				if (!fgset) {
					fgset = 1;
					fg = tilebit;
					sbit |= mask;
				} else if (!bgset) {
					bgset = 1;
					bg = tilebit;
				} else
					return FALSE;
			}
			mask >>= 1;
		}
		for (s_x = w; s_x < 32; s_x <<= 1)
			sbit = sbit | (sbit >> s_x);
		sbit = (sbit >> ox) | (sbit << (32 - ox));
		for (s_y = y; s_y < 32; s_y += h)
			sbits[(s_y + oy) & 31] = sbit;
	}
	stipple->fg = fg;
	stipple->bg = bg;
	return TRUE;
}

int
LeoCheckStipple (PixmapPtr pPixmap, LeoStipplePtr stipple, int ox, int oy)
{
	unsigned int *sbits;
	unsigned int *stippleBits;
	unsigned int sbit, mask, nbwidth;
	int h, w;
	int y;
	int s_y, s_x;

	h = pPixmap->drawable.height;
	if (h > 32 || (h & (h - 1)))
		return FALSE;
	w = pPixmap->drawable.width;
	if (w > 32 || (w & (w - 1)))
		return FALSE;
	stipple->patalign = (oy << 16) | ox;
	sbits = stipple->bits;
	stippleBits = (unsigned int *) pPixmap->devPrivate.ptr;
	nbwidth = pPixmap->devKind / sizeof(unsigned int);
	mask = ~0 << (32 - w);
	for (y = 0; y < h; y++) {
		sbit = (*stippleBits) & mask;
		stippleBits += nbwidth;
		for (s_x = w; s_x < 32; s_x <<= 1)
			sbit = sbit | (sbit >> s_x);
		sbit = (sbit >> ox) | (sbit << (32 - ox));
		for (s_y = y; s_y < 32; s_y += h)
			sbits[(s_y + oy) & 31] = sbit;
	}
	return TRUE;
}

int
LeoCheckFill (GCPtr pGC, DrawablePtr pDrawable)
{
	LeoPrivGCPtr gcPriv = LeoGetGCPrivate (pGC);
	LeoPtr pLeo = LeoGetScreenPrivate(pDrawable->pScreen);
	LeoStipplePtr stipple;
	unsigned int alu;
	int xrot, yrot;

	if (pGC->fillStyle == FillSolid) {
		if (gcPriv->stipple) {
			xfree (gcPriv->stipple);
			gcPriv->stipple = 0;
		}
		return TRUE;
	}
	if (!(stipple = gcPriv->stipple)) {
		if (!pLeo->tmpStipple) {
			pLeo->tmpStipple = (LeoStipplePtr) xalloc (sizeof *pLeo->tmpStipple);
			if (!pLeo->tmpStipple)
				return FALSE;
		}
		stipple = pLeo->tmpStipple;
	}
	xrot = (pGC->patOrg.x + pDrawable->x) & 31;
	yrot = (pGC->patOrg.y + pDrawable->y) & 31;
	alu = pGC->alu;
	switch (pGC->fillStyle) {
	case FillTiled:
		if (!LeoCheckTile (pGC->tile.pixmap, stipple, xrot, yrot)) {
			if (gcPriv->stipple) {
				xfree (gcPriv->stipple);
				gcPriv->stipple = 0;
			}
			return FALSE;
		}
		break;
	case FillStippled:
		alu |= 0x80;
	case FillOpaqueStippled:
		if (!LeoCheckStipple (pGC->stipple, stipple, xrot, yrot)) {
			if (gcPriv->stipple) {
				xfree (gcPriv->stipple);
				gcPriv->stipple = 0;
			}
			return FALSE;
		}
		stipple->fg = pGC->fgPixel;
		stipple->bg = pGC->bgPixel;
		break;
	}
	stipple->alu = alu;
	gcPriv->stipple = stipple;
	if (stipple == pLeo->tmpStipple)
		pLeo->tmpStipple = 0;
	return TRUE;
}
