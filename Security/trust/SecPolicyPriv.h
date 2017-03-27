/*
 * Copyright (c) 2003-2016 Apple Inc. All Rights Reserved.
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
 @header SecPolicyPriv
 The functions provided in SecPolicyPriv provide an interface to various
 X.509 certificate trust policies.
*/

#ifndef _SECURITY_SECPOLICYPRIV_H_
#define _SECURITY_SECPOLICYPRIV_H_

#include <Security/SecBase.h>
#include <Security/SecPolicy.h>
#include <Security/SecCertificate.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFString.h>
#include <Availability.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN
CF_IMPLICIT_BRIDGING_ENABLED

/*!
	@enum Policy Constants (Private)
	@discussion Predefined constants used to specify a policy.
	@constant kSecPolicyAppleMobileStore
	@constant kSecPolicyAppleTestMobileStore
	@constant kSecPolicyAppleEscrowService
	@constant kSecPolicyAppleProfileSigner
	@constant kSecPolicyAppleQAProfileSigner
	@constant kSecPolicyAppleServerAuthentication
	@constant kSecPolicyAppleOTAPKISigner
	@constant kSecPolicyAppleTestOTAPKISigner
	@constant kSecPolicyAppleIDValidationRecordSigning
	@constant kSecPolicyAppleSMPEncryption
	@constant kSecPolicyAppleTestSMPEncryption
	@constant kSecPolicyApplePCSEscrowService
	@constant kSecPolicyApplePPQSigning
	@constant kSecPolicyAppleTestPPQSigning
	@constant kSecPolicyAppleSWUpdateSigning
	@constant kSecPolicyApplePackageSigning
	@constant kSecPolicyAppleOSXProvisioningProfileSigning
	@constant kSecPolicyAppleATVVPNProfileSigning
	@constant kSecPolicyAppleAST2DiagnosticsServerAuth
	@constant kSecPolicyAppleEscrowProxyServerAuth
	@constant kSecPolicyAppleFMiPServerAuth
	@constant kSecPolicyAppleMMCService
	@constant kSecPolicyAppleGSService
	@constant kSecPolicyApplePPQService
	@constant kSecPolicyAppleHomeKitServerAuth
	@constant kSecPolicyAppleiPhoneActivation
	@constant kSecPolicyAppleiPhoneDeviceCertificate
	@constant kSecPolicyAppleFactoryDeviceCertificate
	@constant kSecPolicyAppleiAP
	@constant kSecPolicyAppleiTunesStoreURLBag
	@constant kSecPolicyAppleiPhoneApplicationSigning
	@constant kSecPolicyAppleiPhoneProfileApplicationSigning
	@constant kSecPolicyAppleiPhoneProvisioningProfileSigning
	@constant kSecPolicyAppleLockdownPairing
	@constant kSecPolicyAppleURLBag
	@constant kSecPolicyAppleOTATasking
	@constant kSecPolicyAppleMobileAsset
	@constant kSecPolicyAppleIDAuthority
	@constant kSecPolicyAppleGenericApplePinned
	@constant kSecPolicyAppleGenericAppleSSLPinned
	@constant kSecPolicyAppleSoftwareSigning
	@constant kSecPolicyAppleExternalDeveloper
	@constant kSecPolicyAppleOCSPSigner
	@constant kSecPolicyAppleIDSService
	@constant kSecPolicyAppleIDSServiceContext
	@constant kSecPolicyApplePushService
	@constant kSecPolicyAppleLegacyPushService
	@constant kSecPolicyAppleTVOSApplicationSigning
	@constant kSecPolicyAppleUniqueDeviceIdentifierCertificate
	@constant kSecPolicyAppleEscrowProxyCompatibilityServerAuth
	@constant kSecPolicyAppleMMCSCompatibilityServerAuth
	@constant kSecPolicyAppleSecureIOStaticAsset
	@constant kSecPolicyAppleWarsaw
	@constant kSecPolicyAppleiCloudSetupServerAuth
	@constant kSecPolicyAppleiCloudSetupCompatibilityServerAuth
 */
extern const CFStringRef kSecPolicyAppleMobileStore
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern const CFStringRef kSecPolicyAppleTestMobileStore
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern const CFStringRef kSecPolicyAppleEscrowService
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern const CFStringRef kSecPolicyAppleProfileSigner
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern const CFStringRef kSecPolicyAppleQAProfileSigner
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern const CFStringRef kSecPolicyAppleServerAuthentication
    __OSX_AVAILABLE_STARTING(__MAC_10_10, __IPHONE_8_0);
extern const CFStringRef kSecPolicyAppleOTAPKISigner
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_7_0);
extern const CFStringRef kSecPolicyAppleTestOTAPKISigner
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_7_0);
extern const CFStringRef kSecPolicyAppleIDValidationRecordSigningPolicy
    __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_NA, __MAC_NA, __IPHONE_7_0, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleIDValidationRecordSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleSMPEncryption
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_8_0);
extern const CFStringRef kSecPolicyAppleTestSMPEncryption
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_8_0);
extern const CFStringRef kSecPolicyApplePCSEscrowService
    __OSX_AVAILABLE_STARTING(__MAC_10_10, __IPHONE_7_0);
extern const CFStringRef kSecPolicyApplePPQSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyAppleTestPPQSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyAppleSWUpdateSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyApplePackageSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyAppleOSXProvisioningProfileSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyAppleATVVPNProfileSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyAppleAST2DiagnosticsServerAuth
    __OSX_AVAILABLE_STARTING(__MAC_10_11_4, __IPHONE_9_3);
