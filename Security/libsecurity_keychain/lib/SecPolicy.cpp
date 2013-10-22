/*
 * Copyright (c) 2002-2013 Apple Inc. All Rights Reserved.
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

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFArray.h>
#include <Security/SecItem.h>
#include <Security/SecPolicy.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <security_keychain/Policies.h>
#include <security_keychain/PolicyCursor.h>
#include "SecBridge.h"


// String constant declarations

#define SEC_CONST_DECL(k,v) CFTypeRef k = (CFTypeRef)(CFSTR(v));

SEC_CONST_DECL (kSecPolicyAppleX509Basic, "1.2.840.113635.100.1.2");
SEC_CONST_DECL (kSecPolicyAppleSSL, "1.2.840.113635.100.1.3");
SEC_CONST_DECL (kSecPolicyAppleSMIME, "1.2.840.113635.100.1.8");
SEC_CONST_DECL (kSecPolicyAppleEAP, "1.2.840.113635.100.1.9");
SEC_CONST_DECL (kSecPolicyAppleIPsec, "1.2.840.113635.100.1.11");
SEC_CONST_DECL (kSecPolicyAppleiChat, "1.2.840.113635.100.1.12");
SEC_CONST_DECL (kSecPolicyApplePKINITClient, "1.2.840.113635.100.1.14");
SEC_CONST_DECL (kSecPolicyApplePKINITServer, "1.2.840.113635.100.1.15");
SEC_CONST_DECL (kSecPolicyAppleCodeSigning, "1.2.840.113635.100.1.16");
SEC_CONST_DECL (kSecPolicyApplePackageSigning, "1.2.840.113635.100.1.17");
SEC_CONST_DECL (kSecPolicyAppleIDValidation, "1.2.840.113635.100.1.18");
SEC_CONST_DECL (kSecPolicyMacAppStoreReceipt, "1.2.840.113635.100.1.19");
SEC_CONST_DECL (kSecPolicyAppleTimeStamping, "1.2.840.113635.100.1.20");
SEC_CONST_DECL (kSecPolicyAppleRevocation, "1.2.840.113635.100.1.21");
SEC_CONST_DECL (kSecPolicyApplePassbookSigning, "1.2.840.113635.100.1.22");
SEC_CONST_DECL (kSecPolicyAppleMobileStore, "1.2.840.113635.100.1.23");
SEC_CONST_DECL (kSecPolicyAppleEscrowService, "1.2.840.113635.100.1.24");
SEC_CONST_DECL (kSecPolicyAppleProfileSigner, "1.2.840.113635.100.1.25");
SEC_CONST_DECL (kSecPolicyAppleQAProfileSigner, "1.2.840.113635.100.1.26");
SEC_CONST_DECL (kSecPolicyAppleTestMobileStore, "1.2.840.113635.100.1.27");


SEC_CONST_DECL (kSecPolicyOid, "SecPolicyOid");
SEC_CONST_DECL (kSecPolicyName, "SecPolicyName");
SEC_CONST_DECL (kSecPolicyClient, "SecPolicyClient");
SEC_CONST_DECL (kSecPolicyRevocationFlags, "SecPolicyRevocationFlags");
SEC_CONST_DECL (kSecPolicyTeamIdentifier, "SecPolicyTeamIdentifier");

SEC_CONST_DECL (kSecPolicyKU_DigitalSignature, "CE_KU_DigitalSignature");
SEC_CONST_DECL (kSecPolicyKU_NonRepudiation, "CE_KU_NonRepudiation");
SEC_CONST_DECL (kSecPolicyKU_KeyEncipherment, "CE_KU_KeyEncipherment");
SEC_CONST_DECL (kSecPolicyKU_DataEncipherment, "CE_KU_DataEncipherment");
SEC_CONST_DECL (kSecPolicyKU_KeyAgreement, "CE_KU_KeyAgreement");
SEC_CONST_DECL (kSecPolicyKU_KeyCertSign, "CE_KU_KeyCertSign");
SEC_CONST_DECL (kSecPolicyKU_CRLSign, "CE_KU_CRLSign");
SEC_CONST_DECL (kSecPolicyKU_EncipherOnly, "CE_KU_EncipherOnly");
SEC_CONST_DECL (kSecPolicyKU_DecipherOnly, "CE_KU_DecipherOnly");

// Private functions

SecPolicyRef SecPolicyCreateWithSecAsn1Oid(SecAsn1Oid *oidPtr);
extern "C" { CFArrayRef SecPolicyCopyEscrowRootCertificates(void); }

//
// CF boilerplate
//
CFTypeID
SecPolicyGetTypeID(void)
{
	BEGIN_SECAPI
	return gTypes().Policy.typeID;
	END_SECAPI1(_kCFRuntimeNotATypeID)
}


//
// Sec API bridge functions
//
OSStatus
SecPolicyGetOID(SecPolicyRef policyRef, CSSM_OID* oid)
{
	BEGIN_SECAPI
	Required(oid) = Policy::required(policyRef)->oid();
	END_SECAPI
}

OSStatus
SecPolicyGetValue(SecPolicyRef policyRef, CSSM_DATA* value)
{
	BEGIN_SECAPI
	Required(value) = Policy::required(policyRef)->value();
	END_SECAPI
}

CFDictionaryRef
SecPolicyCopyProperties(SecPolicyRef policyRef)
{
	/* can't use SECAPI macros, since this function does not return OSStatus */
	CFDictionaryRef result = NULL;
	try {
		result = Policy::required(policyRef)->properties();
	}
	catch (...) {
		if (result) {
			CFRelease(result);
			result = NULL;
		}
	};
	return result;
}

