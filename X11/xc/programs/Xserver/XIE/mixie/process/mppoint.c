/* $Xorg: mppoint.c,v 1.4 2001/02/09 02:04:31 xorgcvs Exp $ */
/**** module mppoint.c ****/
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
  
	mppoint.c -- DDXIE point element
  
	Larry Hare -- AGE Logic, Inc. May, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/process/mppoint.c,v 3.5 2001/12/14 19:58:46 dawes Exp $ */


#define _XIEC_MPPOINT
#define _XIEC_PPOINT

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


/*
 *  routines referenced by other DDXIE modules
 */
int	miAnalyzePoint();

/*
 *  routines used internal to this module
 */
static int CreatePoint();
static int InitializePoint();
static int ActivatePoint();
static int ResetPoint();
static int DestroyPoint();

#if XIE_FULL
static int ActivatePointExplode();
static int ActivatePointExplodeMsk();
static int ActivatePointCombine();
static int ActivatePointROI();

#define LUT_EXPLODE
#endif

/*
 * DDXIE Point entry points
 */
static ddElemVecRec PointVec = {
  CreatePoint,
  InitializePoint,
  ActivatePoint,
  (xieIntProc) NULL,
  ResetPoint,
  DestroyPoint
  };

/*
* Local Declarations.
*/
typedef struct _mppointdef {
	void	(*action) ();
#if XIE_FULL
				/* below here for crazy pixels only */
	pointer (*convert) ();
	pointer buffer;
	CARD32	constant;
	CARD8   shiftok;
	CARD8   shiftamt;
	CARD32	mask;		/* for all 3 bands */
	CARD32	width;		/* for all 3 bands */
	void	(*merge) ();	/* for all 3 bands */
	void	(*store) ();	/* for all 3 bands */
#endif
} mpPointPvtRec, *mpPointPvtPtr;

static void P11_bb0(), P11_bb1();
static void P11_bb(), P11_Bb(), P11_Pb(), P11_Qb();
static void P11_bB(), P11_BB(), P11_PB(), P11_QB();
static void P11_bP(), P11_BP(), P11_PP(), P11_QP();
static void P11_bQ(), P11_BQ(), P11_PQ(), P11_QQ();

static void (*action_pnt11[4][4])() = {
	P11_bb, P11_Bb, P11_Pb, P11_Qb,	/* [out=1][inp=1...4] */
	P11_bB, P11_BB, P11_PB, P11_QB,	/* [out=2][inp=1...4] */
	P11_bP, P11_BP, P11_PP, P11_QP,	/* [out=3][inp=1...4] */
	P11_bQ, P11_BQ, P11_PQ, P11_QQ,	/* [out=4][inp=1...4] */
};

#if XIE_FULL
static void Proi11_bb0();
static void Proi11_bb(), Proi11_BB(), Proi11_PP(), Proi11_QQ();

static void (*action_pntroi11[4])() = {
	Proi11_bb, Proi11_BB, Proi11_PP, Proi11_QQ /* [out==inp=1...4] */
};

static void crazy_horse();

static void CPM_B3BB(), CPS_B3BB(), CPA_B3BB();

static void CPMRG_B(), CPMRG_P(), CPMRG_Q();

static void (*action_merge[3])() = {	/* [intclass - 2] */
	CPMRG_B, CPMRG_P, CPMRG_Q	/* [byte,pair,quad] */
};

static pointer CPCNV_bB(), CPCNV_BB();
static pointer CPCNV_bP(), CPCNV_BP(), CPCNV_PP();
static pointer CPCNV_bQ(), CPCNV_BQ(), CPCNV_PQ();

static pointer (*action_convert[3][3])() = { /* [intclass-2][ii-1] */
	CPCNV_bB, CPCNV_BB, 0,		/* [out=Byte] [inp=bits,byte,pair] */
	CPCNV_bP, CPCNV_BP, CPCNV_PP,	/* [out=Pair] [inp=bits,byte,pair] */
	CPCNV_bQ, CPCNV_BQ, CPCNV_PQ	/* [out=Quad] [inp=bits,byte,pair] */
};
#endif

/*------------------------------------------------------------------------
------------------------  fill in the vector  ---------------------------
------------------------------------------------------------------------*/
int miAnalyzePoint(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    ped->ddVec = PointVec;
    return TRUE;
}


/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreatePoint(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    return MakePETex(flo,ped,
		     xieValMaxBands * sizeof(mpPointPvtRec),
		     SYNC,	/* InSync: Make sure Lut exists first */
		     NO_SYNC	/* bandSync: see InitializePoint */
		     );
} 

