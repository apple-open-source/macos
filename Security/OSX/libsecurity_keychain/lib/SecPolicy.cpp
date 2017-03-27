
/*
 * Copyright (c) 2002-2015 Apple Inc. All Rights Reserved.
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
#include "utilities/SecCFRelease.h"
#include <syslog.h>


// String constant declarations

#define SEC_CONST_DECL(k,v) const CFStringRef k = CFSTR(v);

/* Some of these aren't defined in SecPolicy.c, but used here. */
SEC_CONST_DECL (kSecPolicyAppleiChat, "1.2.840.113635.100.1.12");

// Private functions

extern "C" {
CFDictionaryRef SecPolicyGetOptions(SecPolicyRef policy);
void SecPolicySetOptionsValue(SecPolicyRef policy, CFStringRef key, CFTypeRef value);
}

// String to CSSM_OID mapping

struct oidmap_entry_s {
		const CFTypeRef oidstr;
		const SecAsn1Oid *oidptr;
};
typedef struct oidmap_entry_s oidmap_entry_t;

// policies enumerated by SecPolicySearch (PolicyCursor.cpp)
/*
	static_cast<const CssmOid *>(&CSSMOID_APPLE_ISIGN), // no longer supported
	static_cast<const CssmOid *>(&CSSMOID_APPLE_X509_BASIC),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_SSL),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_SMIME),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_EAP),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_SW_UPDATE_SIGNING),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_IP_SEC),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_ICHAT), // no longer supported
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_RESOURCE_SIGN),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_PKINIT_CLIENT),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_PKINIT_SERVER),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_CODE_SIGNING),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_PACKAGE_SIGNING),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_REVOCATION_CRL),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_REVOCATION_OCSP),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_MACAPPSTORE_RECEIPT),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_APPLEID_SHARING),
	static_cast<const CssmOid *>(&CSSMOID_APPLE_TP_TIMESTAMPING),
*/
const oidmap_entry_t oidmap[] = {
	{ kSecPolicyAppleX509Basic, &CSSMOID_APPLE_X509_BASIC },
	{ kSecPolicyAppleSSL, &CSSMOID_APPLE_TP_SSL },
	{ kSecPolicyAppleSMIME, &CSSMOID_APPLE_TP_SMIME },
	{ kSecPolicyAppleEAP, &CSSMOID_APPLE_TP_EAP },
	{ kSecPolicyAppleSWUpdateSigning, &CSSMOID_APPLE_TP_SW_UPDATE_SIGNING },
	{ kSecPolicyAppleIPsec, &CSSMOID_APPLE_TP_IP_SEC },
	{ kSecPolicyAppleiChat, &CSSMOID_APPLE_TP_ICHAT },
	{ kSecPolicyApplePKINITClient, &CSSMOID_APPLE_TP_PKINIT_CLIENT },
	{ kSecPolicyApplePKINITServer, &CSSMOID_APPLE_TP_PKINIT_SERVER },
	{ kSecPolicyAppleCodeSigning, &CSSMOID_APPLE_TP_CODE_SIGNING },
	{ kSecPolicyApplePackageSigning, &CSSMOID_APPLE_TP_PACKAGE_SIGNING },
	{ kSecPolicyAppleIDValidation, &CSSMOID_APPLE_TP_APPLEID_SHARING },
	{ kSecPolicyMacAppStoreReceipt, &CSSMOID_APPLE_TP_MACAPPSTORE_RECEIPT },
	{ kSecPolicyAppleTimeStamping, &CSSMOID_APPLE_TP_TIMESTAMPING },
	{ kSecPolicyAppleRevocation, &CSSMOID_APPLE_TP_REVOCATION },
	{ kSecPolicyAppleRevocation, &CSSMOID_APPLE_TP_REVOCATION_OCSP },
	{ kSecPolicyAppleRevocation, &CSSMOID_APPLE_TP_REVOCATION_CRL },
	{ kSecPolicyApplePassbookSigning, &CSSMOID_APPLE_TP_PASSBOOK_SIGNING },
	{ kSecPolicyAppleMobileStore, &CSSMOID_APPLE_TP_MOBILE_STORE },
	{ kSecPolicyAppleEscrowService, &CSSMOID_APPLE_TP_ESCROW_SERVICE },
	{ kSecPolicyAppleProfileSigner, &CSSMOID_APPLE_TP_PROFILE_SIGNING },
	{ kSecPolicyAppleQAProfileSigner, &CSSMOID_APPLE_TP_QA_PROFILE_SIGNING },
	{ kSecPolicyAppleTestMobileStore, &CSSMOID_APPLE_TP_TEST_MOBILE_STORE },
	{ kSecPolicyApplePCSEscrowService, &CSSMOID_APPLE_TP_PCS_ESCROW_SERVICE },
	{ kSecPolicyAppleOSXProvisioningProfileSigning, &CSSMOID_APPLE_TP_PROVISIONING_PROFILE_SIGNING },
};

