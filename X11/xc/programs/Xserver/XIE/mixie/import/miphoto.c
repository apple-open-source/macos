/* $Xorg: miphoto.c,v 1.4 2001/02/09 02:04:27 xorgcvs Exp $ */
/**** module miphoto.c ****/
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
  
	miphoto.c -- DDXIE prototype import client photomap element
  
	Robert NC Shelley -- AGE Logic, Inc. April, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/import/miphoto.c,v 3.5 2001/12/14 19:58:28 dawes Exp $ */

#define _XIEC_MIPHOTO
#define _XIEC_IPHOTO

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
#include <miuncomp.h>
#include <xiemd.h>
#include <memory.h>


/*
 *  routines referenced by other DDXIE modules
 */
int	miAnalyzeIPhoto();

/*
 *  routines used internal to this module
 */
static int CreateIPhotoUncom();
static int InitializeIPhotoUncomByPlane();
static int ActivateIPhotoUncomByPlane();
static int ResetIPhoto();
static int DestroyIPhoto();

static int CreateIPhotoStream();
static int InitializeIPhotoStream();
static int ActivateIPhotoStream();
static int ResetIPhotoStream();

#if XIE_FULL
static int InitializeIPhotoUncomByPixel();
static int ActivateIPhotoUncomByPixel();
#endif
/*
 * routines we need from somewhere else
 */
extern bandMsk miImportCanonic();
extern bandMsk miImportStream();

extern int CreateICPhotoFax();
extern int InitializeIPhotoFax();
extern int ActivateICPhotoFax();
extern int ResetICPhotoFax();
extern int DestroyICPhotoFax();

#if XIE_FULL
extern int CreateIPhotoJpegBase();
extern int InitializeIPhotoJpegBase();
extern int ActivateIPhotoJpegBase();
extern int ResetIPhotoJpegBase();
extern int DestroyIPhotoJpegBase();
#endif

/*
 * DDXIE ImportPhotomap entry points
 */
static ddElemVecRec IPhotoUncomByPlaneVec = {
  CreateIPhotoUncom,
  InitializeIPhotoUncomByPlane,
  ActivateIPhotoUncomByPlane,
  (xieIntProc)NULL,
  ResetIPhoto,
  DestroyIPhoto
  };

static ddElemVecRec IPhotoStreamVec = {
  CreateIPhotoStream,
  InitializeIPhotoStream,
  ActivateIPhotoStream,
  (xieIntProc)NULL,
  ResetIPhotoStream,
  DestroyIPhoto
  };

static ddElemVecRec IPhotoFaxVec = {
  CreateICPhotoFax,	/* share this with ICphoto stuff 	*/
  InitializeIPhotoFax,	/* note that only this is unshared 	*/
  ActivateICPhotoFax,
  (xieIntProc)NULL,
  ResetICPhotoFax,
  DestroyICPhotoFax
  };

#if XIE_FULL
static ddElemVecRec IPhotoUncomByPixelVec = {
  CreateIPhotoUncom,
  InitializeIPhotoUncomByPixel,
  ActivateIPhotoUncomByPixel,
  (xieIntProc)NULL,
  ResetIPhoto,
  DestroyIPhoto
  };

static ddElemVecRec IPhotoJPEGBaselineVec = {
  CreateIPhotoJpegBase,
  InitializeIPhotoJpegBase,
  ActivateIPhotoJpegBase,
  (xieIntProc)NULL,
  ResetIPhotoJpegBase,
  DestroyIPhotoJpegBase
  };
#endif /* XIE_FULL */


