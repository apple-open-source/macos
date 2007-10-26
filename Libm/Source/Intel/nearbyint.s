
/*
 *  nearbyint.s
 *
 *      by Ian Ollmann
 *
 *  Copyright (c) 2007, Apple Inc.  All Rights Reserved.
 */
 
#include <machine/asm.h>
#include "abi.h"

/*
 *  MXCSR rounding control bits 13-14
 *
 *  00  --  Round to nearest even
 *  01  --  Round to -Inf
 *  10  --  Round to +Inf
 *  11  --  Round to zero
 *
 */

ENTRY( nearbyintf )
    SUBP    $4,                     STACKP
    
    //write the current rounding mode to the stack.  Using the mxcsr instead cost an extra 20 cycles.
    fnstcw  (STACKP) 

//Load the value
#if defined( __i386__ )
    movl    4+FRAME_SIZE( STACKP),    %ecx
    movss   4+FRAME_SIZE( STACKP),    %xmm0
#else
    movd    %xmm0,                  %ecx
#endif


    //Detect special cases (for which no rounding is required) and send them to another path
    movl    %ecx,                   %edx        // x
    andl    $0x7fffffff,            %ecx        // |x|
    xorl    %ecx,                   %edx        // signof( x )
    movd    %edx,                   %xmm2       // move signof(x) to xmm, where it will be needed
    addl    $0x35000000,            %ecx        // add 128-22 to exponent. This forces Inf, NaN, numbers >= 2**23 to be negative
    xorl    %eax,                   %eax        // zero eax. This will get used as a truncation mask if |x| < 1. 
    cmpl    $0x74800000,            %ecx        // values with exponents less than 128-22 are fractional numbers only
    jl      3f                                  // branch for |x| < 1.0f, |x| >= 0x1.0p23f, NaN
    
    //Now we know we have a number 1.0 <= |x| < 2**23

    //create a mask of non-fractional bits to calculate trunc(x)
    shrl    $23,                    %ecx        // extract biased exponent
    subl    $(127+128-22),          %ecx        // unbias the exponent, undo our magic above
    movl    $0xff800000,            %eax        // mask suitable for 1.0f
    sarl    %cl,                    %eax        // extend mask to cover additional bits for exponents > 0
    movd    %eax,                   %xmm1       // move mask to vector domain
    
    //Round. We get here in one of two ways:
    //      fall through from above:    xmm2 has a truncation mask   
    //      jmp from |x| < 1.0:         xmm2 holds the sign of x (stored as +-0.0f), which rounds x to zero of the correct sign in this step
1:  andps   %xmm0,                  %xmm1       // trunc(x)
    ucomiss %xmm1,                  %xmm0       // trunc(x) == x ?
    je      2f                                  // If the value was an integer, we are done, so return x  (satisfies x==0, small integer cases)

    //find our what our rounding mode is
    movl    (STACKP),               %ecx
    andl    $0xC00,                %ecx        // isolate the RC field
    subl    $0x400,                %ecx        // move one of the possible values negative
    cmpw    $0x400,                %cx         // test the RC value
    
    // Rounding modes in eax:
    //      -0x400          Round to nearest even
    //      0               Round to -Inf
    //      0x400           Round to +inf
    //      0x800           Round to zero
    
    jg      4f                                  //Round to zero mode.               Note: signed compare here
    je      5f                                  //Round to +Inf, Go there
    jb      6f                                  //Round to -Inf, Go there.          Note: unsigned compare here
    
    //Round to nearest even is the fall through path, because it is most common
    subss   %xmm1,                  %xmm0       //Get the fractional part.  Exact. 0.0f < |xmm0| < 1.0f with same sign as x.
    movd    %xmm0,                  %ecx        //move fract to integer unit
    orl     $0x3f000000,            %edx        // copysign( 0.5f, x )
    xorl    %edx,                   %ecx        // fract ^ copysign( 0.5f, x ).  Requires some explanation:
                                                //      |fract| > 0.5f  --->   positive denormal
                                                //      |fract| = 0.5f  --->   +0.0f
                                                //      |fract| < 0.5f  --->   positive normal
    jz      7f      // |fract| == 0.5f, figure out which way to round in the comfort and safety of a separate code branch
    subl    $0x00800000,            %ecx        // |fract| > 0.5f ? negative : positive
    sarl    $31,                    %ecx        // |fract| > 0.5f ? -1U : 0
    andl    %ecx,                   %edx        // |fract| > 0.5f ? copysign( 0.5f, x ) : 0.0f
    movd    %edx,                   %xmm0       // copy to vector unit
    addss   %xmm0,                  %xmm0       // |fract| > 0.5f ? copysign( 1.0f, x ) : 0.0f
    addss   %xmm1,                  %xmm0       // correctly rounded result
    
