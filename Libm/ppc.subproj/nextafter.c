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
*     File nextafter.c,                                                        *
*     Function nextafterd for PowerPC based machines.                          *
*                                                                              *
*     The computation of IEEE-754 nextafter using a floating-point method.     *
*                                                                              *
*     Copyright © 1991-2001 Apple Computer, Inc.  All rights reserved.         *
*                                                                              *
*     Written by A. Sazegari, started on September 1991.                       *
*     Modified and ported by Robert A. Murley (ram) for Mac OS X.              *
*                                                                              *
*     A MathLib v4 file.                                                       *
*                                                                              *
*     Change History (most recent last):                                       *
*                                                                              *
*     October    01  1991: passes all test vectors with no tolerances.         *
*     November   11  1991: changed +INF representation from 7FFF8000...        *
*                          to 7FFF000... same thing for -INF.                  *
*     November   15  1991: changed classdouble and copysign to CLASSIFY and    *
*                          COPYSIGN.                                           *
*     November   17  1991: changed classdouble to CLASSEXTENDEDint.  passes    *
*                          the test suite.                                     *
*     February   07  1992: changed COPYSIGN with COPYSIGNold.                  *
*     September  24  1992: took the "#include support.h" out.                  *
*     December   03  1992: first rs6000 port.                                  *
*     December   13  1992: added the environmental support.                    *
*     December   15  1993: created a version without environmental enquires,   *
*                          added the rounding direction control previously     *
*                          taken care of by feholdexcept (not really!) and     *
*                          feupdateenv.  changed pack4.h to fp_private.h.      *
*     October    12  1994: fixed the environmental problem and tested it.      *
*     September  21  1995: corrected the error in the restoration of the       *
*                          rounding mode.                                      *
*     June       27  2001: ported routine to OSX, replaced calls to fenv.c     *
*                          with inline routines. This technique caused the     *
*                          routine to speed up by a factor of three.           *
*     July       16  2001: replaced __setflm with FEGETENVD/FESETENVD.         *
*     August     28  2001: added description of routine.                       *
*     September  05  2001: added #ifdef __ppc__.                               *
*     September  10  2001: added macros to detect PowerPC and correct compiler.*
*     September  10  2001: added more comments.                                *
*     September  18  2001: added <CoreServices/CoreServices.h> to get <fenv.h>.*
*     October    08  2001: removed <CoreServices/CoreServices.h>.              *
*                          changed compiler errors to warnings.                *
*     November   02  2001: added stub for i386 version of nextafterd.          *
*     November   06  2001: commented out warning about Intel architectures.    *
*                          changed i386 stub to call abort().                  *
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

#include      "math.h"
#include      "fp_private.h"
#include      "fenv_private.h"

/*******************************************************************************
********************************************************************************
*                        N  E  X  T  A  F  T  E  R  D                          *
********************************************************************************
*                                                                              *
*      Computes the next representable double value after 'x' in the direction *
*      of 'y'.  if x == y then y is returned.                                  *
*      This algorithm uses the smallest positive number with rounding          *
*      directions to compute the nextafter of a double number.                 *
*                                                                              *
*      Functions used in this program: __fpclassify.                           *
*                                                                              *
*******************************************************************************/

static long int ___fpclassifyf ( float x )
{
   unsigned long int iexp;
   hexsingle      z;
   
   z.fval = x;
   iexp = z.lval & 0x7f800000;             // isolate float exponent
   
   if (iexp == 0x7f800000) {               // NaN or INF case
      if ((z.lval & 0x007fffff) == 0)
         return (long int) FP_INFINITE;
      else if ((z.lval & fQuietNan) != 0)
         return (long int) FP_QNAN;
      else
         return (long int) FP_SNAN;
   }
   
   if (iexp != 0)                             // normal float
      return (long int) FP_NORMAL;
      
   if ((z.lval & 0x007fffff) == 0)
      return (long int) FP_ZERO;             // zero
   else
      return (long int) FP_SUBNORMAL;        //must be subnormal
}
   

static long int ___fpclassifyd ( double arg )
{
      register unsigned long int exponent;
      hexdouble      x;
            
      x.d = arg;
      
      exponent = x.i.hi & 0x7ff00000;
      if ( exponent == 0x7ff00000 )
      {
            if ( ( ( x.i.hi & 0x000fffff ) | x.i.lo ) == 0 )
                  return (long int) FP_INFINITE;
            else
                  return ( x.i.hi & dQuietNan ) ? FP_QNAN : FP_SNAN; 
      }
      else if ( exponent != 0)
            return (long int) FP_NORMAL;
      else
      {
            if ( ( ( x.i.hi & 0x000fffff ) | x.i.lo ) == 0 )
                  return (long int) FP_ZERO;
            else
                  return (long int) FP_SUBNORMAL;
      }
}

