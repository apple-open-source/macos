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
*    FastSinCos.c                                                              *
*                                                                              *
*    Double precision Sine and Cosine.                                         *
*                                                                              *
*    Copyright © 1997-2001 by Apple Computer, Inc. All rights reserved.        *
*                                                                              *
*    Written by A. Sazegari, started on June 1997.                             *
*    Modified and ported by Robert A. Murley (ram) for Mac OS X.               *
*                                                                              *
*    A MathLib v4 file.                                                        *
*                                                                              *
*    Based on the trigonometric functions from IBM/Taligent.                   *
*                                                                              *
*    November  06 2001: commented out warning about Intel architectures.       *
*    July      20 2001: replaced __setflm with fegetenvd/fesetenvd.            *
*                       replaced DblInHex typedef with hexdouble.              *
*    September 07 2001: added #ifdef __ppc__.                                  *
*    September 09 2001: added more comments.                                   *
*    September 10 2001: added macros to detect PowerPC and correct compiler.   *
*    September 18 2001: added <CoreServices/CoreServices.h> to get to <fp.h>   *
*                       and <fenv.h>, removed denormal comments.               *
*    October   08 2001: removed <CoreServices/CoreServices.h>.                 *
*                       changed compiler errors to warnings.                   *
*                                                                              *
*    These routines have a long double (107-bits) argument reduction to        *
*    better match their long double counterpart.                               *
*                                                                              *
*    W A R N I N G:                                                            *
*    These routines require a 64-bit double precision IEEE-754 model.          *
*    They are written for PowerPC only and are expecting the compiler          *
*    to generate the correct sequence of multiply-add fused instructions.      *
*                                                                              *
*    These routines are not intended for 32-bit Intel architectures.           *
*                                                                              *
*    A version of gcc higher than 932 is required.                             *
*                                                                              *
*    GCC compiler options:                                                     *
*          optimization level 3 (-O3)                                          *
*          -fschedule-insns -finline-functions -funroll-all-loops              *
*                                                                              *
*******************************************************************************/

#ifdef      __APPLE_CC__
#if         __APPLE_CC__ > 930

#include    "fenv_private.h"
#include    "fp_private.h"

#define     TRIG_NAN          "33"

/*******************************************************************************
*      Floating-point constants.                                               *
*******************************************************************************/

static const double kPiScale42 = 1.38168706094305449e13;   // 0x1.921fb54442d17p43
static const double kPiScale53 = 2.829695100811376e16;     // 0x1.921fb54442d18p54
static const double piOver4 = 0.785398163397448390;        // 0x1.921fb54442d19p-1
static const double piOver2 = 1.570796326794896619231322;  // 0x1.921fb54442d18p0
static const double piOver2Tail = 6.1232339957367660e-17;  // 0x1.1a62633145c07p-54
static const double twoOverPi = 0.636619772367581382;      // 0x1.45f306dc9c883p-1
//static const double k2ToM26 = 1.490116119384765625e-8;     // 0x1.0p-26;
static const double kMinNormal = 2.2250738585072014e-308;  // 0x1.0p-1022
static const double kRintBig = 2.7021597764222976e16;      // 0x1.8p54
static const double kRint = 6.755399441055744e15;          // 0x1.8p52
static const hexdouble infinity = HEXDOUBLE(0x7ff00000, 0x00000000);

/*******************************************************************************
*      Approximation coefficients.                                             *
*******************************************************************************/

static const double s13 = 1.5868926979889205164e-10;       // 1.0/13!
static const double s11 = -2.5050225177523807003e-8;       // -1.0/11!
static const double s9 =  2.7557309793219876880e-6;        // 1.0/9!
static const double s7 = -1.9841269816180999116e-4;        // -1.0/7!
static const double s5 =  8.3333333332992771264e-3;        // 1.0/5!
static const double s3 = -0.16666666666666463126;          // 1.0/3!
static const double c14 = -1.138218794258068723867e-11;    // -1.0/14!
static const double c12 = 2.087614008917893178252e-9;      // 1.0/12!
static const double c10 = -2.755731724204127572108e-7;     // -1.0/10!
static const double c8 =  2.480158729870839541888e-5;      // 1.0/8!
static const double c6 = -1.388888888888735934799e-3;      // -1.0/6!
static const double c4 =  4.166666666666666534980e-2;      // 1.0/4!
static const double c2 = -.5;                              // -1.0/2!


