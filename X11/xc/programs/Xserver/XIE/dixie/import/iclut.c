/* $Xorg: iclut.c,v 1.4 2001/02/09 02:04:20 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module iclut.c ****/
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
  
	iclut.c -- DIXIE routines for managing the ImportClientLUT element
  
	Dean Verheiden 	-- AGE Logic, Inc. April 1993
	Ben Fahy 	-- AGE Logic, Inc. May   1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/import/iclut.c,v 3.5 2001/12/14 19:58:00 dawes Exp $ */

#define _XIEC_ICLUT

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
#include <dixie_i.h>
  /*
   *  Server XIE Includes
   */
#include <corex.h>
#include <error.h>
#include <macro.h>
#include <element.h>
#include <lut.h>

/*
 *  routines internal to this module
 */
static Bool PrepICLUT(floDefPtr flo, peDefPtr ped);

/*
 * dixie entry points
 */
static diElemVecRec iCLUTVec = {
    PrepICLUT			/* prepare for analysis and execution	*/
    };

/*------------------------------------------------------------------------
--------------- routine: make an import client lut element -------------
------------------------------------------------------------------------*/
peDefPtr MakeICLUT(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
  peDefPtr ped;
  ELEMENT(xieFloImportClientLUT);
  ELEMENT_SIZE_MATCH(xieFloImportClientLUT);
  
  if(!(ped = MakePEDef(1, (CARD32)stuff->elemLength<<2, 0)))
    FloAllocError(flo,tag,xieElemImportClientLUT,return(NULL));

  ped->diVec	     = &iCLUTVec;
  ped->phototag      = tag;
  ped->flags.import  = TRUE;
  ped->flags.putData = TRUE;
  raw = (xieFloImportClientLUT *)ped->elemRaw;
  /*
   * copy the client element parameters (swap if necessary)
   */
  if( flo->reqClient->swapped ) {
    raw->elemType   = stuff->elemType;
    raw->elemLength = stuff->elemLength;
    raw->class  = stuff->class;
    raw->bandOrder = stuff->bandOrder;
    cpswapl(stuff->length0, raw->length0);
    cpswapl(stuff->length1, raw->length1);
    cpswapl(stuff->length2, raw->length2);
    cpswapl(stuff->levels0, raw->levels0);
    cpswapl(stuff->levels1, raw->levels1);
    cpswapl(stuff->levels2, raw->levels2);
  }    
  else
    memcpy((char *)raw, (char *)stuff, sizeof(xieFloImportClientLUT));

  return(ped);
}                               /* end MakeICLUT */


/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/
static Bool PrepICLUT(floDefPtr flo, peDefPtr ped)
{
  xieFloImportClientLUT *raw = (xieFloImportClientLUT *)ped->elemRaw;
  inFloPtr inflo = &ped->inFloLst[IMPORT];
  int b;

  /*
   * check for data-class, length, and levels errors, and stash attributes
   * since this is STREAM data, we only have to record the class and band 
   * numbers in the inflos.
   */
  if(raw->bandOrder != xieValLSFirst && raw->bandOrder != xieValMSFirst)
    ValueError(flo,ped,raw->bandOrder, return(FALSE));

  switch(raw->class) {
  case xieValSingleBand :

    if(!raw->length0)
      ValueError(flo,ped,0, return(FALSE));
    if(raw->levels0 < 2 || raw->levels0 > MAX_LEVELS(1))
      MatchError(flo,ped, return(FALSE));
    inflo->bands = 1;
    break;
#if XIE_FULL
  case xieValTripleBand :
    if(!raw->length0 || !raw->length1 || !raw->length2)
      ValueError(flo,ped,0, return(FALSE));
    if(raw->levels0 < 2 || raw->levels0 > MAX_LEVELS(3) ||
       raw->levels1 < 2 || raw->levels1 > MAX_LEVELS(3) ||
       raw->levels2 < 2 || raw->levels2 > MAX_LEVELS(3))
      MatchError(flo,ped, return(FALSE));

    inflo->bands	  = 3;
    inflo->format[1].band = 1;
    inflo->format[2].band = 2;
    ped->outFlo.format[1] = inflo->format[1];
    ped->outFlo.format[2] = inflo->format[2];
    ped->outFlo.format[1].levels = raw->levels1;
    ped->outFlo.format[2].levels = raw->levels2;
    ped->outFlo.format[1].height = raw->length1;
    ped->outFlo.format[2].height = raw->length2;
    break;
#endif
  default :
    ValueError(flo,ped,raw->class, return(FALSE));
  }

  inflo->format[0].band = 0;
  ped->outFlo.format[0] = inflo->format[0];
  ped->outFlo.format[0].levels =  raw->levels0;
  ped->outFlo.format[0].height =  raw->length0;

  for (b=0; b < inflo->bands; b++) {
	formatPtr fmt = &(ped->outFlo.format[b]);

  	inflo->format[b].class  = STREAM;
	ped->swapUnits[b] = LutPitch(fmt->levels);

	fmt->class  = LUT_ARRAY;
	fmt->interleaved = FALSE;
	fmt->width  = raw->bandOrder; /* see miclut.c, mppoint crazypixel */
	fmt->depth  = 8;
	fmt->stride = 8;
	fmt->pitch  = 8 * fmt->height;
  }

  ped->outFlo.bands = inflo->bands;

  return(TRUE);
}                               /* end PrepICLUT */

/* end module iclut.c */
