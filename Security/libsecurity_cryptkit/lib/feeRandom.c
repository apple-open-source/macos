/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE COMPUTER, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE COMPUTER,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * FeeRandom.c - generic, portable random number generator object
 *
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 * 19 Jun 97 	Doug Mitchell at Apple
 *	Eliminated predictability of bytes 4 thru 15 of random data
 * 18 Jun 97 	Doug Mitchell at Apple
 *	Reduced size of per-instance giants from 128 to 32 shorts
 * 23 Aug 96	Doug Mitchell at NeXT
 *	Created, based on Blaine Garst's NSRandomNumberGenerator class
 */

#include "feeRandom.h"
#include "giantIntegers.h"
#include "elliptic.h"
#include "falloc.h"
#include "feeDebug.h"
#include "byteRep.h"
#include <stdlib.h>
#include "platform.h"

/*
 * 1 ==> do extra nextNum on feeRandAllocWithSeed()
 */
#define EXTRA_NEXT_NUM	0

#define RANDBITS			128		/* must be 0 mod GIANT_BITS_PER_DIGIT */
#define RAND_GIANT_DIGITS	(RANDBITS/GIANT_BITS_PER_DIGIT)

typedef struct {
	giant A;
	giant C;
	giant SEED;
	giant x;
} randInst;

#if		GIANTS_VIA_STACK

/*
 * Prime the curveParams and giants modules for quick allocs of giants.
 */
static int giantsInitd = 0;

static void feeRandInitGiants()
{
	if(giantsInitd) {
		return;
	}
	curveParamsInitGiants();
	giantsInitd = 1;
}
#endif

static void pmod(giant x, int bits) {
	/* Force x to be x (mod 2^bits). */
	int j;
	int digits = bits / GIANT_BITS_PER_DIGIT;
	
	for(j = (digits-1); j >= 0; j--) {
		if(x->n[j] != 0) break;
	}
	x->sign = j+1;
}


feeRand feeRandAllocWithSeed(unsigned seed)
{
	randInst *rinst = (randInst *) fmalloc(sizeof(randInst));
	int digits = RAND_GIANT_DIGITS * 4;
	unsigned j;

	#if		GIANTS_VIA_STACK
	feeRandInitGiants();
	#endif
	rinst->SEED = newGiant(digits);
	rinst->C    = newGiant(digits);
	rinst->A    = newGiant(digits);
	rinst->x    = newGiant(digits);
	rinst->C->sign = rinst->A->sign = rinst->SEED->sign = RAND_GIANT_DIGITS;
	for(j=0; j<RAND_GIANT_DIGITS; j++) {
	    rinst->C->n[j]    = (giantDigit)(seed + 0xdddddddd - j);
	    rinst->A->n[j]    = (giantDigit)(seed + 0xfff12223 + j);
	    rinst->SEED->n[j] = (giantDigit)(seed + j);
	}

	/*
	 * on the first feeRandBytes or feeRandNextNum, bytes 4 and 5 of
	 * the result are duplicated 4.5 times (up to byte 15). Subsequent
	 * data is indeed random. Thus...
	 */
	#if	EXTRA_NEXT_NUM
	feeRandNextNum(rinst);
	#endif	// EXTRA_NEXT_NUM
	return rinst;
}

feeRand feeRandAlloc(void)
{
	return feeRandAllocWithSeed(createRandomSeed());
}

void feeRandFree(feeRand frand)
{
	randInst *rinst = (randInst *) frand;

	clearGiant(rinst->A);
	freeGiant(rinst->A);
	clearGiant(rinst->C);
	freeGiant(rinst->C);
	clearGiant(rinst->SEED);
	freeGiant(rinst->SEED);
	clearGiant(rinst->x);
	freeGiant(rinst->x);
	ffree(rinst);
}

unsigned feeRandNextNum(feeRand frand)
{
	randInst *rinst = (randInst *) frand;
	unsigned rtn;

	mulg(rinst->A, rinst->SEED);
	addg(rinst->C, rinst->SEED);
	pmod(rinst->SEED, RANDBITS);
	gtog(rinst->SEED, rinst->x);

	/*
	 * FIXME - this is not quite correct; rinst->x only has 4 bytes
	 * of valid data if RANDBITS is known to be greater than or equal
	 * to 32.
	 */
	rtn = byteRepToInt((unsigned char *)&rinst->x->n);
	return rtn;
}

void feeRandBytes(feeRand frand,
	unsigned char *bytes,		/* must be alloc'd by caller */
	unsigned numBytes)
{
	randInst *rinst = (randInst *) frand;
	int length;
   	unsigned toCopy;
	unsigned char *cp = bytes;

	for (length = numBytes; length > 0; length -= RANDBITS/8) {
		mulg(rinst->A, rinst->SEED);
		addg(rinst->C, rinst->SEED);
		pmod(rinst->SEED, RANDBITS);
		gtog(rinst->SEED, rinst->x);

		toCopy = RANDBITS/8;
		if(length < toCopy) {
			toCopy = length;
		}

		/*
		 * FIXME - not 100% platform independent....
		 */
		bcopy(rinst->x->n, cp, toCopy);
		cp += toCopy;
	}
}

/* new function, 5 March 1999 - dmitch */
void feeRandAddEntropy(feeRand frand, unsigned entropy)
{
        randInst *rinst = (randInst *) frand;
        giant tmp = borrowGiant(RAND_GIANT_DIGITS);
	unsigned i;
	
	if(entropy == 0) {
		/* boy would that be a mistake */
		entropy = 0x12345;
	}
	for(i=0; i<RAND_GIANT_DIGITS; i++) {
		tmp->n[i] = (giantDigit)entropy;
	}
	tmp->sign = RAND_GIANT_DIGITS;
        mulg(tmp, rinst->SEED);
        addg(rinst->C, rinst->SEED);
        pmod(rinst->SEED, RANDBITS);
        entropy ^= 0xff0ff0ff; 
	if(entropy == 0) {
		entropy = 0x12345;
	}
 	for(i=0; i<RAND_GIANT_DIGITS; i++) {
		tmp->n[i] = (giantDigit)entropy;
	}
       	mulg(tmp, rinst->A);
        addg(rinst->C, rinst->A);
        pmod(rinst->A, RANDBITS);
        /* leave C alone */
       	returnGiant(tmp);
}
