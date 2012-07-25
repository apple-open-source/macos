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
 * NSFEEPublicKey.m - NSFEEPublicKey class implementation
 *
 * Revision History
 * ----------------
 * 17 Jul 97	Doug Mitchell at Apple
 *	Added ECDSA signature routines.
 * 21 Aug 96	Doug Mitchell at NeXT
 *	Modified to use C-only FeePublicKey module.
 *  ???? 1994	Blaine Garst at NeXT
 *	Created.
 */

#import <Foundation/Foundation.h>
#import <Foundation/NSUtilities.h>

#import "NSCryptors.h"
#import "NSFEEPublicKeyPrivate.h"
#import "feePublicKey.h"
#import "feePublicKeyPrivate.h"
#import "ckutilities.h"
#import "mutils.h"
#import "feeTypes.h"
#import "curveParams.h"
#import "falloc.h"
#import "feeDigitalSignature.h"
#import "feeHash.h"
#import "feeFunctions.h"
#import "feeFEEDExp.h"

/*
   Elliptic curve algebra over finite fields F(p**k), where p = 2**q -1 is a
   	Mersenne prime.
   q is bit-depth.
   A private key (a) is a large integer that when multiplied by an initial
   	curve point P yields the public key aP.
   Public keys can be used to generate one-time pads because multiplication
   	is commutative:

	a(bP) == b(aP)
 */

@implementation NSFEEPublicKey

/*
 * Root method to create new public key from private "password" data.
 */
+ keyWithPrivateData:(NSData *)passwd
	depth:(unsigned)depth
	usageName:(NSString *)uname
{
	NSFEEPublicKey *result;
	feeReturn frtn;
	unichar *uc;

	result = [[self alloc] autorelease];
	result->_pubKey = feePubKeyAlloc();
	uc = fmalloc([uname length] * sizeof(unichar));
	[uname getCharacters:uc];
	frtn = feePubKeyInitFromPrivData(result->_pubKey,
		[passwd bytes],	[passwd length],
		uc, [uname length],
		depth);
	ffree(uc);
	if(frtn) {
		NSLog(@"keyWithPrivateData: %s\n", feeReturnString(frtn));
		return nil;
	}
	return result;
}

/*
 * Create new key with curve parameters matching existing oldKey.
 */
+ keyWithPrivateData:(NSData *)passwd
	andKey:(NSFEEPublicKey *)oldKey
	usageName:(NSString *)uname
{
	NSFEEPublicKey *result;
	feeReturn frtn;
	unichar *uc;

	result = [[self alloc] autorelease];
	result->_pubKey = feePubKeyAlloc();
	uc = fmalloc([uname length] * sizeof(unichar));
	[uname getCharacters:uc];
	frtn = feePubKeyInitFromKey(result->_pubKey,
		[passwd bytes],	[passwd length],
		uc, [uname length],
		oldKey->_pubKey);
	ffree(uc);
	if(frtn) {
		NSLog(@"keyWithPrivateData:andKey: %s\n",
			feeReturnString(frtn));
		return nil;
	}
	return result;
}

+ keyWithPrivateData:(NSData *)passwd
	usageName:(NSString *)uname
{
	// 4 gives 127 bits of protection
	// although the RSA challenge number of 127 bits has been
	// broken, FEE is much stronger at the same length
	return [self keyWithPrivateData:passwd
		depth:FEE_DEPTH_DEFAULT
		usageName:uname];
}

/*
 * The standard way of creating a new key given a private "password" string.
 */
+ keyWithPrivateString:(NSString *)private
	usageName:(NSString *)uname
{
	NSData *pdata;
	id result;

	/*
	 * FIXME - handle other encodings?
	 */
	pdata = [private dataUsingEncoding:NSUTF8StringEncoding];
	result = [self keyWithPrivateData:pdata usageName:uname];
	return result;
}

+ keyWithPrivateString:(NSString *)private
	andKey:(NSFEEPublicKey *)oldKey
	usageName:(NSString *)uname
{
	NSData *pdata;
	id result;

	if (!uname) return nil;

	pdata = [private dataUsingEncoding:NSUTF8StringEncoding];
	result = [self keyWithPrivateData:pdata andKey:oldKey usageName:uname];
	return result;
}

+ keyWithPrivateString:(NSString *)private
	depth:(unsigned)depth
	usageName:(NSString *)uname
{
	NSData *pdata;
	id result;

	if (!uname) return nil;

	pdata = [private dataUsingEncoding:NSUTF8StringEncoding];
	result = [self keyWithPrivateData:pdata depth:depth usageName:uname];
	return result;
}