const oidmap_entry_t oidmap_priv[] = {
    { CFSTR("basicX509"), &CSSMOID_APPLE_X509_BASIC },
    { CFSTR("sslServer"), &CSSMOID_APPLE_TP_SSL },
    { CFSTR("sslClient"), &CSSMOID_APPLE_TP_SSL },
    { CFSTR("SMIME"), &CSSMOID_APPLE_TP_SMIME },
    { CFSTR("eapServer"), &CSSMOID_APPLE_TP_EAP },
    { CFSTR("eapClient"), &CSSMOID_APPLE_TP_EAP },
    { CFSTR("AppleSWUpdateSigning"), &CSSMOID_APPLE_TP_SW_UPDATE_SIGNING },
    { CFSTR("ipsecServer"), &CSSMOID_APPLE_TP_IP_SEC },
    { CFSTR("ipsecClient"), &CSSMOID_APPLE_TP_IP_SEC },
    { CFSTR("CodeSigning"), &CSSMOID_APPLE_TP_CODE_SIGNING },
    { CFSTR("PackageSigning"), &CSSMOID_APPLE_TP_PACKAGE_SIGNING },
    { CFSTR("AppleIDAuthority"), &CSSMOID_APPLE_TP_APPLEID_SHARING },
    { CFSTR("MacAppStoreReceipt"), &CSSMOID_APPLE_TP_MACAPPSTORE_RECEIPT },
    { CFSTR("AppleTimeStamping"), &CSSMOID_APPLE_TP_TIMESTAMPING },
    { CFSTR("revocation"), &CSSMOID_APPLE_TP_REVOCATION },
    { CFSTR("ApplePassbook"), &CSSMOID_APPLE_TP_PASSBOOK_SIGNING },
    { CFSTR("AppleMobileStore"), &CSSMOID_APPLE_TP_MOBILE_STORE },
    { CFSTR("AppleEscrowService"), &CSSMOID_APPLE_TP_ESCROW_SERVICE },
    { CFSTR("AppleProfileSigner"), &CSSMOID_APPLE_TP_PROFILE_SIGNING },
    { CFSTR("AppleQAProfileSigner"), &CSSMOID_APPLE_TP_QA_PROFILE_SIGNING },
    { CFSTR("AppleTestMobileStore"), &CSSMOID_APPLE_TP_TEST_MOBILE_STORE },
    { CFSTR("ApplePCSEscrowService"), &CSSMOID_APPLE_TP_PCS_ESCROW_SERVICE },
    { CFSTR("AppleOSXProvisioningProfileSigning"), &CSSMOID_APPLE_TP_PROVISIONING_PROFILE_SIGNING },
};

