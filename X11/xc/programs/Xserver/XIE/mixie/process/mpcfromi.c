/* $Xorg: mpcfromi.c,v 1.4 2001/02/09 02:04:30 xorgcvs Exp $ */
/**** module mpcfromi.c ****/
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
  
	mpcfromi.c -- DDXIE ConvertFromIndex element
  
	Robert NC Shelley -- AGE Logic, Inc. July, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/process/mpcfromi.c,v 3.5 2001/12/14 19:58:43 dawes Exp $ */

#define _XIEC_MPCFROMI
#define _XIEC_PCFROMI

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
#include <xiemd.h>
#include <texstr.h>
#include <memory.h>

extern int QueryColors(); /* in ...server/dix/colormap.c */

/* routines referenced by other DDXIE modules
 */
int	miAnalyzeCvtFromInd();

/* routines used internal to this module
 */
static int CreateCfromI();
static int InitCfromI();
static int DoSingleCfromI();
static int DoTripleCfromI();
static int ResetCfromI();
static int DestroyCfromI();


/* DDXIE ConvertFromIndex entry points
 */
static ddElemVecRec mpCfromIVec = {
  CreateCfromI,
  InitCfromI,
  (xieIntProc)NULL,
  (xieIntProc)NULL,
  ResetCfromI,
  DestroyCfromI
  };


/* Local Declarations.
 */
typedef struct _mpcfromi {
  pCfromIDefPtr  dix;
  xieIntProc	 action;
  bandPtr        iband;
  bandPtr        oband;
  Pixel         *pixLst;
  xrgb          *rgbLst;
  CARD32	 width;
  pointer	ibuf;
  pointer	obuf[xieValMaxBands];
} mpCfromIRec, *mpCfromIPtr;

/* action routines
 */
#define    CfromI_1bb CfromI_1BB
#define    CfromI_1bB CfromI_1BB
#define    CfromI_1bP CfromI_1BP
#define    CfromI_1Bb CfromI_1BB
#define    CfromI_1Pb CfromI_1PB
#define    CfromI_1Qb CfromI_1QB
#define    CfromI_3bb CfromI_3BB
#define    CfromI_3bB CfromI_3BB
#define    CfromI_3bP CfromI_3BP
#define    CfromI_3Bb CfromI_3BB
#define    CfromI_3Pb CfromI_3PB
#define    CfromI_3Qb CfromI_3QB
static int CfromI_1BB(), CfromI_1BP(), CfromI_3BB(), CfromI_3BP();
static int CfromI_1PB(), CfromI_1PP(), CfromI_3PB(), CfromI_3PP();
static int CfromI_1QB(), CfromI_1QP(), CfromI_3QB(), CfromI_3QP();

static int (*action_CfromI[2][3][4])() = {
  CfromI_1bb, CfromI_1Bb, CfromI_1Pb, CfromI_1Qb, /* single, o=1, i=1..4 */
  CfromI_1bB, CfromI_1BB, CfromI_1PB, CfromI_1QB, /* single, o=2, i=1..4 */
  CfromI_1bP, CfromI_1BP, CfromI_1PP, CfromI_1QP, /* single, o=3, i=1..4 */
  CfromI_3bb, CfromI_3Bb, CfromI_3Pb, CfromI_3Qb, /* triple, o=1, i=1..4 */
  CfromI_3bB, CfromI_3BB, CfromI_3PB, CfromI_3QB, /* triple, o=2, i=1..4 */
  CfromI_3bP, CfromI_3BP, CfromI_3PP, CfromI_3QP, /* triple, o=3, i=1..4 */
};


/*------------------------------------------------------------------------
------------------- see if we can handle this element --------------------
------------------------------------------------------------------------*/
int miAnalyzeCvtFromInd(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* stash the entry point vector with the appropriate activate routine
   */
  ped->ddVec = mpCfromIVec;

  return(TRUE);
}                               /* end miAnalyzeCvtFromInd */

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateCfromI(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  return(MakePETex(flo, ped, sizeof(mpCfromIRec), NO_SYNC, NO_SYNC));
}                               /* end CreateCfromI */


