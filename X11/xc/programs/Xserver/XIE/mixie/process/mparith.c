/* $Xorg: mparith.c,v 1.6 2001/02/09 02:04:29 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module mparith.c ****/
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
  
	mparith.c -- DDXIE arithmetic and math element
  
	Larry Hare -- AGE Logic, Inc. August, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/process/mparith.c,v 3.5 2001/12/14 19:58:43 dawes Exp $ */


#define _XIEC_MPARITH
#define _XIEC_PARITH

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
    **  Most machines will work fine using the double precision versions
    **  of the math functions.  If they give you problems, or if you 
    **  want a bit more speed, then pick one of the following sets of
    **  defines.  Enjoy.   While you are thinking about this you might
    **  want to consider changing the cube root function in mprgb.c
    **  with fpow(x,1./3.) or powf(x,1./3.).  Another similar area is
    **  in mpgeomaa.c for the gaussian technique.
    */

#if defined(USE_EXPF)
    /*
    ** The very newest ANSI-C libraries promote this style of floating
    ** functions.  Even if you have these available, you may need to 
    ** use special compiler options, and prototypes from math.h
    */
#   define exp expf
#   define log logf
#   define pow powf
#   define sqrt sqrtf
#endif

#if defined(USE_FEXP)
    /*
    ** This is the more antique way to get floating math functions. But
    ** you may have to use compiler options or otherwise stand on your
    ** head to do this safely.  Otherwise the argument may get promoted.
    */
#   define exp fexp
#   define log flog
#   define pow fpow
#   define sqrt fsqrt
#endif

/*
 *  routines referenced by other DDXIE modules
 */
int	miAnalyzeArith();
int	miAnalyzeMath();

/*
 *  routines used internal to this module
 */
static int CreateArith();
static int InitializeArith();
static int ResetArith();
static int DestroyArith();

static int ActivateArithMROI();
static int ActivateArithDROI();
static int SetupArith();
static void ClearArith();		/* and Math */

static int CreateMath();
static int InitializeMath();
static int SetupMath();

/*
 * DDXIE Arithmetic and Math entry points
 */
static ddElemVecRec ArithVec = {
  CreateArith,
  InitializeArith,
  (xieIntProc) NULL,
  (xieIntProc) NULL,
  ResetArith,
  DestroyArith
  };

static ddElemVecRec MathVec = {
  CreateMath,
  InitializeMath,
  ActivateArithMROI,	/* yes, arith */
  (xieIntProc) NULL,
  ResetArith,		/* yes, arith */
  DestroyArith		/* yes, arith */
  };

/*
* Local Declarations.
*/

typedef struct _mparithdef {
    void	(*action) ();		/* to do arithmetic */
    void	(*passive) ();		/* to copy src1 in passive areas */
    CARD32	*lutptr;		/* in case we do a lookup */
    CARD32	nlev;			/* integer clipping */
    CARD32	nclip;			/* integer clipping */
    CARD32	iconstant;
    RealPixel	fconstant;
} mpArithPvtRec, *mpArithPvtPtr;	/* also used for Math */

/*
**  NOTE: What happens if element is run over and over with same
**	parameters.  If we are using a LUT, should we try to keep it?
**	If KEEP_LUTS is enabled, then we build any required luts in
**	the Create routine and free them in Destroy. Otherwise we do
**	so in Initialize and Reset.  This is more important on
**	operations like divide on non-fpu machines.
*/

#define KEEP_LUTS


/*------------------------------------------------------------------------
------------------------  fill in the vector  ---------------------------
------------------------------------------------------------------------*/
int miAnalyzeArith(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    ped->ddVec = ArithVec;
    return TRUE;
}

int miAnalyzeMath(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    ped->ddVec = MathVec;
    return TRUE;
}

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateArith(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    /* always force syncing between inputs (is nop if only one input) */
    if (!MakePETex(flo,ped,
		     xieValMaxBands * sizeof(mpArithPvtRec),
		     SYNC,	/* InSync: */
		     NO_SYNC	/* bandSync: */))
	return FALSE;

#if defined(KEEP_LUTS)
    if (!SetupArith(flo, ped, 0 /* modify ?? */))
	return FALSE;
#endif
 
    return TRUE;

} 

static int CreateMath(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    /* always force syncing between inputs (is nop if only one input) */
    if (!MakePETex(flo,ped,
		     xieValMaxBands * sizeof(mpArithPvtRec),
		     SYNC,	/* InSync: */
		     NO_SYNC	/* bandSync: */))
	return FALSE;

#if defined(KEEP_LUTS)
    if (!SetupMath(flo, ped, 0 /* modify ?? */))
	return FALSE;
#endif
 
    return TRUE;

} 

/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/

