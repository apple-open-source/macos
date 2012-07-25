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
 * NSRandomNumberGenerator.h
 *
 * Revision History
 * ----------------
 * 28 Mar 97	Doug Mitchell at Apple
 *	Simplified.
 * ??     96	Blaine Garst at NeXT
 *	Created.
 */

#import <Foundation/NSData.h>

@interface NSRandomNumberGenerator : NSObject
{
	void *_priv;
}

- initWithSeed:(unsigned)seed;		// designated initializer
- init;					// we'll come up with the best seed
					//    we can

- (unsigned)nextNumber;
- (unsigned)nextNumberInRange:(NSRange)range;
- (NSData *)randomDataWithLength:(unsigned)l;

@end
