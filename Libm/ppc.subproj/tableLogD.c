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
*     File tableLogD.c,                                                        *
*     Functions log, log1p and log10.                                          *
*     Implementation of log( x ) based on IBM/Taligent table method.           *
*                                                                              *
*     Copyright © 1997-2001 Apple Computer, Inc.  All rights reserved.         *
*                                                                              *
*     Written by Ali Sazegari, started on April 1997.                          *
*     Modified and ported by Robert A. Murley (ram) for Mac OS X.              *
*                                                                              *
*     A MathLib v4 file.                                                       *
*                                                                              *
*     Nov   07 2001: removed log2 to prevent conflict with CarbonCore.         *
*     Nov   06 2001: commented out warning about Intel architectures.          *
*                    changed i386 stub to call abort().                        *
*     Nov   02 2001: added stub for i386 version of log2.                      *
*     April 28 1997: port of the ibm/taligent log routines.                    *
*     Sept  06 2001: replaced __setflm with fegetenvd/fesetenvd.               *
*                    replaced DblInHex typedef with hexdouble.                 *
*                    used standard exception symbols from fenv.h.              *
*                    added #ifdef __ppc__.                                     *
*                    used faster fixed-point instructions to calculate 'n'     *
*     Sept  09 2001: added more comments.                                      *
*     Sept  10 2001: added macros to detect PowerPC and correct compiler.      *
*     Sept  18 2001: added <CoreServices/CoreServices.h> to get <fp.h> and     *
*                    <fenv.h>.                                                 *
*     Oct   08 2001: removed <CoreServices/CoreServices.h>.                    *
*                    changed compiler errors to warnings.                      *
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

#include      "fp_private.h"
#include      "fenv_private.h"

#define  LOGORITHMIC_NAN  "36"

/*******************************************************************************
*      Floating-point constants.                                               *
*******************************************************************************/

static const double ln2            = 6.931471805599452862e-1;
static const double ln2Tail        = 2.319046813846299558e-17;
static const double log10e         = 4.342944819032518200e-01;  // head of log10 of e { 0x3fdbcb7b, 0x1526e50e }
static const double log10eTail     = 1.098319650216765200e-17;  // tail of log10 of e { 0x3c695355, 0xbaaafad4 }
static const double log2e          = 1.4426950408889634e+0;     // head of log2  of e { 0x3ff71547, 0x652b82fe }
static const double log2eTail      = 2.0355273740931033e-17;    // tail of log2  of e { 0x3c7777d0, 0xffda0d24 }
static const double twoTo128       = 3.402823669209384635e+38;
static const double largestDenorm  = 2.2250738585072009e-308;   // { 0x000fffff, 0xffffffff }
static const hexdouble infinity     = HEXDOUBLE(0x7ff00000, 0x00000000);

/*******************************************************************************
*      Approximation coefficients.                                             *
*******************************************************************************/

static const double c2 = -0.5000000000000000042725;
static const double c3 =  0.3333333333296328456505;
static const double c4 = -0.2499999999949632195759;
static const double c5 =  0.2000014541571685319141;
static const double c6 = -0.1666681511199968257150;

static const double d2 = -0.5000000000000000000000;             // { 0xBFE00000, 0x00000000 }
static const double d3 =  0.3333333333333360968212;             // { 0x3FD55555, 0x55555587 }
static const double d4 = -0.2500000000000056849516;             // { 0xBFD00000, 0x00000066 }
static const double d5 =  0.1999999996377879912068;             // { 0x3FC99999, 0x98D278CB }
static const double d6 = -0.1666666662009609743117;             // { 0xBFC55555, 0x54554F15 }
static const double d7 =  0.1428690115662276259345;             // { 0x3FC24988, 0x2224F72F }
static const double d8 = -0.1250122079043555957999;             // { 0xBFC00066, 0x68466517 }

extern const unsigned long int logTable[];

struct logTableEntry 
       {
       double X;
       double G;
       hexdouble F;
       };

#if !defined(BUILDING_FOR_CARBONCORE_LEGACY)

/*******************************************************************************
*                                                                              *
*    The base e logorithm function.  Caller’s rounding direction is honored.   *
*                                                                              *
********************************************************************************
*                                                                              *
*    calls the intrinsic functions __setflm, __fabs.                           *
*    raised exceptions are inexact, divide by zero and invalid.                *
*                                                                              *
*******************************************************************************/

