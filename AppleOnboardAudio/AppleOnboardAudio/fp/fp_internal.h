/*
     File:       CarbonCore/fp.h
 
     Contains:   FPCE Floating-Point Definitions and Declarations.
 
     Version:    CarbonCore-458~10
 
     Copyright:  © 1987-2002 by Apple Computer, Inc., all rights reserved.
 
     Bugs?:      For bug reports, consult the following page on
                 the World Wide Web:
 
                     http://developer.apple.com/bugreporter/
 
*/
#ifndef __FP__
#define __FP__

#include "math.h"
/********************************************************************************
*                                                                               *
*    A collection of numerical functions designed to facilitate a wide          *
*    range of numerical programming as required by C9X.                         *
*                                                                               *
*    The <fp.h> declares many functions in support of numerical programming.    *
*    It provides a superset of <math.h> and <SANE.h> functions.  Some           *
*    functionality previously found in <SANE.h> and not in the FPCE <fp.h>      *
*    can be found in this <fp.h> under the heading "__NOEXTENSIONS__".          *
*                                                                               *
*    All of these functions are IEEE 754 aware and treat exceptions, NaNs,      *
*    positive and negative zero and infinity consistent with the floating-      *
*    point standard.                                                            *
*                                                                               *
********************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

//#pragma options align=mac68k

/********************************************************************************
*                                                                               *
*                            Efficient types                                    *
*                                                                               *
*    float_t         Most efficient type at least as wide as float              *
*    double_t        Most efficient type at least as wide as double             *
*                                                                               *
*      CPU            float_t(bits)                double_t(bits)               *
*    --------        -----------------            -----------------             *
*    PowerPC          float(32)                    double(64)                   *
*    68K              long double(80/96)           long double(80/96)           *
*    x86              double(64)                   double(64)                   *
*                                                                               *
********************************************************************************/

//typedef float                           float_t;
//typedef double                          double_t;

/********************************************************************************
*                                                                               *
*                              Define some constants.                           *
*                                                                               *
*    HUGE_VAL            IEEE 754 value of infinity.                            *
*    INFINITY            IEEE 754 value of infinity.                            *
*    NAN                 A generic NaN (Not A Number).                          *
*    DECIMAL_DIG         Satisfies the constraint that the conversion from      *
*                        double to decimal and back is the identity function.   *
*                                                                               *
********************************************************************************/

#define      DECIMAL_DIG              17 /* does not exist for double-double */

/********************************************************************************
*                                                                               *
*                            Trigonometric functions                            *
*                                                                               *
*   acos        result is in [0,pi].                                            *
*   asin        result is in [-pi/2,pi/2].                                      *
*   atan        result is in [-pi/2,pi/2].                                      *
*   atan2       Computes the arc tangent of y/x in [-pi,pi] using the sign of   *
*               both arguments to determine the quadrant of the computed value. *
*                                                                               *
********************************************************************************/
/*
 *  cos()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
 double_t  cos(double_t x);


/*
 *  sin()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  sin(double_t x);


/*
 *  tan()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  tan(double_t x);


/*
 *  acos()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  acos(double_t x);


/*
 *  asin()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  asin(double_t x);


/*
 *  atan()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  atan(double_t x);


/*
 *  atan2()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  atan2(double_t y, double_t x);




/********************************************************************************
*                                                                               *
*                              Hyperbolic functions                             *
*                                                                               *
********************************************************************************/
/*
 *  cosh()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  cosh(double_t x);


/*
 *  sinh()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  sinh(double_t x);


/*
 *  tanh()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  tanh(double_t x);


/*
 *  acosh()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  acosh(double_t x);


/*
 *  asinh()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  asinh(double_t x);


/*
 *  atanh()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  atanh(double_t x);




/********************************************************************************
*                                                                               *
*                              Exponential functions                            *
*                                                                               *
*   expm1       expm1(x) = exp(x) - 1.  But, for small enough arguments,        *
*               expm1(x) is expected to be more accurate than exp(x) - 1.       *
*   frexp       Breaks a floating-point number into a normalized fraction       *
*               and an integral power of 2.  It stores the integer in the       *
*               object pointed by *exponent.                                    *
*   ldexp       Multiplies a floating-point number by an integer power of 2.    *
*   log1p       log1p = log(1 + x). But, for small enough arguments,            *
*               log1p is expected to be more accurate than log(1 + x).          *
*   logb        Extracts the exponent of its argument, as a signed integral     *
*               value. A subnormal argument is treated as though it were first  *
*               normalized. Thus:                                               *
*                                  1   <=   x * 2^(-logb(x))   <   2            *
*   modf        Returns fractional part of x as function result and returns     *
*               integral part of x via iptr. Note C9X uses double not double_t. *
*   scalb       Computes x * 2^n efficently.  This is not normally done by      *
*               computing 2^n explicitly.                                       *
*                                                                               *
********************************************************************************/
/*
 *  exp()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  exp(double_t x);


/*
 *  expm1()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  expm1(double_t x);


/*
 *  exp2()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  exp2(double_t x);


/*
 *  frexp()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  frexp(double_t x, int *exponent);


/*
 *  ldexp()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  ldexp(double_t x, int n);


/*
 *  log()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  log(double_t x);


/*
 *  log2()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  log2(double_t x);


/*
 *  log1p()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  log1p(double_t x);


/*
 *  log10()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  log10(double_t x);


/*
 *  logb()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  logb(double_t x);


#if !TYPE_LONGDOUBLE_IS_DOUBLE
/*
 *  modfl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
long double  modfl(long double x, long double *iptrl);


#endif  /* !TYPE_LONGDOUBLE_IS_DOUBLE */

