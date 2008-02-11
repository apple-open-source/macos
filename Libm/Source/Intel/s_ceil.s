/*
 * Written by Ian Ollmann
 *
 * Copyright © 2005, Apple Computer.  All Rights Reserved.
 */

#include <machine/asm.h>
#include "abi.h"

//We play a few games with the sign here to get the sign of ceil( -1 < x < 0 ) to come out right
/*
ENTRY( ceill )
    xorl        %ecx,                       %ecx
    movw        8+FIRST_ARG_OFFSET(STACKP), %cx
    andw        $0x7fff,                    %cx
	fldt		FIRST_ARG_OFFSET(STACKP)	// { f }
    cmpw        $(16383+63),                %cx         //test for NaNs, Infs, large integer
    jae         1f
    fldz                                    // { 0, f }
    fucomip      %st(1),     %st(0)                      // test for zeros
    je          1f

    fld         %st(0)                      //{ f, f }

    //do rint
    fistpll     FRAME_SIZE( STACKP )        // { f }
    fildll      FRAME_SIZE( STACKP )        // { rintl(f), f }
    fxch                                    // { f, rintl(f) }
	fucomip		%ST(1), %ST					//  test for f <= rintl(f) or NaN
    jbe         1f
	fld1									//{ 1, rintl(f) }
	faddp		%ST(0), %ST(1)				//{ correct }
1:	ret
*/
/*
ENTRY(ceilf)
#if defined( __LP64__ )
    SUBP        $4, STACKP
    movss       %xmm0, (STACKP)
    flds        (STACKP)
#else
	flds		FIRST_ARG_OFFSET(STACKP)	//{ f }
#endif
	fld			%ST(0)						//{ f, f }
	frndint									//{ rounded, f }
	fucomi		%ST(1), %ST					//  test for rounded > f
	fldz									//{ 0, rounded, f } 
	fld1									//{ 1, 0, rounded, f }
	fchs									//{ -1, 0, rounded, f }
	fcmovnb		%ST(1), %ST(0)				//{ 0 or -1, 0, rounded, f }
	fsubp		%ST(0), %ST(2)				//{ 0, (0 or -1) - rounded, f }
	fucomip		%ST(2), %ST					//{ (0 or -1) - rounded, f }
	fchs									//{ -((0 or -1) - rounded), f }
	fxch									//{ f, rounded - (0 or 1) }
	fcmovne		%ST(1), %ST(0)				//{ correct, rounded - (0 or 1)}
	fstp		%ST(1)
#if defined( __LP64__ )
    fstps        (STACKP)
    movss       (STACKP), %xmm0
    ADDP        $4, STACKP
#endif
	ret
*/


ENTRY( ceilf )
#if defined( __i386__ )
    movl    FRAME_SIZE( STACKP ),       %ecx
    movss   FRAME_SIZE( STACKP ),       %xmm0
#else
    movd    %xmm0,                      %ecx
#endif
    andl    $0x7fffffff,                %ecx            // |x|
    subl    $1,                         %ecx            // subtract 1. This forces |+-0| to -0
    cmpl    $0x4afffffe,                %ecx            // values >= 0x4b000000 - 1 are either integers, NaN or Inf
    ja      1f                                          // unsigned compare adds 0 to the list

    cvttps2dq   %xmm0,                  %xmm2
    movl    $0x3f800000,                %edx            // 1.0f
    cvtdq2ps    %xmm2,                  %xmm2
    movd    %edx,                       %xmm1
    cmpnless %xmm2,                      %xmm0
    andps   %xmm1,                      %xmm0
    addss   %xmm2,                      %xmm0
#if defined( __i386__ )
    movss   %xmm0,                      FRAME_SIZE( STACKP )
#endif
    
1:
#if defined( __i386__ )
    flds    FRAME_SIZE( STACKP )
#endif
    ret



#if defined ( __i386__ )

    ENTRY( ceil )
        movsd       FRAME_SIZE( STACKP ),   %xmm0
        fldl        FRAME_SIZE( STACKP )
        xorps       %xmm1,                  %xmm1
        ucomisd     %xmm0,                  %xmm1
        psrlq       $32,                    %xmm0
        je          1f
        movd        %xmm0,                  %ecx
        andl        $0x7fffffff,            %ecx
        cmpl        $0x43300000,            %ecx
        jae         1f

        //rint(f)
        fld         %st(0)                      // { x, x}
        fistpll     FRAME_SIZE( STACKP )        // { x }
        fildll      FRAME_SIZE( STACKP )        // { rint(x), x }
                                                
        fucomi      %st(1), %st(0)          // { rint(x), x }
        fstp        %st(1)                  // { rint(x) }
        jae         1f

        fld1                                // { 1, rint(x) }
        faddp      %st(0), %st(1)           // { ceil(x) }
            
    1:  ret

#else //x86_64

    ENTRY( ceil )
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
        movapd      %xmm0,                  %xmm1
        cvtsi2sd    %rax,                   %xmm0
        cmpnlesd    %xmm0,                  %xmm1           
        cvtdq2pd    %xmm1,                  %xmm1
        subsd       %xmm1,                  %xmm0
        
    1:  ret

#endif


