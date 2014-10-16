/* Copyright (c) 1998,2011-2014 Apple Inc.  All Rights Reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************

   giantIntegers.c - library for large-integer arithmetic.

 Revision History
 ----------------
 	Fixed a==b bug in addg(). 
 10/06/98		ap
 	Changed to compile with C++.
 13 Apr 98	Fixed shiftright(1) bug in modg_via_recip.
 09 Apr 98 at Apple
 	Major rewrite of core arithmetic routines to make this module
		independent of size of giantDigit.
 	Removed idivg() and radixdiv().
 20 Jan 98 at Apple
 	Deleted FFT arithmetic; simplified mulg().
 09 Jan 98 at Apple
 	gshiftright() optimization.
 08 Jan 98 at Apple
 	newGiant() returns NULL on malloc failure
 24 Dec 97 at Apple
 	New grammarSquare(); optimized modg_via_recip()
 11 Jun 97 at Apple
 	Added modg_via_recip(), divg_via_recip(), make_recip()
	Added new multiple giant stack mechanism
	Fixed potential packing/alignment bug in copyGiant()
	Added profiling for borrowGiant(), returnGiant()
	Deleted obsolete ifdef'd code
	Deleted newgiant()
	All calls to borrowGiant() now specify required size (no more
		borrowGiant(0) calls)
 08 May 97 at Apple
 	Changed size of giantstruct.n to 1 for Mac build
 05 Feb 97 at Apple
 	newGiant() no longer modifies CurrentMaxShorts or giant stack
	Added modg profiling
 01 Feb 97 at NeXT
 	Added iszero() check in gcompg
 17 Jan 97 at NeXT
 	Fixed negation bug in gmersennemod()
  	Fixed n[words-1] == 0 bug in extractbits()
	Cleaned up lots of static declarations
 19 Sep 96 at NeXT
 	Fixed --size underflow bug in normal_subg().
  4 Sep 96 at NeXT
  	Fixed (b<n), (sign<0) case in gmersennemod() to allow for arbitrary n.
  9 Aug 96 at NeXT
  	Fixed sign-extend bug in data_to_giant().
  7 Aug 96 at NeXT
  	Changed precision in newtondivide().
	Removed #ifdef UNUSED code.
 24 Jul 96 at NeXT
 	Added scompg().
      	Created.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "giantIntegers.h"
#include "feeDebug.h"
#include "ckconfig.h"
#include "ellipticMeasure.h"
#include "falloc.h"
#include "giantPortCommon.h"

#ifdef	FEE_DEBUG
#if (GIANT_LOG2_BITS_PER_DIGIT == 4)
#warning Compiling with two-byte giantDigits
#endif
#endif

#if 0
#if	FEE_DEBUG
char printbuf1[200];
char printbuf2[200];
char printbuf3[200];
void printGiantBuf(giant x)
{
	int i;

	sprintf(printbuf2, "sign=%d cap=%d n[]=", x->sign, x->capacity);
	for(i=0; i<abs(x->sign); i++) {
		sprintf(printbuf3 + 10*i, "%u:", x->n[i]);
	}
}

char printbuf4[200];
char printbuf5[200];
void printGiantBuf2(giant x)
{
	int i;

	sprintf(printbuf4, "sign=%d cap=%d n[]=", x->sign, x->capacity);
	for(i=0; i<abs(x->sign); i++) {
		sprintf(printbuf5 + 10*i, "%u:", x->n[i]);
	}
}
#endif	/* FEE_DEBUG */
#endif /* 0 */

/******** debugging flags *********/

/*
 * Flag use of unoptimized divg, modg, binvg
 */
//#define WARN_UNOPTIMIZE			FEE_DEBUG
#define WARN_UNOPTIMIZE			0

/*
 * Log interesting giant stack events
 */
#define LOG_GIANT_STACK			0

/*
 * Log allocation of giant larger than stack size
 */
#define LOG_GIANT_STACK_OVERFLOW	1

/*
 * Flag newGiant(0) and borrowGiant(0) calls
 */
#define WARN_ZERO_GIANT_SIZE		FEE_DEBUG

/* temp mac-only giant initialization debug */
#define GIANT_MAC_DEBUG		0
#if	GIANT_MAC_DEBUG

#include <string.h>
#include <TextUtils.h>

/* this one needs a writable string */
static void logCom(unsigned char *str) {
	c2pstr((char *)str);
	DebugStr(str);
}

/* constant strings */
void dblog0(const char *str)	{		
	Str255	outStr;				
	strcpy((char *)outStr, str);	
	logCom(outStr);					
}

#else	
#define dblog0(s)

#endif	/* GIANT_MAC_DEBUG */

#ifndef	min
#define min(a,b) ((a)<(b)? (a) : (b))
#endif	// min
#ifndef	max
#define max(a,b) ((a)>(b)? (a) : (b))
#endif	// max

#ifndef	TRUE
#define TRUE		1
#endif	// TRUE
#ifndef	FALSE
#define FALSE		0
#endif	// FALSE

static void absg(giant g);  /* g := |g|. */

/************** globals *******************/


/* ------ giant stack package ------ */

/*
 * The giant stack package is a local cache which allows us to avoid calls
 * to malloc() for borrowGiant(). On a 90 Mhz Pentium, enabling the
 * giant stack package shows about a 1.35 speedup factor over an identical
 * CryptKit without the giant stacks enabled.
 */

#if	GIANTS_VIA_STACK

#if	LOG_GIANT_STACK
#define gstackDbg(x)		printf x
#else	// LOG_GIANT_STACK
#define gstackDbg(x)
#endif	// LOG_GIANT_STACK

typedef struct {
	unsigned 	numDigits;	// capacity of giants in this stack
	unsigned	numFree;	// number of free giants in stack
	unsigned	totalGiants;	// total number in *stack
	giant 		*stack;
} gstack;

static gstack *gstacks = NULL;		// array of stacks
static unsigned numGstacks = 0;		// # of elements in gstacks
static int gstackInitd = 0;		// this module has been init'd

