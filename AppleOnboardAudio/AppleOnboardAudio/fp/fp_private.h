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
/*******************************************************************************
*                                                                              *
*     File fp_private.h                                                        *
*     Masks used for single and double floating point representations          *
*     on PowerPC.                                                              *
*                                                                              *
*******************************************************************************/
#ifndef __FP_PRIVATE__
#define __FP_PRIVATE__

/******************************************************************************
*       Functions used internally                                             *
******************************************************************************/
double   copysign ( double arg2, double arg1 );
double	 fabs ( double x );
double   nan   ( const char *string );

#include <ppc_intrinsics.h>

#define __FMADD __fmadd
#define __FMADDS __fmadds
#define __FMSUB __fmsub
#define __FNMSUB __fnmsub
#define __FMUL __fmul

/* N.B. gcc 2.95 inlines fabs() and fabsf() of its own accord. */ 
#define      __FABS(x)	fabs(x)
#define      __FABSF(x)	fabsf(x)

#define __ORI_NOOP \
({ \
    asm volatile ( "ori r0, r0, 0" ); /* NOOP */ \
})  

#define __ENSURE(x, y, z) \
({ \
    double __value, __argx = (x), __argy = (y), __argz = (z); \
    asm volatile ("fmadd %0,%1,%2,%3" : "=f" (__value): "f" (__argx), "f" (__argy), "f" (__argz)); \
    __value; \
}) 
 
#define __PROD(x, y) \
({ \
    double __value, __argx = (x), __argy = (y); \
    asm volatile ("fmul %0,%1,%2" : "=f" (__value): "f" (__argx), "f" (__argy)); \
    __value; \
})  

#define __PROG_INEXACT( x ) (void)__PROD( x, x ) /* Raises INEXACT for suitable choice of x */

#define __PROG_UF_INEXACT( x ) (void)__PROD( x, x ) /* Raises UNDERFLOW and INEXACT for suitable choice of x e.g. MIN_NORMAL */

#define __PROG_OF_INEXACT( x ) (void)__PROD( x, x ) /* Raises OVERFLOW and INEXACT for suitable choice of x e.g. MAX_NORMAL */

/******************************************************************************
*       Single precision                                                      *
******************************************************************************/

#define       fQuietNan           0x00400000

typedef union {
       long int       lval;
       float          fval;
} hexsingle;

/******************************************************************************
*       Double precision                                                      *
******************************************************************************/

#define       dQuietNan           0x00080000

#if defined(__BIG_ENDIAN__)

typedef union {
       struct {
		unsigned long hi;
		unsigned long lo;
	} i;
       double            d;
} hexdouble;

#define HEXDOUBLE(hi, lo) { { hi, lo } }

#elif defined(__LITTLE_ENDIAN__)

typedef union {
       struct {
		unsigned long lo;
		unsigned long hi;
	} i;
       double            d;
} hexdouble;

#define HEXDOUBLE(hi, lo) { { lo, hi } }

#else
#error Unknown endianness
#endif

#endif      /* __FP_PRIVATE__ */