/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitCfromI(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  xieFloConvertFromIndex *raw = (xieFloConvertFromIndex *)ped->elemRaw;
  peTexPtr      pet =  ped->peTex;
  formatPtr     ift = &ped->inFloLst[SRCtag].format[0];
  formatPtr     oft = &ped->outFlo.format[0];
  pCfromIDefPtr dix = (pCfromIDefPtr) ped->elemPvt;
  mpCfromIPtr   ddx = (mpCfromIPtr)pet->private;
  CARD32  i, pseudo = !dix->pixMsk[0];
  CARD32 cells, odx =  ped->outFlo.bands == 1 ? 0 : 1;
  CARD8          oc = oft->class, ic = ift->class;
  Pixel  *p;
  xrgb   *rgb;
  
  /* set up action parameters
   */
  ddx->dix    = dix;
  ddx->width  = oft->width;
  ddx->iband  = &pet->receptor[SRCtag].band[0];
  ddx->oband  = &pet->emitter[0];
  ddx->action = action_CfromI[odx][oc-1][ift->class-1];
  if(!ddx->action) ImplementationError(flo,ped, return(FALSE));
  ped->ddVec.activate = !odx ? DoSingleCfromI : DoTripleCfromI;

  if(ic == BIT_PIXEL && !(ddx->ibuf = (BytePixel*)XieMalloc(ddx->width+7)))
    AllocError(flo,ped, return(FALSE));

  if(oc == BIT_PIXEL)
    if( odx && (!(ddx->obuf[0] = (BytePixel*)XieMalloc(ddx->width+7))  ||
		!(ddx->obuf[1] = (BytePixel*)XieMalloc(ddx->width+7))  ||
		!(ddx->obuf[2] = (BytePixel*)XieMalloc(ddx->width+7))) ||
       !odx &&  !(ddx->obuf[0] = (BytePixel*)XieMalloc(ddx->width+7)))
      AllocError(flo,ped, return(FALSE));
  
  /* snapshot the current contents of the colormap
   */
  SetDepthFromLevels(dix->cells,i); cells = 1<<i;
  if(!(ddx->pixLst = (Pixel*) XieMalloc(cells * sizeof(Pixel))) ||
     !(ddx->rgbLst = (xrgb *) XieMalloc(cells * sizeof(xrgb))))
    AllocError(flo,ped, return(FALSE));
  for(p = ddx->pixLst, i = 0; i < cells; ++i)
    *p++ = pseudo ? i : (i << dix->pixPos[0] & dix->pixMsk[0] |
                         i << dix->pixPos[1] & dix->pixMsk[1] |
                         i << dix->pixPos[2] & dix->pixMsk[2]);
  if(QueryColors(dix->cmap,cells,ddx->pixLst,ddx->rgbLst))
    ColormapError(flo,ped,raw->colormap, return(FALSE));   /* XXX hmmm? */

  /* adjust the RGB values according to the client's precision requirements
   */
  for(rgb = ddx->rgbLst, i = 0; i < cells; ++rgb, ++i) {
    rgb->red   >>= dix->precShift;
    rgb->green >>= dix->precShift;
    rgb->blue  >>= dix->precShift;
  }
    
  return(InitReceptors(flo, ped, NO_DATAMAP, 1) &&
         InitEmitter(flo, ped, NO_DATAMAP, NO_INPLACE));
}                               /* end InitCfromI */


/*------------------------------------------------------------------------
------------------------- crank some input data --------------------------
------------------------------------------------------------------------*/
static int DoSingleCfromI(flo,ped,pet)	/* one band out */
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  mpCfromIPtr  ddx = (mpCfromIPtr) pet->private;
  bandPtr    iband = ddx->iband;
  bandPtr    oband = ddx->oband;
  CARD32     width = iband->format->width;
  pointer src, dst;
  
  if((src = GetCurrentSrc(flo,pet,iband)) &&
     (dst = GetCurrentDst(flo,pet,oband)))
    do {
      if(ddx->ibuf) src = bitexpand(src,ddx->ibuf,width,(char)1,(char)0);

      (*ddx->action)(ddx, src, ddx->obuf[0] ? ddx->obuf[0] : dst);

      if(ddx->obuf[0]) bitshrink(ddx->obuf[0],dst,width,(char)1);

      src = GetNextSrc(flo,pet,iband,FLUSH);
      dst = GetNextDst(flo,pet,oband,FLUSH);
    } while(src && dst);
    
  FreeData(flo,pet,iband,iband->current);

  return(TRUE);
}                               /* end DoSingleCfromI */

