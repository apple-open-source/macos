/*
 * Original public domain code for expl Written by J.T. Conklin <jtc@netbsd.org>.
 *     fixed up to deliver correct edge cases by Ian Ollmann (expl)
 *
 *		exp2l by Ian Ollmann based on algorithm by J.T. Conklin
 *
 *		expm1l is new code by Ian Ollmann
 */

#include <machine/asm.h>
#include "abi.h"

#if defined( __LP64__ )
	#error not 64-bit ready
#endif

/* e^x = 2^(x * log2(e)) */
ENTRY(expl)    
	push	$0x7f800000					// inf
	push	$0x50000000					// limit

	//test for inf or NaN
	fldt	12(%esp)					// { f }
	fabs								// { |f| }
	flds	4(%esp)						// { inf, |f| }
	fucomip	%ST(1), %ST(0)				// { |f| }		if |f| == inf or f is NaN 
	fstp	%ST(0)						// {}
	fldt	12(%esp)					// { f }
	flds	(%esp)						// { limit, f }
	je		expl_special

	//clip f to be between limit and -limit.
	//If we dont do this, when we scale by log2e then it is possible that we will generate spurious overflow
	fld		%ST(0)						// { limit, limit, f }
	fchs								// { -limit, limit, f }
	fucomi  %ST(2), %ST(0)				// { -limit, limit, f }	if( -limit < f )
	fcmovb  %ST(2), %ST(0)				// { max( -limit, f ), limit, f }
	fucomi  %ST(1), %ST(0)				// { max( -limit, f ), limit, f }	if( max( -limit, f ) > limit )
	fcmovnbe  %ST(1), %ST(0)			// { clipped, limit, f }
	fstp	%ST(2)						// { limit, clipped }
	fstp	%ST(0)

	//do exp
	fldl2e								// { log2e, max( -1024, f ) } 
	fmulp								// { log2e*f }
	fld		%st(0)						// { f*log2e, f*log2e }
	frndint								// { [f*log2e], f*log2e }
	fxch	%st(1)						// { f*log2e, [f*log2e] }
	fsub	%st(1),%st					// { fract( f*log2e ), [f*log2e] } 
	f2xm1								// { 2**fract( f*log2e ) -  1, [f*log2e]  }
	fld1								// { 1, 2**fract( f*log2e ) -  1, [f*log2e]  }
	faddp								// { 2**fract( f*log2e ), [f*log2e] }
	fscale								// { result, [f*log2e] }
	fstp	%st(1)						// { result }
	addl	$8,	%esp
    ret

expl_special:  //handles +-Inf, QNaN, SNaN
	fucomip  %ST(1), %ST(0)				// { f }	if( limit > f )
	fldz								// { 0, f }
	fxch								// { f, 0 }
	fcmovnbe %ST(1), %ST(0)				// set the -Inf case to 0
	fadd	 %ST(0), %ST(0)				// silence NaNs
	fstp	 %ST(1)
	addl	$8,	%esp
	ret


ENTRY(exp2l)    
	push	$0x7f800000					// inf
	push	$0x1e000000					// small (0x1.0p-67f)
	
	//test for +inf, NaN
	fldt	12(%esp)					// { f }
	flds	4(%esp)						// { inf, f }
	fucomip %ST(1), %ST(0)				// { f }
	je		exp2l_exit					//	return f if Inf or NaN

	//test for -inf
	flds	4(%esp)						// { inf, f }
	fchs								// {-inf, f }
	fucomip	%ST(1), %ST(0)				// { f }
	jne		exp2l_main
	fldz								// { 0, f }
	fstp	%st(1)						// { f }
	jmp		exp2l_exit
	
