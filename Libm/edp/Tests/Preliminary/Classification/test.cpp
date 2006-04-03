/*	Rudimentary test program for floating-point classification, including
	Intel's 80-bit long double.
*/


#include <cmath>
#include <iostream>


#include "interface.h"


// Ensure macros are not defined.
#undef	fpclassify
#undef	isnormal
#undef	isfinite
#undef	isinf
#undef	isnan
#undef	signbit

int fpclassify(float x)			{ return fpclassifyF(x); }
int fpclassify(double x)		{ return fpclassifyD(x); }
int fpclassify(long double x)	{ return fpclassifyL(x); }

int isnormal(float x)			{ return isnormalF(x); }
int isnormal(double x)			{ return isnormalD(x); }
int isnormal(long double x)		{ return isnormalL(x); }

int isfinite(float x)			{ return isfiniteF(x); }
int isfinite(double x)			{ return isfiniteD(x); }
int isfinite(long double x)		{ return isfiniteL(x); }

int isinf(float x)				{ return isinfF(x); }
int isinf(double x)				{ return isinfD(x); }
int isinf(long double x)		{ return isinfL(x); }

int isnan(float x)				{ return isnanF(x); }
int isnan(double x)				{ return isnanD(x); }
int isnan(long double x)		{ return isnanL(x); }

int signbit(float x)			{ return signbitF(x); }
int signbit(double x)			{ return signbitD(x); }
int signbit(long double x)		{ return signbitL(x); }


#if 0
// Work around GCC bug:  names should be in std but are in global namespace.
namespace std
{
	using ::scalbnf;
	using ::scalbn ;
	using ::scalbnl;
	using ::nextafterf;
	using ::nextafter ;
	using ::nextafterl;
}


float		scalb(float x, int n)			{ return std::scalbnf(x, n); }
double		scalb(double x, int n)			{ return std::scalbn (x, n); }
long double	scalb(long double x, int n)		{ return std::scalbnl(x, n); }

float		nextafter(float x, float y)		{ return std::nextafterf(x, y); }
double		nextafter(double x, double y)	{ return std::nextafter (x, y); }
long double	nextafter(long double x, long double y)
											{ return std::nextafterl(x, y); }
#endif


// Report the name of a type.
template<class Type> const char *Name();
template<> const char *Name<float>()		{ return "float"; }
template<> const char *Name<double>()		{ return "double"; }
template<> const char *Name<long double>()	{ return "long double"; }


class Context
{
public:
	int errors;
	Context() : errors(0) {}
};


void DeclareError(Context &context)
{
	if (10 <= ++context.errors)
	{
		std::cout << "Error limit exceeded, aborting.\n";
		throw;
	}
}


// Return a string containing the name of a floating-point class.
const char *ClassName(int Class)
{
	switch (Class)
	{
		case FP_INFINITE:	return "FP_INFINITE";
		case FP_NAN:		return "FP_NAN";
		case FP_NORMAL:		return "FP_NORMAL";
		case FP_SUBNORMAL:	return "FP_SUBNORMAL";
		case FP_ZERO:		return "FP_ZERO";
		default:			return "unknown value";
	}
}


// Return a string containing "zero" or "nonzero" describing an int.
const char *ZeroNonzero(int n)
{
	return n ? "nonzero" : "zero";
}


#include <limits>


enum Sign { Positive, Negative };


// Test C's classification macros as applied to a single value.
template<class FloatingType> void TestClass(
	FloatingType x, int ExpectedClass, Sign ExpectedSign, Context &context)
{
	int Expected, Observed;

	// Test fpclassify.
	int ObservedClass = fpclassify(x);
	if (ObservedClass != ExpectedClass)
	{
		std::cout << "Error, " << Name<FloatingType>()
			<< " fpclassify(" << x << ") returned "
			<< ClassName(ObservedClass) << " but should return "
			<< ClassName(ExpectedClass) << ".\n";
		DeclareError(context);
	}

	// Test isnormal.
	Expected = ExpectedClass == FP_NORMAL;
	Observed = isnormal(x);
	if (!Expected != !Observed)
	{
		std::cout << "Error, " << Name<FloatingType>()
			<< " isnormal(" << x << ") returned "
			<< ZeroNonzero(Observed) << " but should return "
			<< ZeroNonzero(Expected) << ".\n";
		DeclareError(context);
	}

	// Test isfinite.
	Expected = ExpectedClass == FP_ZERO
		|| ExpectedClass == FP_SUBNORMAL
		|| ExpectedClass == FP_NORMAL;
	Observed = isfinite(x);
	if (!Expected != !Observed)
	{
		std::cout << "Error, " << Name<FloatingType>()
			<< " isfinite(" << x << ") returned "
			<< ZeroNonzero(Observed) << " but should return "
			<< ZeroNonzero(Expected) << ".\n";
		DeclareError(context);
	}

	// Test isinf.
	Expected = ExpectedClass == FP_INFINITE;
	Observed = isinf(x);
	if (!Expected != !Observed)
	{
		std::cout << "Error, " << Name<FloatingType>()
			<< " isinf(" << x << ") returned "
			<< ZeroNonzero(Observed) << " but should return "
			<< ZeroNonzero(Expected) << ".\n";
		DeclareError(context);
	}

	// Test isnan.
	Expected = ExpectedClass == FP_NAN;
	Observed = isnan(x);
	if (!Expected != !Observed)
	{
		std::cout << "Error, " << Name<FloatingType>()
			<< " isnan(" << x << ") returned "
			<< ZeroNonzero(Observed) << " but should return "
			<< ZeroNonzero(Expected) << ".\n";
		DeclareError(context);
	}

	// Test signbit.
	Expected = ExpectedSign == Negative;
	Observed = signbit(x);
	if (!Expected != !Observed)
	{
		std::cout << "Error, " << Name<FloatingType>()
			<< " signbit(" << x << ") returned "
			<< ZeroNonzero(Observed) << " but should return "
			<< ZeroNonzero(Expected) << ".\n";
		DeclareError(context);
	}
}