extern const CFStringRef kSecPolicyAppleEscrowProxyServerAuth
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleFMiPServerAuth
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleMMCService
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleGSService
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyApplePPQService
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleHomeKitServerAuth
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleiPhoneActivation
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleiPhoneDeviceCertificate
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleFactoryDeviceCertificate
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleiAP
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleiTunesStoreURLBag
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleiPhoneApplicationSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleiPhoneProfileApplicationSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleiPhoneProvisioningProfileSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleLockdownPairing
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleURLBag
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleOTATasking
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleMobileAsset
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleIDAuthority
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleGenericApplePinned
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleGenericAppleSSLPinned
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleSoftwareSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleExternalDeveloper
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleOCSPSigner
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleIDSService
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleIDSServiceContext
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyApplePushService
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleLegacyPushService
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleTVOSApplicationSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleUniqueDeviceIdentifierCertificate
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleEscrowProxyCompatibilityServerAuth
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleMMCSCompatibilityServerAuth
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyAppleSecureIOStaticAsset
    __OSX_AVAILABLE(10.12.1) __IOS_AVAILABLE(10.1) __TVOS_AVAILABLE(10.0.1) __WATCHOS_AVAILABLE(3.1);
extern const CFStringRef kSecPolicyAppleWarsaw
    __OSX_AVAILABLE(10.12.1) __IOS_AVAILABLE(10.1) __TVOS_AVAILABLE(10.0.1) __WATCHOS_AVAILABLE(3.1);
extern const CFStringRef kSecPolicyAppleiCloudSetupServerAuth
    __OSX_AVAILABLE(10.12.4) __IOS_AVAILABLE(10.3) __TVOS_AVAILABLE(10.2) __WATCHOS_AVAILABLE(3.2);
extern const CFStringRef kSecPolicyAppleiCloudSetupCompatibilityServerAuth
    __OSX_AVAILABLE(10.12.4) __IOS_AVAILABLE(10.3) __TVOS_AVAILABLE(10.2) __WATCHOS_AVAILABLE(3.2);



/*!
 @enum Policy Value Constants
 @abstract Predefined property key constants used to get or set values in
 a dictionary for a policy instance.
 @discussion
    All policies will have the following read-only value:
        kSecPolicyOid       (the policy object identifier)

    Additional policy values which your code can optionally set:
        kSecPolicyName              (name which must be matched)
        kSecPolicyClient            (evaluate for client, rather than server)
        kSecPolicyRevocationFlags   (only valid for a revocation policy)
        kSecPolicyRevocationFlags   (only valid for a revocation policy)
        kSecPolicyTeamIdentifier    (only valid for a Passbook signing policy)
        kSecPolicyContext           (valid for policies below that take a context parameter)
        kSecPolicyPolicyName        (only valid for GenericApplePinned or
                                    GenericAppleSSLPinned policies)
        kSecPolicyIntermediateMarkerOid (only valid for GenericApplePinned or
                                        GenericAppleSSLPinned policies)
        kSecPolicyLeafMarkerOid     (only valid for GenericApplePinned or
                                    GenericAppleSSLPinned policies)
        kSecPolicyRootDigest        (only valid for the UniqueDeviceCertificate policy)

 @constant kSecPolicyContext Specifies a CFDictionaryRef with keys and values
    specified by the particular SecPolicyCreate function.
 @constant kSecPolicyPolicyName Specifies a CFStringRef of the name of the
    desired policy result.
 @constant kSecPolicyIntermediateMarkerOid Specifies a CFStringRef of the
    marker OID (in decimal format) required in the intermediate certificate.
 @constant kSecPolicyLeafMarkerOid Specifies a CFStringRef of the
    marker OID (in decimal format) required in the leaf certificate.
 @constant kSecPolicyRootDigest Specifies a CFDataRef of digest required to
    match the SHA-256 of the root certificate.
 */
extern const CFStringRef kSecPolicyContext
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyPolicyName
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyIntermediateMarkerOid
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyLeafMarkerOid
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecPolicyRootDigest
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

/*!
 @enum Revocation Policy Constants
 @abstract Predefined constants which allow you to specify how revocation
 checking will be performed for a trust evaluation.
 @constant kSecRevocationOnlineCheck If this flag is set, perform an online
 revocation check, ignoring cached revocation results. This flag will not force
 an online check if an online check was done within the last 5 minutes. Online
 checks are only applicable to OCSP; this constant will not force a fresh
 CRL download.
 */
extern const CFOptionFlags kSecRevocationOnlineCheck;

