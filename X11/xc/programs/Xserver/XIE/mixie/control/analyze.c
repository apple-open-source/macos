/* $Xorg: analyze.c,v 1.4 2001/02/09 02:04:23 xorgcvs Exp $ */
/**** module analyze.c ****/
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
  
	analyze.c -- DDXIE prototype (simple minded) DAG analyzer
  
	Robert NC Shelley -- AGE Logic, Inc. April, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/control/analyze.c,v 1.5 2001/12/14 19:58:16 dawes Exp $ */

#define _XIEC_ANALYZE

/*
 *  Include files
 */

/*
 *  Core X Includes
 */
#include <X.h>
#include <Xproto.h>
/*
 *  XIE Includes
 */
#include <XIE.h>
#include <XIEproto.h>
/*
 *  more X server includes.
 */
#include <misc.h>
#include <dixstruct.h>
/*
 *  Server XIE Includes
 */
#include <error.h>
#include <macro.h>
#include <flostr.h>
#include <texstr.h>


/*
 *  routines called from DIXIE
 */
int DAGalyze();


/*------------------------------------------------------------------------
----------------------- analyze (sort of) the DAG ------------------------
------------------------------------------------------------------------*/
int DAGalyze(flo)
     floDefPtr flo;
{
  int ok = TRUE;
  peDefPtr ped;
  pedLstPtr lst = ListEmpty(&flo->optDAG) ? &flo->defDAG : &flo->optDAG;
  
  /* establish our default flo manager
   */
  InitFloManager(flo);

  /* choose element handlers
   */
  for(ped = lst->flink; ok && !ListEnd(ped,lst); ped = ped->flink)
    switch(ped->elemRaw->elemType) {
#if XIE_FULL
      case xieElemImportClientLUT:   ok = miAnalyzeICLUT(flo,ped);       break;
      case xieElemImportClientPhoto: ok = miAnalyzeICPhoto(flo,ped);     break;
      case xieElemImportClientROI:   ok = miAnalyzeICROI(flo,ped);       break;
      case xieElemImportDrawable:    ok = miAnalyzeIDraw(flo,ped);       break;
      case xieElemImportDrawablePlane:ok = miAnalyzeIDrawP(flo,ped);     break;
      case xieElemImportLUT:         ok = miAnalyzeILUT(flo,ped);        break;
      case xieElemImportPhotomap:    ok = miAnalyzeIPhoto(flo,ped);      break;
      case xieElemImportROI:         ok = miAnalyzeIROI(flo,ped);        break;
      case xieElemArithmetic:	     ok = miAnalyzeArith(flo,ped);       break;
      case xieElemBandCombine:	     ok = miAnalyzeBandCom(flo,ped);     break;
      case xieElemBandExtract:	     ok = miAnalyzeBandExt(flo,ped);     break;
      case xieElemBandSelect:	     ok = miAnalyzeBandSel(flo,ped);     break;
      case xieElemBlend:	     ok = miAnalyzeBlend(flo,ped);       break;
      case xieElemCompare:	     ok = miAnalyzeCompare(flo,ped);     break;
      case xieElemConstrain:	     ok = miAnalyzeConstrain(flo,ped);   break;
      case xieElemConvertFromIndex:  ok = miAnalyzeCvtFromInd(flo,ped);  break;
      case xieElemConvertFromRGB:    ok = miAnalyzeFromRGB(flo,ped);     break;
      case xieElemConvertToIndex:    ok = miAnalyzeCvtToInd(flo,ped);    break;
      case xieElemConvertToRGB:      ok = miAnalyzeToRGB(flo,ped);       break;
      case xieElemConvolve:	     ok = miAnalyzeConvolve(flo,ped);    break;
      case xieElemDither:	     ok = miAnalyzeDither(flo,ped);      break;
      case xieElemGeometry:	     ok = miAnalyzeGeometry(flo,ped);    break;
      case xieElemLogical:	     ok = miAnalyzeLogic(flo,ped);       break;
      case xieElemMatchHistogram:    ok = miAnalyzeMatchHist(flo,ped);   break;
      case xieElemMath:              ok = miAnalyzeMath(flo,ped);        break;
      case xieElemPasteUp:           ok = miAnalyzePasteUp(flo,ped);     break;
      case xieElemPoint:	     ok = miAnalyzePoint(flo,ped);       break;
      case xieElemUnconstrain:       ok = miAnalyzeUnconstrain(flo,ped); break;
      case xieElemExportClientHistogram:ok = miAnalyzeECHist(flo,ped);   break;
      case xieElemExportClientLUT:   ok = miAnalyzeECLUT(flo,ped);       break;
      case xieElemExportClientPhoto: ok = miAnalyzeECPhoto(flo,ped);     break;
      case xieElemExportClientROI:   ok = miAnalyzeECROI(flo,ped);       break;
      case xieElemExportDrawable:    ok = miAnalyzeEDraw(flo,ped);       break;
      case xieElemExportDrawablePlane:ok = miAnalyzeEDrawP(flo,ped);     break;
      case xieElemExportLUT:         ok = miAnalyzeELUT(flo,ped);        break;
      case xieElemExportPhotomap:    ok = miAnalyzeEPhoto(flo,ped);      break;
      case xieElemExportROI:         ok = miAnalyzeEROI(flo,ped);        break;
#else
      case xieElemImportClientLUT:   ok = miAnalyzeICLUT(flo,ped);       break;
      case xieElemImportClientPhoto: ok = miAnalyzeICPhoto(flo,ped);     break;
      case xieElemImportDrawable:    ok = miAnalyzeIDraw(flo,ped);       break;
      case xieElemImportDrawablePlane:ok = miAnalyzeIDrawP(flo,ped);     break;
      case xieElemImportLUT:         ok = miAnalyzeILUT(flo,ped);        break;
      case xieElemImportPhotomap:    ok = miAnalyzeIPhoto(flo,ped);      break;
      case xieElemGeometry:	     ok = miAnalyzeGeometry(flo,ped);    break;
      case xieElemPoint:	     ok = miAnalyzePoint(flo,ped);       break;
      case xieElemExportClientLUT:   ok = miAnalyzeECLUT(flo,ped);       break;
      case xieElemExportClientPhoto: ok = miAnalyzeECPhoto(flo,ped);     break;
      case xieElemExportDrawable:    ok = miAnalyzeEDraw(flo,ped);       break;
      case xieElemExportDrawablePlane:ok = miAnalyzeEDrawP(flo,ped);     break;
      case xieElemExportLUT:         ok = miAnalyzeELUT(flo,ped);        break;
      case xieElemExportPhotomap:    ok = miAnalyzeEPhoto(flo,ped);      break;
#endif
      default: ElementError(flo,ped, return(FALSE));
    }
  return(ok);
}                               /* end DAGalyze */

/* end module analyze.c */