#define INIT_NUM_GIANTS		16	/* initial # of giants / stack */
#define MIN_GIANT_SIZE		4	/* numDigits for gstack[0]  */
#define GIANT_SIZE_INCR		2	/* in << bits */

/*
 * Initialize giant stacks, with up to specified max giant size.
 */
void initGiantStacks(unsigned maxDigits)
{
	unsigned curSize = MIN_GIANT_SIZE;
	unsigned sz;
	unsigned i;

	dblog0("initGiantStacks\n");
	
	if(gstackInitd) {
		/*
		 * Shouldn't be called more than once...
		 */
		printf("multiple initGiantStacks calls\n");
		return;
	}
	gstackDbg(("initGiantStacks(%d)\n", maxDigits));

	/*
	 * How many stacks?
	 */
	numGstacks = 1;
	while(curSize<=maxDigits) {
		curSize <<= GIANT_SIZE_INCR;
		numGstacks++;
	}

	sz = sizeof(gstack) * numGstacks;
	gstacks = (gstack*) fmalloc(sz);
	bzero(gstacks, sz);

	curSize = MIN_GIANT_SIZE;
	for(i=0; i<numGstacks; i++) {
		gstacks[i].numDigits = curSize;
		curSize <<= GIANT_SIZE_INCR;
	}

	gstackInitd = 1;
}

/* called at shut down - free resources */ 
void freeGiantStacks(void)
{
	int i;
	int j;
	gstack *gs;
	
	if(!gstackInitd) {
		return;
	}
	for(i=0; i<numGstacks; i++) {
		gs = &gstacks[i];
		for(j=0; j<gs->numFree; j++) {
			freeGiant(gs->stack[j]);
			gs->stack[j] = NULL;
		}
		/* and the stack itself - may be null if this was never used */
		if(gs->stack != NULL) {
			ffree(gs->stack);
			gs->stack = NULL;
		}
	}
	ffree(gstacks);
	gstacks = NULL;
	gstackInitd = 0;
}

#endif	// GIANTS_VIA_STACK

giant borrowGiant(unsigned numDigits)
{
	giant 		result;

	#if	GIANTS_VIA_STACK

	unsigned 	stackNum;
	gstack 		*gs = gstacks;

	#if 	WARN_ZERO_GIANT_SIZE
	if(numDigits == 0) {
		printf("borrowGiant(0)\n");
		numDigits = gstacks[numGstacks-1].numDigits;
	}
	#endif	// WARN_ZERO_GIANT_SIZE

	/*
	 * Find appropriate stack
	 */
	if(numDigits <= MIN_GIANT_SIZE)
	        stackNum = 0;
	else if (numDigits <= (MIN_GIANT_SIZE << GIANT_SIZE_INCR))
	        stackNum = 1;
	else if (numDigits <= (MIN_GIANT_SIZE << (2 * GIANT_SIZE_INCR)))
	        stackNum = 2;
	else if (numDigits <= (MIN_GIANT_SIZE << (3 * GIANT_SIZE_INCR)))
	        stackNum = 3;
	else if (numDigits <= (MIN_GIANT_SIZE << (4 * GIANT_SIZE_INCR)))
	        stackNum = 4;
	else
		stackNum = numGstacks;

	if(stackNum >= numGstacks) {
		/*
		 * out of bounds; just malloc
		 */
		#if	LOG_GIANT_STACK_OVERFLOW
		gstackDbg(("giantFromStack overflow; numDigits %d\n",
			numDigits));
		#endif	// LOG_GIANT_STACK_OVERFLOW
		return newGiant(numDigits);
	}
 	gs = &gstacks[stackNum];

	#if	GIANT_MAC_DEBUG
	if((gs->numFree != 0) && (gs->stack == NULL)) {
		dblog0("borrowGiant: null stack!\n");
	}
	#endif
 
   	if(gs->numFree != 0) {
        	result = gs->stack[--gs->numFree];
	}
    	else {
		/*
		 * Stack empty; malloc
		 */
    		result = newGiant(gs->numDigits);
	}

	#else	/* GIANTS_VIA_STACK */

	result = newGiant(numDigits);

	#endif	/* GIANTS_VIA_STACK */

	PROF_INCR(numBorrows);
	return result;
}

void returnGiant(giant g)
{

	#if	GIANTS_VIA_STACK

	unsigned 	stackNum;
	gstack 		*gs;
	unsigned 	cap = g->capacity;


	#if	FEE_DEBUG
	if(!gstackInitd) {
		CKRaise("returnGiant before stacks initialized!");
	}
	#endif	// FEE_DEBUG

	#if	GIANT_MAC_DEBUG
	if(g == NULL) {
		dblog0("returnGiant: null g!\n");
	}
	#endif

	/*
	 * Find appropriate stack. Note we expect exact match of
	 * capacity and stack's giant size.
	 */
	/*
	 * Optimized unrolled loop. Just make sure there are enough cases
	 * to handle all of the stacks. Errors in this case will be flagged
	 * via LOG_GIANT_STACK_OVERFLOW.
	 */
	switch(cap) {
	    case MIN_GIANT_SIZE:
	        stackNum = 0;
		break;
	    case MIN_GIANT_SIZE << GIANT_SIZE_INCR:
	        stackNum = 1;
		break;
	    case MIN_GIANT_SIZE << (2 * GIANT_SIZE_INCR):
	        stackNum = 2;
		break;
	    case MIN_GIANT_SIZE << (3 * GIANT_SIZE_INCR):
	        stackNum = 3;
		break;
	    case MIN_GIANT_SIZE << (4 * GIANT_SIZE_INCR):
	        stackNum = 4;
		break;
	    default:
	        stackNum = numGstacks;
		break;
	}

	if(stackNum >= numGstacks) {
		/*
		 * out of bounds; just free
		 */
		#if	LOG_GIANT_STACK_OVERFLOW
		gstackDbg(("giantToStack overflow; numDigits %d\n", cap));
		#endif	// LOG_GIANT_STACK_OVERFLOW
		freeGiant(g);
		return;
	}
	gs = &gstacks[stackNum];
    	if(gs->numFree == gs->totalGiants) {
	    	if(gs->totalGiants == 0) {
			gstackDbg(("Initial alloc of gstack(%d)\n",
				gs->numDigits));
	    		gs->totalGiants = INIT_NUM_GIANTS;
	    	}
	    	else {
			gs->totalGiants *= 2;
			gstackDbg(("Bumping gstack(%d) to %d\n",
				gs->numDigits, gs->totalGiants));
		}
	    	gs->stack = (giantstruct**) frealloc(gs->stack, gs->totalGiants*sizeof(giant));
    	}
   	g->sign = 0;		// not sure this is important...
    	gs->stack[gs->numFree++] = g;

	#if	GIANT_MAC_DEBUG
	if((gs->numFree != 0) && (gs->stack == NULL)) {
		dblog0("borrowGiant: null stack!\n");
	}
	#endif
	
	#else	/* GIANTS_VIA_STACK */

	freeGiant(g);

	#endif	/* GIANTS_VIA_STACK */
}

