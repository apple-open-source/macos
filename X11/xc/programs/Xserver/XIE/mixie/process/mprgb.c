/* $Xorg: mprgb.c,v 1.4 2001/02/09 02:04:32 xorgcvs Exp $ */
/**** module mprgb.c ****/
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
  
	mprgb.c -- DDXIE Convert To/From RGB elements
  
	Larry Hare -- AGE Logic, Inc. August, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/process/mprgb.c,v 3.5 2001/12/14 19:58:46 dawes Exp $ */


#define _XIEC_MPRGB
#define _XIEC_PCFRGB
#define _XIEC_PCTRGB

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
#include <technq.h>
#include <xiemd.h>
#include <memory.h>

/*
 *  routines referenced by other DDXIE modules
 */
int	miAnalyzeToRGB();
int	miAnalyzeFromRGB();

/*
 *  routines used internal to this module
 */

static int CreateToRGB();
static int InitializeToRGB();
static int SetupToRGB();

static int CreateFromRGB();
static int InitializeFromRGB();
static int SetupFromRGB();

static int ResetRGB();
static int DestroyRGB();
static void ClearRGB();
static void CheckRGB();

static int ActivateRGB();

/*
 * DDXIE Convert To/From RGB element entry points.
 */
static ddElemVecRec ToRGBVec = {
  CreateToRGB,
  InitializeToRGB,
  ActivateRGB,
  (xieIntProc) NULL,
  ResetRGB,
  DestroyRGB
  };

static ddElemVecRec FromRGBVec = {
  CreateFromRGB,
  InitializeFromRGB,
  ActivateRGB,
  (xieIntProc) NULL,
  ResetRGB,
  DestroyRGB
  };

/*
**  Local Declarations.
**	NOTE: use #define EARLY_SETUP to do setup at create time
**	NOTE: uses RGBFloat's for matrix multiply.
*/

#define EARLY_SETUP
typedef RealPixel RGBFloat;

typedef struct _mprgbdef {
    void	(*action) ();		/* every one needs these */
    void	(*post)   ();		/* clipping, or CIELab, for floats */
    CARD32	iclip[3];		/* clip values for integers */
    pointer	(*cvt_in[3])();	/* !0 if need to expand input */
    pointer	(*cvt_out[3])();	/* !0 if need to compress output */
    pointer	aux_buf[3];		/* used for cvt_in/cvt_out */
    RGBFloat	matrix[12];
    INT32	imatrix[12];
} mpRGBPvtRec, *mpRGBPvtPtr;

/*
**   NOTES on BASIC ALGORITHM:
**	1)  All transforms center on a matrix.  Either a passed in XYZMAT
**	    or one synthesized from Luma values.
**	2)  White Point adjustments, if specified are applied to this matrix.
**	3)  Bias (YCbCr) or Implicit Biasing and Scaling (YCC) are applied.
**		(Float to Float stops here, matmul is done as floats)
**	4)  If inputs are integers, a scale is applied to reduce input 
**	    input numbers to range 0.0 to 1.0.
**		(Int to Float stops here, matmul is done as floats)
**	5)  If outputs are integers, a scale is applied to expand values
**	    back to output range.  For tripleband with all 256 levels
**	    for inputs and outputs, this cancels step 4.  But allows
**	    for mapping say an (8,8,4) image to say (16,4,4).  This
**	    final matrix is turned into a scaled fraction integer matrix.
**		(Int to Int stops here, matmul is done as scaled fractions)
*/

/*------------------------------------------------------------------------
------------------------  fill in the vector  ---------------------------
------------------------------------------------------------------------*/
int miAnalyzeToRGB(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    ped->ddVec = ToRGBVec;
    return TRUE;
}

int miAnalyzeFromRGB(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    ped->ddVec = FromRGBVec;
    return TRUE;
}

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int
CreateToRGB(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    if (!MakePETex(flo,ped,sizeof(mpRGBPvtRec), NO_SYNC, SYNC))
	return FALSE;

#if defined(EARLY_SETUP)
    if (!SetupToRGB(flo, ped, 0 /* modify ?? */))
	return FALSE;
#endif
 
    return TRUE;
} 

static int
CreateFromRGB(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    /* always force syncing between inputs (is nop if only one input) */
    if (!MakePETex(flo,ped,sizeof(mpRGBPvtRec), NO_SYNC, SYNC))
	return FALSE;

#if defined(EARLY_SETUP)
    if (!SetupFromRGB(flo, ped, 0 /* modify ?? */))
	return FALSE;
#endif
 
    return TRUE;
} 

/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/

static int
InitializeToRGB(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
#if !defined(EARLY_SETUP)
    if (!SetupToRGB(flo, ped, 0 /* modify ?? */))
	return FALSE;
#endif

    InitReceptors(flo, ped, NO_DATAMAP, 1);
    InitEmitter(flo, ped, NO_DATAMAP, NO_INPLACE);

    return !ferrCode(flo);
}

static int
InitializeFromRGB(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
#if !defined(EARLY_SETUP)
    if (!SetupFromRGB(flo, ped, 0 /* modify ?? */))
	return FALSE;
#endif

    InitReceptors(flo, ped, NO_DATAMAP, 1);
    InitEmitter(flo, ped, NO_DATAMAP, NO_INPLACE);

    return !ferrCode(flo);
}

/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/

