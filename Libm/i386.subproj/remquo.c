/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/********************************************************************************
*     File: remquo.c                                                            *
*                                                                               *
*     Contains: C source code for implementations of some floating-point        *
*                functions defined in header <fp.h>.  In particular, this       *
*                file contains implementations of functions fmod, remainder,    *
*                and remquo.                                                    *
*                                                                               *
*     Copyright © 1992-2001 by Apple Computer, Inc. All rights reserved.        *
*                                                                               *
*     Written by Jon Okada, started on December 7th, 1992.                      *
*     Modified by Paul Finlayson (PAF) for MathLib v2.                          *
*     Modified by A. Sazegari (ali) for MathLib v3.                             *
*     Modified and ported by Robert A. Murley (ram) for Mac OS X.               *
*                                                                               *
*     A MathLib v4 file.                                                        *
*                                                                               *
*     Change History (most recent first):                                       *
*                                                                               *
*        08 Nov 01   ram  renamed remquo to avoid conflict with CarbonCore.     *
*        06 Nov 01   ram  commented out warning about Intel architectures.      *
*                         changed i386 stub to call abort().                    *
*        02 Nov 01   ram  added stub for i386 version of remquo.                *
*        08 Oct 01   ram  removed <CoreServices/CoreServices.h>.                *
*                         changed compiler errors to warnings.                  *
*        18 Sep 01   ali  added <CoreServices/CoreServices.h> to get <fp.h>.    *
*        17 Sep 01   ali  replaced "fp.h" & "fenv.h" with <fp.h> & <fenv.h>.    *
*        13 Sep 01   ali  replaced double_t by double.                          *
*        10 Sep 01   ali  added more comments.                                  *
*        09 Sep 01   ali  added macros to detect PowerPC and correct compiler.  *
*        06 Sep 01   ram  added #ifdef __ppc__.                                 *
*        16 Jul 01   ram  Replaced __setflm with FEGETENVD/FESETENVD.           *
*                          replaced DblInHex typedef with hexdouble.            *
*        09 Oct 94   ali  made environmental changes to use __setflm            *
*                         instead of _feprocentry.                              *
*        05 Oct 93   PAF  Fixed rounding sensitivity and flag errors.           *
*        14 Dec 92   JPO  Fixed case where |x| = |y|.                           *
*        11 Dec 92   JPO  Fixed bug that created overflow for |x| in            *
*                         highest binade.                                       *
*        07 Dec 92   JPO  First created.                                        *
*                                                                               *
*     W A R N I N G:                                                            *
*     These routines require a 64-bit double precision IEEE-754 model.          *
*     They are written for PowerPC only and are expecting the compiler          *
*     to generate the correct sequence of multiply-add fused instructions.      *
*                                                                               *
*     These routines are not intended for 32-bit Intel architectures.           *
*                                                                               *
*     A version of gcc higher than 932 is required.                             *
*                                                                               *
*     GCC compiler options:                                                     *
*           optimization level 3 (-O3)                                          *
*           -fschedule-insns -finline-functions -funroll-all-loops              *
*                                                                               *
********************************************************************************/

#ifdef      __APPLE_CC__
#if         __APPLE_CC__ > 930

#include      "math.h"
#include      "math_private.h"
#include      "fenv.h"
#include      "fp_private.h"

#define      REM_NAN      "9"
static const double zero	 = 0.0;
static const hexdouble Huge     = HEXDOUBLE(0x7ff00000, 0x00000000);
static const hexdouble HugeHalved     = HEXDOUBLE(0x7fe00000, 0x00000000);
static const hexsingle HugeF     = { 0x7f800000 };
static const hexsingle HugeFHalved     = { 0x7f000000 };

/***********************************************************************
   The function remquo returns the IEEE-mandated floating-point remainder
   of its floating-point arguments x and y:  x REM y.  It also calculates
   the low seven bits of the integral quotient and writes the signed
   low quotient result to the location pointed to by the int pointer
   argument, quo:  -127 <= iquo <= +127.
   
   This function calls:  __fpclassifyd, logb, scalbn, __FABS, signbitd.
***********************************************************************/

#if defined(BUILDING_FOR_CARBONCORE_LEGACY)

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

extern double __logb ( double x );

static const double twoTo1023  = 8.988465674311579539e307;   // 0x1p1023
static const double twoToM1022 = 2.225073858507201383e-308;  // 0x1p-1022

