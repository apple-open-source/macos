/* $Xorg: ppoint.c,v 1.4 2001/02/09 02:04:22 xorgcvs Exp $ */
/**** module ppoint.c ****/
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
  
	ppoint.c -- DIXIE routines for managing the Point element
  
	Robert NC Shelley -- AGE Logic, Inc. April 1993
	Ben Fahy -- AGE Logic, Inc. May 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/process/ppoint.c,v 3.5 2001/12/14 19:58:07 dawes Exp $ */

#define _XIEC_PPOINT
#define _XIEC_POINT

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
#include <error.h>

extern peDefPtr MakePoint(floDefPtr flo, xieTypPhototag tag, xieFlo *pe);

/*
 *  routines internal to this module
 */
static Bool PrepPoint(floDefPtr flo, peDefPtr ped);

/*
 * dixie entry points
 */
static diElemVecRec pPointVec = {
  PrepPoint		/* prepare for analysis and execution	*/
  };


/*------------------------------------------------------------------------
----------------------- routine: make a point element --------------------
------------------------------------------------------------------------*/
peDefPtr MakePoint(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
  int inputs;
  peDefPtr ped;
  inFloPtr inFlo;
  ELEMENT(xieFloPoint);
  ELEMENT_SIZE_MATCH(xieFloPoint);
  ELEMENT_NEEDS_2_INPUTS(src,lut);
  inputs = stuff->domainPhototag ? 3 : 2;
  
  if(!(ped = MakePEDef(inputs, (CARD32)stuff->elemLength<<2, 0))) 
    FloAllocError(flo,tag,xieElemPoint, return(NULL)) ;

  ped->diVec	     = &pPointVec;
  ped->phototag      = tag;
  ped->flags.process = TRUE;
  raw = (xieFloPoint *)ped->elemRaw;
  /*
   * copy the client element parameters (swap if necessary)
   */
  if( flo->reqClient->swapped ) {
    raw->elemType   = stuff->elemType;
    raw->elemLength = stuff->elemLength;
    cpswaps(stuff->src, raw->src);
    cpswaps(stuff->lut, raw->lut);
    cpswapl(stuff->domainOffsetX, raw->domainOffsetX);
    cpswapl(stuff->domainOffsetY, raw->domainOffsetY);
    cpswaps(stuff->domainPhototag,raw->domainPhototag);
    raw->bandMask = stuff->bandMask;
  }
  else
    memcpy((char *)raw, (char *)stuff, sizeof(xieFloPoint));
  /*
   * assign phototags to inFlos
   */
  inFlo = ped->inFloLst;
  inFlo[SRCtag].srcTag = raw->src;
  inFlo[LUTtag].srcTag = raw->lut;
  if(raw->domainPhototag)
#if XIE_FULL
    inFlo[ped->inCnt-1].srcTag = raw->domainPhototag;
#else
    DomainError(flo,ped,raw->domainPhototag, return(ped));
#endif
  
  return(ped);
}                               /* end MakePoint */


