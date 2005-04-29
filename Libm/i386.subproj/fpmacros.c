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
*     File:  fpmacros.c                                                        *
*                                                                              *
*     Contains:  C source code for PowerPC implementations of the inquiry      *
*     macros, as defined in C99.                                               *
*                                                                              *
*     Copyright © 1992-2001 Apple Computer, Inc.  All rights reserved.         *
*                                                                              *
*     Written by A. Sazegari and Jon Okada, started on November 29 1992.       *
*     Modified and ported by Robert A. Murley (ram) for Mac OS X.              *
*                                                                              *
*     fpmacros is a new file that brings all of C99 macros together.           *
*                                                                              *
*     A MathLib v4 file.                                                       *
*                                                                              *
*     Macros __isnormald, __isfinited, __isnand and __inf were previously      *
*     in fp.c.                                                                 *
*     Macros __fpclassifyf, __isnormalf, __isfinitef, __isnanf and             *
*     __signbitf were previously in fpfloatfunc.c.                             *
*     Macro __fpclassifyd was in classify.c.                                   *
*     Macro __signbitd was in sign.c.                                          *
*                                                                              *
*     Change History (most recent first):                                      *
*                                                                              *
*     06 Nov 01   ram   commented out warning about Intel architectures.       *
*                       changed i386 stubs to call abort().                    *
*     02 Nov 01   ram   added stubs for i386 routines.                         *
*     08 Oct 01   ram   removed <CoreServices/CoreServices.h>.                 *
*                       changed compiler errors to warnings.                   *
*     24 Sep 01   ram   corrected mantissa mask in fpclassifyf and isnanf.     *
*     18 Sep 01   ali   added <CoreServices/CoreServices.h> to get to <fp.h>.  *
*     10 Sep 01   ali   added macros to detect PowerPC and correct compiler.   *
*     09 Sep 01   ali   added more comments.                                   *
*     05 Sep 01   ram   added __inf routine.                                   *
*                       added #ifdef __ppc__.                                  *
*     07 Jul 01   ram   first created from fpfloatfunc.c, fp.c,                *
*                       classify.c and sign.c in MathLib v3 Mac OS9.           *
*                       replaced DblInHex typedef with hexdouble.              *
*                                                                              *
*     A version of gcc higher than 932 is required.                            *
*                                                                              *
*      GCC compiler options:                                                   *
*            optimization level 3 (-O3)                                        *
*            -fschedule-insns -finline-functions -funroll-all-loops            *
*                                                                              *
*******************************************************************************/

#include      "math.h"
#include      "fp_private.h"
#include      "fenv.h"

#if !defined(BUILDING_FOR_CARBONCORE_LEGACY)

/******************************************************************************
*     No other functions are called by these routines outside of fpmacros.c.  *
******************************************************************************/

unsigned int __math_errhandling ( void )
{
    return (MATH_ERREXCEPT); // return the bitmask indicating the error discipline(s) in use.
}

/**************************************************************************
   Function __fpclassifyf
   Returns the classification code of the argument float x, as defined in 
   C99.
**************************************************************************/

int __fpclassifyf ( float x )
{
   uint32_t iexp;
   hexsingle      z;
   
   z.fval = x;
   iexp = z.lval & 0x7f800000;             // isolate float exponent
   
   if (iexp == 0x7f800000) {               // NaN or INF case
      if ((z.lval & 0x007fffff) == 0)
         return FP_INFINITE;
      else if ((z.lval & fQuietNan) != 0)
         return FP_QNAN;
      else
         return FP_SNAN;
   }
   
   if (iexp != 0)                             // normal float
      return FP_NORMAL;
      
   if ((z.lval & 0x007fffff) == 0)
      return FP_ZERO;             // zero
   else
      return FP_SUBNORMAL;        //must be subnormal
}
   

/*************************************************************************
      Function __fpclassifyd                                               
      Returns the classification code of the argument double x, as 
      defined in C99.
*************************************************************************/

