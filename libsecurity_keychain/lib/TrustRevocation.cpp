/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
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

/* 
* TrustRevocation.cpp - private revocation policy manipulation
*/

#include <security_keychain/Trust.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/simpleprefs.h>
#include <CoreFoundation/CFData.h>
#include "SecBridge.h"
#include <Security/cssmapplePriv.h>
#include <Security/oidsalg.h>

/* 
 * These may go into an SPI header for the SecTrust object.
 */
typedef enum {
	/* this revocation policy disabled */
	kSecDisabled,			
	/* try, but tolerate inability to complete */
	kSecBestAttempt,	
	/* require successful revocation check if certificate indicates
     * the policy is supported */
	kSecRequireIfPresentInCertificate,
	/* require for every cert */
	kSecRequireForAllCertificates
} SecRevocationPolicyStyle;

using namespace KeychainCore;

/* 
 * Given an app-specified array of Policies, determine if at least one of them
 * is an explicit revocation policy.
 */
bool Trust::revocationPolicySpecified(CFArrayRef policies)
{
	if(policies == NULL) {
		return false;
	}
	CFIndex numPolicies = CFArrayGetCount(policies);
	for(CFIndex dex=0; dex<numPolicies; dex++) {
		SecPolicyRef secPol = (SecPolicyRef)CFArrayGetValueAtIndex(policies, dex);
		SecPointer<Policy> pol = Policy::required(SecPolicyRef(secPol));
		const CssmOid &oid = pol->oid();
		if(oid == CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_CRL)) {
			return true;
		}
		if(oid == CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_OCSP)) {
			return true;
		}
	}	
	return false;
}

CFMutableArrayRef Trust::addSpecifiedRevocationPolicies(
	uint32 &numAdded, 
	Allocator &alloc)
{
	/* policies specified by SPI not implemented */
	return NULL;
}

void Trust::freeSpecifiedRevocationPolicies(
	CFArrayRef policies,
	uint32 numAdded, 
	Allocator &alloc)
{
	/* shouldn't be called */
	MacOSError::throwMe(unimpErr);
}

static SecRevocationPolicyStyle parseRevStyle(CFStringRef val)
{
	if(CFEqual(val, kSecRevocationOff)) {
		return kSecDisabled;
	}
	else if(CFEqual(val, kSecRevocationBestAttempt)) {
		return kSecBestAttempt;
	}
	else if(CFEqual(val, kSecRevocationRequireIfPresent)) {
		return kSecRequireIfPresentInCertificate;
	}
	else if(CFEqual(val, kSecRevocationRequireForAll)) {
		return kSecRequireForAllCertificates;
	}
	else {
		return kSecDisabled;
	}
}

