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
*     File tableExpD.c,                                                        *
*     Functions exp, exp2 and expm1.                                           *
*     Implementation of exp(x) based on IBM/Taligent table method.             *
*                                                                              *
*     Copyright © 1997-2001 Apple Computer, Inc.  All rights reserved.         *
*                                                                              *
*     Written by Ali Sazegari, started on April 1997.                          *
*     Modified and ported by Robert A. Murley (ram) for Mac OS X.              *
*                                                                              *
*     A MathLib v4 file.                                                       *
*                                                                              *
*     November 07 2001: removed exp2 to prevent conflict with CarbonCore.      *
*     November 06 2001: commented out warning about Intel architectures.       *
*                       changed i386 stub to call abort().                     *
*     November 02 2001: added stub for i386 version of exp2.                   *
*     April    28 1997: port of the ibm/taligent exp routines.                 *
*     July     16 1997: changed the rounding direction sensitivity of          *
*                       delivered results to default rounding at all times.    *
*     August   28 2001: replaced __setflm with fegetenvd/fesetenvd.            *
*                       replaced DblInHex typedef with hexdouble.              *
*                       used standard exception symbols from fenv.h.           *
*                       added #ifdef __ppc__.                                  *
*     September 09 2001: added more comments.                                  *
*     September 10 2001: added macros to detect PowerPC and correct compiler.  *
*     September 18 2001: added <CoreServices/CoreServices.h> to get <fenv.h>.  *
*     October   08 2001: removed <CoreServices/CoreServices.h>.                *
*                        changed compiler errors to warnings.                  *
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

#include      "math.h"
#include      "fenv_private.h"
#include      "fp_private.h"

/*******************************************************************************
*      Floating-point constants.                                               *
*******************************************************************************/

static const double rintFactor    = 6.7553994410557440000e+15;  /* 0x43380000, 0x00000000 */
static const double oneOverLn2    = 1.4426950408889633000e+00;  /* 0x3ff71547, 0x652b82fe */
static const double ln2Head       = 6.9314718055994530000e-01;  /* 0x3fe62e42, 0xfefa39ef */
static const double ln2Tail       = 2.3190468138462996000e-17;  /* 0x3c7abc9e, 0x3b39803f */
static const double maxExp        =  7.0978271289338397000e+02; /* 0x40862e42, 0xfefa39ef */
static const double minExp        = -7.4513321910194122000e+02; /* 0xc0874910, 0xd52d3052 */
static const double maxExp2       =  1024.0; 
static const double minNormExp2   = -1022.0;
static const double minExp2       = -1075.0;
static const double denormal      =  2.9387358770557188000e-39; /* 0x37f00000, 0x00000000 */
static const double oneOverDenorm =  3.402823669209384635e+38;  /* 0x47f00000, 0x00000000 */
static const hexdouble infinity   = HEXDOUBLE(0x7ff00000, 0x00000000);

/*******************************************************************************
*      Approximation coefficients.                                             *
*******************************************************************************/

static const double cm1 = 8.3333336309523691e-03;               /* 0x3f811111,0x1b4af38e */
static const double c0  = 4.1666668402777808000e-02;            /* 0x3fa55555,0x643f1505 */
static const double c1  = 1.6666666666666655000e-01;            /* 0x3fc55555,0x55555551 */
static const double c2  = 4.9999999999999955000e-01;            /* 0x3fdfffff,0xfffffff8 */

static const double cc4 = 0.5000000000000000000;
static const double cc3 = 0.16666666666663809522820;
static const double cc2 = 0.04166666666666111110939;
static const double cc1 = 0.008333338095239329810170;
static const double cc0 = 0.001388889583333492938381;

extern const unsigned long int expTable[];

struct expTableEntry 
      { 
      double x;
      double f;
      };

#if !defined(BUILDING_FOR_CARBONCORE_LEGACY)

