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
*     File atan2.c,                                                            *
*     Function atan2(y/x),                                                     *
*     Implementation of arc tangent of ( y / x ) for the PowerPC.              *
*                                                                              *
*     Copyright c 1991-2001 Apple Computer, Inc.  All rights reserved.         *
*                                                                              *
*     Written by Ali Sazegari, started on November 1991,                       *
*     Modified and ported by Robert A. Murley (ram) for Mac OS X.              *
*                                                                              *
*     A MathLib v4 file.                                                       *
*                                                                              *
*     December 03 1992: first rs6000 port.                                     *
*     January  05 1993: added the environmental controls.                      *
*     July     07 1993: fixed the argument of __fpclassifyd.                   *
*     July     11 1993: changed feholdexcept to _feprocentry to set rounding   *
*                       to zero. removed the constant nan, fixed all edge      *
*                       cases to reflect the fpce-nceg requirements.           *
*     September19 1994: changed all the environemntal enquiries to __setflm.   *
*     October  06 1994: initialized CurrentEnvironment to correct an invalid   *
*                        flag problem.                                         *
*     July     23 2001: replaced __setflm with FEGETENVD/FESETENVD;            *
*                       replaced DblInHex typedef with hexdouble.              *
*     September 07 2001: added #ifdef __ppc__.                                 *
*     September 09 2001: added more comments.                                  *
*     September 10 2001: added macros to detect PowerPC and correct compiler.  *
*     September 18 2001: added <CoreServices/CoreServices.h> to get to <fp.h>  *
*                        and <fenv.h>.                                         *
*     September 25 2001: removed fenv_access pragma.                           *
*     October   05 2001: added const double pi.                                *
*     October   08 2001: removed <CoreServices/CoreServices.h>.                *
*                        changed compiler errors to warnings.                  *
*     November  06 2001: commented out warning about Intel architectures.      *
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

#include    "math.h"
#include    "fenv_private.h"
#include    "fp_private.h"

static const double pi = 3.141592653589793116e+00;

/*******************************************************************************
*            Functions needed for the computation.                             *
*******************************************************************************/

/*     the following fp.h functions are used:                                 */
/*     atan, __fpclassifyd, copysign and __signbitd.                          */

