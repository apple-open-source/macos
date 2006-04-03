
//
//	asinh
//
//		by Ian Ollmann
//
//	Based on algorithms from MathLib v3
//
//	Copyright © 2005, Apple Computer Inc.  All Rights Reserved.
//

#include <math.h>


float asinhf( float x )
{
	static const float ln2 = 0x1.62e42fefa39fa39ep-1f;			//ln(2)

	if( x != x )	return x + x;
	
	long  double fabsx = __builtin_fabsf( x );
	
	if( fabsx < 0x1.0p-12f )			//sqrt( negative epsilon )
	{
		if( x == 0.0f )
			return x;
	
		fabsx *= 0x1.0p25;
		fabsx -= 0x1.0p-126f;
		fabsx *= 0x1.0p-25;
	}
	else if( fabsx <= 4.0L / 3.0L )
	{
		float r = 1.0f / fabsx;
		
		fabsx = log1pf( fabsx + fabsx / ( r + sqrtf( 1 + r * r)) );
	}
	else if( fabsx <= 0x1.0p32 )		//1/sqrt( negative epsilon )
	{
		fabsx = logf( fabsx + fabsx + 1.0f / (fabsx + sqrtf( 1.0f + fabsx * fabsx)) );
	}
	else
		fabsx = logf( fabsx ) + ln2;

	if( x < 0 )
		fabsx = -fabsx;
		
	return fabsx;
}


double asinh( double x )
{
	static const double ln2 = 0x1.62e42fefa39fa39ep-1;			//ln(2)

	if( x != x )	return x + x;
	
	long  double fabsx = __builtin_fabs( x );
	
	if( fabsx < 0x1.0p-27 )			//sqrt( negative epsilon )
	{
		if( x == 0.0L )
			return x;
	
		fabsx *= 0x1.0p55;
		fabsx -= 0x1.0p-1022;
		fabsx *= 0x1.0p-55;
	}
	else if( fabsx <= 4.0 / 3.0 )
	{
		double r = 1.0 / fabsx;
		
		fabsx = log1p( fabsx + fabsx / ( r + sqrt( 1 + r * r)) );
	}
	else if( fabsx <= 0x1.0p27 )		//1/sqrt( negative epsilon )
	{
		fabsx = log( fabsx + fabsx + 1.0 / (fabsx + sqrt( 1.0 + fabsx * fabsx)) );
	}
	else
		fabsx = log( fabsx ) + ln2;

	if( x < 0 )
		fabsx = -fabsx;
		
	return fabsx;
}


long double asinhl( long double x )
{
	static const long double ln2 = 0x1.62e42fefa39fa39ep-1L;			//ln(2)

	if( x != x )	return x + x;
	
	long  double fabsx = __builtin_fabsl( x );
	
	if( fabsx < 0x1.0p-32 )			//sqrt( negative epsilon )
	{
		if( x == 0.0L )
			return x;
	
		fabsx *= 0x1.0p65L;
		fabsx -= 0x1.0p-16382L;
		fabsx *= 0x1.0p-65L;
	}
	else if( fabsx <= 4.0L / 3.0L )
	{
		long double r = 1.0L / fabsx;
		
		fabsx = log1pl( fabsx + fabsx / ( r + sqrtl( 1 + r * r)) );
	}
	else if( fabsx <= 0x1.0p32 )		//1/sqrt( negative epsilon )
	{
		fabsx = logl( fabsx + fabsx + 1.0L / (fabsx + sqrtl( 1.0L + fabsx * fabsx)) );
	}
	else
		fabsx = logl( fabsx ) + ln2;

	if( x < 0 )
		fabsx = -fabsx;
		
	return fabsx;
}
