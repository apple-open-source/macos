/*
 * Copyright (c) 2008-2014 Apple Inc. All Rights Reserved.
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
	CFDictionaryRef		_options;
};

/*!
    @enum Policy Check Keys
    @discussion Keys that represent various checks that can be done in a trust
	policy.
    @constant kSecPolicyCheckCriticalExtensions Ensure that no certificate in the chain has any critical extensions that we do not understand.
    @constant kSecPolicyCheckIdLinkage Check that all the certificates in the chain that have a SubjectId, match the AuthorityId of the certificate they sign.  This check is optional, in that if either certificate is missing the required extension the check succeeds.
	@constant kSecPolicyCheckBasicContraints Fails if the basic constraints for the certificate chain are not met, this allows for basic constraints to be non critical and doesn't require every CA certificate to have a basic constraints extension, and allows for leaf certificates to have basic constraints extensions.
	@constant kSecPolicyCheckExtendedKeyUsage @@@
	@constant kSecPolicyCheckIdLinkage Fails if the AuthorityKeyID -> SubjectKeyID chaining isn't right.
	@constant kSecPolicyCheckKeyUsage @@@
	@constant kSecPolicyCheckNonEmptySubject Perform the following check: RFC 3280, 4.1.2.6, says that an empty subject name can only appear in a leaf cert, and only if subjectAltName is present and marked critical.
	@constant kSecPolicyCheckQualifiedCertStatements Perform the following check: RFC 3739: if this cert has a Qualified Cert Statements extension, and it's Critical, make sure we understand all of the extension's statementIds.
	@constant kSecPolicyCheckValidIntermediates Fails if any certificates in the chain are not valid at the verify time other than the leaf and the root.
	@constant kSecPolicyCheckValidLeaf Fails if the leaf certificate is not valid at the verify time.
	@constant kSecPolicyCheckValidRoot Fails if the root certificate is not valid at the verify time.
	@constant kSecPolicyCheckAnchorTrusted @@@.
	@constant kSecPolicyCheckAnchorSHA1 @@@.
	@constant kSecPolicyCheckSSLHostname @@@.
	@constant kSecPolicyCheckEmail @@@.
	@constant kSecPolicyCheckIssuerCommonName @@@.
	@constant kSecPolicyCheckSubjectCommonNamePrefix @@@.
	@constant kSecPolicyCheckChainLength @@@.
	@constant kSecPolicyCheckNotValidBefore @@@.
	@constant kSecPolicyCheckEAPTrustedServerNames @@@.
	@constant kSecPolicyCheckBasicCertificateProcessing @@@.
	@constant kSecPolicyCheckExtendedValidation @@@.
	@constant kSecPolicyCheckRevocation @@@.
	@constant kSecPolicyCheckNoNetworkAccess @@@.
    @constant kSecPolicyCheckBlackListedLeaf @@@.
*/
extern CFStringRef kSecPolicyCheckBasicContraints;
extern CFStringRef kSecPolicyCheckCriticalExtensions;
extern CFStringRef kSecPolicyCheckExtendedKeyUsage;
extern CFStringRef kSecPolicyCheckIdLinkage;
extern CFStringRef kSecPolicyCheckKeyUsage;
extern CFStringRef kSecPolicyCheckNonEmptySubject;
extern CFStringRef kSecPolicyCheckQualifiedCertStatements;
extern CFStringRef kSecPolicyCheckValidIntermediates;
extern CFStringRef kSecPolicyCheckValidLeaf;
extern CFStringRef kSecPolicyCheckValidRoot;
extern CFStringRef kSecPolicyCheckAnchorTrusted;
extern CFStringRef kSecPolicyCheckAnchorSHA1;
extern CFStringRef kSecPolicyCheckSSLHostname;
extern CFStringRef kSecPolicyCheckEmail;
extern CFStringRef kSecPolicyCheckIssuerCommonName;
extern CFStringRef kSecPolicyCheckSubjectCommonName;
extern CFStringRef kSecPolicyCheckSubjectCommonNameTEST;
extern CFStringRef kSecPolicyCheckSubjectOrganization;
extern CFStringRef kSecPolicyCheckSubjectOrganizationalUnit;
extern CFStringRef kSecPolicyCheckSubjectCommonNamePrefix;
extern CFStringRef kSecPolicyCheckChainLength;
extern CFStringRef kSecPolicyCheckNotValidBefore;
extern CFStringRef kSecPolicyCheckEAPTrustedServerNames;
extern CFStringRef kSecPolicyCheckCertificatePolicy;
extern CFStringRef kSecPolicyCheckBasicCertificateProcessing;
extern CFStringRef kSecPolicyCheckExtendedValidation;
extern CFStringRef kSecPolicyCheckRevocation;
extern CFStringRef kSecPolicyCheckNoNetworkAccess;
extern CFStringRef kSecPolicyCheckBlackListedLeaf;
extern CFStringRef kSecPolicyCheckBlackListedKey;
extern CFStringRef kSecPolicyCheckGrayListedLeaf;
extern CFStringRef kSecPolicyCheckLeafMarkerOid;
extern CFStringRef kSecPolicyCheckIntermediateMarkerOid;
extern CFStringRef kSecPolicyCheckGrayListedKey;

SecPolicyRef SecPolicyCreate(CFStringRef oid, CFDictionaryRef options);

xpc_object_t SecPolicyArrayCopyXPCArray(CFArrayRef policies, CFErrorRef *error);
CFArrayRef SecPolicyXPCArrayCopyArray(xpc_object_t xpc_policies, CFErrorRef *error);

__END_DECLS

#endif /* !_SECURITY_SECPOLICYINTERNAL_H_ */
