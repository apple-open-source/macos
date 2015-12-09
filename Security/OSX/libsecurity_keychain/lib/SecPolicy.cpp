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

SEC_CONST_DECL (kSecPolicyAppleX509Basic, "1.2.840.113635.100.1.2");
SEC_CONST_DECL (kSecPolicyAppleSSL, "1.2.840.113635.100.1.3");
SEC_CONST_DECL (kSecPolicyAppleSMIME, "1.2.840.113635.100.1.8");
SEC_CONST_DECL (kSecPolicyAppleEAP, "1.2.840.113635.100.1.9");
SEC_CONST_DECL (kSecPolicyAppleSWUpdateSigning, "1.2.840.113635.100.1.10");
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
#if TARGET_OS_IPHONE
SEC_CONST_DECL (kSecPolicyAppleOTAPKISigner, "1.2.840.113635.100.1.28");
SEC_CONST_DECL (kSecPolicyAppleTestOTAPKISigner, "1.2.840.113635.100.1.29");
/* FIXME: this policy name should be deprecated and replaced with "kSecPolicyAppleIDValidationRecordSigning" */
SEC_CONST_DECL (kSecPolicyAppleIDValidationRecordSigningPolicy, "1.2.840.113625.100.1.30");
SEC_CONST_DECL (kSecPolicyAppleSMPEncryption, "1.2.840.113625.100.1.31");
SEC_CONST_DECL (kSecPolicyAppleTestSMPEncryption, "1.2.840.113625.100.1.32");
#endif
SEC_CONST_DECL (kSecPolicyAppleServerAuthentication, "1.2.840.113635.100.1.33");
SEC_CONST_DECL (kSecPolicyApplePCSEscrowService, "1.2.840.113635.100.1.34");
SEC_CONST_DECL (kSecPolicyApplePPQSigning, "1.2.840.113625.100.1.35");
SEC_CONST_DECL (kSecPolicyAppleTestPPQSigning, "1.2.840.113625.100.1.36");
SEC_CONST_DECL (kSecPolicyAppleATVAppSigning, "1.2.840.113625.100.1.37");
SEC_CONST_DECL (kSecPolicyAppleTestATVAppSigning, "1.2.840.113625.100.1.38");
SEC_CONST_DECL (kSecPolicyApplePayIssuerEncryption, "1.2.840.113625.100.1.39");
SEC_CONST_DECL (kSecPolicyAppleOSXProvisioningProfileSigning, "1.2.840.113625.100.1.40");

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

extern "C" {
CFArrayRef SecPolicyCopyEscrowRootCertificates(void);
#if SECTRUST_OSX
CFStringRef SecPolicyGetOidString(SecPolicyRef policy);
CFDictionaryRef SecPolicyGetOptions(SecPolicyRef policy);
void SecPolicySetOptionsValue(SecPolicyRef policy, CFStringRef key, CFTypeRef value);
#endif
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

// TBD: have only one set of policy identifiers in SecPolicy.c so we can get rid of this
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
// CF boilerplate
//
#if !SECTRUST_OSX
CFTypeID
SecPolicyGetTypeID(void)
{
	BEGIN_SECAPI
	return gTypes().Policy.typeID;
	END_SECAPI1(_kCFRuntimeNotATypeID)
}
#endif

//
// Sec API bridge functions
//
/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus
SecPolicyGetOID(SecPolicyRef policyRef, CSSM_OID* oid)
{
#if !SECTRUST_OSX
	BEGIN_SECAPI
	Required(oid) = Policy::required(policyRef)->oid();
	END_SECAPI
#else
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
#endif
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

#if SECTRUST_OSX
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
#endif

/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus
SecPolicyGetValue(SecPolicyRef policyRef, CSSM_DATA* value)
{
#if !SECTRUST_OSX
	BEGIN_SECAPI
	Required(value) = Policy::required(policyRef)->value();
	END_SECAPI
#else
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
#endif
}

#if !SECTRUST_OSX
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
#endif

/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus
SecPolicySetValue(SecPolicyRef policyRef, const CSSM_DATA *value)
{
#if !SECTRUST_OSX
	BEGIN_SECAPI
	Required(value);
	const CssmData newValue(value->Data, value->Length);
	Policy::required(policyRef)->setValue(newValue);
	END_SECAPI
#else
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
	CFNumberRef cnum = NULL;
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
			CFOptionFlags revocationFlags = 0;
			if ((crlFlags & CSSM_TP_ACTION_FETCH_CRL_FROM_NET) == 0) {
				/* disable network access */
				revocationFlags |= kSecRevocationNetworkAccessDisabled;
			}
			if ((crlFlags & CSSM_TP_ACTION_CRL_SUFFICIENT) == 0) {
				/* if OCSP method is not sufficient, must use CRL */
				revocationFlags |= (kSecRevocationCRLMethod | kSecRevocationPreferCRL);
			} else {
				/* either method is sufficient */
				revocationFlags |= kSecRevocationUseAnyAvailableMethod;
			}
			if ((crlFlags & CSSM_TP_ACTION_REQUIRE_CRL_PER_CERT) != 0) {
				/* require a response */
				revocationFlags |= kSecRevocationRequirePositiveResponse;
			}
			cnum = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &revocationFlags);
			if (cnum) {
				SecPolicySetOptionsValue(policyRef, kSecPolicyRevocationFlags, cnum);
			}
		}
	}
	else {
		syslog(LOG_ERR, "SecPolicySetValue: unrecognized policy OID");
		status = errSecParam;
	}
	if (data) { CFRelease(data); }
	if (name) { CFRelease(name); }
	if (cnum) { CFRelease(cnum); }
	return status;
