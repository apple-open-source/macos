/*
static char *rid="$Xorg: sunGX.c,v 1.5 2001/02/09 02:04:44 xorgcvs Exp $";
 */
/*
Copyright 1991, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 *
 * Author:  Keith Packard, MIT X Consortium
 */

/* $XFree86: xc/programs/Xserver/hw/sun/sunGX.c,v 1.9 2003/11/17 22:20:36 dawes Exp $ */

#include	"sun.h"

#include	"Xmd.h"
#include	"gcstruct.h"
#include	"scrnintstr.h"
#include	"pixmapstr.h"
#include	"regionstr.h"
#include	"mistruct.h"
#include	"fontstruct.h"
#include	"dixfontstr.h"
#include	"cfb/cfb.h"
#include	"cfb/cfbmskbits.h"
#include	"cfb/cfb8bit.h"
#include	"fastblt.h"
#include	"mergerop.h"
#include	"sunGX.h"
#include	"migc.h"
#include	"mispans.h"

#define sunGXFillSpan(gx,y,x1,x2,r) {\
    (gx)->apointy = (y); \
    (gx)->apointx = (x1); \
    (gx)->arectx = (x2); \
    GXDrawDone(gx,r); \
}

#define GXSetClip(gx,pbox) {\
    (gx)->clipminx = (pbox)->x1; \
    (gx)->clipminy = (pbox)->y1; \
    (gx)->clipmaxx = (pbox)->x2 - 1; \
    (gx)->clipmaxy = (pbox)->y2 - 1; \
}

#define GXSetOff(gx,x,y) {\
    (gx)->offx = (x); \
    (gx)->offy = (y); \
}

#define GXResetClip(gx,pScreen) { \
    (gx)->clipminx = 0; \
    (gx)->clipminy = 0; \
    (gx)->clipmaxx = (pScreen)->width - 1; \
    (gx)->clipmaxy = (pScreen)->height - 1; \
}

#define GXResetOff(gx) {\
    (gx)->offx = 0; \
    (gx)->offy = 0; \
}

#define sunGXGetAddrRange(pDrawable,extents,base,lo,hi) {\
    int	__x__; \
    cfbGetWindowByteWidthAndPointer((WindowPtr)pDrawable,__x__,base); \
    lo = (base) + WIDTH_MUL((extents)->y1) + (extents)->x1; \
    hi = (base) + WIDTH_MUL((extents)->y2 - 1) + (extents)->x2 - 1; \
    (base) = (base) + WIDTH_MUL(pDrawable->y) + pDrawable->x; \
}

/*
   rop_tables
   ==========
   lookup tables for GX raster ops, with the plane_mask,pixel_mask,pattern_mask
   ,attrib_sel, polygon_draw,raster_mode encoded into the top half.
   There is a lookup table for each commonly used combination.
*/

/* rops for bit blit / copy area
   with:
       Plane Mask - use plane mask reg.
       Pixel Mask - use all ones.
       Patt  Mask - use all ones.
*/

#define POLY_O		GX_POLYG_OVERLAP
#define POLY_N		GX_POLYG_NONOVERLAP

#define ROP_STANDARD	(GX_PLANE_MASK |\
			GX_PIXEL_ONES |\
			GX_ATTR_SUPP |\
			GX_RAST_BOOL |\
			GX_PLOT_PLOT)

/* fg = don't care  bg = don't care */

#define ROP_BLIT(O,I)	(ROP_STANDARD | \
			GX_PATTERN_ONES |\
			GX_ROP_11_1(I) |\
			GX_ROP_11_0(O) |\
			GX_ROP_10_1(I) |\
			GX_ROP_10_0(O) |\
			GX_ROP_01_1(I) |\
			GX_ROP_01_0(O) |\
			GX_ROP_00_1(I) |\
			GX_ROP_00_0(O))

/* fg = fgPixel	    bg = don't care */

#define ROP_FILL(O,I)	(ROP_STANDARD | \
			GX_PATTERN_ONES |\
			GX_ROP_11_1(I) |\
			GX_ROP_11_0(I) |\
			GX_ROP_10_1(I) |\
			GX_ROP_10_0(I) | \
			GX_ROP_01_1(O) |\
			GX_ROP_01_0(O) |\
			GX_ROP_00_1(O) |\
			GX_ROP_00_0(O))

/* fg = fgPixel	    bg = don't care */
 
#define ROP_STIP(O,I)   (ROP_STANDARD |\
			GX_ROP_11_1(I) |\
			GX_ROP_11_0(GX_ROP_NOOP) |\
			GX_ROP_10_1(I) |\
			GX_ROP_10_0(GX_ROP_NOOP) | \
			GX_ROP_01_1(O) |\
			GX_ROP_01_0(GX_ROP_NOOP) |\
			GX_ROP_00_1(O) |\
			GX_ROP_00_0(GX_ROP_NOOP))

/* fg = fgPixel	    bg = bgPixel */
			    
#define ROP_OSTP(O,I)   (ROP_STANDARD |\
			GX_ROP_11_1(I) |\
			GX_ROP_11_0(I) |\
			GX_ROP_10_1(I) |\
			GX_ROP_10_0(O) |\
			GX_ROP_01_1(O) |\
			GX_ROP_01_0(I) |\
			GX_ROP_00_1(O) |\
			GX_ROP_00_0(O))

#define ROP_ITXT(O,I)   (ROP_STANDARD |\
			GX_PATTERN_ONES |\
			GX_ROP_11_1(I) |\
			GX_ROP_11_0(I) |\
			GX_ROP_10_1(I) |\
			GX_ROP_10_0(O) |\
			GX_ROP_01_1(O) |\
			GX_ROP_01_0(I) |\
			GX_ROP_00_1(O) |\
			GX_ROP_00_0(O))

#define ROP_PTXT(O,I)   (ROP_STANDARD |\
			GX_PATTERN_ONES |\
			GX_ROP_11_1(I) |\
			GX_ROP_11_0(GX_ROP_NOOP) |\
			GX_ROP_10_1(I) |\
			GX_ROP_10_0(GX_ROP_NOOP) | \
			GX_ROP_01_1(O) |\
			GX_ROP_01_0(GX_ROP_NOOP) |\
			GX_ROP_00_1(O) |\
			GX_ROP_00_0(GX_ROP_NOOP))

static Uint gx_blit_rop_table[16]={
    ROP_BLIT(GX_ROP_CLEAR,  GX_ROP_CLEAR),	/* GXclear */
    ROP_BLIT(GX_ROP_CLEAR,  GX_ROP_NOOP),	/* GXand */
    ROP_BLIT(GX_ROP_CLEAR,  GX_ROP_INVERT),	/* GXandReverse */
    ROP_BLIT(GX_ROP_CLEAR,  GX_ROP_SET),	/* GXcopy */
    ROP_BLIT(GX_ROP_NOOP,   GX_ROP_CLEAR),	/* GXandInverted */
    ROP_BLIT(GX_ROP_NOOP,   GX_ROP_NOOP),	/* GXnoop */
    ROP_BLIT(GX_ROP_NOOP,   GX_ROP_INVERT),	/* GXxor */
    ROP_BLIT(GX_ROP_NOOP,   GX_ROP_SET),	/* GXor */
    ROP_BLIT(GX_ROP_INVERT, GX_ROP_CLEAR),	/* GXnor */
    ROP_BLIT(GX_ROP_INVERT, GX_ROP_NOOP),	/* GXequiv */
    ROP_BLIT(GX_ROP_INVERT, GX_ROP_INVERT),	/* GXinvert */
    ROP_BLIT(GX_ROP_INVERT, GX_ROP_SET),	/* GXorReverse */
    ROP_BLIT(GX_ROP_SET,    GX_ROP_CLEAR),	/* GXcopyInverted */
    ROP_BLIT(GX_ROP_SET,    GX_ROP_NOOP),	/* GXorInverted */
    ROP_BLIT(GX_ROP_SET,    GX_ROP_INVERT),	/* GXnand */
    ROP_BLIT(GX_ROP_SET,    GX_ROP_SET),	/* GXset */
};

/* rops for solid drawing
   with:
       Plane Mask - use plane mask reg.
       Pixel Mask - use all ones.
       Patt  Mask - use all ones.
*/

static Uint gx_solid_rop_table[16]={
    ROP_FILL(GX_ROP_CLEAR,  GX_ROP_CLEAR),	/* GXclear */
    ROP_FILL(GX_ROP_CLEAR,  GX_ROP_NOOP),	/* GXand */
    ROP_FILL(GX_ROP_CLEAR,  GX_ROP_INVERT),	/* GXandReverse */
    ROP_FILL(GX_ROP_CLEAR,  GX_ROP_SET),	/* GXcopy */
    ROP_FILL(GX_ROP_NOOP,   GX_ROP_CLEAR),	/* GXandInverted */
    ROP_FILL(GX_ROP_NOOP,   GX_ROP_NOOP),	/* GXnoop */
    ROP_FILL(GX_ROP_NOOP,   GX_ROP_INVERT),	/* GXxor */
    ROP_FILL(GX_ROP_NOOP,   GX_ROP_SET),	/* GXor */
    ROP_FILL(GX_ROP_INVERT, GX_ROP_CLEAR),	/* GXnor */
    ROP_FILL(GX_ROP_INVERT, GX_ROP_NOOP),	/* GXequiv */
    ROP_FILL(GX_ROP_INVERT, GX_ROP_INVERT),	/* GXinvert */
    ROP_FILL(GX_ROP_INVERT, GX_ROP_SET),	/* GXorReverse */
    ROP_FILL(GX_ROP_SET,    GX_ROP_CLEAR),	/* GXcopyInverted */
    ROP_FILL(GX_ROP_SET,    GX_ROP_NOOP),	/* GXorInverted */
    ROP_FILL(GX_ROP_SET,    GX_ROP_INVERT),	/* GXnand */
    ROP_FILL(GX_ROP_SET,    GX_ROP_SET),	/* GXset */
};

static Uint gx_stipple_rop_table[16]={
    ROP_STIP(GX_ROP_CLEAR,  GX_ROP_CLEAR),	/* GXclear */
    ROP_STIP(GX_ROP_CLEAR,  GX_ROP_NOOP),	/* GXand */
    ROP_STIP(GX_ROP_CLEAR,  GX_ROP_INVERT),	/* GXandReverse */
    ROP_STIP(GX_ROP_CLEAR,  GX_ROP_SET),	/* GXcopy */
    ROP_STIP(GX_ROP_NOOP,   GX_ROP_CLEAR),	/* GXandInverted */
    ROP_STIP(GX_ROP_NOOP,   GX_ROP_NOOP),	/* GXnoop */
    ROP_STIP(GX_ROP_NOOP,   GX_ROP_INVERT),	/* GXxor */
    ROP_STIP(GX_ROP_NOOP,   GX_ROP_SET),	/* GXor */
    ROP_STIP(GX_ROP_INVERT, GX_ROP_CLEAR),	/* GXnor */
    ROP_STIP(GX_ROP_INVERT, GX_ROP_NOOP),	/* GXequiv */
    ROP_STIP(GX_ROP_INVERT, GX_ROP_INVERT),	/* GXinvert */
    ROP_STIP(GX_ROP_INVERT, GX_ROP_SET),	/* GXorReverse */
    ROP_STIP(GX_ROP_SET,    GX_ROP_CLEAR),	/* GXcopyInverted */
    ROP_STIP(GX_ROP_SET,    GX_ROP_NOOP),	/* GXorInverted */
    ROP_STIP(GX_ROP_SET,    GX_ROP_INVERT),	/* GXnand */
    ROP_STIP(GX_ROP_SET,    GX_ROP_SET),	/* GXset */
};

static Uint gx_opaque_stipple_rop_table[16]={
    ROP_OSTP(GX_ROP_CLEAR,  GX_ROP_CLEAR),	/* GXclear */
    ROP_OSTP(GX_ROP_CLEAR,  GX_ROP_NOOP),	/* GXand */
    ROP_OSTP(GX_ROP_CLEAR,  GX_ROP_INVERT),	/* GXandReverse */
    ROP_OSTP(GX_ROP_CLEAR,  GX_ROP_SET),	/* GXcopy */
    ROP_OSTP(GX_ROP_NOOP,   GX_ROP_CLEAR),	/* GXandInverted */
    ROP_OSTP(GX_ROP_NOOP,   GX_ROP_NOOP),	/* GXnoop */
    ROP_OSTP(GX_ROP_NOOP,   GX_ROP_INVERT),	/* GXxor */
    ROP_OSTP(GX_ROP_NOOP,   GX_ROP_SET),	/* GXor */
    ROP_OSTP(GX_ROP_INVERT, GX_ROP_CLEAR),	/* GXnor */
    ROP_OSTP(GX_ROP_INVERT, GX_ROP_NOOP),	/* GXequiv */
    ROP_OSTP(GX_ROP_INVERT, GX_ROP_INVERT),	/* GXinvert */
    ROP_OSTP(GX_ROP_INVERT, GX_ROP_SET),	/* GXorReverse */
    ROP_OSTP(GX_ROP_SET,    GX_ROP_CLEAR),	/* GXcopyInverted */
    ROP_OSTP(GX_ROP_SET,    GX_ROP_NOOP),	/* GXorInverted */
    ROP_OSTP(GX_ROP_SET,    GX_ROP_INVERT),	/* GXnand */
    ROP_OSTP(GX_ROP_SET,    GX_ROP_SET),	/* GXset */
};