void freeGiant(giant x) {
    ffree(x);
}

giant newGiant(unsigned numDigits) {
    // giant sufficient for 2^numbits+16 sized ops
    int size;
    giant result;

    #if 	WARN_ZERO_GIANT_SIZE
    if(numDigits == 0) {
        printf("newGiant(0)\n");
		#if	GIANTS_VIA_STACK
        numDigits = gstacks[numGstacks-1].totalGiants;
		#else
		/* HACK */
		numDigits = 20;
		#endif
    }
    #endif	// WARN_ZERO_GIANT_SIZE

    size = (numDigits-1) * GIANT_BYTES_PER_DIGIT + sizeof(giantstruct);
    result = (giant)fmalloc(size);
    if(result == NULL) {
    	return NULL;
    }
    result->sign = 0;
    result->capacity = numDigits;
    return result;
}

giant copyGiant(giant x)
{
	int bytes;

	giant result = newGiant(x->capacity);

	/*
	 * 13 Jun 1997
	 * NO! this assumes packed alignment
	 */
	bytes = sizeof(giantstruct) +
		((x->capacity - 1) * GIANT_BYTES_PER_DIGIT);
	bcopy(x, result, bytes);
	return result;
}

/* ------ initialization and utility routines ------ */


unsigned bitlen(giant n) {
    unsigned 	b = GIANT_BITS_PER_DIGIT;
    giantDigit 	c = 1 << (GIANT_BITS_PER_DIGIT - 1);
    giantDigit 	w;

    if (isZero(n)) {
    	return(0);
    }
    w = n->n[abs(n->sign) - 1];
    if (!w) {
    	CKRaise("bitlen - no bit set!");
    }
    while((w&c) == 0) {
        b--;
        c >>= 1;
    }
    return(GIANT_BITS_PER_DIGIT * (abs(n->sign)-1) + b);
}

int bitval(giant n, int pos) {
    int i = abs(pos) >> GIANT_LOG2_BITS_PER_DIGIT;
    giantDigit c = 1 << (pos & (GIANT_BITS_PER_DIGIT - 1));

    return((n->n[i]) & c);
}

int gsign(giant g)
/* returns the sign of g */
{
	if (isZero(g)) return(0);
	if (g->sign > 0) return(1);
	return(-1);
}

/*
 * Adjust sign for possible leading (m.s.) zero digits
 */
void gtrimSign(giant g)
{
	int numDigits = abs(g->sign);
	int i;

	for(i=numDigits-1; i>=0; i--) {
		if(g->n[i] == 0) {
			numDigits--;
		}
		else {
			break;
		}
	}
	if(g->sign < 0) {
		g->sign = -numDigits;
	}
	else {
		g->sign = numDigits;
	}
}


int isone(giant g) {
    return((g->sign==1)&&(g->n[0]==1));
}

int isZero(giant thegiant) {
/* Returns TRUE if thegiant == 0.  */
    int count;
    int length = abs(thegiant->sign);
    giantDigit *numpointer;

    if (length) {
        numpointer = thegiant->n;

        for(count = 0; count<length; ++count,++numpointer)
            if (*numpointer != 0 ) return(FALSE);
    }
    return(TRUE);
}

int gcompg(giant a, giant b)
/* returns -1,0,1 if a<b, a=b, a>b, respectively */
{
    int sa = a->sign;
    int j;
    int sb = b->sign;
    giantDigit va;
    giantDigit vb;
    int sgn;

    if(isZero(a) && isZero(b)) return 0;
    if(sa > sb) return(1);
    if(sa < sb) return(-1);
    if(sa < 0) {
    	sa = -sa; /* Take absolute value of sa */
	sgn = -1;
    } else sgn = 1;
    for(j = sa-1; j >= 0; j--) {
        va = a->n[j]; vb = b->n[j];
	if (va > vb) return(sgn);
	if (va < vb) return(-sgn);
    }
    return(0);
}

/* destgiant becomes equal to srcgiant */
void gtog(giant srcgiant, giant destgiant) {

    int numbytes;

    CKASSERT(srcgiant != NULL);
    numbytes =  abs(srcgiant->sign) * GIANT_BYTES_PER_DIGIT;
    if (destgiant->capacity < abs(srcgiant->sign))
	CKRaise("gtog overflow!!");
    memcpy((char *)destgiant->n, (char *)srcgiant->n, numbytes);
    destgiant->sign = srcgiant->sign;
}

void int_to_giant(int i, giant g) {
/* The giant g becomes set to the integer value i. */
    int isneg = (i<0);
    unsigned int j = abs(i);
    unsigned dex;

    g->sign = 0;
    if (i==0) {
	g->n[0] = 0;
	return;
    }

    if(GIANT_BYTES_PER_DIGIT == sizeof(int)) {
    	g->n[0] = j;
	g->sign = 1;
    }
    else {
	/* one loop per digit */
	unsigned scnt = GIANT_BITS_PER_DIGIT;	// fool compiler

	for(dex=0; dex<sizeof(int); dex++) {
	    g->n[dex] = j & GIANT_DIGIT_MASK;
	    j >>= scnt;
	    g->sign++;
	    if(j == 0) {
		break;
	    }
	}
    }
    if (isneg) {
    	g->sign = -(g->sign);
    }
}

/*------------- Arithmetic --------------*/

