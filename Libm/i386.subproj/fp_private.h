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
*     File:  fp_private.h                                                      *
*                                                                              *
*     Contains: Prototypes and typedefs for internal consumption. 	       *
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
#define      __FABS(x)	fabs(x)
#define      __FABSF(x)	fabsf(x)

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

/******************************************************************************
*       Long Double precision                                                 *
******************************************************************************/

#define       lQuietNan           0x00008000

typedef union {
        struct {
        unsigned long    least_mantissa;
        unsigned long    most_mantissa;
        unsigned short	 head;
        } u;
        long double      e80;
} hexlongdouble;

#endif      /* __FP_PRIVATE__ */
