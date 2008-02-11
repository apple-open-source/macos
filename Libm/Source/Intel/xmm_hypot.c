/*
 *  xmm_hypot.c
 *  LibmV5
 *
 *  Created by iano on 11/8/05.
 *  Copyright 2005 __MyCompanyName__. All rights reserved.
 *
 */

#include "math.h"
#include "xmmLibm_prefix.h"

double hypot( double x, double y )
{
	static const long double inf = __builtin_inf();
	long double X = __builtin_fabsl(x);
	long double Y = __builtin_fabsl(y);
	if ( X == inf || Y == inf )
		return (double)inf;
    if( x == 0.0 )
        return (double) Y;
    else if( y == 0.0 )
        return (double) X;
    else if( x != x || y != y )
        return (float)(X + Y);
        
	return (double)__builtin_sqrtl(X*X + Y*Y);
	
}

float hypotf( float x, float y )
{
    static const double inf = __builtin_inf();
    double X = __builtin_fabs(x);
    double Y = __builtin_fabs(y);
	if ( X == inf || Y == inf )
		return (float) inf;
    if( x == 0.0f )
        return (float) Y;
    else if( y == 0.0f )
        return (float) X;
    else if( x != x || y != y )
        return (float)(X + Y);
    if ( X == inf || Y == inf )
        return (float)inf;
    return (float)__builtin_sqrt(X*X + Y*Y);
}

typedef union
{
	long double ld;
	struct
	{
		uint64_t	mantissa;
		int16_t		sexp;
	};
}ld_parts;

long double hypotl( long double x, long double y )
{
    static const long double infinity = __builtin_infl();
	ld_parts *large = (ld_parts*) &x;
	ld_parts *small = (ld_parts*) &y;

	large->ld = __builtin_fabsl( large->ld );
	small->ld = __builtin_fabsl( small->ld );
    
	if( EXPECT_FALSE( large->sexp == 0x7fff || small->sexp == 0x7fff ) )
	{
		if( __builtin_fabsl(large->ld) == infinity || __builtin_fabsl(small->ld) == infinity )
			return infinity;
	
		return x + y;
	}

	if( x == 0.0L || y == 0.0L )
		return __builtin_fabsl( x + y );

	if( large->ld < small->ld )
	{
		ld_parts *p = large;
		large = small;
		small = p;
	}
	
	int lexp = large->sexp;
	int sexp = small->sexp;
	if( lexp == 0 )
	{
		large->ld = large->mantissa;
		lexp = large->sexp - 16445;
	}
	if( sexp == 0 )
	{
		small->ld = small->mantissa;
		sexp = small->sexp - 16445;
	}
	large->sexp = 0x3fff;
	int scale = 0x3fff - lexp;
	int small_scale = sexp + scale;
	if( small_scale < 64 )
		small_scale = 64;
	small->sexp = small_scale;
	
	long double r = sqrtl( large->ld * large->ld + small->ld * small->ld );
	
	int halfScale = scale >> 1;
	scale -= halfScale;
	halfScale = 0x3fff - halfScale;
	scale = 0x3fff - scale;
	large->sexp = halfScale;
	large->mantissa = 0x8000000000000000ULL;
	small->sexp = scale;
	small->mantissa = 0x8000000000000000ULL;

	r *= large->ld;
	r *= small->ld;
		
	return r;
}



