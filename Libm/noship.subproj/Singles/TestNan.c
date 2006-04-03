/*	This module contains a program to test Apple's implementation of the C
	standard functions nanf, nan, and nanl.

	$Revision: 1.2 $, $Date: 2005/06/16 20:14:10 $


	This program tests our implementation and not just the C standard.  That
	is, it expects the functions to return specific values we have defined
	where the standard allows more flexibility.

	This program expects that NaNs are supported; it does not check this.
	(Support for NaNs could be tested by checking whether the nan function
	returns a zero instead of a nan, whether strtod("NAN", &EndPointer) sets
	EndPointer to point to the first "N" rather than the terminating NULL, or
	whether gcc's preprocessor identifiers __FLT_HAS_QUIET_NAN__,
	__DBL__HAS_QUIET_NAN__, and __LDBL__HAS_QUIET_NAN__ are not defined.)

	This program assumes PowerPC systems are big-endian and IA-32 systems are
	little-endian.
*/


#include <fenv.h>
#include <inttypes.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#pragma STDC FENV_ACCESS ON


// Determine how many bits are in the significand of a long double.
#if defined __POWERPC__
	/*	On a PowerPC with Apple's GCC, a long double is represented with one or
		two doubles.  In either case, only the significand of one double is
		used as the significand for the long double.
	*/
	#define	BitsPerLongDoubleSignificand	52
#elif defined __i386__
	/*	On IA-32, a long double has a 63-bit significand (just the fraction
		part, not including the integer bit).
	*/
	#define	BitsPerLongDoubleSignificand	63
#else
	#error "I do not know what kind of freaky architecture you are using."
#endif


/*	We will watch for a floating-point exception.  When one occurs, the signal
	handler will communicate that to the regular code by setting SignalOccurred
	to the number of the signal that occurred (SIGFPE).
*/
static volatile sig_atomic_t SignalOccurred = 0;


// When a signal is received, record it.
static void Handler(int signal)
{
	SignalOccurred = signal;
}


/*	Define a large integer type for holding significands of floating-point
	numbers.
*/
typedef uint64_t Significand;

/*	Define a structure to hold test program context.  For now, we just need
	to know how many errors have been found.
*/
typedef struct
{
	unsigned int errors;
} Context;


// Extract the significand from a float.
static uint64_t ExtractSignificandF(float x)
{
	union {
		float f;
		uint32_t u;
	} t = { x };
	return t.u & (1<<23)-1;
}


// Extract the significand from a double.
static uint64_t ExtractSignificandD(double x)
{
	union {
		double d;
		uint64_t u;
	} t = { x };
	return t.u & (UINT64_C(1)<<52)-1;
}


// Extract the significand from a long double.
static uint64_t ExtractSignificandL(long double x)
{
#if defined __POWERPC__
	union {
		double ld;
		uint64_t u;
	} t = { x };
	return t.u & (UINT64_C(1)<<52)-1;
#elif defined __i386__
	union {
		long double ld;
		struct {
			uint64_t IntegerAndSignificand;
			unsigned int : 1+15;
		} s;
	} t = { x };
	return t.s.IntegerAndSignificand & (UINT64_C(1)<<63)-1;
#else
	#error "I do not know what kind of freaky architecture you are using."
#endif
}


/*	Return true if an except occurred, indicated either by a signal or by a
	floating-point environment flag.
*/
bool DidExceptionOccur(void)
{
	bool result = false;

	// Check for and reset signal.
	if (SignalOccurred != 0)
	{
		result = true;
		SignalOccurred = 0;
	}
	
	// Check for and reset floating-point environment flags.
	if (fetestexcept(FE_ALL_EXCEPT))
	{
		result = true;

		/*	Signaling NaNs might generated an invalid exception.  Other
			exceptions would be more puzzling.
		*/
		if (fetestexcept(FE_ALL_EXCEPT & ~FE_INVALID))
			fprintf(stderr,
"Error, received floating-point exception other than invalid.\n");
		feclearexcept(FE_ALL_EXCEPT);
	}

	return result;
}


