/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tga/tga_line.c,v 1.2 1999/12/16 02:26:30 robin Exp $ */

/*
 * Copyright 1999 by Matthew Grossman, Seattle, USA.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Matthew
 * Grossman not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Matthew Grossman makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * MATTHEW GROSSMAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL MATTHEW GROSSMAN BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Author:  Matthew Grossman, mattg@oz.net
 * 
 */

/* tga_line.c */
/* accelerated solid and dashed lines */
/* adapted from xaa/xaaLine.c */

#include "X.h"
#include "misc.h"
#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86_OSproc.h"

#include "scrnintstr.h"
#include "pixmapstr.h"
#include "miline.h"
#include "xf86str.h"
#include "xaa.h"
#include "xaalocal.h"

/*  #include "tga.h" */
#include "tga_regs.h"

/* line functions */
extern void
TGASetupForSolidLine(ScrnInfoPtr pScrn, int color, int rop,
		     unsigned int planemask);
extern void
TGASubsequentSolidHorVertLine(ScrnInfoPtr pScrn, int x, int y, int len,
			      int dir);
extern void
TGASubsequentSolidLine(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2,
		       int octant, int flags);
extern void
TGASetupForClippedLine(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2,
		       int octant);
extern void
TGASubsequentClippedSolidLine(ScrnInfoPtr pScrn, int x1, int y1, int len,
			      int err);

extern void
TGASetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, int rop,
		      unsigned int planemask, int length,
		      unsigned char *pattern);
extern void
TGASubsequentDashedLine(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2,
			int octant, int flags, int phase);
extern void
TGASubsequentClippedDashedLine(ScrnInfoPtr pScrn, int x1, int y1, int len,
			       int err, int phase);


extern void
TGASync(ScrnInfoPtr pScrn);

void TGAPolySegment(DrawablePtr pDrawable, GCPtr pGC, int nseg,
		    xSegment *pSeg);
void TGAPolyLines(DrawablePtr pDrawable, GCPtr pGC, int mode, int npt,
		  DDXPointPtr pptInit);
void TGAPolySegmentDashed(DrawablePtr pDrawable, GCPtr pGC, int	nseg,
			  xSegment *pSeg);
void TGAPolyLinesDashed(DrawablePtr pDrawable, GCPtr pGC, int mode, int npt,
			DDXPointPtr pptInit);