/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/
static Bool PrepPoint(floDefPtr flo, peDefPtr ped)
{
  xieFloPoint *raw = (xieFloPoint *)ped->elemRaw;

  inFloPtr  indom,inlut= &ped->inFloLst[LUTtag],insrc = &ped->inFloLst[SRCtag];
  outFloPtr outdom, outlut= &inlut->srcDef->outFlo,
	outsrc= &insrc->srcDef->outFlo;
  outFloPtr dst = &ped->outFlo;
  int b;

  /* propage band attributes */
  insrc->bands = outsrc->bands;
  inlut->bands = outlut->bands;

  dst->bands   = outlut->bands;	/* see V4.12 spec, page 6-2 */

  /* if process domain src and lut class must match */
  if (raw->domainPhototag && insrc->bands != inlut->bands)
     	MatchError(flo,ped, return(FALSE));

  /* check to make sure input image is constrained, and lut is a lut */
  if(IsntConstrained(outsrc->format[0].class) ||
     IsntLut(outlut->format[0].class) )
    MatchError(flo,ped, return(FALSE));

  /* propagate outflo format of src to our inflo for src */
  for (b=0; b<outsrc->bands; ++b)
    insrc->format[b] = outsrc->format[b];

  /* propagate outflo format of lut to our inflo for lut */
  for (b=0; b<inlut->bands; ++b)
    inlut->format[b] = outlut->format[b];

  /* do same with process domain, if it is specified */
  if(raw->domainPhototag) {
    indom = &ped->inFloLst[ped->inCnt-1];
    outdom = &indom->srcDef->outFlo;
    if(IsntDomain(outdom->format[0].class) ||
       (indom->bands = outdom->bands) != 1)
      DomainError(flo,ped,raw->domainPhototag, return(FALSE));
    indom->format[0] = outdom->format[0];
  } else
    outdom = NULL;

/***	Painful enumeration of cases	***/

  if (outlut->bands == 1 && outsrc->bands == 3) {
    int level_product;

    /* Width and heights of all bands must match */
    if (insrc->format[0].width  != insrc->format[1].width  ||
        insrc->format[1].width  != insrc->format[2].width  ||
        insrc->format[0].height != insrc->format[1].height ||
        insrc->format[1].height != insrc->format[2].height)
	MatchError(flo,ped, return(FALSE));

    /* make tripleband src into CRAZY PIXELS! produce singleband */
    if ((raw->bandMask !=7) || (outdom != NULL))
     	MatchError(flo,ped, return(FALSE)); /* see p7-25 of v4.12 spec */

    /* check to make sure length of lut is sufficient */
    level_product = insrc->format[0].levels *
		    insrc->format[1].levels *
		    insrc->format[2].levels;

    if (inlut->format[0].height < level_product)
	MatchError(flo,ped, return(FALSE));

    dst->format[0] = insrc->format[0];
    dst->format[0].levels = inlut->format[0].levels;
    if (!UpdateFormatfromLevels(ped))
	MatchError(flo,ped, return(FALSE));

  }

  else if (outlut->bands == 3 && outsrc->bands == 1) {
    /* apply lut for each band to src */

    /* this variation does not support Domains. */
    if (outdom != NULL)
    	MatchError(flo,ped, return(FALSE));

    /* destination format will be close to insrc, but not same */
    for(b = 0; b < dst->bands; b++)  {
    	dst->format[b] = insrc->format[0];
    	dst->format[b].band = b;
	if ((raw->bandMask & (1<<b)) == 0) continue;
	dst->format[b].levels = inlut->format[b].levels;
        if (inlut->format[b].height < insrc->format[0].levels)
		MatchError(flo,ped, return(FALSE));
    }
    if (!UpdateFormatfromLevels(ped))
	MatchError(flo,ped, return(FALSE));

  }

  else if (outlut->bands == outsrc->bands && 
	   (outlut->bands == 3 || outlut->bands == 1) ) {

    /* apply lut for each band to src of each band */

    for(b = 0; b < dst->bands; b++)  {

    	dst->format[b] = insrc->format[b];
	if ((raw->bandMask & (1<<b)) == 0) continue;

	dst->format[b].levels = inlut->format[b].levels;

	/* check to make sure length of lut is sufficient */
        if (inlut->format[b].height < insrc->format[b].levels)
    		MatchError(flo,ped, return(FALSE));

	/* if domain is used, lut levels must be == src levels */
	/* (or else we don't know what to do with pass-thru data */
	if (outdom != NULL)
	  if (inlut->format[b].levels != insrc->format[b].levels)
     		MatchError(flo,ped, return(FALSE));
    }
    if (!UpdateFormatfromLevels(ped))
	MatchError(flo,ped, return(FALSE));

  }
  else {
	/* is this possible? */
	ImplementationError(flo,ped,return(FALSE));
  }

  return(TRUE);
}                               /* end PrepPoint */

/* end module ppoint.c */
