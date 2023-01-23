/*
 * Copyright (c) 2002-2022 Apple Inc. All Rights Reserved.
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
#include <Security/SecCertificate.h>
#include <Security/SecCMS.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN

extern const CFStringRef kSecOidCommonName;
extern const CFStringRef kSecOidCountryName;
extern const CFStringRef kSecOidStateProvinceName;
extern const CFStringRef kSecOidLocalityName;
extern const CFStringRef kSecOidOrganization;
extern const CFStringRef kSecOidOrganizationalUnit;

extern const unsigned char SecASN1PrintableString;
extern const unsigned char SecASN1UTF8String;

/*
        Parameter keys for certificate request generation:
        @param kSecCSRChallengePassword CFStringRef
            conversion to PrintableString or UTF8String needs to be possible.
        @param kSecCertificateKeyUsage  CFNumberRef
            with key usage mask using kSecKeyUsage constants.
        @param kSecSubjectAltName       CFDictionaryRef
            with keys defined below.
        @param kSecCSRBasicConstraintsCA   CFBooleanRef
            defaults to true, must not set a path length if this is false.
            Basic constraints will always be marked critical.
        @param kSecCSRBasicContraintsPathLen    CFNumberRef
            if set will include basic constraints and mark it as
            a CA cert.  If 0 <= number < 256, specifies path length, otherwise
            path length will be omitted.  Basic contraints will always be
            marked critical.
        @param kSecCertificateExtendedKeyUsage     CFArrayRef
            an array of all ExtendedKeyUsage (EKU) OIDs to include. EKUs may be
            specified using one of the constants below, or as a CFStringRef
            using the "dot" notation.
        @param kSecCertificateExtensions     CFDictionaryRef
            if set all keys (strings with oids in dotted notation) will be added
            as extensions with accompanying value in binary (CFDataRef) or
            appropriate string (CFStringRef) type (based on used character set).
        @param kSecCertificateExtensionsEncoded     CFDictionaryRef
            if set all keys (strings with oids in dotted notation) will be added
            as extensions with accompanying value.  It is assumed that the value
            is a CFDataRef and is already properly encoded.  This value will be
            placed straight into the extension value OCTET STRING.
         @param kSecCMSSignHashAlgorithm    CFStringRef
             (Declared in SecCMS.h)
             if set, determines the hash algorithm used to create the signing
             request or certificate. If this parameter is omitted, the default
             hash algorithm will be used (SHA1 for RSA and SHA256 for ECDSA).
             Supported digest algorithm strings are defined in
             SecCMS.h, e.g. kSecCMSHashingAlgorithmSHA256;.
        @param kSecCertificateLifetime  CFNumberRef
            Lifetime of certificate in seconds. If unspecified, the lifetime will
            be 365 days. Only applicable to certificate generation
            (SecGenerateSelfSignedCertificate and SecIdentitySignCertificate)
        @param kSecCertificateSerialNumber  CFDataRef
            Data containing the certificate's serial number. This value must be
            at least 1 byte and not more than 20 bytes in length. If unset,
            certificates are created with a default serial number of 0x01.
            Only applicable to certificate generation functions.
*/
extern const CFStringRef kSecCSRChallengePassword;
extern const CFStringRef kSecSubjectAltName;
extern const CFStringRef kSecCertificateKeyUsage;
extern const CFStringRef kSecCSRBasicConstraintsCA;
extern const CFStringRef kSecCSRBasicContraintsPathLen;
extern const CFStringRef kSecCertificateExtendedKeyUsage;
extern const CFStringRef kSecCertificateExtensions;
extern const CFStringRef kSecCertificateExtensionsEncoded;
extern const CFStringRef kSecCertificateLifetime;
extern const CFStringRef kSecCertificateSerialNumber;

