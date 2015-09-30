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
 * giantPorti486.h - OpenStep-dependent giant definitions.
 *
 * Revision History
 * ----------------
 * 06 Apr 1998 at Apple
 *	Created.
 */

#ifndef	_CK_NSGIANT_PORT_I486_H_
#define _CK_NSGIANT_PORT_I486_H_

#include "giantIntegers.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Add two digits, return sum. Carry bit returned as an out parameter.
 */
static inline giantDigit giantAddDigits(
	giantDigit dig1,
 	giantDigit dig2,
 	giantDigit *carry) 	/* RETURNED, 0 or 1 */
{
	giantDigit _sum;	/* r/w %0 */
	asm volatile(
		"movl %2, %0	/* _sum = dig1 */	\n"
		"addl %3, %0	/* _sum += dig2 */	\n"
		"jc .+9					\n"
		"movl $0, %1	/* carry = 0 */		\n"
		"jmp .+7				\n"
		"movl $1, %1	/* carry = 1 */		\n"
	    	: "=&r" (_sum), "=&r" (*carry)
		: "r" (dig1), "r" (dig2));
	return _sum;
}

/*
 * Add a single digit value to a double digit accumulator in place.
 * Carry out of the MSD of the accumulator is not handled.
 */
static inline void giantAddDouble(
 	giantDigit *accLow,		/* IN/OUT */
 	giantDigit *accHigh,		/* IN/OUT */
 	giantDigit val)
{
	asm volatile(
		"addl %4, %0	/* accLow += val */	\n"
		"jnc .+3				\n"
		"incl %1	/* accHigh++ */		\n"
	    	: "=&r" (*accLow), "=&r" (*accHigh)
		: "0" (*accLow), "1" (*accHigh), "r" (val));
}

/*
 * Subtract a - b, return difference. Borrow bit returned as an out parameter.
 */
static inline giantDigit giantSubDigits(
 	giantDigit a,
 	giantDigit b,
 	giantDigit *borrow)		/* RETURNED, 0 or 1 */
{
	giantDigit _diff;	/* r/w %0 */
	asm volatile(
		"movl %2, %0	/* _diff = a */		\n"
		"subl %3, %0	/* _diff -= b */	\n"
		"jc .+9					\n"
		"movl $0, %1	/* borrow = 0 */	\n"
		"jmp .+7				\n"
		"movl $1, %1	/* borrow = 1 */	\n"
	    	: "=&r" (_diff), "=&r" (*borrow)
		: "r" (a), "r" (b));
	return _diff;
}

/*
 * Multiply two digits, return two digits.
 */
static inline void giantMulDigits(
 	giantDigit	dig1,
 	giantDigit	dig2,
 	giantDigit	*lowProduct,	// RETURNED, low digit
 	giantDigit	*hiProduct)	// RETURNED, high digit

{
	asm volatile(
		"movl %2, %%eax	/* eax = dig1 */	\n"
		"movl %3, %%edx /* edx = dig2 */	\n"
		"mull %%edx	/* eax *= dig2 */	\n"
	    	: "=&a" (*lowProduct), "=&d" (*hiProduct)
		: "r" (dig1), "r" (dig2)
		: "%eax", "%edx" );
}

/*
 * Multiply a vector of giantDigits, candVector, by a single giantDigit,
 * plierDigit, adding results into prodVector. Returns m.s. digit from
 * final multiply; only candLength digits of *prodVector will be written.
 *
 * This one's implemented in a .s file.
 */
extern giantDigit vectorMult_x86(
	giantDigit plierDigit,
	giantDigit *candVector,
	unsigned candLength,
	giantDigit *prodVector);

#define VectorMultiply(pd, cv, cl, pv) vectorMult_x86(pd, cv, cl, pv)


#ifdef __cplusplus
}
#endif

#endif	_CK_NSGIANT_PORT_I486_H_
