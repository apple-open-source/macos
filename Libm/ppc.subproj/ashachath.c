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
*      File ashachath.c,                                                       *
*      Function ArcSinh(x), ArcCosh(x), ArcTanh(x);                            *
*      Implementation of arc sine, arc cosine and arc tangent hyperbolic for   *
*      the PowerPC.                                                            *
*                                                                              *
*      Copyright © 1991 Apple Computer, Inc.  All rights reserved.             *
*                                                                              *
*      Written by Ali Sazegari, started on December 1991,                      *
*      Modified and ported by Robert A. Murley (ram) for Mac OS X.             *
*                                                                              *
*      A MathLib v4 file.                                                      *
*                                                                              *
*      January  28 1992: added velvel’s algorithms for ArcSinh and ArcCosh.    *
*                                                                              *
*      December 03 1992: first rs6000 port.                                    *
*      January  05 1993: added the environmental controls.                     *
*      July     07 1993: changed the comment for square root of epsilon,       *
*                        made atanh symmetric about zero using copysign,       *
*                        changed the exception switch argument for asinh from  *
*                        x to PositiveX, switched to the use of string         *
*                        oriented nan.                                         *
*      July     29 1993: corrected the string nan.                             *
*      July     18 1994: changed the logic to avoid checking for NaNs,         *
*                        introduced an automatic xMinusOne in acosh. Replaced  *
*                        fenv functions with __setflm.                         *
*      August   08 2001: replaced __setflm with FEGETENVD/FESETENVD.           *
*                        replaced DblInHex typedef with hexdouble.             *
*      Sept     19 2001: added macros to detect PowerPC and correct compiler.  *
*      Sept     19 2001: added <CoreServices/CoreServices.h> to get to <fp.h>  *
*                        and <fenv.h>, removed denormal comments.              *
*     October   08 2001: removed <CoreServices/CoreServices.h>.                *
*                        changed compiler errors to warnings.                  *
*     November  06 2001: commented out warning about Intel architectures.      *
*                                                                              *
*    W A R N I N G:                                                            *
*    These routines require a 64-bit double precision IEEE-754 model.          *
*    They are written for PowerPC only and are expecting the compiler          *
*    to generate the correct sequence of multiply-add fused instructions.      *
*                                                                              *
*    These routines are not intended for 32-bit Intel architectures.           *
*                                                                              *
*    A version of gcc higher than 932 is required.                            *
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

#pragma fenv_access on

/*******************************************************************************
*            Functions needed for the computation.                             *
*******************************************************************************/

/*     the following fp.h functions are used:                                 */
/*     __fpclassifyd, log1p, log, sqrt, copysign and __FABS;                  */

#define      INVERSE_HYPERBOLIC_NAN            "40"

static hexdouble SqrtNegEps  = HEXDOUBLE(0x3e400000, 0x00000000); /* = 7.4505805969238281e-09   */
static hexdouble Log2        = HEXDOUBLE(0x3FE62E42, 0xFEFA39EF); /* = 6.9314718055994530942E-1 */
static hexdouble FiveFourth  = HEXDOUBLE(0x3FF40000, 0x00000000); /* = 1.250000000000000E+00    */

