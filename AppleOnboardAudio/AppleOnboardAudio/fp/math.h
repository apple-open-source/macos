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
*     File:  math.h	                                                       *
*                                                                              *
*     Contains: typedefs, prototypes, and macros germane to C99 floating point.*
*                                                                              *
*******************************************************************************/
#ifndef __MATH__
#define __MATH__

#ifdef __cplusplus
"C" {
#endif

/******************************************************************************
*       Floating point data types                                             *
******************************************************************************/

typedef float		float_t;
typedef double		double_t;

#define	HUGE_VAL	1e500
#define	HUGE_VALF	1e50f

#define INFINITY	HUGE_VALF

#define NAN		__nan( )

/******************************************************************************
*      Taxonomy of floating point data types                                  *
******************************************************************************/

enum {
	FP_NAN          = 1,                   /*      NaN                    */
	FP_INFINITE     = 2,                   /*      + or - infinity        */
	FP_ZERO         = 3,                   /*      + or - zero            */
	FP_NORMAL       = 4,                   /*      all normal numbers     */
	FP_SUBNORMAL    = 5                    /*      denormal numbers       */
};

/* "Fused" multiply-add is well supported on PowerPC */
#define FP_FAST_FMA	1
#define FP_FAST_FMAF	1

/* The values returned by `ilogb' for 0 and NaN respectively.  */
#define FP_ILOGB0	(-2147483647)
#define FP_ILOGBNAN	(2147483647)

/* Bitmasks for the math_errhandling macro.  */
#define MATH_ERRNO	1	/* errno set by math functions.  */
#define MATH_ERREXCEPT	2	/* Exceptions raised by math functions.  */

#define math_errhandling (__math_errhandling())
unsigned int __math_errhandling ( void );

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

#define      fpclassify( x )    ( ( sizeof ( x ) == sizeof(double) ) ?           \
                              __fpclassifyd  ( x ) :                           \
                                ( sizeof ( x ) == sizeof( float) ) ?            \
                              __fpclassifyf ( x ) :                            \
                              __fpclassify  ( x ) )
#define      isnormal( x )      ( ( sizeof ( x ) == sizeof(double) ) ?           \
                              __isnormald ( x ) :                              \
                                ( sizeof ( x ) == sizeof( float) ) ?            \
                              __isnormalf ( x ) :                              \
                              __isnormal  ( x ) )
#define      isfinite( x )      ( ( sizeof ( x ) == sizeof(double) ) ?           \
                              __isfinited ( x ) :                              \
                                ( sizeof ( x ) == sizeof( float) ) ?            \
                              __isfinitef ( x ) :                              \
                              __isfinite  ( x ) )
#define      isinf( x )         ( ( sizeof ( x ) == sizeof(double) ) ?           \
                              __isinfd ( x ) :                                 \
                                ( sizeof ( x ) == sizeof( float) ) ?            \
                              __isinff ( x ) :                                 \
                              __isinf  ( x ) )
#define      isnan( x )         ( ( sizeof ( x ) == sizeof(double) ) ?           \
                              __isnand ( x ) :                                 \
                                ( sizeof ( x ) == sizeof( float) ) ?            \
                              __isnanf ( x ) :                                 \
                              __isnan  ( x ) )
#define      signbit( x )       ( ( sizeof ( x ) == sizeof(double) ) ?           \
                              __signbitd ( x ) :                               \
                                ( sizeof ( x ) == sizeof( float) ) ?            \
                              __signbitf ( x ) :                               \
                              __signbitl  ( x ) )

#define __TYPE_LONGDOUBLE_IS_DOUBLE 1
                              
long  __fpclassifyd( double );
long  __fpclassifyf( float );
long  __fpclassify( long double );
#if __TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long __fpclassify( long double x ) { return __fpclassifyd((double)( x )); }
  #else
    #define __fpclassify( x ) (__fpclassifyd((double)( x )))
  #endif
#endif