/*******************************************************************************
*     This function is odd.  The positive interval is computed and for         *
*     negative values, the sign is reflected in the computation.               *
*******************************************************************************/
#ifdef notdef
double atan2 ( double y, double x )
      {
      register double result;
      hexdouble OldEnvironment, CurrentEnvironment;
      CurrentEnvironment.i.lo = 0;
      

/*******************************************************************************
*     If argument is SNaN then a QNaN has to be returned and the invalid       *
*     flag signaled.                                                           * 
*******************************************************************************/

      if ( ( x != x ) || ( y != y ) )
            return x + y;
      
      FEGETENVD( OldEnvironment.d );
      FESETENVD( 0.0 );

/*******************************************************************************
*     The next switch will decipher what sort of argument we have:             *
*                                                                              *
*     atan2 ( ±0, x ) = ±0, if x > 0,                                          *
*     atan2 ( ±0, +0) = ±0,                                                    *
*     atan2 ( ±0, x ) = ±π, if x < 0,                                          *
*     atan2 ( ±0, -0) = ±π,                                                    *
*     atan2 ( y, ±0 ) = π/2, if y > 0,                                         *
*     atan2 ( y, ±0 ) = -π/2, if y < 0,                                        *
*     atan2 ( ±y, ∞ ) = ±0, for finite y > 0,                                  *
*     atan2 ( ±∞, x ) = ±π/2, for finite x,                                    *
*     atan2 ( ±y, -∞) = ±π, for finite y > 0,                                  *
*     atan2 ( ±∞, ∞ ) = ±π/4,                                                  *
*     atan2 ( ±∞, -∞) = ±3π/4.                                                 *
*                                                                              *
*     note that the non obvious cases are y and x both infinite or both zero.  *
*     for more information, see “Branch Cuts for Complex Elementary Functions, *
*     or much Much Ado About Nothing’s Sign bit”, by W. Kahan, Proceedings of  *
*     the joint IMA/SIAM conference on The state of the Art in Numerical       *
*     Analysis, 14-18 April 1986, Clarendon Press (1987).                      *
*                                                                              *
*     atan2(y,0) does not raise the divide-by-zero exception, nor does         *
*     atan2(0,0) raise the invalid exception.                                  *
*******************************************************************************/

      switch ( __fpclassifyd ( x ) )
            {
            case FP_ZERO:
                  CurrentEnvironment.i.lo |= FE_INEXACT;
                  if ( y > 0.0 )
                        x = + 0.5 * pi;
                  else if ( y < 0.0 )
                        x = - 0.5 * pi;
                  else
                        {
                        if ( __signbitd ( x ) )
                              x = copysign ( pi, y );
                        else
                              {
                              CurrentEnvironment.i.lo &= ~FE_INEXACT;
                              x = y;
                              }
                        }
                  OldEnvironment.i.lo |= CurrentEnvironment.i.lo;
                  FESETENVD( OldEnvironment.d );
                  return x;
            case FP_INFINITE:
                  if ( x > 0.0 )
                        {
                        if ( __isfinited ( y ) )
                              x = copysign ( 0.0, y );
                        else
                              {
                              CurrentEnvironment.i.lo |= FE_INEXACT;
                              x = copysign ( 0.25 * pi, y );
                              }
                        }
                  else
                        {
                        CurrentEnvironment.i.lo |= FE_INEXACT;
                        if ( __isfinited ( y ) )
                              x = copysign ( pi, y );
                        else
                              x = copysign ( 0.75 * pi, y );
                        }
                  OldEnvironment.i.lo |= CurrentEnvironment.i.lo;
                  FESETENVD( OldEnvironment.d );
                  return x;
            default:
                  break;
            }
                        
      switch ( __fpclassifyd ( y ) )
            {
            case FP_ZERO:
                  if ( x > 0.0 )
                        x = y;
                  else
                        {
                        CurrentEnvironment.i.lo |= FE_INEXACT;
                        x = copysign ( pi, y );
                        }
                  OldEnvironment.i.lo |= CurrentEnvironment.i.lo;
                  FESETENVD( OldEnvironment.d );
                  return x;
            case FP_INFINITE:
                  CurrentEnvironment.i.lo |= FE_INEXACT;
                  x = copysign ( 0.5 * pi, y );
                  OldEnvironment.i.lo |= CurrentEnvironment.i.lo;
                  FESETENVD( OldEnvironment.d );
                  return x;
            default:
                  break;
            }
      
/*******************************************************************************
*     End of the special case section. atan2 is mostly a collection of special *
*     case functions.  Next we will carry out the main computation which at    *
*     this point will only receive normal or denormal numbers.                 *
*******************************************************************************/
      
      result = atan ( __FABS ( y / x ) );
      FEGETENVD( CurrentEnvironment.d );
      CurrentEnvironment.i.lo &= ~( FE_UNDERFLOW | FE_OVERFLOW );
      if ( __signbitd ( x ) )
            result = pi - result;

      switch ( __fpclassifyd ( result ) )
            {
            case FP_SUBNORMAL:
                  CurrentEnvironment.i.lo |= FE_UNDERFLOW;
                  /* FALL THROUGH */
            case FP_NORMAL:
                  CurrentEnvironment.i.lo |= FE_INEXACT;
                  /* FALL THROUGH */
            default:
                  break;
            }
            
      OldEnvironment.i.lo |= CurrentEnvironment.i.lo;
      FESETENVD( OldEnvironment.d );

      return ( copysign ( result, y ) );
      }
#else

static const double kMinNormal = 2.2250738585072014e-308;  // 0x1.0p-1022
static const double kMaxNormal = 1.7976931348623157e308;
static const double kHalf = 0.5;

extern double atanCore ( double );
extern double atanCoreInv ( double );

