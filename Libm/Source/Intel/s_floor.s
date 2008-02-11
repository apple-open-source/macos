/*
 * Written by Ian Ollmann
 * Copyright � 2005, Apple Computer Inc. All rights reserved.
 */

#include <machine/asm.h>
#include "abi.h"

/*
ENTRY( floorl )
    xorl        %ecx,                       %ecx
    fldz                                    // { 0 }
    movw        8+FIRST_ARG_OFFSET(STACKP), %cx
    andw        $0x7fff,                    %cx
	fldt		FIRST_ARG_OFFSET(STACKP)	// { f, 0 }
    cmpw        $(16383+63),                %cx         //test for NaNs, Infs, large integer
    jae         1f
    fucomi      %st(1),     %st(0)                      // test for zeros
    je          1f

    fadd        %st(0),     %st(1)          //{ f, f }

    //do rint
    fistpll    FRAME_SIZE( STACKP )         // { f }
    fildll      FRAME_SIZE( STACKP )        // { rintl(f), f }

	fucomi		%ST(1), %ST					//  test for rintl(f) <= f or NaN
    jbe         1f
	fld1									//{ 1, rintl(f), f }
	fsubrp		%ST(0), %ST(1)				//{ floorl(f), f }
1:  fstp        %st(1)
	ret
*/

ENTRY( floorf )
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
    movl    $0xBf800000,                %edx            // -1.0f
    cvtdq2ps    %xmm2,                  %xmm2
    movd    %edx,                       %xmm1
    cmpltss %xmm2,                      %xmm0
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

    ENTRY( floor )
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
        fistpll     FRAME_SIZE( STACKP )
        fildll      FRAME_SIZE( STACKP )
                                                
        fucomi      %st(1), %st(0)          // { rint(x), x }
        fstp        %st(1)                  // { rint(x) }
        jbe         1f

        fld1                                // { 1, trunc(x) }
        fsubrp      %st(0), %st(1)          // { floor(x) }
            
    1:  ret

#else //x86_64

    ENTRY( floor )
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
        cvtsi2sd    %rax,                   %xmm1
        cmpltsd		%xmm1,                  %xmm0           
        cvtdq2pd    %xmm0,                  %xmm0
        addsd       %xmm1,                  %xmm0
        
    1:  ret

#endif