/*
        Parameter keys for client identity request (SecRequestClientIdentity):
        @param kSecClientIdentifier     CFStringRef
            if set, specifies an identifier used for this certificate. This
            will normally be the same as the common name of the certificate
            subject, and is considered the entity to which this certificate
            is being issued.
        @param kSecAttestationOids     CFArrayRef
            if set, specifies an array of CFStringRef values which contain OIDs
            for the type of attestation being requested.
        @param kSecLocalIssuerIdentity   SecIdentityRef
            if set, specifies a local identity which is used to issue the
            certificate. This key is ignored if kSecACMEDirectoryURL is present.
        @param kSecUseHardwareBoundKey  CFBooleanRef
            if set with a value of kCFBooleanTrue, will generate a key pair for
            certification whose private key is bound to the secure enclave.
            Note: if the private key should be added to the keychain, you must
            additionally specify kSecAttrIsPermanent with a value of kCFBooleanTrue.
        @param kSecACMEDirectoryURL    CFURLRef
            if set, specifies the initial directory location on an ACME server
            which will be contacted to request a client certificate.
            Note: the ACME server is expected to understand and issue a custom
            challenge type which accepts an attestation from the client.
            if not set, the certificate is issued from kSecLocalIssuerIdentity.
            if both are unset, a self-signed certificate will be generated.
        @param kSecACMEPermitLocalIssuer  CFBooleanRef
            if set with a value of kCFBooleanTrue, and kSecACMEDirectoryURL is
            also set, then local fallback behavior is enabled. This will attempt
            to issue the certificate locally if the ACME server cannot be reached
            or does not return the requested certificate (normally, ACME failures
            are reported as an error.) This key should only be used for test
            purposes; in production, a better approach is to call
            SecRequestClientIdentity again without providing kSecACMEDirectoryURL.
*/
extern const CFStringRef kSecClientIdentifier
    __OSX_AVAILABLE(13.0) __IOS_AVAILABLE(16.0) __TVOS_AVAILABLE(16.0) __WATCHOS_AVAILABLE(9.0);
extern const CFStringRef kSecAttestationOids
    __OSX_AVAILABLE(13.0) __IOS_AVAILABLE(16.0) __TVOS_AVAILABLE(16.0) __WATCHOS_AVAILABLE(9.0);
extern const CFStringRef kSecLocalIssuerIdentity
    __OSX_AVAILABLE(13.0) __IOS_AVAILABLE(16.0) __TVOS_AVAILABLE(16.0) __WATCHOS_AVAILABLE(9.0);
extern const CFStringRef kSecUseHardwareBoundKey
    __OSX_AVAILABLE(13.0) __IOS_AVAILABLE(16.0) __TVOS_AVAILABLE(16.0) __WATCHOS_AVAILABLE(9.0);
extern const CFStringRef kSecACMEDirectoryURL
    __OSX_AVAILABLE(13.0) __IOS_AVAILABLE(16.0) __TVOS_AVAILABLE(16.0) __WATCHOS_AVAILABLE(9.0);
extern const CFStringRef kSecACMEPermitLocalIssuer
    __OSX_AVAILABLE(13.0) __IOS_AVAILABLE(16.0) __TVOS_AVAILABLE(16.0) __WATCHOS_AVAILABLE(9.0);


/* NOTICE: do not use the following constants -- they will be removed! */
extern const CFStringRef kSecAttestationIdentity
    __OSX_AVAILABLE(13.0) __IOS_AVAILABLE(16.0) __TVOS_AVAILABLE(16.0) __WATCHOS_AVAILABLE(9.0);
extern const CFStringRef kSecChallengeToken
    __OSX_AVAILABLE(13.0) __IOS_AVAILABLE(16.0) __TVOS_AVAILABLE(16.0) __WATCHOS_AVAILABLE(9.0);
extern const CFStringRef kSecACMEServerURL
    __OSX_AVAILABLE(13.0) __IOS_AVAILABLE(16.0) __TVOS_AVAILABLE(16.0) __WATCHOS_AVAILABLE(9.0);

/*
 Keys for kSecSubjectAltName dictionaries:
 @param kSecSubjectAltNameDNSName CFArrayRef or CFStringRef
     The value for this key is either a CFStringRef containing a single DNS name,
     or a CFArrayRef of CFStringRefs, each containing a single DNS Name.
 @param kkSecSubjectAltNameEmailAddress CFArrayRef or CFStringRef
     The value for this key is either a CFStringRef containing a single email
     address (RFC 822 Name), or a CFArrayRef of CFStringRefs, each containing a
     single email address.
 @param kSecSubjectAltNameURI CFArrayRef or CFStringRef
     The value for this key is either a CFStringRef containing a single URI,
     or a CFArrayRef of CFStringRefs, each containing a single URI.
 @param kSecSubjectAltNameNTPrincipalName CFStringRef
     The value for this key is a CFStringRef containing the NTPrincipalName.
*/
extern const CFStringRef kSecSubjectAltNameDNSName;
extern const CFStringRef kSecSubjectAltNameEmailAddress;
extern const CFStringRef kSecSubjectAltNameURI;
extern const CFStringRef kSecSubjectAltNameNTPrincipalName;

