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
 * X509Templates.h - X.509 Certificate and CRL ASN1 templates
 */

#ifndef	_NSS_X509_TEMPLATES_H_
#define _NSS_X509_TEMPLATES_H_

#include <SecurityNssAsn1/secasn1.h>
#include <SecurityNssAsn1/nameTemplates.h>
#include <Security/x509defs.h>

/*
 * Arrays of SEC_ASN1Templates are always associated with a specific
 * C struct. We attempt to use C structs which are defined in CDSA
 * if at all possible; these always start with the CSSM_ prefix.
 * Otherwise we define the struct here, with an NSS_ prefix.
 * In either case, the name of the C struct is listed in comments
 * along with the extern declaration of the SEC_ASN1Template array.
 */
 
SEC_BEGIN_PROTOS

#pragma mark --- X509 Validity support ---

/* 
 * ASN Class : Validity
 * C struct  : NSS_Validity
 *
 * The low-level Time values, which are CHOICE of generalized
 * time or UTC time, still DER-encoded upon decoding of this object.
 */
/*
 * The low-level time values are eitehr Generalized Time 
 * (SEC_ASN1_GENERALIZED_TIME) or UTC time (SEC_ASN1_UTC_TIME).
 */
typedef NSS_TaggedItem	NSS_Time;

typedef struct  {
    NSS_Time notBefore;	
    NSS_Time notAfter;		
} NSS_Validity;

extern const SEC_ASN1Template NSS_ValidityTemplate[];

#pragma mark --- Certificate ---

/*
 * X509 cert extension
 * ASN Class : Extension
 * C struct  : NSS_CertExtension
 *
 * With a nontrivial amount of extension-specific processing,
 * this maps to a CSSM_X509_EXTENSION.
 */
typedef struct {
    CSSM_DATA extnId;
    CSSM_DATA critical;		// optional, default = false
    CSSM_DATA value;		// OCTET string whose decoded value is
							// an id-specific DER-encoded thing
} NSS_CertExtension;

extern const SEC_ASN1Template NSS_CertExtensionTemplate[];
extern const SEC_ASN1Template NSS_SequenceOfCertExtensionTemplate[];

/*
 * X.509 certificate object (the unsigned form)
 *
 * ASN class : TBSCertificate
 * C struct  : NSS_TBSCertificate
 */
typedef struct  {
    CSSM_DATA 							version;			// optional
    CSSM_DATA 							serialNumber;
    CSSM_X509_ALGORITHM_IDENTIFIER 		signature;
    NSS_Name 							issuer;
    NSS_Validity 						validity;
    NSS_Name 							subject;
    CSSM_X509_SUBJECT_PUBLIC_KEY_INFO 	subjectPublicKeyInfo;
    CSSM_DATA 							issuerID;			// optional, BITS
    CSSM_DATA 							subjectID;			// optional, BITS
    NSS_CertExtension 					**extensions;		// optional
	
	/*
	 * Additional DER-encoded fields copied (via SEC_ASN1_SAVE)
	 * during decoding. 
	 */
	CSSM_DATA							derIssuer;
	CSSM_DATA							derSubject;	
} NSS_TBSCertificate;

extern const SEC_ASN1Template NSS_TBSCertificateTemplate[];

/*
 * Fully specified signed certificate.
 *
 * ASN class : Certificate
 * C struct  : NSS_Certificate
 */
typedef struct {
	NSS_TBSCertificate				tbs;
    CSSM_X509_ALGORITHM_IDENTIFIER 	signatureAlgorithm;
    CSSM_DATA 						signature;// BIT STRING, length in bits	
} NSS_Certificate;

extern const SEC_ASN1Template NSS_SignedCertTemplate[];

#pragma mark --- CRL ---

/*
 * ASN class : revokedCertificate
 * C struct  : NSS_RevokedCert
 */
typedef struct {
	CSSM_DATA			userCertificate;	// serial number
	NSS_Time			revocationDate;
    NSS_CertExtension 	**extensions;		// optional
} NSS_RevokedCert;

extern const SEC_ASN1Template NSS_RevokedCertTemplate[];
extern const SEC_ASN1Template NSS_SequenceOfRevokedCertTemplate[];

/*
 * X509 Cert Revocation List (the unsigned form)
 * ASN class : TBSCertList
 * C struct  : NSS_TBSCrl
 */
typedef struct {
    CSSM_DATA 							version;		// optional
    CSSM_X509_ALGORITHM_IDENTIFIER 		signature;
    NSS_Name 							issuer;
    NSS_Time 							thisUpdate;	
    NSS_Time 							nextUpdate;		// optional
	NSS_RevokedCert						**revokedCerts;	// optional
    NSS_CertExtension 					**extensions;	// optional

	/*
	 * Additional DER-encoded fields copied (via SEC_ASN1_SAVE)
	 * during decoding. 
	 */
	CSSM_DATA							derIssuer;
	
} NSS_TBSCrl;

extern const SEC_ASN1Template NSS_TBSCrlTemplate[];

/*
 * Fully specified signed CRL.
 *
 * ASN class : CertificateList
 * C struct  : NSS_CRL
 */
typedef struct {
	NSS_TBSCrl						tbs;
    CSSM_X509_ALGORITHM_IDENTIFIER 	signatureAlgorithm;
    CSSM_DATA 						signature;// BIT STRING, length in bits	
} NSS_Crl;

extern const SEC_ASN1Template NSS_SignedCrlTemplate[];

/* 
 * signed data - top-level view of a signed Cert or CRL, for
 * signing and verifying only. Treats the TBS and AlgId portions 
 * as opaque ASN_ANY blobs.
 */
typedef struct {
    CSSM_DATA 						tbsBlob;  // ANY, DER encoded cert or CRL
    CSSM_DATA 						signatureAlgorithm;
    CSSM_DATA 						signature;// BIT STRING, length in bits	
} NSS_SignedCertOrCRL;

extern const SEC_ASN1Template NSS_SignedCertOrCRLTemplate[];


SEC_END_PROTOS

#endif	/* _NSS_X509_TEMPLATES_H_ */