/*******************************************************************************
*                                                                              *
*    The base e exponential function.  					       *
*                                                                              *
********************************************************************************
*                                                                              *
*    Raised exceptions are inexact, overflow & inexact and underflow & inexact.*
*                                                                              *
*******************************************************************************/
double exp ( double x ) 
      {
      hexdouble scale, xInHex, yInHex, OldEnvironment;
      register double d, y, yTail, z, zTail, z2, temp1, temp2, power, result;
      register long int i;
      struct expTableEntry *tablePointer = ( struct expTableEntry * ) expTable + 177;
      
      xInHex.d = x;
      if ( ( xInHex.i.hi & 0x7ff00000 ) < 0x40800000ul ) 
            {                                                // abs( x ) < 512
            if ( ( ( xInHex.i.hi & 0x7fffffff ) | xInHex.i.lo ) == 0x0ul ) 
                  return 1.0;
            scale.d = 0.0;
            
            fegetenvd ( OldEnvironment.d );                  // save old environment, set default
            fesetenvd ( 0.0 );
            yInHex.d = x * oneOverLn2 + rintFactor;
            scale.i.hi = ( yInHex.i.lo + 1023 ) << 20;
            yInHex.d -= rintFactor;
            y = x - ln2Head * yInHex.d;
            yTail = ln2Tail * yInHex.d;
            xInHex.d = 512.0 * y + rintFactor; 
            i = xInHex.i.lo;
            d = y - tablePointer[i].x;
            z = d - yTail;
            zTail = d - z - yTail;
            z2 = z * z;
            temp1 = cm1 * z2 + c1;
            temp2 = c0  * z2 + c2;
            OldEnvironment.i.lo |= FE_INEXACT;               // set inexact flag
            temp1 = temp1 * z + temp2;
            temp2 = temp1 * z2 + z;
            result = scale.d * tablePointer[i].f;
            temp1 = result * ( temp2 + ( zTail + zTail * temp2 ) );
            result += temp1;
            fesetenvd ( OldEnvironment.d );
            return result;
            }
      
      if ( ( x <= maxExp ) && ( x > minExp ) ) 
            {
            scale.d = 0.0;
            fegetenvd ( OldEnvironment.d );                  // save old environment, set default
            fesetenvd ( 0.0 );
            yInHex.d = x * oneOverLn2 + rintFactor;
            if ( x >= 512.0 ) 
                  {
                  power = 2.0;
                  scale.i.hi = ( yInHex.i.lo + 1022 ) << 20;
                  }
            else 
                  {
                  power = denormal;
                  scale.i.hi = ( yInHex.i.lo + 1023+128 ) << 20;
                  }
            yInHex.d -= rintFactor;
            y = x - ln2Head * yInHex.d;
            yTail = ln2Tail * yInHex.d;
            xInHex.d = 512.0 * y + rintFactor; 
            i = xInHex.i.lo;
            d = y - tablePointer[i].x;
            z = d - yTail;
            zTail = d - z - yTail;
            z2 = z * z;
            temp1 = cm1 * z2 + c1;
            temp2 = c0  * z2 + c2;
            OldEnvironment.i.lo |= FE_INEXACT;            // raise inexact flag 
            temp1 = temp1 * z + temp2;
            temp2 = temp1 * z2 + z;
            result = scale.d * tablePointer[i].f;
            temp1 = result * ( temp2 + ( zTail + zTail * temp2 ) ); 
            result = ( result + temp1 ) * power;
            fesetenvd ( OldEnvironment.d );
            return result;
            }
      else if ( x != x ) 
            return x;
      else if ( x == infinity.d ) 
            return infinity.d;
      else if ( x == -infinity.d ) 
            return +0.0;
      else if ( x > maxExp )
            {
            fegetenvd ( OldEnvironment.d );                  // get old environment
            result = infinity.d;
            OldEnvironment.i.lo |= FE_INEXACT | FE_OVERFLOW;
            fesetenvd ( OldEnvironment.d );
            return result;
            }
      else 
            {
            fegetenvd ( OldEnvironment.d );                  // get old environment
            result = +0.0;
            OldEnvironment.i.lo |= FE_INEXACT | FE_UNDERFLOW;
            fesetenvd ( OldEnvironment.d );
            return result;
            }
      }

#ifdef notdef
float expf( float x)
{
    return (float)exp( x );
}
#endif

/*******************************************************************************
*                                                                              *
*    The expm1 function.  						       *
*                                                                              *
*******************************************************************************/

