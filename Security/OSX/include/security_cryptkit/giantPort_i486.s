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
 * giantPorti486.s - i486-specific assembly routines.
 *
 * Revision History
 * ----------------
 * 17 Apr 1998 at Apple
 *	Created.
 */

#if defined (i386) || defined(__i386__)
.text

/*
 * Multiply a vector a giantDigits, candVector, by a single giantDigit,
 * plierDigit, adding results in prodVector.
 *
 * void VectorMultiply(
 *	giantDigit plierDigit,
 *	giantDigit *candVector,
 *	unsigned candLength,
 *	giantDigit *prodVector)
 */

.align 2,0x90
.globl _vectorMult_x86

/*
 * Stack locations, relative to adjusted bp.
 */
#define LOCAL_SPACE		0x4

#define ARG_START		(LOCAL_SPACE + 8)	/* rtn ptr plus bp */
#define ARG_PLIER_DIGIT		(ARG_START + 0)
#define ARG_CAND_VECTOR		(ARG_START + 4)		/* cached in ecx */
#define ARG_CAND_LENGTH		(ARG_START + 8)
#define ARG_PROD_VECTOR		(ARG_START + 12)	/* cached in esi */

#define LOCAL_START		(0)
#define LOC_CAND_DEX		(LOCAL_START + 0)  /* index into candVector */

/*
 * ebx : lastCarry
 * esi : prodVector
 * ecx : candVector
 */
_vectorMult_x86:

	pushl   %ebp
	subl    $LOCAL_SPACE,%esp
	movl    %esp,%ebp
	pushl   %edi
	pushl   %esi
	pushl   %ebx

	/* esp not used again 'til we pop these off stack */

	/* prodVector = %esi */
	movl    ARG_PROD_VECTOR(%ebp),%esi

	/* lastCarry = 0 */
	/* 0 --> candDex in 0xf0(%ebp) */
	xorl    %ebx,%ebx

	/* candVector --> %ecx */
	movl    ARG_CAND_VECTOR(%ebp),%ecx

	/* for(candDex=0; candDex<candLength; ++candDex) */
	movl    $0, LOC_CAND_DEX(%ebp)

	/* make sure candLenth > 0 to start...*/
	cmpl    %ebx,ARG_CAND_LENGTH(%ebp)
	jbe     _loopEnd

_loopTop:
	/* branch back to top of for loop */

	/* *candVector--> %eax */
	movl    (%ecx),%eax

	/* candVector++ */
	addl    $0x04,%ecx

	/* plierDigit --> %edx */
	movl    ARG_PLIER_DIGIT(%ebp),%edx

	/* eax = *candVector
	 * edx = plierDigit
	 * edx:eax := (plierDigit * *candVector) */
	mull    %edx

	/* from here to end of loop:
	   prodLo : eax
	   prodHi : edx */

	/* prodLo += *prodVector */
	addl    (%esi),%eax

	/* add carry to hi digit */
	adc	$0,%edx

	/* prodLo += lastCarry */
	addl    %ebx,%eax
	/* add carry to hi digit */
	adc	$0,%edx

	/* *(prodVector++) = prodLo; */
	movl    %eax,(%esi)
	addl    $0x04,%esi

	/* lastCarry = prodHi */
	movl	%edx, %ebx

	/* candDex++ */
	incl    LOC_CAND_DEX(%ebp)

	/* top of loop if candDex < candLength */
	movl    ARG_CAND_LENGTH(%ebp),%eax
	cmpl    %eax,LOC_CAND_DEX(%ebp)
	jb      _loopTop

_loopEnd:
	/* out of for loop */
	/* *prodVector += lastCarry; */
#if	0
	addl    %ebx,(%esi)

	/* return carry from last addition */
	xorl	%eax,%eax
	adc	$0,%eax
#else
	/* return lastCarry */
	movl	%ebx,%eax
#endif
	popl   	%ebx
	popl   	%esi
	popl   	%edi
	addl	$LOCAL_SPACE,%esp
	popl   	%ebp
	ret
#endif /* i386 */