static int DoTripleCfromI(flo,ped,pet)	/* three bands out */
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  mpCfromIPtr  ddx = (mpCfromIPtr) pet->private;
  bandPtr    iband = ddx->iband;
  bandPtr    oband = ddx->oband;
  CARD32     width = iband->format->width;
  pointer src, dstR, dstG, dstB;
  
  src  = GetCurrentSrc(flo,pet,iband);
  dstR = GetCurrentDst(flo,pet,oband); oband++;
  dstG = GetCurrentDst(flo,pet,oband); oband++;
  dstB = GetCurrentDst(flo,pet,oband); oband -=2;

  while(src && dstR && dstG && dstB) {
        
    if(ddx->ibuf) src = bitexpand(src,ddx->ibuf,width,(char)1,(char)0);

    (*ddx->action)(ddx, src,
		   ddx->obuf[0] ? ddx->obuf[0] : dstR,
		   ddx->obuf[1] ? ddx->obuf[1] : dstG,
		   ddx->obuf[2] ? ddx->obuf[2] : dstB);

    if(ddx->obuf[0]) bitshrink(ddx->obuf[0],dstR,width,(char)1);
    if(ddx->obuf[1]) bitshrink(ddx->obuf[1],dstG,width,(char)1);
    if(ddx->obuf[2]) bitshrink(ddx->obuf[2],dstB,width,(char)1);

    src  = GetNextSrc(flo,pet,iband,FLUSH);
    dstR = GetNextDst(flo,pet,oband,FLUSH); oband++;
    dstG = GetNextDst(flo,pet,oband,FLUSH); oband++;
    dstB = GetNextDst(flo,pet,oband,FLUSH); oband -=2;
  }
  FreeData(flo,pet,iband,iband->current);

  return(TRUE);
}                               /* end DoTripleCfromI */



/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetCfromI(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  mpCfromIPtr ddx = (mpCfromIPtr) ped->peTex->private;

  if(ddx->pixLst ) ddx->pixLst  = (Pixel*) XieFree(ddx->pixLst );
  if(ddx->rgbLst ) ddx->rgbLst  = (xrgb *) XieFree(ddx->rgbLst );
  if(ddx->ibuf   ) ddx->ibuf    = (pointer ) XieFree(ddx->ibuf   );
  if(ddx->obuf[0]) ddx->obuf[0] = (pointer ) XieFree(ddx->obuf[0]);
  if(ddx->obuf[1]) ddx->obuf[1] = (pointer ) XieFree(ddx->obuf[1]);
  if(ddx->obuf[2]) ddx->obuf[2] = (pointer ) XieFree(ddx->obuf[2]);

  ResetReceptors(ped);
  ResetEmitter(ped);
  
  return(TRUE);
}                               /* end ResetCfromI */


/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyCfromI(flo,ped)
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
}                               /* end DestroyCfromI */

/* Single band output action routines:
 */

