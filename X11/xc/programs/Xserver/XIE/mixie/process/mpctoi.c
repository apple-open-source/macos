/* $Xorg: mpctoi.c,v 1.4 2001/02/09 02:04:30 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module mpctoi.c ****/
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
  
	mpctoi.c -- DDXIE ConvertToIndex element
  
	Robert NC Shelley -- AGE Logic, Inc. July, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/process/mpctoi.c,v 3.6 2001/12/14 19:58:44 dawes Exp $ */

#define _XIEC_MPCTOI
#define _XIEC_PCTOI

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
#include <scrnintstr.h>
#include <colormapst.h>
/*
 *  Server XIE Includes
 */
#include <error.h>
#include <macro.h>
#include <colorlst.h>
#include <element.h>
#include <texstr.h>
#include <xiemd.h>
#include <technq.h>
#include <memory.h>


extern int AllocColor();  /* in . . . server/dix/colormap.c */


/* routines referenced by other DDXIE modules
 */
int	miAnalyzeCvtToInd();

/* routines used internal to this module
 */
static int CreateCtoIAll();
static int InitializeCtoIAll();
static int DoGrayCtoIAll();
static int DoRGB1CtoIAll();
static int DoRGB3CtoIAll();
static int DoRGB3CtoIAll();
static int ResetCtoIAll();
static int DestroyCtoI();

static int allocDirect();
static int allocGray1();
static int allocGray3();
static int allocRGB1();
static int allocRGB3();
static pointer cvt();


/* DDXIE ConvertToIndex entry points
 */
static ddElemVecRec mpCtoIAllVec = {
  CreateCtoIAll,
  InitializeCtoIAll,
  (xieIntProc)NULL,
  (xieIntProc)NULL,
  ResetCtoIAll,
  DestroyCtoI
  };


/* Local Declarations.
 */
#define HASH_POINT 14         /* use hash table if sum of depth exceeds this */

#define HR  19		      /* prime hash multipliers (sum also prime)     */
#define HG  23
#define HB  17

#define NADA (xieVoidProc)NULL

typedef struct _ctihash {
  CARD32	 rgbVal;
  Pixel		 pixdex;
} ctiHashRec, *ctiHashPtr;


typedef struct _mpctiall {
  xieVoidProc	 action;		   /* scanline action routine        */
  xieVoidProc	 action2;		   /* pass 2 scanline action routine */
  ColormapPtr    cmap;			   /* colormap-id		     */
  int		 cmapFull;		   /* == Success until allocs fail   */
  int		 clindex;		   /* index of client doing allocs   */
  int	       (*alloc)();		   /* color allocation routine	     */
  Pixel		*pixLst;		   /* list of alloc'd pixels         */
  CARD32	 pixCnt;		   /* count of alloc'd pixels (total)*/
  CARD32	 allocMatch;		   /* count of alloc'd/matched pixels*/
  CARD32	 shareMatch;		   /* count of shared/matched pixels */
  CARD32	 width;			   /* image width		     */
  CARD32	 fill;			   /* value to use if alloc fails    */
  BOOL		 hashing;		   /* true if using hash table	     */
  CARD8 	 trim[xieValMaxBands];     /* # LS bits to trim from pixels  */
  CARD32	 mask[xieValMaxBands];     /* mask for keeping useful bits   */
  CARD32         shft[xieValMaxBands];     /* crazy-pixel shift counts       */
  float		 coef[xieValMaxBands];     /* scale pixel up to 16 bits      */
  CARD32	 tmpLen[xieValMaxBands];   /* length of tmpLsts...           */
  pointer        tmpLst[xieValMaxBands];   /* lists where we remember stuff  */
  Bool		 tmpSet;		   /* initialize tmpLsts to 0 or ~0  */
  pointer	 auxbuf[xieValMaxBands];   /* format-class conversion buffers*/
  CARD8		 iclass[xieValMaxBands];   /* input format classes	     */
  CARD8		 cclass;		   /* conversion format class	     */
} ctiAllRec, *ctiAllPtr;


/*****************************************************************************
 *
 * Convert to Index alloc-All action routines:
 * CtoIall_bmctio
 *         |||||`-- output format class (b=bit, B=byte, P=pair, Q=quad)
 *         ||||`--- input  format class (b=bit, B=byte, P=pair, Q=quad)
 *         |||`---- L:lookup, H:hash, U:usage map
 *         ||`----- class colormap: d=dynamic, s=static
 *         |`------ 1 or 3 map colormap (blank if it supports both)
 *         `------- 1 or 3 band image
 */
static void CtoIall_1_dLBB(), CtoIall_1_dLBP(), CtoIall_1_dLBQ();
static void CtoIall_1_dLPB(), CtoIall_1_dLPP(), CtoIall_1_dLPQ();
static void CtoIall_31dLBB(), CtoIall_31dLBP();
static void CtoIall_31dLPB(), CtoIall_31dLPP();
static void CtoIall_31dHBB(), CtoIall_31dHBP();
static void CtoIall_31dHPB(), CtoIall_31dHPP();

static void CtoIall_33dUB_(), CtoIall_33dUP_();

static void CtoIall_33dLBB(), CtoIall_33dLBP(), CtoIall_33dLBQ();
static void CtoIall_33dLPB(), CtoIall_33dLPP(), CtoIall_33dLPQ();

/* input bits are promoted to bytes, so they share the same action routines
 */
