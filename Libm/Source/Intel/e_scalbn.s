/*
 *  e_scalbn.s
 *
 *	by Ian Ollmann
 *
 *	Copyright © 2005 Apple Computer. All Rights Reserved.
 */
 


#define LOCAL_STACK_SIZE	(48 - FRAME_SIZE)

#if defined( __LP64__ )
    #define ARG_I   %edi
    #define ARG_Iw  %di
#else
    #define ARG_I   %eax
    #define ARG_Iw  %ax
#endif

#include <machine/asm.h>
#include "abi.h"


ENTRY( scalbnl )
ENTRY( ldexpl )
    SUBP    $LOCAL_STACK_SIZE,   STACKP

    //load i if necessary
#if !defined( __LP64__ )
    movl    SECOND_ARG_OFFSET(STACKP),  ARG_I
#endif
    
    //load x
    fldt        FIRST_ARG_OFFSET(STACKP)
    
    //dump 1.0L to the stack  (we make 2**i constant out of this later)
    fld1
    fstpt       (STACKP)
    
    // if i > 16382 || i < -16382, jump to special case handling code to reduce i
    cmpl        $16382,     ARG_I
    jg          2f
    cmpl        $-16382,    ARG_I
    jl          4f

    //I tried being fancy here with packing data into a vector and doing a 16-byte store
    //to avoid the store small, load large stall, but it was 16 cycles slower on Merom.
    //I think maybe Merom does not store forward from vector to x87.
    //So fix up the exponent in memory directly
1:  addw        ARG_Iw,      8(STACKP)
       
    //load 2**i in and scale x by it
    fldt        (STACKP)
    fmulp
    
    ADDP    $LOCAL_STACK_SIZE, STACKP
    ret

2: //special case for i is huge positive

    //assemble the 0x1.0p16382 constant
    fld1
    fstpt       16(STACKP)
    addw        $16382,  24(STACKP)   
    fldt        16(STACKP)

    //clamp i so we don't iterate for a long long time
    movl        $(3*16382),     %edx
    cmpl        %edx,           ARG_I
    cmovg       %edx,           ARG_I

    //do while i > 16382
3:  fmul        %st(0),         %st(1)
    sub         $16382,         ARG_I
    cmp         $16382,         ARG_I
    jg          3b

    //jump back to the main routine to do the rest of scalbn
    fstp        %st(0)
    jmp         1b
    
4: //special case for i is huge negative

    //assemble the 0x1.0p16382 constant
    fld1
    fstpt       16(STACKP)
    addw        $-16382,  24(STACKP)   
    fldt        16(STACKP)

    //clamp i so we don't iterate for a long long time
    movl        $(-3*16382),        %edx
    cmpl        %edx,               ARG_I
    cmovl       %edx,               ARG_I

    //do while i > 16382
5:  fmul        %st(0),         %st(1)
    sub         $-16382,         ARG_I
    cmp         $-16382,         ARG_I
    jl          5b

    //jump back to the main routine to do the rest of scalbn
    fstp        %st(0)
    jmp         1b



ENTRY( scalbn )
ENTRY( ldexp )
    SUBP    $LOCAL_STACK_SIZE,   STACKP

    //load i if necessary
#if defined( __LP64__ )
	movsd	%xmm0,		(STACKP)
	fldl	(STACKP)
#else
    movl    8+FIRST_ARG_OFFSET(STACKP),  ARG_I
    //load x
    fldl        FIRST_ARG_OFFSET(STACKP)
#endif
    
    
    //dump 1.0L to the stack  (we make 2**i constant out of this later)
    fld1
    fstpt       (STACKP)
    
    // clamp i between -16382 <= i < 16382
    movl        $16382,     %edx
    cmpl        %edx,       ARG_I
    cmovg       %edx,       ARG_I
    movl        $-16382,    %edx
    cmpl        %edx,       ARG_I
    cmovl       %edx,       ARG_I

    //I tried being fancy here with packing data into a vector and doing a 16-byte store
    //to avoid the store small, load large stall, but it was 16 cycles slower on Merom.
    //I think maybe Merom does not store forward from vector to x87.
    //So fix up the exponent in memory directly
    addw        ARG_Iw,      8(STACKP)
       
    //load 2**i in and scale x by it
    fldt        (STACKP)
    fmulp               //exact
    
    //round to double
    fstpl       (STACKP)

#if defined( __i386__ )
    fldl        (STACKP)
#else
    movsd       (STACKP), %xmm0
#endif
    
    ADDP    $LOCAL_STACK_SIZE, STACKP
    ret

ENTRY( scalbnf )
ENTRY( ldexpf )
    SUBP    $LOCAL_STACK_SIZE,   STACKP

    //load i if necessary
#if defined( __LP64__ )
	movss	%xmm0, (STACKP)
	flds	(STACKP)
#else
    movl    4+FIRST_ARG_OFFSET(STACKP),  ARG_I
    //load x
    flds        FIRST_ARG_OFFSET(STACKP)
#endif
    
    
    //dump 1.0L to the stack  (we make 2**i constant out of this later)
    fld1
    fstpt       (STACKP)
    
    // clamp i between -16382 <= i < 16382
    movl        $16382,     %edx
    cmpl        %edx,       ARG_I
    cmovg       %edx,       ARG_I
    movl        $-16382,    %edx
    cmpl        %edx,       ARG_I
    cmovl       %edx,       ARG_I

    //I tried being fancy here with packing data into a vector and doing a 16-byte store
    //to avoid the store small, load large stall, but it was 16 cycles slower on Merom.
    //I think maybe Merom does not store forward from vector to x87.
    //So fix up the exponent in memory directly
    addw        ARG_Iw,      8(STACKP)
       
    //load 2**i in and scale x by it
    fldt        (STACKP)
    fmulp               //exact
    
    //round to float
    fstps       (STACKP)
#if defined( __i386__ )
    flds        (STACKP)
#else
    movss       (STACKP), %xmm0
#endif
    
    ADDP    $LOCAL_STACK_SIZE, STACKP
    ret



