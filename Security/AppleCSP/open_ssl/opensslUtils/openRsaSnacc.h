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
 * openRsaSnacc.h - glue between openrsa and SNACC
 */
 
#ifndef	_OPEN_RSA_SNACC_H_
#define _OPEN_RSA_SNACC_H_


#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/dh.h>
#include <Security/cssmtype.h>
#include <Security/cssmdata.h>
#include <Security/asn-incl.h>
#include <Security/sm_vdatypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Convert between SNACC-style BigIntegerStr and openssl-style BIGNUM.
 */
BIGNUM *bigIntStrToBn(
	BigIntegerStr 	&snaccInt);
void bnToBigIntStr(
	BIGNUM 			*bn,
	BigIntegerStr 	&snaccInt);
	

/* estimate size of encoded BigIntegerStr */
unsigned sizeofBigInt(
	BigIntegerStr 	&bigInt);

/*
 * int --> BigIntegerStr
 */
void snaccIntToBigIntegerStr(
	int 			i,
	BigIntegerStr 	&bigInt);

/*
 * Replacements for d2i_RSAPublicKey, etc. 
 */
CSSM_RETURN RSAPublicKeyDecode(
	RSA 			*openKey, 
	void 			*p, 
	size_t			length);
CSSM_RETURN	RSAPublicKeyEncode(
	RSA 			*openKey, 
	CssmOwnedData	&encodedKey);
CSSM_RETURN RSAPrivateKeyDecode(
	RSA 			*openKey, 
	void 			*p, 
	size_t	 		length);
CSSM_RETURN	RSAPrivateKeyEncode(
	RSA 			*openKey, 
	CssmOwnedData	&encodedKey);

CSSM_RETURN generateDigestInfo(
	const void		*messageDigest,
	size_t			digestLen,
	CSSM_ALGORITHMS	digestAlg,		// CSSM_ALGID_SHA1, etc.
	CssmOwnedData	&encodedInfo,
	size_t			maxEncodedSize);

CSSM_RETURN DSAPublicKeyDecode(
	DSA 			*openKey, 
	unsigned char 	*p, 
	unsigned		length);
CSSM_RETURN	DSAPublicKeyEncode(
	DSA 			*openKey, 
	CssmOwnedData	&encodedKey);
CSSM_RETURN DSAPrivateKeyDecode(
	DSA 			*openKey, 
	unsigned char 	*p, 
	unsigned 		length);
CSSM_RETURN	DSAPrivateKeyEncode(
	DSA 			*openKey, 
	CssmOwnedData	&encodedKey);

CSSM_RETURN DSASigEncode(
	DSA_SIG			*openSig,
	CssmOwnedData	&encodedSig);
CSSM_RETURN DSASigDecode(
	DSA_SIG 		*openSig, 
	const void 		*p, 
	unsigned		length);

CSSM_RETURN DHPrivateKeyDecode(
	DH	 			*openKey, 
	unsigned char 	*p, 
	unsigned 		length);
CSSM_RETURN	DHPrivateKeyEncode(
	DH	 			*openKey, 
	CssmOwnedData	&encodedKey);


#ifdef	__cplusplus
}
#endif

#endif	/* _OPEN_RSA_SNACC_H_ */
