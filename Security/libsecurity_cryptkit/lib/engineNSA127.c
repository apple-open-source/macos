/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE COMPUTER, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE COMPUTER,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************

   CONFIDENTIAL   CONFIDENTIAL    CONFIDENTIAL
   engineNSA.c

   Security Engine code, to be compiled prior to software
   distribution.  The code performs the
   elliptic curve algebra fundamental to the patented FEE
   system.

   This Engine is designed to be virtually nonmalleable
   with respect to key size.  This is achieved herein
   via hard-coding of numerical algorithms with respect to
   the DEPTH = 4 security level (127 bit Mersenne prime).

   In meetings between the NSA and NeXT Software, Inc. in
   1995-1996, the notion of Security Engine emerged as a
   means by which one could discourage disassembly of
   FEE compilations, especially when such disassembly
   has the sinister goal of modifying encryption depth.

   DO NOT EVEN THINK ABOUT READING THE SOURCE CODE
   BELOW UNLESS YOU ARE EXPLICITLY AUTHORIZED TO DO SO
   BY NeXT OR ITS DESIGNEE.

   R. E. Crandall
   c. 1996, NeXT Software, Inc.
   All Rights Reserved.
*/

/* This engine requires no initialization.  There is one
   function to becalled externally, namely elliptic().
 */



















/*
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 *  6 Aug 06	Doug Mitchell at NeXT
 *	'a' argument to elliptic() and ell_even() is now a giant.
 * 25 Jul 96  	Richard Crandall and Doug Mitchell at NeXT
 * 	Wrapped ENGINEmul() with gmersennemod(127,.) to guarantee no
 *		overflow in the hard-coded mul.
 * 	Fixed sign calculation bug in ENGINEmul().
 * 24 Jul 96	Doug Mitchell at NeXT
 *	Made conditional on ENGINE_127_BITS.
 *	Made all functions except for elliptic() static.
 *	Renamed some giants function calls via #define.
 *	Deleted use of array of static pseudo-giants.
 *	Cosmetic changes for debuggability.
 * 19 Jun 96	Richard Crandall at NeXT
 *	Created.
 */

#include "ckconfig.h"

#if	ENGINE_127_BITS
/*
 * This file is obsolete as of 8 January 1997.
 */
#error	Hey! New curveParam-dependent 127-bit elliptic() needed!
#warning Using NSA-approved 127-bit security engine...

#include	"NSGiantIntegers.h"

#define D 65536
#define DM 65535

/*
 * Size of 127-bit giantstruct n[] array, in shorts.
 */
#define SHORTCOUNT 		(8 * 2)
#define BORROW_SIZE		0


static void
ENGINEmul(giant a, giant b) {
	int a0,a1,a2,a3,a4,a5,a6,a7,
       b0,b1,b2,b3,b4,b5,b6,b7;
   int asign, bsign;
   int	 i, j, car;
   unsigned int prod;
   unsigned short mult;

   gmersennemod(127, a);
   gmersennemod(127, b);
   asign = a->sign;
   bsign = b->sign;

   for(j = abs(asign); j < SHORTCOUNT; j++) a->n[j] = 0;
   for(j = abs(bsign); j < SHORTCOUNT; j++) b->n[j] = 0;
	a0 = a->n[0];
	a1 = a->n[1];
	a2 = a->n[2];
	a3 = a->n[3];
	a4 = a->n[4];
	a5 = a->n[5];
	a6 = a->n[6];
	a7 = a->n[7];
	b0 = b->n[0];
	b1 = b->n[1];
	b2 = b->n[2];
	b3 = b->n[3];
	b4 = b->n[4];
	b5 = b->n[5];
	b6 = b->n[6];
	b7 = b->n[7];
   for(j = 0; j < SHORTCOUNT; j++) b->n[j] = 0;

   i = 0;
   mult = b0;
   car = 0;

      prod = a0 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a1 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a2 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a3 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a4 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a5 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a6 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a7 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      b->n[i] = car;

   i = 1;
   mult = b1;
   car = 0;

      prod = a0 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a1 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a2 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a3 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a4 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a5 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a6 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a7 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      b->n[i] = car;

   i = 2;
   mult = b2;
   car = 0;

      prod = a0 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a1 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a2 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a3 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a4 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a5 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a6 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a7 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      b->n[i] = car;

   i = 3;
   mult = b3;
   car = 0;

      prod = a0 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a1 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a2 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a3 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a4 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a5 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a6 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a7 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      b->n[i] = car;

   i = 4;
   mult = b4;
   car = 0;

      prod = a0 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a1 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a2 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a3 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a4 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a5 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a6 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a7 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      b->n[i] = car;

   i = 5;
   mult = b5;
   car = 0;

      prod = a0 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a1 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a2 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a3 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a4 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a5 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a6 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a7 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      b->n[i] = car;

   i = 6;
   mult = b6;
   car = 0;

      prod = a0 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a1 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a2 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a3 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a4 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a5 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a6 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a7 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      b->n[i] = car;

   i = 7;
   mult = b7;
   car = 0;

      prod = a0 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a1 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a2 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a3 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a4 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a5 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a6 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      prod = a7 * mult + b->n[i] + car;
      b->n[i++] = prod & DM;
      car = prod/D;

      b->n[i] = car;
      b->sign = abs(b->sign) + abs(a->sign);
      for(j = (b->sign)-1; j >= 0; j--) {
				if(b->n[j] != 0) {
                break;
            }
      }
      b->sign = j+1;
      gmersennemod(127,b);
}

