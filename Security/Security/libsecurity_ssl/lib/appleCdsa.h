/*
 * Copyright (c) 1999-2001,2005-2007,2010-2012,2014 Apple Inc. All Rights Reserved.
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
 * appleCdsa.h - interface between SSL and CDSA
 */

#ifndef	_APPLE_CDSA_H_
#define _APPLE_CDSA_H_	1

#include "ssl.h"
#include "sslPriv.h"
#include "sslContext.h"
#include <Security/cssmtype.h>

#ifdef __cplusplus
extern "C" {
#endif

extern OSStatus sslSetUpSymmKey(
	CSSM_KEY_PTR	symKey,
	CSSM_ALGORITHMS	alg,
	CSSM_KEYUSE		keyUse, 		// CSSM_KEYUSE_ENCRYPT, etc.
	CSSM_BOOL		copyKey,		// true: copy keyData   false: set by reference
	uint8 			*keyData,
	size_t		keyDataLen);	// in bytes

extern OSStatus sslFreeKey(CSSM_CSP_HANDLE cspHand,
	CSSM_KEY_PTR 	*key,
	#if		ST_KC_KEYS_NEED_REF
	SecKeychainRef	*kcItem);
	#else	/* !ST_KC_KEYS_NEED_REF */
	void			*kcItem);
	#endif	/* ST_KC_KEYS_NEED_REF*/

extern OSStatus attachToCsp(SSLContext *ctx);
extern OSStatus attachToCl(SSLContext *ctx);
extern OSStatus attachToTp(SSLContext *ctx);
extern OSStatus attachToAll(SSLContext *ctx);
extern OSStatus detachFromAll(SSLContext *ctx);

extern CSSM_DATA_PTR stMallocCssmData(size_t size);
extern void stFreeCssmData(CSSM_DATA_PTR data, CSSM_BOOL freeStruct);
extern OSStatus stSetUpCssmData(CSSM_DATA_PTR data, size_t length);


/*
 * Given a DER-encoded cert, obtain its public key as a CSSM_KEY_PTR.
 */
extern OSStatus sslPubKeyFromCert(
	SSLContext 				*ctx,
	const SSLBuffer			*derCert,
	CSSM_KEY_PTR			*pubKey,		// RETURNED
	CSSM_CSP_HANDLE			*cspHand);		// RETURNED

/*
 * Verify a cert chain.
 */
extern OSStatus sslVerifyCertChain(
	SSLContext				*ctx,
	const SSLCertificate	*certChain,
	bool					arePeerCerts);

/*
 * Raw RSA/DSA sign/verify.
 */
OSStatus sslRawSign(
	SSLContext			*ctx,
	SecKeyRef			privKeyRef,
	const UInt8			*plainText,
	size_t			plainTextLen,
	UInt8				*sig,			// mallocd by caller; RETURNED
	size_t			sigLen,			// available
	size_t			*actualBytes);	// RETURNED

OSStatus sslRawVerify(
	SSLContext			*ctx,
	const CSSM_KEY		*pubKey,
	CSSM_CSP_HANDLE		cspHand,
	const UInt8			*plainText,
	size_t			plainTextLen,
	const UInt8			*sig,
	size_t			sigLen);		// available

/*
 * Encrypt/Decrypt
 */
OSStatus sslRsaEncrypt(
	SSLContext			*ctx,
	const CSSM_KEY		*pubKey,
	CSSM_CSP_HANDLE		cspHand,
	CSSM_PADDING		padding,		// CSSM_PADDING_PKCS1, CSSM_PADDING_APPLE_SSLv2
	const UInt8			*plainText,
	size_t				plainTextLen,
	UInt8				*cipherText,	// mallocd by caller; RETURNED
	size_t				cipherTextLen,	// available
	size_t				*actualBytes);	// RETURNED