/*!
 @function SecPolicyCreateApplePinned
 @abstract Returns a policy object for verifying Apple certificates.
 @param policyName A string that identifies the policy name.
 @param intermediateMarkerOID A string containing the decimal representation of the
 extension OID in the intermediate certificate.
 @param leafMarkerOID A string containing the decimal representation of the extension OID
 in the leaf certificate.
 @discussion The resulting policy uses the Basic X.509 policy with validity check and
 pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if the value true is set for the key
    "ApplePinningAllowTestCerts%@" (where %@ is the policyName parameter) in the
    com.apple.security preferences for the user of the calling application.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID matching the intermediateMarkerOID
    parameter.
    * The leaf has a marker extension with OID matching the leafMarkerOID parameter.
    * Revocation is checked via any available method.
    * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePinned(CFStringRef policyName,
                                        CFStringRef intermediateMarkerOID, CFStringRef leafMarkerOID)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecPolicyCreateAppleSSLPinned
 @abstract Returns a policy object for verifying Apple SSL certificates.
 @param policyName A string that identifies the service/policy name.
 @param hostname hostname to verify the certificate name against.
 @param intermediateMarkerOID A string containing the decimal representation of the
 extension OID in the intermediate certificate. If NULL is passed, the default OID of
 1.2.840.113635.100.6.2.12 is checked.
 @param leafMarkerOID A string containing the decimal representation of the extension OID
 in the leaf certificate.
 @discussion The resulting policy uses the Basic X.509 policy with validity check and
 pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if the value true is set for the key
    "ApplePinningAllowTestCerts%@" (where %@ is the policyName parameter) in the
    com.apple.security preferences for the user of the calling application.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID matching the intermediateMarkerOID
    parameter, or 1.2.840.113635.100.6.2.12 if NULL is passed.
    * The leaf has a marker extension with OID matching the leafMarkerOID parameter.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
    extension or Common Name.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
    * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleSSLPinned(CFStringRef policyName, CFStringRef hostname,
                                           CFStringRef __nullable intermediateMarkerOID, CFStringRef leafMarkerOID)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecPolicyCreateiPhoneActivation
 @abstract Returns a policy object for verifying iPhone Activation
 certificate chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * There are exactly 3 certs in chain.
    * The intermediate has Common Name "Apple iPhone Certification Authority".
    * The leaf has Common Name "iPhone Activation".
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiPhoneActivation(void);

/*!
 @function SecPolicyCreateiPhoneDeviceCertificate
 @abstract Returns a policy object for verifying iPhone Device certificate
 chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
     the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
     * There are exactly 4 certs in chain.
     * The first intermediate has Common Name "Apple iPhone Device CA".
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiPhoneDeviceCertificate(void);

/*!
 @function SecPolicyCreateFactoryDeviceCertificate
 @abstract Returns a policy object for verifying Factory Device certificate
 chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to the Factory Device CA.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateFactoryDeviceCertificate(void);

/*!
 @function SecPolicyCreateiAP
 @abstract Returns a policy object for verifying iAP certificate chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The leaf has notBefore date after 5/31/2006 midnight GMT.
     * The leaf has Common Name beginning with "IPA_".
 The intended use of this policy is that the caller pass in the
 intermediates for iAP1 and iAP2 to SecTrustSetAnchorCertificates().
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiAP(void);

/*!
 @function SecPolicyCreateiTunesStoreURLBag
 @abstract Returns a policy object for verifying iTunes Store URL bag
 certificates.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to the iTMS CA.
     * There are exactly 2 certs in the chain.
     * The leaf has Organization "Apple Inc.".
     * The leaf has Common Name "iTunes Store URL Bag".
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiTunesStoreURLBag(void);

/*!
 @function SecPolicyCreateEAP
 @abstract Returns a policy object for verifying for 802.1x/EAP certificates.
 @param server Passing true for this parameter create a policy for EAP
 server certificates.
 @param trustedServerNames Optional; if present, the hostname in the leaf
 certificate must be in the trustedServerNames list.  Note that contrary
 to all other policies the trustedServerNames list entries can have wildcards
 whilst the certificate cannot.  This matches the existing deployments.
 @discussion This policy uses the Basic X.509 policy with validity check but
 disallowing network fetching. If trustedServerNames param is non-null, the
 ExtendedKeyUsage extension, if present, of the leaf certificate is verified
 to contain either the ServerAuth OID, if the server param is true or
 ClientAuth OID, otherwise.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateEAP(Boolean server, CFArrayRef __nullable trustedServerNames);

/*!
 @function SecPolicyCreateIPSec
 @abstract Returns a policy object for evaluating IPSec certificate chains.
 @param server Passing true for this parameter create a policy for IPSec
 server certificates.
 @param hostname Optional; if present, the policy will require the specified
 hostname or ip address to match the hostname in the leaf certificate.
 @discussion This policy uses the Basic X.509 policy with validity check.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateIPSec(Boolean server, CFStringRef __nullable  hostname);

/*!
 @function SecPolicyCreateAppleSWUpdateSigning
 @abstract Returns a policy object for evaluating SW update signing certs.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
     the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
     * There are exactly 3 certs in the chain.
     * The intermediate ExtendedKeyUsage Extension contains 1.2.840.113635.100.4.1.
     * The leaf ExtendedKeyUsage extension contains 1.2.840.113635.100.4.1.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleSWUpdateSigning(void);

/*!
 @function SecPolicyCreateApplePackageSigning
 @abstract Returns a policy object for evaluating installer package signing certs.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
     the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
     * There are exactly 3 certs in the chain.
     * The leaf KeyUsage extension has the digital signature bit set.
     * The leaf ExtendedKeyUsage extension has the CodeSigning OID.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePackageSigning(void);

/*!
 @function SecPolicyCreateiPhoneApplicationSigning
 @abstract Returns a policy object for evaluating signed application
 signatures.  This is for apps signed directly by the app store.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
     the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
     * There are exactly 3 certs in the chain.
     * The intermediate has Common Name "Apple iPhone Certification Authority".
     * The leaf has Common Name "Apple iPhone OS Application Signing".
     * The leaf has a marker extension with OID 1.2.840.113635.100.6.1.3 or OID
     1.2.840.113635.100.6.1.6.
     * The leaf has ExtendedKeyUsage, if any, with the AnyExtendedKeyUsage OID
       or the CodeSigning OID.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiPhoneApplicationSigning(void);

/*!
 @function SecPolicyCreateiPhoneProfileApplicationSigning
 @abstract Returns a policy object for evaluating signed application
 signatures. This policy is for certificates inside a UPP or regular
 profile.
 @discussion  This policy only verifies that the leaf is temporally valid
 and not revoked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiPhoneProfileApplicationSigning(void);

/*!
 @function SecPolicyCreateiPhoneProvisioningProfileSigning
 @abstract Returns a policy object for evaluating provisioning profile signatures.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
     the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
     * There are exactly 3 certs in the chain.
     * The intermediate has Common Name "Apple iPhone Certification Authority".
     * The leaf has Common Name "Apple iPhone OS Provisioning Profile Signing".
     * If the device is not a production device and is running an internal
       release, the leaf may have the Common Name "TEST Apple iPhone OS
       Provisioning Profile Signing TEST".
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateiPhoneProvisioningProfileSigning(void);

/*!
 @function SecPolicyCreateAppleTVOSApplicationSigning
 @abstract Returns a policy object for evaluating signed application
 signatures.  This is for apps signed directly by the Apple TV app store,
 and allows for both the prod and the dev/test certs.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
     * The chain is anchored to any of the production Apple Root CAs.
       Test roots are never permitted.
     * There are exactly 3 certs in the chain.
     * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.1.
     * The leaf has ExtendedKeyUsage, if any, with the AnyExtendedKeyUsage OID or
       the CodeSigning OID.
     * The leaf has a marker extension with OID 1.2.840.113635.100.6.1.24 or OID
       1.2.840.113635.100.6.1.24.1.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleTVOSApplicationSigning(void);

/*!
 @function SecPolicyCreateOCSPSigner
 @abstract Returns a policy object for evaluating ocsp response signers.
 @discussion This policy uses the Basic X.509 policy with validity check and
 requires the leaf to have an ExtendedKeyUsage of OCSPSigning.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateOCSPSigner(void);


enum {
    kSecSignSMIMEUsage = (1 << 0),
    kSecKeyEncryptSMIMEUsage = (1 << 1),
    kSecDataEncryptSMIMEUsage = (1 << 2),
    kSecKeyExchangeDecryptSMIMEUsage = (1 << 3),
    kSecKeyExchangeEncryptSMIMEUsage = (1 << 4),
    kSecKeyExchangeBothSMIMEUsage = (1 << 5),
    kSecAnyEncryptSMIME = kSecKeyEncryptSMIMEUsage | kSecDataEncryptSMIMEUsage |
        kSecKeyExchangeDecryptSMIMEUsage | kSecKeyExchangeEncryptSMIMEUsage
};

/*!
 @function SecPolicyCreateSMIME
 @abstract Returns a policy object for evaluating S/MIME certificate chains.
 @param smimeUsage Pass the bitwise or of one or more kSecXXXSMIMEUsage
 flags, to indicate the intended usage of this certificate.
 @param email Optional; if present, the policy will require the specified
 email to match the email in the leaf certificate.
 @discussion This policy uses the Basic X.509 policy with validity check and
 requires the leaf to have
     * a KeyUsage matching the smimeUsage,
     * an ExtendedKeyUsage, if any, with the AnyExtendedKeyUsage OID or the
       EmailProtection OID, and
     * if the email param is specified, the email address in the RFC822Name in the
       SubjectAlternativeName extension or in the Email Address field of the
       Subject Name.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateSMIME(CFIndex smimeUsage, CFStringRef __nullable email);

/*!
 @function SecPolicyCreateCodeSigning
 @abstract Returns a policy object for evaluating code signing certificate chains.
 @discussion This policy uses the Basic X.509 policy with validity check and
 requires the leaf to have
     * a KeyUsage with both the DigitalSignature and NonRepudiation bits set, and
     * an ExtendedKeyUsage with the AnyExtendedKeyUsage OID or the CodeSigning OID.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateCodeSigning(void);

/*!
 @function SecPolicyCreateLockdownPairing
 @abstract basic x509 policy for checking lockdown pairing certificate chains.
 @disucssion This policy checks some of the Basic X.509 policy options with no
 validity check. It explicitly allows for empty subjects.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateLockdownPairing(void);

/*!
 @function SecPolicyCreateURLBag
 @abstract Returns a policy object for evaluating certificate chains for signing URL bags.
 @discussion This policy uses the Basic X.509 policy with no validity check and requires
 that the leaf has ExtendedKeyUsage extension with the CodeSigning OID.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateURLBag(void);

/*!
 @function SecPolicyCreateOTATasking
 @abstract  Returns a policy object for evaluating certificate chains for signing OTA Tasking.
 @discussion This policy uses the Basic X.509 policy with validity check and
 pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * There are exactly 3 certs in the chain.
    * The intermediate has Common Name "Apple iPhone Certification Authority".
    * The leaf has Common Name "OTA Task Signing".
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateOTATasking(void);

/*!
 @function SecPolicyCreateMobileAsset
 @abstract  Returns a policy object for evaluating certificate chains for signing Mobile Assets.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * There are exactly 3 certs in the chain.
    * The intermediate has Common Name "Apple iPhone Certification Authority".
    * The leaf has Common Name "Asset Manifest Signing".
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateMobileAsset(void);

/*!
 @function SecPolicyCreateAppleIDAuthorityPolicy
 @abstract Returns a policy object for evaluating certificate chains for Apple ID Authority.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * The intermediate(s) has(have) a marker extension with OID 1.2.840.113635.100.6.2.3
      or OID 1.2.840.113635.100.6.2.7.
    * The leaf has a marker extension with OID 1.2.840.113635.100.4.7.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleIDAuthorityPolicy(void);

/*!
 @function SecPolicyCreateMacAppStoreReceipt
 @abstract Returns a policy object for evaluating certificate chains for signing
 Mac App Store Receipts.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.1.
    * The leaf has CertificatePolicy extension with OID 1.2.840.113635.100.5.6.1.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.11.1.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateMacAppStoreReceipt(void);

/*!
 @function SecPolicyCreatePassbookCardSigner
 @abstract Returns a policy object for evaluating certificate chains for signing Passbook cards.
 @param cardIssuer Required; must match name in marker extension.
 @param teamIdentifier Optional; if present, the policy will require the specified
 team ID to match the organizationalUnit field in the leaf certificate's subject.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.1.16 and containing the
      cardIssuer.
    * The leaf has ExtendedKeyUsage with OID 1.2.840.113635.100.4.14.
    * The leaf has a Organizational Unit matching the TeamID.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreatePassbookCardSigner(CFStringRef cardIssuer,
	CFStringRef __nullable teamIdentifier);

/*!
 @function SecPolicyCreateMobileStoreSigner
 @abstract Returns a policy object for evaluating Mobile Store certificate chains.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * There are exactly 3 certs in the chain.
    * The intermediate has Common Name "Apple System Integration 2 Certification Authority".
    * The leaf has KeyUsage with the DigitalSignature bit set.
    * The leaf has CertificatePolicy extension with OID 1.2.840.113635.100.5.12.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateMobileStoreSigner(void);

/*!
 @function SecPolicyCreateTestMobileStoreSigner
 @abstract  Returns a policy object for evaluating Test Mobile Store certificate chains.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * There are exactly 3 certs in the chain.
    * The intermediate has Common Name "Apple System Integration 2 Certification Authority".
    * The leaf has KeyUsage with the DigitalSignature bit set.
    * The leaf has CertificatePolicy extension with OID 1.2.840.113635.100.5.12.1.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateTestMobileStoreSigner(void);

/*!
 @function SecPolicyCreateEscrowServiceSigner
 @abstract Returns a policy object for evaluating Escrow Service certificate chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to the current Escrow Roots in the OTAPKI asset.
    * There are exactly 2 certs in the chain.
    * The leaf has KeyUsage with the KeyEncipherment bit set.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateEscrowServiceSigner(void);

/*!
 @function SecPolicyCreatePCSEscrowServiceSigner
 @abstract Returns a policy object for evaluating PCS Escrow Service certificate chains.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to the current PCS Escrow Roots in the OTAPKI asset.
    * There are exactly 2 certs in the chain.
    * The leaf has KeyUsage with the KeyEncipherment bit set.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreatePCSEscrowServiceSigner(void);

/*!
 @function SecPolicyCreateOSXProvisioningProfileSigning
 @abstract  Returns a policy object for evaluating certificate chains for signing OS X
 Provisioning Profiles.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.1.
    * The leaf has KeyUsage with the DigitalSignature bit set.
    * The leaf has a marker extension with OID 1.2.840.113635.100.4.11.
    * Revocation is checked via OCSP.
 @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateOSXProvisioningProfileSigning(void);

/*!
 @function SecPolicyCreateConfigurationProfileSigner
 @abstract Returns a policy object for evaluating certificate chains for signing
 Configuration Profiles.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.3.
    * The leaf has ExtendedKeyUsage with OID 1.2.840.113635.100.4.16.
 @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateConfigurationProfileSigner(void);

/*!
 @function SecPolicyCreateQAConfigurationProfileSigner
 @abstract Returns a policy object for evaluating certificate chains for signing
 QA Configuration Profiles. On customer builds, this function returns the same
 policy as SecPolicyCreateConfigurationProfileSigner.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.3.
    * The leaf has ExtendedKeyUsage with OID 1.2.840.113635.100.4.17.
 @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateQAConfigurationProfileSigner(void);

/*!
 @function SecPolicyCreateOTAPKISigner
 @abstract Returns a policy object for evaluating OTA PKI certificate chains.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to Apple PKI Settings CA.
    * There are exactly 2 certs in the chain.
 @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateOTAPKISigner(void);

/*!
 @function SecPolicyCreateTestOTAPKISigner
 @abstract Returns a policy object for evaluating OTA PKI certificate chains.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to Apple Test PKI Settings CA.
    * There are exactly 2 certs in the chain.
 @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateTestOTAPKISigner(void);

/*!
 @function SecPolicyCreateAppleIDValidationRecordSigningPolicy
 @abstract Returns a policy object for evaluating certificate chains for signing
 Apple ID Validation Records.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * The intermediate(s) has(have) a marker extension with OID 1.2.840.113635.100.6.2.3
      or OID 1.2.840.113635.100.6.2.10.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.25.
    * Revocation is checked via OCSP.
 @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleIDValidationRecordSigningPolicy(void);

/*!
 @function SecPolicyCreateAppleSMPEncryption
 @abstract Returns a policy object for evaluating SMP certificate chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.13.
    * The leaf has KeyUsage with the KeyEncipherment bit set.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.30.
    * Revocation is checked via OCSP.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleSMPEncryption(void);

/*!
 @function SecPolicyCreateTestAppleSMPEncryption
 @abstract Returns a policy object for evaluating Test SMP certificate chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to a Test Apple Root with ECC public key certificate.
    * There are exactly 3 certs in the chain.
    * The intermediate has Common Name "Test Apple System Integration CA - ECC".
    * The leaf has KeyUsage with the KeyEncipherment bit set.
    * Revocation is checked via OCSP.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateTestAppleSMPEncryption(void);

/*!
 @function SecPolicyCreateApplePPQSigning
 @abstract Returns a policy object for verifying production PPQ Signing certificates.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * There are exactly 3 certs in the chain.
    * The intermediate has Common Name "Apple System Integration 2 Certification
      Authority".
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.10.
    * The leaf has KeyUsage with the DigitalSignature bit set.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.38.2.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePPQSigning(void);

/*!
 @function SecPolicyCreateTestApplePPQSigning
 @abstract Returns a policy object for verifying test PPQ Signing certificates. On
 customer builds, this function returns the same policy as SecPolicyCreateApplePPQSigning.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * There are exactly 3 certs in the chain.
    * The intermediate has Common Name "Apple System Integration 2 Certification
      Authority".
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.10.
    * The leaf has KeyUsage with the DigitalSignature bit set.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.38.1.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateTestApplePPQSigning(void);

/*!
 @function SecPolicyCreateAppleIDSService
 @abstract Ensure we're appropriately pinned to the IDS service (SSL + Apple restrictions)
 @discussion This policy uses the SSL server policy.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleIDSService(CFStringRef __nullable hostname);

/*!
 @function SecPolicyCreateAppleIDSServiceContext
 @abstract Ensure we're appropriately pinned to the IDS service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATIDS" with value
 Boolean true will allow Test Apple roots on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Test Apple Root CAs
      are permitted only on internal releases either using the context dictionary or with
      defaults write.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.4.2 or,
      if Test Roots are allowed, OID 1.2.840.113635.100.6.27.4.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
      extension or Common Name.
    * The leaf is checked against the Black and Gray lists.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleIDSServiceContext(CFStringRef hostname, CFDictionaryRef __nullable context);

/*!
 @function SecPolicyCreateApplePushService
 @abstract Ensure we're appropriately pinned to the Apple Push service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATAPN" with value
 Boolean true will allow Test Apple roots on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Test Apple Root CAs
      are permitted only on internal releases either using the context dictionary or with
      defaults write.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.5.2 or,
      if Test Roots are allowed, OID 1.2.840.113635.100.6.27.5.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
      extension or Common Name.
    * The leaf is checked against the Black and Gray lists.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePushService(CFStringRef hostname, CFDictionaryRef __nullable context);

/*!
 @function SecPolicyCreateApplePushServiceLegacy
 @abstract Ensure we're appropriately pinned to the Push service (via Entrust)
 @param hostname Required; hostname to verify the certificate name against.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to an Entrust Intermediate.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
      extension or Common Name.
    * The leaf is checked against the Black and Gray lists.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePushServiceLegacy(CFStringRef hostname);

/*!
 @function SecPolicyCreateAppleMMCSService
 @abstract Ensure we're appropriately pinned to the MMCS service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATMMCS" with value
 Boolean true will allow Test Apple roots and test OIDs on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.11.2 or, if
    enabled, OID 1.2.840.113635.100.6.27.11.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
    extension or Common Name.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleMMCSService(CFStringRef hostname, CFDictionaryRef __nullable context);

/*!
 @function SecPolicyCreateAppleCompatibilityMMCSService
 @abstract Ensure we're appropriately pinned to the MMCS service using compatibility certs
 @param hostname Required; hostname to verify the certificate name against.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to the GeoTrust Global CA
    * The intermediate has a subject public key info hash matching the public key of
    the Apple IST CA G1 intermediate.
    * The chain length is 3.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.11.2 or
    OID 1.2.840.113635.100.6.27.11.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
    extension or Common Name.
    * The leaf is checked against the Black and Gray lists.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleCompatibilityMMCSService(CFStringRef hostname)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

/*!
 @function SecPolicyCreateAppleGSService
 @abstract Ensure we're appropriately pinned to the GS service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATGS" with value
 Boolean true will allow Test Apple roots on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Test Apple Root CAs
      are permitted only on internal releases either using the context dictionary or with
      defaults write.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.2.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
      extension or Common Name.
    * The leaf is checked against the Black and Gray lists.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleGSService(CFStringRef hostname, CFDictionaryRef __nullable context)
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

/*!
 @function SecPolicyCreateApplePPQService
 @abstract Ensure we're appropriately pinned to the PPQ service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATPPQ" with value
 Boolean true will allow Test Apple roots on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Test Apple Root CAs
      are permitted only on internal releases either using the context dictionary or with
      defaults write.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.3.2 or,
      if Test Roots are allowed, OID 1.2.840.113635.100.6.27.3.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
      extension or Common Name.
    * The leaf is checked against the Black and Gray lists.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePPQService(CFStringRef hostname, CFDictionaryRef __nullable context)
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

/*!
 @function SecPolicyCreateAppleAST2Service
 @abstract Ensure we're appropriately pinned to the AST2 Diagnostic service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATAST2" with value
 Boolean true will allow Test Apple roots on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Test Apple Root CAs
      are permitted either using the context dictionary or with defaults write.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.8.2 or,
      if Test Roots are allowed, OID 1.2.840.113635.100.6.27.8.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
      extension or Common Name.
    * The leaf is checked against the Black and Gray lists.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleAST2Service(CFStringRef hostname, CFDictionaryRef __nullable context)
    __OSX_AVAILABLE_STARTING(__MAC_10_11_4, __IPHONE_9_3);

/*!
 @function SecPolicyCreateAppleEscrowProxyService
 @abstract Ensure we're appropriately pinned to the iCloud Escrow Proxy service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATEscrow" with value
Boolean true will allow Test Apple roots on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
and pinning options:
    * The chain is anchored to any of the production Apple Root CAs via full certificate
      comparison. Test Apple Root CAs are permitted only on internal releases either
      using the context dictionary or with defaults write.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.7.2 or,
      if Test Roots are allowed, OID 1.2.840.113635.100.6.27.7.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
      extension or Common Name.
    * The leaf is checked against the Black and Gray lists.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleEscrowProxyService(CFStringRef hostname, CFDictionaryRef __nullable context)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

/*!
 @function SecPolicyCreateAppleCompatibilityEscrowProxyService
 @abstract Ensure we're appropriately pinned to the iCloud Escrow Proxy service using compatibility certs
 @param hostname Required; hostname to verify the certificate name against.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to the GeoTrust Global CA
    * The intermediate has a subject public key info hash matching the public key of
    the Apple IST CA G1 intermediate.
    * The chain length is 3.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.7.2 or,
    if UAT is enabled with a defaults write (internal devices only),
    OID 1.2.840.113635.100.6.27.7.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
    extension or Common Name.
    * The leaf is checked against the Black and Gray lists.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleCompatibilityEscrowProxyService(CFStringRef hostname)
__OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

/*!
 @function SecPolicyCreateAppleFMiPService
 @abstract Ensure we're appropriately pinned to the Find My iPhone service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATFMiP" with value
 Boolean true will allow Test Apple roots on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs via full certificate
    comparison. Test Apple Root CAs are permitted only on internal releases either
    using the context dictionary or with defaults write.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.6.2 or,
    if Test Roots are allowed, OID 1.2.840.113635.100.6.27.6.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
    extension or Common Name.
    * The leaf is checked against the Black and Gray lists.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleFMiPService(CFStringRef hostname, CFDictionaryRef __nullable context)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

/*!
 @function SecPolicyCreateAppleSSLService
 @abstract Ensure we're appropriately pinned to an Apple server (SSL + Apple restrictions)
 @param hostname Optional; hostname to verify the certificate name against.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.1
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
    extension or Common Name.
    * The leaf is checked against the Black and Gray lists.
    * The leaf has ExtendedKeyUsage, if any, with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleSSLService(CFStringRef __nullable hostname);

/*!
 @function SecPolicyCreateAppleTimeStamping
 @abstract Returns a policy object for evaluating time stamping certificate chains.
 @discussion This policy uses the Basic X.509 policy with validity check
 and requires the leaf has ExtendedKeyUsage with the TimeStamping OID.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleTimeStamping(void);

/*!
 @function SecPolicyCreateApplePayIssuerEncryption
 @abstract  Returns a policy object for evaluating Apple Pay Issuer Encryption certificate chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * There are exactly 3 certs in the chain.
    * The intermediate has Common Name "Apple Worldwide Developer Relations CA - G2".
    * The leaf has KeyUsage with the KeyEncipherment bit set.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.39.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateApplePayIssuerEncryption(void)
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

/*!
 @function SecPolicyCreateAppleATVVPNProfileSigning
 @abstract  Returns a policy object for evaluating Apple TV VPN Profile certificate chains.
 @discussion This policy uses the Basic X.509 policy with no validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Test Apple Root CAs
      are permitted only on internal releases.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.10.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.43.
    * Revocation is checked via OCSP.
 @result A policy object. The caller is responsible for calling CFRelease
     on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleATVVPNProfileSigning(void)
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

/*!
 @function SecPolicyCreateAppleHomeKitServerAuth
 @abstract Ensure we're appropriately pinned to the HomeKit service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs via full certificate
    comparison. Test Apple Root CAs are permitted only on internal releases with defaults write.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.16
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.9.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
    extension or Common Name.
    * The leaf is checked against the Black and Gray lists.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleHomeKitServerAuth(CFStringRef hostname)
    __OSX_AVAILABLE_STARTING(__MAC_10_11_4, __IPHONE_9_3);

/*!
 @function SecPolicyCreateAppleExternalDeveloper
 @abstract Returns a policy object for verifying Apple-issued external developer
 certificates.
 @discussion The resulting policy uses the Basic X.509 policy with validity check and
 pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * There are exactly 3 certs in the chain.
    * The intermediate has a marker extension with OID matching 1.2.840.113635.100.6.2.1
    (WWDR CA) or 1.2.840.113635.100.6.2.6 (Developer ID CA).
    * The leaf has a marker extension with OID matching one of the following:
        * 1.2.840.113635.100.6.1.2  ("iPhone Developer" leaf)
        * 1.2.840.113635.100.6.1.4  ("iPhone Distribution" leaf)
        * 1.2.840.113635.100.6.1.5  ("Safari Developer" leaf)
        * 1.2.840.113635.100.6.1.7  ("3rd Party Mac Developer Application" leaf)
        * 1.2.840.113635.100.6.1.8  ("3rd Party Mac Developer Installer" leaf)
        * 1.2.840.113635.100.6.1.12 ("Mac Developer" leaf)
        * 1.2.840.113635.100.6.1.13 ("Developer ID Application" leaf)
        * 1.2.840.113635.100.6.1.14 ("Developer ID Installer" leaf)
    * The leaf has an ExtendedKeyUsage OID matching one of the following:
        * 1.3.6.1.5.5.7.3.3         (CodeSigning EKU)
        * 1.2.840.113635.100.4.8    ("Safari Developer" EKU)
        * 1.2.840.113635.100.4.9    ("3rd Party Mac Developer Installer" EKU)
        * 1.2.840.113635.100.4.13   ("Developer ID Installer" EKU)
    * Revocation is checked via any available method.
    * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleExternalDeveloper(void)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecPolicyCreateAppleSoftwareSigning
 @abstract Returns a policy object for verifying the Apple Software Signing certificate.
 @discussion The resulting policy uses the Basic X.509 policy with validity check and
 pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * There are exactly 3 certs in the chain.
    * The intermediate has the Common Name "Apple Code Signing Certification Authority".
    * The leaf has a marker extension with OID matching 1.2.840.113635.100.6.22.
    * The leaf has an ExtendedKeyUsage OID matching 1.3.6.1.5.5.7.3.3 (Code Signing).
    * Revocation is checked via any available method.
    * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleSoftwareSigning(void)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecPolicyGetName
 @abstract Returns a policy's name.
 @param policy A policy reference.
 @result A policy name.
 */
