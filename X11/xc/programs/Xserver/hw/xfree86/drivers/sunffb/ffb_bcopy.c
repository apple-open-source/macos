/*
 * Acceleration for the Creator and Creator3D framebuffer - Bit-blit copies.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_bcopy.c,v 1.2 2000/05/23 04:47:44 dawes Exp $ */

#include "ffb.h"
#include "ffb_regs.h"
#include "ffb_rcache.h"
#include "ffb_fifo.h"
#include "ffb_loops.h"

#include "pixmapstr.h"
#include "scrnintstr.h"

#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb32.h"
#include "cfb8_32wid.h"

#ifdef FFB_BLOCKCOPY_IMPLEMENTED
/* Due to VIS based copyarea and ffb rop vertscroll being significantly faster
 * than the blockcopy rop, blockcopy was not implemented at all in the final
 * FFB hardware design.  This code is left here for hack value.  -DaveM
 */
#error To use it, we would need to fix this to use WIDs in FFB attributes -DaveM
void
CreatorDoHWBitblt(DrawablePtr pSrc, DrawablePtr pDst, int alu, RegionPtr prgnDst,
		  DDXPointPtr pptSrc, unsigned long planemask)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pSrc->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
	BoxPtr pboxTmp;
	DDXPointPtr pptTmp;
	int nbox;
	BoxPtr pboxNext, pboxBase, pbox;

	nbox = REGION_NUM_RECTS(prgnDst);
	pbox = REGION_RECTS(prgnDst);

	FFB_WRITE_ATTRIBUTES(pFfb,
			     FFB_PPC_ACE_DISABLE|FFB_PPC_APE_DISABLE|FFB_PPC_CS_CONST,
			     FFB_PPC_ACE_MASK|FFB_PPC_APE_MASK|FFB_PPC_CS_MASK,
			     planemask,
			     FFB_ROP_NEW,
			     FFB_DRAWOP_BCOPY, pFfb->fg_cache,
			     FFB_FBC_DEFAULT);
	
	/* need to blit rectangles in different orders, depending on the direction of copy
           so that an area isnt overwritten before it is blitted */
	if (pptSrc->y < pbox->y1 && nbox > 1) {
		if (pptSrc->x < pbox->x1) {
			pboxTmp = pbox + nbox;
			pptTmp = pptSrc + nbox;
			while(nbox--) {
				pboxTmp--;
				pptTmp--;
				FFBFifo(pFfb, 6);
				FFB_WRITE64(&ffb->by, pptTmp->y, pptTmp->x);
				FFB_WRITE64_2(&ffb->dy, pboxTmp->y1, pboxTmp->x1);
				FFB_WRITE64_3(&ffb->bh,
					      (pboxTmp->y2 - pboxTmp->y1),
					      (pboxTmp->x2 - pboxTmp->x1));
			}
		} else {
			/* keep ordering in each band, reverse order of bands */
			pboxBase = pboxNext = pbox+nbox-1;
			
			while (pboxBase >= pbox) { /* for each band */
				/* find first box in band */
				while (pboxNext >= pbox && pboxBase->y1 == pboxNext->y1)
					pboxNext--;
		
				pboxTmp = pboxNext + 1;			/* first box in band */
				pptTmp = pptSrc + (pboxTmp - pbox);	/* first point in band */
		
				while (pboxTmp <= pboxBase) { /* for each box in band */
					FFBFifo(pFfb, 6);
					FFB_WRITE64(&ffb->by, pptTmp->y, pptTmp->x);
					FFB_WRITE64_2(&ffb->dy, pboxTmp->y1, pboxTmp->x1);
					FFB_WRITE64_3(&ffb->bh,
						      (pboxTmp->y2 - pboxTmp->y1),
						      (pboxTmp->x2 - pboxTmp->x1));
					++pboxTmp;
					++pptTmp;	
				}
				pboxBase = pboxNext;
			}
		}
	} else {
		if((pptSrc->x < pbox->x1) && (nbox > 1)) {
			pboxBase = pboxNext = pbox;
			while(pboxBase < pbox+nbox) {
				while((pboxNext<pbox+nbox) &&
				      (pboxNext->y1 == pboxBase->y1))
					pboxNext++;
				pboxTmp = pboxNext;
				pptTmp = pptSrc + (pboxTmp - pbox);
				while(pboxTmp != pboxBase) {
					--pboxTmp;
					--pptTmp;
					FFBFifo(pFfb, 6);
					FFB_WRITE64(&ffb->by, pptTmp->y, pptTmp->x);
					FFB_WRITE64_2(&ffb->dy, pboxTmp->y1, pboxTmp->x1);
					FFB_WRITE64_3(&ffb->bh,
						      (pboxTmp->y2 - pboxTmp->y1),
						      (pboxTmp->x2 - pboxTmp->x1));
				}
				pboxBase = pboxNext;
			}
		} else {
			/* dont need to change order of anything */
			pptTmp = pptSrc;
			pboxTmp = pbox;
	    
			while (nbox--) {
				FFBFifo(pFfb, 6);
				FFB_WRITE64(&ffb->by, pptTmp->y, pptTmp->x);
				FFB_WRITE64_2(&ffb->dy, pboxTmp->y1, pboxTmp->x1);
				FFB_WRITE64_3(&ffb->bh,
					      (pboxTmp->y2 - pboxTmp->y1),
					      (pboxTmp->x2 - pboxTmp->x1));
				pboxTmp++;
				pptTmp++;
			}
		}
	}
	pFfb->rp_active = 1;
	FFBSync(pFfb, ffb);
}
#endif /* FFB_BLOCKCOPY_IMPLEMENTED */

