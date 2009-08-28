/*
 *  hypotf.c
 *  cLibm
 *
 *  Created by Ian Ollmann on 6/13/07.
 *  Copyright 2007 Apple Inc. All rights reserved.
 *
 */

#include <math.h>
#include <stdint.h>

typedef union{ uint64_t u; double d; } du;

double hypot( double x, double y )
{
	static const double inf = __builtin_inf();
	du u[3];
	du *large = u;
	du *small = &u[1];
	
	u[0].d = __builtin_fabs(x);
	u[1].d = __builtin_fabs(y);
	
	// handle inf / NaN
	if( 0x7ff0000000000000ULL == ( u[0].u & 0x7ff0000000000000ULL)  || 
		0x7ff0000000000000ULL == ( u[1].u & 0x7ff0000000000000ULL)	)
	{
		if( 0x7ff0000000000000ULL == u[0].u || 0x7ff0000000000000ULL == u[1].u )
			return inf;

		return x + y;		// NaN
	}

	if( x == 0.0 || y == 0.0 )
		return __builtin_fabs( x + y );

	//fix pointers to large and small if necessary
	if( u[0].d < u[1].d )
	{
		large = &u[1];
		small = &u[0];
	}

	//break values up into exponent and mantissa
	int64_t largeExp = large->u >> 52;
	int64_t smallExp = small->u >> 52;
	int64_t diff = largeExp - smallExp;
	if( diff >= 55L )
		return large->d + small->d;

	large->u &= 0x000fffffffffffffULL;
	small->u &= 0x000fffffffffffffULL;
	large->u |= 0x3ff0000000000000ULL;
	small->u |= 0x3ff0000000000000ULL;

	//fix up denormals
	if( 0 == smallExp )
	{ 
		if( 0 == largeExp )
		{
			large->d -= 1.0;		
			largeExp = (large->u >> 52) - (1022);
			large->u &= 0x000fffffffffffffULL;
			large->u |= 0x3ff0000000000000ULL;
		}
		small->d -= 1.0;		
		smallExp = (small->u >> 52) - (1022);
		small->u &= 0x000fffffffffffffULL;
		small->u |= 0x3ff0000000000000ULL;
	}
	
	u[2].u = (1023ULL - largeExp + smallExp) << 52;
	small->d *= u[2].d;
	
	double r = __builtin_sqrt( large->d * large->d + small->d * small->d );
	
	if( largeExp < 0 )
	{
		largeExp += 1022;
		r *= 0x1.0p-1022;
	}
	
	u[2].u = largeExp << 52;
	r *= u[2].d;

	return r;
}
