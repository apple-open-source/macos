/*
 *  xmm_floor.c
 *  xmmLibm
 *
 *  Created by Ian Ollmann on 8/15/05.
 *  Copyright 2005 Apple Computer. All rights reserved.
 *
 */

#if defined( __i386__ )

#include "xmmLibm_prefix.h"

static const long double stepL = 0x1.0p63;
static const long double oneL = 1.0L;

static const double stepD = 0x1.0p52;
static const double oneD = 1.0;

static const float stepF = 0x1.0p23f;
static const float oneF = 1.0f;

#if defined( BUILDING_FOR_CARBONCORE_LEGACY )

float modff( float f, float *i )
{
    xFloat x = FLOAT_2_XFLOAT( f );

    //load some constants
    xFloat step = _mm_load_ss( &stepF );
    xFloat one = _mm_load_ss( &oneF );

    //set aside sign and take absolute value
    xFloat sign = _mm_and_ps( minusZeroF, x );
    xFloat fabsx = _mm_andnot_ps( minusZeroF, x );
	xFloat isNaN = _mm_cmpunord_ss( x, x );
	xFloat safeX = _mm_andnot_ps( isNaN, fabsx );
	xFloat isNotInt = _mm_cmplt_ssm( safeX, &stepF );
    
    //set step to zero if |x| is guaranteed to be an integer (i.e. >= 0x1.0p52
    step = _mm_and_ps( step, isNotInt );

    //test for x == 0.0 special case
    xFloat xGTzero = _mm_cmplt_ss( minusZeroF, safeX );

    //round to integer
    xFloat r = _mm_add_ss( safeX, step );
    r = _mm_sub_ss( r, step );
    
    //Fix up mistaken rounding for rounding modes other than round to -Inf
    r = _mm_sub_ss( r, _mm_and_ps( one, _mm_cmplt_ss( safeX, r ) ) );

    //restore the sign
    r = _mm_or_ps( r, sign );
    
    //fix up -0.0
    r = _mm_sel_ps( x, r, xGTzero );
    *i = XFLOAT_2_FLOAT( r );
	
	r = _mm_and_ps( r, isNotInt );
	x = _mm_and_ps( x, isNotInt );
    x = _mm_sub_ss( x, r );
	x = _mm_or_ps( x, sign );

    f = XFLOAT_2_FLOAT( x );

    return f;
}

#else

double floor( double f )
{
    xDouble x = DOUBLE_2_XDOUBLE( f );

    //load some constants
    xDouble step = _mm_load_sd( &stepD );
    xDouble one = _mm_load_sd( &oneD );

    //set aside sign and take absolute value
    xDouble sign = _mm_and_pd( minusZeroD, x );
    xDouble fabsx = _mm_andnot_pd( minusZeroD, x );
	xDouble isNaN = _mm_cmpunord_pd( x, x );
	xDouble safeX = _mm_andnot_pd( isNaN, fabsx );	//zero any NaNs to prevent invalid flag from being set
    
    //set step to zero if |x| is guaranteed to be an integer (i.e. >= 0x1.0p52
    step = _mm_and_pd( step, _mm_cmplt_sdm( safeX, &stepD ) );

    //test for x == 0.0 special case
    xDouble xGTzero = _mm_cmplt_sd( minusZeroD, safeX );

    //round to integer
    xDouble intX = _mm_add_sd( safeX, step );
    intX = _mm_sub_sd( intX, step );
    
    //restore the sign
    intX = _mm_or_pd( intX, sign );
	safeX = _mm_or_pd( safeX, sign );
    
    //Fix up mistaken rounding for rounding modes other than round to -Inf
    intX = _mm_sub_sd( intX, _mm_and_pd( one, _mm_cmplt_sd( safeX, intX ) ) );

    //fix up -0.0 and NaN
    intX = _mm_sel_pd( x, intX, xGTzero );

    f = XDOUBLE_2_DOUBLE( intX );

    return f;
}

