/* $Xorg: ephoto.c,v 1.4 2001/02/09 02:04:20 xorgcvs Exp $ */
/**** module ephoto.c ****/
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
  
	ephoto.c -- DIXIE routines for managing the ExportPhotomap element
  
	Robert NC Shelley && Dean Verheiden -- AGE Logic, Inc. April 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/export/ephoto.c,v 3.6 2001/12/14 19:57:59 dawes Exp $ */

#define _XIEC_EPHOTO

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
   *  more X server includes.
   */
#include <servermd.h>
  /*
   *  Server XIE Includes
   */
#include <corex.h>
#include <error.h>
#include <macro.h>
#include <flo.h>
#include <photomap.h>
#include <element.h>
#include <technq.h>
#include <tables.h>	/* For Server Choice function */
#include <memory.h>

/*
 *  routines internal to this module
 */
static Bool CopyEPhotoServerChoice(
			floDefPtr  flo,
			peDefPtr   ped,
			xieTecEncodeServerChoice *sparms,
			xieTecEncodeServerChoice *rparms,
			CARD16	tsize);
static Bool PrepEPhoto(floDefPtr flo, peDefPtr ped);
static Bool DebriefEPhoto(floDefPtr flo, peDefPtr ped, Bool ok);

extern pointer	GetImportTechnique();

/*
 * dixie entry points
 */
static diElemVecRec ePhotoVec = {
    PrepEPhoto,
    DebriefEPhoto
    };


/*------------------------------------------------------------------------
----------------- routine: make an ExportPhotomap element ----------------
------------------------------------------------------------------------*/
peDefPtr MakeEPhoto(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
  peDefPtr ped;
  inFloPtr inFlo;
  ELEMENT(xieFloExportPhotomap);
  ELEMENT_AT_LEAST_SIZE(xieFloExportPhotomap);
  ELEMENT_NEEDS_1_INPUT(src);
  
  if(!(ped = MakePEDef(1,(CARD32)stuff->elemLength<<2,sizeof(ePhotoDefRec)))) 
    FloAllocError(flo,tag,xieElemExportPhotomap, return(NULL));

  ped->diVec	    = &ePhotoVec;
  ped->phototag     = tag;
  ped->flags.export = TRUE;
  raw = (xieFloExportPhotomap *)ped->elemRaw;
  /*
   * copy the standard client element parameters (swap if necessary)
   */
  if( flo->reqClient->swapped ) {
    raw->elemType   = stuff->elemType;
    raw->elemLength = stuff->elemLength;
    cpswaps(stuff->src, raw->src);
    cpswapl(stuff->photomap, raw->photomap);
    cpswaps(stuff->encodeTechnique, raw->encodeTechnique);
    cpswaps(stuff->lenParams, raw->lenParams);
  }
  else  
    memcpy((char *)raw, (char *)stuff, sizeof(xieFloExportPhotomap));
  /*
   * assign phototags to inFlos
   */
  inFlo = ped->inFloLst;
  inFlo[SRCtag].srcTag = raw->src;

  /* 
   * copy technique data (if any)
   */
  if (raw->encodeTechnique == xieValEncodeServerChoice) {
    if (!CopyEPhotoServerChoice(flo, ped,
		(xieTecEncodeServerChoice *) &stuff[1],
		(xieTecEncodeServerChoice *) &raw[1],
		raw->lenParams))
       TechniqueError(flo,ped,xieValEncode,raw->encodeTechnique,raw->lenParams,
		     return(ped));
  } else if (!(ped->techVec = FindTechnique(xieValEncode,raw->encodeTechnique)) 
         || !(ped->techVec->copyfnc(flo,ped,&stuff[1],&raw[1],raw->lenParams,0)))
       TechniqueError(flo,ped,xieValEncode,raw->encodeTechnique,raw->lenParams,
		     return(ped));

  return(ped);
}                               /* end MakeEPhoto */

static Bool CopyEPhotoServerChoice(
     floDefPtr  flo,
     peDefPtr   ped,
     xieTecEncodeServerChoice *sparms,
     xieTecEncodeServerChoice *rparms,
     CARD16	tsize)
{
  if(tsize == 1)
    rparms->preference = sparms->preference;

  return(tsize <= 1);
}

/* All other technique-specific copy routines are defined in ecphoto.c */


