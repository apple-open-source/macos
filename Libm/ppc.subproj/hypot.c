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
/*     fabs and sqrt;                                                	      */
/*     the following environmental functions are used:                        */
/*     feclearexcept, _feprocentry and feupdateenv.                           */


#pragma fenv_access on

static const hexdouble Huge = HEXDOUBLE(0x7FF00000, 0x00000000);

#ifdef notdef
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
            x = __FABS ( x + y );
            return x;
            }
            
        FEGETENVD( OldEnvironment.d );              // save environment, set default
        FESETENVD( 0.0 );

        if ( ( x = __FABS ( x ) ) > ( y = __FABS ( y ) ) )  /* make sure |x| <= |y| */
            {
            temp = x;
            x = y;
            y = temp;
            }
            
        if ( ( y != 0.0 ) && ( y != INFINITY ) )
            {
            temp = x / y;
            temp = sqrt ( 1.0 + temp * temp );
            FEGETENVD( CurrentEnvironment.d );
            CurrentEnvironment.i.lo &= ~FE_UNDERFLOW;
            FESETENVD( CurrentEnvironment.d );
            y = y * temp;
            }
            
        FEGETENVD( CurrentEnvironment.d );
        OldEnvironment.i.lo |= CurrentEnvironment.i.lo;
        FESETENVD( OldEnvironment.d );         //   restore caller's environment

        return y;
      }
#else

static const hexdouble NegHuge = HEXDOUBLE(0xFFF00000, 0x00000000);

double hypot ( double x, double y )
{
        register double temp;
	hexdouble OldEnvironment, CurrentEnvironment;
        
        register double FPR_env, FPR_z, FPR_one, FPR_inf, FPR_Minf, 
                        FPR_absx, FPR_absy, FPR_big, FPR_small;
        
        FPR_z = 0.0;					FPR_one = 1.0;
        FPR_inf = Huge.d;				FPR_Minf = NegHuge.d;
        FPR_absx = __FABS( x );				FPR_absy = __FABS( y );
      
/*******************************************************************************
*     If argument is SNaN then a QNaN has to be returned and the invalid       *
*     flag signaled.                                                           * 
*******************************************************************************/
	
	if ( ( x == FPR_inf ) || ( y == FPR_inf ) || ( x == FPR_Minf ) || ( y == FPR_Minf ) )
            return FPR_inf;
                
        if ( ( x != x ) || ( y != y ) )
        {
            x = __FABS ( x + y );
            return x;
        }
            
        if ( FPR_absx > FPR_absy )
        {
            FPR_big = FPR_absx;
            FPR_small = FPR_absy;
        }
        else
        {
            FPR_big = FPR_absy;
            FPR_small = FPR_absx;
        }
        
        // Now +0.0 <= FPR_small <= FPR_big < INFINITY
        
        if ( FPR_small == FPR_z )
            return FPR_big;
            
        FEGETENVD( FPR_env );				// save environment, set default
        FESETENVD( FPR_z );

        temp = FPR_small / FPR_big;			OldEnvironment.d = FPR_env;
        temp = sqrt ( FPR_one + temp * temp );	   
        
        FEGETENVD( CurrentEnvironment.d );
        CurrentEnvironment.i.lo &= ~FE_UNDERFLOW;	// Clear any inconsequential underflow
        FESETENVD( CurrentEnvironment.d );
        
        temp = FPR_big * temp;				// Might raise UNDERFLOW or OVERFLOW
            
        FEGETENVD( CurrentEnvironment.d );
        OldEnvironment.i.lo |= CurrentEnvironment.i.lo; // Pick up any UF or OF
        FESETENVD( OldEnvironment.d );        		// restore caller's environment

        return temp;
}
#endif


#else       /* __APPLE_CC__ version */
#error Version gcc-932 or higher required.  Compilation terminated.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
