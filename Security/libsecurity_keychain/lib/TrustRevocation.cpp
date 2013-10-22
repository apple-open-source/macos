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
 * matches the given policy OID.
 */
bool Trust::policySpecified(CFArrayRef policies, const CSSM_OID &inOid)
{
	if(policies == NULL) {
		return false;
	}
	CFIndex numPolicies = CFArrayGetCount(policies);
	for(CFIndex dex=0; dex<numPolicies; dex++) {
		SecPolicyRef secPol = (SecPolicyRef)CFArrayGetValueAtIndex(policies, dex);
		SecPointer<Policy> pol = Policy::required(SecPolicyRef(secPol));
		const CssmOid &oid = pol->oid();
		if(oid == CssmOid::overlay(inOid)) {
			return true;
		}
	}
	return false;
}

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

/*
 * Replace a unified revocation policy instance in the mPolicies array
 * with specific instances of the OCSP and/or CRL policies which the TP
 * module understands. Returns a (possibly) modified copy of the mPolicies
 * array, which the caller is responsible for releasing.
 */
CFMutableArrayRef Trust::convertRevocationPolicy(
	uint32 &numAdded,
	Allocator &alloc)
{
	numAdded = 0;
	if (!mPolicies) {
		return NULL;
	}
	CFIndex numPolicies = CFArrayGetCount(mPolicies);
	CFAllocatorRef allocator = CFGetAllocator(mPolicies);
	CFMutableArrayRef policies = CFArrayCreateMutableCopy(allocator, numPolicies, mPolicies);
	SecPolicyRef revPolicy = NULL;
	for(CFIndex dex=0; dex<numPolicies; dex++) {
		SecPolicyRef secPol = (SecPolicyRef)CFArrayGetValueAtIndex(policies, dex);
		SecPointer<Policy> pol = Policy::required(SecPolicyRef(secPol));
		const CssmOid &oid = pol->oid();
		if(oid == CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION)) {
			CFRetain(secPol);
			if (revPolicy)
				CFRelease(revPolicy);
			revPolicy = secPol;
			CFArrayRemoveValueAtIndex(policies, dex--);
			numPolicies--;
		}
	}
	if(!revPolicy) {
		CFRelease(policies);
		return NULL;
	}

	SecPointer<Policy> ocspPolicy;
	SecPointer<Policy> crlPolicy;

	// fetch policy value
	CFIndex policyValue = kSecRevocationUseAnyAvailableMethod; //%%%FIXME
	CFRelease(revPolicy); // all done with this policy reference
	if (policyValue & kSecRevocationOCSPMethod) {
		/* cook up a new Policy object */
		ocspPolicy = new Policy(mTP, CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_OCSP));
		CSSM_APPLE_TP_OCSP_OPT_FLAGS ocspFlags = CSSM_TP_ACTION_OCSP_SUFFICIENT;
		CSSM_APPLE_TP_OCSP_OPTIONS opts;
		memset(&opts, 0, sizeof(opts));
		opts.Version = CSSM_APPLE_TP_OCSP_OPTS_VERSION;
		opts.Flags = ocspFlags;

		/* Policy manages its own copy of this data */
		CSSM_DATA optData = {sizeof(opts), (uint8 *)&opts};
		ocspPolicy->value() = optData;

		/* Policies array retains the Policy object */
		CFArrayAppendValue(policies, ocspPolicy->handle(false));
		numAdded++;
	}
	if (policyValue & kSecRevocationCRLMethod) {
		/* cook up a new Policy object */
		crlPolicy = new Policy(mTP, CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_CRL));
		CSSM_APPLE_TP_CRL_OPT_FLAGS crlFlags = 0;
		CSSM_APPLE_TP_CRL_OPTIONS opts;
		memset(&opts, 0, sizeof(opts));
		opts.Version = CSSM_APPLE_TP_CRL_OPTS_VERSION;
		opts.CrlFlags = crlFlags;

		/* Policy manages its own copy of this data */
		CSSM_DATA optData = {sizeof(opts), (uint8 *)&opts};
		crlPolicy->value() = optData;

		/* Policies array retains the Policy object */
		CFArrayAppendValue(policies, crlPolicy->handle(false));
		numAdded++;
	}
	return policies;
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

