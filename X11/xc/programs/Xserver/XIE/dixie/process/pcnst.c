/* $Xorg: pcnst.c,v 1.4 2001/02/09 02:04:20 xorgcvs Exp $ */
/**** module pcnst.c ****/
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
  
	pcnst..c -- DIXIE routines for managing the Constrain element
  
	Dean Verheiden -- AGE Logic, Inc. May 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/process/pcnst.c,v 3.6 2001/12/14 19:58:04 dawes Exp $ */

#define _XIEC_PCNST

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
#include <memory.h>

/*
 *  routines internal to this module
 */
static Bool PrepPConstrain(floDefPtr flo, peDefPtr ped);

/*
 * dixie element entry points
 */
static diElemVecRec pConstrainVec = {
  PrepPConstrain		/* prepare for analysis and execution	*/
  };

/*------------------------------------------------------------------------
-------------------- routine: make a constrain element ------------------
------------------------------------------------------------------------*/
peDefPtr MakeConstrain(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
  peDefPtr ped;
  ELEMENT(xieFloConstrain);
  ELEMENT_AT_LEAST_SIZE(xieFloConstrain);
  ELEMENT_NEEDS_1_INPUT(src);
  
  if(!(ped = MakePEDef(1, (CARD32)stuff->elemLength<<2, 0)))
    FloAllocError(flo,tag,xieElemConstrain, return(NULL)) ;

  ped->diVec	     = &pConstrainVec;
  ped->phototag      = tag;
  ped->flags.process = TRUE;
  raw = (xieFloConstrain *)ped->elemRaw;
  /*
   * copy the client element parameters (swap if necessary)
   */
  if( flo->reqClient->swapped ) {
    raw->elemType   = stuff->elemType;
    raw->elemLength = stuff->elemLength;
    cpswaps(stuff->src, raw->src);
    cpswapl(stuff->levels0,  raw->levels0);
    cpswapl(stuff->levels1,  raw->levels1);
    cpswapl(stuff->levels2,  raw->levels2);
    cpswaps(stuff->lenParams, raw->lenParams);
    cpswaps(stuff->constrain, raw->constrain);
  }
  else
    memcpy((char *)raw, (char *)stuff, sizeof(xieFloConstrain));
  /*
   * copy technique data (if any)
   */
  if(!(ped->techVec = FindTechnique(xieValConstrain, raw->constrain)) ||
     !(ped->techVec->copyfnc(flo, ped, &stuff[1], &raw[1], raw->lenParams, 0)))
    TechniqueError(flo,ped,xieValConstrain,raw->constrain,raw->lenParams,
		   return(ped));

 /*
   * assign phototag to inFlo
   */
  ped->inFloLst[SRCtag].srcTag = raw->src;


  return(ped);
}                               /* end MakePConstrain */

/*------------------------------------------------------------------------
---------------- routine: copy routine for no param techniques -------------
------------------------------------------------------------------------*/

Bool CopyPConstrainStandard(TECHNQ_COPY_ARGS)
{
  return(tsize == 0);
}

/*------------------------------------------------------------------------
---------------- routine: copy routine for Clip-Scale technique  ---------
------------------------------------------------------------------------*/

#undef  sparms
#define sparms ((xieTecClipScale *)sParms)
#undef  rparms
#define rparms ((xieTecClipScale *)rParms)

