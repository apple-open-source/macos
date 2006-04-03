/*
 *  xmm_arcsincos.c
 *  xmmLibm
 *
 *  Created by Ian Ollmann, Ph.D. on 8/2/05.
 *  Copyright © 2005 Apple Computer, Inc. All rights reserved.
 *
 *  Based on code by Jim Thomas for 68881/68882 Macintoshes
 *  underlying arc tangent follows algorithm by Ali Sazegari, Jan 1991
 *          which in turn was based on SANE pack 5 for 68881/68882
 *          by Ali Sazegari and pascal by K Hanson and C McMaster
 */

#if defined( __i386__ )

#include "xmmLibm_prefix.h"
#include <math.h>

static inline xDouble _fatn( xDouble ) ALWAYS_INLINE;
static inline xDouble _xatan( xDouble ) ALWAYS_INLINE;
static inline xDouble _xasin( xDouble ) ALWAYS_INLINE;
static inline xDouble _xacos( xDouble ) ALWAYS_INLINE;

static const union
{
    uint64_t    u;
    double      d;
}inverseTrigNaN = { 0x7ff8000000000022ULL };


#pragma mark -

static const double q0 =  0x1.880B38AA99E19p+5; //49.0054791763506685243
static const double q1 =  0x1.9B497D4358B3Ep+6; //102.821766903199176113
static const double q2 =  0x1.1C6FDB0AE1716p+6; //71.1092340183352860095
static const double q3 =  0x1.1EF299B1679BEp+4; //17.9342286043045007204
static const double q4 =  0x1.31A63D5C55F4Cp+0; //1.19394286636630209131

static const double p0 =                   0.0; //                0
static const double p1 =  0x1.055CD071BBEBBp+4; //16.3351597254502216572
static const double p2 =  0x1.8790B27BF4749p+4; //24.47282646579632015
static const double p3 =  0x1.4476D8263F242p+3; //10.1395073649410996097
static const double p4 =  0x1.0000000000000p+0; //                1

static inline xDouble _fatn( xDouble x )
{
    x = _mm_mul_sd( x, x );         //x = x*x
    
    //num   = ( x * ( p[1].dbl + x * ( p[2].dbl + x * ( p[3].dbl + x ) ) ) );
    //denum = ( q[0].dbl + x * ( q[1].dbl + x * ( q[2].dbl + x * ( q[3].dbl + q[4].dbl * x ) ) ) );
    xDouble denom = _mm_mul_sdm( x, &q4);       xDouble num = _mm_add_sdm( x, &p3 );
    denom = _mm_add_sdm( denom, &q3 );          num = _mm_mul_sd( num, x );
    denom = _mm_mul_sd( denom, x );             num = _mm_add_sdm( num, &p2 );
    denom = _mm_add_sdm( denom, &q2 );          num = _mm_mul_sd( num, x );
    denom = _mm_mul_sd( denom, x );             num = _mm_add_sdm( num, &p1 );
    denom = _mm_add_sdm( denom, &q1 );          num = _mm_mul_sd( num, x );
    denom = _mm_mul_sd( denom, x );             
    denom = _mm_add_sdm( denom, &q0 );             

    return _mm_div_sd( num, denom );
}


static const double FPKX2       =  0x1.279A74590331Cp-1; //0.577350269189625731059
static const double FPKX2FX2    =  0x1.B8550D62BFB6Dp-5; //0.0537514935913268945833
static const double FPKATNCONS  =  0x1.126145E9ECD56p-2; //0.267949192431122695801
static const double FPKPI2      =  0x1.921FB54442D18p+0; //1.570796326794896558
static const double one = 1.0;


