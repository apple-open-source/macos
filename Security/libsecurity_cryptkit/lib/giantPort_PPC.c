/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE COMPUTER, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE COMPUTER,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * giantPort_PPC.c - PPC-dependent giant definitions.
 *
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 * 06 Apr 1998	Doug Mitchell at Apple
 *	Created.
 */

#include "feeDebug.h"
#include "platform.h"
#include "giantPort_PPC.h"

#if	!PPC_GIANT_PORT_INLINE


/*
 * Multiple-precision arithmetic routines/macros.
 */

asm giantDigit giantAddDigits(
	register giantDigit dig1,
	register giantDigit dig2,
	register giantDigit *carry)	/* RETURNED, 0 or 1 */

{
	/*
	 * dig1  : r3
	 * dig2  : r4
	 * carry : r5
	 * sum   : r6
	 */

	/* sum = dig1 + dig2 */
	add	r6, dig1, dig2;

	/* if((sum < dig1) || (sum < dig2)) */
	cmpl	crf0,0,r6,dig1
	bc	12,0,*+12
	cmpl	crf0,0,r6,dig2
	bc	4,0,*+16

	/* *carry = 1; */
	li	r7,1
	stw	r7, 0(r5)
	b	*+12

	/* else *carry = 0; */
	li	r7,0
	stw	r7, 0(r5)

	/* return sum in r3 */
	mr.	r3,r6
	blr
}

/*
 * Add a single digit value to a double digit accumulator in place.
 * Carry out of the MSD of the accumulator is not handled.
 * This should work any size giantDigits up to unsigned int.
 */
asm void giantAddDouble(
 	register giantDigit *accLow,		/* IN/OUT */
	register giantDigit *accHigh,		/* IN/OUT */
	register giantDigit val)
{
	/*
	 * r3 : accLow
	 * r4 : accHi
	 * r5 : val
	 * r6 : sumLo
	 * r7 : *accLow
	 */

	/* giantDigit sumLo = *accLow + val; */
	lwz	r7,0(accLow)
	add	r6,r7,val

	/* if((sumLo < *accLow) || (sumLo < val)) { */
	cmpl     crf0,0,r6,r7
	bc       12,0,*+12
	cmpl     crf0,0,r6,val
	bc       4,0,*+16

	/* (*accHigh)++; */
	lwz	r7, 0(accHigh)
	addi	r7,r7,1
	stw	r7, 0(accHigh)

	/* *accLow = sumLo; */
	stw		r6,0(accLow)
	blr
}

asm giantDigit giantSubDigits(
	register giantDigit a,
	register giantDigit b,
	register giantDigit *borrow)		/* RETURNED, 0 or 1 */

{
	/* a  : r3
	   b  : r4
	   borrow : r5
	   diff   : r6 */

	/* giantDigit diff = a - b; */
	subf	r6, b, a;

	/* if(a < b) */
	cmpl	crf0,0,a,b
	bc	4,0,*+16

	/* *borrow = 1; */
	li       r7,1
	stw      r7, 0(borrow)
	b        *+12

	/* else *borrow = 0; */
	li       r7,0
	stw      r7, 0(borrow)

	/* return diff in r3 */
	mr.      r3,r6
	blr
}

asm void giantMulDigits(
	register giantDigit dig1,
	register giantDigit dig2,
 	register giantDigit *lowProduct,	/* RETURNED, low digit */
	register giantDigit *hiProduct)		/* RETURNED, high digit */
{
	/* r3 : dig1
	   r4 : dig2
	   r5 : lowProduct
	   r6 : hiProduct */

	/* dprod = (unsigned long long)dig1 * (unsigned long long)dig2; */
	mullw	r7, dig1, dig2	/* r7 = low(dig1 * dig2) */
	mulhwu	r8, dig1, dig2	/* r8 - hi(dig1 * dig2) */

	/* *hiProduct = (giantDigit)(dprod >> GIANT_BITS_PER_DIGIT); */
	stw	r8, 0(hiProduct)

	/* *lowProduct = (giantDigit)dprod; */
	stw	r7, 0(lowProduct)
	blr
}

asm giantDigit VectorMultiply(
	register giantDigit plierDigit,		/* r3 */
	register giantDigit *candVector,	/* r4 */
	register unsigned candLength,		/* r5 */
	register giantDigit *prodVector)	/* r6 */
{
	register unsigned candDex; 			/* index into multiplicandVector */
	register giantDigit lastCarry;
	register giantDigit prodLo;
	register giantDigit prodHi;
	register unsigned scr1;
	register unsigned sumLo;

	fralloc

    /* giantDigit lastCarry = 0; */
	li       lastCarry,0


	/* for(candDex=0; candDex<candLength; ++candDex) { */
	li       candDex,0
	b		_endLoop

	    /*
	     * prod = *(candVector++) * plierDigit + *prodVector + lastCarry
	     */
_topLoop:
		lwz      scr1,0(candVector)				/* *candVector --> scr1 */
		addi     candVector,candVector,4		/* candVector++ */

		mullw	prodLo,scr1,plierDigit	/* prodLo = low(*candVector * plierDigit) */
		mulhwu	prodHi,scr1,plierDigit	/* prodHi = high(*candVector * plierDigit) */

	    /* giantAddDouble(&prodLo, &prodHi, *prodVector); */
		lwz     scr1,0(prodVector)		/* *prodVector --> r9 */
		add		sumLo,prodLo,scr1		/* prodLo + *prodVector --> sumLo */
		cmpl	crf0,0,sumLo,prodLo		/* sumLo < prodLo? */
		bc		12,0,_carry1
		cmpl	crf0,0,sumLo,scr1		/* sumLo < *prodVector? */
		bc		4,0,_noCar1
_carry1:
		addi	prodHi,prodHi,1			/* prodHi++ */
_noCar1:
		mr.		prodLo,sumLo			/* prodLo := sumLo */

	    /* giantAddDouble(&prodLo, &prodHi, lastCarry); */
		add		sumLo,sumLo,lastCarry	/* sumLo += lastCarry */
		cmpl	crf0,0,sumLo,prodLo		/* sumLo < prodLo? */
		bc		12,0,_carry2
		cmpl	crf0,0,sumLo,lastCarry	/* sumLo < lastCarry? */
		bc		4,0,_noCar2
_carry2:
		addi	prodHi,prodHi,1			/* prodHi++ */
_noCar2:
		mr.		prodLo,sumLo			/* prodLo := sumLo */

	    /* *(prodVector++) = prodLo; */
		stw      prodLo,0(prodVector)		/* prodLo --> *prodVector */
		addi     prodVector,prodVector,4	/* prodVector++ */

	    /* lastCarry = prodHi; */
		mr.		lastCarry,prodHi

	/* } */
	addi     candDex,candDex,1			/* candDex++ */
_endLoop:
	cmpl     crf0,0,candDex,candLength	/* candDex < candLength? */
	bc       12,0,_topLoop

	/* return lastCarry; */
	mr.      r3,lastCarry				/* return lastCarry in r3 */
	frfree
	blr
}

#endif	// PPC_GIANT_PORT_INLINE
