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
*     File asinacos.c,                                                         *
*     Implementation of asin and acos for the PowerPC.                         *
*                                                                              *
*     Copyright © 1991-2001 Apple Computer, Inc.  All rights reserved.         *
*                                                                              *
*     Written and ported by Robert A. Murley (ram) for Mac OS X.               *
*     Modified by A. Sazegari, September 2001.                                 *
*                                                                              *
*     A MathLib v4 file.                                                       *
*                                                                              *
*     Based on the trigonometric functions from IBM/Taligent.                  *
*                                                                              *
*     July	20 2001: replaced __setflm with fegetenvd/fesetenvd.           *
*                        replaced DblInHex typedef with hexdouble.             *
*     September 07 2001: added #ifdef __ppc__.                                 *
*     September 09 2001: added more comments.                                  *
*     September 10 2001: added macros to detect PowerPC and correct compiler.  *
*     September 21 2001: added <CoreServices/CoreServices.h> to get to <fp.h>  *
*                        and <fenv.h>.                                         *
*     September 24 2001: used standard symbols for environment flags.          *
*     September 25 2001: added fesetenvd(0.0) at start of acos.                *
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
*******************************************************************************/

#ifdef      __APPLE_CC__
#if         __APPLE_CC__ > 930

#include    "fp_private.h"
#include    "fenv_private.h"

extern const unsigned short SqrtTable[];

// Floating-point constants
static const double k2ToM26    = 1.490116119384765625e-8;     // 0x1.0p-26;
static const double kPiBy2     = 1.570796326794896619231322;  // 0x1.921fb54442d18p0
static const double kPiBy2Tail = 6.1232339957367660e-17;      // 0x1.1a62633145c07p-54
static const double kMinNormal = 2.2250738585072014e-308;     // 0x1.0p-1022

const static double a14  = 0.03085091303188211304259;
const static double a13  =-0.02425125216179617744614;
const static double a12  = 0.02273334623573271023373;
const static double a11  = 0.0002983797813053653120360;
const static double a10  = 0.008819738782817955152102;
const static double a9   = 0.008130738583790024803650;
const static double a8   = 0.009793486386035222577675;
const static double a7   = 0.01154901189590083099584;
const static double a6   = 0.01396501522140471126730;
const static double a5   = 0.01735275722383019335276;
const static double a4   = 0.02237215928757573539490;
const static double a3   = 0.03038194444121688698408;
const static double a2   = 0.04464285714288482921087;
const static double a1   = 0.07499999999999990640378;
const static double a0   = 0.1666666666666666667191;
const static double aa11 = 0.0002983797813053653120360;
const static double aa10 = 0.008819738782817955152102;
const static double aa9  = 0.008130738583790024803650;
const static double aa8  = 0.009793486386035222577675;
const static double aa7  = 0.01154901189590083099584;
const static double aa6  = 0.01396501522140471126730;
const static double aa5  = 0.01735275722383019335276;
const static double aa4  = 0.02237215928757573539490;
const static double aa3  = 0.03038194444121688698408;
const static double aa2  = 0.04464285714288482921087;
const static double aa1  = 0.07499999999999990640378;
const static double aa0  = 0.1666666666666666667191;

static const hexdouble kInfinityu = HEXDOUBLE(0x7ff00000, 0x00000000);
static const hexdouble kPiu       = HEXDOUBLE(0x400921fb, 0x54442d18);

#define      INVERSE_TRIGONOMETRIC_NAN      "34"

/****************************************************************************

FUNCTION:  double asin (double x)
    
DESCRIPTION:  returns the inverse sine of its argument in the range of
              -pi/2 to pi/2 (radians).
    
CALLS:  __fabs
    
EXCEPTIONS raised:  INEXACT, UNDERFLOW/INEXACT or INVALID.
    
ACCURACY:  Error is less than an ulp and usually less than half an ulp.
           Caller's rounding direction is honored.

****************************************************************************/

