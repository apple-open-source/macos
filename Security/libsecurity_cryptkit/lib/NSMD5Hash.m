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
 * NSMD5Hash.h
 *
 * Revision History
 * ----------------
 * 28 Mar 97	Doug Mitchell at Apple
 *	Created.
 */

/*
 * Note: our _priv ivar is actually a feeHash pointer.
 */
#import "NSCryptors.h"
#import "NSMD5Hash.h"
#import "feeHash.h"
#import "falloc.h"

@implementation NSMD5Hash

+ digester
{
	return [[self alloc] init];
}

- init
{
	if(_priv == NULL) {
		_priv = feeHashAlloc();
	}
	else {
		feeHashReinit(_priv);
	}
	return self;
}

- (void)digestData:(NSData *)data
{
	if(_priv == NULL) {
		return;
	}
	feeHashAddData(_priv, [data bytes], [data length]);
}

- (NSData *)messageDigest
{
	unsigned char *cp;
	NSData *md;

	if(_priv == NULL) {
		return nil;
	}
	cp = feeHashDigest(_priv);
	md = [NSData dataWithBytes:cp length:feeHashDigestLen()];
	feeHashReinit(_priv);
	return md;
}

- (NSData *)digestData:(NSData *)data withSalt:(NSData *)salt
{
	if(_priv == NULL) {
		return nil;
	}
	if(salt != nil) {
		[self digestData:salt];
	}
	[self digestData:data];
	return [self messageDigest];
}

@end
