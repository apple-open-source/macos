/*
 *  roundl.s
 *
 *		by Ian Ollmann
 *
 *  Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 *	Implementation for C99 round, lround and llround functions for __i386__ and __x86_64__.
 */

#include "machine/asm.h"

#define LOCAL_STACK_SIZE	12
#include "abi.h"

ENTRY(roundl)
    fldt    FRAME_SIZE( STACKP )                // { x }
    xor     %ecx,   %ecx
    xor     %edx,   %edx
    fldz                                        // { 0, x }
    movw    8+FRAME_SIZE( STACKP ),     %cx
    fucomip %st(1), %st(0)                      // { x }    early out for x == 0.0L
    je      1f
    
    movw    %cx,                        %dx
    andw    $0x7fff,                    %cx
    cmpw    $(16383+63),                %cx
    jae     1f                                  // early out for integer, Inf, NaN
    
    //truncate
    fld     %st(0)                              // { x, x }
    #if defined( __SSE3__ )    
        fisttpll    FRAME_SIZE( STACKP )
        fildll      FRAME_SIZE( STACKP )
    #else
        fnstcw      FRAME_SIZE( STACKP )
        movw        FRAME_SIZE( STACKP ),       %cx
        movw        %cx,                        %dx
        orw         $0xc00,                     %cx
        movw        %cx,                        FRAME_SIZE( STACKP )
        fldcw       FRAME_SIZE( STACKP )
        frndint
        movw        %dx,                        FRAME_SIZE( STACKP )
        fldcw       FRAME_SIZE( STACKP )    
    #endif
                                                // { trunc(x), x }
    fsubr   %st(0),  %st(1)                     // { trunc(x), x - trunc(x) }
    pcmpeqb %xmm0,  %xmm0                       // -1ULL
    fxch                                        // { x - trunc(x), trunc(x) }
    psllq   $63,    %xmm0                       // 0x8000000000000000
    fstpt   FRAME_SIZE( STACKP )                // { trunc(x) }

    movw    8+FRAME_SIZE(STACKP),       %cx    
    andw    $0x7fff,                    %cx
    cmpw    $16382,                     %cx
    jb      1f

    movq    %xmm0,  FRAME_SIZE(STACKP)
    fldt    FRAME_SIZE(STACKP)                  // { copysign( 0.5L, x ), trunc(x) }
    fadd    %st(0), %st(0)                      // { copysign( 1.0L, x ), trunc(x) }
    faddp                                       // { round(x) }
    
1:  ret
    

#if defined( __i386__ )

ENTRY( lroundl )
	movzwl	8+FRAME_SIZE(STACKP),		%eax	//signed exponent
	movswl	%ax,						%edx	// sign extended
	andl	$0x7fff,					%eax	// exponent
	subl	$0x3ffe,					%eax	// push |x| < 0.5 negative
	cmpl	$32,						%eax	// if( |x| < 0.5 || |x| >= 0x1.0p31L || isnan(x) )
	jae		2f									//		goto 2
	
	fldt	FRAME_SIZE( STACKP )				// { x }
	andl	$0x80000000,				%edx	// signof( x )
	orl		$0x3f000000,				%edx	// copysign( 0.5f, x )
	fld		%st(0)								// { x, x }

#if defined( __SSE3__ )
	fisttpl	4+FRAME_SIZE( STACKP )				//	{ x }
#else
	fnstcw  8+FRAME_SIZE( STACKP )
	movw    8+FRAME_SIZE( STACKP ),			%cx
	movw	%cx,							%ax
	orw     $0xc00,							%cx
	movw    %cx,							16( STACKP )
	fldcw   8+FRAME_SIZE( STACKP )
	fistpl	4+FRAME_SIZE( STACKP )			// { x }
#endif

	fildl	4+FRAME_SIZE( STACKP )			// { ltruncl( x ), x }
	fucomip	%st(1), %st(0)					// { x }
	je		1f

	movl	%edx,		FRAME_SIZE( STACKP )
	flds	FRAME_SIZE( STACKP )				// { copysign( 0.5f, x ), x }
	faddp	%st(1), %st(0)						// { x + copysign( 0.5f, x ) }

#if defined( __SSE3__ )
	fisttpl	FRAME_SIZE( STACKP )				//	{ }
#else
	fistpl	FRAME_SIZE( STACKP )			// { x }
	movw	%ax,	8+FRAME_SIZE( STACKP )
	fldcw	8+FRAME_SIZE( STACKP )
#endif

	movl	FRAME_SIZE( STACKP ),		%eax
	ret
	
1:	// integer
	fstp	%st(0)
	movl	4+FRAME_SIZE( STACKP ),		%eax
	ret
	
