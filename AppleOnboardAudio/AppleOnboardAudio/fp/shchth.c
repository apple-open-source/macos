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
*      File shchth.c,                                                          *
*      Function Sinh(x), Cosh(x) and Tanh(x);                                  *
*      Implementation of sine, cosine and tangent hyperbolic for the PowerPC.  *
*                                                                              *
*      Copyright © 1991-2001 Apple Computer, Inc.  All rights reserved.        *
*                                                                              *
*      Written by Ali Sazegari, started on November 1991,                      *
*      Modified and ported by Robert A. Murley (ram) for Mac OS X.             *
*                                                                              *
*      A MathLib v4 file.                                                      *
*                                                                              *
*      January  06 1992: changed the value of SEAM to Ãepsilon to conserve     *
*                        function monotonicity:                                *
*                        sqrt ( epsilon ) = 1.646361269956798117e-10           *
*                                         = 3fde0000b504f333f9de6484           *
*                                                                              *
*      December 03 1992: first rs6000 port.                                    *
*      January  05 1993: added the environmental controls.                     *
*      July     11 1993: changed feholdexcept to _feprocentry to set rounding  *
*                        to zero. added fenv_access pragama and fp_private.h,  *
*                        corrected the argument of the classification switch.  *
*      July     18 1994: merged th with sh and ch.                             *
*      August   02 1994: corrected the flags that we missed for the 601 ROM.   *
*      August   08 2001: replaced __setflm with FEGETENVD/FESETENVD.           *
*                        replaced DblInHex typedef with hexdouble.             *
*      Sept     19 2001: added macros to detect PowerPC and correct compiler.  *
*      Sept     19 2001: added <CoreServices/CoreServices.h> to get to <fp.h>  *
*                        and <fenv.h>, removed denormal comments.              *
*      Sept     24 2001: removed fenv_access pragma and some old comments.     *
*      Oct      08 2001: removed <CoreServices/CoreServices.h>.                *
*                        changed compiler errors to warnings.                  *
*      Nov      06 2001: commented out warning about Intel architectures.      *
*                                                                              *
*      W A R N I N G:                                                          * 
*      These routines require a 64-bit double precision IEEE-754 model.        *
*      They are written for PowerPC only and are expecting the compiler        *
*      to generate the correct sequence of multiply-add fused instructions.    *
*                                                                              *
*      These routines are not intended for 32-bit Intel architectures.         *
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

#include    "math.h"
#include    "fenv_private.h"
#include    "fp_private.h"

/*******************************************************************************
*            Functions needed for the computation.                             *
********************************************************************************
*                                                                              *
*     the following fp.h functions are used:                                   *
*     __fpclassifyd, copysign, exp, expm1 and __FABS.                          *
*******************************************************************************/

static hexdouble SqrtNegEps = HEXDOUBLE(0x3e400000, 0x00000000); 
static hexdouble Huge       = HEXDOUBLE(0x7ff00000, 0x00000000);
static const double kMinNormal = 2.2250738585072014e-308;  // 0x1.0p-1022


/*******************************************************************************
*     The function is odd.  The positive interval is computed and for          *
*     negative values, the sign is reflected in the computation.               *
*     Some computational flags are set in the FPSCR.  It is important to fold  *
*     them in at the end.                                                      *
********************************************************************************
*                            S      I      N      H                            *
*******************************************************************************/
#ifdef notdef
double sinh ( double x )
      {
      register double PositiveX;
      hexdouble OldEnvironment, NewEnvironment;
      
      FEGETENVD ( OldEnvironment.d );
      FESETENVD ( 0.0 );
      PositiveX = __FABS ( x );

      if ( PositiveX > SqrtNegEps.d )       /* return the arg if too small  */
            {                  
            PositiveX = expm1 ( PositiveX );
            if ( PositiveX != Huge.d )
                  PositiveX = 0.5 * ( PositiveX + PositiveX / ( 1.0 + PositiveX ) );
            }

      switch ( __fpclassifyd ( PositiveX ) )
            {
            case FP_SUBNORMAL:
                  OldEnvironment.i.lo |= FE_UNDERFLOW;
                  /* Falls through! */
            case FP_NORMAL:
                  OldEnvironment.i.lo |= FE_INEXACT;
                  /* Falls through! */
            default:
                  break;
            }

      FEGETENVD ( NewEnvironment.d );
      OldEnvironment.i.lo |= ( NewEnvironment.i.lo & EXCEPT_MASK );
      FESETENVD ( OldEnvironment.d );

      return ( copysign ( PositiveX, x ) );
      }
