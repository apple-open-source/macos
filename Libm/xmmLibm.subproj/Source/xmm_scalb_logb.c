/*
 *  xmm_scalb_logb.c
 *  xmmLibm
 *
 *  Created by Ian Ollmann on 8/5/05.
 *  Copyright 2005 Apple Computer Inc. All rights reserved.
 *
 */

#if defined( __i386__ )

#include "xmmLibm_prefix.h"
#include <math.h>

double scalbn( double x, int n );
float scalbnf( float x, int n );
double scalbln( double x, long int n );
float scalblnf( float x, long int n );
int ilogb( double x );
int ilogbf( float x );
double ldexp( double x, int exp );
float ldexpf( float x, int exp ); 

#pragma mark -

#if ! defined( BUILDING_FOR_CARBONCORE_LEGACY )

int ilogb( double x )
{
    static const double infinity = __builtin_inf();                  //0x7F800000
    static const double denormBias = 2.0;    
    static const xSInt64 doublebias = {1023, 0}; 
    
    xUInt64 correction;                                                                          

    //Do some introspection about X
	xDouble X = DOUBLE_2_XDOUBLE( x );
    xDouble  absX = _mm_andnot_pd( minusZeroD, X );                                                // fabs(X)
	xDouble inf = _mm_load_sd( &infinity);
    xDouble  exponent = _mm_and_pd( X, inf );                                                       // exponent of X, in place
    xDouble isExpZero = _mm_cmpeq_sd( exponent, minusZeroD );										// -1 if exponent of X is 0, 0 otherwise
    xDouble isZero = _mm_cmpeq_sd( absX, minusZeroD );                                           // -1 if X is 0, 0 otherwise
    xDouble isDenormal = _mm_andnot_pd( isZero, isExpZero );                                         // -1 if X is a (non-zero) denormal, 0 otherwise 
    xDouble isInf = _mm_cmpeq_sd( absX, inf );                              // -1 if X is +- Inf, 0 otherwise
    xDouble isNaN = _mm_cmpunord_sd( X, X );                                                  // -1 if X is a NaN, 0 otherwise
	xDouble isSpecial = _mm_or_pd( isZero, isInf );
	isSpecial = _mm_or_pd( isSpecial, isNaN );

    //Add in the bias so denormals come out right, shift, convert to double and then remove the bias if any.
    //Solves for normals and denormals
    xDouble  bias = _mm_and_pd( (xDouble) isDenormal, _mm_load_sd( &denormBias) );                                   // exponent 23 (-127) if denormal, 0 otherwise
    xDouble  biasedExponent = _mm_or_pd( bias, absX );
	biasedExponent = _mm_sub_pd( biasedExponent, bias );
    xUInt64 shiftedBiasedExponent = _mm_srli_epi64( (xUInt64) biasedExponent, 52 );
    xUInt64  reverseBias = _mm_add_epi64( _mm_and_si128( (xUInt64) isDenormal, doublebias ), doublebias );
	shiftedBiasedExponent = _mm_sub_epi64( shiftedBiasedExponent, reverseBias );
    
    //At this point we have normals and denormals taken care of. We need to fix zero, Inf and NaN
    //This is done by adding by a correction value, as appropriate:
    // normals:     0.0f
    // denormals:   0.0f
    // +-zero:      -INF
    // +-inf:       INF
    // NaN:         X

    //Set the Zero case first
	isZero = _mm_or_pd( isZero, ( xDouble) _mm_cvtpd_epi32( isZero ) );	//set invalid flag if zero
    correction = _mm_and_si128( (xUInt64) isZero, _mm_cvtsi32_si128( FP_ILOGB0 ) );                                      // -INF if X is zero, 0 otherwise
    
    //Set the +- inf case 
    correction = _mm_or_si128( correction, _mm_srli_epi64( (xUInt64) isInf, 33 ));							// OR in INT_MAX if X is infinite
    
    //Do NaNs 
    correction = _mm_or_si128( correction, _mm_and_si128( (xUInt64) isNaN, _mm_cvtsi32_si128( FP_ILOGBNAN ) ) );                 // OR in X if X is NaN
    
    //deal with zero, inf and NaN
    shiftedBiasedExponent = _mm_andnot_si128( (xUInt64) isSpecial, shiftedBiasedExponent ); 
	shiftedBiasedExponent = _mm_or_si128( shiftedBiasedExponent, correction );

    return _mm_cvtsi128_si32( shiftedBiasedExponent );
}


