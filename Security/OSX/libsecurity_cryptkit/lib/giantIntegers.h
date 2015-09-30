/* Copyright (c) 1998,2011-2012,2014 Apple Inc.  All Rights Reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * giantIntegers.h - large-integer arithmetic library.
 *
 * Revision History
 * ----------------
 * 05 Oct 98 at Apple
 *	Default "unsigned int" giantDigit for __i386__ and __i486__
 * 08 May 97 at Apple
 *	Changed size of giantstruct.n to 1 for Mac build
 * 	Created.
 */

#ifndef	_CK_NSGIANTINTS_H_
#define _CK_NSGIANTINTS_H_

#include <security_cryptkit/ckconfig.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Size of giant digit.
 */
#if	NeXT || __i386__ || __i486__

typedef unsigned int giantDigit;

/*
 * used to divide by GIANT_BITS_PER_DIGIT via shift - no easy way to get
 * the compiler to calculate this.
 */
#define GIANT_LOG2_BITS_PER_DIGIT 5

#elif defined(macintosh) || defined(__ppc__)

typedef unsigned int giantDigit;
#define GIANT_LOG2_BITS_PER_DIGIT	5

#else

typedef unsigned short giantDigit;
#define GIANT_LOG2_BITS_PER_DIGIT	4

#endif

/* platform-independent digit manipulation macros */

#define GIANT_BYTES_PER_DIGIT	(sizeof(giantDigit))
#define GIANT_BITS_PER_DIGIT	(8 * GIANT_BYTES_PER_DIGIT)
#define GIANT_DIGIT_MASK	((giantDigit)~0)
#define BYTES_TO_GIANT_DIGITS(x)	\
	((x + GIANT_BYTES_PER_DIGIT - 1) / GIANT_BYTES_PER_DIGIT)

#define MAX_DIGITS ((1<<18)+(1<<17))
			/* 2^(16*MAX_DIGITS)-1 will fit into a giant. */

/*
 * The giant stack package is a local cache which allows us to avoid calls
 * to malloc() for borrowGiant(). On a 90 Mhz Pentium, enabling the
 * giant stack package shows about a 1.35 speedup factor over an identical
 * CryptKit without the giant stacks enabled.
 */
#define GIANTS_VIA_STACK	CRYPTKIT_GIANT_STACK_ENABLE

typedef struct {
	 int sign;              /* number of giantDigits = abs(sign) */
     unsigned capacity;		/* largest possible number of giantDigits */
	 giantDigit n[1];		/* n[0] is l.s. digit */
} giantstruct;
typedef giantstruct *giant;

#if		GIANTS_VIA_STACK
/*
 * For giant stack debug only
 * Set default giant size (i.e., for newGiant(0) and borrowGiant(0))
 */
void setGiantSize(unsigned numDigits);

/*
 * Initialize giant stacks, with up to specified max giant size.
 */
void initGiantStacks(unsigned maxDigits);

/* 
 * Free giant stacks on shutdown.
 */
void freeGiantStacks(void);

#endif	/* GIANTS_VIA_STACK */

giant newGiant(unsigned numDigits);
giant copyGiant(giant x);
void freeGiant(giant x);

giant borrowGiant(unsigned numDigits);	/* get a temporary */
void returnGiant(giant);		/* return it */
unsigned bitlen(giant n); 		/* Returns the bit-length n;
 					 * e.g. n=7 returns 3. */
int bitval(giant n, int pos); 		/* Returns the value of bit pos of n */
int isZero(giant g);  			/* Returns whether g is zero */
int isone(giant g);			/* Returns whether g is 1 */
void gtog(giant src, giant dest);  	/* Copies one giant to another */
void int_to_giant(int n, giant g);  	/* Gives a giant an int value */
int gcompg(giant a, giant b); 		/* Returns 1, 0, -1 as a>b, a=b, a<b */
void addg(giant a, giant b);  		/* b += a */
void iaddg(int a, giant b);		/* b += a */
void subg(giant a, giant b);  		/* b -= a. */
void imulg(unsigned n, giant g);  	/* g *= n */
void negg(giant g);  			/* g := -g. */
int binvg(giant n, giant x);   		/* Same as invg(), but uses binary
					 * division. */
int binvaux(giant p, giant x);
void gmersennemod(int n, giant g);  	/* g := g (mod 2^n-1). */
void gshiftleft(int bits, giant g);  	/* Shift g left by bits, introducing
					 * zeros on the right. */
void gshiftright(int bits, giant g); 	/* Shift g right by bits, losing bits
					 * on the right. */
void extractbits(unsigned n, giant src, giant dest);
					/* dest becomes lowermost n bits of
					 * src.  Equivalent to
					 * dest = src % 2^n */

void grammarSquare(giant a);		/* g *= g. */
#define gsquare(g) grammarSquare(g)

void mulg(giant a, giant b);  		/* b *= a. */
int gsign(giant g);  			/* Returns the sign of g: -1, 0, 1. */
void gtrimSign(giant g);		/* Adjust sign for possible leading
					 * (m.s.) zero digits */

void divg(giant d, giant n);		/* n becomes |n|/d. n is arbitrary,
					 * but the denominator d must be
					 * positive! */
int scompg(int n, giant g);
void modg(giant den, giant num);  	/* num := num mod den, any positive
					 * den. */
void clearGiant(giant g);		/* zero a giant's data */

/*
 * Optimized modg and divg, with routine to calculate necessary reciprocal
 */
void make_recip(giant d, giant r);
void divg_via_recip(giant denom, giant recip, giant numer);
					/* numer := |n|/d. */
void modg_via_recip(giant denom, giant recip, giant numer);
					/* num := num mod den */

#ifdef __cplusplus
}
#endif

#endif	/* _CK_NSGIANTINTS_H_ */