int	sunGXScreenPrivateIndex;
int	sunGXGCPrivateIndex;
int	sunGXWindowPrivateIndex;
int	sunGXGeneration;

/*
  sunGXDoBitBlt
  =============
  Bit Blit for all window to window blits.
*/
static void
sunGXDoBitblt(pSrc, pDst, alu, prgnDst, pptSrc, planemask)
    DrawablePtr	    pSrc, pDst;
    int		    alu;
    RegionPtr	    prgnDst;
    DDXPointPtr	    pptSrc;
    unsigned long   planemask;
{
    register sunGXPtr	gx = sunGXGetScreenPrivate (pSrc->pScreen);
    register long r;
    register BoxPtr pboxTmp;
    register DDXPointPtr pptTmp;
    register int nbox;
    BoxPtr pboxNext,pboxBase,pbox;

    /* setup GX ( need fg of 0xff for blits ) */
    GXBlitInit(gx,gx_blit_rop_table[alu]|POLY_O,planemask);

    pbox = REGION_RECTS(prgnDst);
    nbox = REGION_NUM_RECTS(prgnDst);

    /* need to blit rectangles in different orders, depending on the direction of copy
       so that an area isnt overwritten before it is blitted */
    if( (pptSrc->y < pbox->y1) && (nbox > 1) ){

	if( (pptSrc->x < pbox->x1) && (nbox > 1) ){

	    /* reverse order of bands and rects in each band */
	    pboxTmp=pbox+nbox;
	    pptTmp=pptSrc+nbox;
	    
	    while (nbox--){
		pboxTmp--;
		pptTmp--;	
		gx->x0=pptTmp->x;
		gx->y0=pptTmp->y;
		gx->x1=pptTmp->x+(pboxTmp->x2-pboxTmp->x1)-1;
		gx->y1=pptTmp->y+(pboxTmp->y2-pboxTmp->y1)-1;
		gx->x2=pboxTmp->x1;
		gx->y2=pboxTmp->y1;
		gx->x3=pboxTmp->x2-1;
		gx->y3=pboxTmp->y2-1;
		GXBlitDone(gx,r);
	    }
	}
	else{

	    /* keep ordering in each band, reverse order of bands */
	    pboxBase = pboxNext = pbox+nbox-1;

	    while (pboxBase >= pbox){ /* for each band */

		/* find first box in band */
		while ((pboxNext >= pbox) &&
		       (pboxBase->y1 == pboxNext->y1))
		    pboxNext--;
		
		pboxTmp = pboxNext+1;			/* first box in band */
		pptTmp = pptSrc + (pboxTmp - pbox);	/* first point in band */
		
		while (pboxTmp <= pboxBase){ /* for each box in band */
		    gx->x0=pptTmp->x;
		    gx->y0=pptTmp->y;
		    gx->x1=pptTmp->x+(pboxTmp->x2-pboxTmp->x1)-1;
		    gx->y1=pptTmp->y+(pboxTmp->y2-pboxTmp->y1)-1;
		    gx->x2=pboxTmp->x1;
		    gx->y2=pboxTmp->y1;
		    gx->x3=pboxTmp->x2-1;
		    gx->y3=pboxTmp->y2-1;
		    ++pboxTmp;
		    ++pptTmp;	
		    GXBlitDone(gx,r);
		}
		pboxBase = pboxNext;
	    }
	}
    }
    else{

	if( (pptSrc->x < pbox->x1) && (nbox > 1) ){
	
	    /* reverse order of rects in each band */
	    pboxBase = pboxNext = pbox;

	    while (pboxBase < pbox+nbox){ /* for each band */

		/* find last box in band */
		while ((pboxNext < pbox+nbox) &&
		       (pboxNext->y1 == pboxBase->y1))
		    pboxNext++;
		
		pboxTmp = pboxNext;			/* last box in band */
		pptTmp = pptSrc + (pboxTmp - pbox);	/* last point in band */
		
		while (pboxTmp != pboxBase){ /* for each box in band */
		    --pboxTmp;
		    --pptTmp;	
		    gx->x0=pptTmp->x;
		    gx->y0=pptTmp->y;
		    gx->x1=pptTmp->x+(pboxTmp->x2-pboxTmp->x1)-1;
		    gx->y1=pptTmp->y+(pboxTmp->y2-pboxTmp->y1)-1;
		    gx->x2=pboxTmp->x1;
		    gx->y2=pboxTmp->y1;
		    gx->x3=pboxTmp->x2-1;
		    gx->y3=pboxTmp->y2-1;
		    GXBlitDone(gx,r);
		}
		pboxBase = pboxNext;
	    }
	}
	else{

	    /* dont need to change order of anything */
	    pptTmp=pptSrc;
	    pboxTmp=pbox;
	    
	    while(nbox--){
		gx->x0=pptTmp->x;
		gx->y0=pptTmp->y;
		gx->x1=pptTmp->x+(pboxTmp->x2-pboxTmp->x1)-1;
		gx->y1=pptTmp->y+(pboxTmp->y2-pboxTmp->y1)-1;
		gx->x2=pboxTmp->x1;
		gx->y2=pboxTmp->y1;
		gx->x3=pboxTmp->x2-1;
		gx->y3=pboxTmp->y2-1;
		pboxTmp++;
		pptTmp++;
		GXBlitDone(gx,r);
	    }
	}
    }
    GXWait(gx,r);
}

RegionPtr
sunGXCopyArea(pSrcDrawable, pDstDrawable,
            pGC, srcx, srcy, width, height, dstx, dsty)
    register DrawablePtr pSrcDrawable;
    register DrawablePtr pDstDrawable;
    GC *pGC;
    int srcx, srcy;
    int width, height;
    int dstx, dsty;
{
    if (pSrcDrawable->type != DRAWABLE_WINDOW)
	return cfbCopyArea (pSrcDrawable, pDstDrawable,
            pGC, srcx, srcy, width, height, dstx, dsty);
    return cfbBitBlt (pSrcDrawable, pDstDrawable,
            pGC, srcx, srcy, width, height, dstx, dsty, sunGXDoBitblt, 0);
}

static unsigned long	copyPlaneFG, copyPlaneBG;

static void
sunGXCopyPlane1to8 (pSrcDrawable, pDstDrawable, rop, prgnDst, pptSrc, planemask, bitPlane)
    DrawablePtr pSrcDrawable;
    DrawablePtr pDstDrawable;
    int	rop;
    RegionPtr prgnDst;
    DDXPointPtr pptSrc;
    unsigned long planemask;
    unsigned long   bitPlane;
{
    register sunGXPtr	gx = sunGXGetScreenPrivate (pDstDrawable->pScreen);
    int			srcx, srcy, dstx, dsty, width, height;
    int			dstLastx, dstRightx;
    int			xoffSrc, widthSrc, widthRest;
    int			widthLast;
    unsigned long	*psrcBase, *psrc;
    unsigned long	bits, tmp;
    register int	leftShift, rightShift;
    register int	nl, nlMiddle;
    int			nbox;
    BoxPtr		pbox;
    register int	r;

    GXDrawInit (gx, copyPlaneFG, 
		gx_opaque_stipple_rop_table[rop]|GX_PATTERN_ONES,
 		planemask);
    gx->bg = copyPlaneBG;
    gx->mode = GX_BLIT_NOSRC | GX_MODE_COLOR1;

    cfbGetLongWidthAndPointer (pSrcDrawable, widthSrc, psrcBase)

    nbox = REGION_NUM_RECTS(prgnDst);
    pbox = REGION_RECTS(prgnDst);
    gx->incx = 32;
    gx->incy = 0;
    while (nbox--)
    {
	dstx = pbox->x1;
	dsty = pbox->y1;
	srcx = pptSrc->x;
	srcy = pptSrc->y;
	dstLastx = pbox->x2;
	width = dstLastx - dstx;
	height = pbox->y2 - dsty;
	pbox++;
	pptSrc++;
	if (!width)
	    continue;
	psrc = psrcBase + srcy * widthSrc + (srcx >> 5);
	dstLastx--;
	dstRightx = dstx + 31;
	nlMiddle = (width + 31) >> 5;
	widthLast = width & 31;
	xoffSrc = srcx & 0x1f;
	leftShift = xoffSrc;
	rightShift = 32 - leftShift;
	widthRest = widthSrc - nlMiddle;
	if (widthLast)
	    nlMiddle--;
	if (leftShift == 0)
	{
	    while (height--)
	    {
	    	gx->x0 = dstx;
	    	gx->x1 = dstRightx;
	    	gx->y0 = dsty++;
	    	nl = nlMiddle;
	    	while (nl--)
		    gx->font = *psrc++;
	    	if (widthLast) 
	    	{
		    gx->x1 = dstLastx;
		    gx->font = *psrc++;
	    	}
		psrc += widthRest;
	    }
	}
	else
	{
	    widthRest--;
	    while (height--)
	    {
	    	gx->x0 = dstx;
	    	gx->x1 = dstRightx;
	    	gx->y0 = dsty++;
	    	bits = *psrc++;
	    	nl = nlMiddle;
	    	while (nl--)
	    	{
		    tmp = BitLeft(bits, leftShift);
		    bits = *psrc++;
		    tmp |= BitRight(bits, rightShift);
		    gx->font = tmp;
	    	}
	    	if (widthLast) 
	    	{
		    tmp = BitLeft(bits, leftShift);
		    bits = *psrc++;
		    tmp |= BitRight(bits, rightShift);
		    gx->x1 = dstLastx;
		    gx->font = tmp;
	    	}
		psrc += widthRest;
	    }
	}
    }
    GXWait (gx, r);
    gx->incx = 0;
    gx->incy = 0;
    gx->mode = GX_BLIT_SRC | GX_MODE_COLOR8;
}

RegionPtr sunGXCopyPlane(pSrcDrawable, pDstDrawable,
	    pGC, srcx, srcy, width, height, dstx, dsty, bitPlane)
    DrawablePtr 	pSrcDrawable;
    DrawablePtr		pDstDrawable;
    GCPtr		pGC;
    int 		srcx, srcy;
    int 		width, height;
    int 		dstx, dsty;
    unsigned long	bitPlane;
{
    RegionPtr		ret;

    if (pSrcDrawable->bitsPerPixel == 1 && pDstDrawable->bitsPerPixel == 8)
    {
    	if (bitPlane == 1)
	{
	    copyPlaneFG = pGC->fgPixel;
	    copyPlaneBG = pGC->bgPixel;
    	    ret = cfbBitBlt (pSrcDrawable, pDstDrawable,
	    	    pGC, srcx, srcy, width, height, dstx, dsty, sunGXCopyPlane1to8, bitPlane);
	}
	else
	    ret = miHandleExposures (pSrcDrawable, pDstDrawable,
	    	pGC, srcx, srcy, width, height, dstx, dsty, bitPlane);
    }
    else if (pSrcDrawable->bitsPerPixel == 8 && pDstDrawable->bitsPerPixel == 1)
    {
	extern	int InverseAlu[16];
	int oldalu;

	oldalu = pGC->alu;
    	if ((pGC->fgPixel & 1) == 0 && (pGC->bgPixel&1) == 1)
	    pGC->alu = InverseAlu[pGC->alu];
    	else if ((pGC->fgPixel & 1) == (pGC->bgPixel & 1))
	    pGC->alu = mfbReduceRop(pGC->alu, pGC->fgPixel);
	ret = cfbCopyPlaneReduce (pSrcDrawable, pDstDrawable,
		    pGC, srcx, srcy, width, height, dstx, dsty, cfbCopyPlane8to1, bitPlane);
	pGC->alu = oldalu;
    }
    else
    {
	PixmapPtr	pBitmap;
	ScreenPtr	pScreen = pSrcDrawable->pScreen;
	GCPtr		pGC1;

	pBitmap = (*pScreen->CreatePixmap) (pScreen, width, height, 1);
	if (!pBitmap)
	    return NULL;
	pGC1 = GetScratchGC (1, pScreen);
	if (!pGC1)
	{
	    (*pScreen->DestroyPixmap) (pBitmap);
	    return NULL;
	}
	/*
	 * don't need to set pGC->fgPixel,bgPixel as copyPlane8to1
	 * ignores pixel values, expecting the rop to "do the
	 * right thing", which GXcopy will.
	 */
	ValidateGC ((DrawablePtr) pBitmap, pGC1);
	/* no exposures here, scratch GC's don't get graphics expose */
	(void) cfbCopyPlaneReduce (pSrcDrawable, (DrawablePtr) pBitmap,
			    pGC1, srcx, srcy, width, height, 0, 0, cfbCopyPlane8to1, bitPlane);
	copyPlaneFG = pGC->fgPixel;
	copyPlaneBG = pGC->bgPixel;
	(void) cfbBitBlt ((DrawablePtr) pBitmap, pDstDrawable, pGC,
			    0, 0, width, height, dstx, dsty, sunGXCopyPlane1to8, 1);
	FreeScratchGC (pGC1);
	(*pScreen->DestroyPixmap) (pBitmap);
	/* compute resultant exposures */
	ret = miHandleExposures (pSrcDrawable, pDstDrawable, pGC,
				 srcx, srcy, width, height,
				 dstx, dsty, bitPlane);
    }
    return ret;
}