exp2l_main:
	flds	(%esp)						// { small, f }
	fld		%st(1)						// { f, small, f }
	fabs								// { |f|, small, f }
	fucomip	%st(1), %st(0)				// { small, f }
	fstp	%st(0)						// { f }
	jb		exp2l_small
	fld		%st(0)						// { f, f }
	frndint								// { intf, f }
	fxch	%st(1)						// { f, intf }
	fsub	%st(1),%st					// { fract(f), intf } 
	f2xm1								// { 2**fract(f) - 1, intf }
	fld1								// { 1, 2**fract(f) - 1, intf }
	faddp								// { 2**fract(f), intf }
	fscale								// { result, intf }
	fstp	%st(1)						// { result }
	//fall through to exit

exp2l_exit:
	addl	$8,	%esp
    ret

exp2l_small:		//avoid setting the underflow flag for denormal inputs
	fld1								//  {1, f }
	faddp								//	{result}
	addl	$8,	%esp
    ret
	

//do expm1
	// e**x - 1 = 2**(x*log2(e)) - 1
	//
	//		f = fract( x*log2e )		//fractional part of x*log2e
	//		i =   int( x*log2e )		//integral part of x*log2e
	//
	// e**x - 1 = 2**( f + i ) - 1
	//			= (2**f)(2**i) - 1
	//			= (2**f)(2**i) - (2**i) + (2**i) - 1 
	//			= (2**f - 1)(2**i)	+ (2**i) - 1 
	//				(exact)           (needs to be done in the right order, or not at all for i == 0)
	//
	//		The multiplication is exact. The two adds need to be done least to greatest in order of magnitude.
	//		Since 2**f - 1 is always in the range [ -0.5, 1.0 ], we know ahead of time that (2**f-1)(2**i)
	//		always has less magnitude than (2**i).  if i < 0 then add the -1 first. Otherwise, add (2**i) first
	//
	//	There are a lot of special cases here, which is why this is a bit branchy. I tried it without
	//	branches and wasn't able to satisfy all the constraints called for by various standards

#define EXPM1L_STACK_SIZE		$12

ENTRY(expm1l)    
	pushl	$0xc6000000					// -8192.0f
	pushl	$0x7f800000					// inf
	pushl	$0x47000000					// 32768

	//test for inf or NaN
	fldt		16(%esp)				// { x }
	fldz								// { 0, x }
	fucomip		%st(1), %st(0)			// { x }
	jnbe			expm1l_neg			//	if 0 > x goto expm1_neg
	
	// x >= 0...
	// get x*log2e
	fldl2e								// { log2e, x } 
	fmulp								// { x*log2e }
	flds		4(%esp)					// { inf, x*log2e }
	fucomip		%ST(1), %ST(0)			// { x*log2e }		if x*log2e == inf  or NaN
	je			expm1l_special

	//next convert to integer and fractional parts using round to zero rounding mode
	//calculate trunc(x*log2e)
	flds		(%esp)					// { 32768.0, x*log2e }
	fucomi		%st(1), %st(0)			// { x*log2e }
	fcmovnbe	%st(1), %st(0)			// { clipped, x*log2e }
	fstp		%st(1)					// { clipped }
	fld			%st(0)					// { clipped, clipped }
	fisttpll	(%esp)					// { clipped }
	fildll		(%esp)					// { i, clipped }

	//test 0 against i
	fldz								// {0, i, x*log2e}
	fucomip		%st(1), %st(0)			// {i, x*log2e}

	//get the fractional part of x*log2e with the same sign as x*log2e
	fsubr		%st(0), %st(1)			// { i, f }
	
	//calculate 2**f-1
	fxch								// { f, i }
	f2xm1								// { 2**f -  1, i }

	//three way switch for 0>i, i==0, i>0
	je		exmpm1l_zero
	
	//This case is a bit trickier, because 2**i can overflow when the result does not
	//we do this as:
	//
	//	e**x - 1	= (2**f - 1)(2**i)	+ (2**i) - 1
	//				= (2**i)( (2**f-1) - 2**(-i) + 1)
	//
	//	we clamp 2**(-i) so that it can't go lower than 2**(-8192) to prevent underflow
	//	When this happens the result is always 0x1.fffffffffffffffep-1 * 2**i, so it doesn't
	//  much matter
	fld		%st(1)						// { i, 2**f-1, i}
	fchs								// {-i, 2**f-1, i}
	flds	8(%esp)						// {-8192.0f, -i, 2**f-1, i}
	fucomi  %st(1), %st(0)				// {-8192.0f, -i, 2**f-1, i}
	fcmovb  %st(1), %st(0)				// { clipped(-i), -i, 2**f-1, i}
	fstp	%st(1)						// { clipped(-i), 2**f-1, i}
	fld1								// {1, clipped(-i), 2**f-1, i}
	fscale								// { 2**-i,clipped(-i), 2**f-1, i}
	fstp	%st(1)						// { 2**-i, 2**f-1, i}
	fsubrp								// { 2**f-1 - 2**-i, i}
	fld1								// { 1, 2**f-1 - 2**-i, i}
	faddp								// { 1+ 2**f-1 - 2**-i, i}
	fscale
	fstp	%st(1)

	addl	EXPM1L_STACK_SIZE,	%esp
    ret