__nullable CFStringRef SecPolicyGetName(SecPolicyRef policy)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecPolicyGetOidString
 @abstract Returns a policy's oid in string decimal format.
 @param policy A policy reference.
 @result A policy oid.
 */
CFStringRef SecPolicyGetOidString(SecPolicyRef policy)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecPolicyCreateAppleUniqueDeviceCertificate
 @abstract Returns a policy object for verifying Unique Device Identifier Certificates.
 @param testRootHash Optional; The SHA-256 fingerprint of a test root for pinning.
 @discussion The resulting policy uses the Basic X.509 policy with no validity check and
 pinning options:
    * The chain is anchored to the SEP Root CA. Internal releases allow the chain to be
    anchored to the testRootHash input if the value true is set for the key
    "ApplePinningAllowTestCertsUCRT" in the com.apple.security preferences for the user
    of the calling application.
    * There are exactly 3 certs in the chain.
    * The intermediate has an extension with OID matching 1.2.840.113635.100.6.44 and value
    of "ucrt".
    * The leaf has a marker extension with OID matching 1.2.840.113635.100.10.1.
    * RSA key sizes are disallowed. EC key sizes are P-256 or larger.
@result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleUniqueDeviceCertificate(CFDataRef __nullable testRootHash)
    __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecPolicyCreateAppleWarsaw
 @abstract Returns a policy object for verifying signed Warsaw assets.
 @discussion The resulting policy uses the Basic X.509 policy with validity check and
 pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * There are exactly 3 certs in the chain.
    * The intermediate has an extension with OID matching 1.2.840.113635.100.6.2.14.
    * The leaf has a marker extension with OID matching 1.2.840.113635.100.6.29.
    * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleWarsaw(void)
    __OSX_AVAILABLE(10.12.1) __IOS_AVAILABLE(10.1) __TVOS_AVAILABLE(10.0.1) __WATCHOS_AVAILABLE(3.1);