CFDictionaryRef Trust::defaultRevocationSettings()
{
    /*
        defaults read ~/Library/Preferences/com.apple.security.revocation
        {
            CRLStyle = BestAttempt;
            CRLSufficientPerCert = 1;
            OCSPStyle = BestAttempt;
            OCSPSufficientPerCert = 1;
            RevocationFirst = OCSP;
        }
    */
    const void *keys[] = {
		kSecRevocationCrlStyle,
        kSecRevocationCRLSufficientPerCert,
        kSecRevocationOcspStyle,
        kSecRevocationOCSPSufficientPerCert,
        kSecRevocationWhichFirst
	};
	const void *values[] = {
		kSecRevocationBestAttempt,
		kCFBooleanTrue,
		kSecRevocationBestAttempt,
		kCFBooleanTrue,
		kSecRevocationOcspFirst
	};

    return CFDictionaryCreate(kCFAllocatorDefault, keys,
		values, sizeof(keys) / sizeof(*keys),
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
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
			pd = NULL;
		}
	}

    if(pd == NULL)
    {
        CFDictionaryRef tempDict = defaultRevocationSettings();
        if (tempDict == NULL)
            return NULL;

        pd = new Dictionary(tempDict);
        CFRelease(tempDict);
    }

	auto_ptr<Dictionary> prefsDict(pd);

	bool doOcsp = false;
	bool doCrl = false;
	CFStringRef val;
	SecRevocationPolicyStyle ocspStyle = kSecBestAttempt;
	SecRevocationPolicyStyle crlStyle = kSecBestAttempt;
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

	/* Must have at least one caller-specified policy
	 * (if they didn't specify any, it's a no-op evaluation, and if they wanted
	 * revocation checking only, that policy would already be in mPolicies) */
	if (!mPolicies || !CFArrayGetCount(mPolicies))
		return NULL;

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
 * (only frees the policy data associated with the extra policies that we inserted;
 * this does not free the policies array itself.)
 */
void Trust::freeAddedRevocationPolicyData(
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
}

/*
 * Comparator function to correctly order revocation policies.
 */
static CFComparisonResult compareRevocationPolicies(
	const void *policy1,
	const void *policy2,
	void *context)
{
	SecPointer<Policy> pol1 = Policy::required(SecPolicyRef(policy1));
	SecPointer<Policy> pol2 = Policy::required(SecPolicyRef(policy2));
	const CssmOid &oid1 = pol1->oid();
	const CssmOid &oid2 = pol2->oid();
	if(oid1 == oid2) {
		return kCFCompareEqualTo;
	}
	bool ocspFirst = true;
	if(context != NULL && CFEqual((CFBooleanRef)context, kCFBooleanFalse)) {
		ocspFirst = false;
	}
	const CssmOid lastRevocationOid = (ocspFirst) ?
		CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_CRL) :
		CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_OCSP);
	const CssmOid firstRevocationOid = (ocspFirst) ?
		CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_OCSP) :
		CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_CRL);
	if(oid1 == lastRevocationOid) {
		/* should be ordered last, after all other policies */
		return kCFCompareGreaterThan;
	}
	if(oid1 == firstRevocationOid) {
		/* should be ordered after any policy except lastRevocationOid */
		if(oid2 == lastRevocationOid) {
			return kCFCompareLessThan;
		}
		return kCFCompareGreaterThan;
	}
	/* normal policy in first position, anything else in second position */
	return kCFCompareLessThan;
}

/*
 * This method reorders any revocation policies which may be present
 * in the provided array so they are at the end and evaluated last.
 */
void Trust::orderRevocationPolicies(
	CFMutableArrayRef policies)
{
	if(!policies || CFGetTypeID(policies) != CFArrayGetTypeID()) {
		return;
	}
	/* check revocation prefs to determine which policy goes first */
	CFBooleanRef ocspFirst = kCFBooleanTrue;
	Dictionary* pd = Dictionary::CreateDictionary(kSecRevocationDomain, Dictionary::US_User, true);
	if (pd) {
		if (!pd->dict()) {
			delete pd;
		} else {
			auto_ptr<Dictionary> prefsDict(pd);
			CFStringRef val = prefsDict->getStringValue(kSecRevocationWhichFirst);
			if((val != NULL) && CFEqual(val, kSecRevocationCrlFirst)) {
				ocspFirst = kCFBooleanFalse;
			}
		}
	}
#if POLICIES_DEBUG
	CFShow(policies); // before sort
	CFArraySortValues(policies, CFRangeMake(0, CFArrayGetCount(policies)), compareRevocationPolicies, (void*)ocspFirst);
	CFShow(policies); // after sort, to see what changed
	// check that policy order is what we expect
	CFIndex numPolicies = CFArrayGetCount(policies);
	for(CFIndex dex=0; dex<numPolicies; dex++) {
		SecPolicyRef secPol = (SecPolicyRef)CFArrayGetValueAtIndex(policies, dex);
		SecPointer<Policy> pol = Policy::required(SecPolicyRef(secPol));
		const CssmOid &oid = pol->oid();
		if(oid == CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_OCSP)) {
			CFStringRef s = CFStringCreateWithFormat(NULL, NULL, CFSTR("idx %d = OCSP"), dex);
			CFShow(s);
			CFRelease(s);
		}
		else if(oid == CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_CRL)) {
			CFStringRef s = CFStringCreateWithFormat(NULL, NULL, CFSTR("idx %d = CRL"), dex);
			CFShow(s);
			CFRelease(s);
		}
		else {
			CFStringRef s = CFStringCreateWithFormat(NULL, NULL, CFSTR("idx %d = normal"), dex);
			CFShow(s);
			CFRelease(s);
		}
	}
