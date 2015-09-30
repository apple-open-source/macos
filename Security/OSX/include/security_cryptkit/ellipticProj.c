/* Copyright (c) 1998,2011,2014 Apple Inc.  All Rights Reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * ellipticProj.c - elliptic projective algebra routines.
 *
 * Revision History
 * ----------------
 * 1 Sep 1998 at Apple
 *	Created.
 *
 **************************************************************

   PROJECTIVE FORMAT

   Functions are supplied herein for projective format
   of points.  Alternative formats differ in their
   range of applicability, efficiency, and so on.
   Primary advantages of the projective format herein are:
    -- No explicit inversions (until perhaps one such at the end of
       an elliptic multiply operation)
    -- Fairly low operation count (~11 muls for point doubling,
       ~16 muls for point addition)

   The elliptic curve is over F_p, with p > 3 prime, and Weierstrass
   parameterization:

      y^2 = x^3 + a x + b

   The projective-format coordinates are actually stored in
   the form {X, Y, Z}, with true x,y
   coordinates on the curve given by {x,y} = {X/Z^2, Y/Z^3}.
   The function normalizeProj() performs the
   transformation from projective->true.
   (The other direction is trivial, i.e. {x,y} -> {x,y,1} will do.)
   The basic point multiplication function is

      ellMulProj()

   which obtains the result k * P for given point P and integer
   multiplier k.  If true {x,y} are required for a multiple, one
   passes a point P = {X, Y, 1} to ellMulProj(), then afterwards
   calls normalizeProj(),

   Projective format is an answer to the typical sluggishness of
   standard elliptic arithmetic, whose explicit inversion in the
   field is, depending of course on the machinery and programmer,
   costly.  Projective format is thus especially interesting for
   cryptography.

   REFERENCES

		perspective," Springer-Verlag, manuscript

   Solinas J 1998, IEEE P1363 Annex A (draft standard)

***********************************************************/

#include "ckconfig.h"
#if CRYPTKIT_ELL_PROJ_ENABLE

#include "ellipticProj.h"
#include "falloc.h"
#include "curveParams.h"
#include "elliptic.h"
#include "feeDebug.h"

/*
 * convert REC-style smulg to generic imulg
 */
#define smulg(s, g) imulg((unsigned)s, g)

pointProj newPointProj(unsigned numDigits)
{
	pointProj pt;

	pt = (pointProj) fmalloc(sizeof(pointProjStruct));
	pt->x = newGiant(numDigits);
	pt->y = newGiant(numDigits);
	pt->z = newGiant(numDigits);
	return(pt);
}

void freePointProj(pointProj pt)
{
	clearGiant(pt->x); freeGiant(pt->x);
	clearGiant(pt->y); freeGiant(pt->y);
	clearGiant(pt->z); freeGiant(pt->z);
	ffree(pt);
}

void ptopProj(pointProj pt1, pointProj pt2)
{
	gtog(pt1->x, pt2->x);
	gtog(pt1->y, pt2->y);
	gtog(pt1->z, pt2->z);
}

/**************************************************************
 *
 *	Elliptic curve operations
 *
 **************************************************************/

/* Begin projective-format functions for

   y^2 = x^3 + a x + b.

   These are useful in elliptic curve cryptography (ECC).
   A point is kept as a triple {X,Y,Z}, with the true (x,y)
   coordinates given by

	{x,y} = {X/Z^2, Y/Z^3}

   The function normalizeProj() performs the inverse conversion to get
   the true (x,y) pair.
 */

