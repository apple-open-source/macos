/*
 *  exp2f.s
 *  LibmV5
 *
 *  Created by Ian Ollmann on 10/4/05.
 *  exp2 polynomial approximation by Ali Sazegari
 *  Copyright 2005 Apple Computer Inc. All rights reserved.
 *
 */

#include <../../i386.subproj/machine/asm.h>
#include "abi.h"

#if defined( __LP64__ )
	#error not 64-bit ready
#endif

#if 0		//this is too error prone currently. Its probably a bug, because it worked in standalone tests.

ENTRY( exp2f )
	movss   4(%esp),	%xmm0
	call	_exp2f$3SSE
	subl	$4,			%esp
	movss	%xmm0,		(%esp)
	flds	(%esp)
	addl	$4,			%esp
	ret

ENTRY(exp2f$3SSE)    
	pushl	$0x4b000000				//0x1.0p23
	pushl	$0x3f800000				//1.0
	pushl	$0x43400000				//192.0
	pushl	$0x3f317214				//0.693147
	pushl	$0x3e75feff				//0.240231
	pushl	$0x3d633f3a				//0.0554802
	pushl	$0x3c1eadbc				//0.00968498
	pushl	$0x3aa252d4				//0.00123843
	pushl	$0x39658674				//0.000218892
	pushl	$0x7e800000				//0x1.0p126
	pushl	$0x00800000				//0x1.0p-126
	
	//calculate e = floor(x)
	//we can cheat a bit here, since values outside of +-150 or so return either inf or zero, so don't have to be correct
	movss	40(%esp),	%xmm1			//load 0x1.0p23
	movss	36(%esp),	%xmm2			//load 1.0
	movaps	%xmm1,		%xmm3			//0x1.0p23
	pslld   $7,			%xmm1			//0x80000000
	ucomiss	%xmm1,		%xmm0			//special case for NaN or 0.0
	movaps  %xmm1,		%xmm7			//0x80000000
	andps	%xmm0,		%xmm1			// sign bit of x
	andnps	%xmm0,		%xmm7			// |x|
	je		__exp2f_nan0
	ucomiss	32(%esp),	%xmm7			// |x| > 192.0 
	movaps	%xmm0,		%xmm5			// copy of x
	orps	%xmm1,		%xmm3			//+-0x1.0p23
	movaps	%xmm0,		%xmm4			// another copy of x

	//if x is infinite, calculating the fractional part will give us a NaN, so weed out large values now
	jnbe		__exp2f_large

	//otherwise find floor(x)
	addss	%xmm3,		%xmm0			// x +- 0x1.0p23
	subss	%xmm3,		%xmm0			// rint(x)
	cmpss	$1,	%xmm0,	%xmm5			// x < rint(x) 
	andps	%xmm2,		%xmm5			// x < rint(x) ? 1.0 : 0.0
	subss	%xmm5,		%xmm0			// e


	//move e to the integer registers
	cvttss2si %xmm0,	%eax			// (int) e
	
	//Calculate the fractional part:  f = x - e
	//and calculate exp2(f).
	subss	%xmm0,		%xmm4			// f
	movaps	%xmm4,		%xmm0			// copy f
	mulss	(%esp),		%xmm0			// f * 0.000218892
	addss	12(%esp),	%xmm0			// f * 0.000218892 + 0.00123843
	mulss	%xmm4,		%xmm0			// (f * 0.000218892 + 0.00123843) * f
	addss	16(%esp),	%xmm0			// 0.00968498 + (f * 0.000218892 + 0.00123843) * f
	mulss	%xmm4,		%xmm0			// (0.00968498 + (f * 0.000218892 + 0.00123843) * f) * f
	addss	20(%esp),	%xmm0			// 0.0554802 + (0.00968498 + (f * 0.000218892 + 0.00123843) * f) * f
	mulss	%xmm4,		%xmm0			// (0.0554802 + (0.00968498 + (f * 0.000218892 + 0.00123843) * f) * f) * f
	addss	24(%esp),	%xmm0			// 0.240231 + (0.0554802 + (0.00968498 + (f * 0.000218892 + 0.00123843) * f) * f) * f
	mulss	%xmm4,		%xmm0			// (0.240231 + (0.0554802 + (0.00968498 + (f * 0.000218892 + 0.00123843) * f) * f) * f) * f
	addss	28(%esp),	%xmm0			// 0.693147 + (0.240231 + (0.0554802 + (0.00968498 + (f * 0.000218892 + 0.00123843) * f) * f) * f) * f
	mulss	%xmm4,		%xmm0			// (0.693147 + (0.240231 + (0.0554802 + (0.00968498 + (f * 0.000218892 + 0.00123843) * f) * f) * f) * f) * f
	addss	%xmm2,		%xmm0			//	exp2(f) = 1.0 + (0.693147 + (0.240231 + (0.0554802 + (0.00968498 + (f * 0.000218892 + 0.00123843) * f) * f) * f) * f) * f
	
	//result = scalbnf( exp2(f), (int) e )
	//start off by figuring out some information about e
	movl	%eax,		%edx			// copy e to edx too
	movl	%eax,		%ecx			// copy e to ecx too
	sarl	$31,		%eax			// e < 0 ? -1 : 0
	xorl	%eax,		%ecx
	subl	%eax,		%ecx			// abs(e)
	movss	4(%esp,%eax,4), %xmm3		// e < 0 ? 0x1.0p-126 : 0x1.0p126
	cmp		$126,		%ecx			// |e| < 126
	jbe		__exp2f_exit
	mulss	%xmm3,		%xmm0			// f *= e < 0 ? 0x1.0p-126 : 0x1.0p126
	movl	%eax,		%ecx
	xorl	$126,		%ecx
	subl	%eax,		%ecx			//+-126
	subl	%ecx,		%edx			// e +- 126

__exp2f_exit:
	addl	$127,		%edx			//bias the exponent. We know this can't overflow/underflow because these cases were sent to __exp2f_large
	shll	$23,		%edx
	movd	%edx,		%xmm5
	mulss	%xmm5,		%xmm0
	addl	$44,		%esp
	ret

__exp2f_large:	//handle large magnitude inputs. Result is either 0 or inf, with overflow or underflow set
	cmpss	$0, %xmm0,	%xmm0			//always true. No NaNs here.
	pslld	$23,		%xmm0			//-inf
	cmpss	$0,	%xmm4,	%xmm0			// x == -inf
	cmpss	$1,	%xmm4,	%xmm2			// 1.0 < e ? -1L : 0
	movss	(%esp),		%xmm3			// 0x1.0p-126
	maxss	%xmm3,		%xmm4
	andnps	%xmm4,		%xmm0
	movaps	%xmm2,		%xmm5			// copy 1.0 < e ? -1L : 0
	pslld	$25,		%xmm2			// 1.0 < e ? 0xfe000000 : 0
	psrld	$1,			%xmm2			// 1.0 < e ? 0x7f000000 : 0
	andnps	%xmm3,		%xmm5			// 1.0 < e ? 0: 0x1.0p-126
	orps	%xmm5,		%xmm2			// 1.0 < e ? 0x7f000000 : 0x00800000
	mulss	%xmm2,		%xmm0
	addl	$44,		%esp
	ret
	
//handle special cases for NaN and 1.0
__exp2f_nan0:
	addss	%xmm2,	%xmm0				//silence NaNs, set xmm0 to 1.0 otherwise
	addl	$44,	%esp
	ret
	
#endif