void
sunGXFillRectAll (pDrawable, pGC, nBox, pBox)
    DrawablePtr	    pDrawable;
    GCPtr	    pGC;
    int		    nBox;
    BoxPtr	    pBox;
{
    register sunGXPtr	gx = sunGXGetScreenPrivate (pDrawable->pScreen);
    register sunGXPrivGCPtr gxPriv = sunGXGetGCPrivate (pGC);
    register int	r;

    GXDrawInit(gx,pGC->fgPixel,gx_solid_rop_table[pGC->alu]|POLY_N,pGC->planemask);
    if (gxPriv->stipple)
	GXStippleInit(gx,gxPriv->stipple);
    while (nBox--) {
	gx->arecty = pBox->y1;
	gx->arectx = pBox->x1;
	gx->arecty = pBox->y2;
	gx->arectx = pBox->x2;
	pBox++;
	GXDrawDone(gx,r);
    }
    GXWait(gx,r);
}

#define NUM_STACK_RECTS	1024

void
sunGXPolyFillRect(pDrawable, pGC, nrectFill, prectInit)
    DrawablePtr pDrawable;
    register GCPtr pGC;
    int		nrectFill; 	/* number of rectangles to fill */
    xRectangle	*prectInit;  	/* Pointer to first rectangle to fill */
{
    xRectangle	    *prect;
    RegionPtr	    prgnClip;
    register BoxPtr pbox;
    register BoxPtr pboxClipped;
    BoxPtr	    pboxClippedBase;
    BoxPtr	    pextent;
    BoxRec	    stackRects[NUM_STACK_RECTS];
    int		    numRects;
    int		    n;
    int		    xorg, yorg;

    prgnClip = pGC->pCompositeClip;
    prect = prectInit;
    xorg = pDrawable->x;
    yorg = pDrawable->y;
    if (xorg || yorg)
    {
	prect = prectInit;
	n = nrectFill;
	while(n--)
	{
	    prect->x += xorg;
	    prect->y += yorg;
	    prect++;
	}
    }

    prect = prectInit;

    numRects = REGION_NUM_RECTS(prgnClip) * nrectFill;
    if (numRects > NUM_STACK_RECTS)
    {
	pboxClippedBase = (BoxPtr)ALLOCATE_LOCAL(numRects * sizeof(BoxRec));
	if (!pboxClippedBase)
	    return;
    }
    else
	pboxClippedBase = stackRects;

    pboxClipped = pboxClippedBase;
	
    if (REGION_NUM_RECTS(prgnClip) == 1)
    {
	int x1, y1, x2, y2, bx2, by2;

	pextent = REGION_RECTS(prgnClip);
	x1 = pextent->x1;
	y1 = pextent->y1;
	x2 = pextent->x2;
	y2 = pextent->y2;
    	while (nrectFill--)
    	{
	    if ((pboxClipped->x1 = prect->x) < x1)
		pboxClipped->x1 = x1;
    
	    if ((pboxClipped->y1 = prect->y) < y1)
		pboxClipped->y1 = y1;
    
	    bx2 = (int) prect->x + (int) prect->width;
	    if (bx2 > x2)
		bx2 = x2;
	    pboxClipped->x2 = bx2;
    
	    by2 = (int) prect->y + (int) prect->height;
	    if (by2 > y2)
		by2 = y2;
	    pboxClipped->y2 = by2;

	    prect++;
	    if ((pboxClipped->x1 < pboxClipped->x2) &&
		(pboxClipped->y1 < pboxClipped->y2))
	    {
		pboxClipped++;
	    }
    	}
    }
    else
    {
	int x1, y1, x2, y2, bx2, by2;

	pextent = REGION_EXTENTS(pGC->pScreen, prgnClip);
	x1 = pextent->x1;
	y1 = pextent->y1;
	x2 = pextent->x2;
	y2 = pextent->y2;
    	while (nrectFill--)
    	{
	    BoxRec box;
    
	    if ((box.x1 = prect->x) < x1)
		box.x1 = x1;
    
	    if ((box.y1 = prect->y) < y1)
		box.y1 = y1;
    
	    bx2 = (int) prect->x + (int) prect->width;
	    if (bx2 > x2)
		bx2 = x2;
	    box.x2 = bx2;
    
	    by2 = (int) prect->y + (int) prect->height;
	    if (by2 > y2)
		by2 = y2;
	    box.y2 = by2;
    
	    prect++;
    
	    if ((box.x1 >= box.x2) || (box.y1 >= box.y2))
	    	continue;
    
	    n = REGION_NUM_RECTS (prgnClip);
	    pbox = REGION_RECTS(prgnClip);
    
	    /* clip the rectangle to each box in the clip region
	       this is logically equivalent to calling Intersect()
	    */
	    while(n--)
	    {
		pboxClipped->x1 = max(box.x1, pbox->x1);
		pboxClipped->y1 = max(box.y1, pbox->y1);
		pboxClipped->x2 = min(box.x2, pbox->x2);
		pboxClipped->y2 = min(box.y2, pbox->y2);
		pbox++;

		/* see if clipping left anything */
		if(pboxClipped->x1 < pboxClipped->x2 && 
		   pboxClipped->y1 < pboxClipped->y2)
		{
		    pboxClipped++;
		}
	    }
    	}
    }
    if (pboxClipped != pboxClippedBase)
	sunGXFillRectAll(pDrawable, pGC,
		    pboxClipped-pboxClippedBase, pboxClippedBase);
    if (pboxClippedBase != stackRects)
    	DEALLOCATE_LOCAL(pboxClippedBase);
}

void
sunGXFillSpans (pDrawable, pGC, n, ppt, pwidth, fSorted)
    DrawablePtr pDrawable;
    GCPtr	pGC;
    int		n;			/* number of spans to fill */
    DDXPointPtr ppt;			/* pointer to list of start points */
    int		*pwidth;		/* pointer to list of n widths */
    int 	fSorted;
{
    int		    x, y;
    int		    width;
				/* next three parameters are post-clip */
    int		    nTmp;
    int		    *pwidthFree;/* copies of the pointers to free */
    DDXPointPtr	    pptFree;
    register sunGXPtr	gx = sunGXGetScreenPrivate (pDrawable->pScreen);
    cfbPrivGCPtr    devPriv = cfbGetGCPrivate(pGC);
    register sunGXPrivGCPtr gxPriv = sunGXGetGCPrivate (pGC);
    register int    r;
    BoxPtr	    extents;

    GXDrawInit(gx,pGC->fgPixel,gx_solid_rop_table[pGC->alu]|POLY_O,pGC->planemask)
    if (gxPriv->stipple)
	GXStippleInit(gx,gxPriv->stipple);
    if (devPriv->oneRect)
    {
	extents = &pGC->pCompositeClip->extents;
	GXSetClip (gx, extents);
    }
    else
    {
    	nTmp = n * miFindMaxBand(pGC->pCompositeClip);
    	pwidthFree = (int *)ALLOCATE_LOCAL(nTmp * sizeof(int));
    	pptFree = (DDXPointRec *)ALLOCATE_LOCAL(nTmp * sizeof(DDXPointRec));
    	if(!pptFree || !pwidthFree)
    	{
	    if (pptFree) DEALLOCATE_LOCAL(pptFree);
	    if (pwidthFree) DEALLOCATE_LOCAL(pwidthFree);
	    return;
    	}
    	n = miClipSpans(pGC->pCompositeClip, ppt, pwidth, n,
		     	 pptFree, pwidthFree, fSorted);
    	pwidth = pwidthFree;
    	ppt = pptFree;
    }
    while (n--)
    {
	x = ppt->x;
	y = ppt->y;
	ppt++;
	width = *pwidth++;
	if (width)
	{
	    sunGXFillSpan(gx,y,x,x + width - 1,r);
	}
    }
    GXWait(gx,r);
    if (devPriv->oneRect) 
    {
	GXResetClip (gx, pDrawable->pScreen);
    }
    else
    {
	DEALLOCATE_LOCAL(pptFree);
	DEALLOCATE_LOCAL(pwidthFree);
    }
}

#ifdef NOTDEF
/* cfb is faster for dots */
void
sunGXPolyPoint(pDrawable, pGC, mode, npt, pptInit)
    DrawablePtr pDrawable;
    GCPtr pGC;
    int mode;
    int npt;
    xPoint *pptInit;
{
    register sunGXPtr	gx = sunGXGetScreenPrivate (pDrawable->pScreen);
    RegionPtr	    cclip;
    int		    nbox;
    register int    i;
    register BoxPtr pbox;
    cfbPrivGCPtr    devPriv;
    xPoint	    *ppt;
    int		    x, y;
    int		    r;
    int		    off;

    devPriv = (cfbPrivGC *)(pGC->devPrivates[cfbGCPrivateIndex].ptr); 
    if (devPriv->rop == GXnoop)
	return;
    cclip = pGC->pCompositeClip;
    GXDrawInit(gx,pGC->fgPixel,gx_solid_rop_table[pGC->alu],pGC->planemask);
    gx->offx = pDrawable->x;
    gx->offy = pDrawable->y;
    for (nbox = REGION_NUM_RECTS(cclip), pbox = REGION_RECTS(cclip);
	 --nbox >= 0;
	 pbox++)
    {
	sunGXSetClip(gx,pbox);
	if (mode != CoordModeOrigin)
	{
	    x = 0;
	    y = 0;
	    for (ppt = pptInit, i = npt; --i >= 0;)
	    {
	    	gx->apointy = y += ppt->y;
	    	gx->apointx = x += ppt->x;
	    	++ppt;
	    	GXDrawDone(gx,r);
	    }
	}
	else
	{
	    for (ppt = pptInit, i = npt; --i >= 0;)
	    {
	    	gx->apointy = ppt->y;
	    	gx->apointx = ppt->x;
	    	++ppt;
	    	GXDrawDone(gx,r);
	    }
	}
    }
    GXWait(gx,r);
    GXResetOff (gx);
    GXResetClip(gx,pDrawable->pScreen);
}
#endif

#include "mifillarc.h"

#define FILLSPAN(gx,y,x1,x2,r) {\
    if (x2 >= x1) {\
	sunGXFillSpan(gx,y,x1,x2,r) \
    } \
}

#define FILLSLICESPANS(flip,y) \
    if (!flip) \
    { \
	FILLSPAN(gx,y,xl,xr,r) \
    } \
    else \
    { \
	xc = xorg - x; \
	FILLSPAN(gx, y, xc, xr, r) \
	xc += slw - 1; \
	FILLSPAN(gx, y, xl, xc, r) \
    }

static void
sunGXFillEllipse (pDraw, gx, arc)
    DrawablePtr	pDraw;
    sunGXPtr	gx;
    xArc	*arc;
{
    int x, y, e;
    int yk, xk, ym, xm, dx, dy, xorg, yorg;
    int	y_top, y_bot;
    miFillArcRec info;
    register int xpos;
    int	r;
    int	slw;

    miFillArcSetup(arc, &info);
    MIFILLARCSETUP();
    y_top = yorg - y;
    y_bot = yorg + y + dy;
    while (y)
    {
	y_top++;
	y_bot--;
	MIFILLARCSTEP(slw);
	if (!slw)
	    continue;
	xpos = xorg - x;
	sunGXFillSpan (gx,y_top,xpos,xpos+slw - 1,r);
	if (miFillArcLower(slw))
	    sunGXFillSpan (gx,y_bot,xpos,xpos+slw - 1,r);
    }
}


