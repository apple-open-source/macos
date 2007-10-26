/*
 *  remainder.c
 *  xmmLibm
 *
 *  Created by Ian Ollmann on 8/29/05.
 *  Copyright 2005 Apple Computer. All rights reserved.
 *
 *  These functions are based on the algorithms from MathLib V3,
 *  Job Okada.
 */

#include "xmmLibm_prefix.h"
#include "math.h"
#include "fenv.h"

#define REM_NAN "9"

static const double infinityD = __builtin_inf();
static const double minNormD = 0x1.0p-1022;
static const double hugeHalvedD = 0x1.0p1023;
static const double largeD = 0x1.0p1022;
static const double oneD = 1.0;
static const xSInt64 denormBiasD = { 1022LL, 0LL }; 

static const float infinityF = __builtin_inff();
static const float minNormF = 0x1.0p-126f;
static const xSInt32 denormBiasF = { 126, 0,0,0 }; 
static const float oneF = 1.0f;
static const float hugeHalvedF = 0x1.0p127f;

long double __remquol( long double x, long double y, int *quo );
long double __fmodl( long double x, long double y );

#if defined( BUILDING_FOR_CARBONCORE_LEGACY )

/*
double remquo( double x, double y, int *quo )
{
    *quo = 0;
    
    xDouble xx = DOUBLE_2_XDOUBLE( x );
    xDouble yy = DOUBLE_2_XDOUBLE( y );
    xDouble fabsx = _mm_andnot_pd( minusZeroD, xx );
    xDouble fabsy = _mm_andnot_pd( minusZeroD, yy );
    xDouble infinity = _mm_load_sd( &infinityD );
    xDouble one = _mm_load_sd( &oneD );
    
    if( _mm_istrue_sd( _mm_cmpunord_sd( xx, yy ) ))
    {
        fabsx = _mm_add_sd( xx, yy );
		return XDOUBLE_2_DOUBLE( fabsx );
    }
    else if( y == 0.0 || __builtin_fabs(x) == infinityD )
    {
        feraiseexcept( FE_INVALID );
        return nan( REM_NAN );
    }
    
	xDouble xIsFinite = _mm_andnot_pd( _mm_cmpeq_sd( xx, minusZeroD ), _mm_cmplt_sd( fabsx, infinity ) );
	xDouble yIsFinite = _mm_andnot_pd( _mm_cmpeq_sd( yy, minusZeroD ), _mm_cmplt_sd( fabsy, infinity ) );

    if( _mm_istrue_sd( _mm_and_pd( xIsFinite, yIsFinite)))
    {
        xSInt64 iquo = (xSInt64) _mm_xor_pd( fabsx, fabsx );        //0

    //get logb(x) and logb(y), scale x and y to unit binade
        //first normalize denormals
        xDouble xIsDenorm = _mm_cmplt_sdm( fabsx, &minNormD );
        xDouble yIsDenorm = _mm_cmplt_sdm( fabsy, &minNormD );
        xDouble xexp = _mm_and_pd( xIsDenorm, one );
        xDouble yexp = _mm_and_pd( yIsDenorm, one );
        xDouble normalX = _mm_or_pd( fabsx, xexp );
        xDouble normalY = _mm_or_pd( fabsy, yexp );
        normalX = _mm_sub_sd( normalX, xexp );
        normalY = _mm_sub_sd( normalY, yexp );
        
        //get the difference in exponents
        xSInt64 expX = _mm_srli_epi64( (xSInt64) normalX, 52 );
        xSInt64 expY = _mm_srli_epi64( (xSInt64) normalY, 52 );        
        expX = _mm_sub_epi64( expX, _mm_and_si128( (xSInt64) xIsDenorm, denormBiasD ) );
        expY = _mm_sub_epi64( expY, _mm_and_si128( (xSInt64) yIsDenorm, denormBiasD ) );
        int diff = _mm_cvtsi128_si32( _mm_sub_epi64( expX, expY ) );

        if( diff >= 0 )
        {
            if( diff > 0 )
            {
                //replace exponents with 1.0
                xDouble xScale = _mm_and_pd( normalY, infinity );                
                xDouble y1 = _mm_andnot_pd( infinity, normalY );
                xDouble yDenormScale = _mm_sel_pd( one, _mm_load_sd( &minNormD ), yIsDenorm );
                normalX = _mm_andnot_pd( infinity, normalX );
                y1 = _mm_or_pd( y1, one );
                normalX = _mm_or_pd( normalX, one );

                //iterate 
                do
                {
                    xDouble yLTx = _mm_cmple_sd( y1, normalX );
                    normalX = _mm_sub_sd( normalX, _mm_and_pd(y1, yLTx) );
                    iquo = _mm_sub_epi64( iquo, (xSInt64) yLTx );
                    normalX = _mm_add_sd( normalX, normalX );
                    iquo = _mm_add_epi64( iquo, iquo );
                }
                while( --diff > 0 );
                
                //scale fabsx scale remainder to binade of |y|
                fabsx = _mm_mul_sd( normalX, xScale );
                fabsx = _mm_mul_sd( fabsx, yDenormScale );
            }

            xDouble fabsyLTfabsx = _mm_cmple_sd( fabsy, fabsx );
            fabsx = _mm_sub_sd( fabsx, _mm_and_pd(fabsy, fabsyLTfabsx) );
            iquo = _mm_sub_epi64( iquo, (xSInt64) fabsyLTfabsx );
        }
        
//         if (likely( x1 < HugeHalved.d ))
//            z = fabsx + fabsx;                              // double remainder, without overflow 
//         else
//            z = Huge.d;
        xDouble z = _mm_cmplt_sdm( fabsx, &hugeHalvedD );
        z = _mm_sel_pd( infinity, fabsx, z );
        z = _mm_add_sd( z, fabsx );

//         if ((z > absy) || ((z == absy) && ((iquo & 1) != 0))) {
//              x1 -= absy;                             // final remainder correction 
//              iquo += 1;
//         }
        xDouble fabsyLTz = _mm_cmplt_sd( fabsy, z );
        xDouble fabsyEQz = _mm_cmpeq_sd( fabsy, z );
        xDouble iquoISodd = _mm_cmplt_sd(_mm_or_pd( (xDouble) _mm_slli_epi64( iquo, 63), one ), one );
        xDouble test = _mm_or_pd( _mm_and_pd( fabsyEQz, iquoISodd), fabsyLTz );
        fabsx = _mm_sub_sd( fabsx, _mm_and_pd( fabsy, test ) );
        iquo = _mm_sub_epi64( iquo, (xSInt64) test );
        
//      if (x < 0.0)
//              x1 = -x1;                               // remainder if x is negative 
        fabsx = _mm_xor_pd( fabsx, _mm_and_pd( xx, minusZeroD ) );

//         iquo &= 0x0000007f;                          // retain low 7 bits of integer quotient      
        static const xSInt64 low7Bits = { 0x7fLL, 0LL };
        iquo = _mm_and_si128( iquo, low7Bits );

//         if ((___signbitd(x) ^ ___signbitd(y)) != 0)    // take care of sign of quotient 
//              iquo = -iquo;
        xDouble xLT0 = _mm_cmplt_sd( xx, minusZeroD );
        xDouble yLT0 = _mm_cmplt_sd( yy, minusZeroD );
        xSInt64 sign = (xSInt64) _mm_xor_pd( xLT0, yLT0 );
        iquo = _mm_xor_si128( iquo, sign );
        iquo = _mm_sub_epi64( iquo, sign );

        *quo = _mm_cvtsi128_si32( iquo );
    }
    else 
    {
        fabsx = xx;
    }

    return XDOUBLE_2_DOUBLE( fabsx );
}
*/


