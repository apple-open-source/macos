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

.literal8
zero:           .long   0,          0x80000000      // { 0.0f, -0.0f }
one:            .long   0x3f800000, 0xbf800000      // { 1.0f, -1.0f }
large:          .long   0x5f000000, 0xdf000000      // { 0x1.0p63, -0x1.0p63 }

.literal16
explicitBit:    .quad   0x8000000000000000,     0
roundMask62:    .quad   0xFFFFFFFFFFFFFFFE,     0

.text
#if defined( __x86_64__ )
ENTRY( roundl )
    movzwq  8+FRAME_SIZE( STACKP ),     %rdx    // sign + biased exponent
    movq    FRAME_SIZE( STACKP ),       %rax    // mantissa
    fldt    FRAME_SIZE( STACKP )                // {x}
    movq    %rdx,                       %r8     // sign + biased exponent
    andq    $0x7fff,                    %rdx    // exponent + bias
    shrq    $15,                        %r8     // x < 0 ? 1 : 0
    subq    $0x3ffe,                    %rdx    // push |x| < 0.5 negative
    cmp     $(63),                      %rdx    // if( |x| >= 0x1.0p62 || |x| < 0.5 || isnan(x) )
    jae     1f                                  //      goto 1
  
//  |x| >= 0.5 and conversion does not overflow.
    movq    $63,                        %rcx    // 63
    subq    %rdx,                       %rcx    // 63-(exponent+1)
    leaq    large(%rip),                %r9     // address of large array
    fadds   (%r9, %r8, 4)                       // { x + (x < 0 ? -0x1.0p63 : 0x1.0p63) }       set inexact as necessary
    shrq    %cl,                        %rax    // shift units bit into 2's position
    fstp    %st(0)                              // { }
    addq    $1,                         %rax    // round away from zero
    shrq    $1,                         %rax    // shift units bit to 1's position

    // find new exponent
    bsrq    %rax,                       %r9     // position of leading set bit. rax is never zero.
    movq    $0x3fff,                    %rdx    // bias
    movq    $63,                        %rcx    // 63
    addq    %r9,                        %rdx    // biased exponent
    subq    %r9,                        %rcx    // 63 - position of leading set bit
    movw    %dx,        8+FRAME_SIZE( STACKP )  // write out new exponent
    
    // shift significand into position
    shlq    %cl,                        %rax    // shift leading bit to higest position
    movq    %rax,       FRAME_SIZE( STACKP )    // write mantissa

    // get sign
    fldt    FRAME_SIZE( STACKP )                // { |result| }
    leaq    one( %rip ),                %rax    // address of one array
    fmuls   (%rax, %r8, 4 )                     // { result }       multiply by +1 or -1 according to sign of original result
    ret
    
//  |x| >= 0x1.0p62 || |x| < 0.5 || isnan(x)
1:  je      3f
    jg      2f

//  |x| < 0.5
    fistpl  FRAME_SIZE( STACKP )                // { } set inexact if x != 0
    leaq    zero( %rip),                %rax    // address of zero array
    flds    (%rax, %r8, 4 )                     // load result
2:  ret
  
//  0x1.0p62 <= |x| < 0x1.0p63
3:  leaq    large(%rip),                %r9     // address of large array
    fadds   (%r9, %r8, 4)                       // { x + (x < 0 ? -0x1.0p63 : 0x1.0p63) }       set inexact as necessary
    fstp    %st(0)                              // { }
    addq    $1,                         %rax    // add 0.5 to significand
    jz      4f                                  // handle overflow
    
    andq    roundMask62(%rip),          %rax    // prune fractional bits
    movq    %rax,                       FRAME_SIZE( STACKP )    // write to mantissa
    fldt    FRAME_SIZE( STACKP )                // load result
    ret

// result is +- 0x1.0p63
4:  flds    (%r9, %r8, 4)                       // load result
    ret
    
    

#else
ENTRY( roundl )
    movzwl  8+FRAME_SIZE( STACKP ),     %edx
    movq    FRAME_SIZE( STACKP ),       %xmm0
    fldt    FRAME_SIZE( STACKP )
    calll   0f
0:  popl    %ecx
    movl    %edx,                       %eax    // sign + biased exponent
    andl    $0x7fff,                    %edx    // biased exponent
    shrl    $15,                        %eax    // signof( x )
    subl    $0x3ffe,                    %edx    // push |x| < 0.5 negative
    cmp     $63,                        %edx    // if( |x| >= 0x1.0p62 || |x| < 0.5 || isnan(x) )
    jae     1f                                  //      goto 1

//  |x| >= 0.5 and conversion does not overflow.
    subl    $63,                        %edx    // (exponent+1) - 63
    fadds   (large-0b)(%ecx, %eax, 4)           // set inexact if necessary
    negl    %edx                                // 63 - (exponent+1)
    fstp    %st(0)                              // {}
    movd    %edx,                       %xmm1   // 63 - (exponent+1)
    psrlq   %xmm1,                      %xmm0   // move 0.5 bit to units position
    pcmpeqb %xmm1,                      %xmm1   // -1
    psubq   %xmm1,                      %xmm0   // add 1
    psrlq   $1,                         %xmm0   // move 1's bit to units position
    movq    %xmm0,                      FRAME_SIZE( STACKP )    // write out
    
    fildll  FRAME_SIZE( STACKP )                // { |result| }
    fmuls   (one-0b)(%ecx, %eax, 4)             // { result }
    ret


//  |x| >= 0x1.0p62 || |x| < 0.5 || isnan(x)
1:  je      3f
    jg      2f

//  |x| < 0.5
    fistpl  FRAME_SIZE( STACKP )                // { } set inexact if x != 0
    flds    (zero-0b)(%ecx, %eax, 4 )                     // load result
2:  ret

//  0x1.0p62 <= |x| < 0x1.0p63
3:  fadds   (large-0b)(%ecx, %eax, 4)           // { x + (x < 0 ? -0x1.0p63 : 0x1.0p63) }       set inexact as necessary
    fstp    %st(0)                              // { }
    movdqa  %xmm0,                      %xmm2   // significand
    pcmpeqb %xmm1,                      %xmm1   // -1LL
    psubq   %xmm1,                      %xmm0   // add 0.5 to significand
    pxor    %xmm0,                      %xmm2   // set leading bit if leading bit changed (overflow)
    movmskpd    %xmm2,                  %edx
    test    $1,                         %edx
    jnz     4f
    
    pand    (roundMask62-0b)(%ecx),     %xmm0    // prune fractional bits
    movq    %xmm0,                      FRAME_SIZE( STACKP )    // write to mantissa
    fldt    FRAME_SIZE( STACKP )                // load result
    ret

// result is +- 0x1.0p63
4:  flds    (large-0b)(%ecx, %eax, 4)           // load result
    ret


#endif

	
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