/*
 *  modf()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  modf(double_t x, double_t *iptr);


/*
 *  modff()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
float  modff(float x, float *iptrf);



/*
    Note: For compatiblity scalb(x,n) has n of type
            int  on Mac OS X 
            long on Mac OS
*/
typedef int                             _scalb_n_type;
/*
 *  scalb()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  scalb(double_t x, _scalb_n_type n);




/********************************************************************************
*                                                                               *
*                     Power and absolute value functions                        *
*                                                                               *
*   hypot       Computes the square root of the sum of the squares of its       *
*               arguments, without undue overflow or underflow.                 *
*   pow         Returns x raised to the power of y.  Result is more accurate    *
*               than using exp(log(x)*y).                                       *
*                                                                               *
********************************************************************************/
/*
 *  fabs()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  fabs(double_t x);


/*
 *  hypot()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  hypot(double_t x, double_t y);


/*
 *  pow()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 2.0 and later
 */
double_t  pow(double_t x, double_t y);


/*
 *  sqrt()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  sqrt(double_t x);




/********************************************************************************
*                                                                               *
*                        Gamma and Error functions                              *
*                                                                               *
*   erf         The error function.                                             *
*   erfc        Complementary error function.                                   *
*   gamma       The gamma function.                                             *
*   lgamma      Computes the base-e logarithm of the absolute value of          *
*               gamma of its argument x, for x > 0.                             *
*                                                                               *
********************************************************************************/
/*
 *  erf()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  erf(double_t x);


/*
 *  erfc()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  erfc(double_t x);


/*
 *  gamma()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  gamma(double_t x);


/*
 *  lgamma()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  lgamma(double_t x);




/********************************************************************************
*                                                                               *
*                        Nearest integer functions                              *
*                                                                               *
*   rint        Rounds its argument to an integral value in floating point      *
*               format, honoring the current rounding direction.                *
*                                                                               *
*   nearbyint   Differs from rint only in that it does not raise the inexact    *
*               exception. It is the nearbyint function recommended by the      *
*               IEEE floating-point standard 854.                               *
*                                                                               *
*   rinttol     Rounds its argument to the nearest long int using the current   *
*               rounding direction.  NOTE: if the rounded value is outside      *
*               the range of long int, then the result is undefined.            *
*                                                                               *
*   round       Rounds the argument to the nearest integral value in floating   *
*               point format similar to the Fortran "anint" function. That is:  *
*               add half to the magnitude and chop.                             *
*                                                                               *
*   roundtol    Similar to the Fortran function nint or to the Pascal round.    *
*               NOTE: if the rounded value is outside the range of long int,    *
*               then the result is undefined.                                   *
*                                                                               *
*   trunc       Computes the integral value, in floating format, nearest to     *
*               but no larger in magnitude than its argument.   NOTE: on 68K    *
*               compilers when using -elems881, trunc must return an int        *
*                                                                               *
********************************************************************************/
/*
 *  ceil()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  ceil(double_t x);


/*
 *  floor()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  floor(double_t x);


/*
 *  rint()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  rint(double_t x);


/*
 *  nearbyint()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  nearbyint(double_t x);


/*
 *  rinttol()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
long  rinttol(double_t x);


/*
 *  round()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  round(double_t x);


/*
 *  roundtol()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
long  roundtol(double_t round);


/*
    Note: For compatiblity trunc(x) has a return type of
            int       for classic 68K with FPU enabled
            double_t  everywhere else
*/
#if TARGET_RT_MAC_68881
typedef int                             _trunc_return_type;
#else
typedef double_t                        _trunc_return_type;
#endif  /* TARGET_RT_MAC_68881 */

