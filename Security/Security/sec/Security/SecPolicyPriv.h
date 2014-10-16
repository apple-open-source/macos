/*
 * Copyright (c) 2007-2014 Apple Inc. All Rights Reserved.
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

#include <Security/SecPolicy.h>
#include <Security/SecCertificate.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFString.h>

__BEGIN_DECLS

/*!
	@enum Policy Constants (Private)
	@discussion Predefined constants used to specify a policy.
	@constant kSecPolicyApplePassbookSigning
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

*/
extern CFTypeRef kSecPolicyApplePassbookSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFTypeRef kSecPolicyAppleMobileStore
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFTypeRef kSecPolicyAppleTestMobileStore
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFTypeRef kSecPolicyAppleEscrowService
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFTypeRef kSecPolicyAppleProfileSigner
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFTypeRef kSecPolicyAppleQAProfileSigner
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFTypeRef kSecPolicyAppleServerAuthentication
    __OSX_AVAILABLE_STARTING(__MAC_10_10, __IPHONE_8_0);
#if TARGET_OS_IPHONE
extern CFTypeRef kSecPolicyAppleOTAPKISigner
    __OSX_AVAILABLE_STARTING(__MAC_NA, __IPHONE_7_0);
extern CFTypeRef kSecPolicyAppleTestOTAPKISigner
    __OSX_AVAILABLE_STARTING(__MAC_NA, __IPHONE_7_0);
extern CFTypeRef kSecPolicyAppleIDValidationRecordSigningPolicy
	__OSX_AVAILABLE_STARTING(__MAC_NA, __IPHONE_7_0);
extern CFTypeRef kSecPolicyAppleSMPEncryption
	__OSX_AVAILABLE_STARTING(__MAC_NA, __IPHONE_8_0);
extern CFTypeRef kSecPolicyAppleTestSMPEncryption
	__OSX_AVAILABLE_STARTING(__MAC_NA, __IPHONE_8_0);
#endif
extern CFTypeRef kSecPolicyApplePCSEscrowService
    __OSX_AVAILABLE_STARTING(__MAC_10_10, __IPHONE_7_0);


/*!
    @enum Policy Value Constants
    @abstract Predefined property key constants used to get or set values in
        a dictionary for a policy instance.
    @constant kSecPolicyTeamIdentifier Specifies a CFStringRef containing a
        team identifier which must be matched in the certificate to satisfy
        this policy. For the Passbook signing policy, this string must match
        the Organizational Unit field of the certificate subject.
*/
extern CFTypeRef kSecPolicyTeamIdentifier
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);