/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/
static Bool PrepEPhoto(floDefPtr flo, peDefPtr ped)
{
  xieFloExportPhotomap *raw = (xieFloExportPhotomap *)ped->elemRaw;
  ePhotoDefPtr          pvt = (ePhotoDefPtr)ped->elemPvt;
  inFloPtr              inf = &ped->inFloLst[SRCtag];
  outFloPtr             dst = &ped->outFlo;
  xieBoolProc        scPrep;
  CARD32 b;

  /* find the photomap resource and bind it to our flo
   */
  if(!(pvt->map = (photomapPtr) LookupIDByType(raw->photomap, RT_PHOTOMAP)))
    PhotomapError(flo,ped,raw->photomap, return(FALSE));
  ++pvt->map->refCnt;

  pvt->congress = FALSE;
  if(raw->encodeTechnique == xieValEncodeServerChoice) {
    if(!(scPrep = ((xieBoolProc (*)())
		   DDInterface[DDServerChoiceIndex]) (flo, ped)))
      TechniqueError(flo,ped,xieValEncode,raw->encodeTechnique,raw->lenParams,
		     return(FALSE));
  } else {
    /* grab a copy of the input attributes and propagate them to our output
     */   
    outFloPtr src = &inf->srcDef->outFlo;
    for(b = 0; b < src->bands; ++b) {
      if(IsntCanonic(src->format[b].class))
        MatchError(flo,ped, return(FALSE));
      dst->format[b] = inf->format[b] = src->format[b];
    dst->bands = inf->bands = src->bands;
    /* dst->bands will be 1 if we encode TripleBand interleaved BandByPixel */
    }
    scPrep = (xieBoolProc)NULL;
  }

  /* do technique-specific preparations
   */
  if(scPrep) {
      if (!(*scPrep)(flo,ped)) 
          TechniqueError(flo,ped,xieValEncode,raw->encodeTechnique,
			raw->lenParams, return(FALSE));
  } else {
    if(!(ped->techVec->prepfnc(flo, ped, &raw[1])))
      TechniqueError(flo,ped,xieValEncode,raw->encodeTechnique,raw->lenParams,
		     return(FALSE));

    pvt->encodeNumber = raw->encodeTechnique; 
    pvt->encodeLen    = raw->lenParams << 2; 
    pvt->encodeParms  = (pointer)&raw[1];

    if(!BuildDecodeFromEncode(flo,ped))
      AllocError(flo,ped, return(FALSE));

    /* see if import data can leap-frog the import and export elements
     */
    if(ped->inFloLst[IMPORT].srcDef->flags.import && CompareDecode(flo,ped)) {
      inFloPtr import = &inf->srcDef->inFloLst[IMPORT];

      /* copy smuggled data attributes to our inFlo */
      inf->bands = import->bands;
      for(b = 0; b < import->bands; ++b)
        inf->format[b] = import->format[b];
      pvt->congress = TRUE;
    }
  }
  return(TRUE);
}                               /* end PrepEPhoto */

/* All technique-specific prep routines are defined in ecphoto.c */

