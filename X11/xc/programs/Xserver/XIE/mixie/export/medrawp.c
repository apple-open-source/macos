/* $Xorg: medrawp.c,v 1.4 2001/02/09 02:04:24 xorgcvs Exp $ */
/**** module medrawp.c ****/
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
  
	medrawp.c -- DDXIE prototype export drawable plane element
  
	Robert NC Shelley && Larry Hare -- AGE Logic, Inc. June, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/export/medrawp.c,v 3.6 2001/12/14 19:58:20 dawes Exp $ */

#define _XIEC_MEDRAWP
#define _XIEC_EDRAWP

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
#include <gcstruct.h>
/*
 *  Server XIE Includes
 */
#include <error.h>
#include <macro.h>
#include <element.h>
#include <texstr.h>
#include <xiemd.h>
#include <memory.h>

extern Bool	DrawableAndGC();

/*
 *  routines referenced by other DDXIE modules
 */
extern int miAnalyzeEDrawP(floDefPtr flo, peDefPtr ped);

/*
 *  routines used internal to this module
 */
static int CreateEDrawP(floDefPtr flo, peDefPtr ped);
static int InitializeEDrawP(floDefPtr flo, peDefPtr ped);
static int ActivateEDrawP(floDefPtr flo, peDefPtr ped, peTexPtr pet);
static int ActivateEDrawPTrans(floDefPtr flo, peDefPtr ped, peTexPtr pet);
static int ResetEDrawP(floDefPtr flo, peDefPtr ped);
static int DestroyEDrawP(floDefPtr flo, peDefPtr ped);

/*
 * DDXIE ExportDrawable entry points
 */
static ddElemVecRec EDrawPVec = {
  CreateEDrawP,
  InitializeEDrawP,
  ActivateEDrawP,
  (xieIntProc)NULL,
  ResetEDrawP,
  DestroyEDrawP
  };

typedef struct _medrawp {
  BytePixel *buf;
} meDrawPRec, *meDrawPPtr;


/*------------------------------------------------------------------------
------------------- see if we can handle this element --------------------
------------------------------------------------------------------------*/
int miAnalyzeEDrawP(floDefPtr flo, peDefPtr ped)
{
  /* for now just stash our entry point vector in the peDef */
  ped->ddVec = EDrawPVec;

  return(TRUE);
}                               /* end miAnalyzeEDrawP */

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateEDrawP(floDefPtr flo, peDefPtr ped)
{
  /* attach an execution context to the photo element definition */
  return MakePETex(flo, ped, sizeof(meDrawPRec), NO_SYNC, NO_SYNC);
}                               /* end CreateEDrawP */

/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeEDrawP(floDefPtr flo, peDefPtr ped)
{
  xieFloExportDrawablePlane *raw = (xieFloExportDrawablePlane *) ped->elemRaw;
  eDrawPDefPtr	dix = (eDrawPDefPtr) ped->elemPvt;

  if (!DrawableAndGC(flo,ped,raw->drawable,raw->gc,&dix->pDraw,&dix->pGC))
    return FALSE;

  if (dix->pGC->fillStyle == FillStippled)
    	ped->ddVec.activate = ActivateEDrawPTrans;
  else		/* normal case: FillSolid || FillTiled || FillOpaqueStippled */
    	ped->ddVec.activate = ActivateEDrawP;

#if (BITMAP_BIT_ORDER != IMAGE_BYTE_ORDER)
  {
  meDrawPPtr ddx = (meDrawPPtr) ped->peTex->private;
  if(!(ddx->buf = ((BytePixel*)XieMalloc(max(ped->outFlo.format[0].pitch+7>>3,
					     flo->floTex->stripSize)))))
    AllocError(flo,ped, return(FALSE));
  }
#endif

  return InitReceptors(flo,ped,NO_DATAMAP,1);
}                               /* end InitializeEDrawP */