/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializePoint(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  xieFloPoint     *raw = (xieFloPoint *)ped->elemRaw;
  peTexPtr 	     pet = ped->peTex;
  mpPointPvtPtr    pvt = (mpPointPvtPtr) pet->private;
  receptorPtr      rcp = pet->receptor;
  CARD32	  nbands = pet->receptor[SRCtag].inFlo->bands;
  CARD32	  lbands = pet->receptor[LUTtag].inFlo->bands;
  bandPtr	   iband = &(pet->receptor[SRCtag].band[0]);
  bandPtr	   oband = &(pet->emitter[0]);
  CARD8	     msk = raw->bandMask;
  BOOL        bandsync = FALSE;
  CARD8	  passmsk, lutmask;
  CARD32	  band;
  
#if XIE_FULL
  if (nbands == 3 && lbands == 1) {
    int oo = oband->format->class;
    int ii = iband->format->class;
    int intclass;
    CARD32 c0, c1, c2, ctotal;
    CARD32 iimin = ii, iimax = ii;
    
    pvt->width = iband->format->width;
    
    c1 = (iband+1)->format->class;
    if (c1 > iimax) iimax = c1; else if (c1 < iimin) iimin = c1;
    c2 = (iband+2)->format->class;
    if (c2 > iimax) iimax = c2; else if (c2 < iimin) iimin = c2;
    
    /* Create CRAZY pixels by multiply/accumulate */
    if (pet->receptor[LUTtag].band[0].format->width == xieValLSFirst) {
      (pvt+0)->constant = c0 = 1;
      (pvt+1)->constant = c1 = (iband+0)->format->levels;
      (pvt+2)->constant = c2 = (iband+0)->format->levels *
	(iband+1)->format->levels;
      ctotal = c2 * (iband+2)->format->levels;
    } else { /* swizzle band-order MSFirst */
      (pvt+2)->constant = c2 = 1;
      (pvt+1)->constant = c1 = (iband+2)->format->levels;
      (pvt+0)->constant = c0 = (iband+2)->format->levels *
	(iband+1)->format->levels;
      ctotal = c0 * (iband+0)->format->levels;
    }
    
    if ((c0 & (c0-1)) == 0) {
      (pvt+0)->shiftamt = ffs(c0) - 1;
      (pvt+0)->shiftok = 1;
    } else  (pvt+0)->shiftok = 0;
    if ((c1 & (c1-1)) == 0) {
      (pvt+1)->shiftamt = ffs(c1) - 1;
      (pvt+1)->shiftok = 1;
    } else  (pvt+1)->shiftok = 0;
    if ((c2 & (c2-1)) == 0) {
      (pvt+2)->shiftamt = ffs(c2) - 1;
      (pvt+2)->shiftok = 1;
    } else  (pvt+2)->shiftok = 0;
    
    if (ctotal <= 2) /* Could happen if levels were 1/1/2. pretty silly */
      ctotal = 4;
    
    SetDepthFromLevels(ctotal,c2);
    pvt->mask = (1<<c2) - 1;
    
    intclass = (ctotal <=   256) ? BYTE_PIXEL :
      (ctotal <= 65536) ? PAIR_PIXEL : QUAD_PIXEL;
    
    if (iimin == BYTE_PIXEL && iimax == BYTE_PIXEL &&
	intclass == BYTE_PIXEL && oo == BYTE_PIXEL ) {
      
      /* BYTE/BYTE/BYTE --> BYTE -> BYTE */
      pvt->action = ((pvt+0)->shiftok &&
		     (pvt+1)->shiftok &&
		     (pvt+2)->shiftok) ? CPS_B3BB : CPM_B3BB;
    } else {
      /* Do it the hard way */
      pvt->action = crazy_horse;
      if (intclass == BYTE_PIXEL && oo == BYTE_PIXEL) {
	pvt->merge = CPA_B3BB;
	pvt->store = 0;
      } else {
	pvt->merge = action_merge[intclass-2];
	pvt->store = (oband->format->levels == 1)
	  ? P11_bb0
	    : action_pnt11[oo-1][intclass-1];
      }
      for(band = 0; band < nbands; band++, pvt++, iband++) {
	ii = iband->format->class;
	pvt->width = iband->format->width;
	/* might allocate extra space and use lut instead of multiply */
	pvt->convert = action_convert[intclass-2][ii-1];
	if (!(pvt->buffer = (pointer)
	      XieMalloc(pvt->width << (intclass - BYTE_PIXEL))))
	  AllocError(flo,ped,return(FALSE));
      }
    }
    
    /* protocol requires msk == ALL_BANDS */
    msk = ALL_BANDS; passmsk = NO_BANDS, lutmask = 1; bandsync = TRUE;
    ped->ddVec.activate = ActivatePointCombine;
    
  } else if (nbands == 1 && lbands == 3) {
    
    for(band = 0; band < lbands; band++, pvt++, oband++) {
      int oo = oband->format->class;
      int ii = iband->format->class;
      
      if (oband->format->levels == 1)
	pvt->action = P11_bb0;
      else if ((iband->format->levels == 1) && (oo == BIT_PIXEL))
	pvt->action = P11_bb1;
      else
	pvt->action = action_pnt11[oo-1][ii-1];
    }
#if defined(LUT_EXPLODE)
    if ((msk & 7) == 7) {
      /* Only works when all 3 bands selected */
      msk = 1; passmsk = NO_BANDS; lutmask = ALL_BANDS; bandsync = TRUE;
      ped->ddVec.activate = ActivatePointExplode;
    } else
#endif
      {
	/* must actually activate these bands instead of using passmsk */
	ped->ddVec.activate = ActivatePointExplodeMsk;
	lutmask = msk; msk = 1; passmsk = NO_BANDS; bandsync = FALSE;
	rcp[SRCtag].band[0].replicate = ~msk;
      }
    
  } else if (nbands == lbands) {
    /* Standard Case.  Map each band thru its own LUT */
    
    if (raw->domainPhototag) {
      for(band = 0; band < nbands; band++, pvt++, iband++, oband++) {
	int oo = oband->format->class;
	
	pvt->action = (oband->format->levels == 1)
	  ? Proi11_bb0
	    : action_pntroi11[oo-1];
      }
      passmsk = ~msk; lutmask = msk;
      rcp[ped->inCnt-1].band[0].replicate = msk;
      ped->ddVec.activate = ActivatePointROI;
    } else {
#endif  
      for(band = 0; band < nbands; band++, pvt++, iband++, oband++) {
	int oo = oband->format->class;
	int ii = iband->format->class;
	
	if (oband->format->levels == 1)
	  pvt->action = P11_bb0;
	else if ((iband->format->levels == 1) && (oo == BIT_PIXEL))
	  pvt->action = P11_bb1;
	else
	  pvt->action = action_pnt11[oo-1][ii-1];
      }
      passmsk = ~msk; lutmask = msk;
      ped->ddVec.activate = ActivatePoint;
#if XIE_FULL
    }
  } else 
    ImplementationError(flo,ped, return(FALSE));
#endif  
  pet->bandSync = bandsync;
  return(InitReceptor(flo,ped,&rcp[SRCtag],NO_DATAMAP,1,msk,passmsk) &&
	 InitReceptor(flo,ped,&rcp[LUTtag],NO_DATAMAP,1,lutmask,NO_BANDS) &&
	 (!raw->domainPhototag ||
	  InitProcDomain(flo, ped, raw->domainPhototag,
			 raw->domainOffsetX, raw->domainOffsetY)) &&
	 InitEmitter(flo, ped, NO_DATAMAP,
		     (raw->domainPhototag ? SRCtag : NO_INPLACE)));
}