#else
	CFArraySortValues(policies, CFRangeMake(0, CFArrayGetCount(policies)), compareRevocationPolicies, (void*)ocspFirst);
#endif
}

/*
 * This method returns a copy of the mPolicies array which ensures that
 * revocation checking (preferably OCSP, otherwise CRL) will be attempted.
 *
 * If OCSP is already in the mPolicies array, this makes sure the
 * CSSM_TP_ACTION_OCSP_REQUIRE_IF_RESP_PRESENT and CSSM_TP_ACTION_OCSP_SUFFICIENT
 * flags are set. If it's not already in the array, a new policy object is added.
 *
 * If CRL is already in the mPolicies array, this makes sure the
 * CSSM_TP_ACTION_FETCH_CRL_FROM_NET and CSSM_TP_ACTION_CRL_SUFFICIENT flags are
 * set. If it's not already in the array, a new policy object is added.
 *
 * Caller is responsible for releasing the returned policies array.
 */
CFMutableArrayRef Trust::forceRevocationPolicies(
	uint32 &numAdded,
	Allocator &alloc,
	bool requirePerCert)
{
	SecPointer<Policy> ocspPolicy;
	SecPointer<Policy> crlPolicy;
	CSSM_APPLE_TP_OCSP_OPT_FLAGS ocspFlags;
	CSSM_APPLE_TP_CRL_OPT_FLAGS crlFlags;
	bool hasOcspPolicy = false;
	bool hasCrlPolicy = false;
	numAdded = 0;

	ocspFlags = CSSM_TP_ACTION_OCSP_SUFFICIENT;
	crlFlags = CSSM_TP_ACTION_FETCH_CRL_FROM_NET | CSSM_TP_ACTION_CRL_SUFFICIENT;
	if (requirePerCert) {
		ocspFlags |= CSSM_TP_ACTION_OCSP_REQUIRE_IF_RESP_PRESENT;
		crlFlags |= CSSM_TP_ACTION_REQUIRE_CRL_IF_PRESENT;
	}

	CFIndex numPolicies = (mPolicies) ? CFArrayGetCount(mPolicies) : 0;
	for(CFIndex dex=0; dex<numPolicies; dex++) {
		SecPolicyRef secPol = (SecPolicyRef)CFArrayGetValueAtIndex(mPolicies, dex);
		SecPointer<Policy> pol = Policy::required(SecPolicyRef(secPol));
		const CssmOid &oid = pol->oid();
		const CssmData &optData = pol->value();
		if(oid == CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_OCSP)) {
			// make sure OCSP options are set correctly
			CSSM_APPLE_TP_OCSP_OPTIONS *opts = (CSSM_APPLE_TP_OCSP_OPTIONS *)optData.Data;
			if (opts) {
				opts->Flags |= ocspFlags;
			} else {
				CSSM_APPLE_TP_OCSP_OPTIONS newOpts;
				memset(&newOpts, 0, sizeof(newOpts));
				newOpts.Version = CSSM_APPLE_TP_OCSP_OPTS_VERSION;
				newOpts.Flags = ocspFlags;
				CSSM_DATA optData = {sizeof(newOpts), (uint8 *)&newOpts};
				pol->value() = optData;
			}
			hasOcspPolicy = true;
		}
		else if(oid == CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_CRL)) {
			// make sure CRL options are set correctly
			CSSM_APPLE_TP_CRL_OPTIONS *opts = (CSSM_APPLE_TP_CRL_OPTIONS *)optData.Data;
			if (opts) {
				opts->CrlFlags |= crlFlags;
			} else {
				CSSM_APPLE_TP_CRL_OPTIONS newOpts;
				memset(&newOpts, 0, sizeof(newOpts));
				newOpts.Version = CSSM_APPLE_TP_CRL_OPTS_VERSION;
				newOpts.CrlFlags = crlFlags;
				CSSM_DATA optData = {sizeof(newOpts), (uint8 *)&newOpts};
				pol->value() = optData;
			}
			hasCrlPolicy = true;
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
		opts.Flags = ocspFlags;

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

	if(!hasCrlPolicy) {
		/* Cook up a new Policy object */
		crlPolicy = new Policy(mTP, CssmOid::overlay(CSSMOID_APPLE_TP_REVOCATION_CRL));
		CSSM_APPLE_TP_CRL_OPTIONS opts;
		memset(&opts, 0, sizeof(opts));
		opts.Version = CSSM_APPLE_TP_CRL_OPTS_VERSION;
		opts.CrlFlags = crlFlags;

		/* Policy manages its own copy of this data */
		CSSM_DATA optData = {sizeof(opts), (uint8 *)&opts};
		crlPolicy->value() = optData;

		/* Policies array retains the Policy object */
		CFArrayAppendValue(policies, crlPolicy->handle(false));
		numAdded++;
	}

	return policies;
}
