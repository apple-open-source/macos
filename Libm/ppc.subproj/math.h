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

#include "sys/cdefs.h" /* For definition of __DARWIN_UNIX03 et al */

#if (!defined(__WANT_LONG_DOUBLE_FORMAT__))
#if defined(__APPLE_CC__) && defined(__LONG_DOUBLE_128__)
#define __WANT_LONG_DOUBLE_FORMAT__ 128
#else
#define __WANT_LONG_DOUBLE_FORMAT__ 64
#endif
#endif

#if ( __WANT_LONG_DOUBLE_FORMAT__ - 0L == 128L )
#define __LIBMLDBL_COMPAT(sym) __asm("_" __STRING(sym) "$LDBL128")
#elif ( __WANT_LONG_DOUBLE_FORMAT__ - 0L == 64L )
#define __LIBMLDBL_COMPAT(sym) /* NOTHING */
#else
#define __LIBMLDBL_COMPAT(sym) /* NOTHING */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
*       Floating point data types                                             *
******************************************************************************/

/*	Define float_t and double_t per C standard, ISO/IEC 9899:1999 7.12 2,
	taking advantage of GCC's __FLT_EVAL_METHOD__ (which a compiler may
	define anytime and GCC does) that shadows FLT_EVAL_METHOD (which a compiler
	must and may define only in float.h).
*/
#if __FLT_EVAL_METHOD__ == 0
	typedef float float_t;
	typedef double double_t;
#elif __FLT_EVAL_METHOD__ == 1
	typedef double float_t;
	typedef double double_t;
#elif __FLT_EVAL_METHOD__ == 2 || __FLT_EVAL_METHOD__ == -1
	typedef long double float_t;
	typedef long double double_t;
#else /* __FLT_EVAL_METHOD__ */
	#error "Unsupported value of __FLT_EVAL_METHOD__."
#endif /* __FLT_EVAL_METHOD__ */

#define	HUGE_VAL	1e500
#define	HUGE_VALF	1e50f
#define	HUGE_VALL	1e500L

#define INFINITY	HUGE_VALF

#if defined(__GNUC__)
#define NAN		__builtin_nanf("0x7fc00000") /* Constant expression, can be used as initializer. */
#else
#define NAN		__nan( )
#endif

/******************************************************************************
*      Taxonomy of floating point data types                                  *
******************************************************************************/

enum {
	_FP_NAN          = 1,                   /*      NaN                    */
	_FP_INFINITE     = 2,                   /*      + or - infinity        */
	_FP_ZERO         = 3,                   /*      + or - zero            */
	_FP_NORMAL       = 4,                   /*      all normal numbers     */
	_FP_SUBNORMAL    = 5,					/*      denormal numbers       */
	_FP_SUPERNORMAL  = 6                    /*      long double delivering > LDBL_DIG, e.g. 1. + 2^-1000 */
};

#define FP_NAN          _FP_NAN
#define FP_INFINITE     _FP_INFINITE
#define FP_ZERO         _FP_ZERO
#define FP_NORMAL       _FP_NORMAL
#define FP_SUBNORMAL    _FP_SUBNORMAL
#define FP_SUPERNORMAL  _FP_SUPERNORMAL

/* fma() *function call* is more costly than equivalent (in-line) multiply and add operations */
#undef FP_FAST_FMA
#undef FP_FAST_FMAF
#undef FP_FAST_FMAL

/* The values returned by `ilogb' for 0 and NaN respectively.  */
#define FP_ILOGB0	(-2147483647)
#define FP_ILOGBNAN	(2147483647)

/* Bitmasks for the math_errhandling macro.  */
#define MATH_ERRNO		1	/* errno set by math functions.  */
#define MATH_ERREXCEPT	2	/* Exceptions raised by math functions.  */

#define math_errhandling (__math_errhandling())
extern unsigned int __math_errhandling ( void );

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

