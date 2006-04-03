/*
 *  xmm_sincostan.c
 *  xmmLibm
 *
 *  Created by Ian Ollmann, Ph.D. on 7/30/05.
 *  Copyright © 2005 Apple Computer, Inc. All rights reserved.
 *
 *    The algorithms for sine and cosine were taken from "Some Software          
 *    Implementations of the Functions Sine and Cosine" by Ping Tak              
 *    Peter Tang (Argonne National Laboratory publication ANL-90/3).             
 *      
 */

#if defined( __i386__ )
#include "xmmLibm_prefix.h"
#include <math.h>

static inline xDouble _xsin( xDouble ) ALWAYS_INLINE;
static inline double _xcos( double ) ALWAYS_INLINE;
static inline xDouble _xtan( xDouble ) ALWAYS_INLINE;
static inline xDouble _halfPiReduce( xDouble, int32_t* ) ALWAYS_INLINE;
static inline xDouble _S( register xDouble ) ALWAYS_INLINE;
static inline xDouble _C( register xDouble ) ALWAYS_INLINE;

#pragma mark -

static const double two52 = 0x1.0p52;   //2**52
static const double S0 = -1.66666666666666796e-01;
static const double S1 =  8.33333333333178931e-03;
static const double S2 = -1.98412698361250482e-04;
static const double S3 =  2.75573156035895542e-06;
static const double S4 = -2.50510254394993115e-08;
static const double S5 =  1.59108690260756780e-10;

static inline xDouble _S(register xDouble x)
{
    register xDouble t, u, k, v, w;

    t = _mm_mul_sd( x, x );                           //x*x
    u = _mm_mul_sd( t, t );                           //t*t;

    k = _mm_add_sdm( _mm_mul_sdm( u, &S5 ), &S3 );    //u*S5 + S3;
    v = _mm_add_sdm( _mm_mul_sdm( u, &S4 ), &S2 );    //u*S4 + S2;
    k = _mm_add_sdm( _mm_mul_sd( u, k ), &S1 );       //u*k  + S1;
    v = _mm_add_sdm( _mm_mul_sd( u, v ), &S0 );       //u*v  + S0;

    u = _mm_mul_sd( x, t );                           //x*t;
    w = _mm_add_sd( _mm_mul_sd( t, k), v );           //t*k + v;

    return _mm_add_sd( _mm_mul_sd( u, w ), x );       //u*w + x;
}

static const double C0 =  4.16666666666666019e-02;
static const double C1 = -1.38888888888744744e-03;
static const double C2 =  2.48015872896717822e-05;
static const double C3 = -2.75573144009911252e-07;
static const double C4 =  2.08757292566166689e-09;
static const double C5 = -1.13599319556004135e-11;
static const double T1 = 5.22344792962423754e-01;
static const double T2 = 2.55389245354663896e-01;
static const double half = 0.5;
static const double one = 1.0;
static const xDouble mThreeSixteenths = { -3.0/16.0, 0.0 };

