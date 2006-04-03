/*
 * by Ian Ollmann
 * Copyright © 2005 by Apple Computer. All rights reserved.
 *
 *	Algorithm from mathLib v3
 */


#include "math.h"
#include "math_private.h"

float sinhf( float x )
{
    static const float overflow = 88;      //~ln(2) * (128)
    
    float fabsx = __builtin_fabsf( x );

	if( x != x )						return x + x;
	if( fabsx == __builtin_inff() )		return x;

	if( fabsx > 0x1.0p-12 )		//sqrt( negative epsilon )
	{
		if( fabsx < overflow )
		{
			fabsx = expm1f( fabsx );	
			fabsx = 0.5f * ( fabsx + fabsx / (1.0f + fabsx ) );
		}
		else
		{
			fabsx = expf( 0.5f * fabsx );
			fabsx = ( 0.5f * fabsx ) * fabsx;
		}
	}
	else
	{
		if( x == 0.0f )
			return x;
	
		//set inexact and underflow, if necessary
		fabsx *= 0x1.0p25f;
		fabsx += 0x1.0p-126f;
		fabsx *= 0x1.0p-25f;		
	}

	if( x < 0.0f )
		fabsx = -fabsx;
	
	return fabsx;
}

double sinh( double x )
{
    static const double overflow = 709;      //~ln(2) * (1024)
    
    double fabsx = __builtin_fabs( x );

	if( x != x )						return x + x;
	if( fabsx == __builtin_inf() )		return x;

	if( fabsx > 0x1.0p-27 )		//sqrt( negative epsilon )
	{
		if( fabsx < overflow )
		{
			fabsx = expm1( fabsx );	
			fabsx = 0.5 * ( fabsx + fabsx / (1.0 + fabsx ) );
		}
		else
		{
			fabsx = exp( 0.5 * fabsx );
			fabsx = ( 0.5 * fabsx ) * fabsx;
		}
	}
	else
	{
		if( x == 0.0 )
			return x;
	
		//set inexact and underflow, if necessary
		fabsx *= 0x1.0p55;
		fabsx += 0x1.0p-1022;
		fabsx *= 0x1.0p-55;		
	}

	if( x < 0.0 )
		fabsx = -fabsx;
	
	return fabsx;
}


long double sinhl( long double x )
{
    static const long double overflow = 11356;      //~ln(2)*16384
    
    long double fabsx = __builtin_fabsl( x );

	if( x != x )						return x + x;
	if( fabsx == __builtin_infl() )		return x;

	if( fabsx > 0x1.0p-32 )		//sqrt( negative epsilon )
	{
		if( fabsx < overflow )
		{
			fabsx = expm1l( fabsx );	
			fabsx = 0.5L * ( fabsx + fabsx / (1.0L + fabsx ) );
		}
		else
		{
			fabsx = expl( 0.5L * fabsx );
			fabsx = ( 0.5L * fabsx ) * fabsx;
		}
	}
	else
	{
		if( x == 0.0L )
			return x;
	
		//set inexact and underflow, if necessary
		fabsx *= 0x1.0p67;
		fabsx += 0x1.0p-16382L;
		fabsx *= 0x1.0p-67;		
	}

	if( x < 0.0 )
		fabsx = -fabsx;
	
	return fabsx;
}