CFMutableArrayRef Trust::addPreferenceRevocationPolicies( 
	uint32 &numAdded, 
	Allocator &alloc)
{
	numAdded = 0;
	
	/* any per-user prefs? */
	Dictionary* pd = Dictionary::CreateDictionary(kSecRevocationDomain, Dictionary::US_User, true);
	if (pd)
	{
		if (!pd->dict()) {
			delete pd;
			pd = NULL;
		}
	}

	if(pd == NULL)
	{
		pd = Dictionary::CreateDictionary(kSecRevocationDomain, Dictionary::US_System, true);
		if (!pd->dict()) {
			delete pd;
		}

		return NULL;
	}
	
	auto_ptr<Dictionary> prefsDict(pd);
	
	bool doOcsp = false;
	bool doCrl = false;
	CFStringRef val;
	SecRevocationPolicyStyle ocspStyle = kSecDisabled;
	SecRevocationPolicyStyle crlStyle = kSecDisabled;
	SecPointer<Policy> ocspPolicy;
	SecPointer<Policy> crlPolicy;
	
	/* Are any revocation policies enabled? */
	val = prefsDict->getStringValue(kSecRevocationOcspStyle);
	if(val != NULL) {
		ocspStyle = parseRevStyle(val);
		if(ocspStyle != kSecDisabled) {
			doOcsp = true;
		}
	}
	val = prefsDict->getStringValue(kSecRevocationCrlStyle);
	if(val != NULL) {
		crlStyle = parseRevStyle(val);
		if(crlStyle != kSecDisabled) {
			doCrl = true;
		}
	}
	if(!doCrl && !doOcsp) {
		return NULL;
	}
	
	/* which policy first? */
	bool ocspFirst = true;		// default if both present
	if(doCrl && doOcsp) {
		val = prefsDict->getStringValue(kSecRevocationWhichFirst);
		if((val != NULL) && CFEqual(val, kSecRevocationCrlFirst)) {
			ocspFirst = false;
		}
	}

	/* We're adding something to mPolicies, so make a copy we can work with */
	CFMutableArrayRef policies = CFArrayCreateMutableCopy(NULL, 0, mPolicies);
	if(policies == NULL) {
		throw std::bad_alloc();
	}
	
	if(doOcsp) {
		/* Cook up a new Policy object */
		ocspPolicy = new Policy(mTP, CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_OCSP));
		CSSM_APPLE_TP_OCSP_OPTIONS opts;
		memset(&opts, 0, sizeof(opts));
		opts.Version = CSSM_APPLE_TP_OCSP_OPTS_VERSION;
		
		/* Now fill in the OCSP-related blanks */
		switch(ocspStyle) {
			case kSecDisabled:
				assert(0);
				break;
			case kSecBestAttempt:
				/* default, nothing to set */
				break;
			case kSecRequireIfPresentInCertificate:
				opts.Flags |= CSSM_TP_ACTION_OCSP_REQUIRE_IF_RESP_PRESENT;
				break;
			case kSecRequireForAllCertificates:
				opts.Flags |= CSSM_TP_ACTION_OCSP_REQUIRE_PER_CERT;
				break;
		}
		
		if(prefsDict->getBoolValue(kSecRevocationOCSPSufficientPerCert)) {
			opts.Flags |= CSSM_TP_ACTION_OCSP_SUFFICIENT;
		}
		
		val = prefsDict->getStringValue(kSecOCSPLocalResponder);
		if(val != NULL) {
			CFDataRef cfData = CFStringCreateExternalRepresentation(NULL,
				val, kCFStringEncodingUTF8, 0);
			CFIndex len = CFDataGetLength(cfData);
			opts.LocalResponder = (CSSM_DATA_PTR)alloc.malloc(sizeof(CSSM_DATA));
			opts.LocalResponder->Data = (uint8 *)alloc.malloc(len);
			opts.LocalResponder->Length = len;
			memmove(opts.LocalResponder->Data, CFDataGetBytePtr(cfData), len);
			CFRelease(cfData);
		}
		
		/* Policy manages its own copy of this data */
		CSSM_DATA optData = {sizeof(opts), (uint8 *)&opts};
		ocspPolicy->value() = optData;
		numAdded++;
	}
	
	if(doCrl) {
		/* Cook up a new Policy object */
		crlPolicy = new Policy(mTP, CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_CRL));
		CSSM_APPLE_TP_CRL_OPTIONS opts;
		memset(&opts, 0, sizeof(opts));
		opts.Version = CSSM_APPLE_TP_CRL_OPTS_VERSION;

		/* Now fill in the CRL-related blanks */
		opts.CrlFlags = CSSM_TP_ACTION_FETCH_CRL_FROM_NET;	// default true
		switch(crlStyle) {
			case kSecDisabled:
				assert(0);
				break;
			case kSecBestAttempt:
				/* default, nothing to set */
				break;
			case kSecRequireIfPresentInCertificate:
				opts.CrlFlags |= CSSM_TP_ACTION_REQUIRE_CRL_IF_PRESENT;
				break;
			case kSecRequireForAllCertificates:
				opts.CrlFlags |= CSSM_TP_ACTION_REQUIRE_CRL_PER_CERT;
				break;
		}
		if(prefsDict->getBoolValue(kSecRevocationCRLSufficientPerCert)) {
			opts.CrlFlags |= CSSM_TP_ACTION_CRL_SUFFICIENT;
		}

		/* Policy manages its own copy of this data */
		CSSM_DATA optData = {sizeof(opts), (uint8 *)&opts};
		crlPolicy->value() = optData;
		numAdded++;
	}
	
	/* append in order */
	if(doOcsp) {
		if(doCrl) {
			if(ocspFirst) {
				/* these SecCFObject go away when the policies array does */
				CFArrayAppendValue(policies, ocspPolicy->handle(false));
				CFArrayAppendValue(policies, crlPolicy->handle(false));
			}
			else {
				CFArrayAppendValue(policies, crlPolicy->handle(false));
				CFArrayAppendValue(policies, ocspPolicy->handle(false));
			}
		}
		else {
			CFArrayAppendValue(policies, ocspPolicy->handle(false));
		}

	}
	else {
		assert(doCrl);
		CFArrayAppendValue(policies, crlPolicy->handle(false));
	}
	return policies;
}

/*
 * Called when we created the last numAdded Policies in the specified Policy array 
 */
void Trust::freePreferenceRevocationPolicies(
	CFArrayRef policies,
	uint32 numAdded, 
	Allocator &alloc)
{
	uint32 numPolicies = (uint32)CFArrayGetCount(policies);
	if(numPolicies < numAdded) {
		/* should never happen - throw? */
		assert(0);
		return;
	}
	for(unsigned dex=numPolicies-numAdded; dex<numPolicies; dex++) {
		SecPolicyRef secPol = (SecPolicyRef)CFArrayGetValueAtIndex(policies, dex);
		//SecPointer<Policy> pol = Policy::required(SecPolicyRef(secPol));
		Policy *pol = Policy::required(secPol);
		const CssmOid &oid = pol->oid();		// required
		const CssmData &optData = pol->value();	// optional
		
		if(optData.Data) {
			if(oid == CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_CRL)) {
				/* currently no CRL-specific policy data */
			}
			else if(oid == CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_OCSP)) {
				CSSM_APPLE_TP_OCSP_OPTIONS *opts = (CSSM_APPLE_TP_OCSP_OPTIONS *)optData.Data;
				if(opts->LocalResponder != NULL) {
				   if(opts->LocalResponder->Data != NULL) {
						alloc.free(opts->LocalResponder->Data);
					}
					alloc.free(opts->LocalResponder);
				}
			}
			// managed by Policy alloc.free(optData.Data);
		}
	}
	CFRelease(policies);
}

