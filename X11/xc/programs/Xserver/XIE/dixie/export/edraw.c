/* $Xorg: edraw.c,v 1.4 2001/02/09 02:04:20 xorgcvs Exp $ */
/**** module edraw.c ****/
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
  
	edraw.c -- DIXIE routines for managing the ExportDrawable element
  
	Robert NC Shelley -- AGE Logic, Inc. April 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/export/edraw.c,v 3.5 2001/12/14 19:57:58 dawes Exp $ */

#define _XIEC_EDRAW

/*
 *  Include files
 */
  /*
   *  Core X Includes
   */
#define NEED_EVENTS
#include <X.h>
#include <Xproto.h>
  /*
   *  XIE Includes
   */
#include <dixie_e.h>
#include <XIEproto.h>
  /*
   *  more X server includes.
   */
#include <pixmapstr.h>
#include <gcstruct.h>
#include <scrnintstr.h>
  /*
   *  Server XIE Includes
   */
#include <corex.h>
#include <error.h>
#include <macro.h>
#include <element.h>

/*
 *  routines internal to this module
 */
static Bool PrepEDraw(floDefPtr flo, peDefPtr ped);

/*
 * dixie entry points
 */
static diElemVecRec eDrawVec = {
    PrepEDraw		/* prepare for analysis and execution	*/
    };


/*------------------------------------------------------------------------
----------------- routine: make an ExportDrawable element ----------------
------------------------------------------------------------------------*/
peDefPtr MakeEDraw(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
  peDefPtr ped;
  inFloPtr inflo;
  ELEMENT(xieFloExportDrawable);
  ELEMENT_SIZE_MATCH(xieFloExportDrawable);
  ELEMENT_NEEDS_1_INPUT(src);
  
  if(!(ped = MakePEDef(1, (CARD32)stuff->elemLength<<2, sizeof(eDrawDefRec)))) 
    FloAllocError(flo,tag,xieElemExportDrawable, return(NULL));
  
  ped->diVec	    = &eDrawVec;
  ped->phototag     = tag;
  ped->flags.export = TRUE;
  raw = (xieFloExportDrawable *)ped->elemRaw;
  /*
   * copy the client element parameters (swap if necessary)
   */
  if( flo->reqClient->swapped ) {
    raw->elemType   = stuff->elemType;
    raw->elemLength = stuff->elemLength;
    cpswaps(stuff->src, raw->src);
    cpswaps(stuff->dstX, raw->dstX);
    cpswaps(stuff->dstY, raw->dstY);
    cpswapl(stuff->drawable, raw->drawable);
    cpswapl(stuff->gc, raw->gc);
  }
  else
    memcpy((char *)raw, (char *)stuff, sizeof(xieFloExportDrawable));
  /*
   * assign phototags to inFlos
   */
  inflo = ped->inFloLst;
  inflo[SRCtag].srcTag = raw->src;
  
  return(ped);
}                               /* end MakeEDraw */


/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/
static Bool PrepEDraw(floDefPtr flo, peDefPtr ped)
{
  xieFloExportDrawable *raw = (xieFloExportDrawable *) ped->elemRaw;
  eDrawDefPtr pvt = (eDrawDefPtr) ped->elemPvt;
  inFloPtr    inf = &ped->inFloLst[SRCtag];
  outFloPtr   src = &inf->srcDef->outFlo; 
  outFloPtr   dst = &ped->outFlo; 
  formatPtr   df  = &dst->format[0];
  CARD32  f, right_padm1;
  /*
   * check out drawable and gc
   */
  if(!DrawableAndGC(flo,ped,raw->drawable,raw->gc,&(pvt->pDraw),&(pvt->pGC)))
    return(FALSE);
  /*
   * check for: constrained, single-band, and levels matching drawable depth
   */
  if(IsntConstrained(src->format[0].class) ||
     src->bands != 1  ||  pvt->pDraw->depth != src->format[0].depth)
    MatchError(flo,ped, return(FALSE));
  /*
   * grab a copy of the input attributes and propagate them to our output
   */
  dst->bands = inf->bands = src->bands;
  df[0]  = inf->format[0] = src->format[0];

  /* search for the stride and pitch requirements that match our depth
   */
  for(f = 0; (f < screenInfo.numPixmapFormats &&
	      df[0].depth != screenInfo.formats[f].depth); ++f);
  if(f == screenInfo.numPixmapFormats)
    DrawableError(flo,ped,raw->drawable, return(FALSE));

  right_padm1  = screenInfo.formats[f].scanlinePad - 1;
  df[0].stride = screenInfo.formats[f].bitsPerPixel;
  df[0].pitch  = (df[0].width * df[0].stride + right_padm1) & ~right_padm1;

  return(TRUE);
}                               /* end PrepEDraw */

/*------------------------------------------------------------------------
--- callable version of GetGCAndDrawableAndValidate (from extension.h) ---
--- made callable because the macro version returned standard X errors ---
------------------------------------------------------------------------*/
Bool DrawableAndGC(
     floDefPtr    flo,
     peDefPtr     ped,
     Drawable	  draw_id,
     GContext	  gc_id,
     DrawablePtr  *draw_ret,
     GCPtr        *gc_ret)
{
  register ClientPtr	client = flo->runClient;
  register DrawablePtr	draw;
  register GCPtr	gc;
  
  if(client->clientGone) AccessError(flo,ped, return(FALSE));

  if((client->lastDrawableID != draw_id) ||
     (client->lastGCID != gc_id)) {
    if(client->lastDrawableID != draw_id)
      draw = (DrawablePtr)LookupIDByClass(draw_id, RC_DRAWABLE);
    else
      draw = client->lastDrawable;
    if(client->lastGCID != gc_id)
      gc = (GCPtr)LookupIDByType(gc_id, RT_GC);
    else
      gc = client->lastGC;
    if(draw && gc) {
      if((draw->type  == UNDRAWABLE_WINDOW) ||
	 (gc->depth   != draw->depth) ||
	 (gc->pScreen != draw->pScreen))
	MatchError(flo,ped, return(FALSE));
      
      client->lastDrawable = draw;
      client->lastDrawableID = draw_id;
      client->lastGC = gc;
      client->lastGCID = gc_id;
    }
  } else {
    gc = client->lastGC;
    draw = client->lastDrawable;
  }
  if(!draw) {
    client->errorValue = draw_id;
    DrawableError(flo,ped,draw_id, return(FALSE));
  }
  if(!gc) {
    client->errorValue = gc_id;
    GCError(flo,ped,gc_id, return(FALSE));
  }
  if(gc->serialNumber != draw->serialNumber)
    ValidateGC(draw, gc);

  *draw_ret = draw;
  *gc_ret = gc;
  
  return(TRUE);
}                               /* end DrawableAndGC */

/* end module edraw.c */