static inline xDouble _xatan( xDouble x )
{
    //get input NaNs out of the way
    if( _mm_istrue_sd( _mm_cmpunord_sd( x, x ) ) )
        return _mm_add_sd( x, x );

//      About the argument reduction:  arctg(x) is evaluated for 0 <= x <= 1    //
//      with either a Short or a Long formula depending on the value of x.      //
//                                                                              //
//      Recall the identities:                                                  //
//                                                                              //
//      arctg(-x)  = -arctg(x)                                                  //
//      arctg(1/x) =  pi/2 - arctg(x)                                           //
//                                                                              //
//      If x < 0 then arctg(-x) is computed, and the result negated.            //
//      If |x| > 1 then arctg(1/|x|) is computed and the result subtracted      //
//      from π/2.                                                               //
//                                                                              //
//      To compute atan of reduced x use Short or Long formula:                 //
//                                                                              //
//      if ( x <= ATnCons =~ 0.267 ) then:                                      //
//            arctg(x) = x - x * P( x * x ) / Q( x * x ),                       //
//      else  arctg(x) = x - ( A + ( B * P( B * B ) / Q( B * B ) + x2fx2 ) ).   //
//                                                                              //
//      where      x2 and x2fx2 are constants, about 0.5 and 0.05 and           //
//            A = ( x - x2 ) / ( 1 + ( 1 / ( x * x2 ) ) ),                      //
//            B = ( x - x2 ) / ( 1 + ( x * x2 ) ).                              //

    
    //Keep track of the original sign, so we can set it back later
    xDouble sign = _mm_and_pd( minusZeroD, x );
    xDouble y = _mm_andnot_pd( minusZeroD, x );    //set x = fabs(x)
    int isLarge = _mm_isfalse_sd( _mm_cmple_sdm( y, &one ) ); 
    
    if( isLarge )    // ! y <= 1.0
        y = _mm_div_sd( _mm_load_sd( &one ), y );


    //we can now be sure that  0 <= y <= 1
    if( _mm_istrue_sd( _mm_cmple_sdm( y, &FPKATNCONS ) ) ) // y <= 0.26794919243
    {
		static const double small = 0x1.0p-63;
		static const double scale = 0x1.0p55;
		static const double mscale = 0x1.0p-55;
		static const double tiny = 0x1.0000000000000p-1022;
        if( _mm_istrue_sd( _mm_cmplt_sdm( y, (double*) &small ) ) )
		{
			if( _mm_istrue_sd( _mm_cmpne_sdm( y, (double*) &minusZeroD ) ) )
				y = _mm_mul_sdm( _mm_add_sdm( _mm_mul_sdm( y, &scale ), &tiny ), &mscale );	//set inexact
		}
		else
            y = _mm_sub_sd( y, _mm_mul_sd( y, _fatn(y) ) );
    }
    else
    {
        xDouble t = _mm_mul_sdm( y, &FPKX2 );               //y * FPKX2
        xDouble a = _mm_sub_sdm( y, &FPKX2 );               //y - FPKX2
        xDouble rt = _mm_div_sd( _mm_load_sd( &one ), t );  //1/t
        xDouble b = _mm_add_sdm( t, &one );                 //t + 1
        xDouble z = _mm_div_sd( a, b );                     //(y - FPKX2) / (t + 1)
        xDouble c = _mm_add_sdm( rt, &one );                //1/t + 1
        c = _mm_div_sd( a, c );                             //(y - FPKX2) / (1/t + 1 )
        xDouble d = _fatn(z);
        xDouble e = _mm_add_sdm( _mm_mul_sd( z, d ), &FPKX2FX2 ); 
        c = _mm_add_sd( c, e );
        y = _mm_sub_sd( y, c );
    }
    
    //correct for large inputs
    if( isLarge )
        y = _mm_sub_sd( _mm_load_sd( &FPKPI2), y );

    //restore the sign
    y = _mm_xor_pd( y, sign );

    return y;
}


static const double half = 0.5;
static const double Emin = 0x1.0p-1022;
static const double plusinf = 1e500;