2:  //Done. Return results.
#if defined( __i386__ )
    movss   %xmm0,                  (STACKP)
    flds    (STACKP)
#endif    
    ADDP    $4,                     STACKP
    ret

3:  //exceptional conditions
    jae     2b                                  // NaN, |x| >= 2**23 we are done
    movaps  %xmm2,                  %xmm1
    jmp     1b                                  // |x| < 1.0f skip to rounding
 
4:  //Round to zero
#if defined( __i386__ )
    movss   %xmm1,                  (STACKP)
    flds    (STACKP)
#else
    movss   %xmm1,                  %xmm0
#endif
    ADDP    $4,                     STACKP
    ret
    
5:  //Round to +inf
    subss   %xmm1,                  %xmm0       // Get the fractional part
    xorps   %xmm3,                  %xmm3       // prepare 0.0f
    cmpltss %xmm0,                  %xmm3       // fract > 0.0f 
    cvtdq2ps %xmm3,                 %xmm3       // fract > 0.0f ? -1.0f : 0.0f       (other three fields 0)
    subss   %xmm3,                  %xmm1       // round up
    orps    %xmm2,                  %xmm1       // make sure the sign of the result matches the sign of the input
    
#if defined( __i386__ )
    movss   %xmm1,                  (STACKP)
    flds    (STACKP)
#else
    movss   %xmm1,                  %xmm0
#endif    
    ADDP    $4,                     STACKP
    ret


6:  //Round to -inf
    subss   %xmm1,                  %xmm0       // Get the fractional part
    xorps   %xmm3,                  %xmm3       // prepare 0.0f
    pcmpeqb %xmm4,                  %xmm4       // prepare -1U
    cmpltss %xmm3,                  %xmm0       // fract < 0.0f 
    psrld   $1,                     %xmm4       // 0x7fffffff
    cvtdq2ps %xmm0,                 %xmm0       // fract < 0.0f ? -1.0f : 0.0f       (other three fields 0)
    orps    %xmm4,                  %xmm2       // copysign( 0x7fffffff, x )
    addss   %xmm1,                  %xmm0       // round down
    andps   %xmm2,                  %xmm0       // fix up rounding so that 1.0 - 1.0 doesnt give -0.0
    
#if defined( __i386__ )
    movss   %xmm0,                  (STACKP)
    flds    (STACKP)
#endif    
    ADDP    $4,                     STACKP
    ret


7:  //Round to even nearest, half way case
    shll    $1,                     %eax        //shift the left edge of the truncation mask to the units bit. (Works for 1.0 too, since least significant bit of exponent has odd parity)
    addss   %xmm0,                  %xmm0       // copysign( 1.0, x )       we know the fract is copysign( 0.5f, x ) here
    orl     $0x80000000,            %eax        //make sure the sign bit is still there in the mask. We dont want the sign bit to cause us to think this is odd.
    movd    %eax,                   %xmm3
    pandn   %xmm1,                  %xmm3       //if trunc(x) is odd, this bit will be non-zero (in the +-0.5, case we have +-0.5 here instead of just a bit set )
    pxor    %xmm4,                  %xmm4       // 0
    pcmpeqd %xmm4,                  %xmm3       // trunc(x) is odd ? 0 : -1U
    andnps  %xmm0,                  %xmm3       // trunc(x) is odd ? copysign( 1.0, x ) : 0.0f
    addss   %xmm3,                  %xmm1       // round
