/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
 * SecPkcs8Templates.h - ASN1 templates for private keys in PKCS8 format.  
 */
 
#ifndef _SEC_PKCS8_TEMPLATES_H_
#define _SEC_PKCS8_TEMPLATES_H_

#include <Security/cssmtype.h>
#include <Security/x509defs.h>
#include <Security/secasn1t.h>

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * This one is the AlgorithmID.Parameters field for PKCS5 v1.5.
 * It looks mighty similar to pkcs-12PbeParams except that this 
 * one has a fixed salt size of 8 bytes (not that we enforce that
 * at decode time). 
 */
typedef struct {
	CSSM_DATA		salt;
	CSSM_DATA		iterations;
} impExpPKCS5_PBE_Parameters;

extern const SecAsn1Template impExpPKCS5_PBE_ParametersTemplate[];

/*
 * This is the AlgorithmID.Parameters of the keyDerivationFunc component
 * of a PBES2-params. PKCS v2.0 only. We do not handle the CHOICE salt;
 * only the specified flavor (as an OCTET STRING).
 */
typedef struct {
	CSSM_DATA		salt;
	CSSM_DATA		iterationCount;
	CSSM_DATA		keyLengthInBytes;	// optional
	CSSM_OID		prf;				// optional, default algid-hmacWithSHA1
} impExpPKCS5_PBKDF2_Params;

extern const SecAsn1Template impExpPKCS5_PBKDF2_ParamsTemplate[];

/*
 * AlgorithmID.Parameters for encryptionScheme component of of a PBES2-params.
 * This one for RC2:
 */
typedef struct {
	CSSM_DATA		version;		// optional
	CSSM_DATA		iv;				// 8 bytes
} impExpPKCS5_RC2Params;

extern const SecAsn1Template impExpPKCS5_RC2ParamsTemplate[];

/*
 * This one for RC5.
 */
typedef struct {
	CSSM_DATA		version;			// not optional
	CSSM_DATA		rounds;				// 8..127
	CSSM_DATA		blockSizeInBits;	// 64 | 128
	CSSM_DATA		iv;					// optional, default is all zeroes
} impExpPKCS5_RC5Params;

extern const SecAsn1Template impExpPKCS5_RC5ParamsTemplate[];

/*
 * The top-level AlgID.Parameters for PKCS5 v2.0. 
 * keyDerivationFunc.Parameters is a impExpPKCS5_PBKDF2_Params.
 * encryptionScheme.Parameters depends on the encryption algorithm:
 *
 * DES, 3DES: encryptionScheme.Parameters is an OCTET STRING containing the 
 *            8-byte IV. 
 * RC2: encryptionScheme.Parameters is impExpPKCS5_RC2Params.
 * RC5: encryptionScheme.Parameters is impExpPKCS5_RC5Params.
 */
typedef struct {
	CSSM_X509_ALGORITHM_IDENTIFIER  keyDerivationFunc;
	CSSM_X509_ALGORITHM_IDENTIFIER  encryptionScheme;
} impExpPKCS5_PBES2_Params;

extern const SecAsn1Template impExpPKCS5_PBES2_ParamsTemplate[];

#ifdef  __cplusplus
}
#endif

#endif  /* _SEC_PKCS8_TEMPLATES_H_ */
