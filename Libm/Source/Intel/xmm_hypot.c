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
	/* New code, uses the x87 to compute
	 *
	 */
	static const long double inf = __builtin_inf();
	long double X = __builtin_fabsl(x);
	long double Y = __builtin_fabsl(y);
	if ( X == inf || Y == inf )
		return (double)inf;
	return (double)__builtin_sqrtl(X*X + Y*Y);
	
	/* old xmm code; this has been replaced with the faster x87 code.
	 *
	 *
    static const double oneD = 1.0;
    static const double infinity = __builtin_inf();
	static const double minNormalD = 0x1.0p-1022;
	static const xSInt64 smallF = { 0x3630000080000000LL, 0 };	//0x1.0p-60, with an extra 8 in there to make the 32-bit test work
	static const xSInt64 denormBias = { 1022LL << 52, 0 };
	static const xSInt64 bias = { 1023LL << 52, 0 };
	xDouble xx = DOUBLE_2_XDOUBLE( x );
	xDouble yy = DOUBLE_2_XDOUBLE( y );
	xDouble isNaN = _mm_cmpunord_sd( xx, yy );
	xx = _mm_andnot_pd( minusZeroD, xx );
	yy = _mm_andnot_pd( minusZeroD, yy );
	xDouble safeX = _mm_andnot_pd( isNaN, xx );
	xDouble safeY = _mm_andnot_pd( isNaN, yy );

	//Handle Inf's and zeros
    if( EXPECT_FALSE( _mm_istrue_sd( _mm_or_pd( _mm_cmpeq_sdm( xx, &infinity), _mm_cmpeq_sdm( yy, &infinity) )) ))
		return infinity;

	//Handle NaN's and zeros
    if( EXPECT_FALSE( _mm_isfalse_sd( _mm_and_pd( _mm_cmpgt_sdm( safeX, (double*) &minusZeroD ), 
												  _mm_cmpgt_sdm( safeY, (double*) &minusZeroD ) )) ) )
        return XDOUBLE_2_DOUBLE( _mm_add_sd( xx, yy ) );

	xDouble expMask = _mm_load_sd( &infinity );
	xDouble one = _mm_load_sd( &oneD );
	xDouble tiny = _mm_load_sd( &minNormalD );
	expMask = _mm_or_pd( expMask, minusZeroD );
	//We calculate this as follows:
	//
	//		sqrt( (x*2^N)^2 + (y*2^M)^2 )	=	2^N * sqrt( x^2 + (y*2^(M-N))^2 )
	//
	//		for |x| > |y|
	//
	
	//order into small and large arguments
	xDouble small = _mm_min_sd( xx, yy );
	xDouble large = _mm_max_sd( xx, yy );

	//set the large argument exponent aside and set it to have an exponent of 1.0
	//(First deal with denormals)
	xDouble largeIsDenormal = _mm_cmplt_sd( large, tiny );
	xDouble smallIsDenormal = _mm_cmplt_sd( small, tiny );
	xDouble denormExpL = _mm_and_pd( one, largeIsDenormal );
	xDouble denormExpS = _mm_and_pd( one, smallIsDenormal );
	large = _mm_or_pd( large, denormExpL );
	small = _mm_or_pd( small, denormExpS );
	large = _mm_sub_pd( large, denormExpL );
	small = _mm_sub_pd( small, denormExpS );

	large = (xDouble) _mm_sub_epi64( (xSInt64) large, _mm_and_si128( (xSInt64) largeIsDenormal, denormBias ) );
	small = (xDouble) _mm_sub_epi64( (xSInt64) small, _mm_and_si128( (xSInt64) smallIsDenormal, denormBias ) );

	//set exponent to 1.0 for large
	xDouble scaledL = _mm_andnot_pd( expMask, large );
	scaledL = _mm_or_pd( scaledL, one );
	
	//Now calculate 2**-N
	xSInt64 scale = _mm_sub_epi64( (xSInt64) scaledL, (xSInt64) large );

	//Scale the small one by 2**-N, so it is now y*2^(M-N).  This might underflow
	xDouble scaledS = (xDouble) _mm_add_epi64( (xSInt64) small, scale );

	//If scaledS underflowed, clip to smallF, so we get inexact flag
	scaledS = _mm_sel_pd( scaledS, (xDouble) smallF, (xDouble) _mm_cmplt_epi32( (xSInt64) scaledS, smallF )  );
	
	//Do sqrt( scaledL**2 + scaledS**2 )
	scaledL = _mm_mul_sd( scaledL, scaledL );
	scaledS = _mm_mul_sd( scaledS, scaledS );
	scaledL = _mm_add_sd( scaledL, scaledS );
	scaledL = _MM_SQRT_SD( scaledL );
	
//Unscale the result
	//Since the scaling is possibly larger than the value representable by a float, we split into two scale operations
	xSInt64 halfScale = _mm_srai_epi32( scale, 1 );							//divide the scale by two
	xSInt64 correction1 = _mm_and_si128( halfScale, (xSInt64) expMask );	//trim off any bits that spill into manitssa
	scale = _mm_sub_epi64( scale, correction1 );							//find the remaining scaling to be done
	correction1 = _mm_sub_epi64( bias, correction1 );						//add the bias and reverse the sign of the correction (it had the opposite sign to begin with because we used it to scale the small argument) 
	scale = _mm_sub_epi64( bias, scale );									//add the bias and reverse the sign of the correction (it had the opposite sign to begin with because we used it to scale the small argument)
	scaledL = _mm_mul_sd( scaledL, (xDouble) correction1 );					//scale part way, this is exact
	scaledL = _mm_mul_sd( scaledL, (xDouble) scale );						//scale the rest of the way. This may trigger overflow, underflow, inexact
			
	return XDOUBLE_2_DOUBLE( scaledL );		*/
}