/*------------------------------------------------------------------------
------------------- see if we can handle this element --------------------
------------------------------------------------------------------------*/
int miAnalyzeIPhoto(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  photomapPtr map = ((iPhotoDefPtr)ped->elemPvt)->map;
  
  if(!miImportCanonic(flo,ped)) {
    ped->ddVec = IPhotoStreamVec;
    return(TRUE);
  }
  switch(map->technique) {
  case xieValDecodeUncompressedSingle:
    ped->ddVec = IPhotoUncomByPlaneVec;
    break;
    
#if XIE_FULL
  case xieValDecodeUncompressedTriple:
    if(((xieTecDecodeUncompressedTriple *)
	map->tecParms)->interleave == xieValBandByPlane) 
      ped->ddVec = IPhotoUncomByPlaneVec;
    else
      ped->ddVec = IPhotoUncomByPixelVec;
    
    break;
  case xieValDecodeJPEGBaseline:
    ped->ddVec = IPhotoJPEGBaselineVec;
    break;
#endif /* XIE_FULL */
  case xieValDecodeTIFFPackBits:
  case xieValDecodeTIFF2:
  case xieValDecodeG31D:
  case xieValDecodeG32D:
  case xieValDecodeG42D:
    ped->ddVec = IPhotoFaxVec;
    break;
  default:
    ImplementationError(flo,ped, return(FALSE));
  }
  return(TRUE);
}                               /* end miAnalyzeIPhoto */

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateIPhotoUncom(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* attach an execution context to the photo element definition */
  return(MakePETex(flo, ped, xieValMaxBands * sizeof(miUncompRec), 
			      NO_SYNC, NO_SYNC) );
}                               /* end CreateIPhotoUncom */


static int CreateIPhotoStream(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* attach an execution context to the photo element definition */
  return( MakePETex(flo, ped, NO_PRIVATE, NO_SYNC, NO_SYNC) );
}                               /* end CreateIPhotoStream */