int __fpclassifyd ( double arg )
{
      uint32_t exponent;
      hexdouble      x;
            
      x.d = arg;
      
      exponent = x.i.hi & 0x7ff00000;
      if ( exponent == 0x7ff00000 )
      {
            if ( ( ( x.i.hi & 0x000fffff ) | x.i.lo ) == 0 )
                  return FP_INFINITE;
            else
                  return ( x.i.hi & dQuietNan ) ? FP_QNAN : FP_SNAN; 
      }
      else if ( exponent != 0)
            return FP_NORMAL;
      else
      {
            if ( ( ( x.i.hi & 0x000fffff ) | x.i.lo ) == 0 )
                  return FP_ZERO;
            else
                  return FP_SUBNORMAL;
      }
}

/*************************************************************************
      Function __fpclassify                                              
      Returns the classification code of the argument long double x, as 
      defined in C99.
*************************************************************************/

int __fpclassify ( long double arg )
{
    register uint16_t exponent;
    hexlongdouble z;
    
    z.e80 = arg;
    
    exponent = z.u.head & 0x7fff;
    if (exponent == 0x7fff)
    {
        if ((z.u.least_mantissa | (z.u.most_mantissa & 0x7fffffff)) == 0)
                return FP_INFINITE;
        else
                return FP_NAN;
    }
    else if (exponent != 0)
            return FP_NORMAL;
    else
    {
        if ((z.u.least_mantissa | (z.u.most_mantissa & 0x7fffffff)) == 0)
                return FP_ZERO;
        else
                return FP_SUBNORMAL;
    }
}

/***********************************************************************
   Function __isnormalf
   Returns nonzero if and only if x is a normalized float number and 
   zero otherwise.
***********************************************************************/

int __isnormalf ( float x )
{
   uint32_t iexp;
   hexsingle      z;
   
   z.fval = x;
   iexp = z.lval & 0x7f800000;                 /* isolate float exponent */
   return ((iexp != 0x7f800000) && (iexp != 0));
}
   

/***********************************************************************
   Function __isnormald
   Returns nonzero if and only if x is a normalized double number and 
   zero otherwise.
***********************************************************************/

int __isnormald ( double x )
{
   uint32_t iexp;
   hexdouble      z;
   
   z.d = x;
   iexp = z.i.hi & 0x7ff00000;                 /* isolate float exponent */
   return ((iexp != 0x7ff00000) && (iexp != 0));
}

/***********************************************************************
   Function __isnormal
   Returns nonzero if and only if x is a normalized long double and 
   zero otherwise.
***********************************************************************/

int __isnormal ( long double arg )
{
    uint16_t iexp;
    hexlongdouble z;
    
    z.e80 = arg;
    iexp = z.u.head & 0x7fff;
    return ((iexp != 0x7fff) && (iexp != 0));
}

/***********************************************************************
   Function __isfinitef
   Returns nonzero if and only if x is a finite (normal, subnormal, 
   or zero) float number and zero otherwise.
***********************************************************************/

int __isfinitef ( float x )
{   
   hexsingle      z;
   
   z.fval = x;
   return ((z.lval & 0x7f800000) != 0x7f800000);
}
   

/***********************************************************************
   Function __isfinited
   Returns nonzero if and only if x is a finite (normal, subnormal, 
   or zero) double number and zero otherwise.
***********************************************************************/

int __isfinited ( double x )
{
   hexdouble      z;
   
   z.d = x;
   return ((z.i.hi & 0x7ff00000) != 0x7ff00000);
}

/***********************************************************************
   Function __isfinite
   Returns nonzero if and only if x is a finite (normal, subnormal, 
   or zero) long double number and zero otherwise.
***********************************************************************/

int __isfinite ( long double arg )
{
    hexlongdouble z;
    
    z.e80 = arg;
    return ((z.u.head & 0x7fff) != 0x7fff);
}


/***********************************************************************
   Function __isinff
   Returns nonzero if and only if x is an infinite float number and zero 
   otherwise.
***********************************************************************/

