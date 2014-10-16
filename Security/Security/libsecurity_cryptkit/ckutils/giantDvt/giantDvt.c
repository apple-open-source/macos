/*
 * Copyright (c) 1998,2011,2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
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
#define MAX_SIZE_DEF	32
#define LOOP_NOTIFY	100

/* quick test to show modg(1,g) broken */
#define MODG1_TEST_ENABLE	1

static void usage(char **argv) 
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   l=loops    (default = %d)\n", LOOPS_DEF); 
	printf("   x=maxBytes (default = %d)\n", MAX_SIZE_DEF);
	printf("   s=seed\n");
	printf("   h(elp)\n");
	exit(1);
}

/*
 * ...min <= return <= max 
 */
static int genRand(int min, int max) 
{	

    /* note random() only yields a 31-bit number... */
	
    if(max == min)			/* avoid % 1 ! */
	return(max);
    else
	return(min + (RAND() % (max-min+1)));
}

/*
 * Fill buffer with random data, random size from 1 to maxSize.
 * Returns size of random data generated.
 */
static unsigned fillData(unsigned maxSize, 
	unsigned char *data,
	int evenOnly)
{
	unsigned 	*ip;
	unsigned 	intCount;
	unsigned 	residue;
	unsigned char	*cp;
	int 		i;
	unsigned	size;
	
	size = genRand(1, maxSize);
	if(evenOnly) {
		size &= ~1;
		if(size == 1) {
			size = 2;
		}
	}
	intCount = size >> 2;
	ip = (unsigned *)data;
	for(i=0; i<intCount; i++) {
		*ip++ = RAND();
	}
	
	residue = size & 0x3;
	cp = (unsigned char *)ip;
	for(i=0; i<residue; i++) {
		*cp++ = (unsigned char)RAND();
	}
	return size;
}

/*
 * create a giant with random size and data. *Buf is mallocd and 
 * uninitialized and will change here.
 */
static giant genGiant(unsigned maxBytes, unsigned char *buf)
{
	int size = fillData(maxBytes, buf, 0);
	int i;
	giant g;
	
	g = giant_with_data(buf, size);	
	
	/* set random sign; giant_with_data() is always positive */
	i = RAND();
	if(i & 1) {
		g->sign = -g->sign;
	}
	
	/* avoid zero data - too many pitfalls with mod and div */
	if(isZero(g)) {
		g->sign = 1;
		g->n[0] = 0x77;
	}
	return g;
}

static int testError()
{
	char resp[100];
	
	printf("Attach via debugger for more info.\n");
	printf("a to abort, c to continue: ");
	gets(resp);
	return (resp[0] != 'c');
}

/* g := |g1| */
static void gabs(giant g)
{
	if(g->sign < 0) {
	    g->sign = -g->sign;
	}				
}

/*
 * Individual tests. API is identical for all tests.
 *
 * g1, g2 : giants with random data, size, and sign. Tests do not modify
 *           these.
 * scr1, scr2 : scratch giants, big enough for all conceivable ops. Can
 *           be modified at will.
 * Return : 0 for sucess, 1 on error.
 */
 
static int compTest(giant g1, giant g2, giant scr1, giant scr2)
{
	gtog(g1, scr1);		// scr1 := g1
	gtog(scr1, scr2);
	if(gcompg(g1, scr2)) {
		printf("gtog/gcompg error\n");
		return testError();
	}
	return 0;
}

static int addSubTest(giant g1, giant g2, giant scr1, giant scr2)
{
	gtog(g1, scr1);		// scr1 := g1
	addg(g2, scr1);		// scr1 := g1 + g2
	subg(g1, scr1);		// scr1 := g1 + g2 - g1 =? g2
	if(gcompg(g2, scr1)) {
		printf("addg/subg error\n");
		return testError();
	}
	return 0;
}

#define LARGEST_MUL	0xffff