/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeIPhotoUncomByPlane(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  photomapPtr              map = ((iPhotoDefPtr)ped->elemPvt)->map;
  peTexPtr                 pet = ped->peTex;
  miUncompPtr              pvt = (miUncompPtr)pet->private;
  CARD32	        nbands = ped->outFlo.bands,b,s;
  formatPtr                inf = ped->inFloLst[IMPORT].format;
  bandMsk 	        import = NO_BANDS;
  bandMsk             reformat = miImportCanonic(flo,ped);
  xieTypOrientation pixelOrder, fillOrder;
  CARD8 *ppad;
  
  if (nbands == 1) {
    xieTecDecodeUncompressedSingle *tec = ((xieTecDecodeUncompressedSingle*)
					   map->tecParms);
    pixelOrder   = tec->pixelOrder;
    fillOrder    = tec->fillOrder;
    pvt->bandMap = 0;
    ppad         = &tec->leftPad;
  } else {
#if XIE_FULL
    xieTecDecodeUncompressedTriple *tec = ((xieTecDecodeUncompressedTriple*)
					   map->tecParms);
    pixelOrder = tec->pixelOrder;
    fillOrder  = tec->fillOrder;
    ppad       = tec->leftPad;
    if (tec->bandOrder == xieValLSFirst) 
      for(b = 0; b < xieValMaxBands; ++b) 
	pvt[b].bandMap = b;
    else 
      for(s = 0, b = xieValMaxBands; b--; ++s)
	pvt[s].bandMap = b;
#else
    ImplementationError(flo,ped, return(FALSE));
#endif
  }
  
  for (b = 0; b < nbands; b++, pvt++, ppad++, inf++) {
    pvt->next_strip = map->strips[b].flink;
    if (IsCanonic(inf->class))/* If data was passed through then no decoding*/
      continue;
    
    import |= 1<<b;
    if(reformat & 1<<b)
      pvt->reformat = TRUE;
    else
      continue;

    pvt->bitOff = pvt->leftPad  = *ppad;
    if(inf->depth == 1) {  
#if (IMAGE_BYTE_ORDER == MSBFirst)
      if (pvt->leftPad & 7 || inf->stride != 1) {
	   pvt->action = (fillOrder == xieValMSFirst) ? CPextractstreambits:
						   CPextractswappedstreambits;
      } else {
      	   pvt->action = (fillOrder == xieValMSFirst) ? CPpass_bits : 
						        CPreverse_bits;
      }
#else
      if (pvt->leftPad & 7 || inf->stride != 1) {
	   pvt->action = (fillOrder == xieValLSFirst) ? CPextractstreambits:
						   CPextractswappedstreambits;
      } else {
      	   pvt->action = (fillOrder == xieValLSFirst) ? CPpass_bits : 
						        CPreverse_bits;
      }
#endif
    } else if (inf->depth <= 8) {
      if (pvt->leftPad & 7 || inf->stride & 7) {
	/* They chose . . . poorly */
	if (pixelOrder == xieValMSFirst) {
	  if(fillOrder == xieValMSFirst)
	    pvt->action = MMUBtoB;
	  else
	    pvt->action = MLUBtoB;
	} else {
	  if(fillOrder == xieValMSFirst)
	    pvt->action = LMUBtoB;
	  else
	    pvt->action = LLUBtoB;
	}
      } else {
	/* They chose wisely */
	pvt->action = CPpass_bytes; 
      }
    } else if (inf->depth <= 16) {
      if (pvt->leftPad & 15 || inf->stride & 15) {
	/* They chose . . . poorly */
	if (pixelOrder == xieValMSFirst) {
	  if(fillOrder == xieValMSFirst)
	    pvt->action = MMUPtoP;
	  else
	    pvt->action = MLUPtoP;
	} else {
	  if(fillOrder == xieValMSFirst)
	    pvt->action = LMUPtoP;
	  else
	    pvt->action = LLUPtoP;
	}
      } else {
	/* They chose wisely */
#if (IMAGE_BYTE_ORDER == MSBFirst)
	pvt->action = fillOrder == xieValMSFirst ? CPpass_pairs : CPswap_pairs;
#else
	pvt->action = fillOrder == xieValLSFirst ? CPpass_pairs : CPswap_pairs;
#endif
      }
    } else if (inf->depth <= 24) {
      if (pvt->leftPad & 31 || inf->stride & 31) {
	/* They chose . . . poorly */
	if (pixelOrder == xieValMSFirst) {
	  if(fillOrder == xieValMSFirst)
	    pvt->action = MMUQtoQ;
	  else
	    pvt->action = MLUQtoQ;
	} else {
	  if(fillOrder == xieValMSFirst)
	    pvt->action = LMUQtoQ;
	  else
	    pvt->action = LLUQtoQ;
	}
      } else {
	/* They chose wisely */
#if (IMAGE_BYTE_ORDER == MSBFirst)
	pvt->action = fillOrder == xieValMSFirst ? CPpass_quads : CPswap_quads;
#else
	pvt->action = fillOrder == xieValLSFirst ? CPpass_quads : CPswap_quads;
#endif
      }
    } else {
      ImplementationError(flo,ped, return(FALSE));
    }
  }
  if(import) {
    bandMsk smuggle = import & ~reformat; /* bands that will bypass emitter */
    receptorPtr rcp = pet->receptor;

    rcp->forward = miImportStream(flo,ped);

    return( InitReceptor(flo, ped, rcp, NO_DATAMAP, 1, import, smuggle) &&
	   (!(import & 1) ||
	    ImportStrips(flo, pet,&rcp->band[0], &map->strips[0])) &&
	   (!(import & 2) ||
	    ImportStrips(flo, pet,&rcp->band[1], &map->strips[1])) &&
	   (!(import & 4) ||
	    ImportStrips(flo, pet,&rcp->band[2], &map->strips[2])) &&
	   InitEmitter(flo, ped, NO_DATAMAP, NO_INPLACE));
  } else {
    return(InitEmitter(flo, ped, NO_DATAMAP, NO_INPLACE));
  }
}                               /* end InitializeIPhotoUncomByPlane */


static int InitializeIPhotoStream(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  photomapPtr map = ((iPhotoDefPtr)ped->elemPvt)->map;
  peTexPtr    pet = ped->peTex;
  receptorPtr rcp = pet->receptor;
  int   b, nbands = ped->inFloLst[IMPORT].bands;

  /* tell data manager to forward our input data to downstream elements
   */
  rcp->forward = miImportStream(flo,ped);

  if(!InitReceptor(flo, ped, rcp, NO_DATAMAP, 1, rcp->forward, ~rcp->forward))
    return(FALSE);

  for(b = 0; b < nbands; ++b)
    if(!ImportStrips(flo, pet, &rcp->band[b], &map->strips[b]))
      return(FALSE);

  return(InitEmitter(flo, ped, NO_DATAMAP, NO_INPLACE));
}                               /* end InitializeIPhotoStream */


