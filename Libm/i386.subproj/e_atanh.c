
//
//	atanh
//
//		by Ian Ollmann
//
//	based on MathLib v3
//
//	Copyright © 2005, Apple Computer, Inc.  All Rights Reserved.
//

#include <math.h>

float atanhf( float x )
{
	if( x != x )	return x + x;
	
	float fabsx = __builtin_fabsf( x );

	if( fabsx > 1.0L)
		return sqrtf( -fabsx );
	
	if( fabsx >= 0x1.0p-12 )		//sqrt( negative epsilon )
	{
		fabsx = 0.5f * log1pf( (fabsx + fabsx) / (1.0f - fabsx) );
		
	}
	else
	{
		if( x == 0.0f )
			return x;
	
		fabsx *= 0x1.0p25;
		fabsx += 0x1.0p-126f;
		fabsx *= 0x1.0p-25;
	}

	if( x < 0 )
		fabsx = -fabsx;

	return fabsx;
}

double atanh( double x )
{
	if( x != x )	return x + x;
	
	double fabsx = __builtin_fabs( x );

	if( fabsx > 1.0)
		return sqrt( -fabsx );
	
	if( fabsx >= 0x1.0p-27 )		//sqrt( negative epsilon )
	{
		fabsx = 0.5 * log1p( (fabsx + fabsx) / (1.0 - fabsx) );
		
	}
	else
	{
		if( x == 0.0 )
			return x;
	
		fabsx *= 0x1.0p55;
		fabsx += 0x1.0p-1022;
		fabsx *= 0x1.0p-55;
	}

	if( x < 0 )
		fabsx = -fabsx;

	return fabsx;
}

long double atanhl( long double x )
{
	if( x != x )	return x + x;
	
	long double fabsx = __builtin_fabsl( x );

	if( fabsx > 1.0L)
		return sqrtl( -fabsx );
	
	if( fabsx >= 0x1.0p-32 )		//sqrt( negative epsilon )
	{
		fabsx = 0.5 * log1pl( (fabsx + fabsx) / (1.0L - fabsx) );
		
	}
	else
	{
		if( x == 0.0 )
			return x;
	
		fabsx *= 0x1.0p65;
		fabsx += 0x1.0p-16382L;
		fabsx *= 0x1.0p-65;
	}

	if( x < 0 )
		fabsx = -fabsx;

	return fabsx;
}