//
// Sec API bridge functions
//
/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus
SecPolicyGetOID(SecPolicyRef policyRef, CSSM_OID* oid)
{
	/* bridge to support old functionality */
	if (!policyRef) {
		return errSecParam;
	}
	CFStringRef oidStr = (CFStringRef) SecPolicyGetOidString(policyRef);
	if (!oidStr || !oid) {
		return errSecParam; // bad policy ref?
	}
	CSSM_OID *oidptr = NULL;
	unsigned int i, oidmaplen = sizeof(oidmap) / sizeof(oidmap_entry_t);
	for (i=0; i<oidmaplen; i++) {
		CFStringRef str = (CFStringRef) oidmap[i].oidstr;
		if (CFStringCompare(str, oidStr, 0) == kCFCompareEqualTo) {
			oidptr = (CSSM_OID*)oidmap[i].oidptr;
			break;
		}
	}
	if (!oidptr) {
		// Check private iOS policy names.
		oidmaplen = sizeof(oidmap_priv) / sizeof(oidmap_entry_t);
		for (i=0; i<oidmaplen; i++) {
			CFStringRef str = (CFStringRef) oidmap_priv[i].oidstr;
			if (CFStringCompare(str, oidStr, 0) == kCFCompareEqualTo) {
				oidptr = (CSSM_OID*)oidmap_priv[i].oidptr;
				break;
			}
		}
	}
	if (oidptr) {
		oid->Data = oidptr->Data;
		oid->Length = oidptr->Length;
		return errSecSuccess;
	}
    CFShow(oidStr);
	syslog(LOG_ERR, "WARNING: SecPolicyGetOID failed to return an OID. This function was deprecated in 10.7. Please use SecPolicyCopyProperties instead.");
	return errSecServiceNotAvailable;
}

// TODO: use a version of this function from a utility library
static CSSM_BOOL compareOids(
	const CSSM_OID *oid1,
	const CSSM_OID *oid2)
{
	if((oid1 == NULL) || (oid2 == NULL)) {
		return CSSM_FALSE;
	}
	if(oid1->Length != oid2->Length) {
		return CSSM_FALSE;
	}
	if(memcmp(oid1->Data, oid2->Data, oid1->Length)) {
		return CSSM_FALSE;
	}
	else {
		return CSSM_TRUE;
	}
}

/* OS X only: */
CFStringRef SecPolicyGetStringForOID(CSSM_OID* oid)
{
	if (!oid) {
		return NULL;
	}
	// given a CSSM_OID pointer, return corresponding string in oidmap
	unsigned int i, oidmaplen = sizeof(oidmap) / sizeof(oidmap_entry_t);
	for (i=0; i<oidmaplen; i++) {
		CSSM_OID* oidptr = (CSSM_OID*)oidmap[i].oidptr;
		if (compareOids(oid, oidptr)) {
			return (CFStringRef) oidmap[i].oidstr;
		}
	}
	return NULL;
}

static bool SecPolicyGetCSSMDataValueForString(SecPolicyRef policyRef, CFStringRef stringRef, CSSM_DATA* value)
{
	// Old API expects to vend a pointer and length for a policy value.
	// The API contract says this pointer is good for the life of the policy.
	// However, the new policy values are CF objects, and we need a separate
	// buffer to get their UTF8 bytes. This buffer needs to be released when
	// the policy object is released.

	CFDataRef data = NULL;
	CFIndex maxLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(stringRef), kCFStringEncodingUTF8) + 1;
	char* buf = (char*) malloc(maxLength);
	if (!buf) {
		return false;
	}
	if (CFStringGetCString(stringRef, buf, (CFIndex)maxLength, kCFStringEncodingUTF8)) {
		CFIndex length = strlen(buf);
		data = CFDataCreate(NULL, (const UInt8 *)buf, length);
	}
	free(buf);
	if (value) {
		value->Data = (uint8*)((data) ? CFDataGetBytePtr(data) : NULL);
		value->Length = (CSSM_SIZE)((data) ? CFDataGetLength(data) : 0);
	}
	if (data) {
		// stash this in a place where it will be released when the policy is destroyed
		if (policyRef) {
			SecPolicySetOptionsValue(policyRef, CFSTR("policy_data"), data);
			CFRelease(data);
		}
		else {
			syslog(LOG_ERR, "WARNING: policy dictionary not found to store returned data; will leak!");
		}
	}
	return true;
}

