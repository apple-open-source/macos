/* $Xorg: parith.c,v 1.6 2001/02/09 02:04:20 xorgcvs Exp $ */
/**** module parith.c ****/
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
  
	parith.c -- DIXIE routines for managing the Arithmetic element
  
	Robert NC Shelley -- AGE Logic, Inc. April 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/process/parith.c,v 3.5 2001/12/14 19:58:03 dawes Exp $ */

#define _XIEC_PARITH

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
static Bool PrepArith(floDefPtr flo, peDefPtr ped);

/*
 * dixie entry points
 */
static diElemVecRec pArithVec = {
    PrepArith			/* prepare for analysis and execution	*/
    };


/*------------------------------------------------------------------------
----------------------- routine: make a arithmetic element --------------------
------------------------------------------------------------------------*/
peDefPtr MakeArith(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
  int inputs;
  peDefPtr ped;
  inFloPtr inFlo;
  pArithDefPtr pvt;
  ELEMENT(xieFloArithmetic);
  ELEMENT_SIZE_MATCH(xieFloArithmetic);
  ELEMENT_NEEDS_1_INPUT(src1);
  inputs = 1 + (stuff->src2 ? 1 : 0) + (stuff->domainPhototag ? 1 :0);
  
  if(!(ped = MakePEDef(inputs, (CARD32)stuff->elemLength<<2,
		       sizeof(pArithDefRec))))
    FloAllocError(flo, tag, xieElemArithmetic, return(NULL));

  ped->diVec	     = &pArithVec;
  ped->phototag      = tag;
  ped->flags.process = TRUE;
  raw = (xieFloArithmetic *)ped->elemRaw;
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
    raw->bandMask = stuff->bandMask;
    cpswapl(stuff->constant0, raw->constant0);
    cpswapl(stuff->constant1, raw->constant1);
    cpswapl(stuff->constant2, raw->constant2);
  }
  else
    memcpy((char *)raw, (char *)stuff, sizeof(xieFloArithmetic));

  if(!raw->src2) {
    /*
     * convert constants
     */
    pvt = (pArithDefPtr)ped->elemPvt;
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
}                               /* end MakeArith */


/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/
static Bool PrepArith(floDefPtr flo, peDefPtr ped)
{
  xieFloArithmetic *raw = (xieFloArithmetic *)ped->elemRaw;
  inFloPtr  ind, in2, in1 = &ped->inFloLst[SRCt1];
  outFloPtr dom, sr2, sr1 = &in1->srcDef->outFlo;
  outFloPtr dst = &ped->outFlo;
  CARD8 bmask = raw->bandMask;
  int b;

  /* make sure input is not bitonal */
  for (b = 0; b < sr1->bands; b++)
    if (IsntCanonic(sr1->format[b].class) ||
	((bmask & (1<<b)) && sr1->format[b].class == BIT_PIXEL))
	    MatchError(flo,ped, return(FALSE));

  /* check out our second source */
  if(raw->src2) {
    in2 = &ped->inFloLst[SRCt2];
    sr2 = &in2->srcDef->outFlo;
    if(sr1->bands != sr2->bands)
      MatchError(flo,ped, return(FALSE));
    for (b = 0; b < sr1->bands; b++) {
	if ((bmask & (1<<b)) == 0) continue;
	if (sr1->format[b].class != sr2->format[b].class ||
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
    if(IsntDomain(dom->format[0].class) || 
       (ind->bands = dom->bands) != 1)
      DomainError(flo,ped,raw->domainPhototag, return(FALSE));
    ind->format[0] = dom->format[0];
  } else
    dom = NULL;

  /* grab a copy of the input attributes and propagate them to our output */
  dst->bands = in1->bands = sr1->bands;
  for(b = 0; b < dst->bands; b++) {
    dst->format[b] = in1->format[b] = sr1->format[b];
    if(sr2)
      in2->format[b] = sr2->format[b];
  }

  /* check out our operator */
  switch(raw->operator) {
  case xieValAdd:
  case xieValMax:
  case xieValMin:
  case xieValSub:
  case xieValSubRev:
    break;
  case xieValDiv:
  case xieValDivRev:
  case xieValGamma:
  case xieValMul:
    if(raw->src2)
      SourceError(flo,ped, return(FALSE));
    break;
  default: OperatorError(flo,ped,raw->operator, return(FALSE));
  }
  return( TRUE );
}                               /* end PrepArith */

/* end module parith.c */