void negg(giant g) {
/* g becomes -g */
    g->sign = -g->sign;
}

void iaddg(int i, giant g) {  /* positive g becomes g + (int)i */
    int j;
    giantDigit carry;
    int size = abs(g->sign);

    if (isZero(g)) {
    	int_to_giant(i,g);
    }
    else {
    	carry = i;
    	for(j=0; ((j<size) && (carry != 0)); j++) {
            g->n[j] = giantAddDigits(g->n[j], carry, &carry);
        }
	if(carry) {
	    ++g->sign;
	    // realloc
	    if (g->sign > (int)g->capacity) CKRaise("iaddg overflow!");
	    g->n[size] = carry;
	}
    }
}

/*
 * g *= (int n)
 *
 * FIXME - we can improve this...
 */
void imulg(unsigned n, giant g)
{
	giant tmp = borrowGiant(abs(g->sign) + sizeof(int));

	int_to_giant(n, tmp);
	mulg(tmp, g);
	returnGiant(tmp);
}

static void normal_addg(giant a, giant b)
/*  b := a + b, both a,b assumed non-negative.    */
{
    giantDigit carry1 = 0;
    giantDigit carry2 = 0;
    int asize = a->sign, bsize = b->sign;
    giantDigit *an = a->n;
    giantDigit *bn = b->n;
    giantDigit tmp;
    int j;
    int comSize;
    int maxSize;

    if(asize < bsize) {
        comSize = asize;
	maxSize = bsize;
    }
    else {
        comSize = bsize;
	maxSize = asize;
    }

    /* first handle the common digits */
    for(j=0; j<comSize; j++) {
	/*
	 * first add the carry, then an[j] - either add could result
	 * in another carry
	 */
	if(carry1 || carry2) {
	    tmp = giantAddDigits(bn[j], (giantDigit)1, &carry1);
	}
	else {
	    carry1 = 0;
	    tmp = bn[j];
	}
	bn[j] = giantAddDigits(tmp, an[j], &carry2);
    }

    if(asize < bsize) {

	/* now propagate remaining carry beyond asize */
	if(carry2) {
	    carry1 = 1;
	}
	if(carry1) {
	    for(; j<bsize; j++) {
	        bn[j] = giantAddDigits(bn[j], (giantDigit)1, &carry1);
		if(carry1 == 0) {
		    break;
		}
	    }
	}
    } else {
	/* now propagate remaining an[] and carry beyond bsize */
	if(carry2) {
	    carry1 = 1;
	}
	for(; j<asize; j++) {
	    if(carry1) {
	    	bn[j] = giantAddDigits(an[j], (giantDigit)1, &carry1);
	    }
	    else {
	        bn[j] = an[j];
		carry1 = 0;
	    }
	}
    }
    b->sign = maxSize;
    if(carry1) {
	// realloc?
	bn[j] = 1;
	b->sign++;
	if (b->sign > (int)b->capacity) CKRaise("iaddg overflow!");
    }

}

static void normal_subg(giant a, giant b)
/* b := b - a; requires b, a non-negative and b >= a. */
{
    int j;
    int size = b->sign;
    giantDigit tmp;
    giantDigit borrow1 = 0;
    giantDigit borrow2 = 0;
    giantDigit *an = a->n;
    giantDigit *bn = b->n;

    if(a->sign == 0) {
    	return;
    }

    for (j=0; j<a->sign; ++j) {
    	if(borrow1 || borrow2) {
	    tmp = giantSubDigits(bn[j], (giantDigit)1, &borrow1);
	}
	else {
	    tmp = bn[j];
	    borrow1 = 0;
	}
	bn[j] = giantSubDigits(tmp, an[j], &borrow2);
    }
    if(borrow1 || borrow2) {
    	/* propagate borrow thru remainder of bn[] */
    	borrow1 = 1;
	for (j=a->sign; j<size; ++j) {
	    if(borrow1) {
		bn[j] = giantSubDigits(bn[j], (giantDigit)1, &borrow1);
	    }
	    else {
		break;
	    }
	}
    }

    /* adjust sign for leading zero digits */
    while((size-- > 0) && (b->n[size] == 0))
    	;
    b->sign = (b->n[size] == 0)? 0 : size+1;
}

static void reverse_subg(giant a, giant b)
/* b := a - b; requires b, a non-negative and a >= b. */
{
    int j;
    int size = a->sign;
    giantDigit tmp;
    giantDigit borrow1 = 0;
    giantDigit borrow2 = 0;
    giantDigit *an = a->n;
    giantDigit *bn = b->n;

    if(b->sign == 0) {
    	gtog(a, b);
	return;
    }
    for (j=0; j<b->sign; ++j) {
    	if(borrow1 || borrow2) {
	    tmp = giantSubDigits(an[j], (giantDigit)1, &borrow1);
	}
	else {
	    tmp = an[j];
	    borrow1 = 0;
	}
	bn[j] = giantSubDigits(tmp, bn[j], &borrow2);
    }
    if(borrow1 || borrow2) {
    	/* propagate borrow thru remainder of bn[] */
    	borrow1 = 1;
    }
    for (j=b->sign; j<size; ++j) {
	if(borrow1) {
	    bn[j] = giantSubDigits(an[j], (giantDigit)1, &borrow1);
	}
	else {
	    bn[j] = an[j];
	    borrow1 = 0;
	}
    }

    b->sign = size; /* REC, 21 Apr 1996. */
    while(!b->n[--size]);
    b->sign = size+1;
}


void addg(giant a, giant b)
/* b := b + a, any signs any result. */
{   int asgn = a->sign, bsgn = b->sign;
    if(asgn == 0) return;
    if(bsgn == 0) {
    	gtog(a,b);
	return;
    }
    if((asgn < 0) == (bsgn < 0)) {
    	if(bsgn > 0) {
		normal_addg(a,b);
		return;
	}
	negg(a); if(a != b) negg(b); normal_addg(a,b);  /* Fix REC 1 Dec 98. */
	negg(a); if(a != b) negg(b); return;  /* Fix REC 1 Dec 98. */
    }
    if(bsgn > 0) {
        negg(a);
	if(gcompg(b,a) >= 0) {
		normal_subg(a,b);
		negg(a);
		return;
	}
	reverse_subg(a,b);
	negg(a);
	negg(b);
	return;
    }
    negg(b);
    if(gcompg(b,a) < 0) {
    	reverse_subg(a,b);
	return;
    }
    normal_subg(a,b);
    negg(b);
    return;
}

