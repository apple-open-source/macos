/* $Xorg: medraw.c,v 1.4 2001/02/09 02:04:24 xorgcvs Exp $ */
/**** module medraw.c ****/
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
  
	medraw.c -- DDXIE prototype export drawable element
  
	Robert NC Shelley -- AGE Logic, Inc. April, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/export/medraw.c,v 3.6 2001/12/14 19:58:19 dawes Exp $ */

#define _XIEC_MEDRAW
#define _XIEC_EDRAW

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


/*
 *  routines referenced by other DDXIE modules
 */
extern int miAnalyzeEDraw(floDefPtr flo, peDefPtr ped);

/*
 *  routines used internal to this module
 */
static int CreateEDraw(floDefPtr flo, peDefPtr ped);
static int InitializeEDraw(floDefPtr flo, peDefPtr ped);
static int ActivateEDrawAlign(floDefPtr flo, peDefPtr ped, peTexPtr pet);
static int ActivateEDrawStrip(floDefPtr flo, peDefPtr ped, peTexPtr pet);
static int ResetEDraw(floDefPtr flo, peDefPtr ped);
static int DestroyEDraw(floDefPtr flo, peDefPtr ped);

static void adjustStride8to4(char *dst, char *src, CARD32 width);
static void adjustStride32to24(char *dst, char *src, CARD32 width);

extern Bool	DrawableAndGC();

/*
 * DDXIE ExportDrawable entry points
 */
static ddElemVecRec EDrawVec = {
  CreateEDraw,
  InitializeEDraw,
  (xieIntProc)NULL,
  (xieIntProc)NULL,
  ResetEDraw,
  DestroyEDraw
  };

typedef struct _medraw {
  xieVoidProc	 adjust;
  char		*buf;
} meDrawRec, *meDrawPtr;


/*------------------------------------------------------------------------
------------------- see if we can handle this element --------------------
------------------------------------------------------------------------*/
int miAnalyzeEDraw(floDefPtr flo, peDefPtr ped)
{
  /* for now just stash our entry point vector in the peDef */
  ped->ddVec = EDrawVec;

  return(TRUE);
}                               /* end miAnalyzeEDraw */

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateEDraw(floDefPtr flo, peDefPtr ped)
{
  /* attach an execution context to the photo element definition */
  return MakePETex(flo, ped, sizeof(meDrawRec), NO_SYNC, NO_SYNC);
}                               /* end CreateEDraw */

/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeEDraw(floDefPtr flo, peDefPtr ped)
{
  peTexPtr    pet = ped->peTex;
  meDrawPtr   ddx = (meDrawPtr) pet->private;
  formatPtr   sf  = &ped->inFloLst[SRCtag].format[0];
  formatPtr   df  = &ped->outFlo.format[0];
  Bool adj_stride = sf->stride != df->stride;
  Bool adj_pitch  = sf->pitch  != df->pitch;
  
  if(adj_stride || adj_pitch) {
    ped->ddVec.activate = ActivateEDrawAlign;

    if(adj_stride) {
      if(!(ddx->buf = (char*) XieMalloc(df->pitch>>3)))
	AllocError(flo,ped, return(FALSE));
      
      if(sf->stride == 32 && df->stride == 24)
	ddx->adjust  = adjustStride32to24;
      
      else if(sf->stride == 8 && df->stride == 4)
	ddx->adjust  = adjustStride8to4;
      
      /* add more adjustment routines as required */
    }
  } else {
    ped->ddVec.activate = ActivateEDrawStrip;
  }
  return(InitReceptors(flo,ped,NO_DATAMAP,1));
}                               /* end InitializeEDraw */


