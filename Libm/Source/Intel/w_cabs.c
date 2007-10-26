/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/****************************************************************************
 double cabs(double complex z) returns the absolute value (magnitude) of its
 complex argument z, avoiding spurious overflow, underflow, and invalid
 exceptions.  The algorithm is from Kahan's paper.
 
 CONSTANTS:  FPKSQT2 = sqrt(2.0) to double precision
 FPKR2P1 = sqrt(2.0) + 1.0 to double precision
 FPKT2P1 = sqrt(2.0) + 1.0 - FPKR2P1 to double precision, so
 that FPKR2P1 + FPKT2P1 = sqrt(2.0) + 1.0 to double
 double precision.
 
 Calls:  fpclassify, fabs, sqrt, feholdexcept, fesetround, feclearexcept,
 and feupdateenv.
 ****************************************************************************/

#include "math.h"
//#include "fenv.h"
//#include "fp_private.h"
#include "xmmLibm_prefix.h"
#include "complex.h"

#define complex _Complex

#define Real(z) (__real__ z)
#define Imag(z) (__imag__ z)

/****************************************************************************
 double cabs(double complex z) returns the absolute value (magnitude) of its
 complex argument z, avoiding spurious overflow, underflow, and invalid
 exceptions.  The algorithm is from Kahan's paper.
 
 CONSTANTS:  FPKSQT2 = sqrt(2.0) to double precision
 FPKR2P1 = sqrt(2.0) + 1.0 to double precision
 FPKT2P1 = sqrt(2.0) + 1.0 - FPKR2P1 to double precision, so
 that FPKR2P1 + FPKT2P1 = sqrt(2.0) + 1.0 to double
 double precision.
 
 Calls:  fpclassify, fabs, sqrt, feholdexcept, fesetround, feclearexcept,
 and feupdateenv.
 **************************************************************************** 
 Heavily modified Sept, 2006 by scanon to fix terrible intel performance.
 code is borrowed wholesale from iano's hypot() implementation.
 ****************************************************************************
 scanon:
 modified Sept 19, 2006 to use x87 instead of xmm registers.
 this gives speedup from ~100 cycles to ~70 cycles on merom, and ~150 to ~70 on yonah.
 the old xmm code is commented out below.
 
 these changes will be reflected in hypot() as well, and the two should be kept
 in sync in the future.
 ****************************************************************************/

double cabs ( double complex z )
{
	static const long double inf = __builtin_inf();
	long double x = __builtin_fabsl(__real__ z);
	long double y = __builtin_fabsl(__imag__ z);
	if ( x == inf || y == inf )
		return (double)inf;
	return (double)__builtin_sqrtl(x*x + y*y);
	
/*
	static const double oneD = 1.0;
	static const double infinity = __builtin_inf();
	static const double minNormalD = 0x1.0p-1022;
	static const xSInt64 smallF = { 0x3630000080000000LL, 0 };	//0x1.0p-60, with an extra 8 in there to make the 32-bit test work
	static const xSInt64 denormBias = { 1022LL << 52, 0 };
	static const xSInt64 bias = { 1023LL << 52, 0 };
	xDouble xx = DOUBLE_2_XDOUBLE( (Real(z)) );	//		xx =	[		Re(z)			0.0		]
	xDouble yy = DOUBLE_2_XDOUBLE( (Imag(z)) ); //		yy =	[		Im(z)			0.0		]
	xDouble isNaN = _mm_cmpunord_sd( xx, yy );  // isNaN =	[		  0x0			0.0		] if x <=> y			[ 0xffffff			0.0		] if x ? y
	xx = _mm_andnot_pd( minusZeroD, xx );				//    xx =	[ |Re(z)|			0.0		]
	yy = _mm_andnot_pd( minusZeroD, yy );				//    yy =	[ |Im(z)|			0.0		]
	xDouble safeX = _mm_andnot_pd( isNaN, xx ); // safeX =  [ |Re(z)|			0.0		] if x <=> y			[ 0x0						0.0		] if x ? y
	xDouble safeY = _mm_andnot_pd( isNaN, yy ); // safeY =  [ |Im(z)|			0.0		] if x <=> y			[ 0x0						0.0		] if x ? y
		
	//Handle Infinities
	if( EXPECT_FALSE( _mm_istrue_sd( _mm_or_pd( _mm_cmpeq_sdm( xx, &infinity), _mm_cmpeq_sdm( yy, &infinity) )) ))
		return infinity;
	
	//Handle NaN's and zeros
	if( EXPECT_FALSE( _mm_isfalse_sd( _mm_and_pd( _mm_cmpgt_sdm( safeX, (float*) &minusZeroD ), _mm_cmpgt_sdm( safeY, (float*) &minusZeroD ) )) ) )
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
	
	return XDOUBLE_2_DOUBLE( scaledL ); */
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

long double cabsl( long double complex z ) {
	long double x = Real(z);
	long double y = Imag(z);
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