double log ( double x ) 
       {
       hexdouble yInHex, xInHex, OldEnvironment;
       register double n, zTail, high, low, z, z2, temp1, temp2, temp3, result;
       register long int i;
       unsigned long int xhead;
       struct logTableEntry *tablePointer = ( struct logTableEntry * ) logTable;
       
       xInHex.d = x;
       fegetenvd( OldEnvironment.d );
       fesetenvd( 0.0 );
       xhead = xInHex.i.hi;
       
       if ( xhead < 0x7ff00000 ) 
              {                                             // x is finite and non-negative
              if ( xhead >= 0x000fffff )  
                     {                                      // x is normal
                     i = xInHex.i.hi & 0x000fffff;
                     yInHex.i.lo = xInHex.i.lo;
                     yInHex.i.hi = i | 0x3ff00000;          // now 1.0 <= y < 2.0
                     if ( yInHex.i.hi & 0x00080000 ) 
                            {                               // 1.5 <= y < 2.0
                            n = (long) ( xInHex.i.hi >> 20 ) - 1022;
                            i = ( i >> 13 ) & 0x3f;         // table lookup index
                            yInHex.d *= 0.5;
                            }
                     else 
                            {                               // 1.0 <= y < 1.5
                            n = (long) ( xInHex.i.hi >> 20 ) - 1023;
                            i = ( i >> 12 ) + 64;           // table lookup index
                            }
                     }
              else if ( x != 0.0 ) 
                     {                                      // x is nonzero, denormal
                     xInHex.d = x * twoTo128;
                     i = xInHex.i.hi & 0x000fffff;
                     yInHex.i.lo = xInHex.i.lo;
                     yInHex.i.hi = i | 0x3ff00000;          // now 1.0 <= y < 2.0
                     if ( yInHex.i.hi & 0x00080000 ) 
                            {                               // 1.5 <= y < 2.0
                            n = (long) ( xInHex.i.hi >> 20 ) - 1150.0;
                            i = ( i >> 13 ) & 0x3f;         // table lookup index
                            yInHex.d *= 0.5;
                            }
                     else 
                            {                               // 1.0 <= y < 1.5
                            n = (long) ( xInHex.i.hi >> 20 ) - 1151;
                            i = ( i >> 12 ) + 64;           // table lookup index
                            }
                     }
              else                                          // x is 0.0
                     {
                     OldEnvironment.i.lo |= FE_DIVBYZERO;
                     result = -infinity.d;
                     fesetenvd( OldEnvironment.d );
                     return result;
                     }
                     
              z = ( yInHex.d - tablePointer[i].X ) * tablePointer[i].G;
              zTail = ( ( yInHex.d - tablePointer[i].X ) - z * tablePointer[i].X ) * tablePointer[i].G;
              z2 = z * z; 
              if ( n == 0.0 ) 
                     {                                     // 0.75 <= x < 1.5
                     if ( x == 1.0 ) 
                            {
                            fesetenvd( OldEnvironment.d );
                            return +0.0;
                            }
                     temp1 = d8 * z2 + d6;
                     temp2 = d7 * z2 + d5;
                     temp1 = temp1 * z2 + d4;
                     temp2 = temp2 * z2 + d3;
                     OldEnvironment.i.lo |= FE_INEXACT; // set inexact flag
                     temp1 = temp1 * z + temp2;
                     temp2 = temp1 * z + d2;
                     temp3 = z + tablePointer[i].F.d;
                     low = tablePointer[i].F.d - temp3 + z + ( zTail - z * zTail );
                     result = temp2 * z2 + low;
                     result += temp3;
                     fesetenvd( OldEnvironment.d );
                     return result;
                     }
              else if ( tablePointer[i].F.i.hi != 0 ) 
                     {                                     // n != 0, and y not close to 1
                     low = n * ln2Tail + zTail;
                     high = z + tablePointer[i].F.d;
                     zTail = tablePointer[i].F.d - high + z;
                     OldEnvironment.i.lo |= FE_INEXACT; // set inexact flag
                     temp1 = n * ln2 + low;
                     low = ( n * ln2 - temp1 ) + low;
                     temp3 = c6 * z2 + c4;
                     temp2 = c5 * z2 + c3;
                     temp3 = temp3 * z2;
                     temp2 = ( temp2 * z + temp3 ) + c2;
                     temp3 = high + temp1; 
                     temp1 = ( temp1 - temp3 ) + high;
                     result = ( ( temp2 * z2 + low ) + temp1 ) + zTail;
                     result += temp3;
                     fesetenvd( OldEnvironment.d );
                     return result;
                     }
              else 
                     {                                     // n != 0 and y close to 1
                     low = n * ln2Tail + zTail;
                     temp3 = n  * ln2 + low;
                     low = ( n * ln2 - temp3 ) + low;
                     OldEnvironment.i.lo |= FE_INEXACT; // set inexact flag
                     temp1 = d8 * z2 + d6;
                     temp2 = d7 * z2 + d5;
                     temp1 = temp1 * z2 + d4;
                     temp2 = temp2 * z2 + d3;
                     temp1 = temp1 * z + temp2;
                     temp2 = temp1 * z + d2;
                     result = ( temp2 * z2 + low ) + z;
                     result += temp3;
                     fesetenvd( OldEnvironment.d );
                     return result;
                     }
              }
       
       if ( x == 0.0 )
              {
              OldEnvironment.i.lo |= FE_DIVBYZERO;
              x = -infinity.d;
              }
       else if ( x < 0.0 )
              {                                           // x < 0.0
              OldEnvironment.i.lo |= SET_INVALID;
              x = nan ( LOGORITHMIC_NAN );
              }
              
       fesetenvd( OldEnvironment.d );
       return x;
       
       }