/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
/*------------------  (1 Band, 3 LUTS) --> (3 Bands) -------------------*/

#if XIE_FULL
#if defined(LUT_EXPLODE)
/*
** NOTE:
**	This code was used in the SI alpha drop and works fine.  However
** 	using our new band replication feature, we can replicate the
**	image band and use the 3x3 mapping code.  On the three machines
**	I compared the execution times on, the results were within a few
**	percent; but this code was faster.  If you want to save a bit
**	of code space, turn off the define LUT_EXPLODE.
*/

static int ActivatePointExplode(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
    mpPointPvtPtr pvt = (mpPointPvtPtr) pet->private;
    bandPtr iband = &(pet->receptor[SRCtag].band[0]);
    bandPtr lband = &(pet->receptor[LUTtag].band[0]);
    bandPtr oband = &(pet->emitter[0]);
    pointer lvoid0, lvoid1, lvoid2;
    pointer ovoid0, ovoid1, ovoid2;
    pointer ivoid;
    int     bw = iband->format->width;

    /* asking for 1 should fetch entire lut strip */
    lvoid0 = GetSrcBytes(flo,pet,lband,0,1,FALSE); lband++;
    lvoid1 = GetSrcBytes(flo,pet,lband,0,1,FALSE); lband++;
    lvoid2 = GetSrcBytes(flo,pet,lband,0,1,FALSE);

    if (!lvoid0 || !lvoid1 || !lvoid2)
	ImplementationError(flo,ped, return(FALSE));

    ivoid  = GetCurrentSrc(flo,pet,iband);
    ovoid0 = GetCurrentDst(flo,pet,oband); oband++;
    ovoid1 = GetCurrentDst(flo,pet,oband); oband++;
    ovoid2 = GetCurrentDst(flo,pet,oband); oband -= 2;
    while (!ferrCode(flo) && ivoid && ovoid0 && ovoid1 && ovoid2) {

        (*((pvt+0)->action)) (ivoid, ovoid0, lvoid0, bw);
        (*((pvt+1)->action)) (ivoid, ovoid1, lvoid1, bw);
        (*((pvt+2)->action)) (ivoid, ovoid2, lvoid2, bw);

	ivoid  = GetNextSrc(flo,pet,iband,TRUE);
	ovoid0 = GetNextDst(flo,pet,oband,TRUE); oband++;
	ovoid1 = GetNextDst(flo,pet,oband,TRUE); oband++;
	ovoid2 = GetNextDst(flo,pet,oband,TRUE); oband -= 2;
    }
    FreeData(flo, pet, iband, iband->current);
    if (iband->final) {
	FreeData(flo, pet, lband ,lband->current); lband--;
	FreeData(flo, pet, lband ,lband->current); lband--;
	FreeData(flo, pet, lband ,lband->current); 
    }
    return TRUE;
}
#endif

