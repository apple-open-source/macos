/*
 *  truncl.s
 *  LibmV5
 *
 *  Created by Ian Ollmann on 9/1/05.
 *  Copyright 2005 Apple Computer. All rights reserved.
 *
 */

#include "machine/asm.h"

#define LOCAL_STACK_SIZE 12
#include "abi.h"


ENTRY(truncl)
    fldt    FRAME_SIZE( STACKP )				// { x }
    movzwl  8+FRAME_SIZE( STACKP ),     %ecx
	movswl	%cx,						%edx
    andl    $0x7fff,                    %ecx
	subl	$0x3fff,					%ecx	// push |x| < 1.0L negative
    cmpl    $63,						%ecx
    jae     1f
    
#if defined( __SSE3__ )    
    fisttpll FRAME_SIZE( STACKP )
    fildll   FRAME_SIZE( STACKP )
#else
    fnstcw  FRAME_SIZE( STACKP )
    movw    FRAME_SIZE( STACKP ),       %cx
    movw    %cx,                        %dx
    orw     $0xc00,                     %cx
    movw    %cx,                        FRAME_SIZE( STACKP )
    fldcw   FRAME_SIZE( STACKP )
    frndint
    movw    %dx,                        FRAME_SIZE( STACKP )
    fldcw   FRAME_SIZE( STACKP )    
#endif
	ret

1:	// |x| < 1.0L  || |x| > 0x1.0p63L || isnan( x )
	jg		2f

	// |x| < 1.0L, return 0 of same sign
	fistpl	FRAME_SIZE( STACKP )				// set inexact if necessary
	andl	$0x80000000,				%edx
	movl	%edx,		FRAME_SIZE( STACKP )
	flds	FRAME_SIZE( STACKP )

2:	ret



ENTRY( truncf )
#if defined( __i386__ )
    movl    FRAME_SIZE( STACKP ),       %ecx
    movss   FRAME_SIZE( STACKP ),       %xmm0
#else
	movd	%xmm0,						%ecx
#endif
    andl    $0x7f800000,                %ecx
    cmpl    $0x4b000000,				%ecx			// if( |x| >= 0x1.0p23 || isnan(x) )	
    jae     1f                                          //		goto 1

	// |x| < 0x1.0p23
	cvttss2si	%xmm0,					%ecx			// convert to int (round to zero )
	psrld		$31,					%xmm0			// isolate sign of x
	pslld		$31,					%xmm0			// move it back into position
	cvtsi2ss	%ecx,					%xmm1			// convert (int) x back to float
	orps		%xmm1,					%xmm0			// copy the sign back in
#if defined( __i386__ )
	movss		%xmm0,			FRAME_SIZE( STACKP )	// push the result to the stack
#endif
	
1:
#if defined( __i386__ )
	flds		FRAME_SIZE( STACKP )
#endif
	ret


#if defined ( __i386__ )

    ENTRY( trunc )
        movl		4+FRAME_SIZE( STACKP ), %ecx
        fldl        FRAME_SIZE( STACKP )
        andl        $0x7ff00000,            %ecx			//extract exponent
		subl		$0x3ff00000,			%ecx			//subtract bias	
        cmpl        $(0x43300000-0x3ff00000), %ecx			// if( isnan(x) || |x| >= 0x1.0p52 || |x| < 1.0 )
        ja			1f										//		goto 1

        //truncate
    #if defined( __SSE3__ )    
        fisttpll    FRAME_SIZE( STACKP )
        fildll      FRAME_SIZE( STACKP )
    #else
		movsd		FRAME_SIZE( STACKP ),	%xmm0
		shrl		$20,					%ecx			// exponent
		movl		$52,					%edx			// 52
		subl		%ecx,					%edx			// 52 - exponent
		movd		%edx,					%xmm2			// 52 - exponent
		pcmpeqb		%xmm1,					%xmm1			// -1ULL
		psllq		%xmm2,					%xmm1			// -1ULL << (52 - exponent)
		andpd		%xmm0,					%xmm1			//	trunc( x )
		subsd		%xmm1,					%xmm0			//  x - trunc(x)
		cvttsd2si	%xmm0,					%ecx			//  set inexact as needed
		movsd		%xmm1,					FRAME_SIZE(STACKP)
		fstp		%st(0)
		fldl		FRAME_SIZE( STACKP )
    #endif
        ret
		
		// isnan(x) || |x| >= 0x1.0p52 || |x| < 1.0
    1:  cmpl        $(0x7ff00000-0x3ff00000), %ecx			// if( isnan(x) || |x| >= 0x1.0p52 )
		jbe			2f										//		goto 2
		
		// |x| < 1.0
		movsd		FRAME_SIZE(STACKP),			%xmm0
		cvttsd2si	%xmm0,						%ecx				// set inexact as necessary
		pcmpeqb		%xmm1,						%xmm1				// -1ULL
		psllq		$63,						%xmm1				// 0x8000000000000000
		andpd		%xmm1,						%xmm0				//	trunc(x)
		movsd		%xmm0,						FRAME_SIZE(STACKP)	
		fstp		%st(0)
		fldl		FRAME_SIZE(STACKP)
	2:	ret

#else //x86_64

	ENTRY( trunc )
		movd	%xmm0,						%rax
		shrq	$52,						%rax			// extract exponent
		andl	$0x7ff,						%eax			// remove sign
		subl	$0x3ff,						%eax			// remove bias
		cmpl	$(0x433-0x3ff),				%eax			// if( |x| < 1.0 || |x| >= 1.0p52 || isnan(x) )
		ja		1f

		// 1.0 <= |x| < 0x1.0p52
        cvttsd2si   %xmm0,                  %rax
        cvtsi2sd    %rax,                   %xmm0
		ret

.align 4
		// |x| < 1.0 || |x| >= 1.0p52 || isnan(x)
1:		cmpl	$(0x7ff-0x3ff),				%eax
		jbe		2f
		
		// |x| < 1.0
		cvttsd2si	%xmm0,					%eax			// set inexact as necessary
		//return appropriate 0
		psrlq	$63,						%xmm0
		psllq	$63,						%xmm0
		
2:		ret

#endif