/*******************************************************************************
*                  A      R      C      T      A      N      H                 *
*******************************************************************************/
#ifdef notdef
double atanh ( double x )
      {
      register double PositiveX;
      hexdouble OldEnvironment, NewEnvironment;

      FEGETENVD ( OldEnvironment.d );
      FESETENVD ( 0.0 );
      PositiveX          = __FABS ( x );
      
/*******************************************************************************
*                                                                              *
*     The expression for computing ArcTanh(x) is:                              *
*                                                                              *
*            ArcTanh(x) = 1/2log[(1+x)/(1-x)],      |x| < 1.                   *
*                                                                              *
*     We use the more accurate log(1+x) for the evaluation, then the ArcTanh   *
*     representation is:                                                       *
*                                                                              *
*           ArcTanh(x) = 1/2log1p(2x/(1-x)),      |x| < 1,                     *
*                                                                              *
*     and for small enough x ( |x| < SqrtNegEps, where 1 - SqrtNegEps^2 = 1 )  *
*     the first term of the taylor series expansion of log[(1+x)/(1-x)] is     *
*     2x/(1-x) ~= 2x.  x is returned for ArcTanh(x).                           *
*                                                                              *
*     the value of SqrtNegEps is:                                              *
*                                                                              *
*     SqrtNegEps =  0x3e40000000000000, then 1-SqrtNegEps^2 = 1.               *
*                                                                              *
*     The monotonicity of the function has been preserved across double        *
*     valued intervals.                                                        *
*                                                                              *
*     -inf    -1          -SqrtNegEps     0    +SqrtNegEps          +1   +inf  *
*     ---------+--------------------+-----+-----+--------------------+-------  *
*     < N a N >|-inf <= ArcTanh(x) >|< x  0  x >|< ArcTanh(x) => +inf|< N a N >*
*                                                                              *
*******************************************************************************/
      if ( PositiveX <= 1.0 )
            {
            if ( PositiveX >= SqrtNegEps.d )
                  {
                  PositiveX = 0.5 * log1p ( 2.0 * PositiveX / ( 1 - PositiveX ) );
                  x =  copysign ( PositiveX, x ); 
                  }
            }
/*******************************************************************************
*      If argument is SNaN then a QNaN has to be returned and the invalid      *
*      flag signaled for SNaN.  Otherwise, the argument is less than 1.0 and   *
*      a hyperbolic nan is returned.                                           * 
*******************************************************************************/
      else 
            {

            if ( x != x )
                  x *= 2.0;
            else 
                  {
                  x = nan ( INVERSE_HYPERBOLIC_NAN );
                  OldEnvironment.i.lo |= SET_INVALID;
                  }
            }
      
      switch ( __fpclassifyd ( x ) )
            {
            case FP_SUBNORMAL:
                  OldEnvironment.i.lo |= FE_UNDERFLOW;
                  /* FALL THROUGH */
            case FP_NORMAL:
                  OldEnvironment.i.lo |= FE_INEXACT;
                  /* FALL THROUGH */
            default:
                  break;
            }

      FEGETENVD ( NewEnvironment.d );
      OldEnvironment.i.lo |= NewEnvironment.i.lo;
      FESETENVD ( OldEnvironment.d );
      return x;
      }
#else

static const double kMinNormal = 2.2250738585072014e-308;  // 0x1.0p-1022
static hexdouble InvSqrtNegEps  = HEXDOUBLE(0x41c00000, 0x00000000); 
static hexdouble Huge       = HEXDOUBLE(0x7ff00000, 0x00000000);