static int mulTest(giant g1, giant g2, giant scr1, giant scr2)
{
	int randInt = genRand(1, LARGEST_MUL);
	int i;
	int rtn = 0;
	
	int_to_giant(randInt, scr1);	// scr1 := randInt
	gtog(g1, scr2);			// scr2 := g1
	mulg(scr1, scr2);		// scr2 := g1 * randInt
	
	/* now do the same thing with multiple adds */
	int_to_giant(0, scr1);		// scr1 := 0
	for(i=0; i<randInt; i++) {
		addg(g1, scr1);		// scr1 += g1
	}				// scr1 =? g1 * randInt
	if(gcompg(scr1, scr2)) {
		printf("g1        : "); printGiantHex(g1);
		printf("randInt   : 0x%x\n", randInt);
		printf("good prod : "); printGiantHex(scr1);
		printf("bad prod  : "); printGiantHex(scr2);
		printf("mulg error\n");
		rtn = testError();
	}
	return rtn;
	
}

static int squareTest(giant g1, giant g2, giant scr1, giant scr2)
{
	gtog(g1, scr1);
	mulg(g1, scr1);			// scr1 := g1 * g1
	gtog(g1, scr2);
	gsquare(scr2);			// scr2 =? g1 * g1
	if(gcompg(scr1, scr2)) {
		printf("gsquare error\n");
		return testError();
	}
	return 0;
}

static int lshiftTest(giant g1, giant g2, giant scr1, giant scr2)
{
    	int maxShift = (scr1->capacity - abs(g1->sign) - 1) *
			 GIANT_BITS_PER_DIGIT;
	int shiftCnt = genRand(1, maxShift);
	giant scr3 = borrowGiant(scr1->capacity);
	int rtn = 0;
	
	gtog(g1, scr1);			// scr1 := g1
	gshiftleft(shiftCnt, scr1);	// scr1 := (g1 << shiftCnt)
	
	gtog(g1, scr2);			// scr2 := g1
	if(shiftCnt <= 30) {
	    int multInt = (1 << shiftCnt);
	    int_to_giant(multInt, scr3);   // scr3 := (1 << shiftCnt)
	}
	else {
	    int_to_giant(1, scr3);	// scr3 := 1;
	    gshiftleft(shiftCnt, scr3);	// scr3 := (1 << shiftCnt)
	}
	mulg(scr3, scr2);		// scr2 := g1 * (1 << shiftCnt);
	if(gcompg(scr1, scr2)) {
		printf("shiftCnt %d 0x%x\n", shiftCnt, shiftCnt);
		printf("g1   : "); printGiantHex(g1);
		printf("scr1 : "); printGiantHex(scr1);
		printf("scr2 : "); printGiantHex(scr2);
		printf("gshiftleft error\n");
		rtn = testError();
	}
	returnGiant(scr3);
	return rtn;
}

static int rshiftTest(giant g1, giant g2, giant scr1, giant scr2)
{
    	int maxShift = bitlen(g1) - 1;
	int shiftCnt;
	giant scr3 = borrowGiant(scr1->capacity);
	int rtn = 0;
	
	/* special case, can't have g1 = 1 */
	if(maxShift == 0) {
		#if	FEE_DEBUG
		printf("...rshiftTest: tweaking g1 = 1\n");
		#endif
		g1->n[0] = 2;
		shiftCnt = 1;
	}
	else {
		shiftCnt = genRand(1, maxShift);
	}
	gtog(g1, scr1);			// scr1 := g1
	gabs(scr1);			// scr1 := |g1|
	gtog(scr1, scr2);		// scr2 := |g1|
	gshiftright(shiftCnt, scr1);	// scr1 := (|g1| >> shiftCnt)
	
	if(shiftCnt <= 30) {
	    int multInt = (1 << shiftCnt);
	    int_to_giant(multInt, scr3);   // scr3 := (1 << shiftCnt)
	}
	else {
	    int_to_giant(1, scr3);	// scr3 := 1;
	    gshiftleft(shiftCnt, scr3);	// scr3 := (1 << shiftCnt)
	}
	divg(scr3, scr2);		// scr2 := g1 / (1 << shiftCnt);
	if(gcompg(scr1, scr2)) {
		printf("shiftCnt %d 0x%x\n", shiftCnt, shiftCnt);
		printf("g1   : "); printGiantHex(g1);
		printf("scr1 : "); printGiantHex(scr1);
		printf("scr2 : "); printGiantHex(scr2);
		printf("gshiftright error\n");
		rtn = testError();
	}
	returnGiant(scr3);
	return rtn;
}