// Test a float to see if it is not the expected NaN.
static int testf(Context *context, const char *tagp, Significand expected)
{
	float f = nanf(tagp);
	uint64_t observed = ExtractSignificandF(f);

	if (f <= 0 || 0 < f)
	{
		printf(
			"Error, nanf(\"%s\") returned object that is not a NaN:  %.8g.\n",
			tagp, f);
		++context->errors;
		return 1;
	}

	if (DidExceptionOccur())
	{
		printf("\
Error, nanf(\"%s\") returned signaling NaN:\n\
	Observed significand = 0x%" PRIx64 ".\n\
	Expected significand = 0x%" PRIx64 ".\n",
			tagp, observed, expected);
		++context->errors;
		return 1;
	}

	if (observed != expected)
	{
		printf("\
Error, nanf(\"%s\") returned object with unexpected significand:\n\
	Observed significand = 0x%" PRIx64 ".\n\
	Expected significand = 0x%" PRIx64 ".\n",
			tagp, observed, expected);
		++context->errors;
		return 1;
	}

	return 0;
}


// Test a double to see if it is not the expected NaN.
static int testd(Context *context, const char *tagp, Significand expected)
{
	double d = nan(tagp);
	uint64_t observed = ExtractSignificandD(d);

	if (d <= 0 || 0 < d)
	{
		printf(
			"Error, nan(\"%s\") returned object that is not a NaN:  %.17g.\n",
			tagp, d);
		++context->errors;
		return 1;
	}

	if (DidExceptionOccur())
	{
		printf("\
Error, nan(\"%s\") returned signaling NaN:\n\
	Observed significand = 0x%" PRIx64 ".\n\
	Expected significand = 0x%" PRIx64 ".\n",
			tagp, observed, expected);
		++context->errors;
		return 1;
	}

	if (observed != expected)
	{
		printf("\
Error, nan(\"%s\") returned object with unexpected significand:\n\
	Observed significand = 0x%" PRIx64 ".\n\
	Expected significand = 0x%" PRIx64 ".\n",
			tagp, observed, expected);
		++context->errors;
		return 1;
	}

	return 0;
}


// Test a long double to see if it is not the expected NaN.
static int testl(Context *context, const char *tagp, Significand expected)
{
	long double ld = nanl(tagp);
	uint64_t observed = ExtractSignificandL(ld);

	if (ld <= 0 || 0 < ld)
	{
		printf(
			"Error, nanl(\"%s\") returned object that is not a NaN:  %.33Lg.\n",
			tagp, ld);
		++context->errors;
		return 1;
	}

	if (DidExceptionOccur())
	{
		printf("\
Error, nanl(\"%s\") returned signaling NaN:\n\
	Observed significand = 0x%" PRIx64 ".\n\
	Expected significand = 0x%" PRIx64 ".\n",
			tagp, observed, expected);
		++context->errors;
		return 1;
	}
	if (observed != expected)
	{
		printf("\
Error, nanl(\"%s\") returned object with unexpected significand:\n\
	Observed significand = 0x%" PRIx64 ".\n\
	Expected significand = 0x%" PRIx64 ".\n",
			tagp, observed, expected);
		++context->errors;
		return 1;
	}

	return 0;
}


#define	QuietF(a)	(UINT64_C(1)<<22 | (a))
#define	QuietD(a)	(UINT64_C(1)<<51 | (a))
#define	QuietL(a)	(UINT64_C(1)<<BitsPerLongDoubleSignificand-1 | (a))


