/*
 * Copyright (c) 2008-2009,2012-2014 Apple Inc. All Rights Reserved.
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
	@header SecCertificateRequest
*/

#ifndef _SECURITY_SECCERTIFICATEREQUEST_H_
#define _SECURITY_SECCERTIFICATEREQUEST_H_

#include <Security/SecCertificatePriv.h>
#include <Security/SecKey.h>

__BEGIN_DECLS

extern const void * kSecOidCommonName;
extern const void * kSecOidCountryName;
extern const void * kSecOidStateProvinceName;
extern const void * kSecOidLocalityName;
extern const void * kSecOidOrganization;
extern const void * kSecOidOrganizationalUnit;

extern const unsigned char SecASN1PrintableString;
extern const unsigned char SecASN1UTF8String;

/*
        Parameter keys for certificate request generation:
        @param kSecCSRChallengePassword CFStringRef 
            conversion to PrintableString or UTF8String needs to be possible.
        @param kSecCertificateKeyUsage  CFNumberRef 
            with key usage mask using kSecKeyUsage constants.
        @param kSecSubjectAltName       CFArrayRef of CFStringRef or CFDataRef
            either dnsName or emailAddress (if contains @) or
            ipAddress, ipv4 (4) or ipv6 (16) bytes
        @param kSecCSRBasicContraintsPathLen    CFNumberRef 
            if set will include basic constraints and mark it as
            a CA cert.  If 0 <= number < 256, specifies path length, otherwise
            path length will be omitted.  Basic contraints will always be 
            marked critical.
        @param kSecCertificateExtensions     CFDictionaryRef
            if set all keys (strings with oids in dotted notation) will be added
            as extensions with accompanying value in binary (CFDataRef) or
            appropriate string (CFStringRef) type (based on used character set).
        @param kSecCertificateExtensionsEncoded     CFDictionaryRef
            if set all keys (strings with oids in dotted notation) will be added
            as extensions with accompanying value.  It is assumed that the value
            is a CFDataRef and is already properly encoded.  This value will be
            placed straight into the extension value OCTET STRING.
*/
extern const void * kSecCSRChallengePassword;
extern const void * kSecSubjectAltName;
extern const void * kSecCertificateKeyUsage;
extern const void * kSecCSRBasicContraintsPathLen;
extern const void * kSecCertificateExtensions;
extern const void * kSecCertificateExtensionsEncoded;

typedef struct {
    const void *oid;          /* kSecOid constant or CFDataRef with oid */
    unsigned char type; /* currently only SecASN1PrintableString */
    CFTypeRef value;    /* CFStringRef -> ASCII, UTF8, CFDataRef -> binary */
} SecATV;

typedef SecATV *SecRDN;

/*
	@function SecGenerateCertificateRequest
	@abstract Return a newly generated CSR for subject and keypair.
	@param subject  RDNs in the subject
    @param num      Number of RDNs
    @param publicKey    Public key
    @param privateKey   Private key
	@discussion only handles RSA keypairs and uses a SHA-1 PKCS1 signature
    @result On success, a newly allocated CSR, otherwise NULL

Example for subject:
    SecATV cn[]   = { { kSecOidCommonName, SecASN1PrintableString, CFSTR("test") }, {} };
    SecATV c[]    = { { kSecOidCountryName, SecASN1PrintableString, CFSTR("US") }, {} };
    SecATV o[]    = { { kSecOidOrganization, SecASN1PrintableString, CFSTR("Apple Inc.") }, {} };
    SecRDN atvs[] = { cn, c, o, NULL };
*/
CFDataRef SecGenerateCertificateRequestWithParameters(SecRDN *subject, 
    CFDictionaryRef parameters, SecKeyRef publicKey, SecKeyRef privateKey) CF_RETURNS_RETAINED;

CFDataRef SecGenerateCertificateRequest(CFArrayRef subject, 
    CFDictionaryRef parameters, SecKeyRef publicKey, SecKeyRef privateKey) CF_RETURNS_RETAINED;

/*
	@function SecVerifyCertificateRequest
	@abstract validate a CSR and return contained information to certify
	@param publicKey (optional/out) SecKeyRef public key to certify
    @param challenge (optional/out) CFStringRef enclosed challenge
    @param subject (optional/out) encoded subject RDNs
    @param extensions (optional/out) encoded extensions
*/
bool SecVerifyCertificateRequest(CFDataRef csr, SecKeyRef *publicKey,
    CFStringRef *challenge, CFDataRef *subject, CFDataRef *extensions);

SecCertificateRef 
SecGenerateSelfSignedCertificate(CFArrayRef subject, CFDictionaryRef parameters, 
    SecKeyRef publicKey, SecKeyRef privateKey);

SecCertificateRef
SecIdentitySignCertificate(SecIdentityRef issuer, CFDataRef serialno,
    SecKeyRef publicKey, CFTypeRef subject, CFTypeRef extensions);


/* PRIVATE */

CF_RETURNS_RETAINED
CFDataRef
SecGenerateCertificateRequestSubject(SecCertificateRef ca_certificate, CFArrayRef subject);

__END_DECLS

#endif /* _SECURITY_SECCERTIFICATEREQUEST_H_ */
