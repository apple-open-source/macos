/* $Xorg: mephoto.c,v 1.4 2001/02/09 02:04:24 xorgcvs Exp $ */
/**** module mephoto.c ****/
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
  
	mephoto.c -- DDXIE prototype export photomap element
  
	Robert NC Shelley -- AGE Logic, Inc. June, 1993
	Dean && Ben - various additions to handle different techniques
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/export/mephoto.c,v 3.6 2001/12/14 19:58:20 dawes Exp $ */

#define _XIEC_MEPHOTO
#define _XIEC_EPHOTO

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
#include <photomap.h>
#include <element.h>
#include <texstr.h>
#include <xiemd.h>
#include <meuncomp.h>
#include <memory.h>

/*
 *  routines used internal to this module
 */
static int CreateEPhotoUncom(floDefPtr flo, peDefPtr ped);
static int InitializeEPhotoBypass(floDefPtr flo, peDefPtr ped);
static int InitializeEPhotoUncomByPlane(floDefPtr flo, peDefPtr ped);
static int ActivateEPhotoUncomByPlane(floDefPtr flo, peDefPtr ped, peTexPtr pet);
static int ResetEPhoto(floDefPtr flo, peDefPtr ped);
static int DestroyEPhoto(floDefPtr flo, peDefPtr ped);

#if XIE_FULL
static int InitializeEPhotoUncomByPixel(floDefPtr flo, peDefPtr ped);
static int ActivateEPhotoUncomByPixel(floDefPtr flo, peDefPtr ped, peTexPtr pet);
#endif /* XIE_FULL */


/*
 * routines we need from somewhere else
 */

extern int CreateEPhotoJPEGBaseline();
extern int InitializeEPhotoJPEGBaseline();
extern int ActivateEPhotoJPEGBaseline();
extern int ResetEPhotoJPEGBaseline();
extern int DestroyEPhotoJPEGBaseline();

extern int CreateEPhotoFAX();
extern int InitializeEPhotoFAX();
extern int ActivateEPhotoFAX();
extern int ResetEPhotoFAX();
extern int DestroyEPhotoFAX();

/*
 * DDXIE ExportPhotomap entry points
 */
static ddElemVecRec EPhotoBypassVec = {
  CreateEPhotoUncom,
  InitializeEPhotoBypass,
  (xieIntProc)NULL,
  (xieIntProc)NULL,
  ResetEPhoto,
  DestroyEPhoto
  };

static ddElemVecRec EPhotoUncomByPlaneVec = {
  CreateEPhotoUncom,
  InitializeEPhotoUncomByPlane,
  ActivateEPhotoUncomByPlane,
  (xieIntProc)NULL,
  ResetEPhoto,
  DestroyEPhoto
  };

static ddElemVecRec EPhotoFAXVec = {
  CreateEPhotoFAX,
  InitializeEPhotoFAX,
  ActivateEPhotoFAX,
  (xieIntProc)NULL,
  ResetEPhotoFAX,
  DestroyEPhotoFAX
  };

#if XIE_FULL
static ddElemVecRec EPhotoUncomByPixelVec = {
  CreateEPhotoUncom,
  InitializeEPhotoUncomByPixel,
  ActivateEPhotoUncomByPixel,
  (xieIntProc)NULL,
  ResetEPhoto,
  DestroyEPhoto
  };

static ddElemVecRec EPhotoJPEGBaselineVec = {
  CreateEPhotoJPEGBaseline,
  InitializeEPhotoJPEGBaseline,
  ActivateEPhotoJPEGBaseline,
  (xieIntProc)NULL,
  ResetEPhotoJPEGBaseline,
  DestroyEPhotoJPEGBaseline
  };
#endif /* XIE_FULL */


