/* $Xorg: pcomp.c,v 1.6 2001/02/09 02:04:21 xorgcvs Exp $ */
/**** module pcomp.c ****/
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
  
	pcomp.c -- DIXIE routines for managing the compare element
  
	Dean Verheiden -- AGE Logic, Inc. July 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/process/pcomp.c,v 3.5 2001/12/14 19:58:05 dawes Exp $ */

#define _XIEC_PCOMP

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
#include <difloat.h>

/*
 *  routines internal to this module
 */
static Bool PrepCompare(floDefPtr flo, peDefPtr ped);

/*
 * dixie entry points
 */
static diElemVecRec pCompareVec = {
    PrepCompare			/* prepare for analysis and execution	*/
    };


/*------------------------------------------------------------------------
----------------------- routine: make a arithmetic element --------------------
------------------------------------------------------------------------*/
peDefPtr MakeCompare(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
  int inputs;
  peDefPtr ped;
  inFloPtr inFlo;
  pCompareDefPtr pvt;
  ELEMENT(xieFloCompare);
  ELEMENT_SIZE_MATCH(xieFloCompare);
  ELEMENT_NEEDS_1_INPUT(src1);
  inputs = 1 + (stuff->src2 ? 1 : 0) + (stuff->domainPhototag ? 1 :0);
  
  if(!(ped = MakePEDef(inputs, (CARD32)stuff->elemLength<<2,
		       sizeof(pCompareDefRec))))
    FloAllocError(flo, tag, xieElemCompare, return(NULL));

  ped->diVec	     = &pCompareVec;
  ped->phototag      = tag;
  ped->flags.process = TRUE;
  raw = (xieFloCompare *)ped->elemRaw;
  /*
   * copy the client element parameters (swap if necessary)
   */
  if( flo->reqClient->swapped ) {
    raw->elemType   = stuff->elemType;
    raw->elemLength = stuff->elemLength;
    cpswaps(stuff->src1, raw->src1);
    cpswaps(stuff->src2, raw->src2);
    cpswapl(stuff->domainOffsetX, raw->domainOffsetX);
    cpswapl(stuff->domainOffsetY, raw->domainOffsetY);
    cpswaps(stuff->domainPhototag,raw->domainPhototag);
    raw->operator = stuff->operator;
    raw->combine  = stuff->combine;
    cpswapl(stuff->constant0, raw->constant0);
    cpswapl(stuff->constant1, raw->constant1);
    cpswapl(stuff->constant2, raw->constant2);
    raw->bandMask = stuff->bandMask;
  }
  else
    memcpy((char *)raw, (char *)stuff, sizeof(xieFloCompare));

  if(!raw->src2) {
    /*
     * convert constants
     */
    pvt = (pCompareDefPtr)ped->elemPvt;
    pvt->constant[0] = ConvertFromIEEE(raw->constant0);
    pvt->constant[1] = ConvertFromIEEE(raw->constant1);
    pvt->constant[2] = ConvertFromIEEE(raw->constant2);
  }
  /*
   * assign phototags to inFlos
   */
  inFlo = ped->inFloLst;
  inFlo[SRCt1].srcTag = raw->src1;
  if(raw->src2) inFlo[SRCt2].srcTag = raw->src2;
  if(raw->domainPhototag) inFlo[ped->inCnt-1].srcTag = raw->domainPhototag;
  
  return(ped);
}                               /* end MakeCompare */


/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/
static Bool PrepCompare(floDefPtr flo, peDefPtr ped)
{
  xieFloCompare *raw = (xieFloCompare *)ped->elemRaw;
  inFloPtr  ind, in2, in1 = &ped->inFloLst[SRCt1];
  outFloPtr dom, sr2, sr1 = &in1->srcDef->outFlo;
  outFloPtr dst = &ped->outFlo;
  CARD8 mask, bandMask = raw->bandMask;
  int b;

  /* check out our second source */
  if(raw->src2) {
    in2 = &ped->inFloLst[SRCt2];
    sr2 = &in2->srcDef->outFlo;
    if(sr1->bands != sr2->bands)
      MatchError(flo,ped, return(FALSE));
    for (b = 0, mask = 1; b < sr1->bands; b++, mask <<= 1) {
	if (sr1->bands == 3 && raw->combine && (!(mask & bandMask)))
	    continue;
        if (IsntCanonic(sr1->format[b].class) ||
	    sr1->format[b].class != sr2->format[b].class ||
            (IsConstrained(sr1->format[b].class) && 
             sr1->format[b].levels != sr2->format[b].levels))
	        MatchError(flo,ped, return(FALSE));
    }
    in2->bands = sr2->bands;
  } else
    sr2 = NULL;

  /* check out our process domain */
  if(raw->domainPhototag) {
    ind = &ped->inFloLst[ped->inCnt-1];
    dom = &ind->srcDef->outFlo;
    if((ind->bands = dom->bands) != 1 || IsntDomain(dom->format[0].class))
      DomainError(flo,ped,raw->domainPhototag, return(FALSE));
    ind->format[0] = dom->format[0];
  } else
    dom = NULL;

  /* grab a copy of the input attributes and propagate them to our inputs */
  in1->bands = sr1->bands;
  for(b = 0; b < sr1->bands; b++) {
    in1->format[b] = sr1->format[b];
    if(sr2)
      in2->format[b] = sr2->format[b];
  }

  /* Determine class of output */
  if (sr1->bands > 1 && !raw->combine) {
    if (~(~0<<sr1->bands) & ~raw->bandMask) /* mask must be for all bands */
      MatchError(flo,ped, return(FALSE));
    dst->bands = sr1->bands;
  } else 
    dst->bands = 1;

  /* In this case, all src bands must have the same dimension */
  if (sr1->bands == 3 && raw->combine &&
      (((bandMask & 3) == 3 && 
	 (sr1->format[0].width  != sr1->format[1].width  ||
          sr1->format[0].height != sr1->format[1].height)) ||
       ((bandMask & 5) == 5 && 
	 (sr1->format[0].width  != sr1->format[2].width  ||
          sr1->format[0].height != sr1->format[2].height)) ||
       ((bandMask & 6) == 6 && 
	 (sr1->format[1].width  != sr1->format[2].width  ||
          sr1->format[1].height != sr1->format[2].height))))
      MatchError(flo,ped, return(FALSE));

  /* Check the operator */
  if (raw->operator != xieValLT && raw->operator != xieValLE &&
      raw->operator != xieValEQ && raw->operator != xieValNE &&
      raw->operator != xieValGT && raw->operator != xieValGE)
        OperatorError(flo,ped,raw->operator, return(FALSE));

  /*  
     Second check necessary because of protocol . . . must distinguish
     between FloMatch errors and FloOperator errors
  */
  if (dst->bands > 1 && raw->combine) {
    if (raw->operator != xieValEQ && raw->operator != xieValNE)
        MatchError(flo,ped, return(FALSE));
  } 

  /* Set up destination parameters */
  for(b = 0; b < dst->bands; b++) {
      CARD32 bits = sr1->format[b].width;

      dst->format[b].class       = BIT_PIXEL;
      dst->format[b].band        = b;
      dst->format[b].interleaved = FALSE;
      dst->format[b].depth       = 1;
      dst->format[b].width	 = bits;
      dst->format[b].height	 = sr1->format[b].height;
      dst->format[b].levels      = 2;
      dst->format[b].stride      = 1;
      dst->format[b].pitch       = bits + Align(bits,PITCH_MOD);
  }
  return( TRUE );
}                               /* end PrepCompare */

/* end module pcomp.c */