#define     CtoIall_1_dLbB    CtoIall_1_dLBB
#define     CtoIall_1_dLbP    CtoIall_1_dLBP
#define     CtoIall_1_dLbQ    CtoIall_1_dLBQ
#define     CtoIall_31dLbB    CtoIall_31dLBB
#define     CtoIall_31dLbP    CtoIall_31dLBP
#define     CtoIall_31dHbB    CtoIall_31dHBB
#define     CtoIall_31dHbP    CtoIall_31dHBP
#define     CtoIall_33dUb_    CtoIall_33dUB_
#define     CtoIall_33dLbB    CtoIall_33dLBB
#define     CtoIall_33dLbP    CtoIall_33dLBP
#define     CtoIall_33dLbQ    CtoIall_33dLBQ


/* single band image, single or triple dynamic colormap, linear lookup tables
 */
static void (*gray_action[4][3])() = {
  NADA,          NADA,          NADA,         	  /* out=b, in=b..P */
  CtoIall_1_dLbB, CtoIall_1_dLBB, CtoIall_1_dLPB, /* out=B, in=b..P */
  CtoIall_1_dLbP, CtoIall_1_dLBP, CtoIall_1_dLPP, /* out=P, in=b..P */
  CtoIall_1_dLbQ, CtoIall_1_dLBQ, CtoIall_1_dLPQ, /* out=Q, in=b..P */
};

/* triple band image, single dynamic colormap, linear or hash tables
 */
static void (*rgb1_action[2][4][3])() = {
  NADA,           NADA,           NADA,		  /* lut,  out=b, in=b..P */
  CtoIall_31dLbB, CtoIall_31dLBB, CtoIall_31dLPB, /* lut,  out=B, in=b..P */
  CtoIall_31dLbP, CtoIall_31dLBP, CtoIall_31dLPP, /* lut,  out=P, in=b..P */
  NADA,           NADA,           NADA,		  /* lut,  out=Q, in=b..P */
  NADA,           NADA,           NADA,		  /* hash, out=b, in=b..P */
  CtoIall_31dHbB, CtoIall_31dHBB, CtoIall_31dHPB, /* hash, out=B, in=b..P */
  CtoIall_31dHbP, CtoIall_31dHBP, CtoIall_31dHPP, /* hash, out=P, in=b..P */
  NADA,           NADA,           NADA,		  /* hash, out=Q, in=b..P */
};

/* triple band image, triple dynamic colormap, map usage (Boolean histogram)
 */
static void (*rgb3_action_usage[3])() = {
  CtoIall_33dUb_, CtoIall_33dUB_, CtoIall_33dUP_, /* usage map out, in=b..P */
};

/* triple band image, triple dynamic colormap, linear lookup tables
 */
static void (*rgb3_action_remap[4][3])() = {
  NADA,           NADA,           NADA,		  /* out=b, in=b..P */
  CtoIall_33dLbB, CtoIall_33dLBB, CtoIall_33dLPB, /* out=B, in=b..P */
  CtoIall_33dLbP, CtoIall_33dLBP, CtoIall_33dLPP, /* out=P, in=b..P */
  CtoIall_33dLbQ, CtoIall_33dLBQ, CtoIall_33dLPQ, /* out=Q, in=b..P */
};


/*------------------------------------------------------------------------
------------------- see if we can handle this element --------------------
------------------------------------------------------------------------*/
int miAnalyzeCvtToInd(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  xieFloConvertToIndex *raw = (xieFloConvertToIndex *)ped->elemRaw;

  /* stash the appropriate entry point vector in the peDef
   */
  switch(ped->techVec->number) {
  case xieValColorAllocAll:	ped->ddVec = mpCtoIAllVec;	break;
  case xieValColorAllocMatch:
  case xieValColorAllocRequantize:
  default: TechniqueError(flo, ped, xieValColorAlloc,
			  raw->colorAlloc, raw->lenParams, return(FALSE));
  }
  return(TRUE);
}                               /* end miAnalyzeCvtToInd */


/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateCtoIAll(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  pCtoIDefPtr dix = (pCtoIDefPtr) ped->elemPvt;
  Bool  band_sync = !dix->graySrc || dix->class != DirectColor;

  return(MakePETex(flo, ped, sizeof(ctiAllRec), NO_SYNC, band_sync));
}                               /* end CreateCtoIAll */