double atanh ( double x )
{
      register double PositiveX;
      hexdouble OldEnvironment;

      register double result, FPR_env, FPR_z, FPR_half, FPR_ln2, FPR_one, FPR_two, 
                      FPR_sqreps, FPR_kMinNormal, FPR_inf;
      
      PositiveX = __FABS ( x );			FPR_ln2 = Log2.d;
      FPR_z = 0.0;				FPR_one = 1.0;
      FPR_half = 0.5;				FPR_two = 2.0;
      FPR_sqreps = SqrtNegEps.d;		FPR_kMinNormal = kMinNormal;
      FPR_ln2 = Log2.d;				FPR_inf = Huge.d;
      
      FEGETENVD ( FPR_env );
      __ENSURE( FPR_z, FPR_kMinNormal, FPR_sqreps );	__ENSURE( FPR_half, FPR_two, FPR_one );
      __ENSURE( FPR_z, FPR_inf, FPR_ln2 );

      FESETENVD ( FPR_z );
      
/*******************************************************************************
*                                                                              *
*     The expression for computing ArcTanh(x) is:                              *
*                                                                              *
*            ArcTanh(x) = 1/2log[(1+x)/(1-x)],      |x| < 1.                   *
*                                                                              *
*     We use the more accurate log(1+x) for the evaluation, then the ArcTanh   *
*     representation is:                                                       *
*                                                                              *
*           ArcTanh(x) = 1/2log1p(2x/(1-x)),      |x| < 1,                     *
*                                                                              *
*     and for small enough x ( |x| < SqrtNegEps, where 1 - SqrtNegEps^2 = 1 )  *
*     the first term of the taylor series expansion of log[(1+x)/(1-x)] is     *
*     2x/(1-x) ~= 2x.  x is returned for ArcTanh(x).                           *
*                                                                              *
*     the value of SqrtNegEps is:                                              *
*                                                                              *
*     SqrtNegEps =  0x3e40000000000000, then 1-SqrtNegEps^2 = 1.               *
*                                                                              *
*     The monotonicity of the function has been preserved across double        *
*     valued intervals.                                                        *
*                                                                              *
*     -inf    -1          -SqrtNegEps     0    +SqrtNegEps          +1   +inf  *
*     ---------+--------------------+-----+-----+--------------------+-------  *
*     < N a N >|-inf <= ArcTanh(x) >|< x  0  x >|< ArcTanh(x) => +inf|< N a N >*
*                                                                              *
*******************************************************************************/
      if ( PositiveX < FPR_one )
      {
            if ( PositiveX >= FPR_sqreps )
            {
                  result = FPR_half * log1p ( FPR_two * PositiveX / ( FPR_one - PositiveX ) );
                  if ( x < FPR_z )
                    result = -result;
            }
            else
                  result = x;
                  
            FESETENVD ( FPR_env );
            if ( result == FPR_z )
            {
                /* NOTHING */
            }
            else if ( __FABS( result ) < FPR_kMinNormal )
                  __PROG_UF_INEXACT( FPR_kMinNormal );
            else
                  __PROG_INEXACT( FPR_ln2 );
      }
      else if ( PositiveX == FPR_one )
      {
            OldEnvironment.d = FPR_env;
            result = (x > FPR_z) ? FPR_inf : -FPR_inf;
            OldEnvironment.i.lo |= FE_DIVBYZERO;
            FESETENVD ( OldEnvironment.d );
      }
/*******************************************************************************
*      If argument is SNaN then a QNaN has to be returned and the invalid      *
*      flag signaled for SNaN.  Otherwise, the argument is greater than 1.0 and*
*      a hyperbolic nan is returned.                                           * 
*******************************************************************************/
      else if ( x != x )
      {
            FESETENVD ( FPR_env );
            result = x + x;
      }
      else 
      {
            OldEnvironment.d = FPR_env;
            result = nan ( INVERSE_HYPERBOLIC_NAN );
            OldEnvironment.i.lo |= SET_INVALID;
            FESETENVD ( OldEnvironment.d );
      }
    
      return result;
}
#endif
#ifdef notdef
float atanhf( float x )
{
    return (float)atanh( x );
}
#endif

/*******************************************************************************
*                  A      R      C      S      I      N      H                 *
*******************************************************************************/
#ifdef notdef
double asinh ( double x )
      {
      register double PositiveX, InvPositiveX;
      hexdouble OldEnvironment, NewEnvironment;

      FEGETENVD ( OldEnvironment.d );
      FESETENVD ( 0.0 );
      PositiveX          = __FABS ( x );
      
/*******************************************************************************
*                                                                              *
*     The expression for computing ArcSinh(x) is:                              *
*                                                                              *
*           ArcSinh(x) = log(x+sqrt(x^2+1)).                                   *
*                                                                              *
*     SqrtNegEps =  0x3e40000000000000, then 1-SqrtNegEps^2 = 1.               *
*                                                                              *
*     Asymtotic behaviors, ( exact with respect to machine arithmetic )        *
*     are filtered out and computed seperately to avoid spurious over and      *
*     underflows.                                                              *
*                                                                              *
*-inf -1/sqrtNegEps -4/3  -sqrtNegEps  0  +sqrtNegEps +4/3  +1/sqrtNegEps  +inf*
*------------+--------+--------+-------+-------+--------+----------+-----------*
*<-log(2|x|)>|<  ArcSinh(x)   >|<     (x)     >|<    ArcSinh(x)   >|< log(2x) >*
*                                                                              *
*     The constant Log2 can be obtained using the 68040 instruction            *
*                                                                              *
*      FMOVECR.X  #$30, FP0 ; -=> FP0 = log(2) = 0x3FE62E42FEFA39EF (in double)*
*                           ;                  = 6.9314718055994530940e-01     *
*                                                                              *
*******************************************************************************/
      
      if ( PositiveX < SqrtNegEps.d )
            ;
      else if ( PositiveX <= 4.0 / 3.0 )
            {
            InvPositiveX = 1.0 / PositiveX;
            PositiveX = log1p ( PositiveX + PositiveX / ( InvPositiveX + 
                        sqrt ( 1.0 + InvPositiveX * InvPositiveX ) ) );
            }
      else if ( PositiveX <= 1.0 / SqrtNegEps.d )
            PositiveX = log ( 2.0 * PositiveX + 1.0 / ( PositiveX + 
                        sqrt ( 1.0 + PositiveX *  PositiveX ) ) );
      else if ( x != x )
            x *= 2.0;
      else
            PositiveX = Log2.d + log ( PositiveX );
      
      switch ( __fpclassifyd ( PositiveX ) )
            {
            case FP_SUBNORMAL:
                  OldEnvironment.i.lo |= FE_UNDERFLOW;
                  /* FALL THROUGH */
            case FP_NORMAL:
                  OldEnvironment.i.lo |= FE_INEXACT;
                  /* FALL THROUGH */
            default:
                  break;
            }

      FEGETENVD ( NewEnvironment.d );
      OldEnvironment.i.lo |= NewEnvironment.i.lo;
      FESETENVD ( OldEnvironment.d );
      return ( copysign ( PositiveX, x ) );
    }
