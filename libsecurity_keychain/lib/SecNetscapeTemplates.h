/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 * SecNetscapeTemplates.h - Structs and templates for DER encoding and 
 *						 decoding of Netscape-style certificate requests 
 *						 and certificate sequences.
 */
 
#ifndef	_SEC_IMPORT_EXPORT_NETSCAPE_TEMPLATES_H_
#define _SEC_IMPORT_EXPORT_NETSCAPE_TEMPLATES_H_

#include <Security/secasn1t.h>
#include <Security/cssmtype.h>
#include <Security/X509Templates.h>
#include <Security/keyTemplates.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * Netscape Certifiate Sequence is defined by Netscape as a PKCS7
 * ContentInfo with a contentType of netscape-cert-sequence and a content
 * consisting of a sequence of certificates.
 *
 * For simplicity - i.e., to avoid the general purpose ContentInfo
 * polymorphism - we'll just hard-code this particular type right here.
 *
 * Inside the ContentInfo is an array of standard X509 certificates.
 * We don't need to parse the certs themselves so they remain as 
 * opaque data blobs. 
 */
typedef struct {
	CSSM_OID		contentType;		// netscape-cert-sequence
	CSSM_DATA		**certs;
} NetscapeCertSequence;

extern const SecAsn1Template NetscapeCertSequenceTemplate[];

/*
 * Public key/challenge, to send to CA.
 *
 * PublicKeyAndChallenge ::= SEQUENCE {
 *
 *   	spki SubjectPublicKeyInfo,
 *   	challenge IA5STRING
 * }
 *
 * SignedPublicKeyAndChallenge ::= SEQUENCE {
 * 		publicKeyAndChallenge PublicKeyAndChallenge,
 *		signatureAlgorithm AlgorithmIdentifier,
 *		signature BIT STRING
 * }
 */
typedef struct {
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO	spki;
	CSSM_DATA							challenge;	// ASCII
} PublicKeyAndChallenge;

typedef struct {
	PublicKeyAndChallenge				pubKeyAndChallenge;
	CSSM_X509_ALGORITHM_IDENTIFIER		algId;
	CSSM_DATA							signature; // length in BITS
} SignedPublicKeyAndChallenge;

extern const SecAsn1Template PublicKeyAndChallengeTemplate[];
extern const SecAsn1Template SignedPublicKeyAndChallengeTemplate[];

#ifdef __cplusplus
}
#endif

#endif	/* _SEC_IMPORT_EXPORT_NETSCAPE_TEMPLATES_H_ */