// x < 0 or x is NaN
expm1l_neg:								// {x}
	flds	4(%esp)						// { inf, x }
	fchs								// { -inf, x }
	fucomip	%ST(1), %ST(0)				// { x }		if x == -inf  
	je		expm1l_special				// NaNs and -Inf go to special case handling code

	// clip large negative values to avoid overflow
	flds	8(%esp)						// { -8192, x }
	fucomi	%st(1), %st(0)				// { -8192, x }
	fcmovb  %st(1), %st(0)				// { clipped, x }
	fstp	%st(1)						// { clipped }

	//scale by log2e
	fldl2e								// { log2e, clipped }	//clipped a.k.a x from here on
	fmulp								// { x*log2e }

	//next convert to integer and fractional parts using round to zero rounding mode
	//calculate trunc(x*log2e)
	fld			%st(0)					// { x*log2e, x*log2e }
	fisttpll	(%esp)					// { x*log2e }
	fildll		(%esp)					// { i, x*log2e }

	//test 0 against i
	fldz								// {0, i, x*log2e}
	fucomip		%st(1), %st(0)			// {i, x*log2e}

	//get the fractional part of x*log2e with the same sign as x*log2e
	fsubr		%st(0), %st(1)			// { i, f }
	
	//calculate 2**f-1
	fxch								// { f, i }
	f2xm1								// { 2**f -  1, i }

	//jump to complex code for handling the i < 0 case
	je	exmpm1l_zero

	
	// In the i < 0 case, 2**i has a smaller magnitude than -1, so it is added first
	//
	//			= (2**f - 1)(2**i)	+ (2**i) - 1 
	//
	// The 2**i scaling is exact, so it can be factored out:
	//
	//			= (2**i)(2**f-1+1) -1
	fld1								// { 1, 2**f - 1, i }
	faddp								// { 2**f-1+1, i }
	fscale								// { (2**f-1+1)*(2**i), i }
	fstp	%st(1)						// { (2**f-1+1)*(2**i) }
	fld1								// { 1, (2**f-1+1)*(2**i) } 
	fsubrp								// { result }
	addl	EXPM1L_STACK_SIZE,	%esp
    ret

exmpm1l_zero:	//the i == 0 case
	fstp	%st(1)						// { result }
	addl	EXPM1L_STACK_SIZE,	%esp
    ret


expm1l_special:  //handles +-Inf, QNaN, SNaN
	fldz								// {0, x}
	fucomip  %ST(1), %ST(0)				// { x }	if( 0 > x )
	fadd	 %ST(0), %ST(0)				// silence NaNs
	fld1								// { 1, x }
	fchs								// { -1, x }
	fxch								// { f, -1 }
	fcmovnbe %ST(1), %ST(0)				// set the -Inf case to -1
	fstp	 %ST(1)
	addl	EXPM1L_STACK_SIZE,	%esp
	ret
