/*
 * Acceleration for the Creator and Creator3D framebuffer - Glyph rops.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_glyph.c,v 1.2 2000/05/23 04:47:45 dawes Exp $ */

#include "ffb.h"
#include "ffb_regs.h"
#include "ffb_rcache.h"
#include "ffb_fifo.h"
#include "ffb_loops.h"

#include "pixmapstr.h"
#include "scrnintstr.h"
#include "fontstruct.h"
#include "dixfontstr.h"

#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb32.h"

void
CreatorPolyGlyphBlt (DrawablePtr pDrawable, GCPtr pGC, int x, int y,
		     unsigned int nglyph, CharInfoPtr *ppci, pointer pGlyphBase)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pGC->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
	FontPtr pfont = pGC->font;
	RegionPtr clip = cfbGetCompositeClip(pGC);
	BoxPtr pbox = REGION_RECTS(clip);
	int nbox = REGION_NUM_RECTS(clip);
	int skippix, skipglyph, width, n, i;
	int Left, Right, Top, Bottom, LeftEdge, RightEdge;

	FFBLOG(("CreatorPolyGlyphBlt: xy[%08x:%08x] nglyph(%d)\n", x, y, nglyph));
	x += pDrawable->x;
	y += pDrawable->y;

	width = 0;
	for(i = 0; i < (nglyph - 1); i++)
		width += (ppci[i])->metrics.characterWidth;

	Left   = x + (ppci[0])->metrics.leftSideBearing;
	Right  = x + (width + (ppci[nglyph - 1])->metrics.rightSideBearing);
	Top    = y - FONTMAXBOUNDS(pfont, ascent);
	Bottom = y + FONTMAXBOUNDS(pfont, descent);

	while(nbox && (Top >= pbox->y2)) {
		pbox++;
		nbox--;
	}

	if(!nbox || Bottom < pbox->y1)
		return;

	/* Ok, setup the chip. */
	{
		unsigned int ppc = (FFB_PPC_APE_DISABLE | FFB_PPC_TBE_TRANSPARENT |
				    FFB_PPC_CS_CONST);
		unsigned int ppc_mask = (FFB_PPC_APE_MASK | FFB_PPC_TBE_MASK |
					 FFB_PPC_CS_MASK);
		unsigned int pmask = pGC->planemask;
		unsigned int rop = (FFB_ROP_EDIT_BIT | pGC->alu) | (FFB_ROP_NEW << 8);
		unsigned int fg = pGC->fgPixel;
		WindowPtr pWin = (WindowPtr) pDrawable;
		unsigned int fbc = FFB_FBC_WIN(pWin);

		fbc = (fbc & ~FFB_FBC_XE_MASK) | FFB_FBC_XE_OFF;

		if((pFfb->ppc_cache & ppc_mask) != ppc ||
		   pFfb->fg_cache != fg ||
		   pFfb->fbc_cache != fbc ||
		   pFfb->rop_cache != rop ||
		   pFfb->pmask_cache != pmask ||
		   pFfb->fontinc_cache != ((1<<16) | 0)) {
			pFfb->ppc_cache &= ~ppc_mask;
			pFfb->ppc_cache |= ppc;
			pFfb->fg_cache = fg;
			pFfb->fbc_cache = fbc;
			pFfb->rop_cache = rop;
			pFfb->pmask_cache = pmask;
			pFfb->fontinc_cache = ((1<<16) | 0);
			pFfb->rp_active = 1;
			FFBFifo(pFfb, 6);
			ffb->ppc = ppc;
			ffb->fg = fg;
			ffb->fbc = fbc;
			ffb->rop = rop;
			ffb->pmask = pmask;
			ffb->fontinc = ((1 << 16) | 0);
		}
	}

	while(nbox && (Bottom >= pbox->y1)) {
		LeftEdge  = max(Left, pbox->x1);
		RightEdge = min(Right, pbox->x2);
		if(RightEdge > LeftEdge) {
			int walk, x_start;

			skippix = LeftEdge - x;
			skipglyph = walk = 0;
			while(skippix >= (walk + (ppci[skipglyph])->metrics.rightSideBearing)) {
				walk += (ppci[skipglyph])->metrics.characterWidth;
				skipglyph++;
			}
			x_start = x + walk;
			skippix = RightEdge - x;
			n = 0;
			i = skipglyph;
			while((i < nglyph) &&
			      (skippix > (walk + (ppci[i])->metrics.leftSideBearing))) {
				walk += (ppci[i])->metrics.characterWidth;
				i++;
				n++;
			}
			if(n) {
				CharInfoPtr *ppci_iter = ppci + skipglyph;
				CharInfoPtr pci;
				unsigned int *bits;
				int w, h, x0, y0, xskip, yskip;

				while(n--) {
					pci = *ppci_iter++;
					w = GLYPHWIDTHPIXELS(pci);
					h = GLYPHHEIGHTPIXELS(pci);
					if(!w || !h)
						goto next_glyph;

					x0 = x_start + pci->metrics.leftSideBearing;
					y0 = y - pci->metrics.ascent;
					bits = (unsigned int *) pci->bits;

					/* Now clip it to the bits we should actually
					 * render.
					 */
					xskip = yskip = 0;
					if(pbox->x1 > x0) {
						xskip = pbox->x1 - x0;
						w -= xskip;
						x0 = pbox->x1;
						if(w <= 0)
							goto next_glyph;
					}
					if(pbox->y1 > y0) {
						yskip = pbox->y1 - y0;
						h -= yskip;
						y0 = pbox->y1;
						if(h <= 0)
							goto next_glyph;
					}
					if(pbox->x2 < (x0 + w)) {
						w = pbox->x2 - x0;
						if(w <= 0)
							goto next_glyph;
					}
					if(pbox->y2 < (y0 + h)) {
						h = pbox->y2 - y0;
						if(h <= 0)
							goto next_glyph;
					}

					FFB_WRITE_FONTW(pFfb, ffb, w);
					FFBFifo(pFfb, 1 + h);
					ffb->fontxy = ((y0 << 16) | x0);
					for(i = 0; i < h; i++)
						ffb->font = bits[yskip + i] << xskip;

				next_glyph:
					x_start += pci->metrics.characterWidth;
				}
			}
		}
		nbox--;
		pbox++;
	}
	pFfb->rp_active = 1;
	FFBSync(pFfb, ffb);
}