double atan2 ( double y, double x )
{
      register double result;      
      register double FPR_env, FPR_z, FPR_half, FPR_pi, FPR_kMinNormal, FPR_kMaxNormal, FPR_absx, FPR_absy, FPR_t;
      
      FPR_z = 0.0;					
      
/*******************************************************************************
*     If argument is SNaN then a QNaN has to be returned and the invalid       *
*     flag signaled.                                                           * 
*******************************************************************************/

      if ( ( x != x ) || ( y != y ) )
            return x + y;
      
      FEGETENVD( FPR_env );
      FESETENVD( FPR_z );
      
      __ORI_NOOP;	// takes slot 0  following the mtfsf
      FPR_t = y / x;	// takes slot 1 (hence fpu1) following the mtfsf
      FPR_pi = pi;
      
      FPR_kMinNormal = kMinNormal;			FPR_kMaxNormal = kMaxNormal; 
           
      FPR_absy = __FABS ( y );
      FPR_half = kHalf;                                
      FPR_absx = __FABS ( x );
                  
/*******************************************************************************
*     The next switch will decipher what sort of argument we have:             *
*                                                                              *
*     atan2 ( ±0, x ) = ±0, if x > 0,                                          *
*     atan2 ( ±0, +0) = ±0,                                                    *
*     atan2 ( ±0, x ) = ±π, if x < 0,                                          *
*     atan2 ( ±0, -0) = ±π,                                                    *
*     atan2 ( y, ±0 ) = π/2, if y > 0,                                         *
*     atan2 ( y, ±0 ) = -π/2, if y < 0,                                        *
*     atan2 ( ±y, ∞ ) = ±0, for finite y > 0,                                  *
*     atan2 ( ±∞, x ) = ±π/2, for finite x,                                    *
*     atan2 ( ±y, -∞) = ±π, for finite y > 0,                                  *
*     atan2 ( ±∞, ∞ ) = ±π/4,                                                  *
*     atan2 ( ±∞, -∞) = ±3π/4.                                                 *
*                                                                              *
*     note that the non obvious cases are y and x both infinite or both zero.  *
*     for more information, see “Branch Cuts for Complex Elementary Functions, *
*     or much Much Ado About Nothing’s Sign bit”, by W. Kahan, Proceedings of  *
*     the joint IMA/SIAM conference on The state of the Art in Numerical       *
*     Analysis, 14-18 April 1986, Clarendon Press (1987).                      *
*                                                                              *
*     atan2(y,0) does not raise the divide-by-zero exception, nor does         *
*     atan2(0,0) raise the invalid exception.                                  *
*******************************************************************************/

      if ( FPR_absx <= FPR_kMaxNormal ) // slot 0 hence fpu0
      {
            if ( x == FPR_z ) // slot 0 hence fpu 0
            {
                  if ( y > FPR_z )
                        result = __FMUL( FPR_half, FPR_pi );
                  else if ( y < FPR_z )
                        result = __FMUL( -FPR_half, FPR_pi );
                  else
                  {
                        if ( __signbitd ( x ) ) // x is +-0
                              result = copysign ( FPR_pi, y ); // y is +-0
                        else
                        {
                              FESETENVD( FPR_env );
                              return y;			// Exact zero result.
                        }
                  }
                  FESETENVD( FPR_env );
                  __PROG_INEXACT( FPR_pi );
                  return result;
            }
      }
      else
      {	    // Infinite x
            if ( x > FPR_z )
            {
                if ( FPR_absy <= FPR_kMaxNormal )
                {
                        FESETENVD( FPR_env );
                        result = copysign ( FPR_z, y );
                        return result;
                }
                else
                {
                        if ( y > FPR_z )
                            result = __FMUL( 0.25f, FPR_pi );
                        else
                            result = __FMUL( -0.25f, FPR_pi );
                }
            }
            else
            {
                if ( FPR_absy <= FPR_kMaxNormal )
                        result = copysign ( FPR_pi, y );
                else
                {
                        if ( y > FPR_z )
                            result = __FMUL( 0.75f, FPR_pi );
                        else
                            result = __FMUL( -0.75f, FPR_pi );
                }
            }
            FESETENVD( FPR_env );
            __PROG_INEXACT( FPR_pi );
            return result;
      }
      
      if ( FPR_absy <= FPR_kMaxNormal )
      {
            if ( y == FPR_z )
            {
                  if ( x > FPR_z )
                  {
                        FESETENVD( FPR_env );
                        return y;
                  }
                  else
                        result = copysign ( FPR_pi, y ); // y is +-0
                        
                  FESETENVD( FPR_env );
                  __PROG_INEXACT( FPR_pi );
                  return result;
            }
      }
      else
      {		  // Infinite y
                  if ( y > FPR_z )
                        result =  __FMUL( FPR_half, FPR_pi );
                  else
                        result =  __FMUL( -FPR_half, FPR_pi );
                  FESETENVD( FPR_env );
                  __PROG_INEXACT( FPR_pi );
                  return result;
      }
      
/*******************************************************************************
*     End of the special case section. atan2 is mostly a collection of special *
*     case functions.  Next we will carry out the main computation which at    *
*     this point will only receive non-zero normal or denormal numbers.        *
*******************************************************************************/
      if (FPR_absy > FPR_absx)
            result = atanCoreInv ( __FABS( FPR_t ) );
      else
            result = atanCore ( __FABS( FPR_t ) );
      
      if ( x < FPR_z )
            result = FPR_pi - result;
            
      FPR_t = __FABS( result );

      FESETENVD( FPR_env );
      if ( FPR_t >= FPR_kMinNormal || FPR_t == FPR_z )
            __PROG_INEXACT( FPR_pi );
      else
            __PROG_UF_INEXACT( FPR_kMinNormal );

      if ( y > FPR_z )
            return result;
      else
            return -result;
}
#endif

#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
