/* $Xorg: pconv.c,v 1.4 2001/02/09 02:04:21 xorgcvs Exp $ */
/**** module pconv.c ****/
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
  
	pconv.c -- DIXIE routines for managing the Convolution element
  
	Dean Verheiden -- AGE Logic, Inc. June 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/process/pconv.c,v 3.6 2001/12/14 19:58:05 dawes Exp $ */

#define _XIEC_PCONV

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
static Bool PrepConvolve(floDefPtr flo, peDefPtr ped);

/*
 * dixie entry points
 */
static diElemVecRec pConvolveVec = {
    PrepConvolve		/* prepare for analysis and execution	*/
    };


/*------------------------------------------------------------------------
----------------------- routine: make a convolution element --------------
------------------------------------------------------------------------*/
peDefPtr MakeConvolve(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
  int inputs;
  peDefPtr ped;
  inFloPtr inFlo;
  ConvFloat *pvt;
  xieTypFloat *kptr;
  int i, numke;
  ELEMENT(xieFloConvolve);
  ELEMENT_AT_LEAST_SIZE(xieFloConvolve);
  ELEMENT_NEEDS_1_INPUT(src);
  inputs = 1 + (stuff->domainPhototag ? 1 :0);


  numke = stuff->kernelSize * stuff->kernelSize;

  if(!(ped = MakePEDef(inputs, (CARD32)stuff->elemLength<<2,
			       numke * sizeof(ConvFloat))))
    FloAllocError(flo, tag, xieElemConvolve, return(NULL));

  ped->diVec	     = &pConvolveVec;
  ped->phototag      = tag;
  ped->flags.process = TRUE;
  raw = (xieFloConvolve *)ped->elemRaw;
  /*
   * copy the client element parameters (swap if necessary)
   */
  if( flo->reqClient->swapped ) {
    raw->elemType   = stuff->elemType;
    raw->elemLength = stuff->elemLength;
    cpswaps(stuff->src, raw->src);
    cpswapl(stuff->domainOffsetX, raw->domainOffsetX);
    cpswapl(stuff->domainOffsetY, raw->domainOffsetY);
    cpswaps(stuff->domainPhototag,raw->domainPhototag);
    raw->bandMask = stuff->bandMask;
    raw->kernelSize = stuff->kernelSize;
    cpswaps(stuff->convolve, raw->convolve);
    cpswaps(stuff->lenParams, raw->lenParams);
  }
  else
    memcpy((char *)raw, (char *)stuff, sizeof(xieFloConvolve));

  /* Copy over and convert the kernel */
  kptr = (xieTypFloat *)&stuff[1];
  pvt = (ConvFloat *)ped->elemPvt;
  if (flo->reqClient->swapped)
	  for (i = 0; i < numke; i++) {
		/* can't use *pvt++ = ConvertFromIEEE(lswapl(*kptr++)); */
		/* because lswapl is a macro, and overincrements kptr   */
		*pvt++ = ConvertFromIEEE(lswapl(*kptr));
		++kptr;
	  }
  else
	  for (i = 0; i < numke; i++) 
		*pvt++ = ConvertFromIEEE(*kptr++);
  /* 
   * Ensure that the kernel size is odd
   */
  if (!(stuff->kernelSize & 1))
    ValueError(flo,ped,(CARD32)raw->kernelSize,return(ped));
	
  /*
   * copy technique data (if any) 
   * Note that we must skip past the convolution kernel to get there
   */
  if(!(ped->techVec = FindTechnique(xieValConvolve, raw->convolve)) ||
     !(ped->techVec->copyfnc(flo, ped, (CARD8 *)&stuff[1] + numke * 4,
				       (CARD8 *)&raw[1] + numke * 4, 
				        raw->lenParams, 
					raw->convolve == xieValDefault))) 
    TechniqueError(flo,ped,xieValConvolve,raw->convolve,raw->lenParams,
		   return(ped));

  /*
   * assign phototags to inFlos
   */
  inFlo = ped->inFloLst;
  inFlo[SRCtag].srcTag = raw->src;
  if(raw->domainPhototag) inFlo[ped->inCnt-1].srcTag = raw->domainPhototag;
  
  return(ped);
}                               /* end MakeConv */