double sin ( double x ) 
      {
      register double absOfX, intquo, arg, argtail, xSquared, xThird, xFourth, temp1, temp2, result;
      register unsigned long int ltable;
      hexdouble      z, OldEnvironment;
      
      absOfX = __fabs ( x );
      
      fegetenvd( OldEnvironment.d );                     // save env, set default
      fesetenvd( 0.0 );
      
      if ( absOfX < piOver4 )       
            {                                                    // |x| < ¹/4
            
            if ( absOfX == 0.0 )
                    {
                    fesetenvd( OldEnvironment.d );       	// restore caller's mode
                    return x; 					// +0 -0 preserved
                    }
                    
/*******************************************************************************
*      at this point, x is normal with magnitude between 0 and ¹/4.            *
*******************************************************************************/

            xSquared = x * x;                     // sin polynomial approximation
            xFourth = xSquared * xSquared;
            OldEnvironment.i.lo |= FE_INEXACT;
            temp1 = s9 + s13*xFourth;
            temp2 = s7 + s11*xFourth;
            temp1 = s5 + temp1*xFourth;
            temp2 = s3 + temp2*xFourth;
            xThird = xSquared * x;
            temp1 = temp2 + xSquared * temp1;
            result = x + xThird * temp1;
            
            if ( fabs ( result ) < kMinNormal )
                    OldEnvironment.i.lo |= FE_UNDERFLOW;

            fesetenvd( OldEnvironment.d );        // restore caller's mode
            return ( result ) ;
            }
      
      if ( x != x )                               // x is a NaN
            {
            fesetenvd( OldEnvironment.d );        // restore caller's mode
            return ( x );
            }

/*******************************************************************************
*      x has magnitude > ¹/4.                                                  *
*******************************************************************************/

      if ( absOfX > kPiScale42 )  

/*******************************************************************************
*      |x| is huge or infinite.                                                *
*******************************************************************************/
            {
            if ( absOfX == infinity.d )  
                  {                               // infinite case is invalid
                  OldEnvironment.i.lo |= SET_INVALID;
                  fesetenvd( OldEnvironment.d );  // restore caller's mode
                  return ( nan ( TRIG_NAN ) );    // return NaN
                  }
            
            while ( absOfX > kPiScale53 )  
                  {                               // loop to reduce x below
                  intquo = x * twoOverPi;         // ¹*2^53 in magnitude
                  x = ( x - intquo * piOver2 )  - intquo * piOver2Tail;
                  absOfX = __fabs ( x ) ;
                  }

/*******************************************************************************
*     final reduction of x to magnitude between 0 and 2*¹.                     *
*******************************************************************************/
            intquo = ( x * twoOverPi + kRintBig)  - kRintBig;
            x = ( x - intquo * piOver2)  - intquo * piOver2Tail;
            absOfX = __fabs( x );
            }
      
/*******************************************************************************
*     |x| < pi*2^42: further reduction is probably necessary.  A double-double *
*     reduced argument is determined ( arg:argtail ) .  It is possible that x  *
*     has been reduced below pi/4 in magnitude, but x is definitely nonzero    *
*     and safely in the normal range.                                          *
*******************************************************************************/
      
      z.d = x * twoOverPi + kRint;              // find integer quotient of x/(¹/2) 
      intquo = z.d - kRint;
      arg = ( x - intquo * piOver2 )  - intquo * piOver2Tail;
      OldEnvironment.i.lo |= FE_INEXACT;        // force the setting the inexact
      xSquared = arg * arg;
      argtail = ( ( x - intquo * piOver2 )  - arg )  - intquo * piOver2Tail;
      xFourth = xSquared * xSquared;
      
/*******************************************************************************
*     multiple of ¹/2 ( mod 4)  determines approx used and sign of result.     *
*******************************************************************************/
      
      ltable = z.i.lo & FE_ALL_RND;
      
      if ( ltable & 0x1ul )  
            {                                    // argument closest to ±¹/2
/*******************************************************************************
*     use cosine approximation.                                                *
*******************************************************************************/
            temp1 = c10 + c14 * xFourth;
            temp2 = c8 + c12 * xFourth;
            temp1 = c6 + temp1 * xFourth;
            temp2 = c4 + temp2 * xFourth;
            temp1 = c2 + temp1 * xFourth;
            temp1 = temp1 + xSquared * temp2;
            temp1 = arg*temp1 - argtail;         // second-order correction
            if ( ltable < 2 )                    // adjust sign of result
                  result = 1.0 + arg * temp1;    // positive
            else 
                  {
                  arg = - arg;
                  result = arg * temp1 - 1.0;    // negative
                  }
            }
      
      else 
            {
/*******************************************************************************
*     use sine approximation.                                                  *
*******************************************************************************/
            temp1 = s9 + s13 * xFourth;
            temp2 = s7 + s11 * xFourth;
            temp1 = s5 + temp1 * xFourth;
            temp2 = s3 + temp2 * xFourth;
            xThird = xSquared * arg;
            temp1 = temp2 + xSquared * temp1;
            temp1 = temp1 * xThird + argtail;    // second-order correction
            if ( ltable < 2 )                    // adjust sign of final result
                  result = arg + temp1 ;         // positive
            else 
                  {
                  arg = - arg;
                  result = arg - temp1;          // negative
                  }
            }
      
      fesetenvd( OldEnvironment.d );             // restore caller's mode
      return ( result ) ;
      }
      
#ifdef notdef
float sinf( float x)
{
    return (float)sin( x );
}
#endif
/*******************************************************************************
*     Cosine section.                                                          *
*******************************************************************************/

