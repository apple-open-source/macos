/*
 *  xmm_scalb_logb.c
 *  xmmLibm
 *
 *  Created by Ian Ollmann on 8/5/05.
 *  Copyright 2005 Apple Computer Inc. All rights reserved.
 *
 */


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

/*
int ilogb( double x )
{
    static const double infinity = __builtin_inf();                  //0x7F800000
    static const double denormBias = 2.0;    
    static const xSInt64 doublebias = {1023, 0}; 

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

    //Determine result for the Zero, inf and NaN cases first
	int specialResult = _mm_cvtsd_si32( isSpecial ) ^ _mm_cvtsi128_si32( (xUInt64) isInf );     //set invalid flag if inf, zero or NaN
            
    //merge with normal / denorm result
    return _mm_cvtsi128_si32( _mm_andnot_si128( (xUInt64) isSpecial, shiftedBiasedExponent ) ) | specialResult;
}
*/

long double scalblnl( long double x, long int n )
{
	if( n > 300000 )	n = 300000;
	if( n < -300000 ) n = -300000;
	
	return scalbnl( x, (int) n );
}

double scalbln( double x, long int n )
{
    if( n > 3000 ) n = 3000;
    if( n < -3000 ) n = -3000;

    return scalbn( x, (int) n );
}

float scalblnf( float x, long int n )
{
    if( n > 300 ) n = 300;
    if( n < -300 ) n = -300;

    return scalbnf( x, (int) n );
}

double scalb( double x, double n )
{
    if( n > 3000 ) n = 3000;
    if( n < -3000 ) n = -3000;

    return scalbn( x, (int) n );
}


//
//  We return INT_MIN for NaNs and zero
//

/*
int ilogbf( float x )
{
    static const float infinity = __builtin_inff();                  //0x7F800000
    static const float denormBias = 2.0f;    
    static const xSInt32 oneTwentySeven = {127, 0, 0, 0}; 
    
    xUInt32 zero = (xUInt32) _mm_setzero_ps();                                                          // 0

    //Do some introspection about X
	xFloat X = FLOAT_2_XFLOAT( x );
    xFloat  absX = _mm_andnot_ps( minusZeroF, X );                                                // fabs(X)
	xFloat inf = _mm_load_ss( &infinity);
    xFloat  exponent = _mm_and_ps( X, inf );                                                       // exponent of X, in place
    xUInt32 isExpZero = _mm_cmpeq_epi32( (xUInt32) exponent, zero );                                    // -1 if exponent of X is 0, 0 otherwise
    xUInt32 isZero = (xUInt32) _mm_cmpeq_ss( X, (xFloat) zero );                                           // -1 if X is 0, 0 otherwise
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

    //Find the result if the input was zero, inf or NaN. Also set invalid flag.
    int specialResult = _mm_cvtss_si32( (xFloat) isSpecial ) ^ _mm_cvtsi128_si32( isInf );
    
    //merge with normal / denorm result
    return _mm_cvtsi128_si32( _mm_andnot_si128( isSpecial, shiftedBiasedExponent ) ) | specialResult ;
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
*/

#endif /* CARBONCORE */

