/*
 * Copyright (c) 2008-2015 Apple Inc. All Rights Reserved.
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
    The functions provided in SecPolicyInternal provide the interface to
    trust policies used by SecTrust.
*/

#ifndef _SECURITY_SECPOLICYINTERNAL_H_
#define _SECURITY_SECPOLICYINTERNAL_H_

#include <Security/SecPolicy.h>
#include <Security/SecTrust.h>
#include <Security/SecCertificatePath.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFRuntime.h>

__BEGIN_DECLS

/********************************************************
 ****************** SecPolicy struct ********************
 ********************************************************/
struct __SecPolicy {
    CFRuntimeBase		_base;
    CFStringRef			_oid;
    CFStringRef		_name;
	CFDictionaryRef		_options;
};

/*!
    @enum Policy Check Keys
    @discussion Keys that represent various checks that can be done in a trust
	policy.
    @constant kSecPolicyCheckCriticalExtensions Ensure that no certificate in the chain has any critical extensions that we do not understand.
    @constant kSecPolicyCheckIdLinkage Check that all the certificates in the chain that have a SubjectId, match the AuthorityId of the certificate they sign.  This check is optional, in that if either certificate is missing the required extension the check succeeds.
	@constant kSecPolicyCheckBasicConstraints Fails if the basic constraints for the certificate chain are not met, this allows for basic constraints to be non critical and doesn't require every CA certificate to have a basic constraints extension, and allows for leaf certificates to have basic constraints extensions.
	@constant kSecPolicyCheckExtendedKeyUsage @@@
	@constant kSecPolicyCheckIdLinkage Fails if the AuthorityKeyID -> SubjectKeyID chaining isn't right.
	@constant kSecPolicyCheckKeyUsage @@@
	@constant kSecPolicyCheckWeakIntermediates Fails if any certificates in the chain (other than the leaf and root) have a too small key size.
	@constant kSecPolicyCheckWeakLeaf Fails if the leaf has a too small key size.
	@constant kSecPolicyCheckWeakRoot Fails if the root has a too small key size.
	@constant kSecPolicyCheckKeySize Fails if any certificates in the chain have key size smaller than the policy allows.
	@constant kSecPolicyCheckSignatureHashAlgorithms Fails if any certificates in the chain use a hash algorithm disallowed by the policy.
	@constant kSecPolicyCheckNonEmptySubject Perform the following check: RFC 3280, 4.1.2.6, says that an empty subject name can only appear in a leaf cert, and only if subjectAltName is present and marked critical.
	@constant kSecPolicyCheckQualifiedCertStatements Perform the following check: RFC 3739: if this cert has a Qualified Cert Statements extension, and it's Critical, make sure we understand all of the extension's statementIds.
	@constant kSecPolicyCheckValidIntermediates Fails if any certificates in the chain are not valid at the verify time other than the leaf and the root.
	@constant kSecPolicyCheckValidLeaf Fails if the leaf certificate is not valid at the verify time.
	@constant kSecPolicyCheckValidRoot Fails if the root certificate is not valid at the verify time.
	@constant kSecPolicyCheckAnchorTrusted @@@.
	@constant kSecPolicyCheckAnchorSHA1 @@@.
	@constant kSecPolicyCheckAnchorSHA256 @@@.
	@constant kSecPolicyCheckAnchorApple @@@.
	@constant kSecPolicyCheckSSLHostname @@@.
	@constant kSecPolicyCheckEmail @@@.
	@constant kSecPolicyCheckIssuerCommonName @@@.
	@constant kSecPolicyCheckSubjectCommonNamePrefix @@@.
	@constant kSecPolicyCheckChainLength @@@.
	@constant kSecPolicyCheckNotValidBefore @@@.
	@constant kSecPolicyCheckEAPTrustedServerNames @@@.
	@constant kSecPolicyCheckBasicCertificateProcessing @@@.
	@constant kSecPolicyCheckExtendedValidation @@@.
	@constant kSecPolicyCheckRevocation Perform a revocation check.
	@constant kSecPolicyCheckRevocationResponseRequired Require positive response for revocation check. Use of thise constant indicates that the policy should "fail closed" in case of missing revocation information.
	@constant kSecPolicyCheckRevocationOCSP Use OCSP to perform revocation check.
	@constant kSecPolicyCheckRevocationCRL Use CRL to perform revocation check.
	@constant kSecPolicyCheckRevocationAny Use any available method (OCSP or CRL) to perform revocation check.
	@constant kSecPolicyCheckRevocationOnline Force an "online" OCSP check.
	@constant kSecPolicyCheckNoNetworkAccess @@@.
    @constant kSecPolicyCheckBlackListedLeaf @@@.
    @constant kSecPolicyCheckUsageConstraints @@@.
    @constant kSecPolicyCheckSystemTrustedWeakHash Check whether the leaf or intermediates are using a weak hash in chains that end with a system-trusted anchor.
    @constant kSecPolicyCheckIntermediateOrganization Fails if any (non-leaf and non-root) certificates in the chain do not have a matching Organization string.
    @constant kSecPolicyCheckIntermediateCountry Fails if any (non-leaf and non-root) certificates in the chain do not have a matching Country string.
*/
extern const CFStringRef kSecPolicyCheckBasicConstraints;
extern const CFStringRef kSecPolicyCheckCriticalExtensions;
extern const CFStringRef kSecPolicyCheckExtendedKeyUsage;
extern const CFStringRef kSecPolicyCheckIdLinkage;
extern const CFStringRef kSecPolicyCheckWeakIntermediates;
extern const CFStringRef kSecPolicyCheckWeakLeaf;
extern const CFStringRef kSecPolicyCheckWeakRoot;
extern const CFStringRef kSecPolicyCheckKeySize;
extern const CFStringRef kSecPolicyCheckSignatureHashAlgorithms;
extern const CFStringRef kSecPolicyCheckKeyUsage;
extern const CFStringRef kSecPolicyCheckNonEmptySubject;
extern const CFStringRef kSecPolicyCheckQualifiedCertStatements;
extern const CFStringRef kSecPolicyCheckValidIntermediates;
extern const CFStringRef kSecPolicyCheckValidLeaf;
extern const CFStringRef kSecPolicyCheckValidRoot;
extern const CFStringRef kSecPolicyCheckAnchorTrusted;
extern const CFStringRef kSecPolicyCheckAnchorSHA1;
extern const CFStringRef kSecPolicyCheckAnchorSHA256;
extern const CFStringRef kSecPolicyCheckAnchorApple;
extern const CFStringRef kSecPolicyCheckSSLHostname;
extern const CFStringRef kSecPolicyCheckEmail;
extern const CFStringRef kSecPolicyCheckIssuerCommonName;
extern const CFStringRef kSecPolicyCheckSubjectCommonName;
extern const CFStringRef kSecPolicyCheckSubjectCommonNameTEST;
extern const CFStringRef kSecPolicyCheckSubjectOrganization;
extern const CFStringRef kSecPolicyCheckSubjectOrganizationalUnit;
extern const CFStringRef kSecPolicyCheckSubjectCommonNamePrefix;
extern const CFStringRef kSecPolicyCheckChainLength;
extern const CFStringRef kSecPolicyCheckNotValidBefore;
extern const CFStringRef kSecPolicyCheckEAPTrustedServerNames;
extern const CFStringRef kSecPolicyCheckCertificatePolicy;
extern const CFStringRef kSecPolicyCheckBasicCertificateProcessing;
extern const CFStringRef kSecPolicyCheckExtendedValidation;
extern const CFStringRef kSecPolicyCheckRevocation;
extern const CFStringRef kSecPolicyCheckRevocationResponseRequired;
extern const CFStringRef kSecPolicyCheckRevocationOCSP;
extern const CFStringRef kSecPolicyCheckRevocationCRL;
extern const CFStringRef kSecPolicyCheckRevocationAny;
extern const CFStringRef kSecPolicyCheckRevocationOnline;
extern const CFStringRef kSecPolicyCheckNoNetworkAccess;
extern const CFStringRef kSecPolicyCheckBlackListedLeaf;
extern const CFStringRef kSecPolicyCheckBlackListedKey;
extern const CFStringRef kSecPolicyCheckGrayListedLeaf;
extern const CFStringRef kSecPolicyCheckLeafMarkerOid;
extern const CFStringRef kSecPolicyCheckLeafMarkerOidWithoutValueCheck;
extern const CFStringRef kSecPolicyCheckLeafMarkersProdAndQA;
extern const CFStringRef kSecPolicyCheckIntermediateMarkerOid;
extern const CFStringRef kSecPolicyCheckIntermediateSPKISHA256;
extern const CFStringRef kSecPolicyCheckIntermediateEKU;
extern const CFStringRef kSecPolicyCheckGrayListedKey;
extern const CFStringRef kSecPolicyCheckCertificateTransparency;
extern const CFStringRef kSecPolicyCheckUsageConstraints;
extern const CFStringRef kSecPolicyCheckSystemTrustedWeakHash;
extern const CFStringRef kSecPolicyCheckIntermediateOrganization;
extern const CFStringRef kSecPolicyCheckIntermediateCountry;

