/* $Xorg: iphoto.c,v 1.4 2001/02/09 02:04:20 xorgcvs Exp $ */
/**** module iphoto.c ****/
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
  
	iphoto.c -- DIXIE routines for managing the ImportPhotomap element
  
	Robert NC Shelley -- AGE Logic, Inc. April 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/import/iphoto.c,v 3.5 2001/12/14 19:58:01 dawes Exp $ */

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
#include <dixie_i.h>
  /*
   *  Server XIE Includes
   */
#include <corex.h>
#include <error.h>
#include <macro.h>
#include <element.h>


/*
 *  routines internal to this module
 */
static Bool PrepIPhoto(floDefPtr flo, peDefPtr ped);
static Bool DebriefIPhoto(floDefPtr flo, peDefPtr ped, Bool ok);

/*
 * dixie entry points
 */
static diElemVecRec iPhotoVec = {
  PrepIPhoto,
  DebriefIPhoto
  };


/*------------------------------------------------------------------------
----------------- routine: make an import photomap element ---------------
------------------------------------------------------------------------*/
peDefPtr MakeIPhoto(floDefPtr flo, xieTypPhototag tag, xieFlo *pe)
{
  peDefPtr ped;
  ELEMENT(xieFloImportPhotomap);
  ELEMENT_SIZE_MATCH(xieFloImportPhotomap);
  
  if(!(ped = MakePEDef(1,(CARD32)stuff->elemLength<<2,sizeof(iPhotoDefRec)))) 
    FloAllocError(flo,tag,xieElemImportPhotomap, return(NULL));

  ped->diVec	    = &iPhotoVec;
  ped->phototag     = tag;
  ped->flags.import = TRUE;
  raw = (xieFloImportPhotomap *)ped->elemRaw;
  
  /*
   * copy the client element parameters (swap if necessary)
   */
  if( flo->reqClient->swapped ) {
    raw->elemType   = stuff->elemType;
    raw->elemLength = stuff->elemLength;
    cpswapl(stuff->photomap, raw->photomap);
    raw->notify = stuff->notify;
  }
  else
    memcpy((char *)raw, (char *)stuff, sizeof(xieFloImportPhotomap));

  return(ped);
}                               /* end MakeIPhoto */


/*------------------------------------------------------------------------
---------------- routine: prepare for analysis and execution -------------
------------------------------------------------------------------------*/
static Bool PrepIPhoto(floDefPtr flo, peDefPtr ped)
{
  xieFloImportPhotomap *raw = (xieFloImportPhotomap *)ped->elemRaw;
  iPhotoDefPtr pvt = (iPhotoDefPtr) ped->elemPvt;
  inFloPtr     inf = &ped->inFloLst[IMPORT];
  outFloPtr    dst = &ped->outFlo;
  photomapPtr  map;
  CARD32 b;

  /* find the photomap resource and bind it to our flo */
  if( !(map = (photomapPtr) LookupIDByType(raw->photomap, RT_PHOTOMAP)) )
	PhotomapError(flo,ped,raw->photomap, return(FALSE));
  ++map->refCnt;

  /* Load up a generic structure for importing photos from map and client */
  pvt->map = map;

  if(!map->bands)
    AccessError(flo,ped, return(FALSE));

  /* grab a copy of the input attributes and propagate them to our output */
  inf->bands = map->bands;

  /* copy map formats to inflo format */
  for(b = 0; b < inf->bands; b++) 
    inf->format[b] = map->format[b];

  /* also copy them to the outflo format, handling interleave if necessary  */
  dst->bands =  (map->dataClass == xieValTripleBand)? 3 : 
		(map->dataClass == xieValSingleBand)? 1 : 0;

  for(b = 0; b < dst->bands; b++)  {
    dst->format[b] = map->format[b];
    dst->format[b].interleaved = FALSE;
  }
  /* NOTE: the loop is over dst->bands,  not map->bands. This is because
   * dst->bands can be 3 when map->bands is 1. The in ephoto.c saves all 
   * the formats of the inflo in the photomap, so they are available now 
   * when we need them. interleaved is FALSE by definition because only
   * ExportPhotomap elements can *produce* interleaved data.
   */

  if (!UpdateFormatfromLevels(ped))
        ImplementationError(flo, ped, return(FALSE));

  return(TRUE);
}                               /* end PrepIPhoto */

/*------------------------------------------------------------------------
---------------------- routine: post execution cleanup -------------------
------------------------------------------------------------------------*/
static Bool DebriefIPhoto(floDefPtr flo, peDefPtr ped, Bool ok)
{
  xieFloImportPhotomap *raw = (xieFloImportPhotomap *)ped->elemRaw;
  iPhotoDefPtr pvt = (iPhotoDefPtr) ped->elemPvt;
  photomapPtr map;

  if(pvt && (map = (photomapPtr)pvt->map))
    if(map->refCnt > 1)
      --map->refCnt;
    else if(LookupIDByType(raw->photomap, RT_PHOTOMAP))
      FreeResourceByType(map->ID, RT_PHOTOMAP, RT_NONE);
    else
      DeletePhotomap(map, map->ID);
  pvt->map = (photomapPtr)NULL;

  return(TRUE);
}                               /* end DebriefIPhoto */

/*------------------------------------------------------------------------
------------------ routine: return import-private stuff  -----------------
------------------------------------------------------------------------*/
photomapPtr GetImportPhotomap(peDefPtr ped)
{
	return(((iPhotoDefPtr)ped->elemPvt)->map);
}

pointer GetImportTechnique(peDefPtr ped, CARD16 *num_ret, CARD16 *len_ret)
{
  switch(ped->elemRaw->elemType) {
  case xieElemImportPhotomap:
    {
      photomapPtr  imap;
  
      imap = GetImportPhotomap(ped);
      *num_ret = imap->technique;
      *len_ret = imap->lenParms;
      return(imap->tecParms);
    }
  case xieElemImportClientPhoto:
    {
      xieFloImportClientPhoto *icp = (xieFloImportClientPhoto*)ped->elemRaw;
      *num_ret = icp->decodeTechnique;
      *len_ret = icp->lenParams << 2;
      return((pointer)&icp[1]);
    }
  default:
    *num_ret = 0;
    *len_ret = 0;
    return(NULL);
  }
} /* GetImportTechnique */

/* end module iphoto.c */