/*!
    @function SecPolicyCreateiPhoneActivation
    @abstract Returns a policy object for verifying iPhone Activation
    certificate chains.
    @discussion This policy is like the Basic X.509 policy with the additional
    requirements that the chain must contain exactly three certificates, the
    anchor is the Apple Inc. CA, and the subject of the first intermediate
    certificate has "Apple iPhone Certification Authority" as its only
    Common Name entry.
    @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
SecPolicyRef SecPolicyCreateiPhoneActivation(void);

/*!
    @function SecPolicyCreateiPhoneDeviceCertificate
    @abstract Returns a policy object for verifying iPhone Device certificate
    chains.
    @discussion This policy is like the Basic X.509 policy with the additional
    requirements that the chain must contain exactly four certificates, the
    anchor is the Apple Inc. CA, and the subject of the first intermediate
    certificate has "Apple iPhone Device CA" as its only Common Name entry.
    @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
SecPolicyRef SecPolicyCreateiPhoneDeviceCertificate(void);

/*!
    @function SecPolicyCreateFactoryDeviceCertificate
    @abstract Returns a policy object for verifying Factory Device certificate
    chains.
    @discussion This policy is like the Basic X.509 policy with the additional
    requirements that the chain must be anchored to the factory device certificate
    issuer.
    @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
SecPolicyRef SecPolicyCreateFactoryDeviceCertificate(void);

/*!
    @function SecPolicyCreateiAP
    @abstract Returns a policy object for verifying iAP certificate chains.
    @discussion This policy is like the Basic X.509 policy with these
	additional requirements:
	 * The leaf's NotValidBefore should be greater than 5/31/06 midnight GMT.
	 * The Common Name of the leaf begins with the characters "IPA_".
	 * No validity checking is performed for any of the certificates.
	The intended use of this policy is that the caller pass in the
	intermediates for iAP1 and iAP2 to SecTrustSetAnchorCertificates().
    @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
SecPolicyRef SecPolicyCreateiAP(void);

/*!
    @function SecPolicyCreateiTunesStoreURLBag
    @abstract Returns a policy object for verifying iTunes Store URL bag
    certificates.
    @discussion This policy is like the Basic X.509 policy with these
	additional requirements:
	 * The leaf's Organization is Apple Inc.
	 * The Common Name of the leaf is "iTunes Store URL Bag".
    @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
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
    @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
SecPolicyRef SecPolicyCreateEAP(Boolean server, CFArrayRef trustedServerNames);

/*!
    @function SecPolicyCreateIPSec
    @abstract Returns a policy object for evaluating IPSec certificate chains.
	@param server Passing true for this parameter create a policy for IPSec
	server certificates.
	@param hostname Optional; if present, the policy will require the specified
	hostname or ip address to match the hostname in the leaf certificate.
    @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
SecPolicyRef SecPolicyCreateIPSec(Boolean server, CFStringRef hostname);

/*!
    @function SecPolicyCreateiPhoneApplicationSigning
    @abstract Returns a policy object for evaluating signed application
    signatures.  This is for apps signed directly by the app store.
    @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
SecPolicyRef SecPolicyCreateiPhoneApplicationSigning(void);

/*!
    @function SecPolicyCreateiPhoneProfileApplicationSigning
    @abstract Returns a policy object for evaluating signed application
    signatures.  This is meant for certificates inside a UPP or regular
    profile.  Currently it only checks for experation of the leaf and
    revocation status.
    @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
SecPolicyRef SecPolicyCreateiPhoneProfileApplicationSigning(void);

/*!
    @function SecPolicyCreateiPhoneProvisioningProfileSigning
    @abstract Returns a policy object for evaluating provisioning profile signatures.
    @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
SecPolicyRef SecPolicyCreateiPhoneProvisioningProfileSigning(void);

/*!
    @function SecPolicyCreateOCSPSigner
    @abstract Returns a policy object for evaluating ocsp response signers.
    @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
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
    flags, to indicated the intended usage of this certificate.  A certificate which allows
	@param email Optional; if present, the policy will require the specified
	email to match the email in the leaf certificate.
    @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
SecPolicyRef SecPolicyCreateSMIME(CFIndex smimeUsage, CFStringRef email);

/*!
    @function SecPolicyCreateCodeSigning
    @abstract Returns a policy object for evaluating code signing certificate chains.
    @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
SecPolicyRef SecPolicyCreateCodeSigning(void);

/*!
    @function SecPolicyCreateLockdownPairing
    @abstract basic x509 policy for checking lockdown pairing certificate chains.
        It explicitly allows for empty subjects
*/
SecPolicyRef SecPolicyCreateLockdownPairing(void);

/*!
 @function SecPolicyCreateURLBag
 @abstract check for private CA, eku codesigning and certificate policy that
 pertains to signing of URL bags.
 */
SecPolicyRef SecPolicyCreateURLBag(void);

/*!
 @function SecPolicyCreateOTATasking
 @abstract check for 3 long chain through Apple Certification Policy with common name
           "OTA Task Signing".
 */
SecPolicyRef SecPolicyCreateOTATasking(void);

/*!
 @function SecPolicyCreateMobileAsset
 @abstract check for 3 long chain through Apple Certification Policy with common name
           "Asset Manifest Signing".
 */
SecPolicyRef SecPolicyCreateMobileAsset(void);

/*!
 @function SecPolicyCreateAppleIDAuthorityPolicy
 @abstract check for an Apple ID identity per marker in the leaf and marker in the intermediate, rooted in the Apple CA.
 */
SecPolicyRef SecPolicyCreateAppleIDAuthorityPolicy(void);

/*!
 @function SecPolicyCreatePassbookCardSigner
 @abstract check rooted in the Apple CA, eku shoebox, marker shoebox and name matching
 @param cardIssuer Required; must match name in marker extension.
 @param teamIdentifier Optional; if present, the policy will require the specified
 team ID to match the organizationalUnit field in the leaf certificate's subject.
 @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
 */
SecPolicyRef SecPolicyCreatePassbookCardSigner(CFStringRef cardIssuer,
	CFStringRef teamIdentifier);

/*!
 @function SecPolicyCreateShoeboxCardSigner
 @abstract Deprecated; use SecPolicyCreatePassbookCardSigner instead
 */
SecPolicyRef SecPolicyCreateShoeboxCardSigner(CFStringRef cardIssuer);

/*!
 @function SecPolicyCreateMobileStoreSigner
 @abstract Check for key usage of digital signature,
	check for 3 long chain through Apple System Integration 2 Certification Authority
	with a certificate policy OID of 1.2.840.113635.100.5.12 that roots to the
	Apple root
 */