#else

double asinh ( double x )
{
      register double PositiveX, InvPositiveX;

      register double result, FPR_env, FPR_z, FPR_sqreps, FPR_4div3, FPR_inf,
                      FPR_one, FPR_two, FPR_ln2, FPR_invsqreps, FPR_kMinNormal;
      
      PositiveX = __FABS ( x );			FPR_inf = Huge.d;
      FPR_z = 0.0;				FPR_sqreps = SqrtNegEps.d;
      FPR_4div3 = 1.333333333333333333333;	FPR_one = 1.0;
      FPR_two = 2.0;				FPR_kMinNormal = kMinNormal;
      FPR_ln2 = Log2.d;				FPR_invsqreps = InvSqrtNegEps.d;
      
      FEGETENVD ( FPR_env );
      __ENSURE( FPR_z, FPR_kMinNormal, FPR_sqreps );
      __ENSURE( FPR_4div3, FPR_two, FPR_one );
      __ENSURE( FPR_ln2, FPR_inf, FPR_invsqreps );
      FESETENVD ( FPR_z );
      
/*******************************************************************************
*                                                                              *
*     The expression for computing ArcSinh(x) is:                              *
*                                                                              *
*           ArcSinh(x) = log(x+sqrt(x^2+1)).                                   *
*                                                                              *
*     SqrtNegEps =  0x3e40000000000000, then 1-SqrtNegEps^2 = 1.               *
*                                                                              *
*     Asymtotic behaviors, ( exact with respect to machine arithmetic )        *
*     are filtered out and computed seperately to avoid spurious over and      *
*     underflows.                                                              *
*                                                                              *
*-inf -1/sqrtNegEps -4/3  -sqrtNegEps  0  +sqrtNegEps +4/3  +1/sqrtNegEps  +inf*
*------------+--------+--------+-------+-------+--------+----------+-----------*
*<-log(2|x|)>|<  ArcSinh(x)   >|<     (x)     >|<    ArcSinh(x)   >|< log(2x) >*
*                                                                              *
*     The constant Log2 can be obtained using the 68040 instruction            *
*                                                                              *
*      FMOVECR.X  #$30, FP0 ; -=> FP0 = log(2) = 0x3FE62E42FEFA39EF (in double)*
*                           ;                  = 6.9314718055994530940e-01     *
*                                                                              *
*******************************************************************************/
      
      if ( x == FPR_z )
      {
            result = x;
            FESETENVD ( FPR_env );
      }
      else if ( PositiveX < FPR_kMinNormal )
      {
            result = x;
            FESETENVD ( FPR_env );
            __PROG_UF_INEXACT( FPR_kMinNormal );
      }
      else if ( PositiveX < FPR_sqreps )
      {
            result = x;
            FESETENVD ( FPR_env );
            __PROG_INEXACT( FPR_ln2 );
      }
      else if ( PositiveX <= FPR_4div3 )
      {
            InvPositiveX = FPR_one / PositiveX;
            result = log1p ( PositiveX + PositiveX / ( InvPositiveX + 
                        sqrt ( FPR_one + InvPositiveX * InvPositiveX ) ) );
            if ( x < FPR_z ) 
                result = -result;
            FESETENVD ( FPR_env );
            __PROG_INEXACT( FPR_ln2 );
      }
      else if ( PositiveX <= FPR_invsqreps )
      {
            result = log ( FPR_two * PositiveX + FPR_one / ( PositiveX + 
                        sqrt ( FPR_one + PositiveX *  PositiveX ) ) );
            if ( x < FPR_z ) 
                result = -result;
            FESETENVD ( FPR_env );
            __PROG_INEXACT( FPR_ln2 );
      }
      else if ( PositiveX < FPR_inf )
      {
            result = FPR_ln2 + log ( PositiveX );
            if ( x < FPR_z ) 
                result = -result;
            FESETENVD ( FPR_env );
            __PROG_INEXACT( FPR_ln2 );
      }
      else if ( x != x )
      {
            FESETENVD ( FPR_env );
            result = x + x;
      }
      else
      {
            if ( x < FPR_z ) 
                result = -FPR_inf;
            else
                result = FPR_inf;
            FESETENVD ( FPR_env );
      }
      
      return result;
}
#endif