/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeCtoIAll(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  xieFloConvertToIndex  *raw = (xieFloConvertToIndex *)ped->elemRaw;
  xieTecColorAllocAll   *tec = (xieTecColorAllocAll *) &raw[1];
  pTecCtoIDefPtr         pvt = (pTecCtoIDefPtr) ped->techPvt;
  peTexPtr		 pet = ped->peTex;
  formatPtr		 ift = &ped->inFloLst[SRCtag].format[0];
  formatPtr		 oft = &ped->outFlo.format[0];
  pCtoIDefPtr		 dix = (pCtoIDefPtr) ped->elemPvt;
  ctiAllPtr		 ddx = (ctiAllPtr)   pet->private;
  CARD8       depth,   bands = dix->graySrc ? 1 : 3;
  CARD32 ic = BYTE_PIXEL, oc = oft->class;
  CARD32 b, size;
  
  /* init generic color allocation params
   */
  ddx->cmap        = dix->cmap;
  ddx->clindex     = dix->list->client->index;
  ddx->fill	   = pvt->fill;
  ddx->width       = oft->width;
  ddx->cmapFull    = 0;
  ddx->pixCnt      = 0;
  ddx->allocMatch  = 0;
  ddx->shareMatch  = 0;
  if(!(ddx->pixLst = (Pixel*) XieCalloc((dix->cells + 1) * sizeof(Pixel))))
    AllocError(flo,ped, return(FALSE));
  
  /* examine input data format-classes
   */
  for(b = 0; b < bands; ++b) {
    ddx->iclass[b] = ift[b].class;
    ic = max(ddx->iclass[b],ic);
  }
  ddx->cclass = ic;
  size = (ic == BYTE_PIXEL ? sz_BytePixel : sz_PairPixel) >> 3;

  /* init format-class and pixel to RGB cell conversion parameters
   */
  for(b = 0; b < bands; ++b) {
    if(ift[b].class != ic &&
       !(ddx->auxbuf[b] = (pointer) XieMalloc((ift->width+7)*size)))
      AllocError(flo,ped, return(FALSE));

    ddx->trim[b] = ift[b].depth > dix->stride ? ift[b].depth - dix->stride : 0;
    ddx->mask[b] = (1 << ift[b].depth - ddx->trim[b]) - 1;
    ddx->coef[b] = 65535.0 / ((ift[b].levels >> ddx->trim[b]) - 1);
  }    
  /* init stuff specific to the image class and visual class
   */
  if(dix->graySrc) {
    /*
     * grayscale image, visual class doesn't matter
     */
    ddx->tmpSet    = TRUE;
    ddx->tmpLen[0] = (ddx->mask[0] + 1) * sizeof(Pixel);
    ddx->action    = gray_action[oc-1][ic-1];
    ddx->alloc	   = (pvt->defTech ? dix->class <= PseudoColor
		      ? allocGray1 : allocGray3 : AllocColor);
    if(ddx->alloc == allocGray3)
       ddx->pixLst[dix->cells] = ~0;
    ped->ddVec.activate = DoGrayCtoIAll;
    
  } else if(dix->class <= PseudoColor) {
    /*
     * RGB image, visual class has a single channel colormap
     */
    for(depth = 0, b = 0; b < xieValMaxBands; ++b) {
      SetDepthFromLevels(ddx->mask[b]+1,size);
      ddx->shft[b] = depth;
      depth += size;
    }
    /* if we have too many levels, we'll have to use a hash table
     */
    if(ddx->hashing  = depth > HASH_POINT) {
      ddx->tmpLen[0] = (HR+HG+HB) * (dix->cells + 1) * sizeof(ctiHashRec);
      ddx->tmpSet    = FALSE;
    } else {
      ddx->tmpLen[0] = (1<<depth) * sizeof(Pixel);
      ddx->tmpSet    = TRUE;
    }
    bands       = 1;	    /* only 1 colormap band */
    ddx->action = rgb1_action[ddx->hashing ? 1 : 0][oc-1][ic-1];
    ddx->alloc	= pvt->defTech ? allocRGB1 : AllocColor;
    ped->ddVec.activate = DoRGB1CtoIAll;
    
  } else {
    /*
     * RGB image, visual class has a three channel colormap
     */
    for(b = 0; b < bands; ++b) {
      SetDepthFromLevels(ddx->mask[b]+1,depth);
      ddx->tmpLen[b] = (1<<depth) * sizeof(Pixel);
    }
    ddx->tmpSet  = FALSE;
    ddx->action  = rgb3_action_usage[ic-1];
    ddx->action2 = rgb3_action_remap[oc-1][ic-1];
    ddx->alloc	 = pvt->defTech ? allocRGB3 : AllocColor;
    ped->ddVec.activate = DoRGB3CtoIAll;
  }
  if(!ddx->action)  ImplementationError(flo,ped, return(FALSE));
  
  /* alloc/init whatever temporary storage we need
   */
  for(b = 0; b < bands; ++b) {
    if(!(ddx->tmpLst[b] = (pointer ) XieMalloc(ddx->tmpLen[b])))
      AllocError(flo,ped, return(FALSE));
    memset((char*)ddx->tmpLst[b],(char)(ddx->tmpSet ? ~0 : 0),ddx->tmpLen[b]);
  }
  return(InitReceptors(flo, ped, NO_DATAMAP, 1) &&
	 InitEmitter(flo, ped, NO_DATAMAP, NO_INPLACE));
}                               /* end InitializeCtoIAll */


/*------------------------------------------------------------------------
------------------------- crank some input data --------------------------
------------------------------------------------------------------------*/
static int DoGrayCtoIAll(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  xieFloConvertToIndex *raw = (xieFloConvertToIndex *)ped->elemRaw;
  ctiAllPtr ddx = (ctiAllPtr)   pet->private;
  bandPtr iband = &pet->receptor[SRCtag].band[0];
  bandPtr oband = &pet->emitter[0];
  pointer ivoid, ovoid;
  
  if(Resumed(flo,pet) &&
     (flo->runClient->clientGone ||
      ddx->cmap != (ColormapPtr) LookupIDByType(raw->colormap, RT_COLORMAP)))
    ColormapError(flo,ped,raw->colormap, return(FALSE));

  if((ivoid = GetCurrentSrc(flo,pet,iband)) &&
     (ovoid = GetCurrentDst(flo,pet,oband)))
  do {
    if(ddx->auxbuf[0]) ivoid = cvt(ivoid, ddx, (CARD8)0);

    (*ddx->action)(ddx, ovoid, ivoid);

    ivoid = GetNextSrc(flo,pet,iband,FLUSH);
    ovoid = GetNextDst(flo,pet,oband,FLUSH);
  } while(ivoid && ovoid);

  FreeData(flo,pet,iband,iband->current);

  return(TRUE);
}                               /* end DoGrayCtoIAll */