double expm1 ( double x ) 
      {
      hexdouble scale, invScale, xInHex, yInHex, OldEnvironment;
      register double d, y, yTail, z, zTail, z2, temp1, temp2, power, result, f;
      register long int i;
      unsigned long int xpower;
      struct expTableEntry *tablePointer = ( struct expTableEntry * ) expTable + 177;
      
      xInHex.d = x;
      xpower = xInHex.i.hi & 0x7ff00000;
      if ( xpower < 0x40800000ul ) 
            {                                                // abs( x ) < 512
            scale.d = 0.0;
            invScale.d = 0.0;
            fegetenvd ( OldEnvironment.d );                  // save old environment, set default
            fesetenvd ( 0.0 );
            if ( xpower < 0x3c800000ul ) 
                  {                                          // |x| < 2^( -55 )
                  if ( x == 0.0 ) 
                        {
                        /* NOTHING */
                        }
                  else 
                        {
                        if ( xpower == 0x0ul )
                            OldEnvironment.i.lo |= FE_INEXACT + FE_UNDERFLOW; // set underflow/inexact
                        else
                            OldEnvironment.i.lo |= FE_INEXACT;  // set inexact
                        }
                  fesetenvd ( OldEnvironment.d );
                  return x;
                  } 
            yInHex.d = x * oneOverLn2 + rintFactor;
            scale.i.hi = ( yInHex.i.lo + 1023 ) << 20;
            yInHex.d -= rintFactor;
            y = x - ln2Head * yInHex.d;
            invScale.i.hi = 0x7fe00000 - scale.i.hi;
            yTail = ln2Tail * yInHex.d;
            xInHex.d = 512.0 * y + rintFactor;
            i = xInHex.i.lo;
            d = y - tablePointer[i].x;
            z = d - yTail;
            zTail = d - z - yTail;
            z2 = z * z;
            temp1 = cc0 * z2 + cc2;
            temp2 = cc1 * z2 + cc3;
#if 0 /* XXX scp: when x is 1, yInHex.d evaluates to 1 (!) and INEXACT is not set. */
            if ( yInHex.d != x ) OldEnvironment.i.lo |= FE_INEXACT;  // set inexact flag
#else
            OldEnvironment.i.lo |= FE_INEXACT;  // set inexact flag
#endif
            temp1 = temp1 * z2 + cc4;
            temp2 = temp1 + temp2 * z;
            temp1 = temp2 * z2 + z;
            d = tablePointer[i].f - invScale.d;
            temp2 = temp1 + ( zTail + zTail * temp1 );
            result = ( d + tablePointer[i].f * temp2 ) * scale.d;
            fesetenvd ( OldEnvironment.d );
            return result;
            }
      
      if ( ( x <= maxExp ) && ( x > minExp ) ) 
            {
            scale.d = 0.0;
            invScale.d = 0.0;
            fegetenvd ( OldEnvironment.d );                  // save old environment, set default
            fesetenvd ( 0.0 );
            yInHex.d = x * oneOverLn2 + rintFactor;
            if ( x >= 512.0 ) 
                  {
                  power = 2.0;
                  f = 0.5;
                  scale.i.hi = ( yInHex.i.lo + 1022 ) << 20;
                  }
            else 
                  {
                  power = denormal;
                  f = oneOverDenorm;
                  scale.i.hi = ( yInHex.i.lo + 1023+128 ) << 20;
                  if ( scale.i.hi < ( 168<<20 ) ) 
                        {
                        OldEnvironment.i.lo |= FE_INEXACT;  // set inexact flag
                        fesetenvd ( OldEnvironment.d );
                        return -1.0;
                        }
                  }
            invScale.i.hi = 0x7fe00000 - scale.i.hi;
            yInHex.d -= rintFactor;
            y = x - ln2Head * yInHex.d;
            yTail = ln2Tail * yInHex.d;
            xInHex.d = 512.0 * y + rintFactor; 
            i = xInHex.i.lo;
            d = y - tablePointer[i].x;
            z = d - yTail;
            zTail = d - z - yTail;
            z2 = z * z;
            temp1 = cc0 * z2 + cc2;
            temp2 = cc1 * z2 + cc3;
#if 0
            if ( yInHex.d != x ) 
                  OldEnvironment.i.lo |= FE_INEXACT;  // set inexact flag
#else
            OldEnvironment.i.lo |= FE_INEXACT;  // set inexact flag
#endif
            temp1 = temp1 * z2 + cc4;
            temp2 = temp1 + temp2 * z;
            temp1 = temp2 * z2 + z;
            d = tablePointer[i].f - f * invScale.d;
            temp2 = temp1 + ( zTail + zTail * temp1 );
            result = ( ( d + tablePointer[i].f * temp2 ) * scale.d ) * power;
            fesetenvd ( OldEnvironment.d );
            return result;
            }
      else if ( x != x ) 
            return x;
      else if ( x == infinity.d ) 
            return infinity.d;
      else if ( x == -infinity.d ) 
            return -1.0;
      else if ( x > maxExp )
            {
            fegetenvd ( OldEnvironment.d );                  // get old environment
            result = infinity.d;
            OldEnvironment.i.lo |= FE_INEXACT | FE_OVERFLOW;
            fesetenvd ( OldEnvironment.d );
            return result;
            }
      else 
            {
            fegetenvd ( OldEnvironment.d );                  // get old environment
            result = -1.0;
#if 0 /* XXX scp: test vectors don't want this to underflow */
            OldEnvironment.i.lo |= FE_INEXACT | FE_UNDERFLOW;
#else
            OldEnvironment.i.lo |= FE_INEXACT;
#endif
            fesetenvd ( OldEnvironment.d );
            return result;
            }
      }

