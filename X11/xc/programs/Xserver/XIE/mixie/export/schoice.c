/* $Xorg: schoice.c,v 1.4 2001/02/09 02:04:24 xorgcvs Exp $ */
/**** module schoice.c ****/
/****************************************************************************

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
****************************************************************************
 	schoice.c: Routines to handle server choice encoding 

	Dean Verheiden && Robert NC Shelley  AGE Logic, Inc.  Jan 1994
****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/export/schoice.c,v 3.6 2001/12/14 19:58:21 dawes Exp $ */

#define _XIEC_SCHOICE
#define _XIEC_IPHOTO

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
#include <XIE.h>
#include <XIEproto.h>
/*
 *  more X server includes.
 */
#include <misc.h>
#include <dixstruct.h>

/*
 *  Element Specific Includes
 */
#include <corex.h>
#include <macro.h>
#include <photomap.h>
#include <element.h>
#include <xiemd.h>
#include <memory.h>

static Bool PrepSCCanonic(floDefPtr flo, peDefPtr ped);
static Bool PrepSCSmuggle(floDefPtr flo, peDefPtr ped);
static Bool PrepSCPackBits(floDefPtr flo, peDefPtr ped);
static Bool PrepSCG42D(floDefPtr flo, peDefPtr ped);


extern Bool		BuildDecodeFromEncode();
extern pointer		GetImportTechnique();


/*  REFORMAT_UNCOMPRESSED	(options for prefer space)
 *	TRUE  = convert uncompressed import data to canonic form before saving
 *	FALSE = save uncompressed import data "as is"
 *
 *  COMPRESS_BITONAL
 *	PrepSCPackBits
 *	PrepSCG42D
 *
 *  DEFAULT_PREFERENCE
 *	xieValPreferSpace
 *	xieValPreferTime
 */
#define REFORMAT_UNCOMPRESSED	TRUE
#define COMPRESS_BITONAL	PrepSCPackBits
#define DEFAULT_PREFERENCE	xieValPreferTime


/*------------------------------------------------------------------------
----------------------- choose an encode technique -----------------------
------------------------------------------------------------------------*/
xieBoolProc GetServerChoice(floDefPtr flo, peDefPtr eped)
{
  ePhotoDefPtr           pvt = (ePhotoDefPtr)eped->elemPvt;
  xieFloExportPhotomap  *raw = (xieFloExportPhotomap *)eped->elemRaw;
  outFloPtr              dst = &eped->outFlo;
  inFloPtr               inf = &eped->inFloLst[SRCtag];
  peDefPtr              iped = inf->srcDef;
  Bool               smuggle = FALSE;
  formatPtr              fmt;
  xieBoolProc         scPrep;
  CARD8 b, bands, preference; 
  CARD16      tecNum, tecLen;
  pointer             import;

  import = GetImportTechnique(iped,&tecNum,&tecLen);

  if(raw->lenParams)
    preference = ((xieTecEncodeServerChoice*)&raw[1])->preference;
  else 
    preference = xieValPreferDefault;
  
  switch(preference) {
#if (DEFAULT_PREFERENCE == xieValPreferSpace)
  case xieValPreferDefault:
#endif
  case xieValPreferSpace:
    if(import) {
      switch(tecNum) {
#if  REFORMAT_UNCOMPRESSED
      case xieValDecodeUncompressedSingle:
	if(iped->outFlo.format[0].levels == 2)
	  scPrep = COMPRESS_BITONAL;
	else
	  scPrep = PrepSCCanonic;
	break;

      case xieValDecodeUncompressedTriple:
	scPrep = PrepSCCanonic;
	break;
#else
      case xieValDecodeUncompressedSingle:
      case xieValDecodeUncompressedTriple:
#endif
      case xieValDecodeG31D:
      case xieValDecodeG32D:
      case xieValDecodeG42D:
      case xieValDecodeJPEGBaseline:
      case xieValDecodeTIFF2:
      case xieValDecodeTIFFPackBits:
	smuggle = TRUE;			/* smuggle import encoding "as is" */
	scPrep  = PrepSCSmuggle;
	break;
      default:
	return((xieBoolProc)NULL);
      }
    } else if(iped->outFlo.bands == 1 && iped->outFlo.format[0].levels == 2) {
      scPrep = COMPRESS_BITONAL;
    } else {
      scPrep = PrepSCCanonic;
    }
    break;

#if (DEFAULT_PREFERENCE == xieValPreferTime)
  case xieValPreferDefault:
#endif
  case xieValPreferTime:
    scPrep = PrepSCCanonic;		/* keep uncompressed   */
    break;

  default:
    return((xieBoolProc)NULL);
  }
  /* grab a copy of the input attributes and propagate them to our output
   */   
  fmt   = smuggle ? iped->inFloLst[SRCtag].format : iped->outFlo.format;
  bands = smuggle ? iped->inFloLst[SRCtag].bands  : iped->outFlo.bands;

  for(b = 0; b < bands; ++b) {
    dst->format[b] = inf->format[b] = fmt[b];
  }
  dst->bands = inf->bands = bands;
  pvt->serverChose = TRUE;

  return(scPrep);
}