/* We know here that only y is changing and that the hw attributes
 * have been set higher up in the call chain.
 */
void
CreatorDoVertBitblt(DrawablePtr pSrc, DrawablePtr pDst, int alu, RegionPtr prgnDst,
		    DDXPointPtr pptSrc, unsigned long planemask)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pSrc->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
	BoxPtr pbox;
	int nbox;

	pbox = REGION_RECTS(prgnDst);
	nbox = REGION_NUM_RECTS(prgnDst);

	/* No garbage please. */
	if(nbox <= 0)
		return;

	FFBLOG(("VSCROLL(%d): ", nbox));

	/* Need to blit rectangles in different orders, depending
	 * on the direction of copy so that an area isnt overwritten
	 * before it is blitted.
	 */
	if (nbox > 1 && pptSrc->y < pbox->y1) {
		BoxPtr pboxBase = pbox + nbox - 1;
		BoxPtr pboxNext = pboxBase;

		/* Keep ordering in each band, reverse order of bands. */
		while (pboxBase >= pbox) { /* for each band */
			BoxPtr pboxTmp;
			DDXPointPtr pptTmp;

			/* find first box in band */
			while (pboxNext >= pbox && pboxBase->y1 == pboxNext->y1)
				pboxNext--;
		
			pboxTmp = pboxNext + 1;			/* first box in band */
			pptTmp = pptSrc + (pboxTmp - pbox);	/* first point in band */
			while (pboxTmp <= pboxBase) {
				FFBLOG(("1[%08x:%08x:%08x:%08x:%08x:%08x] ",
					pptTmp->x, pptTmp->y, pboxTmp->x1, pboxTmp->y1,
					pboxTmp->x2, pboxTmp->y2));
				FFBFifo(pFfb, 7);
				ffb->drawop = FFB_DRAWOP_VSCROLL;
				FFB_WRITE64(&ffb->by, pptTmp->y, pptTmp->x);
				FFB_WRITE64_2(&ffb->dy, pboxTmp->y1, pboxTmp->x1);
				FFB_WRITE64_3(&ffb->bh, (pboxTmp->y2 - pboxTmp->y1),
					      (pboxTmp->x2 - pboxTmp->x1));
				pboxTmp++;
				pptTmp++;
			}
			pboxBase = pboxNext;
		}
	} else {
		/* Dont need to change order of anything. */
		while (nbox--) {
			FFBLOG(("2[%08x:%08x:%08x:%08x:%08x:%08x] ",
				pptSrc->x, pptSrc->y, pbox->x1, pbox->y1,
				pbox->x2, pbox->y2));
			FFBFifo(pFfb, 7);
			ffb->drawop = FFB_DRAWOP_VSCROLL;
			FFB_WRITE64(&ffb->by, pptSrc->y, pptSrc->x);
			FFB_WRITE64_2(&ffb->dy, pbox->y1, pbox->x1);
			FFB_WRITE64_3(&ffb->bh, (pbox->y2 - pbox->y1),
				      (pbox->x2 - pbox->x1));
			pbox++;
			pptSrc++;
		}
	}
	pFfb->rp_active = 1;
	FFBLOG(("done\n"));
	FFBSync(pFfb, ffb);
}

