/*
 *  xmm_fabs_copysign.c
 *  xmmLibm
 *
 *  Created by Ian Ollmann on 8/15/05.
 *  Copyright 2005 Apple Computer. All rights reserved.
 *
 */
 
#if defined( __i386__ )
#include "xmmLibm_prefix.h"

long double fabsl( long double x )
{
    return __builtin_fabsl( x );
}

double fabs( double x )
{
    xDouble xx = DOUBLE_2_XDOUBLE( x );
    xx = _mm_andnot_pd( minusZeroD, xx );
    return XDOUBLE_2_DOUBLE( xx ); 
}

float fabsf( float x )
{
    xFloat xx = FLOAT_2_XFLOAT( x );
    xx = _mm_andnot_ps( minusZeroF, xx );
    return XFLOAT_2_FLOAT( xx ); 
}

double copysign( double x, double y )
{   
    xDouble xx = DOUBLE_2_XDOUBLE( x );
    xDouble yy = DOUBLE_2_XDOUBLE( y );
    xx = _mm_sel_pd( xx, yy, minusZeroD );
    return XDOUBLE_2_DOUBLE( xx );
}

float copysignf( float x, float y )
{   
    xFloat xx = FLOAT_2_XFLOAT( x );
    xFloat yy = FLOAT_2_XFLOAT( y );
    xx = _mm_sel_ps( xx, yy, minusZeroF );
    return XFLOAT_2_FLOAT( xx );
}

long double copysignl( long double x, long double y )
{
    static const xUInt64 mask = { 0ULL, 0x8000ULL };
    xUInt64 xx = _mm_loadu_si128( (__m128i*) &x );
    xUInt64 yy = _mm_loadu_si128( (__m128i*) &y );
    union
    {
        long double ld;
        xUInt64     v;
    }u;

    //copy the sign
    yy = _mm_and_si128( yy, mask );
    xx = _mm_andnot_si128( mask, xx );
    xx = _mm_or_si128( xx, yy );

    //write back result
    u.v = xx;    
    return u.ld;
}

#endif /* defined( __i386__ ) */