static int divTest(giant g1, giant g2, giant scr1, giant scr2)
{
	gtog(g1, scr1);		// scr1 := g1
	mulg(g2, scr1);		// scr1 := g1 * g2
	gtog(g2, scr2);		// scr2 := g2
	gabs(scr2);		// scr2 := |g2|
	divg(scr2, scr1);	// scr1 := (g1 * g2) / |g2|
	
	/* weird case - if g2 is negative, this result is -g1! */
	if(g2->sign < 0) {
	    scr1->sign = -scr1->sign;
	}
	if(gcompg(scr1, g1)) {
	    printf("g1 : "); printGiantHex(g1);		
	    printf("g2 : "); printGiantHex(g1);		
	    printf("scr1 : "); printGiantHex(scr1);		
	    printf("divTest error\n");
	    return testError();
	}
	return 0;
}

#define LARGEST_MOD_MUL	0x40

static int modTest(giant g1, giant g2, giant scr1, giant scr2)
{
	int randInt = genRand(1, LARGEST_MOD_MUL);
	giant scr3 = borrowGiant(scr1->capacity);
	/* debug only */
	giant scr4 = borrowGiant(scr1->capacity);
	/* end debug */
	int rtn = 0;
	
	int_to_giant(randInt, scr1);	// scr1 := rand
	gtog(g1, scr2);
	gabs(scr2);			// scr2 := |g1|
	
	/* current modg can't deal with g mod 1 ! */
	if((scr2->sign == 1) && (scr2->n[0] == 1)) {
		#if	MODG1_TEST_ENABLE
		/* assume that this is legal... */
		#if	FEE_DEBUG
		printf("..modTest: g1 = 1, no tweak\n");
		#endif
		#else
		printf("..modTest: tweaking g1 = 1\n");
		scr2->n[0] = 0x54;
		#endif	MODG1_TEST_ENABLE
	}
	/* end modg workaround */
	
	gtog(g2, scr3);
	gabs(scr3);			// scr3 := |g2|
	
	/* this will only work if randInt < |g1| */
	if(gcompg(scr1, scr2) >= 0) {
	    #if	FEE_DEBUG
	    printf("..modTest: tweaking rand, > g1 = "); printGiantHex(g1);
	    printf("                            g2 = "); printGiantHex(g2);
	    printf("                          rand = "); printGiantHex(scr1);
	    #endif
	    modg(scr2, scr1);		// scr1 := rand mod g1
	    if(gcompg(scr1, scr2) >= 0) {
		printf("simple modg error\n");
		return testError();
	    }
	}	
	
	mulg(scr2, scr3);		// scr3 := |g1 * g2|
	addg(scr1, scr3);		// scr3 := (|g1 * g2|) + rand
	gtog(scr3, scr4);
	modg(scr2, scr3);		// scr3 := scr3 mod |g1| =? rand
	if(gcompg(scr1, scr3)) {
		printf("g1 : "); printGiantHex(g1);
		printf("g2 : "); printGiantHex(g2);
		printf("rand : 0x%x\n", randInt);
		printf("randG : "); printGiantHex(scr1);
		printf("scr4 : "); printGiantHex(scr4);
		printf("mod  : "); printGiantHex(scr3);
		printf("modTest error\n");
		rtn = testError();
	}
	returnGiant(scr3);
	returnGiant(scr4);
	return rtn;

	
}

#if	MODG1_TEST_ENABLE
/* quickie test to demonstrate failure of modg(1, g). Known failure
 * as of 10 Apr 1998. 
 * modg(1,g) fixed on 13 Apr 1998, so this should now work.
 */
