/* $Xorg: midraw.c,v 1.4 2001/02/09 02:04:27 xorgcvs Exp $ */
/**** module midraw.c ****/
/******************************************************************************

Copyright 1993, 1994, 1998  The Open Group

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


				NOTICE
                              
This software is being provided by AGE Logic, Inc. under the
following license.  By obtaining, using and/or copying this software,
you agree that you have read, understood, and will comply with these
terms and conditions:

     Permission to use, copy, modify, distribute and sell this
     software and its documentation for any purpose and without
     fee or royalty and to grant others any or all rights granted
     herein is hereby granted, provided that you agree to comply
     with the following copyright notice and statements, including
     the disclaimer, and that the same appears on all copies and
     derivative works of the software and documentation you make.
     
     "Copyright 1993, 1994 by AGE Logic, Inc."
     
     THIS SOFTWARE IS PROVIDED "AS IS".  AGE LOGIC MAKES NO
     REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  By way of
     example, but not limitation, AGE LOGIC MAKE NO
     REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS
     FOR ANY PARTICULAR PURPOSE OR THAT THE SOFTWARE DOES NOT
     INFRINGE THIRD-PARTY PROPRIETARY RIGHTS.  AGE LOGIC 
     SHALL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE.  IN NO
     EVENT SHALL EITHER PARTY BE LIABLE FOR ANY INDIRECT,
     INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS
     OF PROFITS, REVENUE, DATA OR USE, INCURRED BY EITHER PARTY OR
     ANY THIRD PARTY, WHETHER IN AN ACTION IN CONTRACT OR TORT OR
     BASED ON A WARRANTY, EVEN IF AGE LOGIC LICENSEES
     HEREUNDER HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
     DAMAGES.
    
     The name of AGE Logic, Inc. may not be used in
     advertising or publicity pertaining to this software without
     specific, written prior permission from AGE Logic.

     Title to this software shall at all times remain with AGE
     Logic, Inc.
*****************************************************************************
  
	midraw.c -- DDXIE prototype import drawable element
  
	Larry Hare -- AGE Logic, Inc. June, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/import/midraw.c,v 3.6 2001/12/14 19:58:27 dawes Exp $ */

#define _XIEC_IDRAW
#define _XIEC_MIDRAW

/*
 *  Include files
 */
/*
 *  Core X Includes
 */
#include <X.h>
#include <Xproto.h>
/*
 *  XIE Includes
 */
#include <XIE.h>
#include <XIEproto.h>
/*
 *  more X server includes.
 */
#include <misc.h>
#include <dixstruct.h>
#include <scrnintstr.h>
#include <pixmapstr.h>
#include <regionstr.h>
#include <gcstruct.h>
#include <windowstr.h>
/*
 *  Server XIE Includes
 */
#include <error.h>
#include <macro.h>
#include <photomap.h>
#include <element.h>
#include <texstr.h>
#include <xiemd.h>
#include <memory.h>


DrawablePtr ValDrawable();


/*
 *  routines referenced by other DDXIE modules
 */
int	miAnalyzeIDraw();

/*
 *  routines used internal to this module
 */
static int CreateIDraw();
static int InitializeIDraw();
static int ActivateIDrawStrip();
static int ActivateIDrawAlign();
static int ActivateIDrawP();
static int ResetIDraw();
static int DestroyIDraw();

static void adjustStride4to8();
static void adjustStride24to32();

static Bool XIEGetImage();


/*
 * DDXIE ImportDrawable && ImportDrawablePlane entry points
 */
static ddElemVecRec miDrawVec = {
  CreateIDraw,
  InitializeIDraw,
  ActivateIDrawStrip,
  (xieIntProc)NULL,
  ResetIDraw,
  DestroyIDraw
  };

static ddElemVecRec miDrawPVec = {
  CreateIDraw,
  InitializeIDraw,
  ActivateIDrawP,
  (xieIntProc)NULL,
  ResetIDraw,
  DestroyIDraw
  };

typedef struct _midraw {
  RegionPtr	pExposed;
  RegionRec	 Exposed;
  xieVoidProc	 adjust;
} miDrawRec, *miDrawPtr;	

