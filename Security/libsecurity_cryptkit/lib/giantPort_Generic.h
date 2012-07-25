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
 * giantPort_Generic.h - Generic giant definitions routines, used when
 *			 no platform-specific version is available.
 *
 * Revision History
 * ----------------
 * 06 Apr 1998	Doug Mitchell at Apple
 *	Created.
 */

#ifndef	_CK_NSGIANT_PORT_GENERIC_H_
#define _CK_NSGIANT_PORT_GENERIC_H_

#include "feeDebug.h"
#include "platform.h"
#include "giantIntegers.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * We'll be using the compiler's 64-bit long long for these routines.
 *
 * Mask for upper word.
 */
#define GIANT_UPPER_DIGIT_MASK	(~(unsigned long long(GIANT_DIGIT_MASK)))

/*
 * Multiple-precision arithmetic routines/macros.
 */

/*
 * Add two digits, return sum. Carry bit returned as an out parameter.
 * This should work any size giantDigits up to unsigned int.
 */
static inline giantDigit giantAddDigits(
	giantDigit dig1,
	giantDigit dig2,
	giantDigit *carry)			/* RETURNED, 0 or 1 */
{
	giantDigit sum = dig1 + dig2;

	if((sum < dig1) || (sum < dig2)) {
	 	*carry = 1;
	}
	else {
		*carry = 0;
	}
	return sum & GIANT_DIGIT_MASK;
}

/*
 * Add a single digit value to a double digit accumulator in place.
 * Carry out of the MSD of the accumulator is not handled.
 * This should work any size giantDigits up to unsigned int.
 */
static inline void giantAddDouble(
	giantDigit *accLow,			/* IN/OUT */
	giantDigit *accHigh,			/* IN/OUT */
	giantDigit val)
{
	giantDigit sumLo = *accLow + val;

	if((sumLo < *accLow) || (sumLo < val)) {
	    (*accHigh)++;
	    #if	FEE_DEBUG
	    if(*accHigh == 0) {
	        CKRaise("giantAddDouble overflow");
	    }
	    #endif	/* FEE_DEBUG */
	}
	*accLow = sumLo;
}

/*
 * Subtract a - b, return difference. Borrow bit returned as an out parameter.
 * This should work any size giantDigits up to unsigned int.
 */
static inline giantDigit giantSubDigits(
	giantDigit a,
	giantDigit b,
	giantDigit *borrow)			/* RETURNED, 0 or 1 */
{
	giantDigit diff = a - b;

	if(a < b) {
		*borrow = 1;
	}
	else {
		*borrow = 0;
	}
	return diff;
}

/*
 * Multiply two digits, return two digits.
 * This should work for 16 or 32 bit giantDigits, though it's kind of
 * inefficient for 16 bits.
 */
static inline void giantMulDigits(
	giantDigit	dig1,
	giantDigit	dig2,
 	giantDigit	*lowProduct,		/* RETURNED, low digit */
	giantDigit	*hiProduct)		/* RETURNED, high digit */
{
	unsigned long long dprod;

	dprod = (unsigned long long)dig1 * (unsigned long long)dig2;
	*hiProduct = (giantDigit)(dprod >> GIANT_BITS_PER_DIGIT);
	*lowProduct = (giantDigit)dprod;
}

/*
 * Multiply a vector of giantDigits, candVector, by a single giantDigit,
 * plierDigit, adding results into prodVector. Returns m.s. digit from
 * final multiply; only candLength digits of *prodVector will be written.
 */
static inline giantDigit VectorMultiply(
	giantDigit plierDigit,
	giantDigit *candVector,
	unsigned candLength,
	giantDigit *prodVector)
{
	unsigned candDex;		// index into multiplicandVector
    	giantDigit lastCarry = 0;
	giantDigit prodLo;
	giantDigit prodHi;

	for(candDex=0; candDex<candLength; ++candDex) {
	    /*
	     * prod = *(candVector++) * plierDigit + *prodVector + lastCarry
	     */
	    giantMulDigits(*(candVector++),
		plierDigit,
		&prodLo,
		&prodHi);
	    giantAddDouble(&prodLo, &prodHi, *prodVector);
	    giantAddDouble(&prodLo, &prodHi, lastCarry);

	    /*
	     * *(destptr++) = prodHi;
	     * lastCarry = prodLo;
	     */
	    *(prodVector++) = prodLo;
	    lastCarry = prodHi;
	}

	return lastCarry;
}

#ifdef __cplusplus
extern "C" {
#endif

#endif	/*_CK_NSGIANT_PORT_GENERIC_H_*/
