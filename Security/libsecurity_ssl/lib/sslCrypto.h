/*
 * Copyright (c) 2006-2008,2010-2012 Apple Inc. All Rights Reserved.
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
 * sslCrypto.h - interface between SSL and crypto libraries
 */

#ifndef	_SSL_CRYPTO_H_
#define _SSL_CRYPTO_H_	1

#include "ssl.h"
#include "sslContext.h"
#include <Security/SecKeyPriv.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef		NDEBUG
extern void stPrintCdsaError(const char *op, OSStatus crtn);
#else
#define stPrintCdsaError(o, cr)
#endif

/*
 * Free a pubKey object.
 */
extern OSStatus sslFreePubKey(SSLPubKey **pubKey);

/*
 * Free a privKey object.
 */
extern OSStatus sslFreePrivKey(SSLPrivKey **privKey);

extern CFIndex sslPubKeyGetAlgorithmID(SSLPubKey *pubKey);
extern CFIndex sslPrivKeyGetAlgorithmID(SSLPrivKey *privKey);

/*
 * Create a new SecTrust object and return it.
 */
OSStatus
sslCreateSecTrust(
	SSLContext				*ctx,
	CFArrayRef				certChain,
	bool					arePeerCerts,
    SecTrustRef             *trust); 	/* RETURNED */


/*
 * Verify a cert chain.
 */
extern OSStatus sslVerifyCertChain(
	SSLContext				*ctx,
#ifdef USE_SSLCERTIFICATE
	const SSLCertificate	*certChain,
#else /* !USE_SSLCERTIFICATE */
	CFArrayRef				certChain,
#endif /* !USE_SSLCERTIFICATE */
	bool					arePeerCerts);

/*
 * Get the peer's public key from the certificate chain.
 */
extern OSStatus sslCopyPeerPubKey(
	SSLContext 				*ctx,
	SSLPubKey               **pubKey);


/*
 * Raw RSA/DSA sign/verify.
 */
OSStatus sslRawSign(
	SSLContext			*ctx,
	SSLPrivKey          *privKey,
	const uint8_t       *plainText,
	size_t              plainTextLen,
	uint8_t				*sig,			// mallocd by caller; RETURNED
	size_t              sigLen,         // available
	size_t              *actualBytes);  // RETURNED

OSStatus sslRawVerify(
	SSLContext			*ctx,
	SSLPubKey           *pubKey,
	const uint8_t		*plainText,
	size_t              plainTextLen,
	const uint8_t       *sig,
	size_t              sigLen);        // available

/* TLS 1.2 style RSA sign */
OSStatus sslRsaSign(
    SSLContext			*ctx,
    SSLPrivKey          *privKey,
    const SecAsn1AlgId  *algId,
    const uint8_t       *plainText,
    size_t              plainTextLen,
    uint8_t				*sig,			// mallocd by caller; RETURNED
    size_t              sigLen,         // available
    size_t              *actualBytes);  // RETURNED

/* TLS 1.2 style RSA verify */
OSStatus sslRsaVerify(
    SSLContext          *ctx,
    SSLPubKey           *pubKey,
    const SecAsn1AlgId  *algId,
    const uint8_t       *plainText,
    size_t              plainTextLen,
    const uint8_t       *sig,
    size_t              sigLen);         // available

/*
 * Encrypt/Decrypt
 */
OSStatus sslRsaEncrypt(
	SSLContext			*ctx,
	SSLPubKey           *pubKey,
#ifdef USE_CDSA_CRYPTO
	CSSM_CSP_HANDLE		cspHand,
#endif
	const uint32_t		padding,
	const uint8_t       *plainText,
	size_t              plainTextLen,
	uint8_t				*cipherText,		// mallocd by caller; RETURNED
	size_t              cipherTextLen,      // available
	size_t              *actualBytes);      // RETURNED
OSStatus sslRsaDecrypt(
	SSLContext			*ctx,
	SSLPrivKey			*privKey,
	const uint32_t		padding,
	const uint8_t       *cipherText,
	size_t              cipherTextLen,
	uint8_t				*plainText,			// mallocd by caller; RETURNED
	size_t              plainTextLen,		// available
	size_t              *actualBytes);		// RETURNED

/*
 * Obtain size of key in bytes.
 */
extern size_t sslPrivKeyLengthInBytes(
	SSLPrivKey *sslKey);

extern size_t sslPubKeyLengthInBytes(
	SSLPubKey *sslKey);

/* Obtain max signature size in bytes. */
extern OSStatus sslGetMaxSigSize(
	SSLPrivKey *privKey,
	size_t           *maxSigSize);

#if 0
/*
 * Get raw key bits from an RSA public key.
 */
OSStatus sslGetPubKeyBits(
	SSLContext			*ctx,
	SSLPubKey           *pubKey,
	SSLBuffer			*modulus,		// data mallocd and RETURNED
	SSLBuffer			*exponent);		// data mallocd and RETURNED
#endif

/*
 * Given raw RSA key bits, cook up a SSLPubKey. Used in
 * Server-initiated key exchange.
 */
OSStatus sslGetPubKeyFromBits(
	SSLContext			*ctx,
	const SSLBuffer		*modulus,
	const SSLBuffer		*exponent,
	SSLPubKey           **pubKey);       // mallocd and RETURNED

OSStatus sslVerifySelectedCipher(
	SSLContext 		*ctx);

#if APPLE_DH
int sslDhGenerateParams(SSLContext *ctx, uint32_t g, size_t prime_size,
    SSLBuffer *params, SSLBuffer *generator, SSLBuffer *prime);

OSStatus sslDhCreateKey(SSLContext *ctx);
OSStatus sslDhGenerateKeyPair(SSLContext *ctx);
OSStatus sslDhKeyExchange(SSLContext *ctx);

OSStatus sslDecodeDhParams(
	const SSLBuffer	*blob,			/* Input - PKCS-3 encoded */
	SSLBuffer		*prime,			/* Output - wire format */
    SSLBuffer		*generator);    /* Output - wire format */

OSStatus sslEncodeDhParams(
    SSLBuffer           *blob,			/* data mallocd and RETURNED - PKCS-3 encoded */
    const SSLBuffer		*prime,			/* Input - wire format */
    const SSLBuffer		*generator);    /* Input - wire format */

#endif /* APPLE_DH */

/*
 * Given an ECDSA public key in CSSM format, extract the SSL_ECDSA_NamedCurve
 * from its algorithm parameters.
 */
OSStatus sslEcdsaPeerCurve(
	SSLPubKey *pubKey,
	SSL_ECDSA_NamedCurve *namedCurve);
OSStatus sslEcdhGenerateKeyPair(
	SSLContext			*ctx,
	SSL_ECDSA_NamedCurve namedCurve);
OSStatus sslEcdhKeyExchange(
	SSLContext		*ctx,
	SSLBuffer		*exchanged);

#ifdef __cplusplus
}
#endif


#endif	/* _SSL_CRYPTO_H_ */