Bool BuildDecodeFromEncode(floDefPtr flo, peDefPtr ped)
{
  ePhotoDefPtr pvt = (ePhotoDefPtr)ped->elemPvt;

  /* Based on the encode technique, build a correspoinding decode technique */
  switch(pvt->encodeNumber) {
    case xieValEncodeUncompressedSingle:
      {
	xieTecEncodeUncompressedSingle *etec = 
	  (xieTecEncodeUncompressedSingle *)pvt->encodeParms;
	xieTecDecodeUncompressedSingle *dtec;
	
	if (!(dtec = (xieTecDecodeUncompressedSingle *)
	      XieMalloc(sizeof(xieTecDecodeUncompressedSingle))))
	  AllocError(flo,ped, return(FALSE));
	pvt->decodeNumber = xieValDecodeUncompressedSingle;
	pvt->decodeLen    = sizeof(xieTecDecodeUncompressedSingle);
	pvt->decodeParms  = (pointer)dtec;
	dtec->fillOrder   = etec->fillOrder;
	dtec->pixelOrder  = etec->pixelOrder;
	dtec->pixelStride = etec->pixelStride;
	dtec->leftPad     = 0;
	dtec->scanlinePad = etec->scanlinePad;
      }
      break;
    case xieValEncodeUncompressedTriple: 
      {
	xieTecEncodeUncompressedTriple *etec = 
	  (xieTecEncodeUncompressedTriple *)pvt->encodeParms;
	xieTecDecodeUncompressedTriple *dtec;
	int i;
	
	if (!(dtec = (xieTecDecodeUncompressedTriple *)
	      XieMalloc(sizeof(xieTecDecodeUncompressedTriple))))
	  AllocError(flo,ped, return(FALSE));
	pvt->decodeNumber = xieValDecodeUncompressedTriple;
	pvt->decodeLen    = sizeof(xieTecDecodeUncompressedTriple);
	pvt->decodeParms  = (pointer)dtec;
	dtec->fillOrder   = etec->fillOrder;
	dtec->pixelOrder  = etec->pixelOrder;
	dtec->bandOrder   = etec->bandOrder;
	dtec->interleave  = etec->interleave;
	for (i = 0; i < 3; i++) {
	  dtec->leftPad[i] = 0;
	  dtec->pixelStride[i] = etec->pixelStride[i];
	  dtec->scanlinePad[i] = etec->scanlinePad[i];
	}
      }
      break;
    case xieValEncodeG31D:
      {
	xieTecEncodeG31D *etec = (xieTecEncodeG31D *)pvt->encodeParms;
	xieTecDecodeG31D *dtec;
	
	if (!(dtec = (xieTecDecodeG31D *)
	      XieMalloc(sizeof(xieTecDecodeG31D))))
	  AllocError(flo,ped, return(FALSE));
	pvt->decodeNumber  = xieValDecodeG31D;
	pvt->decodeLen     = sizeof(xieTecDecodeG31D);
	pvt->decodeParms   = (pointer)dtec;
	dtec->normal       = TRUE;
	dtec->radiometric  = etec->radiometric;
	dtec->encodedOrder = etec->encodedOrder;
      }
      break;
    case xieValEncodeG32D:
      {
	xieTecEncodeG32D *etec = (xieTecEncodeG32D *)pvt->encodeParms;
	xieTecDecodeG32D *dtec;
	
	if (!(dtec = (xieTecDecodeG32D *)
	      XieMalloc(sizeof(xieTecDecodeG32D))))
	  AllocError(flo,ped, return(FALSE));
	pvt->decodeNumber  = xieValDecodeG32D;
	pvt->decodeLen     = sizeof(xieTecDecodeG32D);
	pvt->decodeParms   = (pointer)dtec;
	dtec->normal       = TRUE;
	dtec->radiometric  = etec->radiometric;
	dtec->encodedOrder = etec->encodedOrder;
      }
      break;
    case xieValEncodeG42D:
      {
	xieTecEncodeG42D *etec = (xieTecEncodeG42D *)pvt->encodeParms;
	xieTecDecodeG42D *dtec;
	
	if (!(dtec = (xieTecDecodeG42D *)
	      XieMalloc(sizeof(xieTecDecodeG42D))))
	  AllocError(flo,ped, return(FALSE));
	pvt->decodeNumber  = xieValDecodeG42D;
	pvt->decodeLen     = sizeof(xieTecDecodeG42D);
	pvt->decodeParms   = (pointer)dtec;
	dtec->normal       = TRUE;
	dtec->radiometric  = etec->radiometric;
	dtec->encodedOrder = etec->encodedOrder;
      }
      break;
    case xieValEncodeJPEGBaseline:
      {
	xieTecEncodeJPEGBaseline *etec = 
	  (xieTecEncodeJPEGBaseline *)pvt->encodeParms;
	xieTecDecodeJPEGBaseline *dtec;
	
	if (!(dtec = (xieTecDecodeJPEGBaseline *)
	      XieMalloc(sizeof(xieTecDecodeJPEGBaseline))))
	  AllocError(flo,ped, return(FALSE));
	pvt->decodeNumber  = xieValDecodeJPEGBaseline;
	pvt->decodeLen     = sizeof(xieTecDecodeJPEGBaseline);
	pvt->decodeParms   = (pointer)dtec;
	dtec->interleave   = etec->interleave;
	dtec->upSample     = etec->interleave == xieValBandByPixel;
	dtec->bandOrder    = etec->bandOrder;
      }
      break;
    case xieValEncodeTIFF2: 
      {
	xieTecEncodeTIFF2 *etec = (xieTecEncodeTIFF2 *)pvt->encodeParms;
	xieTecDecodeTIFF2 *dtec;
	
	if (!(dtec = (xieTecDecodeTIFF2 *)
	      XieMalloc(sizeof(xieTecDecodeTIFF2))))
	  AllocError(flo,ped, return(FALSE));
	pvt->decodeNumber  = xieValDecodeTIFF2;
	pvt->decodeLen     = sizeof(xieTecDecodeTIFF2);
	pvt->decodeParms   = (pointer)dtec;
	dtec->normal       = TRUE;
	dtec->radiometric  = etec->radiometric;
	dtec->encodedOrder = etec->encodedOrder;
      }
      break;
    case xieValEncodeTIFFPackBits:
      {
	xieTecEncodeTIFFPackBits *etec = 
	  (xieTecEncodeTIFFPackBits *)pvt->encodeParms;
	xieTecDecodeTIFFPackBits *dtec;
	
	if (!(dtec = (xieTecDecodeTIFFPackBits *)
	      XieMalloc(sizeof(xieTecDecodeTIFFPackBits))))
	  AllocError(flo,ped, return(FALSE));
	pvt->decodeNumber  = xieValDecodeTIFFPackBits;
	pvt->decodeLen     = sizeof(xieTecDecodeTIFFPackBits);
	pvt->decodeParms   = (pointer)dtec;
	dtec->normal       = TRUE;
	dtec->encodedOrder = etec->encodedOrder;
      }
      break;
    case xieValEncodeJPEGLossless: /* not implemented in SI */
    default:
      ImplementationError(flo,ped, return(FALSE));
  }
  return(TRUE);
}