static int ActivatePointExplodeMsk(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
    mpPointPvtPtr pvt = (mpPointPvtPtr) pet->private;
    xieFloPoint *raw = (xieFloPoint *)ped->elemRaw;
    int band, nbands = pet->receptor[LUTtag].inFlo->bands;
    bandPtr iband = &(pet->receptor[SRCtag].band[0]);
    bandPtr lband = &(pet->receptor[LUTtag].band[0]);
    bandPtr oband = &(pet->emitter[0]);
    CARD8     msk = raw->bandMask;
    pointer lvoid;

    for(band = 0; band < nbands; band++, pvt++, iband++, oband++, lband++) {
	register int bw = iband->format->width;
	pointer ivoid, ovoid;

	if ((msk & (1<<band)) == 0) { 
	    /* Pass source band similar to BandSelect code */
	    if(GetCurrentSrc(flo,pet,iband)) {
		do {
		    if(!PassStrip(flo,pet,oband,iband->strip))
			return(FALSE);
		} while(GetSrc(flo,pet,iband,iband->maxLocal,FLUSH));
		FreeData(flo,pet,iband,iband->maxLocal);
	    }
	    continue;
	}

	/* Or process similar to ordinary Point code. */
    	if (!(lvoid = GetSrcBytes(flo,pet,lband,0,1,FALSE)) ||
	    !(ivoid = GetCurrentSrc(flo,pet,iband)) ||
	    !(ovoid = GetCurrentDst(flo,pet,oband))) continue;

	do {
	    (*(pvt->action)) (ivoid, ovoid, lvoid, bw);
	    ivoid = GetNextSrc(flo,pet,iband,TRUE);
	    ovoid = GetNextDst(flo,pet,oband,TRUE);
	} while (!ferrCode(flo) && ivoid && ovoid) ;

	FreeData(flo, pet, iband, iband->current);
	if (iband->final)
	    FreeData(flo, pet, lband, lband->current);
    }
    return TRUE;
}

/*------------------  (3 Band, 1 LUTS) --> (1 Bands) -------------------*/

static int ActivatePointCombine(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
    mpPointPvtPtr pvt = (mpPointPvtPtr) pet->private;
    bandPtr iband = &(pet->receptor[SRCtag].band[0]);
    bandPtr lband = &(pet->receptor[LUTtag].band[0]);
    bandPtr oband = &(pet->emitter[0]);
    pointer ivoid0, ivoid1, ivoid2, lvoid, ovoid;

    /* asking for 1 should fetch entire lut strip */
    if (!(lvoid = GetSrcBytes(flo,pet,lband,0,1,FALSE)))
	return FALSE;

    ovoid  = GetCurrentDst(flo,pet,oband);
    ivoid0 = GetCurrentSrc(flo,pet,iband); iband++;
    ivoid1 = GetCurrentSrc(flo,pet,iband); iband++;
    ivoid2 = GetCurrentSrc(flo,pet,iband); iband -= 2;

    while (!ferrCode(flo) && ovoid && ivoid0 && ivoid1 && ivoid2) {

        (*(pvt->action)) (ivoid0, ivoid1, ivoid2, lvoid, ovoid, pvt);

	ovoid  = GetNextDst(flo,pet,oband,TRUE);
	ivoid0 = GetNextSrc(flo,pet,iband,TRUE); iband++;
	ivoid1 = GetNextSrc(flo,pet,iband,TRUE); iband++;
	ivoid2 = GetNextSrc(flo,pet,iband,TRUE); iband -= 2;
    }

    FreeData(flo, pet, iband, iband->current); iband++;
    FreeData(flo, pet, iband, iband->current); iband++;
    FreeData(flo, pet, iband, iband->current);

    if (iband->final)
	FreeData(flo, pet, lband, lband->current);

    return TRUE;
}

/*------------------  (3 Band, 3 LUTS, ROI) --> (3 Bands) -------------------*/
/*------------------  (1 Band, 1 LUTS, ROI) --> (1 Bands) -------------------*/