/*
 * If OCSP is already in the mPolicies array, this makes sure the
 * CSSM_TP_ACTION_OCSP_REQUIRE_IF_RESP_PRESENT and CSSM_TP_ACTION_OCSP_SUFFICIENT
 * flags are set. If it's not already in the array, a new policy object is added.
 * Caller is responsible for releasing the returned policies array.
 */
CFMutableArrayRef Trust::forceOCSPRevocationPolicy( 
	uint32 &numAdded, 
	Allocator &alloc)
{
	SecPointer<Policy> ocspPolicy;
	CSSM_APPLE_TP_OCSP_OPT_FLAGS flags = CSSM_TP_ACTION_OCSP_REQUIRE_IF_RESP_PRESENT | CSSM_TP_ACTION_OCSP_SUFFICIENT;
	bool hasOcspPolicy = false;
	numAdded = 0;

	CFIndex numPolicies = (mPolicies) ? CFArrayGetCount(mPolicies) : 0;
	for(CFIndex dex=0; dex<numPolicies; dex++) {
		SecPolicyRef secPol = (SecPolicyRef)CFArrayGetValueAtIndex(mPolicies, dex);
		SecPointer<Policy> pol = Policy::required(SecPolicyRef(secPol));
		const CssmOid &oid = pol->oid();
		const CssmData &optData = pol->value();
		if(oid == CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_OCSP)) {
			// make sure flags are set correctly
			CSSM_APPLE_TP_OCSP_OPTIONS *opts = (CSSM_APPLE_TP_OCSP_OPTIONS *)optData.Data;
			if (opts) {
				opts->Flags |= flags;
			} else {
				CSSM_APPLE_TP_OCSP_OPTIONS newOpts;
				memset(&newOpts, 0, sizeof(newOpts));
				newOpts.Version = CSSM_APPLE_TP_OCSP_OPTS_VERSION;
				newOpts.Flags = flags;
				CSSM_DATA optData = {sizeof(newOpts), (uint8 *)&newOpts};
				pol->value() = optData;
			}
			hasOcspPolicy = true;
		}
	}	

	/* We're potentially adding something to mPolicies, so make a copy we can work with */
	CFMutableArrayRef policies = CFArrayCreateMutableCopy(NULL, 0, mPolicies);
	if(policies == NULL) {
		throw std::bad_alloc();
	}

	if(!hasOcspPolicy) {
		/* Cook up a new Policy object */
		ocspPolicy = new Policy(mTP, CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_OCSP));
		CSSM_APPLE_TP_OCSP_OPTIONS opts;
		memset(&opts, 0, sizeof(opts));
		opts.Version = CSSM_APPLE_TP_OCSP_OPTS_VERSION;
		opts.Flags = flags;
		
		/* Check prefs dict for local responder info */
		Dictionary *prefsDict = NULL;
		try { /* per-user prefs */
			prefsDict = Dictionary::CreateDictionary(kSecRevocationDomain, Dictionary::US_User, true);
			if (!prefsDict->dict()) {
				delete prefsDict;
				prefsDict = NULL;
			}
		}
		catch(...) {}
		if(prefsDict == NULL) {
			try { /* system prefs */
				prefsDict = Dictionary::CreateDictionary(kSecRevocationDomain, Dictionary::US_System, true);
				if (!prefsDict->dict()) {
					delete prefsDict;
					prefsDict = NULL;
				}
			}
			catch(...) {}
		}
		if(prefsDict != NULL) {
			CFStringRef val = prefsDict->getStringValue(kSecOCSPLocalResponder);
			if(val != NULL) {
				CFDataRef cfData = CFStringCreateExternalRepresentation(NULL,
					val, kCFStringEncodingUTF8, 0);
				CFIndex len = CFDataGetLength(cfData);
				opts.LocalResponder = (CSSM_DATA_PTR)alloc.malloc(sizeof(CSSM_DATA));
				opts.LocalResponder->Data = (uint8 *)alloc.malloc(len);
				opts.LocalResponder->Length = len;
				memmove(opts.LocalResponder->Data, CFDataGetBytePtr(cfData), len);
				CFRelease(cfData);
			}
		}

		/* Policy manages its own copy of the options data */
		CSSM_DATA optData = {sizeof(opts), (uint8 *)&opts};
		ocspPolicy->value() = optData;
		
		/* Policies array retains the Policy object */
		CFArrayAppendValue(policies, ocspPolicy->handle(false));
		numAdded++;
		
		if(prefsDict != NULL)
			delete prefsDict;
	}
	return policies;
}