2:	jl		3f									// if( |x| < 0.5 ) goto 3

	// |x| is large or NaN
	xorl	%eax,						%eax
	fldz										// { 0 }
	fldt	FRAME_SIZE( STACKP )				// { x, 0 }
	fucomi	%st(1), %st(0)						// { x, 0 }	x > 0
	fistpl	FRAME_SIZE( STACKP )				// { 0 }
	fstp	%st(0)								// { }
	seta								%al
	negl								%eax
	xorl	FRAME_SIZE( STACKP ),		%eax
	ret
	

3:	// |x| < 0.5
	fldt	FRAME_SIZE( STACKP )				// {x}
	fistpl	FRAME_SIZE( STACKP )				// { } set inexact
	xorl	%eax,						%eax
	ret

#endif

#if defined( __x86_64__ )
ENTRY( lroundl )
#endif
ENTRY( llroundl )
	movzwl	8+FRAME_SIZE(STACKP),		%eax	//signed exponent
	movswl	%ax,						%edx	// sign extended
	andl	$0x7fff,					%eax	// exponent
	subl	$0x3ffe,					%eax	// push |x| < 0.5 negative
	cmpl	$64,						%eax	// if( |x| < 0.5 || |x| >= 0x1.0p63L || isnan(x) )
	jae		2f									//		goto 2
	
	fldt	FRAME_SIZE( STACKP )				// { x }
	andl	$0x80000000,				%edx	// signof( x )
	orl		$0x3f000000,				%edx	// copysign( 0.5f, x )
	fld		%st(0)								// { x, x }

#if defined( __SSE3__ )
	fisttpll FRAME_SIZE( STACKP )				//	{ x }
#else
	fnstcw  8+FRAME_SIZE( STACKP )
	movw    8+FRAME_SIZE( STACKP ),			%cx
	movw	%cx,							%ax
	orw     $0xc00,							%cx
	movw    %cx,							16( STACKP )
	fldcw   8+FRAME_SIZE( STACKP )
	fistpll	FRAME_SIZE( STACKP )			// { x }
#endif

	fildll	FRAME_SIZE( STACKP )			// { lltruncl( x ), x }
	fucomip	%st(1), %st(0)					// { x }
	je		1f

	movl	%edx,		FRAME_SIZE( STACKP )
	flds	FRAME_SIZE( STACKP )				// { copysign( 0.5f, x ), x }
	faddp	%st(1), %st(0)						// { x + copysign( 0.5f, x ) }

#if defined( __SSE3__ )
	fisttpll	FRAME_SIZE( STACKP )				//	{ }
#else
	fistpll	FRAME_SIZE( STACKP )			// { x }
	movw	%ax,	8+FRAME_SIZE( STACKP )
	fldcw	8+FRAME_SIZE( STACKP )
#endif

#if defined( __i386__ )
	movl	FRAME_SIZE( STACKP ),		%eax
	movl	4+FRAME_SIZE( STACKP ),		%edx
#else
	movq	FRAME_SIZE( STACKP ),		%rax
#endif
	ret
	
1:	// integer
	fstp	%st(0)
#if defined( __i386__ )
	movl	FRAME_SIZE( STACKP ),		%eax
	movl	4+FRAME_SIZE( STACKP ),		%edx
#else
	movq	FRAME_SIZE( STACKP ),		%rax
#endif
	ret
	
2:	jl		3f									// if( |x| < 0.5 ) goto 3

	// |x| is large or NaN
	xor		AX_P,						AX_P
	fldz										// { 0 }
	fldt	FRAME_SIZE( STACKP )				// { x, 0 }
	fucomi	%st(1), %st(0)						// { x, 0 }	x > 0
	fistpll	FRAME_SIZE( STACKP )				// { 0 }
	fstp	%st(0)								// { }
	seta								%al
	neg									AX_P
#if defined( __i386__ )
	movl	4+FRAME_SIZE( STACKP ),		%edx
	xorl	%eax,						%edx
	xorl	FRAME_SIZE( STACKP ),		%eax
#else
	xorq	FRAME_SIZE( STACKP ),		%rax
#endif
	ret
	

3:	// |x| < 0.5
	fldt	FRAME_SIZE( STACKP )				// {x}
	fistpll	FRAME_SIZE( STACKP )				// { } set inexact
#if defined( __i386__ )
	xorl	%edx,						%edx
#endif
	xor		AX_P,						AX_P
	ret
    
	