#if defined( __i386__ )
    movss   %xmm1,                  (STACKP)
    flds    (STACKP)
#else
    movss   %xmm1,                  %xmm0
#endif    
    ADDP    $4,                     STACKP
    ret
    
    
//---------------------------
    
    
ENTRY( nearbyint )
    SUBP    $(16-FRAME_SIZE),       STACKP
    
    //write the current rounding mode to the stack.  Using the mxcsr instead cost an extra 20 cycles.
    fnstcw  (STACKP) 

//Load the top 32-bits of the value
#if defined( __i386__ )
    movsd   16( STACKP ),           %xmm1
    movapd  %xmm1,                  %xmm0
    psrlq   $32,                    %xmm1
    movd    %xmm1,                  %ecx
#else
    movd    %xmm0,                  %rcx
    shrq    $32,                    %rcx
#endif


    //Detect special cases (for which no rounding is required) and send them to another path
    movl    %ecx,                   %edx        // x
    pcmpeqb %xmm2,                  %xmm2       // -1ULL
    andl    $0x7fffffff,            %ecx        // |x|
    psllq   $63,                    %xmm2       // -0.0
    xorl    %ecx,                   %edx        // signof( x )
    andpd   %xmm0,                  %xmm2       // signof (x) in vector
    addl    $((1024-51)<<20),       %ecx        // add 1024-51 to exponent. This forces Inf, NaN, numbers >= 2**52 to be negative
    movl    $62,                    %eax        // This will get used as a truncation mask if |x| < 1.
    movd    %eax,                   %xmm7       // 62
    cmpl    $0x7cc00000,            %ecx        // values with exponents less than 1024-51 are fractional numbers only
    jl      3f                                  // branch for |x| < 1.0f, |x| >= 0x1.0p52f, NaN
    
    //Now we know we have a number 1.0 <= |x| < 2**52

    //create a mask of non-fractional bits to calculate trunc(x)
    shrl    $20,                    %ecx        // extract biased exponent
    subl    $(1023+1024-51+52),     %ecx        // unbias the exponent, undo our magic above
    negl                            %ecx        // bits to shift -1ULL left to create a truncation mask
    pcmpeqb %xmm1,                  %xmm1       // -1ULL
    movd    %ecx,                   %xmm7       // bits to shift -1ULL left to create a truncation mask
    psllq   %xmm7,                  %xmm1       // left shift -1ULL by number of fractional bits in x
    
    //Round. We get here in one of two ways:
    //      fall through from above:    xmm2 has a truncation mask   
    //      jmp from |x| < 1.0:         xmm2 holds the sign of x (stored as +-0.0f), which rounds x to zero of the correct sign in this step
1:  andpd   %xmm0,                  %xmm1       // trunc(x)
    ucomisd %xmm1,                  %xmm0       // trunc(x) == x ?
    je      2f                                  // If the value was an integer, we are done, so return x  (satisfies x==0, small integer cases)

    //find our what our rounding mode is
    movl    (STACKP),               %ecx
    andl    $0xC00,                 %ecx        // isolate the RC field
    subl    $0x400,                 %ecx        // move one of the possible values negative
    cmpw    $0x400,                 %cx         // test the RC value
    
    // Rounding modes in eax:
    //      -0x400          Round to nearest even
    //      0               Round to -Inf
    //      0x400           Round to +inf
    //      0x800           Round to zero
    
    jg      4f                                  //Round to zero mode.               Note: signed compare here
    je      5f                                  //Round to +Inf, Go there
    jb      6f                                  //Round to -Inf, Go there.          Note: unsigned compare here
    
    //Round to nearest even is the fall through path, because it is most common
    subsd   %xmm1,                  %xmm0       //Get the fractional part.  Exact. 0.0f < |xmm0| < 1.0f with same sign as x.

    //create 0.5
    pcmpeqb %xmm6,                  %xmm6
    psllq   $55,                    %xmm6
    psrlq   $2,                     %xmm6       // 0.5
    
    //find |fract|
    xorpd   %xmm2,                  %xmm0       // |fract|

    //check for |fract| == 0.5
    ucomisd %xmm6,                  %xmm0
    je      7f                      // |fract| == 0.5, figure out which way to round in the comfort and safety of a separate code branch

    cmpltsd %xmm6,                  %xmm0       // |fract| < 0.5 ? -1ULL : 0
    addsd   %xmm6,                  %xmm6       // 1.0
    
    orpd    %xmm2,                  %xmm6       // copysign( 1.0, x )
    andnpd  %xmm6,                  %xmm0       // |fract| < 0.5 ? 0.0 : copysign( 1.0, x )
    addsd   %xmm1,                  %xmm0       // correctly rounded result
    
