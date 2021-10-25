/*
 * Copyright (c) 2003-2006,2008,2010-2012 Apple Inc. All Rights Reserved.
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
 *
 * X509Templates.h - X.509 Certificate and CRL ASN1 templates
 */

#ifndef	_NSS_X509_TEMPLATES_H_
#define _NSS_X509_TEMPLATES_H_

#include <Security/SecAsn1Types.h>
#include <Security/nameTemplates.h>

/*
 * Arrays of SecAsn1Templates are always associated with a specific
 * C struct. We attempt to use C structs which are defined in CDSA
 * if at all possible; these always start with the CSSM_ prefix.
 * Otherwise we define the struct here, with an NSS_ prefix.
 * In either case, the name of the C struct is listed in comments
 * along with the extern declaration of the SecAsn1Template array.
 */
 
#ifdef  __cplusplus
extern "C" {
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// MARK: --- X509 Validity support ---

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
typedef NSS_TaggedItem	NSS_Time SEC_ASN1_API_DEPRECATED;

extern const SecAsn1Template kSecAsn1TimeTemplate[] SEC_ASN1_API_DEPRECATED;

typedef struct  SEC_ASN1_API_DEPRECATED {
    NSS_Time notBefore;	
    NSS_Time notAfter;		
} NSS_Validity SEC_ASN1_API_DEPRECATED;

extern const SecAsn1Template kSecAsn1ValidityTemplate[] SEC_ASN1_API_DEPRECATED;

// MARK: --- Certificate ---

/*
 * X509 cert extension
 * ASN Class : Extension
 * C struct  : NSS_CertExtension
 *
 * With a nontrivial amount of extension-specific processing,
 * this maps to a CSSM_X509_EXTENSION.
 */
typedef struct SEC_ASN1_API_DEPRECATED {
    SecAsn1Item extnId;
    SecAsn1Item critical;		// optional, default = false
    SecAsn1Item value;		// OCTET string whose decoded value is
							// an id-specific DER-encoded thing
} NSS_CertExtension SEC_ASN1_API_DEPRECATED;

extern const SecAsn1Template kSecAsn1CertExtensionTemplate[] SEC_ASN1_API_DEPRECATED;
extern const SecAsn1Template kSecAsn1SequenceOfCertExtensionTemplate[] SEC_ASN1_API_DEPRECATED;

/*
 * X.509 certificate object (the unsigned form)
 *
 * ASN class : TBSCertificate
 * C struct  : NSS_TBSCertificate
 */
typedef struct SEC_ASN1_API_DEPRECATED {
    SecAsn1Item 							version;			// optional
    SecAsn1Item 							serialNumber;
    SecAsn1AlgId 		signature;
    NSS_Name 							issuer;
    NSS_Validity 						validity;
    NSS_Name 							subject;
    SecAsn1PubKeyInfo 	subjectPublicKeyInfo;
    SecAsn1Item 							issuerID;			// optional, BITS
    SecAsn1Item 							subjectID;			// optional, BITS
    NSS_CertExtension 					**extensions;		// optional
	
	/*
	 * Additional DER-encoded fields copied (via SEC_ASN1_SAVE)
	 * during decoding. 
	 */
	SecAsn1Item							derIssuer;
	SecAsn1Item							derSubject;	
} NSS_TBSCertificate SEC_ASN1_API_DEPRECATED;

extern const SecAsn1Template kSecAsn1TBSCertificateTemplate[] SEC_ASN1_API_DEPRECATED;

/*
 * Fully specified signed certificate.
 *
 * ASN class : Certificate
 * C struct  : NSS_Certificate
 */
typedef struct SEC_ASN1_API_DEPRECATED {
	NSS_TBSCertificate				tbs;
    SecAsn1AlgId 	signatureAlgorithm;
    SecAsn1Item 						signature;// BIT STRING, length in bits	
} NSS_Certificate SEC_ASN1_API_DEPRECATED;

extern const SecAsn1Template kSecAsn1SignedCertTemplate[] SEC_ASN1_API_DEPRECATED;

// MARK: --- CRL ---

/*
 * ASN class : revokedCertificate
 * C struct  : NSS_RevokedCert
 */
typedef struct SEC_ASN1_API_DEPRECATED {
	SecAsn1Item			userCertificate;	// serial number
	NSS_Time			revocationDate;
    NSS_CertExtension 	**extensions;		// optional
} NSS_RevokedCert SEC_ASN1_API_DEPRECATED;

extern const SecAsn1Template kSecAsn1RevokedCertTemplate[] SEC_ASN1_API_DEPRECATED;
extern const SecAsn1Template kSecAsn1SequenceOfRevokedCertTemplate[] SEC_ASN1_API_DEPRECATED;

/*
 * X509 Cert Revocation List (the unsigned form)
 * ASN class : TBSCertList
 * C struct  : NSS_TBSCrl
 */
typedef struct SEC_ASN1_API_DEPRECATED {
    SecAsn1Item 							version;		// optional
    SecAsn1AlgId 		signature;
    NSS_Name 							issuer;
    NSS_Time 							thisUpdate;	
    NSS_Time 							nextUpdate;		// optional
	NSS_RevokedCert						**revokedCerts;	// optional
    NSS_CertExtension 					**extensions;	// optional

	/*
	 * Additional DER-encoded fields copied (via SEC_ASN1_SAVE)
	 * during decoding. 
	 */
	SecAsn1Item							derIssuer;
	
} NSS_TBSCrl SEC_ASN1_API_DEPRECATED;

extern const SecAsn1Template kSecAsn1TBSCrlTemplate[] SEC_ASN1_API_DEPRECATED;

/*
 * Fully specified signed CRL.
 *
 * ASN class : CertificateList
 * C struct  : NSS_CRL
 */
typedef struct SEC_ASN1_API_DEPRECATED {
	NSS_TBSCrl						tbs;
    SecAsn1AlgId 	signatureAlgorithm;
    SecAsn1Item 						signature;// BIT STRING, length in bits	
} NSS_Crl SEC_ASN1_API_DEPRECATED;

extern const SecAsn1Template kSecAsn1SignedCrlTemplate[] SEC_ASN1_API_DEPRECATED;

/* 
 * signed data - top-level view of a signed Cert or CRL, for
 * signing and verifying only. Treats the TBS and AlgId portions 
 * as opaque ASN_ANY blobs.
 */
typedef struct SEC_ASN1_API_DEPRECATED {
    SecAsn1Item 						tbsBlob;  // ANY, DER encoded cert or CRL
    SecAsn1Item 						signatureAlgorithm;
    SecAsn1Item 						signature;// BIT STRING, length in bits	
} NSS_SignedCertOrCRL SEC_ASN1_API_DEPRECATED;

extern const SecAsn1Template kSecAsn1SignedCertOrCRLTemplate[] SEC_ASN1_API_DEPRECATED;

#pragma clang diagnostic pop

#ifdef  __cplusplus
}
#endif

#endif	/* _NSS_X509_TEMPLATES_H_ */