static double __nextafter ( double x, double y )
      {
      static hexdouble EPSILON = HEXDOUBLE(0x00000000, 0x00000001);
      static hexdouble PosINF  = HEXDOUBLE(0x7ff00000, 0x00000000);
      static hexdouble NegINF  = HEXDOUBLE(0xfff00000, 0x00000000);
      static hexdouble PosBig  = HEXDOUBLE(0x7fefffff, 0xffffffff);
      static hexdouble NegBig  = HEXDOUBLE(0xffefffff, 0xffffffff);
      double arg;
      hexdouble      temp, xsign, ysign;
      register int newexc;
      fenv_t envp;
      
//    Save old environment
      FEGETENVD(temp.d);
      envp = temp.i.lo;
      
//    Clear all flags in temp variable and set rounding to nearest
      temp.i.lo &= (FE_NO_FLAGS & FE_NO_ENABLES & FE_NO_RND);
      temp.i.lo |= FE_TONEAREST;
      
      if ( ( x != x ) || ( y != y ) )      // one of the arguments is a NaN
            arg = y + x;
      else if ( x == y )      /*  Exit here in case the answer is ±INF.      */
            {                 /*  Otherwise unwanted flags will be set.      */
            // Save exceptions, restore environment with saved exceptions
            newexc = temp.i.lo & FE_ALL_EXCEPT;
            temp.i.lo = envp;
            if ((newexc & FE_INVALID) != 0)
                  temp.i.lo |= SET_INVALID;
            temp.i.lo |=  newexc & ( FE_INEXACT | FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW );
            FESETENVD(temp.d);
            if ( ( x ==  0.0 ) && ( y == 0.0 )) // (*0, -0)-> -0, (*0, +0)-> +0 i.e. "y"
                return y;
            else
                return x;
            }
      else if ( ( x == PosINF.d ) || ( x == NegINF.d ) )
            arg = ( x > 0 ) ? PosBig.d : NegBig.d;      // x and/or y is INF
      else if ( x == 0.0 )
            {
            xsign.d = EPSILON.d;      // copy y's sign to EPSILON
            ysign.d = y;
            xsign.i.hi &= 0x7fffffff;
            xsign.i.hi |= ysign.i.hi & 0x80000000;
            arg = xsign.d;
            temp.i.lo |=  FE_INEXACT | FE_UNDERFLOW;
            }
      else if ( ( ( x < 0.0 ) && ( x < y ) ) || ( ( x > 0.0 ) && ( x > y ) ) )
            {            /*  Always clear intermediate spurious inexact.     */
            temp.i.lo = (temp.i.lo & FE_NO_RND) | FE_TOWARDZERO;
            FESETENVD(temp.d);
            arg = ( x < 0.0 ) ? x + EPSILON.d : x - EPSILON.d;
            temp.i.lo &= ~( FE_INEXACT | FE_UNDERFLOW );
            if ((temp.i.lo & FE_ALL_EXCEPT) == 0)
                  temp.i.lo &= FE_CLR_FX;
            }
      else if ( ( x < 0.0 ) && ( x > y ) )
            {
            temp.i.lo = (temp.i.lo & FE_NO_RND) | FE_DOWNWARD;
            FESETENVD(temp.d);
            arg = x - EPSILON.d;
            temp.i.lo &= ~( FE_INEXACT | FE_UNDERFLOW );
            if ((temp.i.lo & FE_ALL_EXCEPT) == 0)
                  temp.i.lo &= FE_CLR_FX;
            }
      else
            {
            temp.i.lo = (temp.i.lo & FE_NO_RND) | FE_UPWARD;
            FESETENVD(temp.d);
            arg = x + EPSILON.d;
            temp.i.lo &= ~( FE_INEXACT | FE_UNDERFLOW );
            if ((temp.i.lo & FE_ALL_EXCEPT) == 0)
                  temp.i.lo &= FE_CLR_FX;
            }
      
/*******************************************************************************
*     Set the flags according to the menu du jour.                             *
*******************************************************************************/

      switch ( ___fpclassifyd ( arg ) )
            {
            case FP_ZERO:
                  xsign.d = arg;      // copy sign from x to arg
                  ysign.d = x;
                  xsign.i.hi &= 0x7fffffff;
                  xsign.i.hi |= ( ysign.i.hi & 0x80000000 );
                  arg = xsign.d;
                  /* FALL THROUGH */
            case FP_SUBNORMAL:
                  temp.i.lo |=  FE_INEXACT | FE_UNDERFLOW;
                  break;
            case FP_INFINITE:
                  temp.i.lo |=  FE_INEXACT | FE_OVERFLOW;
                  break;
            }
      
/*******************************************************************************
*     Not to worry about the rounding directions.  feupdateenv saves the day.  *
*     In this version, one must restore the rounding direction.                *
*******************************************************************************/
      
      newexc = temp.i.lo & FE_ALL_EXCEPT;
      temp.i.lo = envp;
      if ((newexc & FE_INVALID) != 0)
            temp.i.lo |= SET_INVALID;
      temp.i.lo |=  newexc & ( FE_INEXACT | FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW );
      FESETENVD(temp.d);
      
      return arg;
      }

#if !defined(BUILDING_FOR_CARBONCORE_LEGACY)

double nextafter(double x, double y)
{
        return __nextafter( x, y );
}

#else /* BUILDING_FOR_CARBONCORE_LEGACY */      
/* Legacy nextafterd() API */

