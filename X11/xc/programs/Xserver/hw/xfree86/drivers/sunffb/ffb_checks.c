/*
 * Acceleration for the Creator and Creator3D framebuffer - stipple/tile/line-pattern
 * verification.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_checks.c,v 1.2 2000/05/23 04:47:44 dawes Exp $ */

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

int
CreatorCheckTile (PixmapPtr pPixmap, CreatorStipplePtr stipple, int ox, int oy, int ph)
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
				} else {
					return FALSE;
				}
			}
			mask >>= 1;
		}
		for (s_x = w; s_x < 32; s_x <<= 1)
			sbit = sbit | (sbit >> s_x);
		sbit = (sbit >> ox) | (sbit << (32 - ox));
		for (s_y = y; s_y < 32; s_y += h)
			sbits[(s_y + oy) & 31] = sbit;
	}
	stipple->pagable = 1;
	for(y = 0; y < (32 - ph); y++) {
		if(sbits[y] != sbits[(y+ph) & (32 - 1)]) {
			stipple->pagable = 0;
			break;
		}
	}
	stipple->fg = fg;
	stipple->bg = bg;
	stipple->inhw = 0;
	return TRUE;
}

int
CreatorCheckStipple (PixmapPtr pPixmap, CreatorStipplePtr stipple, int ox, int oy, int ph)
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
	stipple->pagable = 1;
	for(y = 0; y < (32 - ph); y++) {
		if(sbits[y] != sbits[(y+ph) & (32 - 1)]) {
			stipple->pagable = 0;
			break;
		}
	}
	stipple->inhw = 0;
	return TRUE;
}

int
CreatorCheckLinePattern(GCPtr pGC, CreatorPrivGCPtr gcPriv)
{
	unsigned int linepat = 0;
	unsigned char *dashp = (unsigned char *)pGC->dash;
	int ndash = pGC->numInDashList;
	int doff = (int) pGC->dashOffset;
	int smallest_dashlen;
	int i, nbits = 0;

	for(i = 0; i < ndash; i++)
		nbits += dashp[i];
	if(nbits <= 16) {
		/* We can do it simply, so don't try to use all of
		 * the hair below.
		 */
		nbits = 0;
		for(i = 0; i < ndash; i++) {
			int this_bits = dashp[i];
			if((i & 1) == 0) {
				int x;

				for(x = 0; x < this_bits; x++)
					linepat |= (1<<(nbits + x));
			}
			nbits += this_bits;
		}
		smallest_dashlen = 1;
	} else {
		/* Iteratively find a usable line pattern bitmap and
		 * assosciated scale.  This is slow, but it works.
		 * Feel free to come up with something more efficient. -DaveM
		 */
		smallest_dashlen = 0;
		while(smallest_dashlen++ < 16) {
			int bits_so_far = 0;

			for(i = 0; i < ndash; i++) {
				if((dashp[i] % smallest_dashlen) != 0)
					break;
				bits_so_far += dashp[i] / smallest_dashlen;
				if(bits_so_far >= 16)
					return FALSE;
			}
			if(i == ndash)
				break;
		}
		if(smallest_dashlen == 16)
			return FALSE;

		/* Compute the final scaled line pattern. */
		nbits = 0;
		for(i = 0; i < ndash; i++) {
			int this_bits = dashp[i] / smallest_dashlen;

			nbits += this_bits;
			if((i & 1) != 0)
				continue;
			while(this_bits--)
				linepat |= (1<<(nbits - this_bits - 1));
		}
	}

	/* We're golden... */
	linepat = ((linepat << FFB_LPAT_PATTERN_SHIFT)					|
		   (smallest_dashlen << FFB_LPAT_SCALEVAL_SHIFT)			|
		   ((nbits & 0xf) << FFB_LPAT_PATLEN_SHIFT)				|
		   (((doff / smallest_dashlen) & 0xf) << FFB_LPAT_PATPTR_SHIFT)	|
		   (((doff % smallest_dashlen) & 0xf) << FFB_LPAT_SCALEPTR_SHIFT));
	gcPriv->linepat = linepat;
	return TRUE;
}

/* cache one stipple; figuring out if we can use the stipple is as hard as
 * computing it, so we just use this one and leave it here if it
 * can't be used this time
 */

CreatorStipplePtr FFB_tmpStipple;

int
CreatorCheckFill (GCPtr pGC, DrawablePtr pDrawable)
{
	CreatorPrivGCPtr gcPriv = CreatorGetGCPrivate (pGC);
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pDrawable->pScreen);
	CreatorStipplePtr stipple;
	unsigned int alu;
	int xrot, yrot, ph = FFB_FFPARMS(pFfb).pagefill_height;

	if (pGC->fillStyle == FillSolid) {
		if (gcPriv->stipple) {
			xfree (gcPriv->stipple);
			gcPriv->stipple = 0;
		}
		return TRUE;
	}
	if (!(stipple = gcPriv->stipple)) {
		if (!FFB_tmpStipple) {
			FFB_tmpStipple = (CreatorStipplePtr) xalloc (sizeof *FFB_tmpStipple);
			if (!FFB_tmpStipple)
				return FALSE;
		}
		stipple = FFB_tmpStipple;
	}
	xrot = (pGC->patOrg.x + pDrawable->x) & 31;
	yrot = (pGC->patOrg.y + pDrawable->y) & 31;
	alu = pGC->alu;
	switch (pGC->fillStyle) {
	case FillTiled:
		if (!CreatorCheckTile (pGC->tile.pixmap, stipple, xrot, yrot, ph)) {
			if (gcPriv->stipple) {
				xfree (gcPriv->stipple);
				gcPriv->stipple = 0;
			}
			return FALSE;
		}
		break;
	case FillStippled:
		alu |= FFB_ROP_EDIT_BIT;
	case FillOpaqueStippled:
		if (!CreatorCheckStipple (pGC->stipple, stipple, xrot, yrot, ph)) {
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
	if (stipple == FFB_tmpStipple)
		FFB_tmpStipple = 0;
	return TRUE;
}