OSStatus
SecPolicySetValue(SecPolicyRef policyRef, const CSSM_DATA *value)
{
	BEGIN_SECAPI
	Required(value);
	const CssmData newValue(value->Data, value->Length);
	Policy::required(policyRef)->setValue(newValue);
	END_SECAPI
}

OSStatus
SecPolicySetProperties(SecPolicyRef policyRef, CFDictionaryRef properties)
{
	BEGIN_SECAPI
	Policy::required(policyRef)->setProperties(properties);
	END_SECAPI
}

OSStatus
SecPolicyGetTPHandle(SecPolicyRef policyRef, CSSM_TP_HANDLE* tpHandle)
{
	BEGIN_SECAPI
	Required(tpHandle) = Policy::required(policyRef)->tp()->handle();
	END_SECAPI
}

OSStatus
SecPolicyCopyAll(CSSM_CERT_TYPE certificateType, CFArrayRef* policies)
{
	BEGIN_SECAPI
	Required(policies);
	CFMutableArrayRef currPolicies = NULL;
	currPolicies = CFArrayCreateMutable(NULL, 0, NULL);
	if ( currPolicies )
	{
		SecPointer<PolicyCursor> cursor(new PolicyCursor(NULL, NULL));
		SecPointer<Policy> policy;
		while ( cursor->next(policy) ) /* copies the next policy */
		{
			CFArrayAppendValue(currPolicies, policy->handle());  /* 'SecPolicyRef' appended */
			CFRelease(policy->handle()); /* refcount bumped up when appended to array */
		}
		*policies = CFArrayCreateCopy(NULL, currPolicies);
		CFRelease(currPolicies);
		CFRelease(cursor->handle());
	}
	END_SECAPI
}

OSStatus
SecPolicyCopy(CSSM_CERT_TYPE certificateType, const CSSM_OID *policyOID, SecPolicyRef* policy)
{
	Required(policy);
	Required(policyOID);

	SecPolicySearchRef srchRef = NULL;
	OSStatus ortn;

	ortn = SecPolicySearchCreate(certificateType, policyOID, NULL, &srchRef);
	if(ortn) {
		return ortn;
	}
	ortn = SecPolicySearchCopyNext(srchRef, policy);
	CFRelease(srchRef);
	return ortn;
}

