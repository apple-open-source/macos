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
*     File:    scalb.c                                                         *
*                                                                              *
*     Contains: C source code for implementation of the IEEE-754 scalb         *
*     function for double format on PowerPC platforms.                         *
*                                                                              *
*     Copyright © 1992-2001 Apple Computer, Inc.  All rights reserved.         *
*                                                                              *
*     Written by Jon Okada, started on December 1992.                          *
*     Modified by A. Sazegari (ali) for MathLib v3.                            *
*     Modified and ported by Robert A. Murley (ram) for Mac OS X.              *
*                                                                              *
*     A MathLib v4 file.                                                       *
*                                                                              *
*     Change History ( most recent first ):                                    *
*                                                                              *
*     06 Nov 01  ram   commented out warning about Intel architectures.        *
*     10 Oct 01  ram   changed compiler errors to warnings.                    *
*     10 Sep 01  ali   added macros to detect PowerPC and correct compiler.    *
*     09 Sep 01  ali   added more comments.                                    *
*     28 Aug 01  ram   added #ifdef __ppc__.                                   *
*     16 Jul 01  ram   replaced DblInHex typedef with hexdouble.               *
*     28 May 97  ali   made a speed improvement for large n,                   *
*                      removed scalbl.                                         *
*     12 Dec 92  JPO   First created.                                          *
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
*            optimization level 3 (-O3)                                        *
*            -fschedule-insns -finline-functions -funroll-all-loops            *
*                                                                              *
*******************************************************************************/

#ifdef      __APPLE_CC__
#if         __APPLE_CC__ > 930

#include       "fp_private.h"

static const double twoTo1023  = 8.988465674311579539e307;   // 0x1p1023
static const double twoToM1022 = 2.225073858507201383e-308;  // 0x1p-1022
static const double twoTo127  = 0.1701411834604692e39;   // 0x1p127
static const double twoToM126 = 0.1175494350822288e-37;  // 0x1p-126


/***********************************************************************
       Function scalbn
      Returns its argument x scaled by the factor 2^m.  NaNs, signed 
       zeros, and infinities are propagated by this function regardless 
       of the value of n.
      
      Exceptions:  OVERFLOW/INEXACT or UNDERFLOW/INEXACT may occur;
                      INVALID for signaling NaN inputs (quiet NaN returned).
***********************************************************************/

double scalbn ( double x, int n  )
{
      hexdouble xInHex;
      
      xInHex.i.lo = 0UL;                        // init. low half of xInHex
      
      if ( n > 1023 ) 
       {                                        // large positive scaling
            if ( n > 2097 )                     // huge scaling
                   return ( ( x * twoTo1023 ) * twoTo1023 ) * twoTo1023;
            while ( n > 1023 ) 
              {                                 // scale reduction loop
                  x *= twoTo1023;               // scale x by 2^1023
                  n -= 1023;                    // reduce n by 1023
              }
       }
      
      else if ( n < -1022 ) 
       {                                        // large negative scaling
            if ( n < -2098 )                    // huge negative scaling
                   return ( ( x * twoToM1022 ) * twoToM1022 ) * twoToM1022;
            while ( n < -1022 ) 
              {                                 // scale reduction loop
                  x *= twoToM1022;              // scale x by 2^( -1022 )
                  n += 1022;                    // incr n by 1022
              }
       }

#ifndef notdef
       if ( -127 < n && n < 128 )
       {
/*******************************************************************************
*      -126 <= n <= -127; convert n to single scale factor.                    *
*	Allows a store-forward to execute successfully 			       *
*******************************************************************************/
            hexsingle XInHex;
            
            XInHex.lval = ( ( unsigned long ) ( n + 127 ) ) << 23;
            
            __ORI_NOOP;
            __ORI_NOOP;
            __ORI_NOOP;
            return ( x * XInHex.fval );
       }
#endif
       
/*******************************************************************************
*      -1022 <= n <= 1023; convert n to double scale factor.                   *
*******************************************************************************/

      xInHex.i.hi = ( ( unsigned long ) ( n + 1023 ) ) << 20;
      return ( x * xInHex.d );
}

double scalbln ( double x, long int n  )
{
    int m;
    
    // Clip n
    if (n > 2097)
        m = 2098;
    else if (n < -2098)
        m = -2099;
    else
        m = n;
    
    return scalbn(x, m);
}

float scalbnf ( float x, int n  )
{
      hexsingle xInHex;
            
      if ( n > 127 ) 
       {                                        // large positive scaling
            if ( n > 276 )                      // huge scaling
                   return ( ( x * twoTo127 ) * twoTo127 ) * twoTo127;
            while ( n > 127 ) 
              {                                 // scale reduction loop
                  x *= twoTo127;                // scale x by 2^127
                  n -= 127;                     // reduce n by 127
              }
       }
      
      else if ( n < -126 ) 
       {                                        // large negative scaling
            if ( n < -277 )                     // huge negative scaling
                   return ( ( x * twoToM126 ) * twoToM126 ) * twoToM126;
            while ( n < -126 ) 
              {                                 // scale reduction loop
                  x *= twoToM126;               // scale x by 2^( -126 )
                  n += 126;                     // incr n by 126
              }
       }

/*******************************************************************************
*      -126 <= n <= 127; convert n to float scale factor.                      *
*******************************************************************************/

      xInHex.lval = ( ( unsigned long ) ( n + 127 ) ) << 23;
      
#ifndef notdef
      // Force the fetch for xInHex.fval to the next cycle to avoid Store/Load hazard.
      __ORI_NOOP;
      __ORI_NOOP;
      __ORI_NOOP;
#endif
      return ( x * xInHex.fval );
}

float scalblnf ( float x, long int n  )
{
    int m;
    
    // Clip n
    if (n > 276)
        m = 277;
    else if (n < -277)
        m = -278;
    else
        m = n;
    
    return scalbnf(x, m);
}

#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