#else

static hexdouble Log2        = HEXDOUBLE(0x3FE62E42, 0xFEFA39EF); /* = 6.9314718055994530942E-1 */
static const double kMaxNormal = 1.7976931348623157e308;

double sinh ( double x )
{
      register double PositiveX;
      
      register double result, FPR_env, FPR_z, FPR_kMinNormal, FPR_half, FPR_one, 
                              FPR_ln2, FPR_sqreps, FPR_kMaxNormal, FPR_inf;
      
      PositiveX = __FABS ( x );
      FPR_z = 0.0f;				FPR_half = 0.5f;
      FPR_one = 1.0f;				FPR_sqreps = SqrtNegEps.d;
      FPR_inf = Huge.d;				FPR_kMinNormal = kMinNormal;
      FPR_ln2 = Log2.d;				FPR_kMaxNormal = kMaxNormal;
      
      FEGETENVD ( FPR_env);
      __ENSURE( FPR_z, FPR_one, FPR_inf );	__ENSURE( FPR_half, FPR_sqreps, FPR_kMinNormal );
      __ENSURE( FPR_z, FPR_kMaxNormal, FPR_ln2 );
      FESETENVD ( FPR_z );

      if ( PositiveX > FPR_sqreps )       	/* return the arg if too small  */
      {                  
            result = expm1 ( PositiveX );
            if ( result != FPR_inf )
                  result = FPR_half * ( result + result / ( FPR_one + result ) );
      }
      else
            result = PositiveX;

      FESETENVD ( FPR_env );
      
      if ( result != result)
            ; /* NOTHING */
      else if ( result == FPR_z )		// iff x == 0.0
            result = x; 			// Get +-0.0 right
      else if ( result < FPR_kMinNormal )
            __PROG_UF_INEXACT( FPR_kMinNormal );
      else if ( result < FPR_inf )
            __PROG_INEXACT( FPR_ln2 );
      else if ( PositiveX < FPR_inf )
            __PROG_OF_INEXACT( FPR_kMaxNormal );
      
      if ( x < FPR_z)
            result = -result;
      
      return result;
}
#endif
      
#ifdef notdef
float sinhf( float x)
{
    return (float)sinh( x );
}
#endif
      
/*******************************************************************************
*                              C      O      S      H                          *
*******************************************************************************/

#ifdef notdeef
double cosh ( double x )
      {
      hexdouble OldEnvironment, NewEnvironment;
      
      FEGETENVD ( OldEnvironment.d );
      FESETENVD ( 0.0 );
      
      x = 0.5 * exp ( __FABS ( x ) );
      x = x + 0.25 / x;
            
      FEGETENVD ( NewEnvironment.d );
      OldEnvironment.i.lo |= ( NewEnvironment.i.lo & EXCEPT_MASK );
      FESETENVD ( OldEnvironment.d );

      return x;
    }
#else
double cosh ( double x )
{
      hexdouble OldEnvironment, NewEnvironment;
      
      register double result, FPR_env, FPR_z, FPR_fourth, FPR_half, FPR_t;
      
      FPR_t = __FABS ( x );
      FPR_z = 0.0f;				FPR_half = 0.5f;
      FPR_fourth = 0.25f;

      FEGETENVD ( FPR_env);
      __ENSURE( FPR_z, FPR_half, FPR_fourth );
      FESETENVD ( FPR_z );
      
      FPR_t = FPR_half * exp ( FPR_t );
      
      result = FPR_t + FPR_fourth / FPR_t;	OldEnvironment.d = FPR_env;
            
      FEGETENVD ( NewEnvironment.d );
      OldEnvironment.i.lo |= ( NewEnvironment.i.lo & EXCEPT_MASK );
      FESETENVD ( OldEnvironment.d );

      return result;
}
#endif

#ifdef notdef
float coshf( float x)
{
    return (float)cosh( x );
}
#endif
      