/*
 *  trunc()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
_trunc_return_type  trunc(double_t x);





/********************************************************************************
*                                                                               *
*                            Remainder functions                                *
*                                                                               *
*   remainder       IEEE 754 floating point standard for remainder.             *
*   remquo          SANE remainder.  It stores into 'quotient' the 7 low-order  *
*                   bits of the integer quotient x/y, such that:                *
*                       -127 <= quotient <= 127.                                *
*                                                                               *
********************************************************************************/
/*
 *  fmod()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  fmod(double_t x, double_t y);


/*
 *  remainder()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  remainder(double_t x, double_t y);


/*
 *  remquo()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  remquo(double_t x, double_t y, int *quo);




/********************************************************************************
*                                                                               *
*                             Auxiliary functions                               *
*                                                                               *
*   copysign        Produces a value with the magnitude of its first argument   *
*                   and sign of its second argument.  NOTE: the order of the    *
*                   arguments matches the recommendation of the IEEE 754        *
*                   floating point standard,  which is opposite from the SANE   *
*                   copysign function.                                          *
*                                                                               *
*   nan             The call 'nan("n-char-sequence")' returns a quiet NaN       *
*                   with content indicated through tagp in the selected         *
*                   data type format.                                           *
*                                                                               *
*   nextafter       Computes the next representable value after 'x' in the      *
*                   direction of 'y'.  if x == y, then y is returned.           *
*                                                                               *
********************************************************************************/
/*
 *  copysign()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  copysign(double_t x, double_t y);


/*
 *  nan()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double  nan(const char * tagp);


/*
 *  nanf()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
float  nanf(const char * tagp);



/*
 *  nanl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  nanl(const char * tagp);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double nanl(const char *tagp) { return (long double) nan(tagp); }
  #else
    #define nanl(tagp) ((long double) nan(tagp))
  #endif
#endif


/*
 *  nextafterd()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double  nextafterd(double x, double y);


/*
 *  nextafterf()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
float  nextafterf(float x, float y);



/*
 *  nextafterl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  nextafterl(long double x, long double y);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double nextafterl(long double x, long double y) { return (long double) nextafterd((double)(x),(double)(y)); }
  #else
    #define nextafterl(x, y) ((long double) nextafterd((double)(x),(double)(y)))
  #endif
#endif


/********************************************************************************
*                                                                               *
*                              Inquiry macros                                   *
*                                                                               *
*   fpclassify      Returns one of the FP_Å values.                             *
*   isnormal        Non-zero if and only if the argument x is normalized.       *
*   isfinite        Non-zero if and only if the argument x is finite.           *
*   isnan           Non-zero if and only if the argument x is a NaN.            *
*   signbit         Non-zero if and only if the sign of the argument x is       *
*                   negative.  This includes, NaNs, infinities and zeros.       *
*                                                                               *
********************************************************************************/
/* #define      fpclassify(x)    ( ( sizeof ( x ) == sizeof(double) ) ?           \
                              __fpclassifyd  ( x ) :                           \
                                ( sizeof ( x ) == sizeof(float) ) ?            \
                              __fpclassifyf ( x ) :                            \
                              __fpclassify  ( x ) )
#define      isnormal(x)      ( ( sizeof ( x ) == sizeof(double) ) ?           \
                              __isnormald ( x ) :                              \
                                ( sizeof ( x ) == sizeof(float) ) ?            \
                              __isnormalf ( x ) :                              \
                              __isnormal  ( x ) )
#define      isfinite(x)      ( ( sizeof ( x ) == sizeof(double) ) ?           \
                              __isfinited ( x ) :                              \
                                ( sizeof ( x ) == sizeof(float) ) ?            \
                              __isfinitef ( x ) :                              \
                              __isfinite  ( x ) )
#define      isnan(x)         ( ( sizeof ( x ) == sizeof(double) ) ?           \
                              __isnand ( x ) :                                 \
                                ( sizeof ( x ) == sizeof(float) ) ?            \
                              __isnanf ( x ) :                                 \
                              __isnan  ( x ) )
#define      signbit(x)       ( ( sizeof ( x ) == sizeof(double) ) ?           \
                              __signbitd ( x ) :                               \
                                ( sizeof ( x ) == sizeof(float) ) ?            \
                              __signbitf ( x ) :                               \
                              __signbit  ( x ) )
*/