static double __scalbn ( double x, int n  )
{
      hexdouble xInHex;
      
      xInHex.i.lo = 0UL;                        // init. low half of xInHex
      
      if ( n > 1023 ) 
       {                                        // large positive scaling
            if ( n > 2097 )                     // huge scaling
                   return ( ( x * twoTo1023 ) * twoTo1023 ) * twoTo1023;
            while ( n > 1023 ) 
              {                                 // scale reduction loop
                  x *= twoTo1023;               // scale x by 2^1023
                  n -= 1023;                    // reduce n by 1023
              }
       }
      
      else if ( n < -1022 ) 
       {                                        // large negative scaling
            if ( n < -2098 )                    // huge negative scaling
                   return ( ( x * twoToM1022 ) * twoToM1022 ) * twoToM1022;
            while ( n < -1022 ) 
              {                                 // scale reduction loop
                  x *= twoToM1022;              // scale x by 2^( -1022 )
                  n += 1022;                    // incr n by 1022
              }
       }

/*******************************************************************************
*      -1022 <= n <= 1023; convert n to double scale factor.                   *
*******************************************************************************/

      xInHex.i.hi = ( ( unsigned long ) ( n + 1023 ) ) << 20;
      return ( x * xInHex.d );
}

static long int ___signbitd ( double arg )
{
      hexdouble z;

      z.d = arg;
      return (((signed long int)z.i.hi) < 0);
}

double remquo ( double x, double y, int *quo)
{
      long int      iclx,icly;                        /* classify results of x,y */
      long int      iquo;                             /* low 32 bits of integral quotient */
      long int      iscx, iscy, idiff;                /* logb values and difference */
      long int      i;                                /* loop variable */
      double        absy,x1,y1,z;                     /* local floating-point variables */
      double        rslt;
      fenv_t 	    OldEnvironment;
    
      (void) feholdexcept( &OldEnvironment );
      fesetenv( FE_DFL_ENV );
      
      *quo = 0;                                       /* initialize quotient result */
      iclx = ___fpclassifyd(x);
      icly = ___fpclassifyd(y);
      if ((iclx & icly) >= FP_NORMAL)    {            /* x,y both nonzero finite case */
         x1 = __FABS(x);                              /* work with absolute values */
         absy = __FABS(y);
         iquo = 0;                                    /* zero local quotient */
         iscx = (long int) __logb(x1);                /* get binary exponents */
         iscy = (long int) __logb(absy);
         idiff = iscx - iscy;                         /* exponent difference */
         if (idiff >= 0) {                            /* exponent of x1 >= exponent of y1 */
              if (idiff != 0) {                       /* exponent of x1 > exponent of y1 */
                   y1 = __scalbn(absy,-iscy);         /* scale |y| to unit binade */
                   x1 = __scalbn(x1,-iscx);           /* ditto for |x| */
                   for (i = idiff; i != 0; i--) {     /* begin remainder loop */
                        if ((z = x1 - y1) >= 0) {     /* nonzero remainder step result */
                            x1 = z;                   /* update remainder (x1) */
                            iquo += 1;                /* increment quotient */
                        }
                        iquo += iquo;                 /* shift quotient left one bit */
                        x1 += x1;                     /* shift (double) remainder */
                   }                                  /* end of remainder loop */
                   x1 = __scalbn(x1,iscy);            /* scale remainder to binade of |y| */
              }                                       /* remainder has exponent <= exponent of y */
              if (x1 >= absy) {                       /* last remainder step */
                   x1 -= absy;
                   iquo +=1;
              }                                       /* end of last remainder step */
         }                                            /* remainder (x1) has smaller exponent than y */
         if ( x1 < HugeHalved.d )
            z = x1 + x1;                              /* double remainder, without overflow */
         else
            z = Huge.d;
         if ((z > absy) || ((z == absy) && ((iquo & 1) != 0))) {
              x1 -= absy;                             /* final remainder correction */
              iquo += 1;
         }
         if (x < 0.0)
              x1 = -x1;                               /* remainder if x is negative */
         iquo &= 0x0000007f;                          /* retain low 7 bits of integer quotient */
         if ((___signbitd(x) ^ ___signbitd(y)) != 0)  /* take care of sign of quotient */
              iquo = -iquo;
         *quo = iquo;                                 /* deliver quotient result */
         rslt = x1;
    }                                                 /* end of x,y both nonzero finite case */
    else if ((iclx <= FP_QNAN) || (icly <= FP_QNAN)) {
         rslt = x+y;                                  /* at least one NaN operand */
    }
    else if ((iclx == FP_INFINITE)||(icly == FP_ZERO)) {    /* invalid result */
         rslt = nan(REM_NAN);
    }
    else                                              /* trivial cases (finite REM infinite   */
         rslt = x;                                    /*  or  zero REM nonzero) with *quo = 0 */

    feupdateenv( &OldEnvironment ); //   restore caller's environment
    
    if ((iclx == FP_INFINITE)||(icly == FP_ZERO)) {
         if (!(*quo + zero/zero > zero)) // always true, INVALID as side effect
            return rslt;
    }
    return rslt;
}