#if (__WANT_LONG_DOUBLE_FORMAT__ - 0L == 128L)
#define      fpclassify( x )    ( ( sizeof ( (x) ) == sizeof(double) ) ?           \
                              __fpclassifyd  ( (double)(x) ) :                           \
                                ( sizeof ( (x) ) == sizeof( float) ) ?            \
                              __fpclassifyf ( (float)(x) ) :                            \
                              __fpclassify  ( ( long double )(x) ) )
#define      isnormal( x )      ( ( sizeof ( (x) ) == sizeof(double) ) ?           \
                              __isnormald ( (double)(x) ) :                              \
                                ( sizeof ( (x) ) == sizeof( float) ) ?            \
                              __isnormalf ( (float)(x) ) :                              \
                              __isnormal  ( ( long double )(x) ) )
#define      isfinite( x )      ( ( sizeof ( (x) ) == sizeof(double) ) ?           \
                              __isfinited ( (double)(x) ) :                              \
                                ( sizeof ( (x) ) == sizeof( float) ) ?            \
                              __isfinitef ( (float)(x) ) :                              \
                              __isfinite  ( ( long double )(x) ) )
#define      isinf( x )         ( ( sizeof ( (x) ) == sizeof(double) ) ?           \
                              __isinfd ( (double)(x) ) :                                 \
                                ( sizeof ( (x) ) == sizeof( float) ) ?            \
                              __isinff ( (float)(x) ) :                                 \
                              __isinf  ( ( long double )(x) ) )
#define      isnan( x )         ( ( sizeof ( (x) ) == sizeof(double) ) ?           \
                              __isnand ( (double)(x) ) :                                 \
                                ( sizeof ( (x) ) == sizeof( float) ) ?            \
                              __isnanf ( (float)(x) ) :                                 \
                              __isnan  ( ( long double )(x) ) )
#define      signbit( x )       ( ( sizeof ( (x) ) == sizeof(double) ) ?           \
                              __signbitd ( (double)(x) ) :                               \
                                ( sizeof ( (x) ) == sizeof( float) ) ?            \
                              __signbitf ( (float)(x) ) :                               \
                              __signbitl  ( ( long double )(x) ) )

extern int  __fpclassify( long double );
extern int  __isnormal( long double );
extern int  __isfinite( long double );
extern int  __isinf( long double );
extern int  __isnan( long double );
extern int  __signbitl( long double );

#else
#define fpclassify( x )	( ( sizeof ( (x) ) == sizeof( float) ) ? \
							__fpclassifyf ( (float)(x) ) : __fpclassifyd  ( (double)(x) ) )

#define isnormal( x )   ( ( sizeof ( (x) ) == sizeof( float) ) ? \
							__isnormalf ( (float)(x) ) : __isnormald  ( (double)(x) ) ) 

#define isfinite( x )   ( ( sizeof ( (x) ) == sizeof( float) ) ? \
							__isfinitef ( (float)(x) ) : __isfinited  ( (double)(x) ) )  

#define isinf( x )      ( ( sizeof ( (x) ) == sizeof( float) ) ? \
							__isinff ( (float)(x) ) : __isinfd  ( (double)(x) ) )   

#define isnan( x )      ( ( sizeof ( (x) ) == sizeof( float) ) ? \
							__isnanf ( (float)(x) ) : __isnand  ( (double)(x) ) ) 

#define signbit( x )    ( ( sizeof ( (x) ) == sizeof( float) ) ? \
							__signbitf ( (float)(x) ) : __signbitd  ( (double)(x) ) ) 
							  
#endif /* __WANT_LONG_DOUBLE_FORMAT__ */
                              
extern int  __fpclassifyd( double );
extern int  __fpclassifyf( float );

extern int  __isnormald( double );
extern int  __isnormalf( float );

extern int  __isfinited( double );
extern int  __isfinitef( float );

extern int  __isinfd( double );
extern int  __isinff( float );

extern int  __isnand( double );
extern int  __isnanf( float );

extern int  __signbitd( double );
extern int  __signbitf( float );


/********************************************************************************
*                                                                               *
*                              Math Functions                                   *
*                                                                               *
********************************************************************************/

extern double acos( double );
extern float acosf( float );