static inline xDouble _xasin( xDouble x )
{
	if( _mm_istrue_sd( _mm_cmpunord_sd( x, x ) ) )
		return _mm_add_sd( x, x );

    xDouble y = _mm_andnot_pd( minusZeroD, x );
    xDouble oneD = _mm_load_sd( &one );
	
	if( _mm_istrue_sd( _mm_cmpgt_sd( y, oneD ) ) )
	{	//deal with overflow gracefully
		y = _mm_or_pd( y, minusZeroD );
		y = _MM_SQRT_SD( y );
	}
	else
	{
		if( _mm_istrue_sd( _mm_cmple_sdm( y, &half ) ) )
		{
			static const double small = 0x1.0p-126;
			y = _mm_max_sdm( y, &small );	//avoid spurious underflow
			y = _mm_sub_sd( oneD, _mm_mul_sd( y, y )  );
		}
		else
		{
			y = _mm_sub_sd( oneD, y );
			y = _mm_sub_sd( _mm_add_sd( y, y ), _mm_mul_sd( y, y ) );
		}

		y = _MM_SQRT_SD( y );
		y = _mm_div_sd( x, y );
		y = _xatan( y );
	}
	
    //if y is NaN but x is real
    if( _mm_istrue_sd( _mm_and_pd( _mm_cmpord_sd( x, x ), _mm_cmpunord_sd( y, y ) ) ) )
        y = _mm_load_sd( &inverseTrigNaN.d );
    
    return y;
}




static inline xDouble _xacos( xDouble x )
{
	static const double three = 3.0;
    //get old fp env, and set the default one
        
    xDouble y = x;
    xDouble xone = _mm_load_sd( &one );

	if( _mm_istrue_sd( _mm_cmpeq_sd( xone, _mm_andnot_pd( minusZeroD, y ) ) ) )
	{	//Handle 1 and -1
		static const double pi = 0x1.921fb54442d18p1;
		y = _mm_and_pd( _mm_load_sd( &pi ), _mm_cmplt_sd( y, minusZeroD ) );
	}
	else
	{	
		xDouble safeY = _mm_andnot_pd( _mm_cmpunord_sd( y, y ), y );
		safeY = _mm_andnot_pd( minusZeroD, safeY );
		y = _mm_sel_pd( y, _mm_load_sd( &three), _mm_cmpgt_sd( safeY, xone ) ); //slam invalid inputs to a value that will calculate exactly until we reach NaN
		y = _mm_div_sd( _mm_sub_sd( xone, y ), _mm_add_sd( xone, y ) );
		y = _MM_SQRT_SD( y );
		y = _xatan( y );
		y = _mm_add_sd( y, y ); 
	}

    //if y is NaN but x is real
    if( _mm_istrue_sd( _mm_and_pd( _mm_cmpord_sd( x, x ), _mm_cmpunord_sd( y, y ) ) ) )
    {
        y = _mm_load_sd( &inverseTrigNaN.d );
    }
    
    return y;
}

#pragma mark -

double asin(double x)
{
    xDouble xd = DOUBLE_2_XDOUBLE( x );
    xd = _xasin( xd );
    return XDOUBLE_2_DOUBLE( xd );
}

float asinf(float x)
{
    xDouble xd = FLOAT_2_XDOUBLE( x );
    xd = _xasin( xd );
    return XDOUBLE_2_FLOAT( xd );
}

double acos(double x)
{
    xDouble xd = DOUBLE_2_XDOUBLE( x );
    xd = _xacos( xd );
    return XDOUBLE_2_DOUBLE( xd );
}

float acosf(float x)
{
    xDouble xd = FLOAT_2_XDOUBLE( x );
    xd = _xacos( xd );
    return XDOUBLE_2_FLOAT( xd );
}

double atan(double x)
{
    xDouble xd = DOUBLE_2_XDOUBLE( x );
    xd = _xatan( xd );
    return XDOUBLE_2_DOUBLE( xd );
}

float atanf(float x)
{
    xDouble xd = FLOAT_2_XDOUBLE( x );
    xd = _xatan( xd );
    return XDOUBLE_2_FLOAT( xd );
}

#warning ******* CHEESY HACK TO GET LIBM BUILDING *********
double atan2( double x, double y )
{
	return atan2l( (long double) x, (long double) y );
}

float atan2f( float x, float y )
{
	return atan2l( (long double) x, (long double) y );
}

#endif /* defined( __i386__ ) */