Bool CompareDecode(floDefPtr flo, peDefPtr ped)
{
  ePhotoDefPtr pvt = (ePhotoDefPtr)ped->elemPvt;
  peDefPtr  srcped = ped->inFloLst[IMPORT].srcDef;
  CARD16 decodeNumber, decodeLen;
  pointer decodeParms;

  decodeParms = GetImportTechnique(srcped,&decodeNumber,&decodeLen);
  if(decodeNumber != pvt->decodeNumber)
	return(FALSE);

  switch (decodeNumber) {
	    case xieValDecodeUncompressedSingle:
		{
		    xieTecDecodeUncompressedSingle *itec =
	     		(xieTecDecodeUncompressedSingle *)decodeParms;
		    xieTecDecodeUncompressedSingle *otec =
	     		(xieTecDecodeUncompressedSingle *)pvt->decodeParms;

		    return (itec->fillOrder   == otec->fillOrder   &&
        		    itec->pixelOrder  == otec->pixelOrder  &&
        		    itec->pixelStride == otec->pixelStride &&
        		    itec->leftPad     == otec->leftPad     && 
        		    itec->pixelStride == otec->pixelStride &&
        		    itec->scanlinePad == otec->scanlinePad);
		}
		break;
	    case xieValDecodeUncompressedTriple:
		{
		    xieTecDecodeUncompressedTriple *itec =
	     		(xieTecDecodeUncompressedTriple *)decodeParms;
		    xieTecDecodeUncompressedTriple *otec =
	     		(xieTecDecodeUncompressedTriple *)pvt->decodeParms;

		    return (itec->fillOrder      == otec->fillOrder      &&
        		    itec->pixelOrder     == otec->pixelOrder     &&
        		    itec->interleave     == otec->interleave     &&
        		    itec->bandOrder      == otec->bandOrder      &&
        		    itec->leftPad[0]     == otec->leftPad[0]     && 
        		    itec->leftPad[1]     == otec->leftPad[1]     && 
        		    itec->leftPad[2]     == otec->leftPad[2]     && 
        		    itec->pixelStride[0] == otec->pixelStride[0] &&
        		    itec->pixelStride[1] == otec->pixelStride[1] &&
        		    itec->pixelStride[2] == otec->pixelStride[2] &&
        		    itec->scanlinePad[0] == otec->scanlinePad[0] &&
        		    itec->scanlinePad[1] == otec->scanlinePad[1] &&
        		    itec->scanlinePad[2] == otec->scanlinePad[2]);
		}
		break;
	    case xieValDecodeG31D: 
	    case xieValDecodeG32D:
	    case xieValDecodeG42D:
	    case xieValDecodeTIFF2:
		{
                    xieTecDecodeG31D *itec = (xieTecDecodeG31D *)decodeParms;
                    xieTecDecodeG31D *otec =
                        	(xieTecDecodeG31D *)pvt->decodeParms;

		    return (itec->normal       == otec->normal      &&
			    itec->radiometric  == otec->radiometric &&
			    itec->encodedOrder == otec->encodedOrder);
		}
		break;
	    case xieValDecodeJPEGBaseline:
	    case xieValDecodeJPEGLossless:
		{
                    xieTecDecodeJPEGBaseline *itec = 
				(xieTecDecodeJPEGBaseline *)decodeParms;
                    xieTecDecodeJPEGBaseline *otec =
                        	(xieTecDecodeJPEGBaseline *)pvt->decodeParms;
		    
		    return (itec->interleave == otec->interleave &&
			    itec->bandOrder  == otec->bandOrder  &&
			    itec->upSample   == otec->upSample);
		}
		break;
	    case xieValDecodeTIFFPackBits:
		{
                    xieTecDecodeTIFFPackBits *itec = 
				(xieTecDecodeTIFFPackBits *)decodeParms;
                    xieTecDecodeTIFFPackBits *otec =
                        	(xieTecDecodeTIFFPackBits *)pvt->decodeParms;

		    return (itec->normal       == otec->normal &&
			    itec->encodedOrder == otec->encodedOrder);
		}
		break;
	    default:
	        return (FALSE);
  }
}

