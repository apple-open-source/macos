/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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
 * opensshCoding.cpp - Encoding and decoding of OpenSSH format public keys.
 *
 * Created 8/29/2006 by dmitch.
 */

#include "opensshCoding.h"
#include <CoreFoundation/CFData.h>
#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <security_cdsa_utils/cuEnc64.h>

#define SSH2_RSA_HEADER		"ssh-rsa"
#define SSH2_DSA_HEADER		"ssh-dss"

#ifndef	NDEBUG
#include <stdio.h>
#define dprintf(s...)		printf(s)
#else
#define dprintf(...)
#endif

#pragma mark --- commmon code --- 

uint32_t readUint32(
	const unsigned char *&cp,		// IN/OUT
	unsigned &len)					// IN/OUT 
{
	uint32_t r = 0;
	
	for(unsigned dex=0; dex<sizeof(uint32_t); dex++) {
		r <<= 8;
		r |= *cp++;
	}
	len -= 4;
	return r;
}

void appendUint32(
	CFMutableDataRef cfOut,
	uint32_t ui)
{
	UInt8 buf[sizeof(uint32_t)];
	
	for(int dex=(sizeof(uint32_t) - 1); dex>=0; dex--) {
		buf[dex] = ui & 0xff;
		ui >>= 8;
	}
	CFDataAppendBytes(cfOut, buf, sizeof(uint32_t));
}


/* parse text as decimal, return BIGNUM */
static BIGNUM *parseDecimalBn(
	const unsigned char *cp,
	unsigned len)
{
	for(unsigned dex=0; dex<len; dex++) {
		char c = *cp;
		if((c < '0') || (c > '9')) {
			return NULL;
		}
	}
	char *str = (char *)malloc(len + 1);
	memmove(str, cp, len);
	str[len] = '\0';
	BIGNUM *bn = NULL;
	BN_dec2bn(&bn, str);
	free(str);
	return bn;
}
	