static inline xDouble _C(register xDouble x)
{
    register xDouble t, u, k, v;
    register xDouble c1, c2;

    t = _mm_mul_sd( x, x );                           //x*x
    xDouble tLTt1 = _mm_cmpge_sdm( t, &T1 );
    xDouble tLTt2 = _mm_cmpge_sdm( t, &T2 );
    u = _mm_mul_sd( t, t );                           //t*t;
    tLTt1 = _mm_and_pd( tLTt1, mThreeSixteenths );     // -3/16 if t < T1, 0 otherwise
    tLTt2 = _mm_and_pd( tLTt2, mThreeSixteenths );     // -3/16 if t < T2, 0 otherwise
    c1 = _mm_add_sd( tLTt1, tLTt2 );                  // { -6/16, -3/16, 0 } for { t >= T1, T2 <= t < T1, t < T2 or NaN }
    c2 = _mm_add_sdm( c1, &one );                     // { 10/16, 13/16, 1.0 } for { t >= T1, T2 <= t < T1, t < T2 or NaN }
    

    k = _mm_add_sdm( _mm_mul_sdm( u, &C5 ), &C3 );    //u*C5 + C3;
    v = _mm_add_sdm( _mm_mul_sdm( u, &C4 ), &C2 );    //u*C4 + C2;
    k = _mm_add_sdm( _mm_mul_sd( u, k ), &C1 );       //u*k  + C1;
    v = _mm_add_sdm( _mm_mul_sd( u, v ), &C0 );       //u*v  + C0;  
    k = _mm_add_sd( _mm_mul_sd( t, k), v );           //t*k + v;

//  The original code is a bit branchy:
//
//  if (t >= T1)
//      return 5.0/8.0 + ((3.0/8.0 - 0.5*t) + u*k);     //10/16 + (u*k - (0.5*t - 6/16))
//  else if (t >= T2)
//      return 13.0/16.0 + ((3.0/16.0 - 0.5*t) + u*k);  //13/16 + (u*k - (0.5*t - 3/16))
//  else
//      return 1.0 - (0.5*t - u*k);                     //1.0 +  (u*k - (0.5*t - 0))
//
// I've straightened it out by doing the tests against t above to produce an index into array of constants
// this lets us use one straight line formula for all three cases.  We do one extra operation for the 
// third case, but we hope this is made up for by not needing to branch at all
//
    t = _mm_add_sd( _mm_mul_sdm( t, &half), c1 );               //0.5*t - c1
    u = _mm_sub_sd( _mm_mul_sd( u, k ), t );                    // u*k - (0.5*t - c1)

    return _mm_add_sd( u, c2 );                                 // c2 + (u*k - (0.5*t - c1))
}

static const double aBigNumber = 3.373e9;

static const double piOver4 = 0x1.921FB54442D18p-1; //7.8539816339744827900E-1;
static const double piOver2 = 0x1.921FB54442D18p+0; //1.570796326794896558
static const double rPiOver2 = 0x1.45F306DC9C882p-1; //0.636619772367581271411  (2/pi, rounded towards zero)

