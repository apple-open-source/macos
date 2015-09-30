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
 * NSFEEPublicKey.h
 *
 * Revision History
 * ----------------
 * 27 Feb 1997 at Apple
 *	Broke out from NSCryptors.h.
 */

#import <CryptKit/NSCryptors.h>

@interface NSFEEPublicKey : NSObject
	<NSPublicKey,NSDigitalSignature,NSCryptor> {
@private
	void	*_pubKey;
}

+ keyWithPrivateData:(NSData *)private
	depth:(unsigned)depth 			// depth is in range 0-23
	usageName:(NSString *)uname;
    // able to encrypt/decrypt data
    // able to create/verify digital signatures

+ keyWithPublicKeyString:(NSString *)hexstr;
    // able to encrypt data
    // able to verify digital signatures

/*
 * Create new key with curve parameters matching existing oldKey.
 */
+ keyWithPrivateData:(NSData *)passwd
	andKey:(NSFEEPublicKey *)oldKey
	usageName:(NSString *)uname;

/*
 * Convenience methods. The first three use the default depth
 * (FEE_DEPTH_DEFAULT).
 */
+ keyWithPrivateData:(NSData *)passwd
	usageName:(NSString *)uname;
+ keyWithPrivateString:(NSString *)private
	usageName:(NSString *)uname;
+ keyWithPrivateString:(NSString *)private
	andKey:(NSFEEPublicKey *)oldKey
	usageName:(NSString *)uname;

+ keyWithPrivateString:(NSString *)private
	depth:(unsigned)depth
	usageName:(NSString *)uname;

/*
 * NSCryptor protocol
 */
- (NSData *)encryptData:(NSData *)data;  // done with public knowledge
- (NSData *)decryptData:(NSData *)data;  // done with private knowledge

/*
 * NSDigitalSignature protocol
 */
- (NSData *)digitalSignatureForData:(NSData *)data;
    // data is hashed with MD5 and then signed with private knowledge
- (BOOL)isValidDigitalSignature:(NSData *)sig forData:(NSData *)data;
    // data is hashed with MD5 and then verified with public knowledge

@end
