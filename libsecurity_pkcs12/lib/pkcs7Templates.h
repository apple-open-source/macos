/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
 * pkcs7Templates.h
 */
 
#ifndef	_PKCS7_TEMPLATES_H_
#define _PKCS7_TEMPLATES_H_

#include <Security/secasn1t.h>
#include <Security/cssmtype.h>
#include <Security/x509defs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DigestInfo ::= SEQUENCE {
 * 		digestAlgorithm DigestAlgorithmIdentifier,
 * 		digest Digest 
 * }
 *
 * Digest ::= OCTET STRING
 */
typedef struct {
	CSSM_X509_ALGORITHM_IDENTIFIER	digestAlgorithm;
	CSSM_DATA						digest;
} NSS_P7_DigestInfo;

extern const SecAsn1Template NSS_P7_DigestInfoTemplate[];

/*
 * Uninterpreted ContentInfo, with content stripped from its
 * EXPLICIT CONTEXT_SPECIFIC wrapper
 *
 * ContentInfo ::= SEQUENCE {
 *  	contentType ContentType,
 * 		content [0] EXPLICIT ANY DEFINED BY contentType OPTIONAL 
 * }
 */
typedef struct {
	CSSM_OID	contentType;
	CSSM_DATA	content;
} NSS_P7_RawContentInfo;

extern const SecAsn1Template NSS_P7_RawContentInfoTemplate[];

#pragma mark ---- ContentInfo.content types -----

/*
 * Expand beyond ASN_ANY/CSSM_DATA as needed
 */
typedef CSSM_DATA NSS_P7_SignedData;
typedef CSSM_DATA NSS_P7_EnvelData;
typedef CSSM_DATA NSS_P7_SignEnvelData;
typedef CSSM_DATA NSS_P7_DigestedData;

/* EncryptedData */

/*
 * EncryptedContentInfo ::= SEQUENCE {
 * 		contentType ContentType,
 * 		contentEncryptionAlgorithm
 *   		ContentEncryptionAlgorithmIdentifier,
 * 		encryptedContent
 * 			[0] IMPLICIT EncryptedContent OPTIONAL 
 * }
 *
 * EncryptedContent ::= OCTET STRING
 */

typedef struct {
	CSSM_OID						contentType;
	CSSM_X509_ALGORITHM_IDENTIFIER	encrAlg;
	CSSM_DATA						encrContent;
} NSS_P7_EncrContentInfo;

/*
 * EncryptedData ::= SEQUENCE {
 *  	version Version,
 * 		encryptedContentInfo EncryptedContentInfo 
 * }
 */
typedef struct {
	CSSM_DATA						version;
	NSS_P7_EncrContentInfo 			contentInfo;
} NSS_P7_EncryptedData;

extern const SecAsn1Template NSS_P7_EncrContentInfoTemplate[];
extern const SecAsn1Template NSS_P7_EncryptedDataTemplate[];
extern const SecAsn1Template NSS_P7_PtrToEncryptedDataTemplate[];

/* the stub templates for unimplemented contentTypes */
#define NSS_P7_PtrToSignedDataTemplate		kSecAsn1PointerToAnyTemplate
#define NSS_P7_PtrToEnvelDataTemplate		kSecAsn1PointerToAnyTemplate
#define NSS_P7_PtrToSignEnvelDataTemplate	kSecAsn1PointerToAnyTemplate
#define NSS_P7_PtrToDigestedDataTemplate	kSecAsn1PointerToAnyTemplate

#pragma mark ---- decoded ContentInfo -----

/*
 * For convenience, out dynamic template chooser for ContentInfo.content
 * drops one of these into the decoded struct. Thus, higher level
 * code doesn't have to grunge around comparing OIDs to figure out
 * what's there. 
 */
typedef enum {
	CT_None = 0,
	CT_Data,
	CT_SignedData,
	CT_EnvData,
	CT_SignedEnvData,
	CT_DigestData,
	CT_EncryptedData
} NSS_P7_CI_Type;

/*
 * Decoded ContentInfo. Decoded via SEC_ASN1_DYNAMIC per contentType.
 */
typedef struct {
	CSSM_OID		contentType;
	NSS_P7_CI_Type	type;
	union {
		CSSM_DATA *data;			// CSSMOID_PKCS7_Data
									//   contents of Octet String
		NSS_P7_SignedData *signedData;	
									// CSSMOID_PKCS7_SignedData
		NSS_P7_EnvelData *envData;	// CSSMOID_PKCS7_EnvelopedData
		NSS_P7_SignEnvelData *signEnvelData;	
									// CSSMOID_PKCS7_SignedAndEnvelopedData
		NSS_P7_DigestedData *digestedData;
									// CSSMOID_PKCS7_DigestedData
		NSS_P7_EncryptedData *encryptData;
									//CSSMOID_PKCS7_EncryptedData
									
	} content;
} NSS_P7_DecodedContentInfo;

extern const SecAsn1Template NSS_P7_DecodedContentInfoTemplate[];

#ifdef __cplusplus
}
#endif

#endif	/* _PKCS7_TEMPLATES_H_ */