extern double asin( double );
extern float asinf( float );

extern double atan( double );
extern float atanf( float );

extern double atan2( double, double );
extern float atan2f( float, float );

extern double cos( double );
extern float cosf( float );

extern double sin( double );
extern float sinf( float );

extern double tan( double );
extern float tanf( float );

extern double acosh( double );
extern float acoshf( float );

extern double asinh( double );
extern float asinhf( float );

extern double atanh( double );
extern float atanhf( float );

extern double cosh( double );
extern float coshf( float );

extern double sinh( double );
extern float sinhf( float );

extern double tanh( double );
extern float tanhf( float );

extern double exp( double );
extern float expf( float );

extern double exp2( double );
extern float exp2f( float );

extern double expm1( double );
extern float expm1f( float );

extern double log( double );
extern float logf( float );

extern double log10( double );
extern float log10f( float );

extern double log2( double );
extern float log2f( float );

extern double log1p( double );
extern float log1pf( float );

extern double logb( double );
extern float logbf( float );

extern double modf( double, double * );
extern float modff( float, float * );

extern double ldexp( double, int );
extern float ldexpf( float, int );

extern double frexp( double, int * );
extern float frexpf( float, int * );

extern int ilogb( double );
extern int ilogbf( float );

extern double scalbn( double, int );
extern float scalbnf( float, int );

extern double scalbln( double, long int );
extern float scalblnf( float, long int );

extern double fabs( double );
extern float fabsf( float );

extern double cbrt( double );
extern float cbrtf( float );

extern double hypot( double, double );
extern float hypotf( float, float );

extern double pow( double, double );
extern float powf( float, float );

extern double sqrt( double );
extern float sqrtf( float );

extern double erf( double );
extern float erff( float );

extern double erfc( double );
extern float erfcf( float );

extern double lgamma( double );
extern float lgammaf( float );

extern double tgamma( double );
extern float tgammaf( float );

extern double ceil( double );
extern float ceilf( float );

extern double floor( double );
extern float floorf( float );

extern double nearbyint( double );
extern float nearbyintf( float );

extern double rint( double );
extern float rintf( float );

extern long int lrint( double );
extern long int lrintf( float );

extern long long int llrint( double );
extern long long int llrintf( float );

extern double round( double );
extern float roundf( float );

extern long int lround( double );
extern long int lroundf( float );

extern long long int llround( double );
extern long long int llroundf( float );

extern double trunc( double );
extern float truncf( float );

extern double fmod( double, double );
extern float fmodf( float, float );

extern double remainder( double, double );
extern float remainderf( float, float );

extern double remquo( double, double, int * );
extern float remquof( float, float, int * );

extern double copysign( double, double );
extern float copysignf( float, float );

extern double nan( const char * );
extern float nanf( const char * );

extern double nextafter( double, double );
extern float nextafterf( float, float );


extern double fdim( double, double );
extern float fdimf( float, float );

extern double fmax( double, double );
extern float fmaxf( float, float );

extern double fmin( double, double );
extern float fminf( float, float );

extern double fma( double, double, double );
extern float fmaf( float, float, float );