/*------------------------------------------------------------------------
------------------- see if we can handle this element --------------------
------------------------------------------------------------------------*/
int miAnalyzeEPhoto(floDefPtr flo, peDefPtr ped)
{
  ePhotoDefPtr pvt = (ePhotoDefPtr)ped->elemPvt;

  if(pvt->congress) {
    ped->ddVec = EPhotoBypassVec;
    return(TRUE);
  }
  switch(pvt->encodeNumber) {

  case xieValEncodeUncompressedSingle:
    ped->ddVec = EPhotoUncomByPlaneVec;
    break;
      
  case xieValEncodeG31D:
  case xieValEncodeG32D:
  case xieValEncodeG42D:
  case xieValEncodeTIFF2:
  case xieValEncodeTIFFPackBits:
    ped->ddVec = EPhotoFAXVec;
    break;
      
#if XIE_FULL
  case xieValEncodeUncompressedTriple:
    {
        xieTecEncodeUncompressedTriple *tecParms = 
	         (xieTecEncodeUncompressedTriple *)pvt->encodeParms;
        if(tecParms->interleave == xieValBandByPlane)
          ped->ddVec = EPhotoUncomByPlaneVec;
        else
          ped->ddVec = EPhotoUncomByPixelVec;
    }
    break;
  case xieValEncodeJPEGBaseline:
    { /*** JPEG for SI can only handle 8 bit image depths ***/
      inFloPtr inf  = &ped->inFloLst[IMPORT];
      outFloPtr src = &inf->srcDef->outFlo;
      int b;
      for (b=0; b< src->bands; ++b) 
        if(src->format[b].depth != 8) {
  	  xieFloExportPhotomap *raw = (xieFloExportPhotomap *)ped->elemRaw;

	  TechniqueError(flo, ped, xieValEncode,
	                 raw->encodeTechnique, raw->lenParams, return(FALSE));
	}
    }
    ped->ddVec = EPhotoJPEGBaselineVec;
    break;
  case xieValEncodeJPEGLossless:
#endif /* XIE_FULL */
  default:  ImplementationError(flo,ped, return(FALSE));
  }
  return(TRUE);
}                               /* end miAnalyzeEPhoto */

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateEPhotoUncom(floDefPtr flo, peDefPtr ped)
{
  /* attach an execution context to the photo element definition */
  return(MakePETex(flo, ped, xieValMaxBands * sizeof(meUncompRec), 
				NO_SYNC, NO_SYNC));
}                               /* end CreateEPhotoUncom */

/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeEPhotoBypass(floDefPtr flo, peDefPtr ped)
{
  /* Allows data manager to bypass element entirely */
  return(InitReceptor(flo, ped, ped->peTex->receptor,
		      NO_DATAMAP, 1, NO_BANDS, ALL_BANDS));
}                               /* end InitializeEPhotoBypass */

