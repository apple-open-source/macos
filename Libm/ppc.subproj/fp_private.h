/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
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

/* gcc 2.95 inlines fabs() and fabsf() of its own accord (as single instructions!) */ 
#define      __fabs(x)	fabs(x)
#define      __fabsf(x)	fabsf(x)

#define __fmadd(x, y, z) \
({ \
    double __value, __argx = (x), __argy = (y), __argz = (z); \
    asm volatile ("fmadd %0,%1,%2,%3" : "=f" (__value): "f" (__argx), "f" (__argy), "f" (__argz)); \
    __value; \
})  

#define __fmaddf(x, y, z) \
({ \
    float __value, __argx = (x), __argy = (y), __argz = (z); \
    asm volatile ("fmadds %0,%1,%2,%3" : "=f" (__value): "f" (__argx), "f" (__argy), "f" (__argz)); \
    __value; \
})  

#define __fmsub(x, y, z) \
({ \
    double __value, __argx = (x), __argy = (y), __argz = (z); \
    asm volatile ("fmsub %0,%1,%2,%3" : "=f" (__value): "f" (__argx), "f" (__argy), "f" (__argz)); \
    __value; \
})  

#define __prod(x, y) \
({ \
    double __value, __argx = (x), __argy = (y); \
    asm volatile ("fmul %0,%1,%2" : "=f" (__value): "f" (__argx), "f" (__argy)); \
    __value; \
})  

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
