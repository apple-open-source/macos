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
*     File ceilfloor.c,                                                        *
*     Function ceil(x) and floor(x),                                           *
*     Implementation of ceil and floor for the PowerPC.                        *
*                                                                              *
*     Copyright © 1991-2001 Apple Computer, Inc.  All rights reserved.         *
*                                                                              *
*     Written by Ali Sazegari, started on November 1991,                       *
*     Modified and ported by Robert A. Murley (ram) for Mac OS X.              *
*                                                                              *
*     A MathLib v4 file.                                                       *
*                                                                              *
*     December 03 1992: first rs6000 port.                                     *
*     July     14 1993: comment changes and addition of #pragma fenv_access.   *
*     May      06 1997: port of the ibm/taligent ceil and floor routines.      *
*     April    11 2001: first port to os x using gcc.                          *
*     June     13 2001: replaced __setflm with FEGETENVD/FESETENVD;            *
*                       replaced DblInHex typedef with hexdouble.              *
*                       used standard exception symbols from fenv.h.           *
*     Sept     06 2001: added #ifdef __ppc__.                                  *
*     Sept     09 2001: added more comments.                                   *
*     Sept     10 2001: added macros to detect PowerPC and correct compiler.   *
*     Sept     17 2001: deleted "fp.h" and changed "fenv.h" to <fenv.h>.       *
*     Sept     18 2001: added <CoreServices/CoreServices.h> to get <fenv.h>.   *
*     October  08 2001: removed <CoreServices/CoreServices.h>.                 *
*                       changed compiler errors to warnings.                   *
*     November 06 2001: commented out warning about Intel architectures.       *
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

#include      "fp_private.h"
#include      "fenv_private.h"

static const double        twoTo52  = 4503599627370496.0;
static const float         twoTo23  = 8388608.0;

/*******************************************************************************
*      Ceil(x) returns the smallest integer not less than x.                   *
*******************************************************************************/

#ifdef notdef
double ceil ( double x )
      {
      hexdouble xInHex, OldEnvironment;
      register double y;
      register unsigned long int xhi;
      register int target;
      
      xInHex.d = x;
      xhi = xInHex.i.hi & 0x7fffffff;        // xhi is the high half of |x|
      target = ( xInHex.i.hi < 0x80000000 );
      
      if ( xhi < 0x43300000ul ) 
/*******************************************************************************
*      Is |x| < 2.0^52?                                                        *
*******************************************************************************/
            {
            if ( xhi < 0x3ff00000 ) 
/*******************************************************************************
*      Is |x| < 1.0?                                                           *
*******************************************************************************/
                  {
                  if ( ( xhi | xInHex.i.lo ) == 0ul )  // zero x is exact case
                        return ( x );
                  else 
                        {                                  // inexact case
                        FEGETENVD( OldEnvironment.d );
                        OldEnvironment.i.lo |= FE_INEXACT;
                        FESETENVD( OldEnvironment.d );
                        if ( target )
                              return ( 1.0 );
                        else
                              return ( -0.0 );
                        }
                  }
/*******************************************************************************
*      Is 1.0 < |x| < 2.0^52?                                                  *
*******************************************************************************/
            if ( target ) 
                  {
                  y = ( x + twoTo52 ) - twoTo52;          // round at binary pt.
                  if ( y < x )
                        return ( y + 1.0 );
                  else
                        return ( y );
                  }
            
            else 
                  {
                  y = ( x - twoTo52 ) + twoTo52;          // round at binary pt.
                  if ( y < x )
                        return ( y + 1.0 );
                  else
                        return ( y );
                  }
            }
/*******************************************************************************
*      |x| >= 2.0^52 or x is a NaN.                                            *
*******************************************************************************/
      return ( x );
      }