static void
sunGXFillArcSlice (pDraw, pGC, gx, arc)
    DrawablePtr pDraw;
    GCPtr	pGC;
    sunGXPtr	gx;
    xArc	*arc;
{
    int yk, xk, ym, xm, dx, dy, xorg, yorg, slw;
    register int x, y, e;
    miFillArcRec info;
    miArcSliceRec slice;
    int xl, xr, xc;
    int	y_top, y_bot;
    int	r;

    miFillArcSetup(arc, &info);
    miFillArcSliceSetup(arc, &slice, pGC);
    MIFILLARCSETUP();
    y_top = yorg - y;
    y_bot = yorg + y + dy;
    while (y > 0)
    {
	y_top++;
	y_bot--;
	MIFILLARCSTEP(slw);
	MIARCSLICESTEP(slice.edge1);
	MIARCSLICESTEP(slice.edge2);
	if (miFillSliceUpper(slice))
	{
	    MIARCSLICEUPPER(xl, xr, slice, slw);
	    FILLSLICESPANS(slice.flip_top, y_top);
	}
	if (miFillSliceLower(slice))
	{
	    MIARCSLICELOWER(xl, xr, slice, slw);
	    FILLSLICESPANS(slice.flip_bot, y_bot);
	}
    }
}

#define FAST_CIRCLES
#ifdef FAST_CIRCLES
#if     (BITMAP_BIT_ORDER == MSBFirst)
#define Bits32(v)   (v)
#define Bits16(v)   (v)
#define Bits8(v)    (v)
#else
#define FlipBits2(a)     ((((a) & 0x1) << 1) | (((a) & 0x2) >> 1))
#define FlipBits4(a)     ((FlipBits2(a) << 2) | FlipBits2(a >> 2))
#define FlipBits8(a)     ((FlipBits4(a) << 4) | FlipBits4(a >> 4))
#define FlipBits16(a)    ((FlipBits8(a) << 8) | FlipBits8(a >> 8))
#define FlipBits32(a)    ((FlipBits16(a) << 16) | FlipBits16(a >> 16))
#define Bits32(v)   FlipBits32(v)
#define Bits16(v)   FlipBits16(v)
#define Bits8(v)    FlipBits8(v)
#endif

#define B(x)	Bits16(x)
#define DO_FILLED_ARCS
#include    "circleset.h"
#undef B
#undef Bits8
#undef Bits16
#undef Bits32
#define UNSET_CIRCLE	if (old_width) \
			{ \
			    gx->alu = gx_solid_rop_table[pGC->alu]; \
			    old_width = -old_width; \
			}

#else
#define UNSET_CIRCLE
#endif

void
sunGXPolyFillArc (pDraw, pGC, narcs, parcs)
    DrawablePtr	pDraw;
    GCPtr	pGC;
    int		narcs;
    xArc	*parcs;
{
    register xArc *arc;
    register int i;
    int		x, y;
    BoxRec box;
    BoxPtr	extents = NULL;
    RegionPtr cclip;
    register sunGXPtr	gx = sunGXGetScreenPrivate (pDraw->pScreen);
    sunGXPrivGCPtr	gxPriv = sunGXGetGCPrivate (pGC);
    cfbPrivGCPtr    devPriv;
    register int	r;
#ifdef FAST_CIRCLES
    int			old_width = 0;
#endif

    GXDrawInit(gx,pGC->fgPixel,gx_solid_rop_table[pGC->alu]|POLY_O,pGC->planemask);
    if (gxPriv->stipple)
	GXStippleInit(gx,gxPriv->stipple);
    devPriv = (cfbPrivGC *)(pGC->devPrivates[cfbGCPrivateIndex].ptr); 
    cclip = pGC->pCompositeClip;
    GXSetOff(gx,pDraw->x,pDraw->y)
    if (devPriv->oneRect) {
	extents = &cclip->extents;
	GXSetClip(gx,extents);
    }
    for (arc = parcs, i = narcs; --i >= 0; arc++)
    {
	if (miFillArcEmpty(arc))
	    continue;
	if (miCanFillArc(arc))
	{
	    x = arc->x;
	    y = arc->y;
	    if (!devPriv->oneRect)
	    {
	    	box.x1 = x + pDraw->x;
	    	box.y1 = y + pDraw->y;
	    	box.x2 = box.x1 + (int)arc->width + 1;
	    	box.y2 = box.y1 + (int)arc->height + 1;
	    }
	    if (devPriv->oneRect ||
		RECT_IN_REGION(pDraw->pScreen, cclip, &box) == rgnIN)
	    {
		if ((arc->angle2 >= FULLCIRCLE) ||
		    (arc->angle2 <= -FULLCIRCLE))
		{
#ifdef FAST_CIRCLES
/* who really needs fast filled circles? */
		    if (arc->width == arc->height && arc->width <= 16 &&
			!gxPriv->stipple)
		    {
			int offx, offy;
			if (arc->width != old_width)
			{
			    int	    i;
			    Uint    *sp;
			    VUint   *dp;

			    if (old_width != -arc->width)
			    {
			    	sp = (Uint *) filled_arcs[arc->width-1];
			    	dp = gx->pattern;
			    	i = 8;
			    	while (i--)
				    dp[i] = sp[i];
			    }
			    gx->alu = gx_stipple_rop_table[pGC->alu]|GX_PATTERN_MASK;
			    old_width = arc->width;
			}
			offx = 16 - ((x + pDraw->x) & 0x0f);
			offy = 16 - ((y + pDraw->y) & 0x0f);
			gx->patalign = (offx << 16) | offy;
			gx->arecty = y;
			gx->arectx = x;
			gx->arecty = y + old_width-1;
			gx->arectx = x + old_width-1;
			GXDrawDone (gx, r);
		    }
		    else
#endif
		    {
			UNSET_CIRCLE
			sunGXFillEllipse (pDraw, gx, arc);
		    }
		}
		else
		{
		    UNSET_CIRCLE
		    sunGXFillArcSlice (pDraw, pGC, gx, arc);
		}
		continue;
	    }
	}
	UNSET_CIRCLE
	GXWait (gx,r);
	GXResetOff (gx);
	if (devPriv->oneRect)
	    GXResetClip (gx, pDraw->pScreen);
	miPolyFillArc(pDraw, pGC, 1, arc);
	GXSetOff (gx, pDraw->x, pDraw->y);
	if (devPriv->oneRect)
	    GXSetClip (gx, extents);
    }
    GXWait (gx, r);
    GXResetOff (gx);
    if (devPriv->oneRect)
	GXResetClip (gx, pDraw->pScreen);
}

void
sunGXFillPoly1Rect (pDrawable, pGC, shape, mode, count, ptsIn)
    DrawablePtr	pDrawable;
    GCPtr	pGC;
    int		count;
    DDXPointPtr	ptsIn;
{
    BoxPtr	    extents;
    int		    x1, x2, x3, x4;
    int		    y1, y2, y3, y4;
    sunGXPtr	    gx = sunGXGetScreenPrivate (pDrawable->pScreen);
    sunGXPrivGCPtr  gxPriv = sunGXGetGCPrivate (pGC);
    int		    r;
    typedef struct {
	Uint	    x;
	Uint	    y;
	Uint	    z;
    } GXPointRec, *GXPointPtr;
    GXPointPtr	    tri, qua;

    if (count < 3)
	return;
    if (shape != Convex && count > 4)
    {
	miFillPolygon (pDrawable, pGC, shape, mode, count, ptsIn);
	return;
    }
    GXDrawInit(gx,pGC->fgPixel,gx_solid_rop_table[pGC->alu]|POLY_N,pGC->planemask);
    if (gxPriv->stipple)
	GXStippleInit(gx,gxPriv->stipple);
    extents = &pGC->pCompositeClip->extents;
    GXSetOff(gx,pDrawable->x, pDrawable->y);
    GXSetClip(gx,extents);
    if (mode == CoordModeOrigin)
    {
	tri = (GXPointPtr) &gx->atrix;
	qua = (GXPointPtr) &gx->aquadx;
    }
    else
    {
	tri = (GXPointPtr) &gx->rtrix;
	qua = (GXPointPtr) &gx->rquadx;
    }
    if (count == 3) {
	gx->apointy = ptsIn[0].y;
	gx->apointx = ptsIn[0].x;
	tri->y = ptsIn[1].y;
	tri->x = ptsIn[1].x;
	tri->y = ptsIn[2].y;
	tri->x = ptsIn[2].x;
	GXDrawDone (gx, r);
    }
    else if (count == 4)
    {
	gx->apointy = ptsIn[0].y;
	gx->apointx = ptsIn[0].x;
	qua->y = ptsIn[1].y;
	qua->x = ptsIn[1].x;
	qua->y = ptsIn[2].y;
	qua->x = ptsIn[2].x;
	qua->y = ptsIn[3].y;
	qua->x = ptsIn[3].x;
	GXDrawDone (gx, r);
	if (r < 0 && shape != Convex)
	{
	    GXWait(gx,r);
	    GXResetOff(gx);
	    GXResetClip(gx,pDrawable->pScreen);
	    miFillPolygon (pDrawable, pGC, shape, mode, count, ptsIn);
	    return;
	}
    }
    else
    {
	y1 = ptsIn[0].y;
	x1 = ptsIn[0].x;
	y2 = ptsIn[1].y;
	x2 = ptsIn[1].x;
	count -= 2;
	ptsIn += 2;
    	while (count) {
	    x3 = ptsIn->x;
	    y3 = ptsIn->y;
	    ptsIn++;
	    count--;
	    gx->apointy = y1;
	    gx->apointx = x1;
	    if (count == 0) {
		tri->y = y2;
		tri->x = x2;
		tri->y = y3;
		tri->x = x3;
	    }
	    else
	    {
		y4 = ptsIn->y;
		x4 = ptsIn->x;
		ptsIn++;
		count--;
		qua->y = y2;
		qua->x = x2;
		qua->y = y3;
		qua->x = x3;
		qua->y = y4;
		qua->x = x4;
		if (mode == CoordModeOrigin)
		{
		    x2 = x4;
		    y2 = y4;
		}
		else
		{
		    x2 = x2 + x3 + x4;
		    y2 = y2 + y3 + y4;
		}
	    }
	    GXDrawDone (gx, r);
    	}
    }
    GXWait(gx,r);
    GXResetOff(gx);
    GXResetClip(gx,pDrawable->pScreen);
}

/*
 * Note that the GX does not allow CapNotLast, so the code fakes it.  This is
 * expensive to do as the GX is asynchronous and must be synced with GXWait
 * before fetching and storing the final line point.  If only the hardware was
 * designed for X.
 */

/* hard code the screen width; otherwise we'd have to check or mul */

#define WIDTH_MUL(y)	(((y) << 10) + ((y) << 7))
#define GX_WIDTH	1152
#define WID_OK(s)	((s)->width == GX_WIDTH)

void
sunGXPolySeg1Rect (pDrawable, pGC, nseg, pSeg)
    DrawablePtr	    pDrawable;
    GCPtr	    pGC;
    int		    nseg;
    xSegment	    *pSeg;
{
    sunGXPtr	    gx = sunGXGetScreenPrivate (pDrawable->pScreen);
    sunGXPrivGCPtr  gxPriv = sunGXGetGCPrivate (pGC);
    BoxPtr	    extents;
    int		    x, y;
    int		    r;
    unsigned char   *baseAddr, *loAddr, *hiAddr, *saveAddr = 0, save = 0;

    GXDrawInit(gx,pGC->fgPixel,gx_solid_rop_table[pGC->alu]|POLY_O,pGC->planemask);
    if (gxPriv->stipple)
	GXStippleInit(gx,gxPriv->stipple);
    GXSetOff (gx, pDrawable->x, pDrawable->y);
    
    extents = &pGC->pCompositeClip->extents;
    GXSetClip (gx, extents);
    if (pGC->capStyle == CapNotLast)
    {
	sunGXGetAddrRange(pDrawable,extents,baseAddr,loAddr,hiAddr);
    	while (nseg--)
    	{
	    gx->aliney = pSeg->y1;
	    gx->alinex = pSeg->x1;
	    y = pSeg->y2;
	    x = pSeg->x2;
	    saveAddr = baseAddr + WIDTH_MUL(y) + x;
	    if (saveAddr < loAddr || hiAddr < saveAddr)
		saveAddr = 0;
	    else
		save = *saveAddr;
	    gx->aliney = y;
	    gx->alinex = x;
	    GXDrawDone (gx, r);
	    if (saveAddr)
	    {
		GXWait(gx,r);
		*saveAddr = save;
	    }
	    pSeg++;
    	}
    }
    else
    {
    	while (nseg--)
    	{
	    gx->aliney = pSeg->y1;
	    gx->alinex = pSeg->x1;
	    gx->aliney = pSeg->y2;
	    gx->alinex = pSeg->x2;
	    pSeg++;
	    GXDrawDone (gx, r);
    	}
    }
    GXWait (gx, r);
    GXResetOff (gx);
    GXResetClip (gx, pDrawable->pScreen);
}