/*  Special option for checking Apple Anchors */
extern const CFStringRef kSecPolicyAppleAnchorIncludeTestRoots;

/* Special option for checking Prod and QA Markers */
extern const CFStringRef kSecPolicyLeafMarkerProd;
extern const CFStringRef kSecPolicyLeafMarkerQA;

SecPolicyRef SecPolicyCreate(CFStringRef oid, CFStringRef name, CFDictionaryRef options);

CFDictionaryRef SecPolicyGetOptions(SecPolicyRef policy);
void SecPolicySetOptionsValue(SecPolicyRef policy, CFStringRef key, CFTypeRef value);

xpc_object_t SecPolicyArrayCopyXPCArray(CFArrayRef policies, CFErrorRef *error);
CFArrayRef SecPolicyXPCArrayCopyArray(xpc_object_t xpc_policies, CFErrorRef *error);

CFArrayRef SecPolicyArrayCreateDeserialized(CFArrayRef serializedPolicies);
CFArrayRef SecPolicyArrayCreateSerialized(CFArrayRef policies);

/*
 * MARK: SecPolicyCheckCert functions
 */
bool SecPolicyCheckCertKeyUsage(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertExtendedKeyUsage(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertSSLHostname(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertEmail(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertSubjectCommonNamePrefix(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertSubjectCommonName(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertSubjectCommonNameTEST(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertNotValidBefore(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertSubjectOrganization(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertSubjectOrganizationalUnit(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertEAPTrustedServerNames(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertLeafMarkerOid(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertLeafMarkerOidWithoutValueCheck(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertSignatureHashAlgorithms(SecCertificateRef cert, CFTypeRef pvcValue);
bool SecPolicyCheckCertSubjectCountry(SecCertificateRef cert, CFTypeRef pvcValue);


/*
 * MARK: SecLeafPVC functions
 */

typedef struct OpaqueSecLeafPVC *SecLeafPVCRef;

struct OpaqueSecLeafPVC {
    SecCertificateRef leaf;
    CFArrayRef policies;
    CFAbsoluteTime verifyTime;
    CFArrayRef details;
    CFMutableDictionaryRef info;
    CFDictionaryRef callbacks;
    CFIndex policyIX;
    bool result;
};

void SecLeafPVCInit(SecLeafPVCRef pvc, SecCertificateRef leaf, CFArrayRef policies, CFAbsoluteTime verifyTime);
void SecLeafPVCDelete(SecLeafPVCRef pvc);
bool SecLeafPVCLeafChecks(SecLeafPVCRef pvc);

__END_DECLS

#endif /* !_SECURITY_SECPOLICYINTERNAL_H_ */