/********************************************************************************
*                                                                               *
*                      Max, Min and Positive Difference                         *
*                                                                               *
*   fdim        Determines the 'positive difference' between its arguments:     *
*               { x - y, if x > y }, { +0, if x <= y }.  If one argument is     *
*               NaN, then fdim returns that NaN.  if both arguments are NaNs,   *
*               then fdim returns the first argument.                           *
*                                                                               *
*   fmax        Returns the maximum of the two arguments.  Corresponds to the   *
*               max function in FORTRAN.  NaN arguments are treated as missing  *
*               data.  If one argument is NaN and the other is a number, then   *
*               the number is returned.  If both are NaNs then the first        *
*               argument is returned.                                           *
*                                                                               *
*   fmin        Returns the minimum of the two arguments.  Corresponds to the   *
*               min function in FORTRAN.  NaN arguments are treated as missing  *
*               data.  If one argument is NaN and the other is a number, then   *
*               the number is returned.  If both are NaNs then the first        *
*               argument is returned.                                           *
*                                                                               *
********************************************************************************/
/*
 *  fdim()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  fdim(double_t x, double_t y);


/*
 *  fmax()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  fmax(double_t x, double_t y);


/*
 *  fmin()
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
double_t  fmin(double_t x, double_t y);


#endif /* (defined(__MWERKS__) && defined(__cmath__)) || (TARGET_RT_MAC_MACHO && defined(__MATH__)) */

/*******************************************************************************
*                                Constants                                     *
*******************************************************************************/
/*
 *  pi
 *  
 *  Availability:
 *    Mac OS X:         in version 10.0 and later in CoreServices.framework
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later
 */
extern const double_t pi;

/********************************************************************************
*                                                                               *
*                         PowerPC-only Function Prototypes                      *
*                                                                               *
********************************************************************************/

#if TARGET_CPU_PPC
#ifndef __MWERKS__  /* Metrowerks does not support double double */