static void
ell_even(giant x1, giant z1, giant x2, giant z2, giant a, int q)
{
	giant t1, t2, t3;

	t1 = borrowGiant(BORROW_SIZE);
	t2 = borrowGiant(BORROW_SIZE);
	t3 = borrowGiant(BORROW_SIZE);

	gtog(x1, t1); gsquare(t1); gmersennemod(q, t1);
	gtog(z1, t2); gsquare(t2); gmersennemod(q, t2);
	gtog(x1, t3); ENGINEmul(z1, t3);
	gtog(t1, x2); subg(t2, x2); gsquare(x2); gmersennemod(q, x2);
	gtog(a, z2);
	ENGINEmul(t3, z2);
	addg(t1, z2); addg(t2, z2); ENGINEmul(t3, z2);
	gshiftleft(2, z2);
	gmersennemod(q, z2);

	returnGiant(t1);
	returnGiant(t2);
	returnGiant(t3);
}

static void
ell_odd(giant x1, giant z1, giant x2, giant z2, giant xor, giant zor, int q)
{
	giant t1, t2, t3;

	t1 = borrowGiant(BORROW_SIZE);
	t2 = borrowGiant(BORROW_SIZE);
	t3 = borrowGiant(BORROW_SIZE);

	gtog(x1, t1); subg(z1, t1);
	gtog(x2, t2); addg(z2, t2);
	ENGINEmul(t1, t2);
	gtog(x1, t1); addg(z1, t1);
	gtog(x2, t3); subg(z2, t3);
	ENGINEmul(t3, t1);
	gtog(t2, x2); addg(t1, x2);
	gsquare(x2);  gmersennemod(q, x2); //?
	gtog(t2, z2); subg(t1, z2);
	gsquare(z2);  gmersennemod(q, z2); //?
	ENGINEmul(zor, x2);
	ENGINEmul(xor, z2);

	returnGiant(t1);
	returnGiant(t2);
	returnGiant(t3);
}

/* Elliptic multiply.
   For given curve parameter a and given prime p = 2^q-1,
   the point (xx,zz) becomes k * (xx,zz), in place.
 */
void
elliptic(giant xx, giant zz, giant k, giant a, int q)
{
	int len = bitlen(k), pos = len-2;
        giant xs;
        giant zs;
        giant xorg;
        giant zorg;

	if(scompg(1,k)) return;
	if(scompg(2,k)) {
		ell_even(xx, zz, xx, zz, a, q);
		return;
	}

        zs = borrowGiant(BORROW_SIZE);
        xs = borrowGiant(BORROW_SIZE);
        zorg = borrowGiant(BORROW_SIZE);
        xorg = borrowGiant(BORROW_SIZE);

	gtog(xx, xorg); gtog(zz, zorg);
	ell_even(xx, zz, xs, zs, a, q);
	do{
	   if(bitval(k, pos--)) {
	   	ell_odd(xs, zs, xx, zz, xorg, zorg, q);
		ell_even(xs, zs, xs, zs, a, q);
	   } else {
	   	ell_odd(xx, zz, xs, zs, xorg, zorg, q);
		ell_even(xx, zz, xx, zz, a, q);
	   }
	} while(pos >=0);

        returnGiant(xs);
        returnGiant(zs);
        returnGiant(xorg);
        returnGiant(zorg);
}

#endif	/* ENGINE_127_BITS */