void
sunGXPolylines1Rect (pDrawable, pGC, mode, npt, ppt)
    DrawablePtr	    pDrawable;
    GCPtr	    pGC;
    int		    mode;
    int		    npt;
    DDXPointPtr	    ppt;
{
    sunGXPtr	    gx = sunGXGetScreenPrivate (pDrawable->pScreen);
    sunGXPrivGCPtr  gxPriv = sunGXGetGCPrivate (pGC);
    BoxPtr	    extents;
    unsigned char   *baseAddr, *loAddr, *hiAddr, *saveAddr, save = 0;
    int		    r;
    Bool	    careful;
    Bool	    capNotLast;

    if (!--npt)
	return;
    GXDrawInit(gx,pGC->fgPixel,gx_solid_rop_table[pGC->alu]|POLY_O,pGC->planemask);
    if (gxPriv->stipple)
	GXStippleInit(gx,gxPriv->stipple);
    careful = ((pGC->alu & 0xc) == 0x8 || (pGC->alu & 0x3) == 0x2);
    capNotLast = pGC->capStyle == CapNotLast;

    extents = &pGC->pCompositeClip->extents;
    GXSetOff (gx, pDrawable->x, pDrawable->y);
    GXSetClip (gx, extents);
    if (careful) 
    {
	int	x, y;
	sunGXGetAddrRange (pDrawable, extents, baseAddr, loAddr, hiAddr);
	gx->apointy = y = ppt->y;
	gx->apointx = x = ppt->x;
	ppt++;
    	while (npt--)
    	{
	    if (mode == CoordModeOrigin)
	    {
		y = ppt->y;
		x = ppt->x;
	    }
	    else
	    {
	    	y += ppt->y;
	    	x += ppt->x;
	    }
	    ppt++;
	    saveAddr = baseAddr + WIDTH_MUL(y) + x;
	    if (saveAddr < loAddr || hiAddr < saveAddr)
		saveAddr = 0;
	    else
		save = *saveAddr;
	    gx->aliney = y;
	    gx->alinex = x;
	    GXDrawDone (gx, r);
	    if (saveAddr)
	    {
		GXWait(gx,r);
	    	*saveAddr = save;
	    }
    	}
	GXWait(gx,r);
    }
    else
    {
	int	x, y;
	if (capNotLast)
	    npt--;
	if (mode == CoordModeOrigin)
	{
	    x = y = 0;
	    gx->apointy = ppt->y;
	    gx->apointx = ppt->x;
	    ppt++;
	    while (npt--)
	    {
		gx->aliney = ppt->y;
		gx->alinex = ppt->x;
		++ppt;
		GXDrawDone(gx,r);
	    }
	}
	else
	{
	    y = gx->apointy = ppt->y;
	    x = gx->apointx = ppt->x;
	    ppt++;
	    while (npt--)
	    {
		y += gx->rliney = ppt->y;
		x += gx->rlinex = ppt->x;
		++ppt;
		GXDrawDone(gx,r);
	    }
	}
	if (capNotLast) 
	{
	    sunGXGetAddrRange (pDrawable, extents, baseAddr, loAddr, hiAddr);
	    x += ppt->x;
	    y += ppt->y;
	    saveAddr = baseAddr + WIDTH_MUL(y) + x;
	    if (saveAddr < loAddr || hiAddr < saveAddr)
		saveAddr = 0;
	    else
		save = *saveAddr;
	    gx->aliney = y;
	    gx->alinex = x;
	    GXDrawDone(gx,r);
	    GXWait(gx,r);
	    if (saveAddr)
		*saveAddr = save;
	}
	else
	{
	    GXWait(gx,r);
	}
    }
    GXResetOff (gx);
    GXResetClip (gx, pDrawable->pScreen);
}

void
sunGXPolyFillRect1Rect (pDrawable, pGC, nrect, prect)
    DrawablePtr	pDrawable;
    GCPtr	pGC;
    int		nrect;
    xRectangle	*prect;
{
    sunGXPtr	    gx = sunGXGetScreenPrivate (pDrawable->pScreen);
    sunGXPrivGCPtr  gxPriv = sunGXGetGCPrivate (pGC);
    BoxPtr	    extents = &pGC->pCompositeClip->extents;
    int		    r;
    int		    x, y;

    GXDrawInit(gx,pGC->fgPixel,gx_solid_rop_table[pGC->alu]|POLY_N,pGC->planemask);
    if (gxPriv->stipple)
	GXStippleInit(gx,gxPriv->stipple);
    GXSetOff (gx, pDrawable->x, pDrawable->y);
    GXSetClip (gx, extents);
    while (nrect--)
    {
	gx->arecty = y = prect->y;
	gx->arectx = x = prect->x;
	gx->arecty = y + (int) prect->height;
	gx->arectx = x + (int) prect->width;
	prect++;
	GXDrawDone (gx, r);
    }
    GXWait (gx, r);
    GXResetOff (gx);
    GXResetClip (gx, pDrawable->pScreen);
}

static void
sunGXPolyGlyphBlt (pDrawable, pGC, x, y, nglyph, ppci, pglyphBase)
    DrawablePtr	    pDrawable;
    GCPtr	    pGC;
    int		    x, y;
    unsigned int    nglyph;
    CharInfoPtr	    *ppci;		/* array of character info */
    pointer         pglyphBase;
{
    sunGXPtr	    gx = sunGXGetScreenPrivate (pDrawable->pScreen);
    int		    h;
    int		    w;
    CharInfoPtr	    pci;
    unsigned long   *bits;
    register int    r;
    RegionPtr	    clip;
    BoxPtr	    extents;
    BoxRec	    box;

    clip = pGC->pCompositeClip;
    extents = &clip->extents;

    if (REGION_NUM_RECTS(clip) == 1)
    {
	GXSetClip (gx, extents);
    }
    else
    {
    	/* compute an approximate (but covering) bounding box */
    	box.x1 = 0;
    	if ((ppci[0]->metrics.leftSideBearing < 0))
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
    
    	switch (RECT_IN_REGION(pGC->pScreen, clip, &box))
	{
	case rgnPART:
	    cfbPolyGlyphBlt8 (pDrawable, pGC, x, y, nglyph, ppci, pglyphBase);
	case rgnOUT:
	    return;
	}
    }

    GXDrawInit (gx, pGC->fgPixel, 
		gx_stipple_rop_table[pGC->alu]|GX_PATTERN_ONES|POLY_N,
 		pGC->planemask);
    gx->mode = GX_BLIT_NOSRC | GX_MODE_COLOR1;
    x += pDrawable->x;
    y += pDrawable->y;

    while (nglyph--)
    {
	pci = *ppci++;
	gx->incx = 0;
	gx->incy = 1;
	gx->x0 = x + pci->metrics.leftSideBearing;
	gx->x1 = (x + pci->metrics.rightSideBearing) - 1;
	gx->y0 = y - pci->metrics.ascent;
	h = pci->metrics.ascent + pci->metrics.descent;
	bits = (unsigned long *) pci->bits;
	while (h--) {
	    gx->font = *bits++;
	}
	x += pci->metrics.characterWidth;
    }
    GXWait (gx, r);
    gx->mode = GX_BLIT_SRC | GX_MODE_COLOR8;
    GXResetClip (gx, pDrawable->pScreen);
}

static void
sunGXTEGlyphBlt (pDrawable, pGC, x, y, nglyph, ppci, pglyphBase)
    DrawablePtr	pDrawable;
    GCPtr	pGC;
    int 	x, y;
    unsigned int nglyph;
    CharInfoPtr *ppci;		/* array of character info */
    pointer pglyphBase;		/* start of array of glyphs */
{
    sunGXPtr	    gx = sunGXGetScreenPrivate (pDrawable->pScreen);
    int		    h, hTmp;
    FontPtr	    pfont = pGC->font;
    register int    r;
    unsigned long   *char1, *char2, *char3, *char4;
    int		    widthGlyphs, widthGlyph;
    BoxRec	    bbox;
    BoxPtr	    extents;
    RegionPtr	    clip;
    unsigned long   rop;

    widthGlyph = FONTMAXBOUNDS(pfont,characterWidth);
    h = FONTASCENT(pfont) + FONTDESCENT(pfont);
    clip = pGC->pCompositeClip;
    extents = &clip->extents;

    if (REGION_NUM_RECTS(clip) == 1)
    {
	GXSetClip (gx, extents);
    }
    else
    {
    	bbox.x1 = x + pDrawable->x;
    	bbox.x2 = bbox.x1 + (widthGlyph * nglyph);
    	bbox.y1 = y + pDrawable->y - FONTASCENT(pfont);
    	bbox.y2 = bbox.y1 + h;
    
    	switch (RECT_IN_REGION(pGC->pScreen, clip, &bbox))
    	{
	case rgnPART:
	    if (pglyphBase)
		cfbPolyGlyphBlt8(pDrawable, pGC, x, y, nglyph, ppci, NULL);
	    else
		miImageGlyphBlt(pDrawable, pGC, x, y, nglyph, ppci, pglyphBase);
	case rgnOUT:
	    return;
    	}
    }

    rop = gx_opaque_stipple_rop_table[GXcopy] | GX_PATTERN_ONES;
    if (pglyphBase)
	rop = gx_stipple_rop_table[pGC->alu] | GX_PATTERN_ONES;
    GXDrawInit (gx, pGC->fgPixel, rop, pGC->planemask);
    gx->bg = pGC->bgPixel;
    gx->mode = GX_BLIT_NOSRC | GX_MODE_COLOR1;

    y = y + pDrawable->y - FONTASCENT(pfont);
    x += pDrawable->x;

#define LoopIt(count, w, loadup, fetch) \
    	while (nglyph >= count) \
    	{ \
	    nglyph -= count; \
	    gx->incx = 0; \
	    gx->incy = 1; \
	    gx->x0 = x; \
	    gx->x1 = (x += w) - 1; \
	    gx->y0 = y; \
	    loadup \
	    hTmp = h; \
	    while (hTmp--) \
	    	gx->font = fetch; \
    	}

    if (widthGlyph <= 8)
    {
	widthGlyphs = widthGlyph << 2;
	LoopIt(4, widthGlyphs,
	    char1 = (unsigned long *) (*ppci++)->bits;
	    char2 = (unsigned long *) (*ppci++)->bits;
	    char3 = (unsigned long *) (*ppci++)->bits;
	    char4 = (unsigned long *) (*ppci++)->bits;,
	    (*char1++ | ((*char2++ | ((*char3++ | (*char4++
		    >> widthGlyph))
		    >> widthGlyph))
		    >> widthGlyph)))
    }
    else if (widthGlyph <= 10)
    {
	widthGlyphs = (widthGlyph << 1) + widthGlyph;
	LoopIt(3, widthGlyphs,
	    char1 = (unsigned long *) (*ppci++)->bits;
	    char2 = (unsigned long *) (*ppci++)->bits;
	    char3 = (unsigned long *) (*ppci++)->bits;,
	    (*char1++ | ((*char2++ | (*char3++ >> widthGlyph)) >> widthGlyph)))
    }
    else if (widthGlyph <= 16)
    {
	widthGlyphs = widthGlyph << 1;
	LoopIt(2, widthGlyphs,
	    char1 = (unsigned long *) (*ppci++)->bits;
	    char2 = (unsigned long *) (*ppci++)->bits;,
	    (*char1++ | (*char2++ >> widthGlyph)))
    }
    while (nglyph--) {
	gx->incx = 0;
	gx->incy = 1;
	gx->x0 = x;
	gx->x1 = (x += widthGlyph) - 1;
	gx->y0 = y;
	char1 = (unsigned long *) (*ppci++)->bits;
	hTmp = h;
	while (hTmp--)
	    gx->font = *char1++;
    }
    gx->incx = 0;
    gx->incy = 0;
    GXWait (gx, r);
    gx->mode = GX_BLIT_SRC | GX_MODE_COLOR8;
    GXResetClip (gx, pDrawable->pScreen);
}

static void
sunGXPolyTEGlyphBlt (pDrawable, pGC, x, y, nglyph, ppci, pglyphBase)
    DrawablePtr	pDrawable;
    GCPtr	pGC;
    int 	x, y;
    unsigned int nglyph;
    CharInfoPtr *ppci;		/* array of character info */
    pointer pglyphBase;		/* start of array of glyphs */
{
    sunGXTEGlyphBlt (pDrawable, pGC, x, y, nglyph, ppci, (char *) 1);
}