static int modg1Test(giant g1, giant scr1, giant scr2)
{
	/* test mod(x, 1) */
	scr1->n[0] = 1;
	scr1->sign = 1;
	gtog(g1, scr2);
	modg(scr1, scr2);
	if(!isZero(scr2)) {
		printf("g1 : "); printGiantHex(g1);
		printf("g1 mod 1 : "); printGiantHex(scr2);
		return testError();
	}
	return 0;
}
#endif	MODG1_TEST_ENABLE

static int mulOnesTest(giant g1, giant g2, giant scr1, giant scr2)
{
	int i;
	int rtn = 0;
	giant gOnes = borrowGiant(scr1->capacity);
	
	/* set up a giant with all ones data */
	gOnes->sign = abs(g1->sign);
	for(i=0; i<gOnes->sign; i++) {
		gOnes->n[i] = (giantDigit)(-1);
	}
	
	gtog(gOnes, scr1);		// scr1 := gOnes
	mulg(g1, scr1);			// scr1 := gOnes * g1
	
	gtog(g1, scr2);
	mulg(gOnes, scr2);

	if(gcompg(scr1, scr2)) {
		printf("good prod : "); printGiantHex(scr1);
		printf("bad prod  : "); printGiantHex(scr2);
		printf("mulOnesTest error\n");
		rtn = testError();
	}
	return rtn;
	
}

int main(int argc, char **argv)
{
	int		arg;
	char		*argp;
	giant		g1;		// init'd randomly
	giant		g2;		// ditto
	giant		scr1;		// scratch
	giant		scr2;		// ditto
	unsigned char	*buf;
	int		loop;
	
	int 		loops = LOOPS_DEF;
	int		seedSpec = 0;
	unsigned	seed = 0;
	unsigned	maxSize = MAX_SIZE_DEF;
	
	initCryptKit();
	
	#ifdef	macintosh
	seedSpec = 1;
	seed = 0;
	argc = 1;
	maxSize = 8;
	#endif
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 'x':
		    	maxSize = atoi(&argp[2]);
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
	buf = malloc(maxSize);

	if(!seedSpec) {
		time_t	tim;
		time(&tim);
		seed = (unsigned)tim;
	}
	SRAND(seed);

	/*
	 * Scratch giants, big enough for anything 
	 */
	scr1 = newGiant(4 * maxSize * GIANT_BYTES_PER_DIGIT);
	scr2 = newGiant(4 * maxSize * GIANT_BYTES_PER_DIGIT);

	printf("Starting giants test: seed %d\n", seed);
	for(loop=0; loop<loops; loop++) {
		
	    if((loop % LOOP_NOTIFY) == 0) {
	    	printf("..loop %d\n", loop);
	    }
	    
	    g1 = genGiant(maxSize, buf);
	    g2 = genGiant(maxSize, buf);
	    
	    #if 1
	    if(compTest(g1, g2, scr1, scr2)) {
		    exit(1);
	    }
	    if(addSubTest(g1, g2, scr1, scr2)) {
		    exit(1);
	    }
	    #endif
	    #if 1
	    if(mulTest(g1, g2, scr1, scr2)) {
		    exit(1);
	    }
	    #endif 
	    #if 1
	    if(squareTest(g1, g2, scr1, scr2)) {
		    exit(1);
	    }
	    if(lshiftTest(g1, g2, scr1, scr2)) {
		    exit(1);
	    }
	    if(modTest(g1, g2, scr1, scr2)) {
		    exit(1);
	    }
	    if(rshiftTest(g1, g2, scr1, scr2)) {
		    exit(1);
	    }
	    /* these all use divide....*/
	    if(divTest(g1, g2, scr1, scr2)) {
		    exit(1);
	    }
	    #if MODG1_TEST_ENABLE
	    if(modg1Test(g1, scr1, scr2)) {
	    	exit(1);
	    }
	    #endif
	    #endif 0
	    if(mulOnesTest(g1, g2, scr1, scr2)) {
	    	exit(1);
	    }
	    freeGiant(g1);
	    freeGiant(g2);
	}
	
	printf("...giantDvt complete\n");
	return 0;
}