static int ActivatePointROI(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
    
    mpPointPvtPtr pvt = (mpPointPvtPtr) pet->private;
    int band, nbands  = pet->receptor[LUTtag].inFlo->bands; /* ??? */
    bandPtr iband     = &(pet->receptor[SRCtag].band[0]);
    bandPtr lband     = &(pet->receptor[LUTtag].band[0]);
    bandPtr rband     = &(pet->receptor[ped->inCnt-1].band[0]);
    bandPtr oband     = &(pet->emitter[0]);
    pointer lvoid;

    for(band=0; band < nbands; band++,pvt++,iband++,oband++,lband++,rband++) {
	pointer ivoid, ovoid;

	/* 1 should fetch entire lut strip */
    	if (!(lvoid = GetSrcBytes(flo,pet,lband,0,1,FALSE)) ||
	    !(ivoid = GetCurrentSrc(flo,pet,iband)) ||
	    !(ovoid = GetCurrentDst(flo,pet,oband))) continue;

	while (!ferrCode(flo) && ivoid && ovoid && 
				SyncDomain(flo,ped,oband,FLUSH)) {
	    INT32 run, currentx = 0;
	   
    	    if(ivoid != ovoid) memcpy(ovoid, ivoid, oband->pitch);

	    while (run = GetRun(flo,pet,oband)) {
		if (run > 0) {
	    	    (*(pvt->action)) (ovoid, lvoid, run, currentx);
		    currentx += run;
		} else
		    currentx -= run;
	    }

	    ivoid = GetNextSrc(flo,pet,iband,TRUE);
	    ovoid = GetNextDst(flo,pet,oband,TRUE);
	}

	FreeData(flo, pet, iband, iband->current);
	if (iband->final)
	    FreeData(flo, pet, lband, lband->current);
    }
    return TRUE;
}
#endif
      
/*------------------  (3 Band, 3 LUTS) --> (3 Bands) -------------------*/
/*------------------  (1 Band, 1 LUTS) --> (1 Bands) -------------------*/

static int ActivatePoint(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
    mpPointPvtPtr pvt = (mpPointPvtPtr) pet->private;
    int band, nbands = pet->receptor[LUTtag].inFlo->bands;
    bandPtr iband = &(pet->receptor[SRCtag].band[0]);
    bandPtr lband = &(pet->receptor[LUTtag].band[0]);
    bandPtr oband = &(pet->emitter[0]);
    pointer lvoid;

    for(band = 0; band < nbands; band++, pvt++, iband++, oband++, lband++) {
	register int bw = iband->format->width;
	pointer ivoid, ovoid;

	/* 1 should fetch entire lut strip */
    	if (!(lvoid = GetSrcBytes(flo,pet,lband,0,1,FALSE)) ||
	    !(ivoid = GetCurrentSrc(flo,pet,iband)) ||
	    !(ovoid = GetCurrentDst(flo,pet,oband))) continue;

	do {
	    (*(pvt->action)) (ivoid, ovoid, lvoid, bw);
	    ivoid = GetNextSrc(flo,pet,iband,TRUE);
	    ovoid = GetNextDst(flo,pet,oband,TRUE);
	} while (!ferrCode(flo) && ivoid && ovoid) ;

	FreeData(flo, pet, iband, iband->current);
	if (iband->final)
	    FreeData(flo, pet, lband, lband->current);
    }
    return TRUE;
}

/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetPoint(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
#if XIE_FULL
    mpPointPvtPtr pvt = (mpPointPvtPtr) (ped->peTex->private);
    int band;

    /* free any dynamic private data */
    if (pvt)
	for (band = 0 ; band < xieValMaxBands ; band++, pvt++)
	    if (pvt->buffer)
		pvt->buffer = (pointer) XieFree(pvt->buffer);
#endif
    ResetReceptors(ped);
    ResetProcDomain(ped);
    ResetEmitter(ped);
    return TRUE;
}

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyPoint(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
    /* get rid of the peTex structure  */
    if(ped->peTex)
	ped->peTex = (peTexPtr) XieFree(ped->peTex);

    /* zap this element's entry point vector */
    ped->ddVec.create = (xieIntProc)NULL;
    ped->ddVec.initialize = (xieIntProc)NULL;
    ped->ddVec.activate = (xieIntProc)NULL;
    ped->ddVec.reset = (xieIntProc)NULL;
    ped->ddVec.destroy = (xieIntProc)NULL;

    return TRUE;
} 

/*------------------------------------------------------------------------
---------------------  Lotsa Little Action Routines  ---------------------
------------------------------------------------------------------------*/

/*
**  bitonal ---> LUT ---> bitonal (only 4 possibilities)
*/

