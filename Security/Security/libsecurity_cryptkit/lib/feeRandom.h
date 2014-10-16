/* Copyright (c) 1998,2011,2014 Apple Inc.  All Rights Reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * FeeRandom.h - generic, portable random number generator object
 *
 * Revision History
 * ----------------
 * 8/25/98		ap
 *	Fixed previous check-in comment.
 * 8/24/98		ap
 *	Added tags around #endif comment.
 * 23 Aug 96 at NeXT
 *	Created.
 */

#ifndef	_CK_FEERANDOM_H_
#define _CK_FEERANDOM_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef void *feeRand;

feeRand feeRandAllocWithSeed(unsigned seed);
feeRand feeRandAlloc(void);

void feeRandFree(feeRand frand);

unsigned feeRandNextNum(feeRand frand);

void feeRandBytes(feeRand frand,
	unsigned char *bytes,		/* must be alloc'd by caller */
	unsigned numBytes);

void feeRandAddEntropy(feeRand frand, unsigned entropy);

#ifdef __cplusplus
}
#endif

#endif	/* _CK_FEERANDOM_H_ */