void ellDoubleProj(pointProj pt, curveParams *cp)
/* pt := 2 pt on the curve. */
{
	giant x = pt->x, y = pt->y, z = pt->z;
	giant t1;
	giant t2;
	giant t3;

	if(isZero(y) || isZero(z)) {
		int_to_giant(1,x); int_to_giant(1,y); int_to_giant(0,z);
		return;
	}
	t1 = borrowGiant(cp->maxDigits);
	t2 = borrowGiant(cp->maxDigits);
	t3 = borrowGiant(cp->maxDigits);

	if((cp->a->sign >= 0) || (cp->a->n[0] != 3)) { /* Path prior to Apr2001. */
		gtog(z,t1); gsquare(t1); feemod(cp, t1);
		gsquare(t1); feemod(cp, t1);
		mulg(cp->a, t1); feemod(cp, t1);			/* t1 := a z^4. */
		gtog(x, t2); gsquare(t2); feemod(cp, t2);
			smulg(3, t2);                           /* t2 := 3x^2. */
		addg(t2, t1); feemod(cp, t1);	 			/* t1 := slope m. */
	} else { /* New optimization for a = -3 (post Apr 2001). */
		gtog(z, t1); gsquare(t1); feemod(cp, t1);   /* t1 := z^2. */
		gtog(x, t2); subg(t1, t2);                  /* t2 := x-z^2. */
		addg(x, t1); smulg(3, t1);                  /* t1 := 3(x+z^2). */
		mulg(t2, t1); feemod(cp, t1);               /* t1 := slope m. */
	}
	mulg(y, z); addg(z,z); feemod(cp, z);	  	/* z := 2 y z. */
	gtog(y, t2); gsquare(t2); feemod(cp, t2); 	/* t2 := y^2. */
	gtog(t2, t3); gsquare(t3); feemod(cp, t3);	/* t3 := y^4. */
	gshiftleft(3, t3);  				/* t3 := 8 y^4. */
	mulg(x, t2); gshiftleft(2, t2); feemod(cp, t2);	/* t2 := 4xy^2. */
	gtog(t1, x); gsquare(x); feemod(cp, x);
	subg(t2, x); subg(t2, x); feemod(cp, x);	/* x done. */
	gtog(t1, y); subg(x, t2); mulg(t2, y); subg(t3, y);
	feemod(cp, y);
	returnGiant(t1);
	returnGiant(t2);
	returnGiant(t3);
}

void ellAddProj(pointProj pt0, pointProj pt1, curveParams *cp)
/* pt0 := pt0 + pt1 on the curve. */
{
	giant x0 = pt0->x, y0 = pt0->y, z0 = pt0->z,
		  x1 = pt1->x, y1 = pt1->y, z1 = pt1->z;
	giant t1;
	giant t2;
	giant t3;
	giant t4;
	giant t5;
	giant t6;
	giant t7;

	if(isZero(z0)) {
		gtog(x1,x0); gtog(y1,y0); gtog(z1,z0);
		return;
	}
	if(isZero(z1)) return;

	t1 = borrowGiant(cp->maxDigits);
	t2 = borrowGiant(cp->maxDigits);
	t3 = borrowGiant(cp->maxDigits);
	t4 = borrowGiant(cp->maxDigits);
	t5 = borrowGiant(cp->maxDigits);
	t6 = borrowGiant(cp->maxDigits);
	t7 = borrowGiant(cp->maxDigits);

	gtog(x0, t1); gtog(y0,t2); gtog(z0, t3);
	gtog(x1, t4); gtog(y1, t5);
	if(!isone(z1)) {
		gtog(z1, t6);
		gtog(t6, t7); gsquare(t7); feemod(cp, t7);
		mulg(t7, t1); feemod(cp, t1);
		mulg(t6, t7); feemod(cp, t7);
		mulg(t7, t2); feemod(cp, t2);
	}
	gtog(t3, t7); gsquare(t7); feemod(cp, t7);
	mulg(t7, t4); feemod(cp, t4);
	mulg(t3, t7); feemod(cp, t7);
	mulg(t7, t5); feemod(cp, t5);
	negg(t4); addg(t1, t4); feemod(cp, t4);
	negg(t5); addg(t2, t5); feemod(cp, t5);
	if(isZero(t4)) {
		if(isZero(t5)) {
			ellDoubleProj(pt0, cp);
	    	} else {
			int_to_giant(1, x0); int_to_giant(1, y0);
			int_to_giant(0, z0);
		}
		goto out;
	}
	addg(t1, t1); subg(t4, t1); feemod(cp, t1);
	addg(t2, t2); subg(t5, t2); feemod(cp, t2);
	if(!isone(z1)) {
		mulg(t6, t3); feemod(cp, t3);
	}
	mulg(t4, t3); feemod(cp, t3);
	gtog(t4, t7); gsquare(t7); feemod(cp, t7);
	mulg(t7, t4); feemod(cp, t4);
	mulg(t1, t7); feemod(cp, t7);
	gtog(t5, t1); gsquare(t1); feemod(cp, t1);
	subg(t7, t1); feemod(cp, t1);
	subg(t1, t7); subg(t1, t7); feemod(cp, t7);
	mulg(t7, t5); feemod(cp, t5);
	mulg(t2, t4); feemod(cp, t4);
	gtog(t5, t2); subg(t4,t2); feemod(cp, t2);
	if(t2->n[0] & 1) { /* Test if t2 is odd. */
		addg(cp->basePrime, t2);
	}
	gshiftright(1, t2);
	gtog(t1, x0); gtog(t2, y0); gtog(t3, z0);
out:
	returnGiant(t1);
	returnGiant(t2);
	returnGiant(t3);
	returnGiant(t4);
	returnGiant(t5);
	returnGiant(t6);
	returnGiant(t7);
}