static void
P11_bb(INP,OUTP,LUTP,bw)
	pointer INP; pointer OUTP; pointer LUTP; int bw;
{
	unsigned char *lutp = (unsigned char *) LUTP;

	if (*lutp++ == 0) {
	    if (*lutp == 0)
		action_clear  (OUTP, bw, 0);	
	    else
		passcopy_bit  (OUTP, INP, bw, 0);
	} else  if (*lutp == 0) {
		passcopy_bit  (OUTP, INP, bw, 0);
		action_invert (OUTP, bw, 0);
	} else		
		action_set    (OUTP, bw, 0);
}

/*
**  bitonal ---> LUT ---> bitonal (input is single level) 
*/

static void
P11_bb1(INP,OUTP,LUTP,bw)
	pointer INP; pointer OUTP; pointer LUTP; int bw;
{
	CARD8 *lutp = (CARD8 *) LUTP;

	if (*lutp)	action_set(OUTP, bw, 0);
	else		action_clear(OUTP, bw, 0);
}

/*
** anything ---> LUT ---> bitonal (output is single level)
*/

static void
P11_bb0(INP,OUTP,LUTP,bw)
	pointer INP; pointer OUTP; pointer LUTP; int bw;
{
	action_clear(OUTP, bw, 0);
}

/*
** DO_P11b - single band, single lut, bit to bit
** DO_P11c - single band, single lut, consume bits
** DO_P11x - single band, single lut, consume bits (to bytes only)
** DO_P11p - single band, single lut, produce bits
** DO_P11  - single band, single lut, non-bit versions
*/

#define DO_P11b(fd_do,itype,otype)	/* see P11_bb, P11_bb1, P11_bb0 above */

#define DO_P11c(fn_do,itype,otype)					\
static void								\
fn_do(INP,OUTP,LUTP,bw)							\
	pointer INP; pointer OUTP; pointer LUTP; int bw;		\
{									\
	LogInt *inp = (LogInt *) INP;					\
	otype *outp = (otype *) OUTP;					\
	otype *lutp = (otype *) LUTP;					\
	otype loval = lutp[0], hival = lutp[1];				\
	LogInt M, inval;						\
	for ( ; bw >= LOGSIZE ; bw -= LOGSIZE)				\
	    for (M=LOGLEFT, inval = *inp++; M; LOGRIGHT(M))		\
		*outp++ =  (inval & M) ? hival : loval;			\
	if (bw > 0)							\
	    for (M=LOGLEFT, inval = *inp++; bw; bw--, LOGRIGHT(M))	\
		*outp++ =  (inval & M) ? hival : loval;			\
}
#define DO_P11p(fn_do,itype,otype)					\
static void								\
fn_do(INP,OUTP,LUTP,bw)							\
	pointer INP; pointer OUTP; pointer LUTP; int bw;		\
{									\
	itype *inp = (itype *) INP;					\
	LogInt *outp = (LogInt *) OUTP, M, outval;			\
	unsigned char *lutp = (unsigned char *) LUTP;			\
	for ( ; bw >= LOGSIZE ; *outp++ = outval, bw -= LOGSIZE)	\
	    for (M=LOGLEFT, outval = 0; M; LOGRIGHT(M))			\
		if (lutp[*inp++])					\
		    outval |= M;					\
	if (bw > 0) {							\
	    for (M=LOGLEFT, outval = 0; bw; bw--, LOGRIGHT(M))		\
		if (lutp[*inp++])					\
		    outval |= M;					\
	    *outp++ = outval;						\
	}								\
}

#define DO_P11x(fn_do,itype,otype)					\
static void								\
fn_do(INP,OUTP,LUTP,bw)							\
	pointer INP; pointer OUTP; pointer LUTP; int bw;		\
{ 									\
	otype *lutp = (otype *) LUTP;					\
	bitexpand(INP,OUTP,bw, lutp[0], lutp[1]);			\
}

#define DO_P11(fn_do,itype,otype)					\
static void								\
fn_do(INP,OUTP,LUTP,bw)							\
	pointer INP; pointer OUTP; pointer LUTP; int bw;		\
{									\
	itype *inp = (itype *) INP;					\
	otype *outp = (otype *) OUTP;					\
	otype *lutp = (otype *) LUTP;					\
	while (bw-- > 0) *outp++ = lutp[*inp++];			\
}

/*
** DO_P11b - bit to bit
** DO_P11c - consume bits 
** DO_P11x - consume bits (faster bit eXpander for bytes only)
** DO_P11p - produce bits
** DO_P11  - non-bit versions
*/

DO_P11b	(P11_bb, BitPixel,  BitPixel)
DO_P11p	(P11_Bb, BytePixel, BitPixel)
DO_P11p	(P11_Pb, PairPixel, BitPixel)
DO_P11p	(P11_Qb, QuadPixel, BitPixel)

