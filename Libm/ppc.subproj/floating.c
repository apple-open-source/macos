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

#include "math.h"
#include "fenv.h"
#include "fp_private.h"
#include "fenv_private.h"

float acosf( float x ) 	{ return (float)acos((double)( x )); }
float asinf( float x ) 	{ return (float)asin((double)( x )); }
float atanf( float x ) 	{ return (float)atan( (double)x ); }
float atan2f( float y, float x) { return (float)atan2( (double)y, (double)x ); }
float cosf( float x ) 	{ return (float)cos((double)( x )); }
float sinf( float x) 	{ return (float)sin( (double)x ); }
float tanf( float x ) 	{ return (float)tan( (double)x ); }
float acoshf( float x )	{ return (float)acosh( (double)x ); }
float asinhf( float x )	{ return (float)asinh( (double)x ); }
float atanhf( float x )	{ return (float)atanh( (double)x ); }
float coshf( float x)	{ return (float)cosh( (double)x ); }
float sinhf( float x)	{ return (float)sinh( (double)x ); }
float tanhf( float x)	{ return (float)tanh( (double)x ); }

float expf( float x)	{ return (float)exp( (double)x ); }
float exp2f( float x)	{ return (float)exp2( (double)x ); }
float expm1f( float x )	{ return (float)expm1( (double)x ); }
float logf ( float x )	{ return (float)log( (double)x ); }
float log10f ( float x ){ return (float)log10( (double)x ); }
float log2f ( float x )	{ return (float)log2( x ); }
float log1pf ( float x ){ return (float)log1p( (double)x ); }

float cbrtf( float x ) 	{ return (float)cbrt((double)( x )); }
float powf ( float x, float y )	{ return (float)pow ( (double)x, (double)y ); }

float erff( float x ) 	{ return (float)erf((double)( x )); }
float erfcf( float x ) 	{ return (float)erfc((double)( x )); }
float lgammaf( float x ){ return (float)lgamma((double)( x )); }
float tgammaf( float x ){ return (float)tgamma((double)( x )); }

#ifdef notdef
/*******************************************************************************
*     									       *
*     Single Precision Implementation.					       *
*     									       *
*******************************************************************************/

#undef	     upHalfOfAnULP
#define      upHalfOfAnULP      0.50000006         	 // 0x1.000002p-1
#define      twoTo64            0.1844674407370955e20    // 2**64    
#define      twoToMinus32      	0.2328306436538696e-9    // 2**-32