/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeEPhotoUncomByPlane(floDefPtr flo, peDefPtr ped)
{
  peTexPtr              pet = ped->peTex;
  meUncompPtr           pvt = (meUncompPtr)pet->private;
  formatPtr             inf = ped->inFloLst[SRCtag].format;
  bandPtr	       sbnd = pet->receptor->band;
  CARD32	     nbands = ped->outFlo.bands, b, s;
  xieTypOrientation pixelOrder, fillOrder;
  Bool inited = FALSE;
  
  if (nbands == 1) {
    xieTecEncodeUncompressedSingle *tecParms = 
				(xieTecEncodeUncompressedSingle *)
				((ePhotoDefPtr)ped->elemPvt)->encodeParms;
    pixelOrder = tecParms->pixelOrder;
    fillOrder  = tecParms->fillOrder;
    pvt[0].bandMap = 0;
  } else {
    xieTecEncodeUncompressedTriple *tecParms = 
				(xieTecEncodeUncompressedTriple *)
				((ePhotoDefPtr)ped->elemPvt)->encodeParms;
    pixelOrder = tecParms->pixelOrder;
    fillOrder  = tecParms->fillOrder;
    if(tecParms->bandOrder == xieValLSFirst) 
      for(b = 0; b < xieValMaxBands; ++b)
	pvt[b].bandMap = b;
    else 
      for(s = 0, b = xieValMaxBands; b--; ++s)
	pvt[s].bandMap = b;
  }
  for (b = 0; b < nbands; b++, pvt++, inf++, sbnd++) {
    bandPtr   dbnd = &pet->emitter[pvt->bandMap];
    formatPtr outf = &ped->outFlo.format[pvt->bandMap];
    CARD8 class    = inf->class;
    if (class == BIT_PIXEL) {
          pvt->width  = inf->width;
#if (IMAGE_BYTE_ORDER == MSBFirst)
          if (outf->stride != 1) {
	      pvt->action = (fillOrder == xieValMSFirst) ? btoIS: sbtoIS;
          } else {
      	      pvt->action = (fillOrder == xieValMSFirst) ? 
						(void (*)())NULL : sbtoS;
          }
#else
          if (outf->stride != 1) {
	      pvt->action = (fillOrder == xieValLSFirst) ? btoIS: sbtoIS;
          } else {
      	      pvt->action = (fillOrder == xieValLSFirst) ? 
						(void (*)())NULL : sbtoS;
          }
#endif
          pvt->stride = outf->stride;
          pvt->pitch  = outf->pitch;
    } else if (class == BYTE_PIXEL)  {
          pvt->width = inf->width;
	  if (!(outf->stride & 7)) {
	      if (outf->stride == 8)
	      	  pvt->action = BtoS;
	      else
	      	  pvt->action = BtoIS;
              pvt->Bstride   = outf->stride >> 3;
	      pvt->dstoffset = 0;
	      pvt->mask      = 0; /* Unused */
	      pvt->shift     = 0; /* Unused */
	      pvt->clear_dst = FALSE;
	  } else {
	      if (pixelOrder == xieValLSFirst) {
		  if (fillOrder == xieValLSFirst) 
	              pvt->action = BtoLLUB;
	          else 
	              pvt->action = BtoLMUB;
	      } else {
		  if (fillOrder == xieValLSFirst) 
	              pvt->action = BtoMLUB;
	          else 
	              pvt->action = BtoMMUB;
	      }	
              pvt->bitOff   = 0;	/* Bit offset to first pixel on line */
              pvt->leftOver = 0;	/* Left over bits from last line     */
              pvt->depth    = inf->depth;
              pvt->stride   = outf->stride;
              pvt->pitch    = outf->pitch;
	  } 
    } else if (class == PAIR_PIXEL) {
          pvt->width = inf->width;
	  if (!(outf->stride & 15)) {
#if (IMAGE_BYTE_ORDER == LSBFirst)
	      if (outf->stride == 16)
	      	  pvt->action = (fillOrder == xieValLSFirst) ? 
						(void (*)())NULL : sPtoS;
	      else
	      	  pvt->action = (fillOrder == xieValLSFirst) ? PtoIS: sPtoIS;
#else
	      if (outf->stride == 16)
	      	  pvt->action = (fillOrder == xieValMSFirst) ? 
						(void (*)())NULL : sPtoS;
	      else
	      	  pvt->action = (fillOrder == xieValMSFirst) ? PtoIS: sPtoIS;
#endif
              pvt->Bstride   = outf->stride >> 3;
	      pvt->dstoffset = 0;
	      pvt->mask      = 0; /* Unused */
	      pvt->shift     = 0; /* Unused */
	      pvt->clear_dst = FALSE;
	  } else {
	      if (pixelOrder == xieValLSFirst) {
		  if (fillOrder == xieValLSFirst) 
	              pvt->action = PtoLLUP;
	          else 
	              pvt->action = PtoLMUP;
	      } else {
		  if (fillOrder == xieValLSFirst) 
	              pvt->action = PtoMLUP;
	          else 
	              pvt->action = PtoMMUP;
	      }	
              pvt->bitOff   = 0;	/* Bit offset to first pixel on line */
              pvt->leftOver = 0;	/* Left over bits from last line     */
              pvt->depth    = inf->depth;
              pvt->stride   = outf->stride;
              pvt->pitch    = outf->pitch;
	  } 
    } else if (class == QUAD_PIXEL) {
          pvt->width = inf->width;
	  if (!(outf->stride & 31)) {
#if (IMAGE_BYTE_ORDER == LSBFirst)
	      if (outf->stride == 32)
	      	  pvt->action = (fillOrder == xieValLSFirst) ? 
						(void (*)())NULL : sQtoS;
	      else
	      	  pvt->action = (fillOrder == xieValLSFirst) ? QtoIS: sQtoIS;
#else
	      if (outf->stride == 32)
	      	  pvt->action = (fillOrder == xieValMSFirst) ? 
						(void (*)())NULL : sQtoS;
	      else
	      	  pvt->action = (fillOrder == xieValMSFirst) ? QtoIS: sQtoIS;
#endif
              pvt->Bstride   = outf->stride >> 3;
	      pvt->dstoffset = 0;
	      pvt->mask      = 0; /* Unused */
	      pvt->shift     = 0; /* Unused */
	      pvt->clear_dst = FALSE;
	  } else {
	      if (pixelOrder == xieValLSFirst) {
		  if (fillOrder == xieValLSFirst) 
	              pvt->action = QtoLLUQ;
	          else 
	              pvt->action = QtoLMUQ;
	      } else {
		  if (fillOrder == xieValLSFirst) 
	              pvt->action = QtoMLUQ;
	          else 
	              pvt->action = QtoMMUQ;
	      }	
	  } 
      } else
        ImplementationError(flo,ped, return(FALSE)); 
    
    if (pvt->action) {
      pvt->bitOff   = 0;		/* Bit offset to first pixel on line */
      pvt->leftOver = 0;		/* Left over bits from last line     */
      pvt->width    = inf->width;
      pvt->depth    = inf->depth;
      pvt->stride   = outf->stride;
      pvt->pitch    = outf->pitch;
      if(!InitBand(flo,ped,sbnd,NO_DATAMAP,(CARD32)1,NO_INPLACE) ||
	 !InitBand(flo,ped,dbnd,NO_DATAMAP,(CARD32)0,NO_INPLACE))
	return(FALSE);
      inited = TRUE;
    } else {
      *outf = *inf;		/* Pass data into photomap in native format */
      sbnd->receptor->bypass |= 1<<b;
    }
  }
  pet->bandSync = NO_SYNC;
  
  return(inited ? TRUE :
	 /* Allows data manager to bypass element entirely */
	 InitReceptor(flo,ped,pet->receptor,NO_DATAMAP,1,NO_BANDS,ALL_BANDS));
}                               /* end InitializeEPhotoUncomByPlane */

