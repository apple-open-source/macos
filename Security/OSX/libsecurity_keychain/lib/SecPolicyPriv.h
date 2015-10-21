/*
 * Copyright (c) 2003-2015 Apple Inc. All Rights Reserved.
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
	Private part of SecPolicy.h
*/

#ifndef _SECURITY_SECPOLICYPRIV_H_
#define _SECURITY_SECPOLICYPRIV_H_

#include <Security/SecPolicy.h>
#include <CoreFoundation/CFArray.h>


#if defined(__cplusplus)
extern "C" {
#endif

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
	@constant kSecPolicyAppleATVAppSigning
	@constant kSecPolicyAppleTestATVAppSigning
	@constant kSecPolicyAppleOSXProvisioningProfileSigning

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
#if TARGET_OS_IPHONE
extern const CFStringRef kSecPolicyAppleOTAPKISigner
    __OSX_AVAILABLE_STARTING(__MAC_NA, __IPHONE_7_0);
extern const CFStringRef kSecPolicyAppleTestOTAPKISigner
    __OSX_AVAILABLE_STARTING(__MAC_NA, __IPHONE_7_0);
extern const CFStringRef kSecPolicyAppleIDValidationRecordSigningPolicy
	__OSX_AVAILABLE_STARTING(__MAC_NA, __IPHONE_7_0);
extern const CFStringRef kSecPolicyAppleSMPEncryption
	__OSX_AVAILABLE_STARTING(__MAC_NA, __IPHONE_8_0);
extern const CFStringRef kSecPolicyAppleTestSMPEncryption
	__OSX_AVAILABLE_STARTING(__MAC_NA, __IPHONE_8_0);
#endif
extern const CFStringRef kSecPolicyApplePCSEscrowService
    __OSX_AVAILABLE_STARTING(__MAC_10_10, __IPHONE_8_0);
extern const CFStringRef kSecPolicyApplePPQSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyAppleTestPPQSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyAppleSWUpdateSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyAppleATVAppSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyAppleTestATVAppSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecPolicyAppleOSXProvisioningProfileSigning
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

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
OSStatus SecPolicyCopy(CSSM_CERT_TYPE certificateType, const CSSM_OID *policyOID, SecPolicyRef* policy)
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
OSStatus SecPolicyCopyAll(CSSM_CERT_TYPE certificateType, CFArrayRef* policies)
	__OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_3, __MAC_10_7, __IPHONE_NA, __IPHONE_NA);

/* Given a unified SecPolicyRef, return a copy with a legacy
   C++ ItemImpl-based Policy instance. Only for internal use;
   legacy references cannot be used by SecPolicy API functions. */
SecPolicyRef SecPolicyCreateItemImplInstance(SecPolicyRef policy);

/* Given a CSSM_OID pointer, return a string which can be passed
   to SecPolicyCreateWithProperties. The return value can be NULL
   if no supported policy was found for the OID argument. */
CFStringRef SecPolicyGetStringForOID(CSSM_OID* oid);

/*!
 @function SecPolicyCreateAppleIDSService
 @abstract Ensure we're appropriately pinned to the IDS service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleIDSService(CFStringRef hostname);

/*!
 @function SecPolicyCreateAppleIDSService
 @abstract Ensure we're appropriately pinned to the IDS service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleIDSServiceContext(CFStringRef hostname, CFDictionaryRef context);

/*!
 @function SecPolicyCreateApplePushService
 @abstract Ensure we're appropriately pinned to the Push service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateApplePushService(CFStringRef hostname, CFDictionaryRef context);

/*!
 @function SecPolicyCreateApplePushServiceLegacy
 @abstract Ensure we're appropriately pinned to the Push service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateApplePushServiceLegacy(CFStringRef hostname);

/*!
 @function SecPolicyCreateAppleMMCSService
 @abstract Ensure we're appropriately pinned to the IDS service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleMMCSService(CFStringRef hostname, CFDictionaryRef context);

/*!
 @function SecPolicyCreateAppleGSService
 @abstract Ensure we're appropriately pinned to the GS service (SSL + Apple restrictions)
*/
SecPolicyRef SecPolicyCreateAppleGSService(CFStringRef hostname, CFDictionaryRef context)
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

/*!
 @function SecPolicyCreateApplePPQService
 @abstract Ensure we're appropriately pinned to the PPQ service (SSL + Apple restrictions)
*/
SecPolicyRef SecPolicyCreateApplePPQService(CFStringRef hostname, CFDictionaryRef context);

/*!
 @function SecPolicyCreateAppleSSLService
 @abstract Ensure we're appropriately pinned to an Apple server (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleSSLService(CFStringRef hostname);

/*!
 @function SecPolicyCreateAppleTimeStampingAndRevocationPolicies
 @abstract Create timeStamping policy array from a given set of policies by applying identical revocation behavior
 @param policyOrArray can be a SecPolicyRef or a CFArray of SecPolicyRef
 */
CFArrayRef SecPolicyCreateAppleTimeStampingAndRevocationPolicies(CFTypeRef policyOrArray);

/*!
 @function SecPolicyCreateAppleATVAppSigning
 @abstract Check for intermediate certificate 'Apple Worldwide Developer Relations Certification Authority' by name,
 and apple anchor.
 Leaf cert must have Digital Signature usage.
 Leaf cert must have Apple ATV App Signing marker OID (1.2.840.113635.100.6.1.24).
 Leaf cert must have 'Apple TVOS Application Signing' common name.
 */
SecPolicyRef SecPolicyCreateAppleATVAppSigning(void)
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

/*!
 @function SecPolicyCreateTestAppleATVAppSigning
 @abstract Check for intermediate certificate 'Apple Worldwide Developer Relations Certification Authority' by name,
 and apple anchor.
 Leaf cert must have Digital Signature usage.
 Leaf cert must have Apple ATV App Signing Test marker OID (1.2.840.113635.100.6.1.24.1).
 Leaf cert must have 'TEST Apple TVOS Application Signing TEST' common name.
 */
SecPolicyRef SecPolicyCreateTestAppleATVAppSigning(void)
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

/*!
 @function SecPolicyCreateApplePayIssuerEncryption
 @abstract Check for intermediate certificate 'Apple Worldwide Developer Relations CA - G2' by name,
 and apple anchor.
 Leaf cert must have Key Encipherment and Key Agreement usage.
 Leaf cert must have Apple Pay Issuer Encryption marker OID (1.2.840.113635.100.6.39).
 */
SecPolicyRef SecPolicyCreateApplePayIssuerEncryption(void)
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

/*!
 @function SecPolicyCreateOSXProvisioningProfileSigning
 @abstract Check for leaf marker OID 1.2.840.113635.100.4.11,
	 intermediate marker OID 1.2.840.113635.100.6.2.1,
	 chains to Apple Root CA
*/
SecPolicyRef SecPolicyCreateOSXProvisioningProfileSigning(void)
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECPOLICYPRIV_H_ */