/* new in 10.6 */
SecPolicyRef
SecPolicyCreateBasicX509(void)
{
	// return a SecPolicyRef object for the X.509 Basic policy
	SecPolicyRef policy = nil;
	SecPolicySearchRef policySearch = nil;
	OSStatus status = SecPolicySearchCreate(CSSM_CERT_X_509v3, &CSSMOID_APPLE_X509_BASIC, NULL, &policySearch);
	if (!status) {
		status = SecPolicySearchCopyNext(policySearch, &policy);
	}
	if (policySearch) {
		CFRelease(policySearch);
	}
	return policy;
}

/* new in 10.6 */
SecPolicyRef
SecPolicyCreateSSL(Boolean server, CFStringRef hostname)
{
	// return a SecPolicyRef object for the SSL policy, given hostname and client options
	SecPolicyRef policy = nil;
	SecPolicySearchRef policySearch = nil;
	OSStatus status = SecPolicySearchCreate(CSSM_CERT_X_509v3, &CSSMOID_APPLE_TP_SSL, NULL, &policySearch);
	if (!status) {
		status = SecPolicySearchCopyNext(policySearch, &policy);
	}
	if (!status && policy) {
		// set options for client-side or server-side policy evaluation
		char *strbuf = NULL;
		const char *hostnamestr = NULL;
		if (hostname) {
			hostnamestr = CFStringGetCStringPtr(hostname, kCFStringEncodingUTF8);
			if (hostnamestr == NULL) {
                CFIndex maxLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(hostname), kCFStringEncodingUTF8) + 1;
				strbuf = (char *)malloc(maxLen);
				if (CFStringGetCString(hostname, strbuf, maxLen, kCFStringEncodingUTF8)) {
					hostnamestr = strbuf;
				}
			}
		}
        uint32 hostnamelen = (hostnamestr) ? (uint32)strlen(hostnamestr) : 0;
        uint32 flags = (!server) ? CSSM_APPLE_TP_SSL_CLIENT : 0;
        CSSM_APPLE_TP_SSL_OPTIONS opts = {CSSM_APPLE_TP_SSL_OPTS_VERSION, hostnamelen, hostnamestr, flags};
        CSSM_DATA data = {sizeof(opts), (uint8*)&opts};
        SecPolicySetValue(policy, &data);

		if (strbuf) {
			free(strbuf);
		}
    }
	if (policySearch) {
		CFRelease(policySearch);
	}
	return policy;
}

SecPolicyRef
SecPolicyCreateWithSecAsn1Oid(SecAsn1Oid *oidPtr)
{
	SecPolicyRef policy = NULL;
	try {
		SecPointer<Policy> policyObj;
		PolicyCursor::policy(oidPtr, policyObj);
		policy = policyObj->handle();
	}
	catch (...) {}

	return policy;
}