/*
 *  cosl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  cosl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double cosl(long double x) { return (long double) cos((double)(x)); }
  #else
    #define cosl(x) ((long double) cos((double)(x)))
  #endif
#endif



/*
 *  sinl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  sinl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double sinl(long double x) { return (long double) sin((double)(x)); }
  #else
    #define sinl(x) ((long double) sin((double)(x)))
  #endif
#endif



/*
 *  tanl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  tanl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double tanl(long double x) { return (long double) tan((double)(x)); }
  #else
    #define tanl(x) ((long double) tan((double)(x)))
  #endif
#endif



/*
 *  acosl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  acosl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double acosl(long double x) { return (long double) acos((double)(x)); }
  #else
    #define acosl(x) ((long double) acos((double)(x)))
  #endif
#endif



/*
 *  asinl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  asinl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double asinl(long double x) { return (long double) asin((double)(x)); }
  #else
    #define asinl(x) ((long double) asin((double)(x)))
  #endif
#endif



/*
 *  atanl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  atanl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double atanl(long double x) { return (long double) atan((double)(x)); }
  #else
    #define atanl(x) ((long double) atan((double)(x)))
  #endif
#endif



/*
 *  atan2l()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  atan2l(long double y, long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double atan2l(long double y, long double x) { return (long double) atan2((double)(y), (double)(x)); }
  #else
    #define atan2l(y, x) ((long double) atan2((double)(y), (double)(x)))
  #endif
#endif



/*
 *  coshl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  coshl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double coshl(long double x) { return (long double) cosh((double)(x)); }
  #else
    #define coshl(x) ((long double) cosh((double)(x)))
  #endif
#endif



/*
 *  sinhl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  sinhl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double sinhl(long double x) { return (long double) sinh((double)(x)); }
  #else
    #define sinhl(x) ((long double) sinh((double)(x)))
  #endif
#endif



/*
 *  tanhl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  tanhl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double tanhl(long double x) { return (long double) tanh((double)(x)); }
  #else
    #define tanhl(x) ((long double) tanh((double)(x)))
  #endif
#endif



/*
 *  acoshl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  acoshl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double acoshl(long double x) { return (long double) acosh((double)(x)); }
  #else
    #define acoshl(x) ((long double) acosh((double)(x)))
  #endif
#endif



/*
 *  asinhl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  asinhl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double asinhl(long double x) { return (long double) asinh((double)(x)); }
  #else
    #define asinhl(x) ((long double) asinh((double)(x)))
  #endif
#endif



/*
 *  atanhl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  atanhl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double atanhl(long double x) { return (long double) atanh((double)(x)); }
  #else
    #define atanhl(x) ((long double) atanh((double)(x)))
  #endif
#endif



/*
 *  expl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  expl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double expl(long double x) { return (long double) exp((double)(x)); }
  #else
    #define expl(x) ((long double) exp((double)(x)))
  #endif
#endif



/*
 *  expm1l()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  expm1l(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double expm1l(long double x) { return (long double) expm1((double)(x)); }
  #else
    #define expm1l(x) ((long double) expm1((double)(x)))
  #endif
#endif



/*
 *  exp2l()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  exp2l(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double exp2l(long double x) { return (long double) exp2((double)(x)); }
  #else
    #define exp2l(x) ((long double) exp2((double)(x)))
  #endif
#endif



/*
 *  frexpl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  frexpl(long double x, int *exponent);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double frexpl(long double x, int *exponent) { return (long double) frexp((double)(x), (exponent)); }
  #else
    #define frexpl(x, exponent) ((long double) frexp((double)(x), (exponent)))
  #endif
#endif



/*
 *  ldexpl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  ldexpl(long double x, int n);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double ldexpl(long double x, int n) { return (long double) ldexp((double)(x), (n)); }
  #else
    #define ldexpl(x, n) ((long double) ldexp((double)(x), (n)))
  #endif
#endif



/*
 *  logl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  logl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double logl(long double x) { return (long double) log((double)(x)); }
  #else
    #define logl(x) ((long double) log((double)(x)))
  #endif
#endif



/*
 *  log1pl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  log1pl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double log1pl(long double x) { return (long double) log1p((double)(x)); }
  #else
    #define log1pl(x) ((long double) log1p((double)(x)))
  #endif
#endif



/*
 *  log10l()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  log10l(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double log10l(long double x) { return (long double) log10((double)(x)); }
  #else
    #define log10l(x) ((long double) log10((double)(x)))
  #endif
#endif



/*
 *  log2l()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  log2l(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double log2l(long double x) { return (long double) log2((double)(x)); }
  #else
    #define log2l(x) ((long double) log2((double)(x)))
  #endif
#endif



/*
 *  logbl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  logbl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double logbl(long double x) { return (long double) logb((double)(x)); }
  #else
    #define logbl(x) ((long double) logb((double)(x)))
  #endif
#endif



/*
 *  scalbl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  scalbl(long double x, long n);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double scalbl(long double x, long n) { return (long double) scalb((double)(x), (n)); }
  #else
    #define scalbl(x, n) ((long double) scalb((double)(x), (n)))
  #endif
#endif



/*
 *  fabsl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  fabsl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double fabsl(long double x) { return (long double) fabs((double)(x)); }
  #else
    #define fabsl(x) ((long double) fabs((double)(x)))
  #endif
#endif



/*
 *  hypotl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  hypotl(long double x, long double y);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double hypotl(long double x, long double y) { return (long double) hypot((double)(x), (double)(y)); }
  #else
    #define hypotl(x, y) ((long double) hypot((double)(x), (double)(y)))
  #endif
#endif



/*
 *  powl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  powl(long double x, long double y);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double powl(long double x, long double y) { return (long double) pow((double)(x), (double)(y)); }
  #else
    #define powl(x, y) ((long double) pow((double)(x), (double)(y)))
  #endif
#endif



/*
 *  sqrtl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  sqrtl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double sqrtl(long double x) { return (long double) sqrt((double)(x)); }
  #else
    #define sqrtl(x) ((long double) sqrt((double)(x)))
  #endif
#endif



/*
 *  erfl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  erfl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double erfl(long double x) { return (long double) erf((double)(x)); }
  #else
    #define erfl(x) ((long double) erf((double)(x)))
  #endif
#endif



/*
 *  erfcl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  erfcl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double erfcl(long double x) { return (long double) erfc((double)(x)); }
  #else
    #define erfcl(x) ((long double) erfc((double)(x)))
  #endif
#endif



/*
 *  gammal()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  gammal(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double gammal(long double x) { return (long double) gamma((double)(x)); }
  #else
    #define gammal(x) ((long double) gamma((double)(x)))
  #endif
#endif



/*
 *  lgammal()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  lgammal(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double lgammal(long double x) { return (long double) lgamma((double)(x)); }
  #else
    #define lgammal(x) ((long double) lgamma((double)(x)))
  #endif
#endif



/*
 *  ceill()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 2.0 and later or as macro/inline
 */