/*------------------------------------------------------------------------
---------------------- routine: post execution cleanup -------------------
------------------------------------------------------------------------*/
static Bool DebriefEPhoto(floDefPtr flo, peDefPtr ped, Bool ok)
{
  xieFloExportPhotomap *raw = (xieFloExportPhotomap *)ped->elemRaw;
  ePhotoDefPtr pvt = (ePhotoDefPtr) ped->elemPvt;
  inFloPtr     inf = &ped->inFloLst[SRCtag];
  outFloPtr    src = &inf->srcDef->outFlo;
  photomapPtr  map;
  CARD32   b;
  
  if(!(pvt && (map = pvt->map))) return(FALSE);

  if(ok && map->refCnt > 1) {
    
    /* free old compression parameters and image data
     */
    if(map->tecParms)
      map->tecParms = (pointer)XieFree(map->tecParms);

    if(map->pvtParms)
      map->pvtParms = (pointer)XieFree(map->pvtParms);

    for(b = 0; b < map->bands; b++) 
      FreeStrips(&map->strips[b]);
    
    /* stash our new attributes and data into the photomap
     */
    map->bands = ped->outFlo.bands;
    map->dataType = (map->format[0].class & UNCONSTRAINED
		     ? xieValUnconstrained : xieValConstrained);
    map->technique = pvt->decodeNumber;
    map->lenParms  = pvt->decodeLen;
    map->tecParms  = pvt->decodeParms;
    map->pvtParms  = pvt->pvtParms;
    map->dataClass = src->bands == 3 ? xieValTripleBand : xieValSingleBand;
    for(b = 0; b < map->bands; ++b) {
        map->format[b] = ped->outFlo.format[b];
        DebriefStrips(&ped->outFlo.output[b],&map->strips[b]);
    }
    pvt->decodeParms = NULL;

    if (src->bands == 3  &&  map->bands == 1) {
      /*
       * save format for the other bands too, we'll need them when we decode
       */
      for(b = 1; b < src->bands; ++b) 
        map->format[b] = src->format[b];
    }
  }

  /* if server choice, free space used to hold fabricated encode parameters */
  if (pvt->serverChose && pvt->encodeParms)
	XieFree(pvt->encodeParms);

  /* Free decodeParms if something went afoul before hooking on to photomap */
  if (pvt->decodeParms)
	XieFree(pvt->decodeParms);

  /* free image data that's left over on our outFlo
   */
  for(b = 0; b < ped->outFlo.bands; b++)
    FreeStrips(&ped->outFlo.output[b]);
  
  /* 
    unbind ourself from the photomap
    */
  if(map->refCnt > 1)
    --map->refCnt;
  else if(LookupIDByType(raw->photomap, RT_PHOTOMAP))
    FreeResourceByType(map->ID, RT_PHOTOMAP, RT_NONE);
  else
    DeletePhotomap(map, map->ID);
  
  return(TRUE);
}                               /* end DebriefEPhoto */

/* end module ephoto.c */