double remquo( double x, double y, int *quo )
{
	return __remquol( x, y, quo );
}

#else

double remainder( double x, double y )
{
	int quo;
	return __remquol( x, y, &quo );
}


/*
float remquof( float x, float y, int *quo )
{
    *quo = 0;
    
    xFloat xx = FLOAT_2_XFLOAT( x );
    xFloat yy = FLOAT_2_XFLOAT( y );
    xFloat fabsx = _mm_andnot_ps( minusZeroF, xx );
    xFloat fabsy = _mm_andnot_ps( minusZeroF, yy );
    xFloat infinity = _mm_load_ss( &infinityF );
    xFloat one = _mm_load_ss( &oneF );
    
    if( _mm_istrue_ss( _mm_cmpunord_ss( xx, yy ) ))
    {
        fabsx = _mm_add_ss( xx, yy );
		return XFLOAT_2_FLOAT( fabsx );
    }
    else if( y == 0.0f || __builtin_fabsf(x) == infinityF )
    {
        feraiseexcept( FE_INVALID );
        return nan( REM_NAN );
    }
    
	xFloat xIsFinite = _mm_andnot_ps( _mm_cmpeq_ss( xx, minusZeroF ), _mm_cmplt_ss( fabsx, infinity ) );
	xFloat yIsFinite = _mm_andnot_ps( _mm_cmpeq_ss( yy, minusZeroF ), _mm_cmplt_ss( fabsy, infinity ) );

    if( _mm_istrue_ss( _mm_and_ps( xIsFinite, yIsFinite)))
    {
        xSInt32 iquo = (xSInt32) _mm_xor_ps( fabsx, fabsx );        //0

    //get logb(x) and logb(y), scale x and y to unit binade
        //first normalize denormals
        xFloat xIsDenorm = _mm_cmplt_ssm( fabsx, &minNormF );
        xFloat yIsDenorm = _mm_cmplt_ssm( fabsy, &minNormF );
        xFloat xexp = _mm_and_ps( xIsDenorm, one );
        xFloat yexp = _mm_and_ps( yIsDenorm, one );
        xFloat normalX = _mm_or_ps( fabsx, xexp );
        xFloat normalY = _mm_or_ps( fabsy, yexp );
        normalX = _mm_sub_ss( normalX, xexp );
        normalY = _mm_sub_ss( normalY, yexp );
        
        //get the difference in exponents
        xSInt32 expX = _mm_srli_epi32( (xSInt32) normalX, 23 );
        xSInt32 expY = _mm_srli_epi32( (xSInt32) normalY, 23 );        
        expX = _mm_sub_epi32( expX, _mm_and_si128( (xSInt32) xIsDenorm, denormBiasF ) );
        expY = _mm_sub_epi32( expY, _mm_and_si128( (xSInt32) yIsDenorm, denormBiasF ) );
        int diff = _mm_cvtsi128_si32( _mm_sub_epi32( expX, expY ) );

        if( diff >= 0 )
        {
            if( diff > 0 )
            {
                //replace exponents with 1.0
                xFloat xScale = _mm_and_ps( normalY, infinity );                
                xFloat y1 = _mm_andnot_ps( infinity, normalY );
                xFloat yDenormScale = _mm_sel_ps( one, _mm_load_ss( &minNormF ), yIsDenorm );
                normalX = _mm_andnot_ps( infinity, normalX );
                y1 = _mm_or_ps( y1, one );
                normalX = _mm_or_ps( normalX, one );

                //iterate 
                do
                {
                    xFloat yLTx = _mm_cmple_ss( y1, normalX );
                    normalX = _mm_sub_ss( normalX, _mm_and_ps(y1, yLTx) );
                    iquo = _mm_sub_epi32( iquo, (xSInt32) yLTx );
                    normalX = _mm_add_ss( normalX, normalX );
                    iquo = _mm_add_epi32( iquo, iquo );
                }
                while( --diff > 0 );
                
                //scale fabsx scale remainder to binade of |y|
                fabsx = _mm_mul_ss( normalX, xScale );
                fabsx = _mm_mul_ss( fabsx, yDenormScale );
            }

            xFloat fabsyLTfabsx = _mm_cmple_ss( fabsy, fabsx );
            fabsx = _mm_sub_ss( fabsx, _mm_and_ps(fabsy, fabsyLTfabsx) );
            iquo = _mm_sub_epi32( iquo, (xSInt32) fabsyLTfabsx );
        }
        
//         if (likely( x1 < HugeHalved.d ))
//            z = fabsx + fabsx;                              // double remainder, without overflow 
//         else
//            z = Huge.d;
        xFloat z = _mm_cmplt_ssm( fabsx, &hugeHalvedF );
        z = _mm_sel_ps( infinity, fabsx, z );
        z = _mm_add_ss( z, fabsx );

//         if ((z > absy) || ((z == absy) && ((iquo & 1) != 0))) {
//              x1 -= absy;                             // final remainder correction 
//              iquo += 1;
//         }
        xFloat fabsyLTz = _mm_cmplt_ss( fabsy, z );
        xFloat fabsyEQz = _mm_cmpeq_ss( fabsy, z );
        xFloat iquoISodd = _mm_cmplt_ss(_mm_or_ps( (xFloat) _mm_slli_epi32( iquo, 31), one ), one );
        xFloat test = _mm_or_ps( _mm_and_ps( fabsyEQz, iquoISodd), fabsyLTz );
        fabsx = _mm_sub_ss( fabsx, _mm_and_ps( fabsy, test ) );
        iquo = _mm_sub_epi32( iquo, (xSInt32) test );
        
//      if (x < 0.0)
//              x1 = -x1;                               // remainder if x is negative 
        fabsx = _mm_xor_ps( fabsx, _mm_and_ps( xx, minusZeroF ) );

//         iquo &= 0x0000007f;                          // retain low 7 bits of integer quotient         
        static const xSInt32 low7Bits = { 0x7f, 0,0,0 };
        iquo = _mm_and_si128( iquo, low7Bits );

//         if ((___signbitd(x) ^ ___signbitd(y)) != 0)    // take care of sign of quotient 
//              iquo = -iquo;
        xFloat xLT0 = _mm_cmplt_ss( xx, minusZeroF );
        xFloat yLT0 = _mm_cmplt_ss( yy, minusZeroF );
        xSInt64 sign = (xSInt64) _mm_xor_ps( xLT0, yLT0 );
        iquo = _mm_xor_si128( iquo, sign );
        iquo = _mm_sub_epi64( iquo, sign );

        *quo = _mm_cvtsi128_si32( iquo );
    }
    else 
    {
        fabsx = xx;
    }

    return XFLOAT_2_FLOAT( fabsx );
}
*/

