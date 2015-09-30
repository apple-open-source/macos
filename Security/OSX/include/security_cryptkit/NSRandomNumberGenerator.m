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
 * NSRandomNumberGenerator.m
 *
 * Revision History
 * ----------------
 * 28 Mar 97 at Apple
 *	Rewrote using feeRandom module.
 * ??     96	Blaine Garst at NeXT
 *	Created.
 */

/*
 * Note: out _priv ivar is actually a feeRand pointer.
 */

#import <Foundation/Foundation.h>
#import "NSRandomNumberGenerator.h"
#import "feeRandom.h"
#import "falloc.h"

@implementation NSRandomNumberGenerator

- init
{
	if(_priv == NULL) {
		_priv = feeRandAlloc();
	}
	/*
	 * else no need to re-init
	 */
	return self;
}

- initWithSeed:(unsigned)seed
{
	if(_priv != NULL) {
		/*
		 * Free & re-init to use new seed
		 */
		feeRandFree(_priv);
	}
	_priv = feeRandAllocWithSeed(seed);
	return self;
}

- (unsigned)nextNumber
{
	if(_priv == NULL) {
		return 0;
	}
	return feeRandNextNum(_priv);
}

- (unsigned)nextNumberInRange:(NSRange)range
{
	if(_priv == NULL) {
		return 0;
	}
    	return range.location + ([self nextNumber] % range.length);
}

- (NSData *)randomDataWithLength:(unsigned)l
{
	unsigned char *cp;

	if(_priv == NULL) {
		return nil;
	}
	cp = fmalloc(l);
	feeRandBytes(_priv, cp, l);
	return [NSData dataWithBytesNoCopy:cp length:l];
}

@end
