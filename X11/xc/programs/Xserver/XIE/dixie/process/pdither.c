/* $Xorg: pdither.c,v 1.4 2001/02/09 02:04:22 xorgcvs Exp $ */
/**** module pdither.c ****/
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
  
	pdither.c -- DIXIE routines for managing the Dither element
  
	Larry Hare -- AGE Logic, Inc. May 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/process/pdither.c,v 3.5 2001/12/14 19:58:05 dawes Exp $ */

#define _XIEC_PDITHER

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
#include <dixie_p.h>
  /*
   *  Server XIE Includes
   */
#include <corex.h>
#include <error.h>
#include <macro.h>
#include <element.h>
#include <technq.h>
#include <difloat.h>

/*
 *  routines internal to this module
 */
static Bool PrepPDither(floDefPtr flo, peDefPtr ped);
static Bool PrepPDitherStandard(floDefPtr flo, peDefPtr ped, pointer raw, pointer tec);

/*
 * dixie element entry points
 */
static diElemVecRec pDitherVec = {
  PrepPDither		/* prepare for analysis and execution	*/
  };

/*------------------------------------------------------------------------
-------------------- routine: make a dither element ------------------
------------------------------------------------------------------------*/
peDefPtr MakeDither(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
  peDefPtr ped;
  ELEMENT(xieFloDither);
  ELEMENT_AT_LEAST_SIZE(xieFloDither);
  ELEMENT_NEEDS_1_INPUT(src);
  
  if(!(ped = MakePEDef(1, (CARD32)stuff->elemLength<<2, 0)))
    FloAllocError(flo,tag,xieElemDither, return(NULL)) ;

  ped->diVec	     = &pDitherVec;
  ped->phototag      = tag;
  ped->flags.process = TRUE;
  raw = (xieFloDither *)ped->elemRaw;
  /*
   * copy the client element parameters (swap if necessary)
   */
  if( flo->reqClient->swapped ) {
    raw->elemType   = stuff->elemType;
    raw->elemLength = stuff->elemLength;
    cpswaps(stuff->src, raw->src);
    raw->bandMask = stuff->bandMask;
    cpswapl(stuff->levels0,  raw->levels0);
    cpswapl(stuff->levels1,  raw->levels1);
    cpswapl(stuff->levels2,  raw->levels2);
    cpswaps(stuff->lenParams, raw->lenParams);
    cpswaps(stuff->dither, raw->dither);
  }
  else
    memcpy((char *)raw, (char *)stuff, sizeof(xieFloDither));
  /*
   * copy technique data (if any)
   */
  if(!(ped->techVec = FindTechnique(xieValDither, raw->dither)) ||
     !(ped->techVec->copyfnc(flo, ped, &stuff[1], &raw[1], raw->lenParams,
					  raw->dither == xieValDefault)))
    TechniqueError(flo,ped,xieValDither,raw->dither,raw->lenParams,
		   return(ped));

 /*
   * assign phototag to inFlo
   */
  ped->inFloLst[SRCtag].srcTag = raw->src;


  return(ped);
}                               /* end MakePDither */

/*------------------------------------------------------------------------
---------------- routine: copy routine for no Error Diffusion technique --
------------------------------------------------------------------------*/

Bool CopyPDitherErrorDiffusion(TECHNQ_COPY_ARGS)
{
  VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, isDefault);

  return(tsize == 0);
}

/*------------------------------------------------------------------------
---------------- routine: copy routine for Ordered techniques  ---------
------------------------------------------------------------------------*/

#undef  sparms
#define sparms ((xieTecDitherOrdered *)sParms)
#undef  rparms
#define rparms ((xieTecDitherOrdered *)rParms)

Bool CopyPDitherOrdered(TECHNQ_COPY_ARGS)
{
     VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, isDefault);

     if (tsize) 
	rparms->thresholdOrder = sparms->thresholdOrder;
     else
    	rparms->thresholdOrder = 4;

     return(TRUE);
}

/*------------------------------------------------------------------------
---------------- routine: prep routine for no param techniques -------------
------------------------------------------------------------------------*/
static
Bool PrepPDitherStandard(
     floDefPtr  flo,
     peDefPtr   ped,
     pointer    raw,
     pointer    tec)
{
  return(TRUE);
}

/*------------------------------------------------------------------------
---------------- routine: prep routine for Error Diffusion techniques ----
------------------------------------------------------------------------*/
Bool PrepPDitherErrorDiffusion(
     floDefPtr  flo,
     peDefPtr   ped,
     xieFloDither *raw,
     pointer tec)
{
  return PrepPDitherStandard(flo, ped, (pointer) raw, tec);
}

/*------------------------------------------------------------------------
---------------- routine: prep routine for Ordered technique -------------
------------------------------------------------------------------------*/
Bool PrepPDitherOrdered(
     floDefPtr  flo,
     peDefPtr   ped,
     xieFloDither *raw,
     xieTecDitherOrdered *tec)
{
  return PrepPDitherStandard(flo, ped, (pointer) raw, (pointer) tec);
}

/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/

static Bool PrepPDither(floDefPtr flo, peDefPtr ped)
{
  inFloPtr inf = &ped->inFloLst[SRCtag];
  outFloPtr src = &inf->srcDef->outFlo;
  outFloPtr dst = &ped->outFlo;
  xieFloDither *raw = (xieFloDither *)ped->elemRaw;
  CARD32 *levels = &(raw->levels0);
  int b;

  /* grab a copy of the src attributes and propagate them to our input */
  dst->bands = inf->bands = src->bands;
  for(b = 0; b < src->bands; b++) {

	dst->format[b] = inf->format[b] = src->format[b];

	if ((raw->bandMask & (1<<b)) == 0)
	    continue;

	if (IsntConstrained(src->format[b].class) &&
	    src->format[b].class == BIT_PIXEL)
		MatchError(flo, ped, return(FALSE));

  	if ((dst->format[b].levels = *(levels+b)) > src->format[b].levels)
		ValueError(flo,ped,*(levels+b),return(FALSE));
  }

  /* Set depth, class, stride, and pitch */
  if (UpdateFormatfromLevels(ped) == FALSE)
	MatchError(flo,ped, return(FALSE));

  /* Take care of any technique parameters */
  if (!(ped->techVec->prepfnc(flo, ped, raw, &raw[1])))
	TechniqueError(flo,ped,xieValDither,raw->dither,raw->lenParams,
		       return(FALSE));

  return (TRUE);
}	

/* end module pdither.c */
