/*
 * Copyright (c) 2011 Apple Inc. All Rights Reserved.
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
#include "policyengine.h"
#include "xar++.h"
#include <security_utilities/cfmunge.h>
#include <Security/Security.h>
#include <Security/SecRequirementPriv.h>
#include <Security/SecPolicyPriv.h>

#include <CoreServices/CoreServicesPriv.h>
#undef check // Macro! Yech.

namespace Security {
namespace CodeSigning {

static const time_t NEGATIVE_HOLD = 60;	// seconds for negative cache entries


//
// Core structure
//
PolicyEngine::PolicyEngine()
	: PolicyDatabase(NULL, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
{
}

PolicyEngine::~PolicyEngine()
{ }


//
// Top-level evaluation driver
//
void PolicyEngine::evaluate(CFURLRef path, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context, CFMutableDictionaryRef result)
{
	switch (type) {
	case kAuthorityExecute:
		evaluateCode(path, flags, context, result);
		break;
	case kAuthorityInstall:
		evaluateInstall(path, flags, context, result);
		break;
	case kAuthorityOpenDoc:
		evaluateDocOpen(path, flags, context, result);
		break;
	default:
		MacOSError::throwMe(errSecCSInvalidAttributeValues);
		break;
	}
}


//
// Executable code.
// Read from disk, evaluate properly, cache as indicated. The whole thing, so far.
//
void PolicyEngine::evaluateCode(CFURLRef path, SecAssessmentFlags flags, CFDictionaryRef context, CFMutableDictionaryRef result)
{
	const AuthorityType type = kAuthorityExecute;
	
	CFRef<SecStaticCodeRef> code;
	MacOSError::check(SecStaticCodeCreateWithPath(path, kSecCSDefaultFlags, &code.aref()));
	
	if (flags & kSecAssessmentFlagRequestOrigin) {
		CFRef<CFDictionaryRef> info;
		MacOSError::check(SecCodeCopySigningInformation(code, kSecCSSigningInformation, &info.aref()));
		if (CFArrayRef chain = CFArrayRef(CFDictionaryGetValue(info, kSecCodeInfoCertificates)))
			setOrigin(chain, result);
	}
	
	SecCSFlags validationFlags = kSecCSDefaultFlags;
	if (overrideAssessment())	// we'll force the verdict to 'pass' at the end, so don't sweat validating code
		validationFlags = kSecCSBasicValidateOnly;

	SQLite::Statement query(*this,
		"SELECT allow, requirement, inhibit_cache, expires, id, label FROM authority WHERE type = ?1 ORDER BY priority DESC;");
	query.bind(1).integer(type);
	while (query.nextRow()) {
		bool allow = int(query[0]);
		const char *reqString = query[1];
		bool inhibit_cache = query[2];
		time_t expires = SQLite::int64(query[3]);
		SQLite3::int64 id = query[4];
		const char *label = query[5];
		
		if (expires && expires < time(NULL))	// no longer active
			continue;
		
		CFRef<SecRequirementRef> requirement;
		MacOSError::check(SecRequirementCreateWithString(CFTempString(reqString), kSecCSDefaultFlags, &requirement.aref()));
		switch (OSStatus rc = SecStaticCodeCheckValidity(code, validationFlags, requirement)) {
		case noErr: // success
			break;
		case errSecCSUnsigned:
			cfadd(result, "{%O=#F}", kSecAssessmentAssessmentVerdict);
			addAuthority(result, "no usable signature");
			return;
		case errSecCSSignatureFailed:
		case errSecCSSignatureInvalid:
		case errSecCSSignatureUnsupported:
		case errSecCSSignatureNotVerifiable:
			cfadd(result, "{%O=#F}", kSecAssessmentAssessmentVerdict);
			addAuthority(result, "invalid signature");
			return;
		case errSecCSReqFailed: // requirement missed, but otherwise okay
			continue;
		default: // broken in some way; all tests will fail like this so bail out
			MacOSError::throwMe(rc);
		}
		if (!inhibit_cache && !(flags & kSecAssessmentFlagNoCache))	// cache inhibit
				this->recordOutcome(code, allow, type, expires, id, label);
		cfadd(result, "{%O=%B}", kSecAssessmentAssessmentVerdict, allow);
		addAuthority(result, label, id);
		return;
	}
	
	// no applicable authority. Deny by default
	if (!(flags & kSecAssessmentFlagNoCache))
		this->recordOutcome(code, false, type, time(NULL) + NEGATIVE_HOLD, 0, NULL);
	cfadd(result, "{%O=%B}", kSecAssessmentAssessmentVerdict, false);
	addAuthority(result, NULL);
}


//
// Installer archive.
// Certs passed from caller (untrusted), no policy engine yet, no caching (since untrusted).
// The current "policy" is to trust any proper signature.
//
void PolicyEngine::evaluateInstall(CFURLRef path, SecAssessmentFlags flags, CFDictionaryRef context, CFMutableDictionaryRef result)
{
	const AuthorityType type = kAuthorityInstall;
	
	Xar xar(cfString(path).c_str());
	if (xar) {
		if (!xar.isSigned()) {
			// unsigned xar
			cfadd(result, "{%O=%B}", kSecAssessmentAssessmentVerdict, false);
			addAuthority(result, "no usable signature");
			return;
		}
		if (CFRef<CFArrayRef> certs = xar.copyCertChain()) {
			CFRef<SecPolicyRef> policy = SecPolicyCreateBasicX509();
			CFRef<SecTrustRef> trust;
			MacOSError::check(SecTrustCreateWithCertificates(certs, policy, &trust.aref()));
//			MacOSError::check(SecTrustSetAnchorCertificates(trust, cfEmptyArray())); // no anchors
			MacOSError::check(SecTrustSetOptions(trust, kSecTrustOptionImplicitAnchors));

			SecTrustResultType trustResult;
			MacOSError::check(SecTrustEvaluate(trust, &trustResult));
			CFRef<CFArrayRef> chain;
			CSSM_TP_APPLE_EVIDENCE_INFO *info;
			MacOSError::check(SecTrustGetResult(trust, &trustResult, &chain.aref(), &info));
			
			if (flags & kSecAssessmentFlagRequestOrigin)
				setOrigin(chain, result);
			
			switch (trustResult) {
			case kSecTrustResultProceed:
			case kSecTrustResultConfirm:
			case kSecTrustResultUnspecified:
				break;
			default:
				{
					OSStatus rc;
					MacOSError::check(SecTrustGetCssmResultCode(trust, &rc));
					MacOSError::throwMe(rc);
				}
			}

			SQLite::Statement query(*this,
				"SELECT allow, requirement, inhibit_cache, expires, id, label FROM authority WHERE type = ?1 ORDER BY priority DESC;");
			query.bind(1).integer(type);
			while (query.nextRow()) {
				bool allow = int(query[0]);
				const char *reqString = query[1];
				bool inhibit_cache = query[2];
				time_t expires = SQLite::int64(query[3]);
				SQLite3::int64 id = query[4];
				const char *label = query[5];
				
				if (expires && expires < time(NULL))	// no longer active
					continue;
		
				CFRef<SecRequirementRef> requirement;
				MacOSError::check(SecRequirementCreateWithString(CFTempString(reqString), kSecCSDefaultFlags, &requirement.aref()));
				switch (OSStatus rc = SecRequirementEvaluate(requirement, chain, NULL, kSecCSDefaultFlags)) {
				case noErr: // success
					break;
				case errSecCSReqFailed: // requirement missed, but otherwise okay
					continue;
				default: // broken in some way; all tests will fail like this so bail out
					MacOSError::throwMe(rc);
				}
				// not adding to the object cache - we could, but it's not likely to be worth it
				cfadd(result, "{%O=%B}", kSecAssessmentAssessmentVerdict, allow);
				addAuthority(result, label, id);
				return;
			}
		}
	}
	
	// no applicable authority. Deny by default
	cfadd(result, "{%O=#F}", kSecAssessmentAssessmentVerdict);
	addAuthority(result, NULL);
}


//
// LaunchServices-layer document open.
// We don't cache those at present. If we ever do, we need to authenticate CoreServicesUIAgent as the source of its risk assessment.
//
void PolicyEngine::evaluateDocOpen(CFURLRef path, SecAssessmentFlags flags, CFDictionaryRef context, CFMutableDictionaryRef result)
{
	if (context) {
		if (CFStringRef riskCategory = CFStringRef(CFDictionaryGetValue(context, kLSDownloadRiskCategoryKey))) {
			if (CFEqual(riskCategory, kLSRiskCategorySafe)
				|| CFEqual(riskCategory, kLSRiskCategoryNeutral)
				|| CFEqual(riskCategory, kLSRiskCategoryUnknown)
				|| CFEqual(riskCategory, kLSRiskCategoryMayContainUnsafeExecutable)) {
				cfadd(result, "{%O=#T}", kSecAssessmentAssessmentVerdict);
			} else {
				cfadd(result, "{%O=#F}", kSecAssessmentAssessmentVerdict);
			}
			addAuthority(result, "_XProtect");
			addToAuthority(result, kLSDownloadRiskCategoryKey, riskCategory);
			return;
		}
	}
	// insufficient information from LS - deny by default
	cfadd(result, "{%O=%F}", kSecAssessmentAssessmentVerdict);
	addAuthority(result, NULL);
}


void PolicyEngine::addAuthority(CFMutableDictionaryRef parent, const char *label, SQLite::int64 row, CFTypeRef cacheInfo)
{
	CFRef<CFMutableDictionaryRef> auth = makeCFMutableDictionary();
	if (label)
		cfadd(auth, "{%O=%s}", kSecAssessmentAssessmentSource, label);
	if (row)
		CFDictionaryAddValue(auth, kSecAssessmentAssessmentAuthorityRow, CFTempNumber(row));
	if (cacheInfo)
		CFDictionaryAddValue(auth, kSecAssessmentAssessmentFromCache, cacheInfo);
	CFDictionaryAddValue(parent, kSecAssessmentAssessmentAuthority, auth);
}

void PolicyEngine::addToAuthority(CFMutableDictionaryRef parent, CFStringRef key, CFTypeRef value)
{
	CFMutableDictionaryRef authority = CFMutableDictionaryRef(CFDictionaryGetValue(parent, kSecAssessmentAssessmentAuthority));
	assert(authority);
	CFDictionaryAddValue(authority, key, value);
}


//
// Add a rule to the policy database
//
bool PolicyEngine::add(CFURLRef path, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context)
{
	if (type != kAuthorityExecute)
		MacOSError::throwMe(errSecCSUnimplemented);

	double priority = 0;
	string label;
	if (context) {
		if (CFTypeRef pri = CFDictionaryGetValue(context, kSecAssessmentUpdateKeyPriority)) {
			if (CFGetTypeID(pri) != CFNumberGetTypeID())
				MacOSError::throwMe(errSecCSBadDictionaryFormat);
			CFNumberGetValue(CFNumberRef(pri), kCFNumberDoubleType, &priority);
		}
		if (CFTypeRef lab = CFDictionaryGetValue(context, kSecAssessmentUpdateKeyLabel)) {
			if (CFGetTypeID(lab) != CFStringGetTypeID())
				MacOSError::throwMe(errSecCSBadDictionaryFormat);
			label = cfString(CFStringRef(lab));
		}
	}

	CFRef<SecStaticCodeRef> code;
	MacOSError::check(SecStaticCodeCreateWithPath(path, kSecCSDefaultFlags, &code.aref()));
	CFRef<CFDictionaryRef> info;
	MacOSError::check(SecCodeCopySigningInformation(code, kSecCSDefaultFlags, &info.aref()));
	CFRef<SecRequirementRef> dr;
	MacOSError::check(SecCodeCopyDesignatedRequirement(code, kSecCSDefaultFlags, &dr.aref()));

	CFRef<CFStringRef> requirementText;
	MacOSError::check(SecRequirementCopyString(dr, kSecCSDefaultFlags, &requirementText.aref()));
	SQLite::Statement insert(*this,
		"INSERT INTO authority (type, allow, requirement, priority, label) VALUES (?1, ?2, ?3, ?4, ?5);");
	insert.bind(1).integer(type);
	insert.bind(2).integer(true);
	insert.bind(3) = requirementText.get();
	insert.bind(4) = priority;
	insert.bind(5) = label.c_str();
	insert.execute();
	return true;
}


//
// Fill in extra information about the originator of cryptographic credentials found - if any
//
void PolicyEngine::setOrigin(CFArrayRef chain, CFMutableDictionaryRef result)
{
	if (chain)
		if (CFArrayGetCount(chain) > 0)
			if (SecCertificateRef leaf = SecCertificateRef(CFArrayGetValueAtIndex(chain, 0)))
				if (CFStringRef summary = SecCertificateCopyLongDescription(NULL, leaf, NULL))
					CFDictionarySetValue(result, kSecAssessmentAssessmentOriginator, summary);
}


//
// Take an assessment outcome and record it in the object cache
//
void PolicyEngine::recordOutcome(SecStaticCodeRef code, bool allow, AuthorityType type, time_t expires, int authority, const char *label)
{
	CFRef<CFDictionaryRef> info;
	MacOSError::check(SecCodeCopySigningInformation(code, kSecCSDefaultFlags, &info.aref()));
	CFDataRef cdHash = CFDataRef(CFDictionaryGetValue(info, kSecCodeInfoUnique));
	CFRef<CFURLRef> path;
	MacOSError::check(SecCodeCopyPath(code, kSecCSDefaultFlags, &path.aref()));
	//@@@ should really be OR REPLACE IF EXPIRED... does it matter? @@@
	SQLite::Transaction xact(*this, SQLite3::Transaction::deferred, "caching");
	SQLite::Statement insert(*this,
		"INSERT OR REPLACE INTO object (type, allow, hash, expires, authority, label, path) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)");
	insert.bind(1).integer(type);
	insert.bind(2).integer(allow);
	insert.bind(3) = cdHash;
	if (expires)
		insert.bind(4).integer(expires);
	insert.bind(5).integer(authority);
	if (label)
		insert.bind(6) = label;
	insert.bind(7) = cfString(path).c_str();
	insert.execute();
	xact.commit();
}


} // end namespace CodeSigning
} // end namespace Security
