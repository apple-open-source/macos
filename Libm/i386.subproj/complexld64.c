/*
 * Copyright (c) 2002, 2005 Apple Computer, Inc. All rights reserved.
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


#include "math.h"
#include "complex.h"


/*	The following implementations of long double functions use double
	functions.

	THESE ARE NOT GOOD IMPLEMENTATIONS.

	These implementations are placedholders to allow programs to build and get
	crude results (results with the accuracies of the double-precision
	functions).  This is only for a proof-of-concept demonstration of Mac OS X
	for WWDC and should not become part of a product.

	-- Eric Postpischil, April 14, 2005.
*/


long double complex cacosl(long double complex z)	{ return cacos(z); }
long double complex casinl(long double complex z)	{ return casin(z); }
long double complex catanl(long double complex z)	{ return catan(z); }

long double complex ccosl(long double complex z)	{ return ccos(z); }
long double complex csinl(long double complex z)	{ return csin(z); }
long double complex ctanl(long double complex z)	{ return ctan(z); }

long double complex cacoshl(long double complex z)	{ return cacosh(z); }
long double complex casinhl(long double complex z)	{ return casinh(z); }
long double complex catanhl(long double complex z)	{ return catanh(z); }

long double complex ccoshl(long double complex z)	{ return ccosh(z); }
long double complex csinhl(long double complex z)	{ return csinh(z); }
long double complex ctanhl(long double complex z)	{ return ctanh(z); }

long double complex cexpl(long double complex z)	{ return cexp(z); }
long double complex clogl(long double complex z)	{ return clog(z); }

long double cabsl(long double complex z)			{ return cabs(z); }
long double complex cpowl(long double complex x, long double complex y)
													{ return cpow(x, y); }
long double complex csqrtl(long double complex z)	{ return csqrt(z); }

long double cargl(long double complex z)			{ return carg(z); }
long double cimagl(long double complex z)			{ return cimag(z); }
long double complex conjl(long double complex z)	{ return conj(z); }
long double complex cprojl(long double complex z)	{ return cproj(z); }
long double creall(long double complex z)			{ return creal(z); }
