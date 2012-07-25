/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * giantAsmBench.c - Benchmark of platform-specific giantInteger primitives.
 *
 * Revision History
 * ----------------
 * 18 Apr 98	Doug Mitchell at Apple
 * 	Created.
 */

#include <security_cryptkit/giantPortCommon.h>
#include <security_cryptkit/feeDebug.h>
#include <security_cryptkit/feeFunctions.h>
#include <stdlib.h>
#include "ckutilsPlatform.h"
#include <stdio.h>
#include <time.h>

#define LOOPS_DEF	10000
#define MIN_SIZE_DEF	1	/* mix digits for vectorMultiply test */
#define MAX_SIZE_DEF	8	/* max digits */


static void usage(char **argv) 
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   l=loops    (default = %d)\n", LOOPS_DEF); 
	printf("   n=maxDigits (default = %d)\n", MIN_SIZE_DEF);
	printf("   x=maxDigits (default = %d)\n", MAX_SIZE_DEF);
	printf("   s=seed\n");
	printf("   h(elp)\n");
	exit(1);
}

/*
 * Fill buffer with random data. Assumes giantDigits is native int size.
 */
static void randDigits(unsigned numDigits, 
	giantDigit *digits)
{
	int 		i;
	
	for(i=0; i<numDigits; i++) {
		/* RAND() only returns 31 bits on Unix.... */
		digits[i]= RAND() + ((RAND() & 1) << 31);
	}
}



int main(int argc, char **argv)
{
	int		arg;
	char		*argp;
	giantDigit	*digit1;		// mallocd arrays
	giantDigit	*digit2;
	giantDigit	*vect1;
	giantDigit	*vect2;
	giantDigit	*dig1p;			// ptr into mallocd arrays
	giantDigit	*dig2p;
	giantDigit	*vect1p;
	giantDigit	*vect2p;
	unsigned	numDigits;
	unsigned	i;
	PLAT_TIME	startTime;
	PLAT_TIME	endTime;
	unsigned	elapsed;
	giantDigit	scr1;			// op result
	giantDigit	scr2;			// op result
	int 		loops = LOOPS_DEF;
	int		seedSpec = 0;
	unsigned	seed = 0;
	unsigned	maxSize = MAX_SIZE_DEF;
	unsigned	minSize = MIN_SIZE_DEF;
	
	initCryptKit();
	
	#if	macintosh
	argc = ccommand(&argv);
	#endif
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 'x':
		    	maxSize = atoi(&argp[2]);
			break;
		    case 'n':
		    	minSize = atoi(&argp[2]);
			break;
		    case 'l':
		    	loops = atoi(&argp[2]);
			break;
		    case 's':
			seed = atoi(&argp[2]);
			seedSpec = 1;
			break;
		    case 'h':
		    default:
		    	usage(argv);
		}
	}

	if(!seedSpec) {
		unsigned long	tim;
		time(&tim);
		seed = (unsigned)tim;
	}
	SRAND(seed);

	/*
	 * Scratch digits, big enough for anything. Malloc here, init with
	 * random data before each test.
	 */
	digit1 = malloc(sizeof(giantDigit) * loops * 2);
	digit2 = malloc(sizeof(giantDigit) * loops * 2);
	
	/* vect1 and vect2 are arrays of giantDigit arrays */
	vect1 = malloc(sizeof(giantDigit) * loops * maxSize);
	vect2 = malloc(sizeof(giantDigit) * loops * maxSize);
	
	if((digit1 == NULL) || (digit1 == NULL) || 
	   (vect1 == NULL) || (vect2 == NULL)) {
	    printf("malloc error\n");
	    exit(1);
	}
	
	printf("Starting giantAsm test: seed %d\n", seed);
	
	/* giantAddDigits test */
	randDigits(loops, digit1);
	randDigits(loops, digit2);
	dig1p = digit1;
	dig2p = digit2;
	PLAT_GET_TIME(startTime);
	for(i=0; i<loops; i++) {
		scr1 = giantAddDigits(*dig1p++, *dig2p++, &scr2);
	}
	PLAT_GET_TIME(endTime);
	elapsed = PLAT_GET_NS(startTime, endTime);
	printf("giantAddDigits: %f ns\n", 
		(double)elapsed / (double)loops);

	/* giantAddDouble test */
	randDigits(loops, digit1);
	randDigits(loops * 2, digit2);
	dig1p = digit1;
	dig2p = digit2;
	PLAT_GET_TIME(startTime);
	for(i=0; i<loops; i++) {
		giantAddDouble(dig2p, dig2p+1, *dig1p++);
		dig2p += 2;
	}
	PLAT_GET_TIME(endTime);
	elapsed = PLAT_GET_NS(startTime, endTime);
	printf("giantAddDouble: %f ns\n", 
		(double)elapsed / (double)loops);

	/* giantSubDigits test */
	randDigits(loops, digit1);
	randDigits(loops, digit2);
	dig1p = digit1;
	dig2p = digit2;
	PLAT_GET_TIME(startTime);
	for(i=0; i<loops; i++) {
		scr1 = giantSubDigits(*dig1p++, *dig2p++, &scr2);
	}
	PLAT_GET_TIME(endTime);
	elapsed = PLAT_GET_NS(startTime, endTime);
	printf("giantSubDigits: %f ns\n", 
		(double)elapsed / (double)loops);

	/* giantMulDigits test */
	randDigits(loops, digit1);
	randDigits(loops, digit2);
	dig1p = digit1;
	dig2p = digit2;
	PLAT_GET_TIME(startTime);
	for(i=0; i<loops; i++) {
		giantMulDigits(*dig1p++, *dig2p++, &scr1, &scr2);
	}
	PLAT_GET_TIME(endTime);
	elapsed = PLAT_GET_NS(startTime, endTime);
	printf("giantMulDigits: %f ns\n", 
		(double)elapsed / (double)loops);

	printf("\nvectorMultiply:\n");
	for(numDigits=minSize; numDigits<=maxSize; numDigits*=2) { 
		    
		randDigits(loops, digit1);		// plierDigit
		randDigits(loops * numDigits, vect1);	// candVector
		randDigits(loops * numDigits, vect2);	// prodVector
		dig1p = digit1;
		vect1p = vect1;
		vect2p = vect2;
		
		PLAT_GET_TIME(startTime);
		for(i=0; i<loops; i++) {
			scr1 = VectorMultiply(*dig1p++,	// plierDigit
				vect1p,			// candVector
				numDigits,
				vect2p);		// prodVector
			vect1p += numDigits;
			vect2p += numDigits;
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_NS(startTime, endTime);
		printf(" bits = %4d  : %f ns\n", 
			numDigits * GIANT_BITS_PER_DIGIT,
			(double)elapsed / (double)loops);
	
		} /* for numDigits */	
	return 0;
}
