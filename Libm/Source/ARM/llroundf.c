
/*
 * llroundf.c
 *
 *		by Ian Ollmann
 *
 * Copyright (c) 2007, Apple Inc. All Rights Reserved.
 *
 * C99 implementation of llroundf.c
 */
 
#include <limits.h>
#include <math.h>
#include <stdint.h>
 
#warning *** untested -- we don't have tests for this

long long llroundf( float x )
{
	union{ float f; uint32_t u;}u = {x};
	uint32_t absx = u.u & 0x7fffffff;
	
	long long result = (long long) x;	//set invalid / inexact if necessary
	
	if( absx >= 0x4b000000U )	// 0, large or NaN
	{
		//Deal with overflow cases
		if( x < (float) LONG_LONG_MIN )
			return LONG_LONG_MIN;

		// Note: float representation of LONG_LONG_MAX likely inexact, 
		//		  which is why we do >= here
		if( x >= -((float) LONG_LONG_MIN) )
			return LONG_LONG_MAX;
	
		return x;
	}
 
	if( (float) result != x )
	{
		// copysign( 0.5f, x )
		union{ uint32_t u;	float f;} v = { (u.u & 0x80000000U) | 0x3f000000U };

		if( absx == 0x3effffffU )
			return result;

		x += v.f;
		
		result = (long long) x;
	}
 
	return result;
}