/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
static int ActivateEDrawP(floDefPtr flo, peDefPtr ped, peTexPtr pet)
{
  xieFloExportDrawablePlane *raw = (xieFloExportDrawablePlane *) ped->elemRaw;
  eDrawPDefPtr	 dix = (eDrawPDefPtr) ped->elemPvt;
  bandPtr	 bnd = &pet->receptor[SRCtag].band[0];
  BytePixel	*src = (BytePixel*)GetCurrentSrc(flo,pet,bnd);
  CARD32    pixtype, depth;

  if(src) {
    if(!DrawableAndGC(flo,ped,raw->drawable,raw->gc,&dix->pDraw,&dix->pGC))
      return FALSE;
    pixtype = (dix->pDraw->type == DRAWABLE_PIXMAP) ? XYPixmap : XYBitmap;
    depth   = (pixtype == XYBitmap) ? 1 : dix->pDraw->depth;

    do {
#if (BITMAP_BIT_ORDER != IMAGE_BYTE_ORDER)
      {
	meDrawPPtr ddx = (meDrawPPtr) pet->private;
	BytePixel *op, *dst;
	int   nb = bnd->pitch * bnd->strip->length;
	dst = op = AlterSrc(flo,pet,bnd->strip) ? src : ddx->buf;
	
	while (nb--)
	  *op++ = _ByteReverseTable[*src++];
	src = dst;
      }
#endif
      (*dix->pGC->ops->PutImage)(dix->pDraw,		  /* drawable	 */
				 dix->pGC,		  /* gc		 */
				 depth,			  /* depth	 */
				 raw->dstX,		  /* drawable-x	 */
				 raw->dstY+bnd->minLocal, /* drawable-y	 */
				 bnd->format->width,	  /* width	 */
				 bnd->strip->length,	  /* height	 */
				 bnd->strip->bitOff,	  /* padding? 	 */
				 pixtype,		  /* data format */
				 (char*)src		  /* data buffer */
				 );
    }
    while ((src = (BytePixel*)GetSrc(flo,pet,bnd,bnd->maxLocal,KEEP)) != 0)
	    ;

    /* make sure the scheduler knows how much src we used */
    FreeData(flo,pet,bnd,bnd->current);
  }
  return(TRUE);
}                               /* end ActivateEDrawP */

