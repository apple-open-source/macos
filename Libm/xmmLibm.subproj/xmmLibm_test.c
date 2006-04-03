/*
 *  xmmLibm_test.c
 *  xmmLibm
 *
 *  Created by iano on 7/14/05.
 *  Copyright 2005 __MyCompanyName__. All rights reserved.
 *
 */

#include "xmmLibm_test.h"

#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>

static inline double checkErrorF( float v, long double c );
static inline double checkErrorD( double v, long double c );

const char *bitNames[16] = {  "Invalid Operation Flag",
							"Denormal Flag",
							"Divide by Zero Flag",
							"Overflow Flag",
							"Underflow Flag", 
							"Inexact Flag",
							"DAZ",
							"Invalid operation Mask",
							"Denormal operation Mask",
							"Divide by Zero Mask",
							"Overflow Mask",
							"Underflow Mask",
							"Inexact Mask",
							"RC 0",
							"RC 1",
							"Flush to Zero"
							};


extern double xexp( double );

int main( void )
{
    int i,j;
    int oldMXCSR = _mm_getcsr();
	UnaryFunction *func;
	BinaryFunction *bfunc;
	long double conversion = 0;
	double clockspeed = 0;

	{ //determine clock speed
		const char *name = "hw.cpufrequency_max";
		uint64_t freq = 0;
		size_t size = sizeof( freq );
		int err = sysctlbyname( name, &freq, &size, NULL, 0 );
		if( 0 == err )
		{
			clockspeed = freq;
		}
		printf( "Clock speed:  %g Hz\n", clockspeed );
	}	
	{ //determine clock speed
		mach_timebase_info_data_t data = {0,0};
		kern_return_t error = mach_timebase_info( &data );
		if( 0 == error )
			conversion = 1e-9 * (long double) data.numer / (long double) data.denom;
	}	

	printf( "starting MXCSR: 0x%4.4x\n", oldMXCSR );
	printf( "set bits:\n" );
	for( i = 0; i < 16; i++ )
	{
		if( oldMXCSR & 1 )
			printf( "\t%s\n", bitNames[i] );
		oldMXCSR = (( oldMXCSR >> 1 ) | ( oldMXCSR << 15 )) & 0xFFFF;
	}


	_mm_setcsr( 0x1f80 | 0x0000 );
	printf( "MXCSR set to 0x%4.4x before running\n", _mm_getcsr() );

#if 1
	//Do speed test
	{
		uint64_t	start, stop, current, best, latency;
		const int count = 10000;
		const int innerCount = 1000;
		volatile double d = 0x1.FFFFFFFFFFFFFp-1;	//nearly 1.0
		volatile float f = 0x1.FFFFF8p-1; //nearly 1.0
		double c = 0;
		float cf = 0;

		double ticks2cycles = clockspeed * conversion / (double) innerCount;
	

		best = -1ULL;
		for( i = 0; i < count; i++ )
		{
			start = mach_absolute_time();
			for( j = 0; j < innerCount; j++ )
				c += d;
			stop = mach_absolute_time();
			current = stop - start;
			if( current < best )
				best = current;
		}
		latency = best;
		printf( "Clock latency: %g cycles (%g)\n\n", (double) latency * ticks2cycles, c );


		for( func = unaries; NULL != func->testF; func++ )
		{
			c = 0;
			cf = 0;
			best = -1ULL;
			for( i = 0; i < count; i++ )
			{
				start = mach_absolute_time();
				for( j = 0; j < innerCount; j++ )
					cf += func->testF( f );
				stop = mach_absolute_time();
				current = stop - start;
				if( current < best )
					best = current;
			}
			best -= latency;
			printf( "%s:\t(F)%17.21g\t", func->name, (double)best  * ticks2cycles );
			fflush( stdout );

			best = -1ULL;
			for( i = 0; i < count; i++ )
			{
				start = mach_absolute_time();
				for( j = 0; j < innerCount; j++ )
					c += func->testD( d );
				stop = mach_absolute_time();
				current = stop - start;
				if( current < best )
					best = current;
			}
			best -= latency;
			printf( "(D)%17.21g\t", (double)best   * ticks2cycles );
			fflush( stdout );

			best = -1ULL;
			for( i = 0; i < count; i++ )
			{
				start = mach_absolute_time();
				for( j = 0; j < innerCount; j++ )
					c += func->correct( d );
				stop = mach_absolute_time();
				current = stop - start;
				if( current < best )
					best = current;
			}
			best -= latency;
			printf( "(L)%17.21g (%g, %g) \n", (double)best   * ticks2cycles, c, cf );
			fflush( stdout );
		}

		for( bfunc = binaries; NULL != bfunc->testF; bfunc++ )
		{
			c = 0;
			cf = 0;
			best = -1ULL;
			for( i = 0; i < count; i++ )
			{
				start = mach_absolute_time();
				for( j = 0; j < innerCount; j++ )
					cf += bfunc->testF( f, f );
				stop = mach_absolute_time();
				current = stop - start;
				if( current < best )
					best = current;
			}
			best -= latency;
			printf( "%s:\t(F)%17.21g\t", bfunc->name, (double)best   * ticks2cycles );
			fflush( stdout );

			best = -1ULL;
			for( i = 0; i < count; i++ )
			{
				start = mach_absolute_time();
				for( j = 0; j < innerCount; j++ )
					c += bfunc->testD( d, d );
				stop = mach_absolute_time();
				current = stop - start;
				if( current < best )
					best = current;
			}
			best -= latency;
			printf( "(D)%17.21g\t", (double)best   * ticks2cycles );
			fflush( stdout );

			best = -1ULL;
			for( i = 0; i < count; i++ )
			{
				start = mach_absolute_time();
				for( j = 0; j < innerCount; j++ )
					c += bfunc->correct( d, d );
				stop = mach_absolute_time();
				current = stop - start;
				if( current < best )
					best = current;
			}
			best -= latency;
			printf( "(L)%17.21g (%g, %g) \n", (double)best   * ticks2cycles, c, cf );
			fflush( stdout );
		}
	
		printf( "\n\n" );
	}
#endif

#if 0
    //test unaries
    for( func = unaries + 0; NULL != func->testF; func++ )
    {
        for( i = 0; i < specialDCount; i++ )
        {
            double valueD = specialD[i].d;
            float valueF = specialF[i].f;
            long double correctD = func->correct( (long double) valueD );
            long double correctF = func->correct( (long double) valueF );
            double testD = func->testD( valueD );
            float testF = func->testF( valueF );
            double errD = checkErrorD( testD, correctD );
            double errF = checkErrorF( testF, correctF );
            
            if( fabs( errD ) > 0.5 )
                printf( "%d) %21.21g ulp error for %s( %21.21g ) = %21.21g vs. %21.21g\n", i, errD, func->name, valueD, testD, (double) correctD );
            if( fabs( errF ) > 0.5 )
                printf( "%d) %21.21g ulp error for %sf( %21.21g ) = %21.21g vs. %21.21g\n", i, errF, func->name, valueF, testF, (double) correctF );
        }
    }

    //test unaries
    for( bfunc = binaries + 0; NULL != bfunc->testF; bfunc++ )
    {
        for( i = 12; i < specialDCount; i++ )
        {
            double valueD = specialD[i].d;
            float valueF = specialF[i].f;
            
			for( j = 6; j < specialDCount; j++ )
			{
				double valueD2 = specialD[j].d;
				float valueF2 = specialF[j].f;
				long double correctD = bfunc->correct( (long double) valueD, (long double) valueD2 );
				long double correctF = bfunc->correct( (long double) valueF, (long double) valueF2 );
				double testD = bfunc->testD( valueD, valueD2 );
				float testF = bfunc->testF( valueF, valueF2 );
				double errD = checkErrorD( testD, correctD );
				double errF = checkErrorF( testF, correctF );
				if( fabs( errD ) > 0.5 )
					printf( "%d,%d) %21.21g ulp error for %s( %21.21g, %21.21g ) = %21.21g vs. %21.21g\n", i,j, errD, bfunc->name, valueD, valueD2, testD, (double) correctD );
				if( fabs( errF ) > 0.5 )
					printf( "%d,%d) %21.21g ulp error for %sf( %21.21g, %21.21g ) = %21.21g vs. %21.21g\n", i,j, errF, bfunc->name, valueF, valueF2, testF, (double) correctF );
			}
        }
    }
	
#endif 

	printf( "done.\n" );

    return 0;
}


static inline double checkErrorF( float v, long double c )
{
	if( c != c )
	{
		if( v != v )
			return 0.0;
		else
			return INFINITY;
	}

	if( v != v )
		return INFINITY;

    long double lv = v;
    long double error = lv - c;
    long double cExp = logb( c );
    long double scale = scalb( 1.0, -cExp );
    
    error *= scale;
    error *= 0x1.0p23;

    return (double) error;
}

static inline double checkErrorD( double v, long double c )
{
	if( c != c )
	{
		if( v != v )
			return 0.0;
		else
			return INFINITY;
	}

	if( v != v )
		return INFINITY;

    long double lv = v;
    long double error = lv - c;
    long double cExp = logb( c );
    long double scale = scalb( 1.0, -cExp );
    
    error *= scale;
    error *= 0x1.0p52;

    return (double) error;
}