long double  ceill(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double ceill(long double x) { return (long double) ceil((double)(x)); }
  #else
    #define ceill(x) ((long double) ceil((double)(x)))
  #endif
#endif



/*
 *  floorl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  floorl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double floorl(long double x) { return (long double) floor((double)(x)); }
  #else
    #define floorl(x) ((long double) floor((double)(x)))
  #endif
#endif



/*
 *  rintl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  rintl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double rintl(long double x) { return (long double) rint((double)(x)); }
  #else
    #define rintl(x) ((long double) rint((double)(x)))
  #endif
#endif



/*
 *  nearbyintl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  nearbyintl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double nearbyintl(long double x) { return (long double) nearbyint((double)(x)); }
  #else
    #define nearbyintl(x) ((long double) nearbyint((double)(x)))
  #endif
#endif



/*
 *  rinttoll()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long  rinttoll(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long rinttoll(long double x) { return rinttol((double)(x)); }
  #else
    #define rinttoll(x) (rinttol((double)(x)))
  #endif
#endif



/*
 *  roundl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  roundl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double roundl(long double x) { return (long double) round((double)(x)); }
  #else
    #define roundl(x) ((long double) round((double)(x)))
  #endif
#endif



/*
 *  roundtoll()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long  roundtoll(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long roundtoll(long double x) { return roundtol((double)(x)); }
  #else
    #define roundtoll(x) (roundtol((double)(x)))
  #endif
#endif



/*
 *  truncl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  truncl(long double x);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double truncl(long double x) { return (long double) trunc((double)(x)); }
  #else
    #define truncl(x) ((long double) trunc((double)(x)))
  #endif
#endif



/*
 *  remainderl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  remainderl(long double x, long double y);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double remainderl(long double x, long double y) { return (long double) remainder((double)(x), (double)(y)); }
  #else
    #define remainderl(x, y) ((long double) remainder((double)(x), (double)(y)))
  #endif
#endif



/*
 *  remquol()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  remquol(long double x, long double y, int *quo);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double remquol(long double x, long double y, int *quo) { return (long double) remquo((double)(x), (double)(y), (quo)); }
  #else
    #define remquol(x, y, quo) ((long double) remquo((double)(x), (double)(y), (quo)))
  #endif
#endif



/*
 *  copysignl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  copysignl(long double x, long double y);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double copysignl(long double x, long double y) { return (long double) copysign((double)(x), (double)(y)); }
  #else
    #define copysignl(x, y) ((long double) copysign((double)(x), (double)(y)))
  #endif
#endif



/*
 *  fdiml()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  fdiml(long double x, long double y);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double fdiml(long double x, long double y) { return (long double) fdim((double)(x), (double)(y)); }
  #else
    #define fdiml(x, y) ((long double) fdim((double)(x), (double)(y)))
  #endif
#endif



/*
 *  fmaxl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  fmaxl(long double x, long double y);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double fmaxl(long double x, long double y) { return (long double) fmax((double)(x), (double)(y)); }
  #else
    #define fmaxl(x, y) ((long double) fmax((double)(x), (double)(y)))
  #endif
#endif



/*
 *  fminl()
 *  
 *  Availability:
 *    Mac OS X:         not available
 *    CarbonLib:        in CarbonLib 1.0 and later
 *    Non-Carbon CFM:   in MathLib 1.0 and later or as macro/inline
 */
long double  fminl(long double x, long double y);
#if TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long double fminl(long double x, long double y) { return (long double) fmin((double)(x), (double)(y)); }
  #else
    #define fminl(x, y) ((long double) fmin((double)(x), (double)(y)))
  #endif
#endif


#endif /* __MWERKS__ */

#pragma options align=reset

#ifdef __cplusplus
}
#endif

#endif /* __FP__ */

