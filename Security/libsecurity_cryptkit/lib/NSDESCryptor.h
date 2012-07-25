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
 * NSDESCryptor.h created by blaine on Thu 22-Feb-1996
 */

#import "NSCryptors.h"

/******  Digital Encryption Standard/Algorithm ********/

@interface NSDESCryptor : NSObject <NSCryptor>
{
	void	*_priv;
}

+ cryptorWithState:(NSData *)s;

- initWithState:(NSData *)state;
     // designated initializer
     // 8 bytes with most sig bit ignored: 56 bits

- (void)setCryptorState:(NSData *)state;	// reset
- (void)setBlockMode:(BOOL)yorn;		// default is chaining mode

/*
 * NSCryptor methods
 */
- (NSData *)encryptData:(NSData *)input;
- (NSData *)decryptData:(NSData *)input;
- (unsigned)keyBitsize;

@end
