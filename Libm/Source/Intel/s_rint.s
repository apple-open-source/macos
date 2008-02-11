/*
 * Written by Ian Ollmann.
 * Copyright © 2005 Apple Computer Inc.
 */

#include <machine/asm.h>

#include "abi.h"

// Note: I tried doing rintl with fistpll, but it fails for 0x1.0p63-1ulp.
ENTRY( rintl )
    movswl      8+FRAME_SIZE(STACKP),       %eax
    movl        %eax,                       %edx        // sign + exponent of x
    andl        $0x7fff,                    %eax        // exponent of x
    andl        $0x80000000,                %edx        // signof (x)
    fldt        FRAME_SIZE( STACKP )                    // { x }
    subl        $16383,                     %eax
    movl        %edx,                       %ecx        // signof (x)
    orl         $0x5f000000,                %edx        // copysignf( 0x1.0p63f, x )
    cmpl        $63,                        %eax        // if( |x| >= 0x1.0p63L || isnan(x) )
    jae         1f                                      //      goto 2
    
    // 1.0L <= |x| < 0x1.0p63L
    movl        %edx,                       FRAME_SIZE( STACKP )
    flds        FRAME_SIZE(STACKP)                      // copysignl( 0x1.0p63L, x )
    fadd        %st(0),                     %st(1)      // x + copysignl( 0x1.0p63, x )
    fsubrp                                              // x + copysignl( 0x1.0p63, x ) - copysignl( 0x1.0p63, x )
    ret
    
1:  jge         2f                                      // if( |x| >= 0x1.0p63L || isnan(x) ) goto 2

    // |x| < 1.0L
    movl        %edx,                       FRAME_SIZE( STACKP )
    orl         $0x3f800000,                 %ecx       // copysign( 1, x )
    flds        FRAME_SIZE(STACKP)                      // copysignl( 0x1.0p63L, x )
    movl        %ecx,                       4+FRAME_SIZE(STACKP)
    fadd        %st(0),                     %st(1)      // x + copysignl( 0x1.0p63, x )
    fsubrp                                              // x + copysignl( 0x1.0p63, x ) - copysignl( 0x1.0p63, x )
    fabs                                                // strip sign       
    fmuls       4+FRAME_SIZE(STACKP)                    // restore sign to signof x
    
2:  ret

ENTRY( rintf )
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

    cvtps2dq   %xmm0,                   %xmm0
    cvtdq2ps    %xmm0,                  %xmm0
#if defined( __i386__ )
    movss   %xmm0,                      FRAME_SIZE( STACKP )
#endif
    
1:
#if defined( __i386__ )
    flds    FRAME_SIZE( STACKP )
#endif
    ret


#if defined( __i386__ )    

ENTRY(rint)
    fldl    FIRST_ARG_OFFSET(STACKP)
    frndint
    ret
#else

ENTRY( rint )
	movd		%xmm0,					%rax
	movl		$0x43300000,			%edx
	movl		$0x80000000,			%ecx
	shlq		$32,					%rdx		// 0x1.0p52
	shlq		$32,					%rcx		// -0.0
	andq		%rax,					%rcx		// signof( x )
	xorq		%rcx,					%rax		// |x|
	cmpq		%rdx,					%rax		// if( |x| >= 0x1.0p52 || isnan(x) )
	ja			1f									//		return x
	
	orq			%rcx,					%rdx		// copysign( 0x1.0p52, x )
	movd		%rdx,					%xmm1		// copysign( 0x1.0p52, x )
	
	addsd		%xmm1,					%xmm0		// x + copysign( 0x1.0p52, x )
	subsd		%xmm1,					%xmm0		// x + copysign( 0x1.0p52, x ) -  copysign( 0x1.0p52, x )
	
1:	ret
	
#endif
    

//i386 versions if these functions are in xmm_floor.c
//On x86_64 we can take advantage of the REX form of cvtsd2si to produce 64-bit values
#if defined( __LP64__ )

ENTRY( lrint )
ENTRY( llrint )
    movl        $0x43e00000, %eax                   //Exponent for 0x1.0p63
    movd        %eax,  %xmm1                        //copy to low 32-bits of xmm1
    psllq       $32,   %xmm1                        //move it to the high 32-bits of the low double in xmm1, to make 0x1.0p63
    cmplesd     %xmm0, %xmm1                        //compare 0x1.0p63 <= x.  Since there are no double precision values between LONG_MAX and 0x1.0p63 we don't need to worry about them
	cvtsd2siq	%xmm0, %rax                         //convert x to long
    movd        %xmm1, %rdx                         //copy compare result (all 64-bits) to %rdx 
    xorq        %rdx,  %rax                         //flip overflow values to 0x7fffffffffffffff
	ret

