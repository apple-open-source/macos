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
*      File hypot.c,                                                           *
*      Function hypot(x,y),                                                    *
*      Implementation of sqrt(x^2+y^2) for the PowerPC.                        *
*                                                                              *
*      Copyright © 1991 Apple Computer, Inc.  All rights reserved.             *
*                                                                              *
*      Written by Ali Sazegari, started on November 1991,                      *
*                                                                              *
*      based on math.h, library code for Macintoshes with a 68881/68882        *
*      by Jim Thomas.                                                          *
*                                                                              *
*      W A R N I N G:  This routine expects a 64 bit double model.             *
*                                                                              *
*      December 03 1992: first rs6000 port.                                    *
*      January  05 1993: added the environmental controls.                     *
*      July     14 1993: added #pragma fenv_access. changed feholdexcept       *
*                        to the internal routine _feprocentry.                 *
*      September22 1993: conforming to nceg-fpce specification for ±° values.  *
*      September19 1994: revamp of the algorithm for performance.	       *
*                                                                              *
*******************************************************************************/
#ifdef      __APPLE_CC__
#if         __APPLE_CC__ > 930

#include      "math.h"
#include      "fenv.h"
#include      "fp_private.h"
#include      "fenv_private.h"

/*******************************************************************************
*            Functions needed for the computation.                             *
*******************************************************************************/

/*     the following fp.h functions are used:                                 */
/*     __inf(), fabs and sqrt;                                                */
/*     the following environmental functions are used:                        */
/*     feclearexcept, _feprocentry and feupdateenv.                           */


#pragma fenv_access on

static hexdouble Huge = HEXDOUBLE(0x7FF00000, 0x00000000);

double hypot ( double x, double y )
      {
        register double temp;
	hexdouble OldEnvironment, CurrentEnvironment;
      
/*******************************************************************************
*     If argument is SNaN then a QNaN has to be returned and the invalid       *
*     flag signaled.                                                           * 
*******************************************************************************/
	
	if ( ( x == Huge.d ) || ( y == Huge.d ) || ( x == - Huge.d ) || ( y == - Huge.d ) )
            {
            return Huge.d;
            }
                
        if ( ( x != x ) || ( y != y ) )
            {
            x = __fabs ( x + y );
            return x;
            }
            
        fegetenvd( OldEnvironment.d );              // save environment, set default
        fesetenvd( 0.0 );

        if ( ( x = __fabs ( x ) ) > ( y = __fabs ( y ) ) )  /* make sure |x| <= |y| */
            {
            temp = x;
            x = y;
            y = temp;
            }
            
        if ( ( y != 0.0 ) && ( y != __inf() ) )
            {
            temp = x / y;
            temp = sqrt ( 1.0 + temp * temp );
            fegetenvd( CurrentEnvironment.d );
            CurrentEnvironment.i.lo &= ~FE_UNDERFLOW;
            fesetenvd( CurrentEnvironment.d );
            y = y * temp;
            }
            
        fegetenvd( CurrentEnvironment.d );
        OldEnvironment.i.lo |= CurrentEnvironment.i.lo;
        fesetenvd( OldEnvironment.d );         //   restore caller's environment

        return y;
      }

#ifdef notdef
static hexsingle HugeF = { 0x7F800000 };
      
float hypotf ( float x, float y)
      {
        register float temp;
	hexdouble OldEnvironment, CurrentEnvironment;
      
/*******************************************************************************
*     If argument is SNaN then a QNaN has to be returned and the invalid       *
*     flag signaled.                                                           * 
*******************************************************************************/
	
        fegetenvd( OldEnvironment.d );               // save environment, set default
        fesetenvd( 0.0 );

	if ( ( x == HugeF.fval ) || ( y == HugeF.fval ) || ( x == - HugeF.fval ) || ( y == - HugeF.fval ) )
            {
            fegetenvd( CurrentEnvironment.d );
            OldEnvironment.i.lo |= CurrentEnvironment.i.lo;
            fesetenvd( OldEnvironment.d );         //   restore caller's environment
            return HugeF.fval;
            }
                
        if ( ( x != x ) || ( y != y ) )
            {
            x = __fabsf ( x + y );
            fegetenvd( CurrentEnvironment.d );
            OldEnvironment.i.lo |= CurrentEnvironment.i.lo;
            fesetenvd( OldEnvironment.d );         //   restore caller's environment
            return x;
            }
            
        if ( ( x = __fabsf ( x ) ) > ( y = __fabsf ( y ) ) )  /* make sure |x| <= |y| */
            {
            temp = x;
            x = y;
            y = temp;
            }
            
      if ( ( y != 0.0 ) && ( y != __inff() ) )
            {
            temp = x / y;
            temp = sqrt ( 1.0 + temp * temp );
            fegetenvd( CurrentEnvironment.d );
            CurrentEnvironment.i.lo &= ~FE_UNDERFLOW;
            fesetenvd( CurrentEnvironment.d );
            y = y * temp;
            }
            
        fegetenvd( CurrentEnvironment.d );
        OldEnvironment.i.lo |= CurrentEnvironment.i.lo;
        fesetenvd( OldEnvironment.d );         //   restore caller's environment

        return y;
      }
#endif

#else       /* __APPLE_CC__ version */
#error Version gcc-932 or higher required.  Compilation terminated.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