double cos ( double x ) 
      {
      register double absOfX, intquo, arg, argtail, xSquared, xThird, xFourth,
                      temp1, temp2, result;
      register unsigned long int iquad;
      hexdouble z, OldEnvironment;
      
      absOfX = __fabs( x );
      
      fegetenvd( OldEnvironment.d );                // save env, set default
      fesetenvd( 0.0 );
      
      if ( absOfX < piOver4 )       
            {                                       // |x| < pi/4
            if ( absOfX == 0.0 )
                    {
                    fesetenvd( OldEnvironment.d );       	// restore caller's mode
                    return 1.0; 				
                    }
                    
            xSquared = x * x;                     // cos polynomial approximation
            xFourth = xSquared * xSquared;
            temp1 = c10 + c14 * xFourth;
            temp2 = c8 + c12 * xFourth;
            temp1 = c6 + temp1 * xFourth;
            temp2 = c4 + temp2 * xFourth;
            temp1 = c2 + temp1 * xFourth;
            OldEnvironment.i.lo |= FE_INEXACT;
            temp2 = temp1 + xSquared * temp2;
            result = 1.0 + xSquared * temp2;
            fesetenvd( OldEnvironment.d );        // restore caller's mode
            return ( result );
            }
      
      if ( x != x )                               // x is a NaN
            {
            fesetenvd( OldEnvironment.d );        // restore caller's mode
            return ( x );
            }

/*******************************************************************************
*      x has magnitude > ¹/4.                                                  *
*******************************************************************************/
      if ( absOfX > kPiScale42 )  

/*******************************************************************************
*      |x| is huge or infinite.                                                *
*******************************************************************************/
            {
            if ( absOfX == infinity.d )  
                  {                               // infinite case is invalid
                  OldEnvironment.i.lo |= SET_INVALID;
                  fesetenvd( OldEnvironment.d );  // restore caller's mode
                  return ( nan ( TRIG_NAN ) );    // return NaN
                  }
            
            while ( absOfX > kPiScale53 )  
                  {                               // loop to reduce x below
                  intquo = x * twoOverPi;         // ¹*2^53 in magnitude
                  x = ( x - intquo * piOver2)  - intquo * piOver2Tail;
                  absOfX = __fabs( x );
                  }
            
/*******************************************************************************
*     final reduction of x to magnitude between 0 and 2*¹.                     *
*******************************************************************************/
            intquo = ( x * twoOverPi + kRintBig)  - kRintBig;
            x = ( x - intquo * piOver2)  - intquo * piOver2Tail;
            absOfX = __fabs ( x );
            }           
      
/*******************************************************************************
*     |x| < pi*2^42: further reduction is probably necessary.  A double-double *
*     reduced argument is determined ( arg:argtail ) .  It is possible that x  *
*     has been reduced below pi/4 in magnitude, but x is definitely nonzero    *
*     and safely in the normal range.                                          *
*******************************************************************************/

      z.d = x*twoOverPi + kRint;                   // find integer quotient of x/(¹/2) 
      OldEnvironment.i.lo |= FE_INEXACT;           // inexact is justified
      iquad = ( z.i.lo + 1 ) & FE_ALL_RND;         // iquad = int multiple mod 4
      intquo = z.d - kRint;
      arg = ( x - intquo * piOver2 )  - intquo * piOver2Tail;
      xSquared = arg*arg;
      argtail = ( ( x - intquo * piOver2)  - arg)  - intquo * piOver2Tail;
      xFourth = xSquared * xSquared;
      
/*******************************************************************************
*     multiple of ¹/2 ( mod 4)  determines approx used and sign of result.     *
*******************************************************************************/

      if ( iquad & 0x1UL)  
            {                                      // arg closest to 0 or ¹
/*******************************************************************************
*     use cosine approximation.                                                *
*******************************************************************************/
            temp1 = c10 + c14 * xFourth;
            temp2 = c8 + c12 * xFourth;
            temp1 = c6 + temp1 * xFourth;
            temp2 = c4 + temp2 * xFourth;
            temp1 = c2 + temp1 * xFourth;
            temp1 = temp1 + xSquared * temp2;
            temp1 = arg * temp1 - argtail;         // second-order correction
            if ( iquad < 2 )                       // adjust sign of result
                  result = 1.0 + arg * temp1;
            else 
                  {
                  arg = - arg;
                  result = arg * temp1 - 1.0;
                  }
            }
      
      else 
            {
/*******************************************************************************
*     use sine approximation.                                                  *
*******************************************************************************/
            temp1 = s9 + s13   * xFourth;
            temp2 = s7 + s11   * xFourth;
            temp1 = s5 + temp1 * xFourth;
            temp2 = s3 + temp2 * xFourth;
            xThird = xSquared * arg;
            temp1 = temp2 + xSquared * temp1;
            temp1 = temp1 * xThird + argtail;     // second-order correction
            if ( iquad < 2 )                      // adjust sign of result
                  result = temp1 + arg;
            else 
                  {
                  arg = - arg;
                  result = arg - temp1;
                  }
            }

      fesetenvd( OldEnvironment.d );              // restore caller's mode
      return ( result ) ;
      }

#ifdef notdef
float cosf( float x)
{
    return (float)cos( x );
}
#endif

#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
