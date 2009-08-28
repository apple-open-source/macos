
/*
 *	fmaf.c
 *
 *		by Ian Ollmann
 *
 *	Copyright (c) 2007, Apple Inc. All Rights Reserved.
 *
 *	C implementation of C99 fmaf() function.
 */
 
#include <math.h>
#include <stdint.h>

float fmaf( float a, float b, float c )
{
	double product = (double) a * (double) b;		//exact
	double dc = (double) c;							//exact

	#warning fmaf not completely correct
	// Simply adding C here is incorrect about 1 in a billion times.
	// While the double precision add here is correctly rounded,
	// we take a second rounding on conversion to float on return
	// which may cause us to be off by very slightly over half an ulp
	// in round to nearest. 
	double sum = product + dc;

	// ideally, we should test here and patch up the result.
	// I think the problem only occurs in round to nearest for 
	// exact half way cases in product with a non-zero c.
	// Presumably, we could check to see if the difference between
	// (float) sum and sum is a power of two (the right exact power 
	// of two) and c is non-zero, and it rounded the wrong way, then 
	// we might tweak the answer by an ulp using something like nextafter.
	// Happily denormals are not a problem during this check.
    //
	// Alternatively, if we figure out the problem of correctly rounded
	// 3-way adds, the product could be broken into 2 floats, and we 
	// could do a 3-way add of prodHi, prodLo and c. Crlibm has a function 
	// that might do the job (DoRenormalize3), bu Im thinking that it doesnt.
	//
	// Finally, to be completely right, we'd have to detect rounding mode.
	// The half way cases are different in other rounding modes.
	
	return (float) sum;
}
