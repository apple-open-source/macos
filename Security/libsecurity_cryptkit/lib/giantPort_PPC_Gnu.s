/* 
 * giantPort_X_PPC.s - PPC/OS X giant port module
 *
 * Created 3/19/2001 by Doug Mitchell. 
 */
 
/*
 * As of 3/19/2001, using this module results in no change in runtime 
 * performance compared to using the inline C functions in 
 * giantPort_Generic.h. Examination of the compiled code shows that
 * the GNU C compiler, when configured for -O2, generates almost
 * exactly the same code as we have here. 
 * We'll leave this code in, to protect against changes in gcc, changes
 * in CFLAGS, and to serve as an example for other PPC implementations. 
 */
 
#if		defined(__ppc__) && defined(__MACH__)

/*********************************************

Add two digits, return sum. Carry bit returned as an out parameter.

giantDigit giantAddDigits(
	register giantDigit dig1,
	register giantDigit dig2,
	register giantDigit *carry)	...RETURNED, 0 or 1 
**********************************************/
 .text
	.align 2
.globl _giantAddDigits
_giantAddDigits:
	/*
	 * dig1  : r3
	 * dig2  : r4
	 * carry : r5
	 * sum   : r6
	 */

	/* sum = dig1 + dig2 */
	add	r6, r3, r4;

	/* if((sum < dig1) || (sum < dig2)) */
	cmplw	cr0,r6,r3
	blt	L1
	cmplw	cr0,r6,r4
	bge	L2
	
L1:
	/* *carry = 1; */
	li	r7,1
	stw	r7, 0(r5)
	b	L3

L2:
	/* else *carry = 0; */
	li	r7,0
	stw	r7, 0(r5)

L3:
	/* return sum in r3 */
	mr.	r3,r6
	blr

/*********************************************
 
Add a single digit value to a double digit accumulator in place.
Carry out of the MSD of the accumulator is not handled.

void giantAddDouble(
	giantDigit *accLow,			-- IN/OUT 
	giantDigit *accHigh,		-- IN/OUT
	giantDigit val);
**********************************************/

	.align 2
.globl _giantAddDouble
_giantAddDouble:
	/*
	 * r3 : accLow
	 * r4 : accHi
	 * r5 : val
	 * r6 : sumLo
	 * r7 : *accLow
	 */

	/* giantDigit sumLo = *accLow + val; */
	lwz	r7,0(r3)
	add	r6,r7,r5

	/* if((sumLo < *accLow) || (sumLo < val)) { */
	cmplw	cr0,r6,r7
	blt 	L10
	cmplw 	cr0,r6,r5
	bge  	L11

L10:
	/* (*accHigh)++; */
	lwz	r7, 0(r4)
	addi	r7,r7,1
	stw	r7, 0(r4)

L11:
	/* *accLow = sumLo; */
	stw		r6,0(r3)
	blr

/*****************************************************************************

Subtract a - b, return difference. Borrow bit returned as an out parameter.

giantDigit giantSubDigits(
	giantDigit a,
	giantDigit b,
	giantDigit *borrow)		-- RETURNED, 0 or 1 
	
******************************************************************************/

	.align 2
.globl _giantSubDigits
_giantSubDigits:

	/* a  : r3
	   b  : r4
	   borrow : r5
	   diff   : r6 */

	/* giantDigit diff = a - b; */
	subf	r6, r4, r3;

	/* if(a < b) */
	cmplw	cr0,r3,r4
	bge		L20

	/* *borrow = 1; */
	li       r7,1
	stw      r7, 0(r5)
	b        L21

L20:
	/* else *borrow = 0; */
	li       r7,0
	stw      r7, 0(r5)

L21:
	/* return diff in r3 */
	mr.      r3,r6
	blr

/*****************************************************************************

Multiply two digits, return two digits.

void giantMulDigits(
	giantDigit	dig1,
	giantDigit	dig2,
 	giantDigit	*lowProduct,	-- RETURNED, low digit
	giantDigit	*hiProduct)		-- RETURNED, high digit 
	
******************************************************************************/

	.align 2
