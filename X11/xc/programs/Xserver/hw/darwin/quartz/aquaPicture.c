/*
 * Support for RENDER extension with rootless Aqua
 */
/*
 * Copyright (c) 2002 Torrey T. Lyons. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written authorization.
 */
/* This file is largely based on fbcompose.c and fbpict.c, which contain
 * the following copyright:
 *
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
 */
 /* $XFree86: xc/programs/Xserver/hw/darwin/quartz/aquaPicture.c,v 1.3 2002/09/28 00:00:03 torrey Exp $ */

#ifdef RENDER

#include "fb.h"
#include "picturestr.h"
#include "mipict.h"
#include "fbpict.h"
#include "aqua.h"

# define mod(a,b)	((b) == 1 ? 0 : (a) >= 0 ? (a) % (b) : (b) - (-a) % (b))


// Replacement for fbStore_x8r8g8b8 that sets the Aqua alpha channel
void
AquaStore_x8r8g8b8 (FbCompositeOperand *op, CARD32 value)
{
    FbBits  *line = op->u.drawable.line; CARD32 offset = op->u.drawable.offset;
    ((CARD32 *)line)[offset >> 5] = (value & 0xffffff) | 0xff000000;
}


// Defined in fbcompose.c
extern FbCombineFunc fbCombineFuncU[];
extern FbCombineFunc fbCombineFuncC[];

void
AquaCompositeGeneral(
    CARD8               op,
    PicturePtr          pSrc,
    PicturePtr          pMask,
    PicturePtr          pDst,
    INT16               xSrc,
    INT16               ySrc,
    INT16               xMask,
    INT16               yMask,
    INT16               xDst,
    INT16               yDst,
    CARD16              width,
    CARD16              height)
{
    FbCompositeOperand	src[4],msk[4],dst[4],*pmsk;
    FbCompositeOperand	*srcPict, *srcAlpha;
    FbCompositeOperand	*dstPict, *dstAlpha;
    FbCompositeOperand	*mskPict = 0, *mskAlpha = 0;
    FbCombineFunc	f;
    int			w;

    if (!fbBuildCompositeOperand (pSrc, src, xSrc, ySrc, TRUE, TRUE))
	return;
    if (!fbBuildCompositeOperand (pDst, dst, xDst, yDst, FALSE, TRUE))
	return;

    // Use Aqua operands for on screen picture formats
    if (pDst->format == PICT_x8r8g8b8) {
        dst[0].store = AquaStore_x8r8g8b8;
    }

    if (pSrc->alphaMap)
    {
	srcPict = &src[1];
	srcAlpha = &src[2];
    }
    else
    {
	srcPict = &src[0];
	srcAlpha = 0;
    }
    if (pDst->alphaMap)
    {
	dstPict = &dst[1];
	dstAlpha = &dst[2];
    }
    else
    {
	dstPict = &dst[0];
	dstAlpha = 0;
    }
    f = fbCombineFuncU[op];
    if (pMask)
    {
	if (!fbBuildCompositeOperand (pMask, msk, xMask, yMask, TRUE, TRUE))
	    return;
	pmsk = msk;
	if (pMask->componentAlpha)
	    f = fbCombineFuncC[op];
	if (pMask->alphaMap)
	{
	    mskPict = &msk[1];
	    mskAlpha = &msk[2];
	}
	else
	{
	    mskPict = &msk[0];
	    mskAlpha = 0;
	}
    }
    else
	pmsk = 0;
    while (height--)
    {
	w = width;
	
	while (w--)
	{
	    (*f) (src, pmsk, dst);
	    (*src->over) (src);
	    (*dst->over) (dst);
	    if (pmsk)
		(*pmsk->over) (pmsk);
	}
	(*src->down) (src);
	(*dst->down) (dst);
	if (pmsk)
	    (*pmsk->down) (pmsk);
    }
}


void
AquaComposite(
    CARD8           op,
    PicturePtr      pSrc,
    PicturePtr      pMask,
    PicturePtr      pDst,
    INT16           xSrc,
    INT16           ySrc,
    INT16           xMask,
    INT16           yMask,
    INT16           xDst,
    INT16           yDst,
    CARD16          width,
    CARD16          height)
{
    RegionRec	    region;
    int		    n;
    BoxPtr	    pbox;
    CompositeFunc   func;
    Bool	    srcRepeat = pSrc->repeat;
    Bool	    maskRepeat = FALSE;
    Bool	    maskAlphaMap = FALSE;
    int		    x_msk, y_msk, x_src, y_src, x_dst, y_dst;
    int		    w, h, w_this, h_this;

    xDst += pDst->pDrawable->x;
    yDst += pDst->pDrawable->y;
    xSrc += pSrc->pDrawable->x;
    ySrc += pSrc->pDrawable->y;
    if (pMask)
    {
	xMask += pMask->pDrawable->x;
	yMask += pMask->pDrawable->y;
	maskRepeat = pMask->repeat;
	maskAlphaMap = pMask->alphaMap != 0;
    }

    if (!miComputeCompositeRegion (&region,
				   pSrc,
				   pMask,
				   pDst,
				   xSrc,
				   ySrc,
				   xMask,
				   yMask,
				   xDst,
				   yDst,
				   width,
				   height))
	return;

    // To preserve the alpha channel we only use a special,
    // non-optimzied compositor.
    func = AquaCompositeGeneral;

    n = REGION_NUM_RECTS (&region);
    pbox = REGION_RECTS (&region);
    while (n--)
    {
	h = pbox->y2 - pbox->y1;
	y_src = pbox->y1 - yDst + ySrc;
	y_msk = pbox->y1 - yDst + yMask;
	y_dst = pbox->y1;
	while (h)
	{
	    h_this = h;
	    w = pbox->x2 - pbox->x1;
	    x_src = pbox->x1 - xDst + xSrc;
	    x_msk = pbox->x1 - xDst + xMask;
	    x_dst = pbox->x1;
	    if (maskRepeat)
	    {
		y_msk = mod (y_msk, pMask->pDrawable->height);
		if (h_this > pMask->pDrawable->height - y_msk)
		    h_this = pMask->pDrawable->height - y_msk;
	    }
	    if (srcRepeat)
	    {
		y_src = mod (y_src, pSrc->pDrawable->height);
		if (h_this > pSrc->pDrawable->height - y_src)
		    h_this = pSrc->pDrawable->height - y_src;
	    }
	    while (w)
	    {
		w_this = w;
		if (maskRepeat)
		{
		    x_msk = mod (x_msk, pMask->pDrawable->width);
		    if (w_this > pMask->pDrawable->width - x_msk)
			w_this = pMask->pDrawable->width - x_msk;
		}
		if (srcRepeat)
		{
		    x_src = mod (x_src, pSrc->pDrawable->width);
		    if (w_this > pSrc->pDrawable->width - x_src)
			w_this = pSrc->pDrawable->width - x_src;
		}
		(*func) (op, pSrc, pMask, pDst,
			 x_src, y_src, x_msk, y_msk, x_dst, y_dst,
			 w_this, h_this);
		w -= w_this;
		x_src += w_this;
		x_msk += w_this;
		x_dst += w_this;
	    }
	    h -= h_this;
	    y_src += h_this;
	    y_msk += h_this;
	    y_dst += h_this;
	}
	pbox++;
    }
    REGION_UNINIT (pDst->pDrawable->pScreen, &region);
}

#endif /* RENDER */