#ifdef notdef
float expm1f( float x )
{
    return (float)expm1( x );
}
#endif

#else /* BUILDING_FOR_CARBONCORE_LEGACY */

/*******************************************************************************
*                                                                              *
*    The base 2 exponential function.  					       *
*                                                                              *
*******************************************************************************/

double exp2 ( double x ) 
      {
      hexdouble scale, xInHex, yInHex, OldEnvironment;
      register double d, y, yTail, z, zTail, z2, temp1, temp2, power, result;
      register long int i;
      struct expTableEntry *tablePointer = ( struct expTableEntry * ) expTable + 177;
      
      xInHex.d = x;
      if ( ( xInHex.i.hi & 0x7ff00000 ) < 0x40800000ul ) 
            {                                                // abs( x ) < 512
            if ( ( ( xInHex.i.hi & 0x7fffffff ) | xInHex.i.lo ) == 0x0ul ) 
                  return 1.0;
            scale.d = 0.0;
            fegetenvd ( OldEnvironment.d );                  // save old environment, set default
            fesetenvd ( 0.0 );
            yInHex.d = x + rintFactor;
            scale.i.hi = ( yInHex.i.lo + 1023 ) << 20;
            yInHex.d -= rintFactor;
            y =  ln2Head * ( x - yInHex.d );
            yTail = ln2Tail * ( x - yInHex.d );
            xInHex.d = 512.0 * y + rintFactor; 
            i = xInHex.i.lo;
            d = y - tablePointer[i].x;
            z = d - yTail;
            zTail = d - z - yTail;
            z2 = z * z;
            temp1 = cm1 * z2 + c1;
            temp2 = c0  * z2 + c2;
            if ( yInHex.d != x ) OldEnvironment.i.lo |= FE_INEXACT;  // set inexact flag
            temp1 = temp1 * z + temp2;
            temp2 = temp1 * z2 + z;
            result = scale.d * tablePointer[i].f;
            temp1 = result * ( temp2 + ( zTail + zTail * temp2 ) ); 
            result += temp1;
            fesetenvd ( OldEnvironment.d );
            return result;
            }
      
      if ( ( x < maxExp2 ) && ( x > minExp2 ) ) 
            {
            scale.d = 0.0;
            fegetenvd ( OldEnvironment.d );                  // save old environment, set default
            fesetenvd ( 0.0 );
            if (x < minNormExp2)
                OldEnvironment.i.lo |= FE_UNDERFLOW;  // set underflow flag
            yInHex.d = x + rintFactor;
            if ( x >= 512.0 ) 
                  {
                  power = 2.0;
                  scale.i.hi = ( yInHex.i.lo + 1022 ) << 20;
                  }
            else 
                  {
                  power = denormal;
                  scale.i.hi = ( yInHex.i.lo + 1023+128 ) << 20;
                  }
            yInHex.d -= rintFactor;
            y =  ln2Head * ( x - yInHex.d );
            yTail = ln2Tail * ( x - yInHex.d );
            xInHex.d = 512.0 * y + rintFactor; 
            i = xInHex.i.lo;
            d = y - tablePointer[i].x;
            z = d - yTail;
            zTail = d - z - yTail;
            z2 = z * z;
            temp1 = cm1 * z2 + c1;
            temp2 = c0  * z2 + c2;
            if ( yInHex.d != x ) OldEnvironment.i.lo |= FE_INEXACT;  // set inexact flag
            temp1 = temp1 * z + temp2;
            temp2 = temp1 * z2 + z;
            result = scale.d * tablePointer[i].f;
            temp1 = result * ( temp2 + ( zTail + zTail * temp2 ) );
            result = ( result + temp1 ) * power;
            fesetenvd ( OldEnvironment.d );
            return result;
            }
      else if ( x != x ) 
            return x;
      else if ( x == infinity.d ) 
            return infinity.d;
      else if ( x == -infinity.d ) 
            return +0.0;
      else if ( x > maxExp )
            {
            fegetenvd ( OldEnvironment.d );                  // get old environment
            result = infinity.d;
            OldEnvironment.i.lo |= FE_INEXACT | FE_OVERFLOW;
            fesetenvd ( OldEnvironment.d );
            return result;
            }
      else 
            {
            fegetenvd ( OldEnvironment.d );                  // get old environment
            result = +0.0;
            OldEnvironment.i.lo |= FE_INEXACT | FE_UNDERFLOW;
            fesetenvd ( OldEnvironment.d );
            return result;
            }
      }

#ifdef notdef
float exp2f( float x)
{
    return (float)exp2( x );
}
#endif

#endif /* BUILDING_FOR_CARBONCORE_LEGACY */

#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