/*!
 @function SecPolicyCreateAppleSecureIOStaticAsset
 @abstract Returns a policy object for verifying signed static assets for Secure IO.
 @discussion The resulting policy uses the Basic X.509 policy with no validity check and
 pinning options:
    * The chain is anchored to any of the production Apple Root CAs. Internal releases allow
    the chain to be anchored to Test Apple Root CAs if a defaults write for the policy is set.
    * There are exactly 3 certs in the chain.
    * The intermediate has an extension with OID matching 1.2.840.113635.100.6.2.10.
    * The leaf has a marker extension with OID matching 1.2.840.113635.100.6.50.
    * RSA key sizes are 2048-bit or larger. EC key sizes are P-256 or larger.
 @result A policy object. The caller is responsible for calling CFRelease on this when
 it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleSecureIOStaticAsset(void)
    __OSX_AVAILABLE(10.12.1) __IOS_AVAILABLE(10.1) __TVOS_AVAILABLE(10.0.1) __WATCHOS_AVAILABLE(3.1);

/*!
 @function SecPolicyCreateAppleiCloudSetupService
 @abstract Ensure we're appropriately pinned to the iCloud Setup service (SSL + Apple restrictions)
 @param hostname Required; hostname to verify the certificate name against.
 @param context Optional; if present, "AppleServerAuthenticationAllowUATiCloudSetup" with value
 Boolean true will allow Test Apple roots and test OIDs on internal releases.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to any of the production Apple Root CAs.
    * The intermediate has a marker extension with OID 1.2.840.113635.100.6.2.12.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.15.2 or, if
    enabled, OID 1.2.840.113635.100.6.27.15.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
    extension or Common Name.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
    * Revocation is checked via any available method.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleiCloudSetupService(CFStringRef hostname, CFDictionaryRef __nullable context)
    __OSX_AVAILABLE(10.12.4) __IOS_AVAILABLE(10.3) __TVOS_AVAILABLE(10.2) __WATCHOS_AVAILABLE(3.2);

/*!
 @function SecPolicyCreateAppleCompatibilityiCloudSetupService
 @abstract Ensure we're appropriately pinned to the iCloud Setup service using compatibility certs
 @param hostname Required; hostname to verify the certificate name against.
 @discussion This policy uses the Basic X.509 policy with validity check
 and pinning options:
    * The chain is anchored to the GeoTrust Global CA
    * The intermediate has a subject public key info hash matching the public key of
    the Apple IST CA G1 intermediate.
    * The chain length is 3.
    * The leaf has a marker extension with OID 1.2.840.113635.100.6.27.15.2 or
    OID 1.2.840.113635.100.6.27.15.1.
    * The leaf has the provided hostname in the DNSName of the SubjectAlternativeName
    extension or Common Name.
    * The leaf is checked against the Black and Gray lists.
    * The leaf has ExtendedKeyUsage with the ServerAuth OID.
 @result A policy object. The caller is responsible for calling CFRelease
 on this when it is no longer needed.
 */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateAppleCompatibilityiCloudSetupService(CFStringRef hostname)
    __OSX_AVAILABLE(10.12.4) __IOS_AVAILABLE(10.3) __TVOS_AVAILABLE(10.2) __WATCHOS_AVAILABLE(3.2);


