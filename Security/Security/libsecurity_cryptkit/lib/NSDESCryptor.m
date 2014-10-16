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
 * NSDESCryptor.m - DES encrypt/decrypt class
 *
 * Revision History
 * ----------------
 * 28 Mar 97 at Apple
 *	Rewrote using feeDES module.
 * 22 Feb 96 at NeXT
 *	Created.
 */

#import <Foundation/Foundation.h>
#import "NSDESCryptor.h"
#import "feeDES.h"
#import "falloc.h"
#import "ckutilities.h"
#import "feeFunctions.h"

/*
 * Note: Our _priv ivar is actuall a feeDES pointer.
 */
@implementation NSDESCryptor

+ cryptorWithState:(NSData *)s {
    return [[[self alloc] initWithState:s] autorelease];
}

- (void)setCryptorState:(NSData *)state {
    if(_priv == NULL) {
    	return;
    }
    feeDESSetState(_priv, [state bytes], [state length]);
}

- initWithState:(NSData *)state {
    feeReturn 	frtn;

    if(_priv == NULL) {
    	_priv = feeDESNewWithState([state bytes], [state length]);
    }
    else {
	frtn = feeDESSetState(_priv, [state bytes], [state length]);
	if(frtn) {
	    NSLog(@"NSDESCryptor: bad initial state\n");
	    return nil;
	}
    }
    return self;
}

- (void)dealloc
{
	if(_priv) {
		feeDESFree(_priv);
	}
	[super dealloc];
}

- (void)setBlockMode:(BOOL)yorn {
    if(_priv == NULL) {
    	return;
    }
    if(yorn) {
    	feeDESSetBlockMode(_priv);
    }
    else {
    	feeDESSetChainMode(_priv);
    }
}

- (NSData *)encryptData:(NSData *)input {
    NSData		*result;
    feeReturn		frtn;
    unsigned char	*cipherText;
    unsigned		cipherTextLen;

    if(_priv == NULL) {
    	return nil;
    }
    frtn = feeDESEncrypt(_priv,
    	[input bytes],
	[input length],
	&cipherText,
	&cipherTextLen);
    if(frtn) {
	NSLog(@"NSDESCryptor encrypt: %s", feeReturnString(frtn));
	return nil;
    }
    result = [NSData dataWithBytes:cipherText length:cipherTextLen];
    ffree(cipherText);
    return result;
}

- (NSData *)decryptData:(NSData *)input {
    NSData		*result;
    feeReturn		frtn;
    unsigned char	*plainText;
    unsigned		plainTextLen;

    if(_priv == NULL) {
    	return nil;
    }
    frtn = feeDESDecrypt(_priv,
    	[input bytes],
	[input length],
	&plainText,
	&plainTextLen);
    if(frtn) {
	NSLog(@"NSDESCryptor decrypt: %s", feeReturnString(frtn));
	return nil;
    }
    result = [NSData dataWithBytes:plainText length:plainTextLen];
    ffree(plainText);
    return result;
}

- (unsigned)keyBitsize {
	return feeDESKeySize(_priv);
}

@end