#ifdef notdef
float asinhf( float x )
{
    return (float)asinh( x );
}
#endif

/*******************************************************************************
*                  A      R      C      C      O      S      H                 *
*******************************************************************************/
#ifdef notdef
double acosh ( double x )
      {
      register double xMinusOne;
      hexdouble OldEnvironment;
      
      FEGETENVD ( OldEnvironment.d );
      FESETENVD ( 0.0 );
      
      xMinusOne = x - 1.0;

/*******************************************************************************
*                                                                              *
*     The expression for computing ArcCosh(x) is:                              *
*                                                                              *
*           ArcCosh(x) = log(x+sqrt(x^2-1)), for x ≥ 1.                        *
*                                                                              *
*     SqrtNegEps =  0x3e40000000000000, then 1-SqrtNegEps^2 = 1.               *
*                                                                              *
*     (1) if x is in [1, 5/4] then we would like to use the more accurate      *
*     log1p.  Make the change of variable x => x-1 to handle operations on a   *
*     lower binade.                                                            *
*                                                                              *
*     (2) If x is in a regular range (5/4,1/sqrteps], then multiply            *
*     x+sqrt(x^2-1) by sqrt(x^2-1)/sqrt(x^2-1) and simplify to get             *
*     2x-1/(x+sqrt(x^2-1).  This operation will increase the accuracy of the   *
*     computation.                                                             *
*                                                                              *
*     (3) For large x, such that x^2 - 1 = x^2, ArcCosh(x) = log(2x).          *
*     This asymtotical behavior ( exact with respect to machine arithmetic,    *
*     is filtered out and computed seperately to avoid spurious overflows.     * 
*                                                                              *
*     -inf    +1                   +5/4              +1/sqrtNegEps       +inf  *
*     ---------+---------------------+---------------------+---------------    *
*     < N a N >|<  (1)  ArcCosh(x)  >|<  (2)  ArcCosh(x)  >|< (3) log(2x) >    *
*                                                                              *
*******************************************************************************/
      
      if ( x >= 1.0 )
            {
            if ( x <= FiveFourth.d )
                  x = log1p ( xMinusOne + sqrt ( 2.0 * xMinusOne + 
                                                 xMinusOne * xMinusOne ) );
            else if ( ( FiveFourth.d < x ) && ( x <= 1.0 / SqrtNegEps.d ) )
                  x = log ( 2 * x - 1.0 / ( x + sqrt ( x * x - 1.0 ) ) );
            else
                  x = Log2.d + log ( x );
            }
/*******************************************************************************
*      If argument is SNaN then a QNaN has to be returned and the invalid      *
*      flag signaled for SNaN.  Otherwise, the argument is less than 1.0 and   *
*      a hyperbolic nan is returned.                                           * 
*******************************************************************************/

      else
            {
            if ( x != x )              /* check for argument = NaN confirmed. */
                  x *= 2.0;
            else 
                  {
                  OldEnvironment.i.lo |= SET_INVALID;
                  x = nan ( INVERSE_HYPERBOLIC_NAN );
                  }
            }

      switch ( __fpclassifyd ( x ) )
            {
            case FP_SUBNORMAL:
                  OldEnvironment.i.lo |= FE_UNDERFLOW;
                  /* FALL THROUGH */
            case FP_NORMAL:
                  OldEnvironment.i.lo |= FE_INEXACT;
                  /* FALL THROUGH */
            default:
                  break;
            }

      FESETENVD ( OldEnvironment.d );

      return x;
      }
