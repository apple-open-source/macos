/* $Xorg: ppaste.c,v 1.4 2001/02/09 02:04:22 xorgcvs Exp $ */
/**** module ppaste.c ****/
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
  
	ppaste.c -- DIXIE routines for managing the PasteUp element
  
	Dean Verheiden -- AGE Logic, Inc. June 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/process/ppaste.c,v 3.5 2001/12/14 19:58:07 dawes Exp $ */

#define _XIEC_PPASTE

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
static Bool PrepPasteUp(floDefPtr flo, peDefPtr ped);

/*
 * dixie entry points
 */
static diElemVecRec pPasteUpVec = {
    PrepPasteUp			/* prepare for analysis and execution	*/
    };


/*------------------------------------------------------------------------
----------------------- routine: make a arithmetic element --------------------
------------------------------------------------------------------------*/
peDefPtr MakePasteUp(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
  int t;
  CARD16 inputs;
  peDefPtr ped;
  inFloPtr inFlo;
  pPasteUpDefPtr pvt;
  xieTypTile *rp;
  ELEMENT(xieFloPasteUp);
  ELEMENT_AT_LEAST_SIZE(xieFloPasteUp);
  ELEMENT_NEEDS_1_INPUT(numTiles);

  if ( flo->reqClient->swapped ) {
 	cpswaps(stuff->numTiles, inputs);
  } else
	inputs = stuff->numTiles;

  if(!(ped = MakePEDef((CARD32)inputs, (CARD32)stuff->elemLength<<2,
		       sizeof(pPasteUpDefRec))))
    FloAllocError(flo, tag, xieElemPasteUp, return(NULL));

  ped->diVec	     = &pPasteUpVec;
  ped->phototag      = tag;
  ped->flags.process = TRUE;
  raw = (xieFloPasteUp *)ped->elemRaw;
  rp  = (xieTypTile *) &(raw[1]);
  /*
   * copy the client element parameters (swap if necessary)
   */

  if( flo->reqClient->swapped ) {
    xieTypTile *sp = (xieTypTile *) &(stuff[1]);

    raw->elemType   = stuff->elemType;
    raw->elemLength = stuff->elemLength;
    /* We already did this one */
    raw->numTiles = inputs;
    cpswapl(stuff->width, raw->width);
    cpswapl(stuff->height, raw->height);
    cpswapl(stuff->constant0, raw->constant0);
    cpswapl(stuff->constant1, raw->constant1);
    cpswapl(stuff->constant2, raw->constant2);
    for (t = 0; t < inputs; t++) {
	cpswaps(sp[t].src,  rp[t].src);
	cpswapl(sp[t].dstX, rp[t].dstX);
	cpswapl(sp[t].dstY, rp[t].dstY);
    }
  }
  else
    memcpy((char *)raw, (char *)stuff, (CARD32)stuff->elemLength<<2);

  /*
   * convert constants
   */
  pvt = (pPasteUpDefPtr)ped->elemPvt;
  pvt->constant[0] = ConvertFromIEEE(raw->constant0);
  pvt->constant[1] = ConvertFromIEEE(raw->constant1);
  pvt->constant[2] = ConvertFromIEEE(raw->constant2);

  /*
   * assign phototags to inFlos
   */
  inFlo = ped->inFloLst;

  for (t = 0; t < inputs; t++) 
	inFlo[t].srcTag = rp[t].src;
  
  return(ped);
}                               /* end MakePasteUp */


/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/
static Bool PrepPasteUp(floDefPtr flo, peDefPtr ped)
{
  xieFloPasteUp *raw = (xieFloPasteUp *)ped->elemRaw;
  inFloPtr  in  = &ped->inFloLst[0];
  outFloPtr src = &in->srcDef->outFlo;
  outFloPtr dst = &ped->outFlo;
  int b, t;

 /* Grab a copy of the input attributes and propagate them to our output.
  * Use the first input as a template, all attributes must match except for  
  * width (and pitch) and height.
  */

  dst->bands = in->bands = src->bands;

  if (raw->numTiles <= 0)
      	SourceError(flo,ped, return(FALSE));

  for(b = 0; b < dst->bands; b++) {
	CARD32 bits;
	if (IsntCanonic(src->format[b].class))
      		MatchError(flo,ped,return(FALSE));
	dst->format[b] = in->format[b] = src->format[b];
	dst->format[b].width = bits = raw->width;
	dst->format[b].height = raw->height;
	bits *= dst->format[b].stride;
	dst->format[b].pitch = bits + Align(bits,PITCH_MOD);
  }

  /* Compare the remaining tiles to ensure all attibutes that must match do */
  for (t = 1; t < raw->numTiles; t++) {
	in = &ped->inFloLst[t];
  	src = &in->srcDef->outFlo;

	if (src->bands != dst->bands) {
      		MatchError(flo,ped,return(FALSE));
	} else
		in->bands = src->bands;

  	for(b = 0; b < dst->bands; b++) {
		formatRec *df  = &(dst->format[b]);
		formatRec *srf = &(src->format[b]);
		if ( srf->class  != df->class  ||
		     srf->depth  != df->depth  ||
		     srf->levels != df->levels ||
		     srf->stride != df->stride) {
      			MatchError(flo,ped,return(FALSE));
		     }	
		in->format[b] = src->format[b];
	}
  }

  return( TRUE );
}                               /* end PrepPasteUp */

/* end module ppaste.c */