/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus
SecPolicyGetValue(SecPolicyRef policyRef, CSSM_DATA* value)
{
	/* bridge to support old functionality */
#if SECTRUST_DEPRECATION_WARNINGS
    syslog(LOG_ERR, "WARNING: SecPolicyGetValue was deprecated in 10.7. Please use SecPolicyCopyProperties instead.");
#endif
    if (!(policyRef && value)) {
		return errSecParam;
	}
	CFDictionaryRef options = SecPolicyGetOptions(policyRef);
	if (!(options && (CFDictionaryGetTypeID() == CFGetTypeID(options)))) {
		return errSecParam;
	}
	CFTypeRef name = NULL;
	do {
		if (CFDictionaryGetValueIfPresent(options, CFSTR("SSLHostname") /*kSecPolicyCheckSSLHostname*/,
			(const void **)&name) && name) {
			break;
		}
		if (CFDictionaryGetValueIfPresent(options, CFSTR("EAPTrustedServerNames") /*kSecPolicyCheckEAPTrustedServerNames*/,
			(const void **)&name) && name) {
			break;
		}
		if (CFDictionaryGetValueIfPresent(options, CFSTR("email") /*kSecPolicyCheckEmail*/,
			(const void **)&name) && name) {
			break;
		}
	} while (0);
	if (name) {
		CFTypeID typeID = CFGetTypeID(name);
		if (CFArrayGetTypeID() == typeID) {
			name = (CFStringRef) CFArrayGetValueAtIndex((CFArrayRef)name, 0);
		}
		SecPolicyGetCSSMDataValueForString(policyRef, (CFStringRef)name, value);
	}
	else {
		value->Data = NULL;
		value->Length = 0;
	}
	return errSecSuccess;
}

