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


ENTRY(acosl)
	pushl		$0x00800000			//  0x1.0p-126f
	fldt		8(%esp)				//	{x}
	fabs							//	{|x| }

	//clip tiny values to 2**-126 to prevent underflow
	flds		(%esp)				//	{2**-126, |x| }
	fucomi		%st(1), %st(0)		//
	fcmovb		%st(1), %st(0)		//	{ 2**-126 or |x|, |x| }
	fstp		%st(1)				//	{ 2**-126 or |x| }

	//handle overflow / NaN input
	fld1							//	{1, 2**-126 or |x| }
	fucomi		%st(1), %st(0)
	jb			acosl_nan

	fstp		%st(1)				//	{ 1 }
	fldt		8(%esp)				//	{ x, 1 }
	je			acosl_one

	// asin(x) = atan( x / sqrt( 1 - x*x ) )
	fld			%st(0)				//	{ x, x, 1 }
	fsubr		%st(2), %st(0)		//	{ 1-x, x, 1 }
	fsqrt							//	{ sqrt( 1 - x ), x, 1 }
	fxch		%st(2)				//	{ 1, x, sqrt( 1 - x ) }
	faddp							//	{ 1 + x, sqrt( 1 - x ) }
	fsqrt							//	{ sqrt(1 + x), sqrt( 1 - x ) }
	fpatan							//	{ result / 2 }
	fadd		%st(0), %st(0)		//	{ result }
	addl		$4, %esp
	ret

acosl_one:							//	{ x, 1 }
	fucomip		%st(1), %st(0)		//	{ 1 }
	fsub		%st(0), %st(0)		//	{ 0 }
	fldpi							//	{ pi, 0 }
	fcmove		%st(1), %st(0)		//	{ pi or 0, 0 }
	fstp		%st(1)				//	{ pi or 0 }
	addl		$4, %esp
	ret

acosl_nan:							//{ 1, |x| }
	fstp		%st(0)				//{ |x| }
	fchs							//{-|x| }
	fsqrt							//{nan}	set invalid flag
	addl		$4, %esp
	ret

