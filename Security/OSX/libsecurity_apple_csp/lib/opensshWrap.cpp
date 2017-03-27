/*
 * Copyright (c) 2006,2011-2012,2014 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * opensshCoding.h - Encoding and decoding of OpenSSH format public keys.
 *
 */

#include "AppleCSPSession.h"
#include "AppleCSPContext.h"
#include "AppleCSPUtils.h"
#include "AppleCSPKeys.h"
#include "RSA_DSA_Keys.h"
#include "opensshCoding.h"
#include "cspdebugging.h"
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonCryptor.h>
#include <openssl/rsa_legacy.h>
#include <openssl/bn_legacy.h>
#include <security_utilities/devrandom.h>

static const char *authfile_id_string = "SSH PRIVATE KEY FILE FORMAT 1.1\n";

/* default comment on encode if app doesn't provide DescriptiveData */
#define OPENSSH1_COMMENT		"Encoded by Mac OS X Security.framework"

/* from openssh cipher.h */
#define SSH_CIPHER_NONE			0	/* no encryption */
#define SSH_CIPHER_IDEA			1	/* IDEA CFB */
#define SSH_CIPHER_DES			2	/* DES CBC */
#define SSH_CIPHER_3DES			3	/* 3DES CBC */
#define SSH_CIPHER_BROKEN_TSS	4	/* TRI's Simple Stream encryption CBC */
#define SSH_CIPHER_BROKEN_RC4	5	/* Alleged RC4 */
#define SSH_CIPHER_BLOWFISH		6
#define SSH_CIPHER_RESERVED		7

#pragma mark --- utilities ---

static void appendUint16(
	CFMutableDataRef cfOut,
	uint16_t ui)
{
	UInt8 buf[sizeof(uint16_t)];
	
	buf[1] = ui & 0xff;
	ui >>= 8;
	buf[0] = ui;
	CFDataAppendBytes(cfOut, buf, sizeof(uint16_t));
}

static uint16_t readUint16(
	const unsigned char *&cp,		// IN/OUT
	unsigned &len)					// IN/OUT 
{
	uint16_t r = *cp++;
	r <<= 8;
	r |= *cp++;
	len -= 2;
	return r;
}

