/*
 * Written by Ian Ollmann
 *
 * Copyright © 2005, Apple Computer Inc. All Rights Reserved.
 */

#include <machine/asm.h>
#include "abi.h"


#if defined( __LP64__ )
	#error not 64-bit ready
#endif

// atan (x / sqrt(1 - x**2))

ENTRY(asinl)
	pushl		$0x00800000			//  0x1.0p-126f
	fldt		8(%esp)				//	{x}
	fld			%st(0)				//	{ x, x }
	fabs							//	{|x|, x }

	//clip tiny values to 2**-126 to prevent underflow
	flds		(%esp)				//	{2**-126, |x|, x }
	fucomi		%st(1), %st(0)		//
	fcmovb		%st(1), %st(0)		//	{ 2**-126 or |x|, |x|, x }
	fstp		%st(1)				//	{ 2**-126 or |x|, x }

	//handle overflow / NaN input
	fld1							//	{1, 2**-126 or |x|, x }
	fucomi	%st(1), %st(0)
	jb		asinl_nan

	// asin(x) = atan( x / sqrt( 1 - x*x ) )
	fld		%st(1)				//	{ |x|, 1, |x|, x }
	fmulp	%st(0), %st(2)		//	{ 1, x*x, x }
	fsubp						//	{ 1 - x*x, x }
	fsqrt						//	{ (1-x*x)**0.5, x }
	fpatan						//	{ result }
	addl	$4, %esp
	ret

asinl_nan:						//{ 1, |x|, x }
	fstp	%st(0)				//{ |x|, x }
	fchs						//{-|x|, x }
	fsqrt						//{nan, x}	set invalid flag
	fstp	%st(1)				//{ nan }
	addl	$4, %esp
	ret