static int
ActivateRGB(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
    mpRGBPvtPtr   pvt = (mpRGBPvtPtr) pet->private;
    bandPtr	sband = &(pet->receptor[SRCt1].band[0]);
    bandPtr	dband = &(pet->emitter[0]);
    CARD32	 npix = sband->format->width;
    pointer svoid[3], dvoid[3], stvoid[3], dtvoid[3];
    CARD32	    b;
    BOOL	 stop;

    for (b = 0; b < 3; b++, sband++, dband++) {
	if (!(svoid[b] = GetCurrentSrc(flo,pet,sband)))
            return TRUE;
	if (!(dvoid[b] = GetCurrentDst(flo,pet,dband)))
	    return TRUE;
	stvoid[b] = pvt->cvt_in[b]
		? (*pvt->cvt_in[b]) (pvt->aux_buf[b], svoid[b], pvt, npix)
		: svoid[b] ;
	dtvoid[b] = pvt->cvt_out[b] ? pvt->aux_buf[b] : dvoid[b];
    }   sband -= 3; dband -= 3;

    do {

	(*(pvt->action)) (dtvoid, stvoid, pvt, npix);

	if (pvt->post) (*(pvt->post)) (dtvoid, npix);

	for (b = 0, stop = FALSE; b < 3; b++, sband++, dband++) {
	    if (pvt->cvt_out[b])
		(*pvt->cvt_out[b]) (dvoid[b], dtvoid[b], pvt, npix);
	    stop |= !(svoid[b] = GetNextSrc(flo,pet,sband,FLUSH));
	    stop |= !(dvoid[b] = GetNextDst(flo,pet,dband,FLUSH));
	    if (!stop) {
		dtvoid[b] = pvt->cvt_out[b] ? dtvoid[b] : dvoid[b];
		stvoid[b] = pvt->cvt_in[b]
		    ? (*pvt->cvt_in[b]) (stvoid[b], svoid[b], pvt, npix)
		    : svoid[b] ;
	    }
	}   sband -= 3; dband -= 3;

    } while (!ferrCode(flo) && !stop) ;

    for (b = 0; b < 3; b++, sband++) {
	FreeData(flo, pet, sband, sband->current); 
    }	sband -= 3;

    return TRUE;
}

