/* $Xorg: phist.c,v 1.4 2001/02/09 02:04:22 xorgcvs Exp $ */
/**** module phist.c ****/
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
  
	phist.c -- DIXIE routines for managing the MatchHistogram element
  
	Dean Verheiden -- AGE Logic, Inc. August 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/process/phist.c,v 3.6 2001/12/14 19:58:06 dawes Exp $ */

#define _XIEC_PHIST

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
#include <macro.h>
#include <element.h>
#include <technq.h>
#include <difloat.h>
#include <memory.h>


/*
 *  routines internal to this module
 */
static Bool PrepMatchHistogram(floDefPtr flo, peDefPtr ped);

/*
 * dixie entry points
 */
static diElemVecRec pMatchHistogramVec =
{
	PrepMatchHistogram	/* prepare for analysis and execution	*/
};


/*------------------------------------------------------------------------
----------------- routine: make a match histogram element ----------------
------------------------------------------------------------------------*/
peDefPtr MakeMatchHistogram(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
    int inputs;
    peDefPtr ped;
    inFloPtr inFlo;
    ELEMENT(xieFloMatchHistogram);
    ELEMENT_AT_LEAST_SIZE(xieFloMatchHistogram);
    ELEMENT_NEEDS_1_INPUT(src);
    
    inputs = 1 + (stuff->domainPhototag ? 1 : 0);
    
    if (!(ped = MakePEDef(inputs, (CARD32)stuff->elemLength<<2, 0)))
    	FloAllocError(flo, tag, xieElemMatchHistogram, return(NULL));

    ped->diVec	   = &pMatchHistogramVec;
    ped->phototag      = tag;
    ped->flags.process = TRUE;
    raw = (xieFloMatchHistogram *)ped->elemRaw;
    /*
     * copy the client element parameters (swap if necessary)
     */
    if (flo->reqClient->swapped) {
    	raw->elemType   = stuff->elemType;
    	raw->elemLength = stuff->elemLength;
    	cpswaps(stuff->src, raw->src);
    	cpswapl(stuff->domainOffsetX, raw->domainOffsetX);
    	cpswapl(stuff->domainOffsetY, raw->domainOffsetY);
    	cpswaps(stuff->domainPhototag,raw->domainPhototag);
    	cpswaps(stuff->shape,raw->shape);
    	cpswaps(stuff->lenParams,raw->lenParams);
    } else
    	memcpy((char *)raw, (char *)stuff, sizeof(xieFloMatchHistogram));
  /*
   * copy technique data (if any)
   */
  if(!(ped->techVec = FindTechnique(xieValHistogram, raw->shape)) || 
     !(ped->techVec->copyfnc(flo, ped, &stuff[1], &raw[1], raw->lenParams, 0)))
    TechniqueError(flo,ped,xieValHistogram,raw->shape,raw->lenParams,
		   return(ped));


    /* assign phototags to inFlos */
    inFlo = ped->inFloLst;
    inFlo[SRCtag].srcTag = raw->src;
    if(raw->domainPhototag)
    	inFlo[ped->inCnt-1].srcTag = raw->domainPhototag;
    return ped;
}                               /* end MakeMatchHistogram */

/*------------------------------------------------------------------------
------------------- routine: copy routine for Flat technique  ------------
------------------------------------------------------------------------*/

Bool CopyPHistogramFlat(TECHNQ_COPY_ARGS)
{
     return (tsize == 0);
}

/*------------------------------------------------------------------------
---------------- routine: copy routine for Gaussian technique  -----------
------------------------------------------------------------------------*/

#undef  sparms
#define sparms ((xieTecHistogramGaussian *)sParms)
#undef  rparms
#define rparms ((xieTecHistogramGaussian *)rParms)