double scalbn( double x, int n )
{
    const double estep[2] = { 0x1.0p-1022, 0x1.0p1022 };
    const int    istep[2] = { -1022, 1022 };
    
    int index = n >> (sizeof(n) * 8 - 1);
    int step = istep[index+1];
    unsigned int value = abs(n);
    xDouble xx = _mm_load_sd( &x );
    xDouble xstep = _mm_load_sd( estep + index + 1 );
    double result;

    if( value > 1022 )
    {
        n -= step;
        value -= 1022;
        xx = _mm_mul_sd( xx, xstep );

        if( value > 1022 )
        {
            n -= step;
            value -= 1022;
            xx = _mm_mul_sd( xx, xstep );

            if( value > 1022 )  //always Inf or 0 if true
            {
                xx = _mm_mul_sd( xx, xstep );
                _mm_store_sd( &result, xx );
                return result;
            }
        }
    }

    xSInt64 exponent = _mm_cvtsi32_si128( n + 1023 );
    xDouble scale = (xDouble) _mm_slli_epi64( exponent, 52 );
    scale = _mm_andnot_pd( minusZeroD, scale );
    xx = _mm_mul_sd( xx, scale );
    _mm_store_sd( &result, xx );

    return result;
}

float scalbnf( float x, int n )
{
    const float  estep[2] = { 0x1.0p-126f, 0x1.0p126f };
    const int    istep[2] = { -126, 126 };
    const xFloat negZeroF = { -0.0f, 0.0, 0.0, 0.0 };
    
    int index = n >> (sizeof(n) * 8 - 1);
    int step = istep[index+1];
    unsigned int value = abs(n);
    xFloat xx = _mm_load_ss( &x );
    xFloat xstep = _mm_load_ss( estep + index + 1 );
    float result;

    if( value > 126 )
    {
        n -= step;
        value -= 126;
        xx = _mm_mul_ss( xx, xstep );

        if( value > 126 )
        {
            n -= step;
            value -= 126;
            xx = _mm_mul_ss( xx, xstep );

            if( value > 126 )  //always Inf or 0 if true
            {
                xx = _mm_mul_ss( xx, xstep );
                _mm_store_ss( &result, xx );
                return result;
            }
        }
    }

    xSInt32 exponent = _mm_cvtsi32_si128( n + 127 );
    xFloat scale = (xFloat) _mm_slli_epi32( exponent, 23 );
    scale = _mm_andnot_ps( negZeroF, scale );
    xx = _mm_mul_ss( xx, scale );
    _mm_store_ss( &result, xx );

    return result;
}

long double scalblnl( long double x, long int n )
{
	if( n > 300000 )	n = 300000;
	if( n < -300000 ) n = -300000;
	
	return scalbnl( x, n );
}

double scalbln( double x, long int n )
{
    if( n > 3000 ) n = 3000;
    if( n < -3000 ) n = -3000;

    return scalbn( x, n );
}

float scalblnf( float x, long int n )
{
    if( n > 300 ) n = 300;
    if( n < -300 ) n = -300;

    return scalbnf( x, n );
}

#warning scalb$UNIX2003 removed because of compiler bug <rdar://problem/4306561>
/*
double scalb$UNIX2003( double x, double n )
{
    if( n > 3000 ) n = 3000;
    if( n < -3000 ) n = -3000;

    return scalbn( x, n );
}

float scalbf$UNIX2003( float x, double n )
{
    if( n > 300 ) n = 300;
    if( n < -300 ) n = -300;

    return scalbnf( x, n );
}
*/

double scalb( double x, double n )
{
    if( n > 3000 ) n = 3000;
    if( n < -3000 ) n = -3000;

    return scalbn( x, n );
}


//
//  We return INT_MIN for NaNs and zero
//


