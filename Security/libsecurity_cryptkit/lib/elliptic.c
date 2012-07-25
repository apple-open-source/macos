/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE COMPUTER, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE COMPUTER,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************

   elliptic.c - Library for Apple-proprietary Fast Elliptic
   Encryption. The algebra in this module ignores elliptic point's
   y-coordinates.

   Patent information:

   FEE is patented, U.S. Patents #5159632 (1992), #5271061 (1993),
	#5463690 (1994).  These patents are implemented
        in various elliptic algebra functions such as
        numer/denom_plus/times(), and in the fact of special
        forms for primes: p = 2^q-k.

   Digital signature using fast elliptic addition, in
        U. S. Patent #5581616 (1996), is implemented in the
        signature_compare() function.

   FEED (Fast Elliptic Encryption) is patent pending (as of Jan 1998).
   	Various functions such as elliptic_add() implement the patent ideas.


   Modification history since the U.S. Patent:
   -------------------------------------------
	10/06/98		ap
  		Changed to compile with C++.
    9 Sep 98	Doug Mitchell at Apple
    	cp->curveType optimizations.
	Removed code which handled "unknown" curve orders.
	elliptic() now exported for timing measurements.
   21 Apr 98	Doug Mitchell at Apple
   	Used inline platform-dependent giant arithmetic.
   20 Jan 98	Doug Mitchell at Apple
   	feemod now handles PT_MERSENNE, PT_FEE, PT_GENERAL.
	Added calcGiantSizes(), rewrote giantMinBytes(), giantMaxShorts().
	Updated heading comments on FEE curve algebra.
   11 Jan 98	Doug Mitchell and Richard Crandall at Apple
   	Microscopic feemod optimization.
   10 Jan 98	Doug Mitchell and Richard Crandall at Apple
 	ell_odd, ell_even() Montgomery optimization.
   09 Jan 98	Doug Mitchell and Richard Crandall at Apple
 	ell_odd, ell_even() Atkin3 optimization.
   08 Jan 97	Doug Mitchell at Apple
   	Cleaned up some debugging code; added gsquareTime
   11 Jun 97	Doug Mitchell and Richard Crandall at Apple
 	Mods for modg_via_recip(), divg_via_recip() math
	Deleted a lot of obsolete code (previously ifdef'd out)
	Added lesserX1OrderJustify(), x1OrderPlusJustify()
	Added binvg_cp(), avoiding general modg in favor of feemod
   05 Feb 97	Doug Mitchell and Richard Crandall at Apple
   	New optimized numer_plus(), denom_double(), and numer_times()
	All calls to borrowGiant() and newGiant have explicit giant size
   08 Jan 97	Doug Mitchell and Richard Crandall at NeXT
   	Major mods to accomodate IEEE-style curve parameters.
	New functions feepowermodg() and curveOrderJustify();
	elliptic_add(), elliptic(), signature_compare(), and
	which_curve() completely rewritten.
   19 Dec 96	Doug Mitchell at NeXT
   	Added mersennePrimes[24..26]
   08 Aug 96	Doug Mitchell at NeXT
   	Fixed giant leak in elliptic_add()
   05 Aug 96	Doug Mitchell at NeXT
   	Removed dead code
   24 Jul 96	Doug Mitchell at NeXT
 	Added ENGINE_127_BITS dependency for use of security engine
   24 Oct 92	Richard Crandall at NeXT
   	Modified new_public_from_private()
        1991 	Richard Crandall at NeXT
	Created.


   FEE curve algebra, Jan 1997.

   Curves are:

      y^2 = x^3 + c x^2 + a x + b

   where useful parameterizations for practical purposes are:

      Montgomery: a = 1, b = 0. (The original 1991 FEE system.)
      Weierstrass: c = 0.  (The basic IEEE form.)
      Atkin3: c = a = 0.
      Atkin4: c = b = 0.

   However, the code is set up to work with any a, b, c.
   The underlying fields F_{p^m} are of odd characteristic,
   with all operations are (mod p).  The special FEE-class
   primes p are of the form:

      p = 2^q - k = 3 (mod 4)

   where k is single-precision.  For such p, the mod operations
   are especially fast (asymptotically vanishing time with respect
   to a multiply).  Note that the whole system
   works equally well (except for slower execution) for arbitrary
   primes p = 3 (mod 4) of the same bit length (q or q+1).

   The elliptic arithmetic now proceeds on the basis of
   five fundamental operations that calculate various
   numerator/denominator parts of the elliptic terms:

   numer_double(giant x, giant z, giant res, curveParams *par)
   res := (x^2 - a z^2)^2 - 4 b (2 x + c z) z^3.

   numer_plus(giant x1, giant x2, giant res, curveParams *par)
   res = (x1 x2 + a)(x1 + x2) + 2(c x1 x2 + b).

   denom_double(giant x, giant z, giant res, curveParams *par)
   res = 4 z (x^3 + c x^2 z + a x z^2 + b z^3).

   denom_times(giant x1, giant z1, giant x2, giant z2, giant res,
     curveParams *par)
   res := (x1 z2 - x2 z1)^2

   numer_times(giant x1, giant z1, giant x2, giant z2, giant res,
      curveParams *par)
   res := (x1 x2 - a z1 z2)^2 - 4 b(x1 z2 + x2 z1 + c z1 z2) z1 z2

   If x(+-) represent the sum and difference x-coordinates
   respectively, then, in pseudocode,

   For unequal starting coords:
    x(+) + x(-) = U = 2 numer_plus/denom_times
     x(+) x(-)  = V = numer_times/denom_times

   and for equal starting coords:
     x(+) = numer_double/denom_double

   The elliptic_add() function uses the fact that

     x(+) = U/2 + s*Sqrt[U^2/4 - V]

   where the sign s = +-1.

*/

#include "ckconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"

#include "giantIntegers.h"
#include "elliptic.h"
#include "ellipticProj.h"
#include "ckutilities.h"
#include "curveParams.h"
#include "feeDebug.h"
#include "ellipticMeasure.h"
#include "falloc.h"
#include "giantPortCommon.h"

#if	FEE_PROFILE

unsigned numerDoubleTime;
unsigned numerPlusTime;
unsigned numerTimesTime;
unsigned denomDoubleTime;
unsigned denomTimesTime;
unsigned ellipticTime;
unsigned sigCompTime;
unsigned powerModTime;
unsigned ellAddTime;
unsigned whichCurveTime;
unsigned modgTime;
unsigned mulgTime;
unsigned binvauxTime;
unsigned gsquareTime;

unsigned numMulg;
unsigned numFeemod;
unsigned numGsquare;
unsigned numBorrows;

void clearProfile()
{
	numerDoubleTime = 0;
	numerPlusTime = 0;
	numerTimesTime = 0;
	denomDoubleTime = 0;
	denomTimesTime = 0;
	ellipticTime = 0;
	sigCompTime = 0;
	powerModTime = 0;
	ellAddTime = 0;
	whichCurveTime = 0;
	modgTime = 0;
	mulgTime = 0;
	binvauxTime = 0;
	gsquareTime = 0;
	numMulg = 0;
	numFeemod = 0;
	numGsquare = 0;
	numBorrows = 0;
}

#endif	// FEE_PROFILE

#if	ELL_PROFILE
unsigned ellOddTime;
unsigned ellEvenTime;
unsigned numEllOdds;
unsigned numEllEvens;

void clearEllProfile()
{
	ellOddTime = 0;
	ellEvenTime = 0;
	numEllOdds = 0;
	numEllEvens = 0;
}

#endif	/* ELL_PROFILE */
#if	ELLIPTIC_MEASURE

int doEllMeasure;	// gather stats on/off */
int bitsInN;
int numFeeMods;
int numMulgs;

void dumpEll()
{
	printf("\nbitlen(n) : %d\n", bitsInN);
	printf("feemods   : %d\n", numFeeMods);
	printf("mulgs     : %d\n", numMulgs);
}

#endif	// ELLIPTIC_MEASURE

/********** Globals ********************************/

static void make_base(curveParams *par, giant result); // result = with 2^q-k
static int keys_inconsistent(key pub1, key pub2);
/* Return non-zero if pub1, pub2 have inconsistent parameters.
 */


static void ell_even(giant x1, giant z1, giant x2, giant z2, curveParams *par);
static void ell_odd(giant x1, giant z1, giant x2, giant z2, giant xxor,
	giant zor, curveParams *par);
static void numer_double(giant x, giant z, giant res, curveParams *par);
static void numer_plus(giant x1, giant x2, giant res, curveParams *par);
static void denom_double(giant x, giant z, giant res, curveParams *par);
static void denom_times(giant x1, giant z1, giant x2, giant z2, giant res,
	curveParams *par);
static void numer_times(giant x1, giant z1, giant x2, giant z2, giant res,
	curveParams *par);
static void feepowermodg(curveParams *par, giant x, giant n);
static void curveOrderJustifyWithRecip(giant g, giant curveOrder, giant recip);

/*
 * Completely rewritten in CryptKit-18, 13 Jan 1997, for new IEEE-style
 * curveParameters.
 */
int which_curve(giant x, curveParams *par)
 /* Returns (+-1) depending on whether x is on curve
   (+-)y^2 = x^3 + c x^2 + a x + b.
 */
{
    giant t1;
    giant t2;
    giant t3;
    int result;
    PROF_START;

    t1 = borrowGiant(par->maxDigits);
    t2 = borrowGiant(par->maxDigits);
    t3 = borrowGiant(par->maxDigits);

   /* First, set t2:= x^3 + c x^2 + a x + b. */
    gtog(x, t2); addg(par->c, t2);
    mulg(x, t2); addg(par->a, t2);  /* t2 := x^2 + c x + a. */
    feemod(par, t2);
    mulg(x, t2); addg(par->b, t2);
    feemod(par, t2);
    /* Next, test whether t2 is a square. */
    gtog(t2, t1);
    make_base(par, t3); iaddg(1, t3); gshiftright(1, t3); /* t3 = (p+1)/2. */
    feepowermodg(par, t1, t3); 		      /* t1 := t2^((p+1)/2) (mod p). */
    if(gcompg(t1, t2) == 0)
            result = CURVE_PLUS;
    else
            result = CURVE_MINUS;
    returnGiant(t1);
    returnGiant(t2);
    returnGiant(t3);
    PROF_END(whichCurveTime);
    return result;
}

key new_public(curveParams *cp, int twist) {
    key k;

    k = (key) fmalloc(sizeof(keystruct));
    k->cp = cp;
    k->twist = twist;

    k->x = newGiant(cp->maxDigits);
    if((twist == CURVE_PLUS) && (cp->curveType == FCT_Weierstrass)) {
		k->y = newGiant(cp->maxDigits);
    }
    else {
    	/*
		 * no projective algebra. We could optimize and save a few bytes
		 * here by setting y to NULL, but that really complicates things
		 * in may other places. Best to have a real giant.
		 */
		k->y = newGiant(1);
    }
    return(k);
}

key new_public_with_key(key old_key, curveParams *cp)
{
	key result;

	result = new_public(cp, old_key->twist);
	CKASSERT((old_key->x != NULL) && (old_key->y != NULL));
	CKASSERT((result->x != NULL) && (result->y != NULL));
	gtog(old_key->x, result->x);
	gtog(old_key->y, result->y);
	return result;
}

void free_key(key pub) {
	if(!pub) {
		return;
	}
	if (pub->x) {
		freeGiant(pub->x);
	}
	if (pub->y) {
		freeGiant(pub->y);
	}
	ffree(pub);
}

/*
 * Specify private data for key created by new_public().
 * Generates k->x.
 */
void set_priv_key_giant(key k, giant privGiant)
{
	curveParams *cp = k->cp;

	/* elliptiy multiply of initial public point times private key */
	#if CRYPTKIT_ELL_PROJ_ENABLE
	if((k->twist == CURVE_PLUS) && (cp->curveType == FCT_Weierstrass)) {
		/* projective */

		pointProj pt1 = newPointProj(cp->maxDigits);

		CKASSERT((cp->y1Plus != NULL) && (!isZero(cp->y1Plus)));
		CKASSERT(k->y != NULL);

		/* pt1 := {x1Plus, y1Plus, 1} */
		gtog(cp->x1Plus, pt1->x);
		gtog(cp->y1Plus, pt1->y);
		int_to_giant(1, pt1->z);

		/* pt1 := pt1 * privateKey */
		ellMulProjSimple(pt1, privGiant, cp);

		/* result back to {k->x, k->y} */
		gtog(pt1->x, k->x);
		gtog(pt1->y, k->y);
		freePointProj(pt1);	// FIXME - clear the giants
	}
	else {
	#else
	{
	#endif	/* CRYPTKIT_ELL_PROJ_ENABLE */
		/* FEE */
		if(k->twist == CURVE_PLUS) {
			gtog(cp->x1Plus, k->x);
		}
		else {
			gtog(cp->x1Minus, k->x);
		}
		elliptic_simple(k->x, privGiant, k->cp);
	}
}

int key_equal(key one, key two) {
    if (keys_inconsistent(one, two)) return 0;
    return !gcompg(one->x, two->x);
}

static void make_base(curveParams *par, giant result)
/* Jams result with 2^q-k. */
{
    gtog(par->basePrime, result);
}

void make_base_prim(curveParams *cp)
/* Jams cp->basePrime with 2^q-k. Assumes valid maxDigits, q, k. */
{
    giant tmp = borrowGiant(cp->maxDigits);

    CKASSERT(cp->primeType != FPT_General);
    int_to_giant(1, cp->basePrime);
    gshiftleft((int)cp->q, cp->basePrime);
    int_to_giant(cp->k, tmp);
    subg(tmp, cp->basePrime);
    returnGiant(tmp);
}

static int sequalg(int n, giant g) {
	if((g->sign == 1) && (g->n[0] == n)) return(1);
	return(0);
}


/*
 * Elliptic multiply: x := n * {x, 1}
 */
void elliptic_simple(giant x, giant n, curveParams *par) {
    giant ztmp = borrowGiant(par->maxDigits);
    giant cur_n = borrowGiant(par->maxDigits);

    START_ELL_MEASURE(n);
    int_to_giant(1, ztmp);
    elliptic(x, ztmp, n, par);
    binvg_cp(par, ztmp);
    mulg(ztmp, x);
    feemod(par, x);
    END_ELL_MEASURE;

    returnGiant(cur_n);
    returnGiant(ztmp);
}

/*
 * General elliptic multiply.
 *
 * {xx, zz} := k * {xx, zz}
 */
void elliptic(giant xx, giant zz, giant k, curveParams *par) {
	int len = bitlen(k);
        int pos = len - 2;
        giant xs;
        giant zs;
        giant xorg;
        giant zorg;

	PROF_START;
	if(sequalg(1,k)) return;
	if(sequalg(2,k)) {
		ell_even(xx, zz, xx, zz, par);
		goto out;
	}
        zs = borrowGiant(par->maxDigits);
        xs = borrowGiant(par->maxDigits);
        zorg = borrowGiant(par->maxDigits);
        xorg = borrowGiant(par->maxDigits);
	gtog(xx, xorg); gtog(zz, zorg);
	ell_even(xx, zz, xs, zs, par);
	do {
	   if(bitval(k, pos--)) {
	   	ell_odd(xs, zs, xx, zz, xorg, zorg, par);
		ell_even(xs, zs, xs, zs, par);
	   } else {
	   	ell_odd(xx, zz, xs, zs, xorg, zorg, par);
		ell_even(xx, zz, xx, zz, par);
	   }
        } while (pos >= 0);	// REC fix 9/23/94
        returnGiant(xs);
        returnGiant(zs);
        returnGiant(xorg);
        returnGiant(zorg);
out:
	PROF_END(ellipticTime);
}

/*
 * Completely rewritten in CryptKit-18, 13 Jan 1997, for new IEEE-style
 * curveParameters.
 */
void elliptic_add(giant x1, giant x2, giant x3, curveParams *par, int s) {

 /* Addition algorithm for x3 = x1 + x2 on the curve, with sign ambiguity s.
    From theory, we know that if {x1,1} and {x2,1} are on a curve, then
    their elliptic sum (x1,1} + {x2,1} = {x3,1} must have x3 as one of two
    values:

       x3 = U/2 + s*Sqrt[U^2/4 - V]

    where sign s = +-1, and U,V are functions of x1,x2.  Tho present function
    is called a maximum of twice, to settle which of +- is s.  When a call
    is made, it is guaranteed already that x1, x2 both lie on the same curve
    (+- curve); i.e., which curve (+-) is not connected at all with sign s of
    the x3 relation.
  */

    giant cur_n;
    giant t1;
    giant t2;
    giant t3;
    giant t4;
    giant t5;

    PROF_START;
    cur_n = borrowGiant(par->maxDigits);
    t1 = borrowGiant(par->maxDigits);
    t2 = borrowGiant(par->maxDigits);
    t3 = borrowGiant(par->maxDigits);
    t4 = borrowGiant(par->maxDigits);
    t5 = borrowGiant(par->maxDigits);

    if(gcompg(x1, x2)==0) {
	int_to_giant(1, t1);
	numer_double(x1, t1, x3, par);
	denom_double(x1, t1, t2, par);
	binvg_cp(par, t2);
	mulg(t2, x3); feemod(par, x3);
	goto out;
    }
    numer_plus(x1, x2, t1, par);
    int_to_giant(1, t3);
    numer_times(x1, t3, x2, t3, t2, par);
    int_to_giant(1, t4); int_to_giant(1, t5);
    denom_times(x1, t4, x2, t5, t3, par);
    binvg_cp(par, t3);
    mulg(t3, t1); feemod(par, t1); /* t1 := U/2. */
    mulg(t3, t2); feemod(par, t2); /* t2 := V. */
    /* Now x3 will be t1 +- Sqrt[t1^2 - t2]. */
    gtog(t1, t4); gsquare(t4); feemod(par, t4);
    subg(t2, t4);
    make_base(par, cur_n); iaddg(1, cur_n); gshiftright(2, cur_n);
    	/* cur_n := (p+1)/4. */
    feepowermodg(par, t4, cur_n);      /* t4 := t2^((p+1)/4) (mod p). */
    gtog(t1, x3);
    if(s != SIGN_PLUS) negg(t4);
    addg(t4, x3);
    feemod(par, x3);

out:
    returnGiant(cur_n);
    returnGiant(t1);
    returnGiant(t2);
    returnGiant(t3);
    returnGiant(t4);
    returnGiant(t5);

    PROF_END(ellAddTime);
}

/*
 * Key exchange atom.
 */
giant make_pad(giant privGiant, key publicKey) {
    curveParams *par = publicKey->cp;
    giant result = newGiant(par->maxDigits);

    gtog(publicKey->x, result);
    elliptic_simple(result, privGiant, par);
    return result;
}

static void ell_even(giant x1, giant z1, giant x2, giant z2, curveParams *par) {
    giant t1;
    giant t2;
    giant t3;

    EPROF_START;

    t1 = borrowGiant(par->maxDigits);
    t2 = borrowGiant(par->maxDigits);
    t3 = borrowGiant(par->maxDigits);

    if(par->curveType == FCT_Montgomery) {
	/* Begin Montgomery OPT: 10 Jan 98 REC. */
	gtog(x1, t1); gsquare(t1); feemod(par, t1); /* t1 := x1^2. */
	gtog(z1, t2); gsquare(t2); feemod(par, t2); /* t2 := z1^2. */

	gtog(x1, t3); mulg(z1, t3); feemod(par, t3);
	gtog(t3, z2); mulg(par->c, z2); feemod(par, z2);
	addg(t1, z2); addg(t2, z2); mulg(t3, z2); gshiftleft(2, z2);
	feemod(par, z2);  /* z2 := 4 x1 z1 (x1^2 + c x1 z1 + z1^2). */
	gtog(t1, x2); subg(t2, x2); gsquare(x2); feemod(par, x2);
						/* x2 := (x1^2 - z1^2)^2. */
	/* End OPT: 10 Jan 98 REC. */
    }
    else if((par->curveType == FCT_Weierstrass) && isZero(par->a)) {
	/* Begin Atkin3 OPT: 9 Jan 98 REC. */
	gtog(x1, t1);
	gsquare(t1); feemod(par, t1);
	mulg(x1, t1); feemod(par, t1);   	/* t1 := x^3. */
	gtog(z1, t2);
	gsquare(t2); feemod(par, t2);
	mulg(z1, t2); feemod(par, t2);	/* t2 := z1^3 */
	mulg(par->b, t2); feemod(par, t2); 	/* t2 := b z1^3. */
	gtog(t1, t3); addg(t2, t3);		/* t3 := x^3 + b z1^3 */
	mulg(z1, t3); feemod(par, t3);	/* t3 *= z1
						*     = z1 ( x^3 + b z1^3 ) */
	gshiftleft(2, t3); feemod(par, t3);	/* t3 = 4 z1 (x1^3 + b z1^3) */

	gshiftleft(3, t2);			/* t2 = 8 b z1^3 */
	subg(t2, t1);			/* t1 = x^3 - 8 b z1^3 */
	mulg(x1, t1); feemod(par, t1);	/* t1 = x1 (x1^3 - 8 b z1^3) */

	gtog(t3, z2);
	gtog(t1, x2);
	/* End OPT: 9 Jan 98 REC. */
    }
    else {
	numer_double(x1, z1, t1, par);
	denom_double(x1, z1, t2, par);
	gtog(t1, x2); gtog(t2, z2);
    }
    returnGiant(t1);
    returnGiant(t2);
    returnGiant(t3);

    EPROF_END(ellEvenTime);
    EPROF_INCR(numEllEvens);

    /*
    printf("ell_even end\n");
    printf(" x1 : "); printGiant(x1);
    printf(" z1 : "); printGiant(z1);
    printf(" x2 : "); printGiant(x2);
    printf(" z2 : "); printGiant(z2);
    */
}

static void ell_odd(giant x1, giant z1, giant x2, giant z2, giant xxor,
	giant zor, curveParams *par)
{

    giant t1;
    giant t2;

    EPROF_START;
    t1 = borrowGiant(par->maxDigits);
    t2 = borrowGiant(par->maxDigits);

    if(par->curveType == FCT_Montgomery) {
	/* Begin Montgomery OPT: 10 Jan 98 REC. */
	giant t3 = borrowGiant(par->maxDigits);
	giant t4 = borrowGiant(par->maxDigits);

	gtog(x1, t1); addg(z1, t1);  	/* t1 := x1 + z1. */
	gtog(x2, t2); subg(z2, t2);  	/* t2 := x2 - z2. */
	gtog(x1, t3); subg(z1, t3);  	/* t3 := x1 - z1. */
	gtog(x2, t4); addg(z2, t4);  	/* t4 := x2 + z2. */
	mulg(t2, t1); feemod(par, t1);      /* t1 := (x1 + z1)(x2 - z2) */
	mulg(t4, t3); feemod(par, t3);      /* t4 := (x2 + z2)(x1 - z1) */
	gtog(t1, z2); subg(t3, z2); 	/*???gshiftright(1, z2);*/
			    /* z2 := ((x1 + z1)(x2 - z2) - x2)/2 */
	gsquare(z2); feemod(par,  z2);
	mulg(xxor, z2); feemod(par, z2);
	gtog(t1, x2); addg(t3, x2); 	/*????gshiftright(1, x2);*/
	gsquare(x2); feemod(par, x2);
	mulg(zor, x2); feemod(par, x2);

	returnGiant(t3);
	returnGiant(t4);
    }
    else if((par->curveType == FCT_Weierstrass) && isZero(par->a)) {
	/* Begin Atkin3 OPT: 9 Jan 98 REC. */

	giant t3 = borrowGiant(par->maxDigits);
	giant t4 = borrowGiant(par->maxDigits);

	gtog(x1, t1); mulg(x2, t1);  feemod(par, t1);  /* t1 := x1 x2. */
	gtog(z1, t2); mulg(z2, t2);  feemod(par, t2);  /* t2 := z1 z2. */
	gtog(x1, t3); mulg(z2, t3);  feemod(par, t3);  /* t3 := x1 z2. */
	gtog(z1, t4); mulg(x2, t4);  feemod(par, t4);  /* t4 := x2 z1. */
	gtog(t3, z2); subg(t4, z2); gsquare(z2); feemod(par, z2);
	mulg(xxor, z2); feemod(par, z2);
	gtog(t1, x2); gsquare(x2); feemod(par, x2);
	addg(t4, t3); mulg(t2, t3); feemod(par, t3);
	mulg(par->b, t3); feemod(par, t3);
	addg(t3, t3); addg(t3, t3);
	subg(t3, x2); mulg(zor, x2); feemod(par, x2);

	returnGiant(t3);
	returnGiant(t4);
	/* End OPT: 9 Jan 98 REC. */
    }
    else {
	numer_times(x1, z1, x2, z2, t1, par);
	mulg(zor, t1); feemod(par, t1);
	denom_times(x1, z1, x2, z2, t2, par);
	mulg(xxor, t2); feemod(par, t2);

	gtog(t1, x2); gtog(t2, z2);
    }

    returnGiant(t1);
    returnGiant(t2);

    EPROF_END(ellOddTime);
    EPROF_INCR(numEllOdds);

    /*
    printf("ell_odd end\n");
    printf(" x2 : "); printGiant(x2);
    printf(" z2 : "); printGiant(z2);
    */
}

/*
 * return codes from keys_inconsistent() and signature_compare(). The actual
 * values are not public; they are defined here for debugging.
 */
#define CURVE_PARAM_MISMATCH	1
#define TWIST_PARAM_MISMATCH 	2
#define SIGNATURE_INVALID 	3


/*
 * Determine whether two keystructs have compatible parameters (i.e., same
 * twist and curveParams). Return 0 if compatible, else non-zero.
 */
static int keys_inconsistent(key pub1, key pub2){
	if(!curveParamsEquivalent(pub1->cp, pub2->cp)) {
		return CURVE_PARAM_MISMATCH;
	}
	if(pub1->twist != pub2->twist) {
		return TWIST_PARAM_MISMATCH;
	}
	return 0;
}

int signature_compare(giant p0x, giant p1x, giant p2x, curveParams *par)
/* Returns non-zero iff p0x cannot be the x-coordinate of the sum of two points whose respective x-coordinates are p1x, p2x. */
{
        int ret = 0;
        giant t1;
	giant t2;
        giant t3;
        giant t4;
        giant t5;

	PROF_START;

        t1 = borrowGiant(par->maxDigits);
	t2 = borrowGiant(par->maxDigits);
        t3 = borrowGiant(par->maxDigits);
        t4 = borrowGiant(par->maxDigits);
        t5 = borrowGiant(par->maxDigits);

        if(gcompg(p1x, p2x) == 0) {
		int_to_giant(1, t1);
		numer_double(p1x, t1, t2, par);
		denom_double(p1x, t1, t3, par);
		mulg(p0x, t3); subg(t3, t2);
		feemod(par, t2);
        } else {
		numer_plus(p1x, p2x, t1, par);
		gshiftleft(1, t1); feemod(par, t1);
		int_to_giant(1, t3);
		numer_times(p1x, t3, p2x, t3, t2, par);
		int_to_giant(1, t4); int_to_giant(1, t5);
		denom_times(p1x, t4 , p2x, t5, t3, par);
		/* Now we require t3 x0^2 - t1 x0 + t2 == 0. */
		mulg(p0x, t3); feemod(par, t3);
		subg(t1, t3); mulg(p0x, t3);
		feemod(par, t3);
		addg(t3, t2);
		feemod(par, t2);
        }

	if(!isZero(t2)) ret = SIGNATURE_INVALID;
        returnGiant(t1);
        returnGiant(t2);
        returnGiant(t3);
        returnGiant(t4);
        returnGiant(t5);
	PROF_END(sigCompTime);
	return(ret);
}


static void numer_double(giant x, giant z, giant res, curveParams *par)
/* Numerator algebra.
   res := (x^2 - a z^2)^2 - 4 b (2 x + c z) z^3.
 */
{
    giant t1;
    giant t2;

    PROF_START;
    t1 = borrowGiant(par->maxDigits);
    t2 = borrowGiant(par->maxDigits);

    gtog(x, t1); gsquare(t1); feemod(par, t1);
    gtog(z, res); gsquare(res); feemod(par, res);
    gtog(res, t2);
    if(!isZero(par->a) ) {
        if(!isone(par->a)) { /* Speedup - REC 17 Jan 1997. */
	    mulg(par->a, res); feemod(par, res);
        }
        subg(res, t1); feemod(par, t1);
    }
    gsquare(t1); feemod(par, t1);
    /* t1 := (x^2 - a z^2)^2. */
    if(isZero(par->b))  {   /* Speedup - REC 17 Jan 1997. */
	gtog(t1, res);
        goto done;
    }
    if(par->curveType != FCT_Weierstrass) {	// i.e., !isZero(par->c)
    						// Speedup - REC 17 Jan 1997.
	gtog(z, res); mulg(par->c, res); feemod(par, res);
    } else {
        int_to_giant(0, res);
    }
    addg(x, res); addg(x, res); mulg(par->b, res);
    feemod(par, res);
    gshiftleft(2, res); mulg(z, res); feemod(par, res);
    mulg(t2, res); feemod(par, res);
    negg(res); addg(t1, res);
    feemod(par, res);

done:
    returnGiant(t1);
    returnGiant(t2);
    PROF_END(numerDoubleTime);
}

static void numer_plus(giant x1, giant x2, giant res, curveParams *par)
/* Numerator algebra.
   res = (x1 x2 + a)(x1 + x2) + 2(c x1 x2 + b).
 */
{
    giant t1;
    giant t2;

    PROF_START;
    t1 = borrowGiant(par->maxDigits);
    t2 = borrowGiant(par->maxDigits);

    gtog(x1, t1); mulg(x2, t1); feemod(par, t1);
    gtog(x2, t2); addg(x1, t2); feemod(par, t2);
    gtog(t1, res);
    if(!isZero(par->a))
    	addg(par->a, res);
    mulg(t2, res); feemod(par, res);
    if(par->curveType == FCT_Weierstrass) {	// i.e., isZero(par->c)
    	int_to_giant(0, t1);
    }
    else {
        mulg(par->c, t1); feemod(par, t1);
    }
    if(!isZero(par->b))
    	addg(par->b, t1);
    gshiftleft(1, t1);
    addg(t1, res); feemod(par, res);

    returnGiant(t1);
    returnGiant(t2);
    PROF_END(numerPlusTime);
}

static void denom_double(giant x, giant z, giant res, curveParams *par)
/* Denominator algebra.
    res = 4 z (x^3 + c x^2 z + a x z^2 + b z^3). */
{
    giant t1;
    giant t2;

    PROF_START;
    t1 = borrowGiant(par->maxDigits);
    t2 = borrowGiant(par->maxDigits);

    gtog(x, res); gtog(z, t1);
    if(par->curveType != FCT_Weierstrass) {	// i.e., !isZero(par->c)
	gtog(par->c, t2); mulg(t1, t2); feemod(par, t2);
	addg(t2, res);
    }
    mulg(x, res); feemod(par, res);
    gsquare(t1); feemod(par, t1);
    if(!isZero(par->a)) {
	gtog(t1, t2);
    	mulg(par->a, t2); feemod(par, t2);
    	addg(t2, res);
    }
    mulg(x, res); feemod(par, res);
    if(!isZero(par->b)) {
	mulg(z, t1); feemod(par, t1);
    	mulg(par->b, t1); feemod(par, t1);
    	addg(t1, res);
    }
    mulg(z, res); gshiftleft(2, res);
    feemod(par, res);

    returnGiant(t1);
    returnGiant(t2);
    PROF_END(denomDoubleTime);
}



static void denom_times(giant x1, giant z1, giant x2, giant z2, giant res,
	curveParams *par)
/* Denominator algebra.
    res := (x1 z2 - x2 z1)^2
 */
{
    giant t1;

    PROF_START;
    t1 = borrowGiant(par->maxDigits);

    gtog(x1, res); mulg(z2, res); feemod(par, res);
    gtog(z1, t1); mulg(x2, t1); feemod(par, t1);
    subg(t1, res); gsquare(res); feemod(par, res);

    returnGiant(t1);
    PROF_END(denomTimesTime);
}

static void numer_times(giant x1, giant z1, giant x2, giant z2, giant res,
	curveParams *par)
/* Numerator algebra.
    res := (x1 x2 - a z1 z2)^2 -
  	          4 b(x1 z2 + x2 z1 + c z1 z2) z1 z2
 */
{
    giant t1;
    giant t2;
    giant t3;
    giant t4;

    PROF_START;
    t1 = borrowGiant(par->maxDigits);
    t2 = borrowGiant(par->maxDigits);
    t3 = borrowGiant(par->maxDigits);
    t4 = borrowGiant(par->maxDigits);

    gtog(x1, t1); mulg(x2, t1); feemod(par, t1);
    gtog(z1, t2); mulg(z2, t2); feemod(par, t2);
    gtog(t1, res);
    if(!isZero(par->a)) {
	gtog(par->a, t3);
      	mulg(t2, t3); feemod(par, t3);
      	subg(t3, res);
    }
    gsquare(res); feemod(par, res);
    if(isZero(par->b))
        goto done;
    if(par->curveType != FCT_Weierstrass) {	// i.e., !isZero(par->c)
        gtog(par->c, t3);
    	mulg(t2, t3); feemod(par, t3);
    } else int_to_giant(0, t3);
    gtog(z1, t4); mulg(x2, t4); feemod(par, t4);
    addg(t4, t3);
    gtog(x1, t4); mulg(z2, t4); feemod(par, t4);
    addg(t4, t3); mulg(par->b, t3); feemod(par, t3);
    mulg(t2, t3); gshiftleft(2, t3); feemod(par, t3);
    subg(t3, res);
    feemod(par, res);

done:
    returnGiant(t1);
    returnGiant(t2);
    returnGiant(t3);
    returnGiant(t4);
    PROF_END(numerTimesTime);
}

/*
 * New, 13 Jan 1997.
 */
static void feepowermodg(curveParams *par, giant x, giant n)
/* Power ladder.
   x := x^n  (mod 2^q-k)
 */
{
    int len, pos;
    giant t1;

    PROF_START;
    t1 = borrowGiant(par->maxDigits);
    gtog(x, t1);
    int_to_giant(1, x);
    len = bitlen(n);
    pos = 0;
    while(1) {
	if(bitval(n, pos++)) {
	    mulg(t1, x);
	    feemod(par, x);
	}
	if(pos>=len) break;
	gsquare(t1);
	feemod(par, t1);
    }
    returnGiant(t1);
    PROF_END(powerModTime);
}

/*
 * Set g := g mod curveOrder;
 * force g to be between 2 and (curveOrder-2), inclusive.
 *
 * Tolerates zero curve orders (indicating "not known").
 */

/*
 * This version is not normally used; it's for when the reciprocal of
 * curveOrder is not known and won't be used again.
 */
void curveOrderJustify(giant g, giant curveOrder)
{
    giant recip;

    CKASSERT(!isZero(curveOrder));

    recip = borrowGiant(2 * abs(g->sign));
    make_recip(curveOrder, recip);
    curveOrderJustifyWithRecip(g, curveOrder, recip);
    returnGiant(recip);
}

/*
 * New optimzation of curveOrderJustify using known reciprocal, 11 June 1997.
 * g is set to be within [2, curveOrder-2].
 */
static void curveOrderJustifyWithRecip(giant g, giant curveOrder, giant recip)
{
    giant tmp;

    CKASSERT(!isZero(curveOrder));

    modg_via_recip(curveOrder, recip, g);	// g now in [0, curveOrder-1]

    if(isZero(g)) {
    	/*
	 * First degenerate case - (g == 0) : set g := 2
	 */
	dbgLog(("curveOrderJustify: case 1\n"));
   	int_to_giant(2, g);
	return;
    }
    if(isone(g)) {
    	/*
	 * Second case - (g == 1) : set g := 2
	 */
 	dbgLog(("curveOrderJustify: case 2\n"));
   	int_to_giant(2, g);
	return;
    }
    tmp = borrowGiant(g->capacity);
    gtog(g, tmp);
    iaddg(1, tmp);
    if(gcompg(tmp, curveOrder) == 0) {
    	/*
	 * Third degenerate case - (g == (curveOrder-1)) : set g -= 1
	 */
	dbgLog(("curveOrderJustify: case 3\n"));
	int_to_giant(1, tmp);
	subg(tmp, g);
    }
    returnGiant(tmp);
    return;
}

#define RECIP_DEBUG	0
#if	RECIP_DEBUG
#define recipLog(x)	printf x
#else	// RECIP_DEBUG
#define recipLog(x)
#endif	// RECIP_DEBUG

/*
 * curveOrderJustify routines with specific orders, using (and possibly
 * generating) associated reciprocals.
 */
void lesserX1OrderJustify(giant g, curveParams *cp)
{
	/*
	 * Note this is a pointer to a giant in *cp, not a newly
	 * malloc'd giant!
	 */
	giant lesserX1Ord = lesserX1Order(cp);

	if(isZero(lesserX1Ord)) {
		return;
	}

	/*
	 * Calculate reciprocal if we don't have it
	 */
	if(isZero(cp->lesserX1OrderRecip)) {
		if((lesserX1Ord == cp->x1OrderPlus) &&
		   (!isZero(cp->x1OrderPlusRecip))) {
		   	/*
			 * lesserX1Ord happens to be x1OrderPlus, and we
			 * have a reciprocal for x1OrderPlus. Copy it over.
			 */
			recipLog((
				"x1OrderPlusRecip --> lesserX1OrderRecip\n"));
		 	gtog(cp->x1OrderPlusRecip, cp->lesserX1OrderRecip);
		}
		else {
			/* Calculate the reciprocal. */
			recipLog(("calc lesserX1OrderRecip\n"));
			make_recip(lesserX1Ord, cp->lesserX1OrderRecip);
		}
	}
	else {
		recipLog(("using existing lesserX1OrderRecip\n"));
	}
	curveOrderJustifyWithRecip(g, lesserX1Ord, cp->lesserX1OrderRecip);
}

/*
 * Common code used by x1OrderPlusJustify() and x1OrderPlusMod() to generate
 * reciprocal of x1OrderPlus.
 * 8 Sep 1998 - also used by feeSigSign().
 */
void calcX1OrderPlusRecip(curveParams *cp)
{
	if(isZero(cp->x1OrderPlusRecip)) {
		if((cp->x1OrderPlus == lesserX1Order(cp)) &&
		   (!isZero(cp->lesserX1OrderRecip))) {
		   	/*
			 * lesserX1Order happens to be x1OrderPlus, and we
			 * have a reciprocal for lesserX1Order. Copy it over.
			 */
			recipLog((
				"lesserX1OrderRecip --> x1OrderPlusRecip\n"));
		 	gtog(cp->lesserX1OrderRecip, cp->x1OrderPlusRecip);
		}
		else {
			/* Calculate the reciprocal. */
			recipLog(("calc x1OrderPlusRecip\n"));
			make_recip(cp->x1OrderPlus, cp->x1OrderPlusRecip);
		}
	}
	else {
		recipLog(("using existing x1OrderPlusRecip\n"));
	}
}

void x1OrderPlusJustify(giant g, curveParams *cp)
{
	CKASSERT(!isZero(cp->x1OrderPlus));

	/*
	 * Calculate reciprocal if we don't have it
	 */
	calcX1OrderPlusRecip(cp);
	curveOrderJustifyWithRecip(g, cp->x1OrderPlus, cp->x1OrderPlusRecip);
}

/*
 * g := g mod x1OrderPlus. Result may be zero.
 */
void x1OrderPlusMod(giant g, curveParams *cp)
{
	CKASSERT(!isZero(cp->x1OrderPlus));

	/*
	 * Calculate reciprocal if we don't have it
	 */
	calcX1OrderPlusRecip(cp);
	modg_via_recip(cp->x1OrderPlus, cp->x1OrderPlusRecip, g);
}

/*
 * New general-purpose giant mod routine, 8 Jan 97.
 * x := x mod basePrime.
 */

/*
 * This stuff is used to analyze the critical loop behavior inside feemod().
 */
#define FEEMOD_LOOP_TEST	0
#if	FEEMOD_LOOP_TEST
/*
 * these two are only examined via debugger
 */
unsigned feemodCalls = 0;		// calls to feemod()
unsigned feemodMultLoops = 0;		// times while() loop runs > once
#define FEEMOD_LOOP_INCR	feemodLoops++
#define FEEMOD_CALL_INCR	feemodCalls++
#else	// FEEMOD_LOOP_TEST
#define FEEMOD_LOOP_INCR
#define FEEMOD_CALL_INCR
#endif	// FEEMOD_LOOP_TEST


void
feemod(curveParams *par, giant x)
{
    int sign, sign2;
    giant t1;
    giant t3;
    giant t4;
    giant t5;
    #if		FEEMOD_LOOP_TEST
    unsigned feemodLoops = 0;
    #endif	// FEEMOD_LOOP_TEST

    FEEMOD_CALL_INCR;		// for FEEMOD_LOOP_TEST
    INCR_FEEMODS;		// for ellipticMeasure
    PROF_INCR(numFeemod);	// for general profiling

    switch(par->primeType) {
        case FPT_Mersenne:
	    /*
	     * Super-optimized Mersenne prime modulus case
	     */
	    gmersennemod(par->q, x);
	    break;

        case FPT_FEE:
	    /*
	     * General 2**q-k case
	     */
	    sign = (x->sign < 0) ? -1 : 1;
	    sign2 = 1;
	    x->sign = abs(x->sign);
	    if(gcompg(par->basePrime, x) >= 0) {
	    	goto outFee;
	    }
	    t1 = borrowGiant(par->maxDigits);
	    t3 = borrowGiant(par->maxDigits);
	    t4 = borrowGiant(par->maxDigits);
	    t5 = borrowGiant(par->maxDigits);

	    /* Begin OPT: 11 Jan 98 REC. */
	    if( ((par->q & (GIANT_BITS_PER_DIGIT - 1)) == 0) &&
	        (par->k >= 0) &&
		(par->k < GIANT_DIGIT_MASK)) {

		/*
		 * Microscopic mod for certain regions of {q,k}
		 * parameter space.
		 */
		int j, digits, excess, max;
		giantDigit carry;
		giantDigit termHi;	// double precision
		giantDigit termLo;
		giantDigit *p1, *p2;

		digits = par->q >> GIANT_LOG2_BITS_PER_DIGIT;
		while(bitlen(x) > par->q) {
		    excess = (x->sign) - digits;
		    max = (excess > digits) ? excess : digits;
		    carry = 0;

		    p1 = &x->n[0];
		    p2 = &x->n[digits];

		    if(excess <= digits) {
			carry = VectorMultiply(par->k,
				p2,
				excess,
				p1);

			/* propagate final carry */
			p1 += excess;
			for(j = excess; j < digits; j++) {

			    /*
			     * term = *p1 + carry;
			     * *p1++ = term & 65535;
			     * carry = term >> 16;
			     */
			    termLo = giantAddDigits(*p1, carry, &carry);
			    *p1++ = termLo;
			}
		    } else {
			carry = VectorMultiply(par->k,
				p2,
				digits,
				p1);
			p1 += digits;
			p2 += digits;
			for(j = digits; j < excess; j++) {
			    /*
			     * term = (par->k)*(*p2++) + carry;
			     */
			    giantMulDigits(par->k,
			    	*p2++,
				&termLo,
				&termHi);
			    giantAddDouble(&termLo, &termHi, carry);

			    /*
			     * *p1++ = term & 65535;
			     * carry = term >> 16;
			     */
			    *p1++ = termLo;
			    carry = termHi;
			}
		    }

		    if(carry > 0) {
			x->n[max] = carry;
		    } else {
			while(--max){
			    if(x->n[max] != 0) break;
			}
		    }
		    x->sign = max + 1;
		    FEEMOD_LOOP_INCR;
		}
	    } else { /* Macroscopic mod for general PT_FEE case. */
		int_to_giant(par->k, t4);
		while(bitlen(x) > par->q) {
		    /* Enter fast loop, as in FEE patent. */
		    int_to_giant(1, t5);
		    gtog(x, t3);
		    extractbits(par->q, t3, x);
		    while(bitlen(t3) > par->q) {
			gshiftright(par->q, t3);
			extractbits(par->q, t3, t1);
			PAUSE_ELL_MEASURE;
			mulg(t4, t5);
			mulg(t5, t1);
			RESUME_ELL_MEASURE;
			addg(t1, x);
		    }
		    FEEMOD_LOOP_INCR;
		}
	    }
	    /* End OPT: 11 Jan 98 REC. */

	    sign2 = (x->sign < 0)? -1: 1;
	    x->sign = abs(x->sign);
	    returnGiant(t1);
	    returnGiant(t3);
	    returnGiant(t4);
	    returnGiant(t5);
	 outFee:
	    if(gcompg(x, par->basePrime) >=0) subg(par->basePrime, x);
	    if(sign != sign2) {
		    giant t2 = borrowGiant(par->maxDigits);
		    gtog(par->basePrime, t2);
		    subg(x, t2);
		    gtog(t2, x);
		    returnGiant(t2);
	    }
	    break;
	    /* end case PT_FEE */

        case FPT_General:
	    /*
	     * Use brute-force modg.
	     */
	    #if		FEE_DEBUG
	    if(par->basePrimeRecip == NULL) {
	    	CKRaise("feemod(PT_GENERAL): No basePrimeRecip!\n");
	    }
	    #endif	/* FEE_DEBUG */
	    modg_via_recip(par->basePrime, par->basePrimeRecip, x);
	    break;
	    
	case FPT_Default:
	    /* never appears here */
	    CKASSERT(0);
	    break;
    } /* switch primeType */

    #if		FEEMOD_LOOP_TEST
    if(feemodLoops > 1) {
    	feemodMultLoops++;
		if(feemodLoops > 2) {
			printf("feemod while loop executed %d times\n", feemodLoops);
		}
    }
    #endif	// FEEMOD_LOOP_TEST

    return;
}

/*
 * For a given curveParams, calculate minBytes and maxDigits.
 * Assumes valid primeType, and also a valid basePrime for PT_GENERAL.
 */
void calcGiantSizes(curveParams *cp)
{

	if(cp->primeType == FPT_General) {
	    cp->minBytes = (bitlen(cp->basePrime) + 7) / 8;
	}
	else {
	    cp->minBytes = giantMinBytes(cp->q, cp->k);
	}
	cp->maxDigits = giantMaxDigits(cp->minBytes);
}

unsigned giantMinBytes(unsigned q, int k)
{
	unsigned kIsNeg = (k < 0) ? 1 : 0;

	return (q + 7 + kIsNeg) / 8;
}

/*
 * min value for "extra" bytes. Derived from the fact that during sig verify,
 * we have to multiply a giant representing a digest - which may be
 * 20 bytes for SHA1 - by a giant of minBytes.
 */
#define MIN_EXTRA_BYTES		20

unsigned giantMaxDigits(unsigned minBytes)
{
	unsigned maxBytes = 4 * minBytes;

	if((maxBytes - minBytes) < MIN_EXTRA_BYTES) {
		maxBytes = minBytes + MIN_EXTRA_BYTES;
	}
	return BYTES_TO_GIANT_DIGITS(maxBytes);
}


/*
 * Optimized binvg(basePrime, x). Avoids the general modg() in favor of
 * feemod.
 */
int binvg_cp(curveParams *cp, giant x)
{
	feemod(cp, x);
	return(binvaux(cp->basePrime, x));
}

/*
 * Optimized binvg(x1OrderPlus, x). Uses x1OrderPlusMod().
 */
int binvg_x1OrderPlus(curveParams *cp, giant x)
{
	x1OrderPlusMod(x, cp);
	return(binvaux(cp->x1OrderPlus, x));
}