long  __isnormald( double );
long  __isnormalf( float );
long  __isnormal( long double );
#if __TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long __isnormal( long double x ) { return __isnormald((double)( x )); }
  #else
    #define __isnormal( x ) (__isnormald((double)( x )))
  #endif
#endif

long  __isfinited( double );
long  __isfinitef( float );
long  __isfinite( long double );
#if __TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long __isfinite( long double x ) { return __isfinited((double)( x )); }
  #else
    #define __isfinite( x ) (__isfinited((double)( x )))
  #endif
#endif


long  __isinfd( double );
long  __isinff( float );
long  __isinf( long double );
#if __TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long __isinf( long double x ) { return __isinfd((double)( x )); }
  #else
    #define __isinf( x ) (__isinfd((double)( x )))
  #endif
#endif


long  __isnand( double );
long  __isnanf( float );
long  __isnan( long double );
#if __TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long __isnan( long double x ) { return __isnand((double)( x )); }
  #else
    #define __isnan( x ) (__isnand((double)( x )))
  #endif
#endif


long  __signbitd( double );
long  __signbitf( float );
long  __signbitl( long double );
#if __TYPE_LONGDOUBLE_IS_DOUBLE
  #ifdef __cplusplus
    inline long __signbitl( long double x ) { return __signbitd((double)( x )); }
  #else
    #define __signbitl( x ) (__signbitd((double)( x )))
  #endif
#endif

/********************************************************************************
*                                                                               *
*                              Math Functions                                   *
*                                                                               *
********************************************************************************/

double  acos( double );
double  asin( double );
double  atan( double );
double  atan2( double, double );
double  cos( double );
double  sin( double );
double  tan( double );
double  acosh( double );
double  asinh( double );
double  atanh( double );
double  cosh( double );
double  sinh( double );
double  tanh( double );

double exp ( double );
double exp2 ( double ); 
double expm1 ( double ); 
double log ( double );
double log10 ( double );
double log2 ( double );
double log1p ( double );

double logb ( double );
float logbf ( float );

double modf ( double, double * );
float modff ( float, float * );

double ldexp ( double, int );
float ldexpf ( float, int );

double frexp ( double, int * );
float frexpf ( float, int * );

int ilogb ( double );
int ilogbf ( float );

double scalbn ( double, int );
float scalbnf ( float, int  );

double scalbln ( double, long int );
float scalblnf ( float, long int );

double  fabs( double );
float  fabsf( float );

double  cbrt( double );
double hypot ( double, double );
double pow ( double, double );
double  sqrt( double );

double  erf( double );
double  erfc( double );
double  lgamma( double );
double  tgamma( double );

double ceil ( double );
float ceilf ( float );

double floor ( double );
float floorf ( float );

double nearbyint ( double );
float nearbyintf ( float );

double rint ( double );
float rintf ( float );

long int lrint ( double );
long int lrintf ( float );

long long int llrint ( double );
long long int llrintf ( float );

double round ( double );
float roundf ( float );

long int lround ( double );
long int lroundf ( float );

long long int llround ( double );
long long int llroundf ( float );

double trunc ( double );
float truncf ( float );

double fmod ( double, double );
float fmodf ( float, float );

double remainder ( double, double );
float remainderf ( float, float );

double remquo ( double, double, int * );
float remquof ( float, float, int * );

double copysign ( double, double );
float copysignf ( float, float );

double nan( const char * );
float nanf( const char * );

double nextafter ( double, double );
float nextafterf ( float, float );

double fdim ( double, double );
float fdimf ( float, float );

double fmax ( double, double );
float fmaxf ( float, float );

double fmin ( double, double );
float fminf ( float, float );

double fma ( double, double, double );
float fmaf ( float, float, float );

#define isgreater(x, y) __builtin_isgreater (x, y)
#define isgreaterequal(x, y) __builtin_isgreaterequal (x, y)
#define isless(x, y) __builtin_isless (x, y)
#define islessequal(x, y) __builtin_islessequal (x, y)
#define islessgreater(x, y) __builtin_islessgreater (x, y)
#define isunordered(x, y) __builtin_isunordered (x, y)

