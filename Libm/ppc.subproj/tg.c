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
*    tg.c                                                                      *
*                                                                              *
*    Double precision Tangent                                                  *
*                                                                              *
*    Copyright: © 1993-1997 by Apple Computer, Inc., all rights reserved       *
*                                                                              *
*    Written and ported by A. Sazegari, started on June 1997.                  *
*    Modified by Robert Murley, 2001                                           *
*                                                                              *
*    A MathLib v4 file.                                                        *
*                                                                              *
*    Based on the trigonometric functions from IBM/Taligent.                   *
*                                                                              *
*    Modification history:                                                     *
*    November  06 2001: commented out warning about Intel architectures.       *
*    July      20 2001: replaced __setflm with fegetenvd/fesetenvd.            *
*                       replaced DblInHex typedef with hexdouble.              *
*    September 07 2001: added #ifdef __ppc__.                                  *
*    September 19 2001: added macros to detect PowerPC and correct compiler.   *
*    September 19 2001: added <CoreServices/CoreServices.h> to get to <fp.h>   *
*                       and <fenv.h>, removed denormal comments.               *
*    September 25 2001: removed more denormal comments.                        *
*    October   08 2001: removed <CoreServices/CoreServices.h>.                 *
*                       changed compiler errors to warnings.                   *
*                                                                              *
*    W A R N I N G:                                                            *
*    This routine requires a 64-bit double precision IEEE-754 model.           *
*    They are written for PowerPC only and are expecting the compiler          *
*    to generate the correct sequence of multiply-add fused instructions.      *
*                                                                              *
*    This routine is not intended for 32-bit Intel architectures.              *
*                                                                              *
*     08 Oct 01   ram   changed compiler errors to warnings.                   *
*                                                                              *
*      GCC compiler options:                                                   *
*            optimization level 3 (-O3)                                        *
*            -fschedule-insns -finline-functions -funroll-all-loops            *
*                                                                              *
*******************************************************************************/

#ifdef      __APPLE_CC__
#if         __APPLE_CC__ > 930

#include    "fenv_private.h"
#include    "fp_private.h"

#define TRIG_NAN  "33"

struct tableEntry                             /* tanatantable entry structure */
      {
      double p;
      double f5;
      double f4;
      double f3;
      double f2;
      double f1;
      double f0;
      };

extern const unsigned long tanatantable[];

static const double kPiBy2 = 1.570796326794896619231322;      // 0x1.921fb54442d18p0
static const double kPiBy2Tail = 6.1232339957367660e-17;      // 0x1.1a62633145c07p-54
static const double k2ByPi = 0.636619772367581382;            // 0x1.45f306dc9c883p-1
static const double kRint = 6.755399441055744e15;             // 0x1.8p52
static const double kRintBig = 2.7021597764222976e16;         // 0x1.8p54
static const double kPiScale42 = 1.38168706094305449e13;      // 0x1.921fb54442d17p43
static const double kPiScale53 = 2.829695100811376e16;        // 0x1.921fb54442d18p54
static const hexdouble infinity = HEXDOUBLE(0x7ff00000, 0x00000000);

/*******************************************************************************
********************************************************************************
*                                 T   A   N                                    *
********************************************************************************
*******************************************************************************/