double asin (double x)
{
    hexdouble     cD, yD ,gD ,fpenv;
    double        absx, x2, x3, x4, temp1, temp2, u, v;
    double        g, y, d, y2, e;
    int           index;
    unsigned long ghead, yhead, rnddir;
    
    if (x != x)                            // NaN argument
        return ( copysign ( nan ( INVERSE_TRIGONOMETRIC_NAN ), x ) );
        
    absx = __fabs(x);
    fegetenvd (fpenv.d);                   // save env, set default
    fesetenvd (0.0);
    cD.d = 0.5 - 0.5*absx;
    x2 = x*x;                              // under/overflows are masked
    
    if (absx <= 0.5)
    {   // |x| <= 0.5
        if (absx < k2ToM26)
        {   // |x| < 2^(-26)
            if (absx == 0.0)
            {                              // x == 0.0 case is exact
                fesetenvd (fpenv.d);
                return (x);
            }

            rnddir = fpenv.i.lo & 0x3ul;   // caller's rounding direction
            fpenv.i.lo |= FE_INEXACT;      // set INEXACT
            yD.d = x;
    
            // Rounding away from zero:  return NextDouble(x,x+x)
            if (((rnddir == 3ul) && (x < 0.0)) || ((rnddir == 2) && (x > 0.0)))
            {
                yD.i.lo += 1ul;
                if (yD.i.lo == 0ul)
                    yD.i.hi += 1ul;
                absx = __fabs(yD.d);
            }

            // Set UNDERFLOW if result is subnormal        
            if (absx < kMinNormal)             // subnormal case
                fpenv.i.lo |= FE_UNDERFLOW;    //   set UNDERFLOW
            fesetenvd (fpenv.d);               // restore caller environment
            return (yD.d);                     // return small result
        }   // [|x| < 2^(-26)]
                
        temp1 = a14*x2 + a13;
        x4 = x2*x2;                            // polynomial approx for ArcSin
        fpenv.i.lo |= FE_INEXACT;              // result is inexact        
            temp1 = temp1*x4 + a11;
        temp2 = a12*x4 + a10;
        temp1 = temp1*x4 + a9;
        temp2 = temp2*x4 + a8;
        temp1 = temp1*x4 + a7;
        temp2 = temp2*x4 + a6;
        temp1 = temp1*x4 + a5;
        temp2 = temp2*x4 + a4;
        temp1 = temp1*x4 + a3;
        temp2 = temp2*x4 + a2;
        temp1 = temp1*x4 + a1;
        x3 = x*x2;
        temp1 = a0 + x2*(temp2*x2 + temp1);
        fesetenvd (fpenv.d);                   // restore caller's mode
        return (x + x3*temp1);
    }   // [|x| <= 0.5]
    
/****************************************************************************
    For 0.5 < |x| < 1.0, a polynomial approximation is used to estimate
    the power series resulting from the identity:
       ArcSin(x) = pi/2 - 2*ArcSin(Sqrt(0.5(1.0 - x))).
    The square root is evaluated in-line and concurrently with the
    evaluation of the ArcSin.
 ***************************************************************************/

    if (absx < 1.0)
    {   // |x| < 1.0
        x2 = cD.d;
        x4 = x2*x2;
        temp2 = (((((a14*x4+a12)*x4+a10)*x4+a8)*x4+aa6)*x4+aa4)*x4+aa2;
        temp1 = (((((a13*x4+a11)*x4+a9)*x4+aa7)*x4+aa5)*x4+aa3)*x4+aa1;

        ghead = ((cD.i.hi + 0x3ff00000ul) >> 1) & 0x7ff00000ul;
        index = (cD.i.hi >> 13) & 0xfful;
        yhead = 0x7fc00000ul - ghead;
        temp1 = temp1 + x2*temp2;
        gD.i.hi = ghead + ((0xff00ul & SqrtTable[index]) << 4);
        gD.i.lo = 0ul;
        yD.i.hi = yhead + ((0xfful & SqrtTable[index]) << 12);
        yD.i.lo = 0ul;
        d = cD.d - gD.d*gD.d;
                                    
        y = yD.d;                   // initial 0.5/Sqrt guess
        g = gD.d + y*d;
        y2 = y + y;                 // 16-bit g
        e = 0.5 - y*g;
        d = cD.d - g*g;
        y = y + e*y2;               // 16-bit y
        g = g + y*d;                // 32-bit g
        y2 = y + y;
        e = 0.5 - y*g;
        d = cD.d - g*g;
        y = y + e*y2;               // 32-bit y
        g = g + y*d;                // 64-bit Sqrt
        d = (cD.d - g*g)*y;         // tail of 0.5*Sqrt (32 bits)
        u = kPiBy2 - 2.0*g;         // pi/2 - 2.0*Sqrt (head)
        temp1 = aa0 + x2*temp1;
        v = (kPiBy2 - u) - 2.0*g;   // pi/2 - 2.0*Sqrt (tail)
        g *= x2;
        d = d - 0.5*kPiBy2Tail;
        fpenv.i.lo |= FE_INEXACT;   // result is inexact
        temp2 = g*temp1 + (d - 0.5*v);
    
        fesetenvd (fpenv.d);        // restore caller's mode
        if (x > 0.0)
        {                           // take care of sign of result
            temp2 = -temp2;
            return (2.0*temp2 + u);
        }
        else
            return (2.0*temp2 - u);
    }   // [|x| < 1.0]
    
    fesetenvd (fpenv.d);            // restore caller's mode
    
    // If |x| == 1.0, return properly signed pi/2.
    if (absx == 1.0)
    {
        if (x > 0.0)
            return (kPiBy2 + x*kPiBy2Tail);
        else
            return (-kPiBy2 + x*kPiBy2Tail);
    }
    
    // |x| > 1.0 signals INVALID and returns a NaN
    fpenv.i.lo |= SET_INVALID;
    fesetenvd (fpenv.d);
    return ( nan ( INVERSE_TRIGONOMETRIC_NAN ) );
}   // ArcSin(x)