//2/Pi broken out into chunks, each with 24 bits of precision. Long doubles here are PowerPC 128 bit head to tail long doubles
static const xDouble halfPiBits[] = {                            //     value,                  sum of elements to here,  (long double) 2/PI,     (long double) sum of elements - (long double) 2/PI
                                            { 0x1.45f3068000000p-1,   0x1.45f3068000000p-1}, //0.63661976158618927002  (0.636619772367581382433, 0.63661976158618927002, 1.07813920730560043469e-08) 1.490116119384765625e-08
                                            { 0x1.7272208000000p-27,  0x1.7272208000000p-27}, //1.07813920013910546913e-08  (0.636619772367581382433, 0.636619772367581271411, 7.16649491121506822786e-17) 1.490116119384765625e-08
                                            { 0x1.4a7f090000000p-54,  0x1.4a7f090000000p-54}, //7.16649463468466406965e-17  (0.636619772367581382433, 0.636619772367581382433, 2.7653040415820229278e-24) 1.490116119384765625e-08
                                            { 0x1.abe8fa8000000p-79,  0x1.abe8fa8000000p-79}, //2.76530402925607128372e-24  (0.636619772367581382433, 0.636619772367581382433, 1.23259516440783094596e-32) 1.490116119384765625e-08
                                            { 0x1.a6ee060000000p-107, 0x1.a6ee060000000p-107}, //1.01816640731342332708e-32  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.b629590000000p-132, 0x1.b629590000000p-132}, //3.14365469230039287906e-40  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.2788720000000p-157, 0x1.2788720000000p-157}, //6.31912116007094114798e-48  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.07f9440000000p-186, 0x1.07f9440000000p-186}, //1.05133589105113888752e-56  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.8eaf7a0000000p-210, 0x1.8eaf7a0000000p-210}, //9.46436187565416189665e-64  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.de2b0d8000000p-235, 0x1.de2b0d8000000p-235}, //3.38291995014597506751e-71  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.c91b8e0000000p-262, 0x1.c91b8e0000000p-262}, //2.40945958739123873608e-79  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.2126e90000000p-287, 0x1.2126e90000000p-287}, //4.54231730662959487246e-87  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.c00c920000000p-313, 0x1.c00c920000000p-313}, //1.04881044512058824906e-94  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.77504e8000000p-339, 0x1.77504e8000000p-339}, //1.30913947869263899574e-102  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.921cfc0000000p-368, 0x1.921cfc0000000p-368}, //2.61258231019769038636e-111  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.0ef58e0000000p-391, 0x1.0ef58e0000000p-391}, //2.09862879884258382793e-118  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.62534e0000000p-417, 0x1.62534e0000000p-417}, //4.08934865555011397489e-126  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.f744118000000p-443, 0x1.f744118000000p-443}, //8.65504745424325487145e-134  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.7d4bae0000000p-470, 0x1.7d4bae0000000p-470}, //4.88566734486928553996e-142  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.a242748000000p-495, 0x1.a242748000000p-495}, //1.5971955391604930051e-149  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.38e04d0000000p-521, 0x1.38e04d0000000p-521}, //1.78034753362911884672e-157  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.a2fbf20000000p-547, 0x1.a2fbf20000000p-547}, //3.55263027418628779663e-165  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.3991d40000000p-576, 0x1.3991d40000000p-576}, //4.9524094219850778908e-174  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.1cc1a98000000p-599, 0x1.1cc1a98000000p-599}, //5.36125256460390096698e-181  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.cfa4e40000000p-627, 0x1.cfa4e40000000p-627}, //3.25190234165883542754e-189  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.17e2ec0000000p-654, 0x1.17e2ec0000000p-654}, //1.46259703143216385431e-197  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.bf25070000000p-677, 0x1.bf25070000000p-677}, //2.78548611181072852123e-204  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.8ffc4b8000000p-703, 0x1.8ffc4b8000000p-703}, //3.71293511764237334717e-212  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.ffbc0b0000000p-729, 0x1.ffbc0b0000000p-729}, //7.07844609560366270571e-220  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.80fef20000000p-756, 0x1.80fef20000000p-756}, //3.96770518332750327739e-228  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.e2316b0000000p-781, 0x1.e2316b0000000p-781}, //1.48099760984394610907e-235  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.05368f8000000p-807, 0x1.05368f8000000p-807}, //1.19549711460546634914e-243  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.b4d9fb0000000p-834, 0x1.b4d9fb0000000p-834}, //1.48962677032793951605e-251  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.e4f9600000000p-861, 0x1.e4f9600000000p-861}, //1.23211808632719765233e-259  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.36e9e88000000p-885, 0x1.36e9e88000000p-885}, //4.70818735526323071585e-267  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.1fb34f0000000p-911, 0x1.1fb34f0000000p-911}, //6.49193984611926221408e-275  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.7fa8b50000000p-938, 0x1.7fa8b50000000p-938}, //6.45014541619370583516e-283  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.a93dd60000000p-963, 0x1.a93dd60000000p-963}, //2.13063913279712139244e-290  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.faf97c0000000p-990, 0x1.faf97c0000000p-990}, //1.89256360589071054229e-298  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                            { 0x1.7b3d070000000p-1016, 0x1.7b3d070000000p-1016} //2.10958355854526332845e-306  (0.636619772367581382433, 0.636619772367581382433,                 0) 1.490116119384765625e-08
                                    };
static const int halfPiBinsCount = sizeof( halfPiBits ) / sizeof( halfPiBits[0] );
//to divide any unsigned 16-bit integer by 24, do ( x * 0x0000aaab ) >> 20
//to divide any unsigned 16-bit integer by 26, do ( x * 0x00009d8a ) >> 20;

static inline xDouble extended_accum_pd( xDouble *a, xDouble b ) ALWAYS_INLINE;
static inline xDouble extended_accum_pd( xDouble *a, xDouble b )
{
    xDouble oldA = *a;
    *a = _mm_add_pd( *a, b );
    xDouble carry = _mm_sub_pd( b, _mm_sub_pd( *a, oldA ) );
    return carry;
}