static int ActivateEDrawPTrans(floDefPtr flo, peDefPtr ped, peTexPtr pet)
{
  xieFloExportDrawablePlane *raw = (xieFloExportDrawablePlane *) ped->elemRaw;
  eDrawPDefPtr	dix = (eDrawPDefPtr) ped->elemPvt;
  bandPtr	bnd = &pet->receptor[SRCtag].band[0];
  DrawablePtr	draw;
  GCPtr		gc, scratch;
  PixmapPtr	bitmap;
  BytePixel	*src;
  int		oldstyle, newstyle = FillSolid;
  XID		gcvals[3];
  
  src = (BytePixel*)GetSrc(flo,pet,bnd,bnd->minGlobal,FALSE);
  if(src) {
    if (!DrawableAndGC(flo,ped,raw->drawable,raw->gc,&dix->pDraw,&dix->pGC))
      return FALSE;

    draw = dix->pDraw;
    gc   = dix->pGC;

    /*
    ** We use PushPixels with a solid fill to move the one bits onto the
    ** screen. Alternatives include:
    **	a) treat the bitmap as a stipple, set it up in a GC, and do a
    **     simple FillRect with FillStippled.  This has all the same
    **	   problems, plus some more.  Notably its more difficult to
    **	   use ChangeGC to transiently save/restore stipples, and to
    **	   compute the stipple offsets.
    **  b) another alternative is to change the incoming bitonal 
    **	   image to run length and do a FillSpans.  This also requires
    **	   either a scratch GC or transiently using the incoming GC.
    **	   It saves the extra bitmap creation and PutImage, but might
    ** 	   become grotesque when a dithered image comes rolling thru.
    */

    /*
    ** Core X does not seem to provide an official interface to create
    ** a pixmap header, or even to replace the data pointer.  We can't
    ** simply memcpy our data to it either, it might be on a separate
    ** cpu/memory system, or even upside down.  So we just use PutImage
    ** to prepare our data. Sigh.  On most cpu's, just explicitly
    ** create a pixmap header yourself and call PushPixels directly.
    ** Another optimization would be to use more lines at once. One 
    ** model for all this is miglblt.c
    **
    ** Since we expect people may want to redo this routine, we have
    ** left the pixmap and GC games local to this function rather than
    ** spreading stuff out into private structures and initialize
    ** and destroy routines.  For instance, the bitmap and scratchGC
    ** could be allocated once for the entire flow.
    **
    ** For now we do 64 lines of bits at a time.  We might come up
    ** with a better estimate based on actual scanline length. We 
    ** need a function to call though to make this unkludgey. Using
    ** NLINES as 64 may be bad if we get a very wide image.
    **
    ** The current performance seems very bad, even doing a large 
    ** number of lines at a time.  Haven't had time to investigate this.
    */

#define NLINES 64

    if (!(scratch =  GetScratchGC(1, draw->pScreen)))
	AllocError(flo,ped,return(FALSE));


    if (!(bitmap = (*draw->pScreen->CreatePixmap) (draw->pScreen,
			bnd->format->width, NLINES, 1))) {
	FreeScratchGC(scratch);
	AllocError(flo,ped,return(FALSE));
    }
    gcvals[0] = 1;
    gcvals[1] = 0;
    ChangeGC(scratch, GCForeground|GCBackground, gcvals);

    oldstyle = gc->fillStyle; 
    ChangeGC(gc, GCFillStyle, (XID *)&newstyle);

    do {
	int iy, ny;
#if (BITMAP_BIT_ORDER != IMAGE_BYTE_ORDER)
	{
	  meDrawPPtr ddx = (meDrawPPtr) pet->private;
	  BytePixel *op, *dst;
	  int   nb = bnd->pitch * bnd->strip->length;
	  dst = op = AlterSrc(flo,pet,bnd->strip) ? src : ddx->buf;
	
	  while (nb--)
	    *op++ = _ByteReverseTable[*src++];
	  src = dst;
	}
#endif
	for (iy = 0 ; iy < bnd->strip->length; iy += ny) {
	    ny = bnd->strip->length - iy;
	    if (ny > NLINES)
		ny = NLINES;
	    if ((scratch->serialNumber) != (bitmap->drawable.serialNumber))
		ValidateGC((DrawablePtr)bitmap, scratch);
	    (*scratch->ops->PutImage) (
		(DrawablePtr)bitmap,		/* drawable bitmap */
		scratch,			/* gc		*/
		1,				/* depth	*/
		0,				/* drawable-x	*/
		0,				/* drawable-y	*/
		bnd->format->width,		/* width	*/
		ny,				/* height	*/
		bnd->strip->bitOff,		/* padding? 	*/
		XYPixmap,			/* data format	*/
		(char*)src			/* data buffer	*/
		);
	    if ((gc->serialNumber) != (draw->serialNumber))
		ValidateGC(draw, gc);
	    (*gc->ops->PushPixels) (
		gc,					/* gc		*/
		bitmap,					/* bitmap	*/
		draw,					/* drawable	*/
		bnd->format->width,			/* width	*/
		ny,					/* height	*/
		raw->dstX+(gc->miTranslate?draw->x:0),	/* dst X/Y */
		raw->dstY+(gc->miTranslate?draw->y:0)+bnd->current+iy
		);
	    src += bnd->pitch * ny; /* gack */
	}
    } while ((src = (BytePixel*)GetSrc(flo,pet,bnd,bnd->maxLocal,FALSE)) != 0);

    /* make sure the scheduler knows how much src we used */
    FreeData(flo,pet,bnd,bnd->current);

    ChangeGC(gc, GCFillStyle, (XID *)&oldstyle);
    ValidateGC(draw, gc);

    (*draw->pScreen->DestroyPixmap) (bitmap);
    FreeScratchGC(scratch);
  }

  return(TRUE);
}                               /* end ActivateEDrawPTrans */


/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetEDrawP(floDefPtr flo, peDefPtr ped)
{
  meDrawPPtr ddx = (meDrawPPtr) ped->peTex->private;

  if(ddx->buf) ddx->buf = (BytePixel*)XieFree(ddx->buf);

  ResetReceptors(ped);
  
  return(TRUE);
}                               /* end ResetEDrawP */


/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyEDrawP(floDefPtr flo, peDefPtr ped)
{
  /* get rid of the peTex structure  */
  if(ped->peTex)
    ped->peTex = (peTexPtr) XieFree(ped->peTex);

  /* zap this element's entry point vector */
  ped->ddVec.create     = (xieIntProc) NULL;
  ped->ddVec.initialize = (xieIntProc) NULL;
  ped->ddVec.activate   = (xieIntProc) NULL;
  ped->ddVec.reset      = (xieIntProc) NULL;
  ped->ddVec.destroy    = (xieIntProc) NULL;

  return(TRUE);
}                               /* end DestroyEDrawP */

/* end module medrawp.c */