#else
double acosh ( double x )
{
      register double xMinusOne;
      hexdouble OldEnvironment;
      
      register double result, FPR_env, FPR_z, FPR_ln2, FPR_one, FPR_5div4, 
                      FPR_two, FPR_invsqreps, FPR_inf;
      
      FPR_z = 0.0;				FPR_one = 1.0;
      FPR_5div4 = FiveFourth.d;			FPR_two = 2.0;
      FPR_ln2 = Log2.d;				FPR_invsqreps = InvSqrtNegEps.d;
      FPR_inf = Huge.d;
      
      FEGETENVD ( FPR_env );
      __ENSURE( FPR_z, FPR_ln2, FPR_invsqreps );	__ENSURE( FPR_z, FPR_inf, FPR_z );
      __ENSURE( FPR_5div4, FPR_two, FPR_one );
      FESETENVD ( FPR_z );
      
      xMinusOne = x - FPR_one;

/*******************************************************************************
*                                                                              *
*     The expression for computing ArcCosh(x) is:                              *
*                                                                              *
*           ArcCosh(x) = log(x+sqrt(x^2-1)), for x ≥ 1.                        *
*                                                                              *
*     SqrtNegEps =  0x3e40000000000000, then 1-SqrtNegEps^2 = 1.               *
*                                                                              *
*     (1) if x is in [1, 5/4] then we would like to use the more accurate      *
*     log1p.  Make the change of variable x => x-1 to handle operations on a   *
*     lower binade.                                                            *
*                                                                              *
*     (2) If x is in a regular range (5/4,1/sqrteps], then multiply            *
*     x+sqrt(x^2-1) by sqrt(x^2-1)/sqrt(x^2-1) and simplify to get             *
*     2x-1/(x+sqrt(x^2-1).  This operation will increase the accuracy of the   *
*     computation.                                                             *
*                                                                              *
*     (3) For large x, such that x^2 - 1 = x^2, ArcCosh(x) = log(2x).          *
*     This asymtotical behavior ( exact with respect to machine arithmetic,    *
*     is filtered out and computed seperately to avoid spurious overflows.     * 
*                                                                              *
*     -inf    +1                   +5/4              +1/sqrtNegEps       +inf  *
*     ---------+---------------------+---------------------+---------------    *
*     < N a N >|<  (1)  ArcCosh(x)  >|<  (2)  ArcCosh(x)  >|< (3) log(2x) >    *
*                                                                              *
*******************************************************************************/
      
      if ( x >= FPR_one )
      {
            if ( x == FPR_one )
            {
                  FESETENVD ( FPR_env );
                  result = FPR_z;
            }
            else if ( x <= FPR_5div4 )
            {
                  result = log1p ( xMinusOne + sqrt ( FPR_two * xMinusOne + 
                                                 xMinusOne * xMinusOne ) );
                  FESETENVD ( FPR_env );
                  __PROG_INEXACT( FPR_ln2 );
            }
            else if ( ( FPR_5div4 < x ) && ( x <= FPR_invsqreps ) )
            {
                  result = log ( FPR_two * x - FPR_one / ( x + sqrt ( x * x - FPR_one ) ) );
                  FESETENVD ( FPR_env );
                  __PROG_INEXACT( FPR_ln2 );
            }
            else if ( x < FPR_inf )
            {
                  result = FPR_ln2 + log ( x );
                  FESETENVD ( FPR_env );
                  __PROG_INEXACT( FPR_ln2 );
            }
            else
            {
                result = FPR_inf;
                FESETENVD ( FPR_env );
            }
      }
/*******************************************************************************
*      If argument is SNaN then a QNaN has to be returned and the invalid      *
*      flag signaled for SNaN.  Otherwise, the argument is less than 1.0 and   *
*      a hyperbolic nan is returned.                                           * 
*******************************************************************************/

      else
      {
            if ( x != x )              /* check for argument = NaN confirmed. */
            {
                  FESETENVD ( FPR_env );
                  result = x + x;
            }
            else 
            {
                OldEnvironment.d = FPR_env;
                result = nan ( INVERSE_HYPERBOLIC_NAN );
                OldEnvironment.i.lo |= SET_INVALID;
                FESETENVD ( OldEnvironment.d );
            }
      }

      return result;
}
#endif
#ifdef notdef
float acoshf( float x )
{
    return (float)acosh( x );
}
#endif

#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