static const xUInt64 himask = { 0xFFFFFFFFF8000000ULL, 0 };
static const xDouble two54D = { 0x1.0p54, 0x1.0p54};
//x must be positive
/*
static inline xDouble _halfPiReduce( xDouble x, int32_t *quo )
{   
    static const xUInt64 mask26 = { 0xFFFFFFFFF8000000ULL, 0ULL };
    xDouble products[ halfPiBinsCount ];
    xDouble accum[ halfPiBinsCount ];
    
    xDouble X = _mm_and_pd( x, (xDouble) mask26 );
    X = _mm_unpacklo_pd( X, _mm_sub_sd( x, X ) );           //{ xHi, xLo }  top 26 bits of x, and bottom 27 bits of x
    accum[0] = _mm_setzero_pd();

    int startBin, endBin;
    int i, j;
    xDouble *startBits = halfPiBits + startBin;

    if( endBin >= halfPiBinsCount )
        endBin = halfPiBinsCount - 1;

    int count = endBin - startBin;  //the real count is count + 1
    
    #warning not done
    if( count > 0 )
    {
        accum[0] = _mm_mul_pd( X, startBits[i] );

        for( i = 1; i <= count; i++ )
        {
            xDouble product = _mm_mul_pd( X, startBits[i] );
            xDouble carry = extended_accum_pd( accum, product );
            
            for( j = 1; j < i; j++ )
                carry = extended_accum_pd( accum + j, carry );
            
            accum[i] = carry;
        }
    }

    
}
*/

//Assumes that x1 is positive or zero
static inline xDouble _halfPiReduce( xDouble x1, int32_t *quo )
{   
    #warning This is Super CHEESY and WRONG!!!!!
    //multiply by reciprocal
    xDouble q = _mm_mul_sdm( x1, &rPiOver2 );

    //take absolute value of the result
//    xDouble sign = _mm_and_pd( minusZeroD, x1 );
//    xDouble fabsq = _mm_andnot_pd( minusZeroD, q );
	xDouble fabsq = q;

    //split into integer and fractional parts
    xDouble step = _mm_and_pd( _mm_load_sd( &two52), _mm_cmplt_sdm( fabsq, &two52 ) );
    xDouble integral = _mm_sub_sd( _mm_add_sd( fabsq, step ), step );                       //integer part of the quotient
    xDouble fractional = _mm_sub_sd( x1, _mm_mul_sdm( integral, &piOver2) );
	xDouble rightSize = _mm_cmple_sdm( fractional, &piOver4 );
	integral = _mm_add_sd( integral, _mm_andnot_pd( rightSize, _mm_load_sd( &one ) ) );
	fractional = _mm_sub_sd( x1, _mm_mul_sdm( integral, &piOver2) );

    //restore sign
//    integral = _mm_or_pd( integral, sign );
//    fractional = _mm_or_pd( fractional, sign );

    //return results
    *quo = _mm_cvtsd_si32( integral );
    return fractional;
}


#pragma mark -

static const union
{
    uint64_t    u;
    double      d;
}trigNaN = { 0x7ff8000000000021ULL };

