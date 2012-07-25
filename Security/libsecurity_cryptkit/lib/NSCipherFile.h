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
 * NSCipherFile.h - ObjC wrapper for feeCipherFile
 *
 * Revision History
 * ----------------
 * 28 Oct 96	Doug Mitchell at NeXT
 *	Created.
 */

#import <CryptKit/CryptKit.h>
#import <CryptKit/CipherFileTypes.h>

@interface NSCipherFile : NSObject
{
	void	*_priv;
}

/*
 * Alloc and return an autoreleased NSCipherFile object associated with
 * the specified data.
 */
+ newFromCipherText : (NSData *)cipherText
	encrType : (cipherFileEncrType)encrType
	sendPubKeyData : (NSData *)sendPubKeyData
	otherKeyData : (NSData *)otherKeyData
	sigData : (NSData *)sigData	// optional; nil means no signature
	userData : (unsigned)userData;	// for caller's convenience

/*
 * Obtain the contents of a feeCipherFile as NSData.
 */
- (NSData *)dataRepresentation;

/*
 * Alloc and return an autoreleased NSCipherFile object given a data
 * representation.
 */
+ newFromDataRepresentation : (NSData *)dataRep;

/*
 * Given an NSCipherFile object, obtain its constituent parts.
 */
- (cipherFileEncrType)encryptionType;
- (NSData *)cipherText;
- (NSData *)sendPubKeyData;
- (NSData *)otherKeyData;
- (NSData *)sigData;
- (unsigned)userData;

/*
 * High-level cipherFile support.
 */

/*
 * Obtain the data representation of a NSCipherFile given the specified
 * plainText and cipherFileEncrType.
 * Receiver's public key is required for all encrTypes; sender's private
 * key is required for signature generation and also for encrType
 * CFE_PublicDES and CFE_FEED.
 */
+(feeReturn)createCipherFileForPrivKey : (NSFEEPublicKey *)sendPrivKey
	recvPubKey : (NSFEEPublicKey *)recvPubKey
	encrType : (cipherFileEncrType)encrType
	plainText : (NSData *)plainText
	genSig : (BOOL)genSig
	doEnc64 : (BOOL)doEnc64			// YES ==> perform enc64
	userData : (unsigned)userData		// for caller's convenience
	cipherFileData : (NSData **)cipherFileData;	// RETURNED

/*
 * Parse and decrypt a data representation of an NSCipherFile object.
 *
 * recvPrivKey is required in all cases. If sendPubKey is present,
 * sendPubKey - rather than the embedded sender's public key - will be
 * used for signature validation.
 */
+ (feeReturn)parseCipherFileData : (NSFEEPublicKey *)recvPrivKey
	sendPubKey : (NSFEEPublicKey *)sendPubKey
	cipherFileData : (NSData *)cipherFileData
	doDec64 : (BOOL)doDec64
	encrType : (cipherFileEncrType *)encrType	// RETURNED
	plainText : (NSData **)plainText		// RETURNED
	sigStatus : (feeSigStatus *)sigStatus		// RETURNED
	sigSigner : (NSString **)sigSigner		// RETURNED
	userData : (unsigned *)userData;		// RETURNED

/*
 * Parse and decrypt an NSCipherFile object obtained via
 * +newFromDataRepresentation.
 *
 * recvPrivKey is required in all cases. If sendPubKey is present,
 * sendPubKey - rather than the embedded sender's public key - will be
 * used for signature validation.
 */
- (feeReturn)decryptCipherFileData : (NSFEEPublicKey *)recvPrivKey
	sendPubKey : (NSFEEPublicKey *)sendPubKey
	plainText : (NSData **)plainText		// RETURNED
	sigStatus : (feeSigStatus *)sigStatus		// RETURNED
	sigSigner : (NSString **)sigSigner;		// RETURNED


@end