#else /* !BUILDING_FOR_CARBONCORE_LEGACY */

float remquof ( float x, float y, int *quo)
{
      long int      iclx,icly;                        /* classify results of x,y */
      long int      iquo;                             /* low 32 bits of integral quotient */
      long int      iscx, iscy, idiff;                /* logb values and difference */
      long int      i;                                /* loop variable */
      float        absy,x1,y1,z;                     /* local floating-point variables */
      float        rslt;
      fenv_t 	    OldEnvironment;
    
      (void) feholdexcept( &OldEnvironment );
      fesetenv( FE_DFL_ENV );
      
      *quo = 0;                                       /* initialize quotient result */
      iclx = __fpclassifyf(x);
      icly = __fpclassifyf(y);
      if ((iclx & icly) >= FP_NORMAL)    {            /* x,y both nonzero finite case */
         x1 = __FABSF(x);                              /* work with absolute values */
         absy = __FABSF(y);
         iquo = 0;                                    /* zero local quotient */
         iscx = (long int) logbf(x1);                  /* get binary exponents */
         iscy = (long int) logbf(absy);
         idiff = iscx - iscy;                         /* exponent difference */
         if (idiff >= 0) {                            /* exponent of x1 >= exponent of y1 */
              if (idiff != 0) {                       /* exponent of x1 > exponent of y1 */
                   y1 = scalbnf(absy,-iscy);            /* scale |y| to unit binade */
                   x1 = scalbnf(x1,-iscx);              /* ditto for |x| */
                   for (i = idiff; i != 0; i--) {     /* begin remainder loop */
                        if ((z = x1 - y1) >= 0) {     /* nonzero remainder step result */
                            x1 = z;                   /* update remainder (x1) */
                            iquo += 1;                /* increment quotient */
                        }
                        iquo += iquo;                 /* shift quotient left one bit */
                        x1 += x1;                     /* shift (double) remainder */
                   }                                  /* end of remainder loop */
                   x1 = scalbnf(x1,iscy);               /* scale remainder to binade of |y| */
              }                                       /* remainder has exponent <= exponent of y */
              if (x1 >= absy) {                       /* last remainder step */
                   x1 -= absy;
                   iquo +=1;
              }                                       /* end of last remainder step */
         }                                            /* remainder (x1) has smaller exponent than y */
         if ( x1 < HugeFHalved.fval )
            z = x1 + x1;                              /* double remainder, without overflow */
         else
            z = HugeF.fval;
         if ((z > absy) || ((z == absy) && ((iquo & 1) != 0))) {
              x1 -= absy;                             /* final remainder correction */
              iquo += 1;
         }
         if (x < 0.0)
              x1 = -x1;                               /* remainder if x is negative */
         iquo &= 0x0000007f;                          /* retain low 7 bits of integer quotient */
         if ((__signbitf(x) ^ __signbitf(y)) != 0)    /* take care of sign of quotient */
              iquo = -iquo;
         *quo = iquo;                                 /* deliver quotient result */
         rslt = x1;
    }                                                 /* end of x,y both nonzero finite case */
    else if ((iclx <= FP_QNAN) || (icly <= FP_QNAN)) {
         rslt = x+y;                                  /* at least one NaN operand */
    }
    else if ((iclx == FP_INFINITE)||(icly == FP_ZERO)) {    /* invalid result */
         rslt = nanf(REM_NAN);
    }
    else                                              /* trivial cases (finite REM infinite   */
         rslt = x;                                    /*  or  zero REM nonzero) with *quo = 0 */

    feupdateenv( &OldEnvironment ); //   restore caller's environment
    
    if ((iclx == FP_INFINITE)||(icly == FP_ZERO)) {
         if (!(*quo + zero/zero > zero)) // always true, INVALID as side effect
            return rslt;
    }
    return rslt;
}

#endif /* BUILDING_FOR_CARBONCORE_LEGACY */

#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