/* write BIGNUM, OpenSSH v2 format (with a 4-byte byte count) */
static CSSM_RETURN appendBigNum2(
	CFMutableDataRef cfOut,
	const BIGNUM *bn)
{
	if(bn == NULL) {
		dprintf("appendBigNum2: NULL bn");
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
	if (BN_is_zero(bn)) {
		appendUint32(cfOut, 0);
		return 0;
	}
	if(bn->neg) {
		dprintf("appendBigNum2: negative numbers not supported\n");
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
	int numBytes = BN_num_bytes(bn);
	unsigned char buf[numBytes];
	int moved = BN_bn2bin(bn, buf);
	if(moved != numBytes) {
		dprintf("appendBigNum: BN_bn2bin() screwup\n");
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
	bool appendZero = false;
	if(buf[0] & 0x80) {
		/* prepend leading zero to make it positive */
		appendZero = true;
		numBytes++;		// to encode the correct 4-byte length 
	}
	appendUint32(cfOut, (uint32_t)numBytes);
	if(appendZero) {
		UInt8 z = 0;
		CFDataAppendBytes(cfOut, &z, 1);
		numBytes--;		// to append the correct number of bytes
	}
	CFDataAppendBytes(cfOut, buf, numBytes);
	memset(buf, 0, numBytes);
	return CSSM_OK;
}

/* read BIGNUM, OpenSSH-2 mpint version */
static BIGNUM *readBigNum2(
	const unsigned char *&cp,	// IN/OUT
	unsigned &remLen)			// IN/OUT
{
	if(remLen < 4) {
		dprintf("readBigNum2: short record(1)\n");
		return NULL;
	}
	uint32_t bytes = readUint32(cp, remLen);
	if(remLen < bytes) {
		dprintf("readBigNum2: short record(2)\n");
		return NULL;
	}
	BIGNUM *bn = BN_bin2bn(cp, bytes, NULL);
	if(bn == NULL) {
		dprintf("readBigNum2: BN_bin2bn error\n");
		return NULL;
	}
	cp += bytes;
	remLen -= bytes;
	return bn;
}

/* Write BIGNUM, OpenSSH-1 decimal (public key) version */
static CSSM_RETURN appendBigNumDec(
	CFMutableDataRef cfOut, 
	const BIGNUM *bn)
{
	char *buf = BN_bn2dec(bn);
	if(buf == NULL) {
		dprintf("appendBigNumDec: BN_bn2dec() error");
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
	CFDataAppendBytes(cfOut, (const UInt8 *)buf, strlen(buf));
	Free(buf);
	return CSSM_OK;
}

/* write string, OpenSSH v2 format (with a 4-byte byte count) */
static void appendString(
	CFMutableDataRef cfOut,
	const char *str,
	unsigned strLen)
{
	appendUint32(cfOut, (uint32_t)strLen);
	CFDataAppendBytes(cfOut, (UInt8 *)str, strLen);
}

/* skip whitespace */
static void skipWhite(
	const unsigned char *&cp,
	unsigned &bytesLeft)
{
	while(bytesLeft != 0) {
		if(isspace((int)(*cp))) {
			cp++;
			bytesLeft--;
		}
		else {
			return;
		}
	}
}

/* find next whitespace or EOF - if EOF, rtn pointer points to one past EOF */
static const unsigned char *findNextWhite(
	const unsigned char *cp,
	unsigned &bytesLeft)
{
	while(bytesLeft != 0) {
		if(isspace((int)(*cp))) {
			return cp;
		}
		cp++;
		bytesLeft--;
	}
	return cp;
}


/* 
 * Decode components from an SSHv2 public key.
 * Also verifies the leading header, e.g. "ssh-rsa".
 * The returned decodedBlob is algorithm-specific.
 */
static CSSM_RETURN parseSSH2PubKey(
	const unsigned char *key,
	unsigned keyLen,
	const char *header,				// SSH2_RSA_HEADER, SSH2_DSA_HEADER
	unsigned char **decodedBlob,	// mallocd and RETURNED
	unsigned *decodedBlobLen)		// RETURNED
{
	unsigned len = strlen(header);
	*decodedBlob = NULL;
	
	/* ID string plus at least one space */
	if(keyLen < (len + 1)) {
		dprintf("parseSSH2PubKey: short record(1)\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	if(memcmp(header, key, len)) {
		dprintf("parseSSH2PubKey: bad header (1)\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	key += len;
	if(*key++ != ' ') {
		dprintf("parseSSH2PubKey: bad header (2)\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	keyLen -= (len + 1);

	/* key points to first whitespace after header */
	skipWhite(key, keyLen);
	if(keyLen == 0) {
		dprintf("parseSSH2PubKey: short key\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	/* key is start of base64 blob */
	const unsigned char *encodedBlob = key;
	const unsigned char *endBlob = findNextWhite(key, keyLen);
	unsigned encodedBlobLen = endBlob - encodedBlob;
	
	/* decode base 64 */
	*decodedBlob = cuDec64(encodedBlob, encodedBlobLen, decodedBlobLen);
	if(*decodedBlob == NULL) {
		dprintf("parseSSH2PubKey: base64 decode error\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	/* skip remainder; it's comment */
	
	return CSSM_OK;
}
	

#pragma mark -- RSA OpenSSHv1 ---

CSSM_RETURN RSAPublicKeyEncodeOpenSSH1(
	RSA 			*rsa, 
	const CssmData	&descData,
	CssmOwnedData	&encodedKey)
{
	CFMutableDataRef cfOut = CFDataCreateMutable(NULL, 0);
	CSSM_RETURN ourRtn = CSSM_OK;
	
	/*
	 * Format is
	 * num_bits in decimal
	 * <space>
	 * e, bignum in decimal
	 * <space>
	 * n, bignum in decimal
	 * <space>
	 * optional comment
	 * newline
	 */
	unsigned numBits = BN_num_bits(rsa->n);
	char bitString[20];
	UInt8 c = ' ';

	snprintf(bitString, sizeof(bitString), "%u ", numBits);
	CFDataAppendBytes(cfOut, (const UInt8 *)bitString, strlen(bitString));
	if(ourRtn = appendBigNumDec(cfOut, rsa->e)) {
		goto errOut;
	}
	CFDataAppendBytes(cfOut, &c, 1);
	if(ourRtn = appendBigNumDec(cfOut, rsa->n)) {
		goto errOut;
	}
	
	if(descData.Length) {
		/* optional comment */
		CFDataAppendBytes(cfOut, &c, 1);
		CFDataAppendBytes(cfOut, (UInt8 *)descData.Data, descData.Length);
	}

	c = '\n';
	CFDataAppendBytes(cfOut, &c, 1);
	encodedKey.copy(CFDataGetBytePtr(cfOut), CFDataGetLength(cfOut));
errOut:
	CFRelease(cfOut);
	return ourRtn;
}

CSSM_RETURN RSAPublicKeyDecodeOpenSSH1(
	RSA 			*rsa, 
	void 			*p, 
	size_t			length)
{
	const unsigned char *cp = (const unsigned char *)p;
	unsigned remLen = length;
	
	skipWhite(cp, remLen);
	
	/* 
	 * cp points to start of size_in_bits in ASCII decimal; we really don't care about 
	 * this field. Find next space.
	 */
	cp = findNextWhite(cp, remLen);
	if(remLen == 0) {
		dprintf("RSAPublicKeyDecodeOpenSSH1: short key (1)\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	skipWhite(cp, remLen);
	if(remLen == 0) {
		dprintf("RSAPublicKeyDecodeOpenSSH1: short key (2)\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	/*
	 * cp points to start of e
	 */
	const unsigned char *ep = findNextWhite(cp, remLen);
	if(remLen == 0) {
		dprintf("RSAPublicKeyDecodeOpenSSH1: short key (3)\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	unsigned len = ep - cp;
	rsa->e = parseDecimalBn(cp, len);
	if(rsa->e == NULL) {
		return CSSMERR_CSP_INVALID_KEY;
	}
	cp += len;
	
	skipWhite(cp, remLen);
	if(remLen == 0) {
		dprintf("RSAPublicKeyDecodeOpenSSH1: short key (4)\n");
		return -1;
	}
	
	/* cp points to start of n */
	ep = findNextWhite(cp, remLen);
	len = ep - cp;
	rsa->n = parseDecimalBn(cp, len);
	if(rsa->n == NULL) {
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	/* remainder is comment, we ignore */
	return CSSM_OK;

}

CSSM_RETURN RSAPrivateKeyEncodeOpenSSH1(
	RSA 			*rsa, 
	const CssmData	&descData,
	CssmOwnedData	&encodedKey)
{
	CFDataRef cfOut;
	CSSM_RETURN ourRtn;

	ourRtn = encodeOpenSSHv1PrivKey(rsa, descData.Data, descData.Length, NULL, &cfOut);
	if(ourRtn) {
		return ourRtn;
	}
	encodedKey.copy(CFDataGetBytePtr(cfOut), CFDataGetLength(cfOut));
	CFRelease(cfOut);
	return CSSM_OK;
}

extern CSSM_RETURN RSAPrivateKeyDecodeOpenSSH1(
	RSA 			*openKey, 
	void 			*p, 
	size_t			length)
{
	return decodeOpenSSHv1PrivKey((const unsigned char *)p, length,
		openKey, NULL, NULL, NULL);
}

#pragma mark -- RSA OpenSSHv2 ---

CSSM_RETURN RSAPublicKeyEncodeOpenSSH2(
	RSA 			*rsa, 
	const CssmData	&descData,
	CssmOwnedData	&encodedKey)
{
	unsigned char *b64 = NULL;
	unsigned b64Len;
	UInt8 c;
	
	/* 
	 * First, the inner base64-encoded blob, consisting of
	 * ssh-rsa
	 * e
	 * n
	 */
	CFMutableDataRef cfOut = CFDataCreateMutable(NULL, 0);
	CSSM_RETURN ourRtn = CSSM_OK;
	appendString(cfOut, SSH2_RSA_HEADER, strlen(SSH2_RSA_HEADER));
	if(ourRtn = appendBigNum2(cfOut, rsa->e)) {
		goto errOut;
	}
	if(ourRtn = appendBigNum2(cfOut, rsa->n)) {
		goto errOut;
	}
	
	/* base64 encode that */
	b64 = cuEnc64((unsigned char *)CFDataGetBytePtr(cfOut), CFDataGetLength(cfOut), &b64Len);
	
	/* cuEnc64 added newline and NULL, which we really don't want */
	b64Len -= 2;
	
	/* Now start over, dropping that base64 into a public blob. */
	CFDataSetLength(cfOut, 0);
	CFDataAppendBytes(cfOut, (UInt8 *)SSH2_RSA_HEADER, strlen(SSH2_RSA_HEADER));
	c = ' ';
	CFDataAppendBytes(cfOut, &c, 1);
	CFDataAppendBytes(cfOut, b64, b64Len);
	
	if(descData.Length) {
		/* optional comment */
		CFDataAppendBytes(cfOut, &c, 1);
		CFDataAppendBytes(cfOut, (UInt8 *)descData.Data, descData.Length);
	}
	
	/* finish it with a newline */
	c = '\n';
	CFDataAppendBytes(cfOut, &c, 1);
	
	encodedKey.copy(CFDataGetBytePtr(cfOut), CFDataGetLength(cfOut));
errOut:
	CFRelease(cfOut);
	if(b64) {
		free(b64);
	}
	return ourRtn;
}

CSSM_RETURN RSAPublicKeyDecodeOpenSSH2(
	RSA 			*rsa, 
	void 			*p, 
	size_t			length)
{
	const unsigned char *key = (const unsigned char *)p;
	unsigned keyLen = length;
	CSSM_RETURN ourRtn;
	
	/* 
	 * Verify header
	 * get base64-decoded blob 
	 */
	unsigned char *decodedBlob = NULL;
	unsigned decodedBlobLen = 0;
	if(ourRtn = parseSSH2PubKey(key, keyLen, SSH2_RSA_HEADER, &decodedBlob, &decodedBlobLen)) {
		return ourRtn;
	}
	/* subsequent errors to errOut: */
	
	/*
	 * The inner base64-decoded blob, consisting of
	 * ssh-rsa
	 * e
	 * n
	 */
	uint32_t decLen;
	unsigned len;
	
	key = decodedBlob;
	keyLen = decodedBlobLen;
	if(keyLen < 12) {
		/* three length fields at least */
		dprintf("RSAPublicKeyDecodeOpenSSH2: short record(2)\n");
		ourRtn = -1;
		goto errOut;
	}
	decLen = readUint32(key, keyLen);
	len = strlen(SSH2_RSA_HEADER);
	if(decLen != len) {
		dprintf("RSAPublicKeyDecodeOpenSSH2: bad header (2)\n");
		ourRtn = CSSMERR_CSP_INVALID_KEY;
		goto errOut;
	}
	if(memcmp(SSH2_RSA_HEADER, key, len)) {
		dprintf("RSAPublicKeyDecodeOpenSSH2: bad header (1)\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	key += len;
	keyLen -= len;
	
	rsa->e = readBigNum2(key, keyLen);
	if(rsa->e == NULL) {
		ourRtn = CSSMERR_CSP_INVALID_KEY;
		goto errOut;
	}
	rsa->n = readBigNum2(key, keyLen);
	if(rsa->n == NULL) {
		ourRtn = CSSMERR_CSP_INVALID_KEY;
		goto errOut;
	}

errOut:
	free(decodedBlob);
	return ourRtn;
}

#pragma mark -- DSA OpenSSHv2 ---

CSSM_RETURN DSAPublicKeyEncodeOpenSSH2(
	DSA 			*dsa, 
	const CssmData	&descData,
	CssmOwnedData	&encodedKey)
{
	unsigned char *b64 = NULL;
	unsigned b64Len;
	UInt8 c;
	
	/* 
	 * First, the inner base64-encoded blob, consisting of
	 * ssh-dss
	 * p
	 * q
	 * g
	 * pub_key
	 */
	CFMutableDataRef cfOut = CFDataCreateMutable(NULL, 0);
	int ourRtn = 0;
	appendString(cfOut, SSH2_DSA_HEADER, strlen(SSH2_DSA_HEADER));
	if(ourRtn = appendBigNum2(cfOut, dsa->p)) {
		goto errOut;
	}
	if(ourRtn = appendBigNum2(cfOut, dsa->q)) {
		goto errOut;
	}
	if(ourRtn = appendBigNum2(cfOut, dsa->g)) {
		goto errOut;
	}
	if(ourRtn = appendBigNum2(cfOut, dsa->pub_key)) {
		goto errOut;
	}
	
	/* base64 encode that */
	b64 = cuEnc64((unsigned char *)CFDataGetBytePtr(cfOut), CFDataGetLength(cfOut), &b64Len);
	
	/* cuEnc64 added newline and NULL, which we really don't want */
	b64Len -= 2;
	
	/* Now start over, dropping that base64 into a public blob. */
	CFDataSetLength(cfOut, 0);
	CFDataAppendBytes(cfOut, (UInt8 *)SSH2_DSA_HEADER, strlen(SSH2_DSA_HEADER));
	c = ' ';
	CFDataAppendBytes(cfOut, &c, 1);
	CFDataAppendBytes(cfOut, b64, b64Len);
	
	if(descData.Length) {
		/* optional comment */
		CFDataAppendBytes(cfOut, &c, 1);
		CFDataAppendBytes(cfOut, (UInt8 *)descData.Data, descData.Length);
	}
	
	/* finish it with a newline */
	c = '\n';
	CFDataAppendBytes(cfOut, &c, 1);
	
	encodedKey.copy(CFDataGetBytePtr(cfOut), CFDataGetLength(cfOut));
	
errOut:
	CFRelease(cfOut);
	if(b64) {
		free(b64);
	}
	return ourRtn;
}

CSSM_RETURN DSAPublicKeyDecodeOpenSSH2(
	DSA 			*dsa, 
	void 			*p, 
	size_t			length)
{
	const unsigned char *key = (const unsigned char *)p;
	unsigned keyLen = length;
	CSSM_RETURN ourRtn;
	
	/* 
	 * Verify header
	 * get base64-decoded blob 
	 */
	unsigned char *decodedBlob = NULL;
	unsigned decodedBlobLen = 0;
	if(ourRtn = parseSSH2PubKey(key, keyLen, SSH2_DSA_HEADER, &decodedBlob, &decodedBlobLen)) {
		return ourRtn;
	}
	/* subsequent errors to errOut: */
	
	/*
	 * The inner base64-decoded blob, consisting of
	 * ssh-dss
	 * p
	 * q
	 * g
	 * pub_key
	 */
	uint32_t decLen;
	unsigned len;
	
	key = decodedBlob;
	keyLen = decodedBlobLen;
	if(keyLen < 20) {
		/* five length fields at least */
		dprintf("DSAPublicKeyDecodeOpenSSH2: short record(2)\n");
		ourRtn = CSSMERR_CSP_INVALID_KEY;
		goto errOut;
	}
	decLen = readUint32(key, keyLen);
	len = strlen(SSH2_DSA_HEADER);
	if(decLen != len) {
		dprintf("DSAPublicKeyDecodeOpenSSH2: bad header (2)\n");
		ourRtn = CSSMERR_CSP_INVALID_KEY;
		goto errOut;
	}
	if(memcmp(SSH2_DSA_HEADER, key, len)) {
		dprintf("DSAPublicKeyDecodeOpenSSH2: bad header (1)\n");
		return CSSMERR_CSP_INVALID_KEY;
	}
	key += len;
	keyLen -= len;
	
	dsa->p = readBigNum2(key, keyLen);
	if(dsa->p == NULL) {
		ourRtn = CSSMERR_CSP_INVALID_KEY;
		goto errOut;
	}
	dsa->q = readBigNum2(key, keyLen);
	if(dsa->q == NULL) {
		ourRtn = CSSMERR_CSP_INVALID_KEY;
		goto errOut;
	}
	dsa->g = readBigNum2(key, keyLen);
	if(dsa->g == NULL) {
		ourRtn = CSSMERR_CSP_INVALID_KEY;
		goto errOut;
	}
	dsa->pub_key = readBigNum2(key, keyLen);
	if(dsa->pub_key == NULL) {
		ourRtn = CSSMERR_CSP_INVALID_KEY;
		goto errOut;
	}

errOut:
	free(decodedBlob);
	return ourRtn;

}

