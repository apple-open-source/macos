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

/****************************************************************************
   double cabs(double complex z) returns the absolute value (magnitude) of its
   complex argument z, avoiding spurious overflow, underflow, and invalid
   exceptions.  The algorithm is from Kahan's paper.
   
   CONSTANTS:  FPKSQT2 = sqrt(2.0) to double precision
               FPKR2P1 = sqrt(2.0) + 1.0 to double precision
               FPKT2P1 = sqrt(2.0) + 1.0 - FPKR2P1 to double precision, so
                  that FPKR2P1 + FPKT2P1 = sqrt(2.0) + 1.0 to double
                  double precision.
            
   Calls:  fpclassify, fabs, sqrt, feholdexcept, fesetround, feclearexcept,
           and feupdateenv.
****************************************************************************/

#include "math.h"
#include "fenv.h"

#define complex _Complex

#define Real(z) (__real__ z)
#define Imag(z) (__imag__ z)

#if defined(__BIG_ENDIAN__)
#define HEXDOUBLE(hi, lo) { { hi, lo } }
#elif defined(__LITTLE_ENDIAN__)
#define HEXDOUBLE(hi, lo) { { lo, hi } }
#else
#error Unknown endianness
#endif

static const union {              /* sqrt(2.0) */
   long int ival[2];
   double dval;
   } FPKSQT2 = HEXDOUBLE(0x3ff6a09e,0x667f3bcd);

static const union {              /* sqrt(2.0) + 1.0 to double */
   long int ival[2];
   double dval;
   } FPKR2P1 = HEXDOUBLE(0x4003504f,0x333f9de6);

static const union {              /* sqrt(2.0) + 1.0 - FPKR2P1 to double */
   long int ival[2];
   double dval;
   } FPKT2P1 = HEXDOUBLE(0x3ca21165,0xf626cdd6);

/****************************************************************************
   double cabs(double complex z) returns the absolute value (magnitude) of its
   complex argument z, avoiding spurious overflow, underflow, and invalid
   exceptions.  The algorithm is from Kahan's paper.
   
   CONSTANTS:  FPKSQT2 = sqrt(2.0) to double precision
               FPKR2P1 = sqrt(2.0) + 1.0 to double precision
               FPKT2P1 = sqrt(2.0) + 1.0 - FPKR2P1 to double precision, so
                  that FPKR2P1 + FPKT2P1 = sqrt(2.0) + 1.0 to double
                  double precision.
            
   Calls:  fpclassify, fabs, sqrt, feholdexcept, fesetround, feclearexcept,
           and feupdateenv.
****************************************************************************/

double cabs ( double complex z )
{
   double a,b,s,t;
   fenv_t env;
   int   clre,clim,ifoo;
   
   clre = fpclassify(Real(z));
   clim = fpclassify(Imag(z));
   
   if ((clre < FP_NORMAL) || (clim < FP_NORMAL)) {
      return (fabs(Real(z)) + fabs(Imag(z))); /* Real(z) or Imag(z) is NaN, INF, or zero */
   }
   
   else {                        /* both components of z are finite, nonzero */
      ifoo = feholdexcept(&env);         /* save environment, clear flags */
      ifoo = fesetround(FE_TONEAREST);   /* set default rounding */
      a = fabs(Real(z));                    /* work with absolute values */
      b = fabs(Imag(z));
      s = 0.0;
      if (a < b) {                       /* order a >= b */
         t = a;
         a = b;
         b = t;
      }
      t = a - b;                         /* magnitude difference */
      
      if (t != a) {                      /* b not negligible relative to a */
         if (t > b) {                    /* a - b > b */
            s = a/b;
            s += sqrt(1.0 + s*s);
         }
         else {                          /* a - b <= b */
            s = t/b;
            t = (2.0 + s)*s;
            s = ((FPKT2P1.dval+t/(FPKSQT2.dval+sqrt(2.0+t)))+s)+FPKR2P1.dval;
         }
         
         s = b/s;                        /* may spuriously underflow */
         feclearexcept(FE_UNDERFLOW);
      }
      
      feupdateenv(&env);                 /* restore environment */
      return (a + s);                    /* deserved overflow occurs here */
   }                                     /* finite, nonzero case */
}   