float ceilf ( float x )
      {
      hexdouble OldEnvironment;
      hexsingle xInHex;
      register float y;
      register unsigned long int xhi;
      register int target;
      
      xInHex.fval = x;
      xhi = xInHex.lval & 0x7fffffff;        // xhi is |x|
      target = ( xInHex.lval < 0x80000000 );

      if ( xhi < 0x4b000000ul ) 
/*******************************************************************************
*      Is |x| < 2.0^23?                                                        *
*******************************************************************************/
            {
            if ( xhi < 0x3f800000 ) 
/*******************************************************************************
*      Is |x| < 1.0?                                                           *
*******************************************************************************/
                  {
                  if ( xhi == 0ul )  // zero x is exact case
                        return ( x );
                  else 
                        {                                  // inexact case
                        FEGETENVD( OldEnvironment.d );
                        OldEnvironment.i.lo |= FE_INEXACT;
                        FESETENVD( OldEnvironment.d );
                        if ( target )
                              return ( 1.0 );
                        else
#if (__GNUC__>=3)
                              return ( -0.0 );
#else /* workaround gcc 2.x botch of -0 return. */
                              {
                              volatile hexsingle zInHex;
                              zInHex.lval = 0x80000000;
                              return zInHex.fval;
                              }
#endif
                        }
                  }
/*******************************************************************************
*      Is 1.0 < |x| < 2.0^23?                                                  *
*******************************************************************************/
            if ( target ) 
                  {
                  y = ( x + twoTo23 ) - twoTo23;          // round at binary pt.
                  if ( y < x )
                        return ( y + 1.0 );
                  else
                        return ( y );
                  }
            
            else 
                  {
                  y = ( x - twoTo23 ) + twoTo23;          // round at binary pt.
                  if ( y < x )
                        return ( y + 1.0 );
                  else
                        return ( y );
                  }
            }
/*******************************************************************************
*      |x| >= 2.0^23 or x is a NaN.                                            *
*******************************************************************************/
      return ( x );
      }
      



/*******************************************************************************
*      Floor(x) returns the largest integer not greater than x.                *
*******************************************************************************/

double floor ( double x )
      {
      hexdouble xInHex, OldEnvironment;
      register double y;
      register unsigned long int xhi;
      register long int target;
      
      xInHex.d = x;
      xhi = xInHex.i.hi & 0x7fffffff;        // xhi is the high half of |x|
      target = ( xInHex.i.hi < 0x80000000 );
      
      if ( xhi < 0x43300000ul ) 
/*******************************************************************************
*      Is |x| < 2.0^52?                                                        *
*******************************************************************************/
            {
            if ( xhi < 0x3ff00000 ) 
/*******************************************************************************
*      Is |x| < 1.0?                                                           *
*******************************************************************************/
                  {
                  if ( ( xhi | xInHex.i.lo ) == 0ul )  // zero x is exact case
                        return ( x );
                  else 
                        {                                  // inexact case
                        FEGETENVD( OldEnvironment.d );
                        OldEnvironment.i.lo |= FE_INEXACT;
                        FESETENVD( OldEnvironment.d );
                        if ( target )
                              return ( 0.0 );
                        else
                              return ( -1.0 );
                        }
                  }
/*******************************************************************************
*      Is 1.0 < |x| < 2.0^52?                                                  *
*******************************************************************************/
            if ( target ) 
                  {
                  y = ( x + twoTo52 ) - twoTo52;          // round at binary pt.
                  if ( y > x )
                        return ( y - 1.0 );
                  else
                        return ( y );
                  }
            
            else 
                  {
                  y = ( x - twoTo52 ) + twoTo52;          // round at binary pt.
                  if ( y > x )
                        return ( y - 1.0 );
                  else
                        return ( y );
                  }
            }
/*******************************************************************************
*      |x| >= 2.0^52 or x is a NaN.                                            *
*******************************************************************************/
      return ( x );
      }

float floorf ( float x )
      {
      hexdouble OldEnvironment;
      hexsingle xInHex;
      register float y;
      register unsigned long int xhi;
      register long int target;
      
      xInHex.fval = x;
      xhi = xInHex.lval & 0x7fffffff;        // xhi is |x|
      target = ( xInHex.lval < 0x80000000 );
      
      if ( xhi < 0x4b000000ul ) 
/*******************************************************************************
*      Is |x| < 2.0^23?                                                        *
*******************************************************************************/
            {
            if ( xhi < 0x3f800000 ) 
/*******************************************************************************
*      Is |x| < 1.0?                                                           *
*******************************************************************************/
                  {
                  if ( xhi == 0ul )  // zero x is exact case
                        return ( x );
                  else 
                        {                                  // inexact case
                        FEGETENVD( OldEnvironment.d );
                        OldEnvironment.i.lo |= FE_INEXACT;
                        FESETENVD( OldEnvironment.d );
                        if ( target )
                              return ( 0.0 );
                        else
                              return ( -1.0 );
                        }
                  }
/*******************************************************************************
*      Is 1.0 < |x| < 2.0^23?                                                  *
*******************************************************************************/
            if ( target ) 
                  {
                  y = ( x + twoTo23 ) - twoTo23;          // round at binary pt.
                  if ( y > x )
                        return ( y - 1.0 );
                  else
                        return ( y );
                  }
            
            else 
                  {
                  y = ( x - twoTo23 ) + twoTo23;          // round at binary pt.
                  if ( y > x )
                        return ( y - 1.0 );
                  else
                        return ( y );
                  }
            }
/*******************************************************************************
*      |x| >= 2.0^23 or x is a NaN.                                            *
*******************************************************************************/
      return ( x );
      }
#else
static const double piOver4 = 0.785398163397448390;        // 0x1.921fb54442d19p-1

double ceil ( double x )
{
      register double y;
      register int target;
      
      register double FPR_absx, FPR_Two52, FPR_one, FPR_zero, FPR_Mzero, FPR_pi4;
      
      FPR_absx = __FABS( x );				FPR_zero = 0.0;
      FPR_Two52 = twoTo52;				FPR_one = 1.0;
      __ENSURE( FPR_zero, FPR_Two52, FPR_one );
      FPR_pi4 = piOver4;				FPR_Mzero = -0.0;
      target = ( x > FPR_zero ); 			__ENSURE( FPR_Mzero, FPR_Two52, FPR_pi4 );
           
      if ( FPR_absx < FPR_Two52 ) 
/*******************************************************************************
*      Is |x| < 2.0^52?                                                        *
*******************************************************************************/
      {
            if ( FPR_absx < FPR_one ) 
/*******************************************************************************
*      Is |x| < 1.0?                                                           *
*******************************************************************************/
            {
                  if ( x == FPR_zero )  // zero x is exact case
                        return ( x );
                  else 
                  {                                  // inexact case
                        __PROG_INEXACT( FPR_pi4 );
                        if ( target )
                              return ( FPR_one );
                        else
                              return ( FPR_Mzero );
                  }
            }
/*******************************************************************************
*      Is 1.0 < |x| < 2.0^52?                                                  *
*******************************************************************************/
            if ( target ) 
            {
                  y = ( x + FPR_Two52 ) - FPR_Two52;          // round at binary pt.
                  if ( y < x )
                        return ( y + FPR_one );
                  else
                        return ( y );
            }
            
            else 
            {
                  y = ( x - FPR_Two52 ) + FPR_Two52;          // round at binary pt.
                  if ( y < x )
                        return ( y + FPR_one );
                  else
                        return ( y );
            }
      }
/*******************************************************************************
*      |x| >= 2.0^52 or x is a NaN.                                            *
*******************************************************************************/
      return ( x );
}

float ceilf ( float x )
{
      register float y;
      register int target;
      
      register float FPR_absx, FPR_Two23, FPR_one, FPR_zero, FPR_Mzero, FPR_pi4;
      
      FPR_absx = __FABS( x );				FPR_zero = 0.0f;
      FPR_Two23 = twoTo23;				FPR_one = 1.0;
      __ENSURE( FPR_zero, FPR_Two23, FPR_one );
      FPR_pi4 = piOver4;				FPR_Mzero = -0.0f;
      target = ( x > FPR_zero ); 			__ENSURE( FPR_Mzero, FPR_Two23, FPR_pi4 );
           
      if ( FPR_absx < FPR_Two23 ) 
/*******************************************************************************
*      Is |x| < 2.0^23?                                                        *
*******************************************************************************/
      {
            if ( FPR_absx < FPR_one ) 
/*******************************************************************************
*      Is |x| < 1.0?                                                           *
*******************************************************************************/
            {
                  if ( x == FPR_zero )  // zero x is exact case
                        return ( x );
                  else 
                  {                                  // inexact case
                        __PROG_INEXACT( FPR_pi4 );
                        if ( target )
                              return ( FPR_one );
                        else
                              return ( FPR_Mzero );
                  }
            }
/*******************************************************************************
*      Is 1.0 < |x| < 2.0^23?                                                  *
*******************************************************************************/
            if ( target ) 
            {
                  y = ( x + FPR_Two23 ) - FPR_Two23;          // round at binary pt.
                  if ( y < x )
                        return ( y + FPR_one );
                  else
                        return ( y );
            }
            
            else 
            {
                  y = ( x - FPR_Two23 ) + FPR_Two23;          // round at binary pt.
                  if ( y < x )
                        return ( y + FPR_one );
                  else
                        return ( y );
            }
      }
/*******************************************************************************
*      |x| >= 2.0^23 or x is a NaN.                                            *
*******************************************************************************/
      return ( x );
}