/*------------------------------------------------------------------------
------------------------- crank some input data --------------------------
------------------------------------------------------------------------*/
static int DoRGB1CtoIAll(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  xieFloConvertToIndex *raw = (xieFloConvertToIndex *)ped->elemRaw;
  ctiAllPtr ddx = (ctiAllPtr)   pet->private;
  bandPtr iband = &pet->receptor[SRCtag].band[0];
  bandPtr oband = &pet->emitter[0];
  pointer ovoid, ivoid0, ivoid1, ivoid2;
  
  if(Resumed(flo,pet) &&
     (flo->runClient->clientGone ||
      ddx->cmap != (ColormapPtr) LookupIDByType(raw->colormap, RT_COLORMAP)))
    ColormapError(flo,ped,raw->colormap, return(FALSE));

  ovoid  = GetCurrentDst(flo,pet,oband);
  ivoid0 = GetCurrentSrc(flo,pet,iband); iband++;
  ivoid1 = GetCurrentSrc(flo,pet,iband); iband++;
  ivoid2 = GetCurrentSrc(flo,pet,iband); iband -= 2;
  
  while(ovoid && ivoid0 && ivoid1 && ivoid2) {

    if(ddx->auxbuf[0]) ivoid0 = cvt(ivoid0, ddx, (CARD8)0);
    if(ddx->auxbuf[1]) ivoid1 = cvt(ivoid1, ddx, (CARD8)1);
    if(ddx->auxbuf[2]) ivoid2 = cvt(ivoid2, ddx, (CARD8)2);

    (*ddx->action)(ddx, ovoid, ivoid0, ivoid1, ivoid2);

    ovoid  = GetNextDst(flo,pet,oband,FLUSH);
    ivoid0 = GetNextSrc(flo,pet,iband,FLUSH); iband++;
    ivoid1 = GetNextSrc(flo,pet,iband,FLUSH); iband++;
    ivoid2 = GetNextSrc(flo,pet,iband,FLUSH); iband -= 2;
  }
  FreeData(flo,pet,iband,iband->current); iband++;
  FreeData(flo,pet,iband,iband->current); iband++;
  FreeData(flo,pet,iband,iband->current);
  
  return(TRUE);
}                               /* end DoRGB1CtoIAll */


/*------------------------------------------------------------------------
------------------------- crank some input data --------------------------
------------------------------------------------------------------------*/
static int DoRGB3CtoIAll(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  bandPtr   iband = &pet->receptor[SRCtag].band[0];
  ctiAllPtr   ddx = (ctiAllPtr)   pet->private;
  
  if(ddx->action) {
    BOOL  final = TRUE;
    pointer ivoid;
    int   b;

    /* PASS 1: generate per-band usage maps of the colors needed
     */
    for(b = 0; b < xieValMaxBands; b++, iband++) {
      for(ivoid = GetCurrentSrc(flo,pet,iband); ivoid;
	  ivoid = GetNextSrc(flo,pet,iband,KEEP)) {

	if(ddx->auxbuf[b]) ivoid = cvt(ivoid, ddx, (CARD8)b);

	(*ddx->action)(ddx, ivoid, b);
      }
      /* if we're done with the band, go back to the first scanline, otherwise
       *  increase the threshold to keep the scheduler out of our hair
       */
      if(iband->final)
	iband->current = 0;
      else {
	final = FALSE;
	SetBandThreshold(iband, iband->current + 1);
      }
    }
    /* now that we know what we need, it's time to allocate colors.
     * we'll continue with PASS 2 when we return from the scheduler
     */
    if(final) {
      ddx->action = (xieVoidProc)NULL;
      return(allocDirect(flo,ped,pet,ddx));
    }
  } else {
    bandPtr oband = &pet->emitter[0];
    pointer ovoid, ivoid0, ivoid1, ivoid2;
    
    /* PASS 2: map src pixesl to allocated colors
     */
    ivoid0 = GetCurrentSrc(flo,pet,iband); iband++;
    ivoid1 = GetCurrentSrc(flo,pet,iband); iband++;
    ivoid2 = GetCurrentSrc(flo,pet,iband); iband -= 2;
    ovoid  = GetCurrentDst(flo,pet,oband);
    
    while(ovoid && ivoid0 && ivoid1 && ivoid2) {

      if(ddx->auxbuf[0]) ivoid0 = cvt(ivoid0, ddx, (CARD8)0);
      if(ddx->auxbuf[1]) ivoid1 = cvt(ivoid1, ddx, (CARD8)1);
      if(ddx->auxbuf[2]) ivoid2 = cvt(ivoid2, ddx, (CARD8)2);

      (*ddx->action2)(ddx, ovoid, ivoid0, ivoid1, ivoid2);
      
      ivoid0 = GetNextSrc(flo,pet,iband,FLUSH); iband++;
      ivoid1 = GetNextSrc(flo,pet,iband,FLUSH); iband++;
      ivoid2 = GetNextSrc(flo,pet,iband,FLUSH); iband -= 2;
      ovoid  = GetNextDst(flo,pet,oband,FLUSH);
    }
    FreeData(flo,pet,iband,iband->current); iband++;
    FreeData(flo,pet,iband,iband->current); iband++;
    FreeData(flo,pet,iband,iband->current);
  }  
  return(TRUE);
}                               /* end DoRGB3CtoIAll */