2:  //Done. Return results.
#if defined( __i386__ )
    movsd   %xmm0,                  (STACKP)
    fldl    (STACKP)
#endif    
    ADDP    $(16-FRAME_SIZE),       STACKP
    ret

3:  //exceptional conditions
    jae     2b                                  // NaN, |x| >= 2**23 we are done
    movapd  %xmm2,                  %xmm1
    jmp     1b                                  // |x| < 1.0f skip to rounding
 
4:  //Round to zero
#if defined( __i386__ )
    movsd   %xmm1,                  (STACKP)
    fldl    (STACKP)
#else
    movsd   %xmm1,                  %xmm0
#endif
    ADDP    $(16-FRAME_SIZE),       STACKP
    ret
    
5:  //Round to +inf
    subsd   %xmm1,                  %xmm0       // Get the fractional part
    xorpd   %xmm3,                  %xmm3       // prepare 0.0f
    cmpltsd %xmm0,                  %xmm3       // fract > 0.0f 
    cvtdq2pd %xmm3,                 %xmm3       // fract > 0.0f ? -1.0 : 0.0       (both fields)
    subsd   %xmm3,                  %xmm1       // round up
    orpd    %xmm2,                  %xmm1       // make sure the sign of the result matches the sign of the input
    
#if defined( __i386__ )
    movsd   %xmm1,                  (STACKP)
    fldl    (STACKP)
#else
    movsd   %xmm1,                  %xmm0
#endif    
    ADDP    $(16-FRAME_SIZE),       STACKP
    ret


6:  //Round to -inf
    subsd   %xmm1,                  %xmm0       // Get the fractional part
    xorpd   %xmm3,                  %xmm3       // prepare 0.0f
    pcmpeqb %xmm4,                  %xmm4       // prepare -1ULL
    cmpltsd %xmm3,                  %xmm0       // fract < 0.0f 
    psrlq   $1,                     %xmm4       // 0x7fffffffffffffff
    cvtdq2pd %xmm0,                 %xmm0       // fract < 0.0f ? -1.0f : 0.0f       (both fields)
    orpd    %xmm4,                  %xmm2       // copysign( 0x7fffffff, x )
    addsd   %xmm1,                  %xmm0       // round down
    andpd   %xmm2,                  %xmm0       // fix up rounding so that 1.0 - 1.0 doesnt give -0.0
    
#if defined( __i386__ )
    movsd   %xmm0,                  (STACKP)
    fldl    (STACKP)
#endif    
    ADDP    $(16-FRAME_SIZE),       STACKP
    ret