/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int
ResetRGB(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{

#if !defined(EARLY_SETUP)
    ClearRGB(flo, ped);
#endif

    ResetReceptors(ped);
    ResetProcDomain(ped);
    ResetEmitter(ped);
    return TRUE;
}

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int
DestroyRGB(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{

#if defined(EARLY_SETUP)
    ClearRGB(flo, ped);
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
------------------------  Action Utility Routines  -----------------------
------------------------------------------------------------------------*/

/*
 * Used for CIELab conversions.
 * 
 * NOTE: Investigate use of stock cbrt() function in libc.
 * NOTE: Borrowed from lib/X/XcmsMath.h
 * NOTE: Copyright 1990 The Open Group
 *
 * NOTE: Investigate use of stock cbrt() function in libc.
 * NOTE: Used better choice of seed values with emphasis on
 *	 range of 0 to 1, and other smallish numbers.
 */

#if !defined(XCMS_CUBEROOT)
#define XCMS_CUBEROOT _cmsCubeRoot

/* Newton's Method:  x_n+1 = x_n - ( f(x_n) / f'(x_n) ) */
/* for cube roots, x^3 - a = 0,  x_new = x - 1/3 (x - a/x^2) */
#ifndef DBL_EPSILON
#define DBL_EPSILON 1e-6
#endif

double
_cmsCubeRoot(a)
    double a;
{
    register double abs_a, cur_guess, delta;
    if (a == 0.)
	return 0.;

    /* convert to positive to speed loop tests */
    abs_a = a < 0. ? -a : a;

    /* arbitrary first guess */
    cur_guess = abs_a  < 1.    ?  0.5  + .500  * abs_a :
	        abs_a  < 1000. ?  1.0  + .125  * abs_a :
			          10.0 + .0125 * abs_a ;
    do {
	delta = (cur_guess - abs_a/(cur_guess*cur_guess))/3.;
	cur_guess -= delta;
	if (delta < 0.) delta = -delta;
    } while (delta >= cur_guess*DBL_EPSILON);

    return a > 0. ? cur_guess : -cur_guess;
}
#endif

/*------------------------------------------------------------------------
---------------------  Lotsa Little Action Routines  ---------------------
------------------------------------------------------------------------*/

/* (*(pvt->cvt_out)) (dvoid, svoid, pvt, npix); */
/* (*(pvt->cvt_in))  (dvoid, svoid, pvt, npix); */
/* (*(pvt->action))  (dvoid, svoid, pvt, npix); */
/* (*(pvt->post))    (dvoid, npix); */

static pointer
cvt_bit_to_pair(dvoid,svoid,pvt,npix)
    pointer dvoid, svoid;
    mpRGBPvtPtr pvt;
    CARD32	npix;
{
    LogInt	*bitp  = (LogInt *) svoid, ival, M;
    PairPixel	*pairp = (PairPixel *) dvoid; 
    int		    nw = npix >> LOGSHIFT;
    for ( ; nw > 0; nw--)
	for (ival = *bitp++, M=LOGLEFT; M ; LOGRIGHT(M))
	    *pairp++ = (ival & M) ? (PairPixel) 1 : (PairPixel) 0;
    if ((npix &= LOGMASK))
	for (ival = *bitp, M=LOGLEFT; npix > 0 ; npix--, LOGRIGHT(M))
	    *pairp++ = (ival & M) ? (PairPixel) 1 : (PairPixel) 0;
    return dvoid;
}

static pointer
cvt_byte_to_pair(dvoid,svoid,pvt,npix)
    pointer dvoid, svoid;
    mpRGBPvtPtr pvt;
    CARD32	npix;
{
    BytePixel *bytep = (BytePixel *) svoid;
    PairPixel *pairp = (PairPixel *) dvoid; 
    for ( ; npix > 0; npix--)
	*pairp++ = *bytep++;
    return dvoid;
}

static pointer
cvt_pair_to_byte(dvoid,svoid,pvt,npix)
    pointer dvoid, svoid;
    mpRGBPvtPtr pvt;
    CARD32	npix;
{
    PairPixel *pairp = (PairPixel *) svoid; 
    BytePixel *bytep = (BytePixel *) dvoid;
    for ( ; npix > 0; npix--)
	*bytep++ = *pairp++; /* already hard-clipped!! */
    return dvoid;
}

static pointer
cvt_pair_to_bit(dvoid,svoid,pvt,npix)
    pointer dvoid, svoid;
    mpRGBPvtPtr pvt;
    CARD32	npix;
{
    PairPixel *pairp = (PairPixel *) svoid; 
    LogInt    *bitp  = (LogInt *) dvoid, bitval, M;
    for ( ; npix >= LOGSIZE; *bitp++ = bitval, npix -= LOGSIZE)
	for (M=LOGLEFT, bitval = 0; M; LOGRIGHT(M))
	    if (*pairp++)
		bitval |= M;
    if (npix > 0) {
	for (M=LOGLEFT, bitval = 0; npix > 0; npix--, LOGRIGHT(M))
	    if (*pairp++)
		bitval |= M;
	*bitp = bitval;
    }
    return dvoid;
}

static void
act_mmRR(dvoid, svoid, pvt, npix)
    RealPixel	*svoid[3], *dvoid[3];
    mpRGBPvtPtr pvt;
    CARD32	npix;
{
    RGBFloat	*mtrx = &(pvt->matrix[0]);
    RGBFloat	r, g, b;
    CARD32      i;

    /*
    ** dvector = matrix * svector
    */
    for (i = 0; i < npix; i++) {
	r = svoid[0][i];
	g = svoid[1][i];
	b = svoid[2][i];
	dvoid[0][i] = mtrx[0] * r  +  mtrx[1] * g  +  mtrx[2] * b;
	dvoid[1][i] = mtrx[3] * r  +  mtrx[4] * g  +  mtrx[5] * b;
	dvoid[2][i] = mtrx[6] * r  +  mtrx[7] * g  +  mtrx[8] * b;
    }
}

#define MakeIntFltMatMul(name, itype)					\
static void								\
name(dvoid, svoid, pvt, npix)						\
    itype	*svoid[3];						\
    RealPixel   *dvoid[3];						\
    mpRGBPvtPtr pvt;							\
    CARD32	npix;							\
{									\
    RealPixel 	*mtrx = &(pvt->matrix[0]);				\
    RealPixel   r, g, b, rr, gg, bb;					\
    CARD32      i;							\
									\
    for (i = 0; i < npix; i++) {					\
	r = (RealPixel) svoid[0][i];					\
	g = (RealPixel) svoid[1][i];					\
	b = (RealPixel) svoid[2][i];					\
	rr = mtrx[0] * r  +  mtrx[1] * g  +  mtrx[2] * b;		\
	gg = mtrx[3] * r  +  mtrx[4] * g  +  mtrx[5] * b;		\
	bb = mtrx[6] * r  +  mtrx[7] * g  +  mtrx[8] * b;		\
	dvoid[0][i] = rr;						\
	dvoid[1][i] = gg;						\
	dvoid[2][i] = bb;						\
    }									\
}

MakeIntFltMatMul(act_mmBR, BytePixel)
MakeIntFltMatMul(act_mmPR, PairPixel)

#define MakeIntIntMatMul(name, iotype, shift)				\
static void								\
name(dvoid, svoid, pvt, npix)						\
    iotype	*svoid[3], *dvoid[3];					\
    mpRGBPvtPtr pvt;							\
    CARD32	npix;							\
{									\
    INT32 	*mtrx = &(pvt->imatrix[0]);				\
    INT32       r, g, b, rr, gg, bb;					\
    CARD32	iclip0 = pvt->iclip[0];					\
    CARD32	iclip1 = pvt->iclip[1];					\
    CARD32	iclip2 = pvt->iclip[2];					\
    INT32	bias0 = mtrx[9]  + (1 << (shift-1));			\
    INT32	bias1 = mtrx[10] + (1 << (shift-1));			\
    INT32	bias2 = mtrx[11] + (1 << (shift-1));			\
    CARD32      i;							\
									\
    for (i = 0; i < npix; i++) {					\
	r = svoid[0][i];						\
	g = svoid[1][i];						\
	b = svoid[2][i];						\
	rr = mtrx[0] * r  +  mtrx[1] * g  +  mtrx[2] * b  +  bias0;	\
	gg = mtrx[3] * r  +  mtrx[4] * g  +  mtrx[5] * b  +  bias1;	\
	bb = mtrx[6] * r  +  mtrx[7] * g  +  mtrx[8] * b  +  bias2;	\
	if (rr < 0) rr = 0;						\
	if (gg < 0) gg = 0;						\
	if (bb < 0) bb = 0;						\
	if ((rr >>= shift) > iclip0) rr = iclip0; 			\
	if ((gg >>= shift) > iclip1) gg = iclip1; 			\
	if ((bb >>= shift) > iclip2) bb = iclip2;			\
	dvoid[0][i] = rr;						\
	dvoid[1][i] = gg;						\
	dvoid[2][i] = bb;						\
    }									\
}

#define SF_BYTESHIFT 20
#define SF_PAIRSHIFT 12
MakeIntIntMatMul(act_mmBB, BytePixel, SF_BYTESHIFT)
MakeIntIntMatMul(act_mmPP, PairPixel, SF_PAIRSHIFT)


static void
act_postClipR(dvoid, npix)
    RealPixel	*dvoid[3];
{
    RGBFloat	*fp, f, zero = (RGBFloat) 0.0, one = (RGBFloat) 1.0;
    CARD32	ipix, band;

    for (band = 0; band < 3; band++) {
	fp = dvoid[band];
	for (ipix = npix; ipix > 0; ipix--, fp++) {
	    f = *fp;
	    if ( f < zero) *fp = zero;
	    if ( f > one)  *fp = one;
	}
    }
}

static void
act_postCIELab(dvoid, npix)
    RealPixel	*dvoid[3];
{
    RGBFloat	*xp, *yp, *zp, x3, y3, z3;
    CARD32	ipix;

    xp = dvoid[0];
    yp = dvoid[1];
    zp = dvoid[2];
    for (ipix = npix; ipix > 0; ipix--) {

	/* XXX, if numbers are small, do some more work */
	x3 = XCMS_CUBEROOT((double)*xp);
	y3 = XCMS_CUBEROOT((double)*yp);
	z3 = XCMS_CUBEROOT((double)*zp);

	*xp++ = 116.0 * y3 - 16.0;	/* L* */
	*yp++ = 500.0 * (x3 - y3);	/* a* */
	*zp++ = 200.0 * (y3 - z3);	/* b* */
    }
}

static void
act_preCIELab(dvoid, svoid, pvt, npix)
    RealPixel	*svoid[3], *dvoid[3];
    mpRGBPvtPtr pvt;
    CARD32	npix;
{
    RGBFloat	*Lp, *ap, *bp, *xp, *yp, *zp, L, a, b;
    CARD32	ipix;

    /* must be careful not to smash the source */
    Lp = svoid[0]; ap = svoid[1]; bp = svoid[2];
    xp = dvoid[0]; yp = dvoid[1]; zp = dvoid[2];

    for (ipix = npix; ipix > 0; ipix--) {

	/* XXX, if numbers are small, do some more work */
	L = (*Lp++ + 16.0) * (1.0 / 116.0);
	a = L + (*ap++ * 0.002);
	b = L - (*bp++ * 0.005);

	*yp++ = (L * L * L);	/* Y = ... */
	*xp++ = (a * a * a);	/* X = ... */
	*zp++ = (b * b * b);	/* Z = ... */
    }
    act_mmRR(dvoid, dvoid, pvt, npix);
}

/*------------------------------------------------------------------------
-------------------- utility routines for initialization  ----------------
------------------------------------------------------------------------*/

/* Scale entire matrix by single scale factor */

static void
scale_mtrx(mtx,imtx,iscl)
    RGBFloat    *mtx;
    INT32	*imtx;
    CARD32	iscl;
{
    RGBFloat	fscl = iscl;
    int		i;

    for (i = 0; i < 12; i++)
	*imtx++ = *mtx++ * fscl;
}

/* Scale each row of matrix by respective factors */

void
scale_rows(mtrx,scale1,scale2,scale3)
    RGBFloat	*mtrx;
    double	scale1, scale2, scale3;
{
    int b;
    for (b = 0; b < 3; b++) *mtrx++ *= scale1;
    for (b = 0; b < 3; b++) *mtrx++ *= scale2;
    for (b = 0; b < 3; b++) *mtrx++ *= scale3;
}

/* Scale each column of matrix by respective factors */

void
scale_columns(mtrx,scale1,scale2,scale3)
    RGBFloat	*mtrx;
    double	scale1, scale2, scale3;
{
    int b;
    for (b = 0; b < 3; b++) {
	*mtrx++ *= scale1;
	*mtrx++ *= scale2;
	*mtrx++ *= scale3;
    }
}

/* 
**  Our matrix routines for integers use a BIAS like so:
**	 YCC = MATRIX * RGB - BIAS
**  But when we are going back to RGB we would have liked to do:
**	RGB = MATRIX * (YCC - BIAS)
**  So we multiply do transitive property of matrix arithmetic
**  to get a new BIAS' (BIAS' = -MATRIX * BIAS) such that:
**	RGB = MATRIX * YCC - MATRIX * BIAS = MATRIX * YCC - BIAS'
*/
static void
flip_bias(mtx)
    RGBFloat    *mtx;
{
    RGBFloat	mtx9, mtx10, mtx11;
    mtx9  = -(mtx[0] * mtx[9] + mtx[1] * mtx[10] + mtx[2] * mtx[11]);
    mtx10 = -(mtx[3] * mtx[9] + mtx[4] * mtx[10] + mtx[5] * mtx[11]);
    mtx11 = -(mtx[6] * mtx[9] + mtx[7] * mtx[10] + mtx[8] * mtx[11]);

    mtx[9]  = mtx9;
    mtx[10] = mtx10;
    mtx[11] = mtx11;
}



/* Seed matrix for CIEXYZ and CIELab */

static void
copymatrix(pvt,input)
    mpRGBPvtPtr pvt;
    double *input;
{ 
    RGBFloat *mtrx = pvt->matrix;
    int i;
	
    for (i = 0; i < 9; i++)
	*mtrx++ = *input++;

    for (; i < 12; i++)
	*mtrx++ = 0.0; 	/* bias */
}

static void
copywhiteLABFromRGB(pvt,tec,vec)
    mpRGBPvtPtr pvt;
    xieTypWhiteAdjustTechnique tec;
    double *vec;
{
    RGBFloat *mtrx = pvt->matrix;
    double a, b, c;

    switch (tec) {
    case xieValWhiteAdjustCIELabShift:	a = vec[0];
					b = vec[1];
					c = vec[2];
					break;

    case xieValWhiteAdjustDefault:
    case xieValWhiteAdjustNone:		/* calculate from pvt->matrix */
					a = mtrx[0] + mtrx[1] + mtrx[2];
					b = mtrx[3] + mtrx[4] + mtrx[5];
					c = mtrx[6] + mtrx[7] + mtrx[8];
					break;

    default:				 return;
    }

    if (a < .0001) return;
    if (b < .0001) return;
    if (c < .0001) return;
    scale_rows (mtrx, 1.0 / a, 1.0 / b, 1.0 / c);
    return;
}

static void
copywhiteLABToRGB(pvt,tec,vec)
    mpRGBPvtPtr pvt;
    xieTypWhiteAdjustTechnique tec;
    double *vec;
{
    RGBFloat *mtrx = pvt->matrix;
    double a, b, c;
    switch (tec) {
    case xieValWhiteAdjustCIELabShift:	a = vec[0];
					b = vec[1];
					c = vec[2];
					break;

    case xieValWhiteAdjustDefault:
    case xieValWhiteAdjustNone:	
	{ /* White Point comes from XYZ Matrix. Must invert it first */
	RGBFloat *m = mtrx - 1;		/* number matrix from 1 to 9 */
	double determinant = m[1] * (m[5]*m[9] - m[6]*m[8]) -
			     m[2] * (m[4]*m[9] - m[6]*m[7]) +
			     m[3] * (m[4]*m[8] - m[5]*m[7]);
	a  =  (m[5]*m[9]-m[6]*m[8]);  /* inv[00] */
	a += -(m[2]*m[9]-m[3]*m[8]);  /* inv[01] */
	a +=  (m[2]*m[6]-m[3]*m[5]);  /* inv[02] */
	b  = -(m[4]*m[9]-m[6]*m[7]);  /* inv[10] */
	b +=  (m[1]*m[9]-m[3]*m[7]);  /* inv[11] */
	b += -(m[1]*m[6]-m[3]*m[4]);  /* inv[12] */
	c  =  (m[4]*m[8]-m[5]*m[7]);  /* inv[20] */
	c += -(m[1]*m[8]-m[2]*m[7]);  /* inv[21] */
	c +=  (m[1]*m[5]-m[2]*m[4]);  /* inv[22] */

	a /= (determinant ? determinant : 1.0);
	b /= (determinant ? determinant : 1.0);
	c /= (determinant ? determinant : 1.0);
	}
					break;

    default:				return;
    }
    scale_columns (pvt->matrix, a, b, c);
    return;
}

static void
copywhiteXYZFromRGB(pvt,tec,vec)
    mpRGBPvtPtr pvt;
    xieTypWhiteAdjustTechnique tec;
    double *vec;
{
    switch (tec) {
    case xieValWhiteAdjustCIELabShift:	break;
    case xieValWhiteAdjustDefault:	return;
    case xieValWhiteAdjustNone:		return;
    default:				return;
    }
    if (vec[0] < .0001) return;
    if (vec[1] < .0001) return;
    if (vec[2] < .0001) return;

    scale_rows (pvt->matrix, 1.0 / vec[0], 1.0 / vec[1], 1.0 / vec[2]);
}

static void
copywhiteXYZToRGB(pvt,tec,vec)
    mpRGBPvtPtr pvt;
    xieTypWhiteAdjustTechnique tec;
    double *vec;
{
    switch (tec) {
    case xieValWhiteAdjustCIELabShift:	break;
    case xieValWhiteAdjustDefault:	return;
    case xieValWhiteAdjustNone:		return;
    default:				return;
    }
    scale_columns (pvt->matrix, vec[0], vec[1], vec[2]);
}

static void
copygamut(pvt,tec)
    mpRGBPvtPtr pvt;
    xieTypGamutTechnique tec;
{
    if (tec == xieValGamutClipRGB)
	pvt->post = act_postClipR;
}

static void
copybiasYCbCr(pvt,bias0,bias1,bias2)
    mpRGBPvtPtr pvt;
    double bias0, bias1, bias2;
{
    pvt->matrix[9]  = bias0;
    pvt->matrix[10] = bias1;
    pvt->matrix[11] = bias2; 
}

static void
copylumaYCbCrfromRGB(pvt,LumaRed,LumaGreen,LumaBlue)
    mpRGBPvtPtr pvt;
    double LumaRed,LumaGreen,LumaBlue;
{
    RGBFloat *mtrx = pvt->matrix;

    if (LumaRed   < .01) LumaRed   = .01;
    if (LumaGreen < .01) LumaGreen = .01;
    if (LumaBlue  < .01) LumaBlue  = .01;
    if (LumaRed   > .99) LumaRed   = .99;
    if (LumaGreen > .99) LumaGreen = .99;
    if (LumaBlue  > .99) LumaBlue  = .99;

    /*
    **  FROM TIFF 6.0
    **
    **	Y  = (LumaRed*R + LumaGreen * G + LumaBlue * B)
    **	Cb = (B - Y) / (2 - 2*LumaBlue)
    **	Cr = (R - Y) / (2 - 2*LumaRed)
    **
    **  Cb = .5 * (B - (Lr*R + Lg*G + Lb*B)) / (1 - Lb)
    **  Cb = .5B/(1-Lb) - .5(Lr*R + Lg*G + Lb*B)) / (1-Lb)
    **  Cb = .5B/(1-Lb) - .5 Lr*R/(1-Lb) -.5 Lg*G/(1-Lb) -.5 Lb*B/(1-Lb)
    **  Cb = .5B(1-Lb)/(1-Lb) -.5 Lr*R/(1-Lb) -.5 Lg*G/(1-Lb)
    **  Cb = .5B -.5 Lr*R/(1-Lb) -.5 Lg*G/(1-Lb)
    */
    mtrx[0] = LumaRed;
    mtrx[1] = LumaGreen;
    mtrx[2] = LumaBlue;
    mtrx[3] = -0.5 * LumaRed   / (1.0 - LumaBlue);
    mtrx[4] = -0.5 * LumaGreen / (1.0 - LumaBlue);
    mtrx[5] =  0.5;
    mtrx[6] =  0.5;
    mtrx[7] = -0.5 * LumaGreen / (1.0 - LumaRed);
    mtrx[8] = -0.5 * LumaBlue  / (1.0 - LumaRed);

    mtrx[9] = mtrx[10] = mtrx[11] = 0.0;	/* bias */
}

static void
copylumaYCbCrtoRGB(pvt,LumaRed,LumaGreen,LumaBlue)
    mpRGBPvtPtr pvt;
    double LumaRed,LumaGreen,LumaBlue;
{
    RGBFloat *mtrx = pvt->matrix;

    /* should/could check that LumaRed + LumaGreen + LumaBlue approx == 1.0 */

    if (LumaGreen < .01) LumaGreen = .01;
    if (LumaGreen > .99) LumaGreen = .99;

    /*
    **  FROM TIFF 6.0
    **
    **	R = Cr * (2 - 2 * LumaRed) + Y
    **  G = (Y - LumaBlue * B - LumaRed * R) / LumaGreen
    **	B = Cb * (2 - 2 * LumaBlue) + Y
    **
    **  G = (Y - Lb*(Cb*(2-2*Lb) + Y) - Lr*(Cr*(2-2*Lr) + Y) ) / Lg
    **  G = (Y - Lb*Cb*(2-2*Lb) - Lb*Y - Lr*Cr*(2-2*Lr) - Lr*Y ) / Lg
    **  G = ((1 - Lb - Lr)*Y  - 2*Lb*(1-Lb)*Cb - 2*Lr*(1-Lr)*Cr ) / Lg
    **
    **	R = 1 * Y  +  0 * Cb  +  (2 - 2 * LumaRed) * Cr 
    **  G = 1 * Y  +  ...
    **	B = 1 * Y  +  (2 - 2 * LumaBlue) * Cb  + 0 * Cr
    **
    ** note: mtrx[3] == 1.0 when LR+LG+LB==1.0
    */
    mtrx[0] = 1.0;
    mtrx[1] = 0.0;
    mtrx[2] = 2.0 - 2.0 * LumaRed;

    mtrx[3] = (1.0 - LumaRed - LumaBlue) / LumaGreen;
    mtrx[4] = -2.0 * LumaBlue * (1.0 - LumaBlue) / LumaGreen;
    mtrx[5] = -2.0 * LumaRed  * (1.0 - LumaRed ) / LumaGreen;

    mtrx[6] = 1.0;
    mtrx[7] = 2.0 - 2.0 * LumaBlue;
    mtrx[8] = 0.0;

    mtrx[9] = mtrx[10] = mtrx[11] = 0.0;	/* bias */

}

static void
copylumaYCCfromRGB(pvt,LumaRed,LumaGreen,LumaBlue,Scale,oband)
    mpRGBPvtPtr pvt;
    double	LumaRed,LumaGreen,LumaBlue,Scale;
    bandPtr	oband;
{
    RGBFloat	*mtrx = pvt->matrix;

    if (LumaGreen < .01) LumaGreen = .01;

    /*
    **  See "A Planning Guide For Developers" from KODAK PhotoCD Products.
    **  Typical CCIR601.1 values of (.299, .587, .114) are used for
    **  (LumaRed,LumaGreen,LumaBlue).
    **
    **	Y  = (    LumaRed*R + LumaGreen * G +     LumaBlue * B)
    **	C1 = (   -LumaRed*R - LumaGreen * G + (1-LumaBlue) * B)
    **	C2 = ((1-LumaRed)*R - LumaGreen * G +    -LumaBlue * B)
    **
    **  From the same paper we are treated with the following equations
    **  for packing 8bit pixels:
    **
    **  PACK:                           UMPACK
    **	Y'  = 255/1.402 * Y             Y  = 1.3584 * Y'
    **	C1' = 111.40 * C1 + 156         C1 = 2.2179 * (C1' - 156)
    **	C2' = 135.64 * C2 + 137         C2 = 1.8215 * (C2' - 137)
    **
    **  Extrapolating a bit we can see that this is:
    **
    **  Y'  = 255/1.402 * Y     
    **  C1' = 255/2.289 * C1 + (.612 * 255)     <2.289 = 1.402 * 1.6327 >
    **  C2' = 255/1.879 * C2 + (.537 * 255) 	<1.879 = 1.402 * 1.3409 >
    **
    **  Y  = 1.3584 * Y'
    **  C1 = 1.3584 * 1.6327 * (C1' - (.612*255))
    **  C2 = 1.3584 * 1.3409 * (C2' - (.537*255))
    **
    **  So we are left with a choice between the magic 1.402 and 1.3584 
    **  values.  Hey, better idea. Its now an XIE protocol parameter
    **  called "scale".
    **
    **  In future versions, its possible we will want to accept the 1.3584
    **  number as a technique parameter in addition to the Luma.
    */
    mtrx[0] =  LumaRed;
    mtrx[1] =  LumaGreen;
    mtrx[2] =  LumaBlue;
    mtrx[3] = -LumaRed;
    mtrx[4] = -LumaGreen;
    mtrx[5] =  1.0 - LumaBlue;
    mtrx[6] =  1.0 - LumaRed;
    mtrx[7] = -LumaGreen;
    mtrx[8] = -LumaBlue;

    if (IsConstrained(oband->format->class)) {

	/* Scale each row of matrix by compression factors */
	scale_rows (mtrx, 1.0 / (Scale         ),
			  1.0 / (Scale * 1.6327),
			  1.0 / (Scale * 1.3409));

	/* Bias expressed as fraction of pixel range */
	mtrx[9]  = 0.0;	
	mtrx[10] = 0.612  * (RGBFloat) ((oband+1)->format->levels-1);
	mtrx[11] = 0.5373 * (RGBFloat) ((oband+2)->format->levels-1); 
    }
}

static void
copylumaYCCtoRGB(pvt,LumaRed,LumaGreen,LumaBlue,Scale,iband)
    mpRGBPvtPtr pvt;
    double	LumaRed,LumaGreen,LumaBlue,Scale;
    bandPtr	iband;
{
    RGBFloat	*mtrx = pvt->matrix;

    if (LumaGreen < .01) LumaGreen = .01;

    /*
    **  See "A Planning Guide For Developers" from KODAK PhotoCD Products.
    **  Typical values of (.299, .587, .114) are used for (LR,LG,LB).
    **
    **	R = 1 * Y +    0 * C1 +    1 * C2
    **  G = 1 * Y - .194 * C1 - .509 * C2
    **	B = 1 * Y +    1 * C1 +    0 * C2
    **
    **  By inverting the matrix symbolically we can deduce that the
    **  constants .194 and .509 are not arbitrary:
    **
    **	R = 1 * Y +     0 * C1 +     1 * C2
    **  G = 1 * Y - LB/LG * C1 - LR/LG * C2
    **	B = 1 * Y +     1 * C1 +     0 * C2
    **
    **  Note: mtrx[3] == 1.0 when LR+LG+LB==1.0
    */
    mtrx[0] = 1.0;
    mtrx[1] = 0.0;
    mtrx[2] = 1.0;

    mtrx[3] = (1.0 - LumaRed - LumaBlue) / LumaGreen;
    mtrx[4] = - LumaBlue/LumaGreen;
    mtrx[5] = - LumaRed/LumaGreen;

    mtrx[6] = 1.0;
    mtrx[7] = 1.0;
    mtrx[8] = 0.0;

    if (IsConstrained(iband->format->class)) {

	/* Scale each column of matrix by compression factors */
	scale_columns (mtrx, Scale, (Scale * 1.6327), (Scale * 1.3409));

	/* Bias expressed as fraction of pixel range */
	mtrx[9]  = 0.0;	
	mtrx[10] = 0.612  * (RGBFloat) ((iband+1)->format->levels-1);
	mtrx[11] = 0.5373 * (RGBFloat) ((iband+2)->format->levels-1); 
    }

}

/*------------------------------------------------------------------------
-------------------------- initialize element . . . ----------------------
------------------------------------------------------------------------*/

static int
SetupToRGB(flo,ped,modify)
    floDefPtr flo;
    peDefPtr  ped;
    int       modify;
{
    peTexPtr 	         pet = ped->peTex;
    mpRGBPvtPtr          pvt = (mpRGBPvtPtr) pet->private;
    pTecCIEToRGBDefPtr   pCIE = (pTecCIEToRGBDefPtr)   ped->techPvt;
    pTecYCCToRGBDefPtr   pYCC = (pTecYCCToRGBDefPtr)   ped->techPvt;
    pTecYCbCrToRGBDefPtr pYCb = (pTecYCbCrToRGBDefPtr) ped->techPvt;

    pvt->action = act_mmRR;
    pvt->post = 0;

    switch(ped->techVec->number) {
	case xieValCIELabToRGB:
		/* if whiteAdjusted, or calculate from inverted matrix */
		copymatrix(pvt, pCIE->matrix);
		copywhiteLABToRGB (pvt, pCIE->whiteAdjusted, pCIE->whitePoint);
		copygamut (pvt, pCIE->gamutCompress);
    		pvt->action = act_preCIELab; /* calls act_mmRR */
		break;
	case xieValCIEXYZToRGB:
		copymatrix(pvt, pCIE->matrix);
		copywhiteXYZToRGB (pvt, pCIE->whiteAdjusted, pCIE->whitePoint);
		copygamut (pvt, pCIE->gamutCompress);
		break;
	case xieValYCbCrToRGB:
		copylumaYCbCrtoRGB(pvt, pYCb->red, pYCb->green, pYCb->blue);
		copybiasYCbCr(pvt, pYCb->bias0, pYCb->bias1, pYCb->bias2);
		copygamut(pvt, pYCb->gamutCompress);
		break;
	case xieValYCCToRGB:
		copylumaYCCtoRGB(pvt, pYCC->red, pYCC->green, pYCC->blue, 
			pYCC->scale, &(pet->receptor[SRCtag].band[0]));
		copygamut (pvt, pYCC->gamutCompress);
		break;
    }

    CheckRGB(flo,ped,FALSE);

    return TRUE;
}

static int
SetupFromRGB(flo,ped,modify)
    floDefPtr flo;
    peDefPtr  ped;
    int       modify;
{
    peTexPtr 	         pet = ped->peTex;
    mpRGBPvtPtr          pvt = (mpRGBPvtPtr) pet->private;
    pTecRGBToCIEDefPtr   pCIE = (pTecRGBToCIEDefPtr)   ped->techPvt;
    pTecRGBToYCCDefPtr   pYCC = (pTecRGBToYCCDefPtr)   ped->techPvt;
    pTecRGBToYCbCrDefPtr pYCb = (pTecRGBToYCbCrDefPtr) ped->techPvt;

    pvt->action = act_mmRR;
    pvt->post = (void (*)()) 0;	

    switch(ped->techVec->number) {
	case xieValRGBToCIELab:
		copymatrix(pvt, pCIE->matrix);
		copywhiteLABFromRGB(pvt,  pCIE->whiteAdjusted,
					  pCIE->whitePoint);
		pvt->post = act_postCIELab;
		break;
	case xieValRGBToCIEXYZ:
		copymatrix(pvt, pCIE->matrix);
		copywhiteXYZFromRGB(pvt,  pCIE->whiteAdjusted,
					  pCIE->whitePoint);
		break;
	case xieValRGBToYCbCr:
		copylumaYCbCrfromRGB(pvt, pYCb->red, pYCb->green, pYCb->blue);
		copybiasYCbCr(pvt, pYCb->bias0, pYCb->bias1, pYCb->bias2);
		break;
	case xieValRGBToYCC:
		copylumaYCCfromRGB(pvt, pYCC->red, pYCC->green, pYCC->blue, 
    					pYCC->scale, &(pet->emitter[0]));
		break;
    }

    CheckRGB(flo,ped,TRUE);

    return TRUE;
}

static void
CheckRGB(flo,ped,fromrgb)
    floDefPtr flo;
    peDefPtr  ped;
    Bool     fromrgb;
{
    peTexPtr         pet = ped->peTex;
    bandPtr        iband = &(pet->receptor[SRCtag].band[0]);
    bandPtr        oband = &(pet->emitter[0]);
    CARD32        nbands = pet->receptor[SRCtag].inFlo->bands;
    mpRGBPvtPtr      pvt = (mpRGBPvtPtr) pet->private;

    CARD32 c, cmin, cmax, l, lmin, lmax;
    CARD32 band;

    pvt->cvt_in[0]  = pvt->cvt_in[1]  = pvt->cvt_in[2]  = (pointer (*)()) 0;
    pvt->cvt_out[0] = pvt->cvt_out[1] = pvt->cvt_out[2] = (pointer (*)()) 0;
    pvt->aux_buf[0] = pvt->aux_buf[1] = pvt->aux_buf[2] = (pointer) 0;

    if (IsntConstrained(iband->format->class)) {
	/* Already set up to activate floating routine */
	return;
    }

    cmin = PAIR_PIXEL;	cmax = BIT_PIXEL;
    lmin = (1<<24);	lmax = 1;

    for (band = 0; band < nbands; band++, iband++, oband++) {

	c = iband->format->class;
	if (c < cmin) cmin = c;
	if (c > cmax) cmax = c;

	l = iband->format->levels;
	if (l < lmin) lmin = l;
	if (l > lmax) lmax = l;

	if (IsntConstrained(oband->format->class)) 
	    continue;

	c = oband->format->class;
	if (c < cmin) cmin = c;
	if (c > cmax) cmax = c;

	l = oband->format->levels;
	if (l < lmin) lmin = l;
	if (l > lmax) lmax = l;

    }	iband -= 3; oband -= 3;


    /* Scale each column of matrix to scale from input levels to 0 ... 1 */
    scale_columns (pvt->matrix,
			(double) (1.0 / ((iband+0)->format->levels - 1)),
			(double) (1.0 / ((iband+1)->format->levels - 1)),
			(double) (1.0 / ((iband+2)->format->levels - 1)));

    if (IsntConstrained(oband->format->class)) {

	/* Only RGBtoCIEXYZ and RGBtoCIELab will Unconstrain the data */

	if (lmin > (1<<1) && lmax <= (1<<8)) {		/* ALL Bytes */
	    pvt->action = act_mmBR;
	    return;
	}

	pvt->action = act_mmPR;
	if (lmin > (1<<8)) 			 	/* ALL Pairs */
	    return;

	for (band = 0; band < nbands; band++, iband++) { /* SOME Pairs */
	    int levels = iband->format->levels; 
	    if (levels <= (1<<8)) {
		pvt->cvt_in[band] = levels < (1<<1)
			? cvt_bit_to_pair
			: cvt_byte_to_pair;
		pvt->aux_buf[band] = (pointer)
			XieMalloc( iband->format->width * sizeof(PairPixel));
		if (!pvt->aux_buf[band])
			AllocError(flo,ped,return);
	    }
	}	iband -= 3;
	return;
    }

    /* If we got to here, we will implicitly HardClip the outputs */
    pvt->post = (void (*)()) 0;	

    pvt->iclip[0] = (oband+0)->format->levels - 1;
    pvt->iclip[1] = (oband+1)->format->levels - 1;
    pvt->iclip[2] = (oband+2)->format->levels - 1;

    /* Scale each row of matrix to map output from 0...1 to 0...nlev-1 */
    scale_rows (pvt->matrix, (double) ((oband+0)->format->levels - 1),
			     (double) ((oband+1)->format->levels - 1),
			     (double) ((oband+2)->format->levels - 1));

    /* Adjust bias for integer YCbCr and YCC */
    if (!fromrgb) switch (ped->techVec->number) {
	case xieValRGBToYCC:	/* fall thru */
	case xieValRGBToYCbCr:	flip_bias(pvt->matrix);
				break;
	default:		break;
    }

    if (lmin > (1<<1) && lmax <= (1<<8)) {		/* ALL Bytes */
	pvt->action = act_mmBB;
	scale_mtrx(pvt->matrix,pvt->imatrix,(1<<SF_BYTESHIFT));
	return;
    }

    /*
    ** If they are not all bytes, we will process as pairs.  But we may
    ** need to convert some sources to pair pixels, and some outputs
    ** may need to be converted back to bits or bytes.
    */

    pvt->action = act_mmPP;
    scale_mtrx(pvt->matrix,pvt->imatrix,(1<<SF_PAIRSHIFT));

    if (lmin > (1<<8))					/* ALL Pairs */
	return;

    for (band = 0; band < nbands; band++, iband++, oband++) {
	int levels;
	levels = iband->format->levels; 
	pvt->cvt_in[band]  = levels <= (1<<1) ? cvt_bit_to_pair :
			     levels <= (1<<8) ? cvt_byte_to_pair :
					        (pointer (*)()) 0;
	levels = oband->format->levels; 
	pvt->cvt_out[band] = levels <= (1<<1) ? cvt_pair_to_bit :
			     levels <= (1<<8) ? cvt_pair_to_byte :
					        (pointer (*)()) 0;
	if ( pvt->cvt_in[band] || pvt->cvt_out[band]) {
	    pvt->aux_buf[band] = (pointer)
		XieMalloc( iband->format->width * sizeof(PairPixel));
	    if (!pvt->aux_buf[band])
		AllocError(flo,ped,return);
	}
    }	iband -= 3; oband -= 3;
    return;
}

static void
ClearRGB(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    mpRGBPvtPtr   pvt = (mpRGBPvtPtr) ped->peTex->private;

    if (!pvt)
	return;

    pvt->cvt_in[0]  = pvt->cvt_in[1]  = pvt->cvt_in[2]  = (pointer (*)()) 0;
    pvt->cvt_out[0] = pvt->cvt_out[1] = pvt->cvt_out[2] = (pointer (*)()) 0;

    if (pvt->aux_buf[0]) pvt->aux_buf[0] = (pointer) XieFree(pvt->aux_buf[0]);
    if (pvt->aux_buf[1]) pvt->aux_buf[1] = (pointer) XieFree(pvt->aux_buf[1]);
    if (pvt->aux_buf[2]) pvt->aux_buf[2] = (pointer) XieFree(pvt->aux_buf[2]);
}
	
/* end module mprgb.c */