#if ( __WANT_LONG_DOUBLE_FORMAT__ - 0L > 0L ) 
extern long double acosl( long double ) __LIBMLDBL_COMPAT(acosl);
extern long double asinl( long double ) __LIBMLDBL_COMPAT(asinl);
extern long double atanl( long double ) __LIBMLDBL_COMPAT(atanl);
extern long double atan2l( long double, long double ) __LIBMLDBL_COMPAT(atan2l);
extern long double cosl( long double ) __LIBMLDBL_COMPAT(cosl);
extern long double sinl( long double ) __LIBMLDBL_COMPAT(sinl);
extern long double tanl( long double ) __LIBMLDBL_COMPAT(tanl);
extern long double acoshl( long double ) __LIBMLDBL_COMPAT(acoshl);
extern long double asinhl( long double ) __LIBMLDBL_COMPAT(asinhl);
extern long double atanhl( long double ) __LIBMLDBL_COMPAT(atanhl);
extern long double coshl( long double ) __LIBMLDBL_COMPAT(coshl);
extern long double sinhl( long double ) __LIBMLDBL_COMPAT(sinhl);
extern long double tanhl( long double ) __LIBMLDBL_COMPAT(tanhl);
extern long double expl( long double ) __LIBMLDBL_COMPAT(expl);
extern long double exp2l( long double ) __LIBMLDBL_COMPAT(exp2l);
extern long double expm1l( long double ) __LIBMLDBL_COMPAT(expm1l);
extern long double logl( long double ) __LIBMLDBL_COMPAT(logl);
extern long double log10l( long double ) __LIBMLDBL_COMPAT(log10l);
extern long double log2l( long double ) __LIBMLDBL_COMPAT(log2l);
extern long double log1pl( long double ) __LIBMLDBL_COMPAT(log1pl);
extern long double logbl( long double ) __LIBMLDBL_COMPAT(logbl);
extern long double modfl( long double, long double * ) __LIBMLDBL_COMPAT(modfl);
extern long double ldexpl( long double, int ) __LIBMLDBL_COMPAT(ldexpl);
extern long double frexpl( long double, int * ) __LIBMLDBL_COMPAT(frexpl);
extern int ilogbl( long double ) __LIBMLDBL_COMPAT(ilogbl);
extern long double scalbnl( long double, int ) __LIBMLDBL_COMPAT(scalbnl);
extern long double scalblnl( long double, long int ) __LIBMLDBL_COMPAT(scalblnl);
extern long double fabsl( long double ) __LIBMLDBL_COMPAT(fabsl);
extern long double cbrtl( long double ) __LIBMLDBL_COMPAT(cbrtl);
extern long double hypotl( long double, long double ) __LIBMLDBL_COMPAT(hypotl);
extern long double powl( long double, long double ) __LIBMLDBL_COMPAT(powl);
extern long double sqrtl( long double ) __LIBMLDBL_COMPAT(sqrtl);
extern long double erfl( long double ) __LIBMLDBL_COMPAT(erfl);
extern long double erfcl( long double ) __LIBMLDBL_COMPAT(erfcl);
extern long double lgammal( long double ) __LIBMLDBL_COMPAT(lgammal);
extern long double tgammal( long double ) __LIBMLDBL_COMPAT(tgammal);
extern long double ceill( long double ) __LIBMLDBL_COMPAT(ceill);
extern long double floorl( long double ) __LIBMLDBL_COMPAT(floorl);
extern long double nearbyintl( long double ) __LIBMLDBL_COMPAT(nearbyintl);
extern long double rintl( long double ) __LIBMLDBL_COMPAT(rintl);
extern long int lrintl( long double ) __LIBMLDBL_COMPAT(lrintl);
extern long long int llrintl( long double ) __LIBMLDBL_COMPAT(llrintl);
extern long double roundl( long double ) __LIBMLDBL_COMPAT(roundl);
extern long int lroundl( long double ) __LIBMLDBL_COMPAT(lroundl);
extern long long int llroundl( long double ) __LIBMLDBL_COMPAT(llroundl);
extern long double truncl( long double ) __LIBMLDBL_COMPAT(truncl);
extern long double fmodl( long double, long double) __LIBMLDBL_COMPAT(fmodl);
extern long double remainderl( long double, long double ) __LIBMLDBL_COMPAT(remainderl);
extern long double remquol( long double, long double, int * ) __LIBMLDBL_COMPAT(remquol);
extern long double copysignl( long double, long double ) __LIBMLDBL_COMPAT(copysignl);
extern long double nanl( const char * ) __LIBMLDBL_COMPAT(nanl);
extern long double nextafterl( long double, long double ) __LIBMLDBL_COMPAT(nextafterl);
extern double nexttoward( double, long double ) __LIBMLDBL_COMPAT(nexttoward);
extern float nexttowardf( float, long double ) __LIBMLDBL_COMPAT(nexttowardf);
extern long double nexttowardl( long double, long double ) __LIBMLDBL_COMPAT(nexttowardl);
extern long double fdiml( long double, long double ) __LIBMLDBL_COMPAT(fdiml);
extern long double fmaxl( long double, long double ) __LIBMLDBL_COMPAT(fmaxl);
extern long double fminl( long double, long double ) __LIBMLDBL_COMPAT(fminl);
extern long double fmal( long double, long double, long double ) __LIBMLDBL_COMPAT(fmal);
#endif /* __WANT_LONG_DOUBLE_FORMAT__ */