int ilogbf( float x )
{
    static const float infinity = __builtin_inff();                  //0x7F800000
    static const float denormBias = 2.0f;    
    static const xSInt32 oneTwentySeven = {127, 0, 0, 0}; 
    
    xUInt32 zero = (xUInt32) _mm_setzero_ps();                                                          // 0
    xUInt32 correction;                                                                          

    //Do some introspection about X
	xFloat X = FLOAT_2_XFLOAT( x );
    xFloat  absX = _mm_andnot_ps( minusZeroF, X );                                                // fabs(X)
	xFloat inf = _mm_load_ss( &infinity);
    xFloat  exponent = _mm_and_ps( X, inf );                                                       // exponent of X, in place
    xUInt32 isExpZero = _mm_cmpeq_epi32( (xUInt32) exponent, zero );                                    // -1 if exponent of X is 0, 0 otherwise
    xUInt32 isZero = _mm_cmpeq_epi32( (xUInt32) absX, zero );                                           // -1 if X is 0, 0 otherwise
    xUInt32 isDenormal = _mm_andnot_si128( isZero, isExpZero );                                         // -1 if X is a (non-zero) denormal, 0 otherwise 
    xUInt32 isInf = _mm_cmpeq_epi32( (xUInt32) absX, (xUInt32) inf );                              // -1 if X is +- Inf, 0 otherwise
    xFloat isNaN = _mm_cmpunord_ss( X, X );                                                  // -1 if X is a NaN, 0 otherwise
	xUInt32 isSpecial = _mm_or_si128( isZero, isInf );
	isSpecial = _mm_or_si128( isSpecial, (xSInt32) isNaN );

    //Add in the bias so denormals come out right, shift, convert to float and then remove the bias if any.
    //Solves for normals and denormals
    xFloat  bias = _mm_and_ps( (xFloat) isDenormal, _mm_load_ss( &denormBias) );                                   // exponent 23 (-127) if denormal, 0 otherwise
    xFloat  biasedExponent = _mm_or_ps( bias, absX );
	biasedExponent = _mm_sub_ps( biasedExponent, bias );
    xUInt32 shiftedBiasedExponent = _mm_srli_epi32( (xUInt32) biasedExponent, 23 );
    xUInt32  reverseBias = _mm_add_epi32( _mm_and_si128( isDenormal, oneTwentySeven ), oneTwentySeven );
	shiftedBiasedExponent = _mm_sub_epi32( shiftedBiasedExponent, reverseBias );
    
    //At this point we have normals and denormals taken care of. We need to fix zero, Inf and NaN
    //This is done by adding by a correction value, as appropriate:
    // normals:     0.0f
    // denormals:   0.0f
    // +-zero:      -INF
    // +-inf:       INF
    // NaN:         X

    //Set the Zero case first
	isZero = (xUInt32) _mm_or_ps( (xFloat) isZero, ( xFloat) _mm_cvtps_epi32( (xFloat) isZero ) );				//set invalid flag if zero
    correction = _mm_and_si128( isZero, _mm_cvtsi32_si128( FP_ILOGB0 ) );                                      // -INF if X is zero, 0 otherwise
    
    //Set the +- inf case 
    correction = _mm_or_si128( correction, _mm_srli_epi32( isInf, 1 ));							// OR in INT_MAX if X is infinite
    
    //Do NaNs 
    correction = _mm_or_si128( correction, _mm_and_si128( (xUInt32) isNaN, _mm_cvtsi32_si128( FP_ILOGBNAN ) ) );                 // OR in X if X is NaN
    
    //deal with zero, inf and NaN
    shiftedBiasedExponent = _mm_andnot_si128( isSpecial, shiftedBiasedExponent ); 
	shiftedBiasedExponent = _mm_or_si128( shiftedBiasedExponent, correction );

    return _mm_cvtsi128_si32( shiftedBiasedExponent );
}

double logb( double x )
{
    static const double plusinf = __builtin_inf();
    static const double two = 2.0;
    static const double EminD = 0x1.0p-1022;
    static const xSInt64 dbias = { 1023, 0 };

    //load data into the xmm register file
    xDouble xx = _mm_load_sd( &x );
    xDouble xIsNaN = _mm_cmpunord_sd( xx, xx );

	//do zero, separated out because it has to set div/0 flag
	if( EXPECT_FALSE( x == 0.0 ) )
	{
		static const xFloat num = { 1.0f, 0.0f, 0.0f, 0.0f };

		xFloat r = _mm_div_ss( num, minusZeroF );	//generate -Inf, div/0 flag. RCPSS doesn't set the flag.
		xx = _mm_cvtss_sd( xx, r );	
		return XDOUBLE_2_DOUBLE( xx );
	}

    //figure out what kind of number x is
	xDouble fabsMask = _mm_andnot_pd( xIsNaN, minusZeroD );
    xDouble fabsx = _mm_andnot_pd( fabsMask, xx );
	xDouble safeX = _mm_andnot_pd( xIsNaN, fabsx );
    xDouble xIsInf = _mm_cmpeq_sdm( fabsx, &plusinf );
    xDouble xIsDenorm = _mm_cmplt_sdm( safeX, &EminD );
    xDouble isSpecial = _mm_or_pd( xIsNaN, xIsInf );

    //take care of denormals and set up the bias
    xDouble denormBias = _mm_and_pd( _mm_load_sd( &two ), xIsDenorm );
    xSInt64 bias = _mm_and_si128( dbias, (xSInt64) xIsDenorm );
    safeX = _mm_or_pd( safeX, denormBias );
    bias = _mm_add_epi64( bias, dbias );
    safeX = _mm_sub_pd( safeX, denormBias );

    //extract out the exponent and correct for the bias
    xSInt64 iresult = _mm_srli_epi64( (xSInt64) safeX, 52 );
    iresult = _mm_sub_epi32( iresult, bias );
	xDouble result = _mm_cvtepi32_pd( iresult );
	
	result = _mm_sel_pd( result, fabsx, isSpecial );
	
	return XDOUBLE_2_DOUBLE( result );
}

