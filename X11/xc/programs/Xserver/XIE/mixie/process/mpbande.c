/* $Xorg: mpbande.c,v 1.4 2001/02/09 02:04:29 xorgcvs Exp $ */
/**** module mpbande.c ****/
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
  
	mpbande.c -- DDXIE BandExtract element
  
	Robert NC Shelley -- AGE Logic, Inc. June, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/process/mpbande.c,v 3.5 2001/12/14 19:58:43 dawes Exp $ */

#define _XIEC_MPBANDE
#define _XIEC_PBANDE

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
#include <element.h>
#include <texstr.h>
#include <xiemd.h>
#include <memory.h>


/* routines referenced by other DDXIE modules
 */
int	miAnalyzeBandExt();

/* routines used internal to this module
 */
static int CreateBandExt();
static int InitializeBandExt();
static int ActivateBandExt();
static int ResetBandExt();
static int DestroyBandExt();

/* DDXIE BandExtract entry points
 */
static ddElemVecRec BandExtVec = {
  CreateBandExt,
  InitializeBandExt,
  ActivateBandExt,
  (xieIntProc)NULL,
  ResetBandExt,
  DestroyBandExt
  };

/* Local Declarations.
 */
typedef float	 bndExtFlt;
typedef int	 bndExtInt;
#define BE_FBITS  6
#define BE_IBITS  32-BE_FBITS
#define BE_LIMIT (1<<BE_IBITS-1)

typedef struct _mpbandext {
  bndExtInt	 ibias;
  bndExtFlt	 fbias;
  bndExtFlt	 coef[xieValMaxBands];
  bndExtInt	*lut[xieValMaxBands];
  bndExtInt	*accumulator;
  xieVoidProc	 accumulate[xieValMaxBands];
  xieVoidProc	 extract;
  xieVoidProc	 output;
  bndExtInt	 bits[xieValMaxBands];
  Bool		 clip;
  Bool		 shift;
} mpBandExtRec, *mpBandExtPtr;

#define     NADA (xieVoidProc)NULL

/* "full service" extraction functions
 */
static void extBB(), extBP(), extPB(), extPP(), extQB(), extQP();
static void extRR(), extB4();
static void (*action_extract[4][3])() = {
  NADA,   NADA,  NADA,		/* out=b, in=b,B,P */
  NADA,  extBB, extBP,		/* out=B, in=b,B,P */
  NADA,  extPB, extPP,		/* out=P, in=b,B,P */
  NADA,  extQB, extQP,		/* out=Q, in=b,B,P */
};

/* accumulator functions
 */
static void acc_b(), acc_B(), acc_P();
static void (*action_accumulate[3])() = {
  acc_b, acc_B, acc_P,
};

/* output functions
 */
static void out_b(), out_B(), out_P(), out_Q();
static void (*action_output[4])() = {
  out_b, out_B, out_P, out_Q,
};


/*------------------------------------------------------------------------
------------------- see if we can handle this element --------------------
------------------------------------------------------------------------*/
int miAnalyzeBandExt(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* for now just stash our entry point vector in the peDef */
  ped->ddVec = BandExtVec;

  return(TRUE);
}                               /* end miAnalyzeBandExt */

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateBandExt(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* attach an execution context to the photo element definition */
  return MakePETex(flo, ped, sizeof(mpBandExtRec), NO_SYNC, SYNC);
}                               /* end CreateBandExt */