double tan (   double x  )
      {
      register double absOfX, intquo ,arg, argtail, aSquared, aThird, aFourth,
                      temp1, temp2, s, u, v, w, y, result;
      long tableIndex;
      unsigned long iz;
      hexdouble zD, aD, OldEnvironment;
      struct tableEntry *tablePointer;
      const double ts11 = 8.898406739539066157565e-3,
                   ts9  = 2.186936821731655951177e-2,
                   ts7  = 5.396825413618260185395e-2,
                   ts5  = 0.1333333333332513155016,
                   ts3  = 0.3333333333333333333333;
      const double tt7  = 0.05396875452473400572649,
                   tt5  = 0.1333333333304691192925,
                   tt3  = 0.3333333333333333357614;
      const double k2ToM1023 = 1.112536929253600692e-308; // 0x1.0p-1023
      static long indexTable[] =
            {  
            16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,
            27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,
            38,  39,  40,  41,  42,  43,  44,  45,  47,  48,  49,
            50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,
            61,  62,  63,  64,  65,  66,  68,  69,  70,  71,  72,
            73,  74,  75,  76,  77,  78,  79,  81,  82,  83,  84,
            85,  86,  87,  88,  89,  91,  92,  93,  94,  95,  96,
            97,  98,  100, 101, 102, 103, 104, 105, 107, 108, 109,
            110, 111, 113, 114, 115, 116, 117, 119, 120, 121, 122,
            123, 125, 126, 127, 128, 130, 131, 132, 133, 135, 136,
            137, 139, 140, 141, 142, 144, 145, 146, 148, 149, 150,
            152, 153, 154, 156, 157, 159, 160, 161, 163, 164, 166,
            167, 168, 170, 171, 173, 174, 176, 177, 179, 180, 182,
            183, 185, 186, 188, 189, 191, 192, 194, 196, 197, 199,
            200, 202, 204, 205, 207, 209, 210, 212, 214, 215, 217,
            219, 220, 222, 224, 226, 228, 229, 231, 233, 235, 237,
            238, 240, 242, 244, 246, 248, 250, 252, 254, 256, 256
            };
            
      static const double kMinNormal = 2.2250738585072014e-308;  // 0x1.0p-1022
     
      absOfX = __fabs( x );
      
      tablePointer = ( struct tableEntry * ) ( tanatantable - ( 16 * 14 ) );      // init tbl ptr
      fegetenvd( OldEnvironment.d );                     // save env, set default
      fesetenvd( 0.0 );
      
/*******************************************************************************
*     |x| < 0.7853980064392090732                                              *
*******************************************************************************/

      if ( absOfX < 0.7853980064392090732 ) 
            {  

            if ( absOfX == 0.0 )
                {
                fesetenvd( OldEnvironment.d );       	// restore caller's mode
                return x; 				// +0 -0 preserved
                }
                
            aD.d = 256.0 * absOfX + kRint;
            OldEnvironment.i.lo |= FE_INEXACT;               // set INEXACT

/*******************************************************************************
*     |x| < 0.0625                                                             *
*******************************************************************************/
            
            if ( aD.i.lo < 16UL ) 
                  {

                  aSquared = absOfX * absOfX;
                  temp1 = ( ( ( ts11 * aSquared + ts9 ) * aSquared + ts7 ) * aSquared + ts5 )*aSquared + ts3;
                  aThird = absOfX * aSquared;
                  if ( x > 0.0 )
                        result = ( temp1 * aThird + absOfX );
                  else 
                        {
                        aThird = -aThird;
                        result = ( temp1 * aThird - absOfX );
                        }
                        
                  if ( fabs ( result ) < kMinNormal )
                        OldEnvironment.i.lo |= FE_UNDERFLOW;
                        
                  fesetenvd( OldEnvironment.d );             // restore caller's mode
                  return result;
                  }
            
/*******************************************************************************
*     .0625 <= x < .7853980064392090732.                                       *
*******************************************************************************/
            else 
                  {
                  tableIndex = indexTable[aD.i.lo - 16];
                  w = absOfX - tablePointer[tableIndex].f0;  // w = deltax
                  y = tablePointer[tableIndex].p;            // y = Tan from table
                  aSquared = w * w;                          // calculate delta Tan
                  temp1 = ( ( tt7 * aSquared + tt5 ) * aSquared + tt3 ) * ( aSquared * w ) + w;
                  v = y * temp1;
                  aSquared = v * v;
                  aThird = 1.0 + v;
                  aFourth = aSquared * aSquared;
                  w = aThird + aThird * aSquared;
                  aThird = w + w * aFourth;
                  temp1 = temp1 + v * y;
                  if ( x > 0.0 )                             // fix final sign
                        result = ( y + temp1 * aThird );
                  else 
                        {
                        aThird = -aThird;
                        result = ( temp1 * aThird - y );
                        }
                  fesetenvd( OldEnvironment.d );             // restore caller's mode
                  return result;
                  }
            }
      
      if ( x != x )                                          // x is a NaN
            {
            fesetenvd( OldEnvironment.d );                   // restore caller's mode
            return ( x );
            }
      
/*******************************************************************************
*     |x| > ¹/4 and perhaps |x| > kPiScale42.                                  *
*******************************************************************************/

      if ( absOfX > kPiScale42 ) 
            {                                                // |x| is huge or infinite
            if ( absOfX == infinity.d ) 
                  {                                          // infinite case is invalid
                  OldEnvironment.i.lo |= SET_INVALID;
                  fesetenvd( OldEnvironment.d );             // restore caller's mode
                  return ( nan ( TRIG_NAN ) );               // return NaN
                  }
            
            while ( absOfX > kPiScale53 ) 
                  {                                          // loop to reduce x below
                  intquo = x * k2ByPi;                       // ¹*2^53 in magnitude
                  x = ( x - intquo * kPiBy2 ) - intquo * kPiBy2Tail;
                  absOfX = __fabs( x );
                  }

/*******************************************************************************
*     final reduction of x to magnitude between 0 and 2*¹.                     *
*******************************************************************************/
            
            intquo = ( x * k2ByPi + kRintBig ) - kRintBig;
            x = ( x - intquo * kPiBy2 ) - intquo * kPiBy2Tail;
            absOfX = __fabs( x );
            }
      
/*******************************************************************************
*     ¹/4 < x < ¹*2^42                                                         *
*******************************************************************************/

      OldEnvironment.i.lo |= FE_INEXACT;                     // set the inexact flag
      
/*******************************************************************************
*     Further argument reduction is probably necessary.  A double-double       *
*     reduced argument is determined ( arg|argtail ).                          *
*******************************************************************************/

      zD.d = x*k2ByPi + kRint;                               // find int quo x / ( pi/2 )
      iz = zD.i.lo & 1UL;                                    // quo modulo 2
      intquo = zD.d - kRint;                                 // Rint( x )
      arg = ( x - intquo*kPiBy2 ) - intquo*kPiBy2Tail;       // reduced arg ( head )
      absOfX = __fabs( arg );
      aD.d = 256.0*absOfX + kRint;
      argtail = ( ( x - intquo*kPiBy2 ) - arg ) - intquo*kPiBy2Tail; // red. arg tail
      
/*******************************************************************************
*     |arg| < .0625.                                                           *
*******************************************************************************/

      if ( aD.i.lo < 16UL ) 
            {
            u = absOfX;
            aSquared = u * u;
            v = argtail;
            temp1 = ( ( ( ts11 * aSquared + ts9 ) * aSquared + ts7 ) * aSquared + ts5 ) * aSquared + ts3;
            aThird = aSquared*u;
            
/*******************************************************************************
*     tangent approximation starts here.                                       *
*******************************************************************************/

            if ( iz == 0 ) 
                  {
                  if ( arg > 0.0 ) 
                        {                                    // positive arg
                        w = temp1 * aThird + v;
                        result =  ( w + u );
                        }
                  
                  else 
                        {                                    // negative arg
                        w = v - temp1 * aThird;
                        result = ( w - u );
                        }
                  fesetenvd( OldEnvironment.d );             // restore caller's mode
                  return result;   
                  }
            
/*******************************************************************************
*     cotangent approximation starts here.                                     *
*******************************************************************************/
            else 
                  {
                  if ( arg > 0.0 ) 
                        {                                    // positive argument
                        s = temp1 * aThird + v;
                        temp2 = s + u;
                        }
                  
                  else 
                        {                                    // negative argument
                        s = temp1 * aThird - v;
                        temp2 = s + u;
                        }
                  
                  aFourth = ( u - temp2 ) + s;
                  
                  if ( __fabs ( temp2 ) < k2ToM1023 ) 
                        {                                    // huge result
                        if ( arg > 0 ) 
                              result = ( 1.0 / temp2 );
                        else 
                              result = ( 1.0 / ( -temp2 ) );
                        fesetenvd( OldEnvironment.d );        // restore caller's mode
                        return result;   
                        }
                  
                  u = 1.0 / temp2;
                  v = 1.0 - u * temp2;
                  temp1 = aFourth * u - v;
                  
                  if ( arg > 0.0 )
                        result = ( temp1 * u - u );
                  else 
                        {
                        temp1 = -temp1;
                        result = ( temp1 * u + u );
                        }
                  fesetenvd( OldEnvironment.d );             // restore caller's mode
                  return result;   
                  }
            } 
      
/*******************************************************************************
*     The following code covers the case where the reduced argument has        *
*     magnitude greater than 0.0625 but less than ¹/4.                         *
*******************************************************************************/
      
      tableIndex = indexTable [ aD.i.lo - 16 ];
      
      if ( arg > 0.0 ) 
            {                                                        // positive argument
            w = absOfX - tablePointer [tableIndex].f0 + argtail;     // argument delta
            aSquared = w * w;
            v = absOfX - tablePointer [tableIndex].f0 - w + argtail; // tail of argument delta
            }
      
      else 
            {                                                        // negative argument
            w = absOfX - tablePointer [tableIndex].f0 - argtail;     // argument delta
            aSquared = w * w;
            v = absOfX - tablePointer [tableIndex].f0 - w - argtail; // tail of argument delta
            }
      
      y = tablePointer[tableIndex].p;
      temp1 = ( ( tt7 * aSquared + tt5 ) * aSquared + tt3 ) * ( aSquared * w ) + v + w;
      v = y * temp1;                                                 // Tan( delta )

/*******************************************************************************
*      polynomial approx of 1/(1-v)                                            *
*******************************************************************************/

      aSquared = v * v;
      aThird = 1.0 + v;
      aFourth = aSquared * aSquared;
      w = aThird + aThird * aSquared;
      aThird = w + w * aFourth;                                      // aThird = 1/(1-v)
      s = ( 1.0 + y * y ) * aThird;
      
      if ( iz == 0 ) 
            {                                                        // tangent approximation
            fesetenvd( OldEnvironment.d );                           // restore caller's mode
            if ( arg > 0.0 )
                  result = ( y + temp1 * s );
            else
                  {
                  y = -y;
                  result = ( y - temp1 * s );
                  }
            fesetenvd( OldEnvironment.d );                           // restore caller's mode
            return result;   
            }
      
      else 
            {                                                        // cotangent approx
            w = y + temp1 * s;
            aFourth = ( y - w ) + temp1 * s;
            u = 1.0 / w;
            v = 1.0 - u * w;
            w = u * aFourth - v;
            if ( arg > 0.0 )
                  result = ( u * w - u );
            else 
                  {
                  w = -w;
                  result = ( u * w + u );
                  }
            fesetenvd( OldEnvironment.d );                            // restore caller's mode
            return result;   
            }
      }

#ifdef notdef
float tanf( float x )
{
    return (float)tan( x );
}
#endif

#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
