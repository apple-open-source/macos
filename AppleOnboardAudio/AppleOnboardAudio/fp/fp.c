/*******************************************************************************
*													 *
*	File fp.c,											 *
*													 *
*	This is a collection of NCEG library glue.					 *
*													 *
*	Copyright © 1992 Apple Computer, Inc.  All rights reserved.			 *
*													 *
*	Written by Ali Sazegari, started on November 29 1992.				 *
*													 *
*	february 22, 1993:	fixed isfinite.						 *
*     march    31, 1993:      added exp2m1 and log21p for use in annuity and   *
*                             compound.                                        *
*     august   02, 1993:      corrected the undeserved flags of exp2 and       *
*                             exp2m1 and guarded against them.                 *
*     august   26, 1993:      defered the implementation of nanf and nanl to   *
*                             nan.c.                                           *
*													 *
*******************************************************************************/

#include	<stdio.h>
#include <CarbonCore/fp.h>
#include    <fenv.h>
#include	"fp_private.h"

#pragma    fenv_access    on

/*******************************************************************************
*					Trigonometric functions					 *
*******************************************************************************/

/*	cos and sin are in sincos.c, tan is in tg.c, acos and asin are in		*/
/*	asinacos.c and atan is in arctg.c.							*/

/*******************************************************************************
*					Hyperbolic functions					 *
*******************************************************************************/

/*	cosh and sinh are in chsh.c, tanh is in the th.c file, functions acosh,	*/ 
/*	asinh, atanh are in the ashachath.c file.						*/

/*******************************************************************************
*					Exponential functions					 *
*******************************************************************************/

/*	exp and expm1 are in tableExpD.c.			.				*/

/*	exp2 computes the base 2 exponential.						*/

#ifndef __MATH__
double_t exp2  ( double_t x )
	{
	register double y;
      fenv_t OldEnvironment;
      
      _feprocentry ( &OldEnvironment );
	y = x * 6.9314718055994530942E-1;
	feclearexcept ( FE_ALL_EXCEPT );
      feupdateenv ( &OldEnvironment );
	
	return ( exp ( y ) );
	}
#endif

double_t exp2m1 ( double_t x )
	{
	register double y;
      fenv_t OldEnvironment;
      
      _feprocentry ( &OldEnvironment );
	y = x * 6.9314718055994530942E-1;
	feclearexcept ( FE_ALL_EXCEPT );
      feupdateenv ( &OldEnvironment );
	return ( expm1 ( y ) );
	}

/*	frexp, ldexp and log are in frexp.c and ldexp.c.				*/

/*	log2 computes the base 2 logarithm.							*/

#ifndef __MATH__
double_t log2 ( double_t x )
	{

	return ( log(x) / 6.9314718055994530942E-1 );
	}
#endif

double_t log21p ( double_t x )
	{
	return ( log1p(x) / 6.9314718055994530942E-1 );
	}

/*	log10 in in log10.c, logb, is in logb.c and modf is in remmod.c.		*/

/*	modff is in fpfloatfunc.c.								*/

/*	scalb computes x * 2^n efficently.  This is not normally done by
	computing 2^n explicitly.								*/

/*	scalb is in scalb.c.									*/

/*******************************************************************************
*				Power and absolute value functions				 *
*******************************************************************************/

/*	fabs is in sign.c.									*/

/*	hypot computes the square root of the sum of the squares of its
	arguments, without undue overflow or underflow.					*/

/*	hypot is in hypot.c, XPWRY is in power.c and sqrt is not yet implemented*/

/*******************************************************************************
*				Gamma and Error functions					 *
*******************************************************************************/

/*	erf and erfc are implemented in erfcerf.c						*/

/*******************************************************************************
*				Nearest integer functions					 *
*******************************************************************************/

/*	ceil and floor are in ceilfloor.c.							*/

/*	the rint function rounds its argument to an integral value in floating
	point format, honoring the current rounding direction.			*/

/*	rint is implemented in the file rint.c.						*/

/*	nearbyint differs from rint only in that it does not raise the
	inexact exception. It is the nearbyint function recommended by the
	IEEE floating-point standard 854.							*/

/*	the function rinttol rounds its argument to the nearest long int using
	the current rounding direction.
	>>Note that if the rounded value is outside the range of long int, then
	the result is undefined.								*/

/*	the round function rounds the argument to the nearest integral value
	in double format similar to the Fortran "anint" function.  That is:
	add half to the magnitude and chop.							*/

/*	roundtol is similar to the Fortran function nint or to the Pascal round
	>>Note that if the rounded value is outside the range of long int, then
	the result is undefined.								*/

/*	trunc computes the integral value, in floating format, nearest to
	but no larger in magnitude than its argument.					*/

/*******************************************************************************
*					Remainder functions					 *
*******************************************************************************/

/*	fmod is covered in the ibm math libraires.					*/

/*	the following two functions compute the remainder.  remainder is required
	by the IEEE 754 floating point standard. The second form correponds to the
	SANE remainder; it stores into 'quotient' the 7 low-order bits of the
	integer quotient x/y, such that -127 <= quotient <= 127.			*/

