/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */
 
/*
 * p12Crypto.h - PKCS12 Crypto routines.
 *
 * Created 2/28/03 by Doug Mitchell.
 */
 
#ifndef	_PKCS12_CRYPTO_H_
#define _PKCS12_CRYPTO_H_

#include <Security/cssmtype.h>
#include <SecurityNssAsn1/SecNssCoder.h>
#include <Security/Security.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Given appropriate P12-style parameters, cook up a CSSM_KEY.
 */
CSSM_RETURN p12KeyGen(
	CSSM_CSP_HANDLE		cspHand,
	bool				isForEncr,	// true: en/decrypt   false: MAC
	CSSM_KEY			&key,
	CSSM_ALGORITHMS		keyAlg,
	CSSM_ALGORITHMS		pbeHashAlg,	// SHA1, MD5 only
	uint32				keySizeInBits,
	uint32				iterCount,
	CSSM_DATA			&salt,
	const CSSM_DATA		&pwd,		// unicode, double null terminated
	CSSM_DATA			&iv,		// referent is optional
	SecNssCoder			&coder);	// for mallocing KeyData
	
/*
 * Decrypt (typically, an encrypted P7 ContentInfo contents or
 * a P12 ShroudedKeyBag).
 */
CSSM_RETURN p12Decrypt(
	CSSM_CSP_HANDLE		cspHand,
	const CSSM_DATA		&cipherText,
	CSSM_ALGORITHMS		keyAlg,				
	CSSM_ALGORITHMS		encrAlg,
	CSSM_ALGORITHMS		pbeHashAlg,			// SHA1, MD5 only
	uint32				keySizeInBits,
	uint32				blockSizeInBytes,	// for IV
	CSSM_PADDING		padding,			// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	mode,				// CSSM_ALGMODE_CBCPadIV8, etc.
	uint32				iterCount,
	const CSSM_DATA		&salt,
	const CSSM_DATA		&pwd,		// unicode, double null terminated
	SecNssCoder			&coder,		// for mallocing KeyData and plainText
	CSSM_DATA			&plainText);

/*
 * Decrypt (typically, an encrypted P7 ContentInfo contents)
 */
CSSM_RETURN p12Encrypt(
	CSSM_CSP_HANDLE		cspHand,
	const CSSM_DATA		&plainText,
	CSSM_ALGORITHMS		keyAlg,				
	CSSM_ALGORITHMS		encrAlg,
	CSSM_ALGORITHMS		pbeHashAlg,			// SHA1, MD5 only
	uint32				keySizeInBits,
	uint32				blockSizeInBytes,	// for IV
	CSSM_PADDING		padding,			// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	mode,				// CSSM_ALGMODE_CBCPadIV8, etc.
	uint32				iterCount,
	const CSSM_DATA		&salt,
	const CSSM_DATA		&pwd,		// unicode, double null terminated
	SecNssCoder			&coder,		// for mallocing cipherText
	CSSM_DATA			&cipherText);

/*
 * Calculate the MAC for a PFX. Caller is either going compare
 * the result against an existing PFX's MAC or drop the result into 
 * a newly created PFX.
 */
CSSM_RETURN p12GenMac(
	CSSM_CSP_HANDLE		cspHand,
	const CSSM_DATA		&ptext,	// e.g., NSS_P12_DecodedPFX.derAuthSaafe
	CSSM_ALGORITHMS		alg,	// better be SHA1!
	unsigned			iterCount,
	const CSSM_DATA		&salt,
	const CSSM_DATA		&pwd,		// unicode, double null terminated
	SecNssCoder			&coder,		// for mallocing macData
	CSSM_DATA			&macData);	// RETURNED 

/*
 * Unwrap a shrouded key.
 */
CSSM_RETURN p12UnwrapKey(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_DL_DB_HANDLE_PTR	dlDbHand,		// optional
	int					keyIsPermanent,		// nonzero - store in DB
	const CSSM_DATA		&shroudedKeyBits,
	CSSM_ALGORITHMS		keyAlg,				// of the unwrapping key
	CSSM_ALGORITHMS		encrAlg,
	CSSM_ALGORITHMS		pbeHashAlg,			// SHA1, MD5 only
	uint32				keySizeInBits,
	uint32				blockSizeInBytes,	// for IV
	CSSM_PADDING		padding,			// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	mode,				// CSSM_ALGMODE_CBCPadIV8, etc.
	uint32				iterCount,
	const CSSM_DATA		&salt,
	const CSSM_DATA		&pwd,		// unicode, double null terminated
	SecNssCoder			&coder,		// for mallocing privKey
	const CSSM_DATA		&labelData,

	/*
	 * Result: a private key, reference format, optionaly stored
	 * in dlDbHand
	 */
	CSSM_KEY_PTR		&privKey);

CSSM_RETURN p12WrapKey(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_KEY_PTR		privKey,
	const CSSM_ACCESS_CREDENTIALS *privKeyCreds,
	CSSM_ALGORITHMS		keyAlg,				// of the unwrapping key
	CSSM_ALGORITHMS		encrAlg,
	CSSM_ALGORITHMS		pbeHashAlg,			// SHA1, MD5 only
	uint32				keySizeInBits,
	uint32				blockSizeInBytes,	// for IV
	CSSM_PADDING		padding,			// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	mode,				// CSSM_ALGMODE_CBCPadIV8, etc.
	uint32				iterCount,
	const CSSM_DATA		&salt,
	const CSSM_DATA		&pwd,		// unicode, double null terminated
	SecNssCoder			&coder,		// for mallocing keyBits
	CSSM_DATA			&shroudedKeyBits);	// RETURNED

#ifdef __cplusplus
}
#endif

#endif	/* _PKCS12_CRYPTO_H_ */