DO_P11x	(P11_bB, BitPixel,  BytePixel)
DO_P11	(P11_BB, BytePixel, BytePixel)
DO_P11	(P11_PB, PairPixel, BytePixel)
DO_P11	(P11_QB, QuadPixel, BytePixel)

DO_P11c	(P11_bP, BitPixel,  PairPixel)
DO_P11	(P11_BP, BytePixel, PairPixel)
DO_P11	(P11_PP, PairPixel, PairPixel)
DO_P11	(P11_QP, QuadPixel, PairPixel)

DO_P11c	(P11_bQ, BitPixel,  QuadPixel)
DO_P11	(P11_BQ, BytePixel, QuadPixel)
DO_P11	(P11_PQ, PairPixel, QuadPixel)
DO_P11	(P11_QQ, QuadPixel, QuadPixel)

/*------------------------------------------------------------------------
---------------------  ROI operations work on subranges ------------------
------------------------------------------------------------------------*/
#if XIE_FULL
/*
**  The Proi11_XX routines only map within a single type.  By making new 
**  routines here, instead of using the P11_XX routines above, it
**  may be easier to optimize the more important P11_XX routines by
**  virtue of starting on nicely aligned boundaries.
*/

static void
Proi11_bb0(INP,LUTP,run,ix)
	pointer INP; pointer LUTP; int run; int ix;
{
	action_clear  (INP, run, ix);	
}

static void
Proi11_bb(INP,LUTP,run,ix)
	pointer INP; pointer LUTP; int run; int ix;
{
	CARD8 *lutp = (CARD8 *) LUTP;

	if (*lutp++ == 0) {
		if (*lutp == 0) action_clear  (INP, run, ix);	
	} else  if (*lutp == 0) action_invert (INP, run, ix);
	else			action_set    (INP, run, ix);
}

#define ROI_P11(fn_do,iotype)					\
static void 							\
fn_do(INP,LUTP,run,ix) 						\
	pointer INP; pointer LUTP; INT32 run; INT32 ix;		\
{ 								\
	iotype *inp  = ((iotype *) INP) + ix;			\
	iotype *lutp = (iotype *) LUTP; 			\
	while (run-- > 0) { *inp = lutp[*inp]; inp++; }		\
}

ROI_P11		(Proi11_BB, BytePixel)
ROI_P11		(Proi11_PP, PairPixel)
ROI_P11		(Proi11_QQ, QuadPixel)
#endif

/*------------------------------------------------------------------------
---------------------  Crazy Pixels Action Routines  ---------------------
------------------------------------------------------------------------*/
#if XIE_FULL
/*
**  The Activate routine will nominally call this routine to do all
**  the necessary moderation per scanline.  However in the typical
**  case of going from a triple byte to a single byte image, one of the
**  CPM_B3BB or CPS_B3BB routines will get called instead.
*/

static void
crazy_horse(ivoid0, ivoid1, ivoid2, lvoid, ovoid, pvt)
    pointer ivoid0, ivoid1, ivoid2, lvoid, ovoid;
    mpPointPvtPtr pvt;
{
	ivoid0 = (*(pvt->convert)) (ivoid0, pvt); pvt++;
	ivoid1 = (*(pvt->convert)) (ivoid1, pvt); pvt++;
	ivoid2 = (*(pvt->convert)) (ivoid2, pvt); pvt -= 2;
	if (pvt->store) {
	    (*(pvt->merge)) (ivoid0, ivoid1, ivoid2, lvoid, pvt->buffer, pvt);
	    (*(pvt->store)) (pvt->buffer, ovoid, lvoid, pvt->width);
	} else
	    (*(pvt->merge)) (ivoid0, ivoid1, ivoid2, lvoid, ovoid, pvt);
}

/*
**  Some cpu's are not to good at multiplies, so we need two versions.
*/

#define MakeCrazyPix(name, itype, otype, OP, field)			\
static void								\
name(ivoid0, ivoid1, ivoid2, lvoid, ovoid, pvt)				\
    pointer ivoid0, ivoid1, ivoid2, lvoid, ovoid;			\
    mpPointPvtPtr pvt;							\
{									\
    itype *i0 = (itype *) ivoid0;					\
    itype *i1 = (itype *) ivoid1;					\
    itype *i2 = (itype *) ivoid2;					\
    otype *l  = (otype *) lvoid;					\
    otype *o  = (otype *) ovoid;					\
    CARD32 c0 = (pvt+0)->field;						\
    CARD32 c1 = (pvt+1)->field;						\
    CARD32 c2 = (pvt+2)->field;						\
    CARD32 msk = pvt->mask;						\
    CARD32 bw  = pvt->width;						\
    CARD32 j;								\
    for (j = 0; j < bw; j++) 						\
	*o++ = l[((i0[j] OP c0) + (i1[j] OP c1) + (i2[j] OP c2)) & msk];\
}