extern void VISmoveImageLR(unsigned char *, unsigned char *, long, long, long, long);
extern void VISmoveImageRL(unsigned char *, unsigned char *, long, long, long, long);

/* The hw attributes have been set by someone higher up in the call
 * chain.
 */
void
CreatorDoBitblt(DrawablePtr pSrc, DrawablePtr pDst, int alu, RegionPtr prgnDst,
		DDXPointPtr pptSrc, unsigned long planemask)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDst->pScreen);
	BoxPtr pboxTmp, pboxNext, pboxBase, pbox;
	DDXPointPtr pptTmp;
	unsigned char *psrcBase, *pdstBase;
	int nbox, widthSrc, widthDst, careful, use_prefetch;
	int psz_shift;

	cfbGetByteWidthAndPointer (pSrc, widthSrc, psrcBase)
	cfbGetByteWidthAndPointer (pDst, widthDst, pdstBase)
	
	careful = ((pSrc == pDst) ||
		  ((pSrc->type == DRAWABLE_WINDOW) &&
		   (pDst->type == DRAWABLE_WINDOW)));
	use_prefetch = (pFfb->use_blkread_prefetch &&
			(psrcBase == (unsigned char *)pFfb->sfb32 ||
			 psrcBase == (unsigned char *)pFfb->sfb8r));

	pbox = REGION_RECTS(prgnDst);
	nbox = REGION_NUM_RECTS(prgnDst);

	pptTmp = pptSrc;
	pboxTmp = pbox;

	FFBLOG(("GCOPY(%d): ", nbox));

	if (pSrc->bitsPerPixel == 8)
		psz_shift = 0;
	else
		psz_shift = 2;

	if (careful && pptSrc->y < pbox->y1) {
		if (pptSrc->x < pbox->x1) {
			/* reverse order of bands and rects in each band */
			pboxTmp=pbox+nbox;
                        pptTmp=pptSrc+nbox;

			while (nbox--){
				pboxTmp--;
				pptTmp--;
				FFBLOG(("[%08x:%08x:%08x:%08x:%08x:%08x] ",
					pptTmp->x, pptTmp->y, pboxTmp->x1, pboxTmp->y1,
					pboxTmp->x2, pboxTmp->y2));
				if (pptTmp->x < pbox->x2) {
					if (use_prefetch) {
						FFBFifo(pFfb, 1);
						pFfb->regs->mer = FFB_MER_EDRA;
						pFfb->rp_active = 1;
						FFBWait(pFfb, pFfb->regs);
					}
					VISmoveImageRL ((psrcBase +
							 ((pptTmp->y + pboxTmp->y2 - pboxTmp->y1 - 1) *
							  widthSrc) +
							 (pptTmp->x << psz_shift)),
				        	        (pdstBase +
							 ((pboxTmp->y2 - 1) * widthDst) +
							 (pboxTmp->x1 << psz_shift)),
					                (pboxTmp->x2 - pboxTmp->x1) << psz_shift,
				        	        (pboxTmp->y2 - pboxTmp->y1),
					                -widthSrc, -widthDst);
				} else {
					if (use_prefetch) {
						FFBFifo(pFfb, 1);
						pFfb->regs->mer = FFB_MER_EIRA;
						pFfb->rp_active = 1;
						FFBWait(pFfb, pFfb->regs);
					}
					VISmoveImageLR ((psrcBase +
							 ((pptTmp->y + pboxTmp->y2 - pboxTmp->y1 - 1) *
							  widthSrc) +
							 (pptTmp->x << psz_shift)),
				        	        (pdstBase +
							 ((pboxTmp->y2 - 1) * widthDst) +
							 (pboxTmp->x1 << psz_shift)),
					                (pboxTmp->x2 - pboxTmp->x1) << psz_shift,
				        	        (pboxTmp->y2 - pboxTmp->y1),
					                -widthSrc, -widthDst);
				}
			}
		} else {
			/* keep ordering in each band, reverse order of bands */
			pboxBase = pboxNext = pbox+nbox-1;

			while (pboxBase >= pbox) {			/* for each band */

				/* find first box in band */
				while (pboxNext >= pbox &&
				       pboxBase->y1 == pboxNext->y1)
					pboxNext--;
		
				pboxTmp = pboxNext+1;			/* first box in band */
				pptTmp = pptSrc + (pboxTmp - pbox);	/* first point in band */
		
				FFBLOG(("[%08x:%08x:%08x:%08x:%08x:%08x] ",
					pptTmp->x, pptTmp->y, pboxTmp->x1, pboxTmp->y1,
					pboxTmp->x2, pboxTmp->y2));
				while (pboxTmp <= pboxBase) {		/* for each box in band */
					if (use_prefetch) {
						FFBFifo(pFfb, 1);
						pFfb->regs->mer = FFB_MER_EIRA;
						pFfb->rp_active = 1;
						FFBWait(pFfb, pFfb->regs);
					}
					VISmoveImageLR ((psrcBase +
							 ((pptTmp->y + pboxTmp->y2 - pboxTmp->y1 - 1) *
							  widthSrc) +
							 (pptTmp->x << psz_shift)),
				        	        (pdstBase +
							 ((pboxTmp->y2 - 1) * widthDst) +
							 (pboxTmp->x1 << psz_shift)),
					                (pboxTmp->x2 - pboxTmp->x1) << psz_shift,
				        	        (pboxTmp->y2 - pboxTmp->y1),
					                -widthSrc, -widthDst);
					++pboxTmp;
					++pptTmp;	
				}
				pboxBase = pboxNext;
			
			}
		}
	} else {
		if (careful && pptSrc->x < pbox->x1) {
			/* reverse order of rects in each band */

			pboxBase = pboxNext = pbox;

			while (pboxBase < pbox+nbox) { /* for each band */

				/* find last box in band */
				while (pboxNext < pbox+nbox &&
				       pboxNext->y1 == pboxBase->y1)
					pboxNext++;

				pboxTmp = pboxNext;			/* last box in band */
				pptTmp = pptSrc + (pboxTmp - pbox);	/* last point in band */

				while (pboxTmp != pboxBase) {		/* for each box in band */
					--pboxTmp;
					--pptTmp;
					FFBLOG(("[%08x:%08x:%08x:%08x:%08x:%08x] ",
						pptTmp->x, pptTmp->y, pboxTmp->x1, pboxTmp->y1,
						pboxTmp->x2, pboxTmp->y2));
					if (pptTmp->x < pbox->x2) {
						if (use_prefetch) {
							FFBFifo(pFfb, 1);
							pFfb->regs->mer = FFB_MER_EDRA;
							pFfb->regs->mer = FFB_MER_EIRA;
							pFfb->rp_active = 1;
						}
						VISmoveImageRL ((psrcBase +
								 (pptTmp->y * widthSrc) +
								 (pptTmp->x << psz_shift)),
						                (pdstBase +
								 (pboxTmp->y1 * widthDst) +
								 (pboxTmp->x1 << psz_shift)),
						                (pboxTmp->x2 - pboxTmp->x1) << psz_shift,
			        	        		(pboxTmp->y2 - pboxTmp->y1),
						                widthSrc, widthDst);
					} else {
						if (use_prefetch) {
							FFBFifo(pFfb, 1);
							pFfb->regs->mer = FFB_MER_EIRA;
							pFfb->rp_active = 1;
							FFBWait(pFfb, pFfb->regs);
						}
						VISmoveImageLR ((psrcBase +
								 (pptTmp->y * widthSrc) +
								 (pptTmp->x << psz_shift)),
						                (pdstBase +
								 (pboxTmp->y1 * widthDst) +
								 (pboxTmp->x1 << psz_shift)),
				                		(pboxTmp->x2 - pboxTmp->x1) << psz_shift,
					        	        (pboxTmp->y2 - pboxTmp->y1),
						                widthSrc, widthDst);
					}
				}
				pboxBase = pboxNext;
			}
		} else {
			while (nbox--) {
				FFBLOG(("[%08x:%08x:%08x:%08x:%08x:%08x] ",
					pptTmp->x, pptTmp->y, pboxTmp->x1, pboxTmp->y1,
					pboxTmp->x2, pboxTmp->y2));
				if (use_prefetch) {
					FFBFifo(pFfb, 1);
					pFfb->regs->mer = FFB_MER_EIRA;
					pFfb->rp_active = 1;
					FFBWait(pFfb, pFfb->regs);
				}
				VISmoveImageLR ((psrcBase +
						 (pptTmp->y * widthSrc) +
						 (pptTmp->x << psz_shift)),
				                (pdstBase +
						 (pboxTmp->y1 * widthDst) +
						 (pboxTmp->x1 << psz_shift)),
				                (pboxTmp->x2 - pboxTmp->x1) << psz_shift,
			        	        (pboxTmp->y2 - pboxTmp->y1),
				                widthSrc, widthDst);
				pboxTmp++;
				pptTmp++;
			}
		}
	}
	if (use_prefetch) {
		FFBFifo(pFfb, 1);
		pFfb->regs->mer = FFB_MER_DRA;
		pFfb->rp_active = 1;
		FFBWait(pFfb, pFfb->regs);
	}
	FFBLOG(("done\n"));
}