/*
 * The standard way of creating a new key given a public key string.
 */
+ keyWithPublicKeyString:(NSString *)hexstr
{
	NSFEEPublicKey 		*result;
	feeReturn		frtn;
	NSStringEncoding	defEndoding;
	const char 		*s;

	/*
	 * Protect against gross errors in the key string formatting...
	 */
	defEndoding = [NSString defaultCStringEncoding];
	if([hexstr canBeConvertedToEncoding:defEndoding] == NO) {
		NSLog(@"NSFEEPublicKey: Bad Public Key String Format (1)\n");
		return nil;
	}

	/*
	 * FIXME - docs say this string is "autoreleased". How is a cString
	 * autoreleased?
	 */
	s = [hexstr cString];
	result = [[self alloc] autorelease];
	result->_pubKey = feePubKeyAlloc();

	frtn = feePubKeyInitFromKeyString(result->_pubKey,
		s, strlen(s));
	if(frtn) {
		NSLog(@"keyWithPublicKeyString:andKey: %s\n",
			feeReturnString(frtn));
		return nil;
	}
	return result;
}

- (void)dealloc
{
	if(_pubKey) {
		feePubKeyFree(_pubKey);
	}
	[super dealloc];
}

/*
 * Create a public key in the form of a string. This string contains an
 * encoded version of all of our ivars except for _private.
 *
 * See KeyStringFormat.doc for info on the format of the public key string;
 * PLEASE UPDATE THIS DOCUMENT WHEN YOU MAKE CHANGES TO THE STRING FORMAT.
 */
- (NSString *)publicKeyString
{
	char		*keyStr;
	unsigned	keyStrLen;
	feeReturn	frtn;
	NSString 	*result;

	if(_pubKey == NULL) {
		return nil;
	}
	frtn = feePubKeyCreateKeyString(_pubKey, &keyStr, &keyStrLen);
	if(frtn) {
		NSLog(@"publicKeyString: %s\n",
			feeReturnString(frtn));
		return nil;
	}
	result = [NSString stringWithCString:keyStr];
	ffree((void *)keyStr);
	return result;
}

- (BOOL)isEqual:(NSFEEPublicKey *)other
{
	if((other == nil) || (other->_pubKey == NULL) || (_pubKey == NULL)) {
		return NO;
	}
	if(feePubKeyIsEqual(_pubKey, other->_pubKey)) {
		return YES;
	}
	else {
		return NO;
	}
}

- (unsigned)keyBitsize
{
	if(_pubKey == NULL) {
		return 0;
	}
	return feePubKeyBitsize(_pubKey);
}

- (NSString *)algorithmName
{
	return [NSString stringWithCString:feePubKeyAlgorithmName()];
}

- (NSString *)usageName
{
	unsigned unameLen;
	const feeUnichar *uname;
	NSString *result;

	if(_pubKey == NULL) {
		return nil;
	}
	uname = feePubKeyUsageName(_pubKey, &unameLen);
	result = [NSString stringWithCharacters:uname length:unameLen];
	return result;
}

- (NSString *)signer
{
	return [self usageName];
}

- (NSData *)padWithPublicKey:(id <NSObject,NSPublicKey>)otherKey
{
	NSFEEPublicKey *other;
	NSMutableData *result;
    	feeReturn frtn;
	unsigned char *padData;
	unsigned padDataLen;

	if(_pubKey == NULL) {
		return nil;
	}
	if (![otherKey isMemberOfClass:isa]) {
		return nil;
	}
	other = otherKey;
	if(other->_pubKey == NULL) {
		return nil;
	}
	frtn = feePubKeyCreatePad(_pubKey,
		other->_pubKey,
		&padData,
		&padDataLen);
	if(frtn) {
		NSLog(@"padWithPublicKey: %s\n", feeReturnString(frtn));
		return nil;
	}
	result = [NSData dataWithBytesNoCopy:padData length:padDataLen];
	return result;
}

