/*
 *  xmm_misc.c
 *  xmmLibm
 *
 *  Created by Ian Ollmann on 8/18/05.
 *  Copyright 2005 Apple Computer. All rights reserved.
 *
 */

#include "xmmLibm_prefix.h"
#include "math.h"

#if defined(BUILDING_FOR_CARBONCORE_LEGACY)
 
double __inf ( void )
{
      return __builtin_inf();
}

#else /* BUILDING_FOR_CARBONCORE_LEGACY */

/******************************************************************************
*     No other functions are called by these routines outside of fpmacros.c.  *
******************************************************************************/

unsigned int __math_errhandling ( void )
{
    return (MATH_ERREXCEPT); // return the bitmask indicating the error discipline(s) in use.
}

int __isfinited( double x )
{
    x = __builtin_fabs(x);
    return x < __builtin_inf();
}

int __isfinitef( float x )
{
    x = __builtin_fabsf(x);
    return x < __builtin_inff();
}

int __isfinite( long double x )
{
    x = __builtin_fabsl(x);
    return x < __builtin_infl();
}

//legacy
int finite( double x )
{
	return __isfinited(  x );
}

int __isinfd( double x )
{
    x = __builtin_fabs(x);
    return x == __builtin_inf();
}

int __isinff( float x )
{
    x = __builtin_fabsf(x);
    return x == __builtin_inff();
}

int __isinf( long double x )
{
    x = __builtin_fabsl(x);
    return x == __builtin_infl();
}

int __isnand( double x )
{
    return x != x;
}

int __isnanf( float x )
{
    return x != x;
}

int __isnan( long double x )
{
    return x != x;
}

int __isnormald( double x )
{
    x = __builtin_fabs(x);
    return 0x1.0p-1022 <= x && x < __builtin_inf();
}

int __isnormalf( float x )
{
    x = __builtin_fabsf(x);
    return 0x1.0p-126f <= x && x < __builtin_inff();
}

int __isnormal( long double x )
{
    x = __builtin_fabsl(x);
    return 0x1.0p-16382L <= x && x < __builtin_infl();
}

int __signbitd( double x )
{
    xDouble xx = DOUBLE_2_XDOUBLE(x);
    return 1 & _mm_movemask_pd( xx );
}

int __signbitf( float x )
{
    xFloat xx = FLOAT_2_XFLOAT(x);
    return 1 & _mm_movemask_ps( xx );
}

int __signbitl( long double x )
{
    union
    {
        long double ld;
        struct
        {
            uint64_t    mantissa;
            uint16_t     sexp __attribute__ ((packed));
        }parts;
    }u={x};

    return u.parts.sexp >> 15;
}

int __fpclassifyd( double x )
{
    x = __builtin_fabs(x);
    if( EXPECT_FALSE( x == 0.0 ) )
        return FP_ZERO;
        
    if( EXPECT_FALSE( x < 0x1.0p-1022 ) )
        return FP_SUBNORMAL;
    
    if( EXPECT_TRUE( x < __builtin_inf() ) )
        return FP_NORMAL;
        
    if( EXPECT_TRUE( x == __builtin_inf() ) )
        return FP_INFINITE;

    return FP_NAN;
}

int __fpclassifyf( float x )
{
    x = __builtin_fabs(x);
    if( EXPECT_FALSE( x == 0.0f ) )
        return FP_ZERO;
        
    if( EXPECT_FALSE( x < 0x1.0p-126f ) )
        return FP_SUBNORMAL;
    
    if( EXPECT_TRUE( x < __builtin_inff() ) )
        return FP_NORMAL;
        
    if( EXPECT_TRUE( x == __builtin_inff() ) )
        return FP_INFINITE;

    return FP_NAN;
}

int __fpclassify( long double x )
{
    x = __builtin_fabsl(x);
    if( EXPECT_FALSE( x == 0.0L ) )
        return FP_ZERO;
        
    if( EXPECT_FALSE( x < 0x1.0p-16382L ) )
        return FP_SUBNORMAL;
    
    if( EXPECT_TRUE( x < __builtin_infl() ) )
        return FP_NORMAL;
        
    if( EXPECT_TRUE( x == __builtin_infl() ) )
        return FP_INFINITE;

    return FP_NAN;
}

float __inff( void )
{
    return __builtin_inff();
}

long double __infl( void )
{
    return __builtin_infl();
}

float __nan( void )
{
    return __builtin_nan("");
}




#endif /* BUILDING_FOR_CARBONCORE_LEGACY */