float floorf( float f )
{
    xFloat x = FLOAT_2_XFLOAT( f );

    //load some constants
    xFloat step = _mm_load_ss( &stepF );
    xFloat one = _mm_load_ss( &oneF );

    //set aside sign and take absolute value
    xFloat sign = _mm_and_ps( minusZeroF, x );
    xFloat fabsx = _mm_andnot_ps( minusZeroF, x );
	xFloat isNaN = _mm_cmpunord_ps( x, x );
	xFloat safeX = _mm_andnot_ps( isNaN, fabsx );	//zero any NaNs to prevent invalid flag from being set
    
    //set step to zero if |x| is guaranteed to be an integer (i.e. >= 0x1.0p52
    step = _mm_and_ps( step, _mm_cmplt_ssm( safeX, &stepF ) );

    //test for x == 0.0 special case
    xFloat xGTzero = _mm_cmplt_ss( minusZeroF, safeX );

    //round to integer
    xFloat intX = _mm_add_ss( safeX, step );
    intX = _mm_sub_ss( intX, step );
    
    //restore the sign
    intX = _mm_or_ps( intX, sign );
	safeX = _mm_or_ps( safeX, sign );
    
    //Fix up mistaken rounding for rounding modes other than round to -Inf
    intX = _mm_sub_ss( intX, _mm_and_ps( one, _mm_cmplt_ss( safeX, intX ) ) );

    //fix up -0.0 and NaN
    intX = _mm_sel_ps( x, intX, xGTzero );

    f = XFLOAT_2_FLOAT( intX );

    return f;
}

double ceil( double f )
{
    xDouble x = DOUBLE_2_XDOUBLE( f );

    //load some constants
    xDouble step = _mm_load_sd( &stepD );
    xDouble one = _mm_load_sd( &oneD );

    //set aside sign and take absolute value
    xDouble sign = _mm_and_pd( minusZeroD, x );
    xDouble fabsx = _mm_andnot_pd( minusZeroD, x );
	xDouble isNaN = _mm_cmpunord_pd( x, x );
	xDouble safeX = _mm_andnot_pd( isNaN, x );
    
    //set step to zero if |x| is guaranteed to be an integer (i.e. >= 0x1.0p52
	xSInt64 isSmall = _mm_cmpgt_epi32( (xSInt32) step, (xSInt32) fabsx );
	isSmall = _mm_shuffle_epi32( isSmall, 0xf5 );
	step = _mm_or_pd( step, sign );
    step = _mm_and_pd( step, (xDouble) isSmall );

    //round to integer
    xDouble intX = _mm_add_sd( x, step );
    intX = _mm_sub_sd( intX, step );
    
    //Fix up mistaken rounding for rounding modes other than round to -Inf
    intX = _mm_add_sd( intX, _mm_and_pd( one, _mm_cmplt_sd( _mm_andnot_pd( isNaN, intX), safeX ) ) );

    //fix up the sign of the result if zero
    intX = _mm_or_pd( intX, sign );
	
    f = XDOUBLE_2_DOUBLE( intX );

    return f;
}

float ceilf( float f )
{
    xFloat x = FLOAT_2_XFLOAT( f );

    //load some constants
    xFloat step = _mm_load_ss( &stepF );
    xFloat one = _mm_load_ss( &oneF );

    //set aside sign and take absolute value
    xFloat sign = _mm_and_ps( minusZeroF, x );
    xFloat fabsx = _mm_andnot_ps( minusZeroF, x );
	xFloat isNaN = _mm_cmpunord_ps( x, x );
	xFloat safeX = _mm_andnot_ps( isNaN, x );
    
    //set step to zero if |x| is guaranteed to be an integer (i.e. >= 0x1.0p52
	xFloat isSmall = (xFloat) _mm_cmpgt_epi32( (xSInt32) step, (xSInt32) fabsx );
	step = _mm_or_ps( step, sign );
    step = _mm_and_ps( step, isSmall );

    //round to integer
    xFloat intX = _mm_add_ss( x, step );
    intX = _mm_sub_ss( intX, step );
    
    //Fix up mistaken rounding for rounding modes other than round to -Inf
    intX = _mm_add_ss( intX, _mm_and_ps( one, _mm_cmplt_ss( _mm_andnot_ps( isNaN, intX), safeX ) ) );

    //fix up the sign of the result if zero
    intX = _mm_or_ps( intX, sign );
	
    f = XFLOAT_2_FLOAT( intX );

    return f;
}

static inline double _xrint( double f ) ALWAYS_INLINE;
static inline float _xrintf( float f ) ALWAYS_INLINE;