#define isgreater(x, y) __builtin_isgreater ((x),(y))
#define isgreaterequal(x, y) __builtin_isgreaterequal ((x),(y))
#define isless(x, y) __builtin_isless ((x),(y))
#define islessequal(x, y) __builtin_islessequal ((x),(y))
#define islessgreater(x, y) __builtin_islessgreater ((x),(y))
#define isunordered(x, y) __builtin_isunordered ((x),(y))

extern double  __inf( void );
extern float  __inff( void );
extern float  __nan( void ); /* 10.3 (and later) must retain in ABI for backward compatability */

#if !defined(_ANSI_SOURCE)
extern double j0 ( double );
extern double j1 ( double );
extern double jn ( int, double );

extern double y0 ( double );
extern double y1 ( double );
extern double yn ( int, double );

#if __DARWIN_UNIX03
extern double scalb ( double, double )  __DARWIN_ALIAS(scalb); /* UNIX03 legacy signature */
#else
extern double scalb ( double, int ); /* Mac OS X legacy signature */
#endif /* __DARWIN_UNIX03 */

#define M_E         2.71828182845904523536028747135266250   /* e */
#define M_LOG2E     1.44269504088896340735992468100189214   /* log_2(e) */
#define M_LOG10E    0.434294481903251827651128918916605082  /* log_10(e) */
#define M_LN2       0.693147180559945309417232121458176568  /* log_e(2) */
#define M_LN10      2.30258509299404568401799145468436421   /* log_e(10) */
#define M_PI        3.14159265358979323846264338327950288   /* pi */
#define M_PI_2      1.57079632679489661923132169163975144   /* pi/2 */
#define M_PI_4      0.785398163397448309615660845819875721  /* pi/4 */
#define M_1_PI      0.318309886183790671537767526745028724  /* 1/pi */
#define M_2_PI      0.636619772367581343075535053490057448  /* 2/pi */
#define M_2_SQRTPI  1.12837916709551257389615890312154517   /* 2/sqrt(pi) */
#define M_SQRT2     1.41421356237309504880168872420969808   /* sqrt(2) */
#define M_SQRT1_2   0.707106781186547524400844362104849039  /* 1/sqrt(2) */

#define	MAXFLOAT	((float)3.40282346638528860e+38)
extern int signgam;

#endif /* !defined(_ANSI_SOURCE) */

#if !defined(__NOEXTENSIONS__) && !defined(_POSIX_C_SOURCE)
#define __WANT_EXTENSIONS__
#endif

#ifdef __WANT_EXTENSIONS__

#define FP_SNAN		FP_NAN
#define FP_QNAN		FP_NAN

extern long int rinttol ( double );

extern long int roundtol ( double );

typedef struct __complex_s {
        double Real;
        double Imag;
} __complex_t;

/*
 * XOPEN/SVID
 */
#if !defined(_ANSI_SOURCE) && !defined(_POSIX_C_SOURCE)

#if !defined(_XOPEN_SOURCE)
enum fdversion {_fdlibm_ieee = -1, _fdlibm_svid, _fdlibm_xopen, _fdlibm_posix}; /* Legacy fdlibm constructs */
#define fdlibm_ieee _fdlibm_ieee
#define fdlibm_svid _fdlibm_svid
#define fdlibm_xopen _fdlibm_xopen
#define fdlibm_posix _fdlibm_posix

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
#endif /* !_ANSI_SOURCE && !_POSIX_C_SOURCE */

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_C_SOURCE)
extern int finite ( double );

extern double gamma ( double );