float logbf( float x )
{
    static const float plusinf = __builtin_inff();
    static const float two = 2.0f;
    static const float EminF = 0x1.0p-126f;
    static const xSInt32 dbias = { 127, 0, 0, 0 };

    //load data into the xmm register file
    xFloat xx = _mm_load_ss( &x );
    xFloat xIsNaN = _mm_cmpunord_ss( xx, xx );

	//do zero, separated out because it has to set div/0 flag
	if( EXPECT_FALSE( x == 0.0f ) )
	{
		static const xFloat num = { 1.0f, 0.0f, 0.0f, 0.0f };

		xx = _mm_div_ss( num, minusZeroF );	//generate -Inf, div/0 flag. RCPSS doesn't set the flag.
		return XFLOAT_2_FLOAT( xx );
	}

    //figure out what kind of number x is
	xFloat fabsMask = _mm_andnot_ps( xIsNaN, minusZeroF );
    xFloat fabsx = _mm_andnot_ps( fabsMask, xx );
	xFloat safeX = _mm_andnot_ps( xIsNaN, fabsx );
    xFloat xIsInf = _mm_cmpeq_ssm( fabsx, &plusinf );
    xFloat xIsDenorm = _mm_cmplt_ssm( safeX, &EminF );
    xFloat isSpecial = _mm_or_ps( xIsNaN, xIsInf );

    //take care of denormals and set up the bias
    xFloat denormBias = _mm_and_ps( _mm_load_ss( &two ), xIsDenorm );
    xSInt32 bias = _mm_and_si128( dbias, (xSInt32) xIsDenorm );
    safeX = _mm_or_ps( safeX, denormBias );
    bias = _mm_add_epi32( bias, dbias );
    safeX = _mm_sub_ps( safeX, denormBias );

    //extract out the exponent and correct for the bias
    xSInt32 iresult = _mm_srli_epi32( (xSInt32) safeX, 23 );
    iresult = _mm_sub_epi32( iresult, bias );
	xFloat result = _mm_cvtepi32_ps( iresult );
	
	result = _mm_sel_ps( result, fabsx, isSpecial );
	
	return XFLOAT_2_FLOAT( result );
}

double ldexp( double x, int exp )
{
    return scalbn( x, exp );
}

float ldexpf( float x, int exp )
{
    return scalbnf( x, exp );
}

double frexp( double value, int *exp )
{
    static const xDouble plusinf = { 1e500, 0 };
    static const xDouble one = { 1.0, 0 };
    static const xDouble half = { 0.5, 0 };
    static const double smallestNormal = 0x1.0p-1022;
    static const xSInt64 bias = {1022, 0};

    xDouble x = DOUBLE_2_XDOUBLE( value );
    xDouble fabsx = _mm_andnot_pd( minusZeroD, x );
    xDouble sign = _mm_and_pd( minusZeroD, x );
    xDouble isZero = _mm_cmpeq_sd( x, minusZeroD );

    //normalize denormals (we change the exponent bias later to compensate)
    xDouble mantissa = _mm_andnot_pd( plusinf, fabsx );
    xDouble isFinite = _mm_cmplt_sd( fabsx, plusinf );
    xDouble isDenormal = _mm_cmplt_sdm( fabsx, &smallestNormal );
    xDouble denormExponent = _mm_and_pd( one, isDenormal );
    fabsx = _mm_or_pd( fabsx, denormExponent );
    fabsx = _mm_sub_sd( fabsx, denormExponent );

    //deal with the mantissa
    mantissa = _mm_andnot_pd( plusinf, fabsx );
    mantissa = _mm_or_pd( mantissa, half );

    //figure out the exponent
    xUInt64 iExp = (__m128i) _mm_and_pd( fabsx, plusinf );
    xUInt32 unbias = _mm_add_epi64( _mm_and_si128( (__m128i) isDenormal, bias ), bias );
    iExp = _mm_srli_epi64( iExp, 52 );
    iExp = _mm_sub_epi64( iExp, unbias );
    iExp = _mm_andnot_si128( (__m128i) isZero, iExp );  //the exponent of zero is zero
    *exp = _mm_cvtsi128_si32( iExp );

    //special case handling, return x if zero or not finite
    xDouble isNotSpecial = _mm_andnot_pd( isZero, isFinite );
    x = _mm_andnot_pd( isNotSpecial, x );
    x = _mm_add_sd( x, x ); //silence NaNs
    mantissa = _mm_and_pd( isNotSpecial, mantissa );
    mantissa = _mm_or_pd( mantissa, x );
    mantissa = _mm_or_pd( mantissa, sign );

    return XDOUBLE_2_DOUBLE( mantissa );
}

