/*
 * Copyright (c) 2002-2004,2008-2009,2011-2014,2016 Apple Inc. All Rights Reserved.
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
     SecCertificateRequest implements a way to issue a certificate request to a
     certificate authority.
*/

#ifndef _SECURITY_SECCERTIFICATEREQUEST_H_
#define _SECURITY_SECCERTIFICATEREQUEST_H_

#include <Security/SecBase.h>
#include <Security/SecKey.h>
#include <Security/SecCertificatePriv.h>

#if SEC_OS_OSX
#include <Security/cssmtype.h>
#endif

__BEGIN_DECLS

#if SEC_OS_OSX

struct SecCertificateRequestAttribute /* for optional oids */
{
    CSSM_OID    oid;
    CSSM_DATA   value;
};
typedef struct SecCertificateRequestAttribute SecCertificateRequestAttribute;

struct SecCertificateRequestAttributeList
{
    UInt32 count;
    SecCertificateRequestAttribute *attr;
};
typedef struct SecCertificateRequestAttributeList  SecCertificateRequestAttributeList;

/*!
    @typedef SecCertificateRequestRef
    @abstract Contains information about a certificate request.
*/
typedef struct OpaqueSecCertificateRequestRef *SecCertificateRequestRef;

/*!
    @function SecCertificateRequestGetTypeID
    Returns the type identifier of all SecCertificateRequest instances.
*/
CFTypeID SecCertificateRequestGetTypeID(void);

/*!
    @function SecCertificateRequestCreate

    Create a certificate request operation based on a policy and certificate
    type.  If a policy is not specified, one will be chosen for the caller.
    Once the requeste is created, a request reference is returned.
    To submit the request call SecCertificateRequestSubmit().

    @param policy A policy.
    @param certificateType The certificate type (i.e. X509, PGP, etc).
       These types are in cssmtype.h
    @param requestType The identifier to the type of request to submit (i.e.
       issue, verify, revoke, etc.). These are defined in cssmtype.h
    @param privateKeyItemRef The keychain item private key to be used for this
       certificate request. The private key item must be of class type
       kSecAppleKeyItemClass.
    @param attributeList An optional list of OIDs for the certificate request.
    @param certRequest A returned reference to the certificate request. Call CFRelease when done with this certificate request.
    @result errSecSuccess 0 No error.
*/
OSStatus SecCertificateRequestCreate(
        const CSSM_OID *policy,
        CSSM_CERT_TYPE certificateType,
        CSSM_TP_AUTHORITY_REQUEST_TYPE requestType,
        SecKeyRef privateKeyItemRef,
        SecKeyRef publicKeyItemRef,
        const SecCertificateRequestAttributeList* attributeList,
        SecCertificateRequestRef* certRequest);

/*!
    @function SecCertificateRequestSubmit

    Submit a certificate request to be processed by the Security framework.
    Once the request is submitted, an estimated time is returned indicating
    when the request results can be retrieved. Once the estimated time has
    elapsed, obtain the result by calling SecCertificateRequestGetResult().

    @param certRequest A reference to the certificate request.
    @param estimatedTime The number of estimated seconds before the result
       can be retrieved.
    @result errSecSuccess 0 No error.
*/
OSStatus SecCertificateRequestSubmit(
        SecCertificateRequestRef certRequest,
        sint32* estimatedTime);

/*!
    @function SecCertificateRequestGetType
    Returns the certificate request type (i.e. issue, revoke, etc) for a given
    certificate request item reference.
    @param certRequestRef A reference to a submitted request.
    @param requestType The returned request type.
    @result errSecSuccess 0 No error.
*/
OSStatus SecCertificateRequestGetType(
        SecCertificateRequestRef certRequestRef,
        CSSM_TP_AUTHORITY_REQUEST_TYPE* requestType);

/*!
    @function SecCertificateRequestGetResult
    Get the results of a certificate request. If the request is still
    pending, the estimated time will be returned which indicates when to
    call this function again.
    @param certRequestRef A reference for the submitted request.
    @param keychain The keychain in which to store the new certificate (for
       a new cert request) and the cert request item reference. Pass NULL
       to specify the default keychain.
    @param estimatedTime The number of estimated seconds before the result can
       be retrieved.
    @param certificateRef The returned certificate reference for a
       CSSM_TP_AUTHORITY_REQUEST_CERTISSUE only. All other request types return
       NULL here. Call CFRelease when done with this certificate reference.
    @result errSecSuccess 0 No error.
*/
OSStatus SecCertificateRequestGetResult(
        SecCertificateRequestRef certRequestRef,
        SecKeychainRef keychain,
        sint32* estimatedTime,
        SecCertificateRef* certificateRef);

/*!
    @function SecCertificateFindRequest
    Find a pending certificate request and return a reference object
       for it. The search criteria is based on the input parameters.
    @param policy A policy.
    @param certificateType The certificate type (i.e. X509, PGP, etc).
       These types are in cssmtype.h
    @param requestType The identifier to the type of request to find (i.e.
       issue, verify, revoke, etc.). These are defined in cssmtype.h
    @param privateKeyItemRef Optional private key to be used
       for the certificate request. Matches the same argument as passed to
       SecCertificateRequestCreate().
    @param publicKeyItemRef Optional public key to be used
       for the certificate request. Matches the same argument as passed to
       SecCertificateRequestCreate().
    @param attributeList An optional list of OID/value pairs for finding the
       certificate request.
    @param certRequest A returned reference to the certificate request. Call CFRelease when done with this reference.
*/
OSStatus SecCertificateFindRequest(
        const CSSM_OID *policy,
        CSSM_CERT_TYPE certificateType,
        CSSM_TP_AUTHORITY_REQUEST_TYPE requestType,
        SecKeyRef privateKeyItemRef,
        SecKeyRef publicKeyItemRef,
        const SecCertificateRequestAttributeList* attributeList,
        SecCertificateRequestRef* certRequest);

/*!
    @function SecCertificateRequestGetData
    Get policy-specific data following a SecCertificateRequestSubmit.
    @param certRequestRef A reference for the submitted request.
    @param data Policy-specific data.
    @result errSecSuccess 0 No error.
*/

OSStatus SecCertificateRequestGetData(
    SecCertificateRequestRef    certRequestRef,
    CSSM_DATA                    *data);


#endif

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
    const void *oid;    /* kSecOid constant or CFDataRef with oid */
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