int __isinff ( float x )
{   
   hexsingle      z;
   
   z.fval = x;
   return (((z.lval&0x7f800000) == 0x7f800000) && ((z.lval&0x007fffff) == 0));
}
   

/***********************************************************************
   Function __isinfd
   Returns nonzero if and only if x is an infinite double number and zero 
   otherwise.
***********************************************************************/

int __isinfd ( double x )
{
   hexdouble      z;
   
   z.d = x;
   return (((z.i.hi&0x7ff00000) == 0x7ff00000) && (((z.i.hi&0x000fffff) | z.i.lo) == 0));
}

/***********************************************************************
   Function __isinf
   Returns nonzero if and only if x is an infinite long double and zero 
   otherwise.
***********************************************************************/

int __isinf ( long double arg )
{
    hexlongdouble z;
    
    z.e80 = arg;
    return (((z.u.head & 0x7fff) == 0x7fff) && 
            ((z.u.least_mantissa | (z.u.most_mantissa & 0x7fffffff)) == 0));
}


/***********************************************************************
   Function __isnanf
   Returns nonzero if and only if x is a float NaN and zero otherwise.
***********************************************************************/

int __isnanf ( float x )
{   
   hexsingle      z;
   
   z.fval = x;
   return (((z.lval&0x7f800000) == 0x7f800000) && ((z.lval&0x007fffff) != 0));
}


/***********************************************************************
   Function __isnand
   Returns nonzero if and only if x is a double NaN and zero otherwise.
***********************************************************************/

int __isnand ( double x )
{
   hexdouble      z;
   
   z.d = x;
   return (((z.i.hi&0x7ff00000) == 0x7ff00000) && (((z.i.hi&0x000fffff) | z.i.lo) != 0));
}

/***********************************************************************
   Function __isnan
   Returns nonzero if and only if x is a long double NaN and zero otherwise.
***********************************************************************/

int __isnan ( long double arg )
{
    hexlongdouble z;
    
    z.e80 = arg;
    return (((z.u.head & 0x7fff) == 0x7fff) && 
            ((z.u.least_mantissa | (z.u.most_mantissa & 0x7fffffff)) != 0));
}


/***********************************************************************
   Function __signbitf
   Returns nonzero if and only if the sign bit of the float number x is 
   set and zero otherwise.
***********************************************************************/

int __signbitf ( float x )
{   
   hexsingle      z;
   
   z.fval = x;
   return (((int32_t)z.lval) < 0);
}


/***********************************************************************
   Function __signbitd
   Returns nonzero if and only if the sign bit of the double number x is 
   set and zero otherwise.
***********************************************************************/

int __signbitd ( double arg )
{
      hexdouble z;

      z.d = arg;
      return (((int32_t)z.i.hi) < 0);
}

/***********************************************************************
   Function __signbitl
   Returns nonzero if and only if the sign bit of the long double x is 
   set and zero otherwise.
***********************************************************************/

int __signbitl ( long double arg )
{
    hexlongdouble z;
    
    z.e80 = arg;
    return (((int16_t)z.u.head) < 0);
}

/***********************************************************************
   Function __inf
   Returns the value of positive infinity for a double.
***********************************************************************/

float __inff ( void )
{
      static const hexsingle PosINF  = { 0x7f800000 };
      return ( PosINF.fval );
}

long double __infl ( void )
{
      static const hexlongdouble PosINF  = { { 0x00000000, 0x00000000, 0x7fff } };
      return ( PosINF.e80 );
}

float __nan ( void )
{
      static const hexsingle aQuietNAN  = { 0x7fc00000 };
      return ( aQuietNAN.fval );
}

#else /* BUILDING_FOR_CARBONCORE_LEGACY */
 
double __inf ( void )
{
      static const hexdouble PosINF  = HEXDOUBLE(0x7ff00000, 0x00000000);
      return ( PosINF.d );
}

#endif /* BUILDING_FOR_CARBONCORE_LEGACY */