double  __inf( void );
float  __inff( void );
float  __nan( void );

#ifndef __NOEXTENSIONS__

#define FP_SNAN		FP_NAN
#define FP_QNAN		FP_NAN

long int rinttol ( double );

long int roundtol ( double );

typedef struct __complex_s {
        double Real;
        double Imag;
} __complex_t;

/*
 * XOPEN/SVID
 */
#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
#define	M_E		2.7182818284590452354	/* e */
#define	M_LOG2E		1.4426950408889634074	/* log 2e */
#define	M_LOG10E	0.43429448190325182765	/* log 10e */
#define	M_LN2		0.69314718055994530942	/* log e2 */
#define	M_LN10		2.30258509299404568402	/* log e10 */
#define	M_PI		3.14159265358979323846	/* pi */
#define	M_PI_2		1.57079632679489661923	/* pi/2 */
#define	M_PI_4		0.78539816339744830962	/* pi/4 */
#define	M_1_PI		0.31830988618379067154	/* 1/pi */
#define	M_2_PI		0.63661977236758134308	/* 2/pi */
#define	M_2_SQRTPI	1.12837916709551257390	/* 2/sqrt(pi) */
#define	M_SQRT2		1.41421356237309504880	/* sqrt(2) */
#define	M_SQRT1_2	0.70710678118654752440	/* 1/sqrt(2) */

#define	MAXFLOAT	((float)3.40282346638528860e+38)
extern int signgam;

#if !defined(_XOPEN_SOURCE)
enum fdversion {fdlibm_ieee = -1, fdlibm_svid, fdlibm_xopen, fdlibm_posix};

#define _LIB_VERSION_TYPE enum fdversion
#define _LIB_VERSION _fdlib_version  

/* if global variable _LIB_VERSION is not desirable, one may 
 * change the following to be a constant by: 
 *	#define _LIB_VERSION_TYPE const enum version
 * In that case, after one initializes the value _LIB_VERSION (see
 * s_lib_version.c) during compile time, it cannot be modified
 * in the middle of a program
 */ 
extern  _LIB_VERSION_TYPE  _LIB_VERSION;

#define _IEEE_  fdlibm_ieee
#define _SVID_  fdlibm_svid
#define _XOPEN_ fdlibm_xopen
#define _POSIX_ fdlibm_posix

#if !defined(__cplusplus)
struct exception {
	int type;
	char *name;
	double arg1;
	double arg2;
	double retval;
};
#endif

#define	HUGE		MAXFLOAT

/* 
 * set X_TLOSS = pi*2**52, which is possibly defined in <values.h>
 * (one may replace the following line by "#include <values.h>")
 */

#define X_TLOSS		1.41484755040568800000e+16 

#define	DOMAIN		1
#define	SING		2
#define	OVERFLOW	3
#define	UNDERFLOW	4
#define	TLOSS		5
#define	PLOSS		6

#endif /* !_XOPEN_SOURCE */
#endif /* !_ANSI_SOURCE && !_POSIX_SOURCE */

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
int finite ( double );

double gamma ( double );

double j0 ( double );
double j1 ( double );
double jn ( int, double );

double y0 ( double );
double y1 ( double );
double yn ( int, double );

#if !defined(_XOPEN_SOURCE)

double scalb ( double, int );

#if !defined(__cplusplus)
int matherr ( struct exception * );
#endif

/*
 * IEEE Test Vector
 */
double significand ( double );

/*
 * BSD math library entry points
 */
double cabs ( __complex_t );

double drem ( double, double );

/*
 * Reentrant version of gamma & lgamma; passes signgam back by reference
 * as the second argument; user must allocate space for signgam.
 */
#ifdef _REENTRANT
double gamma_r ( double, int * );
double lgamma_r ( double, int * );
#endif /* _REENTRANT */
#endif /* !_XOPEN_SOURCE */
#endif /* !_ANSI_SOURCE && !_POSIX_SOURCE */

#endif /* __NOEXTENSIONS__ */

#ifdef __cplusplus
}
#endif

#endif /* __MATH__ */