float remquof( float x, float y, int *quo )
{
	return __remquol( x, y, quo );
}

float remainderf( float x, float y )
{
	int quo;
	return __remquol( x, y, &quo );
}

double fmod( double x, double y )
{
	return __fmodl( x, y );
}

float fmodf( float x, float y )
{
	return __fmodl( x, y );
}

/*
double fmod( double x, double y )
{
    xDouble xx = DOUBLE_2_XDOUBLE( x );
    xDouble yy = DOUBLE_2_XDOUBLE( y );
    xDouble fabsx = _mm_andnot_pd( minusZeroD, xx );
    xDouble fabsy = _mm_andnot_pd( minusZeroD, yy );
    xDouble infinity = _mm_load_sd( &infinityD );
    xDouble one = _mm_load_sd( &oneD );
    
    if( _mm_istrue_sd( _mm_cmpunord_sd( xx, yy ) ))
    {
        fabsx = _mm_add_sd( xx, yy );
		return XDOUBLE_2_DOUBLE( fabsx );
    }
    else if( y == 0.0 || __builtin_fabs(x) == infinityD )
    {
        feraiseexcept( FE_INVALID );
        return nan( REM_NAN );
    }
        
	xDouble xIsFinite = _mm_andnot_pd( _mm_cmpeq_sd( xx, minusZeroD ), _mm_cmplt_sd( fabsx, infinity ) );
	xDouble yIsFinite = _mm_andnot_pd( _mm_cmpeq_sd( yy, minusZeroD ), _mm_cmplt_sd( fabsy, infinity ) );

    if( _mm_istrue_sd( _mm_and_pd( xIsFinite, yIsFinite)))
    {
        if( _mm_istrue_sd( _mm_cmplt_sd( fabsx, fabsy ) ) )
            return x;

    //get logb(x) and logb(y), scale x and y to unit binade
        //first normalize denormals
        xDouble xIsDenorm = _mm_cmplt_sdm( fabsx, &minNormD );
        xDouble yIsDenorm = _mm_cmplt_sdm( fabsy, &minNormD );
        xDouble xexp = _mm_and_pd( xIsDenorm, one );
        xDouble yexp = _mm_and_pd( yIsDenorm, one );
        xDouble normalX = _mm_or_pd( fabsx, xexp );
        xDouble normalY = _mm_or_pd( fabsy, yexp );
        normalX = _mm_sub_sd( normalX, xexp );
        normalY = _mm_sub_sd( normalY, yexp );
        
        //get the difference in exponents
        xSInt64 expX = _mm_srli_epi64( (xSInt64) normalX, 52 );
        xSInt64 expY = _mm_srli_epi64( (xSInt64) normalY, 52 );        
        expX = _mm_sub_epi64( expX, _mm_and_si128( (xSInt64) xIsDenorm, denormBiasD ) );
        expY = _mm_sub_epi64( expY, _mm_and_si128( (xSInt64) yIsDenorm, denormBiasD ) );
        int diff = _mm_cvtsi128_si32( _mm_sub_epi64( expX, expY ) );

        if( diff >= 0 )
        {
            if( diff > 0 )
            {
                //replace exponents with 1.0
                xDouble xScale = _mm_and_pd( normalY, infinity );                
                xDouble y1 = _mm_andnot_pd( infinity, normalY );
                xDouble yDenormScale = _mm_sel_pd( one, _mm_load_sd( &minNormD ), yIsDenorm );
                normalX = _mm_andnot_pd( infinity, normalX );
                y1 = _mm_or_pd( y1, one );
                normalX = _mm_or_pd( normalX, one );

                //iterate 
                do
                {
                    xDouble yLTx = _mm_cmple_sd( y1, normalX );
                    normalX = _mm_sub_sd( normalX, _mm_and_pd(y1, yLTx) );
                    normalX = _mm_add_sd( normalX, normalX );
                }
                while( --diff > 0 );
                
                //scale fabsx scale remainder to binade of |y|
                fabsx = _mm_mul_sd( normalX, xScale );
                fabsx = _mm_mul_sd( fabsx, yDenormScale );
            }

            xDouble fabsyLTfabsx = _mm_cmple_sd( fabsy, fabsx );
            fabsx = _mm_sub_sd( fabsx, _mm_and_pd(fabsy, fabsyLTfabsx) );
        }
        
//      if (x < 0.0)
//              x1 = -x1;                               // remainder if x is negative 
        fabsx = _mm_xor_pd( fabsx, _mm_and_pd( xx, minusZeroD ) );
    }
    else 
    {
        fabsx = xx;
    }

    return XDOUBLE_2_DOUBLE( fabsx );
}


float fmodf( float x, float y )
{
    xFloat xx = FLOAT_2_XFLOAT( x );
    xFloat yy = FLOAT_2_XFLOAT( y );
    xFloat fabsx = _mm_andnot_ps( minusZeroF, xx );
    xFloat fabsy = _mm_andnot_ps( minusZeroF, yy );
    xFloat infinity = _mm_load_ss( &infinityF );
    xFloat one = _mm_load_ss( &oneF );
    
    if( _mm_istrue_ss( _mm_cmpunord_ss( xx, yy ) ))
    {
        fabsx = _mm_add_ss( xx, yy );
		return XFLOAT_2_FLOAT( fabsx );
    }
    else if( y == 0.0 || __builtin_fabsf(x) == infinityF )
    {
        feraiseexcept( FE_INVALID );
        return nanf( REM_NAN );
    }
        
	xFloat xIsFinite = _mm_andnot_ps( _mm_cmpeq_ss( xx, minusZeroF ), _mm_cmplt_ss( fabsx, infinity ) );
	xFloat yIsFinite = _mm_andnot_ps( _mm_cmpeq_ss( yy, minusZeroF ), _mm_cmplt_ss( fabsy, infinity ) );

    if( _mm_istrue_ss( _mm_and_ps( xIsFinite, yIsFinite)))
    {
        if( _mm_istrue_ss( _mm_cmplt_ss( fabsx, fabsy ) ) )
            return x;

    //get logb(x) and logb(y), scale x and y to unit binade
        //first normalize denormals
        xFloat xIsDenorm = _mm_cmplt_ssm( fabsx, &minNormF );
        xFloat yIsDenorm = _mm_cmplt_ssm( fabsy, &minNormF );
        xFloat xexp = _mm_and_ps( xIsDenorm, one );
        xFloat yexp = _mm_and_ps( yIsDenorm, one );
        xFloat normalX = _mm_or_ps( fabsx, xexp );
        xFloat normalY = _mm_or_ps( fabsy, yexp );
        normalX = _mm_sub_ss( normalX, xexp );
        normalY = _mm_sub_ss( normalY, yexp );
        
        //get the difference in exponents
        xSInt32 expX = _mm_srli_epi32( (xSInt32) normalX, 23 );
        xSInt32 expY = _mm_srli_epi32( (xSInt32) normalY, 23 );        
        expX = _mm_sub_epi32( expX, _mm_and_si128( (xSInt32) xIsDenorm, denormBiasF ) );
        expY = _mm_sub_epi32( expY, _mm_and_si128( (xSInt32) yIsDenorm, denormBiasF ) );
        int diff = _mm_cvtsi128_si32( _mm_sub_epi32( expX, expY ) );

        if( diff >= 0 )
        {
            if( diff > 0 )
            {
                //replace exponents with 1.0
                xFloat xScale = _mm_and_ps( normalY, infinity );                
                xFloat y1 = _mm_andnot_ps( infinity, normalY );
                xFloat yDenormScale = _mm_sel_ps( one, _mm_load_ss( &minNormF ), yIsDenorm );
                normalX = _mm_andnot_ps( infinity, normalX );
                y1 = _mm_or_ps( y1, one );
                normalX = _mm_or_ps( normalX, one );

                //iterate 
                do
                {
                    xFloat yLTx = _mm_cmple_ss( y1, normalX );
                    normalX = _mm_sub_ss( normalX, _mm_and_ps(y1, yLTx) );
                    normalX = _mm_add_ss( normalX, normalX );
                }
                while( --diff > 0 );
                
                //scale fabsx scale remainder to binade of |y|
                fabsx = _mm_mul_ss( normalX, xScale );
                fabsx = _mm_mul_ss( fabsx, yDenormScale );
            }

            xFloat fabsyLTfabsx = _mm_cmple_ss( fabsy, fabsx );
            fabsx = _mm_sub_ss( fabsx, _mm_and_ps(fabsy, fabsyLTfabsx) );
        }
        
//      if (x < 0.0)
//              x1 = -x1;                               // remainder if x is negative 
        fabsx = _mm_xor_ps( fabsx, _mm_and_ps( xx, minusZeroF ) );
    }
    else 
    {
        fabsx = xx;
    }

    return XFLOAT_2_FLOAT( fabsx );
}
*/



#endif /* CARBONCORE_LEGACY */