#ifdef notdef
float logf ( float x )
{
    return (float)log( x );
}
#endif

/*******************************************************************************
*                                                                              *
*    The base 10 logorithm function.  Caller’s rounding direction is honored.  *
*                                                                              *
*******************************************************************************/

double log10 ( double x ) 
       {
       hexdouble yInHex, xInHex, OldEnvironment;
       register double n, zTail, high, low, z, z2, temp1, temp2, temp3, result, resultLow;
       register long int i;
       unsigned long int xhead;
       struct logTableEntry *tablePointer = ( struct logTableEntry * ) logTable;
       
       xInHex.d = x;
       fegetenvd( OldEnvironment.d );
       fesetenvd( 0.0 );
       xhead = xInHex.i.hi;
       
       if ( xhead < 0x7ff00000 ) 
              {                                             // x is finite and non-negative
              if ( xhead >= 0x000fffff ) 
                     {                                      // x is normal
                     i = xInHex.i.hi & 0x000fffff;
                     yInHex.i.lo = xInHex.i.lo;
                     yInHex.i.hi = i | 0x3ff00000;          // now 1.0 <= y < 2.0
                     if ( yInHex.i.hi & 0x00080000 ) 
                            {                               // 1.5 <= y < 2.0
                            n = (long) ( xInHex.i.hi >> 20 ) - 1022;
                            i = ( i >> 13 ) & 0x3f;         // table lookup index
                            yInHex.d *= 0.5;
                            }
                     else 
                            {                               // 1.0 <= y < 1.5
                            n = (long) ( xInHex.i.hi >> 20 ) - 1023;
                            i = ( i >> 12 ) + 64;           // table lookup index
                            }
                     }
              else if ( x != 0.0 ) 
                     {                                      // x is nonzero, denormal
                     xInHex.d = x * twoTo128;
                     i = xInHex.i.hi & 0x000fffff;
                     yInHex.i.lo = xInHex.i.lo;
                     yInHex.i.hi = i | 0x3ff00000;          // now 1.0 <= y < 2.0
                     if ( yInHex.i.hi & 0x00080000 ) 
                            {                               // 1.5 <= y < 2.0
                            n = (long) ( xInHex.i.hi >> 20 ) - 1150;
                            i = ( i >> 13 ) & 0x3f;         // table lookup index
                            yInHex.d *= 0.5;
                            }
                     else 
                            {                               // 1.0 <= y < 1.5
                            n = (long) ( xInHex.i.hi >> 20 ) - 1151;
                            i = ( i >> 12 ) + 64;           // table lookup index
                            }
                     }
              else 
                     {
                     OldEnvironment.i.lo |= FE_DIVBYZERO;
                     result = -infinity.d;
                     fesetenvd( OldEnvironment.d );
                     return result;
                     }

              z = ( yInHex.d - tablePointer[i].X ) * tablePointer[i].G;
              zTail = ( ( yInHex.d - tablePointer[i].X ) - z * tablePointer[i].X ) * tablePointer[i].G;
              z2 = z * z; 
              if ( n == 0.0 ) 
                     {                                     // 0.75 <= x < 1.5
                     if ( x == 1.0 ) 
                            {
                            fesetenvd( OldEnvironment.d );
                            return +0.0;
                            }
                     temp1 = d8 * z2 + d6;
                     temp2 = d7 * z2 + d5;
                     temp1 = temp1 * z2 + d4;
                     temp2 = temp2 * z2 + d3;
                     OldEnvironment.i.lo |= FE_INEXACT; // set inexact flag
                     temp1 = temp1 * z + temp2;
                     temp2 = temp1 * z + d2;
                     temp3 = z + tablePointer[i].F.d;
                     low = tablePointer[i].F.d - temp3 + z + ( zTail - z * zTail );
                     result = ( temp2 * z2 + low ) + temp3;
                     resultLow = temp3 - result + ( temp2 * z2 + low );
                     resultLow = resultLow * log10e + result * log10eTail;
                     result = ( resultLow + result * log10e );
                     fesetenvd( OldEnvironment.d );
                     return result;
                     }
              else if ( tablePointer[i].F.i.hi != 0 ) 
                     {
                     low = n * ln2Tail + zTail;
                     high = z + tablePointer[i].F.d;
                     zTail = tablePointer[i].F.d - high + z;
                     OldEnvironment.i.lo |= FE_INEXACT; // set inexact flag
                     temp1 = n * ln2 + low;
                     low = ( n* ln2 - temp1 ) + low;
                     temp3 = c6 * z2 + c4;
                     temp2 = c5 * z2 + c3;
                     temp3 = temp3 * z2;
                     temp2 = ( temp2 * z + temp3 ) + c2;
                     temp3 = high + temp1; 
                     temp1 = ( temp1 - temp3 ) + high;
                     result = ( ( ( temp2 * z2 + low ) + temp1 ) + zTail ) + temp3;
                     resultLow = temp3 - result + zTail + temp1 + ( temp2 * z2 + low );
                     resultLow = resultLow * log10e + result * log10eTail;
                     result = ( resultLow + result * log10e ); 
                     fesetenvd( OldEnvironment.d );
                     return result;
                     }
              else 
                     {
                     low = n * ln2Tail + zTail;
                     temp3 = n * ln2 + low;
                     low = ( n * ln2 - temp3 ) + low;
                     OldEnvironment.i.lo |= FE_INEXACT; // set inexact flag
                     temp1 = d8 * z2 + d6;
                     temp2 = d7 * z2 + d5;
                     temp1 = temp1 * z2 + d4;
                     temp2 = temp2 * z2 + d3;
                     temp1 = temp1 * z + temp2;
                     temp2 = temp1 * z + d2;
                     result = ( ( temp2 * z2 + low ) + z ) + temp3;
                     resultLow = temp3 - result + z + ( temp2 * z2 + low );
                     resultLow = resultLow * log10e + result * log10eTail;
                     result = ( resultLow + result * log10e );
                     fesetenvd( OldEnvironment.d );
                     return result;
                     }
              }
       
       if ( x == 0.0 )
              {
              OldEnvironment.i.lo |= FE_DIVBYZERO;
              x = -infinity.d;
              }
       else if ( x < 0.0 )
              {                                           // x < 0.0
              OldEnvironment.i.lo |= SET_INVALID;
              x = nan ( LOGORITHMIC_NAN );
              }
              
       fesetenvd( OldEnvironment.d );
       return x;
       
       }

