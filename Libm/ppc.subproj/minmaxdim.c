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
*     File:  minmaxdim.c                                                       *
*                                                                              *
*     Contains: C99 fmin, fmax, fdim, and fma				       *
*                                                                              *
*     Copyright © 2001 Apple Computer, Inc.  All rights reserved.              *
*                                                                              *
*     Written by Stephen C. Peters, started in November 2001.                  *
*                                                                              *
*     A MathLib v5 file.                                                       *
*                                                                              *
*     Change History (most recent first):                                      *
*                                                                              *
*     21 Nov 01   scp   First created.                                         *
*                                                                              *
*     W A R N I N G:                                                           *
*     These routines require a 64-bit double precision IEEE-754 model.         *
*     They are written for PowerPC only and are expecting the compiler         *
*     to generate the correct sequence of multiply-add fused instructions.     *
*                                                                              *
*     These routines are not intended for 32-bit Intel architectures.          *
*                                                                              *
*     A version of gcc higher than 932 is required.                            *
*                                                                              *
*     GCC compiler options:                                                    *
*           optimization level 3 (-O3)                                         *
*           -fschedule-insns -finline-functions -funroll-all-loops             *
*                                                                              *
*******************************************************************************/

#ifdef      __APPLE_CC__
#if         __APPLE_CC__ > 930

#include "fp_private.h"

#if defined(BUILDING_FOR_CARBONCORE_LEGACY)

double fdim ( double x, double y )
{
    if ((x != x) || (y != y))
        return ( x + y );
    else if (x > y)
        return ( x - y );
    else
        return 0.0;
}

//
// N.B. max/min (-0, 0) allows implementation dependent result
//
#define __fmax(x, y) \
({ \
    double __value, __argx = (x), __argy = (y); \
    asm volatile ( \
        "fcmpu 		cr0,%1,%2 ; 	/* Compare unordered */ 				\n \
         blt		cr0, 0f ; 	/* Order discerned? Then we have our answer */ 		\n \
         bnu+		cr0, 1f ; 	/* Opposite order discerned? Then we have our answer */ \n \
         fcmpu 		cr1,%2,%2 ; 	/* x, y or both are NAN. Is y NAN? */			\n \
         bun-		cr1, 1f ; 	/* If so, x is our answer */ 				\n \
    0:	 fmr		%0, %2; 	/* Else y is our answer */ 				\n \
         b		2f									\n \
    1:	 fmr		%0,%1;									\n \
    2:												\n \
        ": "=f"(__value) : "f" (__argx), "f" (__argy)); \
    __value; \
})  

double fmax ( double x, double y )
{
    return __fmax( x, y );
}

#define __fmin(x, y) \
({ \
    double __value, __argx = (x), __argy = (y); \
    asm volatile ( \
        "fcmpu 		cr0,%1,%2 ; 	/* Compare unordered */ 				\n \
         bgt		cr0, 0f ; 	/* Order discerned? Then we have our answer */ 		\n \
         bnu+		cr0, 1f ; 	/* Opposite order discerned? Then we have our answer */ \n \
         fcmpu 		cr1,%2,%2 ; 	/* x, y or both are NAN. Is y NAN? */			\n \
         bun-		cr1, 1f ; 	/* If so, x is our answer */ 				\n \
    0:	 fmr		%0, %2; 	/* Else y is our answer */ 				\n \
         b		2f									\n \
    1:	 fmr		%0,%1;									\n \
    2:												\n \
        ": "=f"(__value) : "f" (__argx), "f" (__argy)); \
    __value; \
})  

double fmin ( double x, double y )
{
    return __fmin( x, y );
}

#else /* !BUILDING_FOR_CARBONCORE_LEGACY */

float fdimf ( float x, float y )
{
    if ((x != x) || (y != y))
        return ( x + y );
    else if (x > y)
        return ( x - y );
    else
        return 0.0;
}


#define __fmaxf(x, y) \
({ \
    float __value, __argx = (x), __argy = (y); \
    asm volatile ( \
        "fcmpu 		cr0,%1,%2 ; 	/* Compare unordered */ 				\n \
         blt		cr0, 0f ; 	/* Order discerned? Then we have our answer */ 		\n \
         bnu+		cr0, 1f ; 	/* Opposite order discerned? Then we have our answer */ \n \
         fcmpu 		cr1,%2,%2 ; 	/* x, y or both are NAN. Is y NAN? */			\n \
         bun-		cr1, 1f ; 	/* If so, x is our answer */ 				\n \
    0:	 fmr		%0, %2; 	/* Else y is our answer */ 				\n \
         b		2f									\n \
    1:	 fmr		%0,%1;									\n \
    2:												\n \
        ": "=f"(__value) : "f" (__argx), "f" (__argy)); \
    __value; \
})  

float fmaxf ( float x, float y )
{
    return __fmaxf( x, y );
}

#define __fminf(x, y) \
({ \
    float __value, __argx = (x), __argy = (y); \
    asm volatile ( \
        "fcmpu 		cr0,%1,%2 ; 	/* Compare unordered */ 				\n \
         bgt		cr0, 0f ; 	/* Order discerned? Then we have our answer */ 		\n \
         bnu+		cr0, 1f ; 	/* Opposite order discerned? Then we have our answer */ \n \
         fcmpu 		cr1,%2,%2 ; 	/* x, y or both are NAN. Is y NAN? */			\n \
         bun-		cr1, 1f ; 	/* If so, x is our answer */ 				\n \
    0:	 fmr		%0, %2; 	/* Else y is our answer */ 				\n \
         b		2f									\n \
    1:	 fmr		%0,%1;									\n \
    2:												\n \
        ": "=f"(__value) : "f" (__argx), "f" (__argy)); \
    __value; \
})  

float fminf ( float x, float y )
{
    return __fminf( x, y );
}

double fma ( double x, double y, double z )
{
    return __FMADD(x, y, z);
}

float fmaf ( float x, float y, float z )
{
    return __FMADDS(x, y, z);
}
#endif /* !BUILDING_FOR_CARBONCORE_LEGACY */

#else       /* __APPLE_CC__ version */
#error Version gcc-932 or higher required.  Compilation terminated.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */

