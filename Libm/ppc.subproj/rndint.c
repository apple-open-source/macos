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
*     File:   rndint.c                                                         *
*                                                                              *
*     Contains: C source code for implementations of floating-point            *
*                functions which round to integral value or format, as         *
*                defined in C99.  In particular, this file contains            *
*                implementations of the following functions:                   *
*                rint, nearbyint, rinttol, round, roundtol, trunc and modf.    *
*                                                                              *
*     Copyright © 1992-2001 by Apple Computer, Inc. All rights reserved.       *
*                                                                              *
*     Written by Jon Okada, started on December 1992.                          *
*     Modified by Paul Finlayson (PAF) for MathLib v2.                         *
*     Modified by A. Sazegari (ali) for MathLib v3.                            *
*     Modified and ported by Robert A. Murley (ram) for Mac OS X.              *
*                                                                              *
*     A MathLib v4 file.                                                       *
*                                                                              *
*     Change History (most recent first):                                      *
*                                                                              *
*     06 Nov 01  ram commented out warning about Intel architectures.          *
*                    changed i386 stubs to call abort().                       *
*     02 Nov 01  ram added stubs for i386 routines.                            *
*     08 Oct 01  ram removed <Limits.h> and <CoreServices/CoreServices.h>.     *
*                    changed compiler errors to warnings.                      *
*     05 Oct 01  ram added defines for LONG_MAX and LONG_MIN                   *
*     18 Sep 01  ali <CoreServices/CoreServices.h> replaced "fp.h" & "fenv.h". *
*     10 Sep 01  ali added more comments.                                      *
*     09 Sep 01  ali added macros to detect PowerPC and correct compiler.      *
*     28 Aug 01  ram  added #ifdef __ppc__.                                    *
*     13 Jul 01  ram  Replaced __setflm with FEGETENVD/FESETENVD.              *
*                            replaced DblInHex typedef with hexdouble.         *
*     03 Mar 01  ali  first port to os x using gcc, added the crucial          *
*                       __setflm definition.                                   *
*                            1. removed double_t, put in double for now.       *
*                            2. removed iclass from nearbyint.                 *
*                            3. removed wrong comments in trunc.               *
*     13 May 97  ali  made performance improvements in rint, rinttol,          *
*                     roundtol and trunc by folding some of the taligent       *
*                     ideas into this implementation.  nearbyint is faster     *
*                     than the one in taligent, rint is more elegant, but      *
*                     slower by %30 than the taligent one.                     *
*     09 Apr 97  ali  deleted modfl and deferred to AuxiliaryDD.c              *
*     15 Sep 94  ali  Major overhaul and performance improvements of all       *
*                     functions.                                               *
*     20 Jul 94  PAF  New faster version.                                      *
*     16 Jul 93  ali  Added the modfl function.                                *
*     18 Feb 93  ali  Changed the return value of fenv functions               *
*                     feclearexcept and feraiseexcept to their new             *
*                     NCEG X3J11.1/93-001 definitions.                         *
*     16 Dec 92  JPO  Removed __itrunc implementation to a separate file.      *
*     15 Dec 92  JPO  Added __itrunc implementation and modified rinttol       *
*                     to include conversion from double to long int format.    *       
*                     Modified roundtol to call __itrunc.                      *
*     10 Dec 92  JPO  Added modf (double) implementation.                      *
*     04 Dec 92  JPO  First created.                                           *
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
*      GCC compiler options:                                                   *
*            optimization level 3 (-O3)                                        *
*            -fschedule-insns -finline-functions -funroll-all-loops            *
*                                                                              *
*******************************************************************************/

#ifdef      __APPLE_CC__
#if         __APPLE_CC__ > 930

#include       "fp_private.h"
#include       "fenv_private.h"

#define  LONG_MAX    2147483647
#define  LONG_MIN    (-LONG_MAX - 1)

static const double twoTo52      = 4503599627370496.0;                  // 2^52
static const double doubleToLong = 4503603922337792.0; 			// 2^52 + 2^32
static const hexdouble TOWARDZERO = HEXDOUBLE(0x00000000, 0x00000001);

static const float twoTo23  = 8388608.0;

#if !defined(BUILDING_FOR_CARBONCORE_LEGACY)

/*******************************************************************************
*                                                                              *
*     The function rint rounds its double argument to integral value           *
*     according to the current rounding direction and returns the result in    *
*     double format.  This function signals inexact if an ordered return       * 
*     value is not equal to the operand.                                       *
*                                                                              *
*******************************************************************************/

/*******************************************************************************
*     First, an elegant implementation.                                        *
********************************************************************************
*
*double rint ( double x )
*      {
*      double y;
*      
*      y = twoTo52.fval;
*      
*      if ( fabs ( x ) >= y )                          // huge case is exact 
*            return x;
*      if ( x < 0 ) y = -y;                            // negative case 
*      y = ( x + y ) - y;                              // force rounding 
*      if ( y == 0.0 )                                 // zero results mirror sign of x 
*            y = copysign ( y, x );
*      return ( y );      
*      }
********************************************************************************
*     Now a bit twidling version that is about %30 faster.                     *
*******************************************************************************/

double rint ( double x )
{
      hexdouble argument;
      register double y;
      unsigned long int xHead;
      register long int target;
      
      argument.d = x;
      xHead = argument.i.hi & 0x7fffffff;              // xHead <- high half of |x|
      target = ( argument.i.hi < 0x80000000 );         // flags positive sign
      
      if ( xHead < 0x43300000ul ) 
/*******************************************************************************
*     Is |x| < 2.0^52?                                                         *
*******************************************************************************/
       {
            if ( xHead < 0x3ff00000 ) 
/*******************************************************************************
*     Is |x| < 1.0?                                                            *
*******************************************************************************/
              {
                  if ( target )
                        y = ( x + twoTo52 ) - twoTo52; // round at binary point
                  else
                        y = ( x - twoTo52 ) + twoTo52; // round at binary point
                  if ( y == 0.0 ) 
                     {                                 // fix sign of zero result
                        if ( target )
                              return ( 0.0 );
                        else
                              return ( -0.0 );
                     }
                  return y;
              }
            
/*******************************************************************************
*     Is 1.0 < |x| < 2.0^52?                                                   *
*******************************************************************************/

            if ( target )
                  return ( ( x + twoTo52 ) - twoTo52 ); // round at binary pt.
            else
                  return ( ( x - twoTo52 ) + twoTo52 );
       }
      
/*******************************************************************************
*     |x| >= 2.0^52 or x is a NaN.                                             *
*******************************************************************************/
      return ( x );
}

float rintf ( float x )
{
      hexsingle argument;
      register float y;
      unsigned long int xHead;
      register long int target;
      
      argument.fval = x;
      xHead = argument.lval & 0x7fffffff;              // xHead <- |x|
      target = ( (unsigned long)argument.lval < 0x80000000ul );         // flags positive sign
      
      if ( xHead < 0x4b000000ul ) 
/*******************************************************************************
*     Is |x| < 2.0^23?                                                         *
*******************************************************************************/
       {
            if ( xHead < 0x3f800000 ) 
/*******************************************************************************
*     Is |x| < 1.0?                                                            *
*******************************************************************************/
              {
                  if ( target )
                        y = ( x + twoTo23 ) - twoTo23; // round at binary point
                  else
                        y = ( x - twoTo23 ) + twoTo23; // round at binary point
                  if ( y == 0.0 ) 
                     {                                 // fix sign of zero result
                        if ( target )
                              return ( 0.0 );
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
                  return y;
              }
            
/*******************************************************************************
*     Is 1.0 < |x| < 2.0^23?                                                   *
*******************************************************************************/

            if ( target )
                  return ( ( x + twoTo23 ) - twoTo23 ); // round at binary pt.
            else
                  return ( ( x - twoTo23 ) + twoTo23 );
       }
      
/*******************************************************************************
*     |x| >= 2.0^23 or x is a NaN.                                             *
*******************************************************************************/
      return ( x );
}

/*******************************************************************************
*                                                                              *
*     The function nearbyint rounds its double argument to integral value      *
*     according to the current rounding direction and returns the result in    *
*     double format.  This function does not signal inexact.                   *
*                                                                              *
*     Functions used in this routine:                                          *
*     fabs and copysign.                                                       *
*                                                                              *
*******************************************************************************/
   
double nearbyint ( double x )
{
       double y, OldEnvironment;
      
        if (x != x)
            return x;
            
       y = twoTo52;
       
       FEGETENVD( OldEnvironment );              /* save the environement */

      if ( fabs ( x ) >= y )                     /* huge case is exact */
            return x;
      if ( x < 0 ) y = -y;                       /* negative case */
      y = ( x + y ) - y;                         /* force rounding */
      if ( y == 0.0 )                            /* zero results mirror sign of x */
            y = copysign ( y, x );
      FESETENVD( OldEnvironment );              /* restore old environment */
      return ( y );      
}
      
float nearbyintf ( float x )
{
    double OldEnvironment;
    float y;

    if (x != x)
        return x;
            
    y = twoTo23;
    
    FEGETENVD( OldEnvironment );              /* save the environement */
    
    if ( fabs ( x ) >= y )                     /* huge case is exact */
        return x;
    if ( x < 0 ) y = -y;                       /* negative case */
        y = ( x + y ) - y;                         /* force rounding */
    if ( y == 0.0 )                            /* zero results mirror sign of x */
        y = copysign ( y, x );
    FESETENVD( OldEnvironment );              /* restore old environment */
    return ( y );      
}
      
long int lrint ( double x )
{
    hexdouble hx;
    
    asm volatile ("fctiw %0,%1" : "=f"(hx.d) : "f"(x));
    return hx.i.lo;    
}

long int lrintf ( float x )
{
    hexdouble hx;
    
    asm volatile ("fctiw %0,%1" : "=f"(hx.d) : "f"(x));
    return hx.i.lo;    
}

long long int llrint ( double x )
{
    return (long long int)rint( x );
}

long long int llrintf (float x)
{
    return (long long int)rintf ( x );
}

/*******************************************************************************
*                                                                              *
*     The function round rounds its double argument to integral value          *
*     according to the "add half to the magnitude and truncate" rounding of    *
*     Pascal's Round function and FORTRAN's ANINT function and returns the     *
*     result in double format.  This function signals inexact if an ordered    *
*     return value is not equal to the operand.                                *
*                                                                              *
*******************************************************************************/
   
double round ( double x )
{      
      hexdouble argument, OldEnvironment;
      register double y, z;
      register unsigned long int xHead;
      register long int target;
      
      argument.d = x;
      xHead = argument.i.hi & 0x7fffffff;              // xHead <- high half of |x|
      target = ( argument.i.hi < 0x80000000 );         // flag positive sign
      
      if ( xHead < 0x43300000ul ) 
/*******************************************************************************
*     Is |x| < 2.0^52?                                                        *
*******************************************************************************/
       {
            if ( xHead < 0x3ff00000 ) 
/*******************************************************************************
*     Is |x| < 1.0?                                                           *
*******************************************************************************/
              {
                     FEGETENVD( OldEnvironment.d );     // get environment
                  if ( xHead < 0x3fe00000ul ) 
/*******************************************************************************
*     Is |x| < 0.5?                                                           *
*******************************************************************************/
                     {
                        if ( ( xHead | argument.i.lo ) != 0ul )
                              OldEnvironment.i.lo |= FE_INEXACT;
                            FESETENVD( OldEnvironment.d );
                        if ( target ) 
                              return ( 0.0 );
                        else
                              return ( -0.0 );
                     }
/*******************************************************************************
*     Is 0.5 ² |x| < 1.0?                                                      *
*******************************************************************************/
                  OldEnvironment.i.lo |= FE_INEXACT;
                     FESETENVD ( OldEnvironment.d );
                  if ( target )
                        return ( 1.0 );
                  else
                        return ( -1.0 );
              }
/*******************************************************************************
*     Is 1.0 < |x| < 2.0^52?                                                   *
*******************************************************************************/
            if ( target ) 
              {                                         // positive x
                  y = ( x + twoTo52 ) - twoTo52;        // round at binary point
                  if ( y == x )                         // exact case
                        return ( x );
                  z = x + 0.5;                          // inexact case
                  y = ( z + twoTo52 ) - twoTo52;        // round at binary point
                  if ( y > z )
                        return ( y - 1.0 );
                  else
                        return ( y );
              }
            
/*******************************************************************************
*     Is x < 0?                                                                *
*******************************************************************************/
            else 
              {
                  y = ( x - twoTo52 ) + twoTo52;        // round at binary point
                  if ( y == x )
                        return ( x );
                  z = x - 0.5;
                  y = ( z - twoTo52 ) + twoTo52;        // round at binary point
                  if ( y < z )
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

float roundf ( float x )
{      
      hexdouble OldEnvironment;
      hexsingle argument;
      register float y, z;
      register unsigned long int xHead;
      register long int target;
      
      argument.fval = x;
      xHead = argument.lval & 0x7fffffff;              // xHead <- |x|
      target = ( (unsigned long)argument.lval < 0x80000000ul );         // flags positive sign
      
      if ( xHead < 0x4b000000ul ) 
/*******************************************************************************
*     Is |x| < 2.0^52?                                                        *
*******************************************************************************/
       {
            if ( xHead < 0x3f800000ul ) 
/*******************************************************************************
*     Is |x| < 1.0?                                                           *
*******************************************************************************/
              {
                  FEGETENVD( OldEnvironment.d );     // get environment
                  if ( xHead < 0x3f000000ul ) 
/*******************************************************************************
*     Is |x| < 0.5?                                                           *
*******************************************************************************/
                     {
                        if ( xHead != 0ul )
                              OldEnvironment.i.lo |= FE_INEXACT;
                            FESETENVD( OldEnvironment.d );
                        if ( target ) 
                              return ( 0.0 );
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
/*******************************************************************************
*     Is 0.5 ² |x| < 1.0?                                                      *
*******************************************************************************/
                  OldEnvironment.i.lo |= FE_INEXACT;
                     FESETENVD ( OldEnvironment.d );
                  if ( target )
                        return ( 1.0 );
                  else
                        return ( -1.0 );
              }
/*******************************************************************************
*     Is 1.0 < |x| < 2.0^23?                                                   *
*******************************************************************************/
            if ( target ) 
              {                                         // positive x
                  y = ( x + twoTo23 ) - twoTo23;        // round at binary point
                  if ( y == x )                         // exact case
                        return ( x );
                  z = x + 0.5;                          // inexact case
                  y = ( z + twoTo23 ) - twoTo23;        // round at binary point
                  if ( y > z )
                        return ( y - 1.0 );
                  else
                        return ( y );
              }
            
/*******************************************************************************
*     Is x < 0?                                                                *
*******************************************************************************/
            else 
              {
                  y = ( x - twoTo23 ) + twoTo23;        // round at binary point
                  if ( y == x )
                        return ( x );
                  z = x - 0.5;
                  y = ( z - twoTo23 ) + twoTo23;        // round at binary point
                  if ( y < z )
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
*                                                                              *
*     The function roundtol converts its double argument to integral format    *
*     according to the "add half to the magnitude and chop" rounding mode of   *
*     Pascal's Round function and FORTRAN's NINT function.  This conversion    *
*     signals invalid if the argument is a NaN or the rounded intermediate     *
*     result is out of range of the destination long int format, and it        *
*     delivers an unspecified result in this case.  This function signals      *
*     inexact if the rounded result is within range of the long int format but *
*     unequal to the operand.                                                  *
*                                                                              *
*******************************************************************************/

long int lround ( double x )
{       
       register double y, z;
       hexdouble argument, OldEnvironment;
       register unsigned long int xhi;
       register long int target;
       const hexdouble kTZ = HEXDOUBLE(0x0, 0x1);
       const hexdouble kUP = HEXDOUBLE(0x0, 0x2);
       
       argument.d = x;
       xhi = argument.i.hi & 0x7fffffff;                    // high 32 bits of x
       target = ( argument.i.hi < 0x80000000 );             // flag positive sign
       
       if ( xhi > 0x41e00000ul ) 
/*******************************************************************************
*     Is x is out of long range or NaN?                                        *
*******************************************************************************/
       {
              FEGETENVD ( OldEnvironment.d );               // get environment
              OldEnvironment.i.lo |= SET_INVALID;
              FESETENVD ( OldEnvironment.d );               // set environment
              if ( target )                                 // pin result
                     return ( LONG_MAX );
              else
                     return ( LONG_MIN );
       }
       
       if ( target ) 
/*******************************************************************************
*     Is sign of x "+"?                                                        *
*******************************************************************************/
       {
              if ( x < 2147483647.5 ) 
/*******************************************************************************
*     x is in the range of a long.                                             *
*******************************************************************************/
              {
                     y = ( x + doubleToLong ) - doubleToLong;    // round at binary point
                     if ( y != x )       
                     {                                           // inexact case
                            FEGETENVD (OldEnvironment.d );       // save environment
                            FESETENVD ( kTZ.d );                 // truncate rounding
                            z = x + 0.5;                         // truncate x + 0.5
                            argument.d = z + doubleToLong;
                            FESETENVD( OldEnvironment.d );       // restore environment
                            return ( ( long ) argument.i.lo );
                     }
                     
                     argument.d = y + doubleToLong;              // force result into argument.i.lo
                     return ( ( long ) argument.i.lo );          // return long result
              }
/*******************************************************************************
*     Rounded positive x is out of the range of a long.                        *
*******************************************************************************/
              FEGETENVD ( OldEnvironment.d );
              OldEnvironment.i.lo |= SET_INVALID;
              FESETENVD ( OldEnvironment.d );
              return ( LONG_MAX );                               // return pinned result
              }
/*******************************************************************************
*     x < 0.0 and may or may not be out of the range of a long.                *
*******************************************************************************/
       if ( x > -2147483648.5 ) 
/*******************************************************************************
*     x is in the range of a long.                                             *
*******************************************************************************/
              {
              y = ( x - doubleToLong ) + doubleToLong;           // round at binary point
              if ( y != x ) 
              {                                                  // inexact case
                     FEGETENVD( OldEnvironment.d );              // save environment
                     FESETENVD( kUP.d );                         // round up
                     z = x - 0.5;                                // truncate x - 0.5
                     argument.d = z + doubleToLong;
                     FESETENVD( OldEnvironment.d );              // restore environment
                     return ( ( long ) argument.i.lo );
              }
              
              argument.d = y + doubleToLong;
              return ( ( long ) argument.i.lo );                 //  return long result
       }
/*******************************************************************************
*     Rounded negative x is out of the range of a long.                        *
*******************************************************************************/
       FEGETENVD( OldEnvironment.d );
       OldEnvironment.i.lo |= SET_INVALID;
       FESETENVD( OldEnvironment.d );
       return ( LONG_MIN );                                      // return pinned result
}

long int lroundf ( float x )
{       
       register float y, z;
       hexdouble OldEnvironment;
       hexsingle argument;
       register unsigned long int xhi;
       register long int target;
       const hexdouble kTZ = HEXDOUBLE(0x0, 0x1);
       const hexdouble kUP = HEXDOUBLE(0x0, 0x2);
       
       argument.fval = x;
       xhi = argument.lval & 0x7fffffff;                    // high 32 bits of x
       target = ( (unsigned long)argument.lval < 0x80000000ul );         // flags positive sign
 
       if ( xhi > 0x4f800000ul ) 
/*******************************************************************************
*     Is x is out of long range or NaN?                                        *
*******************************************************************************/
       {
              FEGETENVD ( OldEnvironment.d );               // get environment
              OldEnvironment.i.lo |= SET_INVALID;
              FESETENVD ( OldEnvironment.d );               // set environment
              if ( target )                                 // pin result
                     return ( LONG_MAX );
              else
                     return ( LONG_MIN );
       }
       
       if ( target ) 
/*******************************************************************************
*     Is sign of x is "+"?                                                     *
*******************************************************************************/
       {
              if ( x < 2147483647.5 ) 
/*******************************************************************************
*     x is in the range of a long.                                             *
*******************************************************************************/
              {
                     y = ( x + twoTo23 ) - twoTo23;    // round at binary point
                     if ( y != x )       
                     {                                           // inexact case
                            FEGETENVD (OldEnvironment.d );       // save environment
                            FESETENVD ( kTZ.d );                 // truncate rounding
                            z = x + 0.5;                         // truncate x + 0.5
                            argument.lval = z;			 // convert float to int
                            FESETENVD( OldEnvironment.d );       // restore environment
                            return ( ( long ) argument.lval );
                     }
                     
                     argument.lval = y;			 	    // convert float to int
                     return ( ( long ) argument.lval );             // return long result
              }
/*******************************************************************************
*     Rounded positive x is out of the range of a long.                        *
*******************************************************************************/
              FEGETENVD ( OldEnvironment.d );
              OldEnvironment.i.lo |= SET_INVALID;
              FESETENVD ( OldEnvironment.d );
              return ( LONG_MAX );                               // return pinned result
              }
/*******************************************************************************
*     x < 0.0 and may or may not be out of the range of a long.                *
*******************************************************************************/
       if ( x > -2147483648.5 ) 
/*******************************************************************************
*     x is in the range of a long.                                             *
*******************************************************************************/
              {
              y = ( x - twoTo23 ) + twoTo23;           // round at binary point
              if ( y != x ) 
              {                                                  // inexact case
                     FEGETENVD( OldEnvironment.d );              // save environment
                     FESETENVD( kUP.d );                         // round up
                     z = x - 0.5;                                // truncate x - 0.5
                     argument.lval = z;			 	 // convert float to int
                     FESETENVD( OldEnvironment.d );              // restore environment
                     return ( ( long ) argument.lval );
              }
              
              argument.lval = y;			 	 // convert float to int
              return ( ( long ) argument.lval );                 //  return long result
       }
/*******************************************************************************
*     Rounded negative x is out of the range of a long.                        *
*******************************************************************************/
       FEGETENVD( OldEnvironment.d );
       OldEnvironment.i.lo |= SET_INVALID;
       FESETENVD( OldEnvironment.d );
       return ( LONG_MIN );                                      // return pinned result
}

long long int llround ( double x )
{
    return (long long int)round ( x );
}

long long int llroundf ( float x )
{
    return (long long int)roundf ( x );
}

/*******************************************************************************
*                                                                              *
*     The function trunc truncates its double argument to integral value       *
*     and returns the result in double format.  This function signals          *
*     inexact if an ordered return value is not equal to the operand.          *
*                                                                              *
*******************************************************************************/
   
double trunc ( double x )
{       
       hexdouble argument, OldEnvironment;
       register double y;
       register unsigned long int xhi;
       register long int target;
       
       argument.d = x;
       xhi = argument.i.hi & 0x7fffffff;                         // xhi <- high half of |x|
       target = ( argument.i.hi < 0x80000000 );                  // flag positive sign
       
       if ( xhi < 0x43300000ul ) 
/*******************************************************************************
*     Is |x| < 2.0^53?                                                         *
*******************************************************************************/
       {
              if ( xhi < 0x3ff00000ul ) 
/*******************************************************************************
*     Is |x| < 1.0?                                                            *
*******************************************************************************/
              {
                     if ( ( xhi | argument.i.lo ) != 0ul ) 
                     {                                          // raise deserved INEXACT
                            FEGETENVD( OldEnvironment.d );
                            OldEnvironment.i.lo |= FE_INEXACT;
                            FESETENVD( OldEnvironment.d );
                     }
                     if ( target )                              // return properly signed zero
                            return ( 0.0 );
                     else
                            return ( -0.0 );
              }
/*******************************************************************************
*     Is 1.0 < |x| < 2.0^52?                                                   *
*******************************************************************************/
              if ( target ) 
              {
                     y = ( x + twoTo52 ) - twoTo52;             // round at binary point
                     if ( y > x )
                            return ( y - 1.0 );
                     else
                            return ( y );
              }
              else 
              {
                     y = ( x - twoTo52 ) + twoTo52;             // round at binary point.
                     if ( y < x )
                            return ( y + 1.0 );
                     else
                            return ( y );
              }
       }
/*******************************************************************************
*      Is |x| >= 2.0^52 or x is a NaN.                                         *
*******************************************************************************/
       return ( x );
}

float truncf ( float x )
{       
       hexdouble OldEnvironment;
       hexsingle argument;
       register float y;
       register unsigned long int xhi;
       register long int target;
       
       argument.fval = x;
       xhi = argument.lval & 0x7fffffff;                         // xhi <- |x|
       target = ( (unsigned long)argument.lval < 0x80000000ul );         // flags positive sign
       
       if ( xhi < 0x4b000000ul ) 
/*******************************************************************************
*     Is |x| < 2.0^23?                                                         *
*******************************************************************************/
       {
              if ( xhi < 0x3f800000 ) 
/*******************************************************************************
*     Is |x| < 1.0?                                                            *
*******************************************************************************/
              {
                     if ( xhi != 0ul ) 
                     {                                          // raise deserved INEXACT
                            FEGETENVD( OldEnvironment.d );
                            OldEnvironment.i.lo |= FE_INEXACT;
                            FESETENVD( OldEnvironment.d );
                     }
                     if ( target )                              // return properly signed zero
                            return ( 0.0 );
                     else
#if (__GNUC__>=3)
                            return ( -0.0 );
#else /* workaround gcc2.x botch of -0 return. */
                             {
                              volatile hexsingle zInHex;
                              zInHex.lval = 0x80000000;
                              return zInHex.fval;
                              }
#endif
              }
/*******************************************************************************
*     Is 1.0 < |x| < 2.0^23?                                                   *
*******************************************************************************/
              if ( target ) 
              {
                     y = ( x + twoTo23 ) - twoTo23;             // round at binary point
                     if ( y > x )
                            return ( y - 1.0 );
                     else
                            return ( y );
              }
              else 
              {
                     y = ( x - twoTo23 ) + twoTo23;             // round at binary point.
                     if ( y < x )
                            return ( y + 1.0 );
                     else
                            return ( y );
              }
       }
/*******************************************************************************
*      Is |x| >= 2.0^23 or x is a NaN.                                         *
*******************************************************************************/
       return ( x );
}

/*******************************************************************************
*                                                                              *
*     The modf family of functions separate a floating-point number into its   *
*     fractional and integral parts, returning the fractional part and writing *
*     the integral part in floating-point format to the object pointed to by a *
*     pointer argument.  If the input argument is integral or infinite in      *
*     value, the return value is a zero with the sign of the input argument.   *
*     The modf family of functions raises no floating-point exceptions. older  *
*     implemenation set the INVALID flag due to signaling NaN input.           *
*                                                                              *
*     modf is the double implementation.                                       *                             
*                                                                              *
*******************************************************************************/

#ifdef notdef
double modf ( double x, double *iptr )
{
      register double OldEnvironment, xtrunc;
      register unsigned long int xHead, signBit;
      hexdouble argument;
      
      argument.d = x;
      xHead = argument.i.hi & 0x7fffffff;                    // |x| high bit pattern
      signBit = ( argument.i.hi & 0x80000000 );              // isolate sign bit
      
      if ( xHead < 0x43300000ul ) 
/*******************************************************************************
*     Is |x| < 2.0^53?                                                         *
*******************************************************************************/
       {
            if ( xHead < 0x3ff00000ul )      
/*******************************************************************************
*     Is |x| < 1.0?                                                            *
*******************************************************************************/
              {
                  argument.i.hi = signBit;                    // truncate to zero
                  argument.i.lo = 0ul;
                  *iptr = argument.d;
                  return ( x );
              }
/*******************************************************************************
*     Is 1.0 < |x| < 2.0^52?                                                   *
*******************************************************************************/
              FEGETENVD( OldEnvironment );                    // save environment
              // round toward zero
              FESETENVD( TOWARDZERO.d );
            if ( signBit == 0ul )                             // truncate to integer
                  xtrunc = ( x + twoTo52 ) - twoTo52;
            else
                  xtrunc = ( x - twoTo52 ) + twoTo52;
              // restore caller's env
              FESETENVD( OldEnvironment );                    // restore environment
            *iptr = xtrunc;                                   // store integral part
            if ( x != xtrunc )                                // nonzero fraction
                  return ( x - xtrunc );
            else 
              {                                               // zero with x's sign
                  argument.i.hi = signBit;
                  argument.i.lo = 0ul;
                  return ( argument.d );
              }
       }
      
      *iptr = x;                                             // x is integral or NaN
      if ( x != x )                                          // NaN is returned
            return x;
      else 
       {                                                     // zero with x's sign
            argument.i.hi = signBit;
            argument.i.lo = 0ul;
            return ( argument.d );
       }
}
#else
static const hexdouble twoTo53 = HEXDOUBLE(0x43300000, 0x00000000);

double modf ( double x, double *iptr )
{
      register double OldEnvironment, xtrunc;
      hexdouble argument;
      
      register double FPR_negZero, FPR_zero, FPR_one, FPR_Two52, FPR_Two53, FPR_TowardZero, FPR_absx;
      
      FPR_absx = __FABS( x );						FPR_Two53 = twoTo53.d;
      FPR_one = 1.0;								argument.d = x;

      FPR_TowardZero = TOWARDZERO.d;				FPR_Two52 = twoTo52;	
      
      FPR_negZero = -0.0;							FPR_zero = 0.0;
      
      __ENSURE(FPR_zero, FPR_TowardZero, FPR_Two53); __ENSURE(FPR_zero, FPR_Two52, FPR_one);
            
/*******************************************************************************
*     Is |x| < 2.0^53?                                                         *
*******************************************************************************/
       if ( FPR_absx < FPR_Two53 ) 
       {
/*******************************************************************************
*     Is |x| < 1.0?                                                            *
*******************************************************************************/
              if ( FPR_absx < FPR_one )      
              {
                  if ( argument.i.hi & 0x80000000 )   		// isolate sign bit
                        *iptr = FPR_negZero;				// truncate to zero
                  else
                        *iptr = FPR_zero;					// truncate to zero
                  return ( x );
              }
/*******************************************************************************
*     Is 1.0 < |x| < 2.0^52?                                                   *
*******************************************************************************/
              FEGETENVD( OldEnvironment );                    // save environment
              // round toward zero
              FESETENVD( FPR_TowardZero );
              if ( x > FPR_zero )                             // truncate to integer
                    xtrunc = ( x + FPR_Two52 ) - FPR_Two52;
              else
                    xtrunc = ( x - FPR_Two52 ) + FPR_Two52;
              *iptr = xtrunc;                                 // store integral part
              // restore caller's env
              FESETENVD( OldEnvironment );                    // restore environment
              if ( x != xtrunc )                              // nonzero fraction
                    return ( x - xtrunc );
              else 
              {                                               // zero with x's sign
                  if ( argument.i.hi & 0x80000000 )   		  // isolate sign bit
                        return FPR_negZero;					  // truncate to zero
                  else
                        return FPR_zero;					  // truncate to zero
              }
       }
      
      *iptr = x;                                             // x is integral or NaN
      if ( x != x )                                          // NaN is returned
            return x;
      else 
      {                                                     // zero with x's sign
            if ( argument.i.hi & 0x80000000 )   			// isolate sign bit
                return FPR_negZero;							// truncate to zero
            else
                return FPR_zero;							// truncate to zero
      }
}
#endif

#else /* BUILDING_FOR_CARBONCORE_LEGACY */

float modff ( float x, float *iptr )
{
      register double OldEnvironment;
      register float xtrunc;
      register unsigned long int xHead, signBit;
      hexsingle argument;
      
      argument.fval = x;
      xHead = argument.lval & 0x7fffffff;                    // |x| high bit pattern
      signBit = ( argument.lval & 0x80000000 );              // isolate sign bit
      
      if ( xHead < 0x4b000000ul ) 
/*******************************************************************************
*     Is |x| < 2.0^23?                                                         *
*******************************************************************************/
       {
            if ( xHead < 0x3f800000 )      
/*******************************************************************************
*     Is |x| < 1.0?                                                            *
*******************************************************************************/
              {
                  argument.lval = signBit;                    // truncate to zero
                  *iptr = argument.fval;
                  return ( x );
              }
/*******************************************************************************
*     Is 1.0 < |x| < 2.0^23?                                                   *
*******************************************************************************/
              FEGETENVD( OldEnvironment );                    // save environment
              // round toward zero
              FESETENVD( TOWARDZERO.d );
            if ( signBit == 0ul )                             // truncate to integer
                  xtrunc = ( x + twoTo23 ) - twoTo23;
            else
                  xtrunc = ( x - twoTo23 ) + twoTo23;
              // restore caller's env
              FESETENVD( OldEnvironment );                    // restore environment
            *iptr = xtrunc;                                   // store integral part
            if ( x != xtrunc )                                // nonzero fraction
                  return ( x - xtrunc );
            else 
              {                                               // zero with x's sign
                  argument.lval = signBit;
                  return ( argument.fval );
              }
       }
      
      *iptr = x;                                             // x is integral or NaN
      if ( x != x )                                          // NaN is returned
            return x;
      else 
       {                                                     // zero with x's sign
            argument.lval = signBit;
            return ( argument.fval );
       }
}

#endif /* BUILDING_FOR_CARBONCORE_LEGACY */

#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