/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
static int ActivateEPhotoUncomByPlane(floDefPtr flo, peDefPtr ped, peTexPtr pet)
{
  meUncompPtr           pvt = (meUncompPtr)pet->private;
  receptorPtr           rcp = pet->receptor;
  CARD32              bands = rcp->inFlo->bands;
  bandPtr              sbnd = rcp->band, dbnd;
  CARD32 b, olddlen, nextdlen, width, stride, pitch;
  pointer src;
  CARD8 *dst;
  
  for(b = 0; b < bands; ++sbnd, ++b, ++pvt) {
    dbnd     = &pet->emitter[pvt->bandMap];
    width    = sbnd->format->width;
    pitch    = dbnd->format->pitch;
    stride   = dbnd->format->stride;
    nextdlen = (pvt->bitOff + pitch + 7) >> 3;
    
    if (!(pet->scheduled & 1<<b)) continue;	/* This band is bypassed */
    
    src      = GetCurrentSrc(flo,pet,sbnd);
    dst      = (CARD8*)GetDstBytes(flo,pet,dbnd,dbnd->current,nextdlen,KEEP);
    
    while(!ferrCode(flo) && src && dst) {
      
      (*pvt->action)(src,dst,pvt);
      
      src         = GetNextSrc(flo,pet,sbnd,FLUSH);
      pvt->bitOff = (pvt->bitOff + pitch) & 7;  /* Set next */
      olddlen     = (pvt->bitOff) ? nextdlen - 1: nextdlen;
      nextdlen    = (pvt->bitOff + pitch + 7) >> 3;
      dst         = (CARD8*)GetDstBytes(flo,pet,dbnd,dbnd->current+olddlen,
					nextdlen,KEEP);
    }
    FreeData(flo,pet,sbnd,sbnd->current);

    if(!src && sbnd->final) {
      if (pvt->bitOff) /* If we have any bits left, send them out now */
	*(CARD8*)GetDstBytes(flo,pet,dbnd,dbnd->current,1,KEEP)=pvt->leftOver;

      SetBandFinal(dbnd);
      PutData(flo,pet,dbnd,dbnd->maxGlobal);   /* write the remaining data */
    }
  }
  return(TRUE);
}                               /* end ActivateEPhotoUncomByPlane  */