#if !defined(_XOPEN_SOURCE)

#if !defined(__cplusplus)
extern int matherr ( struct exception * );
#endif

/*
 * IEEE Test Vector
 */
extern double significand ( double );

/*
 * BSD math library entry points
 */
extern double drem ( double, double );

/*
 * Reentrant version of gamma & lgamma; passes signgam back by reference
 * as the second argument; user must allocate space for signgam.
 */
#ifdef _REENTRANT
extern double gamma_r ( double, int * );
extern double lgamma_r ( double, int * );
#endif /* _REENTRANT */
#endif /* !_XOPEN_SOURCE */
#endif /* !_ANSI_SOURCE && !_POSIX_C_SOURCE */

#endif /* __WANT_EXTENSIONS__ */

/*
 * The following replacements for libm's floor, floorf, ceil, and ceilf are activated
 * when the flag "-ffast-math" is passed to the gcc compiler. These functions do not
 * distinguish between -0.0 and 0.0, so are not IEC6509 compliant for argument -0.0.
 */
#if defined(__FAST_MATH__) && !defined(__cplusplus)

#define __FSELS(e,t,f) (((e) >= 0.0f) ? (t) : (f))
#define __FSEL(e,t,f) (((e) >= 0.0) ? (t) : (f))

static inline float __fastmath_floorf( float f ) __attribute__((always_inline));
static inline float __fastmath_floorf( float f )
{
    float b, c, d, e, g, h, t;

    c = __FSELS( f, -0x1.0p+23f, 0x1.0p+23f );          b = fabsf( f ); 
    d = f - c;                                          e = b - 0x1.0p+23f;                        
	__asm__("" : "+f" (d));	/* Tell compiler value of d cannot be optimized away. */
	d = d + c;
    g = f - d;
    h = __FSELS( g, 0.0f, 1.0f );
    t = d - h;
    return __FSELS( e, f, t );
}

static inline float __fastmath_ceilf( float f ) __attribute__((always_inline));
static inline float __fastmath_ceilf( float f )
{
    float b, c, d, e, g, h, t;

    c = __FSELS( f, -0x1.0p+23f, 0x1.0p+23f );          b = fabsf( f ); 
    d = f - c;                                          e = b - 0x1.0p+23f;                        
	__asm__("" : "+f" (d));	/* Tell compiler value of d cannot be optimized away. */
	d = d + c;
    g = d - f;
    h = __FSELS( g, 0.0f, 1.0f );
    t = d + h;
    return __FSELS( e, f, t );
}

static inline double __fastmath_floor( double f ) __attribute__((always_inline));
static inline double __fastmath_floor( double f )
{
    double b, c, d, e, g, h, t;

    c = __FSEL( f, -0x1.0p+52, 0x1.0p+52 );             b = fabs( f );      
    d = f - c;                                          e = b - 0x1.0p+52;                    
	__asm__("" : "+f" (d));	/* Tell compiler value of d cannot be optimized away. */
	d = d + c;
    g = f - d;
    h = __FSEL( g, 0.0, 1.0 );
    t = d - h;
    return __FSEL( e, f, t );
}

static inline double __fastmath_ceil( double f ) __attribute__((always_inline));
static inline double __fastmath_ceil( double f )
{
    double b, c, d, e, g, h, t;

    c = __FSEL( f, -0x1.0p+52, 0x1.0p+52 );             b = fabs( f );      
    d = f - c;                                          e = b - 0x1.0p+52;                    
	__asm__("" : "+f" (d));	/* Tell compiler value of d cannot be optimized away. */
	d = d + c;
    g = d - f;
    h = __FSEL( g, 0.0, 1.0 );
    t = d + h;
    return __FSEL( e, f, t );
}

#define floorf(x) __fastmath_floorf((x))
#define ceilf(x) __fastmath_ceilf((x))
#define floor(x) __fastmath_floor((x))
#define ceil(x) __fastmath_ceil((x))

#endif /* __FAST_MATH__ && !__cplusplus */

#ifdef __cplusplus
}
#endif

#endif /* __MATH__ */