/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
static int ActivateIPhotoUncomByPlane(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  photomapPtr           map = ((iPhotoDefPtr)ped->elemPvt)->map;
  xieFloImportPhotomap *raw = (xieFloImportPhotomap *)ped->elemRaw;
  miUncompPtr           pvt = (miUncompPtr)pet->private;
  formatPtr		fmt = ped->inFloLst[IMPORT].format;
  bandPtr               bnd = &pet->emitter[0];
  bandPtr              sbnd = pet->receptor[SRCtag].band;
  bandPtr dbnd;
  CARD32 oldslen, final, nextslen, b;
  
  for(b = 0; b < map->bands; b++, pvt++, fmt++, sbnd++, bnd++)
    if(!bnd->final)
      if(pet->receptor[SRCtag].active & 1<<b) {
	void (*action)() = pvt->action;
	pointer src, dst;
	
        if(!pvt->reformat) {
          FreeData(flo, pet, sbnd, sbnd->maxGlobal);
          continue;
        }
	nextslen = pvt->bitOff + sbnd->format->pitch + 7 >> 3;
	dbnd = &pet->emitter[pvt->bandMap];
	if((src = GetSrcBytes(flo,pet,sbnd,sbnd->current,nextslen,KEEP))
	   && (dst = GetCurrentDst(flo,pet,dbnd)))
	  
	  do {
	    (*action)(src, dst, sbnd->format->width, 
		      (CARD32)pvt->bitOff,
		      (CARD32)sbnd->format->depth,
		      sbnd->format->stride);
	    
	    pvt->bitOff = pvt->bitOff + sbnd->format->pitch & 7;
	    oldslen  = (pvt->bitOff) ? nextslen - 1 : nextslen;
	    nextslen = pvt->bitOff + sbnd->format->pitch + 7 >> 3;
	    src = GetSrcBytes(flo,pet,sbnd,sbnd->current+oldslen, 
			      nextslen,KEEP);
	    dst = GetNextDst(flo,pet,dbnd,FLUSH);
	  } while(src && dst);

	final = sbnd->strip && sbnd->strip->final;
	if(!src && final && dbnd->current < dbnd->format->height) {
	  /*
	   * the client lied about the image size!
	   */
	  if(raw->notify)
	    SendDecodeNotifyEvent(flo, ped, 0, map->technique,
				  dbnd->format->width, dbnd->current, TRUE);
	  /* 
	   * If the client didn't send enough data, we could zero-fill the
	   * remaining lines.  Since we sent the "aborted" status, we won't
	   * bother (the protocol offers both choices).
	   */
	  ValueError(flo,ped,dbnd->format->height, return(FALSE));
	}
	if(!src || dbnd->final) {
	  /* free whatever we've used so far
	   */
	  FreeData(flo, pet, sbnd, final ? sbnd->maxGlobal : sbnd->current);
	}
      } else {
	stripPtr  strip = pvt->next_strip;
	pvt->next_strip = strip->flink;
	
	if(ListEnd(strip,&map->strips[b]))
	  AccessError(flo,ped, return(FALSE));
	
	/* pass a clone of the current strip to our recipients
	 */
	if(!PassStrip(flo,pet,bnd,strip))
	  return(FALSE);	/* alloc error */
      }
  return(TRUE);
}                               /* end ActivateIPhotoUncomByPlane */


static int ActivateIPhotoStream(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  CARD32 nbands = ped->inFloLst[IMPORT].bands;
  bandPtr   bnd = pet->receptor[IMPORT].band;
  CARD32    b;
  
  for(b = 0; b < nbands; ++bnd, ++b) {
    if(GetSrcBytes(flo,pet,bnd,bnd->current,1,KEEP)) {
      FreeData(flo,pet,bnd,bnd->maxLocal);
      if(ListEmpty(&bnd->stripLst))
	DisableDst(flo,pet,&pet->emitter[b]);
    }
  }
  return(TRUE);
}                               /* end ActivateIPhotoStream */



/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetIPhoto(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  miUncompPtr pvt = (miUncompPtr)ped->peTex->private;
  int i;

  for(i = 0; i < xieValMaxBands; ++i)
    if(pvt[i].buf) pvt[i].buf = (pointer) XieFree(pvt[i].buf);

  ResetReceptors(ped);
  ResetEmitter(ped);
  
  return(TRUE);
}                               /* end ResetIPhoto */