/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetCtoIAll(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  xieFloConvertToIndex *raw = (xieFloConvertToIndex *)ped->elemRaw;
  pCtoIDefPtr  dix = (pCtoIDefPtr) ped->elemPvt;
  ctiAllPtr    ddx = (ctiAllPtr)   ped->peTex->private;
  colorListPtr lst = dix->list;
  CARD32       b, i, j;
  /*
   * if we've got pixels, they might need to be transfered into the ColorList
   */
  if((lst->cellPtr = ddx->pixLst) && (lst->cellCnt = ddx->pixCnt)) {

    if(dix->class <= PseudoColor) {
      for(i = j = 0; i < ddx->pixCnt; ++j)
        if(lst->cellPtr[j])
           lst->cellPtr[i++] = (Pixel)j;

    } else if(dix->graySrc && ddx->alloc == AllocColor) {
      Pixel p, *ppix = (Pixel*) ddx->tmpLst[0];
      
      for(i = 0; i < ddx->pixCnt; ++ppix)
	if((INT32)(p = *ppix) >= 0)
	  lst->cellPtr[i++] = p;

    } /* else pixels are already in place */
  }
  if(raw->notify && !ferrCode(flo) && !flo->flags.aborted &&
     (ddx->cmapFull || ddx->allocMatch || ddx->shareMatch))
    SendColorAllocEvent(flo, ped, lst->mapID, raw->colorAlloc,
			ddx->pixCnt - ddx->allocMatch +	(ddx->cmapFull
			? 0 : ddx->shareMatch + ddx->allocMatch << 16));
  
  for(b = 0; b < xieValMaxBands; ++b) {
    if(ddx->tmpLst[b]) ddx->tmpLst[b] = (pointer ) XieFree(ddx->tmpLst[b]);
    if(ddx->auxbuf[b]) ddx->auxbuf[b] = (pointer ) XieFree(ddx->auxbuf[b]);
  }
  ddx->pixLst = NULL;  
  ddx->pixCnt = 0;
  ResetReceptors(ped);
  ResetEmitter(ped);
  
  return(TRUE);
}                               /* end ResetCtoIAll */


/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyCtoI(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* get rid of the peTex structure  */
  ped->peTex = (peTexPtr) XieFree(ped->peTex);
  
  /* zap this element's entry point vector */
  ped->ddVec.create     = (xieIntProc)NULL;
  ped->ddVec.initialize = (xieIntProc)NULL;
  ped->ddVec.activate   = (xieIntProc)NULL;
  ped->ddVec.reset      = (xieIntProc)NULL;
  ped->ddVec.destroy    = (xieIntProc)NULL;
  
  return(TRUE);
}                               /* end DestroyCtoI */


/*------------------------------------------------------------------------
------------- allocate DirectColors based on usage map -------------------
------------------------------------------------------------------------*/
static int allocDirect(flo,ped,pet,ddx)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
     ctiAllPtr ddx;
{
  xieFloConvertToIndex *raw = (xieFloConvertToIndex *)ped->elemRaw;
  pCtoIDefPtr	    dix = (pCtoIDefPtr) ped->elemPvt;
  formatPtr	    fmt = &ped->inFloLst[SRCtag].format[0];
  unsigned short     value[xieValMaxBands];
  int   	  b, next[xieValMaxBands];
  Bool        final, done[xieValMaxBands];
  Pixel       *pix, *used[xieValMaxBands];
  
  if(Resumed(flo,pet) &&
     (flo->runClient->clientGone ||
      ddx->cmap != (ColormapPtr)LookupIDByType(raw->colormap,RT_COLORMAP)))
    ColormapError(flo,ped,raw->colormap, return(FALSE));
  
  /* find the first color needed in each usage map
   */
  for(b = 0; b < xieValMaxBands; ++b) {
    done[b] = FALSE;
    next[b] = 0;
    used[b] = (Pixel*)ddx->tmpLst[b];
    while(!(used[b][next[b]])) ++next[b];
  }
  do {
    /* plant the current round of RGB values
     */
    for(b = 0; b < xieValMaxBands; ++b)
      if(!done[b])
	 value[b] = next[b] * ddx->coef[b];
    
    /* alloc a triplet of color cells
     */
    pix = &ddx->pixLst[ddx->pixCnt];

    if( ddx->cmapFull ||
       (ddx->cmapFull = (*ddx->alloc)(ddx->cmap,&value[0],&value[1],&value[2],
				      pix,ddx->clindex,ddx)))
      *pix = ddx->fill;
    else
      ddx->cmapFull = ++ddx->pixCnt > dix->cells;
    
    /* save the result and find the next color needed in each usage map
     */
    for(final = TRUE, b = 0; b < xieValMaxBands; ++b)
      if(!done[b]) {
	used[b][next[b]] = *pix & dix->mask[b];
	while(!(done[b]  = ++next[b] >= fmt[b].levels >> ddx->trim[b]) &&
	      !(used[b][next[b]]));
	final &= done[b];
      }
  } while(!final);

  return(TRUE);
}

/*------------------------------------------------------------------------
------------- allocate closest match for Default Technique ---------------
------------------------------------------------------------------------*/
static int allocGray1(cmap, red, grn, blu, pix, client, ddx)
     ColormapPtr cmap;
     CARD16	*red, *grn, *blu;
     Pixel	*pix;
     int	 client;
     ctiAllPtr	 ddx;
{
  int status;

  if(status = AllocColor(cmap, red, grn, blu, pix, client)) {
    xColorItem rgb;

    rgb.pixel = 0;
    rgb.red   = rgb.green = rgb.blue = *red;
    FakeAllocColor(cmap, &rgb);

    if(!ddx->pixLst[(*pix = rgb.pixel)]++) {
      xrgb match;

      QueryColors(cmap,1,&rgb.pixel,&match);
      FakeFreeColor(cmap, rgb.pixel);
      *red = match.red;
      *grn = match.green;
      *blu = match.blue;
      if(status = AllocColor(cmap, red, grn, blu, pix, client))
	ddx->pixLst[rgb.pixel] = 0;
      else
	ddx->allocMatch++;
    } else {
      FakeFreeColor(cmap, rgb.pixel);
      ++ddx->shareMatch;
      --ddx->pixCnt;
      status = 0;
    }
  } else {
    ++ddx->pixLst[*pix];
  }
  return(status);
}

static int allocGray3(cmap, red, grn, blu, pix, client, ddx)
     ColormapPtr cmap;
     CARD16	*red, *grn, *blu;
     Pixel	*pix;
     int	 client;
     ctiAllPtr	 ddx;
{
  int status;

  if(ddx->pixLst[ddx->pixCnt]) return(BadAlloc);

  if(status = AllocColor(cmap, red, grn, blu, pix, client)) {
    xColorItem rgb;
    xrgb     match;

    rgb.pixel = 0;
    rgb.red   = rgb.green = rgb.blue = *red;
    FakeAllocColor(cmap, &rgb);

    QueryColors(cmap,1,&rgb.pixel,&match);
    FakeFreeColor(cmap, rgb.pixel);
    *pix = rgb.pixel;
    *red = match.red;
    *grn = match.green;
    *blu = match.blue;
    if(!(status = AllocColor(cmap, red, grn, blu, pix, client)))
      ddx->allocMatch++;
  }
  ddx->pixLst[ddx->pixCnt] = *pix;
  return(status);
}

static int allocRGB1(cmap, red, grn, blu, pix, client, ddx)
     ColormapPtr cmap;
     CARD16	*red, *grn, *blu;
     Pixel	*pix;
     int	 client;
     ctiAllPtr	 ddx;
{
  int status;

  if(status = AllocColor(cmap, red, grn, blu, pix, client)) {
    xColorItem rgb;

    rgb.pixel = 0;
    rgb.red   = *red;
    rgb.green = *grn;
    rgb.blue  = *blu;
    FakeAllocColor(cmap, &rgb);

    if(!ddx->pixLst[(*pix = rgb.pixel)]++) {
      xrgb match;

      QueryColors(cmap,1,&rgb.pixel,&match);
      FakeFreeColor(cmap, rgb.pixel);
      *red = match.red;
      *grn = match.green;
      *blu = match.blue;
      if(status = AllocColor(cmap, red, grn, blu, pix, client))
	ddx->pixLst[rgb.pixel] = 0;
      else
	ddx->allocMatch++;
    } else {
      if(!ddx->hashing ||
	 *ddx->tmpLen > ddx->shareMatch * sizeof(ctiHashRec) << 1) {
	++ddx->shareMatch;
	--ddx->pixCnt;
	status = 0;
      }
      FakeFreeColor(cmap, rgb.pixel);
    }
  } else {
    ++ddx->pixLst[*pix];
  }
  return(status);
}

static int allocRGB3(cmap, red, grn, blu, pix, client, ddx)
     ColormapPtr cmap;
     CARD16	*red, *grn, *blu;
     Pixel	*pix;
     int	 client;
     ctiAllPtr	 ddx;
{
  int status;

  if(status = AllocColor(cmap, red, grn, blu, pix, client)) {
    xColorItem rgb;
    xrgb     match;

    rgb.pixel = 0;
    rgb.red   = *red;
    rgb.green = *grn;
    rgb.blue  = *blu;
    FakeAllocColor(cmap, &rgb);

    QueryColors(cmap,1,&rgb.pixel,&match);
    FakeFreeColor(cmap, rgb.pixel);
    *pix = rgb.pixel;
    *red = match.red;
    *grn = match.green;
    *blu = match.blue;
    if(!(status = AllocColor(cmap, red, grn, blu, pix, client)))
      ddx->allocMatch++;
  }
  return(status);
}

/*------------------------------------------------------------------------
------------- convert bits to bytes or pairs, or bytes to pairs ----------
------------------------------------------------------------------------*/
static pointer cvt(src, ddx, band)
     pointer	 src;
     ctiAllPtr	 ddx;
     CARD8       band;
{
  if(ddx->iclass[band] == BIT_PIXEL) {
    if(ddx->cclass == BYTE_PIXEL) {
      bitexpand(src,ddx->auxbuf[band],ddx->width,(BytePixel)0,(BytePixel)1);
      
    } else /* BIT_PIXEL --> PAIR_PIXEL */ {
      LogInt	*i = (LogInt *) src, ival, M;
      PairPixel	*o = (PairPixel *) ddx->auxbuf[band];
      int	bw = ddx->width, nw = bw >> LOGSHIFT;

      while(nw--)
	for(ival = *i++, M = LOGLEFT; M; LOGRIGHT(M))
	  *o++ = (ival & M) ? (PairPixel) 1 : (PairPixel) 0;
      if(bw &= LOGMASK)
	for(ival = *i, M = LOGLEFT; bw--; LOGRIGHT(M))
	  *o++ = (ival & M) ? (PairPixel) 1 : (PairPixel) 0;
    }
  } else /* BYTE_PIXEL --> PAIR_PIXEL */ {
    CARD32  i, width = ddx->width;
    BytePixel *ip = (BytePixel*)src;
    PairPixel *op = (PairPixel*)ddx->auxbuf[band];
    
    for(i = 0; i < width; *op++ = *ip++, ++i);
  }
  return(ddx->auxbuf[band]);
}