/*******************************************************************************
*					Auxiliary functions					 *
*******************************************************************************/

/*	copysign produces a value with the magnitude of its first argument
	and sign of its second argument. NOTE: the order of the arguments
	matches the recommendation of the IEEE 754 floating point standard,
	which is opposite from the SANE copysign function.				*/

/*	copysign is covered in sign.c.							*/

/*	the call 'nan ( "n-char-sequence" )' returns a quiet NaN with content
	indicated through tagp in the selected data type format.			*/

/*	nan, nanl and nanf are covered in nan.c.                                */

/*	these compute the next representable value, in the type indicated,
	after 'x' in the direction of 'y'.  if x == y then y is returned.		*/

/*	nextafterd is covered in nextafter.c.						*/

/*	nextafterf is implemented in fpfloatfunc.c					*/

/*******************************************************************************
*				Max, Min and Positive Difference				 *
*******************************************************************************/

/*	These extension functions correspond to the standard functions, dim
	max and min.

	The fdim function determines the 'positive difference' between its
	arguments: { x - y, if x > y }, { +0, if x <= y }.  If one argument is
	NaN, then fdim returns that NaN.  if both arguments are NaNs, then fdim
	returns the first argument.								*/

#ifndef __MATH__
double_t fdim ( double_t x, double_t y )
	{
	if ( x != x ) return x;
	else if ( y != y ) return y;
	else if ( x > y ) return ( x - y );
	else return +0;
	}


/*	max and min return the maximum and minimum of their two arguments,
	respectively.  They correspond to the max and min functions in FORTRAN.
	NaN arguments are treated as missing data.  If one argument is NaN and
	the other is a number, then the number is returned.  If both are NaNs
	then the first argument is returned.						*/

double_t fmax ( double_t x, double_t y )
	{
	return ( __isnand(y) ? x : ( y <= x ? x : y ) );
	}

double_t fmin ( double_t x, double_t y )
	{
	return ( __isnand(y) ? x : ( y > x ? x : y ) );
	}
#endif

/*******************************************************************************
*						Constants						 *
*******************************************************************************/

const double_t pi = 3.141592653589793116e+00;

/*******************************************************************************
*					Internal prototypes					 *
*******************************************************************************/

/*	__fpclassify is implemented in ldclasssign.c					*/

/*	__fpclassifyd is covered in classify.c.						*/

/*	__fpclassifyf is implemented in fpfloatfunc.c					*/

/*	__isnormal is implemented in ldclasssign.c.					*/

#if SYSTEM_FRAMEWORK_NO_LONGER_HAS_MATH_SYMBOLS
long int __isnormald ( double x )
	{
	return ( __fpclassifyd ( x ) == FP_NORMAL ); 
	}
#endif

/*	__isnormalf is implemented in fpflaotfunc.c.					*/

/*	__isfinite is implemented in ldclasssign.c.					*/

#if SYSTEM_FRAMEWORK_NO_LONGER_HAS_MATH_SYMBOLS
long int __isfinited ( double x )
	{
	return ( __fpclassifyd ( x ) >= FP_ZERO ); 
	}
#endif

/*	isfinitef is implemented in fpflaotfunc.c.					*/

/*	__isnan is implemented in ldclasssign.c.						*/

#if SYSTEM_FRAMEWORK_NO_LONGER_HAS_MATH_SYMBOLS
long int __isnand ( double x )
	{
	long int class = __fpclassifyd(x);
	return ( ( class == FP_SNAN ) || ( class == FP_QNAN ) ); 
	}
#endif

/*	__isnanf is implemented in fpflaotfunc.c.						*/

/*	__signbit is implemented in ldclasssign.c.					*/

/*	__signbitd and __signbitf are in sign.c.						*/

double __inf ( void )
	{
	DblInHex infinity = { { 0x7F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };
	return ( infinity.dbl );
	}

/*******************************************************************************
*					Non NCEG extensions					 *
*******************************************************************************/

#ifndef __NOEXTENSIONS__

/*******************************************************************************
*					Financial functions					 *
*******************************************************************************/

/*	compound computes the compound interest factor "(1 + rate) ^ periods"
	more accurately than the straightforward computation with the Power
	function.  This is SANE's compound function.					*/

/*	replaced by its implementation in the file compound.c				*/

/*	The function annuity computes the present value factor for an annuity 
	"( 1 - ( 1 + rate ) ^ ( - periods ) ) / rate" more accurately than the
	straightforward computation with the Power function. This is SANE's 
	annuity function.										*/

/*	replaced by its implementation in the file annuity.c				*/

/*******************************************************************************
*					Random function						 *
*******************************************************************************/

/*	implementation provided in the file random.c.					*/

/*******************************************************************************
*					Relational operator					 *
*******************************************************************************/

relop relation ( double_t x, double_t y )
	{
				/*	It is important to compare for nans first.	*/
	if ( ( x != x ) || ( y != y )  )
		return UNORDERED;
	else if ( x == y )
		return EQUALTO;
	else if ( x < y )
		return LESSTHAN;
	else
		return GREATERTHAN;
	}


#endif	/* __NOEXTENSIONS__ */