ENTRY( lrintf )
ENTRY( llrintf )
    movl        $0x5f000000, %eax                   //load 0x1.063f
    movd        %eax,  %xmm1                        //copy to xmm
    cmpless     %xmm0, %xmm1                        //compare 0x1.063f <= x
	cvtss2siq	%xmm0, %rdx                         //convert x to long
    movd        %xmm1, %rax                         //copy 64 bits of the comparison result to %rdx
	cdqe											//sign extend 
    xorq        %rdx,  %rax                         //flip overflow results to 0x7fffffffffffffff
	ret
    
#else

ENTRY( lrintf )
    movl        $0x4f000000, %eax                           //load 0x1.0p31f
    movss       (FIRST_ARG_OFFSET)( STACKP ), %xmm0         //load x
    movd        %eax, %xmm1                                 //copy 0x1.0p31f to xmm1
    cmpless     %xmm0, %xmm1                                //compare 0x1.0p31f <= x. There are no single precision values between INT_MAX and 0x1.0p31f, so no need to worry here.
    cvtss2si    %xmm0, %eax                                 //convert to int
    movd        %xmm1,  %edx                                //move the compare result to edx
    xorl        %edx, %eax                                  //saturate overflow results to 0x7fffffff
    ret
    
ENTRY( lrint )
    movsd       (FIRST_ARG_OFFSET)( STACKP ), %xmm0         // load x
    xorpd       %xmm1, %xmm1                                // load 0.0f
    cmpltsd     %xmm0, %xmm1                                // test 0.0f < x
    cvtsd2si    %xmm0, %eax                                 // convert x to int
    movd        %xmm1,  %edx                                // copy the compare result to %edx
    xorl        %ecx, %ecx                                  // set %ecx to 0
    cmp         $0x80000000, %eax                           // check the result to see if it is 0x80000000 -- the overflow result
    cmovne      %ecx, %edx                                  // if the result is not 0x80000000, overwrite the earlier compare result with 0
    xorl        %edx, %eax                                  // saturate overflow results to 0x7fffffff (was 0x80000000)
    ret
    
ENTRY( llrintf )
	SUBP		$12, STACKP
	movl		$0x5f000000, 8(STACKP)						//0x1.0p63f
	xor			%edx,		%edx

	flds		8(STACKP)                                   //{0x1.0p63 }
	flds		(FIRST_ARG_OFFSET+12)( STACKP )				//{f, 0x1.0p63}
	fucomi 		%ST(1), %ST                                 //{f, 0x1.0p63}		f>=0x1.0p63
	fistpll		(STACKP)                                    //{0x1.0p63}
	fstp		%ST(0)                                      //{}

	setnb		%dl                                         // copy f >= 0x1.0p63 to the d register
	negl		%edx                                        // convert [0,1] to [0,-1]
	movl		(STACKP),	%eax                            // load low 32-bits of the result
	xorl		%edx,		%eax                            // saturate to 0xffffffff if overflow
	xorl		4(STACKP),	%edx                            // load the high 32-bits of the result and saturate to 0x7fffffff if overflow
	
	ADDP		$12,		STACKP
	ret
    
ENTRY( llrint )
	SUBP		$12, STACKP
	movl		$0x5f000000, 8(STACKP)						//0x1.0p63f
	xor			%edx,		%edx

	flds		8(STACKP)                                   //{0x1.0p63 }
	fldl		(FIRST_ARG_OFFSET+12)( STACKP )				//{f, 0x1.0p63}
	fucomi 		%ST(1), %ST                                 //{f, 0x1.0p63}		f>=0x1.0p63
	fistpll		(STACKP)                                    //{0x1.0p63}
	fstp		%ST(0)                                      //{}

	setnb		%dl                                         // copy f >= 0x1.0p63 to the d register
	negl		%edx                                        // convert [0,1] to [0,-1]
	movl		(STACKP),	%eax                            // load low 32-bits of the result
	xorl		%edx,		%eax                            // saturate to 0xffffffff if overflow
	xorl		4(STACKP),	%edx                            // load the high 32-bits of the result and saturate to 0x7fffffff if overflow
	
	ADDP		$12,		STACKP
	ret
    
#endif