/*------------------------------------------------------------------------
------------------- see if we can handle this element --------------------
------------------------------------------------------------------------*/
int miAnalyzeIDraw(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* for now just stash our entry point vector in the peDef */
  ped->ddVec = miDrawVec;
  return(TRUE);
}                               /* end miAnalyzeIDraw */

int miAnalyzeIDrawP(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* for now just stash our entry point vector in the peDef */
  ped->ddVec = miDrawPVec;
  return(TRUE);
}                               /* end miAnalyzeIDrawP */


/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateIDraw(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* attach an execution context to the photo element definition */
  return MakePETex(flo, ped, sizeof(miDrawRec), NO_SYNC, NO_SYNC);
}                               /* end CreateIDraw */


/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeIDraw(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  iDrawDefPtr		dix = (iDrawDefPtr) ped->elemPvt;
  miDrawPtr		ddx = (miDrawPtr) ped->peTex->private;
  formatPtr		sf  = &ped->inFloLst[SRCtag].format[0];
  formatPtr		df  = &ped->outFlo.format[0];
  BOOL notify;

  if(((xieFloImportDrawable*)ped->elemRaw)->elemType==xieElemImportDrawable) {
    Bool adj_stride = sf->stride != df->stride;
    Bool adj_pitch  = sf->pitch  != df->pitch;

    if(adj_stride || adj_pitch) {
      ped->ddVec.activate = ActivateIDrawAlign;

      if(adj_stride) {
	if(sf->stride == 24 && df->stride == 32)
	  ddx->adjust  = adjustStride24to32;
      
	else if(sf->stride == 4 && df->stride == 8)
	  ddx->adjust  = adjustStride4to8;
      
	/* add more adjustment routines as required */
      }
    } else {
      ped->ddVec.activate = ActivateIDrawStrip;
    }
    notify = ((xieFloImportDrawable*)ped->elemRaw)->notify;
  } else
    notify = ((xieFloImportDrawablePlane*)ped->elemRaw)->notify;

  if(notify && dix->pDraw->type != DRAWABLE_PIXMAP) {
    ddx->pExposed = &ddx->Exposed;
    REGION_INIT(dix->pDraw->pScreen, ddx->pExposed, NullBox, 0);
  }
  /* note: ImportResource elements don't use their receptors */
  return InitEmitter(flo,ped,NO_DATAMAP,NO_INPLACE);
}                               /* end InitializeIDraw */


/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
static int ActivateIDrawAlign(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  xieFloImportDrawable	 *raw = (xieFloImportDrawable *) ped->elemRaw;
  miDrawPtr		  ddx = (miDrawPtr) pet->private;
  bandPtr		  bnd = &pet->emitter[0];
  DrawablePtr	        pDraw = ValDrawable(flo,ped,raw->drawable);
  char			 *dst = (char*)GetCurrentDst(flo,pet,bnd);
  CARD16		width = bnd->format->width;

  if(!pDraw || !dst) return FALSE;

  do {
    if(!XIEGetImage(pDraw,				/* drawable	 */
		    (CARD16) raw->srcX,			/* drawable-x	 */
		    (CARD16)(raw->srcY+bnd->current),	/* drawable-y	 */
		    (CARD16) width, (CARD16)1,		/* width, height */
		    (CARD32)ZPixmap,			/* data format   */
		    (CARD32)~0,				/* plane mask	 */
		    dst,				/* data buffer   */
		    raw->fill,				/* augment	 */ 
		    ddx->pExposed			/* accumulate    */
		    ))
      DrawableError(flo,ped,raw->drawable, return(FALSE));

    if(ddx->adjust)
      (*ddx->adjust)(dst, width);

  } while(dst = (char*)GetNextDst(flo,pet,bnd,FLUSH));

  return TRUE;
}                               /* end ActivateIDrawAlign */