/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
static int ActivateEDrawAlign(floDefPtr flo, peDefPtr ped, peTexPtr pet)
{
  xieFloExportDrawable  *raw = (xieFloExportDrawable *) ped->elemRaw;
  eDrawDefPtr		 dix = (eDrawDefPtr) ped->elemPvt;
  meDrawPtr		 ddx = (meDrawPtr) pet->private;
  bandPtr		 bnd = &pet->receptor[SRCtag].band[0];
  char		  *dst, *src = (char*)GetCurrentSrc(flo,pet,bnd);
  CARD32	       width = bnd->format->width;

  if (!DrawableAndGC(flo,ped,raw->drawable,raw->gc,&dix->pDraw,&dix->pGC))
    return FALSE;
  
  do {
    if(ddx->adjust) {
      dst = AlterSrc(flo,pet,bnd->strip) ? src : ddx->buf;
      (*ddx->adjust)(dst, src, width);
    } else
      dst = src;

    (*dix->pGC->ops->PutImage)(dix->pDraw,		  /* drawable	   */
			       dix->pGC,		  /* gc		   */
			       dix->pDraw->depth,	  /* depth	   */
			       raw->dstX,		  /* drawable-x	   */
			       raw->dstY + bnd->current,  /* drawable-y	   */
			       width, 1,		  /* width, height */
			       bnd->strip->bitOff,	  /* padding? 	   */
			       ZPixmap,		  	  /* data format   */
			       dst			  /* data buffer   */
			       );
  } while ((src = (char*)GetNextSrc(flo,pet,bnd,KEEP)) != 0);
  
  /* make sure the scheduler knows how much src we used */
  FreeData(flo,pet,bnd,bnd->current);
  
  return(TRUE);
}                               /* end ActivateEDrawAlign */


/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
static int ActivateEDrawStrip(floDefPtr flo, peDefPtr ped, peTexPtr pet)
{
  xieFloExportDrawable  *raw = (xieFloExportDrawable *) ped->elemRaw;
  eDrawDefPtr		 pvt = (eDrawDefPtr) ped->elemPvt;
  bandPtr		 bnd = &pet->receptor[SRCtag].band[0];
  char			*src = (char*)GetCurrentSrc(flo,pet,bnd);
  
  if(src) {
    if (!DrawableAndGC(flo,ped,raw->drawable,raw->gc,&pvt->pDraw,&pvt->pGC))
      return FALSE;
    do    
      (*pvt->pGC->ops->PutImage)(pvt->pDraw,		  /* drawable	 */
				 pvt->pGC,		  /* gc		 */
				 pvt->pDraw->depth,	  /* depth	 */
				 raw->dstX,		  /* drawable-x	 */
				 raw->dstY+bnd->minLocal, /* drawable-y	 */
				 bnd->format->width,	  /* width	 */
				 bnd->strip->length,	  /* height	 */
				 bnd->strip->bitOff,	  /* padding? 	 */
				 ZPixmap,		  /* data format */
				 src			  /* data buffer */
				 );
    while ((src = (char*)GetSrc(flo,pet,bnd,bnd->maxLocal,KEEP)) != 0);
  }
  /* make sure the scheduler knows how much src we used */
  FreeData(flo,pet,bnd,bnd->current);

  return(TRUE);
}                               /* end ActivateEDrawStrip */


/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetEDraw(floDefPtr flo, peDefPtr ped)
{
  meDrawPtr ddx = (meDrawPtr) ped->peTex->private;

  if(ddx->buf) ddx->buf = (char*)XieFree(ddx->buf);
  ddx->adjust = (xieVoidProc)NULL;

  ResetReceptors(ped);
  
  return(TRUE);
}                               /* end ResetEDraw */


/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyEDraw(floDefPtr flo, peDefPtr ped)
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
}                               /* end DestroyEDraw */


/*------------------------------------------------------------------------
---------------------------- alignment routines --------------------------
------------------------------------------------------------------------*/
static void adjustStride32to24(char *dst, char *src, CARD32 width)
{
  register char *ip, *op;
  CARD32   i;
  
#if (BITMAP_BIT_ORDER == IMAGE_BYTE_ORDER)
  ip = src;
  op = dst;
  for(i = 0; i < width; ip += 2, ++i) {
    *op++ = *ip++; *op++ = *ip++; *op++ = *ip;
  }
#else
  /* we'll do the first 2 pixels by hand in case we're doing this in-place
   */
  char tmp = src[0];
  dst[0]   = src[2];
  dst[2]   = tmp;
  if(width > 1) {
    tmp    = src[4];
    dst[3] = src[6];
    dst[4] = src[5];
    dst[5] = tmp;
  }
  if(width > 2) {
      ip = src + 8;
      op = dst + 6;
      for(i = 2; i < width; ip += 4, ++i) {
	*op++ = ip[2]; *op++ = ip[1]; *op++ = *ip;
      }
    }
#endif
}

static void adjustStride8to4(char *dst, char *src, CARD32 width)
{
  register char *ip = src, *op = dst;
  CARD32   i;
  
  for(i = 0; i < width; ip += 2, ++i) {
#if (BITMAP_BIT_ORDER == LSBFirst)
    *op++ = *ip | *(ip+1)<<8;
#else
    *op++ = *ip<<8 | *(ip+1);
#endif
  }
}

/* end module medraw.c */