SecPolicyRef SecPolicyCreateMobileStoreSigner(void);

/*!
 @function SecPolicyCreateTestMobileStoreSigner
 @abstract Check for key usage of digital signature,
	check for 3 long chain through Apple System Integration 2 Certification Authority
	with a certificate policy OID of 1.2.840.113635.100.5.12.1 that roots to the
	Apple root
 */
SecPolicyRef SecPolicyCreateTestMobileStoreSigner(void);

/*!
 @function SecPolicyCreateEscrowServiceSigner
 @abstract Check for key usage of digital signature, has a leaf marker OID of
	 1.2.840.113635.100.6.23.1 and roots to the production Escrow Root
 */
SecPolicyRef SecPolicyCreateEscrowServiceSigner(void);

/*!
 @function SecPolicyCreatePCSEscrowServiceSigner
 @abstract Check for key usage of digital signature, has a leaf marker OID of
	 1.2.840.113635.100.6.23.1 and roots to the production PCS Escrow Root
 */
SecPolicyRef SecPolicyCreatePCSEscrowServiceSigner(void);

/*!
 @function SecPolicyCopyEscrowRootCertificate
 @abstract Return back the Root certificate for the Escrow service
*/
SecCertificateRef SecPolicyCopyEscrowRootCertificate(void);

/*!
 @function SecPolicyCreateConfigurationProfileSigner
 @abstract Check for key usage of digital signature, has a EKU OID of
     1.2.840.113635.100.4.16 and
    roots to Apple Application Integration 2 Certification Authority
*/
SecPolicyRef SecPolicyCreateConfigurationProfileSigner(void);

/*!
 @function SecPolicyCreateQAConfigurationProfileSigner
 @abstract Check for key usage of digital signature, has a EKU OID of
    1.2.840.113635.100.4.17 and
    roots to Apple Application Integration 2 Certification Authority
*/
SecPolicyRef SecPolicyCreateQAConfigurationProfileSigner(void);

#if TARGET_OS_IPHONE
/*!
 @function SecPolicyCreateOTAPKISigner
 @abstract Check for key usage of digital signature, and
    roots to Apple PKI Settings Root Certification Authority
*/
SecPolicyRef SecPolicyCreateOTAPKISigner(void);

/*!
 @function SecPolicyCreateTestOTAPKISigner
 @abstract Check for key usage of digital signature, and
    roots to Apple PKI Settings Root - TESTING
*/
SecPolicyRef SecPolicyCreateTestOTAPKISigner(void);

/*!
 @function SecPolicyCreateAppleIDValidationRecordSigningPolicy
 @abstract Check for leaf certificate contains the
	appleIDValidationRecordSigning (1 2 840 113635 100 6 25), and
	intermediate certificate contains
	appleCertificateExtensionApplicationIntegrationIntermediate
	(1 2 840 113635 100 6 2 3) and
	appleCertificateExtensionSystemIntegration2Intermediate
	(1 2 840 113635 100 6 2 10) and roots to the Apple root
*/
SecPolicyRef SecPolicyCreateAppleIDValidationRecordSigningPolicy(void);

/*!
 @function SecPolicyCreateAppleSMPEncryption
 @abstract Check for intermediate certificate 'Apple System Integration CA - ECC' by name,
    and root certificate 'Apple Root CA - ECC' by hash.
    Leaf cert must have Key Encipherment usage. Other checks TBD.
 */
SecPolicyRef SecPolicyCreateAppleSMPEncryption(void);

/*!
 @function SecPolicyCreateTestAppleSMPEncryption
 @abstract Check for intermediate certificate 'Test Apple System Integration CA - ECC' by name,
    and root certificate 'Test Apple Root CA - ECC' by hash.
    Leaf cert must have Key Encipherment usage. Other checks TBD.
 */
SecPolicyRef SecPolicyCreateTestAppleSMPEncryption(void);


#endif

/*!
 @function SecPolicyCreateAppleIDSService
 @abstract Ensure we're appropriately pinned to the IDS service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleIDSService(CFStringRef hostname);

/*!
 @function SecPolicyCreateApplePushService
 @abstract Ensure we're appropriately pinned to the Push service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateApplePushService(CFStringRef hostname);

/*!
 @function SecPolicyCreateAppleMMCSService
 @abstract Ensure we're appropriately pinned to the IDS service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleMMCSService(CFStringRef hostname);

/*!
 @function SecPolicyCreateAppleSSLService
 @abstract Ensure we're appropriately pinned to an Apple server (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleSSLService(CFStringRef hostname);

__END_DECLS

#endif /* !_SECURITY_SECPOLICYPRIV_H_ */
