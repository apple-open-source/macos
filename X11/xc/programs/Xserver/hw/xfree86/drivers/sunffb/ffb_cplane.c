/*
 * Acceleration for the Creator and Creator3D framebuffer - Plane copies.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_cplane.c,v 1.2 2000/05/23 04:47:44 dawes Exp $ */

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

#include "cfbmskbits.h"
#include "mi.h"

/* Blatantly stolen from mach64 driver. */
#define mfbmaskbits(x, w, startmask, endmask, nlw) \
	startmask = starttab[(x)&0x1f]; \
	endmask = endtab[((x)+(w)) & 0x1f]; \
	if (startmask) \
		nlw = (((w) - (32 - ((x)&0x1f))) >> 5); \
	else \
		nlw = (w) >> 5;

#define mfbmaskpartialbits(x, w, mask) \
	mask = partmasks[(x)&0x1f][(w)&0x1f];

#define LeftMost    0
#define StepBit(bit, inc)  ((bit) += (inc))


#define GetBits(psrc, nBits, curBit, bitPos, bits) {\
	bits = 0; \
	while (nBits--) { \
		bits |= ((*psrc++ >> bitPos) & 1) << curBit; \
		StepBit (curBit, 1); \
	} \
}

static void
CreatorCopyPlane32to1 (DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable, int rop, RegionPtr prgnDst, 
		       DDXPointPtr pptSrc, unsigned long planemask, unsigned long bitPlane)
{
	int			srcx, srcy, dstx, dsty, width, height;
	unsigned long		*psrcBase;
	unsigned long		*pdstBase;
	int			widthSrc, widthDst;
	unsigned int		*psrcLine;
	unsigned int		*pdstLine;
	register unsigned int   *psrc;
	register int		i;
	register int		curBit;
	register int		bitPos;
	register unsigned int   bits;
	register unsigned int   *pdst;
	unsigned int		startmask, endmask;
	int			niStart = 0, niEnd = 0;
	int			bitStart = 0, bitEnd = 0;
	int			nl, nlMiddle;
	int			nbox;
	BoxPtr			pbox;
	int result;

	extern int starttab[32], endtab[32];
	extern unsigned int partmasks[32][32];

	if (!(planemask & 1))
		return;

	/* must explicitly ask for "int" widths, as code below expects it */
	/* on some machines (Sparc64), "long" and "int" are not the same size */
	cfbGetTypedWidthAndPointer (pSrcDrawable, widthSrc, psrcBase, int, unsigned long)
	cfbGetTypedWidthAndPointer (pDstDrawable, widthDst, pdstBase, int, unsigned long)

	bitPos = ffs (bitPlane) - 1;

	nbox = REGION_NUM_RECTS(prgnDst);
	pbox = REGION_RECTS(prgnDst);
	while (nbox--) {
		dstx = pbox->x1;
		dsty = pbox->y1;
		srcx = pptSrc->x;
		srcy = pptSrc->y;
		width = pbox->x2 - pbox->x1;
		height = pbox->y2 - pbox->y1;
		pbox++;
		pptSrc++;
		psrcLine = (unsigned int *)psrcBase + srcy * widthSrc + srcx;
		pdstLine = (unsigned int *)pdstBase + dsty * widthDst + (dstx >> 5);
		if (dstx + width <= 32) {
			mfbmaskpartialbits(dstx, width, startmask);
			nlMiddle = 0;
			endmask = 0;
		} else {
			mfbmaskbits (dstx, width, startmask, endmask, nlMiddle);
		}
		if (startmask) {
			niStart = 32 - (dstx & 0x1f);
			bitStart = LeftMost;
			StepBit (bitStart, (dstx & 0x1f));
		}
		if (endmask) {
			niEnd = (dstx + width) & 0x1f;
			bitEnd = LeftMost;
		}
		if (rop == GXcopy) {
			while (height--) {
				psrc = psrcLine;
				pdst = pdstLine;
				psrcLine += widthSrc;
				pdstLine += widthDst;
				if (startmask) {
					i = niStart;
					curBit = bitStart;
					GetBits (psrc, i, curBit, bitPos, bits);
				
					*pdst = (*pdst & ~startmask) | bits;
					pdst++;
				}
				nl = nlMiddle;
				
				while (nl--) {
					i = 32;
					curBit = LeftMost;
					GetBits (psrc, i, curBit, bitPos, bits);
					*pdst++ = bits;
				}
				if (endmask) {
					i = niEnd;
					curBit = bitEnd;
					GetBits (psrc, i, curBit, bitPos, bits);

					*pdst = (*pdst & ~endmask) | bits;
				}
			}
		} else {
			while (height--) {
				psrc = psrcLine;
				pdst = pdstLine;
				psrcLine += widthSrc;
				pdstLine += widthDst;
				if (startmask) {
					i = niStart;
					curBit = bitStart;
					GetBits (psrc, i, curBit, bitPos, bits);
					DoRop (result, rop, bits, *pdst);
				
					*pdst = (*pdst & ~startmask) | (result & startmask);
					pdst++;
				}
				nl = nlMiddle;
				while (nl--) {
					i = 32;
					curBit = LeftMost;
					GetBits (psrc, i, curBit, bitPos, bits);
					DoRop (result, rop, bits, *pdst);
					*pdst = result;
					++pdst;
				}
				if (endmask) {
					i = niEnd;
					curBit = bitEnd;
					GetBits (psrc, i, curBit, bitPos, bits);
					DoRop (result, rop, bits, *pdst);
				
					*pdst = (*pdst & ~endmask) | (result & endmask);
				}
			}
		}
	}
}

