/* $Xorg: ecphoto.c,v 1.4 2001/02/09 02:04:20 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module ecphoto.c ****/
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
  
	ecphoto.c -- DIXIE routines for managing the ExportClientPhoto element
  
	Dean Verheiden -- AGE Logic, Inc. June 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/export/ecphoto.c,v 3.6 2001/12/14 19:57:57 dawes Exp $ */

#define _XIEC_ECPHOTO

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
#include <dixie_e.h>
#include <XIEproto.h>
  /*
   *  Server XIE Includes
   */
#include <corex.h>
#include <error.h>
#include <macro.h>
#include <photomap.h>
#include <element.h>
#include <technq.h>
#include <memory.h>

/*
 *  routines internal to this module
 */
static Bool PrepECPhoto(floDefPtr flo, peDefPtr ped);

/*
 * dixie element entry points
 */
static diElemVecRec eCPhotoVec = {
  PrepECPhoto			/* prepare for analysis and execution	*/
  };


/*------------------------------------------------------------------------
--------------- routine: make an import client photo element -------------
------------------------------------------------------------------------*/
peDefPtr MakeECPhoto(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
  peDefPtr ped;
  ELEMENT(xieFloExportClientPhoto);
  ELEMENT_AT_LEAST_SIZE(xieFloExportClientPhoto);
  ELEMENT_NEEDS_1_INPUT(src);
  
  if(!(ped = MakePEDef(1, (CARD32)stuff->elemLength<<2, sizeof(ePhotoDefRec))))
    FloAllocError(flo,tag,xieElemExportClientPhoto, return(NULL)) ;

  ped->diVec	     = &eCPhotoVec;
  ped->phototag      = tag;
  ped->flags.getData = TRUE;
  ped->flags.export  = TRUE;
  raw = (xieFloExportClientPhoto *)ped->elemRaw;
  /*
   * copy the standard client element parameters (swap if necessary)
   */
  if( flo->reqClient->swapped ) {
    raw->elemType   = stuff->elemType;
    raw->elemLength = stuff->elemLength;
    cpswaps(stuff->src,  raw->src);
    raw->notify = stuff->notify;
    cpswaps(stuff->encodeTechnique, raw->encodeTechnique);
    cpswaps(stuff->lenParams, raw->lenParams);
  }
  else
    memcpy((char *)raw, (char *)stuff, sizeof(xieFloExportClientPhoto));

  /* copy technique data (if any)
   */
  if(!(ped->techVec = FindTechnique(xieValEncode,raw->encodeTechnique)) 
    || !(ped->techVec->copyfnc(flo, ped, &stuff[1], &raw[1], raw->lenParams, 0)))
    TechniqueError(flo,ped,xieValEncode,raw->encodeTechnique,raw->lenParams,
		   return(ped));

  /* assign phototag to inFlo
   */
  ped->inFloLst[SRCtag].srcTag = raw->src;

  return(ped);
}                               /* end MakeECPhoto */

#undef  sparms
#define sparms ((xieTecEncodeUncompressedSingle *)sParms)
#undef  rparms
#define rparms ((xieTecEncodeUncompressedSingle *)rParms)

Bool CopyECPhotoUnSingle(TECHNQ_COPY_ARGS)
{
  VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, FALSE);
  
  /* Nothing to swap for this technique */
  memcpy((char *)rparms, (char *)sparms, tsize<<2);
  
  return(TRUE);
}

#undef  sparms
#define sparms ((xieTecEncodeG31D *)sParms)
#undef  rparms
#define rparms ((xieTecEncodeG31D *)rParms)

Bool CopyECPhotoG31D(TECHNQ_COPY_ARGS)
{
  VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, FALSE);
  
  /* Nothing to swap for this technique */
  memcpy((char *)rparms, (char *)sparms, tsize<<2);
  
  return(TRUE);
}

#undef  sparms
#define sparms ((xieTecEncodeG32D *)sParms)
#undef  rparms
#define rparms ((xieTecEncodeG32D *)rParms)

Bool CopyECPhotoG32D(TECHNQ_COPY_ARGS)
{
  VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, FALSE);
  
  if( flo->reqClient->swapped ) {
    rparms->radiometric  = sparms->radiometric;
    rparms->uncompressed = sparms->uncompressed;
    rparms->alignEol     = sparms->alignEol;
    rparms->encodedOrder = sparms->encodedOrder;
    cpswapl(sparms->kFactor, rparms->kFactor);
  } else
    memcpy((char *)rparms, (char *)sparms, tsize<<2);
  
  return(TRUE);
}