#endif
}

#if !SECTRUST_OSX
OSStatus
SecPolicySetProperties(SecPolicyRef policyRef, CFDictionaryRef properties)
{
	BEGIN_SECAPI
	Policy::required(policyRef)->setProperties(properties);
	END_SECAPI
}
#endif

/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus
SecPolicyGetTPHandle(SecPolicyRef policyRef, CSSM_TP_HANDLE* tpHandle)
{
#if !SECTRUST_OSX
	BEGIN_SECAPI
	Required(tpHandle) = Policy::required(policyRef)->tp()->handle();
	END_SECAPI
#else
	/* this function is unsupported in unified SecTrust */
#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecPolicyGetTPHandle was deprecated in 10.7, and does nothing in 10.11. Please stop using it.");
#endif
	return errSecServiceNotAvailable;
#endif
}

/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_3, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus
SecPolicyCopyAll(CSSM_CERT_TYPE certificateType, CFArrayRef* policies)
{
#if !SECTRUST_OSX
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
#else
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
#endif
}

/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_3, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus
SecPolicyCopy(CSSM_CERT_TYPE certificateType, const CSSM_OID *policyOID, SecPolicyRef* policy)
{
#if !SECTRUST_OSX
	Required(policy);
	Required(policyOID);
#else
	if (!policyOID || !policy) {
		return errSecParam;
	}
#endif
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
#if !SECTRUST_OSX
    return (SecPolicyRef)(policy ? CFRetain(policy) : NULL);
#else
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
#endif
}

#if !SECTRUST_OSX
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
#endif

#if !SECTRUST_OSX
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
#endif

#if !SECTRUST_OSX
/* not exported */
static SecPolicyRef
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
#endif

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
#if !SECTRUST_OSX
		if (!policy) {
			policy = SecPolicyCreateWithSecAsn1Oid((SecAsn1Oid*)oidPtr);
		}
#endif
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

#if !SECTRUST_OSX
/* new in 10.9 */
SecPolicyRef
SecPolicyCreateWithProperties(CFTypeRef policyIdentifier, CFDictionaryRef properties)
{
	SecPolicyRef policy = _SecPolicyCreateWithOID(policyIdentifier);
	SecPolicySetProperties(policy, properties);

	return policy;
}
#endif

#if !SECTRUST_OSX
/* new in 10.9 */
SecPolicyRef
SecPolicyCreateRevocation(CFOptionFlags revocationFlags)
{
	// return a SecPolicyRef object for the unified revocation policy
	SecAsn1Oid *oidPtr = (SecAsn1Oid*)&CSSMOID_APPLE_TP_REVOCATION;
	SecPolicyRef policy = SecPolicyCreateWithSecAsn1Oid(oidPtr);
	if (policy) {
		CSSM_DATA policyData = { (CSSM_SIZE)sizeof(CFOptionFlags), (uint8*)&revocationFlags };
		SecPolicySetValue(policy, &policyData);
	}
	return policy;
}
#endif

/* OS X only: deprecated SPI entry point */
/* new in 10.9 ***FIXME*** TO BE REMOVED */
CFArrayRef SecPolicyCopyEscrowRootCertificates(void)
{
	return SecCertificateCopyEscrowRoots(kSecCertificateProductionEscrowRoot);
}

SecPolicyRef SecPolicyCreateAppleIDSService(CFStringRef hostname)
{
    return SecPolicyCreateSSL(true, hostname);
}

SecPolicyRef SecPolicyCreateAppleIDSServiceContext(CFStringRef hostname, CFDictionaryRef __unused context)
{
    return SecPolicyCreateSSL(true, hostname);
}

SecPolicyRef SecPolicyCreateApplePushService(CFStringRef hostname, CFDictionaryRef __unused context)
{
    return SecPolicyCreateSSL(true, hostname);
}

SecPolicyRef SecPolicyCreateApplePushServiceLegacy(CFStringRef hostname)
{
    return SecPolicyCreateSSL(true, hostname);
}

SecPolicyRef SecPolicyCreateAppleMMCSService(CFStringRef hostname, CFDictionaryRef __unused context)
{
    return SecPolicyCreateSSL(true, hostname);
}