static unsigned int copyPlaneFG, copyPlaneBG;

static void
CreatorCopyPlane1toFbBpp (DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable, int alu, RegionPtr prgnDst, DDXPointPtr pptSrc, unsigned long planemask, unsigned long bitPlane)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDstDrawable->pScreen);
	WindowPtr pWin = (WindowPtr) pDstDrawable;
	ffb_fbcPtr ffb = pFfb->regs;
	int srcx, srcy, dstx, dsty, width, height;
	int xoffSrc, widthSrc;
	unsigned int *psrcBase, *psrc, *psrcStart;
	unsigned int w, tmp, i;
	int nbox;
	BoxPtr pbox;

	{
		unsigned int ppc = (FFB_PPC_APE_DISABLE | FFB_PPC_TBE_OPAQUE |
				    FFB_PPC_CS_CONST);
		unsigned int ppc_mask = (FFB_PPC_APE_MASK | FFB_PPC_TBE_MASK |
					 FFB_PPC_CS_MASK);
		unsigned int rop = (FFB_ROP_EDIT_BIT | alu) | (FFB_ROP_NEW << 8);
		unsigned int fbc = FFB_FBC_WIN(pWin);

		fbc = (fbc & ~FFB_FBC_XE_MASK) | FFB_FBC_XE_OFF;

		if((pFfb->ppc_cache & ppc_mask) != ppc ||
		   pFfb->fg_cache != copyPlaneFG ||
		   pFfb->fbc_cache != fbc ||
		   pFfb->rop_cache != rop ||
		   pFfb->pmask_cache != planemask ||
		   pFfb->bg_cache != copyPlaneBG) {
			pFfb->ppc_cache &= ~ppc_mask;
			pFfb->ppc_cache |= ppc;
			pFfb->fg_cache = copyPlaneFG;
			pFfb->fbc_cache = fbc;
			pFfb->rop_cache = rop;
			pFfb->pmask_cache = planemask;
			pFfb->bg_cache = copyPlaneBG;
			pFfb->rp_active = 1;
			FFBFifo(pFfb, 6);
			ffb->ppc = ppc;
			ffb->fg = copyPlaneFG;
			ffb->fbc = fbc;
			ffb->rop = rop;
			ffb->pmask = planemask;
			ffb->bg = copyPlaneBG;
		}
	}

	cfbGetTypedWidthAndPointer (pSrcDrawable, widthSrc, psrcBase, unsigned int, unsigned int)

	nbox = REGION_NUM_RECTS(prgnDst);
	pbox = REGION_RECTS(prgnDst);
	while (nbox--) {
		dstx = pbox->x1;
		dsty = pbox->y1;
		srcx = pptSrc->x;
		srcy = pptSrc->y;
		width = pbox->x2 - dstx;
		height = pbox->y2 - dsty;
		pbox++;
		pptSrc++;
		if (!width)
			continue;
		psrc = psrcBase + srcy * widthSrc + (srcx >> 5);
		for (xoffSrc = srcx & 0x1f; height--; psrc = psrcStart + widthSrc) {
			w = width;
			psrcStart = psrc;
			FFBFifo(pFfb, (1 + (xoffSrc != 0)));
			ffb->fontxy = ((dsty++ << 16) | (dstx & 0xffff));
			if (xoffSrc) {
				tmp = 32 - xoffSrc;
				if (tmp > w)
					tmp = w;
				FFB_WRITE_FONTW(pFfb, ffb, tmp);
				FFB_WRITE_FONTINC(pFfb, ffb, tmp);
				ffb->font = *psrc++ << xoffSrc;
				w -= tmp;
			}
			if (!w)
				continue;
			FFB_WRITE_FONTW(pFfb, ffb, 32);
			FFB_WRITE_FONTINC(pFfb, ffb, 32);
			while (w >= 256) {
				FFBFifo(pFfb, 8);
				for (i = 0; i < 8; i++)
					ffb->font = *psrc++;
				w -= 256;
			}
			while (w >= 32) {
				FFBFifo(pFfb, 1);
				ffb->font = *psrc++;
				w -= 32;
			}
			if (w) {
				FFB_WRITE_FONTW(pFfb, ffb, w);
				FFBFifo(pFfb, 1);
				ffb->font = *psrc++;
			}
		}
	}
	pFfb->rp_active = 1;
	FFBSync(pFfb, ffb);
}