/*******************************************************************************
*      Floor(x) returns the largest integer not greater than x.                *
*******************************************************************************/

double floor ( double x )
{
      register double y;
      register int target;
      
      register double FPR_absx, FPR_Two52, FPR_one, FPR_zero, FPR_Mone, FPR_pi4;
      
      FPR_absx = __FABS( x );				FPR_zero = 0.0;
      FPR_Two52 = twoTo52;				FPR_one = 1.0;
      __ENSURE( FPR_zero, FPR_Two52, FPR_one );
      FPR_pi4 = piOver4;				FPR_Mone = -1.0;
      target = ( x > FPR_zero ); 			__ENSURE( FPR_Mone, FPR_zero, FPR_pi4 );
           
      if ( FPR_absx < FPR_Two52 ) 
/*******************************************************************************
*      Is |x| < 2.0^52?                                                        *
*******************************************************************************/
      {
            if ( FPR_absx < FPR_one ) 
/*******************************************************************************
*      Is |x| < 1.0?                                                           *
*******************************************************************************/
            {
                  if ( x == FPR_zero )  // zero x is exact case
                        return ( x );
                  else 
                  {                                  // inexact case
                        __PROG_INEXACT( FPR_pi4 );
                        if ( target )
                              return ( FPR_zero );
                        else
                              return ( FPR_Mone );
                  }
            }
/*******************************************************************************
*      Is 1.0 < |x| < 2.0^52?                                                  *
*******************************************************************************/
            if ( target ) 
            {
                  y = ( x + FPR_Two52 ) - FPR_Two52;          // round at binary pt.
                  if ( y > x )
                        return ( y - FPR_one );
                  else
                        return ( y );
            }
            
            else 
            {
                  y = ( x - FPR_Two52 ) + FPR_Two52;          // round at binary pt.
                  if ( y > x )
                        return ( y - FPR_one );
                  else
                        return ( y );
            }
      }
/*******************************************************************************
*      |x| >= 2.0^52 or x is a NaN.                                            *
*******************************************************************************/
      return ( x );
}

float floorf ( float x )
{
      register float y;
      register int target;
      
      register float FPR_absx, FPR_Two23, FPR_one, FPR_zero, FPR_Mone, FPR_pi4;
      
      FPR_absx = __FABS( x );				FPR_zero = 0.0f;
      FPR_Two23 = twoTo23;				FPR_one = 1.0f;
      __ENSURE( FPR_zero, FPR_Two23, FPR_one );
      FPR_pi4 = piOver4;				FPR_Mone = -1.0f;
      target = ( x > FPR_zero ); 			__ENSURE( FPR_Mone, FPR_zero, FPR_pi4 );
           
      if ( FPR_absx < FPR_Two23 ) 
/*******************************************************************************
*      Is |x| < 2.0^23?                                                        *
*******************************************************************************/
      {
            if ( FPR_absx < FPR_one ) 
/*******************************************************************************
*      Is |x| < 1.0?                                                           *
*******************************************************************************/
            {
                  if ( x == FPR_zero )  // zero x is exact case
                        return ( x );
                  else 
                  {                                  // inexact case
                        __PROG_INEXACT( FPR_pi4 );
                        if ( target )
                              return ( FPR_zero );
                        else
                              return ( FPR_Mone );
                  }
            }
/*******************************************************************************
*      Is 1.0 < |x| < 2.0^23?                                                  *
*******************************************************************************/
            if ( target ) 
            {
                  y = ( x + FPR_Two23 ) - FPR_Two23;          // round at binary pt.
                  if ( y > x )
                        return ( y - FPR_one );
                  else
                        return ( y );
            }
            
            else 
            {
                  y = ( x - FPR_Two23 ) + FPR_Two23;          // round at binary pt.
                  if ( y > x )
                        return ( y - FPR_one );
                  else
                        return ( y );
            }
      }
/*******************************************************************************
*      |x| >= 2.0^23 or x is a NaN.                                            *
*******************************************************************************/
      return ( x );
}
#endif

#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