CF_IMPLICIT_BRIDGING_DISABLED
CF_ASSUME_NONNULL_END

/*
 *  Legacy functions (OS X only)
 */
#if TARGET_OS_MAC && !TARGET_OS_IPHONE

CF_ASSUME_NONNULL_BEGIN
CF_IMPLICIT_BRIDGING_ENABLED

/*!
     @function SecPolicyCopy
     @abstract Returns a copy of a policy reference based on certificate type and OID.
     @param certificateType A certificate type.
     @param policyOID The OID of the policy you want to find. This is a required parameter. See oidsalg.h to see a list of policy OIDs.
     @param policy The returned policy reference. This is a required parameter.
     @result A result code.  See "Security Error Codes" (SecBase.h).
     @discussion This function is deprecated in Mac OS X 10.7 and later;
     to obtain a policy reference, use one of the SecPolicyCreate* functions in SecPolicy.h.
 */
OSStatus SecPolicyCopy(CSSM_CERT_TYPE certificateType, const CSSM_OID *policyOID, SecPolicyRef * __nonnull CF_RETURNS_RETAINED policy)
    __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_3, __MAC_10_7, __IPHONE_NA, __IPHONE_NA);

/*!
     @function SecPolicyCopyAll
     @abstract Returns an array of all known policies based on certificate type.
     @param certificateType A certificate type. This is a optional parameter. Pass CSSM_CERT_UNKNOWN if the certificate type is unknown.
     @param policies The returned array of policies. This is a required parameter.
     @result A result code.  See "Security Error Codes" (SecBase.h).
     @discussion This function is deprecated in Mac OS X 10.7 and later;
     to obtain a policy reference, use one of the SecPolicyCreate* functions in SecPolicy.h. (Note: there is normally
     no reason to iterate over multiple disjointed policies, except to provide a way to edit trust settings for each
     policy, as is done in certain certificate UI views. In that specific case, your code should call SecPolicyCreateWithOID
     for each desired policy from the list of supported OID constants in SecPolicy.h.)
 */