/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus
SecPolicySetValue(SecPolicyRef policyRef, const CSSM_DATA *value)
{
	/* bridge to support old functionality */
#if SECTRUST_DEPRECATION_WARNINGS
    syslog(LOG_ERR, "WARNING: SecPolicySetValue was deprecated in 10.7. Please use SecPolicySetProperties instead.");
#endif
	if (!(policyRef && value)) {
		return errSecParam;
	}
	OSStatus status = errSecSuccess;
	CFDataRef data = NULL;
	CFStringRef name = NULL;
	CFStringRef oid = (CFStringRef) SecPolicyGetOidString(policyRef);
	if (!oid) {
		syslog(LOG_ERR, "SecPolicySetValue: unknown policy OID");
		return errSecParam; // bad policy ref?
	}
	if (CFEqual(oid, CFSTR("sslServer") /*kSecPolicyOIDSSLServer*/) ||
		CFEqual(oid, CFSTR("sslClient") /*kSecPolicyOIDSSLClient*/) ||
		CFEqual(oid, CFSTR("ipsecServer") /*kSecPolicyOIDIPSecServer*/) ||
		CFEqual(oid, CFSTR("ipsecClient") /*kSecPolicyOIDIPSecClient*/) ||
		CFEqual(oid, kSecPolicyAppleSSL) ||
		CFEqual(oid, kSecPolicyAppleIPsec) ||
		CFEqual(oid, kSecPolicyAppleIDValidation)
		) {
		CSSM_APPLE_TP_SSL_OPTIONS *opts = (CSSM_APPLE_TP_SSL_OPTIONS *)value->Data;
		if (opts->Version == CSSM_APPLE_TP_SSL_OPTS_VERSION) {
			if (opts->ServerNameLen > 0) {
				data = CFDataCreate(NULL, (const UInt8 *)opts->ServerName, opts->ServerNameLen);
				name = (data) ? CFStringCreateFromExternalRepresentation(NULL, data, kCFStringEncodingUTF8) : NULL;
			}
		}
		if (name) {
			SecPolicySetOptionsValue(policyRef, CFSTR("SSLHostname") /*kSecPolicyCheckSSLHostname*/, name);
		}
		else {
			status = errSecParam;
		}
	}
	else if (CFEqual(oid, CFSTR("eapServer") /*kSecPolicyOIDEAPServer*/) ||
			 CFEqual(oid, CFSTR("eapClient") /*kSecPolicyOIDEAPClient*/) ||
			 CFEqual(oid, kSecPolicyAppleEAP)
		) {
		CSSM_APPLE_TP_SSL_OPTIONS *opts = (CSSM_APPLE_TP_SSL_OPTIONS *)value->Data;
		if (opts->Version == CSSM_APPLE_TP_SSL_OPTS_VERSION) {
			if (opts->ServerNameLen > 0) {
				data = CFDataCreate(NULL, (const UInt8 *)opts->ServerName, opts->ServerNameLen);
				name = (data) ? CFStringCreateFromExternalRepresentation(NULL, data, kCFStringEncodingUTF8) : NULL;
			}
		}
		if (name) {
			SecPolicySetOptionsValue(policyRef, CFSTR("EAPTrustedServerNames") /*kSecPolicyCheckEAPTrustedServerNames*/, name);
		}
		else {
			status = errSecParam;
		}
	}
	else if (CFEqual(oid, CFSTR("SMIME") /*kSecPolicyOIDSMIME*/) ||
			 CFEqual(oid, CFSTR("AppleShoebox") /*kSecPolicyOIDAppleShoebox*/) ||
			 CFEqual(oid, CFSTR("ApplePassbook") /*kSecPolicyOIDApplePassbook*/) ||
			 CFEqual(oid, kSecPolicyAppleSMIME) ||
			 CFEqual(oid, kSecPolicyApplePassbookSigning)
		) {
		CSSM_APPLE_TP_SMIME_OPTIONS *opts = (CSSM_APPLE_TP_SMIME_OPTIONS *)value->Data;
		if (opts->Version == CSSM_APPLE_TP_SMIME_OPTS_VERSION) {
            if (opts->SenderEmailLen > 0) {
				data = CFDataCreate(NULL, (const UInt8 *)opts->SenderEmail, opts->SenderEmailLen);
				name = (data) ? CFStringCreateFromExternalRepresentation(NULL, data, kCFStringEncodingUTF8) : NULL;
			}
		}
		if (name) {
			SecPolicySetOptionsValue(policyRef, CFSTR("email") /*kSecPolicyCheckEmail*/, name);
		}
		else {
			status = errSecParam;
		}
	}
	else if (CFEqual(oid, CFSTR("revocation") /* kSecPolicyOIDRevocation */) ||
			 CFEqual(oid, kSecPolicyAppleRevocation)
		) {
		CSSM_APPLE_TP_CRL_OPTIONS *opts = (CSSM_APPLE_TP_CRL_OPTIONS *)value->Data;
		if (opts->Version == CSSM_APPLE_TP_CRL_OPTS_VERSION) {
			CSSM_APPLE_TP_CRL_OPT_FLAGS crlFlags = opts->CrlFlags;
			if ((crlFlags & CSSM_TP_ACTION_FETCH_CRL_FROM_NET) == 0) {
				/* disable network access */
				SecPolicySetOptionsValue(policyRef, CFSTR("NoNetworkAccess") /*kSecPolicyCheckNoNetworkAccess*/, kCFBooleanTrue);
			}
			if ((crlFlags & CSSM_TP_ACTION_CRL_SUFFICIENT) == 0) {
				/* if CRL method is not sufficient, must use OCSP */
				SecPolicySetOptionsValue(policyRef, CFSTR("Revocation") /*kSecPolicyCheckRevocation*/,
                                         CFSTR("OCSP")/*kSecPolicyCheckRevocationOCSP*/);
			} else {
				/* either method is sufficient */
				SecPolicySetOptionsValue(policyRef, CFSTR("Revocation") /*kSecPolicyCheckRevocation*/,
                                         CFSTR("AnyRevocationMethod") /*kSecPolicyCheckRevocationAny*/);
			}

			if ((crlFlags & CSSM_TP_ACTION_REQUIRE_CRL_PER_CERT) != 0) {
				/* require a response */
				SecPolicySetOptionsValue(policyRef,
										 CFSTR("RevocationResponseRequired") /*kSecPolicyCheckRevocationResponseRequired*/,
										 kCFBooleanTrue);
			}
        }
	}
	else {
		syslog(LOG_ERR, "SecPolicySetValue: unrecognized policy OID");
		status = errSecParam;
	}
	if (data) { CFRelease(data); }
	if (name) { CFRelease(name); }
	return status;
}