void
CreatorTEGlyphBlt (DrawablePtr pDrawable, GCPtr pGC, int x, int y,
		   unsigned int nglyph, CharInfoPtr *ppci, pointer pGlyphBase)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pGC->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
	RegionPtr clip = cfbGetCompositeClip(pGC);
	int nbox = REGION_NUM_RECTS(clip);
	BoxPtr pbox = REGION_RECTS(clip);
	FontPtr pfont = pGC->font;
	int glyphWidth = FONTMAXBOUNDS(pfont, characterWidth);
	int skippix, skipglyphs, Left, Right, Top, Bottom;
	int LeftEdge, RightEdge, ytop, ybot, h, w;

	FFBLOG(("CreatorTEGlyphBlt: xy[%08x:%08x] nglyph(%d) pgbase(%p)\n",
		x, y, nglyph, pGlyphBase));

	Left = x + pDrawable->x;
	Right = Left + (glyphWidth * nglyph);
	y += pDrawable->y;
	Top = y - FONTASCENT(pfont);
	Bottom = y + FONTDESCENT(pfont);

	while(nbox && (Top >= pbox->y2)) {
		pbox++;
		nbox--;
	}

	if(!nbox || Bottom <= pbox->y1)
		return;

	/* Ok, setup the chip. */
	{
		unsigned int ppc = FFB_PPC_APE_DISABLE|FFB_PPC_CS_CONST;
		unsigned int ppc_mask = FFB_PPC_APE_MASK|FFB_PPC_TBE_MASK|FFB_PPC_CS_MASK;
		unsigned int pmask = pGC->planemask;
		unsigned int rop;
		unsigned int fg = pGC->fgPixel;
		unsigned int bg = pGC->bgPixel;
		WindowPtr pWin = (WindowPtr) pDrawable;
		unsigned int fbc = FFB_FBC_WIN(pWin);

		fbc = (fbc & ~FFB_FBC_XE_MASK) | FFB_FBC_XE_OFF;

		if(pGlyphBase) {
			ppc |= FFB_PPC_TBE_TRANSPARENT;
			rop = FFB_ROP_EDIT_BIT | pGC->alu;
		} else {
			ppc |= FFB_PPC_TBE_OPAQUE;
			rop = FFB_ROP_EDIT_BIT | GXcopy;
		}
		rop |= (FFB_ROP_NEW << 8);
		if((pFfb->ppc_cache & ppc_mask) != ppc ||
		   pFfb->fg_cache != fg ||
		   pFfb->fbc_cache != fbc ||
		   pFfb->rop_cache != rop ||
		   pFfb->pmask_cache != pmask ||
		   pFfb->fontinc_cache != ((1<<16) | 0) ||
		   (!pGlyphBase && pFfb->bg_cache != bg)) {
			pFfb->ppc_cache &= ~ppc_mask;
			pFfb->ppc_cache |= ppc;
			pFfb->fg_cache = fg;
			pFfb->fbc_cache = fbc;
			pFfb->rop_cache = rop;
			pFfb->pmask_cache = pmask;
			pFfb->fontinc_cache = ((1<<16) | 0);
			if(!pGlyphBase)
				pFfb->bg_cache = bg;
			FFBFifo(pFfb, (!pGlyphBase ? 7 : 6));
			ffb->ppc = ppc;
			ffb->fg = fg;
			ffb->fbc = fbc;
			ffb->rop = rop;
			ffb->pmask = pmask;
			ffb->fontinc = ((1 << 16) | 0);
			if(!pGlyphBase)
				ffb->bg = bg;
		}
	}

	while(nbox && (Bottom > pbox->y1)) {
		LeftEdge = max(Left, pbox->x1);
		RightEdge = min(Right, pbox->x2);

		if(RightEdge > LeftEdge) {
			ytop = max(Top, pbox->y1);
			ybot = min(Bottom, pbox->y2);

			if((skippix = LeftEdge - Left)) {
				skipglyphs = skippix / glyphWidth;
				skippix %= glyphWidth;
			} else
				skipglyphs = 0;
			w = RightEdge - LeftEdge;

			/* Get aligned onto a character. */
			if(skippix) {
				unsigned int *gbits = (unsigned int *) ppci[skipglyphs++]->bits;
				int chunk_size = (glyphWidth - skippix);

				if (chunk_size > w)
					chunk_size = w;

				FFB_WRITE_FONTW(pFfb, ffb, chunk_size);
				FFBFifo(pFfb, 1 + (ybot - ytop));
				ffb->fontxy = ((ytop << 16) | LeftEdge);
				for(h = (ytop - Top); h < (ybot - Top); h++)
					ffb->font = gbits[h] << skippix;
				LeftEdge += chunk_size;
				w -= chunk_size;
			}
			/* And now blit the rest with unrolled loops. */
#define 		LoopIt(chunkW, loadup, fetch) \
			FFB_WRITE_FONTW(pFfb, ffb, chunkW); \
			while (w >= chunkW) { \
				loadup \
				FFBFifo(pFfb, 1 + (ybot - ytop)); \
				ffb->fontxy = ((ytop << 16) | LeftEdge); \
				for(h = (ytop - Top); h < (ybot - Top); h++) \
					ffb->font = fetch; \
				LeftEdge += chunkW; \
				w -= chunkW; \
			}
			if(glyphWidth <= 8) {
				int chunk_size = glyphWidth << 2;
				LoopIt(chunk_size,
				       unsigned int *char1 = ((unsigned int *)ppci[skipglyphs++]->bits)+(ytop-Top);
				       unsigned int *char2 = ((unsigned int *)ppci[skipglyphs++]->bits)+(ytop-Top);
				       unsigned int *char3 = ((unsigned int *)ppci[skipglyphs++]->bits)+(ytop-Top);
				       unsigned int *char4 = ((unsigned int *)ppci[skipglyphs++]->bits)+(ytop-Top);,
				       (*char1++ | ((*char2++ | ((*char3++ | (*char4++ >> glyphWidth))
								 >> glyphWidth))
						    >> glyphWidth)))
			} else if(glyphWidth <= 10) {
				int chunk_size = (glyphWidth << 1) + glyphWidth;
				LoopIt(chunk_size,
				       unsigned int *char1 = ((unsigned int *)ppci[skipglyphs++]->bits)+(ytop-Top);
				       unsigned int *char2 = ((unsigned int *)ppci[skipglyphs++]->bits)+(ytop-Top);
				       unsigned int *char3 = ((unsigned int *)ppci[skipglyphs++]->bits)+(ytop-Top);,
				       (*char1++ | ((*char2++ | (*char3++ >> glyphWidth)) >> glyphWidth)));
			} else if(glyphWidth <= 16) {
				int chunk_size = glyphWidth << 1;
				LoopIt(chunk_size,
				       unsigned int *char1 = ((unsigned int *)ppci[skipglyphs++]->bits)+(ytop-Top);
				       unsigned int *char2 = ((unsigned int *)ppci[skipglyphs++]->bits)+(ytop-Top);,
				       (*char1++ | (*char2++ >> glyphWidth)));
			}
#undef LoopIt
			/* Take care of any final glyphs. */
			while(w > 0) {
				unsigned int *gbits = (unsigned int *) ppci[skipglyphs++]->bits;
				int pix = glyphWidth;

				if(w < pix)
					pix = w;
				FFB_WRITE_FONTW(pFfb, ffb, pix);
				FFBFifo(pFfb, 1 + (ybot - ytop));
				ffb->fontxy = ((ytop << 16) | LeftEdge);
				for(h = (ytop - Top); h < (ybot - Top); h++)
					ffb->font = gbits[h];
				LeftEdge += pix;
				w -= pix;
			}
		}
		nbox--;
		pbox++;
	}

	pFfb->rp_active = 1;
	FFBSync(pFfb, ffb);
}

void
CreatorPolyTEGlyphBlt (DrawablePtr pDrawable, GCPtr pGC, int x, int y,
		       unsigned int nglyph, CharInfoPtr *ppci, pointer pGlyphBase)
{
	CreatorTEGlyphBlt (pDrawable, pGC, x, y, nglyph, ppci, (char *) 1);
}
