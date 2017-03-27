/*
 * Copyright (c) 2006,2011,2014 Apple Inc. All Rights Reserved.
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

#ifndef	_OPENSSH_CODING_H_
#define _OPENSSH_CODING_H_

#include <openssl/rsa_legacy.h>
#include <openssl/dsa_legacy.h>
#include <Security/cssmtype.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <CoreFoundation/CFData.h>

#ifdef	__cplusplus
extern "C" {
#endif

void appendUint32(
	CFMutableDataRef cfOut,
	uint32_t ui);
uint32_t readUint32(
	const unsigned char *&cp,		// IN/OUT
	unsigned &len);					// IN/OUT 

extern CSSM_RETURN RSAPublicKeyEncodeOpenSSH1(
	RSA 			*openKey, 
	const CssmData	&descData,
	CssmOwnedData	&encodedKey);

extern CSSM_RETURN RSAPublicKeyDecodeOpenSSH1(
	RSA 			*openKey, 
	void 			*p, 
	size_t			length);

extern CSSM_RETURN RSAPrivateKeyEncodeOpenSSH1(
	RSA 			*openKey, 
	const CssmData	&descData,
	CssmOwnedData	&encodedKey);

extern CSSM_RETURN RSAPrivateKeyDecodeOpenSSH1(
	RSA 			*openKey, 
	void 			*p, 
	size_t			length);

extern CSSM_RETURN RSAPublicKeyEncodeOpenSSH2(
	RSA 			*openKey, 
	const CssmData	&descData,
	CssmOwnedData	&encodedKey);

extern CSSM_RETURN RSAPublicKeyDecodeOpenSSH2(
	RSA 			*openKey, 
	void 			*p, 
	size_t			length);

extern CSSM_RETURN DSAPublicKeyEncodeOpenSSH2(
	DSA 			*openKey, 
	const CssmData	&descData,
	CssmOwnedData	&encodedKey);

extern CSSM_RETURN DSAPublicKeyDecodeOpenSSH2(
	DSA 			*openKey, 
	void 			*p, 
	size_t			length);

/* In opensshWrap.cpp */

/* Encode OpenSSHv1 private key, with or without encryption */
extern CSSM_RETURN encodeOpenSSHv1PrivKey(
	RSA					*r,
	const uint8			*comment,		/* optional */
	unsigned			commentLen,
	const uint8			*encryptKey,	/* optional; if present, it's 16 bytes of MD5(password) */
	CFDataRef			*encodedKey);	/* RETURNED */

extern CSSM_RETURN decodeOpenSSHv1PrivKey(
	const unsigned char *encodedKey,
	unsigned			encodedKeyLen,
	RSA					*r,
	const uint8			*decryptKey,	/* optional; if present, it's 16 bytes of MD5(password) */
	uint8				**comment,		/* mallocd and RETURNED */
	unsigned			*commentLen);	/* RETURNED */

#ifdef	__cplusplus
}
#endif

#endif	/* _OPENSSH_CODING_H_ */
