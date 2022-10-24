/*
 * Copyright (c) 2009,2012-2014 Apple Inc. All Rights Reserved.
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

/*!
	@header SecOCSPRequest
	The functions and data types in SecOCSPRequest implement ocsp request
    creation.
*/

#ifndef _SECURITY_SECOCSPREQUEST_H_
#define _SECURITY_SECOCSPREQUEST_H_

#include <CoreFoundation/CFData.h>
#include <libDER/libDER.h>
#include <Security/Security.h>

__BEGIN_DECLS

/*
 CertID          ::=     SEQUENCE {
     hashAlgorithm       AlgorithmIdentifier,
     issuerNameHash      OCTET STRING, -- Hash of Issuer's DN
     issuerKeyHash       OCTET STRING, -- Hash of Issuers public key
     serialNumber        CertificateSerialNumber }
 */
typedef struct {
    DERItem        hashAlgorithm;
    DERItem        issuerNameHash;
    DERItem        issuerKeyHash;
    DERItem        serialNumber;
} DER_OCSPCertID;

extern const DERSize DERNumOCSPCertIDItemSpecs;
extern const DERItemSpec DER_OCSPCertIDItemSpecs[];

/*!
	@typedef SecOCSPRequestRef
	@abstract Object used for ocsp response decoding.
*/
typedef struct __SecOCSPRequest *SecOCSPRequestRef;

struct __SecOCSPRequest {
    SecCertificateRef certificate;
    SecCertificateRef issuer;
    CFDataRef der;
    DERItem certIdHash;
    CFDataRef issuerNameDigest;
    CFDataRef issuerPubKeyDigest;
    CFDataRef serial;
};

/*!
	@function SecOCSPRequestCreate
	@abstract Returns a SecOCSPRequestRef from a BER encoded ocsp response.
	@param certificate The certificate for which we want a OCSP request created.
	@param issuer The parent of certificate.
	@result A SecOCSPRequestRef.
*/
SecOCSPRequestRef SecOCSPRequestCreate(SecCertificateRef certificate,
    SecCertificateRef issuer);

/*!
	@function SecOCSPRequestCopyDER
	@abstract Returns a DER encoded ocsp request.
	@param ocspRequest A SecOCSPRequestRef.
	@result DER encoded ocsp request.
*/
CFDataRef SecOCSPRequestGetDER(SecOCSPRequestRef ocspRequest);

/*!
	@function SecOCSPRequestFinalize
	@abstract Frees a SecOCSPRequestRef.
	@param ocspRequest A SecOCSPRequestRef.
	@note The passed in SecOCSPRequestRef is deallocated
*/
void SecOCSPRequestFinalize(SecOCSPRequestRef ocspRequest);

/* Testing Decoder */
SecOCSPRequestRef SecOCSPRequestCreateWithData(CFDataRef der_ocsp_request);
SecOCSPRequestRef SecOCSPRequestCreateWithDataStrict(CFDataRef der_ocsp_request);

__END_DECLS

#endif /* !_SECURITY_SECOCSPREQUEST_H_ */
