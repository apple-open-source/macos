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
*     08 Dec 01   scp   First created.                                         *
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
double fmax ( double x, double y )
{
    if (x != x)
        return y;
    else if (y != y)
        return x;
    else if (x < y)
        return y;
    else
        return x;
}

double fmin ( double x, double y )
{
    if (x != x)
        return y;
    else if (y != y)
        return x;
    else if (x > y)
        return y;
    else
        return x;
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

float fmaxf ( float x, float y )
{
    if (x != x)
        return y;
    else if (y != y)
        return x;
    else if (x < y)
        return y;
    else
        return x;
}

float fminf ( float x, float y )
{
    if (x != x)
        return y;
    else if (y != y)
        return x;
    else if (x > y)
        return y;
    else
        return x;
}

double fma ( double x, double y, double z )
{
    return (x * y) + z;
}

float fmaf ( float x, float y, float z )
{
    return (x * y) + z;
}
#endif /* !BUILDING_FOR_CARBONCORE_LEGACY */

#else       /* __APPLE_CC__ version */
#error Version gcc-932 or higher required.  Compilation terminated.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */

