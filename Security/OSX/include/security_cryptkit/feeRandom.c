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
 * FeeRandom.c - generic, portable random number generator object
 *
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 * 19 Jun 97 at Apple
 *	Eliminated predictability of bytes 4 thru 15 of random data
 * 18 Jun 97 at Apple
 *	Reduced size of per-instance giants from 128 to 32 shorts
 * 23 Aug 96 at NeXT
 *	Created, based on Blaine Garst's NSRandomNumberGenerator class
 */

#include "feeRandom.h"
#include "platform.h"
#include <Security/SecRandom.h>

feeRand feeRandAllocWithSeed(__attribute__((unused)) unsigned seed)
{
    return NULL;
}

feeRand feeRandAlloc(void)
{
	return NULL;
}

void feeRandFree(__attribute__((unused)) feeRand frand)
{

}

unsigned feeRandNextNum(feeRand frand)
{
    unsigned rand;

    feeRandBytes(frand, &rand, sizeof(rand));

	return rand;
}

void feeRandBytes(__attribute__((unused)) feeRand frand, void *bytes, unsigned numBytes)
{
    int err;

    err = SecRandomCopyBytes(kSecRandomDefault, numBytes, bytes);
    if (err != errSecSuccess) {
        CKRaise("feeRandBytes");
    }
}

/* new function, 5 March 1999 - dmitch */
void feeRandAddEntropy(__attribute__((unused)) feeRand frand, __attribute__((unused)) unsigned entropy)
{

}