float sqrtf ( float x )
      {
      register int index;
      hexdouble OldEnvironment;
      hexsingle xInHex, yInHex, gInHex;
      register float g, y, y2, d, e;
      register unsigned long int xhead, ghead, yhead;
            
      xInHex.fval = x;
      xhead = xInHex.lval;                         // 32 bits of x
      FEGETENVD( OldEnvironment.d );               // save environment, set default
      FESETENVD( 0.0 );
    
/*******************************************************************************
*     ∞ > x ≥ 0.0.  This section includes +0.0, but not -0.0.                  *
*******************************************************************************/

      if ( xhead < 0x7f800000 ) 
            {

/*******************************************************************************
*     First and most common section: argument > 2.0^(-76), about 0.1323488e-22.*
*******************************************************************************/
            
            if ( xhead > 0x1a000000ul )
                  {

/*******************************************************************************
*     Calculate initial estimates for g and y from x and SqrtTable[].          *
*******************************************************************************/

                  ghead = ( ( xhead + 0x3f800000 ) >> 1 ) & 0x7f800000;
                  index = ( xhead >> 16 ) & 0xffUL; // table index
                  yhead = 0x7e000000UL - ghead;
                  gInHex.lval = ghead + ( ( 0xff00UL & SqrtTable[index] ) << 7 );
                  yInHex.lval = yhead + ( ( 0xffUL & SqrtTable[index] ) << 15 );
                  g = gInHex.fval;
                  y = yInHex.fval;
      
/*******************************************************************************
*     Iterate to refine both g and y.                                          *
*******************************************************************************/

                  d = x - g * g;
                  y2 = y + y;
                  g = g + y * d;                   //   16-bit g
                  e = upHalfOfAnULP - y * g;
                  d = x - g * g;
                  y = y + e * y2;                  //   16-bit y
                  g = g + y * d;                   //   32-bit g before rounding
                  y2 = y + y;
                  e = upHalfOfAnULP - y * g;
                  d = x - g * g;
                  y = y + e * y2;                  //   32-bit y
                  FESETENVD( OldEnvironment.d );   //   restore caller's environment
                  return ( g + y * d );            //   final step
                  }

/*******************************************************************************
*     Second section: 0.0 < argument < 2.0^(-76) which is about 0.1323488e-22. *
*     Identical to the previous segment aside from 2^64 scale factor.          *
*******************************************************************************/

            if ( xhead != 0UL ) 
                  { 
                  xInHex.fval = x * twoTo64;         //   scale up by 2^64
                  xhead = xInHex.lval;

/*******************************************************************************
*     Calculate initial estimates for g and y from x and SqrtTable[].          *
*******************************************************************************/

                  ghead = ( ( xhead + 0x3f800000 ) >> 1 ) & 0x7f800000;
                  index = ( xhead >> 16 ) & 0xffUL; // table index
                  yhead = 0x7e000000UL - ghead;
                  gInHex.lval = ghead + ( ( 0xff00UL & SqrtTable[index] ) << 7 );
                  yInHex.lval = yhead + ( ( 0xffUL & SqrtTable[index] ) << 15 );
                  x = xInHex.fval;
                  g = gInHex.fval;
                  y = yInHex.fval;

/*******************************************************************************
*     Iterate to refine both g and y.                                          *
*******************************************************************************/
            
                  d = x - g * g;
                  y2 = y + y;
                  g = g + y * d;                   //   16-bit g
                  e = upHalfOfAnULP - y * g;
                  d = x - g * g;
                  y = y + e * y2;                  //   16-bit y
                  g = g + y * d;                   //   32-bit g before rounding
                  y2 = y + y;
                  e = upHalfOfAnULP - y * g;
                  d = x - g * g;
                  y = y + e * y2;                  //   32-bit y
                  g *= twoToMinus32;               //   undo scaling
                  d *= twoToMinus32;
                  FESETENVD( OldEnvironment.d );   //   restore caller's environment
                  return ( g + y * d );            //   final step            
                  }

/*******************************************************************************
*     Third section: handle x = +0.0 that slipped through.                     *
*******************************************************************************/

            else 
                  {                                // x = +0.0
                  FESETENVD( OldEnvironment.d );   //   restore caller's environment
                  return ( x );
                  }
            }
/*******************************************************************************
*     Fourth section: special cases: argument is +INF, NaN, -0.0, or <0.       *
*******************************************************************************/
   
      FESETENVD( OldEnvironment.d );               //   restore caller's environment

      if ( xhead < 0x80000000 )                    // x is +NaN or +INF
            return ( x );

      if ( ( x == 0.0 ) || ( x != x ) )            // return -0.0 or -NaN argument
            return x;
      else                                         // negative x is INVALID
            {
            x = nan ( SQRT_NAN );
            OldEnvironment.i.lo |= SET_INVALID;
            FESETENVD( OldEnvironment.d );         //   restore caller's environment
            return ( x );                          // return NAN
            }
      }
#else /* Let the 970 hardware have a crack at this. */
float sqrtf ( float x )	{ return (float) sqrt((double) x); }
#endif

static const hexsingle HugeF = { 0x7F800000 };
static const hexsingle NegHugeF = { 0xFF800000 };

float hypotf ( float x, float y )
{
        register float temp;
	hexdouble OldEnvironment, CurrentEnvironment;
        
        register float FPR_z, FPR_one, FPR_inf, FPR_Minf, FPR_absx, FPR_absy, FPR_big, FPR_small;
        register double FPR_env;
        
        FPR_z = 0.0;					FPR_one = 1.0;
        FPR_inf = HugeF.fval;				FPR_Minf = NegHugeF.fval;
        FPR_absx = __FABSF( x );			FPR_absy = __FABSF( y );
      
/*******************************************************************************
*     If argument is SNaN then a QNaN has to be returned and the invalid       *
*     flag signaled.                                                           * 
*******************************************************************************/
	
	if ( ( x == FPR_inf ) || ( y == FPR_inf ) || ( x == FPR_Minf ) || ( y == FPR_Minf ) )
            return FPR_inf;
                
        if ( ( x != x ) || ( y != y ) )
        {
            x = __FABSF ( x + y );
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
        temp = sqrtf ( FPR_one + temp * temp );	   
        
        FEGETENVD( CurrentEnvironment.d );
        CurrentEnvironment.i.lo &= ~FE_UNDERFLOW;	// Clear any inconsequential underflow
        FESETENVD( CurrentEnvironment.d );
        
        temp = FPR_big * temp;				// Might raise UNDERFLOW or OVERFLOW
            
        FEGETENVD( CurrentEnvironment.d );
        OldEnvironment.i.lo |= CurrentEnvironment.i.lo; // Pick up any UF or OF
        FESETENVD( OldEnvironment.d );        		// restore caller's environment

        return temp;
}