#ifdef notdef
float log10f ( float x )
{
    return (float)log10( x );
}
#endif

/*******************************************************************************
*                                                                              *
*    The base e log(1+x) function.  Caller’s rounding direction is honored.    *
*                                                                              *
*******************************************************************************/

double log1p ( double x ) 
       {
       hexdouble yInHex, xInHex, OldEnvironment;
       register double yLow, n, zTail, high, low, z, z2, temp1, temp2, temp3, result;
       register long int i;
       struct logTableEntry *tablePointer = ( struct logTableEntry * ) logTable;
       
//     N.B. x == NAN fails all comparisons and falls through to the return.

       fegetenvd( OldEnvironment.d );
       fesetenvd( 0.0 );
       
       if ( ( x > -1.0 ) && ( x < infinity.d ) ) 
              {
              yInHex.d = 1.0 + x;                          // yInHex.d cannot be a denormal number
              yLow = ( x < 1.0 ) ? ( 1.0 - yInHex.d ) + x : ( x - yInHex.d ) + 1.0; 
              
              i = yInHex.i.hi & 0x000fffff;
              n = (long) ( yInHex.i.hi >> 20 ) - 1023;
              xInHex.i.lo = 0x0;
              xInHex.i.hi = 0x7fe00000 - ( yInHex.i.hi & 0x7ff00000 );
              yLow *= xInHex.d;
              yInHex.i.hi = i | 0x3ff00000;                // now 1.0 <= y < 2.0
              if ( yInHex.i.hi & 0x00080000 ) 
                     {                                     // 1.5 <= y < 2.0
                     n += 1.0;
                     yInHex.d *= 0.5;
                     yLow *= 0.5;
                     i = ( i >> 13 ) & 0x3f;               // table lookup index
                     }
              else 
                     {                                     // 1.0 <= y < 1.5
                     i = ( i >> 12 ) + 64;                 // table lookupndex
                     }
              z = ( yInHex.d - tablePointer[i].X ) * tablePointer[i].G;
              zTail = ( ( yInHex.d - tablePointer[i].X ) - z * tablePointer[i].X + yLow ) * tablePointer[i].G;
              z2 = z * z; 
              if ( n == 0.0 ) 
                     {
                     if ( yInHex.d == 1.0 ) 
                            {
                            if ( x != 0.0 ) 
                                   {
                                   if ( __fabs( x ) <= largestDenorm )
                                          OldEnvironment.i.lo |= FE_UNDERFLOW | FE_INEXACT; // set underflow/inexact flag
                                   else
                                          OldEnvironment.i.lo |= FE_INEXACT; // set inexact flag
                                   }
                            fesetenvd( OldEnvironment.d );
                            return x;
                            }
                     temp1 = d8 * z2 + d6;
                     temp2 = d7 * z2 + d5;
                     temp1 = temp1 * z2 + d4;
                     temp2 = temp2 * z2 + d3;
                     temp1 = temp1 * z + temp2;
                     temp2 = temp1 * z + d2;
                     temp3 = z + tablePointer[i].F.d;
                     low = tablePointer[i].F.d - temp3 + z + ( zTail - z * zTail );
                     result = temp2 * z2 + low;
                     }
              else if ( tablePointer[i].F.i.hi != 0 ) 
                     {
                     low = n * ln2Tail + zTail;
                     high = z + tablePointer[i].F.d;
                     zTail = tablePointer[i].F.d - high + z;
                     temp1 = n * ln2 + low;
                     low = ( n * ln2 - temp1 ) + low;
                     temp3 = c6 * z2 + c4;
                     temp2 = c5 * z2 + c3;
                     temp3 = temp3 * z2;
                     temp2 = ( temp2 * z + temp3 ) + c2;
                     temp3 = high + temp1; 
                     temp1 = ( temp1 - temp3 ) + high;
                     result = ( ( temp2 * z2 + low ) + temp1 ) + zTail;
                     }
              else 
                     {
                     low = n * ln2Tail + zTail;
                     temp3  = n * ln2 + low;
                     low = ( n * ln2 - temp3 ) + low + yLow;
                     temp1  = d8 * z2 + d6;
                     temp2  = d7 * z2 + d5;
                     temp1  = temp1 * z2 + d4;
                     temp2  = temp2 * z2 + d3;
                     temp1  = temp1 * z + temp2;
                     temp2  = temp1 * z + d2;
                     result = ( temp2 * z2 + low ) + z;
                     }
              OldEnvironment.i.lo |= FE_INEXACT;       // set inexact flag
              result += temp3;
              fesetenvd( OldEnvironment.d );
              return result;
              }
       
       if ( x == -1.0 ) 
              {
              OldEnvironment.i.lo |= FE_DIVBYZERO;
              x = -infinity.d;
              }
       else if ( x < -1.0 )
              {                                            // x < 0.0
              OldEnvironment.i.lo |= SET_INVALID;
              x = nan ( LOGORITHMIC_NAN );
              }
       
       fesetenvd( OldEnvironment.d );
       return x;
       }

