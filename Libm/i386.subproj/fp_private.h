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
*     File:  fp_private.h                                                      *
*                                                                              *
*     Contains: Prototypes and typedefs for internal consumption. 	       *
*                                                                              *
*******************************************************************************/

#ifndef __FP_PRIVATE__
#define __FP_PRIVATE__
#include "stdint.h"

/******************************************************************************
*       Functions used internally                                             *
******************************************************************************/
double   copysign ( double arg2, double arg1 );
double	 fabs ( double x );
double   nan   ( const char *string );

/* gcc inlines fabs() and fabsf()  */ 
#define      __FABS(x)	__builtin_fabs(x)
#define      __FABSF(x)	__builtin_fabsf(x)

#if defined(__APPLE_CC__)
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect((x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

/******************************************************************************
*       Single precision                                                      *
******************************************************************************/

#define       fQuietNan           0x00400000

typedef union {
       int32_t		  lval;
       float          fval;
} hexsingle;

/******************************************************************************
*       Double precision                                                      *
******************************************************************************/

#define       dQuietNan           0x00080000

#if defined(__BIG_ENDIAN__)
typedef union {
       struct {
		uint32_t hi;
		uint32_t lo;
	} i;
       double            d;
} hexdouble;

#define HEXDOUBLE(hi, lo) { { hi, lo } }

#elif defined(__LITTLE_ENDIAN__)
typedef union {
       struct {
		uint32_t lo;
		uint32_t hi;
	} i;
       double            d;
} hexdouble;

#define HEXDOUBLE(hi, lo) { { lo, hi } }

#else
#error Unknown endianness
#endif

/******************************************************************************
*       Long Double precision                                                 *
******************************************************************************/

#define       lQuietNan           0x00008000

typedef union {
        struct {
        uint32_t     least_mantissa;
        uint32_t     most_mantissa;
        uint16_t	 head;
        } u;
        long double      e80;
} hexlongdouble;

#endif      /* __FP_PRIVATE__ */