/*------------------------------------------------------------------------
-- action routines: single band image, single or triple dynamic colormap -
------------------------------------------------------------------------*/
#define DO_GRAY_CtoI_ALL(fn_do,itype,otype)				      \
static void fn_do(ddx, DST, SRC)  					      \
  ctiAllPtr ddx; pointer DST,SRC; 					      \
{									      \
  itype *src = (itype *)SRC;						      \
  otype *dst = (otype *)DST;						      \
  Pixel  px, *pp, *lst = (Pixel *)ddx->tmpLst[0];			      \
  CARD32  w, val, mask = ddx->mask[0], trim = ddx->trim[0];		      \
  CARD16  r, g, b;							      \
  for(w = ddx->width; w--; *dst++ = px) {				      \
    if((INT32)(px = *(pp = &lst[(val = *src++ >> trim & mask)])) < 0) {       \
      if(!ddx->cmapFull) {						      \
	r = g = b = (unsigned short)((float)val * ddx->coef[0]);	      \
	if(!(ddx->cmapFull = (*ddx->alloc)(ddx->cmap,&r,&g,&b,pp,	      \
					   ddx->clindex,ddx))) {	      \
	  ++ddx->pixCnt;						      \
	  px = *pp;							      \
	  continue;							      \
	}								      \
      }									      \
      px = ddx->fill;							      \
    }									      \
  }									      \
}
DO_GRAY_CtoI_ALL(CtoIall_1_dLBB, BytePixel, BytePixel)
DO_GRAY_CtoI_ALL(CtoIall_1_dLBP, BytePixel, PairPixel)
DO_GRAY_CtoI_ALL(CtoIall_1_dLBQ, BytePixel, QuadPixel)
DO_GRAY_CtoI_ALL(CtoIall_1_dLPB, PairPixel, BytePixel)
DO_GRAY_CtoI_ALL(CtoIall_1_dLPP, PairPixel, PairPixel)
DO_GRAY_CtoI_ALL(CtoIall_1_dLPQ, PairPixel, QuadPixel)


/*------------------------------------------------------------------------
--- triple band, single dynamic colormap, sum of depths <= HASH_POINT ----
------------------------------------------------------------------------*/
#define DO_RGB31L_CtoI_ALL(fn_do,itype,otype)				      \
static void fn_do(ddx, DST, SRCR, SRCG, SRCB)				      \
  ctiAllPtr ddx; pointer DST,SRCR,SRCG,SRCB;				      \
{									      \
  itype  *srcR = (itype*)SRCR, *srcG = (itype*)SRCG, *srcB = (itype*)SRCB;    \
  otype  *dst  = (otype*)DST;						      \
  CARD32 Rmask = ddx->mask[0], Rtrim = ddx->trim[0];			      \
  CARD32 Gmask = ddx->mask[1], Gtrim = ddx->trim[1];			      \
  CARD32 Bmask = ddx->mask[2], Btrim = ddx->trim[2];			      \
  CARD32 Gshft = ddx->shft[1], Bshft = ddx->shft[2];			      \
  Pixel  px, *pp, *lst = (Pixel*)ddx->tmpLst[0];			      \
  CARD32 rv, gv, bv, w;							      \
  CARD16 r, g, b;							      \
  for(w = ddx->width; w--; *dst++ = px) {				      \
    rv  = *srcR++ >> Rtrim & Rmask;					      \
    gv  = *srcG++ >> Gtrim & Gmask;					      \
    bv  = *srcB++ >> Btrim & Bmask;					      \
    if((INT32)(px = *(pp = &lst[rv | gv<<Gshft | bv<<Bshft])) < 0) {	      \
      if(!ddx->cmapFull) {					      	      \
	r = (unsigned short)((float)rv * ddx->coef[0]);			      \
	g = (unsigned short)((float)gv * ddx->coef[1]);			      \
	b = (unsigned short)((float)bv * ddx->coef[2]);			      \
	if(!(ddx->cmapFull = (*ddx->alloc)(ddx->cmap,&r,&g,&b,pp,	      \
					   ddx->clindex,ddx))) {	      \
	  ++ddx->pixCnt;						      \
	  px = *pp;							      \
	  continue;							      \
	}								      \
      }									      \
      px = ddx->fill;							      \
    }									      \
  }									      \
}
DO_RGB31L_CtoI_ALL(CtoIall_31dLBB,BytePixel,BytePixel)
DO_RGB31L_CtoI_ALL(CtoIall_31dLBP,BytePixel,PairPixel)
DO_RGB31L_CtoI_ALL(CtoIall_31dLPB,PairPixel,BytePixel)
DO_RGB31L_CtoI_ALL(CtoIall_31dLPP,PairPixel,PairPixel)


