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
 * NSCryptors.h - common cryptographic protocols
 *
 * Revision History
 * ----------------
 *  ??? 1994	Blaine Garst at NeXT
 *	Created.
 */


#import <Foundation/NSObject.h>
#import <Foundation/NSData.h>
#import <Foundation/NSString.h>


/************ Utilities ******************************************/

#ifdef	NeXT

NSString *NSPromptForPassPhrase(NSString *prompt);
	// useful for command line (/dev/tty) programs

#endif 	NeXT

/************ Data Hashing Protocol *****************/

@protocol NSDataDigester
+ digester;				// provides a concrete digester

// primitives
- (void)digestData:(NSData *)data;	// use for multi-bite messages
- (NSData *)messageDigest;		// provide digest; re-init

// conveniences that only use the above primitives
// all in one gulp (eats salt first, if present)
- (NSData *)digestData:(NSData *)data withSalt:(NSData *)salt;

@end


/******  Encryption/Decryption Protocol ***********/

@protocol NSCryptor
- (NSData *)encryptData:(NSData *)input;
- (NSData *)decryptData:(NSData *)input;
- (unsigned)keyBitsize;
@end


/*************** Public Key Services *************/

@protocol NSPublicKey
- (NSString *)publicKeyString;
- (NSString *)algorithmName;	// "Diffie-Hellman" "FEE" ...
- (NSString *)usageName;	// "Blaine Garst - home"
- (NSData *)padWithPublicKey:(id <NSPublicKey>)otherKey;
- (unsigned)keyBitsize;
@end

/********* Key Ring ************************/

@protocol NSKeyRing
- keyForUsageName:(NSString *)user;
@end

/********** Digital Signatures **************/

// protocol adapted by various signature schemes (FEE, DSA, RSA...)
@protocol NSDigitalSignature
- (NSData *)digitalSignatureForData:(NSData *)message;
  // generate a signature for the data

- (BOOL)isValidDigitalSignature:(NSData *)sig forData:(NSData *)data;
@end
