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
 * csrTemplates.cpp - ASN1 templates Cert Signing Requests (per PKCS10).
 */

#include <secasn1.h>
#include "csrTemplates.h"
#include "keyTemplates.h"

const SEC_ASN1Template NSS_CertRequestInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSCertRequestInfo) },
    { SEC_ASN1_INTEGER,  offsetof(NSSCertRequestInfo,version) },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCertRequestInfo,subject),
	  NSS_NameTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCertRequestInfo,subjectPublicKeyInfo),
	  NSS_SubjectPublicKeyInfoTemplate },
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(NSSCertRequestInfo,attributes),
	  NSS_SetOfAttributeTemplate },
    { 0 }
};

const SEC_ASN1Template NSS_CertRequestTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSCertRequest) },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCertRequest,reqInfo),
	  NSS_CertRequestInfoTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCertRequest,signatureAlgorithm),
	  NSS_AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING, offsetof(NSSCertRequest,signature) },
	{ 0 }
};

const SEC_ASN1Template NSS_SignedCertRequestTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSS_SignedCertRequest) },
    { SEC_ASN1_ANY,
	  offsetof(NSS_SignedCertRequest,certRequestBlob),
	  NSS_CertRequestInfoTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(NSS_SignedCertRequest,signatureAlgorithm),
	  NSS_AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING, offsetof(NSS_SignedCertRequest,signature) },
	{ 0 }
};