#if XIE_FULL
/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeEPhotoUncomByPixel(floDefPtr flo, peDefPtr ped)
{
  peTexPtr                 pet = ped->peTex;
  meUncompPtr              pvt = (meUncompPtr)pet->private;
  xieTecEncodeUncompressedTriple *tec =
			(xieTecEncodeUncompressedTriple *)
			((ePhotoDefPtr)ped->elemPvt)->encodeParms;
  formatPtr	          outf = pet->emitter[0].format;
  bandPtr sbnd1,sbnd2,sbnd3;
  CARD32 depth1,depth2,depth3,dstride;
  int s, d;
  
  pvt->unaligned = (tec->pixelStride[0] & 7) != 0;
  
  if(tec->bandOrder == xieValLSFirst)
    for(d = 0; d < xieValMaxBands; ++d)
      pvt[d].bandMap = d;
  else 
    for(s = 0, d = xieValMaxBands; d--; ++s)
      pvt[s].bandMap = d;
  
  sbnd1   = &pet->receptor[SRCtag].band[pvt[0].bandMap];
  sbnd2   = &pet->receptor[SRCtag].band[pvt[1].bandMap];
  sbnd3   = &pet->receptor[SRCtag].band[pvt[2].bandMap];
  depth1  = pvt[0].depth = sbnd1->format->depth;
  depth2  = pvt[1].depth = sbnd2->format->depth;
  depth3  = pvt[2].depth = sbnd3->format->depth;
  dstride = tec->pixelStride[0]>>3;
  pvt[0].width = pvt[1].width = pvt[2].width = sbnd1->format->width;

  if (!pvt->unaligned) {
    /* Look for special cases */
    if (depth1 == 16 && depth2 == 16 && depth3 == 16) {
#if (IMAGE_BYTE_ORDER == MSBFirst)
      void (*pa)() = (tec->pixelOrder == xieValMSFirst) ? PtoIS : sPtoIS;
#else
      void (*pa)() = (tec->pixelOrder == xieValMSFirst) ? sPtoIS : PtoIS;
#endif
      for(s = 0; s < xieValMaxBands; s++, pvt++) {
	pvt->action    = pa;
	pvt->Bstride   = dstride;
	pvt->dstoffset = s;
	pvt->mask      = 0; /* Unused */
	pvt->shift     = 0; /* Unused */
	pvt->clear_dst = FALSE;
      }
    } else if (depth1 == 8 && depth2 == 8 && depth3 == 8) {
      for(s = 0; s < xieValMaxBands; s++, pvt++) {
	pvt->action    = BtoIS;
	pvt->Bstride   = dstride;
	pvt->dstoffset = s;
	pvt->mask      = 0; /* Unused */
	pvt->shift     = 0; /* Unused */
	pvt->clear_dst = FALSE;
      }
    } else if (depth1 == 4 && depth2 == 4 && depth3 == 4) {
      if (tec->fillOrder == xieValMSFirst) {
	pvt->action    = BtoISb;
	pvt->Bstride   = dstride;
	pvt->dstoffset = 0;
	pvt->mask      = 0xf0; 
	pvt->clear_dst = FALSE;
	(pvt++)->shift = 4; 
	pvt->action    = BtoISb;
	pvt->Bstride   = dstride;
	pvt->dstoffset = 0;
	pvt->mask      = 0x0f; 
	pvt->clear_dst = FALSE;
	(pvt++)->shift = 0; 
	pvt->action    = BtoISb;
	pvt->Bstride   = dstride;
	pvt->dstoffset = 1;
	pvt->mask      = 0xf0; 
	pvt->clear_dst = FALSE;
	pvt->shift     = 4; 
      } else { /* xieValLSFirst */
	pvt->action    = BtoISb;
	pvt->Bstride   = dstride;
	pvt->dstoffset = 0;
	pvt->clear_dst = FALSE;
	pvt->mask      = 0x0f; 
	(pvt++)->shift = 0; 
	pvt->action    = BtoISb;
	pvt->Bstride   = dstride;
	pvt->dstoffset = 0;
	pvt->mask      = 0xf0; 
	pvt->clear_dst = FALSE;
	(pvt++)->shift = 4; 
	pvt->action    = BtoISb;
	pvt->Bstride   = dstride;
	pvt->dstoffset = 1;
	pvt->mask      = 0x0f; 
	pvt->clear_dst = FALSE;
	pvt->shift     = 0; 
      }
    } else if (depth1 + depth2 + depth3 <= 8) {
      CARD8 ones = 0xff,smask1,smask2,smask3,shift1,shift2,shift3;
      if (tec->fillOrder == xieValMSFirst) {
	smask1 = ~(ones>>depth1);
	smask2 = ~(ones>>(depth1 + depth2) | smask1);
	smask3 = ~(ones>>(depth1 + depth2 + depth3) | smask1 | smask2);
	shift1 = 8 - depth1;
	shift2 = 8 - (depth1 + depth2);
	shift3 = 8 - (depth1 + depth2 + depth3);
      } else { /* fillOrder == xieValLSFirst */
	smask3 = ~(ones<<depth3);
	smask2 = ~(ones<<(depth2 + depth3) | smask3);
	smask1 = ~(ones<<(depth1 + depth2 + depth3) | smask2 | smask3);
	shift3 = 0;
	shift2 = depth3;
	shift1 = depth2 + depth3;
      }
      pvt->action    = (depth1 > 1) ? BtoISb : btoISb;
      pvt->Bstride   = dstride;
      pvt->dstoffset = 0;
      pvt->mask      = smask1;
      pvt->clear_dst = TRUE;
      (pvt++)->shift = shift1;
      pvt->action    = (depth2 > 1) ? BtoISb : btoISb;
      pvt->Bstride   = dstride;
      pvt->dstoffset = 0;
      pvt->mask      = smask2;
      pvt->clear_dst = TRUE;
      (pvt++)->shift = shift2; 
      pvt->action    = (depth3 > 1) ? BtoISb : btoISb;
      pvt->Bstride   = dstride;
      pvt->dstoffset = 0;
      pvt->mask      = smask3;
      pvt->clear_dst = TRUE;
      pvt->shift     = shift3; 
    } else
	pvt->unaligned = TRUE;
  } 
  pvt = (meUncompPtr)pet->private;
  if (pvt->unaligned) { 
    /* No special cases, do it the hard way */
    pvt[0].pitch = outf->pitch;
    pvt->action = EncodeTripleFuncs[tec->pixelOrder == xieValLSFirst ? 0 : 1]
				   [tec->fillOrder  == xieValLSFirst ? 0 : 1]
				   [depth1 <= 8 ? 0 : 1]
				   [depth2 <= 8 ? 0 : 1]
				   [depth3 <= 8 ? 0 : 1];
    if((depth1 == 1 && !(pvt[0].buf = (pointer)XieMalloc(pvt[0].width+7))) ||
       (depth2 == 1 && !(pvt[1].buf = (pointer)XieMalloc(pvt[1].width+7))) ||
       (depth3 == 1 && !(pvt[2].buf = (pointer)XieMalloc(pvt[2].width+7))))
      AllocError(flo,ped, return(FALSE));
  }
  pet->bandSync = SYNC;
  
  return(InitReceptors(flo, ped, NO_DATAMAP, 1) && 
	 InitEmitter(flo, ped, NO_DATAMAP, NO_INPLACE));
}                               /* end InitializeEPhotoUncomByPixel */


