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
 * NSCipherFile.m - ObjC wrapper for feeCipherFile
 *
 * Revision History
 * ----------------
 * 28 Oct 96 at NeXT
 *	Created.
 */

#import "NSCipherFile.h"
#import "feeCipherFile.h"
#import "falloc.h"
#import "NSFEEPublicKeyPrivate.h"	/* for -feePubKey */

/*
 * Private instance data.
 */
typedef struct {
	feeCipherFile	cfile;
} _cfPriv;

@implementation NSCipherFile

- (void)dealloc
{
	if(_priv) {
		_cfPriv *cfPriv = _priv;
		if(cfPriv->cfile) {
		    feeCFileFree(cfPriv->cfile);
		}
	}
	[super dealloc];
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
	userData : (unsigned)userData	// for caller's convenience
{
	NSCipherFile *result;
	_cfPriv *cfPriv;

	result = [[self alloc] autorelease];
	result->_priv = cfPriv = fmalloc(sizeof(_cfPriv));
	cfPriv->cfile = feeCFileNewFromCipherText(encrType,
		[cipherText bytes],
		[cipherText length],
		[sendPubKeyData bytes],
		[sendPubKeyData length],
		[otherKeyData bytes],
		[otherKeyData length],
		[sigData bytes],
		[sigData length],
		userData);
	if(cfPriv->cfile) {
		return result;
	}
	else {
		return nil;
	}
}

/*
 * Obtain the contents of a feeCipherFile as NSData.
 */
- (NSData *)dataRepresentation
{
	_cfPriv 		*cfPriv = _priv;
	NSData 			*result;
	const unsigned char 	*rep;
	unsigned 		repLen;
	feeReturn 		frtn;

	if(cfPriv == NULL) {
		return nil;
	}
	frtn = feeCFileDataRepresentation(cfPriv->cfile,
		&rep,
		&repLen);
	if(frtn) {
		return nil;
	}
	result = [NSData dataWithBytesNoCopy:(unsigned char *)rep
		length:repLen];
	return result;
}

/*
 * Alloc and return an autoreleased NSCipherFile object given a data
 * representation.
 */
+ newFromDataRepresentation : (NSData *)dataRep
{
	NSCipherFile *result;
	_cfPriv *cfPriv;
	feeReturn frtn;

	result = [[self alloc] autorelease];
	result->_priv = cfPriv = fmalloc(sizeof(_cfPriv));
	frtn = feeCFileNewFromDataRep([dataRep bytes],
		[dataRep length],
		&cfPriv->cfile);
	if(frtn) {
		return nil;
	}
	else {
		return result;
	}
}

/*
 * Given an NSCipherFile object, obtain its constituent parts.
 */
- (cipherFileEncrType)encryptionType
{
	_cfPriv 		*cfPriv = _priv;

	if(cfPriv == NULL) {
		return CFE_Other;
	}
	return feeCFileEncrType(cfPriv->cfile);
}

- (NSData *)cipherText
{
	_cfPriv 		*cfPriv = _priv;
	const unsigned char 	*ctext;
	unsigned 		ctextLen;

	if(cfPriv == NULL) {
		return nil;
	}
	ctext = feeCFileCipherText(cfPriv->cfile, &ctextLen);
	return [NSData dataWithBytesNoCopy:(unsigned char *)ctext
		length:ctextLen];
}

- (NSData *)sendPubKeyData
{
	_cfPriv 		*cfPriv = _priv;
	const unsigned char 	*key;
	unsigned 		keyLen;

	if(cfPriv == NULL) {
		return nil;
	}
	key = feeCFileSendPubKeyData(cfPriv->cfile, &keyLen);
	if(key) {
	    return [NSData dataWithBytesNoCopy:(unsigned char *)key
		length:keyLen];
	}
	else {
	    return nil;
	}
}

- (NSData *)otherKeyData
{
	_cfPriv 		*cfPriv = _priv;
	const unsigned char 	*key;
	unsigned 		keyLen;

	if(cfPriv == NULL) {
		return nil;
	}
	key = feeCFileOtherKeyData(cfPriv->cfile, &keyLen);
	if(key) {
	    return [NSData dataWithBytesNoCopy:(unsigned char *)key
		length:keyLen];
	}
	else {
	    return nil;
	}
}

- (NSData *)sigData
{
	_cfPriv 		*cfPriv = _priv;
	const unsigned char 	*sig;
	unsigned 		sigLen;

	if(cfPriv == NULL) {
		return nil;
	}
	sig = feeCFileSigData(cfPriv->cfile, &sigLen);
	if(sig) {
	    return [NSData dataWithBytesNoCopy:(unsigned char *)sig
	    	length:sigLen];
	}
	else {
	    return nil;
	}
}

- (unsigned)userData
{
	_cfPriv 		*cfPriv = _priv;

	if(cfPriv == NULL) {
		return 0;
	}
	return feeCFileUserData(cfPriv->cfile);
}

/*
 * High-level cipherFile support.
 */

/*
 * Create a cipherfile of specified cipherFileEncrType for given plaintext.
 */
+(feeReturn)createCipherFileForPrivKey : (NSFEEPublicKey *)sendPrivKey
	recvPubKey : (NSFEEPublicKey *)recvPubKey
	encrType : (cipherFileEncrType)encrType
	plainText : (NSData *)plainText
	genSig : (BOOL)genSig
	doEnc64 : (BOOL)doEnc64			// YES ==> perform enc64
	userData : (unsigned)userData		// for caller's convenience
	cipherFileData : (NSData **)cipherFileData	// RETURNED
{
	feeReturn frtn;
	unsigned char *cfileData;
	unsigned cfileDataLen;
	feePubKey privKey = NULL;

	if(sendPrivKey) {
		privKey = [sendPrivKey feePubKey];
	}
	frtn = createCipherFile(privKey,
		[recvPubKey feePubKey],
		encrType,
		[plainText bytes],
		[plainText length],
		genSig,
		doEnc64,
		userData,
		&cfileData,
		&cfileDataLen);
	if(frtn) {
		return frtn;
	}
	*cipherFileData =
		[NSData dataWithBytesNoCopy:(unsigned char *)cfileData
			length:cfileDataLen];
	return frtn;
}

/*
 * Parse and decrypt a data representation of an NSCipherFile object.
 */
+ (feeReturn)parseCipherFileData : (NSFEEPublicKey *)recvPrivKey
	sendPubKey : (NSFEEPublicKey *)sendPubKey
	cipherFileData : (NSData *)cipherFileData
	doDec64 : (BOOL)doDec64
	encrType : (cipherFileEncrType *)encrType	// RETURNED
	plainText : (NSData **)plainText		// RETURNED
	sigStatus : (feeSigStatus *)sigStatus		// RETURNED
	sigSigner : (NSString **)sigSigner		// RETURNED
	userData : (unsigned *)userData			// RETURNED
{
	feeReturn 	frtn;
	unsigned char 	*ptext;
	unsigned 	ptextLen;
	feeUnichar 	*signer;
	unsigned 	signerLen;
	feePubKey 	_pubKey = NULL;

	if(recvPrivKey == nil) {
		return FR_IllegalArg;			// always required
	}
	if(sendPubKey) {
		_pubKey = [sendPubKey feePubKey];
	}

	frtn = parseCipherFile([recvPrivKey feePubKey],
		_pubKey,
		[cipherFileData bytes],
		[cipherFileData length],
		doDec64,
		encrType,
		&ptext,
		&ptextLen,
		sigStatus,
		&signer,
		&signerLen,
		userData);
	if(frtn) {
		return frtn;
	}
	*plainText = [NSData dataWithBytesNoCopy:ptext length:ptextLen];
	*sigSigner = [NSString stringWithCharacters:signer length:signerLen];
	ffree(signer);
	return frtn;
}

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
	sigSigner : (NSString **)sigSigner		// RETURNED
{
	_cfPriv 	*cfPriv = _priv;
	feeReturn 	frtn;
	unsigned char 	*ptext;
	unsigned 	ptextLen;
	feeUnichar 	*signer;
	unsigned 	signerLen;
	feePubKey 	_pubKey = NULL;

	if(cfPriv == NULL) {
		return FR_IllegalArg;
	}
	if(recvPrivKey == nil) {
		return FR_IllegalArg;			// always required
	}
	if(sendPubKey) {
		_pubKey = [sendPubKey feePubKey];
	}

	frtn = decryptCipherFile(cfPriv->cfile,
		[recvPrivKey feePubKey],
		_pubKey,
		&ptext,
		&ptextLen,
		sigStatus,
		&signer,
		&signerLen);
	if(frtn) {
		return frtn;
	}
	*plainText = [NSData dataWithBytesNoCopy:ptext length:ptextLen];
	*sigSigner = [NSString stringWithCharacters:signer length:signerLen];
	ffree(signer);
	return frtn;

}
@end