#undef  sparms
#define sparms ((xieTecEncodeG42D *)sParms)
#undef  rparms
#define rparms ((xieTecEncodeG42D *)rParms)

Bool CopyECPhotoG42D(TECHNQ_COPY_ARGS)
{
  VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, FALSE);
  
  /* Nothing to swap for this technique */
  memcpy((char *)rparms, (char *)sparms, tsize<<2);
  
  return(TRUE);
}

#undef  sparms
#define sparms ((xieTecEncodeTIFF2 *)sParms)
#undef  rparms
#define rparms ((xieTecEncodeTIFF2 *)rParms)

Bool CopyECPhotoTIFF2(TECHNQ_COPY_ARGS)
{
  VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, FALSE);
  
  /* Nothing to swap for this technique */
  memcpy((char *)rparms, (char *)sparms, tsize<<2);
  
  return(TRUE);
}

#undef  sparms
#define sparms ((xieTecEncodeTIFFPackBits *)sParms)
#undef  rparms
#define rparms ((xieTecEncodeTIFFPackBits *)rParms)

Bool CopyECPhotoTIFFPackBits(TECHNQ_COPY_ARGS)
{
  VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, FALSE);
  
  /* Nothing to swap for this technique */
  memcpy((char *)rparms, (char *)sparms, tsize<<2);
  
  return(TRUE);
}

#undef  sparms
#define sparms ((xieTecEncodeUncompressedTriple *)sParms)
#undef  rparms
#define rparms ((xieTecEncodeUncompressedTriple *)rParms)

#if XIE_FULL
Bool CopyECPhotoUnTriple(TECHNQ_COPY_ARGS)
{
  VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, FALSE);
  
  /* Nothing to swap for this technique */
  memcpy((char *)rparms, (char *)sparms, tsize<<2);
  
  return(TRUE);
}

#undef  sparms
#define sparms ((xieTecEncodeJPEGBaseline *)sParms)
#undef  rparms
#define rparms ((xieTecEncodeJPEGBaseline *)rParms)

Bool CopyECPhotoJPEGBaseline(TECHNQ_COPY_ARGS)
{
  eTecEncodeJPEGBaselineDefPtr pvt;
  VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, FALSE);
  
  memcpy((char *)rparms, (char *)sparms, tsize<<2);
  
  if( flo->reqClient->swapped ) {
    register int n;
    swaps(&rparms->lenQtable,n);
    swaps(&rparms->lenACtable,n);
    swaps(&rparms->lenDCtable,n);
  }
  if(rparms->lenQtable  & 3 ||
     rparms->lenACtable & 3 ||
     rparms->lenDCtable & 3) 
    return(FALSE);
  
  if(!(ped->techPvt=(pointer)XieMalloc(sizeof(eTecEncodeJPEGBaselineDefRec))))
    FloAllocError(flo, ped->phototag, xieElemExportClientPhoto, return(TRUE));
  
  pvt = (eTecEncodeJPEGBaselineDefPtr)ped->techPvt;
  pvt->q = (rparms->lenQtable
            ? (CARD8 *)rparms + sizeof(xieTecEncodeJPEGBaseline)
            : (CARD8 *) NULL);
  pvt->a = (rparms->lenACtable
            ? (CARD8 *)rparms + sizeof(xieTecEncodeJPEGBaseline)
	    + rparms->lenQtable
            : (CARD8 *) NULL);
  pvt->d = (rparms->lenDCtable
            ? (CARD8 *)rparms + sizeof(xieTecEncodeJPEGBaseline)
            + rparms->lenQtable + rparms->lenACtable
            : (CARD8 *) NULL);
  return(TRUE);
}

#undef  sparms
#define sparms ((xieTecEncodeJPEGLossless *)sParms)
#undef  rparms
#define rparms ((xieTecEncodeJPEGLossless *)rParms)

#ifdef BEYOND_SI
Bool CopyECPhotoJPEGLossless(TECHNQ_COPY_ARGS)
{
  VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, FALSE);

  memcpy((char *)rparms, (char *)sparms, tsize<<2);

  if( flo->reqClient->swapped ) {
    register int n;
    swaps(&rparms->lenTable,n);
  }
  return(TRUE);
}
#endif /* BEYOND_SI */
#endif

#undef  sparms
#undef  rparms