double nextafterd(double x, double y)
{
        return __nextafter( x, y );
}

float nextafterf ( float x, float y )
      {
      static hexsingle EPSILON = { 0x00000001 };
      static hexsingle PosINF  = { 0x7f800000 };
      static hexsingle NegINF  = { 0xff800000 };
      static hexsingle PosBig  = { 0x7f7fffff };
      static hexsingle NegBig  = { 0xff7fffff };
      double arg;
      hexdouble      temp;
      hexsingle      xsign, ysign;
      register int newexc;
      fenv_t envp;
      
//    Save old environment
      FEGETENVD(temp.d);
      envp = temp.i.lo;
      
//    Clear all flags in temp variable and set rounding to nearest
      temp.i.lo &= (FE_NO_FLAGS & FE_NO_ENABLES & FE_NO_RND);
      temp.i.lo |= FE_TONEAREST;
      
      if ( ( x != x ) || ( y != y ) )      // one of the arguments is a NaN
            arg = y + x;
      else if ( x == y )      /*  Exit here in case the answer is ±INF.      */
            {                 /*  Otherwise unwanted flags will be set.      */
            // Save exceptions, restore environment with saved exceptions
            newexc = temp.i.lo & FE_ALL_EXCEPT;
            temp.i.lo = envp;
            if ((newexc & FE_INVALID) != 0)
                  temp.i.lo |= SET_INVALID;
            temp.i.lo |=  newexc & ( FE_INEXACT | FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW );
            FESETENVD(temp.d);
            if ( ( x ==  0.0 ) && ( y == 0.0 )) // (*0, -0)-> -0, (*0, +0)-> +0
                return y;
            else
                return x;
            }
      else if ( ( x == PosINF.fval ) || ( x == NegINF.fval ) )
            arg = ( x > 0 ) ? PosBig.fval : NegBig.fval;      // x and/or y is INF
      else if ( x == 0.0 )
            {
            xsign.fval = EPSILON.fval;      // copy y's sign to EPSILON
            ysign.fval = y;
            xsign.lval &= 0x7fffffff;
            xsign.lval |= ysign.lval & 0x80000000;
            arg = xsign.fval;
            temp.i.lo |=  FE_INEXACT | FE_UNDERFLOW;
            }
      else if ( ( ( x < 0.0 ) && ( x < y ) ) || ( ( x > 0.0 ) && ( x > y ) ) )
            {            /*  Always clear intermediate spurious inexact.     */
            temp.i.lo = (temp.i.lo & FE_NO_RND) | FE_TOWARDZERO;
            FESETENVD(temp.d);
            arg = ( x < 0.0 ) ? x + EPSILON.fval : x - EPSILON.fval;
            temp.i.lo &= ~( FE_INEXACT | FE_UNDERFLOW );
            if ((temp.i.lo & FE_ALL_EXCEPT) == 0)
                  temp.i.lo &= FE_CLR_FX;
            }
      else if ( ( x < 0.0 ) && ( x > y ) )
            {
            temp.i.lo = (temp.i.lo & FE_NO_RND) | FE_DOWNWARD;
            FESETENVD(temp.d);
            arg = x - EPSILON.fval;
            temp.i.lo &= ~( FE_INEXACT | FE_UNDERFLOW );
            if ((temp.i.lo & FE_ALL_EXCEPT) == 0)
                  temp.i.lo &= FE_CLR_FX;
            }
      else
            {
            temp.i.lo = (temp.i.lo & FE_NO_RND) | FE_UPWARD;
            FESETENVD(temp.d);
            arg = x + EPSILON.fval;
            temp.i.lo &= ~( FE_INEXACT | FE_UNDERFLOW );
            if ((temp.i.lo & FE_ALL_EXCEPT) == 0)
                  temp.i.lo &= FE_CLR_FX;
            }
      
/*******************************************************************************
*     Set the flags according to the menu du jour.                             *
*******************************************************************************/

      switch ( ___fpclassifyf ( arg ) )
            {
            case FP_ZERO:
                  xsign.fval = arg;      // copy sign from x to arg
                  ysign.fval = x;
                  xsign.lval &= 0x7fffffff;
                  xsign.lval |= ( ysign.lval & 0x80000000 );
                  arg = xsign.fval;
            case FP_SUBNORMAL:
                  temp.i.lo |=  FE_INEXACT | FE_UNDERFLOW;
                  break;
            case FP_INFINITE:
                  temp.i.lo |=  FE_INEXACT | FE_OVERFLOW;
                  break;
            }
      
/*******************************************************************************
*     Not to worry about the rounding directions.  feupdateenv saves the day.  *
*     In this version, one must restore the rounding direction.                *
*******************************************************************************/
      
      newexc = temp.i.lo & FE_ALL_EXCEPT;
      temp.i.lo = envp;
      if ((newexc & FE_INVALID) != 0)
            temp.i.lo |= SET_INVALID;
      temp.i.lo |=  newexc & ( FE_INEXACT | FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW );
      FESETENVD(temp.d);
      
      return arg;
      }

#endif /* BUILDING_FOR_CARBONCORE_LEGACY */

#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