7:  //Round to even nearest, half way case
    addsd   %xmm6,                  %xmm6       // 1.0
    pcmpeqb %xmm3,                  %xmm3       // -1ULL
    psubq   %xmm3,                  %xmm7       // add one to the truncation mask shift constant
    psllq   %xmm7,                  %xmm3       // prepare a new truncation mask with left edge past unit bit. (Works for 1.0 too, since least significant bit of exponent has odd parity)
    orpd    %xmm2,                  %xmm6       // copysign( 1.0, x )
    pandn   %xmm1,                  %xmm3       // if trunc(x) is odd, this bit will be non-zero (in the +-0.5, case we have +-0.5 here instead of just a bit set )
    xorpd   %xmm4,                  %xmm4       // 0.0
    pcmpeqd %xmm4,                  %xmm3       // check if each 32-bit chunk is equal to 0. Unforunately there is no 64-bit integer compare. Wed hit denorms here if we use double precision compare.
    pshufd  $0xE1, %xmm3,           %xmm4       // swap both chunks
    pand    %xmm4,                  %xmm3       // trunc(x) is odd ? 0 : -1U    (make sure the other chunk is also equal to 0)
    andnpd  %xmm6,                  %xmm3       // trunc(x) is odd ? copysign( 1.0, x ) : 0.0f
    addsd   %xmm3,                  %xmm1       // round

#if defined( __i386__ )
    movsd   %xmm1,                  (STACKP)
    fldl    (STACKP)
#else
    movsd   %xmm1,                  %xmm0
#endif    
    ADDP    $(16-FRAME_SIZE),       STACKP
    ret
    
//-----------------------

ENTRY( nearbyintl )
    SUBP    $(32-FRAME_SIZE),       STACKP
    
    //write the current rounding mode to the stack.  Using the mxcsr instead cost an extra 20 cycles.
    fnstcw  16(STACKP) 

    fldz                                        // { 0 }
    movq    32(STACKP),             %xmm0       //Load the mantissa
    fldt    32(STACKP)                          // { x, 0 }
    pxor    %xmm1,                  %xmm1       // set our default truncation mask to 0
    xorl    %ecx,                   %ecx
    movw    40(STACKP),             %cx         //Load the signed exponent
    movl    %ecx,                   %edx        // save signed exponent
    movw    %cx,                    8(STACKP)   // write out exponent of truncated value
    andl    $0x8000,                %edx        // sign
    andl    $0x7fff,                %ecx        // biased exponent
    shll    $16,                    %edx        // sign << 16
    addl    $(16384-62),            %ecx        // add 16384-62 to exponent. This forces Inf, NaN, numbers >= 2**63 to be negative
    cmpw    $(16383+16384-62),      %cx         // values with exponents less than 16384-62 are fractional numbers only
    jl      3f                                  // branch for |x| < 1.0L, |x| >= 0x1.0p63L, NaN

    //Now we know we have a number 1.0 <= |x| < 2**63

    fstp    %st(1)                              // { x }
    //create a mask of non-fractional bits to calculate trunc(x)
    subl    $(16383+16384-62+63),   %ecx        // unbias the exponent, undo our magic above
    negl                            %ecx        // bits to shift -1ULL left to create a truncation mask
    pcmpeqb %xmm1,                  %xmm1       // -1ULL
    movd    %ecx,                   %xmm7       // bits to shift -1ULL left to create a truncation mask
    psllq   %xmm7,                  %xmm1       // left shift -1ULL by number of fractional bits in x
    
    //Round. We get here in one of two ways:
    //      fall through from above:    xmm2 has a truncation mask   
    //      jmp from |x| < 1.0:         xmm2 holds the sign of x (stored as +-0.0f), which rounds x to zero of the correct sign in this step