static inline double _xrint( double f )
{
    xDouble x = DOUBLE_2_XDOUBLE( f );

    //load some constants
    xDouble step = _mm_load_sd( &stepD );

    //set aside sign and take absolute value
    xDouble sign = _mm_and_pd( minusZeroD, x );
    xDouble fabsx = _mm_andnot_pd( minusZeroD, x );
	xDouble isNaN = _mm_cmpunord_sd( x, x );
	xDouble safeX = _mm_andnot_pd( isNaN, fabsx );	//flush NaNs to zero to avoid invalid flag
	
    //set step to zero if |x| is guaranteed to be an integer (i.e. >= 0x1.0p23
	xDouble isSmall = _mm_cmplt_sd( safeX, step );  
	step = _mm_and_pd( step, isSmall );

	//set the sign on step
	step = _mm_or_pd( step, sign );

    //round to integer
    x = _mm_add_sd( x, step );
    x = _mm_sub_sd( x, step );
        
    //set the sign of the result to be the same as the input
    x = _mm_or_pd( x, sign );

    f = XDOUBLE_2_DOUBLE( x );

    return f;
}


static inline float _xrintf( float f )
{
    xFloat x = FLOAT_2_XFLOAT( f );

    //load some constants
    xFloat step = _mm_load_ss( &stepF );

    //set aside sign and take absolute value
    xFloat sign = _mm_and_ps( minusZeroF, x );
    xFloat fabsx = _mm_andnot_ps( minusZeroF, x );
	
    //set step to zero if |x| is guaranteed to be an integer (i.e. >= 0x1.0p23
	xSInt32 isLarge = _mm_cmpgt_epi32( (xSInt32) fabsx, (xSInt32) step );  //use integer compare to avoid invalid flag with NaN
	step = _mm_andnot_ps( (xFloat) isLarge, step );

	//set the sign on step
	step = _mm_or_ps( step, sign );

    //round to integer
    x = _mm_add_ss( x, step );
    x = _mm_sub_ss( x, step );
        
    //set the sign of the result to be the same as the input
    x = _mm_or_ps( x, sign );

    f = XFLOAT_2_FLOAT( x );

    return f;
}

double rint( double f ){ return _xrint( f );  }
float rintf( float f  ){ return _xrintf( f );  }

double nearbyint( double f )
{
    int mxcsr = _mm_getcsr();
    int new_mxcsr = mxcsr | INEXACT_MASK;
    if( new_mxcsr != mxcsr )
        _mm_setcsr( new_mxcsr );

    f = _xrint( f );
    
    _mm_setcsr( mxcsr );

    return f;
}

float nearbyintf( float f )
{
    int mxcsr = _mm_getcsr();
    int new_mxcsr = mxcsr | INEXACT_MASK;
    if( new_mxcsr != mxcsr )
        _mm_setcsr( new_mxcsr );

    f = _xrintf( f );
    
    _mm_setcsr( mxcsr );

    return f;
}

#define GET_FCW()      ({ unsigned short _result; asm volatile ("fnstcw %0" : "=m" (_result)::"memory"); /*return*/ _result; })
#define SET_FCW(_a)    { unsigned short _aa = _a; asm volatile ("fldcw %0" : :"m" (_aa)); }


static inline long long int _llrint( long double x )  ALWAYS_INLINE;
static inline long long int _llrint( long double x )
{ 
	int64_t result = CVTLD_SI64( x ); 
	int64_t mask = x >= 0x1.0p63L;

	return result ^ -mask;
}

long long int  llrint( double x ){ return _llrint(x); }
long long int  llrintf( float x ){ return _llrint(x); }

#if defined( __LP64__ )
    long int  lrint( double x ){ return _llrint(x); }
    long int lrintf( float x ){ return _llrint(x); }
    long int  lrintl( double x ){ return llrintl(x); }
#else
    long int  lrint( double x )
	{ 
		int result = _mm_cvtsd_si32( DOUBLE_2_XDOUBLE(x) ); 
		int test = result == 0x80000000;		//1 for over/underflow
		int test2 = x > 2147483647.0;		//1 for overflow
		test &= test2;
		return result ^ -test;
	}
    long int  lrintf( float x )
	{ 
		int test = x >= 0x1.0p31f;
		int result = _mm_cvtss_si32( FLOAT_2_XFLOAT(x) ); 
		return result ^ (-test);
	}
#endif


