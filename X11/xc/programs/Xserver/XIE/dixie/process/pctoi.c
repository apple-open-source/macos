/* $Xorg: pctoi.c,v 1.4 2001/02/09 02:04:21 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module pctoi.c ****/
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
  
	pctoi.c -- DIXIE routines for managing the ConvertToIndex element
  
	Dean Verheiden && Robert NC Shelley -- AGE Logic, Inc. June 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/process/pctoi.c,v 3.6 2001/12/14 19:58:05 dawes Exp $ */

#define _XIEC_PCTOI
#define _XIEC_PCI

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
   *  more X server includes.
   */
#include <scrnintstr.h>
#include <colormapst.h>
  /*
   *  Server XIE Includes
   */
#include <corex.h>
#include <error.h>
#include <macro.h>
#include <colorlst.h>
#include <element.h>
#include <technq.h>
#include <difloat.h>
#include <memory.h>


/* routines internal to this module
 */
static Bool PrepConvertToIndex(floDefPtr flo, peDefPtr ped);
static Bool DebriefConvertToIndex(floDefPtr flo, peDefPtr ped, Bool ok);

/* dixie entry points
 */
static diElemVecRec pCtoIVec = {
    PrepConvertToIndex,
    DebriefConvertToIndex
    };


/*------------------------------------------------------------------------
----------------- routine: make an ExportPhotomap element ----------------
------------------------------------------------------------------------*/
peDefPtr MakeConvertToIndex(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
  peDefPtr ped;
  inFloPtr inFlo;
  ELEMENT(xieFloConvertToIndex);
  ELEMENT_AT_LEAST_SIZE(xieFloConvertToIndex);
  ELEMENT_NEEDS_1_INPUT(src);
  
  if(!(ped = MakePEDef(1,(CARD32)stuff->elemLength<<2,sizeof(pCtoIDefRec)))) 
    FloAllocError(flo,tag,xieElemConvertToIndex, return(NULL));

  ped->diVec	     = &pCtoIVec;
  ped->phototag      = tag;
  ped->flags.process = TRUE;
  raw = (xieFloConvertToIndex *)ped->elemRaw;
  /*
   * copy the standard client element parameters (swap if necessary)
   */
  if( flo->reqClient->swapped ) {
    raw->elemType   = stuff->elemType;
    raw->elemLength = stuff->elemLength;
    cpswaps(stuff->src, raw->src);
    raw->notify     = stuff->notify;
    cpswapl(stuff->colormap, raw->colormap);
    cpswapl(stuff->colorList, raw->colorList);
    cpswaps(stuff->colorAlloc, raw->colorAlloc);
    cpswaps(stuff->lenParams, raw->lenParams);
  }
  else  
    memcpy((char *)raw, (char *)stuff, sizeof(xieFloConvertToIndex));
  /*
   * assign phototags to inFlos
   */
  inFlo = ped->inFloLst;
  inFlo[SRCtag].srcTag = raw->src;

  /*
   * copy technique data (if any)
   */
  if(!(ped->techVec = FindTechnique(xieValColorAlloc,raw->colorAlloc)) ||
     !(ped->techVec->copyfnc(flo, ped, &stuff[1], &raw[1], raw->lenParams, 
					raw->colorAlloc == xieValDefault)))
    TechniqueError(flo,ped,xieValColorAlloc,raw->colorAlloc,raw->lenParams,
		   return(ped));

  return(ped);
}                               /* end MakeConvertToIndex */

/*------------------------------------------------------------------------
-----------------------  copy routines for techniques  -------------------
------------------------------------------------------------------------*/

#undef  sparms
#define sparms ((xieTecColorAllocAll *)sParms)
#undef  rparms
#define rparms ((xieTecColorAllocAll *)rParms)

Bool CopyCtoIAllocAll(TECHNQ_COPY_ARGS)
{
  pTecCtoIDefPtr pvt;

  VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, isDefault);

  if(!(ped->techPvt=(pointer)XieMalloc(sizeof(pTecCtoIDefRec))))
    FloAllocError(flo,ped->phototag,xieElemConvertToIndex, return(TRUE));

  pvt = (pTecCtoIDefPtr)ped->techPvt;

  pvt->defTech = isDefault;

  if (isDefault) 
    pvt->fill = 0;	/* Not really a good way to pick this so . . . */
  else if( flo->reqClient->swapped ) {
    cpswapl(sparms->fill, pvt->fill);
  } else
    pvt->fill = sparms->fill;
  
  return(TRUE);
}