1:  pand    %xmm0,                  %xmm1       // trunc(mantissa)
    movq    %xmm1,                  (STACKP)    
    fldt    (STACKP)                            // { trunc(x), x }
    fucomi  %st(1), %st(0)                      // trunc(x) == x ?
    je      2f                                  // if the value was an integer, we are done, so return x (satisfies x==0, small integer cases )

    //find our what our rounding mode is
    movl    16(STACKP),             %ecx
    andl    $0xC00,                 %ecx        // isolate the RC field
    subl    $0x400,                 %ecx        // move one of the possible values negative
    cmpw    $0x400,                 %cx         // test the RC value
    
    // Rounding modes in eax:
    //      -0x400          Round to nearest even
    //      0               Round to -Inf
    //      0x400           Round to +inf
    //      0x800           Round to zero
    
    jg      2f                                  //Round to zero mode.               Note: signed compare here
    je      4f                                  //Round to +Inf, Go there
    jb      5f                                  //Round to -Inf, Go there.          Note: unsigned compare here
    
    //Round to nearest even is the fall through path, because it is most common
    fsubr   %st(0), %st(1)                      // { trunc(x), fract }

    orl     $0x3f000000,            %edx        // copysign( 0.5f, x )
    movl    %edx,                   16(STACKP)
    flds    16(STACKP)                          // { copysign( 0.5L, x ), trunc(x), fract }
    
    //check for fract == copysign( 0.5f, x )

    fld     %st(2)                              // { fract, copysign( 0.5L, x ), trunc(x), fract }
    fabs                                        // { |fract|, copysign( 0.5L, x ), trunc(x), fract } 
    fld     %st(1)                              // { copysign( 0.5L, x ), |fract|, copysign( 0.5L, x ), trunc(x), fract }
    fabs                                        // { 0.5L, |fract|, copysign( 0.5L, x ), trunc(x), fract }
    fucomip %st(1), %st(0)                      // { |fract|, copysign( 0.5L, x ), trunc(x), fract }        
    fstp    %st(0)                              // { copysign( 0.5L, x ), trunc(x), fract } 
    je      6f                                  // |fract| == 0.5, figure out which way to round in the comfort and safety of a separate code branch

    fldz                                        // { 0.0L, copysign( 0.5L, x ), trunc(x), fract } 
    fcmovb  %st(1), %st(0)                      // { 0.5L < |fract| ? copysign( 0.5L, x ) : 0.0L, copysign( 0.5L, x ), trunc(x), fract }
    fstp    %st(1)                              // { 0.5L < |fract| ? copysign( 0.5L, x ) : 0.0L, trunc(x), fract }
    fadd    %st(0), %st(0)                      // { 0.5L < |fract| ? copysign( 1.0L, x ) : 0.0L, trunc(x), fract }
    faddp                                       // { correctly rounded x, fract }
    
2:  //Done. Return results.
    fstp    %st(1)
    ADDP    $(32-FRAME_SIZE),       STACKP
    ret

3:  //exceptional conditions
    jae     2b                                  // NaN, |x| >= 2**63 we are done

    // |x| < 1.0f skip to rounding
    fstp    %st(1)                              // { x }
    andw    $0x8000,    8(STACKP)               // zero exponent of truncated value, but preserve sign
    jmp     1b                                  
 
    
4:  //Round to +inf                             // { trunc(x), x }
    fsubr   %st(0), %st(1)                      // { trunc(x), fract }
    fldz                                        // { 0.0L, trunc(x), fract }
    fucomi  %st(2), %st(0)                      // { 0.0L, trunc(x), fract }      0.0L < fract ?
    fld1                                        // { 1.0L, 0.0L, trunc(x), fract }
    fcmovnbe %st(1), %st(0)                     // { 0.0L or 1.0L, 0.0L, trunc(x), fract }
    fstp    %st(3)                              // { 0.0L, trunc(x), 0.0L or 1.0L }
    fstp    %st(0)                              // { trunc(x), 0.0L or 1.0L }
    fadd    %st(1), %st(0)                      // { correctly rounded x, 0.0L or 1.0L }
    
    //we might have the wrong sign if we were small negative number that just rounded up. Waste 30 cycles patching up a sign not required by C99 standard.
    fldz                                        // { 0.0L, correctly rounded x, 0.0L or 1.0L }
    fchs                                        // { -0.0L, correctly rounded x, 0.0L or 1.0L }
    fucomi  %st(2), %st(0)                      // { -0.0L, correctly rounded x, 0.0L or 1.0L }   -0.0L == 0.0L or 1.0L ?  Can only be -0.0 if second term is 1.0L. 
    fcmovne %st(2), %st(0)                      // { +-0.0L, correctly rounded x, 0.0L or 1.0L }
    fstp    %st(2)                              // { correctly rounded x, +-0.0L }
    fucomi  %st(1), %st(0)                      // { correctly rounded x, +-0.0L }     correctly rounded x == +-0.0L  Only use the +-0 result if the result is 0
    fcmove  %st(1), %st(0)                      // { correctly rounded correctly signed x, +-0.0L }
    fstp    %st(1)                              // { correctly rounded correctly signed x }

    ADDP    $(32-FRAME_SIZE),       STACKP
    ret