.globl _giantMulDigits
_giantMulDigits:

	/* r3 : dig1
	   r4 : dig2
	   r5 : lowProduct
	   r6 : hiProduct */

	/* dprod = (unsigned long long)dig1 * (unsigned long long)dig2; */
	mullw	r7, r3, r4		/* r7 = low(dig1 * dig2) */
	mulhwu	r8, r3, r4	/* r8 - hi(dig1 * dig2) */

	/* *hiProduct = (giantDigit)(dprod >> GIANT_BITS_PER_DIGIT); */
	stw	r8, 0(r6)

	/* *lowProduct = (giantDigit)dprod; */
	stw	r7, 0(r5)
	blr


/*****************************************************************************

Multiply a vector of giantDigits, candVector, by a single giantDigit,
plierDigit, adding results into prodVector. Returns m.s. digit from
final multiply; only candLength digits of *prodVector will be written.

giantDigit VectorMultiply(
	giantDigit plierDigit,
	giantDigit *candVector,
	unsigned candLength,
	giantDigit *prodVector)

******************************************************************************/

/* 
 * Register definitions
 * Input paramters:
 */
#define plierDigit	r3
#define candVector	r4
#define candLength 	r5
#define prodVector	r6
	
/*
 * PPC ABI specifies:
 *    r3..r10 for parameter passing
 *    r11, r12 volatile (caller saved, we can write)
 *
 * We'll use the remainder of the registers normally used for parameter passing
 * and also the other volatile register for local variables.
 */
#define candDex		r7
#define lastCarry	r8
#define prodLo		r9
#define prodHi		r10
#define scr1		r11
#define sumLo		r12

	.align 2
.globl _VectorMultiply
_VectorMultiply:

    /* giantDigit lastCarry = 0; */
	li       lastCarry,0


	/* for(candDex=0; candDex<candLength; ++candDex) { */
	li       candDex,0
	b		L_endLoop

	    /*
	     * prod = *(candVector++) * plierDigit + *prodVector + lastCarry
	     */
L_topLoop:
		lwz      scr1,0(candVector)				/* *candVector --> scr1 */
		addi     candVector,candVector,4		/* candVector++ */

		mullw	prodLo,scr1,plierDigit	/* prodLo = low(*candVector * plierDigit) */
		mulhwu	prodHi,scr1,plierDigit	/* prodHi = high(*candVector * plierDigit) */

	    /* giantAddDouble(&prodLo, &prodHi, *prodVector); */
		lwz     scr1,0(prodVector)		/* *prodVector --> r9 */
		add		sumLo,prodLo,scr1		/* prodLo + *prodVector --> sumLo */
		cmplw	cr0,sumLo,prodLo		/* sumLo < prodLo? */
		blt		L_carry1
		cmplw	cr0,sumLo,scr1			/* sumLo < *prodVector? */
		bge		L_noCar1
L_carry1:
		addi	prodHi,prodHi,1			/* prodHi++ */
L_noCar1:
		mr.		prodLo,sumLo			/* prodLo := sumLo */

	    /* giantAddDouble(&prodLo, &prodHi, lastCarry); */
		add		sumLo,sumLo,lastCarry	/* sumLo += lastCarry */
		cmplw	cr0,sumLo,prodLo		/* sumLo < prodLo? */
		blt		L_carry2
		cmplw	cr0,sumLo,lastCarry	/* sumLo < lastCarry? */
		bge		L_noCar2
L_carry2:
		addi	prodHi,prodHi,1			/* prodHi++ */
L_noCar2:
		mr.		prodLo,sumLo			/* prodLo := sumLo */

	    /* *(prodVector++) = prodLo; */
		stw      prodLo,0(prodVector)		/* prodLo --> *prodVector */
		addi     prodVector,prodVector,4	/* prodVector++ */

	    /* lastCarry = prodHi; */
		mr.		lastCarry,prodHi

	/* } */
	addi     candDex,candDex,1			/* candDex++ */
L_endLoop:
	cmplw    cr0,candDex,candLength		/* candDex < candLength? */
	blt      L_topLoop

	/* return lastCarry; */
	mr.      r3,lastCarry				/* return lastCarry in r3 */
	blr

#endif	/* defined(__ppc__) && defined(__MACH__) */
