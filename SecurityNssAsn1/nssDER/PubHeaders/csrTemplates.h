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
 * csrTemplates.h - ASN1 templates Cert Signing Requests (per PKCS10).
 */

#ifndef	_NSS_CSR_TEMPLATES_H_
#define _NSS_CSR_TEMPLATES_H_

#include <SecurityNssAsn1/secasn1.h>
#include <SecurityNssAsn1/X509Templates.h>
#include <SecurityNssAsn1/keyTemplates.h>	/* for NSS_Attribute */
#include <Security/x509defs.h>

SEC_BEGIN_PROTOS

/*
 * ASN class : CertificationRequestInfo
 * C struct  : NSSCertRequestInfo
 */
typedef struct {
	CSSM_DATA							version;
    NSS_Name 							subject;
    CSSM_X509_SUBJECT_PUBLIC_KEY_INFO 	subjectPublicKeyInfo;
	NSS_Attribute						**attributes;
} NSSCertRequestInfo;

extern const SEC_ASN1Template NSS_CertRequestInfoTemplate[];

/* 
 * ASN class : CertificationRequest
 * C struct  : NSSCertRequest
 */
typedef struct {
	NSSCertRequestInfo				reqInfo;
    CSSM_X509_ALGORITHM_IDENTIFIER 	signatureAlgorithm;
    CSSM_DATA 						signature;// BIT STRING, length in bits	
} NSSCertRequest;

extern const SEC_ASN1Template NSS_CertRequestTemplate[];

/*
 * This is what we use use to avoid unnecessary setup and teardown of 
 * a full NSSCertRequest when signing and verifying.
 */
typedef struct {
	CSSM_DATA						certRequestBlob;	// encoded, ASN_ANY
    CSSM_X509_ALGORITHM_IDENTIFIER 	signatureAlgorithm;
    CSSM_DATA 						signature;// BIT STRING, length in bits	
} NSS_SignedCertRequest;

extern const SEC_ASN1Template NSS_SignedCertRequestTemplate[];

SEC_END_PROTOS

#endif	/* _NSS_CSR_TEMPLATES_H_ */