/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/
static Bool PrepECPhoto(floDefPtr flo, peDefPtr ped)
{
  xieFloExportClientPhoto *raw = (xieFloExportClientPhoto *)ped->elemRaw;
  ePhotoDefPtr             pvt = (ePhotoDefPtr)ped->elemPvt;
  inFloPtr                 inf = &ped->inFloLst[SRCtag];
  outFloPtr                src = &inf->srcDef->outFlo;
  outFloPtr                dst = &ped->outFlo;
  int b;
  
  /* Make sure notify value is valid
   */ 
  if(raw->notify != xieValDisable   &&
     raw->notify != xieValFirstData &&
     raw->notify != xieValNewData)
    ValueError(flo,ped,raw->notify, return(FALSE));
		
  pvt->congress = FALSE;
  dst->bands = inf->bands = src->bands;
  for(b = 0; b < src->bands; b++) {
    if (IsntConstrained(src->format[b].class))
      MatchError(flo, ped, return(FALSE));
    dst->format[b] = inf->format[b] = src->format[b];
  }
  if(!(ped->techVec->prepfnc(flo, ped, &raw[1])))
    TechniqueError(flo,ped,xieValEncode,raw->encodeTechnique,raw->lenParams,
                   return(FALSE));

  pvt->encodeNumber = raw->encodeTechnique;
  pvt->encodeLen    = raw->lenParams << 2;
  pvt->encodeParms  = (pointer)&raw[1];

  if(ped->inFloLst[IMPORT].srcDef->flags.import) {
    /*
     * see if import data can leap-frog the import and export elements
     */
    if(BuildDecodeFromEncode(flo,ped) && CompareDecode(flo,ped)) {
      inFloPtr import = &inf->srcDef->inFloLst[IMPORT];

      inf->bands = import->bands;
      for(b = 0; b < import->bands; ++b)
	inf->format[b] = import->format[b];
      pvt->congress = TRUE;
    }
    if(pvt->decodeParms)
       pvt->decodeParms = (pointer)XieFree(pvt->decodeParms);
  }
  return(TRUE);
}                               /* end PrepECPhoto */


/*------------------------------------------------------------------------
----- routines: verify technique parameters against element parameters ----
-------------- and prepare for analysis and execution                 ----
------------------------------------------------------------------------*/

/* Prep routine for uncompressed single band data */
Bool PrepECPhotoUnSingle(
     floDefPtr flo,
     peDefPtr  ped,
     xieTecEncodeUncompressedSingle *tec)
{
  outFloPtr dst = &ped->outFlo;
  CARD32 padmod =  tec->scanlinePad * 8;
  CARD32 pitch  =  tec->pixelStride * dst->format[0].width;
  BOOL  aligned = !(tec->pixelStride & (tec->pixelStride-1)) ||
    tec->pixelStride == 24;
  
  if(tec->fillOrder  != xieValLSFirst &&	    /* check fill-order     */
     tec->fillOrder  != xieValMSFirst)
    return(FALSE);
  if(tec->pixelOrder != xieValLSFirst &&	    /* check pixel-order    */
     tec->pixelOrder != xieValMSFirst)
    return(FALSE);
  if(tec->pixelStride < dst->format[0].depth)       /* check pixel-stride   */
    return(FALSE);
  if(ALIGNMENT == xieValAlignable &&		    /* scanline alignment   */
     !tec->scanlinePad && !aligned)
    return(FALSE);
  if(tec->scanlinePad & (tec->scanlinePad-1) ||     /* check scanline-pad   */
     tec->scanlinePad > 16)
    return(FALSE);
  
  dst->format[0].interleaved = FALSE;
  dst->format[0].class       = STREAM;
  dst->format[0].stride      = tec->pixelStride;
  dst->format[0].pitch       = pitch + (padmod ? Align(pitch,padmod) : 0);
  
  return(TRUE);
} /* PrepECPhotoUnSingle */

Bool PrepECPhotoG31D(
     floDefPtr flo,
     peDefPtr  ped,
     xieTecEncodeG31D *tec)
{
  outFloPtr dst = &ped->outFlo;
  
  if(tec->encodedOrder  != xieValLSFirst &&	    /* check encoding-order  */
     tec->encodedOrder  != xieValMSFirst)
    return(FALSE);
  
  dst->format[0].interleaved = FALSE;
  dst->format[0].class  = STREAM;
  
  return(TRUE);
  
} /* PrepECPhotoG31D */