static int ResetIPhotoStream(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  ResetReceptors(ped);
  ResetEmitter(ped);
  
  return(TRUE);
}                               /* end ResetIPhotoStream */


/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyIPhoto(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
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
}                               /* end DestroyIPhoto */


#if XIE_FULL
/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeIPhotoUncomByPixel(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  photomapPtr  map = ((iPhotoDefPtr)ped->elemPvt)->map;
  xieTecDecodeUncompressedTriple *tec = ((xieTecDecodeUncompressedTriple *)
					 map->tecParms);
  peTexPtr     pet = ped->peTex;
  miUncompPtr  pvt = (miUncompPtr)pet->private;
  bandPtr     sbnd = &ped->peTex->receptor[IMPORT].band[0];
  CARD32   sstride = sbnd->format->stride>>3;
  CARD8    leftPad = tec->leftPad[0]>>3;
  xieTypOrientation  pixelOrder, fillOrder;
  CARD32     depth0, depth1, depth2;
  int        s, d;
  
  pvt->unaligned = FALSE;	/* Hope for the best */
  
  if(tec->bandOrder == xieValLSFirst)
    for(d = 0; d < xieValMaxBands; ++d)
      pvt[d].bandMap = d;
  else 
    for(s = 0, d = xieValMaxBands; d--; ++s)
      pvt[s].bandMap = d;
  
  depth0 = pet->emitter[pvt[0].bandMap].format->depth;
  depth1 = pet->emitter[pvt[1].bandMap].format->depth;
  depth2 = pet->emitter[pvt[2].bandMap].format->depth;
  
  pixelOrder        = tec->pixelOrder;
  fillOrder         = tec->fillOrder;
  pvt[0].next_strip = map->strips[0].flink;
  
  pvt->bitOff = pvt->leftPad = tec->leftPad[0];
  
  /* See if data is nicely aligned */
  if (!(tec->leftPad[0] & 7) && !(sbnd->format->stride & 7)) {
    if (depth0 == 16 && depth1 == 16 && depth2 == 16) {
#if (IMAGE_BYTE_ORDER == MSBFirst)
      void (*pa)() = (tec->pixelOrder == xieValMSFirst) ? StoP  : StosP;
#else
      void (*pa)() = (tec->pixelOrder == xieValMSFirst) ? StosP : StoP;
#endif
      for(s = 0; s < xieValMaxBands; s++, pvt++) {
	pvt->action    = pa;
	pvt->Bstride   = sstride;
	pvt->srcoffset = s + leftPad;
	pvt->mask      = 0; /* Unused */
	pvt->shift     = 0; /* Unused */
      }
    } else if (depth0 == 8 && depth1 == 8 && depth2 == 8) {
      for(s = 0; s < xieValMaxBands; s++, pvt++) {
	pvt->action    = StoB;
	pvt->Bstride   = sstride;
	pvt->srcoffset = s + leftPad;
	pvt->mask      = 0; /* Unused */
	pvt->shift     = 0; /* Unused */
      }
    } else if (depth0 == 4 && depth1 == 4 && depth2 == 4) {
      if (tec->fillOrder == xieValMSFirst) {
	pvt->action    = SbtoB;
	pvt->Bstride   = sstride;
	pvt->srcoffset = leftPad;
	pvt->mask      = 0xf0; 
	(pvt++)->shift = 4; 
	pvt->action    = SbtoB;
	pvt->Bstride   = sstride;
	pvt->srcoffset = leftPad;
	pvt->mask      = 0x0f; 
	(pvt++)->shift = 0; 
	pvt->action    = SbtoB;
	pvt->Bstride   = sstride;
	pvt->srcoffset = 1 + leftPad;
	pvt->mask      = 0xf0; 
	pvt->shift     = 4; 
      } else { /* xieValLSFirst */
	pvt->action    = SbtoB;
	pvt->Bstride   = sstride;
	pvt->srcoffset = leftPad;
	pvt->mask      = 0x0f; 
	(pvt++)->shift = 0; 
	pvt->action    = SbtoB;
	pvt->Bstride   = sstride;
	pvt->srcoffset = leftPad;
	pvt->mask      = 0xf0; 
	(pvt++)->shift = 4; 
	pvt->action    = SbtoB;
	pvt->Bstride   = sstride;
	pvt->srcoffset = 1 + leftPad;
	pvt->mask      = 0x0f; 
	pvt->shift     = 0; 
      }
    } else if (depth0 + depth1 + depth2 <= 8) {
      CARD8 ones = 0xff,smask0,smask1,smask2,shift0,shift1,shift2;
      if (tec->fillOrder == xieValMSFirst) {
	smask0 = ~(ones>>depth0);
	smask1 = ~(ones>>(depth0 + depth1) | smask0);
	smask2 = ~(ones>>(depth0 + depth1 + depth2) | smask0 | smask1);
	shift0 = 8 - depth0;
	shift1 = 8 - (depth0 + depth1);
	shift2 = 8 - (depth0 + depth1 + depth2);
      } else { /* fillOrder == xieValLSFirst */
	smask2 = ~(ones<<depth2);
	smask1 = ~(ones<<(depth1 + depth2) | smask2);
	smask0 = ~(ones<<(depth0 + depth1 + depth2) | smask1 | smask2);
	shift2 = 0;
	shift1 = depth2;
	shift0 = depth1 + depth2;
      }
      pvt->action    = (depth0 > 1) ? SbtoB : Sbtob;
      pvt->Bstride   = sstride;
      pvt->srcoffset = leftPad;
      pvt->mask      = smask0;
      (pvt++)->shift = shift0;
      pvt->action    = (depth1 > 1) ? SbtoB : Sbtob;
      pvt->Bstride   = sstride;
      pvt->srcoffset = leftPad;
      pvt->mask      = smask1;
      (pvt++)->shift = shift1; 
      pvt->action    = (depth2 > 1) ? SbtoB : Sbtob;
      pvt->Bstride   = sstride;
      pvt->srcoffset = leftPad;
      pvt->mask      = smask2;
      pvt->shift     = shift2; 
    } else {
      pvt->unaligned = TRUE;
    }
  } else {
    pvt->unaligned = TRUE;
  }
  pvt = (miUncompPtr)pet->private;
  pvt->reformat = TRUE;

  if(pvt->unaligned) {
    /* since it's unaligned and somebody wants our data, do it the hard way */
    CARD32 width = sbnd->format->width;
    pvt->action = ExtractTripleFuncs[tec->pixelOrder == xieValLSFirst ? 0 : 1]
      				    [tec->fillOrder  == xieValLSFirst ? 0 : 1]
				    [depth0 <= 8 ? 0 : 1] 
				    [depth1 <= 8 ? 0 : 1] 
				    [depth2 <= 8 ? 0 : 1];
    if(depth0 == 1 && !(pvt[0].buf = (pointer)XieMalloc(width+7)) ||
       depth1 == 1 && !(pvt[1].buf = (pointer)XieMalloc(width+7)) ||
       depth2 == 1 && !(pvt[2].buf = (pointer)XieMalloc(width+7)))
      AllocError(flo,ped, return(FALSE));
  }
  /* see if data manager should forward our input data to downstream elements
   */
  pet->receptor->forward = miImportStream(flo,ped);

  return(InitReceptor(flo,ped, pet->receptor,NO_DATAMAP,1,1,NO_BANDS)    &&
	 ImportStrips(flo,pet,&pet->receptor[0].band[0],&map->strips[0]) &&
	 InitEmitter(flo,ped,NO_DATAMAP,NO_INPLACE));
}                               /* end InitializeIPhotoUnTriple */


