/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
	File:		appleCdsa.h

	Contains:	interface between SSL and CDSA

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#ifndef	_APPLE_CDSA_H_
#define _APPLE_CDSA_H_	1

#include "ssl.h"
#include "sslPriv.h"
#include "sslctx.h"
#include "sslerrs.h"
#include <Security/cssmtype.h>

#ifdef __cplusplus
extern "C" {
#endif

#if		SSL_DEBUG
extern void stPrintCdsaError(const char *op, CSSM_RETURN crtn);
extern char *stCssmErrToStr(CSSM_RETURN err);
#else
#define stPrintCdsaError(o, cr)
#endif

extern SSLErr sslSetUpSymmKey(
	CSSM_KEY_PTR	symKey,
	CSSM_ALGORITHMS	alg,
	CSSM_KEYUSE		keyUse, 		// CSSM_KEYUSE_ENCRYPT, etc.
	CSSM_BOOL		copyKey,		// true: copy keyData   false: set by reference
	uint8 			*keyData,
	uint32			keyDataLen);	// in bytes

extern SSLErr sslFreeKey(CSSM_CSP_HANDLE cspHand, 
	CSSM_KEY_PTR 	*key,
	#if		ST_KEYCHAIN_ENABLE && ST_KC_KEYS_NEED_REF
	SecKeychainRef	*kcItem);
	#else	/* !ST_KEYCHAIN_ENABLE */
	void			*kcItem);
	#endif	/* ST_KEYCHAIN_ENABLE*/

extern SSLErr attachToCsp(SSLContext *ctx);
extern SSLErr attachToCl(SSLContext *ctx);
extern SSLErr attachToTp(SSLContext *ctx);
extern SSLErr attachToAll(SSLContext *ctx);
extern SSLErr detachFromAll(SSLContext *ctx);

extern CSSM_DATA_PTR stMallocCssmData(uint32 size);
extern void stFreeCssmData(CSSM_DATA_PTR data, CSSM_BOOL freeStruct);
extern SSLErr stSetUpCssmData(CSSM_DATA_PTR data, uint32 length);


/*
 * Common RNG function; replaces SSLRef's SSLRandomFunc
 */
extern SSLErr sslRand(
	SSLContext *ctx, 
	SSLBuffer *buf);

/*
 * Given a DER-encoded cert, obtain its public key as a CSSM_KEY_PTR.
 */
extern SSLErr sslPubKeyFromCert(
	SSLContext 				*ctx,
	const SSLBuffer			*derCert,
	CSSM_KEY_PTR			*pubKey,		// RETURNED
	CSSM_CSP_HANDLE			*cspHand);		// RETURNED

/*
 * Verify a cert chain.
 */
extern SSLErr sslVerifyCertChain(
	SSLContext				*ctx,
	const SSLCertificate	*certChain); 

/*
 * Raw RSA sign/verify.
 */
SSLErr sslRsaRawSign(
	SSLContext			*ctx,
	const CSSM_KEY		*privKey,
	CSSM_CSP_HANDLE		cspHand,
	const UInt8			*plainText,
	UInt32				plainTextLen,
	UInt8				*sig,			// mallocd by caller; RETURNED
	UInt32				sigLen,			// available
	UInt32				*actualBytes);	// RETURNED
	
SSLErr sslRsaRawVerify(
	SSLContext			*ctx,
	const CSSM_KEY		*pubKey,
	CSSM_CSP_HANDLE		cspHand,
	const UInt8			*plainText,
	UInt32				plainTextLen,
	const UInt8			*sig,
	UInt32				sigLen);		// available
	
/*
 * Encrypt/Decrypt
 */
SSLErr sslRsaEncrypt(
	SSLContext			*ctx,
	const CSSM_KEY		*pubKey,
	CSSM_CSP_HANDLE		cspHand,
	const UInt8			*plainText,
	UInt32				plainTextLen,
	UInt8				*cipherText,		// mallocd by caller; RETURNED 
	UInt32				cipherTextLen,		// available
	UInt32				*actualBytes);		// RETURNED
SSLErr sslRsaDecrypt(
	SSLContext			*ctx,
	const CSSM_KEY		*privKey,
	CSSM_CSP_HANDLE		cspHand,
	const UInt8			*cipherText,
	UInt32				cipherTextLen,		
	UInt8				*plainText,			// mallocd by caller; RETURNED
	UInt32				plainTextLen,		// available
	UInt32				*actualBytes);		// RETURNED

/*
 * Obtain size of key in bytes.
 */
extern UInt32 sslKeyLengthInBytes(
	const CSSM_KEY	*key);

/*
 * Get raw key bits from an RSA public key.
 */
SSLErr sslGetPubKeyBits(
	SSLContext			*ctx,
	const CSSM_KEY		*pubKey,
	CSSM_CSP_HANDLE		cspHand,
	SSLBuffer			*modulus,		// data mallocd and RETURNED
	SSLBuffer			*exponent);		// data mallocd and RETURNED

/*
 * Given raw RSA key bits, cook up a CSSM_KEY_PTR. Used in 
 * Server-initiated key exchange. 
 */
SSLErr sslGetPubKeyFromBits(
	SSLContext			*ctx,
	const SSLBuffer		*modulus,	
	const SSLBuffer		*exponent,	
	CSSM_KEY_PTR		*pubKey,		// mallocd and RETURNED
	CSSM_CSP_HANDLE		*cspHand);		// RETURNED

/*
 * Given two certs, verify subjectCert with issuerCert. Returns 
 * CSSM_TRUE on successful verify.
 * Only special case on error is "subject cert expired", indicated by
 * *subjectExpired returned as CSSM_TRUE.
 */
#if 0
/* no longer needed */
CSSM_BOOL sslVerifyCert(
	SSLContext				*ctx,
	const CSSM_DATA_PTR		subjectCert,
	const CSSM_DATA_PTR		issuerCert,
	CSSM_CSP_HANDLE			cspHand,			// can verify with issuerCert
	CSSM_BOOL				*subjectExpired);	// RETURNED
#endif

/*
 * Given a DER-encoded cert, obtain its DER-encoded subject name.
 */
#if		ST_KEYCHAIN_ENABLE 
CSSM_DATA_PTR sslGetCertSubjectName( 
	SSLContext			*ctx,
    const CSSM_DATA_PTR cert);
#endif		ST_KEYCHAIN_ENABLE 

#if		(SSL_DEBUG && ST_KEYCHAIN_ENABLE)
void verifyTrustedRoots(SSLContext *ctx,
	CSSM_DATA_PTR	certs,
	unsigned		numCerts);
#endif

void * stAppMalloc (uint32 size, void *allocRef);
void stAppFree (void *mem_ptr, void *allocRef);
void * stAppRealloc (void *ptr, uint32 size, void *allocRef);
void * stAppCalloc (uint32 num, uint32 size, void *allocRef);

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