/*******************************************************************************
*     This function is odd.  The positive interval is computed and for         *
*     negative values, the sign is reflected in the computation.               *
*     This calculation has spurious flags that need to be cleared before final *
*     exit.  Instead of clearing them, we keep the only set flag (inexact)     *
*     and do not fold the sticky FPSCR excpeions.  It makes for a faster tanh  *
*     and less errors with the test vectors.                                   *
********************************************************************************
*                            T      A      N      H                            *
*******************************************************************************/
#ifdef notdef
double tanh ( double x )
      {
      register double PositiveX;
      hexdouble OldEnvironment;

      PositiveX = __FABS ( x );

      /* N.B., NaN's fail all if's and the switch, and so trickle through. */
      
      if ( PositiveX == Huge.d )
            return (x >= 0 ? 1.0 : -1.0);

      FEGETENVD ( OldEnvironment.d );
      FESETENVD ( 0.0 );

/*******************************************************************************
*     Reduce the number of calls to exp1 function by using the identity:       *
*     th(x) = ( e^x - e^-x ) / ( e^x + e^-x )                                  *
*           = - ( e^-2x - 1 ) / ( 2 + ( e^-2x - 1 ) )                          *
*******************************************************************************/
      if ( PositiveX > SqrtNegEps.d ) /* return the arg if too small  */
            {                  
            PositiveX = expm1 ( -2.0 * PositiveX ); /* call exp1 once   */
            PositiveX = - PositiveX / ( 2.0 + PositiveX );
            }

/*******************************************************************************
*     If the argument to expm1 above is 7fe0000000000000 or 40d0000000000000   *
*     then expm1 will either set an overflow or an underflow which is          *
*     undeserved for tanh.                                                     *
*******************************************************************************/

      switch ( __fpclassifyd ( PositiveX ) )
            {
            case FP_SUBNORMAL:
                  OldEnvironment.i.lo |= FE_UNDERFLOW;
                  /* Falls through! */
            case FP_NORMAL:
                  OldEnvironment.i.lo |= FE_INEXACT;
                  /* Falls through! */
            default:
                  break;
            }

      FESETENVD ( OldEnvironment.d );

      if (x == 0 || x != x)	// +0 -0 NaN preserved
        return x;
      else
        return (x > 0 ? PositiveX : -PositiveX);
      }
#else
double tanh ( double x )
{
      register double PositiveX;

      register double result, FPR_env, FPR_z, FPR_kMinNormal, FPR_two, FPR_negTwo, 
                              FPR_ln2, FPR_sqreps, FPR_kMaxNormal, FPR_inf, FPR_t;
      
      PositiveX = __FABS ( x );
      FPR_z = 0.0f;				FPR_inf = Huge.d;
      FPR_two = 2.0f;				FPR_negTwo = -2.0f;
      FPR_sqreps = SqrtNegEps.d;		FPR_kMinNormal = kMinNormal;
      FPR_ln2 = Log2.d;				FPR_kMaxNormal = kMaxNormal;
      
      if ( PositiveX == FPR_inf )
            return (x >= FPR_z ? 1.0 : -1.0);

      FEGETENVD ( FPR_env );
      __ENSURE( FPR_z, FPR_inf, FPR_two );	__ENSURE( FPR_negTwo, FPR_sqreps, FPR_kMinNormal );
      __ENSURE( FPR_z, FPR_kMaxNormal, FPR_ln2 );
      FESETENVD ( FPR_z );

/*******************************************************************************
*     Reduce the number of calls to expm1 function by using the identity:      *
*     th(x) = ( e^x - e^-x ) / ( e^x + e^-x )                                  *
*           = - ( e^-2x - 1 ) / ( 2 + ( e^-2x - 1 ) )                          *
*******************************************************************************/
      if ( PositiveX > FPR_sqreps) /* return the arg if too small  */
      {                  
            FPR_t = expm1 ( FPR_negTwo * PositiveX ); /* call exp1 once   */
            result = - FPR_t / ( FPR_two + FPR_t );
      }
      else
            result = PositiveX;

/*******************************************************************************
*     If the argument to expm1 above is 7fe0000000000000 or 40d0000000000000   *
*     then expm1 will either set an overflow or an underflow which is          *
*     undeserved for tanh.                                                     *
*******************************************************************************/

      FESETENVD ( FPR_env );
      
      if ( result != result)
            ; /* NOTHING */
      else if ( result == FPR_z )		// iff x == 0.0
            result = x; 			// Get +-0.0 right
      else if ( result < FPR_kMinNormal )
            __PROG_UF_INEXACT( FPR_kMinNormal );
      else if ( result < FPR_inf )
            __PROG_INEXACT( FPR_ln2 );
      else if ( PositiveX < FPR_inf )
            __PROG_OF_INEXACT( FPR_kMaxNormal );
      
      if ( x < FPR_z)
            result = -result;
      
      return result;
}
#endif

#ifdef notdef
float tanhf( float x)
{
    return (float)tanh( x );
}
#endif
      
#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
