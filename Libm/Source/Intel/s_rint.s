/*
 * Written by Ian Ollmann.
 * Copyright © 2005 Apple Computer Inc.
 */

#include <machine/asm.h>

#include "abi.h"


ENTRY( rintl )
    xorl        %ecx,                       %ecx
    movw        8+FIRST_ARG_OFFSET(STACKP), %cx
    andw        $0x7fff,                    %cx
	fldt		FIRST_ARG_OFFSET(STACKP)	// { f }
    cmpw        $(16383+63),                %cx         //test for NaNs, Infs, large integer
    jae         1f
    fldz                                    // { 0, f }
    fucomip      %st(1),     %st(0)                      // test for zeros
    je          1f

    //do rint
    fistpll     FRAME_SIZE( STACKP )        // { f }
    fildll      FRAME_SIZE( STACKP )        // { rintl(f) }

1:	ret

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
/*
ENTRY( nearbyintl )
	SUBP	$28, STACKP
	
	fldt	(FIRST_ARG_OFFSET+28)( STACKP )				//{f}
	
	//read fpcw + fpsw
	fnstenv	(STACKP)
	movw	(STACKP),	%ax

	//or it with 0x20 
	movl	%eax, %edx
	orl		$0x20,  %eax

	//stick it back int the fpcw
	movw	%ax, (STACKP)
	fldenv	(STACKP)
	
	//round
	frndint							//{ result }
		
	//reset fpsw and fpcw
	movw	%dx, (STACKP)
	fldenv	(STACKP)
	
	ADDP	$28, STACKP
	ret
*/

# if defined( __LP64__ )
ENTRY( llrintl )
ENTRY( lrintl )
	SUBP		$12, STACKP
	movl		$0x5f000000, 8(STACKP)						//limit = 0x1.0p63f
#else
ENTRY( llrintl )
	SUBP		$12, STACKP
	movl		$0x5f000000, 8(STACKP)						//0x1.0p63f
	xor			%edx,		%edx

	flds		8(STACKP)                                   //{0x1.0p63 }
	fldt		(FIRST_ARG_OFFSET+12)( STACKP )				//{f, 0x1.0p63}
	fucomi 		%ST(1), %ST                                 //{f, 0x1.0p63}		f>=0x1.0p63
	fistpll		(STACKP)                                    //{0x1.0p63}
	fstp		%ST(0)                                      //{}

	setnb		%dl                                         // copy f >= 0x1.0p63 to the d register
	negl		%edx                                        // edx = -edx
	movl		(STACKP),	%eax                            // load in the low part of the result from the fistpll above to eax
	xorl		%edx,		%eax                            // xor with edx. This flips 0x8000000000000000 to 0x7fffffffffffffff for overflow
	xorl		4(STACKP),	%edx                            // load in the high part and flip it
	
	ADDP		$12,		STACKP                          
	ret

ENTRY( lrintl )
	SUBP		$12, STACKP
	movl		$0x4f000000, 8(STACKP)						//limit = 0x1.0p31f

#endif

	XORP		DX_P,		DX_P

	flds		8(STACKP)							//{limit }
	fldt		(FIRST_ARG_OFFSET+12)( STACKP )		//{f, limit}
	fucomi 		%ST(1), %ST                         //{f, limit}		f>=limit   test for overflow
	FISTPP		(STACKP)							//{limit}
	fstp		%ST(0)                              //{}

	setnb		%dl                                 // copy f >= limit to the d register
	NEGP		DX_P                                // rdx = -rdx
	MOVP		(STACKP),	AX_P                    // load in the result from the fistpll to the a register
	XORP		DX_P,		AX_P                    // xor with the d register to flip 0x8000... to 0x7fff... in the case of overflow
	
	ADDP		$12,		STACKP
	ret
    

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