void ellNegProj(pointProj pt, curveParams *cp)
/* pt := -pt on the curve. */
{
	negg(pt->y); feemod(cp, pt->y);
}

void ellSubProj(pointProj pt0, pointProj pt1, curveParams *cp)
/* pt0 := pt0 - pt1 on the curve. */
{
	ellNegProj(pt1, cp);
	ellAddProj(pt0, pt1,cp);
	ellNegProj(pt1, cp);
}

/*
 * Simple projective multiply.
 *
 * pt := pt * k, result normalized.
 */
void ellMulProjSimple(pointProj pt0, giant k, curveParams *cp)
{
	pointProjStruct pt1;	// local, giants borrowed

	CKASSERT(isone(pt0->z));
	CKASSERT(cp->curveType == FCT_Weierstrass);

	/* ellMulProj assumes constant pt0, can't pass as src and dst */
	pt1.x = borrowGiant(cp->maxDigits);
	pt1.y = borrowGiant(cp->maxDigits);
	pt1.z = borrowGiant(cp->maxDigits);
	ellMulProj(pt0, &pt1, k, cp);
	normalizeProj(&pt1, cp);
	CKASSERT(isone(pt1.z));

	ptopProj(&pt1, pt0);
	returnGiant(pt1.x);
	returnGiant(pt1.y);
	returnGiant(pt1.z);
}

void ellMulProj(pointProj pt0, pointProj pt1, giant k, curveParams *cp)
/* General elliptic multiplication;
   pt1 := k*pt0 on the curve,
   with k an arbitrary integer.
 */
{
	giant x = pt0->x, y = pt0->y, z = pt0->z,
		  xx = pt1->x, yy = pt1->y, zz = pt1->z;
	int ksign, hlen, klen, b, hb, kb;
    	giant t0;

	CKASSERT(cp->curveType == FCT_Weierstrass);
	if(isZero(k)) {
		int_to_giant(1, xx);
		int_to_giant(1, yy);
		int_to_giant(0, zz);
		return;
	}
	t0 = borrowGiant(cp->maxDigits);
    	ksign = k->sign;
	if(ksign < 0) negg(k);
	gtog(x,xx); gtog(y,yy); gtog(z,zz);
	gtog(k, t0); addg(t0, t0); addg(k, t0); /* t0 := 3k. */
	hlen = bitlen(t0);
	klen = bitlen(k);
	for(b = hlen-2; b > 0; b--) {
		ellDoubleProj(pt1,cp);
		hb = bitval(t0, b);
		if(b < klen) kb = bitval(k, b); else kb = 0;
		if((hb != 0) && (kb == 0))
			ellAddProj(pt1, pt0, cp);
		else if((hb == 0) && (kb !=0))
			ellSubProj(pt1, pt0, cp);
	}
	if(ksign < 0) {
		ellNegProj(pt1, cp);
		k->sign = -k->sign;
	}
	returnGiant(t0);
}

