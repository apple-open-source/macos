/* $Xorg: pbandc.c,v 1.4 2001/02/09 02:04:20 xorgcvs Exp $ */
/**** module pbandc.c ****/
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
  
	pbandc.c -- DIXIE routines for managing the band combine element
  
	Dean Verheiden -- AGE Logic, Inc. July 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/process/pbandc.c,v 3.5 2001/12/14 19:58:03 dawes Exp $ */

#define _XIEC_PBANDC

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

/*
 *  routines internal to this module
 */
static Bool PrepBandCom(floDefPtr flo, peDefPtr ped);

/*
 * dixie entry points
 */
static diElemVecRec pBandComVec = {
    PrepBandCom		/* prepare for analysis and execution	*/
    };


/*------------------------------------------------------------------------
----------------------- routine: make a blend element --------------------
------------------------------------------------------------------------*/
peDefPtr MakeBandCom(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
  peDefPtr ped;
  inFloPtr inFlo;
  ELEMENT(xieFloBandCombine);
  ELEMENT_SIZE_MATCH(xieFloBandCombine);
  ELEMENT_NEEDS_3_INPUTS(src1,src2,src3); 

  if(!(ped = MakePEDef(3, (CARD32)stuff->elemLength<<2, 0)))
    FloAllocError(flo, tag, xieElemBandCombine, return(NULL));

  ped->diVec	     = &pBandComVec;
  ped->phototag      = tag;
  ped->flags.process = TRUE;
  raw = (xieFloBandCombine *)ped->elemRaw;
  /*
   * copy the client element parameters (swap if necessary)
   */
  if( flo->reqClient->swapped ) {
    raw->elemType   = stuff->elemType;
    raw->elemLength = stuff->elemLength;
    cpswaps(stuff->src1, raw->src1);
    cpswaps(stuff->src2, raw->src2);
    cpswaps(stuff->src3, raw->src3);
  }
  else
    memcpy((char *)raw, (char *)stuff, sizeof(xieFloBandCombine));

  /* assign phototags to inFlos
   */
  inFlo = ped->inFloLst;
  inFlo[SRCt1].srcTag = raw->src1;
  inFlo[SRCt2].srcTag = raw->src2;
  inFlo[SRCt3].srcTag = raw->src3;
  
  return(ped);
}                               /* end MakeBandCom */


/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/
static Bool PrepBandCom(floDefPtr flo, peDefPtr ped)
{
  inFloPtr  in1 = &ped->inFloLst[SRCt1]; 
  inFloPtr  in2 = &ped->inFloLst[SRCt2]; 
  inFloPtr  in3 = &ped->inFloLst[SRCt3]; 
  inFloPtr  inf = in1;
  outFloPtr sr1 = &in1->srcDef->outFlo;
  outFloPtr sr2 = &in2->srcDef->outFlo;
  outFloPtr sr3 = &in3->srcDef->outFlo;
  outFloPtr dst = &ped->outFlo;
  int b;

  /* All inputs must be single band */
  if(sr1->bands != 1 || sr2->bands != 1 || sr3->bands != 1)
    MatchError(flo,ped, return(FALSE));
  
  /* All sources must be either constrained or unconstrained */
  if(IsConstrained(sr1->format[0].class)) {
    if(IsntConstrained(sr2->format[0].class) || 
       IsntConstrained(sr3->format[0].class))
      MatchError(flo,ped, return(FALSE));
  } else {
    if(sr1->format[0].class != UNCONSTRAINED ||
       sr2->format[0].class != UNCONSTRAINED ||
       sr3->format[0].class != UNCONSTRAINED)
      MatchError(flo,ped, return(FALSE));
  }
  /* grab a copy of the input attributes and propagate them to our output */
  in1->bands = in2->bands = in3->bands = 1;
  dst->bands = xieValMaxBands;
  for(b = 0; b < xieValMaxBands; ++inf, ++b) {
    dst->format[b] = inf->format[0] = inf->srcDef->outFlo.format[0];
    dst->format[b].band = b;
    if(IsConstrained(dst->format[b].class) &&
       dst->format[b].depth > MAX_DEPTH(xieValMaxBands))
      MatchError(flo,ped, return(FALSE));
  }
  return( TRUE );
}                               /* end PrepBandCom */

/* end module pbandc.c */