/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
static int ActivateIDrawStrip(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  xieFloImportDrawable	*raw = (xieFloImportDrawable *) ped->elemRaw;
  miDrawPtr		 ddx = (miDrawPtr) pet->private;
  bandPtr		 bnd = &pet->emitter[0];
  char			*dst;
  DrawablePtr		 pDraw;

  if (!(pDraw = ValDrawable(flo,ped,raw->drawable)))
    return FALSE;

  if (!(dst = (char*)GetCurrentDst(flo,pet,bnd)))
    return FALSE;

  if(!XIEGetImage( pDraw,				/* drawable	*/
		  (CARD16) raw->srcX,			/* drawable-x	*/
		  (CARD16)(raw->srcY+bnd->minLocal),	/* drawable-y	*/
		  (CARD16)(bnd->format->width),		/* width	*/
		  (CARD16)(bnd->strip->length),		/* height	*/
		  (CARD32)ZPixmap,			/* data format  */
		  (CARD32)~0,				/* plane mask	*/
		  dst,					/* data buffer  */
		  raw->fill,				/* augment	*/ 
		  ddx->pExposed				/* accumulate   */
		  ))
    DrawableError(flo,ped,raw->drawable, return(FALSE));

  PutData(flo,pet,bnd,bnd->maxLocal);

  return TRUE;
}                               /* end ActivateIDrawStrip */

static int ActivateIDrawP(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  xieFloImportDrawablePlane *raw = (xieFloImportDrawablePlane *) ped->elemRaw;
  miDrawPtr		ddx = (miDrawPtr) pet->private;
  bandPtr		bnd = &pet->emitter[0];
  char			*dst;
  DrawablePtr		pDraw;

  if (!(pDraw = ValDrawable(flo,ped,raw->drawable)))
    return FALSE;

  if(!(dst = (char*)GetCurrentDst(flo,pet,bnd)))
    return FALSE;

  if(!XIEGetImage( pDraw,				/* drawable	*/
		  (CARD16)(raw->srcX),			/* drawable-x   */
		  (CARD16)(raw->srcY+bnd->minLocal),	/* drawable-y   */
		  (CARD16)(bnd->format->width),		/* width	*/
		  (CARD16)(bnd->strip->length),		/* height	*/
		  (CARD32)XYPixmap,			/* data format  */
		  raw->bitPlane,			/* plane mask   */
		  dst,					/* data buffer  */
		  raw->fill,				/* augment	*/ 
		  ddx->pExposed				/* accumulate   */
		  ))
    DrawableError(flo,ped,raw->drawable, return(FALSE));

#if (BITMAP_BIT_ORDER != IMAGE_BYTE_ORDER)
  {
    int nb = bnd->pitch * bnd->strip->length;
    while (nb--) *dst++ = (char)_ByteReverseTable[*dst];
  }
#endif

  PutData(flo,pet,bnd,bnd->maxLocal);

  return TRUE;
}                               /* end ActivateIDrawP */


/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static void FlushExposeEvents(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  xieFloImportDrawablePlane *raw = (xieFloImportDrawablePlane *) ped->elemRaw;
  miDrawPtr		     pvt = (miDrawPtr) ped->peTex->private;

  if (pvt->pExposed) {
    RegionPtr      rp = pvt->pExposed;
    DrawablePtr pDraw = ValDrawable(flo,ped,raw->drawable);
    int      numrects = REGION_NUM_RECTS(rp);

    if (pDraw) {
	if (numrects > 0 && !ferrCode(flo) && !flo->flags.aborted) {
	    BoxPtr rects = REGION_RECTS(rp);

	    /* if numrects is large, send 1 event for extents ?? */
	    for ( ; numrects > 0; numrects--, rects++) {
		SendImportObscuredEvent(flo,ped,raw->drawable,
					rects->x1,
					rects->y1,
					rects->x2 - rects->x1,
					rects->y2 - rects->y1);
	    }
	}
    	REGION_UNINIT(pDraw->pScreen, rp);
    } /* else Memory Leak */
    pvt->pExposed = NullRegion;
  }
}                               /* end FlushExposeEvents */


static int ResetIDraw(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  FlushExposeEvents(flo,ped); 
  ResetEmitter(ped);
  
  return(TRUE);
}                               /* end ResetIDraw */

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyIDraw(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* get rid of the peTex structure  */
  ped->peTex = (peTexPtr) XieFree(ped->peTex);

  /* zap this element's entry point vector */
  ped->ddVec.create     = (xieIntProc) NULL;
  ped->ddVec.initialize = (xieIntProc) NULL;
  ped->ddVec.activate   = (xieIntProc) NULL;
  ped->ddVec.reset      = (xieIntProc) NULL;
  ped->ddVec.destroy    = (xieIntProc) NULL;

  return(TRUE);
}                               /* end DestroyIDraw */