float hypotf( float x, float y )
{
  static const double inf = __builtin_inf();
	double X = __builtin_fabs(x);
	double Y = __builtin_fabs(y);
	if ( X == inf || Y == inf )
		return (float)inf;
	return (float)__builtin_sqrt(X*X + Y*Y);
/* old code; this has been replaced with the above code which promotes to double.
  static const float oneF = 1.0f;
    static const float infinity = __builtin_inff();
	static const float minNormalF = 0x1.0p-126f;
	static const xSInt32 smallF = { 0x21800000, 0, 0, 0};	//0x1.0p-60
	static const xSInt32 denormBias = { 126 << 23, 0, 0, 0 };
	static const xSInt32 bias = { 127 << 23, 0, 0, 0 };
	xFloat xx = FLOAT_2_XFLOAT( x );
	xFloat yy = FLOAT_2_XFLOAT( y );
	xFloat isNaN = _mm_cmpunord_ss( xx, yy );
	xx = _mm_andnot_ps( minusZeroF, xx );
	yy = _mm_andnot_ps( minusZeroF, yy );
	xFloat safeX = _mm_andnot_ps( isNaN, xx );
	xFloat safeY = _mm_andnot_ps( isNaN, yy );

	//Handle Inf's and zeros
    if( EXPECT_FALSE( _mm_istrue_ss( _mm_or_ps( _mm_cmpeq_ssm( xx, &infinity), _mm_cmpeq_ssm( yy, &infinity) )) ))
		return infinity;

	//Handle NaN's and zeros
    if( EXPECT_FALSE( _mm_isfalse_ss( _mm_and_ps( _mm_cmpgt_ssm( safeX, (float*) &minusZeroF ), 
												  _mm_cmpgt_ssm( safeY, (float*) &minusZeroF ) )) ) )
        return XFLOAT_2_FLOAT( _mm_add_ss( xx, yy ) );

	xFloat expMask = _mm_load_ss( &infinity );
	xFloat one = _mm_load_ss( &oneF );
	xFloat tiny = _mm_load_ss( &minNormalF );
	expMask = _mm_or_ps( expMask, minusZeroF );
	//We calculate this as follows:
	//
	//		sqrt( (x*2^N)^2 + (y*2^M)^2 )	=	2^N * sqrt( x^2 + (y*2^(M-N))^2 )
	//
	//		for |x| > |y|
	//
	
	//order into small and large arguments
	xFloat small = _mm_min_ss( xx, yy );
	xFloat large = _mm_max_ss( xx, yy );

	//set the large argument exponent aside and set it to have an exponent of 1.0
	//(First deal with denormals)
	xFloat largeIsDenormal = _mm_cmplt_ss( large, tiny );
	xFloat smallIsDenormal = _mm_cmplt_ss( small, tiny );
	xFloat denormExpL = _mm_and_ps( one, largeIsDenormal );
	xFloat denormExpS = _mm_and_ps( one, smallIsDenormal );
	large = _mm_or_ps( large, denormExpL );
	small = _mm_or_ps( small, denormExpS );
	large = _mm_sub_ps( large, denormExpL );
	small = _mm_sub_ps( small, denormExpS );

	large = (xFloat) _mm_sub_epi32( (xSInt32) large, _mm_and_si128( (xSInt32) largeIsDenormal, denormBias ) );
	small = (xFloat) _mm_sub_epi32( (xSInt32) small, _mm_and_si128( (xSInt32) smallIsDenormal, denormBias ) );

	//set exponent to 1.0 for large
	xFloat scaledL = _mm_andnot_ps( expMask, large );
	scaledL = _mm_or_ps( scaledL, one );
	
	//Now calculate 2**-N
	xSInt32 scale = _mm_sub_epi32( (xSInt32) scaledL, (xSInt32) large );

	//Scale the small one by 2**-N, so it is now y*2^(M-N).  This might underflow
	xFloat scaledS = (xFloat) _mm_add_epi32( (xSInt32) small, scale );

	//If scaledS underflowed, clip to smallF, so we get inexact flag
	scaledS = _mm_sel_ps( scaledS, (xFloat) smallF, (xFloat) _mm_cmplt_epi32( (xSInt32) scaledS, smallF )  );
	
	//Do sqrt( scaledL**2 + scaledS**2 )
	scaledL = _mm_mul_ss( scaledL, scaledL );
	scaledS = _mm_mul_ss( scaledS, scaledS );
	scaledL = _mm_add_ss( scaledL, scaledS );
	scaledL = _mm_sqrt_ss( scaledL );
	
//Unscale the result
	//Since the scaling is possibly larger than the value representable by a float, we split into two scale operations
	xSInt32 halfScale = _mm_srai_epi32( scale, 1 );							//divide the scale by two
	xSInt32 correction1 = _mm_and_si128( halfScale, (xSInt32) expMask );	//trim off any bits that spill into manitssa
	scale = _mm_sub_epi32( scale, correction1 );							//find the remaining scaling to be done
	correction1 = _mm_sub_epi32( bias, correction1 );						//add the bias and reverse the sign of the correction (it had the opposite sign to begin with because we used it to scale the small argument) 
	scale = _mm_sub_epi32( bias, scale );									//add the bias and reverse the sign of the correction (it had the opposite sign to begin with because we used it to scale the small argument)
	scaledL = _mm_mul_ss( scaledL, (xFloat) correction1 );					//scale part way, this is exact
	scaledL = _mm_mul_ss( scaledL, (xFloat) scale );						//scale the rest of the way. This may trigger overflow, underflow, inexact
			
	return XFLOAT_2_FLOAT( scaledL ); */
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



