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
 * sslCrypto.h - internally implemented handshake layer crypto.
 */

#ifndef	_SSLCRYPTO_H_
#define _SSLCRYPTO_H_	1

#include "tls_handshake_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <corecrypto/ccdh.h>
#include <corecrypto/ccec.h>


int sslDhCreateKey(ccdh_const_gp_t params, ccdh_full_ctx_t *dhKey);
int sslDhExportPub(ccdh_full_ctx_t dhKey, tls_buffer *pubKey);
int sslDhKeyExchange(ccdh_full_ctx_t dhKey, const tls_buffer *dhPeerPublic, tls_buffer *preMasterSecret);

int sslDecodeDhParams(
	ccdh_const_gp_t params,			/* Input - corecrypto object */
	tls_buffer		*prime,			/* Output - wire format */
    tls_buffer		*generator);    /* Output - wire format */

int sslEncodeDhParams(
    ccdh_gp_t           *params,		/* data mallocd and RETURNED - corecrypto object */
    const tls_buffer		*prime,			/* Input - wire format */
    const tls_buffer		*generator);    /* Input - wire format */


int sslEcdhCreateKey(ccec_const_cp_t cp, ccec_full_ctx_t *ecdhKey);
int sslEcdhExportPub(ccec_full_ctx_t ecdhKey, tls_buffer *pubKey);
int sslEcdhKeyExchange(ccec_full_ctx_t ecdhKey, ccec_pub_ctx_t ecdhPeerPublic, tls_buffer *preMasterSecret);


int sslRand(tls_buffer *buf);

#include <corecrypto/ccrsa.h>
#include <corecrypto/ccec.h>


/*
 * Free a pubKey object.
 */
int sslFreePubKey(SSLPubKey *pubKey);

/*
 * Free a privKey object.
 */
int sslFreePrivKey(tls_private_key_t *privKey);

//extern int sslPubKeyGetAlgorithmID(SSLPubKey *pubKey);
//extern int sslPrivKeyGetAlgorithmID(tls_private_key_t privKey);



/*
 * Raw RSA/DSA sign/verify.
 */
int sslRawSign(
	tls_private_key_t   privKey,
	const uint8_t       *plainText,
	size_t              plainTextLen,
	uint8_t				*sig,			// mallocd by caller; RETURNED
	size_t              sigLen,         // available
	size_t              *actualBytes);  // RETURNED

int sslRawVerify(
	SSLPubKey           *pubKey,
	const uint8_t		*plainText,
	size_t              plainTextLen,
	const uint8_t       *sig,
	size_t              sigLen);        // available

/* TLS 1.2 style RSA sign */
int sslRsaSign(
    tls_private_key_t   privKey,
    tls_hash_algorithm   algId,
    const uint8_t       *plainText,
    size_t              plainTextLen,
    uint8_t				*sig,			// mallocd by caller; RETURNED
    size_t              sigLen,         // available
    size_t              *actualBytes);  // RETURNED

/* TLS 1.2 style ECDSA sign */
int sslEcdsaSign(
   tls_private_key_t   privKey,
   const uint8_t       *plainText,
   size_t              plainTextLen,
   uint8_t				*sig,			// mallocd by caller; RETURNED
   size_t              sigLen,         // available
   size_t              *actualBytes);  // RETURNED

/* TLS 1.2 style RSA verify */
int sslRsaVerify(
    SSLPubKey           *pubKey,
    tls_hash_algorithm   algId,
    const uint8_t       *plainText,
    size_t              plainTextLen,
    const uint8_t       *sig,
    size_t              sigLen);         // available

/*
 * Encrypt/Decrypt
 */
int sslRsaEncrypt(
	SSLPubKey           *pubKey,
	const uint8_t       *plainText,
	size_t              plainTextLen,
	uint8_t				*cipherText,		// mallocd by caller; RETURNED
	size_t              cipherTextLen,      // available
	size_t              *actualBytes);      // RETURNED

int sslRsaDecrypt(
	tls_private_key_t   privKey,
	const uint8_t       *cipherText,
	size_t              cipherTextLen,
	uint8_t				*plainText,			// mallocd by caller; RETURNED
	size_t              plainTextLen,		// available
	size_t              *actualBytes);		// RETURNED

/*
 * Obtain size of key in bytes.
 */
extern size_t sslPrivKeyLengthInBytes(
	tls_private_key_t sslKey);

extern size_t sslPubKeyLengthInBytes(
	SSLPubKey *sslKey);

/* Obtain max signature size in bytes. */
extern int sslGetMaxSigSize(
	tls_private_key_t privKey,
	size_t            *maxSigSize);


/*
 * Given raw RSA key bits, cook up a SSLPubKey. Used in
 * Server-initiated key exchange.
 */
int sslGetPubKeyFromBits(
	const tls_buffer		*modulus,
	const tls_buffer		*exponent,
	SSLPubKey           *pubKey);       // RETURNED

/*
 * Given raw ECDSA key bits and curve, cook up a SSLPubKey. Used in
 * Server-initiated key exchange.
 */
int sslGetEcPubKeyFromBits(
    tls_named_curve namedCurve,
    const tls_buffer     *pubKeyBits,
    SSLPubKey           *pubKey);


#ifdef __cplusplus
}
#endif


#endif	/* _SSL_CRYPTO_H_ */