static inline long long int _xllround( double x ) ALWAYS_INLINE;
static inline long long int _xllround( double x )
{
    long double lx = x;
    long long int result;
    int index = x < 0.0;
    long double fabslx = __builtin_fabs( x );
    const float limit[4] = {0.0f, 0.0f, 0x1.0p64f, -0x1.0p64f };
    const float addend[4] = {0.0f, 0.0f, 0.5f, -0.5f };

    index += 2 * ( fabslx < 0x1.0p64L );    // 0: x >= 2**64, 1: x <= -2**64,  2: 0 <= x < 2**64, 3: -2**64 < x <= 0

    //check to see if lx is an int. Set inexact flag if it is not.
    long double test = lx - limit[ index ];
    test += limit[ index ];
    
    //if not an int, add signed 0.5
    if( test != lx )
        lx += addend[ index ];
    
    result = CVTTLD_SI64( lx );
    
    return result;
}


#if defined( __LP64__ )
    long int  lround( double x ){ return _xllround( x ); }
    long int  lroundf( float x ){ return _xllround( x ); }
#else
	//This has a bug: For -2147483649 < x < -2147483648.5, this function returns inexact in addition to invald.
    long int  lround( double x )
    {
        static const double half = 0.5;
		static const double large = 0x1.0p31;
		static const double larger = 0x1.00000002p31;
	
        xDouble xx = DOUBLE_2_XDOUBLE( x );
        xDouble sign   = _mm_and_pd( xx, minusZeroD );
        xDouble fabsxx = _mm_andnot_pd( minusZeroD, xx );
		xDouble isNotLarger = _mm_cmplt_sdm( fabsxx, &larger );  //invalid flag okay here for NaN

		//calculate floor( |x| )	
		xDouble step = _mm_load_sd( &stepD );
		step = _mm_and_pd( step, isNotLarger );		
        xDouble floor = _mm_sub_sd( _mm_add_sd( fabsxx, step ), step );
		floor = _mm_sub_sd( floor, _mm_and_pd( _mm_load_sd( &oneD ), _mm_cmplt_sd( fabsxx, floor ) ) );

		//calculate fabsxx + min( 0.5, floor( fabsxx))
		xDouble diff = _mm_sub_sd( fabsxx, floor );
		xDouble addend = _mm_min_sdm( diff, &half );
		fabsxx = _mm_add_sd( fabsxx, addend );
		
		//set the sign
		fabsxx = _mm_or_pd( fabsxx, sign );
		
		int overflow = _mm_ucomige_sd( fabsxx, _mm_load_sd( &large ) );
		int result = _mm_cvttsd_si32( fabsxx );
		
		return result ^ (-overflow );
    }

    long int  lroundf( float x )
    {
        static const float half = 0.5f;
		static const float limit = 0x1.0p31f;
		static const float small = 0x1.0p23f;
	
        xFloat xx = FLOAT_2_XFLOAT( x );
        xFloat sign   = _mm_and_ps( xx, minusZeroF );
        xFloat fabsxx = _mm_andnot_ps( minusZeroF, xx );

		//calculate trunc(xx)
		xFloat isSmall = _mm_cmplt_ssm( fabsxx, &small );
		xFloat trunc = _mm_and_ps( isSmall, xx );					//clip large integers to zero
		trunc = _mm_cvtsi32_ss( trunc, _mm_cvttss_si32( trunc ) );	//truncate
		trunc = _mm_or_ps( trunc, _mm_andnot_ps( isSmall, xx ));		//or in large xx again

		//if the difference between floor and safex >= 0.5, add 0.5. Otherwise add 0.
		xFloat diff = _mm_sub_ss( xx, trunc );
		xFloat addend = _mm_min_ssm( _mm_andnot_ps( minusZeroF, diff), &half );	//use a smaller value than 0.5 if under 0.5. Should be exact. This is cheaper than slamming zero here
		addend = _mm_or_ps( addend, sign );
		addend = _mm_and_ps( addend, isSmall );
		xx = _mm_add_ss( xx, addend );
		
		int overflow = _mm_ucomige_ss( xx, _mm_load_ss( &limit) );
		int result = _mm_cvttss_si32( xx );
		
		return result ^ (-overflow);		
    }
#endif

long long int  llround( double x ){ return _xllround( x ); }
long long int  llroundf( float x ){ return _xllround( x ); }