void subg(giant a, giant b)
/* b := b - a, any signs, any result. */
{
  int asgn = a->sign, bsgn = b->sign;
  if(asgn == 0) return;
  if(bsgn == 0) {
    	gtog(a,b);
	negg(b);
	return;
  }
  if((asgn < 0) != (bsgn < 0)) {
  	if(bsgn > 0) {
		negg(a);
		normal_addg(a,b);
		negg(a);
		return;
	}
	negg(b);
	normal_addg(a,b);
	negg(b);
	return;
  }
  if(bsgn > 0) {
  	if(gcompg(b,a) >= 0) {
		normal_subg(a,b);
		return;
	}
	reverse_subg(a,b);
	negg(b);
	return;
  }
  negg(a); negg(b);
  if(gcompg(b,a) >= 0) {
		normal_subg(a,b);
		negg(a);
		negg(b);
		return;
  }
  reverse_subg(a,b);
  negg(a);
  return;
}

static void bdivg(giant v, giant u)
/* u becomes greatest power of two not exceeding u/v. */
{
    int diff = bitlen(u) - bitlen(v);
    giant scratch7;

    if (diff<0) {
        int_to_giant(0,u);
        return;
    }
    scratch7 = borrowGiant(u->capacity);
    gtog(v, scratch7);
    gshiftleft(diff,scratch7);
    if(gcompg(u,scratch7) < 0) diff--;
    if(diff<0) {
        int_to_giant(0,u);
    	returnGiant(scratch7);
        return;
    }
    int_to_giant(1,u);
    gshiftleft(diff,u);
    returnGiant(scratch7);
}

int binvaux(giant p, giant x)
/* Binary inverse method.
   Returns zero if no inverse exists, in which case x becomes
   GCD(x,p). */
{
    giant scratch7;
    giant u0;
    giant u1;
    giant v0;
    giant v1;
    int result = 1;
    int giantSize;
    PROF_START;

    if(isone(x)) return(result);
    giantSize = 4 * abs(p->sign);
    scratch7 = borrowGiant(giantSize);
    u0 = borrowGiant(giantSize);
    u1 = borrowGiant(giantSize);
    v0 = borrowGiant(giantSize);
    v1 = borrowGiant(giantSize);
    int_to_giant(1, v0); gtog(x, v1);
    int_to_giant(0,x); gtog(p, u1);
    while(!isZero(v1)) {
        gtog(u1, u0); bdivg(v1, u0);
        gtog(x, scratch7);
        gtog(v0, x);
        mulg(u0, v0);
        subg(v0,scratch7);
        gtog(scratch7, v0);

        gtog(u1, scratch7);
        gtog(v1, u1);
        mulg(u0, v1);
        subg(v1,scratch7);
        gtog(scratch7, v1);
    }
    if (!isone(u1)) {
        gtog(u1,x);
        if(x->sign<0) addg(p, x);
        result = 0;
        goto done;
    }
    if (x->sign<0) addg(p, x);
  done:
    returnGiant(scratch7);
    returnGiant(u0);
    returnGiant(u1);
    returnGiant(v0);
    returnGiant(v1);
    PROF_END(binvauxTime);
    return(result);
}

/*
 * Superceded by binvg_cp()
 */
#if	0
int binvg(giant p, giant x)
{
	modg(p, x);
	return(binvaux(p,x));
}
#endif

static void absg(giant g) {
/* g becomes the absolute value of g */
    if (g->sign < 0) g->sign = -g->sign;
}

void gshiftleft(int bits, giant g) {
/* shift g left bits bits.  Equivalent to g = g*2^bits */
    int 	rem = bits & (GIANT_BITS_PER_DIGIT - 1);
    int 	crem = GIANT_BITS_PER_DIGIT - rem;
    int 	digits = 1 + (bits >> GIANT_LOG2_BITS_PER_DIGIT);
    int 	size = abs(g->sign);
    int 	j;
    int 	k;
    int 	sign = gsign(g);
    giantDigit 	carry;
    giantDigit 	dat;

    #if		FEE_DEBUG
    if(bits < 0) {
        CKRaise("gshiftleft(-bits)\n");
    }
    #endif	/* FEE_DEBUG */

    if(!bits) return;
    if(!size) return;
    if((size+digits) > (int)g->capacity) {
        CKRaise("gshiftleft overflow");
        return;
    }
    k = size - 1 + digits;	// (MSD of result + 1)
    carry = 0;

    /* bug fix for 32-bit giantDigits; this is also an optimization for
     * other sizes. rem=0 means we're shifting strictly by digits, no
     * bit shifts. */
    if(rem == 0) {
        g->n[k] = 0;		// XXX hack - for sign fixup
	for(j=size-1; j>=0; j--) {
	    g->n[--k] = g->n[j];
	}
	do{
	    g->n[--k] = 0;
	} while(k>0);
    }
    else {
    	/*
	 * normal unaligned case
	 * FIXME - this writes past g->n[size-1] the first time thru!
	 */
	for(j=size-1; j>=0; j--) {
	    dat = g->n[j];
	    g->n[k--] = (dat >> crem) | carry;
	    carry = (dat << rem);
	}
	do{
	    g->n[k--] = carry;
	    carry = 0;
	} while(k>=0);
    }
    k = size - 1 + digits;
    if(g->n[k] == 0) --k;
    g->sign = sign * (k+1);
    if (abs(g->sign) > g->capacity) {
    	CKRaise("gshiftleft overflow");
    }
}