void
#ifdef POLYSEGMENT
TGAPolySegment(
	       DrawablePtr	pDrawable,
	       GCPtr	pGC,
	       int		nseg,
	       xSegment	*pSeg
#else
	       TGAPolyLines(
			    DrawablePtr pDrawable,
			    GCPtr	pGC,
			    int		mode,		/* Origin or Previous */
			    int		npt,		/* number of points */
			    DDXPointPtr pptInit
#endif
			    ){
  XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_GC(pGC);
  BoxPtr pboxInit = REGION_RECTS(pGC->pCompositeClip);
  int nboxInit = REGION_NUM_RECTS(pGC->pCompositeClip);
  unsigned int bias = miGetZeroLineBias(pDrawable->pScreen);
  int xorg = pDrawable->x;
  int yorg = pDrawable->y;
  int nbox;
  BoxPtr pbox;
#ifndef POLYSEGMENT
  DDXPointPtr ppt;
#endif
  int x1, x2, y1, y2, tmp, len;

#ifdef POLYSEGMENT
/*    ErrorF("TGAPolySegment called!\n"); */
#else
/*    ErrorF("TGAPolyLines called\n"); */
#endif
    
  if(!nboxInit)
    return;
  /****************/
  /* TGA FUNCTION */
  /****************/
  TGASetupForSolidLine(infoRec->pScrn, pGC->fgPixel, pGC->alu,
		       pGC->planemask);

#ifdef POLYSEGMENT
  while (nseg--)
#else
    ppt = pptInit;
  x2 = ppt->x + xorg;
  y2 = ppt->y + yorg;
  while(--npt)
#endif
    {
      nbox = nboxInit;
      pbox = pboxInit;

#ifdef POLYSEGMENT
      x1 = pSeg->x1 + xorg;	
      y1 = pSeg->y1 + yorg;
      x2 = pSeg->x2 + xorg;	
      y2 = pSeg->y2 + yorg;
      pSeg++;
#else
      x1 = x2; 
      y1 = y2;
      ++ppt;
      if (mode == CoordModePrevious) {
	xorg = x1; 
	yorg = y1;
      }
      x2 = ppt->x + xorg; 
      y2 = ppt->y + yorg;
#endif

      if (x1 == x2) { /* vertical line */
	/* make the line go top to bottom of screen, keeping
	   endpoint semantics
	*/
	if (y1 > y2) {
	  tmp = y2; 
	  y2 = y1 + 1; 
	  y1 = tmp + 1;
#ifdef POLYSEGMENT
	  if (pGC->capStyle != CapNotLast) y1--;
#endif
	}
#ifdef POLYSEGMENT
	else if (pGC->capStyle != CapNotLast) y2++;
#endif
	/* get to first band that might contain part of line */
	while(nbox && (pbox->y2 <= y1)) {
	  pbox++;
	  nbox--;
	}

	/* stop when lower edge of box is beyond end of line */
	while(nbox && (y2 >= pbox->y1)) {
	  if ((x1 >= pbox->x1) && (x1 < pbox->x2)) {
	    tmp = max(y1, pbox->y1);
	    len = min(y2, pbox->y2) - tmp;
	    if (len)
	      TGASubsequentSolidHorVertLine(infoRec->pScrn, x1, tmp,
					    len, DEGREES_270);
	  }
	  nbox--;
	  pbox++;
	}
#ifndef POLYSEGMENT
	y2 = ppt->y + yorg;
#endif
      } else if (y1 == y2) { /* horizontal line */
	/* force line from left to right, keeping endpoint semantics */
	if (x1 > x2) {
	  tmp = x2; 
	  x2 = x1 + 1; 
	  x1 = tmp + 1;
#ifdef POLYSEGMENT
	  if (pGC->capStyle != CapNotLast)  x1--;
#endif
	}
#ifdef POLYSEGMENT
	else if (pGC->capStyle != CapNotLast) x2++;
#endif

	/* find the correct band */
	while(nbox && (pbox->y2 <= y1)) {
	  pbox++;
	  nbox--;
	}

	/* try to draw the line, if we haven't gone beyond it */
	if (nbox && (pbox->y1 <= y1)) {
	  int orig_y = pbox->y1;
	  /* when we leave this band, we're done */
	  while(nbox && (orig_y == pbox->y1)) {
	    if (pbox->x2 <= x1) {
	      /* skip boxes until one might contain start point */
	      nbox--;
	      pbox++;
	      continue;
	    }

	    /* stop if left of box is beyond right of line */
	    if (pbox->x1 >= x2) {
	      nbox = 0;
	      break;
	    }

	    tmp = max(x1, pbox->x1);
	    len = min(x2, pbox->x2) - tmp;
	    if (len)
	      TGASubsequentSolidHorVertLine(infoRec->pScrn, tmp,
					    y1, len, DEGREES_0);
	    nbox--;
	    pbox++;
	  }
	}
#ifndef POLYSEGMENT
	x2 = ppt->x + xorg;
#endif
      } else{ /* sloped line */
	unsigned int oc1, oc2;
	int dmin, dmaj, e, octant;


	if((dmaj = x2 - x1) < 0) {
	  dmaj = -dmaj;
	  octant = XDECREASING;
	} else octant = 0;		   

	if((dmin = y2 - y1) < 0) {
	  dmin = -dmin;
	  octant |= YDECREASING;
	}	
	
	if(dmin >= dmaj){
	  tmp = dmin; dmin = dmaj; dmaj = tmp;
	  octant |= YMAJOR;
	}

	e = -dmaj - ((bias >> octant) & 1);
	len = dmaj;
	dmin *= 2;
	dmaj *= 2;


	while(nbox--) {
	  oc1 = oc2 = 0;
	  OUTCODES(oc1, x1, y1, pbox);
	  OUTCODES(oc2, x2, y2, pbox);
	  if (!(oc1 | oc2)) {   /* unclipped */
	    TGASubsequentSolidLine(infoRec->pScrn, x1, y1, x2, y2,
				   octant, 
#ifdef POLYSEGMENT
				   (pGC->capStyle != CapNotLast) ? 0 :
#endif
				   OMIT_LAST
				   );
	    break;
	  } else if (oc1 & oc2) { /* completely clipped */
	    pbox++;
		    
	  } else { /* partially clipped */
	    int new_x1 = x1, new_y1 = y1, new_x2 = x2, new_y2 = y2;
	    int clip1 = 0, clip2 = 0;
	    int err, adx, ady;
		    
	    if(octant & YMAJOR) {
	      ady = dmaj /= 2;
	      adx = dmin /= 2;
	    } else {
	      ady = dmin /= 2;
	      adx = dmaj /= 2;
	    }

	    if (miZeroClipLine(pbox->x1, pbox->y1, 
			       pbox->x2 - 1, pbox->y2 - 1,
			       &new_x1, &new_y1, &new_x2, &new_y2,
			       adx, ady, &clip1, &clip2,
			       octant, bias, oc1, oc2) == -1)
	      {
		pbox++;
		continue;
	      }

	    if (octant & YMAJOR)
	      len = abs(new_y2 - new_y1);
	    else
	      len = abs(new_x2 - new_x1);
#ifdef POLYSEGMENT
	    if (clip2 != 0 || pGC->capStyle != CapNotLast)
	      len++;
#else
	    len += (clip2 != 0);
#endif
	    if (len) { /* we have a real line */
	      int abserr, clipdx, clipdy;

	      /* unwind bresenham error term to first point */
	      if (clip1) {
		clipdx = abs(new_x1 - x1);
		clipdy = abs(new_y1 - y1);

		if (octant & YMAJOR)
		  err = e + clipdy*dmin - clipdx*dmaj;
		else
		  err = e + clipdx*dmin - clipdy*dmaj;
	      } else
		err = e;

#define range infoRec->SolidBresenhamLineErrorTermBits
	      abserr = abs(err);			    
	      while((abserr & range) || 
		    (dmaj & range) ||
		    (dmin & range)) {
		dmin /= 2;
		dmaj /= 2;
		abserr /= 2;
		err /= 2;
	      }
	      TGASetupForClippedLine(infoRec->pScrn, x1, y1, x2,
				     y2, octant);
	      TGASubsequentClippedSolidLine(infoRec->pScrn,
					    new_x1, new_y1, len,
					    err);


	    }
	    pbox++;
	  }
	} /* while (nbox--) */
      } /* sloped line */
    } /* while (nline--) */

#ifndef POLYSEGMENT
  /* paint the last point if the end style isn't CapNotLast.
       (Assume that a projecting, butt, or round cap that is one
        pixel wide is the same as the single pixel of the endpoint.)
    */

  if ((pGC->capStyle != CapNotLast) &&
      ((ppt->x + xorg != pptInit->x + pDrawable->x) ||
       (ppt->y + yorg != pptInit->y + pDrawable->y) ||
       (ppt == pptInit + 1)))
    {
      nbox = nboxInit;
      pbox = pboxInit;
      while (nbox--)
	{
	  if ((x2 >= pbox->x1) && (y2 >= pbox->y1) &&
	      (x2 <  pbox->x2) && (y2 <  pbox->y2))
	    {
	      TGASubsequentSolidHorVertLine(infoRec->pScrn, x2, y2, 1,
					    DEGREES_0);
	      break;
	    }
	  else
	    pbox++;
	}
    }
#endif

  TGASync(infoRec->pScrn);
  return;
}

#undef range

  void
#ifdef POLYSEGMENT
  TGAPolySegmentDashed(
		       DrawablePtr	pDrawable,
		       GCPtr	pGC,
		       int		nseg,
		       xSegment	*pSeg
#else
		       TGAPolyLinesDashed(
					  DrawablePtr pDrawable,
					  GCPtr	pGC,
					  int		mode,		/* Origin or Previous */
					  int		npt,		/* number of points */
					  DDXPointPtr pptInit
#endif
					  ){
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_GC(pGC);
    XAAGCPtr   pGCPriv = (XAAGCPtr) (pGC)->devPrivates[XAAGCIndex].ptr;
    BoxPtr pboxInit = REGION_RECTS(pGC->pCompositeClip);
    int nboxInit = REGION_NUM_RECTS(pGC->pCompositeClip);
    unsigned int bias = miGetZeroLineBias(pDrawable->pScreen);
    int xorg = pDrawable->x;
    int yorg = pDrawable->y;
    int nbox;
    BoxPtr pbox;
#ifndef POLYSEGMENT
    DDXPointPtr ppt;
#endif
    unsigned int oc1, oc2;
    int dmin, dmaj, e, octant;
    int x1, x2, y1, y2, tmp, len, offset;
    int PatternLength, PatternOffset;

#ifdef POLYSEGMENT
/*      ErrorF("TGAPolySegmentDashed called\n"); */
#else
/*      ErrorF("TGAPolyLinesDashed called\n"); */
#endif
    
    if(!nboxInit)
      return;

    PatternLength = pGCPriv->DashLength; 
    PatternOffset = pGC->dashOffset % PatternLength;

    TGASetupForDashedLine(infoRec->pScrn, pGC->fgPixel,
			  (pGC->lineStyle == LineDoubleDash) ? pGC->bgPixel : -1,
			  pGC->alu, pGC->planemask, PatternLength, pGCPriv->DashPattern);


#ifdef POLYSEGMENT
    while (nseg--)
#else
      ppt = pptInit;
    x2 = ppt->x + xorg;
    y2 = ppt->y + yorg;
    while(--npt)
#endif
      {
	nbox = nboxInit;
	pbox = pboxInit;

#ifdef POLYSEGMENT
	x1 = pSeg->x1 + xorg;	
	y1 = pSeg->y1 + yorg;
	x2 = pSeg->x2 + xorg;	
	y2 = pSeg->y2 + yorg;
	pSeg++;
#else
	x1 = x2; 
	y1 = y2;
	++ppt;
	if (mode == CoordModePrevious) {
	  xorg = x1; 
	  yorg = y1;
	}
	x2 = ppt->x + xorg; 
	y2 = ppt->y + yorg;
#endif



	if((dmaj = x2 - x1) < 0) {
	  dmaj = -dmaj;
	  octant = XDECREASING;
	} else octant = 0;		   

	if((dmin = y2 - y1) < 0) {
	  dmin = -dmin;
	  octant |= YDECREASING;
	}	
	
	if(dmin >= dmaj){
	  tmp = dmin; dmin = dmaj; dmaj = tmp;
	  octant |= YMAJOR;
	}

	e = -dmaj - ((bias >> octant) & 1);
	len = dmaj;
	dmin <<= 1;
	dmaj <<= 1;


	while(nbox--) {
	  oc1 = oc2 = 0;
	  OUTCODES(oc1, x1, y1, pbox);
	  OUTCODES(oc2, x2, y2, pbox);
	  if (!(oc1 | oc2)) {   /* unclipped */
	    TGASubsequentDashedLine(infoRec->pScrn, x1, y1, x2, y2,
				    octant,
#ifdef POLYSEGMENT
				    (pGC->capStyle != CapNotLast) ? 0 :
#endif
				    OMIT_LAST, PatternOffset);
	    break;
	  } else if (oc1 & oc2) { /* completely clipped */
	    pbox++;
	  } else { /* partially clipped */
	    int new_x1 = x1, new_y1 = y1, new_x2 = x2, new_y2 = y2;
	    int clip1 = 0, clip2 = 0;
	    int err, adx, ady;
		    
	    if(octant & YMAJOR) {
	      ady = dmaj >> 1;
	      adx = dmin >> 1;
	    } else {
	      ady = dmin >> 1;
	      adx = dmaj >> 1;
	    }

	    if (miZeroClipLine(pbox->x1, pbox->y1, 
			       pbox->x2 - 1, pbox->y2 - 1,
			       &new_x1, &new_y1, &new_x2, &new_y2,
			       adx, ady, &clip1, &clip2,
			       octant, bias, oc1, oc2) == -1)
	      {
		pbox++;
		continue;
	      }

	    if (octant & YMAJOR)
	      len = abs(new_y2 - new_y1);
	    else
	      len = abs(new_x2 - new_x1);
#ifdef POLYSEGMENT
	    if (clip2 != 0 || pGC->capStyle != CapNotLast)
	      len++;
#else
	    len += (clip2 != 0);
#endif
	    if (len) {
	      int abserr, clipdx, clipdy;
	      /* unwind bresenham error term to first point */
	      if (clip1) {
		clipdx = abs(new_x1 - x1);
		clipdy = abs(new_y1 - y1);

		if (octant & YMAJOR)
		  err = e + clipdy*dmin - clipdx*dmaj;
		else
		  err = e + clipdx*dmin - clipdy*dmaj;
	      } else
		err = e;

#define range infoRec->DashedBresenhamLineErrorTermBits
	      abserr = abs(err);			    
	      while((abserr & range) || 
		    (dmaj & range) ||
		    (dmin & range)) {
		dmin >>= 1;
		dmaj >>= 1;
		abserr >>= 1;
		err /= 2;
	      }

	      if(octant & YMAJOR)
		offset = abs(new_y1 - y1);
	      else 
		offset = abs(new_x1 - x1);

	      offset += PatternOffset;
	      offset %= PatternLength;

	      TGASetupForClippedLine(infoRec->pScrn, x1, x2, y1, y2,
				     octant);
	      TGASubsequentClippedDashedLine(infoRec->pScrn, new_x1,
					     new_y1, len, err,
					     PatternOffset);
	    }
	    pbox++;
	  }
	} /* while (nbox--) */
#ifndef POLYSEGMENT
	len = abs(y2 - y1);
	tmp = abs(x2 - x1);
	PatternOffset += (len > tmp) ? len : tmp;
	PatternOffset %= PatternLength;
#endif
      } /* while (nline--) */

#ifndef POLYSEGMENT
    /* paint the last point if the end style isn't CapNotLast.
       (Assume that a projecting, butt, or round cap that is one
        pixel wide is the same as the single pixel of the endpoint.)
    */

    if ((pGC->capStyle != CapNotLast) &&
	((ppt->x + xorg != pptInit->x + pDrawable->x) ||
	 (ppt->y + yorg != pptInit->y + pDrawable->y) ||
	 (ppt == pptInit + 1)))
      {
	nbox = nboxInit;
	pbox = pboxInit;
	while (nbox--) {
	  if ((x2 >= pbox->x1) && (y2 >= pbox->y1) &&
	      (x2 <  pbox->x2) && (y2 <  pbox->y2))
	    {
	      TGASubsequentDashedLine(infoRec->pScrn, x2, y2, x2, y2, 0, 0,
				      PatternOffset);
	      break;
	    } else
		pbox++;
	}
    }
#endif

    TGASync(infoRec->pScrn);
    return;
}

    