/*------------------------------------------------------------------------
--- similar to GetGCAndDrawableAndValidate (from extension.h)          ---
--- made callable because the macro version returned standard X errors ---
------------------------------------------------------------------------*/
DrawablePtr ValDrawable(flo,ped,draw_id)
     floDefPtr    flo;
     peDefPtr     ped;
     Drawable	  draw_id;
{
  register ClientPtr	client = flo->runClient;
  register DrawablePtr	draw;
  
  if (client->clientGone) return(NULL);

  if (client->lastDrawableID != draw_id) {
    draw = (DrawablePtr)LookupIDByClass(draw_id, RC_DRAWABLE);
    if (draw->type  == UNDRAWABLE_WINDOW)
	MatchError(flo,ped, return(NULL));
      
    client->lastDrawable   = draw;
    client->lastDrawableID = draw_id;
  } else {
    draw = client->lastDrawable;
  }
  if(!draw) {
    client->errorValue = draw_id;
    DrawableError(flo,ped,draw_id, return(NULL));
  }
  return draw;
}                               /* end ValDrawable */


/*
**  In order to support 'fill' and 'expose events' we must layer some
**  code on top of the normal screen GetImage procedure.  Use the defined
**  backing store interface that aid's in copy expose processing, to try
**  to knowingly recover backing store.  Keep track of remaining regions
**  which were not visible or in backing store.  Always fill such leftover
**  regions, and queue them up for later expose event processing.
*/

static Bool
XIEGetImage (pDrawable, sx, sy, w, h, format, pmask, pdst, fill, Exposed)
    DrawablePtr	pDrawable;
    CARD16	 sx, sy, w, h;
    CARD32	 format;
    CARD32	 pmask;
    char	*pdst;
    CARD32	 fill;
    RegionPtr	 Exposed;
{
    ScreenPtr		    pScreen = pDrawable->pScreen;
    BoxRec		    bounds;
    unsigned char	    depth;

    if(pDrawable->type != DRAWABLE_PIXMAP && (sx + w > pDrawable->width ||
					      sy + h > pDrawable->height))
      return(FALSE);

    if(pDrawable->type != DRAWABLE_PIXMAP &&
	((WindowPtr)pDrawable)->visibility != VisibilityUnobscured) {
	PixmapPtr	pPixmap;
	GCPtr		pGC;
	WindowPtr	pWin, pSrcWin;
	RegionRec	Remaining;
	BoxPtr		pBox;
	int		i;
	int		x, y;
	xRectangle	*pRect;
	int		numRects;

	pWin = (WindowPtr) pDrawable;
	pPixmap = 0;
	depth = pDrawable->depth;
	bounds.x1 = sx + pDrawable->x;
	bounds.y1 = sy + pDrawable->y;
	bounds.x2 = bounds.x1 + w;
	bounds.y2 = bounds.y1 + h;
	REGION_INIT(pScreen, &Remaining, &bounds, 0);
	if (!(pPixmap = (*pScreen->CreatePixmap) (pScreen, w, h, depth)))
	    goto punt;
	if(!(pGC = GetScratchGC (depth, pScreen))) {
	    (*pScreen->DestroyPixmap) (pPixmap);
	    goto punt;
	}
	if (pWin->viewable && 
	    RECT_IN_REGION(pScreen, &Remaining,
		   REGION_EXTENTS(pScreen, &pWin->borderSize)) != rgnOUT) {
	    XID	subWindowMode = IncludeInferiors;
	    ChangeGC (pGC, GCSubwindowMode, &subWindowMode);
	    ValidateGC ((DrawablePtr)pPixmap, pGC);
	    pSrcWin = (WindowPtr) pDrawable;
	    x = sx;
	    y = sy;
	    if (pSrcWin->parent) {
		x += pSrcWin->origin.x;
		y += pSrcWin->origin.y;
		pSrcWin = pSrcWin->parent;
	    }
	    (*pGC->ops->CopyArea) ((DrawablePtr)pSrcWin,
				   (DrawablePtr)pPixmap, pGC,
				   x, y, w, h,
				   0, 0);
	    REGION_SUBTRACT(pScreen, &Remaining, &Remaining,
				  &((WindowPtr) pDrawable)->borderClip);
	    if (REGION_NUM_RECTS(&Remaining) == 0) goto done;
	}
        REGION_TRANSLATE(pScreen, &Remaining,
	    -pWin->drawable.x, -pWin->drawable.y);

	/* Copy in Backstore now */
	if (pWin->backStorage) {
		(*pWin->drawable.pScreen->ExposeCopy) (pWin,
						       (DrawablePtr)pPixmap,
						       pGC, &Remaining,
						       sx, sy,
						       0, 0,
						       0);
					
	}

	/* Fill in Remaining Area now */
	ChangeGC (pGC, GCForeground, &fill);
	ValidateGC ((DrawablePtr)pPixmap, pGC);
	numRects = REGION_NUM_RECTS(&Remaining);
	pBox = REGION_RECTS(&Remaining);
	pRect = (xRectangle *)ALLOCATE_LOCAL(numRects * sizeof(xRectangle));
	if (pRect) {
	    for (i = numRects; --i >= 0; pBox++, pRect++) {
		pRect->x = pBox->x1 - sx;
		pRect->y = pBox->y1 - sy;
		pRect->width = pBox->x2 - pBox->x1;
		pRect->height = pBox->y2 - pBox->y1;
	    }
	    pRect -= numRects;
	    (*pGC->ops->PolyFillRect) ((DrawablePtr)pPixmap, pGC,
				       numRects, pRect);
	    DEALLOCATE_LOCAL (pRect);
	}

	/* Accumulate Exposures if requested */
	if (REGION_NUM_RECTS(&Remaining) > 0) {
	    if (Exposed) {
		REGION_UNION(pScreen, Exposed, Exposed, &Remaining);
	    }
	}

done:
	REGION_UNINIT(pScreen, &Remaining);
	(*pScreen->GetImage) ((DrawablePtr) pPixmap, 0, 0, w, h,
			      format, pmask, pdst);
	(*pScreen->DestroyPixmap) (pPixmap);
	FreeScratchGC (pGC);
    }
    else
    {
punt:	;
	(*pScreen->GetImage) (pDrawable, sx, sy, w, h,
			      format, pmask, pdst);
    }
  return(TRUE);
}