- (NSData *)encryptData:(NSData *)data
{
	feeFEEDExp	feed;
	NSData		*result;
	feeReturn	frtn;
	unsigned char	*ctext;
	unsigned	ctextLen;

	if(_pubKey == NULL) {
		return nil;
	}
	feed = feeFEEDExpNewWithPubKey(_pubKey);
	frtn = feeFEEDExpEncrypt(feed,
		[data bytes],
		[data length],
		&ctext,
		&ctextLen);
	if(frtn == FR_Success) {
		result = [NSData dataWithBytesNoCopy:ctext length:ctextLen];
	}
	else {
		NSLog(@"feeFEEDEncrypt: %s\n", feeReturnString(frtn));
		result = nil;
	}
	feeFEEDExpFree(feed);
	return result;
}

- (NSData *)decryptData:(NSData *)data
{
	feeFEEDExp	feed;
	NSData		*result;
	feeReturn	frtn;
	unsigned char	*ptext;
	unsigned	ptextLen;

	if(_pubKey == NULL) {
		return nil;
	}
	feed = feeFEEDExpNewWithPubKey(_pubKey);
	frtn = feeFEEDExpDecrypt(feed,
		[data bytes],
		[data length],
		&ptext,
		&ptextLen);
	if(frtn == FR_Success) {
		result = [NSData dataWithBytesNoCopy:ptext length:ptextLen];
	}
	else {
		NSLog(@"feeFEEDDecrypt: %s\n", feeReturnString(frtn));
		result = nil;
	}
	feeFEEDExpFree(feed);
	return result;
}

/*
 * When 1, we use ECDSA unless we're using a depth which does not
 * have curve orders.
 * WARNING - enabling ECDSA by default breaks ICE and compatibility
 * with Java signatures, at least until we have a Java ECDSA
 * implementation.
 */
#define ECDSA_SIG_DEFAULT	0

- (NSData *)digitalSignatureForData:(NSData *)data
{
	NSData		*result;
	unsigned char	*sig;
	unsigned	sigLen;
	feeReturn	frtn;
	curveParams	*cp;

	if(_pubKey == NULL) {
		return nil;
	}
	cp = feePubKeyCurveParams(_pubKey);
	if(!ECDSA_SIG_DEFAULT || isZero(cp->x1OrderPlus)) {
	    frtn = feePubKeyCreateSignature(_pubKey,
		[data bytes],
		[data length],
		&sig,
		&sigLen);
	}
	else {
	    frtn = feePubKeyCreateECDSASignature(_pubKey,
		[data bytes],
		[data length],
		&sig,
		&sigLen);
	}
	if(frtn) {
		NSLog(@"digitalSignatureForData: %s\n", feeReturnString(frtn));
		return nil;
	}
	result = [NSData dataWithBytesNoCopy:sig length:sigLen];
	return result;
}

- (BOOL)isValidDigitalSignature:(NSData *)signa
	forData:(NSData *)data
{
	feeReturn	frtn;
	feeUnichar 	*sigSigner;
	unsigned	sigSignerLen;
	curveParams	*cp;

	if(_pubKey == NULL) {
		return NO;
	}
	cp = feePubKeyCurveParams(_pubKey);
	if(!ECDSA_SIG_DEFAULT || isZero(cp->x1OrderPlus)) {
	    frtn = feePubKeyVerifySignature(_pubKey,
		[data bytes],
		[data length],
		[signa bytes],
		[signa length],
		&sigSigner,
		&sigSignerLen);
	}
	else {
	    frtn = feePubKeyVerifyECDSASignature(_pubKey,
		[data bytes],
		[data length],
		[signa bytes],
		[signa length],
		&sigSigner,
		&sigSignerLen);
	}

	/*
	 * FIXME - We just throw away the signer for now...
	 */
	if(sigSignerLen) {
		ffree(sigSigner);
	}

	switch(frtn) {
	    case FR_Success:
	    	return YES;
	    case FR_InvalidSignature:
	    	return NO;
	    default:
	    	/*
		 * Something other than simple signature mismatch...
		 */
		NSLog(@"isValidDigitalSignature: %s\n", feeReturnString(frtn));
		return NO;
	}
}

@end

@implementation NSFEEPublicKey(Private)

- (key)minus
{
	if(_pubKey == NULL) {
		return NULL;
	}
	return feePubKeyMinusCurve(_pubKey);
}

- (key)plus
{
	if(_pubKey == NULL) {
		return NULL;
	}
	return feePubKeyPlusCurve(_pubKey);
}

- (feePubKey)feePubKey
{
	return _pubKey;
}

#if 	FEE_DEBUG
- (void)dump
{
	printPubKey(_pubKey);
}
#endif	FEE_DEBUG

@end
