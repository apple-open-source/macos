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
*     File logb.c,                                                             *
*     Functions logb.                                                          *
*     Implementation of IEEE-754 logb for the PowerPC platforms.               *
*                                                                              *
*     Copyright © 1991-2001 Apple Computer, Inc.  All rights reserved.         *
*                                                                              *
*     Written by A. Sazegari, started on June 1991.                            *
*     Modified and ported by Robert A. Murley (ram) for Mac OS X.              *
*                                                                              *
*     A MathLib v4 file.                                                       *
*                                                                              *
*     Change History (most recent last):                                       *
*                                                                              *
*     August    26 1991: removed CFront Version 1.1d17 warnings.               *
*     August    27 1991: no errors reported by the test suite.                 *
*     November  11 1991: changed CLASSEXTENDED to the macro CLASSIFY and       *
*                        + or - infinity to constants.                         *
*     November  18 1991: changed the macro CLASSIFY to CLASSEXTENDEDint to     *
*                        improve performance.                                  *
*     February  07 1992: changed bit operations to macros (  object size is    *
*                        unchanged  ).                                         *
*     September 24 1992: took the "#include support.h" out.                    *
*     December  03 1992: first rs/6000 port.                                   *
*     August    30 1992: set the divide by zero for the zero argument case.    *
*     October   05 1993: corrected the environment.                            *
*     October   17 1994: replaced all environmental functions with __setflm.   *
*     May       28 1997: made speed improvements.                              *
*     April     30 2001: first mac os x port using gcc.                        *
*     July      16 2001: replaced __setflm with FEGETENVD/FESETENVD.           *
*     August    28 2001: added description of logb function.                   *
*     September 06 2001: added #if __ppc__.                                    *
*     September 09 2001: added more comments.                                  *
*     September 10 2001: added macros to detect PowerPC and correct compiler.  *
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
*      GCC compiler options:                                                   *
*            optimization level 3 (-O3)                                        *
*            -fschedule-insns -finline-functions -funroll-all-loops            *
*                                                                              *
********************************************************************************
*     The C math library offers a similar function called "frexp".  It is      *
*     different in details from logb, but similar in spirit.  This current     *
*     implementation of logb follows the recommendation in IEEE Standard 854   *
*     which is different in its handling of denormalized numbers from the IEEE *
*     Standard 754.                                                            *
*******************************************************************************/

#ifdef      __APPLE_CC__
#if         __APPLE_CC__ > 930

#include      "math.h"
#include      "fenv.h"
#include      "fp_private.h"
#include      "fenv_private.h"

static const double twoTo52 = 4.50359962737049600e15;              // 0x1p52
static const double klTod = 4503601774854144.0;                    // 0x1.000008p52
static const hexdouble minusInf  = HEXDOUBLE(0xfff00000, 0x00000000);

static const double twoTo23 = 8388608.0e0;              	   // 0x1p23
static const hexsingle minusInff  = { 0xff800000 };

#define INT_MAX	(2147483647)

/*******************************************************************************
*                                    L  O  G  B                                *
********************************************************************************
*                                                                              *
*     logb extracts the exponent of its argument, as a signed integral         *
*     value. A subnormal argument is treated as though it were first           *
*     normalized. Thus                                                         *
*            1 <= x * 2^( - Logb ( x ) ) < 2                                   *
*******************************************************************************/