Bool PrepECPhotoG32D(
     floDefPtr flo,
     peDefPtr  ped,
     xieTecEncodeG32D *tec)
{
  outFloPtr dst = &ped->outFlo;
  
  if(tec->encodedOrder  != xieValLSFirst &&	    /* check encoding-order  */
     tec->encodedOrder  != xieValMSFirst)
    return(FALSE);
  
  dst->format[0].interleaved = FALSE;
  dst->format[0].class  = STREAM;
  
  return(TRUE);
  
} /* PrepECPhotoG32D */

Bool PrepECPhotoG42D(
     floDefPtr flo,
     peDefPtr  ped,
     xieTecEncodeG42D *tec)
{
  outFloPtr dst = &ped->outFlo;
  
  if(tec->encodedOrder  != xieValLSFirst &&	    /* check encoding-order  */
     tec->encodedOrder  != xieValMSFirst)
    return(FALSE);
  
  dst->format[0].interleaved = FALSE;
  dst->format[0].class  = STREAM;
  
  return(TRUE);
  
} /* PrepECPhotoG42D */

Bool PrepECPhotoTIFF2(
     floDefPtr flo,
     peDefPtr  ped,
     xieTecEncodeTIFF2 *tec)
{
  outFloPtr dst = &ped->outFlo;
  
  if(tec->encodedOrder  != xieValLSFirst &&	    /* check encoding-order  */
     tec->encodedOrder  != xieValMSFirst)
    return(FALSE);
  
  dst->format[0].interleaved = FALSE;
  dst->format[0].class  = STREAM;
  
  return(TRUE);
  
} /* PrepECPhotoTIFF2 */

Bool PrepECPhotoTIFFPackBits(
     floDefPtr flo,
     peDefPtr  ped,
     xieTecEncodeTIFFPackBits *tec)
{
  outFloPtr dst = &ped->outFlo;
  
  if(tec->encodedOrder  != xieValLSFirst &&	    /* check encoding-order  */
     tec->encodedOrder  != xieValMSFirst)
    return(FALSE);
  
  dst->format[0].interleaved = FALSE;
  dst->format[0].class  = STREAM;
  
  return(TRUE);
} /* PrepECPhotoTIFFPackBits */
  
#if XIE_FULL
/* Prep routine for uncompressed triple band data */
Bool PrepECPhotoUnTriple(
     floDefPtr flo,
     peDefPtr  ped,
     xieTecEncodeUncompressedTriple *tec)
{
  outFloPtr dst = &ped->outFlo;
  int b;
  
  if(tec->fillOrder  != xieValLSFirst &&	    /* check fill-order     */
     tec->fillOrder  != xieValMSFirst)
    return(FALSE);
  if(tec->pixelOrder != xieValLSFirst &&	    /* check pixel-order    */
     tec->pixelOrder != xieValMSFirst)
    return(FALSE);
  if(tec->bandOrder  != xieValLSFirst &&	    /* check band-order     */
     tec->bandOrder  != xieValMSFirst)
    return(FALSE);
  if(tec->interleave != xieValBandByPixel &&	    /* check interleave     */
     tec->interleave != xieValBandByPlane)
    return(FALSE);
  if (tec->interleave == xieValBandByPixel && 	    /* check inter-band dim */
      (dst->format[0].width  != dst->format[1].width   ||
       dst->format[1].width  != dst->format[2].width   ||
       dst->format[0].height != dst->format[1].height  ||
       dst->format[1].height != dst->format[2].height))
    return(FALSE);
  if (tec->interleave == xieValBandByPixel) {
     CARD32  padmod =   tec->scanlinePad[0] * 8;
     CARD32   pitch =   tec->pixelStride[0] * dst->format[0].width;

     if (tec->pixelStride[0] <
         dst->format[0].depth + dst->format[1].depth + dst->format[2].depth ||
	 dst->format[0].depth > 16 ||
	 dst->format[1].depth > 16 ||
	 dst->format[2].depth > 16)
        return(FALSE);
      if(tec->scanlinePad[0] & (tec->scanlinePad[0]-1) || /*check scanln-pad*/
         tec->scanlinePad[0] > 16)
           return(FALSE);

      dst->bands = 1;
      dst->format[0].interleaved = TRUE;
      dst->format[0].class  = STREAM;
      dst->format[0].stride = tec->pixelStride[0];
      dst->format[0].pitch  = pitch + (padmod ? Align(pitch,padmod) : 0);
  } else {
      if(tec->pixelStride[0] < dst->format[0].depth || /* check pixel-stride */
         tec->pixelStride[1] < dst->format[1].depth || 
         tec->pixelStride[2] < dst->format[2].depth)
       return(FALSE);
      for (b = 0; b < 3; b++) {
        CARD32  padmod =   tec->scanlinePad[b] * 8;
        CARD32   pitch =   tec->pixelStride[b] * dst->format[b].width;
    
        if(dst->format[b].depth > 16)  /* check pixel-depth   */
          return(FALSE);
        if(tec->scanlinePad[b] & (tec->scanlinePad[b]-1) || /*check scanln-pad*/
           tec->scanlinePad[b] > 16)
           return(FALSE);
    
        dst->format[b].interleaved = FALSE;
        dst->format[b].class  = STREAM;
        dst->format[b].stride = tec->pixelStride[b];
        dst->format[b].pitch  = pitch + (padmod ? Align(pitch,padmod) : 0);
      }
  }
  
  return(TRUE);
} /* PrepECPhotoUnTriple */

