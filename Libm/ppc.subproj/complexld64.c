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

#define __WANT_LONG_DOUBLE_FORMAT__ 0 /* Defeat long double prototypes in math.h and complex.h */
#include "math.h"
#include "complex.h" /* for its *double* prototypes */

#undef __LIBMLDBL_COMPAT
#if 0 /* Temporarily disable the plan-in-blue. */
#define __LIBMLDBL_64(sym) __asm("_" __STRING(sym) "$LDBL64")
#define __LIBMLDBL_GLOB(sym) __asm(".text"); __asm(".align 4"); __asm(".globl " "_" __STRING(sym))
#define __LIBMLDBL_NO_DECOR(sym) __asm("_" __STRING(sym) ": b " "_" __STRING(sym) "$LDBL64")
#define __LIBMLDBL_COMPAT(sym) __LIBMLDBL_64(sym) ; __LIBMLDBL_GLOB(sym) ; __LIBMLDBL_NO_DECOR(sym)
#else
#define __LIBMLDBL_64(sym) /* NOTHING */
#define __LIBMLDBL_GLOB(sym) __asm(".text"); __asm(".align 4"); __asm(".globl " "_" __STRING(sym) "$LDBL64")
#define __LIBMLDBL_NO_DECOR(sym) __asm("_" __STRING(sym) "$LDBL64" ": b " "_" __STRING(sym))
#define __LIBMLDBL_COMPAT(sym) __LIBMLDBL_64(sym) ; __LIBMLDBL_GLOB(sym) ; __LIBMLDBL_NO_DECOR(sym)
#endif

//
// Introduce prototypes and (trivial) implementations for long double == double scheme.
// Programs compiled with -mlong-double-64 see these.
//
extern double complex cacosl( double complex ) __LIBMLDBL_COMPAT(cacosl);
extern double complex casinl( double complex ) __LIBMLDBL_COMPAT(casinl);
extern double complex catanl( double complex ) __LIBMLDBL_COMPAT(catanl);

extern double complex ccosl( double complex ) __LIBMLDBL_COMPAT(ccosl);
extern double complex csinl( double complex ) __LIBMLDBL_COMPAT(csinl);
extern double complex ctanl( double complex ) __LIBMLDBL_COMPAT(ctanl);

extern double complex cacoshl( double complex ) __LIBMLDBL_COMPAT(cacoshl);
extern double complex casinhl( double complex ) __LIBMLDBL_COMPAT(casinhl);
extern double complex catanhl( double complex ) __LIBMLDBL_COMPAT(catanhl);

extern double complex ccoshl( double complex ) __LIBMLDBL_COMPAT(ccoshl);
extern double complex csinhl( double complex ) __LIBMLDBL_COMPAT(csinhl);
extern double complex ctanhl( double complex ) __LIBMLDBL_COMPAT(ctanhl);

extern double complex cexpl( double complex ) __LIBMLDBL_COMPAT(cexpl);
extern double complex clogl( double complex ) __LIBMLDBL_COMPAT(clogl);

extern double cabsl( double complex ) __LIBMLDBL_COMPAT(cabsl);
extern double complex cpowl( double complex, double complex ) __LIBMLDBL_COMPAT(cpowl);
extern double complex csqrtl( double complex ) __LIBMLDBL_COMPAT(csqrtl);

extern double cargl( double complex ) __LIBMLDBL_COMPAT(cargl);
extern double cimagl( double complex ) __LIBMLDBL_COMPAT(cimagl);
extern double complex conjl( double complex ) __LIBMLDBL_COMPAT(conjl);
extern double complex cprojl( double complex ) __LIBMLDBL_COMPAT(cprojl);
extern double creall( double complex ) __LIBMLDBL_COMPAT(creall);

double complex cacosl( double complex z ) { return cacos( z); }
double complex casinl( double complex z ) { return casin( z); }
double complex catanl( double complex z ) { return catan( z); }

double complex ccosl( double complex z ) { return ccos( z); }
double complex csinl( double complex z ) { return csin( z); }
double complex ctanl( double complex z ) { return ctan( z); }

double complex cacoshl( double complex z ) { return cacosh( z); }
double complex casinhl( double complex z ) { return casinh( z); }
double complex catanhl( double complex z ) { return catanh( z); }

double complex ccoshl( double complex z ) { return ccosh( z); }
double complex csinhl( double complex z ) { return csinh( z); }
double complex ctanhl( double complex z ) { return ctanh( z); }

double complex cexpl( double complex z ) { return cexp( z); }
double complex clogl( double complex z ) { return clog( z); }

double cabsl( double complex z ) { return cabs( z); }
double complex cpowl( double complex x, double complex y ) { return cpow( x,  y); }
double complex csqrtl( double complex z ) { return csqrt( z); }

double cargl( double complex z ) { return carg( z); }
double cimagl( double complex z ) { return cimag( z); }
double complex conjl( double complex z ) { return conj( z); }
double complex cprojl( double complex z ) { return cproj( z); }
double creall( double complex z ) { return creal( z); }