double logb (  double x  )
{
      hexdouble xInHex;
      long int shiftedExp;
      
      xInHex.d = x;
      shiftedExp = ( xInHex.i.hi & 0x7ff00000 ) >> 20;
      
      if ( shiftedExp == 2047 ) 
      {                                                  // NaN or INF
            if ( ( ( xInHex.i.hi & 0x80000000 ) == 0 ) || ( x != x ) )
                  return x;                              // NaN or +INF return x
            else
                  return -x;                             // -INF returns +INF
      }
      
      if ( shiftedExp != 0 )                             // normal number
            shiftedExp -= 1023;                          // unbias exponent
      else if ( x == 0.0 ) 
      {                                                  // zero
            hexdouble OldEnvironment;
            FEGETENVD( OldEnvironment.d );             // raise zero divide for DOMAIN error
            OldEnvironment.i.lo |= FE_DIVBYZERO;
            FESETENVD( OldEnvironment.d );
            return ( minusInf.d );			 // return -infinity
      }
      else 
      {                                                  // subnormal number
            xInHex.d *= twoTo52;                         // scale up
            shiftedExp = ( xInHex.i.hi & 0x7ff00000 ) >> 20;
            shiftedExp -= 1075;                          // unbias exponent
      }
      
      if ( shiftedExp == 0 )                             // zero result
            return ( 0.0 );
      else 
      {                                                  // nonzero result
            xInHex.d = klTod;
            xInHex.i.lo += shiftedExp;
            return ( xInHex.d - klTod );
      }
}

int ilogb (  double x  )
{
      hexdouble xInHex;
      long int shiftedExp;
      
      xInHex.d = x;
      shiftedExp = ( xInHex.i.hi & 0x7ff00000 ) >> 20;
      
      if ( shiftedExp == 2047 ) 
      {                                                  // NaN or INF
            if (x != x)
                return FP_ILOGBNAN;
            else
                return INT_MAX;
      }
      
      if ( shiftedExp != 0 )                             // normal number
            shiftedExp -= 1023;                          // unbias exponent
      else if ( x == 0.0 ) 
      {                                                  // zero
            return FP_ILOGB0;			 	 // return -infinity
      }
      else 
      {                                                  // subnormal number
            xInHex.d *= twoTo52;                         // scale up
            shiftedExp = ( xInHex.i.hi & 0x7ff00000 ) >> 20;
            shiftedExp -= 1075;                          // unbias exponent
      }
      
      return shiftedExp;
}

float logbf (  float x  )
{
      hexsingle xInHex;
      long int shiftedExp;
      
      xInHex.fval = x;
      shiftedExp = ( xInHex.lval & 0x7f800000 ) >> 23;
      
      if ( shiftedExp == 255 ) 
      {                                                  // NaN or INF
            if ( ( ( xInHex.lval & 0x80000000 ) == 0 ) || ( x != x ) )
                  return x;                              // NaN or +INF return x
            else
                  return -x;                             // -INF returns +INF
      }
      
      if ( shiftedExp != 0 )                             // normal number
            shiftedExp -= 127;                           // unbias exponent
      else if ( x == 0.0 ) 
      {                                                  // zero
            hexdouble OldEnvironment;
            FEGETENVD( OldEnvironment.d );             // raise zero divide for DOMAIN error
            OldEnvironment.i.lo |= FE_DIVBYZERO;
            FESETENVD( OldEnvironment.d );
            return ( minusInff.fval );			 // return -infinity
      }
      else 
      {                                                  // subnormal number
            xInHex.fval *= twoTo23;                      // scale up
            shiftedExp = ( xInHex.lval & 0x7f800000 ) >> 23;
            shiftedExp -= 150;                          // unbias exponent
      }
      
      return (float)shiftedExp;
}

int ilogbf (  float x  )
{
      hexsingle xInHex;
      long int shiftedExp;
      
      xInHex.fval = x;
      shiftedExp = ( xInHex.lval & 0x7f800000 ) >> 23;
      
      if ( shiftedExp == 255 ) 
      {                                                  // NaN or INF
            if (x != x)
                return FP_ILOGBNAN;
            else
                return INT_MAX;
      }
      
      if ( shiftedExp != 0 )                             // normal number
            shiftedExp -= 127;                           // unbias exponent
      else if ( x == 0.0 ) 
      {                                                  // zero
            return FP_ILOGB0;			 	 // return -infinity
      }
      else 
      {                                                  // subnormal number
            xInHex.fval *= twoTo23;                      // scale up
            shiftedExp = ( xInHex.lval & 0x7f800000 ) >> 23;
            shiftedExp -= 150;                          // unbias exponent
      }
      
      return shiftedExp;
}

#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