void normalizeProj(pointProj pt, curveParams *cp)
/* Obtain actual x,y coords via normalization:
   {x,y,z} := {x/z^2, y/z^3, 1}.
 */

{	giant x = pt->x, y = pt->y, z = pt->z;
	giant t1;

	CKASSERT(cp->curveType == FCT_Weierstrass);
	if(isZero(z)) {
		int_to_giant(1,x); int_to_giant(1,y);
		return;
	}
	t1 = borrowGiant(cp->maxDigits);
	binvg_cp(cp, z);		// was binvaux(p, z);
		gtog(z, t1);
	gsquare(z); feemod(cp, z);
	mulg(z, x); feemod(cp, x);
	mulg(t1, z); mulg(z, y); feemod(cp, y);
	int_to_giant(1, z);
	returnGiant(t1);
}

static int
jacobi_symbol(giant a, curveParams *cp)
/* Standard Jacobi symbol (a/cp->basePrime).
  basePrime must be odd, positive. */
{
	int t = 1, u;
	giant t5 = borrowGiant(cp->maxDigits);
	giant t6 = borrowGiant(cp->maxDigits);
	giant t7 = borrowGiant(cp->maxDigits);
	int rtn;

	gtog(a, t5); feemod(cp, t5);
	gtog(cp->basePrime, t6);
	while(!isZero(t5)) {
	    u = (t6->n[0]) & 7;
		while((t5->n[0] & 1) == 0) {
			gshiftright(1, t5);
			if((u==3) || (u==5)) t = -t;
		}
		gtog(t5, t7); gtog(t6, t5); gtog(t7, t6);
		u = (t6->n[0]) & 3;
		if(((t5->n[0] & 3) == 3) && ((u & 3) == 3)) t = -t;
		modg(t6, t5);
	}
	if(isone(t6)) {
		rtn = t;
	}
	else {
		rtn = 0;
	}
	returnGiant(t5);
	returnGiant(t6);
	returnGiant(t7);

	return rtn;
}

static void
powFp2(giant a, giant b, giant w2, giant n, curveParams *cp)
/* Perform powering in the field F_p^2:
   a + b w := (a + b w)^n (mod p), where parameter w2 is a quadratic
   nonresidue (formally equal to w^2).
 */
{
	int j;
	giant t6;
	giant t7;
	giant t8;
	giant t9;

	if(isZero(n)) {
		int_to_giant(1,a);
		int_to_giant(0,b);
		return;
	}
    	t6 = borrowGiant(cp->maxDigits);
    	t7 = borrowGiant(cp->maxDigits);
    	t8 = borrowGiant(cp->maxDigits);
    	t9 = borrowGiant(cp->maxDigits);
	gtog(a, t8); gtog(b, t9);
	for(j = bitlen(n)-2; j >= 0; j--) {
		gtog(b, t6);
		mulg(a, b); addg(b,b); feemod(cp, b);  /* b := 2 a b. */
		gsquare(t6); feemod(cp, t6);
		mulg(w2, t6); feemod(cp, t6);
		gsquare(a); addg(t6, a); feemod(cp, a);
						/* a := a^2 + b^2 w2. */
		if(bitval(n, j)) {
			gtog(b, t6); mulg(t8, b); feemod(cp, b);
			gtog(a, t7); mulg(t9, a); addg(a, b); feemod(cp, b);
			mulg(t9, t6); feemod(cp, t6);
			mulg(w2, t6); feemod(cp, t6);
			mulg(t8, a); addg(t6, a); feemod(cp, a);
		}
	}
	returnGiant(t6);
	returnGiant(t7);
	returnGiant(t8);
	returnGiant(t9);
	return;
}

static void
powermodg(
	giant		x,
	giant		n,
	curveParams	*cp
)
/* x becomes x^n (mod basePrime). */
{
	int 		len, pos;
	giant		scratch2 = borrowGiant(cp->maxDigits);

	gtog(x, scratch2);
	int_to_giant(1, x);
	len = bitlen(n);
	pos = 0;
	while (1)
	{
		if (bitval(n, pos++))
		{
			mulg(scratch2, x);
			feemod(cp, x);
		}
		if (pos>=len)
			break;
		gsquare(scratch2);
		feemod(cp, scratch2);
	}
	returnGiant(scratch2);
}