static void
sunGXFillBoxSolid (pDrawable, nBox, pBox, pixel)
    DrawablePtr	    pDrawable;
    int		    nBox;
    BoxPtr	    pBox;
    unsigned long   pixel;
{
    register sunGXPtr	gx = sunGXGetScreenPrivate (pDrawable->pScreen);
    register int	r;

    GXDrawInit(gx,pixel,gx_solid_rop_table[GXcopy]|POLY_N,~0);
    while (nBox--) {
	gx->arecty = pBox->y1;
	gx->arectx = pBox->x1;
	gx->arecty = pBox->y2;
	gx->arectx = pBox->x2;
	pBox++;
	GXDrawDone(gx,r);
    }
    GXWait(gx,r);
}

void
sunGXFillBoxStipple (pDrawable, nBox, pBox, stipple)
    DrawablePtr	    pDrawable;
    int		    nBox;
    BoxPtr	    pBox;
    sunGXStipplePtr stipple;
{
    register sunGXPtr	gx = sunGXGetScreenPrivate (pDrawable->pScreen);
    register int	r;
    int			patx, paty;

    patx = 16 - (pDrawable->x & 0xf);
    paty = 16 - (pDrawable->y & 0xf);
    stipple->patalign = (patx <<  16) | paty;
    GXDrawInit(gx,0,gx_solid_rop_table[GXcopy]|POLY_N,~0);
    GXStippleInit(gx, stipple);
    while (nBox--) {
	gx->arecty = pBox->y1;
	gx->arectx = pBox->x1;
	gx->arecty = pBox->y2;
	gx->arectx = pBox->x2;
	pBox++;
	GXDrawDone(gx,r);
    }
    GXWait(gx,r);
}

Bool
sunGXCheckTile (pPixmap, stipple)
    PixmapPtr	    pPixmap;
    sunGXStipplePtr stipple;
{
    unsigned short  *sbits;
    unsigned int    fg = (unsigned int)~0, bg = (unsigned int)~0;
    unsigned char   *tilebitsLine, *tilebits, tilebit;
    unsigned short  sbit, mask;
    int		    nbwidth;
    int		    h, w;
    int		    x, y;
    int		    s_y, s_x;

    h = pPixmap->drawable.height;
    if (h > 16 || (h & (h - 1)))
	return FALSE;
    w = pPixmap->drawable.width;
    if (w > 16 || (w & (w - 1)))
	return FALSE;
    sbits = (unsigned short *) stipple->bits;
    tilebitsLine = (unsigned char *) pPixmap->devPrivate.ptr;
    nbwidth = pPixmap->devKind;
    for (y = 0; y < h; y++) {
	tilebits = tilebitsLine;
	tilebitsLine += nbwidth;
	sbit = 0;
	mask = 1 << 15;
	for (x = 0; x < w; x++)
	{
	    tilebit = *tilebits++;
	    if (tilebit == fg)
		sbit |=  mask;
	    else if (tilebit != bg)
	    {
		if (fg == ~0)
		{
		    fg = tilebit;
		    sbit |= mask;
		}
		else if (bg == ~0)
		{
		    bg = tilebit;
		}
		else
		{
		    return FALSE;
		}
	    }
	    mask >>= 1;
	}
	for (s_x = w; s_x < 16; s_x <<= 1)
	    sbit = sbit | (sbit >> s_x);
	for (s_y = y; s_y < 16; s_y += h)
	    sbits[s_y] = sbit;
    }
    stipple->fore = fg;
    stipple->back = bg;
    return TRUE;
}

Bool
sunGXCheckStipple (pPixmap, stipple)
    PixmapPtr	    pPixmap;
    sunGXStipplePtr stipple;
{
    unsigned short  *sbits;
    unsigned long   *stippleBits;
    unsigned long   sbit, mask;
    int		    h, w;
    int		    y;
    int		    s_y, s_x;

    h = pPixmap->drawable.height;
    if (h > 16 || (h & (h - 1)))
	return FALSE;
    w = pPixmap->drawable.width;
    if (w > 16 || (w & (w - 1)))
	return FALSE;
    sbits = (unsigned short *) stipple->bits;
    stippleBits = (unsigned long *) pPixmap->devPrivate.ptr;
    mask = ((1 << w) - 1) << (16 - w);
    for (y = 0; y < h; y++) {
	sbit = (*stippleBits++ >> 16) & mask;
	for (s_x = w; s_x < 16; s_x <<= 1)
	    sbit = sbit | (sbit >> s_x);
	for (s_y = y; s_y < 16; s_y += h)
	    sbits[s_y] = sbit;
    }
    return TRUE;
}

/* cache one stipple; figuring out if we can use the stipple is as hard as
 * computing it, so we just use this one and leave it here if it
 * can't be used this time
 */

static  sunGXStipplePtr tmpStipple;

Bool
sunGXCheckFill (pGC, pDrawable)
    GCPtr	pGC;
    DrawablePtr	pDrawable;
{
    sunGXPrivGCPtr	    gxPriv = sunGXGetGCPrivate (pGC);
    sunGXStipplePtr	    stipple;
    Uint		    alu;
    int			    xrot, yrot;

    if (pGC->fillStyle == FillSolid)
    {
	if (gxPriv->stipple)
	{
	    xfree (gxPriv->stipple);
	    gxPriv->stipple = 0;
	}
	return TRUE;
    }
    if (!(stipple = gxPriv->stipple))
    {
	if (!tmpStipple)
	{
	    tmpStipple = (sunGXStipplePtr) xalloc (sizeof *tmpStipple);
	    if (!tmpStipple)
		return FALSE;
	}
	stipple = tmpStipple;
    }
    alu =  gx_opaque_stipple_rop_table[pGC->alu]|GX_PATTERN_MASK;
    switch (pGC->fillStyle) {
    case FillTiled:
	if (!sunGXCheckTile (pGC->tile.pixmap, stipple))
	{
	    if (gxPriv->stipple)
	    {
		xfree (gxPriv->stipple);
		gxPriv->stipple = 0;
	    }
	    return FALSE;
	}
	break;
    case FillStippled:
	alu = gx_stipple_rop_table[pGC->alu]|GX_PATTERN_MASK;
    case FillOpaqueStippled:
	if (!sunGXCheckStipple (pGC->stipple, stipple))
	{
	    if (gxPriv->stipple)
	    {
	    	xfree (gxPriv->stipple);
	    	gxPriv->stipple = 0;
	    }
	    return FALSE;
	}
	stipple->fore = pGC->fgPixel;
	stipple->back = pGC->bgPixel;
	break;
    }
    xrot = (pGC->patOrg.x + pDrawable->x) & 0xf;
    yrot = (pGC->patOrg.y + pDrawable->y) & 0xf;
/*
    stipple->patalign = ((16 - (xrot & 0xf)) << 16) | (16 - (yrot & 0xf));
*/
    xrot = 16 - xrot;
    yrot = 16 - yrot;
    stipple->patalign = (xrot << 16) | yrot;
    stipple->alu = alu;
    gxPriv->stipple = stipple;
    if (stipple == tmpStipple)
	tmpStipple = 0;
    return TRUE;
}

void	sunGXValidateGC ();
void	sunGXDestroyGC ();

GCFuncs	sunGXGCFuncs = {
    sunGXValidateGC,
    miChangeGC,
    miCopyGC,
    sunGXDestroyGC,
    miChangeClip,
    miDestroyClip,
    miCopyClip
};

GCOps	sunGXTEOps1Rect = {
    sunGXFillSpans,
    cfbSetSpans,
    cfbPutImage,
    sunGXCopyArea,
    sunGXCopyPlane,
    cfbPolyPoint,
    sunGXPolylines1Rect,
    sunGXPolySeg1Rect,
    miPolyRectangle,
    cfbZeroPolyArcSS8Copy,
    sunGXFillPoly1Rect,
    sunGXPolyFillRect1Rect,
    sunGXPolyFillArc,
    miPolyText8,
    miPolyText16,
    miImageText8,
    miImageText16,
    sunGXTEGlyphBlt,
    sunGXPolyTEGlyphBlt,
    cfbPushPixels8
#ifdef NEED_LINEHELPER
    ,NULL
#endif
};

GCOps	sunGXTEOps = {
    sunGXFillSpans,
    cfbSetSpans,
    cfbPutImage,
    sunGXCopyArea,
    sunGXCopyPlane,
    cfbPolyPoint,
    cfbLineSS,
    cfbSegmentSS,
    miPolyRectangle,
    cfbZeroPolyArcSS8Copy,
    miFillPolygon,
    sunGXPolyFillRect,
    sunGXPolyFillArc,
    miPolyText8,
    miPolyText16,
    miImageText8,
    miImageText16,
    sunGXTEGlyphBlt,
    sunGXPolyTEGlyphBlt,
    cfbPushPixels8
#ifdef NEED_LINEHELPER
    ,NULL
#endif
};

GCOps	sunGXNonTEOps1Rect = {
    sunGXFillSpans,
    cfbSetSpans,
    cfbPutImage,
    sunGXCopyArea,
    sunGXCopyPlane,
    cfbPolyPoint,
    sunGXPolylines1Rect,
    sunGXPolySeg1Rect,
    miPolyRectangle,
    cfbZeroPolyArcSS8Copy,
    sunGXFillPoly1Rect,
    sunGXPolyFillRect1Rect,
    sunGXPolyFillArc,
    miPolyText8,
    miPolyText16,
    miImageText8,
    miImageText16,
    miImageGlyphBlt,
    sunGXPolyGlyphBlt,
    cfbPushPixels8
#ifdef NEED_LINEHELPER
    ,NULL
#endif
};

GCOps	sunGXNonTEOps = {
    sunGXFillSpans,
    cfbSetSpans,
    cfbPutImage,
    sunGXCopyArea,
    sunGXCopyPlane,
    cfbPolyPoint,
    cfbLineSS,
    cfbSegmentSS,
    miPolyRectangle,
    cfbZeroPolyArcSS8Copy,
    miFillPolygon,
    sunGXPolyFillRect,
    sunGXPolyFillArc,
    miPolyText8,
    miPolyText16,
    miImageText8,
    miImageText16,
    miImageGlyphBlt,
    sunGXPolyGlyphBlt,
    cfbPushPixels8
#ifdef NEED_LINEHELPER
    ,NULL
#endif
};

#define FONTWIDTH(font)	(FONTMAXBOUNDS(font,rightSideBearing) - \
			 FONTMINBOUNDS(font,leftSideBearing))

GCOps *
sunGXMatchCommon (pGC, devPriv)
    GCPtr	    pGC;
    cfbPrivGCPtr    devPriv;
{
    if (pGC->lineWidth != 0)
	return 0;
    if (pGC->lineStyle != LineSolid)
	return 0;
    if (pGC->fillStyle != FillSolid)
	return 0;
    if (devPriv->rop != GXcopy)
	return 0;
    if (!WID_OK(pGC->pScreen))
	return 0;
    if (pGC->font &&
        FONTWIDTH (pGC->font) <= 32 &&
	FONTMINBOUNDS(pGC->font,characterWidth) >= 0)
    {
	if (TERMINALFONT(pGC->font))
	    if (devPriv->oneRect)
		return &sunGXTEOps1Rect;
	    else
		return &sunGXTEOps;
	else
	    if (devPriv->oneRect)
		return &sunGXNonTEOps1Rect;
	    else
		return &sunGXNonTEOps;
    }
    return 0;
}