/*------------------------------------------------------------------------
--- triple band, single dynamic colormap, sum of depths > HASH_POINT -----
------------------------------------------------------------------------*/
#define DO_RGB31H_CtoI_ALL(fn_do,itype,otype)				      \
static void fn_do(ddx, DST, SRCR, SRCG, SRCB)				      \
  ctiAllPtr ddx; pointer DST,SRCR,SRCG,SRCB;				      \
{									      \
  itype  *srcR = (itype*)SRCR, *srcG = (itype*)SRCG, *srcB = (itype*)SRCB;    \
  otype  *dst  = (otype*)DST;						      \
  CARD32 Rmask = ddx->mask[0], Rtrim = ddx->trim[0];			      \
  CARD32 Gmask = ddx->mask[1], Gtrim = ddx->trim[1];			      \
  CARD32 Bmask = ddx->mask[2], Btrim = ddx->trim[2];			      \
  CARD32 width = ddx->width;						      \
  ctiHashPtr  hash, list = (ctiHashPtr)ddx->tmpLst[0];			      \
  CARD16      r, g, b;							      \
  union{ CARD32 rgbGlob; CARD8 rgbVals[4]; } rgb;			      \
  rgb.rgbVals[3] = 1;							      \
  while(width--) {							      \
    hash = &list[HR * (rgb.rgbVals[0] = *srcR++ >> Rtrim & Rmask) +	      \
		 HG * (rgb.rgbVals[1] = *srcG++ >> Gtrim & Gmask) +	      \
		 HB * (rgb.rgbVals[2] = *srcB++ >> Btrim & Bmask)];	      \
    while(hash->rgbVal && hash->rgbVal != rgb.rgbGlob) ++hash;		      \
    if(hash->rgbVal) { *dst++ = hash->pixdex; continue; }		      \
    if(!ddx->cmapFull) {						      \
      r = (unsigned short)((float)rgb.rgbVals[0] * ddx->coef[0]);	      \
      g = (unsigned short)((float)rgb.rgbVals[1] * ddx->coef[1]);	      \
      b = (unsigned short)((float)rgb.rgbVals[2] * ddx->coef[2]);	      \
      if(!(ddx->cmapFull = (*ddx->alloc)(ddx->cmap,&r,&g,&b,&hash->pixdex,    \
					 ddx->clindex,ddx))) {		      \
	++ddx->pixCnt;							      \
	*dst++ = hash->pixdex;						      \
	hash->rgbVal = rgb.rgbGlob;					      \
	continue;							      \
      }									      \
    }									      \
    *dst++ = ddx->fill;							      \
  }									      \
}
DO_RGB31H_CtoI_ALL(CtoIall_31dHBB,BytePixel,BytePixel)
DO_RGB31H_CtoI_ALL(CtoIall_31dHBP,BytePixel,PairPixel)
DO_RGB31H_CtoI_ALL(CtoIall_31dHPB,PairPixel,BytePixel)
DO_RGB31H_CtoI_ALL(CtoIall_31dHPP,PairPixel,PairPixel)


/*------------------------------------------------------------------------
------------- action routines for triple band, triple colormap -----------
------------- U? routines generate usage maps of src pixels    -----------
------------- L? routines re-map src pixels thru lookup tables -----------
------------------------------------------------------------------------*/
#define DO_RGB33U_CtoI_ALL(fn_do,itype)					      \
static void fn_do(ddx, SRC, band)  					      \
  ctiAllPtr ddx; pointer SRC; CARD8 band;				      \
{									      \
  itype   *src = (itype *)SRC;						      \
  CARD32  mask = ddx->mask[band];					      \
  CARD32  trim = ddx->trim[band];					      \
  Pixel  *used = (Pixel*)ddx->tmpLst[band];				      \
  CARD32  w    = ddx->width;						      \
  while(w--) used[*src++ >> trim & mask] = TRUE;			      \
}
DO_RGB33U_CtoI_ALL(CtoIall_33dUB_, BytePixel)
DO_RGB33U_CtoI_ALL(CtoIall_33dUP_, PairPixel)


#define DO_RGB33L_CtoI_ALL(fn_do,itype,otype)				      \
static void fn_do(ddx, DST, SRCR, SRCG, SRCB) 				      \
  ctiAllPtr ddx; pointer DST,SRCR,SRCG,SRCB;				      \
{									      \
  itype  *srcR = (itype *)SRCR;						      \
  itype  *srcG = (itype *)SRCG;						      \
  itype  *srcB = (itype *)SRCB;						      \
  otype  *dst  = (otype *)DST;						      \
  CARD32 Rmask = ddx->mask[0], Rtrim = ddx->trim[0];			      \
  CARD32 Gmask = ddx->mask[1], Gtrim = ddx->trim[1];			      \
  CARD32 Bmask = ddx->mask[2], Btrim = ddx->trim[2];			      \
  Pixel  *Rlut = (Pixel*)ddx->tmpLst[0];				      \
  Pixel  *Glut = (Pixel*)ddx->tmpLst[1];				      \
  Pixel  *Blut = (Pixel*)ddx->tmpLst[2];				      \
  CARD32  w    = ddx->width;						      \
  while(w--) *dst++ = (Rlut[*srcR++ >> Rtrim & Rmask] |			      \
		       Glut[*srcG++ >> Gtrim & Gmask] |			      \
		       Blut[*srcB++ >> Btrim & Bmask]);			      \
}
DO_RGB33L_CtoI_ALL(CtoIall_33dLBB, BytePixel, BytePixel)
DO_RGB33L_CtoI_ALL(CtoIall_33dLBP, BytePixel, PairPixel)
DO_RGB33L_CtoI_ALL(CtoIall_33dLBQ, BytePixel, QuadPixel)
DO_RGB33L_CtoI_ALL(CtoIall_33dLPB, PairPixel, BytePixel)
DO_RGB33L_CtoI_ALL(CtoIall_33dLPP, PairPixel, PairPixel)
DO_RGB33L_CtoI_ALL(CtoIall_33dLPQ, PairPixel, QuadPixel)

/* end module mpctoi.c */