void gshiftright(int bits, giant g) {
/* shift g right bits bits.  Equivalent to g = g/2^bits */
    int j;
    int size=abs(g->sign);
    giantDigit carry;
    int digits = bits >> GIANT_LOG2_BITS_PER_DIGIT;
    int remain = bits & (GIANT_BITS_PER_DIGIT - 1);
    int cremain = GIANT_BITS_PER_DIGIT - remain;

    #if		FEE_DEBUG
    if(bits < 0) {
        CKRaise("gshiftright(-bits)\n");
    }
    #endif	/* FEE_DEBUG */
    if(bits==0) return;
    if(isZero(g)) return;
    if (digits >= size) {
        g->sign = 0;
        return;
    }

    size -= digits;

/* Begin OPT: 9 Jan 98 REC. */
    if(remain == 0) {
        if(g->sign > 0) {
	    g->sign = size;
	}
	else {
	    g->sign = -size;
	}
        for(j=0; j < size; j++) {
	    g->n[j] = g->n[j+digits];
	}
        return;
    }
/* End OPT: 9 Jan 98 REC. */

    for(j=0;j<size;++j) {
        if (j==size-1) {
	    carry = 0;
	}
        else {
	    carry = (g->n[j+digits+1]) << cremain;
	}
        g->n[j] = ((g->n[j+digits]) >> remain ) | carry;
    }
    if (g->n[size-1] == 0) {
    	--size;
    }
    if(g->sign > 0) {
    	g->sign = size;
    }
    else {
        g->sign = -size;
    }
    if (abs(g->sign) > g->capacity) {
    	CKRaise("gshiftright overflow");
    }
}


void extractbits(unsigned n, giant src, giant dest) {
/* dest becomes lowermost n bits of src.  Equivalent to dest = src % 2^n */
    int digits = n >> GIANT_LOG2_BITS_PER_DIGIT;
    int numbytes = digits * GIANT_BYTES_PER_DIGIT;
    int bits = n & (GIANT_BITS_PER_DIGIT - 1);

    if (n <= 0) {
    	return;
    }
    if (dest->capacity * 8 * GIANT_BYTES_PER_DIGIT < n) {
    	CKRaise("extractbits - not enough room");
    }
    if (digits >= abs(src->sign)) {
    	gtog(src,dest);
    }
    else {
          memcpy((char *)(dest->n), (char *)(src->n), numbytes);
          if (bits) {
              dest->n[digits] = src->n[digits] & ((1<<bits)-1);
              ++digits;
          }
	  /* Next, fix by REC, 12 Jan 97. */
          // while((dest->n[words-1] == 0) && (words > 0)) --words;
          while((digits > 0) && (dest->n[digits-1] == 0)) {
	      --digits;
	  }
          if(src->sign < 0) {
	      dest->sign = -digits;
	  }
          else {
	      dest->sign = digits;
	  }
    }
    if (abs(dest->sign) > dest->capacity) {
    	CKRaise("extractbits overflow");
    }
}

#define NEW_MERSENNE	0

/*
 * New gmersennemod, 24 Dec 1997. This runs significantly slower than the
 * original.
 */
#if	NEW_MERSENNE

void
gmersennemod(
	int 	n,
	giant 	g
)
/* g := g (mod 2^n - 1) */
{
    int the_sign;
    giant scratch3 = borrowGiant(g->capacity);
    giant scratch4 = borrowGiant(1);

    if ((the_sign = gsign(g)) < 0) absg(g);
    while (bitlen(g) > n) {
	gtog(g,scratch3);
	gshiftright(n,scratch3);
	addg(scratch3,g);
	gshiftleft(n,scratch3);
	subg(scratch3,g);
    }
    if(isZero(g)) goto out;
    int_to_giant(1,scratch3);
    gshiftleft(n,scratch3);
    int_to_giant(1,scratch4);
    subg(scratch4,scratch3);
    if(gcompg(g,scratch3) >= 0) subg(scratch3,g);
    if (the_sign < 0) {
	g->sign = -g->sign;
	addg(scratch3,g);
    }
out:
    returnGiant(scratch3);
    returnGiant(scratch4);
}

#else	/* NEW_MERSENNE */

void gmersennemod(int n, giant g) {
/*   g becomes g mod ((2^n)-1)
     31 Jul 96 modified REC.
     17 Jan 97 modified REC.
*/
    unsigned bits = n & (GIANT_BITS_PER_DIGIT - 1);
    unsigned digits =  1 + ((n-1) >> GIANT_LOG2_BITS_PER_DIGIT);
    int isPositive = (g->sign > 0);
    int j;
    int b;
    int size;
    int foundzero;
    giantDigit mask = (bits == 0) ? GIANT_DIGIT_MASK : (giantDigit)((1<<bits)-1);
    giant scratch1;

    b = bitlen(g);
    if(b < n) {
        unsigned numDigits = (n + GIANT_BITS_PER_DIGIT - 1) >>
		GIANT_LOG2_BITS_PER_DIGIT;
	giantDigit lastWord = 0;
	giantDigit bits = 1;

        if(g->sign >= 0) return;

	/*
	 * Cons up ((2**n)-1), add to g.
	 */
	scratch1 = borrowGiant(numDigits + 1);
	scratch1->sign = numDigits;
	for(j=0; j<(int)(numDigits-1); j++) {
		scratch1->n[j] = GIANT_DIGIT_MASK;
	}

	/*
	 * Last word has lower (n & (GIANT_BITS_PER_DIGIT-1)) bits set.
	 */
	for(j=0; j < (int)(n & (GIANT_BITS_PER_DIGIT-1)); j++) {
		lastWord |= bits;
		bits <<= 1;
	}
	scratch1->n[numDigits-1] = lastWord;
	addg(g, scratch1);   /* One version. */
	gtog(scratch1, g);
	returnGiant(scratch1);
	return;
    }
    if(b == n) {
        for(foundzero=0, j=0; j<b; j++) {
            if(bitval(g, j)==0) {
                foundzero = 1;
                break;
            }
        }
        if (!foundzero) {
            int_to_giant(0, g);
            return;
        }
    }

    absg(g);
    scratch1 = borrowGiant(g->capacity);
    while ( ((unsigned)(g->sign) > digits) ||
            ( ((unsigned)(g->sign)==digits) && (g->n[digits-1] > mask))) {
        extractbits(n, g, scratch1);
        gshiftright(n, g);
        addg(scratch1, g);
    }
    size = g->sign;

/* Commence new negation routine - REC 17 Jan 1997. */
    if (!isPositive) { /* Mersenne negation is just bitwise complement. */
        for(j = digits-1; j >= size; j--) {
	    g->n[j] = GIANT_DIGIT_MASK;
	}
        for(j = size-1; j >= 0; j--) {
	    g->n[j] = ~g->n[j];
	}
	g->n[digits-1] &= mask;
    	j = digits-1;
        while((g->n[j] == 0) && (j > 0)) {
	    --j;
	}
        size = j+1;
    }
/* End new negation routine. */

    g->sign = size;
    if (abs(g->sign) > g->capacity) {
    	CKRaise("gmersennemod overflow");
    }
    if (size < (int)digits) {
    	goto bye;
    }
    if (g->n[size-1] != mask) {
    	goto bye;
    }
    mask = GIANT_DIGIT_MASK;
    for(j=0; j<(size-1); j++) {
    	if (g->n[j] != mask) {
	    goto bye;
	}
    }
    g->sign = 0;
  bye:
    returnGiant(scratch1);
}