void
sunGXValidateGC (pGC, changes, pDrawable)
    GCPtr	pGC;
    Mask	changes;
    DrawablePtr	pDrawable;
{
    int         mask;		/* stateChanges */
    int         index;		/* used for stepping through bitfields */
    int		new_rrop;
    int         new_line, new_text, new_fillspans, new_fillarea;
    int		new_rotate;
    int		xrot, yrot;
    /* flags for changing the proc vector */
    cfbPrivGCPtr devPriv;
    sunGXPrivGCPtr  gxPriv;
    int		oneRect;
    int		canGX;
    int		widOK;

    gxPriv = sunGXGetGCPrivate (pGC);
    widOK = WID_OK(pGC->pScreen);
    if (pDrawable->type != DRAWABLE_WINDOW)
    {
	if (gxPriv->type == DRAWABLE_WINDOW)
	{
	    extern GCOps    cfbNonTEOps;

	    miDestroyGCOps (pGC->ops);
	    pGC->ops = &cfbNonTEOps;
	    changes = (1 << (GCLastBit + 1)) - 1;
	    pGC->stateChanges = changes;
	    gxPriv->type = pDrawable->type;
	}
	cfbValidateGC (pGC, changes, pDrawable);
	return;
    }
    if (gxPriv->type != DRAWABLE_WINDOW)
    {
	changes = (1 << (GCLastBit + 1)) - 1;
	gxPriv->type = DRAWABLE_WINDOW;
    }

    new_rotate = pGC->lastWinOrg.x != pDrawable->x ||
		 pGC->lastWinOrg.y != pDrawable->y;

    pGC->lastWinOrg.x = pDrawable->x;
    pGC->lastWinOrg.y = pDrawable->y;

    devPriv = ((cfbPrivGCPtr) (pGC->devPrivates[cfbGCPrivateIndex].ptr));

    new_rrop = FALSE;
    new_line = FALSE;
    new_text = FALSE;
    new_fillspans = FALSE;
    new_fillarea = FALSE;

    /*
     * if the client clip is different or moved OR the subwindowMode has
     * changed OR the window's clip has changed since the last validation
     * we need to recompute the composite clip 
     */

    if ((changes & (GCClipXOrigin|GCClipYOrigin|GCClipMask|GCSubwindowMode)) ||
	(pDrawable->serialNumber != (pGC->serialNumber & DRAWABLE_SERIAL_BITS))
	)
    {
	miComputeCompositeClip(pGC, pDrawable);
	oneRect = REGION_NUM_RECTS(pGC->pCompositeClip) == 1;
	if (oneRect != devPriv->oneRect)
	{
	    new_line = TRUE;
	    new_fillarea = TRUE;
	    devPriv->oneRect = oneRect;
	}
    }

    mask = changes;
    while (mask) {
	index = lowbit (mask);
	mask &= ~index;

	/*
	 * this switch acculmulates a list of which procedures might have
	 * to change due to changes in the GC.  in some cases (e.g.
	 * changing one 16 bit tile for another) we might not really need
	 * a change, but the code is being paranoid. this sort of batching
	 * wins if, for example, the alu and the font have been changed,
	 * or any other pair of items that both change the same thing. 
	 */
	switch (index) {
	case GCFunction:
	case GCForeground:
	    new_rrop = TRUE;
	    break;
	case GCPlaneMask:
	    new_rrop = TRUE;
	    new_text = TRUE;
	    break;
	case GCBackground:
	    break;
	case GCLineStyle:
	case GCLineWidth:
	    new_line = TRUE;
	    break;
	case GCCapStyle:
	    break;
	case GCJoinStyle:
	    break;
	case GCFillStyle:
	    new_text = TRUE;
	    new_fillspans = TRUE;
	    new_line = TRUE;
	    new_fillarea = TRUE;
	    break;
	case GCFillRule:
	    break;
	case GCTile:
	    new_fillspans = TRUE;
	    new_fillarea = TRUE;
	    break;

	case GCStipple:
	    new_fillspans = TRUE;
	    new_fillarea = TRUE;
	    break;

	case GCTileStipXOrigin:
	    new_rotate = TRUE;
	    break;

	case GCTileStipYOrigin:
	    new_rotate = TRUE;
	    break;

	case GCFont:
	    new_text = TRUE;
	    break;
	case GCSubwindowMode:
	    break;
	case GCGraphicsExposures:
	    break;
	case GCClipXOrigin:
	    break;
	case GCClipYOrigin:
	    break;
	case GCClipMask:
	    break;
	case GCDashOffset:
	    break;
	case GCDashList:
	    break;
	case GCArcMode:
	    break;
	default:
	    break;
	}
    }

    /*
     * If the drawable has changed,  check its depth & ensure suitable
     * entries are in the proc vector. 
     */
    if (pDrawable->serialNumber != (pGC->serialNumber & (DRAWABLE_SERIAL_BITS))) {
	new_fillspans = TRUE;	/* deal with FillSpans later */
    }

    if ((new_rotate || new_fillspans))
    {
	Bool new_pix = FALSE;
	xrot = pGC->patOrg.x + pDrawable->x;
	yrot = pGC->patOrg.y + pDrawable->y;

	if (!sunGXCheckFill (pGC, pDrawable))
	{
	    switch (pGC->fillStyle)
	    {
	    case FillTiled:
	    	if (!pGC->tileIsPixel)
	    	{
		    int width = pGC->tile.pixmap->drawable.width * PSZ;
    
		    if ((width <= 32) && !(width & (width - 1)))
		    {
		    	cfbCopyRotatePixmap(pGC->tile.pixmap,
					    &pGC->pRotatedPixmap,
					    xrot, yrot);
		    	new_pix = TRUE;
		    }
	    	}
	    	break;
	    case FillStippled:
	    case FillOpaqueStippled:
	    	{
		    int width = pGC->stipple->drawable.width;
    
		    if ((width <= 32) && !(width & (width - 1)))
		    {
		    	mfbCopyRotatePixmap(pGC->stipple,
					    &pGC->pRotatedPixmap, xrot, yrot);
		    	new_pix = TRUE;
		    }
	    	}
	    	break;
	    }
	}
	if (!new_pix && pGC->pRotatedPixmap)
	{
	    cfbDestroyPixmap(pGC->pRotatedPixmap);
	    pGC->pRotatedPixmap = (PixmapPtr) NULL;
	}
    }

    if (new_rrop)
    {
	int old_rrop;

	if (gxPriv->stipple)
	{
	    if (pGC->fillStyle == FillStippled)
		gxPriv->stipple->alu = gx_stipple_rop_table[pGC->alu]|GX_PATTERN_MASK;
	    else
		gxPriv->stipple->alu = gx_opaque_stipple_rop_table[pGC->alu]|GX_PATTERN_MASK;
	    if (pGC->fillStyle != FillTiled)
	    {
		gxPriv->stipple->fore = pGC->fgPixel;
		gxPriv->stipple->back = pGC->bgPixel;
	    }
	}
	old_rrop = devPriv->rop;
	devPriv->rop = cfbReduceRasterOp (pGC->alu, pGC->fgPixel,
					   pGC->planemask,
					   &devPriv->and, &devPriv->xor);
	if (old_rrop == devPriv->rop)
	    new_rrop = FALSE;
	else
	{
	    new_line = TRUE;
	    new_text = TRUE;
	    new_fillspans = TRUE;
	    new_fillarea = TRUE;
	}
    }

    if (new_rrop || new_fillspans || new_text || new_fillarea || new_line)
    {
	GCOps	*newops;

	if ((newops = sunGXMatchCommon (pGC, devPriv)))
 	{
	    if (pGC->ops->devPrivate.val)
		miDestroyGCOps (pGC->ops);
	    pGC->ops = newops;
	    new_rrop = new_line = new_fillspans = new_text = new_fillarea = 0;
	}
 	else
 	{
	    if (!pGC->ops->devPrivate.val)
	    {
		pGC->ops = miCreateGCOps (pGC->ops);
		pGC->ops->devPrivate.val = 1;
	    }
	}
    }

    canGX = pGC->fillStyle == FillSolid || gxPriv->stipple;

    /* deal with the changes we've collected */
    if (new_line)
    {
	pGC->ops->FillPolygon = miFillPolygon;
	if (devPriv->oneRect && canGX)
	    pGC->ops->FillPolygon = sunGXFillPoly1Rect;
	if (pGC->lineWidth == 0)
	{
	    if ((pGC->lineStyle == LineSolid) && (pGC->fillStyle == FillSolid))
	    {
		switch (devPriv->rop)
		{
		case GXxor:
		    pGC->ops->PolyArc = cfbZeroPolyArcSS8Xor;
		    break;
		case GXcopy:
		    pGC->ops->PolyArc = cfbZeroPolyArcSS8Copy;
		    break;
		default:
		    pGC->ops->PolyArc = cfbZeroPolyArcSS8General;
		    break;
		}
	    }
	    else
		pGC->ops->PolyArc = miZeroPolyArc;
	}
	else
	    pGC->ops->PolyArc = miPolyArc;
	pGC->ops->PolySegment = miPolySegment;
	switch (pGC->lineStyle)
	{
	case LineSolid:
	    if(pGC->lineWidth == 0)
	    {
		if (devPriv->oneRect && canGX && widOK)
		{
		    pGC->ops->PolySegment = sunGXPolySeg1Rect;
		    pGC->ops->Polylines = sunGXPolylines1Rect;
		}
		else if (pGC->fillStyle == FillSolid)
		{
		    if (devPriv->oneRect)
		    {
			pGC->ops->Polylines = cfb8LineSS1Rect;
			pGC->ops->PolySegment = cfb8SegmentSS1Rect;
		    }
		    else
		    {
		    	pGC->ops->Polylines = cfbLineSS;
		    	pGC->ops->PolySegment = cfbSegmentSS;
		    }
		}
		else
		    pGC->ops->Polylines = miZeroLine;
	    }
	    else
		pGC->ops->Polylines = miWideLine;
	    break;
	case LineOnOffDash:
	case LineDoubleDash:
	    if (pGC->lineWidth == 0 && pGC->fillStyle == FillSolid)
	    {
		pGC->ops->Polylines = cfbLineSD;
		pGC->ops->PolySegment = cfbSegmentSD;
	    } else
		pGC->ops->Polylines = miWideDash;
	    break;
	}
    }

    if (new_text && (pGC->font))
    {
        if (FONTWIDTH(pGC->font) > 32 ||
	    FONTMINBOUNDS(pGC->font,characterWidth) < 0)
        {
            pGC->ops->PolyGlyphBlt = miPolyGlyphBlt;
            pGC->ops->ImageGlyphBlt = miImageGlyphBlt;
        }
        else
        {
	    if (pGC->fillStyle == FillSolid) 
	    {
		if (TERMINALFONT (pGC->font))
		    pGC->ops->PolyGlyphBlt = sunGXPolyTEGlyphBlt;
		else
		    pGC->ops->PolyGlyphBlt = sunGXPolyGlyphBlt;
	    }
	    else
		pGC->ops->PolyGlyphBlt = miPolyGlyphBlt;
            /* special case ImageGlyphBlt for terminal emulator fonts */
            if (TERMINALFONT(pGC->font))
		pGC->ops->ImageGlyphBlt = sunGXTEGlyphBlt;
            else
                pGC->ops->ImageGlyphBlt = miImageGlyphBlt;
        }
    }    


    if (new_fillspans) {
	if (canGX)
	    pGC->ops->FillSpans = sunGXFillSpans;
	else switch (pGC->fillStyle) {
	case FillTiled:
	    if (pGC->pRotatedPixmap)
	    {
		if (pGC->alu == GXcopy && (pGC->planemask & PMSK) == PMSK)
		    pGC->ops->FillSpans = cfbTile32FSCopy;
		else
		    pGC->ops->FillSpans = cfbTile32FSGeneral;
	    }
	    else
		pGC->ops->FillSpans = cfbUnnaturalTileFS;
	    break;
	case FillStippled:
	    if (pGC->pRotatedPixmap)
		pGC->ops->FillSpans = cfb8Stipple32FS;
	    else
		pGC->ops->FillSpans = cfbUnnaturalStippleFS;
	    break;
	case FillOpaqueStippled:
	    if (pGC->pRotatedPixmap)
		pGC->ops->FillSpans = cfb8OpaqueStipple32FS;
	    else
		pGC->ops->FillSpans = cfbUnnaturalStippleFS;
	    break;
	default:
	    FatalError("cfbValidateGC: illegal fillStyle\n");
	}
    } /* end of new_fillspans */

    if (new_fillarea) {
	pGC->ops->PolyFillRect = cfbPolyFillRect;
	pGC->ops->PolyFillArc = miPolyFillArc;
	if (canGX)
	{
	    pGC->ops->PolyFillArc = sunGXPolyFillArc;
	    pGC->ops->PolyFillRect = sunGXPolyFillRect;
	    if (devPriv->oneRect)
		pGC->ops->PolyFillRect = sunGXPolyFillRect1Rect;
	}
	pGC->ops->PushPixels = mfbPushPixels;
	if (pGC->fillStyle == FillSolid && devPriv->rop == GXcopy)
	    pGC->ops->PushPixels = cfbPushPixels8;
    }
}

void
sunGXDestroyGC (pGC)
    GCPtr   pGC;
{
    sunGXPrivGCPtr	    gxPriv = sunGXGetGCPrivate (pGC);

    if (gxPriv->stipple)
	xfree (gxPriv->stipple);
    miDestroyGC (pGC);
}

Bool
sunGXCreateGC (pGC)
    GCPtr   pGC;
{
    sunGXPrivGCPtr  gxPriv;
    if (pGC->depth == 1)
	return mfbCreateGC (pGC);
    if (!cfbCreateGC (pGC))
	return FALSE;
    pGC->ops = &sunGXNonTEOps;
    pGC->funcs = &sunGXGCFuncs;
    gxPriv = sunGXGetGCPrivate(pGC);
    gxPriv->type = DRAWABLE_WINDOW;
    gxPriv->stipple = 0;
    return TRUE;
}

Bool
sunGXCreateWindow (pWin)
    WindowPtr	pWin;
{
    if (!cfbCreateWindow (pWin))
	return FALSE;
    pWin->devPrivates[sunGXWindowPrivateIndex].ptr = 0;
    return TRUE;
}