float frexpf( float value, int *exp )
{
    static const xFloat plusinf = { 1e50f, 0.0f, 0.0f, 0.0f };
    static const xFloat one = { 1.0f, 0.0f, 0.0f, 0.0f };
    static const xFloat half = { 0.5f, 0.0f, 0.0f, 0.0f };
    static const float smallestNormal = 0x1.0p-126f;
    static const xSInt32 bias = {126, 0, 0, 0};

    xFloat x = FLOAT_2_XFLOAT( value );
    xFloat fabsx = _mm_andnot_ps( minusZeroF, x );
    xFloat sign = _mm_and_ps( minusZeroF, x );
    xFloat isZero = _mm_cmpeq_ss( x, minusZeroF );
    
    //normalize denormals (we change the exponent bias later to compensate)
    xFloat mantissa = _mm_andnot_ps( plusinf, fabsx );
    xFloat isFinite = _mm_cmplt_ss( fabsx, plusinf );
    xFloat isDenormal = _mm_cmplt_ssm( fabsx, &smallestNormal );
    xFloat denormExponent = _mm_and_ps( one, isDenormal );
    fabsx = _mm_or_ps( fabsx, denormExponent );
    fabsx = _mm_sub_ss( fabsx, denormExponent );
    
    //deal with the mantissa
    mantissa = _mm_andnot_ps( plusinf, fabsx );
    mantissa = _mm_or_ps( mantissa, half );
    
    //figure out the exponent
    xUInt32 iExp = (__m128i) _mm_and_ps( fabsx, plusinf );
    xUInt32 unbias = _mm_add_epi32( _mm_and_si128( (__m128i) isDenormal, bias ), bias );
    iExp = _mm_srli_epi32( iExp, 23 );
    iExp = _mm_sub_epi32( iExp, unbias );
    iExp = _mm_andnot_si128( (__m128i) isZero, iExp );  //the exponent of zero is zero
    *exp = _mm_cvtsi128_si32( iExp );
    
    //special case handling, return x if zero or not finite
    xFloat isNotSpecial = _mm_andnot_ps( isZero, isFinite );
    x = _mm_andnot_ps( isNotSpecial, x );
    x = _mm_add_ss( x, x ); //silence NaNs
    mantissa = _mm_and_ps( isNotSpecial, mantissa );
    mantissa = _mm_or_ps( mantissa, x );
    mantissa = _mm_or_ps( mantissa, sign );

    return XFLOAT_2_FLOAT( mantissa );
}

long double frexpl( long double value, int *exp )
{
    union
    {
        long double     ld;
        struct
        {
            uint64_t    mantissa;
            int16_t     sexp __attribute__ ((packed));
        }parts;
    }u = {value};

    int exponent = u.parts.sexp & 0x7fff;
    if( exponent == 0x7fff || 0.0L == value )   //inf, NaN and 0
    {
        *exp = 0;
        return value + value;
    }

    int sign = u.parts.sexp & 0x8000;
    if( exponent == 0 )
    { //denormals
        u.ld = u.parts.mantissa;
        exponent = u.parts.sexp & 0x7fff;
        u.parts.sexp = sign | 16382;
        *exp = exponent - 32827;
        return u.ld;
    }

    *exp = exponent - 16382;
    u.parts.sexp = sign | 16382;

    return  u.ld;
}

#endif /* CARBONCORE */

#endif /* defined( __i386__ ) */