/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
static int ActivateIPhotoUncomByPixel(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  xieFloImportPhotomap *raw = (xieFloImportPhotomap *)ped->elemRaw;
  miUncompPtr     pvt = (miUncompPtr) (pet->private);
  bandPtr        sbnd = &pet->receptor[IMPORT].band[0];
  bandPtr         db0 = &pet->emitter[pvt[0].bandMap];
  bandPtr         db1 = &pet->emitter[pvt[1].bandMap];
  bandPtr         db2 = &pet->emitter[pvt[2].bandMap];
  CARD32 final, width = db0->format->width;
  pointer src, dp0, dp1, dp2;
  
  if(pvt->unaligned) {
    CARD32 oldslen, nextslen;
    void (*action)() = pvt->action;
    CARD32 depth0 = db0->format->depth;
    CARD32 depth1 = db1->format->depth;
    CARD32 depth2 = db2->format->depth;
    CARD32 stride = sbnd->format->stride;
    
    nextslen = pvt->bitOff + sbnd->format->pitch + 7 >> 3;
    if((src = GetSrcBytes(flo,pet,sbnd,sbnd->current,nextslen,KEEP)) &&
       (dp0 = GetCurrentDst(flo,pet,db0)) &&
       (dp1 = GetCurrentDst(flo,pet,db1)) &&
       (dp2 = GetCurrentDst(flo,pet,db2)))
      do {
	
	(*action)(src,
		  pvt[0].buf ? pvt[0].buf : dp0,
		  pvt[1].buf ? pvt[1].buf : dp1,
		  pvt[2].buf ? pvt[2].buf : dp2,
		  width, pvt->bitOff, depth0, depth1, depth2, stride);

	if(pvt[0].buf) bitshrink(pvt[0].buf,dp0,width,(BytePixel)1);
	if(pvt[1].buf) bitshrink(pvt[1].buf,dp1,width,(BytePixel)1);
	if(pvt[2].buf) bitshrink(pvt[2].buf,dp2,width,(BytePixel)1);

	pvt->bitOff = pvt->bitOff + sbnd->format->pitch & 7;
	oldslen     = pvt->bitOff ? nextslen - 1 : nextslen;
	nextslen    = pvt->bitOff + sbnd->format->pitch + 7 >> 3;
	src = GetSrcBytes(flo,pet,sbnd,sbnd->current+oldslen,nextslen,KEEP);
	dp0 = GetNextDst(flo,pet,db0,FLUSH);
	dp1 = GetNextDst(flo,pet,db1,FLUSH);
	dp2 = GetNextDst(flo,pet,db2,FLUSH);
      } while(src && dp0 && dp1 && dp2);
  } else {
    CARD32   slen = sbnd->format->pitch+7>>3;
    if((src = GetSrcBytes(flo,pet,sbnd,sbnd->current,slen,KEEP)) && 
       (dp0 = GetCurrentDst(flo,pet,db0)) &&
       (dp1 = GetCurrentDst(flo,pet,db1)) && 
       (dp2 = GetCurrentDst(flo,pet,db2)))
      do {
	
	(*pvt[0].action)(src,dp0,width,&pvt[0]);
	(*pvt[1].action)(src,dp1,width,&pvt[1]);
	(*pvt[2].action)(src,dp2,width,&pvt[2]);
	
	src =GetSrcBytes(flo,pet,sbnd,sbnd->current+slen,slen,KEEP);
	dp0 = GetNextDst(flo,pet,db0,FLUSH);
	dp1 = GetNextDst(flo,pet,db1,FLUSH);
	dp2 = GetNextDst(flo,pet,db2,FLUSH);
      } while(src && dp0 && dp1 && dp2);
  }
  final = sbnd->strip && sbnd->strip->final;
  if(!src && final && db0->current < db0->format->height) {
    /*
     * the client lied about the image size!
     */
    if(raw->notify)
      SendDecodeNotifyEvent(flo, ped, 0, xieValDecodeUncompressedTriple,
			    db0->format->width, db0->current, TRUE);
    /* 
     * If the client didn't send enough data, we could zero-fill the
     * remaining lines.  Since we sent the "aborted" status, we won't
     * bother (the protocol offers both choices).
     */
    ValueError(flo,ped,db0->format->height, return(FALSE));
  }
  if(!src || db0->final && db1->final && db2->final) {
    /* free whatever we've used so far
     */
    FreeData(flo, pet, sbnd, final ? sbnd->maxGlobal : sbnd->current);
  }
  return(TRUE);
}                               /* end ActivateIPhotoUncomByPixel */
#endif /* XIE_FULL */

/* end module miphoto.c */