Bool
sunGXDestroyWindow (pWin)
    WindowPtr	pWin;
{
    sunGXStipplePtr stipple = sunGXGetWindowPrivate(pWin);
    xfree (stipple);
    return cfbDestroyWindow (pWin);
}

Bool
sunGXChangeWindowAttributes (pWin, mask)
    WindowPtr	pWin;
    Mask	mask;
{
    sunGXStipplePtr stipple;
    Mask	    index;
    WindowPtr	pBgWin;
    register cfbPrivWin *pPrivWin;
    int		    width;

    pPrivWin = (cfbPrivWin *)(pWin->devPrivates[cfbWindowPrivateIndex].ptr);
    /*
     * When background state changes from ParentRelative and
     * we had previously rotated the fast border pixmap to match
     * the parent relative origin, rerotate to match window
     */
    if (mask & (CWBackPixmap | CWBackPixel) &&
	pWin->backgroundState != ParentRelative &&
	pPrivWin->fastBorder &&
	(pPrivWin->oldRotate.x != pWin->drawable.x ||
	 pPrivWin->oldRotate.y != pWin->drawable.y))
    {
	cfbXRotatePixmap(pPrivWin->pRotatedBorder,
		      pWin->drawable.x - pPrivWin->oldRotate.x);
	cfbYRotatePixmap(pPrivWin->pRotatedBorder,
		      pWin->drawable.y - pPrivWin->oldRotate.y);
	pPrivWin->oldRotate.x = pWin->drawable.x;
	pPrivWin->oldRotate.y = pWin->drawable.y;
    }
    while (mask)
    {
	index = lowbit(mask);
	mask &= ~index;
	switch (index)
	{
	case CWBackPixmap:
	    stipple = sunGXGetWindowPrivate(pWin);
	    if (pWin->backgroundState == None ||
		pWin->backgroundState == ParentRelative)
	    {
		pPrivWin->fastBackground = FALSE;
		if (stipple)
		{
		    xfree (stipple);
		    sunGXSetWindowPrivate(pWin,0);
		}
		/* Rotate border to match parent origin */
		if (pWin->backgroundState == ParentRelative &&
		    pPrivWin->pRotatedBorder) 
		{
		    for (pBgWin = pWin->parent;
			 pBgWin->backgroundState == ParentRelative;
			 pBgWin = pBgWin->parent);
		    cfbXRotatePixmap(pPrivWin->pRotatedBorder,
				  pBgWin->drawable.x - pPrivWin->oldRotate.x);
		    cfbYRotatePixmap(pPrivWin->pRotatedBorder,
				  pBgWin->drawable.y - pPrivWin->oldRotate.y);
		}
		
		break;
	    }
	    if (!stipple)
	    {
		if (!tmpStipple)
		    tmpStipple = (sunGXStipplePtr) xalloc (sizeof *tmpStipple);
		stipple = tmpStipple;
	    }
 	    if (stipple && sunGXCheckTile (pWin->background.pixmap, stipple))
	    {
		stipple->alu = gx_opaque_stipple_rop_table[GXcopy]|GX_PATTERN_MASK;
		pPrivWin->fastBackground = FALSE;
		if (stipple == tmpStipple)
		{
		    sunGXSetWindowPrivate(pWin, stipple);
		    tmpStipple = 0;
		}
		break;
	    }
	    if ((stipple = sunGXGetWindowPrivate(pWin)))
	    {
		xfree (stipple);
		sunGXSetWindowPrivate(pWin,0);
	    }	    
 	    if (((width = (pWin->background.pixmap->drawable.width * PSZ)) <= 32) &&
		       !(width & (width - 1)))
	    {
		cfbCopyRotatePixmap(pWin->background.pixmap,
				  &pPrivWin->pRotatedBackground,
				  pWin->drawable.x,
				  pWin->drawable.y);
		if (pPrivWin->pRotatedBackground)
		{
    	    	    pPrivWin->fastBackground = TRUE;
    	    	    pPrivWin->oldRotate.x = pWin->drawable.x;
    	    	    pPrivWin->oldRotate.y = pWin->drawable.y;
		}
		else
		{
		    pPrivWin->fastBackground = FALSE;
		}
		break;
	    }
	    pPrivWin->fastBackground = FALSE;
	    break;

	case CWBackPixel:
	    pPrivWin->fastBackground = FALSE;
	    break;

	case CWBorderPixmap:
	    /* don't bother with accelerator for border tiles (just lazy) */
	    if (((width = (pWin->border.pixmap->drawable.width * PSZ)) <= 32) &&
		!(width & (width - 1)))
	    {
		for (pBgWin = pWin;
		     pBgWin->backgroundState == ParentRelative;
		     pBgWin = pBgWin->parent);
		cfbCopyRotatePixmap(pWin->border.pixmap,
				    &pPrivWin->pRotatedBorder,
				    pBgWin->drawable.x,
				    pBgWin->drawable.y);
		if (pPrivWin->pRotatedBorder)
		{
		    pPrivWin->fastBorder = TRUE;
		    pPrivWin->oldRotate.x = pBgWin->drawable.x;
		    pPrivWin->oldRotate.y = pBgWin->drawable.y;
		}
		else
		{
		    pPrivWin->fastBorder = TRUE;
		}
	    }
	    else
	    {
		pPrivWin->fastBorder = FALSE;
	    }
	    break;
	case CWBorderPixel:
	    pPrivWin->fastBorder = FALSE;
	    break;
	}
    }
    return (TRUE);
}

void
sunGXPaintWindow(pWin, pRegion, what)
    WindowPtr	pWin;
    RegionPtr	pRegion;
    int		what;
{
    register cfbPrivWin	*pPrivWin;
    sunGXStipplePtr stipple;
    WindowPtr	pBgWin;
    pPrivWin = (cfbPrivWin *)(pWin->devPrivates[cfbWindowPrivateIndex].ptr);

    switch (what) {
    case PW_BACKGROUND:
	stipple = sunGXGetWindowPrivate(pWin);
	switch (pWin->backgroundState) {
	case None:
	    return;
	case ParentRelative:
	    do {
		pWin = pWin->parent;
	    } while (pWin->backgroundState == ParentRelative);
	    (*pWin->drawable.pScreen->PaintWindowBackground)(pWin, pRegion,
							     what);
	    return;
	case BackgroundPixmap:
	    if (stipple)
	    {
		sunGXFillBoxStipple ((DrawablePtr)pWin,
				  (int)REGION_NUM_RECTS(pRegion),
				  REGION_RECTS(pRegion),
				  stipple);
	    }
	    else if (pPrivWin->fastBackground)
	    {
		cfbFillBoxTile32 ((DrawablePtr)pWin,
				  (int)REGION_NUM_RECTS(pRegion),
				  REGION_RECTS(pRegion),
				  pPrivWin->pRotatedBackground);
	    }
	    else
	    {
		cfbFillBoxTileOdd ((DrawablePtr)pWin,
				   (int)REGION_NUM_RECTS(pRegion),
				   REGION_RECTS(pRegion),
				   pWin->background.pixmap,
				   (int) pWin->drawable.x, (int) pWin->drawable.y);
	    }
	    return;
	case BackgroundPixel:
	    sunGXFillBoxSolid((DrawablePtr)pWin,
			     (int)REGION_NUM_RECTS(pRegion),
			     REGION_RECTS(pRegion),
			     pWin->background.pixel);
	    return;
    	}
    	break;
    case PW_BORDER:
	if (pWin->borderIsPixel)
	{
	    sunGXFillBoxSolid((DrawablePtr)pWin,
			     (int)REGION_NUM_RECTS(pRegion),
			     REGION_RECTS(pRegion),
			     pWin->border.pixel);
	    return;
	}
	else if (pPrivWin->fastBorder)
	{
	    cfbFillBoxTile32 ((DrawablePtr)pWin,
			      (int)REGION_NUM_RECTS(pRegion),
			      REGION_RECTS(pRegion),
			      pPrivWin->pRotatedBorder);
	    return;
	}
	else
	{
	    for (pBgWin = pWin;
		 pBgWin->backgroundState == ParentRelative;
		 pBgWin = pBgWin->parent);

	    cfbFillBoxTileOdd ((DrawablePtr)pWin,
			       (int)REGION_NUM_RECTS(pRegion),
			       REGION_RECTS(pRegion),
			       pWin->border.pixmap,
			       (int) pBgWin->drawable.x,
 			       (int) pBgWin->drawable.y);
	    return;
	}
	break;
    }
}

void 
sunGXCopyWindow(pWin, ptOldOrg, prgnSrc)
    WindowPtr pWin;
    DDXPointRec ptOldOrg;
    RegionPtr prgnSrc;
{
    DDXPointPtr pptSrc;
    register DDXPointPtr ppt;
    RegionPtr prgnDst;
    register BoxPtr pbox;
    register int dx, dy;
    register int i, nbox;
    WindowPtr pwinRoot;
    extern WindowPtr *WindowTable;

    pwinRoot = WindowTable[pWin->drawable.pScreen->myNum];

    prgnDst = REGION_CREATE(pWin->drawable.pScreen, NULL, 1);

    dx = ptOldOrg.x - pWin->drawable.x;
    dy = ptOldOrg.y - pWin->drawable.y;
    REGION_TRANSLATE(pWin->drawable.pScreen, prgnSrc, -dx, -dy);
    REGION_INTERSECT(pWin->drawable.pScreen, prgnDst, &pWin->borderClip, prgnSrc);

    pbox = REGION_RECTS(prgnDst);
    nbox = REGION_NUM_RECTS(prgnDst);
    if(!(pptSrc = (DDXPointPtr )ALLOCATE_LOCAL(nbox * sizeof(DDXPointRec))))
	return;
    ppt = pptSrc;

    for (i = nbox; --i >= 0; ppt++, pbox++)
    {
	ppt->x = pbox->x1 + dx;
	ppt->y = pbox->y1 + dy;
    }

    sunGXDoBitblt ((DrawablePtr)pwinRoot, (DrawablePtr)pwinRoot,
		    GXcopy, prgnDst, pptSrc, ~0L);
    DEALLOCATE_LOCAL(pptSrc);
    REGION_DESTROY(pWin->drawable.pScreen, prgnDst);
}

Bool
sunGXInit (
    ScreenPtr	pScreen,
    fbFd	*fb)
{
    sunGXPtr	    gx;
    Uint	    mode;
    register long   r;

    if (serverGeneration != sunGXGeneration)
    {
	sunGXScreenPrivateIndex = AllocateScreenPrivateIndex();
	if (sunGXScreenPrivateIndex == -1)
	    return FALSE;
	sunGXGCPrivateIndex = AllocateGCPrivateIndex ();
	sunGXWindowPrivateIndex = AllocateWindowPrivateIndex ();
	sunGXGeneration = serverGeneration;
    }
    if (!AllocateGCPrivate(pScreen, sunGXGCPrivateIndex, sizeof (sunGXPrivGCRec)))
	return FALSE;
    if (!AllocateWindowPrivate(pScreen, sunGXWindowPrivateIndex, 0))
	return FALSE;
    gx = (sunGXPtr) fb->fb;
    mode = gx->mode;
    GXWait(gx,r);
    mode &= ~(	GX_BLIT_ALL |
		GX_MODE_ALL | 
		GX_DRAW_ALL |
 		GX_BWRITE0_ALL |
		GX_BWRITE1_ALL |
 		GX_BREAD_ALL |
 		GX_BDISP_ALL);
    mode |=	GX_BLIT_SRC |
		GX_MODE_COLOR8 |
		GX_DRAW_RENDER |
		GX_BWRITE0_ENABLE |
		GX_BWRITE1_DISABLE |
		GX_BREAD_0 |
		GX_BDISP_0;
    gx->mode = mode;
    gx->clip = 0;
    gx->offx = 0;
    gx->offy = 0;
    gx->clipminx = 0;
    gx->clipminy = 0;
    gx->clipmaxx = fb->info.fb_width - 1;
    gx->clipmaxy = fb->info.fb_height - 1;
    pScreen->devPrivates[sunGXScreenPrivateIndex].ptr = (pointer) gx;
    /*
     * Replace various screen functions
     */
    pScreen->CreateGC = sunGXCreateGC;
    pScreen->CreateWindow = sunGXCreateWindow;
    pScreen->ChangeWindowAttributes = sunGXChangeWindowAttributes;
    pScreen->DestroyWindow = sunGXDestroyWindow;
    pScreen->PaintWindowBackground = sunGXPaintWindow;
    pScreen->PaintWindowBorder = sunGXPaintWindow;
    pScreen->CopyWindow = sunGXCopyWindow;
    return TRUE;
}
