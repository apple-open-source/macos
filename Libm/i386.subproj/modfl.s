/*
 *  modfl.s
 *  LibmV5
 *
 *  Created by Ian Ollmann on 9/6/05.
 *  Copyright 2005 Apple Computer. All rights reserved.
 *
 */

#include <machine/asm.h>
#include "abi.h"

#if defined( __LP64__ )
	#error not 64-bit ready
#endif

#if ! defined( __SSE3__ )
	#error	modfl function requires SSE3
#endif

// I tried branch free code here. Alas there were so many special cases, that 2/3 of the code was patchup after the fidll instruction.
// I've moved some special cases { 0, +-inf, NaN} out, which simplifies things quite a bit on any path you care to follow.

ENTRY( modfl )
	pushl		$0x7f800000				//inf
	pushl		$0x5f000000				//1.0p63f
	subl		$4, %esp
	
	fldt		16(%esp)				// {x}
	fld			%st(0)					// {x}
	fabs								// {|x|, x}
	movl		32(%esp), %eax			//				load *iptr
	flds		4(%esp)					// {1.0p63, |x|, x}
	fucomip		%st(1), %st(0)			// {|x|, x}			1.0p63 > |x| 
	fldz								// { 0, |x|, x }
	fcmovnbe	%st(2), %st(0)			// { 0 or x, |x|, x }
	fisttpll	(%esp)					// { |x|, x }					***uses sse3***

	//patch up x is finite integer case
	fildll		(%esp)					// { i or 0, |x|, x}
	fcmovbe		%st(2), %st(0)			// { i, |x|, x }					//copy back x for all large integers, Inf and NaN

	//get zero and NaN out of the main path
	fldz
	fucomip		%st(1), %st(0)
	je			modfl_zero
		
	//deal with infinity
	flds		8(%esp)					// { inf, i, |x|, x }
	fucomip		%st(2), %st(0)			// { i, |x|, x }
	fstp		%st(1)					// { i, x }
	je			modfl_inf
	
	//find the fraction
	fsubr		%st(0), %st(1)			// {i, f }

	//deal with the sign of f == 0
	fld			%st(1)					// { f, i, f}
	fchs								// { -f, i, f }
	fucomi		%st(2), %st(0)			// { -f, i, f }
	fcmovne		%st(2), %st(0)			// { +-f, i, f }
	fucomi		%st(1), %st(0)			
	fcmovb		%st(2), %st(0)
	fstp		%st(2)

	//return result
	fstpt		(%eax)					// { i, f }
	
	addl		$12, %esp
	ret
	
modfl_inf:
	fstp		%st(0)					// { x }
	fldz								// { 0, x }
	fchs								// { -0, x }
	fldz								// { 0, -0, x }
	fucomi		%st(2), %st(0)			// { 0, -0, x }
	fcmovnb		%st(1), %st(0)			// {+-0, -0, x }
	fxch		%st(2)					// { x, -0, +-0 }
	fstpt		(%eax)					// { -0, +-0 }
	fstp		%st(0)
	addl		$12, %esp
	ret
	
modfl_zero:								// { i, |x|, x }		handles 0 and NaN
	//set i to have the same sign as x
	//while we could do this for most cases by giving a signed x to fisttpll, the zero case always comes back +0
	//so we save a fabs operation by throwing away the sign early
	fstp		%st(1)					// { i, x }
	fabs								// { |i|, x }
	fucomi		%st(1), %st(0)			// { |i|, x }
	fld			%st(0)					// { |i|, |i|, x }
	fchs								// { -|i|, |i|, x }
	fcmovb		%st(1), %st(0)			// { +-|i|, |i|, x }		//Handle x > 0
	fcmove		%st(2), %st(0)			// { i, |i|,  x }			//Handle x == 0 and i is NaN
	fstp		%st(1)					// { i, x }
	fstpt		(%eax)					// { x }
	addl		$12, %esp
	ret
	
	
	