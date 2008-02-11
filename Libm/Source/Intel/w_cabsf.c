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

/* Modified by Stephen Canon on Sept 18, 2006 to do computation in double
 * precision, without calling cabs(double complex). This works because we
 * can't get undue underflow or overflow, but we do need to check for ±°.
 *
 * Also tried out the following SSE3 code:
 *
 *     xDouble xy = _mm_cvtps_pd(_mm_load_ps(&z));
 *     xy = _mm_andnot_pd(minusZeroD,xy);
 *     if ( EXPECT_FALSE( _mm_movemask_pd ( _mm_cmpeq_pd( xy, infinityD ) ) ) )
 *       return __builtin_inf();
 *	   xy = _mm_mul_pd(xy,xy);
 *     xy = _mm_hadd_pd(xy,xy);
 *     xy = __builtin_ia32_sqrtsd(xy);
 *     return XDOUBLE_2_FLOAT(xy);
 *
 * but the x87 code is about twice as fast, and doesn't need SSE3.
 * (and doesn't depend on stack alignment magic)
 ************************************************************************
 *
 * the old code for this function:
 * float cabsf( float complex z ) { return (float)cabs((double complex)z); }
 *
 ************************************************************************/

float cabsf( float _Complex z ) {
	static const double inf = __builtin_inf();
	double x = __builtin_fabs(__real__ z);
	double y = __builtin_fabs(__imag__ z);
	if ( x == inf || y == inf )
		return (float)inf;
	return (float)__builtin_sqrt(x*x + y*y);
}
