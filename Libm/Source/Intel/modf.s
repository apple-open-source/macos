
/*
 *  modf.s
 *
 *      by Ian Ollmann
 *
 *  Copyright (c) 2007 Apple Inc. All Rights Reserved.
 */

#include <machine/asm.h>
#include "abi.h"

#if defined( __i386__ )
    #define RESULT_P        %eax
#else
    #define RESULT_P        %rdi
#endif


ENTRY( modf )
#if defined( __i386__ )
    movsd   FRAME_SIZE( STACKP ),   %xmm0
#endif
    movapd  %xmm0,          %xmm2       // x
    pcmpeqb %xmm1,          %xmm1
    psrlq   $32,            %xmm2
    movd    %xmm2,          %eax        //sign + exponent of x
    psllq   $63,            %xmm1
    andpd   %xmm0,          %xmm1       //signof(x)

    andl    $0x7fffffff,    %eax        //get rid of sign bit
    addl    $((1024-51)<<20),   %eax    //add (1024-51) to exponent. This forces Inf, NaN, Numbers >= 2**52 to be negate
    cmpl    $0x7cc00000,    %eax        // values with exponents less than 1024-51 are fractional numbers only
    jl      1f

    //common case of 1.0 <= x < 2**53
    shrl    $20,            %eax
    subl    $(1023+1024-51), %eax
    movl    $52,            %edx
    subl    %eax,           %edx
    movd    %edx,           %xmm3       //shift value for mask

#if defined( __i386__ )
    movl    8+FRAME_SIZE( STACKP),  RESULT_P
#endif

    pcmpeqb %xmm4,          %xmm4
    psllq   %xmm3,          %xmm4
    andpd   %xmm0,          %xmm4       //trunc(x)
    subsd   %xmm4,          %xmm0       //fractional part

#if defined( __i386__ )
    movsd   %xmm0,          (RESULT_P)
    fldl    (RESULT_P)
#endif    
    movsd   %xmm4,          (RESULT_P)
    ret
    
1:  jae     2f                          //Inf, NaN, big numbers go to 2
    // |x| < 1.0
#if defined( __i386__ )
    movl    8+FRAME_SIZE( STACKP),  RESULT_P
    fldl    FRAME_SIZE(STACKP)
#endif
    movsd   %xmm1,          (RESULT_P)
    ret
    
2:  cmp     $0xbcc00000,    %eax
    ja      3f                          //do NaNs elsewhere
    
    // |x| >= 2**52
#if defined( __i386__ )
    movl    8+FRAME_SIZE( STACKP),  RESULT_P
    movsd   %xmm1,          (RESULT_P)
    fldl    (RESULT_P)
    movsd   %xmm0,          (RESULT_P)
#else
    movsd   %xmm0,          (RESULT_P)
    movapd  %xmm1,          %xmm0
#endif
    ret
    
3:  //NaN
#if defined( __i386__ )
    movl    8+FRAME_SIZE( STACKP),  RESULT_P
    fldl    FRAME_SIZE( STACKP )
#endif
    movsd   %xmm0,          (RESULT_P)
    ret