OSStatus SecPolicyCopyAll(CSSM_CERT_TYPE certificateType, CFArrayRef * __nonnull CF_RETURNS_RETAINED policies)
    __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_3, __MAC_10_7, __IPHONE_NA, __IPHONE_NA);

/* Given a unified SecPolicyRef, return a copy with a legacy
 C++ ItemImpl-based Policy instance. Only for internal use;
 legacy references cannot be used by SecPolicy API functions. */
__nullable CF_RETURNS_RETAINED
SecPolicyRef SecPolicyCreateItemImplInstance(SecPolicyRef policy);

/* Given a CSSM_OID pointer, return a string which can be passed
 to SecPolicyCreateWithProperties. The return value can be NULL
 if no supported policy was found for the OID argument. */
__nullable
CFStringRef SecPolicyGetStringForOID(CSSM_OID* oid);

/*!
 @function SecPolicyCreateAppleTimeStampingAndRevocationPolicies
 @abstract Create timeStamping policy array from a given set of policies by applying identical revocation behavior
 @param policyOrArray can be a SecPolicyRef or a CFArray of SecPolicyRef
 @discussion This function is soon to be deprecated. Callers should create an array of the non-deprecated timestamping
 and revocation policies.
 */
__nullable CF_RETURNS_RETAINED
CFArrayRef SecPolicyCreateAppleTimeStampingAndRevocationPolicies(CFTypeRef policyOrArray);

CF_IMPLICIT_BRIDGING_DISABLED
CF_ASSUME_NONNULL_END

#endif /* TARGET_OS_MAC && !TARGET_OS_IPHONE */

__END_DECLS

#endif /* !_SECURITY_SECPOLICYPRIV_H_ */