RegionPtr CreatorCopyPlane(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable,
			   GCPtr pGC, int srcx, int srcy, int width, int height,
			   int dstx, int dsty, unsigned long bitPlane)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pSrcDrawable->pScreen);
	WindowPtr pWin = (WindowPtr) pDstDrawable;
	ffb_fbcPtr ffb = pFfb->regs;
	RegionPtr ret;

	FFBLOG(("CreatorCopyPlane: sbpp(%d) dbpp(%d) src[%08x:%08x] dst[%08x:%08x] bplane(%08x)\n",
		pSrcDrawable->bitsPerPixel, pDstDrawable->bitsPerPixel,
		srcx, srcy, dstx, dsty, bitPlane));
	if (pSrcDrawable->bitsPerPixel == 1 &&
	    (pDstDrawable->bitsPerPixel == 32 || pDstDrawable->bitsPerPixel == 8)) {
		if (bitPlane == 1) {
			copyPlaneFG = pGC->fgPixel;
			copyPlaneBG = pGC->bgPixel;
			ret = cfbBitBlt (pSrcDrawable, pDstDrawable,
					 pGC, srcx, srcy, width, height,
					 dstx, dsty, CreatorCopyPlane1toFbBpp, bitPlane);
		} else
			ret = miHandleExposures (pSrcDrawable, pDstDrawable,
						 pGC, srcx, srcy, width, height, dstx, dsty, bitPlane);
	} else if ((pSrcDrawable->bitsPerPixel == 32 || pSrcDrawable->bitsPerPixel == 8)
		   && pDstDrawable->bitsPerPixel == 1) {
		extern int InverseAlu[16];
		int oldalu;

		oldalu = pGC->alu;
		if ((pGC->fgPixel & 1) == 0 && (pGC->bgPixel&1) == 1)
			pGC->alu = InverseAlu[pGC->alu];
		else if ((pGC->fgPixel & 1) == (pGC->bgPixel & 1))
			pGC->alu = mfbReduceRop(pGC->alu, pGC->fgPixel);
		FFB_ATTR_SFB_VAR_WIN(pFfb, 0x00ffffff, GXcopy, pWin);
		FFBWait(pFfb, ffb);
		if (pSrcDrawable->bitsPerPixel == 32) {
			ret = cfbBitBlt (pSrcDrawable, pDstDrawable,
					 pGC, srcx, srcy, width, height, dstx, dsty,
					 CreatorCopyPlane32to1, bitPlane);
		} else {
			ret = cfbBitBlt (pSrcDrawable, pDstDrawable,
					 pGC, srcx, srcy, width, height, dstx, dsty,
					 cfbCopyPlane8to1, bitPlane);
		}
		pGC->alu = oldalu;
	} else {
		PixmapPtr pBitmap;
		ScreenPtr pScreen = pSrcDrawable->pScreen;
		GCPtr pGC1;

		pBitmap = (*pScreen->CreatePixmap) (pScreen, width, height, 1);
		if (!pBitmap)
			return NULL;
		pGC1 = GetScratchGC (1, pScreen);
		if (!pGC1) {
			(*pScreen->DestroyPixmap) (pBitmap);
			return NULL;
		}
		/*
		 * don't need to set pGC->fgPixel,bgPixel as copyPlane{8,32}to1
		 * ignores pixel values, expecting the rop to "do the
		 * right thing", which GXcopy will.
		 */
		ValidateGC ((DrawablePtr) pBitmap, pGC1);
		/* no exposures here, scratch GC's don't get graphics expose */
		FFB_ATTR_SFB_VAR_WIN(pFfb, 0x00ffffff, GXcopy, pWin);
		FFBWait(pFfb, ffb);
		if (pSrcDrawable->bitsPerPixel == 32) {
			cfbBitBlt (pSrcDrawable, (DrawablePtr) pBitmap,
				   pGC1, srcx, srcy, width, height, 0, 0,
				   CreatorCopyPlane32to1, bitPlane);
		} else {
			cfbBitBlt (pSrcDrawable, (DrawablePtr) pBitmap,
				   pGC1, srcx, srcy, width, height, 0, 0,
				   cfbCopyPlane8to1, bitPlane);
		}
		copyPlaneFG = pGC->fgPixel;
		copyPlaneBG = pGC->bgPixel;
		cfbBitBlt ((DrawablePtr) pBitmap, pDstDrawable, pGC,
			   0, 0, width, height, dstx, dsty, CreatorCopyPlane1toFbBpp, 1);
		FreeScratchGC (pGC1);
		(*pScreen->DestroyPixmap) (pBitmap);
		/* compute resultant exposures */
		ret = miHandleExposures (pSrcDrawable, pDstDrawable, pGC,
					 srcx, srcy, width, height,
					 dstx, dsty, bitPlane);
	}
	return ret;
}