static inline xDouble _xsin( xDouble x )
{
    //get old fp env, and set the default one
    int oldMXCSR = _mm_getcsr();
	int newMXCSR = (oldMXCSR | DEFAULT_MXCSR) & DEFAULT_MASK;		//set standard masks, enable denormals, set round to nearest
	if( newMXCSR != oldMXCSR )
		_mm_setcsr( newMXCSR );

    xDouble y = _mm_andnot_pd( minusZeroD, x );
    xDouble xEQZero = _mm_cmpeq_sdm( x, (double*) &minusZeroD );
    
    //if y > pi/4
    if( _mm_istrue_sd( _mm_cmpgt_sdm( y, &piOver4 ) ) )
    {
        int quo = 0;
        y = _halfPiReduce( y, &quo );
        
        if( _mm_istrue_sd( _mm_cmpunord_sd( y, y ) ) )
        {
            y = _mm_load_sd( &trigNaN.d );
            oldMXCSR |= INVALID_FLAG;
        }
        else
        {
            if( quo & 1 )
                y = _C(y);
            else
                y = _S(y);
            
            if( quo & 2 )   
                y = _mm_xor_pd( y, minusZeroD );    //y = -y;

            //if( x < 0 ) y = -y;    
            x = _mm_and_pd( x, xEQZero );
            y = _mm_or_pd( y, x );
            oldMXCSR |= INEXACT_FLAG;
        }
    }
    else
    {
        //if y > 0
        if( _mm_istrue_sd( _mm_cmpgt_sd( y, minusZeroD ) ) )
        {
            y = _S(y);
            oldMXCSR |= INEXACT_FLAG;
        }
        else
        {
            //if( x == 0 ) y = x;
            x = _mm_and_pd( x, xEQZero );
            y = _mm_or_pd( y, x );
        }
    }

    _mm_setcsr( oldMXCSR );
    return y;
}


static inline double _xcos( double xx )
{
	if( xx != xx )
		return xx + xx;

	xDouble x = DOUBLE_2_XDOUBLE( xx );
    xDouble y = _mm_andnot_pd( minusZeroD, x );
    xDouble xEQZero = _mm_cmpeq_sdm( x, (double*) &minusZeroD );
    
    //if y > pi/4
    if( _mm_istrue_sd( _mm_cmpgt_sdm( y, &piOver4 ) ) )
    {
        int quo = 0;
        y = _halfPiReduce( y, &quo );
        
        if( _mm_istrue_sd( _mm_cmpunord_sd( y, y ) ) )
        {
//			int z;
            y = _mm_load_sd( &trigNaN.d );
//			asm volatile( "cvtss2si %1, %0" : "=r" (z) : "x" (y) );		//set invalid flag
        }
        else
        {
            if( quo & 1 )
                y = _S(y);
            else
                y = _C(y);
            
            if( (quo+1) & 2 )   
                y = _mm_xor_pd( y, minusZeroD );    //y = -y;

        }
    }
    else
    {
        //if y > 0
        if( _mm_istrue_sd( _mm_cmpgt_sd( y, minusZeroD ) ) )
		{
			static const double minY = 0x1.0000000000001p-511;
			y = _mm_max_sdm( y, &minY );	//a
            y = _C(y);
        }
        else
        {
			return 1.0;
        }
    }

    return XDOUBLE_2_DOUBLE(y);
}

static const double plusinf = 1e500;    //infinity
static const double p0 =  0x1.E1346CE13A963p+8;  //481.20478637390459653
static const double p1 = -0x1.D39B1C4972CB0p+4;  //-29.2253687733725087128
static const double p2 =  0x1.553AE3B07D8EAp-2;  //0.333232457778093960066
static const double p3 = -0x1.63F866C7AF365p-22; //-3.31523193324546743998e-07
static const double q0 =  0x1.C321261326ECDp+11; //3609.03589780428455924
static const double q1 = -0x1.A3FF6484AEC5Cp+10; //-1679.9905101496469797
static const double q2 =  0x1.6A26064907801p+6;  //90.5371333514049325686
static const double twom2 = 0x1.0p-2;
static const double three = 3.0;