static int sqrtmod(giant x, curveParams *cp)
/* If Sqrt[x] (mod p) exists, function returns 1, else 0.
   In either case x is modified, but if 1 is returned,
   x:= Sqrt[x] (mod p).
 */
{
	int rtn;
	giant t0 = borrowGiant(cp->maxDigits);
	giant t1 = borrowGiant(cp->maxDigits);
	giant t2 = borrowGiant(cp->maxDigits);
	giant t3 = borrowGiant(cp->maxDigits);
	giant t4 = borrowGiant(cp->maxDigits);

	giant p = cp->basePrime;

    	feemod(cp, x);			/* Justify the argument. */
    	gtog(x, t0);  /* Store x for eventual validity check on square root. */
    	if((p->n[0] & 3) == 3) {  /* The case p = 3 (mod 4). */
		gtog(p, t1);
		iaddg(1, t1); gshiftright(2, t1);
		powermodg(x, t1, cp);
		goto resolve;
    	}
	/* Next, handle case p = 5 (mod 8). */
    	if((p->n[0] & 7) == 5) {
		gtog(p, t1); int_to_giant(1, t2);
		subg(t2, t1); gshiftright(2, t1);
		gtog(x, t2);
		powermodg(t2, t1, cp);  /* t2 := x^((p-1)/4) % p. */
		iaddg(1, t1);
		gshiftright(1, t1); /* t1 := (p+3)/8. */
		if(isone(t2)) {
			powermodg(x, t1, cp);  /* x^((p+3)/8) is root. */
			goto resolve;
		} else {
			int_to_giant(1, t2); subg(t2, t1);
				/* t1 := (p-5)/8. */
			gshiftleft(2,x);
			powermodg(x, t1, cp);
			mulg(t0, x); addg(x, x); feemod(cp, x);
				/* 2x (4x)^((p-5)/8. */
			goto resolve;
		}
	}

	/* Next, handle tougher case: p = 1 (mod 8). */
	int_to_giant(2, t1);
	while(1) {  /* Find appropriate nonresidue. */
		gtog(t1, t2);
		gsquare(t2); subg(x, t2); feemod(cp, t2);
		if(jacobi_symbol(t2, cp) == -1) break;
		iaddg(1, t1);
	}  /* t2 is now w^2 in F_p^2. */
   	int_to_giant(1, t3);
   	gtog(p, t4); iaddg(1, t4); gshiftright(1, t4);
	powFp2(t1, t3, t2, t4, cp);
	gtog(t1, x);

resolve:
   	gtog(x,t1); gsquare(t1); feemod(cp, t1);
    	if(gcompg(t0, t1) == 0) {
		rtn = 1; 	/* Success. */
	}
	else {
		rtn = 0;	/* no square root */
	}
	returnGiant(t0);
	returnGiant(t1);
	returnGiant(t2);
	returnGiant(t3);
	returnGiant(t4);
	return rtn;
}


void findPointProj(pointProj pt, giant seed, curveParams *cp)
/* Starting with seed, finds a random (projective) point {x,y,1} on curve.
 */
{
	giant x = pt->x, y = pt->y, z = pt->z;

	CKASSERT(cp->curveType == FCT_Weierstrass);
	feemod(cp, seed);
    	while(1) {
		gtog(seed, x);
		gsquare(x); feemod(cp, x);	// x := seed^2
		addg(cp->a, x);			// x := seed^2 + a
		mulg(seed,x); 			// x := seed^3 + a*seed
		addg(cp->b, x);
		feemod(cp, x);			// x := seed^3 + a seed + b.
		/* test cubic form for having root. */
		if(sqrtmod(x, cp)) break;
		iaddg(1, seed);
	}
	gtog(x, y);
    	gtog(seed,x);
	int_to_giant(1, z);
}

#endif	/* CRYPTKIT_ELL_PROJ_ENABLE */