#define DO_SINGLE_CFROMI(fn_do,itype,otype)			\
static int fn_do(ddx,SRC,DST)					\
  mpCfromIPtr ddx; pointer SRC; pointer DST;			\
{								\
  itype *s = (itype *)SRC;					\
  otype *d = (otype *)DST;					\
  xrgb  *x =  ddx->rgbLst;					\
  int    w =  ddx->width;					\
  switch(ddx->dix->class) {					\
  case StaticGray:						\
  case GrayScale:						\
  case StaticColor:						\
  case PseudoColor:						\
    while(w--) *d++ = x[*s++].red;				\
    break;							\
  case TrueColor:						\
  case DirectColor:						\
    { int rm = ddx->dix->pixMsk[0], rs = ddx->dix->pixPos[0];	\
      while(w--) *d++ = x[(*s++ & rm) >> rs].red;		\
    }								\
  }								\
}
/* bit versions - nyi */
/*		 CfromI_1bb,BitPixel, BitPixel  */
/*		 CfromI_1bB,BitPixel, BytePixel */
/*		 CfromI_1bP,BitPixel, PairPixel */
/*		 CfromI_1Bb,BytePixel,BitPixel  */
DO_SINGLE_CFROMI(CfromI_1BB,BytePixel,BytePixel)
DO_SINGLE_CFROMI(CfromI_1BP,BytePixel,PairPixel)
/*		 CfromI_1Pb,PairPixel,BitPixel  */
DO_SINGLE_CFROMI(CfromI_1PB,PairPixel,BytePixel)
DO_SINGLE_CFROMI(CfromI_1PP,PairPixel,PairPixel)
/*		 CfromI_1Qb,QuadPixel,BitPixel  */
DO_SINGLE_CFROMI(CfromI_1QB,QuadPixel,BytePixel)
DO_SINGLE_CFROMI(CfromI_1QP,QuadPixel,PairPixel)


/* Triple band output action routines:
 */

#define DO_TRIPLE_CFROMI(fn_do,itype,otype)			\
static int fn_do(ddx,SRC,DSTR,DSTG,DSTB)			\
    mpCfromIPtr ddx; pointer SRC, DSTR, DSTG, DSTB;		\
{								\
  itype  *s = (itype *)SRC;					\
  otype  *r = (otype *)DSTR;					\
  otype  *g = (otype *)DSTG;					\
  otype  *b = (otype *)DSTB;					\
  xrgb   *x =  ddx->rgbLst;					\
  int     w =  ddx->width;					\
  switch(ddx->dix->class) {					\
  case StaticGray:						\
  case GrayScale:						\
    while(w--) *r++ = *g++ = *b++ = x[*s++].red;		\
    break;							\
  case StaticColor:						\
  case PseudoColor:						\
    while(w--) {						\
      xrgb *p = x + *s++;					\
      *r++ = p->red; *g++ = p->green; *b++ = p->blue;		\
    }								\
    break;							\
  case TrueColor:						\
  case DirectColor:						\
    { int rm = ddx->dix->pixMsk[0], rs = ddx->dix->pixPos[0];	\
      int gm = ddx->dix->pixMsk[1], gs = ddx->dix->pixPos[1];	\
      int bm = ddx->dix->pixMsk[2], bs = ddx->dix->pixPos[2];	\
      while(w--) {						\
	Pixel p = *s++;						\
	*r++ = x[(p & rm) >> rs].red;				\
	*g++ = x[(p & gm) >> gs].green;				\
	*b++ = x[(p & bm) >> bs].blue;				\
      }								\
    }								\
  }								\
}
/* bit versions - nyi */
/*		 CfromI_3bb,BitPixel, BitPixel  */
/*		 CfromI_3bB,BitPixel, BytePixel */
/*		 CfromI_3bP,BitPixel, PairPixel */
/*		 CfromI_3Bb,BytePixel,BitPixel  */
DO_TRIPLE_CFROMI(CfromI_3BB,BytePixel,BytePixel)
DO_TRIPLE_CFROMI(CfromI_3BP,BytePixel,PairPixel)
/*		 CfromI_3Pb,PairPixel,BitPixel  */
DO_TRIPLE_CFROMI(CfromI_3PB,PairPixel,BytePixel)
DO_TRIPLE_CFROMI(CfromI_3PP,PairPixel,PairPixel)
     /*		 CfromI_3Qb,QuadPixel,BitPixel  */
DO_TRIPLE_CFROMI(CfromI_3QB,QuadPixel,BytePixel)
DO_TRIPLE_CFROMI(CfromI_3QP,QuadPixel,PairPixel)

/* end module mpcfromi.c */