/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeBandExt(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  peTexPtr       pet = ped->peTex;
  formatPtr      ift = &ped->inFloLst[SRCtag].format[0];
  formatPtr      oft = &ped->outFlo.format[0];
  pBandExtDefPtr dix = (pBandExtDefPtr)ped->elemPvt;
  mpBandExtPtr   ddx = (mpBandExtPtr)  pet->private;
  bandMsk        msk = 0;
  
  if(IsntConstrained(ift->class)) {
    /*
     * the choice here is easy -- have to do floating calculation of each pixel
     */
    ddx->extract = extRR;
    ddx->coef[0] = dix->coef[0];
    ddx->coef[1] = dix->coef[1];
    ddx->coef[2] = dix->coef[2];
    ddx->fbias   = dix->bias;
    msk          = ALL_BANDS;
  } else {
    int size, b, i, ic = ift->class, shift[xieValMaxBands];
    Bool         match = TRUE;
    bndExtFlt  val, lo = dix->bias, hi = dix->bias;

    /* first we'll examine the numbers we have to work with...
     */
    for(b = 0; b < xieValMaxBands; ++b) {
      if((val = dix->coef[b]) < 0.0)
	lo += val * (ift[b].levels-1);
      else
	hi += val * (ift[b].levels-1);
      if(ift[b].levels <= 2 || (bndExtFlt)(i = val) != val || !i || i & i-1)
	shift[b] = ift[b].levels < 2 || val == 0.0 ? -1 : 0;
      else
	SetDepthFromLevels((int)val,shift[b]);  /* coef is a power of 2 */
      match &= ic == ift[b].class;
    }
    if(val = hi >= BE_LIMIT ? hi : lo <= -BE_LIMIT ? lo : 0.0)
      ValueError(flo,ped,(int)val, return(FALSE));

    /* if all bands share a common data class, see if we have an extractor,
     * else we'll have to accumulate input, then convert to the output class
     */
    if(match && (ddx->extract = action_extract[oft->class-1][ic-1]))
      msk = ALL_BANDS;
    else if(ddx->accumulator =
            (bndExtInt*) XieMalloc(oft->width*sizeof(bndExtInt)))
      ddx->output = action_output[oft->class-1];
    else
      AllocError(flo,ped, return(FALSE));

    /* init the control parameters for the action routines
     */
    ddx->clip  = lo < 0.0 || hi >= oft->levels;
    ddx->shift = ddx->extract && shift[0] > 0 && shift[1] > 0 && shift[2] > 0;
    ddx->ibias = dix->bias * (ddx->shift ? 1 : (1<<BE_FBITS));
    if(ddx->shift && !ddx->clip && ddx->extract == extBB) {
      ddx->extract = extB4;     /* we can use the turbo multi-byte shifter */
      ddx->ibias  |= ddx->ibias <<  8;
      ddx->ibias  |= ddx->ibias << 16;
    }
    for(b = 0; b < xieValMaxBands; ++b) {
      if(!ddx->shift && ddx->extract || shift[b] == 0) {
	/*
	 * build luts and assign accumulator functions
	 */
	size = 1<<ift[b].depth;
	ddx->bits[b] = size-1;
	if(!(ddx->lut[b] = (bndExtInt*) XieMalloc(size * sizeof(bndExtInt))))
	  AllocError(flo,ped, return(FALSE));
	
	for(i = 0; i < ift[b].levels; ++i)
	  ddx->lut[b][i]   = dix->coef[b] * (i<<BE_FBITS);
	while(i < size)
	  ddx->lut[b][i++] = 0;
      } else if(shift[b] > 0) {
	/*
	 * coef is an exact power of 2, so just use a left shift count
	 */
	ddx->bits[b] = shift[b] + ((ddx->shift ? 0 : BE_FBITS) -
				   (dix->coef[b] == 1.0 ? 1 : 0));
      }
      if(!ddx->extract && shift[b] >= 0) {
	/*
	 * assign accumulator function and add this band to the active mask
	 */
	ddx->accumulate[b] = action_accumulate[ift[b].class-1];
	msk |= 1<<b;
      }
    }
  }
  return(!msk ||
	 InitReceptor(flo,ped,&pet->receptor[SRCtag],NO_DATAMAP,1,msk,NO_BANDS)
	 && InitEmitter(flo, ped, NO_DATAMAP, NO_INPLACE));
}                               /* end InitializeBandExt */


/*------------------------------------------------------------------------
------------------------ crank some single input data --------------------
------------------------------------------------------------------------*/
static int ActivateBandExt(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  mpBandExtPtr   ddx = (mpBandExtPtr)  pet->private;
  bandPtr        ib  = &pet->receptor[SRCtag].band[0];
  bandPtr        ob  = &pet->emitter[0];
  bndExtInt     *acc = ddx->accumulator;
  formatPtr      oft = ob->format;
  CARD32 b, i, width = oft->width;
  CARD32      maxLev = oft->levels-1;
  bandMsk  ok, ready = pet->scheduled;
  pointer o, i0 = NULL, i1 = NULL, i2 = NULL;
  
  ok = NO_BANDS;
  if(ready & 1 && (i0 = GetCurrentSrc(flo,pet,&ib[0]))) ok |= 1;
  if(ready & 2 && (i1 = GetCurrentSrc(flo,pet,&ib[1]))) ok |= 2;
  if(ready & 4 && (i2 = GetCurrentSrc(flo,pet,&ib[2]))) ok |= 4;
  if((o = GetCurrentDst(flo,pet,ob)) && ok == ready)
    do {
      /* if we have a "full service" extractor, go for the whole enchalada
       */
      if( ddx->extract)
	(*ddx->extract)(o, i0, i1, i2, width, maxLev, ddx);
      else {
	/* initialize accumulator with the bias value
	 */
	if(ddx->ibias)
	  for(i = 0; i < width; acc[i++] = ddx->ibias);
	else
	  bzero((char *)acc, width * sizeof(bndExtInt));

	/* accumulate data from each active band, then convert to output class
	 */
	if( ddx->accumulate[0])
	  (*ddx->accumulate[0])(acc, i0, width, ddx->bits[0], ddx->lut[0]);
	if( ddx->accumulate[1])
	  (*ddx->accumulate[1])(acc, i1, width, ddx->bits[1], ddx->lut[1]);
	if( ddx->accumulate[2])
	  (*ddx->accumulate[2])(acc, i2, width, ddx->bits[2], ddx->lut[2]);
	(*ddx->output)(o, acc, width, maxLev, ddx->clip);
      }
      ok = NO_BANDS;
      if(ready & 1 && (i0 = GetNextSrc(flo,pet,&ib[0],FLUSH))) ok |= 1;
      if(ready & 2 && (i1 = GetNextSrc(flo,pet,&ib[1],FLUSH))) ok |= 2;
      if(ready & 4 && (i2 = GetNextSrc(flo,pet,&ib[2],FLUSH))) ok |= 4;
    } while((o = GetNextDst(flo,pet,ob,FLUSH)) && ok == ready);

  /* free the input data we used
   */
  for(b = 0; b < xieValMaxBands; ++b)
    if(ready & 1<<b)
      FreeData(flo,pet,&ib[b],ib[b].current);
  
  return(TRUE);
}                               /* end ActivateBandExt */