#if defined ( __i386__ )

    ENTRY( round )
        movsd       FRAME_SIZE( STACKP ),   %xmm0
        fldl        FRAME_SIZE( STACKP )
        xorps       %xmm1,                  %xmm1
        ucomisd     %xmm0,                  %xmm1
        psrlq       $32,                    %xmm0
        je          1f
        movd        %xmm0,                  %ecx
        movl        %ecx,                   %eax
        andl        $0x7fffffff,            %ecx
        cmpl        $0x43300000,            %ecx
        jae         1f

        //truncate
        fld         %st(0)                      // { x, x }
    #if defined( __SSE3__ )    
        fisttpll    FRAME_SIZE( STACKP )
        andl        $0x80000000,            %eax
        orl         $0x3f800000,            %eax
        fildll      FRAME_SIZE( STACKP )
    #else
        fnstcw      FRAME_SIZE( STACKP )
        andl        $0x80000000,            %eax
        orl         $0x3f800000,            %eax
        movw        FRAME_SIZE( STACKP ),       %cx
        movw        %cx,                        %dx
        orw         $0xc00,                     %cx
        movw        %cx,                        FRAME_SIZE( STACKP )
        fldcw       FRAME_SIZE( STACKP )
        frndint
        movw        %dx,                        FRAME_SIZE( STACKP )
        fldcw       FRAME_SIZE( STACKP )    
    #endif
        
        fsubr       %st(0), %st(1)              // { trunc(x), fract }
        fxch                                    // { fract, trunc(x) }
        fstpl       FRAME_SIZE(STACKP)          // { trunc(x) }
        movl        4+FRAME_SIZE(STACKP), %ecx
        andl        $0x3fe00000,        %ecx
        cmpl        $0x3fe00000,        %ecx
        jne         1f
        movl        %eax,   FRAME_SIZE( STACKP )
        flds        FRAME_SIZE(STACKP)          // { copysign( 1.0, x ), trunc(x) }
        faddp                                   // { round(x) }
        
    1:  ret

#else //x86_64

    ENTRY( round )
        movd    %xmm0,                      %rcx
        movq    $-1,                        %rdx            // -1ULL
        movq    $0x43300000,                %rax            
        shrq    $1,                         %rdx            // 0x7fffffffffffffff
        shlq    $32,                        %rax            // 0x1.0p52
        andq    %rdx,                       %rcx            // |x|
        subq    $1,                         %rcx            // push 0 negative
        cmpq    %rax,                       %rcx            // |x| - 1 ulp >= 0x1.0p52?
        jae     1f                                          // if x == 0.0f, x >= 2**23 or x is NaN, skip to the end

        //find trunc(x)
        cvttsd2si   %xmm0,                  %rax
        movl        $0x3ff00000,            %edx
        movl        $0x80000000,            %ecx
		movsd		%xmm0,					%xmm1
        cvtsi2sd    %rax,                   %xmm0           // trunc(x)
        subsd       %xmm0,                  %xmm1           // fraction
        movd        %xmm1,                  %rax
        shrq        $32,                    %rax			// top 32-bits of the fraction
        andl        %eax,                   %ecx			// set aside sign bit
        andl        %edx,                   %eax			// and with 0x3ff00000 to remove sign (mantissa unimportant here)
        cmpl        $0x3fe00000,            %eax			// if the fraction was less than 0.5
        jb          1f										//  then exit

        orl         %ecx,                   %edx			// copysign( 1.0, x ) >> 32
        shlq        $32,                    %rdx			// copysign( 1.0, x )
        movd        %rdx,                   %xmm1			// move to vr
        addsd       %xmm1,                  %xmm0			// round trunc(x) away from zero
        
    1:  ret

#endif


ENTRY( roundf )
#if defined( __i386__ )
    movl    FRAME_SIZE( STACKP ),       %ecx
    movss   FRAME_SIZE( STACKP ),       %xmm0
#else
    movd    %xmm0,                      %ecx
#endif
    movl    %ecx,                       %edx
    andl    $0x7fffffff,                %ecx
    subl    $1,                         %ecx            // push 0 negative
    cmpl    $0x4affffff,                %ecx            
    jae     1f                                          // if x == 0.0f, x >= 2**23 or x is NaN, skip to the end

    cvttps2dq   %xmm0,                  %xmm1           // Find trunc(x) as integer
    andl    $0x80000000,                %edx            // signof(x)
    movl    $0x3f000000,                %ecx            // 0.5f
    cvtdq2ps    %xmm1,                  %xmm1           // trunc(x) as float
    orl     $0x3f800000,                %edx            // copysign( 1.0f, x )
    movd    %ecx,                       %xmm3           // 0.5f

    subss   %xmm1,                      %xmm0           // fract = x - trunc(x):  calculate the fractional part
    movd    %edx,                       %xmm2           // copysign( 1.0f, x )
    andps   %xmm3,                      %xmm0           // |fract| >= 0.5f ? 0.5f : something else 
    pcmpeqd %xmm3,                      %xmm0           // |fract| >= 0.5f ? -1U : 0
    andps   %xmm2,                      %xmm0           // |fract| >= 0.5f ? copysign( 1.0f, x ) : 0
    addss   %xmm1,                      %xmm0           // roundf(x)

#if defined( __i386__ )
    movss       %xmm0,                  FRAME_SIZE( STACKP )
#endif

1:
#if defined( __i386__ )
    flds        FRAME_SIZE( STACKP )
#endif
    ret