#ifdef notdef
float log1pf ( float x )
{
    return (float)log1p( x );
}
#endif

#else /* BUILDING_FOR_CARBONCORE_LEGACY */

/*******************************************************************************
*                                                                              *
*    The base 2 logorithm function.  Caller’s rounding direction is honored.   *
*                                                                              *
*******************************************************************************/

double log2 ( double x ) 
       {
       hexdouble yInHex, xInHex, OldEnvironment;
       register double n, zTail, low, z, z2, temp1, temp2, temp3, result;
       register long int i;
       unsigned long int xhead;
       struct logTableEntry *tablePointer = ( struct logTableEntry * ) logTable;
       
       xInHex.d = x;
       fegetenvd( OldEnvironment.d );
       fesetenvd( 0.0 );
       xhead = xInHex.i.hi;
       
       if ( xhead < 0x7ff00000 ) 
              {                                             // x is finite and non-negative
              if ( xhead >= 0x000fffff ) 
                     {                                      // x is normal
                     i = xInHex.i.hi & 0x000fffff;
                     yInHex.i.lo = xInHex.i.lo;
                     yInHex.i.hi = i | 0x3ff00000;          // now 1.0 <= y < 2.0
                     if ( yInHex.i.hi & 0x00080000 ) 
                            {                               // 1.5 <= y < 2.0
                            n = (long) ( xInHex.i.hi >> 20 ) - 1022;
                            i = ( i >> 13 ) & 0x3f;         // table lookup index
                            yInHex.d *= 0.5;
                            }
                     else 
                            {                               // 1.0 <= y < 1.5
                            n = (long) ( xInHex.i.hi >> 20 ) - 1023;
                            i = ( i >> 12 ) + 64;           // table lookup index
                            }
                     }
              else if ( x != 0.0 ) 
                     { // x is nonzero, denormal
                     xInHex.d = x * twoTo128;
                     i = xInHex.i.hi & 0x000fffff;
                     yInHex.i.lo = xInHex.i.lo;
                     yInHex.i.hi = i | 0x3ff00000;          // now 1.0 <= y < 2.0
                     if ( yInHex.i.hi & 0x00080000 ) 
                            {    // 1.5 <= y < 2.0
                            n = (long) ( xInHex.i.hi >> 20 ) - 1150;
                            i = ( i >> 13 ) & 0x3f;         // table lookup index
                            yInHex.d *= 0.5;
                            }
                     else 
                            {                               // 1.0 <= y < 1.5
                            n = (long) ( xInHex.i.hi >> 20 ) - 1151;
                            i = ( i >> 12 ) + 64;           // table lookup index
                            }
                     }
              else                                          // x is 0.0
                     {
                     OldEnvironment.i.lo |= FE_DIVBYZERO;
                     result = -infinity.d;
                     fesetenvd( OldEnvironment.d );
                     return result;
                     }
              
              z = ( yInHex.d - tablePointer[i].X ) * tablePointer[i].G;
              zTail = ( ( yInHex.d - tablePointer[i].X ) - z * tablePointer[i].X ) * tablePointer[i].G;
              if ( yInHex.d != 1.0 ) 
                     OldEnvironment.i.lo |= FE_INEXACT;     // set inexact flag
              z2 = z * z;
              temp1 = d8 * z2 + d6;
              temp2 = d7 * z2 + d5;
              temp1 = temp1 * z2 + d4;
              temp2 = temp2 * z2 + d3;
              temp1 = temp1 * z + temp2;
              temp2 = temp1 * z + d2;
              temp3 = z + tablePointer[i].F.d;
              low = tablePointer[i].F.d - temp3 + z + ( zTail - z * zTail );
              temp1 = ( temp2 * z2 + low ) + temp3;
              temp2 = temp3 - temp1 + ( temp2 * z2 + low );
              temp3 = temp2 * log2e + temp1 * log2eTail;
              if ( n == 0.0 ) 
                     {
                     result =  temp3 + temp1 * log2e;
                     fesetenvd( OldEnvironment.d );
                     return result;
                     }
              else 
                     {                                      // n != 0
                     result = n + temp1 * log2e;
                     temp3 = ( ( n - result ) + temp1 * log2e ) + temp3;
                     result += temp3;
                     fesetenvd( OldEnvironment.d );
                     return result;
                     }
              }
       
       if ( x == 0.0 )
              {
              OldEnvironment.i.lo |= FE_DIVBYZERO;
              x = -infinity.d;
              }
       else if ( x < 0.0 )
              {                                             // x < 0.0
              OldEnvironment.i.lo |= SET_INVALID;
              x = nan ( LOGORITHMIC_NAN );
              }
              
       fesetenvd( OldEnvironment.d );
       return x;
       
       }

#ifdef notdef
float log2f ( float x )
{
    return (float)log2( x );
}
#endif

#endif /* BUILDING_FOR_CARBONCORE_LEGACY */

#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