/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetBandExt(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  mpBandExtPtr ddx = (mpBandExtPtr) ped->peTex->private;
  CARD32 b = xieValMaxBands;

  if(ddx->accumulator)
     ddx->accumulator = (bndExtInt*)XieFree(ddx->accumulator);

  while(b--) {
    if(ddx->lut[b])
       ddx->lut[b] = (bndExtInt*) XieFree(ddx->lut[b]);
    ddx->accumulate[b] = (xieVoidProc)NULL;
  }
  ddx->extract = (xieVoidProc)NULL;
  ddx->output  = (xieVoidProc)NULL;
  
  ResetReceptors(ped);
  ResetEmitter(ped);
  
  return(TRUE);
}                               /* end ResetBandExt */


/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyBandExt(flo,ped)
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
}                               /* end DestroyBandExt */

/*------------------------------------------------------------------------
--------- extract functions -- used when input data classes match --------
------------------------------------------------------------------------*/
static void extB4(O, I0, I1, I2, width, dummy, ddx)
  pointer O, I0, I1, I2; CARD32 width, dummy; mpBandExtPtr ddx;
{
  QuadPixel *o  = (QuadPixel*)O;
  QuadPixel *i0 = (QuadPixel*)I0, *i1 = (QuadPixel*)I1, *i2 = (QuadPixel*)I2;
  bndExtInt  b0 = ddx->bits[0],    b1 = ddx->bits[1],    b2 = ddx->bits[2];
  bndExtInt  d  = ddx->ibias;
  int     i, w  = width+3>>2;

  for(i = 0; i < w; ++i)
    *o++ = (i0[i]<<b0)+(i1[i]<<b1)+(i2[i]<<b2)+d;
}

static void extRR(O, I0, I1, I2, width, dummy, ddx)
  pointer O, I0, I1, I2; CARD32 width, dummy; mpBandExtPtr ddx;
{
  RealPixel *o  = (RealPixel*)O,
	    *i0 = (RealPixel*)I0, *i1 = (RealPixel*)I1, *i2 = (RealPixel*)I2;
  bndExtFlt  c0 =  ddx->coef[0],   c1 =  ddx->coef[1],   c2 =  ddx->coef[2],
	     d  =  ddx->fbias;
  int i;
  for(i = 0; i < width; ++i)
    o[i] = i0[i] * c0 + i1[i] * c1 + i2[i] * c2 + d;
}