5:  //Round to -inf                             // { trunc(x), x }
    fsubr   %st(0), %st(1)                      // { trunc(x), fract }
    fldz                                        // { 0.0L, trunc(x), fract }
    fucomi  %st(2), %st(0)                      // { 0.0L, trunc(x), fract }      0.0L > fract ?
    fld1                                        // { 1.0L, 0.0L, trunc(x), fract }
    fcmovb  %st(1), %st(0)                      // { 0.0L or 1.0L, 0.0L, trunc(x), fract }
    fstp    %st(3)                              // { 0.0L, trunc(x), 0.0L or 1.0L }
    fstp    %st(0)                              // { trunc(x), 0.0L or 1.0L }
    fsub    %st(1), %st(0)                      // { correctly rounded x, 0.0L or 1.0L }

    //we might have the wrong sign if we were small positive number that just rounded down. Waste 30 cycles patching up a sign not required by C99 standard.
    fldz                                        // { 0.0L, correctly rounded x, 0.0L or 1.0L }
    fxch    %st(2)                              // { 0.0L or 1.0L, correctly rounded x, 0.0L }
    fchs                                        // { -0.0L or -1.0L, correctly rounded x, 0.0L }
    fucomi  %st(2), %st(0)                      // { -0.0L or -1.0L, correctly rounded x, 0.0L }   -0.0L == 0.0L or 1.0L ?  Can only be -0.0 if second term is 1.0L. 
    fcmovne %st(2), %st(0)                      // { +-0.0L, correctly rounded x, 0.0L }
    fstp    %st(2)                              // { correctly rounded x, +-0.0L }
    fucomi  %st(1), %st(0)                      // { correctly rounded x, +-0.0L }     correctly rounded x == +-0.0L  Only use the +-0 result if the result is 0
    fcmove  %st(1), %st(0)                      // { correctly rounded correctly signed x, +-0.0L }
    fstp    %st(1)                              // { correctly rounded correctly signed x }

    ADDP    $(32-FRAME_SIZE),       STACKP
    ret


6:  //Round to even nearest, half way case      // { copysign( 0.5L, x ), trunc(x), fract }
    addl    $0x20000000,            %edx        // copysign( 0x1.0p63f, x )
    movl    %edx,                   16(STACKP)  // copysign( 0x1.0p63f, x )
    flds    16(STACKP)                          // { copysign( 0x1.0p63L, x ), copysign( 0.5L, x ), trunc(x), fract }
    fadd    %st(2), %st(0)                      // { trunc(x) + copysign( 0x1.0p63L, x ), copysign( 0.5L, x ), trunc(x), fract }
    fstpt   (STACKP)                            // { copysign( 0.5L, x ), trunc(x), fract }
    fstp    %st(0)                              // { trunc(x), fract }
    fxch                                        // { fract, trunc(x) }
    fadd    %st(0), %st(0)                      // { copysign( 1.0L, x ), trunc(x) }
    movl    $1,                     %eax        //
    fldz                                        // { 0, copysign( 1.0L, x ), trunc(x) }
    andl    (STACKP),               %eax        // (mantissa & 1ULL) == 0
    fcmovne %st(1), %st(0)                      // { rounding value, copysign( 1.0L, x ), trunc(x) }
    fstp    %st(1)                              // { rounding value, trunc(x) }
    faddp                                       // { result }
    
    //There are no cases where trunc(x) can have the wrong sign. Moving the value away from zero should not cause a change in sign
    // so there are no signs to patch up here.

    ADDP    $(32-FRAME_SIZE),       STACKP
    ret
    