#ifdef BEYOND_SI

#undef  sparms
#define sparms ((xieTecColorAllocMatch *)sParms)
#undef  rparms
#define rparms ((xieTecColorAllocMatch *)rParms)

Bool CopyCtoIAllocMatch(TECHNQ_COPY_ARGS)
{
  pConvertToIndexMatchDefPtr pvt;

  VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, isDefault);
  
  if (!(ped->techPvt=(pointer )XieMalloc(sizeof(pTecConvertToIndexMatchDefRec))))
    AllocError(flo,ped, return(TRUE));
  
  pvt = (pConvertToIndexMatchDefPtr)ped->techPvt;
  
  if( flo->reqClient->swapped ) {
    pvt->matchLimit = ConvertFromIEEE(lswapl(sparms->matchLimit));
    pvt->grayLimit  = ConvertFromIEEE(lswapl(sparms->grayLimit));
  } else {
    pvt->matchLimit = ConvertFromIEEE(sparms->matchLimit);
    pvt->grayLimit  = ConvertFromIEEE(sparms->grayLimit);
  }
  return(TRUE);
}

#undef  sparms
#define sparms ((xieTecColorAllocRequantize *)sParms)
#undef  rparms
#define rparms ((xieTecColorAllocRequantize *)rParms)

Bool CopyCtoIAllocRequantize(TECHNQ_COPY_ARGS)
{
  VALIDATE_TECHNIQUE_SIZE(ped->techVec, tsize, isDefault);
  
  if( flo->reqClient->swapped ){
    cpswapl(sparms->maxCells, rparms->maxCells);
  } else
    rparms->maxCells = sparms->maxCells;
  
  return(TRUE);
}
#endif /* BEYOND_SI */


/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/
static Bool PrepConvertToIndex(floDefPtr flo, peDefPtr ped)
{
  xieFloConvertToIndex *raw = (xieFloConvertToIndex *)ped->elemRaw;
  pCtoIDefPtr pvt = (pCtoIDefPtr) ped->elemPvt;
  inFloPtr    inf = &ped->inFloLst[SRCt1];
  outFloPtr   src = &inf->srcDef->outFlo;
  outFloPtr   dst = &ped->outFlo;
  formatPtr   sf  = &src->format[0];
  formatPtr   df  = &dst->format[0];
  CARD32      b;

  /* must be constrained and inter-band dimensions must match
   */
  if(IsntConstrained(sf[0].class) ||
    ((src->bands == 3) && (IsntConstrained(sf[1].class)  ||
			 IsntConstrained(sf[2].class)  ||
			 sf[0].width  != sf[1].width  ||
			 sf[1].width  != sf[2].width  ||
			 sf[0].height != sf[1].height ||
			 sf[1].height != sf[2].height)))
    MatchError(flo,ped, return(FALSE));

  /* determine our output attributes from the input (figure out levels later)
   */
  df[0] = sf[0];
  dst->bands = 1;
  inf->bands = src->bands;
  for(b = 0; b < src->bands; ++b)
    inf->format[b] = sf[b];

  /* find the ColorList and Colormap resources
   */
  if(raw->colorList) {
    if(!(pvt->list = LookupColorList(raw->colorList)))
      ColorListError(flo,ped,raw->colorList, return(FALSE));
    if(pvt->list->refCnt != 1)
      AccessError(flo,ped, return(FALSE));
    ++pvt->list->refCnt;
  } else {
    pvt->list = NULL;
  }
  if(!(pvt->cmap = (ColormapPtr) LookupIDByType(raw->colormap, RT_COLORMAP)))
    ColormapError(flo,ped,raw->colormap, return(FALSE));

  /* grab attributes from colormap, visual, ...
   */
  pvt->class   = pvt->cmap->class;
  pvt->visual  = pvt->cmap->pVisual;
  pvt->stride  = pvt->visual->bitsPerRGBValue;
  pvt->cells   = pvt->visual->ColormapEntries;
  pvt->mask[0] = pvt->visual->redMask;
  pvt->mask[1] = pvt->visual->greenMask;
  pvt->mask[2] = pvt->visual->blueMask;
  pvt->shft[0] = pvt->visual->offsetRed;
  pvt->shft[1] = pvt->visual->offsetGreen;
  pvt->shft[2] = pvt->visual->offsetBlue;
  pvt->dynamic = pvt->cmap->class & DynamicClass;
  pvt->graySrc = src->bands == 1;
  pvt->preFmt  = pvt->doHist = FALSE;

  switch(pvt->class) {
  case DirectColor :
  case TrueColor   :
  case StaticColor :
    for(b = 0; b < 3; ++b)
      pvt->levels[b] = pvt->mask[b] >> pvt->shft[b];
    
    /* see if we have a full set of masks (by turning them into levels)
     */
    if(pvt->levels[0]++ & pvt->levels[1]++ & pvt->levels[2]++) {
      /* see what limitations we have for grayscale images */
      if(pvt->graySrc && !pvt->dynamic && (pvt->levels[0] != pvt->levels[1] ||
					   pvt->levels[1] != pvt->levels[2]))
	MatchError(flo,ped, return(FALSE));

      /* set output levels and depth based on colormap masks
       */
      df[0].levels = pvt->levels[0] * pvt->levels[1] * pvt->levels[2];
      SetDepthFromLevels(df[0].levels, pvt->depth);
      break;
    }                     /* for StaticColor with no asks, we'll fall thru */
  case PseudoColor :
    pvt->levels[1] = pvt->levels[2] = 1;
  case GrayScale   :
  case StaticGray  :
    /* set output levels and depth based on colormap size
     */
    SetDepthFromLevels(pvt->cells, pvt->depth);
    df[0].levels = 1<<pvt->depth;
    if((pvt->preFmt = !pvt->graySrc) && pvt->class <= GrayScale)
      MatchError(flo,ped, return(FALSE));
  }
  /* set output stride and pitch to match the colormap depth
   */
  if(!UpdateFormatfromLevels(ped))
    MatchError(flo,ped, return(FALSE));

  /* go do technique-specific stuff
   */
  if(!(ped->techVec->prepfnc(flo, ped, raw, &raw[1])))
    TechniqueError(flo,ped,xieValColorAlloc,raw->colorAlloc,raw->lenParams,
		   return(FALSE));

  /* init the colorlist resource
   */
  if(pvt->list) {
    ResetColorList(pvt->list, pvt->list->mapPtr);
    pvt->list->mapID  = raw->colormap;
    pvt->list->mapPtr = pvt->cmap;
    pvt->list->client = flo->runClient;
  }
  return(TRUE);
}                               /* end PrepConvertToIndex */

