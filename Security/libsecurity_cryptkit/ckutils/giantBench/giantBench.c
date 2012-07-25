/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * giantBench.c - Benchmark of NSGiantInteger primitives.
 *
 * Revision History
 * ----------------
 * 13 Apr 98	Doug Mitchell at Apple
 * 	Created.
 */

#include "giantIntegers.h"
#include "ckutilities.h"
#include "feeFunctions.h"
#include "feeDebug.h"
#include <stdlib.h>
#include "ckutilsPlatform.h"
#include <stdio.h>
#include <time.h>

#define LOOPS_DEF	100
#define MIN_SIZE_DEF	4	/* min giant size in bytes */
#define MAX_SIZE_DEF	32	/* max in bytes */
#define LOOP_NOTIFY	100


static void usage(char **argv) 
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   l=loops    (default = %d)\n", LOOPS_DEF); 
	printf("   n=maxBytes (default = %d\n", MIN_SIZE_DEF);
	printf("   x=maxBytes (default = %d\n", MAX_SIZE_DEF);
	printf("   o (use old 16-bit CryptKit\n");
	printf("   s=seed\n");
	printf("   h(elp)\n");
	exit(1);
}

/*
 * Fill buffer with random data.
 */
static void fillData(unsigned bufSize, 
	unsigned char *buf)
{
	unsigned 	*ip;
	unsigned 	intCount;
	unsigned 	residue;
	unsigned char	*cp;
	int 		i;
	
	intCount = bufSize >> 2;
	ip = (unsigned *)buf;
	for(i=0; i<intCount; i++) {
		*ip++ = RAND();
	}
	
	residue = bufSize & 0x3;
	cp = (unsigned char *)ip;
	for(i=0; i<residue; i++) {
		*cp++ = (unsigned char)RAND();
	}
}

/*
 * fill a pre-allocd giant with specified number of bytes of random 
 * data. *Buf is mallocd and uninitialized and will change here.
 */
static void genGiant(giant g, 
	unsigned numBytes, 
	unsigned char *buf)
{
	int i;
	
	fillData(numBytes, buf);
	deserializeGiant(buf, g, numBytes);
	
	/* set random sign; deserializeGiant() is always positive */
	i = RAND();
	if(i & 1) {
		g->sign = -g->sign;
	}
	
	/* avoid zero data - too many pitfalls with mod and div */
	while(isZero(g)) {
		g->sign = 1;
		g->n[0] = RAND();
	}
}

/*
 * Init giant arrays with random data.
 */
static void initRandGiants(unsigned numBytes, 
	unsigned char *buf,
	unsigned numGiants,
	giant *g1, 
	giant *g2)
{
	int i;
	
	for(i=0; i<numGiants; i++) {
	    genGiant(g1[i], numBytes, buf);
	    genGiant(g2[i], numBytes, buf);
	}
}

/*
 * Individual tests. API is identical for all tests.
 *
 * loops  : number of ops to perform.
 * g1, g2 : arrays of giants with random data and sign. Tests may modify
 *          these. Size of array = 'loops'. Capacity big enough for all
 *          conceivable ops.
 * Return : total microseconds to do 'loops' ops.
 */
 
static int mulgTest(unsigned loops, 
	giant *g1, 
	giant *g2)
{
	int loop;
	PLAT_TIME startTime;
	PLAT_TIME endTime;
	
	PLAT_GET_TIME(startTime);
	for(loop=0; loop<loops; loop++) {
		mulg(*g1++, *g2++);
	}
	PLAT_GET_TIME(endTime);
	return PLAT_GET_NS(startTime, endTime);
}

static int squareTest(unsigned loops, 
	giant *g1, 
	giant *g2)
{
	int loop;
	PLAT_TIME startTime;
	PLAT_TIME endTime;
	
	PLAT_GET_TIME(startTime);
	for(loop=0; loop<loops; loop++) {
		gsquare(*g1++);
	}
	PLAT_GET_TIME(endTime);
	return PLAT_GET_NS(startTime, endTime);
}


int main(int argc, char **argv)
{
	int		arg;
	char		*argp;
	giant		*g1;
	giant		*g2;		// ditto
	unsigned char	*buf;		// random data
	unsigned	numDigits;
	unsigned	i;
	unsigned	numBytes;
	unsigned	mulgElapsed;
	unsigned	sqrElapsed;
	
	int 		loops = LOOPS_DEF;
	int		seedSpec = 0;
	unsigned	seed = 0;
	unsigned	maxSize = MAX_SIZE_DEF;
	unsigned	minSize = MIN_SIZE_DEF;
	int 		useOld = 0;
	
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
		    case 'o':
		    	useOld = 1;
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
	buf = malloc(maxSize);

	if(!seedSpec) {
		unsigned long	tim;
		time(&tim);
		seed = (unsigned)tim;
	}
	SRAND(seed);

	/*
	 * Scratch giants, big enough for anything. Malloc here, init with
	 * random data before each test
	 * note these mallocs will be too big in the useOld case...
	 */
	g1 = malloc(sizeof(giant) * loops);
	g2 = malloc(sizeof(giant) * loops);
    if((g1 == NULL) || (g2 == NULL)) {
    	printf("malloc error\n");
    	exit(1);
    }
	if(useOld) {
	    numDigits = ((2 * maxSize) + 1) / 2;
	}
	else {
	    numDigits = BYTES_TO_GIANT_DIGITS(2 * maxSize);
	}
	for(i=0; i<loops; i++) {
	    g1[i] = newGiant(numDigits);
	    g2[i] = newGiant(numDigits);
	    if((g1[i] == NULL) || (g2[i] == NULL)) {
	    	printf("malloc error\n");
	    	exit(1);
	    }
	}

	printf("Starting giants test: seed %d\n", seed);
	for(numBytes=minSize; numBytes<=maxSize; numBytes*=2) {
		    
		initRandGiants(numBytes, 
			buf,
			loops,
			g1, 
			g2);

		mulgElapsed = mulgTest(loops, g1, g2);
		initRandGiants(numBytes, 
			buf,
			loops,
			g1, 
			g2);

		sqrElapsed = squareTest(loops, g1, g2);
		printf("  bits : %4d   mulg : %3d ns   gsquare : %3d ns\n", 
			numBytes * 8, 
			mulgElapsed / loops,
			sqrElapsed / loops);

	} /* for numBytes */
	return 0;
}