/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
static int ActivateEPhotoUncomByPixel(floDefPtr flo, peDefPtr ped, peTexPtr pet)
{
  meUncompPtr pvt = (meUncompPtr)pet->private;
  bandPtr   sb0 = &pet->receptor[SRCtag].band[pvt[0].bandMap];
  bandPtr   sb1 = &pet->receptor[SRCtag].band[pvt[1].bandMap];
  bandPtr   sb2 = &pet->receptor[SRCtag].band[pvt[2].bandMap];
  bandPtr  dbnd = &pet->emitter[0];
  CARD32  pitch = dbnd->format->pitch;
  pointer sp0 = (pointer )NULL, sp1 = (pointer )NULL, sp2 = (pointer )NULL;
  CARD8 *dst;
  
  
  if (pvt->unaligned) {
    CARD32 stride   = dbnd->format->stride;
    CARD32 width    = dbnd->format->width;
    CARD32 nextdlen = (pvt->bitOff + pitch + 7) >> 3, olddlen;
    if((sp0 = GetCurrentSrc(flo,pet,sb0)) &&
       (sp1 = GetCurrentSrc(flo,pet,sb1)) && 
       (sp2 = GetCurrentSrc(flo,pet,sb2)) &&
       (dst = (BytePixel*)GetDstBytes(flo,pet,dbnd,dbnd->current,
				      nextdlen,KEEP)))
      do {
	if(pvt[0].buf) sp0 = bitexpand(sp0,pvt[0].buf,width,(char)0,(char)1);
	if(pvt[1].buf) sp1 = bitexpand(sp1,pvt[1].buf,width,(char)0,(char)1);
	if(pvt[2].buf) sp2 = bitexpand(sp2,pvt[2].buf,width,(char)0,(char)1);

	(*pvt->action)(sp0,sp1,sp2,dst,stride,pvt);
	
	sp0         = GetNextSrc(flo,pet,sb0,FLUSH);
	sp1         = GetNextSrc(flo,pet,sb1,FLUSH);
	sp2         = GetNextSrc(flo,pet,sb2,FLUSH);
	pvt->bitOff = (pvt->bitOff + pitch) & 7;  /* Set next */
	olddlen     = (pvt->bitOff) ? nextdlen - 1 : nextdlen;
	nextdlen    = (pvt->bitOff + pitch + 7) >> 3;
	dst         = (BytePixel*)GetDstBytes(flo,pet,dbnd,
					dbnd->current+olddlen,nextdlen,KEEP);
      } while(dst && sp0 && sp1 && sp2);

  } else {
    CARD32  dlen  = pitch >> 3;	/* For nicely aligned data */
    if((sp0 = GetCurrentSrc(flo,pet,sb0)) &&
       (sp1 = GetCurrentSrc(flo,pet,sb1)) && 
       (sp2 = GetCurrentSrc(flo,pet,sb2)) &&
       (dst = (BytePixel*)GetDstBytes(flo,pet,dbnd,dbnd->current,dlen,KEEP)))

      do {
	
	if (pvt[0].clear_dst) bzero(dst,(int)dlen);
	
	(*pvt[0].action)(sp0,dst,&pvt[0]);
	(*pvt[1].action)(sp1,dst,&pvt[1]);
	(*pvt[2].action)(sp2,dst,&pvt[2]);
	
	sp0 = GetNextSrc(flo,pet,sb0,FLUSH);
	sp1 = GetNextSrc(flo,pet,sb1,FLUSH);
	sp2 = GetNextSrc(flo,pet,sb2,FLUSH);
	dst = (BytePixel*)GetDstBytes(flo,pet,dbnd,dbnd->current+dlen,
				      dlen,KEEP);
      } while(dst && sp0 && sp1 && sp2);
  }
  
  FreeData(flo,pet,sb0,sb0->current);
  FreeData(flo,pet,sb1,sb1->current);
  FreeData(flo,pet,sb2,sb2->current);

  if(!sp0 && sb0->final && !sp1 && sb1->final && !sp2 && sb2->final) {
    if (pvt->bitOff) /* If we have any bits left, send them out now */
      *(CARD8*)GetDstBytes(flo,pet,dbnd,dbnd->current,1,KEEP) = pvt->leftOver;

    SetBandFinal(dbnd);
    PutData(flo,pet,dbnd,dbnd->maxGlobal);   /* write the remaining data */
  }
  
  return(TRUE);
}                               /* end ActivateEPhotoUncomByPixel */
#endif /* XIE_FULL */

/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetEPhoto(floDefPtr flo, peDefPtr ped)
{
  meUncompPtr pvt = (meUncompPtr)ped->peTex->private;
  int i;

  for(i = 0; i < xieValMaxBands; ++i)
    if(pvt[i].buf) pvt[i].buf = (pointer) XieFree(pvt[i].buf);

  ResetReceptors(ped);
  ResetEmitter(ped);
  
  return(TRUE);
}                               /* end ResetEPhoto */

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyEPhoto(floDefPtr flo, peDefPtr ped)
{
  /* get rid of the peTex structure  */
  ped->peTex = (peTexPtr) XieFree(ped->peTex);

  /* zap this element's entry point vector */
  ped->ddVec.create     = (xieIntProc) NULL;
  ped->ddVec.initialize = (xieIntProc) NULL;
  ped->ddVec.activate   = (xieIntProc) NULL;
  ped->ddVec.reset      = (xieIntProc) NULL;
  ped->ddVec.destroy    = (xieIntProc) NULL;

  return(TRUE);
}                               /* end DestroyEPhoto */

/* end module mephoto.c */