/* Extended Key Usage OIDs */
extern const CFStringRef kSecEKUServerAuth;
extern const CFStringRef kSecEKUClientAuth;
extern const CFStringRef kSecEKUCodesigning;
extern const CFStringRef kSecEKUEmailProtection;
extern const CFStringRef kSecEKUTimeStamping;
extern const CFStringRef kSecEKUOCSPSigning;

typedef struct {
    CFTypeRef oid;    /* kSecOid constant or CFDataRef with oid */
    unsigned char type; /* currently only SecASN1PrintableString or SecASN1UTF8String */
    CFTypeRef value;    /* CFStringRef -> ASCII, UTF8, CFDataRef -> binary */
} SecATV;

typedef SecATV *SecRDN;

/*
    @function SecGenerateCertificateRequestWithParameters
    @abstract Return a newly generated CSR for subject and keypair.
    @param subject  RDNs in the subject
    @param paramters    Parameters for the CSR generation. See above.
    @param publicKey    Public key
    @param privateKey   Private key
    @result On success, a newly allocated CSR, otherwise NULL

Example for subject:
    SecATV cn[]   = { { kSecOidCommonName, SecASN1PrintableString, CFSTR("test") }, {} };
    SecATV c[]    = { { kSecOidCountryName, SecASN1PrintableString, CFSTR("US") }, {} };
    SecATV o[]    = { { kSecOidOrganization, SecASN1PrintableString, CFSTR("Apple Inc.") }, {} };
    SecRDN atvs[] = { cn, c, o, NULL };
*/
CF_RETURNS_RETAINED _Nullable
CFDataRef SecGenerateCertificateRequestWithParameters(SecRDN _Nonnull * _Nonnull subject,
    CFDictionaryRef _Nullable parameters, SecKeyRef _Nullable publicKey, SecKeyRef privateKey) CF_RETURNS_RETAINED;

/*
 @function SecGenerateCertificateRequest
 @abstract Return a newly generated CSR for subject and keypair.
 @param subject  RDNs in the subject in array format
 @param paramters    Parameters for the CSR generation. See above.
 @param publicKey    Public key
 @param privateKey   Private key
 @result On success, a newly allocated CSR, otherwise NULL
 @discussion The subject array contains an array of the RDNS. Each RDN is
 itself an array of ATVs. Each ATV is an array of length two containing
 first the OID and then the value.

Example for subject (in Objective-C for ease of reading):
 NSArray *subject = @[
     @[@[(__bridge NSString*)kSecOidCommonName, @"test"]]
     @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
     @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc"]],
 ];
 */
CF_RETURNS_RETAINED _Nullable
CFDataRef SecGenerateCertificateRequest(CFArrayRef subject,
    CFDictionaryRef _Nullable parameters, SecKeyRef _Nullable publicKey, SecKeyRef privateKey) CF_RETURNS_RETAINED;

/*
    @function SecVerifyCertificateRequest
    @abstract validate a CSR and return contained information to certify
    @param publicKey (optional/out) SecKeyRef public key to certify
    @param challenge (optional/out) CFStringRef enclosed challenge
    @param subject (optional/out) encoded subject RDNs
    @param extensions (optional/out) encoded extensions
*/
bool SecVerifyCertificateRequest(CFDataRef csr, SecKeyRef CF_RETURNS_RETAINED * _Nullable publicKey,
                                 CFStringRef CF_RETURNS_RETAINED _Nullable * _Nullable challenge,
                                 CFDataRef CF_RETURNS_RETAINED _Nullable * _Nullable subject,
                                 CFDataRef CF_RETURNS_RETAINED _Nullable * _Nullable extensions);


/*
 @function SecGenerateSelfSignedCertificate
 @abstract Return a newly generated certificate for subject and keypair.
 @param subject  RDNs in the subject in array format
 @param parameters    Parameters for the CSR generation. See above.
 @param publicKey    Public key (NOTE: This is unused)
 @param privateKey   Private key
 @result On success, a newly allocated certificate, otherwise NULL
 @discussion The subject array contains an array of the RDNS. Each RDN is
 itself an array of ATVs. Each ATV is an array of length two containing
 first the OID and then the value.

 Example for subject (in Objective-C for ease of reading):
 NSArray *subject = @[
     @[@[(__bridge NSString*)kSecOidCommonName, @"test"]]
     @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
     @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc"]],
 ];
 */