/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus
SecPolicyGetTPHandle(SecPolicyRef policyRef, CSSM_TP_HANDLE* tpHandle)
{
	/* this function is unsupported in unified SecTrust */
#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecPolicyGetTPHandle was deprecated in 10.7, and does nothing in 10.11. Please stop using it.");
#endif
	return errSecServiceNotAvailable;
}

/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_3, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus
SecPolicyCopyAll(CSSM_CERT_TYPE certificateType, CFArrayRef* policies)
{
	/* bridge to support old functionality */
#if SECTRUST_DEPRECATION_WARNINGS
    syslog(LOG_ERR, "WARNING: SecPolicyCopyAll was deprecated in 10.7. Please use SecPolicy creation functions instead.");
#endif
    if (!policies) {
		return errSecParam;
	}
	CFMutableArrayRef curPolicies = CFArrayCreateMutable(NULL, 0, NULL);
	if (!curPolicies) {
		return errSecAllocate;
	}
	/* build the subset of policies which were supported on OS X,
	   and which are also implemented on iOS */
	CFStringRef supportedPolicies[] = {
		kSecPolicyAppleX509Basic, /* CSSMOID_APPLE_X509_BASIC */
		kSecPolicyAppleSSL, /* CSSMOID_APPLE_TP_SSL */
		kSecPolicyAppleSMIME, /* CSSMOID_APPLE_TP_SMIME */
		kSecPolicyAppleEAP, /*CSSMOID_APPLE_TP_EAP */
		kSecPolicyAppleSWUpdateSigning, /* CSSMOID_APPLE_TP_SW_UPDATE_SIGNING */
		kSecPolicyAppleIPsec, /* CSSMOID_APPLE_TP_IP_SEC */
		kSecPolicyAppleCodeSigning, /* CSSMOID_APPLE_TP_CODE_SIGNING */
		kSecPolicyMacAppStoreReceipt, /* CSSMOID_APPLE_TP_MACAPPSTORE_RECEIPT */
		kSecPolicyAppleIDValidation, /* CSSMOID_APPLE_TP_APPLEID_SHARING */
		kSecPolicyAppleTimeStamping, /* CSSMOID_APPLE_TP_TIMESTAMPING */
		kSecPolicyAppleRevocation, /* CSSMOID_APPLE_TP_REVOCATION_{CRL,OCSP} */
		NULL
	};
	CFIndex ix = 0;
	while (true) {
		CFStringRef policyID = supportedPolicies[ix++];
		if (!policyID) {
			break;
		}
		SecPolicyRef curPolicy = SecPolicyCreateWithProperties(policyID, NULL);
		if (curPolicy) {
			CFArrayAppendValue(curPolicies, curPolicy);
			CFRelease(curPolicy);
		}
	}
	*policies = CFArrayCreateCopy(NULL, curPolicies);
	CFRelease(curPolicies);
	return errSecSuccess;
}