RegionPtr
CreatorCopyArea(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable,
		GCPtr pGC, int srcx, int srcy, int width, int height, int dstx, int dsty)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pDstDrawable->pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
	RegionPtr ret;
	unsigned char *dptr, *sptr, *sfb;
	int garbage, all_planes;
	
	cfbGetByteWidthAndPointer (pDstDrawable, garbage, dptr);
	cfbGetByteWidthAndPointer (pSrcDrawable, garbage, sptr);
	if (pSrcDrawable->bitsPerPixel == 8) {
		sfb = (unsigned char *) pFfb->sfb8r;
		all_planes = 0xff;
	} else {
		sfb = (unsigned char *) pFfb->sfb32;
		all_planes = 0xffffff;
	}

	FFBLOG(("CreatorCopyArea: SFB(%p) s(%p) d(%p) alu(%x) pmsk(%08x) "
		"src(%08x:%08x) dst(%08x:%08x)\n",
		sfb, sptr, dptr, pGC->alu, pGC->planemask,
		srcx, srcy, dstx, dsty));
	if (((pGC->planemask & all_planes) != all_planes || pGC->alu != GXcopy) &&
	    dptr != sfb) {
		if(sptr == sfb) {
			WindowPtr pWin = (WindowPtr) pSrcDrawable;

			FFB_ATTR_SFB_VAR_WIN(pFfb, pGC->planemask, pGC->alu, pWin);
			FFBWait(pFfb, ffb);
		}
		if (pSrcDrawable->bitsPerPixel == 8)
			return cfbCopyArea (pSrcDrawable, pDstDrawable,
					    pGC, srcx, srcy, width, height, dstx, dsty);
		else
			return cfb32CopyArea (pSrcDrawable, pDstDrawable,
					      pGC, srcx, srcy, width, height, dstx, dsty);
	}

	/* Try to use hw VSCROLL if possible */
	if (!pFfb->disable_vscroll &&	/* must not be ffb1 in hires */
	    pGC->alu == GXcopy &&	/* it must be a copy */
	    dstx == srcx &&		/* X must be unchanging */
	    dsty != srcy &&		/* Y must be changing */
	    sptr == dptr &&		/* src and dst must be the framebuffer */
	    dptr == sfb) {
		WindowPtr pWin = (WindowPtr) pSrcDrawable;
		CreatorPrivWinPtr pFfbPrivWin = CreatorGetWindowPrivate(pWin);
		unsigned int fbc = pFfbPrivWin->fbc_base;
		int same_buffer;

		/* One last check, the read buffer and the write buffer
		 * must be the same.  VSCROLL only allows to move pixels
		 * within the same buffer.
		 */
		if (!pFfb->has_double_buffer) {
			same_buffer = 1;
		} else {
			same_buffer = 0;
			if ((((fbc & FFB_FBC_WB_MASK) == FFB_FBC_WB_A) &&
			     ((fbc & FFB_FBC_RB_MASK) == FFB_FBC_RB_A)) ||
			    (((fbc & FFB_FBC_WB_MASK) == FFB_FBC_WB_B) &&
			     ((fbc & FFB_FBC_RB_MASK) == FFB_FBC_RB_B)))
				same_buffer = 1;
		}

		if (same_buffer != 0) {
			FFB_ATTR_VSCROLL_WIN(pFfb, pGC->planemask, pWin);
			if (pSrcDrawable->bitsPerPixel == 8)
				ret = cfbBitBlt (pSrcDrawable, pDstDrawable,
						 pGC, srcx, srcy, width, height,
						 dstx, dsty,
						 (void (*)())CreatorDoVertBitblt, 0);
			else
				ret = cfb32BitBlt (pSrcDrawable, pDstDrawable,
						   pGC, srcx, srcy, width, height,
						   dstx, dsty,
						   (void (*)())CreatorDoVertBitblt, 0);
			FFBLOG(("CreatorCopyArea: Done, returning %p\n", ret));
			return ret;
		}
	}

	/* OK, we have to use GCOPY. */

	/* Even when we are only reading from the framebuffer, we must
	 * set the SFB_VAR attributes to handle double-buffering correctly.
	 */
	if(dptr == sfb || sptr == sfb) {
		WindowPtr pWin;

		if (dptr == sfb)
			pWin = (WindowPtr) pDstDrawable;
		else
			pWin = (WindowPtr) pSrcDrawable;
		FFB_ATTR_SFB_VAR_WIN(pFfb, pGC->planemask, pGC->alu, pWin);
		FFBWait(pFfb, ffb);
	}
	if (pSrcDrawable->bitsPerPixel == 8)
		ret = cfbBitBlt (pSrcDrawable, pDstDrawable,
				 pGC, srcx, srcy, width, height,
				 dstx, dsty, (void (*)())CreatorDoBitblt, 0);
	else
		ret = cfb32BitBlt (pSrcDrawable, pDstDrawable,
				   pGC, srcx, srcy, width, height,
				   dstx, dsty, (void (*)())CreatorDoBitblt, 0);

	FFBLOG(("CreatorCopyArea: Done, returning %p\n", ret));
	return ret;
}