SecPolicyRef SecPolicyCreateAppleGSService(CFStringRef hostname, CFDictionaryRef __unused context)
{
    return SecPolicyCreateSSL(true, hostname);
}

SecPolicyRef SecPolicyCreateApplePPQService(CFStringRef hostname, CFDictionaryRef __unused context)
{
    return SecPolicyCreateSSL(true, hostname);
}

#if !SECTRUST_OSX
/* new in 10.11 */
SecPolicyRef SecPolicyCreateAppleATVAppSigning(void)
{
    return _SecPolicyCreateWithOID(kSecPolicyAppleX509Basic);
}
#endif

#if !SECTRUST_OSX
/* new in 10.11 */
SecPolicyRef SecPolicyCreateTestAppleATVAppSigning(void)
{
    return _SecPolicyCreateWithOID(kSecPolicyAppleX509Basic);
}
#endif

#if !SECTRUST_OSX
/* new in 10.11 */
SecPolicyRef SecPolicyCreateApplePayIssuerEncryption(void)
{
    return _SecPolicyCreateWithOID(kSecPolicyAppleX509Basic);
}
#endif

#if !SECTRUST_OSX
/* new in 10.11 */
SecPolicyRef SecPolicyCreateOSXProvisioningProfileSigning(void)
{
	return _SecPolicyCreateWithOID(kSecPolicyAppleOSXProvisioningProfileSigning);
}
#endif


#if !SECTRUST_OSX
/* new in 10.11 */
SecPolicyRef SecPolicyCreateAppleATVVPNProfileSigning(void)
{
    return _SecPolicyCreateWithOID(kSecPolicyAppleX509Basic);
}
#endif

#if !SECTRUST_OSX
SecPolicyRef SecPolicyCreateAppleSSLService(CFStringRef hostname)
{
	// SSL server, pinned to an Apple intermediate
	SecPolicyRef policy = SecPolicyCreateSSL(true, hostname);
	if (policy) {
		// change options for policy evaluation
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
		uint32 flags = 0x00000002; // 2nd-lowest bit set to require Apple intermediate pin
		CSSM_APPLE_TP_SSL_OPTIONS opts = {CSSM_APPLE_TP_SSL_OPTS_VERSION, hostnamelen, hostnamestr, flags};
		CSSM_DATA data = {sizeof(opts), (uint8*)&opts};
		SecPolicySetValue(policy, &data);
	}
	return policy;
}
#endif

/* OS X only: TBD */
#include <security_utilities/cfutilities.h>
/* New in 10.10 */
// Takes the "context" policies to extract the revocation and apply it to timeStamp.
CFArrayRef
SecPolicyCreateAppleTimeStampingAndRevocationPolicies(CFTypeRef policyOrArray)
{
#if !SECTRUST_OSX
    /* can't use SECAPI macros, since this function does not return OSStatus */
    CFArrayRef resultPolicyArray=NULL;
    try {
        // Set default policy
        CFRef<CFArrayRef> policyArray = cfArrayize(policyOrArray);
        CFRef<SecPolicyRef> defaultPolicy = _SecPolicyCreateWithOID(kSecPolicyAppleTimeStamping);
        CFRef<CFMutableArrayRef> appleTimeStampingPolicies = makeCFMutableArray(1,defaultPolicy.get());

        // Parse the policy and add revocation related ones
        CFIndex numPolicies = CFArrayGetCount(policyArray);
        for(CFIndex dex=0; dex<numPolicies; dex++) {
            SecPolicyRef secPol = (SecPolicyRef)CFArrayGetValueAtIndex(policyArray, dex);
            SecPointer<Policy> pol = Policy::required(SecPolicyRef(secPol));
            const CssmOid &oid = pol->oid();
            if ((oid == CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION))
                || (oid == CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_CRL))
                || (oid == CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_OCSP)))
            {
                CFArrayAppendValue(appleTimeStampingPolicies, secPol);
            }
        }
        // Transfer of ownership
        resultPolicyArray=appleTimeStampingPolicies.yield();
    }
    catch (...) {
        syslog(LOG_ERR, "SecPolicyCreateAppleTimeStampingAndRevocationPolicies: unable to create policy array");
        CFReleaseNull(resultPolicyArray);
    };
#else
    /* implement with unified SecPolicyRef instances */
	/* %%% FIXME revisit this since SecPolicyCreateWithOID is OSX-only; */
	/* should use SecPolicyCreateWithProperties instead */
    SecPolicyRef policy = NULL;
    CFMutableArrayRef resultPolicyArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateWithOID(kSecPolicyAppleTimeStamping);
    if (policy) {
        CFArrayAppendValue(resultPolicyArray, policy);
        CFReleaseNull(policy);
    }
    policy = SecPolicyCreateWithOID(kSecPolicyAppleRevocation);
    if (policy) {
        CFArrayAppendValue(resultPolicyArray, policy);
        CFReleaseNull(policy);
    }
#endif
    return resultPolicyArray;
}