#define MakeCrazyMergeLut(name, itype, otype)				\
static void								\
name(ivoid0, ivoid1, ivoid2, lvoid, ovoid, pvt)				\
    pointer ivoid0, ivoid1, ivoid2, lvoid, ovoid;			\
    mpPointPvtPtr pvt;							\
{									\
    itype *i0 = (itype *) ivoid0;					\
    itype *i1 = (itype *) ivoid1;					\
    itype *i2 = (itype *) ivoid2;					\
    otype *l  = (otype *) lvoid;					\
    otype *o  = (otype *) ovoid;					\
    CARD32 msk = pvt->mask;						\
    CARD32 bw  = pvt->width;						\
    CARD32 j;								\
    for (j = 0; j < bw; j++) 						\
	*o++ = l[(i0[j] + i1[j] + i2[j]) & msk];			\
}

#define MakeCrazyMerge(name, iotype)					\
static void								\
name(ivoid0, ivoid1, ivoid2, lvoid, ovoid, pvt)				\
    pointer ivoid0, ivoid1, ivoid2, lvoid, ovoid;			\
    mpPointPvtPtr pvt;							\
{									\
    iotype *i0 = (iotype *) ivoid0;					\
    iotype *i1 = (iotype *) ivoid1;					\
    iotype *i2 = (iotype *) ivoid2;					\
    iotype *op = (iotype *) ovoid;					\
    CARD32 msk = pvt->mask;						\
    CARD32 bw  = pvt->width;						\
    CARD32 j;								\
    for (j = 0; j < bw; j++) 						\
	op[j] = (i0[j] + i1[j] + i2[j]) & msk;				\
}

#define MakeCrazyConvert(name, itype, otype)				\
static pointer								\
name(ivoid,pvt)								\
    pointer ivoid;							\
    mpPointPvtPtr pvt;							\
{									\
    itype *i = (itype *) ivoid;						\
    otype *o = (otype *) pvt->buffer;					\
    CARD32 c = pvt->constant;						\
    CARD32 bw = pvt->width;						\
    if ( (sizeof(itype) == sizeof(otype))  && (c == 1))			\
	return ivoid;							\
    if (pvt->shiftok) 							\
	for (c = pvt->shiftamt ; bw > 0; bw--) *o++ = *i++ << c;  	\
    else								\
	for ( ; bw > 0; bw--) *o++ = *i++ * c;  			\
    return pvt->buffer;							\
}

static pointer
CPCNV_bB(ivoid,pvt)
    pointer ivoid;
    mpPointPvtPtr pvt;
{
    bitexpand(ivoid, pvt->buffer, pvt->width, 0, pvt->constant);
    return pvt->buffer;
}

#define MakeCrazyConvertBit(name, otype)				\
static pointer								\
name(ivoid,pvt)								\
    pointer ivoid;							\
    mpPointPvtPtr pvt;							\
{									\
    LogInt *i = (LogInt *) ivoid, ival, M;				\
    otype  *o = (otype *) pvt->buffer;					\
    CARD32  c = pvt->constant;						\
    int    bw = pvt->width;						\
    int    nw = bw >> LOGSHIFT;						\
    for ( ; nw > 0; nw--)						\
	for (ival = *i++, M=LOGLEFT; M ; LOGRIGHT(M))			\
	    *o++ = (ival & M) ? c : 0;					\
    if ((bw &= LOGMASK))						\
	for (ival = *i, M=LOGLEFT; bw > 0 ; bw--, LOGRIGHT(M))		\
	    *o++ = (ival & M) ? c : 0;					\
    return pvt->buffer;							\
}


MakeCrazyPix		(CPM_B3BB, BytePixel, BytePixel,  *, constant)
MakeCrazyPix		(CPS_B3BB, BytePixel, BytePixel, <<, shiftamt)
MakeCrazyMergeLut	(CPA_B3BB, BytePixel, BytePixel    )
MakeCrazyMerge		(CPMRG_B, BytePixel)
MakeCrazyMerge		(CPMRG_P, PairPixel)
MakeCrazyMerge		(CPMRG_Q, QuadPixel)


MakeCrazyConvert	(CPCNV_BB, BytePixel, BytePixel)
MakeCrazyConvert	(CPCNV_BP, BytePixel, PairPixel)
MakeCrazyConvert	(CPCNV_BQ, BytePixel, QuadPixel)
MakeCrazyConvert	(CPCNV_PP, PairPixel, PairPixel)
MakeCrazyConvert	(CPCNV_PQ, PairPixel, QuadPixel)
MakeCrazyConvertBit	(CPCNV_bP, PairPixel)
MakeCrazyConvertBit	(CPCNV_bQ, QuadPixel)
#endif

/* end module mppoint.c */