double trunc( double f )
{
    xDouble x = DOUBLE_2_XDOUBLE( f );

    //load some constants
    xDouble step = _mm_load_sd( &stepD );
    xDouble one = _mm_load_sd( &oneD );

    //set aside sign and take absolute value
    xDouble sign = _mm_and_pd( minusZeroD, x );
    xDouble fabsx = _mm_andnot_pd( minusZeroD, x );
	xDouble isNaN = _mm_cmpunord_sd( x, x );
	xDouble safeX = _mm_andnot_pd( isNaN, fabsx );
    
    //set step to zero if |x| is guaranteed to be an integer (i.e. >= 0x1.0p52
    step = _mm_and_pd( step, _mm_cmplt_sdm( safeX, &stepD ) );

    //test for x == 0.0 special case
    xDouble xGTzero = _mm_cmplt_sd( minusZeroD, safeX );

    //round to integer
    xDouble r = _mm_add_sd( safeX, step );
    r = _mm_sub_sd( r, step );
    
    //Fix up mistaken rounding for rounding modes other than round to -Inf
    r = _mm_sub_sd( r, _mm_and_pd( one, _mm_cmplt_sd( safeX, r ) ) );

    //restore the sign
    r = _mm_or_pd( r, sign );
    
    //fix up -0.0
    r = _mm_sel_pd( x, r, xGTzero );

    f = XDOUBLE_2_DOUBLE( r );

    return f;
}

float truncf( float f )
{
    xFloat x = FLOAT_2_XFLOAT( f );

    //load some constants
    xFloat step = _mm_load_ss( &stepF );
    xFloat one = _mm_load_ss( &oneF );

    //set aside sign and take absolute value
    xFloat sign = _mm_and_ps( minusZeroF, x );
    xFloat fabsx = _mm_andnot_ps( minusZeroF, x );
	xFloat isNaN = _mm_cmpunord_ss( x, x );
	xFloat safeX = _mm_andnot_ps( isNaN, fabsx );
    
    //set step to zero if |x| is guaranteed to be an integer (i.e. >= 0x1.0p52
    step = _mm_and_ps( step, _mm_cmplt_ssm( safeX, &stepF ) );

    //test for x == 0.0 special case
    xFloat xGTzero = _mm_cmplt_ss( minusZeroF, safeX );

    //round to integer
    xFloat r = _mm_add_ss( safeX, step );
    r = _mm_sub_ss( r, step );
    
    //Fix up mistaken rounding for rounding modes other than round to -Inf
    r = _mm_sub_ss( r, _mm_and_ps( one, _mm_cmplt_ss( safeX, r ) ) );

    //restore the sign
    r = _mm_or_ps( r, sign );
    
    //fix up -0.0
    r = _mm_sel_ps( x, r, xGTzero );

    f = XFLOAT_2_FLOAT( r );

    return f;
}

double modf( double f, double *i )
{
    xDouble x = DOUBLE_2_XDOUBLE( f );

    //load some constants
    xDouble step = _mm_load_sd( &stepD );
    xDouble one = _mm_load_sd( &oneD );

    //set aside sign and take absolute value
    xDouble sign = _mm_and_pd( minusZeroD, x );
    xDouble fabsx = _mm_andnot_pd( minusZeroD, x );
	xDouble isNaN = _mm_cmpunord_sd( x, x );
	xDouble safeX = _mm_andnot_pd( isNaN, fabsx );
	xDouble isNotInt = _mm_cmplt_sdm( safeX, &stepD );
    
    //set step to zero if |x| is guaranteed to be an integer (i.e. >= 0x1.0p52
    step = _mm_and_pd( step, isNotInt );

    //test for x == 0.0 special case
    xDouble xGTzero = _mm_cmplt_sd( minusZeroD, safeX );

    //round to integer
    xDouble r = _mm_add_sd( safeX, step );
    r = _mm_sub_sd( r, step );
    
    //Fix up mistaken rounding for rounding modes other than round to -Inf
    r = _mm_sub_sd( r, _mm_and_pd( one, _mm_cmplt_sd( safeX, r ) ) );

    //restore the sign
    r = _mm_or_pd( r, sign );
    
    //fix up -0.0
    r = _mm_sel_pd( x, r, xGTzero );
    *i = XDOUBLE_2_DOUBLE( r );
	
	r = _mm_and_pd( r, isNotInt );
	x = _mm_and_pd( x, isNotInt );
    x = _mm_sub_sd( x, r );
	x = _mm_or_pd( x, sign );

    f = XDOUBLE_2_DOUBLE( x );

    return f;
}

#endif /* CARBON_CORE */
#endif /* defined( __i386__ ) */
