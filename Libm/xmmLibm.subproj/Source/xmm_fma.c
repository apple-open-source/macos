/*
 *  xmm_fma.c
 *  xmmLibm
 *
 *  Created by Ian Ollmann on 8/8/05.
 *  Copyright 2005 Apple Computer Inc. All rights reserved.
 *
 */

#if defined( __i386__ )

#include "xmmLibm_prefix.h"

#include <math.h>

//For any rounding mode, we can calculate A + B exactly as a head to tail result as:
//
//  Rhi = A + B     (A has a larger magnitude than B)
//  Rlo = B - (Rhi - A);
//  
//  Rhi is rounded to the current rounding mode
//  Rlo represents the next 53+ bits of precision

//returns carry bits that don't fit into A
static inline long double extended_accum( long double *A, long double B ) ALWAYS_INLINE;
static inline long double extended_accum( long double *A, long double B ) 
{
    long double r = *A + B;
    long double carry = B - ( r - *A );
    *A = r;
    return carry;
}


double fma( double a, double b, double c )
{
    static const xUInt64 mask26 = { 0xFFFFFFFFFC000000ULL, 0 };
    double ahi, bhi;

    //break a, b and c into high and low components
    //The high components have 26 bits of precision
    //The low components have 27 bits of precision
    xDouble xa = DOUBLE_2_XDOUBLE(a);
    xDouble xb = DOUBLE_2_XDOUBLE(b);

    xa = _mm_and_pd( xa, (xDouble) mask26 );
    xb = _mm_and_pd( xb, (xDouble) mask26 );
    
    ahi = XDOUBLE_2_DOUBLE( xa );
    bhi = XDOUBLE_2_DOUBLE( xb );

    //double precision doesn't have enough precision for the next part. 
    //so we abandond it and go to extended.
    ///
    //The problem is that the intermediate multiplication product needs to be infinitely
    //precise. While we can fix the mantissa part of the infinite precision problem,
    //we can't deal with the case where the product is outside the range of the
    //representable double precision values. Extended precision allows us to solve
    //that problem, since all double values and squares of double values are normalized
    //numbers in extended precision
    long double Ahi = ahi;
    long double Bhi = bhi;
    long double Alo = (long double) a - Ahi;
    long double Blo = (long double) b - Bhi;
    long double C = c;
    
    //The result of A * B is now exactly:
    //
    //  B1 + Ahi*Bhi + Alo*Bhi + Ahi*Blo + Alo*Blo 
    //  all of these intermediate terms have either 52 or 53 bits of precision and are exact
    long double AhiBhi = Ahi * Bhi;        //52 bits 
    long double AloBhi = Alo * Bhi;        //53 bits  
    long double AhiBlo = Ahi * Blo;        //53 bits  
    long double AloBlo = Alo * Blo;        //54 bits   
    
    //accumulate the results into two head/tail buffers. This is infinitely precise.
    //no effort is taken to make sure that a0 and a1 are actually head to tail
    long double a0 = AloBlo;
    long double a1 = extended_accum( &a0, AhiBlo );
    a1 += extended_accum( &a0, AloBhi );
    a1 += extended_accum( &a0, AhiBhi );

    //Add C. If C has greater magnitude than a0, we need to swap them
    if( fabsl( C ) > fabsl( a0 ) )
    {
        long double temp = C;
        C = a0;
        a0 = temp;
    }

    //this will probably overflow, but we have 128 bits of precision here, which should mean we are covered. 
    a1 += extended_accum( &a0, C );
    
    //push overflow in a1 back into a0. This should give us the correctly rounded result
    a1 = extended_accum( &a0, a1 );

    return a0;
}




float fmaf( float a, float b, float c )
{
    xDouble xa = FLOAT_2_XDOUBLE( a );
    xDouble xb = FLOAT_2_XDOUBLE( b );
    xDouble xc = FLOAT_2_XDOUBLE( c );

    xa = _mm_mul_sd( xa, xb );                     //exact
    xa = _mm_add_sd( xa, xc );                     //inexact

    return XDOUBLE_2_FLOAT( xa );                //double rounding, alas
}

#if 0       //doesn't work yet
long double fmal( long double a, long double b, long double c )
{
    union{ long double ld; struct{ uint64_t mantissa; int16_t sexp; }parts; }ua = {a};
    union{ long double ld; struct{ uint64_t mantissa; int16_t sexp; }parts; }ub = {b};

    int16_t sign = (ua.parts.sexp ^ ub.parts.sexp) & 0x8000;
    int32_t aexp = (ua.parts.sexp & 0x7fff);
    int32_t bexp = (ub.parts.sexp & 0x7fff);
    int32_t exp  =  aexp + (ub.parts.sexp & 0x7fff) - 16383;

    //deal with NaN
    if( a != a )    return a;
    if( b != b )    return b;
    if( c != c )    return c;
    
    //deal with inf
    if( exp > 16384 )   //inf
    {
        if( __builtin_fabs(c) == __builtin_infl() )
        {
            if( sign & 0x8000 )
            {
                if( c > 0 )
                    return __builtin_nan();
                return c;
            }

            if( c < 0 )
                return __builtin_nan();
            return c;
        }
        
        if( sign & 0x8000 )
            return -__builtin_infl();

        return __builtin_infl();
    }
    
    if( exp < -16382 - 63 ) //sub denormal
        return c;

    
    
    //mantissa product
    //  hi(a.hi*b.hi)   lo(a.hi*b.hi)
    //                  hi(a.hi*b.lo)   lo(a.hi*b.lo)
    //                  hi(a.lo*b.hi)   lo(a.lo*b.hi)
    // +                                hi(a.lo*b.lo)   lo(a.lo*b.lo)
    // --------------------------------------------------------------

    uint32_t    ahi = ua.parts.mantissa >> 32;
    uint32_t    bhi = ub.parts.mantissa >> 32;
    uint32_t    alo = ua.parts.mantissa & 0xFFFFFFFFULL;
    uint32_t    blo = ub.parts.mantissa & 0xFFFFFFFFULL;

    uint64_t    templl, temphl, templh, temphh;
    xUInt64     l, h, a;
    
    templl = (uint64_t) alo * (uint64_t) blo;
    temphl = (uint64_t) ahi * (uint64_t) blo;
    templh = (uint64_t) alo * (uint64_t) bhi;
    temphh = (uint64_t) ahi * (uint64_t) bhi;

    l = _mm_cvtsi32_si128( (int32_t) templl );          templl >>= 32;

    temphl +=  templl;
    temphl +=  templh & 0xFFFFFFFFULL;
    h = _mm_cvtsi32_si128( (int32_t) temphl);           temphl >>= 32;
    a = _mm_unpacklo_epi32( l, h );
    

    temphh += templh >> 32;
    temphh += temphl;

    a = _mm_cvtsi32_si128( (int32_t) temphh );          temphh >>= 32;
    h = _mm_cvtsi32_si128( (int32_t) temphh);           
    a = _mm_unpacklo_epi32( a, h );
    l = _mm_unpacklo_epi64( l, a );
    
    union
    {
        xUInt64     v;
        uint64_t    u[2];
    }z = { l };

    long double lo = (long double) temphh.
}
#endif

#endif /* defined( __i386__ ) */