/* new in 10.7 */
SecPolicyRef
SecPolicyCreateWithOID(CFTypeRef policyOID)
{
	// for now, we only accept the policy constants that are defined in SecPolicy.h
	CFStringRef oidStr = (CFStringRef)policyOID;
	CSSM_OID *oidPtr = NULL;
	SecPolicyRef policy = NULL;
	struct oidmap_entry_t {
		const CFTypeRef oidstr;
		const SecAsn1Oid *oidptr;
	};
	const oidmap_entry_t oidmap[] = {
		{ kSecPolicyAppleX509Basic, &CSSMOID_APPLE_X509_BASIC },
		{ kSecPolicyAppleSSL, &CSSMOID_APPLE_TP_SSL },
		{ kSecPolicyAppleSMIME, &CSSMOID_APPLE_TP_SMIME },
		{ kSecPolicyAppleEAP, &CSSMOID_APPLE_TP_EAP },
		{ kSecPolicyAppleIPsec, &CSSMOID_APPLE_TP_IP_SEC },
		{ kSecPolicyAppleiChat, &CSSMOID_APPLE_TP_ICHAT },
		{ kSecPolicyApplePKINITClient, &CSSMOID_APPLE_TP_PKINIT_CLIENT },
		{ kSecPolicyApplePKINITServer, &CSSMOID_APPLE_TP_PKINIT_SERVER },
		{ kSecPolicyAppleCodeSigning, &CSSMOID_APPLE_TP_CODE_SIGNING },
		{ kSecPolicyMacAppStoreReceipt, &CSSMOID_APPLE_TP_MACAPPSTORE_RECEIPT },
		{ kSecPolicyAppleIDValidation, &CSSMOID_APPLE_TP_APPLEID_SHARING },
		{ kSecPolicyAppleTimeStamping, &CSSMOID_APPLE_TP_TIMESTAMPING },
		{ kSecPolicyAppleRevocation, &CSSMOID_APPLE_TP_REVOCATION },
		{ kSecPolicyApplePassbookSigning, &CSSMOID_APPLE_TP_PASSBOOK_SIGNING },
		{ kSecPolicyAppleMobileStore, &CSSMOID_APPLE_TP_MOBILE_STORE },
		{ kSecPolicyAppleEscrowService, &CSSMOID_APPLE_TP_ESCROW_SERVICE },
		{ kSecPolicyAppleProfileSigner, &CSSMOID_APPLE_TP_PROFILE_SIGNING },
		{ kSecPolicyAppleQAProfileSigner, &CSSMOID_APPLE_TP_QA_PROFILE_SIGNING },
		{ kSecPolicyAppleTestMobileStore, &CSSMOID_APPLE_TP_TEST_MOBILE_STORE },
	};
	unsigned int i, oidmaplen = sizeof(oidmap) / sizeof(oidmap_entry_t);
	for (i=0; i<oidmaplen; i++) {
		CFStringRef str = (CFStringRef) oidmap[i].oidstr;
		if (CFStringCompare(str, oidStr, 0) == kCFCompareEqualTo) {
			oidPtr = (CSSM_OID*)oidmap[i].oidptr;
			break;
		}
	}
	if (oidPtr) {
		SecPolicySearchRef policySearch = NULL;
		OSStatus status = SecPolicySearchCreate(CSSM_CERT_X_509v3, oidPtr, NULL, &policySearch);
		if (!status && policySearch) {
			status = SecPolicySearchCopyNext(policySearch, &policy);
			CFRelease(policySearch);
		}
		if (!policy && CFEqual(policyOID, kSecPolicyAppleRevocation)) {
			policy = SecPolicyCreateRevocation(kSecRevocationUseAnyAvailableMethod);
		}
		if (!policy) {
			policy = SecPolicyCreateWithSecAsn1Oid((SecAsn1Oid*)oidPtr);
		}
	}
	return policy;
}

/* new in 10.9 */
SecPolicyRef
SecPolicyCreateWithProperties(CFTypeRef policyIdentifier, CFDictionaryRef properties)
{
	SecPolicyRef policy = SecPolicyCreateWithOID(policyIdentifier);
	SecPolicySetProperties(policy, properties);

	return policy;
}

/* new in 10.9 */
SecPolicyRef
SecPolicyCreateRevocation(CFOptionFlags revocationFlags)
{
	// return a SecPolicyRef object for the unified revocation policy
	SecAsn1Oid *oidPtr = (SecAsn1Oid*)&CSSMOID_APPLE_TP_REVOCATION;
	SecPolicyRef policy = SecPolicyCreateWithSecAsn1Oid(oidPtr);
	//%%% FIXME set policy value with revocationFlags

	return policy;
}

/* new in 10.9 ***FIXME*** TO BE REMOVED */
CFArrayRef SecPolicyCopyEscrowRootCertificates(void)
{
	return SecCertificateCopyEscrowRoots(kSecCertificateProductionEscrowRoot);
}