CF_RETURNS_RETAINED _Nullable
SecCertificateRef SecGenerateSelfSignedCertificate(CFArrayRef subject, CFDictionaryRef parameters,
    SecKeyRef _Nullable publicKey, SecKeyRef privateKey);

/*
 @function SecIdentitySignCertificate
 @param issuer      issuer's identity (certificate/private key pair)
 @param serialno    serial number for the issued certificate
 @param publicKey   public key for the issued certificate
 @param subject     subject name for the issued certificate
 @param extensions  extensions for the issued certificate
 @param hashingAlgorithm hash algorithm to use for signature
 @param parameters  parameters dictionary as per above. Extensions specified
 via this parameter override all extensions in the extensions parameter. Extensions
 set via the "parameters" are assumed to be set by the "CA", whereas those in the
 "extensions" are set by the requestor.
 @result On success, a newly allocated certificate, otherwise NULL
 @discussion This call can be used in combination with SecVerifyCertificateRequest
 to generate a signed certifcate from a CSR after verifying it. The outputs
 of SecVerifyCertificateRequest may be passed as inputs to this function.

 The subject may be an array, as specified in SecCertificateGenerateRequest,
 or a data containing an encoded subject sequence, as specified by RFC 5280.

 Supported digest algorithm strings are defined in SecCMS.h, e.g.
 kSecCMSHashingAlgorithmSHA256.
 */
CF_RETURNS_RETAINED _Nullable
SecCertificateRef SecIdentitySignCertificate(SecIdentityRef issuer, CFDataRef serialno,
    SecKeyRef publicKey, CFTypeRef subject, CFTypeRef _Nullable extensions);

CF_RETURNS_RETAINED _Nullable
SecCertificateRef SecIdentitySignCertificateWithAlgorithm(SecIdentityRef issuer, CFDataRef serialno,
    SecKeyRef publicKey, CFTypeRef subject, CFTypeRef _Nullable extensions, CFStringRef _Nullable hashingAlgorithm);

CF_RETURNS_RETAINED _Nullable
SecCertificateRef SecIdentitySignCertificateWithParameters(SecIdentityRef issuer, CFDataRef serialno,
    SecKeyRef publicKey, CFTypeRef subject, CFTypeRef _Nullable extensions, CFDictionaryRef _Nullable parameters);

#ifdef __BLOCKS__
/*!
    @typedef SecRequestIdentityCallback
    @abstract Delivers the result of an asynchronous identity request.
    @param identity On success, a SecIdentityRef for the newly created identity.
    @param error On failure, a CFErrorRef with the error result, or NULL on success.
    @discussion The identity and error parameters will be released automatically after
    the callback block completes execution. They do not need to be explicitly released. If
    your code needs to reference these parameters outside the block's scope, you should
    explicitly retain them, e.g. by assigning to a __block variable.
 */
typedef void (^SecRequestIdentityCallback)(
    SecIdentityRef _Nullable identity,
    CFErrorRef _Nullable error);

/*!
    @function SecRequestClientIdentity
    @abstract Request a new TLS client identity.
    @param subject An array of values for the requested certificate subject.
    Example subject (in Objective-C for ease of reading):
       NSArray *subject = @[
        @[@[(__bridge NSString*)kSecOidCommonName, @"test"]]
        @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
        @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc"]],
     ];
    @param parameters A dictionary containing parameters for key and
    CSR generation, optionally using ACME. See keys and values defined above.
    @param queue A dispatch queue on which the result callback should be
    executed. This parameter cannot be NULL.
    @param result A SecRequestIdentityCallback block which will be executed
    when the identity has been issued. Note that issuance may take some time.
 */
void SecRequestClientIdentity(CFArrayRef subject,
    CFDictionaryRef parameters,
    dispatch_queue_t queue,
    SecRequestIdentityCallback result)
    __OSX_AVAILABLE(13.0) __IOS_AVAILABLE(16.0) __TVOS_AVAILABLE(16.0) __WATCHOS_AVAILABLE(9.0);

#endif /* __BLOCKS__ */

/* PRIVATE */

CF_RETURNS_RETAINED _Nullable
CFDataRef SecGenerateCertificateRequestSubject(SecCertificateRef ca_certificate, CFArrayRef subject);

CF_ASSUME_NONNULL_END

__END_DECLS

#endif /* _SECURITY_SECCERTIFICATEREQUEST_H_ */