// Test C's classification macros for several values.
template<class FloatingType> void TestClasses(Context &context)
{
	FloatingType x;

	// Test basic properties required of the floating-point system.
	if (std::numeric_limits<FloatingType>::radix != 2)
	{
		std::cerr
			<< "Error, this program expects floating-point radix to be 2.\n";
		throw;
	}

	if (!std::numeric_limits<FloatingType>::is_iec559)
	{
		std::cerr <<
"Error, this program expects floating-point to conform to IEC-559.\n";
		throw;
	}

	if (std::numeric_limits<FloatingType>::has_denorm != std::denorm_present)
	{
		std::cerr <<
"Error, this program expects floating-point to have denormalized values.\n";
		throw;
	}

#if 0
	x = 0;
	if (nextafter(x, 1) != std::numeric_limits<FloatingType>::denorm_min())
	{
		std::cerr << "Error, nextafter(0, 1) is not equal to denorm_min().\n";
		throw;
	}
#endif

#if 0
	x = 1;
	if (std::numeric_limits<FloatingType>::min()
		!= scalb(x, std::numeric_limits<FloatingType>::min_exponent + 1))
	{
		std::cerr << "Error, value of min() is inconsistent.\n";
		throw;
	}

	x = 2;
	if (std::numeric_limits<FloatingType>::epsilon()
		!= scalb(x, -std::numeric_limits<FloatingType>::digits))
	{
		std::cerr << "Error, value of epsilon() is inconsistent.\n";
		throw;
	}
#endif

	const FloatingType LargestSignificand
		= 2 - std::numeric_limits<FloatingType>::epsilon();

#if 0
	if (std::numeric_limits<FloatingType>::max() != scalb(LargestSignificand,
			std::numeric_limits<FloatingType>::min_exponent - 1))
	{
		std::cerr << "Error, value of max() is inconsistent.\n";
		throw;
	}
#endif

	// Test zero.
	x = 0;
	TestClass( x, FP_ZERO, Positive, context);
	TestClass(-x, FP_ZERO, Negative, context);

	// Test least subnormal number.
#if 0
	x = nextafter(x, 1);
#else
	x = std::numeric_limits<FloatingType>::denorm_min();
#endif
	TestClass( x, FP_SUBNORMAL, Positive, context);
	TestClass(-x, FP_SUBNORMAL, Negative, context);

	// Test greatest subnormal number.
	x = std::numeric_limits<FloatingType>::denorm_min()
		* (1 - std::numeric_limits<FloatingType>::epsilon());
	TestClass( x, FP_SUBNORMAL, Positive, context);
	TestClass(-x, FP_SUBNORMAL, Negative, context);

	// Test least normal number.
	x = std::numeric_limits<FloatingType>::min();
	TestClass( x, FP_NORMAL, Positive, context);
	TestClass(-x, FP_NORMAL, Negative, context);

	// Test lots of normal numbers.
	x = std::numeric_limits<FloatingType>::min();
	for (	int i = std::numeric_limits<FloatingType>::min_exponent;
			i <= std::numeric_limits<FloatingType>::max_exponent;
			++i
		)
	{
		TestClass( x, FP_NORMAL, Positive, context);
		TestClass(-x, FP_NORMAL, Negative, context);
		TestClass( x * LargestSignificand, FP_NORMAL, Positive, context);
		TestClass(-x * LargestSignificand, FP_NORMAL, Negative, context);
		x += x;
	}

	// Test greatest normal number.
	x = std::numeric_limits<FloatingType>::max();
	TestClass( x, FP_NORMAL, Positive, context);
	TestClass(-x, FP_NORMAL, Negative, context);

	// Test infinity.
	x = std::numeric_limits<FloatingType>::infinity();
	TestClass( x, FP_INFINITE, Positive, context);
	TestClass(-x, FP_INFINITE, Negative, context);

	// Test a quiet NaN.
	x = std::numeric_limits<FloatingType>::quiet_NaN();
	TestClass( x, FP_NAN, Positive, context);
	TestClass(-x, FP_NAN, Negative, context);

	// Test a signaling NaN.
	x = std::numeric_limits<FloatingType>::signaling_NaN();
	TestClass( x, FP_NAN, Positive, context);
	TestClass(-x, FP_NAN, Negative, context);
	
	// It would be good to test other NaNs.
}


int main()
{
	Context context;

	TestClasses<float>(context);
	TestClasses<double>(context);
	TestClasses<long double>(context);

	std::cout << "Exiting with " << context.errors << " errors.\n";

	return context.errors == 0;
}