#define BAND_EXTRACT(name,otype,itype)					\
static  void name(O, I0, I1, I2, width, maxLev, ddx)			\
  pointer O, I0, I1, I2; CARD32 width, maxLev; mpBandExtPtr ddx;	\
{									\
  otype     *o  = (otype*)O;						\
  itype     *i0 = (itype*)I0,  *i1 = (itype*)I1,  *i2 = (itype*)I2;	\
  bndExtInt  b0 = ddx->bits[0], b1 = ddx->bits[1], b2 = ddx->bits[2];	\
  bndExtInt  d  = ddx->ibias;						\
  int i;								\
  if(ddx->shift) {							\
    if(ddx->clip)							\
      for(i = 0; i < width; ++i) {					\
        int tmp = (i0[i]<<b0)+(i1[i]<<b1)+(i2[i]<<b2)+d;		\
        if( tmp < 0)		*o++ = 0;				\
        else if(tmp > maxLev)	*o++ = maxLev;				\
        else			*o++ = tmp;				\
      }									\
    else								\
      for(i = 0; i < width; ++i)					\
        *o++ = (i0[i]<<b0)+(i1[i]<<b1)+(i2[i]<<b2)+d;			\
  } else {								\
    bndExtInt *L0 = ddx->lut[0], *L1 = ddx->lut[1], *L2 = ddx->lut[2];	\
    if(ddx->clip) {							\
      bndExtInt limit = (maxLev+1)<<BE_FBITS;				\
      for(i = 0; i < width; ++i) {					\
        int tmp = L0[i0[i] & b0]+L1[i1[i] & b1]+L2[i2[i] & b2]+d;	\
        if( tmp < 0)		*o++ = 0;				\
        else if(tmp >= limit)	*o++ = maxLev;				\
        else			*o++ = tmp>>BE_FBITS;			\
      }									\
    } else								\
      for(i = 0; i < width; ++i)					\
        *o++ = L0[i0[i] & b0]+L1[i1[i] & b1]+L2[i2[i] & b2]+d>>BE_FBITS;\
  }									\
}
BAND_EXTRACT(extBB,BytePixel,BytePixel)
BAND_EXTRACT(extBP,BytePixel,PairPixel)
BAND_EXTRACT(extPB,PairPixel,BytePixel)
BAND_EXTRACT(extPP,PairPixel,PairPixel)
BAND_EXTRACT(extQB,QuadPixel,BytePixel)
BAND_EXTRACT(extQP,QuadPixel,PairPixel)

/*------------------------------------------------------------------------
---- accumulator functions (accumulate products from individual bands) ---
------------------------------------------------------------------------*/
static   void acc_b(acc, SRC, width, dummy, lut)
   bndExtInt *acc; pointer SRC; CARD32 width; bndExtInt dummy, *lut;
{
  LogInt    *i = (LogInt   *)SRC, ival, M;
  bndExtInt *o = (bndExtInt*)acc, c = lut[1];
  int	    bw = width;
  int       nw = bw >> LOGSHIFT;
  while(nw--)
    for(ival = *i++,M = LOGLEFT; M;    ++o, LOGRIGHT(M)) if(ival & M) *o += c;
  if(bw &= LOGMASK)
    for(ival = *i,  M = LOGLEFT; bw--; ++o, LOGRIGHT(M)) if(ival & M) *o += c;
}

#define BAND_ACCUMULATE(name,itype)					\
static void name(acc, SRC, width, bits, lut)				\
  bndExtInt *acc; pointer SRC; CARD32 width; bndExtInt bits, *lut;	\
{									\
  itype *src = (itype*)SRC;						\
  int      i = 0;							\
  if(lut) while(i < width) acc[i++] += lut[*src++ & bits];		\
  else    while(i < width) acc[i++] += *src++ << bits;			\
}
BAND_ACCUMULATE(acc_B,BytePixel)
BAND_ACCUMULATE(acc_P,PairPixel)

/*------------------------------------------------------------------------
---- output functions (convert accumulated data to output data class) ----
------------------------------------------------------------------------*/
static void out_b(DST, acc, width, dummy1, dummy2)
  pointer DST; bndExtInt *acc; CARD32 width, dummy1; Bool dummy2;
{
  bndExtInt *i = acc;
  LogInt    *o = (LogInt*)DST, M, v;
  int bw;
  for(bw = width; bw >= LOGSIZE; *o++ = v, bw -= LOGSIZE)
    for(v = 0, M = LOGLEFT; M;    LOGRIGHT(M))	if(*i++ > 0) v |= M;
  if(bw > 0) {
    for(v = 0, M = LOGLEFT; bw--; LOGRIGHT(M))	if(*i++ > 0) v |= M;
    *o++ = v;
  }
}

#define BAND_OUTPUT(name,otype)						\
static void name(DST, acc, width, maxLev, clip)				\
     pointer DST; bndExtInt *acc; CARD32 width, maxLev; Bool clip;	\
{									\
  otype      *dst = (otype*)DST;					\
  bndExtInt limit = (maxLev+1)<<BE_FBITS;				\
  int      tmp, i = 0;							\
  if(clip)								\
    while(i < width)							\
      if((tmp = acc[i++]) < 0) *dst++ = 0;				\
      else if(tmp >= limit)    *dst++ = maxLev;				\
      else                     *dst++ = tmp >> BE_FBITS;		\
  else									\
    while(i < width)	       *dst++ = acc[i++] >> BE_FBITS;		\
}
BAND_OUTPUT(out_B,BytePixel)
BAND_OUTPUT(out_P,PairPixel)
BAND_OUTPUT(out_Q,QuadPixel)

/* end module mpbande.c */