/* Write BIGNUM, OpenSSH-1 version */
static CSSM_RETURN appendBigNum(
	CFMutableDataRef cfOut, 
	const BIGNUM *bn)
{
	/* 16 bits of numbits */
	unsigned numBits = BN_num_bits(bn);
	appendUint16(cfOut, numBits);
	
	/* serialize the bytes */
	int numBytes = (numBits + 7) / 8;
	unsigned char outBytes[numBytes];	// gcc is so cool...
	int moved = BN_bn2bin(bn, outBytes);
	if(moved != numBytes) {
		errorLog0("appendBigNum: BN_bn2bin() screwup\n");
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
	CFDataAppendBytes(cfOut, (UInt8 *)outBytes, numBytes);
	return CSSM_OK;
}

/* Read BIGNUM, OpenSSH-1 version */
static BIGNUM *readBigNum(
	const unsigned char *&cp,	// IN/OUT
	unsigned &remLen)			// IN/OUT
{
	if(remLen < sizeof(uint16_t)) {
		errorLog0("readBigNum: short record(1)\n");
		return NULL;
	}
	uint16_t numBits = readUint16(cp, remLen);
	unsigned bytes = (numBits + 7) / 8;
	if(remLen < bytes) {
		errorLog0("readBigNum: short record(2)\n");
		return NULL;
	}
	BIGNUM *bn = BN_bin2bn(cp, bytes, NULL);
	if(bn == NULL) {
		errorLog0("readBigNum: BN_bin2bn error\n");
		return NULL;
	}
	cp += bytes;
	remLen -= bytes;
	return bn;
}

/* 
 * Calculate d mod{p-1,q-1}
 * Used when decoding OpenSSH-1 private RSA key.
 */
static CSSM_RETURN rsa_generate_additional_parameters(RSA *rsa)
{
	BIGNUM *aux;
	BN_CTX *ctx;
	
	if((rsa->dmq1 = BN_new()) == NULL) {
		errorLog0("rsa_generate_additional_parameters: BN_new failed");
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
	if((rsa->dmp1 = BN_new()) == NULL) {
		errorLog0("rsa_generate_additional_parameters: BN_new failed");
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
	if ((aux = BN_new()) == NULL) {
		errorLog0("rsa_generate_additional_parameters: BN_new failed");
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
	if ((ctx = BN_CTX_new()) == NULL) {
		errorLog0("rsa_generate_additional_parameters: BN_CTX_new failed");
		BN_clear_free(aux);
		return CSSMERR_CSP_INTERNAL_ERROR;
	} 

	BN_sub(aux, rsa->q, BN_value_one());
	BN_mod(rsa->dmq1, rsa->d, aux, ctx);

	BN_sub(aux, rsa->p, BN_value_one());
	BN_mod(rsa->dmp1, rsa->d, aux, ctx);

	BN_clear_free(aux);
	BN_CTX_free(ctx);
	return CSSM_OK;
}

#pragma mark --- encrypt/decrypt ---

/* 
 * Encrypt/decrypt the secret portion of an OpenSSHv1 format RSA private key.
 */
static CSSM_RETURN ssh1DES3Crypt(
	unsigned char cipher,
	bool doEncrypt,
	const unsigned char *inText,
	unsigned inTextLen,
	const uint8 *key,			// MD5(password)
	CSSM_SIZE keyLen,
	unsigned char *outText,		// data RETURNED here, caller mallocs.
	unsigned *outTextLen)		// RETURNED
{
	switch(cipher) {
		case SSH_CIPHER_3DES:
			break;
		case SSH_CIPHER_NONE:
			/* cleartext RSA private key, e.g. host key. */
			memmove(outText, inText, inTextLen);
			*outTextLen = inTextLen;
			return CSSM_OK;
		default:
			/* who knows how we're going to figure these out */
			errorLog1("***ssh1DES3Crypt: Unsupported cipher (%u)\n", cipher);
			return CSSMERR_CSP_INVALID_KEY;
	}
	
	if(keyLen != CC_MD5_DIGEST_LENGTH) {
		errorLog0("ssh1DES3Crypt: bad key length\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	/* three keys from that, like so: */
	unsigned char k1[kCCKeySizeDES];
	unsigned char k2[kCCKeySizeDES];
	unsigned char k3[kCCKeySizeDES];
	memmove(k1, key, kCCKeySizeDES);
	memmove(k2, key + kCCKeySizeDES, kCCKeySizeDES);
	memmove(k3, key, kCCKeySizeDES);
	
	CCOperation op1_3;
	CCOperation op2;
	if(doEncrypt) {
		op1_3 = kCCEncrypt;
		op2   = kCCDecrypt;
	}
	else {
		op1_3 = kCCDecrypt;
		op2   = kCCEncrypt;
	}
	
	/* the openssh v1 pseudo triple DES. Each DES pass has its own CBC. */
	size_t moved = 0;

	CCCryptorStatus cstat = CCCrypt(op1_3, kCCAlgorithmDES, 
		0,						// no padding
		k1, kCCKeySizeDES, 
		NULL,		// IV
		inText, inTextLen,
		outText, inTextLen, &moved);
	if(cstat) {
		/* should never happen */
		errorLog1("***ssh1DES3Crypt: CCCrypt()(1) returned %u\n", (unsigned)cstat);
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
	cstat = CCCrypt(op2, kCCAlgorithmDES, 
		0,						// no padding - SSH does that itself 
		k2, kCCKeySizeDES, 
		NULL,		// IV
		outText, moved,
		outText, inTextLen, &moved);
	if(cstat) {
		errorLog1("***ssh1DES3Crypt: CCCrypt()(2) returned %u\n", (unsigned)cstat);
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
	cstat = CCCrypt(op1_3, kCCAlgorithmDES, 
		0,						// no padding - SSH does that itself 
		k3, kCCKeySizeDES, 
		NULL,		// IV
		outText, moved,
		outText, inTextLen, &moved);
	if(cstat) {
		errorLog1("***ssh1DES3Crypt: CCCrypt()(3) returned %u\n", (unsigned)cstat);
		return CSSMERR_CSP_INTERNAL_ERROR;
	}

	*outTextLen = (unsigned)moved;
	return CSSM_OK;
}

#pragma mark --- DeriveKey ---

/*
 * Key derivation for OpenSSH1 private key wrap/unwrap.
 * This is pretty trivial, it's just an MD5() operation. The main 
 * purpose for doing this in a DeriveKey operation is to enable the 
 * use of either Secure Passphrases, obtained by securityd/SecurityAgent,
 * or app-specified data.
 */
void AppleCSPSession::DeriveKey_OpenSSH1(
	const Context &context,
	CSSM_ALGORITHMS algId,
	const CssmData &Param,			// IV optional, mallocd by app to indicate
									//   size 
	CSSM_DATA *keyData)				// mallocd by caller to indicate size - must be
									// size of MD5 digest!
{
	CSSM_DATA pwd = {0, NULL};
	
	if(keyData->Length != CC_MD5_DIGEST_LENGTH) {
		errorLog0("DeriveKey_OpenSSH1: invalid key length\n");
		CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEY_SIZE);
	}
	
	/* password from either Seed.Param or from base key */
	CssmCryptoData *cryptData = 
		context.get<CssmCryptoData>(CSSM_ATTRIBUTE_SEED);
	if((cryptData != NULL) && (cryptData->Param.Length != 0)) {
		pwd = cryptData->Param;
	}
	else {
		/* Get secure passphrase from base key */
		CssmKey *passKey = context.get<CssmKey>(CSSM_ATTRIBUTE_KEY);
		if (passKey != NULL) {
			AppleCSPContext::symmetricKeyBits(context, *this,
				CSSM_ALGID_SECURE_PASSPHRASE, CSSM_KEYUSE_DERIVE, 
				pwd.Data, pwd.Length);
		}
	}

	if(pwd.Data == NULL) {
		errorLog0("DeriveKey_PKCS5_V1_5: null Passphrase\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_DATA);
	}
	if(pwd.Length == 0) {		
		errorLog0("DeriveKey_PKCS5_V1_5: zero length passphrase\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_INPUT_POINTER);
	}

	/* here it is */
	CC_MD5(pwd.Data, (CC_LONG)pwd.Length, keyData->Data);

}

#pragma mark --- Encode/Wrap OpenSSHv1 private key ---

/* 
 * Encode OpenSSHv1 private key, with or without encryption.
 * This used for generating key blobs of format CSSM_KEYBLOB_RAW_FORMAT_OPENSSH
 * as well as wrapping keys in format CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSH1.
 */
CSSM_RETURN encodeOpenSSHv1PrivKey(
	RSA *rsa,
	const uint8 *comment,		/* optional */
	unsigned commentLen,
	const uint8 *encryptKey,	/* optional; if present, it's 16 bytes of MD5(password) */
	CFDataRef *encodedKey)		/* RETURNED */
{
	CFMutableDataRef cfOut = CFDataCreateMutable(NULL, 0);
	CSSM_RETURN ourRtn = CSSM_OK;
	
	/* ID string including NULL */
	CFDataAppendBytes(cfOut, (const UInt8 *)authfile_id_string, strlen(authfile_id_string) + 1);
	
	/* one byte cipher */
	UInt8 cipherSpec = encryptKey ? SSH_CIPHER_3DES : SSH_CIPHER_NONE;
	CFDataAppendBytes(cfOut, &cipherSpec, 1);
	
	/* spares */
	UInt8 spares[4] = {0};
	CFDataAppendBytes(cfOut, spares, 4);
	
	/*
	 * Clear text public key:
	 * uint32 bits
	 * bignum n
	 * bignum e
	 */
	uint32_t keybits = RSA_size(rsa) * 8;
	appendUint32(cfOut, keybits);
	appendBigNum(cfOut, rsa->n);
	appendBigNum(cfOut, rsa->e);

	/* 
	 * Comment string.
	 * The format appears to require this, or else we wouldn't know
	 * when we've got to the ciphertext on decode.
	 */
	if((comment == NULL) || (commentLen == 0)) {
		comment = (const UInt8 *)OPENSSH1_COMMENT;
		commentLen = strlen(OPENSSH1_COMMENT);
	}
	appendUint32(cfOut, commentLen);
	CFDataAppendBytes(cfOut, comment, commentLen);
	
	/* 
	 * Remainder is encrypted, consisting of
	 *
	 * [0-1]		-- random bytes
	 * [2-3]		-- copy of [01] for passphrase validity checking
	 * buffer_put_bignum(d)
	 * buffer_put_bignum(iqmp)
	 * buffer_put_bignum(q)
	 * buffer_put_bignum(p)
	 * pad to block size
	 */
	CFMutableDataRef ptext = CFDataCreateMutable(NULL, 0);
	
	/* [0..3] check bytes */
	UInt8 checkBytes[4];
	DevRandomGenerator rng = DevRandomGenerator();
	rng.random(checkBytes, 2);
	checkBytes[2] = checkBytes[0];
	checkBytes[3] = checkBytes[1];
	CFDataAppendBytes(ptext, checkBytes, 4);
	
	/* d, iqmp, q, p */
	appendBigNum(ptext, rsa->d);
	appendBigNum(ptext, rsa->iqmp);
	appendBigNum(ptext, rsa->q);
	appendBigNum(ptext, rsa->p);
	
	/* pad to block boundary */
	CFIndex ptextLen = CFDataGetLength(ptext);
	unsigned padding = 0;
	unsigned rem = (unsigned)ptextLen & 0x7;
	if(rem) {
		padding = 8 - rem;
	}
	UInt8 padByte = 0;
	for(unsigned dex=0; dex<padding; dex++) {
		CFDataAppendBytes(ptext, &padByte, 1);
	}
	
	/* encrypt it */
	ptextLen = CFDataGetLength(ptext);
	unsigned char ctext[ptextLen];
	unsigned ctextLen;
	ourRtn = ssh1DES3Crypt(cipherSpec, true, 
		(unsigned char *)CFDataGetBytePtr(ptext), (unsigned)ptextLen,
		encryptKey, encryptKey ? CC_MD5_DIGEST_LENGTH : 0,
		ctext, &ctextLen);
	if(ourRtn != 0) {
		goto errOut;
	}
	
	/* appended encrypted portion */
	CFDataAppendBytes(cfOut, ctext, ctextLen);
	*encodedKey = cfOut;
errOut:
	/* it would be proper to zero out ptext here, but we can't do that to a CFData */
	CFRelease(ptext);
	return ourRtn;
}

void AppleCSPSession::WrapKeyOpenSSH1(
	CSSM_CC_HANDLE CCHandle,
	const Context &context,
	const AccessCredentials &AccessCred,
	BinaryKey &unwrappedBinKey,
	CssmData &rawBlob,
	bool allocdRawBlob,			// callee has to free rawBlob
	const CssmData *DescriptiveData,
	CssmKey &WrappedKey,
	CSSM_PRIVILEGE Privilege)
{
	/*
	 * The job here is to convert the RSA key in binKey to the OpenSSHv1 private
	 * key format, and drop that into WrappedKey.KeyData (allocated by the session).
	 *
	 * This cast throws an exception if the key is not an RSA key, which 
	 * would be a major bogon, since our caller verified that the unwrapped key
	 * is a private RSA key.
	 */
	RSABinaryKey &rPubBinKey = dynamic_cast<RSABinaryKey &>(unwrappedBinKey);
	RSA *rsa = rPubBinKey.mRsaKey;
	CASSERT(rsa != NULL);
	
	/*
	 * Get the raw password bits from the wrapping key.
	 * Our caller verified that the context has a symmetric key; this call
	 * ensures that the key is of algorithm CSSM_ALGID_OPENSSH1.
	 * Key length 0 means no encryption. 
	 */ 
	CSSM_SIZE	wrappingKeyLen = 0;
	uint8		*wrappingKey = NULL;
	
	AppleCSPContext::symmetricKeyBits(context, *this,
		CSSM_ALGID_OPENSSH1, CSSM_KEYUSE_WRAP, 
		wrappingKey, wrappingKeyLen);
	if(wrappingKeyLen != CC_MD5_DIGEST_LENGTH) {
		errorLog0("AppleCSPSession::WrapKeyOpenSSH1: bad wrapping key length\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}	
	
	CFDataRef cfOut = NULL;
	
	/* 
	 * Optional comment string from DescriptiveData.
	 */
	const UInt8 *comment = NULL;
	unsigned commentLen = 0;
	if((DescriptiveData != NULL) && (DescriptiveData->Length != 0)) {
		comment = (const UInt8 *)DescriptiveData->Data;
		commentLen = (unsigned)DescriptiveData->Length;
	}

	/* generate the encrypted blob */
	CSSM_RETURN crtn = encodeOpenSSHv1PrivKey(rsa, comment, commentLen, wrappingKey, &cfOut);
	if(crtn) {	
		CssmError::throwMe(crtn);
	}
	
	/* allocate key data in session's memory space */
	CFIndex len = CFDataGetLength(cfOut);
	setUpData(WrappedKey.KeyData, len, normAllocator);
	memmove(WrappedKey.KeyData.Data, CFDataGetBytePtr(cfOut), len);
	CFRelease(cfOut);
	
	/* outgoing header */
	WrappedKey.KeyHeader.BlobType = CSSM_KEYBLOB_WRAPPED;
	// OK to be zero or not present 
	WrappedKey.KeyHeader.WrapMode = CSSM_ALGMODE_NONE;
	WrappedKey.KeyHeader.Format = CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSH1;
}

#pragma mark --- Decode/Unwrap OpenSSHv1 private key ---

/* 
 * Decode OpenSSHv1 private, optionally decrypting the secret portion. 
 * This used for decoding key blobs of format CSSM_KEYBLOB_RAW_FORMAT_OPENSSH
 * as well as unwrapping keys in format CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSH1.
 */
CSSM_RETURN decodeOpenSSHv1PrivKey(
	const unsigned char *encodedKey,
	unsigned encodedKeyLen,
	RSA *rsa,
	const uint8 *decryptKey,	/* optional; if present, it's 16 bytes of MD5(password) */
	uint8 **comment,			/* optional, mallocd and RETURNED */
	unsigned *commentLen)		/* RETURNED */
{
	unsigned len = (unsigned)strlen(authfile_id_string);
	const unsigned char *cp = encodedKey;
	unsigned remLen = encodedKeyLen;
	CSSM_RETURN ourRtn = CSSM_OK;
	
	/* length: ID string, NULL, Cipher, 4-byte spare */
	if(remLen < (len + 6)) {
		errorLog0("decodeOpenSSHv1PrivKey: short record(1)\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	/* ID string plus a NULL */
	if(memcmp(authfile_id_string, cp, len)) {
		errorLog0("decodeOpenSSHv1PrivKey: bad header\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	cp += (len + 1);
	remLen -= (len + 1);
	
	/* cipher */
	unsigned char cipherSpec = *cp;
	switch(cipherSpec) {
		case SSH_CIPHER_NONE:
			if(decryptKey != NULL) {
				errorLog0("decodeOpenSSHv1PrivKey: Attempt to decrypt plaintext key\n");
				return CSSMERR_CSP_INVALID_KEY;
			}
			break;
		case SSH_CIPHER_3DES:
			if(decryptKey == NULL) {
				errorLog0("decodeOpenSSHv1PrivKey: Encrypted key with no decryptKey\n");
				return CSSMERR_CSP_INVALID_KEY;
			}
			break;
		default:
			/* I hope we don't see any other values here */
			errorLog1("decodeOpenSSHv1PrivKey: unknown cipherSpec (%u)\n", cipherSpec);
			return CSSMERR_CSP_INVALID_KEY;
	}
		
	/* skip cipher, spares */
	cp += 5;
	remLen -= 5;
	
	/*
	 * Clear text public key:
	 * uint32 bits
	 * bignum n
	 * bignum e
	 */
	if(remLen < sizeof(uint32_t)) {
		errorLog0("decodeOpenSSHv1PrivKey: bad len(1)\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	/* skip over bits */
	readUint32(cp, remLen);
	rsa->n = readBigNum(cp, remLen);
	if(rsa->n == NULL) {
		errorLog0("decodeOpenSSHv1PrivKey: error decoding n\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	rsa->e = readBigNum(cp, remLen);
	if(rsa->e == NULL) {
		errorLog0("decodeOpenSSHv1PrivKey: error decoding e\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	/* comment string: 4-byte length and the string w/o NULL */
	if(remLen < sizeof(uint32_t)) {
		errorLog0("decodeOpenSSHv1PrivKey: bad len(2)\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	uint32_t commLen = readUint32(cp, remLen);
	if(commLen > remLen) {
		errorLog0("decodeOpenSSHv1PrivKey: bad len(3)\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	if(comment) {
		*comment = (uint8 *)malloc(commLen);
		*commentLen = commLen;
		memcpy(*comment, cp, commLen);
	}
	
	cp += commLen;
	remLen -= commLen;
	
	/* everything that remains is ciphertext */
	unsigned char *ptext = (unsigned char *)malloc(remLen);
	unsigned ptextLen = 0;
	ourRtn = ssh1DES3Crypt(cipherSpec, false, cp, remLen, 
		decryptKey, decryptKey ? CC_MD5_DIGEST_LENGTH : 0, 
		ptext, &ptextLen);
	if(ourRtn) {
		errorLog0("UnwrapKeyOpenSSH1: decrypt error\n");
		ourRtn = CSSMERR_CSP_INVALID_KEY;
		goto errOut;
	}
		
	/* plaintext contents:
	
	[0-1]		-- random bytes
	[2-3]		-- copy of [01] for passphrase validity checking
	buffer_put_bignum(d)
	buffer_put_bignum(iqmp)
	buffer_put_bignum(q)
	buffer_put_bignum(p)
	pad to block size
	*/
	cp = ptext;
	remLen = ptextLen;
	if(remLen < 4) {
		errorLog0("UnwrapKeyOpenSSH1: bad len(4)\n");
		ourRtn = CSSMERR_CSP_INVALID_KEY;
		goto errOut;
	}
	if((cp[0] != cp[2]) || (cp[1] != cp[3])) {
		/* decrypt fail */
		errorLog0("UnwrapKeyOpenSSH1: check byte error\n");
		ourRtn = CSSMERR_CSP_INVALID_KEY;
		goto errOut;
	}
	cp += 4;
	remLen -= 4;
	
	/* remainder comprises private portion of RSA key */
	rsa->d = readBigNum(cp, remLen);
	if(rsa->d == NULL) {
		errorLog0("UnwrapKeyOpenSSH1: error decoding d\n");
		ourRtn = CSSMERR_CSP_INVALID_KEY;
		goto errOut;
	}
	rsa->iqmp = readBigNum(cp, remLen);
	if(rsa->iqmp == NULL) {
		errorLog0("UnwrapKeyOpenSSH1: error decoding iqmp\n");
		ourRtn = CSSMERR_CSP_INVALID_KEY;
		goto errOut;
	}
	rsa->q = readBigNum(cp, remLen);
	if(rsa->q == NULL) {
		errorLog0("UnwrapKeyOpenSSH1: error decoding q\n");
		ourRtn = CSSMERR_CSP_INVALID_KEY;
		goto errOut;
	}
	rsa->p = readBigNum(cp, remLen);
	if(rsa->p == NULL) {
		errorLog0("UnwrapKeyOpenSSH1: error decoding p\n");
		ourRtn = CSSMERR_CSP_INVALID_KEY;
		goto errOut;
	}

	/* calculate d mod{p-1,q-1} */
	ourRtn = rsa_generate_additional_parameters(rsa);
	
errOut:
	if(ptext) {
		memset(ptext, 0, ptextLen);
		free(ptext);
	}
	return ourRtn;
}

void AppleCSPSession::UnwrapKeyOpenSSH1(
	CSSM_CC_HANDLE CCHandle,
	const Context &context,
	const CssmKey &WrappedKey,
	const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
	CssmKey &UnwrappedKey,
	CssmData &DescriptiveData,
	CSSM_PRIVILEGE Privilege,
	cspKeyStorage keyStorage)
{
	/*
	 * Get the raw password bits from the unwrapping key.
	 * Our caller verified that the context has a symmetric key; this call
	 * ensures that the key is of algorithm CSSM_ALGID_OPENSSH1.
	 */ 
	CSSM_SIZE		unwrapKeyLen = 0;
	uint8			*unwrapKey = NULL;
	
	AppleCSPContext::symmetricKeyBits(context, *this,
		CSSM_ALGID_OPENSSH1, CSSM_KEYUSE_UNWRAP, 
		unwrapKey, unwrapKeyLen);
	if((unwrapKey == NULL) || (unwrapKeyLen != CC_MD5_DIGEST_LENGTH)) {
		errorLog0("AppleCSPSession::UnwrapKeyOpenSSH1: bad unwrapping key length\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}	

	RSA *rsa = RSA_new();
	CSSM_RETURN ourRtn = CSSM_OK;
	unsigned char *comment = NULL;
	unsigned commentLen = 0;
	RSABinaryKey *binKey = NULL;
	
	ourRtn = decodeOpenSSHv1PrivKey((const unsigned char *)WrappedKey.KeyData.Data,
		(unsigned)WrappedKey.KeyData.Length,
		rsa, unwrapKey, &comment, &commentLen);
	if(ourRtn) {
		goto errOut;
	}
	if(comment) {
		setUpCssmData(DescriptiveData, commentLen, normAllocator);
		memcpy(DescriptiveData.Data, comment, commentLen);
	}

	/* 
	 * Our caller ensured that we're only generating a reference key,
	 * which we do like so:
	 */
	binKey = new RSABinaryKey(rsa);
	addRefKey(*binKey, UnwrappedKey);
	
errOut:
	if(ourRtn) {
		if(rsa) {
			RSA_free(rsa);
		}
		CssmError::throwMe(ourRtn);
	}
}