/*------------------------------------------------------------------------
------------------------ technique prep routines  ------------------------
------------------------------------------------------------------------*/
Bool PrepCtoIAllocAll(
     floDefPtr flo,
     peDefPtr  ped,
     xieFloConvertToIndex *raw,
     xieTecColorAllocAll  *tec)
{
  pCtoIDefPtr pvt = (pCtoIDefPtr) ped->elemPvt;
  inFloPtr     inf = &ped->inFloLst[SRCtag];
  formatPtr    fmt = &inf->format[0];
  
  if(!(pvt->class & DynamicClass) || !pvt->list)
    return(FALSE);	/* AllocAll needs a dynamic colormap and a colorlist */
  
  /* Check the depth of each band to make sure they're reasonable ... deep
   * images have to have sparse histograms to avoid running out of colors
   *
   *     XIE only supports up to 16 bits per band for "non-index" data,
   *     we will further limit RGB images to a total depth of 31.
   */
  if((inf->bands == 1 &&  fmt[0].depth > 16) ||
     (inf->bands == 3 && (fmt[0].depth + fmt[1].depth + fmt[2].depth > 31)))
    return(FALSE);
  
  return(TRUE);
}				/* end PrepCtoIAllocAll */

/*------------------------------------------------------------------------
---------------------- routine: post execution cleanup -------------------
------------------------------------------------------------------------*/
static Bool DebriefConvertToIndex(floDefPtr flo, peDefPtr ped, Bool ok)
{
  pCtoIDefPtr pvt = (pCtoIDefPtr) ped->elemPvt;
  colorListPtr lst;
  
  if(pvt && (lst = pvt->list))
    if(lst->refCnt > 1) {
      if(!ok || !lst->cellCnt)
	ResetColorList(lst, lst->mapPtr);
      --lst->refCnt;
    } else if(LookupIDByType(lst->ID, RT_COLORLIST)) {
      FreeResourceByType(lst->ID, RT_COLORLIST, RT_NONE);
    } else {
      DeleteColorList(lst, lst->ID);
    }
  return(TRUE);
}                               /* end DebriefConvertToIndex */

/* end module pctoi.c */
