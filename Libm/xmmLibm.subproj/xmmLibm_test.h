/*
 *  xmmLibm_test.h
 *  xmmLibm
 *
 *  Created by iano on 7/14/05.
 *  Copyright 2005 __MyCompanyName__. All rights reserved.
 *
 */

#include <math.h>
#include <fenv.h>
#include <stdint.h>
#include <stdio.h>

static const hexdouble specialD[] = { 
                                        { 0xFFF8000000000001ULL }, //-QNaN
                                        { 0xFFF0000000000001ULL }, //-SNaN
                                        { 0xFFF0000000000000ULL }, //-Inf
                                        { 0xFFEFFFFFFFFFFFFFULL }, //-MaxDouble
                                        { 0xBFF0000000000000ULL }, //-1.0
                                        { 0xBFE0000000000000ULL }, //-0.5
                                        { 0x8010000000000000ULL }, //-small normal
                                        { 0x800FFFFFFFFFFFFFULL }, //-large denormal
                                        { 0x8000000000FFFFFFULL }, //-denormal
                                        { 0x8000000000000001ULL }, //-smalldenormal
                                        { 0x8000000000000000ULL }, //-0
                                        { 0x0000000000000000ULL }, //0
                                        { 0x0000000000000001ULL }, //smalldenormal
                                        { 0x0000000000FFFFFFULL }, //denormal
                                        { 0x000FFFFFFFFFFFFFULL }, //large denormal
                                        { 0x0010000000000000ULL }, //small normal
                                        { 0x3FE0000000000000ULL }, //0.5
                                        { 0x3FF0000000000000ULL }, //1.0
                                        { 0x7FEFFFFFFFFFFFFFULL }, //MaxDouble
                                        { 0x7FF0000000000000ULL }, //Inf
                                        { 0x7FF0000000000001ULL }, //SNaN
                                        { 0x7FF8000000000001ULL }  //QNaN
                                    };
                                    
static const unsigned int specialDCount = sizeof( specialD ) / sizeof( specialD[0] );

static const hexfloat specialF[] = { 
                                        { 0xFFC00001U }, //-QNaN
                                        { 0xFF800001U }, //-SNaN
                                        { 0xFF800000U }, //-Inf
                                        { 0xFF7FFFFFU }, //-MaxFloat
                                        { 0xBF800000U }, //-1.0
                                        { 0xBF000000U }, //-0.5
                                        { 0x80800000U }, //-small normal
                                        { 0x807FFFFFU }, //-large denormal
                                        { 0x800001FFU }, //-denormal
                                        { 0x80000001U }, //-smalldenormal
                                        { 0x80000000U }, //-0
                                        { 0x00000000U }, //0
                                        { 0x00000001U }, //smalldenormal
                                        { 0x000001FFU }, //denormal
                                        { 0x007FFFFFU }, //large denormal
                                        { 0x00800000U }, //small normal
                                        { 0x3F000000U }, //0.5
                                        { 0x3F800000U }, //1.0
                                        { 0x7F7FFFFFU }, //MaxFloat
                                        { 0x7F800000U }, //Inf
                                        { 0x7F800001U }, //SNaN
                                        { 0x7FC00001U }  //QNaN
                                    };
                                    
static const unsigned int specialFCount = sizeof( specialF ) / sizeof( specialF[0] );


typedef struct
{
    float (*testF)(float);
    double (*testD)(double);
    long double (*correct)(long double);
    const char *name;
}UnaryFunction;

typedef struct
{
    float (*testF)(float, float);
    double (*testD)(double, double);
    long double (*correct)(long double, long double);
    const char *name;
}BinaryFunction;

extern float xexpf( float );
extern float xexpm1f( float );
extern float xlogf( float );
extern float xlog1pf( float );
extern float xsqrtf( float );
extern float xpowf( float, float );

extern double xexp( double );
extern double xexpm1( double );
extern double xlog( double );
extern double xlog1p( double );
extern double xsqrt( double );
extern double xpow( double, double );

static long double my_pow( long double a, long double b )
{
	return 3.0L;
}

UnaryFunction unaries[] =   {
                                { xexpf, xexp, expl, "exp" },
                                { xexpm1f, xexpm1, expm1l, "expm1" },
                                { xlogf, xlog, logl, "log" },
                                { xlog1pf, xlog1p, log1pl, "log1p" },
                                { xsqrtf, xsqrt, sqrtl, "sqrt" },
                                
                                { NULL, NULL, NULL, NULL }
                            };

BinaryFunction binaries[] = {
                                { xpowf, xpow, powl, "pow" },
                                
                                { NULL, NULL, NULL, NULL }
                            };