#endif	/* NEW_MERSENNE */

void mulg(giant a, giant b) { /* b becomes a*b. */

    int i;
    int asize, bsize;
    giantDigit *bptr = b->n;
    giantDigit mult;
    giant scratch1;
    giantDigit carry;
    giantDigit *scrPtr;


    if (isZero(b)) {
	return;
    }
    if (isZero(a)) {
	gtog(a, b);
	return;
    }
    if(a == b) {
	grammarSquare(b);
	return;
    }

    bsize = abs(b->sign);
    asize = abs(a->sign);
    scratch1 = borrowGiant((asize+bsize));
    scrPtr = scratch1->n;

    for(i=0; i<asize+bsize; ++i) {
    	scrPtr[i]=0;
    }

    for(i=0; i<bsize; ++i, scrPtr++) {
	mult = bptr[i];
	if (mult != 0) {
	    carry = VectorMultiply(mult,
	    	a->n,
		asize,
		scrPtr);
	    /* handle MSD carry */
	    scrPtr[asize] += carry;
	}
    }
    bsize+=asize;
     if(scratch1->n[bsize - 1] == 0) {
        --bsize;
    }
    scratch1->sign = gsign(a) * gsign(b) * bsize;
    if (abs(scratch1->sign) > scratch1->capacity) {
    	CKRaise("GiantGrammarMul overflow");
    }
    gtog(scratch1,b);
    returnGiant(scratch1);

    #if		FEE_DEBUG
    (void)bitlen(b); // Assertion....
    #endif	/* FEE_DEBUG */
    PROF_INCR(numMulg);			// for normal profiling
    INCR_MULGS;				// for ellipticMeasure
}

void grammarSquare(giant a) {
    /*
     * For now, we're going to match the old implementation line for
     * line by maintaining prod, carry, and temp as double precision
     * giantDigits. There is probably a much better implementation....
     */
    giantDigit		prodLo;
    giantDigit		prodHi;
    giantDigit		carryLo = 0;
    giantDigit		carryHi = 0;
    giantDigit		tempLo;
    giantDigit		tempHi;
    unsigned int	cur_term;
    unsigned		asize;
    unsigned		max;
    giantDigit		*ptr = a->n;
    giantDigit		*ptr1;
    giantDigit		*ptr2;
    giant 		scratch;

    /* dmitch 11 Jan 1998 - special case for a == 0 */
    if(a->sign == 0) {
    	goto end;
    }
    /* end a == 0 case */
    asize = abs(a->sign);
    max = asize * 2 - 1;
    scratch = borrowGiant(2 * asize);
    asize--;

    /*
     * temp = *ptr;
     * temp *= temp;
     * scratch->n[0] = temp;
     * carry = temp >> 16;
     */
    giantMulDigits(*ptr, *ptr, &tempLo, &tempHi);
    scratch->n[0] = tempLo;
    carryLo = tempHi;
    carryHi = 0;

    for (cur_term = 1; cur_term < max; cur_term++) {
	ptr1 = ptr2 = ptr;
	if (cur_term <= asize) {
	    ptr2 += cur_term;
	} else {
	    ptr1 += cur_term - asize;
	    ptr2 += asize;
	}

	/*
	 * prod = carry & 0xFFFF;
	 * carry >>= 16;
	 */
	prodLo = carryLo;
	prodHi = 0;
	carryLo = carryHi;
	carryHi = 0;
	while(ptr1 < ptr2) {
	    /*
	     * temp = *ptr1++ * *ptr2--;
	     */
	    giantMulDigits(*ptr1++, *ptr2--, &tempLo, &tempHi);

	    /*
	     * prod += (temp << 1) & 0xFFFF;
	     */
	    giantAddDouble(&prodLo, &prodHi, (tempLo << 1));

	    /*
	     * carry += (temp >> 15);
	     * use bits from both product digits..
	     */
	    giantAddDouble(&carryLo, &carryHi,
	    	(tempLo >> (GIANT_BITS_PER_DIGIT - 1)));
	    giantAddDouble(&carryLo, &carryHi, (tempHi << 1));

	    /* snag the msb from that last shift */
	    carryHi += (tempHi >> (GIANT_BITS_PER_DIGIT - 1));
	}
	if (ptr1 == ptr2) {
	    /*
	     * temp = *ptr1;
	     * temp *= temp;
	     */
	    giantMulDigits(*ptr1, *ptr1, &tempLo, &tempHi);

	    /*
	     * prod += temp & 0xFFFF;
	     */
	    giantAddDouble(&prodLo, &prodHi, tempLo);

	    /*
	     * carry += (temp >> 16);
	     */
	    giantAddDouble(&carryLo, &carryHi, tempHi);
	}

	/*
	 * carry += prod >> 16;
	 */
	giantAddDouble(&carryLo, &carryHi, prodHi);

	scratch->n[cur_term] = prodLo;
    }
    if (carryLo) {
	scratch->n[cur_term] = carryLo;
	scratch->sign = cur_term+1;
    } else scratch->sign = cur_term;

    gtog(scratch,a);
    returnGiant(scratch);
end:
    PROF_INCR(numGsquare);
}