static void adjustStride24to32(buf,width)
     char      *buf;
     CARD32	width;
{
  register BytePixel *ip = (BytePixel*)(&buf[3 * width]);
  register QuadPixel *op = (QuadPixel*)(&buf[4 * width]);
  int   i;

  for(i = width; i; --i)
#if (IMAGE_BYTE_ORDER == LSBFirst && IMAGE_BYTE_ORDER == BITMAP_BIT_ORDER ||     IMAGE_BYTE_ORDER == MSBFirst && IMAGE_BYTE_ORDER != BITMAP_BIT_ORDER)
  { ip -= 3; *(--op) = (int)ip[2]<<16 | (int)ip[1]<<8 | (int)ip[0]; }
#else
  { ip -= 3; *(--op) = (int)ip[0]<<16 | (int)ip[1]<<8 | (int)ip[2]; }
#endif
}

static void adjustStride4to8(buf,width)
     char      *buf;
     CARD32	width;
{
  register BytePixel *ip = (BytePixel*)(&buf[1 * width]);
  register BytePixel *op = (BytePixel*)(&buf[2 * width]);
  int   i;

  for(i = width; i; --i)
#if (IMAGE_BYTE_ORDER == LSBFirst && IMAGE_BYTE_ORDER == BITMAP_BIT_ORDER ||     IMAGE_BYTE_ORDER == MSBFirst && IMAGE_BYTE_ORDER != BITMAP_BIT_ORDER)
  { *(--op) = *(--ip) & (BytePixel)0x0f; *(--op) = *ip >> 4; }
#else
  { *(--op) = *(--ip) >> 4; *(--op) = *ip & (BytePixel)0x0f; }
#endif
}


/* end module midraw.c */