int main(void)
{
	// Initialize.
	Context context = { 0 };		// No errors yet.
	signal(SIGFPE, Handler);		// Set signal handler.
	feclearexcept(FE_ALL_EXCEPT);	// Clear exception flags.


	//-------------------------------------------------------------------------
	// Test float.


	// Test NULL and empty string.

	testf(&context, NULL,		QuietF(0x0));
	testf(&context, "",			QuietF(0x0));

	// Test a few bits.

	testf(&context, "00",		QuietF(00));
	testf(&context, "01",		QuietF(01));
	testf(&context, "02",		QuietF(02));

	testf(&context, "0",		QuietF(0));
	testf(&context, "1",		QuietF(1));
	testf(&context, "2",		QuietF(2));
	testf(&context, "9",		QuietF(9));

	testf(&context, "0x0",		QuietF(0x0));
	testf(&context, "0x1",		QuietF(0x1));
	testf(&context, "0x2",		QuietF(0x2));
	testf(&context, "0xf",		QuietF(0xf));

	// Test each digit.

	testf(&context, "0716",		QuietF(0716));
	testf(&context, "02534",	QuietF(02534));
	testf(&context, "08",		QuietF(0));
	testf(&context, "09",		QuietF(0));
	testf(&context, "0a",		QuietF(0));
	testf(&context, "0A",		QuietF(0));
	testf(&context, "0f",		QuietF(0));
	testf(&context, "0F",		QuietF(0));

	testf(&context, "13579",	QuietF(13579));
	testf(&context, "2468",		QuietF(2468));
	testf(&context, "a",		QuietF(0));
	testf(&context, "A",		QuietF(0));
	testf(&context, "f",		QuietF(0));
	testf(&context, "F",		QuietF(0));

	testf(&context, "0x0f1e",	QuietF(0x0f1e));
	testf(&context, "0x2d3c",	QuietF(0x2d3c));
	testf(&context, "0X4b5a",	QuietF(0x4b5a));
	testf(&context, "0x6978",	QuietF(0x6978));
	testf(&context, "0xCBAD",	QuietF(0xcbad));
	testf(&context, "0XF0E0",	QuietF(0xF0E0));

	// Test many leading zeroes.

	testf(&context, "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000034",		QuietF(034));

	testf(&context, "0x000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000034",		QuietF(0x34));

	// Test n-char-sequences that are not numerals.

	testf(&context, "_",		QuietF(0));
	testf(&context, "g",		QuietF(0));

	// Test non-n-char-sequences.

	testf(&context, "@",		QuietF(0));
	testf(&context, "z @ #$Q",	QuietF(0));

	// Test lots of digits.

	testf(&context, "00000000000000000000000g",		QuietF(0));
	testf(&context, "07777777777777777777777G",		QuietF(0));

	testf(&context, "1111111111111111111111g",		QuietF(0));

	testf(&context, "0x0000000000000000000g",		QuietF(0));
	testf(&context, "0xfffffffffffffffffffG",		QuietF(0));
	testf(&context, "0xfffffffffffffffffff@",		QuietF(0));

	// Test all bits.

	testf(&context, "017777777",	QuietF(0x3fffff));

	testf(&context, "4194303",		QuietF(0x3fffff));

	testf(&context, "0x3fffff",		QuietF(0x3fffff));

	// Test "quiet" bit in the significand.

	testf(&context, "020000000",	QuietF(0));
	testf(&context, "030000001",	QuietF(010000001));
	testf(&context, "034000003",	QuietF(014000003));
	testf(&context, "070000005",	QuietF(010000005));
	testf(&context, "037777777",	QuietF(017777777));

	testf(&context, "4194304",		QuietF(0));
	testf(&context, "4194305",		QuietF(1));
	testf(&context, "6291459",		QuietF(0x200003));
	testf(&context, "8000000",		QuietF(8000000-4194304));
	testf(&context, "8388607",		QuietF(0x3fffff));

	testf(&context, "0x400000",		QuietF(0x0));
	testf(&context, "0x400001",		QuietF(0x1));
	testf(&context, "0x600003",		QuietF(0x200003));
	testf(&context, "0x700005",		QuietF(0x300005));
	testf(&context, "0x7fffff",		QuietF(0x3fffff));

	// Test overflow.

	testf(&context, "040000000",	QuietF(00));
	testf(&context, "040000001",	QuietF(01));
	testf(&context, "0100000047",	QuietF(047));
	testf(&context, "0123456701",	QuietF(023456701));

	testf(&context, "8388608",		QuietF(0));
	testf(&context, "8388609",		QuietF(1));
	testf(&context, "16777219",		QuietF(3));

	testf(&context, "0x800000",		QuietF(0x0));
	testf(&context, "0x800001",		QuietF(0x1));
	testf(&context, "0x1000003",	QuietF(0x3));
	testf(&context, "0xf000004",	QuietF(0x4));
	testf(&context, "0x123456789A",	QuietF(0x16789a));


	//-------------------------------------------------------------------------
	// Test double.


	// Test NULL and empty string.

	testd(&context, NULL,		QuietD(0x0));
	testd(&context, "",			QuietD(0x0));

	// Test a few bits.

	testd(&context, "00",		QuietD(00));
	testd(&context, "01",		QuietD(01));
	testd(&context, "02",		QuietD(02));

	testd(&context, "0",		QuietD(0));
	testd(&context, "1",		QuietD(1));
	testd(&context, "2",		QuietD(2));
	testd(&context, "9",		QuietD(9));

	testd(&context, "0x0",		QuietD(0x0));
	testd(&context, "0x1",		QuietD(0x1));
	testd(&context, "0x2",		QuietD(0x2));
	testd(&context, "0xf",		QuietD(0xf));

	// Test each digit.

	testd(&context, "0716",		QuietD(0716));
	testd(&context, "02534",	QuietD(02534));
	testd(&context, "08",		QuietD(0));
	testd(&context, "09",		QuietD(0));
	testd(&context, "0a",		QuietD(0));
	testd(&context, "0A",		QuietD(0));
	testd(&context, "0f",		QuietD(0));
	testd(&context, "0F",		QuietD(0));

	testd(&context, "13579",	QuietD(13579));
	testd(&context, "2468",		QuietD(2468));
	testd(&context, "a",		QuietD(0));
	testd(&context, "A",		QuietD(0));
	testd(&context, "f",		QuietD(0));
	testd(&context, "F",		QuietD(0));

	testd(&context, "0x0f1e",	QuietD(0x0f1e));
	testd(&context, "0x2d3c",	QuietD(0x2d3c));
	testd(&context, "0X4b5a",	QuietD(0x4b5a));
	testd(&context, "0x6978",	QuietD(0x6978));
	testd(&context, "0xCBAD",	QuietD(0xcbad));
	testd(&context, "0XF0E0",	QuietD(0xF0E0));

	// Test many leading zeroes.

	testd(&context, "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000034",		QuietD(034));

	testd(&context, "0x000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000034",		QuietD(0x34));

	// Test n-char-sequences that are not numerals.

	testd(&context, "_",		QuietD(0));
	testd(&context, "g",		QuietD(0));

	// Test non-n-char-sequences.

	testd(&context, "@",		QuietD(0));
	testd(&context, "z @ #$Q",	QuietD(0));

	// Test lots of digits.

	testd(&context, "00000000000000000000000g",		QuietD(0));
	testd(&context, "07777777777777777777777G",		QuietD(0));

	testd(&context, "1111111111111111111111g",		QuietD(0));

	testd(&context, "0x0000000000000000000g",		QuietD(0));
	testd(&context, "0xfffffffffffffffffffG",		QuietD(0));
	testd(&context, "0xfffffffffffffffffff@",		QuietD(0));

	// Test all bits.

	testd(&context, "077777777777777777",	QuietD(0x7ffffffffffff));

	testd(&context, "2251799813685247",		QuietD(0x7ffffffffffff));

	testd(&context, "0x7ffffffffffff",		QuietD(0x7ffffffffffff));

	// Test "quiet" bit in the significand.

	testd(&context, "0100000000000000000",	QuietD(0));
	testd(&context, "0100000000000000001",	QuietD(1));
	testd(&context, "0300000000000000000",	QuietD(0));
	testd(&context, "0500000000000000001",	QuietD(1));
	testd(&context, "0777777777777777777",	QuietD(0x7ffffffffffff));

	testd(&context, "2251799813685248",		QuietD(0));
	testd(&context, "2251799813685249",		QuietD(1));

	testd(&context, "0x8000000000000",		QuietD(0x0));
	testd(&context, "0x8000000000001",		QuietD(0x1));
	testd(&context, "0xc000000000000",		QuietD(0x4000000000000));
	testd(&context, "0xe000000000000",		QuietD(0x6000000000000));
	testd(&context, "0xeffffffffffff",		QuietD(0x6ffffffffffff));

	// Test overflow.

	testd(&context, "0200000000000000001",		QuietD(01));
	testd(&context, "01000000000000000010",		QuietD(010));
	testd(&context, "02000000000000000100",		QuietD(0100));
	testd(&context, "037000000000000001000",	QuietD(01000));
	testd(&context, "012345670123456701234",	QuietD(0145670123456701234));

	testd(&context, "4503599627370496",			QuietD(0));
	testd(&context, "4503599627370497",			QuietD(1));
	testd(&context, "9007199254741002",			QuietD(10));
	testd(&context, "18014398509482084",		QuietD(100));

	testd(&context, "0x10000000000000",			QuietD(0x0));
	testd(&context, "0x10000000000001",			QuietD(0x1));
	testd(&context, "0x20000000000003",			QuietD(0x3));
	testd(&context, "0xf0000000000004",			QuietD(0x4));
	testd(&context, "0x123456789AbCdeF",		QuietD(0x3456789abcdef));


	//-------------------------------------------------------------------------
	// Test long double.


	// Test NULL and empty string.

	testl(&context, NULL,		QuietL(0x0));
	testl(&context, "",			QuietL(0x0));

	// Test a few bits.

	testl(&context, "00",		QuietL(00));
	testl(&context, "01",		QuietL(01));
	testl(&context, "02",		QuietL(02));

	testl(&context, "0",		QuietL(0));
	testl(&context, "1",		QuietL(1));
	testl(&context, "2",		QuietL(2));
	testl(&context, "9",		QuietL(9));

	testl(&context, "0x0",		QuietL(0x0));
	testl(&context, "0x1",		QuietL(0x1));
	testl(&context, "0x2",		QuietL(0x2));
	testl(&context, "0xf",		QuietL(0xf));

	// Test each digit.

	testl(&context, "0716",		QuietL(0716));
	testl(&context, "02534",	QuietL(02534));
	testl(&context, "08",		QuietL(0));
	testl(&context, "09",		QuietL(0));
	testl(&context, "0a",		QuietL(0));
	testl(&context, "0A",		QuietL(0));
	testl(&context, "0f",		QuietL(0));
	testl(&context, "0F",		QuietL(0));

	testl(&context, "13579",	QuietL(13579));
	testl(&context, "2468",		QuietL(2468));
	testl(&context, "a",		QuietL(0));
	testl(&context, "A",		QuietL(0));
	testl(&context, "f",		QuietL(0));
	testl(&context, "F",		QuietL(0));

	testl(&context, "0x0f1e",	QuietL(0x0f1e));
	testl(&context, "0x2d3c",	QuietL(0x2d3c));
	testl(&context, "0X4b5a",	QuietL(0x4b5a));
	testl(&context, "0x6978",	QuietL(0x6978));
	testl(&context, "0xCBAD",	QuietL(0xcbad));
	testl(&context, "0XF0E0",	QuietL(0xF0E0));

	// Test many leading zeroes.

	testl(&context, "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000034",		QuietL(034));

	testl(&context, "0x000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000034",		QuietL(0x34));

	// Test n-char-sequences that are not numerals.

	testl(&context, "_",		QuietL(0));
	testl(&context, "g",		QuietL(0));

	// Test non-n-char-sequences.

	testl(&context, "@",		QuietL(0));
	testl(&context, "z @ #$Q",	QuietL(0));

	// Test lots of digits.

	testl(&context, "00000000000000000000000g",		QuietL(0));
	testl(&context, "07777777777777777777777G",		QuietL(0));

	testl(&context, "1111111111111111111111g",		QuietL(0));

	testl(&context, "0x0000000000000000000g",		QuietL(0));
	testl(&context, "0xfffffffffffffffffffG",		QuietL(0));
	testl(&context, "0xfffffffffffffffffff@",		QuietL(0));

#if BitsPerLongDoubleSignificand == 52

	// Test all bits.

	testl(&context, "077777777777777777",	QuietL(0x7ffffffffffff));

	testl(&context, "2251799813685247",		QuietL(0x7ffffffffffff));

	testl(&context, "0x7ffffffffffff",		QuietL(0x7ffffffffffff));

	// Test "quiet" bit in the significand.

	testl(&context, "0100000000000000000",	QuietL(0));
	testl(&context, "0100000000000000001",	QuietL(1));
	testl(&context, "0300000000000000000",	QuietL(0));
	testl(&context, "0500000000000000001",	QuietL(1));
	testl(&context, "0777777777777777777",	QuietL(0x7ffffffffffff));

	testl(&context, "2251799813685248",		QuietL(0));
	testl(&context, "2251799813685249",		QuietL(1));

	testl(&context, "0x8000000000000",		QuietL(0x0));
	testl(&context, "0x8000000000001",		QuietL(0x1));
	testl(&context, "0xc000000000000",		QuietL(0x4000000000000));
	testl(&context, "0xe000000000000",		QuietL(0x6000000000000));
	testl(&context, "0xeffffffffffff",		QuietL(0x6ffffffffffff));

	// Test overflow.

	testl(&context, "0200000000000000001",		QuietL(01));
	testl(&context, "01000000000000000010",		QuietL(010));
	testl(&context, "02000000000000000100",		QuietL(0100));
	testl(&context, "037000000000000001000",	QuietL(01000));
	testl(&context, "012345670123456701234",	QuietL(0145670123456701234));

	testl(&context, "4503599627370496",			QuietL(0));
	testl(&context, "4503599627370497",			QuietL(1));
	testl(&context, "9007199254741002",			QuietL(10));
	testl(&context, "18014398509482084",		QuietL(100));

	testl(&context, "0x10000000000000",			QuietL(0x0));
	testl(&context, "0x10000000000001",			QuietL(0x1));
	testl(&context, "0x20000000000003",			QuietL(0x3));
	testl(&context, "0xf0000000000004",			QuietL(0x4));
	testl(&context, "0x123456789AbCdeF",		QuietL(0x3456789abcdef));

#elif BitsPerLongDoubleSignificand == 63

	// Test all bits.

	testl(&context, "0377777777777777777777",	QuietL(0x3fffffffffffffff));

	testl(&context, "4611686018427387903",		QuietL(0x3fffffffffffffff));

	testl(&context, "0x3fffffffffffffff",		QuietL(0x3fffffffffffffff));

	// Test "quiet" bit in the significand.

	testl(&context, "0400000000000000000000",	QuietL(0));
	testl(&context, "0400000000000000000001",	QuietL(1));
	testl(&context, "0600000000000000000004",	QuietL(0200000000000000000004));
	testl(&context, "0700000000000000000006",	QuietL(0300000000000000000006));
	testl(&context, "0777777777777777757777",	QuietL(0377777777777777757777));

	testl(&context, "4611686018427387904",		QuietL(0));
	testl(&context, "4611686018427387905",		QuietL(1));

	testl(&context, "0x4000000000000000",		QuietL(0x0));
	testl(&context, "0x4000000000000001",		QuietL(0x1));
	testl(&context, "0x6000000000000000",		QuietL(0x2000000000000000));
	testl(&context, "0x7000000000000000",		QuietL(0x3000000000000000));
	testl(&context, "0x7fffffffffffffff",		QuietL(0x3fffffffffffffff));

	// Test overflow.

	testl(&context, "01000000000000000000000",	QuietL(00));
	testl(&context, "01000000000000000000001",	QuietL(01));
	testl(&context, "02000000000000000000010",	QuietL(010));
	testl(&context, "07000000000000000000100",	QuietL(0100));
	testl(&context, "01234567012345670123456",	QuietL(0234567012345670123456));

	testl(&context, "4611686018427387904",		QuietL(0));
	testl(&context, "4611686018427387905",		QuietL(1));
	testl(&context, "9223372036854775818",		QuietL(10));
	testl(&context, "18446744073709551716",		QuietL(100));

	testl(&context, "0x8000000000000000",		QuietL(0x0));
	testl(&context, "0x8000000000000001",		QuietL(0x1));
	testl(&context, "0x10000000000000003",		QuietL(0x3));
	testl(&context, "0xf0000000000000004",		QuietL(0x4));
	testl(&context, "0x123456789AbCDef1234567",	QuietL(0x789abcdef1234567));

#else	// BitsPerLongDoubleSignificand

	#error "There are no test cases for this situation."

#endif


	//-------------------------------------------------------------------------


	printf("Exiting with %d error%s.\n", context.errors,
		context.errors == 1 ? "" : "s");

	return context.errors == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
