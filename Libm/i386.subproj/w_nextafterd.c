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
*     File:  w_nextafterd.c                                                    *
*                                                                              *
*     Contains: Legacy nextafterd(). 		       			       *
*                                                                              *
*     Copyright © 2001 Apple Computer, Inc.  All rights reserved.              *
*                                                                              *
*     Written by Stephen C. Peters, started in February 2021.                  *
*                                                                              *
*     A MathLib v5 file.                                                       *
*                                                                              *
*     Change History (most recent first):                                      *
*                                                                              *
*     06 Feb 02   scp   First created.                                         *
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
#include "math.h"
#include "xmmLibm_prefix.h"

/* Legacy nextafterd() API */

double nextafterd(double x, double y)
{
    static const double smallest = 0x0.0000000000001p-1022;
    static const double tiny = 0x1.0000000000000p-1022;

    //must be a x or y is NaN
    if( EXPECT_FALSE( x != x ) )
        return x + x;
    
    if( EXPECT_TRUE( x < y ) )
    {
		if( EXPECT_FALSE( x == - __builtin_inf() ) )
			return -0x1.fffffffffffffp1023;

        int oldmxcsr = _mm_getcsr();
        int newmxcsr = (oldmxcsr & ~ROUND_MASK ) | ROUND_TO_INFINITY;
        _mm_setcsr( newmxcsr );
         
        x += smallest;
    
		int test = __builtin_fabs( x ) < tiny;
		oldmxcsr |= -test & ( UNDERFLOW_FLAG | INEXACT_FLAG );
	
        oldmxcsr |= _mm_getcsr() & ALL_FLAGS;
        _mm_setcsr( oldmxcsr );
        return x;
    }

    if( EXPECT_TRUE( x > y ) )
    {
		if( EXPECT_FALSE( x == __builtin_inf() ) )
			return 0x1.fffffffffffffp1023;

        int oldmxcsr = _mm_getcsr();
        int newmxcsr = (oldmxcsr & ~ROUND_MASK ) | ROUND_TO_NEG_INFINITY;
        _mm_setcsr( newmxcsr );
         
        x -= smallest;
    
		int test = __builtin_fabs( x ) < tiny;
		oldmxcsr |= -test & ( UNDERFLOW_FLAG | INEXACT_FLAG );

        oldmxcsr |= _mm_getcsr() & ALL_FLAGS;
        _mm_setcsr( oldmxcsr );
        return x;
    }

    if( EXPECT_TRUE( x == y ) )
        return y;
        
    return y + y;
}