static int InitializeArith(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    xieFloArithmetic *raw = (xieFloArithmetic *) ped->elemRaw;
    peTexPtr 	     pet = ped->peTex;
    receptorPtr      rcp = pet->receptor;
    CARD8	     msk = raw->bandMask;

#if !defined(KEEP_LUTS)
    if (!SetupArith(flo, ped, 0 /* modify ?? */))
	return FALSE;
#endif

    ped->ddVec.activate = raw->src2 ? ActivateArithDROI : ActivateArithMROI;

    /* If processing domain, allow replication */
    if (raw->domainPhototag)
	rcp[ped->inCnt-1].band[0].replicate = msk;

    InitReceptor(flo, ped, &rcp[SRCt1], NO_DATAMAP, 1, msk, ~msk);

    if (raw->src2)
	InitReceptor(flo, ped, &rcp[SRCt2], NO_DATAMAP, 1, msk, NO_BANDS);

    InitProcDomain(flo, ped, raw->domainPhototag, raw->domainOffsetX, 
						  raw->domainOffsetY);
    InitEmitter(flo, ped, NO_DATAMAP, SRCt1);

    return !ferrCode(flo);
}

static int InitializeMath(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    xieFloMath	    *raw = (xieFloMath *) ped->elemRaw;
    peTexPtr 	     pet = ped->peTex;
    receptorPtr      rcp = pet->receptor;
    CARD8	     msk = raw->bandMask;

#if !defined(KEEP_LUTS)
    if (!SetupMath(flo, ped, 0 /* modify ?? */))
	return FALSE;
#endif
    if (raw->domainPhototag)
	rcp[ped->inCnt-1].band[0].replicate = msk;
    InitReceptor(flo, ped, &rcp[SRCt1], NO_DATAMAP, 1, msk, ~msk);
    InitProcDomain(flo, ped, raw->domainPhototag, raw->domainOffsetX, 
						  raw->domainOffsetY);
    InitEmitter(flo, ped, NO_DATAMAP, SRCt1);

    return !ferrCode(flo);
}

/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/

static int ActivateArithMROI(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
    mpArithPvtPtr pvt = (mpArithPvtPtr) pet->private;
    int band, nbands  = pet->receptor[SRCt1].inFlo->bands;
    bandPtr sband     = &(pet->receptor[SRCt1].band[0]);
    bandPtr dband     = &(pet->emitter[0]);

    for(band = 0; band < nbands; band++, pvt++, sband++, dband++) {
	pointer svoid, dvoid;

	if (!(pet->scheduled & 1<<band)) continue;

    	if (!(svoid = GetCurrentSrc(flo,pet,sband)) ||
	    !(dvoid = GetCurrentDst(flo,pet,dband))) continue;

	while (!ferrCode(flo) && svoid && dvoid && 
				SyncDomain(flo,ped,dband,FLUSH)) {
	    INT32 run, ix = 0;
	   
	    while (run = GetRun(flo,pet,dband)) {
		if (run > 0) {
	    	    (*(pvt->action)) (dvoid, svoid, run, ix, pvt);
		    ix += run;
		} else {
		    if (dvoid != svoid)
	    	    	(*(pvt->passive)) (dvoid, svoid, -run, ix);
		    ix -= run;
		}
	    }
	    svoid = GetNextSrc(flo,pet,sband,FLUSH);
	    dvoid = GetNextDst(flo,pet,dband,FLUSH);
	}

	FreeData(flo, pet, sband, sband->current);
    }
    return TRUE;
}

static int ActivateArithDROI(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
    mpArithPvtPtr pvt = (mpArithPvtPtr) pet->private;
    int band, nbands  = pet->receptor[SRCt1].inFlo->bands;
    bandPtr sband     = &(pet->receptor[SRCt1].band[0]);
    bandPtr tband     = &(pet->receptor[SRCt2].band[0]);
    bandPtr dband     = &(pet->emitter[0]);

    for(band = 0; band < nbands; band++, pvt++, sband++, tband++, dband++) {
	pointer svoid, tvoid, dvoid;
	CARD32 w;

	if (!(pet->scheduled & 1<<band)) continue;

	w = sband->format->width;
	if (w > tband->format->width) w = tband->format->width;

    	if (!(svoid = GetCurrentSrc(flo,pet,sband)) ||
    	    !(tvoid = GetCurrentSrc(flo,pet,tband)) ||
	    !(dvoid = GetCurrentDst(flo,pet,dband))) continue;

	while (!ferrCode(flo) && svoid && tvoid && dvoid && 
				SyncDomain(flo,ped,dband,FLUSH)) {
	    INT32 run, ix = 0;
	   
	    while (run = GetRun(flo,pet,dband)) {
		if (run > 0) {
		    /* needs to clip to second source, yuck */
		    if ((ix + run) > w) {
			if (ix < w)
			    (*(pvt->action)) (dvoid,svoid,tvoid, w-ix, ix, pvt);
			if (dvoid != svoid) {
			    run = sband->format->width - w;
			    if (run > 0)
	    	    		(*(pvt->passive)) (dvoid, svoid, run, w);
			}
			break;
		    }
	    	    (*(pvt->action)) (dvoid, svoid, tvoid, run, ix, pvt);
		    ix += run;
		} else {
		    if (dvoid != svoid)
	    	    	(*(pvt->passive)) (dvoid, svoid, -run, ix);
		    ix -= run; 
		}
	    }
	    svoid = GetNextSrc(flo,pet,sband,FLUSH);
	    tvoid = GetNextSrc(flo,pet,tband,FLUSH);
	    dvoid = GetNextDst(flo,pet,dband,FLUSH);
	}

	if(!svoid && sband->final) {
	    DisableSrc(flo,pet,tband,FLUSH);
	} else if(!tvoid && tband->final) { 
	    BypassSrc(flo,pet,sband);
	} else {
	    /* both inputs still active, keep the scheduler up to date  */
	    FreeData(flo,pet,sband,sband->current);
	    FreeData(flo,pet,tband,tband->current);
	}
    }
    return TRUE;
}

/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetArith(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{

#if !defined(KEEP_LUTS)
    ClearArith(flo,ped);
#endif

    ResetReceptors(ped);
    ResetProcDomain(ped);
    ResetEmitter(ped);
    return TRUE;
}

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyArith(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{

#if defined(KEEP_LUTS)
    ClearArith(flo,ped);
#endif

    /* get rid of the peTex structure and private structures  */
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

/*	xieTypArithmeticOp
**
**	#define xieValAdd	1
**	#define xieValSub	2
**	#define xieValSubRev	3
**	#define xieValMul	4
**	#define xieValDiv	5
**	#define xieValDivRev	6
**	#define xieValMin	7
**	#define xieValMax	8
**	#define xieValGamma	9
**
**      xieTypMathOp
**
**	#define xieValExp	1
**	#define xieValLn	2
**	#define xieValLog2	3
**	#define xieValLog10	4
**	#define xieValSquare	5
**	#define xieValSqrt	6
*/

#define	NADA	0 

#define AADD	D = S1 + S2; if (D >= nlev) D = nlev - 1;
#define ASUB	D = (S1 > S2) ? S1 - S2 : 0;
#define ASUBREV	D = (S2 > S1) ? S2 - S1 : 0;
#define AMIN	D = S1 < S2 ? S1 : S2;
#define AMAX	D = S1 > S2 ? S1 : S2;


#define FADD	D = S1 + S2;
#define FSUB	D = S1 - S2;
#define FSUBREV	D = S2 - S1;
#define FMUL	D = S1 * S2;
#define FDIV	D = S1 / S2;
#define FDIVREV	D = S1 ? S2 / S1 : S2;
#define FMIN	D = S1 < S2 ? S1 : S2;
#define FMAX	D = S1 > S2 ? S1 : S2;
					/* beware float vs double */
					/* consider using fpow() if available */
#define FGAM	D = pow(S1,S2);
#define IFGAM	D = (nlev - 1) * pow(S1/(nlev-1),S2);

#define BADD	D = S1 | S2;
#define BSUB	D = S1 & ~S2;
#define BSUBREV	D = S2 & ~S1;
#define BMUL	D = S1 & S2;
#define BDIV	D = S1;
#define BDIVREV	D = S2;
#define BMIN	D = S1 & S2;	/* same as BMUL */
#define BMAX	D = S1 | S2;	/* same as BADD */
#define BGAM	D = S1;


/*------------------------------------------------------------------------
---------------------  ROI operations work on subranges ------------------
------------------------------------------------------------------------*/

/* MROI: (*(pvt->action)) (dvoid, src1, run, ix, pvt); */
/* DROI: (*(pvt->action)) (dvoid, src1, src2, run, ix, pvt); */

#define MakeLutI(name1, expr)						\
static void name1(pvt)							\
    mpArithPvtPtr pvt;							\
{									\
    CARD32 *lut = pvt->lutptr;						\
    CARD32 nlev = pvt->nlev;						\
    CARD32 nclip = pvt->nclip;						\
    CARD32 i;								\
    CARD32 D, S1, S2 = pvt->iconstant;					\
    for ( i = 0; i < nlev ; i++) {					\
	S1 = i;								\
	expr;								\
	lut[i] = D;							\
    }									\
    for ( ; i < nclip ; i++) {						\
	lut[i] = 0;							\
    }									\
}

#define MakeLutF2(name1, expr)						\
static void name1(pvt)							\
    mpArithPvtPtr pvt;							\
{									\
    CARD32 *lut = pvt->lutptr;						\
    CARD32 nlev = pvt->nlev;						\
    CARD32 nclip = pvt->nclip;						\
    RealPixel Half = (RealPixel) .5;					\
    RealPixel dlev = (RealPixel) nlev - Half;				\
    RealPixel D, S1, S2 = pvt->fconstant;				\
    CARD32 i;								\
    for ( i = 0; i < nlev ; i++) {					\
	S1 = i;								\
	expr;								\
	D += .5;							\
	if (D < 0.) D = 0;						\
	else if (D > dlev) D = dlev;					\
	lut[i] = D;							\
    }									\
    for ( ; i < nclip ; i++) {						\
	lut[i] = 0;							\
    }									\
}

#define MakeLutF1(name1, expr)						\
static void name1(pvt)							\
    mpArithPvtPtr pvt;							\
{									\
    CARD32 *lut = pvt->lutptr;						\
    CARD32 nlev = pvt->nlev;						\
    CARD32 nclip = pvt->nclip;						\
    RealPixel Half = (RealPixel) .5;					\
    RealPixel dlev = (RealPixel) nlev - Half;				\
    RealPixel D, S1;							\
    CARD32 i;								\
    for ( i = 0; i < nlev ; i++) {					\
	S1 = i;								\
	expr;								\
	D += .5;							\
	if (D < 0.) D = 0;						\
	else if (D > dlev) D = dlev;					\
	lut[i] = D;							\
    }									\
    for ( ; i < nclip ; i++) {						\
	lut[i] = 0;							\
    }									\
}

#define MakeLook(name1, itype)						\
static void name1(dst1,src1,nx,x,pvt)					\
    pointer dst1, src1;							\
    CARD32 nx, x;							\
    mpArithPvtPtr pvt;							\
{									\
    itype *dst = ((itype *) dst1) + x;					\
    itype *src = ((itype *) src1) + x;					\
    CARD32 *lut = pvt->lutptr;						\
    CARD32 nmask = pvt->nclip - 1;					\
    for ( ; nx > 0 ; nx--) {						\
	*dst++ = lut[*src++ & nmask];					\
    }									\
}

MakeLutI(pr_a,  AADD )
MakeLutI(pr_s,  ASUB )
MakeLutI(pr_sr, ASUBREV )
MakeLutI(pr_mn, AMIN )
MakeLutI(pr_mx, AMAX )

MakeLutF2(pr_m,  FMUL )
MakeLutF2(pr_d,  FDIV )
MakeLutF2(pr_dr, FDIVREV )
MakeLutF2(pr_gm, IFGAM )

static void (*prep_mono[xieValGamma])() = {
pr_a, pr_s, pr_sr, pr_m, pr_d, pr_dr, pr_mn, pr_mx, pr_gm
};

MakeLook(lr_B, BytePixel)
MakeLook(lr_P, PairPixel)
MakeLook(lr_Q, QuadPixel)

static void (*action_lut[5])() = {
    NADA,
    NADA,
    lr_B,
    lr_P,
    lr_Q
};



/*
**  NOTE:
**	The MakePixI and MakePixF? macros may be used to synthesize monadic
**	action routines which can be used in place of look up table based
**	routines.  They were actually tested at one point in time.  The
**	invocations still remain below, and the routines can be plugged 
**	into the action_monoROI[itype][operator] table below.  As noted
**	elsewhere, these routines might be more appropriate for bigger
**	pixels to avoid lookup table explosion; or for machines without
**	a data cache, or a very small data cache.  IN summary, which
**	of these routines should be used depends on:
**		a) existence of data cache?
**		b) size, organization, and fill method of data cache?
**		c) speed of floating operations
*/

#define MakePixI(name1, itype, expr)					\
static void name1(dst1,src1,nx,x,pvt)					\
    pointer dst1, src1;							\
    CARD32 nx, x;							\
    mpArithPvtPtr pvt;							\
{									\
    itype *dst = (itype *) dst1;					\
    itype *src = (itype *) src1;					\
    CARD32 nlev = pvt->nlev;						\
    CARD32 D, S1, S2 = (itype) pvt->iconstant;				\
    for ( ; nx > 0 ; nx--, x++) {					\
	S1 = src[x];							\
	expr								\
	dst[x] = D;							\
    }									\
}

#define MakePixF2(name1, itype, expr)					\
static void name1(dst1,src1,nx,x,pvt)					\
    pointer dst1, src1;							\
    CARD32 nx, x;							\
    mpArithPvtPtr pvt;							\
{									\
    itype *dst = (itype *) dst1;					\
    itype *src = (itype *) src1;					\
    RealPixel Half = (RealPixel) .5;					\
    RealPixel dlev = (RealPixel) pvt->nlev - Half;			\
    RealPixel D, S1, S2 = (itype) pvt->fconstant;			\
    for ( ; nx > 0 ; nx--, x++) {					\
	S1 = (RealPixel) src[x];					\
	expr								\
	D += Half;							\
	if (D < 0.) D = 0;						\
	else if (D > dlev) D = dlev;					\
	dst[x] = (itype) D;						\
    }									\
}

#define MakePixF1(name1, itype, expr)					\
static void name1(dst1,src1,nx,x,pvt)					\
    pointer dst1, src1;							\
    CARD32 nx, x;							\
    mpArithPvtPtr pvt;							\
{									\
    itype *dst = (itype *) dst1;					\
    itype *src = (itype *) src1;					\
    RealPixel Half = (RealPixel) .5;					\
    RealPixel dlev = (RealPixel) pvt->nlev - Half;			\
    RealPixel D, S1;							\
    for ( ; nx > 0 ; nx--, x++) {					\
	S1 = (RealPixel) src[x];					\
	expr								\
	D += Half;							\
	if (D < 0.) D = 0;						\
	else if (D > dlev) D = dlev;					\
	dst[x] = (itype) D;						\
    }									\
}

#define DakePix(name2, itype, expr)					\
static void name2(dst1,src1,src2,nx,x,pvt)				\
    pointer dst1, src1, src2;						\
    CARD32 nx, x;							\
    mpArithPvtPtr pvt;							\
{									\
    itype *dst = ((itype *) dst1) + x;					\
    itype *src = ((itype *) src1) + x;					\
    itype *trc = ((itype *) src2) + x;					\
    CARD32 nlev = pvt->nlev;						\
    CARD32 D, S1, S2;							\
    for ( ; nx > 0 ; nx--) {						\
	S1 = *src++;							\
	S2 = *trc++;							\
	expr								\
	*dst++ = D;							\
    }									\
}

#define MakeFlt2(name1, expr)						\
static void name1(dst1,src1,nx,x,pvt)					\
    pointer dst1, src1;							\
    CARD32 nx, x;							\
    mpArithPvtPtr pvt;							\
{									\
    RealPixel *dst = ((RealPixel *) dst1) + x;				\
    RealPixel *src = ((RealPixel *) src1) + x;				\
    RealPixel D, S1, S2 = pvt->fconstant;				\
    for ( ; nx > 0 ; nx--) {						\
	S1 = *src++;							\
	expr								\
	*dst++ = D;							\
    }									\
}

#define MakeFlt1(name1, expr)						\
static void name1(dst1,src1,nx,x,pvt)					\
    pointer dst1, src1;							\
    CARD32 nx, x;							\
    mpArithPvtPtr pvt;							\
{									\
    RealPixel *dst = ((RealPixel *) dst1) + x;				\
    RealPixel *src = ((RealPixel *) src1) + x;				\
    RealPixel D, S1;							\
    for ( ; nx > 0 ; nx--) {						\
	S1 = *src++;							\
	expr								\
	*dst++ = D;							\
    }									\
}

#define DakeFlt(name2, expr)						\
static void name2(dst1,src1,src2,nx,x,pvt)				\
    pointer dst1, src1, src2;						\
    CARD32 nx, x;							\
    mpArithPvtPtr pvt;							\
{									\
    RealPixel *dst = ((RealPixel *) dst1) + x;				\
    RealPixel *src = ((RealPixel *) src1) + x;				\
    RealPixel *trc = ((RealPixel *) src2) + x;				\
    RealPixel D, S1, S2;						\
    for ( ; nx > 0 ; nx--) {						\
	S1 = *src++;							\
	S2 = *trc++;							\
	expr								\
	*dst++ = D;							\
    }									\
}

/* Bits no longer required by protocol */
#define	mr_b_a	NADA
#define	mr_b_s	NADA
#define	mr_b_sr	NADA
#define	mr_b_m	NADA
#define	mr_b_d	NADA
#define	mr_b_dr	NADA
#define	mr_b_mn	NADA
#define	mr_b_mx	NADA
#define	mr_b_gm	NADA

#define dr_b_a	NADA
#define	dr_b_s	NADA
#define	dr_b_sr	NADA
#define	dr_b_mn	NADA
#define	dr_b_mx	NADA

/* MakePixI	(mr_B_a,  BytePixel, AADD ) 	*/
/* MakePixI	(mr_B_s,  BytePixel, ASUB ) 	*/
/* MakePixI	(mr_B_sr, BytePixel, ASUBREV ) 	*/
/* MakePixI	(mr_B_mn, BytePixel, AMIN ) 	*/
/* MakePixI	(mr_B_mx, BytePixel, AMAX ) 	*/

/* MakePixF2	(mr_B_m,  BytePixel, FMUL ) 	*/
/* MakePixF2	(mr_B_d,  BytePixel, FDIV ) 	*/
/* MakePixF2	(mr_B_dr, BytePixel, FDIVREV ) 	*/
/* MakePixF2	(mr_B_gm, BytePixel, IFGAM ) 	*/

   DakePix	(dr_B_a,  BytePixel, AADD )
   DakePix	(dr_B_s,  BytePixel, ASUB )
   DakePix	(dr_B_sr, BytePixel, ASUBREV )
   DakePix	(dr_B_mn, BytePixel, AMIN )
   DakePix	(dr_B_mx, BytePixel, AMAX )

/* MakePixI	(mr_P_a,  PairPixel, AADD ) 	*/
/* MakePixI	(mr_P_s,  PairPixel, ASUB ) 	*/
/* MakePixI	(mr_P_sr, PairPixel, ASUBREV ) 	*/
/* MakePixI	(mr_P_mn, PairPixel, AMIN ) 	*/
/* MakePixI	(mr_P_mx, PairPixel, AMAX ) 	*/

/* MakePixF2	(mr_P_m,  PairPixel, FMUL ) 	*/
/* MakePixF2	(mr_P_d,  PairPixel, FDIV ) 	*/
/* MakePixF2	(mr_P_dr, PairPixel, FDIVREV ) 	*/
/* MakePixF2	(mr_P_gm, PairPixel, IFGAM ) 	*/

   DakePix	(dr_P_a,  PairPixel, AADD )
   DakePix	(dr_P_s,  PairPixel, ASUB )
   DakePix	(dr_P_sr, PairPixel, ASUBREV )
   DakePix	(dr_P_mn, PairPixel, AMIN )
   DakePix	(dr_P_mx, PairPixel, AMAX )

/* MakePixI	(mr_Q_a,  QuadPixel, AADD ) 	*/
/* MakePixI	(mr_Q_s,  QuadPixel, ASUB ) 	*/
/* MakePixI	(mr_Q_sr, QuadPixel, ASUBREV ) 	*/
/* MakePixI	(mr_Q_mn, QuadPixel, AMIN ) 	*/
/* MakePixI	(mr_Q_mx, QuadPixel, AMAX ) 	*/

/* MakePixF2	(mr_Q_m,  QuadPixel, FMUL ) 	*/
/* MakePixF2	(mr_Q_d,  QuadPixel, FDIV ) 	*/
/* MakePixF2	(mr_Q_dr, QuadPixel, FDIVREV ) 	*/
/* MakePixF2	(mr_Q_gm, QuadPixel, IFGAM ) 	*/

   DakePix	(dr_Q_a,  QuadPixel, AADD )
   DakePix	(dr_Q_s,  QuadPixel, ASUB )
   DakePix	(dr_Q_sr, QuadPixel, ASUBREV )
   DakePix	(dr_Q_mn, QuadPixel, AMIN )
   DakePix	(dr_Q_mx, QuadPixel, AMAX )

MakeFlt2	(mr_R_a,  FADD )
MakeFlt2	(mr_R_s,  FSUB )
MakeFlt2	(mr_R_sr, FSUBREV )
MakeFlt2	(mr_R_m,  FMUL )
MakeFlt2	(mr_R_d,  FDIV )
MakeFlt2	(mr_R_dr, FDIVREV )
MakeFlt2	(mr_R_mn, FMIN )
MakeFlt2	(mr_R_mx, FMAX )
MakeFlt2	(mr_R_gm, FGAM )

DakeFlt	(dr_R_a,  FADD )
DakeFlt	(dr_R_s,  FSUB )
DakeFlt	(dr_R_sr, FSUBREV )
DakeFlt (dr_R_mn, FMIN )
DakeFlt	(dr_R_mx, FMAX )

/*
ADD,	SUB,	SUBREV,	MUL,	DIV,	DIVREV,	MIN,	MAX,	GAMMA
*/

static void (*action_monoROI[5][xieValGamma])() = {
mr_R_a, mr_R_s, mr_R_sr, mr_R_m, mr_R_d, mr_R_dr, mr_R_mn, mr_R_mx, mr_R_gm,
mr_b_a, mr_b_s, mr_b_sr, mr_b_m, mr_b_d, mr_b_dr, mr_b_mn, mr_b_mx, mr_b_gm,
NADA,	NADA,	NADA,	NADA,	NADA,	NADA,	NADA,	NADA,	NADA,
NADA,	NADA,	NADA,	NADA,	NADA,	NADA,	NADA,	NADA,	NADA,
NADA,	NADA,	NADA,	NADA,	NADA,	NADA,	NADA,	NADA,	NADA
};

static void (*action_dyadROI[5][xieValGamma])() = {
dr_R_a,	dr_R_s, dr_R_sr, NADA, NADA, NADA, dr_R_mn, dr_R_mx, NADA,
dr_b_a, dr_b_s, dr_b_sr, NADA, NADA, NADA, dr_b_mn, dr_b_mx, NADA,
dr_B_a, dr_B_s, dr_B_sr, NADA, NADA, NADA, dr_B_mn, dr_B_mx, NADA,
dr_P_a, dr_P_s, dr_P_sr, NADA, NADA, NADA, dr_P_mn, dr_P_mx, NADA,
dr_Q_a, dr_Q_s, dr_Q_sr, NADA, NADA, NADA, dr_Q_mn, dr_Q_mx, NADA
};


extern void (*passive_copy[5])();

/*------------------------------------------------------------------------
-------------------------- . . . Math Sandbox . . . ----------------------
------------------------------------------------------------------------*/

/*
**  Math NOTES:
**	#define xieValExp	1
**	#define xieValLn	2
**	#define xieValLog2	3
**	#define xieValLog10	4
**	#define xieValSquare	5
**	#define xieValSqrt	6
**
**	log(==0) is -infinity with divide by zero exception. sigh.
**  	log( <0) is quiet NaN  with invalid operation exception, sigh.
**	sqrt(<0) might be error, 0., or -sqrt(-x) ???
**
**      for log2(), it may be quicker to count bits for integer arguments :-)
**
**	also, not all machines have a log2 and log10 in their math library
**	so we multiply log() by the magic values M_LOG2E and M_LOG10E.
**
**	one can argue that taking logs and exponents and such of small
**	integers is a somewhat frenetic exercise.  while it might make
**	sense to use square or squareroot as an image contrast enhancer,
**	it doesn't make sense when applied to small integers either.
**
**	just for the heck of it, here are some values for small inputs:
**	i       log2       logN       log10      exp        sqrt   square
**	0       -Inf       -Inf       -Inf          1          0	0
**	1          0          0          0      2.718          1	1
**	2          1     0.6931      0.301      7.389      1.414	4
**	3      1.585      1.099     0.4771      20.09      1.732	9
**	4          2      1.386     0.6021       54.6          2       16
**	5      2.322      1.609      0.699      148.4      2.236       25
**
**	also note that LN_MAXFLOAT (or LN_MAXDOUBLE) and MAXFLOAT are
**	typically defined in values.h, but including it conflicts with
**	misc.h on many machines.  values.h isn't a very modern .h file
**	as standards go these days.  help.....
*/

#ifndef M_LOG2E
#define M_LOG2E 1.4426950408889634074
#endif

#ifndef M_LOG10E
#define M_LOG10E 0.43429448190325182765
#endif

#ifndef LN_MAXFLOAT
#define LN_MAXFLOAT 88.7228394
#endif

#ifndef MAXFLOAT
#define MAXFLOAT ((float)3.40282346638528860e+38)
#endif

#define BANY	D = S1;
#define FEXP    D = (S1 <= (LN_MAXFLOAT-2.) ? exp(S1) :  MAXFLOAT);
#define FLGN	D = (S1 > 0. ? log(S1) : 0. );
#define FLG2	D = (S1 > 0. ? log(S1) * M_LOG2E : 0. );  
#define FLG10	D = (S1 > 0. ? log(S1) * M_LOG10E : 0. );
#define FSQR	D = S1 * S1;
#define FSQRT	D = (S1 >= 0. ? sqrt(S1) : 0. );

MakeLutF1(mpr_exp,  FEXP )
MakeLutF1(mpr_lgN,  FLGN )
MakeLutF1(mpr_lg2,  FLG2 )
MakeLutF1(mpr_lg10, FLG10)
MakeLutF1(mpr_sqr,  FSQR )
MakeLutF1(mpr_sqrt, FSQRT)	


/* Bits no longer required by protocol */
#define m_b_exp	 NADA
#define m_b_lgN  NADA
#define m_b_lg2  NADA
#define m_b_lg10 NADA
#define m_b_sqr	 NADA
#define m_b_sqrt NADA

MakePixF1	(m_P_sqr, PairPixel, FSQR)	/* ??? or use LUT */
MakePixF1	(m_Q_sqr, QuadPixel, FSQR)

MakeFlt1	(m_R_exp,  FEXP )
MakeFlt1	(m_R_lgN,  FLGN )
MakeFlt1	(m_R_lg2,  FLG2 )
MakeFlt1	(m_R_lg10, FLG10 )
MakeFlt1	(m_R_sqr,  FSQR )
MakeFlt1	(m_R_sqrt, FSQRT )

static void (*action_mathROI[5][xieValSqrt])() = {
/* EXP,   LOG,      LOG2,     LOG10,    SQUARE,   SQRT */
m_R_exp,  m_R_lgN,  m_R_lg2,  m_R_lg10, m_R_sqr,  m_R_sqrt,	/* floats */
m_b_exp,  m_b_lgN,  m_b_lg2,  m_b_lg10, m_b_sqr,  m_b_sqrt,	/* bits */
NADA,     NADA,     NADA,     NADA,     NADA,     NADA,		/* bytes */
NADA,     NADA,     NADA,     NADA,     m_P_sqr,  NADA,		/* pairs */
NADA,     NADA,     NADA,     NADA,     m_Q_sqr,  NADA		/* quads */
/* EXP,   LOG,      LOG2,     LOG10,    SQUARE,   SQRT */
};

static void (*prep_math[xieValSqrt])() = {
mpr_exp, mpr_lgN, mpr_lg2, mpr_lg10, mpr_sqr, mpr_sqrt
};

/*------------------------------------------------------------------------
-------------------------- initialize element . . . ----------------------
------------------------------------------------------------------------*/

static int SetupArith(flo,ped,modify)
    floDefPtr flo;
    peDefPtr  ped;
    int       modify;
{
    xieFloArithmetic *raw = (xieFloArithmetic *) ped->elemRaw;
    peTexPtr 	     pet = ped->peTex;
    pArithDefPtr    epvt = (pArithDefPtr)  ped->elemPvt;
    mpArithPvtPtr    pvt = (mpArithPvtPtr) pet->private;
    receptorPtr      rcp = pet->receptor;
    CARD32	  nbands = rcp[SRCt1].inFlo->bands;
    bandPtr	   sband = &(rcp[SRCt1].band[0]);
    bandPtr 	   tband = &(rcp[SRCt2].band[0]);
    bandPtr	   dband = &(pet->emitter[0]);
    CARD32	    band;

    for (band=0; band<nbands; band++, pvt++, sband++, tband++, dband++) {
	CARD32 iclass = IndexClass(sband->format->class);
	void (*act)() = 0;

	if ((raw->bandMask & (1<<band)) == 0) 
	    continue;

	if (!raw->src2) /* only used for mul, div, divrev, gamma */
	    pvt->fconstant = (RealPixel) epvt->constant[band];

	switch(raw->operator) {
	    case xieValDiv:	if (pvt->fconstant == 0.0) /* epsilon ?? */
				    pvt->fconstant = 1.0;
				break;
	}

	if (IsConstrained(sband->format->class)) {
	    pvt->nlev = sband->format->levels;
	    if (!raw->src2) {
		CARD32 deep;
		SetDepthFromLevels(pvt->nlev,deep); pvt->nclip = 1 << deep;
		/* only used for add, sub, subrev, min, max */
		pvt->iconstant = ConstrainConst(epvt->constant[band],pvt->nlev);
	    }
	}

	/* Try to find a dyadic operator */
	if (!act && raw->src2)
	    act = action_dyadROI[iclass][raw->operator-1];

	/*
	**  NOTE:
	**	For larger pixels a look up table begins to be less
	**	attractive since the malloc is larger, it takes longer
	**	to compute, and the cache hit rate may go down.  So
	**	we may want to add more actual monodaic arithmetic
	**	elements to the table (see MakePixel, it worked once
	**	upon a time) and choose between an arithmetic and lookup
	**	version based on number of levels.
	*/

	/* Try to find a monadic operator */
	if (!act && !raw->src2)
	    act = action_monoROI[iclass][raw->operator-1];

	/* Or maybe a monadic look up table op */
	if (!act && !raw->src2) {
	     act = action_lut[iclass];
	     if (act) {
		/* Allocate Lut and Fill it in */
		if (!(pvt->lutptr = (CARD32 *)
			XieMalloc(pvt->nclip * sizeof(CARD32))))
		    AllocError(flo,ped,return(FALSE));
		(*prep_mono[raw->operator-1]) (pvt);
	     }
	}

	if (!act)
	    ImplementationError(flo,ped,return(FALSE));
	pvt->action = act;
	pvt->passive = passive_copy[iclass];
    }
    return TRUE;
}

static int SetupMath(flo,ped,modify)
    floDefPtr flo;
    peDefPtr  ped;
    int       modify;
{
    xieFloMath	    *raw = (xieFloMath *) ped->elemRaw;
    peTexPtr 	     pet = ped->peTex;
    mpArithPvtPtr    pvt = (mpArithPvtPtr) pet->private;
    receptorPtr      rcp = pet->receptor;
    CARD32	  nbands = rcp[SRCt1].inFlo->bands;
    bandPtr	   sband = &(rcp[SRCt1].band[0]);
    bandPtr	   dband = &(pet->emitter[0]);
    CARD32	    band;

    for (band=0; band<nbands; band++, pvt++, sband++, dband++) {
	CARD32 iclass = IndexClass(sband->format->class);
	void (*act)() = 0;

	if ((raw->bandMask & (1<<band)) == 0) 
	    continue;

	if (IsConstrained(sband->format->class)) {
	    CARD32 deep;
	    pvt->nlev = sband->format->levels;
	    SetDepthFromLevels(pvt->nlev,deep); pvt->nclip = 1 << deep;
	}

	/*
	**  NOTE:
	**	For larger sized pixels, a lookup table my be counter
	**	productive.  In addition to the malloc space (eg 64k 
	**	words), and the low cache hit rate, the time to fill the
	**	lookup table can become significant.  Even for medium
	**	sized pixels, some sort of hashed cache might be a 
	**	better solution.  But I seriously doubt anyone will
	**	use these operations on integer pixels very much ...
	*/

	/* Try to find a monadic operator */
	if (!act)
	    act = action_mathROI[iclass][raw->operator-1];

	/* Or maybe a monadic look up table operator */
	if (!act) {
	     act = action_lut[iclass];
	     if (act) {
		/* Allocate Lut and Fill it in */
		if (!(pvt->lutptr = (CARD32 *)
			XieMalloc(pvt->nclip * sizeof(CARD32))))
		    AllocError(flo,ped,return(FALSE));
		(*prep_math[raw->operator-1]) (pvt);
	     }
	}
	if (!act)
	    ImplementationError(flo,ped,return(FALSE));

	pvt->action = act;
	pvt->passive = passive_copy[iclass];
    }
    return TRUE;
}
	
static void ClearArith(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    mpArithPvtPtr pvt = (mpArithPvtPtr) (ped->peTex->private);
    int band;

    /* free any dynamic private data */
    if (pvt)
	for (band = 0 ; band < xieValMaxBands ; band++, pvt++)
	    if (pvt->lutptr)
		pvt->lutptr = (CARD32 *) XieFree(pvt->lutptr);

}

/* end module mparith.c */
