
/*
 *	lround.s
 *
 *		by Ian Ollmann
 *
 *	Copyright (c) 2007,  Apple Inc.  All Rights Reserved.
 *
 *	Implementation of C99 lround and llround functions for i386 and x86_64.
 */
 
#include <machine/asm.h>
#include "abi.h"


#if defined( __i386__ )


	ENTRY( llround )
		movl	4+FRAME_SIZE(STACKP),	%eax
		fldl	FRAME_SIZE(STACKP)				//  { x }
		movsd	FRAME_SIZE(STACKP),		%xmm0
		fld		%st(0)							//	{ x, x }
		SUBP	$12,					STACKP

#if defined( __SSE3__ )
		fisttpll	(STACKP)					// { x }, trunc(x), set invalid / inexact if necessary
#else
        fnstcw      8( STACKP )
        movw        8( STACKP ),		%dx
        movw        %dx,                %cx
        orw         $0xc00,             %dx
        movw        %dx,				8( STACKP )
        fldcw       8( STACKP )
		fistpll		(STACKP)					// { x }, trunc(x), set invalid / inexact if necessary
#endif
		fildll	(STACKP)						// { trunc(x), x }
		fucomip %st(1),					%st(0)	// { x }	if( x == trunc(x) || isnan(x) )
		je		1f								//				use the result we already calculated  (avoid setting inexact)

		movl	%eax,					%edx	// x >> 32
		andl	$0x7fffffff,			%eax	// |x >> 32|
		xorl	%eax,					%edx	// signof( x )		
		cmpl	$0x43e00000,			%eax	// |x| >= 0x1.0p63
		jae		2f


		orl		$0x3f000000,			%edx	//	copysign( 0.5f, x )
		movl	%edx,					8(STACKP)
		fadds	8(STACKP)						// { copysign( 0.5f, x ) + x }		//exact due to extra precision. (We dont support the case where someone changes the precsion control bits.)

#if defined( __SSE3__ )
		fisttpll	(STACKP)					// trunc(x), set invalid / inexact if necessary
#else
		fistpll		(STACKP)					// trunc(x), set invalid / inexact if necessary
#endif

		//exit
#if ! defined( __SSE3__ )
		movw	%cx,					8(STACKP)
		fldcw	8(STACKP)
#endif
		movl	(STACKP),				%eax	
		movl	4(STACKP),				%edx
		ADDP	$12,					STACKP
		ret

//		x is an integer or NaN
1:		fstp	%st(0)							// {}
#if ! defined( __SSE3__ )
		movw	%cx,					8(STACKP)
		fldcw	8(STACKP)
#endif
		movl	(STACKP),				%eax	
		movl	4(STACKP),				%edx
		ADDP	$12,					STACKP
		ret

2:		// overflow
		fstp	%st(0)							// {}
		subl	$1,						%edx
		sarl	$31,					%edx
#if ! defined( __SSE3__ )
		movw	%cx,					8(STACKP)
		fldcw	8(STACKP)
#endif
		movl	(STACKP),				%eax	
		xorl	%edx,					%eax
		xorl	4(STACKP),				%edx
		ADDP	$12,					STACKP
		ret

		
#define LONG_MIN_hi		0x41E00000

	ENTRY( lround )
		movsd	FRAME_SIZE(STACKP),		%xmm1
		movapd	%xmm1,					%xmm0
		psrlq	$32,					%xmm1
		movd	%xmm1,					%edx
#elif defined( __x86_64__ )

#define LONG_MIN_hi		0x43E00000


	ENTRY( lround )
	ENTRY( llround )
		movd	%xmm0,					%rdx		// x
		shrq	$32,					%rdx		// x >> 32
#endif

		cvttsd2si	%xmm0,				AX_P		// (long) x, set invalid / inexact if necessary

		cvtsi2sd	AX_P,				%xmm1		// trunc(x)
		ucomisd		%xmm0,				%xmm1		// x == trunc(x) || isnan(x)
		je		1f									//		return (long) x

		MOVP	DX_P,					CX_P		// x >> 32
		and		$0x7fffffff,			DX_P		// |x >> 32 |
		XORP	DX_P,					CX_P		// signof( x )
		cmpl	$LONG_MIN_hi,			%edx		// |x >> 32| >= 0x1.0p63 >> 32
		jae		3f

		orl		$0x3fe00000,			%ecx		// copysign( 0.5, x ) >> 32
		movd	%ecx,					%xmm1		// copysign( 0.5, x ) >> 32
		psllq	$32,					%xmm1		// copysign( 0.5, x )
		pcmpeqb %xmm2,					%xmm2		// -1ULL
		paddq	%xmm1,					%xmm2		// copysign( 0.5 - 1 ulp, x )
		ucomisd	%xmm0,					%xmm2		// |x| == 0.5 - 1 ulp
		je		1f									//		return (long) x

		addsd	%xmm1,					%xmm0		// x += copysign( 0.5, x )
		cvttsd2si %xmm0,				AX_P		// (int) (x + copysign( 0.5, x ) )
#if defined( __i386__ )
		cmpl	$0x80000000,			AX_P
		je		2f
#endif

	1:	ret
			
		// overflow
#if defined( __i386__ )
	2:	andl	$0x80000000,			%ecx
#endif
	3:	SUBP	$1,						CX_P		// x < 0 ? 0x7fffffff : -1
		sar		$31,					CX_P		// x < 0 ? 0 : -1
		XORP	CX_P,					AX_P		// flip LONG_LONG_MIN to LONG_LONG_MAX if needed
		ret
	