Bool PrepECPhotoJPEGBaseline(
     floDefPtr flo,
     peDefPtr  ped,
     xieTecEncodeJPEGBaseline *tec)
{
  outFloPtr dst = &ped->outFlo;
  
  if (dst->bands == 1) 
    dst->format[0].interleaved = FALSE;
  else {
    if(tec->bandOrder  != xieValLSFirst &&	   /* check encoding-order  */
       tec->bandOrder  != xieValMSFirst)
      return(FALSE);
    
    if(tec->interleave != xieValBandByPixel && /* check interleave     */
       tec->interleave != xieValBandByPlane)
      return(FALSE);
    
    dst->format[0].interleaved =
      dst->format[1].interleaved =
	dst->format[2].interleaved =
	  (tec->interleave == xieValBandByPixel);

    if (tec->interleave == xieValBandByPixel)
      dst->bands = 1;
    else {
      dst->format[1].class  = STREAM;
      dst->format[2].class  = STREAM;
    }
  }
  
  dst->format[0].class  = STREAM;
  
  return(TRUE);
  
} /* PrepECPhotoJPEGBaseline */

#ifdef BEYOND_SI
Bool PrepECPhotoJPEGLossless(
     floDefPtr flo,
     peDefPtr  ped,
     xieTecEncodeJPEGLossless *tec)
{
  outFloPtr dst = &ped->outFlo;
  CARD8 pred;
  
  if ((pred = tec->predictor[0]) != xieValPredictorNone &&
      pred != xieValPredictorA    &&
      pred != xieValPredictorB    &&
      pred != xieValPredictorC    &&
      pred != xieValPredictorABC  &&
      pred != xieValPredictorABC2 &&
      pred != xieValPredictorBAC2 &&
      pred != xieValPredictorAB2)
    return(FALSE);
  
  if (dst->bands == 1) 
    dst->format[0].interleaved = FALSE;
  else {
    if(tec->bandOrder  != xieValLSFirst &&	    /* check encoding-order  */
       tec->bandOrder  != xieValMSFirst)
      return(FALSE);
    
    if(tec->interleave != xieValBandByPixel && /* check interleave     */
       tec->interleave != xieValBandByPlane)
      return(FALSE);
    
    if ((pred = tec->predictor[1]) != xieValPredictorNone &&
	pred != xieValPredictorA    &&
	pred != xieValPredictorB    &&
	pred != xieValPredictorC    &&
	pred != xieValPredictorABC  &&
	pred != xieValPredictorABC2 &&
	pred != xieValPredictorBAC2 &&
	pred != xieValPredictorAB2)
      return(FALSE);
    
    if ((pred = tec->predictor[2]) != xieValPredictorNone &&
	pred != xieValPredictorA    &&
	pred != xieValPredictorB    &&
	pred != xieValPredictorC    &&
	pred != xieValPredictorABC  &&
	pred != xieValPredictorABC2 &&
	pred != xieValPredictorBAC2 &&
	pred != xieValPredictorAB2)
      return(FALSE);
    
    dst->format[0].interleaved = 
      dst->format[1].interleaved  =
	dst->format[2].interleaved  =
	  (tec->interleave == xieValBandByPixel);

    if (tec->interleave == xieValBandByPixel)
      dst->bands = 1;
    else {
      dst->format[1].class  = STREAM;
      dst->format[2].class  = STREAM;
    }
  }
  
  dst->format[0].class  = STREAM;
  
  return(TRUE);
  
} /* PrepECPhotoJPEGLossless */
#endif /* BEYOND_SI */
#endif

/* end module ecphoto.c */
