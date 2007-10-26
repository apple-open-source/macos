/*
 *  nextafterl.s
 *  LibmV5
 *
 *  Written by Ian Ollmann on 1/18/06.
 *  Copyright 2006 Apple Computer. All rights reserved.
 *
 */

#include <machine/asm.h>

#define LOCAL_STACK_SIZE	12
#include "abi.h"

.const
.align 4
smallest:       .long           0x00000001, 0x00000000, 0x00000000, 0x00000000                  //0x0.0000000000000002p-16382L
tiny:			.long           0x00000000, 0x80000000, 0x00000001, 0x00000000                  //0x1.0000000000000000p-16382L 
infinity:		.long           0x00000000, 0x80000000, 0x00007FFF, 0x00000000                  //Inf 
max_ld:			.long           0xFFFFFFFF, 0xFFFFFFFF, 0x00007FFE, 0x00000000                  //LDBL_MAX 


.text


#if defined( __LP64__ )
	#define RELATIVE_ADDR( _a)								(_a)(%rip)
#else
	//a short routine to get the local address
	local_addr:
		MOVP    (STACKP), BX_P
		ret

	#define RELATIVE_ADDR( _a)								(_a)-rel_addr(%ebx)
#endif

//long double nextafter( long double x, long double y )
ENTRY(nextafterl)
ENTRY(nexttowardl)
	SUBP	$LOCAL_STACK_SIZE, STACKP
	fldt	FIRST_ARG_OFFSET(STACKP)				//{x}
	fldt	SECOND_ARG_OFFSET(STACKP)				//{y, x}
	
	//if( y != y || x != x )	return x + y
	fucomi	%st(1), %st(0)							// compare y and x
	jp		1f										// if y or x is NaN, jump to 1
	je		2f										// if y == x, jump to 2


	fstp	%st(0)									// {x}
	fnstcw  2(STACKP)								//record control word
	movw	$0x0832, %cx
	movw	$0x0432, %dx
	cmovbw	%dx, %cx								// if( y > x ) cx = 0x0402 else cx = 0x0802

#if defined( __i386__ )
	//get local address
	MOVP	BX_P,	4(STACKP)
	call 	local_addr

rel_addr:
#endif
	
	//set addend for result to be +- smallest according to y > x
	fldt	RELATIVE_ADDR( smallest )				// {smallest, x }
	fld		%st(0)									// {smallest, smallest, x }
	fchs											// {-smallest, smallest, x }
	fcmovnbe %st(1), %st(0)							// {+-smallest, smallest, x }
	fstp	%st(1)									// { +-smallest, x }

	//set the appropriate rounding mode (inf or -inf)
	movw	2(STACKP),	%ax							//load save FP control word
	andw	$0xf3ff,	%ax			//zero the rounding mode
	orw		%cx,		%ax			//set appropriate bits (mask out denorm exceptions, set correct rounding mode). Nukes EFLAGS
	movw	%ax, (STACKP)
	fldcw	(STACKP)

	//calculate result
	faddp	%st(0),	%st(1)							// { result }							find result, set overflow if it occurred
	
	//look for underflow:  do tiny *  (|result| < tiny ? tiny : 0)
	fldt	RELATIVE_ADDR( tiny )					// { tiny, result }
	fld		%st(1)									// { result, tiny, result }
	fabs											// { |result|, tiny, result }
	fucomi  %st(1), %st(0)							// { |result|, tiny, result }
	fldz											// { 0, |result|, tiny, result }
	fcmovb	%st(2), %st(0)							// { 0 or tiny, |result|, tiny, result }
	fmulp	%st(0), %st(2)							// { |result|, junk, result }						set underflow if underflow
	fstp	%st(1)									// { |result|, result }
	
	//Check for infinity
	fldt	RELATIVE_ADDR( infinity )				// { inf, |result|, result }
	fucomip %st(1), %st(0)							// { |result|, result }
	je		3f										// if inf, goto 3
	
	//restore the old rounding mode, bx register and stack pointer
	fstp	%st(0)									// { result }
	fldcw	2(STACKP)
#if defined( __i386__ )
	MOVP	4(STACKP), BX_P							//restore the bx register
#endif
	ADDP	$LOCAL_STACK_SIZE, STACKP				//restore stack
	ret


1:	//Handle NaN cases -- return x + y
	faddp											//{ x + y }		//clear SNaN
	ADDP	$LOCAL_STACK_SIZE, STACKP				//restore stack
	ret

2:	//Handle y == x case, return x
	fstp	%st(0)									//{ x + y }		//clear SNaN
	ADDP	$LOCAL_STACK_SIZE, STACKP				//restore stack
	ret

3:	//Handle result is infinite case				// { |result|, result }
	fucomip	%st(1), %st(0)							// { result }
	jne		4f										//  if result is +Inf, goto 4
	//This is now the result is +Inf case
	fldt	FIRST_ARG_OFFSET(STACKP)				// { x, result }
	fucomip %st(1), %st(0)							// { result }
	fldt	RELATIVE_ADDR(max_ld)					// { LDBL_MAX, result }
	fcmovne %st(1), %st(0)							// { correct result, result }
	fstp	%st(1)									// { correct result }
	fldcw	2(STACKP)
#if defined( __i386__ )
	MOVP	4(STACKP), BX_P							//restore the bx register
#endif
	ADDP	$LOCAL_STACK_SIZE, STACKP				//restore stack
	ret
	
4: //Handle -inf case
	fldt	FIRST_ARG_OFFSET(STACKP)				// { x, result }
	fucomip %st(1), %st(0)							// { result }
	fldt	RELATIVE_ADDR(max_ld)					// { LDBL_MAX, result }
	fchs											// { -LDBL_MAX, result }
	fcmovne %st(1), %st(0)							// { correct result, result }
	fstp	%st(1)									// { correct result }
	fldcw	2(STACKP)
#if defined( __i386__ )
	MOVP	4(STACKP), BX_P							//restore the bx register
#endif
	ADDP	$LOCAL_STACK_SIZE, STACKP				//restore stack
	ret