/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_3, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus
SecPolicyCopy(CSSM_CERT_TYPE certificateType, const CSSM_OID *policyOID, SecPolicyRef* policy)
{
	if (!policyOID || !policy) {
		return errSecParam;
	}

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

/* OS X only: convert a new-world SecPolicyRef to an old-world ItemImpl instance */
SecPolicyRef
SecPolicyCreateItemImplInstance(SecPolicyRef policy)
{
	if (!policy) {
		return NULL;
	}
	CSSM_OID oid;
	OSStatus status = SecPolicyGetOID(policy, &oid);
	if (status) {
		return NULL;
	}
	SecPolicyRef policyRef = NULL;
	CFDictionaryRef properties = SecPolicyCopyProperties(policy);
	try {
		SecPointer<Policy> policyObj;
		PolicyCursor::policy(&oid, policyObj);
		policyRef = policyObj->handle();
		Policy::required(policyRef)->setProperties(properties);
	}
	catch (...) {
		policyRef = NULL;
	}
	if (properties) {
		CFRelease(properties);
	}
	return policyRef;
}

static SecPolicyRef
_SecPolicyCreateWithOID(CFTypeRef policyOID)
{
	// for now, we only accept the policy constants that are defined in SecPolicy.h
	CFStringRef oidStr = (CFStringRef)policyOID;
	CSSM_OID *oidPtr = NULL;
	SecPolicyRef policy = NULL;
	if (!oidStr) {
		return policy;
	}
	unsigned int i, oidmaplen = sizeof(oidmap) / sizeof(oidmap_entry_t);
	for (i=0; i<oidmaplen; i++) {
		CFStringRef str = (CFStringRef) oidmap[i].oidstr;
		if (CFStringCompare(str, oidStr, 0) == kCFCompareEqualTo) {
			oidPtr = (CSSM_OID*)oidmap[i].oidptr;
			break;
		}
	}
	if (CFEqual(oidStr, kSecPolicyAppleServerAuthentication)) {
		return SecPolicyCreateAppleSSLService(NULL);
	}
	if (oidPtr) {
		SecPolicySearchRef policySearch = NULL;
		OSStatus status = SecPolicySearchCreate(CSSM_CERT_X_509v3, oidPtr, NULL, &policySearch);
		if (!status && policySearch) {
			status = SecPolicySearchCopyNext(policySearch, &policy);
			if (status != errSecSuccess) {
				policy = NULL;
			}
			CFRelease(policySearch);
		}
		if (!policy && CFEqual(policyOID, kSecPolicyAppleRevocation)) {
			policy = SecPolicyCreateRevocation(kSecRevocationUseAnyAvailableMethod);
		}
	}
	return policy;
}

/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_7, __MAC_10_9, __IPHONE_NA, __IPHONE_NA) */
SecPolicyRef
SecPolicyCreateWithOID(CFTypeRef policyOID)
{
	SecPolicyRef policy = _SecPolicyCreateWithOID(policyOID);
	if (!policy) {
		syslog(LOG_ERR, "WARNING: SecPolicyCreateWithOID was unable to return the requested policy. This function was deprecated in 10.9. Please use supported SecPolicy creation functions instead.");
	}
	return policy;
}

/* OS X only: TBD */
#include <security_utilities/cfutilities.h>
/* New in 10.10 */
// Takes the "context" policies to extract the revocation and apply it to timeStamp.
CFArrayRef
SecPolicyCreateAppleTimeStampingAndRevocationPolicies(CFTypeRef policyOrArray)
{
    /* implement with unified SecPolicyRef instances */
    SecPolicyRef policy = NULL;
    CFMutableArrayRef resultPolicyArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (!resultPolicyArray) {
        return NULL;
    }
    policy = SecPolicyCreateWithProperties(kSecPolicyAppleTimeStamping, NULL);
    if (policy) {
        CFArrayAppendValue(resultPolicyArray, policy);
        CFReleaseNull(policy);
    }
    policy = SecPolicyCreateWithProperties(kSecPolicyAppleRevocation, NULL);
    if (policy) {
        CFArrayAppendValue(resultPolicyArray, policy);
        CFReleaseNull(policy);
    }
    return resultPolicyArray;
}

