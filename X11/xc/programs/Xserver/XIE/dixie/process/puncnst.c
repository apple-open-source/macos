/* $Xorg: puncnst.c,v 1.4 2001/02/09 02:04:22 xorgcvs Exp $ */
/**** module puncnst.c ****/
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
  
	puncnst.c -- DIXIE routines for managing the Unconstrain element
  
	Dean Verheiden -- AGE Logic, Inc. May 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/process/puncnst.c,v 3.5 2001/12/14 19:58:07 dawes Exp $ */

#define _XIEC_PUNCNST

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
static Bool PrepUnconstrain(floDefPtr flo, peDefPtr ped);

/*
 * dixie entry points
 */
static diElemVecRec pUnconstrainVec = {
    PrepUnconstrain		/* prepare for analysis and execution	*/
    };


/*------------------------------------------------------------------------
----------------------- routine: make an unconstrain element -------------
------------------------------------------------------------------------*/
peDefPtr MakeUnconstrain(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
  peDefPtr ped;
  inFloPtr inFlo;
  ELEMENT(xieFloUnconstrain);
  ELEMENT_SIZE_MATCH(xieFloUnconstrain);
  ELEMENT_NEEDS_1_INPUT(src);
  
  if(!(ped = MakePEDef(1, (CARD32)stuff->elemLength<<2, 0)))
    FloAllocError(flo, tag, xieElemUnconstrain, return(NULL));

  ped->diVec	     = &pUnconstrainVec;
  ped->phototag      = tag;
  ped->flags.process = TRUE;
  raw = (xieFloUnconstrain *)ped->elemRaw;
  /*
   * copy the client element parameters (swap if necessary)
   */
  if( flo->reqClient->swapped ) {
    raw->elemType   = stuff->elemType;
    raw->elemLength = stuff->elemLength;
    cpswaps(stuff->src, raw->src);
  }
  else
    memcpy((char *)raw, (char *)stuff, sizeof(xieFloUnconstrain));
  /*
   * assign phototags to the inFlo
   */
  inFlo = ped->inFloLst;
  inFlo[SRCtag].srcTag = raw->src;
  
  return(ped);
}                               /* end MakeUnconstrain */


/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/
static Bool PrepUnconstrain(floDefPtr flo, peDefPtr ped)
{
  inFloPtr  inf = &ped->inFloLst[SRCtag];
  outFloPtr src = &inf->srcDef->outFlo;
  outFloPtr dst = &ped->outFlo;
  int b;

  /* grab a copy of the input attributes and propagate them to our output */
  dst->bands = inf->bands = src->bands;
  for(b = 0; b < dst->bands; b++) {
	/* All band formats should be the same but check anyway */
	if (IsntCanonic(src->format[b].class))
		MatchError(flo, ped, return(FALSE));

	/* First, copy everything over */
	dst->format[b] = inf->format[b] = src->format[b];
	
	/* Now, fix up everthing in the destination that will change */
	dst->format[b].class = UNCONSTRAINED;
	dst->format[b].depth = sz_RealPixel;
	dst->format[b].levels = 0;	/* Unconstrained */
	dst->format[b].stride = sz_RealPixel;
	dst->format[b].pitch = sz_RealPixel * dst->format[b].width;
  }
  return( TRUE );
}                               /* end PrepUnconstrain */

/* end module puncnst.c */