/*------------------------------------------------------------------------
---------------------- server choice prep routines -----------------------
------------------------------------------------------------------------*/
static Bool PrepSCCanonic(floDefPtr flo, peDefPtr ped)
{
  ePhotoDefPtr pvt = (ePhotoDefPtr)ped->elemPvt;

  pvt->encodeNumber = xieValEncodeServerChoice;
  pvt->encodeLen    = 0;
  pvt->congress     = TRUE;

  if(ped->inFloLst[SRCtag].bands == 1) {
    xieTecDecodeUncompressedSingle *dtec;
    
    if(!(dtec = ((xieTecDecodeUncompressedSingle *)
		 XieMalloc(sizeof(xieTecDecodeUncompressedSingle)))))
      AllocError(flo,ped, return(FALSE));

    pvt->decodeNumber = xieValDecodeUncompressedSingle;
    pvt->decodeParms  = (pointer)dtec;
    pvt->decodeLen    = sizeof(xieTecDecodeUncompressedSingle);
#if (IMAGE_BYTE_ORDER == MSBFirst) /* Conform to server's "native" format */
    dtec->fillOrder   = xieValMSFirst;
    dtec->pixelOrder  = xieValMSFirst; 
#else
    dtec->fillOrder   = xieValLSFirst;
    dtec->pixelOrder  = xieValLSFirst; 
#endif
    dtec->pixelStride = ped->outFlo.format[0].stride;
    dtec->leftPad     = 0;
    dtec->scanlinePad = PITCH_MOD >> 3;
  } else {
    xieTecDecodeUncompressedTriple *dtec;
    int i;
    
    if(!(dtec = ((xieTecDecodeUncompressedTriple *)
		 XieMalloc(sizeof(xieTecDecodeUncompressedTriple)))))
      AllocError(flo,ped, return(FALSE));

    pvt->decodeNumber = xieValDecodeUncompressedTriple;
    pvt->decodeParms  = (pointer)dtec;
    pvt->decodeLen    = sizeof(xieTecDecodeUncompressedTriple);
#if (IMAGE_BYTE_ORDER == MSBFirst) /* Conform to server's "native" format */
    dtec->fillOrder   = xieValMSFirst;
    dtec->pixelOrder  = xieValMSFirst; 
#else
    dtec->fillOrder   = xieValLSFirst;
    dtec->pixelOrder  = xieValLSFirst; 
#endif
    dtec->bandOrder   = xieValLSFirst;
    dtec->interleave  = xieValBandByPlane; 
    for(i = 0; i < 3; i++) {
      dtec->leftPad[i]     = 0;
      dtec->pixelStride[i] = ped->outFlo.format[i].stride;
      dtec->scanlinePad[i] = PITCH_MOD >> 3;
    }
  }
  return(TRUE);
} /* PrepSCCanonic */


static Bool PrepSCSmuggle(floDefPtr flo, peDefPtr ped)
{
  ePhotoDefPtr pvt = (ePhotoDefPtr)ped->elemPvt;
  peDefPtr    iped = ped->inFloLst[SRCtag].srcDef;
  pointer    parms;

  pvt->encodeNumber = xieValEncodeServerChoice;
  pvt->encodeLen    = 0;
  pvt->congress     = TRUE;

  if(!(parms = GetImportTechnique(iped,&pvt->decodeNumber,&pvt->decodeLen)) ||
     !(pvt->decodeParms = (pointer)XieMalloc(pvt->decodeLen)))
    return(FALSE);

  memcpy((char*)pvt->decodeParms, (char*)parms, (int)pvt->decodeLen);
  
  return(TRUE);
} /* PrepECSmuggle */


static Bool PrepSCPackBits(floDefPtr flo, peDefPtr ped)
{
  ePhotoDefPtr pvt = (ePhotoDefPtr)ped->elemPvt;
  formatPtr    fmt = ped->outFlo.format;
  xieTecEncodeTIFFPackBits *tp;
  
  pvt->encodeNumber = xieValEncodeTIFFPackBits;
  pvt->encodeLen    = sizeof(xieTecEncodeTIFFPackBits);

  if(!(tp = (xieTecEncodeTIFFPackBits*)XieMalloc(pvt->encodeLen)))
    return(FALSE);
  
  pvt->encodeParms = (pointer)tp;
  
  tp->encodedOrder = xieValMSFirst;
  fmt->interleaved = FALSE;
  fmt->class       = STREAM;
  
  return(BuildDecodeFromEncode(flo,ped));
} /* PrepSCPackBits */


static Bool PrepSCG42D(floDefPtr flo, peDefPtr ped)
{
  ePhotoDefPtr pvt = (ePhotoDefPtr)ped->elemPvt;
  formatPtr    fmt = ped->outFlo.format;
  xieTecEncodeG42D *tp;
  
  pvt->encodeNumber = xieValEncodeG42D;
  pvt->encodeLen    = sizeof(xieTecEncodeG42D);

  if(!(tp = (xieTecEncodeG42D*)XieMalloc(pvt->encodeLen)))
    return(FALSE);
  
  pvt->encodeParms = (pointer)tp;
  tp->encodedOrder = xieValMSFirst;
  tp->uncompressed = FALSE;
  tp->radiometric  = FALSE;
  fmt->interleaved = FALSE;
  fmt->class       = STREAM;
  
  return(BuildDecodeFromEncode(flo,ped));
} /* PrepSCG42D */

/* end module schoice.c */