Bool CopyPHistogramGaussian(TECHNQ_COPY_ARGS)
{
     pTecHistogramGaussianDefPtr pvt;

     VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, FALSE);

     if (!(ped->techPvt = 
			(pointer )XieMalloc(sizeof(pTecHistogramGaussianDefRec))))
         FloAllocError(flo, ped->phototag, xieElemMatchHistogram, return(TRUE));

     pvt = (pTecHistogramGaussianDefPtr)ped->techPvt;

     if( flo->reqClient->swapped ) {
         pvt->mean     = ConvertFromIEEE(lswapl(sparms->mean));
	 pvt->sigma    = ConvertFromIEEE(lswapl(sparms->sigma));
     } else {
	 pvt->mean     = ConvertFromIEEE(sparms->mean);
	 pvt->sigma    = ConvertFromIEEE(sparms->sigma);
     }

     return (TRUE);
}

/*------------------------------------------------------------------------
-------------- routine: copy routine for Hyperbolic technique  -----------
------------------------------------------------------------------------*/

#undef  sparms
#define sparms ((xieTecHistogramHyperbolic *)sParms)
#undef  rparms
#define rparms ((xieTecHistogramHyperbolic *)rParms)

Bool CopyPHistogramHyperbolic(TECHNQ_COPY_ARGS)
{
     pTecHistogramHyperbolicDefPtr pvt;

     VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, FALSE);

     if (!(ped->techPvt = 
		(pointer )XieMalloc(sizeof(pTecHistogramHyperbolicDefRec))))
         FloAllocError(flo, ped->phototag, xieElemMatchHistogram, return(TRUE));

     pvt = (pTecHistogramHyperbolicDefPtr)ped->techPvt;

     pvt->shapeFactor = sparms->shapeFactor;
     if( flo->reqClient->swapped ) {
         pvt->constant = ConvertFromIEEE(lswapl(sparms->constant));
     } else {
	 pvt->constant = ConvertFromIEEE(sparms->constant);
     }

     return (TRUE);
}

/*------------------------------------------------------------------------
---------------- routine: prep routine for Gaussian technique ------------
------------------------------------------------------------------------*/
Bool PrepPHistogramFlat(floDefPtr flo, peDefPtr ped)
{
  return(TRUE);
}

/*------------------------------------------------------------------------
---------------- routine: prep routine for Gaussian technique ------------
------------------------------------------------------------------------*/
Bool PrepPHistogramGaussian(floDefPtr flo, peDefPtr ped)
{
  if (((pTecHistogramGaussianDefPtr)ped->techPvt)->sigma <= 0)
      return(FALSE);

  return(TRUE);
}

/*------------------------------------------------------------------------
---------------- routine: prep routine for Hyperbolic technique ----------
------------------------------------------------------------------------*/
Bool PrepPHistogramHyperbolic(floDefPtr flo, peDefPtr ped)
{
  double constant = ((pTecHistogramHyperbolicDefPtr)ped->techPvt)->constant;

  if (constant >= -1 && constant <= 0)
      return(FALSE);

  return(TRUE);
}

/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/
static Bool PrepMatchHistogram(floDefPtr flo, peDefPtr ped)
{
    xieFloMatchHistogram *raw = (xieFloMatchHistogram *)ped->elemRaw;
    inFloPtr  ind, in = &ped->inFloLst[SRCtag];
    outFloPtr src = &in->srcDef->outFlo;
    outFloPtr dst = &ped->outFlo;

    /* make sure source parameters are legal for MatchHistogram */
    if (IsntConstrained(src->format[0].class) ||
	src->format[0].class == BIT_PIXEL ||
        src->bands != 1)
        MatchError(flo,ped, return(FALSE));

    /* check out our process domain */
    if(raw->domainPhototag) {
    	outFloPtr dom;

    	ind = &ped->inFloLst[ped->inCnt-1];
    	dom = &ind->srcDef->outFlo;
    	if((ind->bands = dom->bands) != 1 || IsntDomain(dom->format[0].class))
    		DomainError(flo,ped,raw->domainPhototag, return(FALSE));
    	ind->format[0] = dom->format[0];
    }

    /* grab a copy of the input attributes and propagate them to output */
    dst->bands = in->bands = src->bands;
    dst->format[0] = in->format[0] = src->format[0];

    /* Take care of any technique parameters */
    if (!(ped->techVec->prepfnc(flo, ped)))
	TechniqueError(flo,ped,xieValHistogram,raw->shape,raw->lenParams,
		       return(FALSE));

    return (TRUE);
}                               /* end PrepMatchHistogram */

/* end module phist.c */