/*------------------------------------------------------------------------
---------------- routine: copy routine for Constant technique  ---------
------------------------------------------------------------------------*/

#undef  sparms
#define sparms ((xieTecConvolveConstant *)sParms)
#undef  rparms
#define rparms ((xieTecConvolveConstant *)rParms)

Bool CopyConvolveConstant(TECHNQ_COPY_ARGS)
{
     pTecConvolveConstantDefPtr pvt;

     VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, isDefault);

     if (!(ped->techPvt=(pointer )XieMalloc(sizeof(pTecConvolveConstantDefRec))))
	     FloAllocError(flo, ped->phototag, xieElemConvolve, return(TRUE));

     pvt = (pTecConvolveConstantDefPtr)ped->techPvt;

     if (isDefault || !tsize) {
	     pvt->constant[0] = pvt->constant[1] = pvt->constant[2] = 0;
     } else if( flo->reqClient->swapped ) {
	     pvt->constant[0] = ConvertFromIEEE(lswapl(sparms->constant0));
	     pvt->constant[1] = ConvertFromIEEE(lswapl(sparms->constant1));
	     pvt->constant[2] = ConvertFromIEEE(lswapl(sparms->constant2));
      } else {
	     pvt->constant[0] = ConvertFromIEEE(sparms->constant0);
	     pvt->constant[1] = ConvertFromIEEE(sparms->constant1);
	     pvt->constant[2] = ConvertFromIEEE(sparms->constant2);
      }

     return (TRUE);
}

#ifdef  BEYOND_SI
/*------------------------------------------------------------------------
---------------- routine: copy routine for no param techniques -------------
------------------------------------------------------------------------*/

Bool CopyConvolveReplicate(TECHNQ_COPY_ARGS)
{
  return(tsize == 0);
}
#endif /* BEYOND_SI */

#undef  sparms
#undef  rparms


/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/
static Bool PrepConvolve(floDefPtr flo, peDefPtr ped)
{
  xieFloConvolve *raw = (xieFloConvolve *)ped->elemRaw;
  inFloPtr  ind, in = &ped->inFloLst[SRCtag];
  outFloPtr dom, src = &in->srcDef->outFlo;
  outFloPtr dst = &ped->outFlo;
  int b;

  /* check out our process domain */
  if(raw->domainPhototag) {
    ind = &ped->inFloLst[ped->inCnt-1];
    dom = &ind->srcDef->outFlo;
    if((ind->bands = dom->bands) != 1 || IsntDomain(dom->format[0].class))
      DomainError(flo,ped,raw->domainPhototag, return(FALSE));
    ind->format[0] = dom->format[0];
  } else
    dom = NULL;

  /* grab a copy of the input attributes and propagate them to our output */
  dst->bands = in->bands = src->bands;

  for(b = 0; b < dst->bands; b++) {
	if (IsntCanonic(src->format[b].class) || 
	     ((raw->bandMask & (1<<b)) && src->format[b].class == BIT_PIXEL))
		MatchError(flo, ped, return(FALSE));
	dst->format[b] = in->format[b] = src->format[b];
  }

  if(!(ped->techVec->prepfnc(flo, ped, raw, &raw[1] + 
		raw->kernelSize * raw->kernelSize * 4)))
    TechniqueError(flo,ped,xieValConvolve,raw->convolve,raw->lenParams,
		   return(FALSE));

  return( TRUE );
}                               /* end PrepConvolve */

/*------------------------------------------------------------------------
---------------- routine: prep routine for no param techniques -----------
------------------------------------------------------------------------*/
Bool PrepConvolveStandard(
     floDefPtr  flo,
     peDefPtr   ped,
     pointer    raw,
     pointer    tec)
{
  return(TRUE);
}
/* end module pconv.c */