static inline xDouble _xtan( xDouble x )
{
    //get old fp env, and set the default one
    int oldMXCSR = _mm_getcsr();
	int newMXCSR = (oldMXCSR | DEFAULT_MXCSR) & DEFAULT_MASK;		//set standard masks, enable denormals, set round to nearest
	if( newMXCSR != oldMXCSR )
		_mm_setcsr( newMXCSR );
        
    xDouble y = _mm_andnot_pd( minusZeroD, x ); //y = fabs(x)
    xDouble sign = _mm_and_pd( minusZeroD, x );
    int quo = 0;
    
/*******************************************************************************
*     If x < pi/4, then no argument reduction necessary, skip the step.        *
*     About the argument reduction: x is reduced REM approximate pi/2, leaving *
*     its magnitude no bigger than approximate pi/4.  HalfPiReduce is a        *
*     specialized REM.                                                         *
*                                                                              *
*     Recall that:                                                             *
*                                                                              *
*     tg(pi/2 + x) = -1/tg(x)                                                  *
*     tg(pi + x)   =  tg(x)                                                    *
*                                                                              *
*     Then if input x = quotient*(pi/2) + r,                                   *
*     quotient MOD 2 determines whether to negate and reciprocate tg(x).       *
*******************************************************************************/
    if( _mm_isfalse_sd( _mm_cmplt_sdm( y, &piOver4 ) ) )
        y = _halfPiReduce( y, &quo );
    
    //copy sign of x to y
    y = _mm_xor_pd( y, sign );

    xDouble fabsy = _mm_andnot_pd( minusZeroD, y );
    
    if( _mm_istrue_sd( _mm_cmplt_sdm( fabsy, &plusinf ) ) )
    {
        xDouble s = _mm_mul_sd( y, y );
        xDouble num = _mm_add_sdm( _mm_mul_sdm( s, &p3 ), &p2 );                                    // s * p3 + p2
        xDouble denom = _mm_add_sdm( _mm_mul_sd( _mm_sub_sd( _mm_load_sd( &q2 ), s ), s ), &q1 );   // s * (q2 - s) + q1
        num = _mm_add_sdm( _mm_mul_sd( num, s ), &p1 );                                             // s * ( s * p3 + p2 ) + p1
        denom = _mm_add_sdm( _mm_mul_sd( denom, s ), &q0 );                                         // s * (s * (q2 - s) + q1) + q0
        xDouble t = _mm_div_sd( num, denom );
        t = _mm_mul_sd( t, s );

    /*
        if( s <= 0.25 )
            y += s * 0 + (y - 0) * s / 3.0 + (s * y) * t;
        else
            y += s * 0.25 + (s * (y - 0.75) / 3) + (s * y) * t;
    */
        xDouble c1 = _mm_load_sd( &twom2 );
        xDouble c2 = _mm_sub_sdm( c1, &one );
        xDouble sLEtwom2 = _mm_cmplt_sd( s, c1 );
        c1 = _mm_and_pd( c1, sLEtwom2 );
        c2 = _mm_and_pd( c2, sLEtwom2 );
        xDouble b = _mm_add_sd( y, c2 );                    //y + {0, -0.75}
        b = _mm_mul_sd( b, s );                             //s * (y + {0, -0.75})
        b = _mm_div_sdm( b, &three );                       // (s * (y + {0, -0.75}))/3
        xDouble c = _mm_mul_sd( _mm_mul_sd( s, y ), t );    //(s*y)*t
        xDouble a = _mm_mul_sd( s, c1 );                    //s * {0, 0.25}
        y = _mm_add_sd( a, b );
        y = _mm_add_sd( y, c );

        if( quo & 1 )
            y = _mm_div_sd( _mm_load_sd( &one ), y );

        newMXCSR = _mm_getcsr() & ~UNDERFLOW_FLAG;
        oldMXCSR |= newMXCSR & ALL_FLAGS;
    }
    else if ( _mm_istrue_sd( _mm_cmpeq_sdm( fabsy, &plusinf ) ) )
    {
        oldMXCSR |= INVALID_FLAG;
        y = _mm_load_sd( &trigNaN.d );
    }
    else
    {
        y = x;
    }
    
    _mm_setcsr( oldMXCSR );
    return y;
}

#pragma mark -

long double __cosl( long double );
long double __sinl( long double );
long double __tanl( long double );

double cos(double x)
{
    return __cosl(  x );
}

float cosf(float x)
{
    return __cosl( x );
}

double sin(double x)
{
    return __sinl(  x );
}

float sinf(float x)
{
    return __sinl( x );
}

double tan(double x)
{
    return __tanl(  x );
}

float tanf(float x)
{
    return __tanl( x );
}



#endif /* defined( __i386__ ) */