Bool CopyPConstrainClipScale(TECHNQ_COPY_ARGS)
{
     pCnstDefPtr pvt;

     VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, FALSE);

     if (!(ped->techPvt = (pointer )XieMalloc(sizeof(pCnstDefRec))))
	     FloAllocError(flo, ped->phototag,xieElemConstrain, return(TRUE));

     pvt = (pCnstDefPtr)ped->techPvt;

     if( flo->reqClient->swapped ) {
	     pvt->input_low[0] = ConvertFromIEEE(lswapl(sparms->inputLow0));
	     pvt->input_low[1] = ConvertFromIEEE(lswapl(sparms->inputLow1));
	     pvt->input_low[2] = ConvertFromIEEE(lswapl(sparms->inputLow2));
	     pvt->input_high[0] = ConvertFromIEEE(lswapl(sparms->inputHigh0));
	     pvt->input_high[1] = ConvertFromIEEE(lswapl(sparms->inputHigh1));
	     pvt->input_high[2] = ConvertFromIEEE(lswapl(sparms->inputHigh2));
	     cpswapl(sparms->outputLow0,  pvt->output_low[0]);
	     cpswapl(sparms->outputLow1,  pvt->output_low[1]);
	     cpswapl(sparms->outputLow2,  pvt->output_low[2]);
	     cpswapl(sparms->outputHigh0, pvt->output_high[0]);
	     cpswapl(sparms->outputHigh1, pvt->output_high[1]);
	     cpswapl(sparms->outputHigh2, pvt->output_high[2]);
      } else {
	     pvt->input_low[0] = ConvertFromIEEE(sparms->inputLow0);
	     pvt->input_low[1] = ConvertFromIEEE(sparms->inputLow1);
	     pvt->input_low[2] = ConvertFromIEEE(sparms->inputLow2);
	     pvt->input_high[0] = ConvertFromIEEE(sparms->inputHigh0);
	     pvt->input_high[1] = ConvertFromIEEE(sparms->inputHigh1);
	     pvt->input_high[2] = ConvertFromIEEE(sparms->inputHigh2);
	     pvt->output_low[0] = sparms->outputLow0;
	     pvt->output_low[1] = sparms->outputLow1;
	     pvt->output_low[2] = sparms->outputLow2;
	     pvt->output_high[0] = sparms->outputHigh0;
	     pvt->output_high[1] = sparms->outputHigh1;
	     pvt->output_high[2] = sparms->outputHigh2;
      }

     return (TRUE);
}
/*------------------------------------------------------------------------
---------------- routine: prep routine for no param techniques -------------
------------------------------------------------------------------------*/
Bool PrepPConstrainStandard(
     floDefPtr  flo,
     peDefPtr   ped,
     pointer raw,
     pointer tec)
{
  return(TRUE);
}
/*------------------------------------------------------------------------
---------------- routine: prep routine for Clip Scale technique ----------
------------------------------------------------------------------------*/
Bool PrepPConstrainClipScale(
     floDefPtr  flo,
     peDefPtr   ped,
     xieTecClipScale *raw,
     xieTecClipScale *tec)
{
  pCnstDefPtr pvt = (pCnstDefPtr)ped->techPvt;

  if (pvt->input_low[0] == pvt->input_high[0] ||
      pvt->output_low[0] > ped->outFlo.format[0].levels - 1 ||
      pvt->output_high[0] > ped->outFlo.format[0].levels - 1)
		return(FALSE);
  if (ped->outFlo.bands > 1) {
  	if (  pvt->input_low[1] == pvt->input_high[1] ||
	      pvt->output_low[1] > ped->outFlo.format[1].levels - 1 ||
	      pvt->output_high[1] > ped->outFlo.format[1].levels - 1 ||
  	      pvt->input_low[2] == pvt->input_high[2] ||
	      pvt->output_low[2] > ped->outFlo.format[2].levels - 1 ||
	      pvt->output_high[2] > ped->outFlo.format[2].levels - 1)
		return(FALSE);
  }

  return(TRUE);
}

/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/

static Bool PrepPConstrain(
     floDefPtr  flo,
     peDefPtr   ped)
{
  inFloPtr inf = &ped->inFloLst[SRCtag];
  outFloPtr src = &inf->srcDef->outFlo;
  outFloPtr dst = &ped->outFlo;
  xieFloConstrain *raw = (xieFloConstrain *)ped->elemRaw;
  int b;

  /* grab a copy of the src attributes and propagate them to our input */
  dst->bands = inf->bands = src->bands;
  for(b = 0; b < src->bands; b++) {

	/* This should be impossible */
	if (IsntCanonic(src->format[b].class))
		ImplementationError(flo, ped, return(FALSE));

	inf->format[b] = src->format[b];

	/* Copy outFlo values that are unchanged by constrain */
	dst->format[b].band 		= b;
  	dst->format[b].interleaved 	= src->format[b].interleaved;
	dst->format[b].width 		= src->format[b].width;
	dst->format[b].height 		= src->format[b].height;
  }
  /* Pull in levels information from the element description */ 
  if ((dst->format[0].levels = raw->levels0) > MAX_LEVELS(src->bands))
	ValueError(flo,ped,raw->levels0,return(FALSE));
  if (dst->bands > 1) {
  	if ((dst->format[1].levels = raw->levels1) > MAX_LEVELS(src->bands))
		ValueError(flo,ped,raw->levels1,return(FALSE));
	if ((dst->format[2].levels = raw->levels2) > MAX_LEVELS(src->bands))
		ValueError(flo,ped,raw->levels2,return(FALSE));
  }
  /* Set depth, class, stride, and pitch */
  if(!UpdateFormatfromLevels(ped))
    MatchError(flo,ped, return(FALSE));

  /* Take care of any technique parameters */
  if (!(ped->techVec->prepfnc(flo, ped, raw, &raw[1])))
	TechniqueError(flo,ped,xieValConstrain,raw->constrain,raw->lenParams,
		       return(FALSE));
  return (TRUE);
}	

/* end module pcnst.c */
