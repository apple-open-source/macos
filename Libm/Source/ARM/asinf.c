/*	This is an implementation of asinf.  It is written in standard C except
	except float and double are expected be IEEE 754 single- and
	double-precision implementations and that "volatile" is used to attempt to
	force certain floating-point operations to occur at run time (to generate
	exceptions that might not be generated if the operations are performed at
	compile time).  It should be good enough to serve as the libm asinf with
	tolerable performance.
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


/*	float asinf(float x).

	(This routine appears below, following the Tail subroutine.)

	Notes:

		Citations in parentheses below indicate the source of a
		requirement.

		"C" stands for ISO/IEC 9899:TC2.

		The Open Group specification (IEEE Std 1003.1, 2004 edition) adds
		no requirements since it defers to C and requires errno behavior
		only if we choose to support it by arranging for "math_errhandling
		& MATH_ERRNO" to be non-zero, which we do not.

	Return value:

		For arcsine of +/- zero, return zero with same sign (C F.9 12 and
		F.9.1.2).

		For 1 < |x| (including infinity), return NaN (C F.9.1.2).

		For a NaN, return the same NaN (C F.9 11 and 13).  (If the NaN is a
		signalling NaN, we return the "same" NaN quieted.)

		Otherwise:

			If the rounding mode is round-to-nearest, return arcsine(x)
			faithfully rounded.

			Returns a value in [-pi/2, +pi/2] (C 7.12.4.2 3).  Note that this
			prohibits returning correctly rounded values for asinf(-1) and
			asinf(+1), since pi/2 rounded to a float lies outside that interval.
		
			Not implemented:  In other rounding modes, return arcsine(x)
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
		spite of C 4.2.1)  or 1 < |x| (including infinity) (C F.9.1.2) but
		not if the input is a quiet NaN (C F.9 11).

		May not raise exceptions otherwise (C F.9 9).

	Properties:

		Monotonic.
*/


// Return arcsine(x) given that .57 < x, with the same properties as asinf.
static float Tail(float x)
{
	static const double HalfPi = 0x3.243f6a8885a308d313198a2e03707344ap-1;

	if (1 <= x)
	{
		// If x is 1, generate inexact and return HalfPi rounded down.
		if (1 == x)
			return 0x1.921fb4p0f + Tiny;
		// If x is outside the domain, generate invalid and return NaN.
		else
			return Infinity - Infinity;
	}

	#if defined __i386__ || defined __x86_64

		float a = 1-x;
		float ef;

		// Estimate 1/sqrt(1-x) with a relative error of at most 1.5*2**-12.
		__asm__("rsqrtss %[a], %[ef]" : [ef] "=x" (ef) : [a] "x" (a));

		// Refine the estimate using a minimax polynomial.
		double e = ef;
		double e1a = e*a;
		double e2a = e*e1a;
		double s = (e2a - 0x1.AAAAABC2AAAAFp1) * e2a + 0x1.3FFFFED400007p2;

		return (float)(HalfPi -
				(e1a
					* ((x + 0x1.5BD56966F3453p1) * x + 0x1.C13379E04F3DDp3))
			*
				(0x1.B0F1B6148BC69p-10
					* ((x - 0x1.09FDD79B2743Ap3) * x + 0x1.965DC0FC92BE7p4))
			*
				s);
	#else

		return HalfPi -
				((x + 0x1.5BD56966F3453p1) * x + 0x1.C13379E04F3DDp3)
			*
				(0x1.20A121259314Dp-8
					* ((x - 0x1.09FDD79B2743Ap3) * x + 0x1.965DC0FC92BE7p4))
			* sqrt(1-x);

	#endif
}


// See documentation above.
float asinf(float x)
{
	if (x < -.57f)
		return -Tail(-x);
	else if (x <= +.57f)
	{
		// Square x.  (Convert to double first to avoid underflow.)
		double x2 = (double) x * x;

		return (float)(x +
			(0x1.A7F2819B28221p-5
				* ((x2 + 0x1.D56F2A71A09C0p0) * x2 + 0x1.9118944A1A3B1p0))
			* x *
			(x2 * ((x2 - 0x1.7B912FDCD7ADBp0) * x2 + 0x1.071C2DE97B47Ep1)));
	}
	else
		return +Tail(+x);
}