/*
 * Clear all of a giant's data fields, for secure erasure of sensitive data.,
 */
void clearGiant(giant g)
{
    unsigned i;

    for(i=0; i<g->capacity; i++) {
    	g->n[i] = 0;
    }
    g->sign = 0;
}

#if	ENGINE_127_BITS
/*
 * only used by engineNSA127.c, which is obsolete as of 16 Jan 1997
 */
int
scompg(int n, giant g) {
    if((g->sign == 1) && (g->n[0] == n)) return(1);
    return(0);
}

#endif	// ENGINE_127_BITS

/*
 */

/*
 * Calculate the reciprocal of a demonimator used in divg_via_recip() and
 * modg_via_recip().
 */
void
make_recip(giant d, giant r)
/* r becomes the steady-state reciprocal
   2^(2b)/d, where b = bit-length of d-1. */
{
	int b;
	int giantSize = 4 * abs(d->sign);
	giant tmp = borrowGiant(giantSize);
	giant tmp2 = borrowGiant(giantSize);

	if (isZero(d) || (d->sign < 0))
	{
		CKRaise("illegal argument to make_recip");
	}
	int_to_giant(1, r); subg(r, d); b = bitlen(d); addg(r, d);
	gshiftleft(b, r); gtog(r, tmp2);
	while(1) {
		gtog(r, tmp);
		gsquare(tmp);
		gshiftright(b, tmp);
		mulg(d, tmp);
		gshiftright(b, tmp);
		addg(r, r); subg(tmp, r);
		if(gcompg(r, tmp2) <= 0) break;
		gtog(r, tmp2);
	}
	int_to_giant(1, tmp);
	gshiftleft(2*b, tmp);
	gtog(r, tmp2); mulg(d, tmp2);
	subg(tmp2, tmp);
	int_to_giant(1, tmp2);
	while(tmp->sign < 0) {
		subg(tmp2, r);
		addg(d, tmp);
	}

	returnGiant(tmp);
	returnGiant(tmp2);
	return;
}

/*
 * Optimized divg, when reciprocal of denominator is known.
 */
void
divg_via_recip(giant d, giant r, giant n)
/* n := n/d, where r is the precalculated
   steady-state reciprocal of d. */
{
	int s = 2*(bitlen(r)-1), sign = gsign(n);
	int giantSize = (4 * abs(d->sign)) + abs(n->sign);
	giant tmp = borrowGiant(giantSize);
	giant tmp2 = borrowGiant(giantSize);

	if (isZero(d) || (d->sign < 0))
	{
		CKRaise("illegal argument to divg_via_recip");
	}
	n->sign = abs(n->sign);
	int_to_giant(0, tmp2);
	while(1) {
		gtog(n, tmp);
		mulg(r, tmp);
		gshiftright(s, tmp);
		addg(tmp, tmp2);
		mulg(d, tmp);
		subg(tmp, n);
		if(gcompg(n,d) >= 0) {
			subg(d,n);
			iaddg(1, tmp2);
		}
   		if(gcompg(n,d) < 0) break;
   	}
	gtog(tmp2, n);
	n->sign *= sign;
	returnGiant(tmp);
	returnGiant(tmp2);
	return;
}

/*
 * Optimized modg, when reciprocal of denominator is known.
 */

/* New version, 24 Dec 1997. */

void
modg_via_recip(
	giant 	d,
	giant 	r,
	giant 	n
)
/* This is the fastest mod of the present collection.
   n := n % d, where r is the precalculated
   steady-state reciprocal of d. */

{
    int		s = (bitlen(r)-1), sign = n->sign;
    int 	giantSize = (4 * abs(d->sign)) + abs(n->sign);
    giant 	tmp, tmp2;

    tmp = borrowGiant(giantSize);
    tmp2 = borrowGiant(giantSize);
    if (isZero(d) || (d->sign < 0))
    {
	CKRaise("illegal argument to modg_via_recip");
    }
    n->sign = abs(n->sign);
    while (1)
    {
	gtog(n, tmp);
	/* bug fix 13 Apr 1998 */
	if(s == 0) {
	    gshiftleft(1, tmp);
	}
	else {
	    gshiftright(s-1, tmp);
	}
	/* end fix */
	mulg(r, tmp);
	gshiftright(s+1, tmp);
	mulg(d, tmp);
	subg(tmp, n);
	if (gcompg(n,d) >= 0)
		subg(d,n);
	if (gcompg(n,d) < 0)
		break;
    }
    if (sign >= 0)
	goto done;
    if (isZero(n))
	goto done;
    negg(n);
    addg(d,n);
done:
    returnGiant(tmp);
    returnGiant(tmp2);
    return;
}

/*
 * Unoptimized, inefficient general modg, when reciprocal of denominator
 * is not known.
 */
void
modg(
	giant 	d,
	giant 	n
)
{
	/* n becomes n%d. n is arbitrary, but the denominator d must be
	 * positive! */

	/*
	 * 4/9/2001: seeing overflow on this recip. Alloc per 
	 * d->capacity, not d->sign.
	 */
	//giant recip = borrowGiant(2 * abs(d->sign));
	giant recip = borrowGiant(2 * d->capacity);

	#if	WARN_UNOPTIMIZE
	dbgLog(("Warning: unoptimized modg!\n"));
	#endif	// WARN_UNOPTIMIZE

	make_recip(d, recip);
	modg_via_recip(d, recip, n);
	returnGiant(recip);
}

/*
 * Unoptimized, inefficient general divg, when reciprocal of denominator
 * is not known.
 */
void
divg(
	giant 	d,
	giant 	n
)
{
	/* n becomes n/d. n is arbitrary, but the denominator d must be
	 * positive!
	 */

	giant recip = borrowGiant(2 * abs(d->sign));

	#if	WARN_UNOPTIMIZE
	dbgLog(("Warning: unoptimized divg!\n"));
	#endif	// WARN_UNOPTIMIZE

	make_recip(d, recip);
	divg_via_recip(d, recip, n);
	returnGiant(recip);
}