#ifdef notdef
float asinf( float x )
{
    return (float) asin ( x );
}
#endif

/****************************************************************************

FUNCTION:  double ArcCos(double x)
    
DESCRIPTION:  returns the inverse cosine of its argument in the range of
              0 to pi (radians).
    
CALLS:  __fabs
    
EXCEPTIONS raised:  INEXACT or INVALID.
    
ACCURACY:  Error is less than an ulp and usually less than half an ulp.
           Caller's rounding direction is honored.

****************************************************************************/

double acos(double x)
{
    hexdouble     cD, yD, gD, fpenv;
    double        absx, x2, x3, x4, temp1, temp2, s, u, v, w;
    double        g, y, d, y2, e;
    int           index;
    unsigned long ghead,yhead;

    if (x != x)                             // NaN
        return ( copysign ( nan ( INVERSE_TRIGONOMETRIC_NAN ), x ) );
        
    absx = __fabs(x);
    fegetenvd (fpenv.d);                   // save env, set default
    fesetenvd (0.0);
    cD.d = 0.5 - 0.5*absx;
    x2 = x*x;

    if (absx <= 0.5)
    {   // |x| <= 0.5
        temp1 = a14*x2 + a13;
        x4 = x2*x2;
        fpenv.i.lo |= FE_INEXACT;	// INEXACT result
        temp1 = temp1*x4 + a11;
        temp2 = a12*x4 + a10;
        temp1 = temp1*x4 + a9;
        temp2 = temp2*x4 + a8;
        temp1 = temp1*x4 + a7;
        temp2 = temp2*x4 + a6;
        temp1 = temp1*x4 + a5;
        temp2 = temp2*x4 + a4;
        temp1 = temp1*x4 + a3;
        temp2 = temp2*x4 + a2;
        temp1 = temp1*x4 + a1;
        
        // finish up ArcCos(x) = pi/2 - ArcSin(x) 
        u = kPiBy2 - x;                     // pi/2 - x (high-order term)
        temp1 = temp1 + x2*temp2;
        v = kPiBy2 - u - x;                 // pi/2 - x (tail)
        x3 = x*x2;
        temp1 = a0 + x2*temp1;
        w = (v + kPiBy2Tail) - x3*temp1;    // combine low order terms
        fesetenvd (fpenv.d);                // restore caller's mode
        return (u+w);
    }   // [|x| <= 0.5]
    
/****************************************************************************
    For 0.5 < |x| < 1.0, a polynomial approximation is used to estimate
    the power series resulting from the identity:
      ArcSin(x) = pi/2 - 2*ArcSin(Sqrt(0.5(1.0 - x))).
    The square root is evaluated in-line and concurrently with the
    evaluation of the ArcSin.
 ****************************************************************************/
    if (absx < 1.0) 
    {   // 0.5 < |x| < 1.0
        x2 = cD.d;                          // 0.5*(1.0 - |x|)
        x4 = x2*x2;
        fpenv.i.lo |= FE_INEXACT;           // INEXACT result
        temp2 = a14*x4 + a12;
        temp1 = a13*x4 + aa11;
        temp2 = temp2*x4 + aa10;
        temp1 = temp1*x4 + aa9;
        temp2 = temp2*x4 + aa8;
        temp1 = temp1*x4 + aa7;
        temp2 = temp2*x4 + aa6;
        temp1 = temp1*x4 + aa5;
        temp2 = temp2*x4 + aa4;
        temp1 = temp1*x4 + aa3;
        temp2 = temp2*x4 + aa2;
        temp1 = temp1*x4 + aa1;
        ghead = ((cD.i.hi + 0x3ff00000ul) >> 1) & 0x7ff00000ul;
        index = (cD.i.hi >> 13) & 0xfful;
        yhead = 0x7fc00000ul - ghead;
        temp1 = temp1 + x2*temp2;           // collect polynomial terms
        gD.i.hi = ghead + ((0xff00ul & SqrtTable[index]) << 4);
        gD.i.lo = 0ul;
        yD.i.hi = yhead + ((0xfful & SqrtTable[index]) << 12);
        yD.i.lo = 0ul;
        d = x2 - gD.d*gD.d;
        y = yD.d;                           // initial 0.5/Sqrt guess
        g = gD.d + y*d;                     // 16-bit g
        y2 = y + y;
        e = 0.5 - y*g;
        d = x2 - g*g;
        y = y + e*y2;                       // 16-bit y
        g = g + y*d;                        // 32-bit g
        y2 = y + y;
        e = 0.5 - y*g;
        d = x2 - g*g;
        y = y + e*y2;                       // 32-bit y
        g = g + y*d;                        // 64-bit Sqrt
        d = (x2 - g*g)*y;                   // tail of 0.5*Sqrt (32 bits)
        y2 = g + g;                         // 2*Sqrt
        
        if (x > 0.0)
        {   // positive x
            s = (g*x2)*(aa0 + x2*temp1) + d;    // low order terms
            fesetenvd (fpenv.d);                // restore caller's mode
            return (y2 + 2.0*s);                // combine terms
        }   // [positive x]
        else
        {   // negative x
            u = 2.0*kPiBy2 - y2;            // high-order term
            v = 2.0*kPiBy2 - u - y2;        // tail correction
            s = (g*x2)*(aa0 + x2*temp1) + ((d - kPiBy2Tail) - 0.5*v);    // collect low-order terms
            fesetenvd (fpenv.d);            // restore caller's mode
            s = -s;
            return (u + 2.0*s);             // combine terms
        }   // [negative x]
    }   // [0.5 < |x| < 1.0]


    // |x| == 1.0:  return exact +0.0 or inexact pi
    if (x == 1.0)
    {
        fesetenvd (fpenv.d);            // restore caller's mode
        return (0.0);
    }
    
    if (x == -1.0)
    {
        fpenv.i.lo |= FE_INEXACT;
        fesetenvd (fpenv.d);            // restore caller's mode
        return (kPiu.d + 2.0*kPiBy2Tail);
    }
    
    // |x| > 1.0 signals INVALID and returns a NaN
    fpenv.i.lo |= SET_INVALID;
    fesetenvd (fpenv.d);
    return ( nan ( INVERSE_TRIGONOMETRIC_NAN ) );
}   // ArcCos(x)

#ifdef notdef
float acosf( float x )
{
    return (float) acos ( x );
}
#endif

#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
