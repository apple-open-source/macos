/*	This is an implementation of acosf.  It is written in standard C except
	float and double are expected be IEEE 754 single- and double-precision
	implementations and that "volatile" is used to attempt to force certain
	floating-point operations to occur at run time (to generate exceptions that
	might not be generated if the operations are performed at compile time).
	It should be good enough to serve as the libm acosf with tolerable
	performance.
*/


// Include math.h to ensure we match the declarations.
#include <math.h>


/*	Declare certain constants volatile to force the compiler to access them
	when we reference them.  This in turn forces arithmetic operations on them
	to be performed at run time (or as if at run time).  We use such operations
	to signal invalid or inexact.
*/
static volatile const float Infinity = INFINITY;
static volatile const float Tiny = 0x1p-126f;


#if defined __STDC__ && 199901L <= __STDC_VERSION__ && !defined __GNUC__
	// GCC does not currently support FENV_ACCESS.  Maybe someday.
	#pragma STDC FENV_ACCESS ON
#endif


/*	float acosf(float x).

	(This routine appears below, following the PositiveTail and NegativeTail
	subroutines.)

	Notes:

		Citations in parentheses below indicate the source of a
		requirement.

		"C" stands for ISO/IEC 9899:TC2.

		The Open Group specification (IEEE Std 1003.1, 2004 edition) adds
		no requirements since it defers to C and requires errno behavior
		only if we choose to support it by arranging for "math_errhandling
		& MATH_ERRNO" to be non-zero, which we do not.

	Return value:

		For arccosine of 1, return +0 (C F.9.1.1).

		For 1 < |x| (including infinity), return NaN (C F.9.1.1).

		For a NaN, return the same NaN (C F.9 11 and 13).  (If the NaN is a
		signalling NaN, we return the "same" NaN quieted.)

		Otherwise:

			If the rounding mode is round-to-nearest, return arccosine(x)
			faithfully rounded.

			Returns a value in [0, pi] (C 7.12.4.1 3).  Note that this
			prohibits returning a correctly rounded value for acosf(-1) pi
			rounded to a float lies outside that interval.
		
			Not implemented:  In other rounding modes, return arccosine(x)
			possibly with slightly worse error, not necessarily honoring
			the rounding mode (Ali Sazegari narrowing C F.9 10).

	Exceptions:

		Raise underflow for a denormal result (C F.9 7 and Draft Standard
		for Floating-Point Arithmetic P754 Draft 1.2.5 9.5).  If the input
		is the smallest normal, underflow may or may not be raised.  This
		is stricter than the older 754 standard.

		May or may not raise inexact, even if the result is exact (C F.9
		8).

		Raise invalid if the input is a signalling NaN (C 5.2.4.2.2 3, in
		spite of C 4.2.1)  or 1 < |x| (including infinity) (C F.9.1.1) but
		not if the input is a quiet NaN (C F.9 11).

		May not raise exceptions otherwise (C F.9 9).

	Properties:

		Monotonic.
*/


/*	Return arccosine(x) given that +.62 < x, with the same properties as
	acosf.
*/
static float PositiveTail(float x)
{
	if (1 <= x)
	{
		// If x is 1, return 0.
		if (1 == x)
			return 0;
		// If x is outside the domain, generate invalid and return NaN.
		else
			return Infinity - Infinity;
	}

	#if defined __i386__ || defined __x86_64__

		float a = 1-x;
		float ef;

		// Estimate 1/sqrt(1-x) with a relative error of at most 1.5*2**-12.
		__asm__("rsqrtss %[a], %[ef]" : [ef] "=x" (ef) : [a] "x" (a));

		// Refine the estimate using a minimax polynomial.
		double e = ef;
		double e1a = e*a;
		double e2a = e*e1a;
		double s = (e2a - 0x1.AAAAABC2AAAAFp1) * e2a + 0x1.3FFFFED400007p2;

		return (float)
((e1a                   * ((x + 0x1.5BF7EF31D03E9p1) * x + 0x1.D75F3135B1D17p3))
*(0x1.82BAABF9AAC5Ep-10 * ((x - 0x1.136F5A328AFC8p3) * x + 0x1.B17BE5D0DECD9p4))
*s);


	#else	// #if defined __i386__ || defined __x86_64__

		return
  (                       ((x + 0x1.5BF7EF31D03E9p1) * x + 0x1.D75F3135B1D17p3))
* (0x1.01D1C56316584p-8 * ((x - 0x1.136F5A328AFC8p3) * x + 0x1.B17BE5D0DECD9p4))
* sqrt(1-x);

	#endif	// #if defined __i386__ || defined __x86_64__
}


/*	Return arccosine(x) given that x < -.62, with the same properties as
	acosf.
*/
static float NegativeTail(float x)
{
	static const double Pi = 0x3.243f6a8885a308d313198a2e03707344ap0;

	if (x <= -1)
	{
		// If x is -1, generate inexact and return pi rounded down.
		if (-1 == x)
			return 0x1.921fb4p1f + Tiny;
		// If x is outside the domain, generate invalid and return NaN.
		else
			return Infinity - Infinity;
	}

	#if defined __i386__ || defined __x86_64__

		float a = 1+x;
		float ef;

		// Estimate 1/sqrt(1+x) with a relative error of at most 1.5*2**-12.
		__asm__("rsqrtss %[a], %[ef]" : [ef] "=x" (ef) : [a] "x" (a));

		// Refine the estimate using a minimax polynomial.
		double e = ef;
		double e1a = e*a;
		double e2a = e*e1a;
		double s = (e2a - 0x1.AAAAABC2AAAAFp1) * e2a + 0x1.3FFFFED400007p2;

		return (float) (Pi -
(e1a                   * ((x - 0x1.5BF7EF31D03E9p1) * x + 0x1.D75F3135B1D17p3))
*(0x1.82BAABF9AAC5Ep-10 * ((x + 0x1.136F5A328AFC8p3) * x + 0x1.B17BE5D0DECD9p4))
*s);


	#else	// #if defined __i386__ || defined __x86_64__

		return Pi -
  (                       ((x - 0x1.5BF7EF31D03E9p1) * x + 0x1.D75F3135B1D17p3))
* (0x1.01D1C56316584p-8 * ((x + 0x1.136F5A328AFC8p3) * x + 0x1.B17BE5D0DECD9p4))
* sqrt(1+x);

	#endif	// #if defined __i386__ || defined __x86_64__
}


// See documentation above.
float acosf(float x)
{
	static const double HalfPi = 0x3.243f6a8885a308d313198a2e03707344ap-1;

	if (x < -.62f)
		return NegativeTail(x);
	else if (x <= +.62f)
	{
		// Square x.  (Convert to double first to avoid underflow.)
		double x2 = (double) x * x;

		return (float)(HalfPi - x -
(0x1.1F1B81164C324p-4 * x
	* ((x2 + 0x1.9249F14B97277p0) * x2 + 0x1.408A21C01FB5Ap0))
*	(x2 * ((x2 - 0x1.899ED21055CD3p0) * x2 + 0x1.E638836E9888Ep0)));
	}
	else
		return PositiveTail(x);
}