OSStatus sslRsaDecrypt(
	SSLContext			*ctx,
	SecKeyRef			privKeyRef,
	CSSM_PADDING		padding,		// CSSM_PADDING_PKCS1, CSSM_PADDING_APPLE_SSLv2
	const UInt8			*cipherText,
	size_t				cipherTextLen,
	UInt8				*plainText,		// mallocd by caller; RETURNED
	size_t				plainTextLen,	// available
	size_t				*actualBytes);	// RETURNED

/*
 * Obtain size of key in bytes.
 */
extern uint32 sslKeyLengthInBytes(
	const CSSM_KEY	*key);

/* Obtain max signature size in bytes. */
extern OSStatus sslGetMaxSigSize(
	const CSSM_KEY	*privKey,
	uint32			*maxSigSize);

/*
 * Get raw key bits from an RSA public key.
 */
OSStatus sslGetPubKeyBits(
	SSLContext			*ctx,
	const CSSM_KEY		*pubKey,
	CSSM_CSP_HANDLE		cspHand,
	SSLBuffer			*modulus,		// data mallocd and RETURNED
	SSLBuffer			*exponent);		// data mallocd and RETURNED

/*
 * Given raw RSA key bits, cook up a CSSM_KEY_PTR. Used in
 * Server-initiated key exchange.
 */
OSStatus sslGetPubKeyFromBits(
	SSLContext			*ctx,
	const SSLBuffer		*modulus,
	const SSLBuffer		*exponent,
	CSSM_KEY_PTR		*pubKey,		// mallocd and RETURNED
	CSSM_CSP_HANDLE		*cspHand);		// RETURNED

/*
 * Given a DER-encoded cert, obtain its DER-encoded subject name.
 */
CSSM_DATA_PTR sslGetCertSubjectName(
	SSLContext			*ctx,
    const CSSM_DATA_PTR cert);

#if		SSL_DEBUG
void verifyTrustedRoots(SSLContext *ctx,
	CSSM_DATA_PTR	certs,
	unsigned		numCerts);
#endif

void * stAppMalloc (size_t size, void *allocRef);
void stAppFree (void *mem_ptr, void *allocRef);
void * stAppRealloc (void *ptr, size_t size, void *allocRef);
void * stAppCalloc (uint32 num, size_t size, void *allocRef);

OSStatus sslDhGenKeyPairClient(
	SSLContext		*ctx,
	const SSLBuffer	*prime,
	const SSLBuffer	*generator,
	CSSM_KEY_PTR	publicKey,			// RETURNED
	CSSM_KEY_PTR	privateKey);		// RETURNED
OSStatus sslDhGenerateKeyPair(
	SSLContext		*ctx,
	const SSLBuffer	*paramBlob,
	uint32			keySizeInBits,
	CSSM_KEY_PTR	publicKey,			// RETURNED
	CSSM_KEY_PTR	privateKey);		// RETURNED
OSStatus sslDhKeyExchange(
	SSLContext		*ctx,
	uint32			deriveSizeInBits,
	SSLBuffer		*exchanged);
OSStatus sslEcdhGenerateKeyPair(
	SSLContext			*ctx,
	SSL_ECDSA_NamedCurve namedCurve);
OSStatus sslEcdhKeyExchange(
	SSLContext		*ctx,
	SSLBuffer		*exchanged);
OSStatus sslVerifySelectedCipher(
	SSLContext 		*ctx,
	const SSLCipherSpec *selectedCipherSpec);

/*
 * Convert between SSLBuffer and CSSM_DATA, which are after all identical.
 * No mallocs, just copy the pointer and length.
 */
#define SSLBUF_TO_CSSM(sb, cd)  {		\
	(cd)->Length = (sb)->length; 		\
	(cd)->Data   = (sb)->data;			\
}

#define CSSM_TO_SSLBUF(cd, sb)  {		\
	(sb)->length = (cd)->Length; 		\
	(sb)->data   = (cd)->Data;			\
}

#ifdef __cplusplus
}
#endif

#endif	/* _APPLE_CDSA_H_ */
