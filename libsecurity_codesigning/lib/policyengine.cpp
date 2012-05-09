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
#include <Security/SecTrustPriv.h>
#include <Security/SecCodeSigner.h>
#include <Security/cssmapplePriv.h>
#include <notify.h>
#include <security_utilities/unix++.h>
#include "quarantine++.h"

#include <CoreServices/CoreServicesPriv.h>
#undef check // Macro! Yech.

namespace Security {
namespace CodeSigning {

static const double NEGATIVE_HOLD = 60.0/86400;	// 60 seconds to cache negative outcomes


static void authorizeUpdate(SecCSFlags flags, CFDictionaryRef context);
static void normalizeTarget(CFRef<CFTypeRef> &target, CFDictionary &context, bool signUnsigned = false);


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
	
	const SecCSFlags validationFlags = kSecCSEnforceRevocationChecks;

	SQLite::Statement query(*this,
		"SELECT allow, requirement, id, label, expires, flags, disabled FROM scan_authority"
		" WHERE type = :type"
		" ORDER BY priority DESC;");
	query.bind(":type").integer(type);
	SQLite3::int64 latentID = 0;		// first (highest priority) disabled matching ID
	std::string latentLabel;			// ... and associated label, if any
	while (query.nextRow()) {
		bool allow = int(query[0]);
		const char *reqString = query[1];
		SQLite3::int64 id = query[2];
		const char *label = query[3];
		double expires = query[4];
		sqlite3_int64 ruleFlags = query[5];
		SQLite3::int64 disabled = query[6];
		
		CFRef<SecRequirementRef> requirement;
		MacOSError::check(SecRequirementCreateWithString(CFTempString(reqString), kSecCSDefaultFlags, &requirement.aref()));

		OSStatus rc = SecStaticCodeCheckValidity(code, validationFlags | kSecCSBasicValidateOnly, requirement);
		// ok, so this rule matches lets do a full validation if not overriding assessments
		if (rc == noErr && !overrideAssessment())
			rc = SecStaticCodeCheckValidity(code, validationFlags, requirement);

		switch (rc) {
		case noErr: // success
			break;
		case errSecCSUnsigned:
			cfadd(result, "{%O=#F}", kSecAssessmentAssessmentVerdict);
			addAuthority(result, "no usable signature");
			return;
		case errSecCSReqFailed: // requirement missed, but otherwise okay
			continue;
		default: // broken in some way; all tests will fail like this so bail out
			MacOSError::throwMe(rc);
		}
		if (disabled) {
			if (latentID == 0) {
				latentID = id;
				if (label)
					latentLabel = label;
			}
			continue;	// the loop
		}
	
		CFRef<CFDictionaryRef> info;	// as needed
		if (flags & kSecAssessmentFlagRequestOrigin) {
			if (!info)
				MacOSError::check(SecCodeCopySigningInformation(code, kSecCSSigningInformation, &info.aref()));
			if (CFArrayRef chain = CFArrayRef(CFDictionaryGetValue(info, kSecCodeInfoCertificates)))
				setOrigin(chain, result);
		}
		if (!(ruleFlags & kAuthorityFlagInhibitCache) && !(flags & kSecAssessmentFlagNoCache)) {	// cache inhibit
			if (!info)
				MacOSError::check(SecCodeCopySigningInformation(code, kSecCSSigningInformation, &info.aref()));
			if (SecTrustRef trust = SecTrustRef(CFDictionaryGetValue(info, kSecCodeInfoTrust))) {
				CFRef<CFDictionaryRef> xinfo;
				MacOSError::check(SecTrustCopyExtendedResult(trust, &xinfo.aref()));
				if (CFDateRef limit = CFDateRef(CFDictionaryGetValue(xinfo, kSecTrustExpirationDate))) {
					double julianLimit = CFDateGetAbsoluteTime(limit) / 86400.0 + 2451910.5;
					this->recordOutcome(code, allow, type, min(expires, julianLimit), id);
				}
			}
		}
		cfadd(result, "{%O=%B}", kSecAssessmentAssessmentVerdict, allow);
		addAuthority(result, label, id);
		return;
	}
	
	// no applicable authority. Deny by default
	if (flags & kSecAssessmentFlagRequestOrigin) {
		CFRef<CFDictionaryRef> info;	// as needed
		MacOSError::check(SecCodeCopySigningInformation(code, kSecCSSigningInformation, &info.aref()));
		if (CFArrayRef chain = CFArrayRef(CFDictionaryGetValue(info, kSecCodeInfoCertificates)))
			setOrigin(chain, result);
	}
	if (!(flags & kSecAssessmentFlagNoCache))
		this->recordOutcome(code, false, type, this->julianNow() + NEGATIVE_HOLD, latentID);
	cfadd(result, "{%O=%B}", kSecAssessmentAssessmentVerdict, false);
	addAuthority(result, latentLabel.c_str(), latentID);
}


//
// Installer archive.
// Certs passed from caller (untrusted), no policy engine yet, no caching (since untrusted).
// The current "policy" is to trust any proper signature.
//
static CFTypeRef installerPolicy();

void PolicyEngine::evaluateInstall(CFURLRef path, SecAssessmentFlags flags, CFDictionaryRef context, CFMutableDictionaryRef result)
{
	const AuthorityType type = kAuthorityInstall;
	
	SQLite3::int64 latentID = 0;		// first (highest priority) disabled matching ID
	std::string latentLabel;			// ... and associated label, if any
	Xar xar(cfString(path).c_str());
	if (xar) {
		if (!xar.isSigned()) {
			// unsigned xar
			cfadd(result, "{%O=%B}", kSecAssessmentAssessmentVerdict, false);
			addAuthority(result, "no usable signature");
			return;
		}
		if (CFRef<CFArrayRef> certs = xar.copyCertChain()) {
			CFRef<CFTypeRef> policy = installerPolicy();
			CFRef<SecTrustRef> trust;
			MacOSError::check(SecTrustCreateWithCertificates(certs, policy, &trust.aref()));
//			MacOSError::check(SecTrustSetAnchorCertificates(trust, cfEmptyArray())); // no anchors
			MacOSError::check(SecTrustSetOptions(trust, kSecTrustOptionAllowExpired | kSecTrustOptionImplicitAnchors));

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
				"SELECT allow, requirement, id, label, flags, disabled FROM scan_authority"
				" WHERE type = :type"
				" ORDER BY priority DESC;");
			query.bind(":type").integer(type);
			while (query.nextRow()) {
				bool allow = int(query[0]);
				const char *reqString = query[1];
				SQLite3::int64 id = query[2];
				const char *label = query[3];
				//sqlite_uint64 ruleFlags = query[4];
				SQLite3::int64 disabled = query[5];
		
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
				if (disabled) {
					if (latentID == 0) {
						latentID = id;
						if (label)
							latentLabel = label;
					}
					continue;	// the loop
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
	addAuthority(result, latentLabel.c_str(), latentID);
}


//
// Create a suitable policy array for verification of installer signatures.
//
static SecPolicyRef makeCRLPolicy()
{
	CFRef<SecPolicyRef> policy;
	MacOSError::check(SecPolicyCopy(CSSM_CERT_X_509v3, &CSSMOID_APPLE_TP_REVOCATION_CRL, &policy.aref()));
	CSSM_APPLE_TP_CRL_OPTIONS options;
	memset(&options, 0, sizeof(options));
	options.Version = CSSM_APPLE_TP_CRL_OPTS_VERSION;
	options.CrlFlags = CSSM_TP_ACTION_FETCH_CRL_FROM_NET | CSSM_TP_ACTION_CRL_SUFFICIENT;
	CSSM_DATA optData = { sizeof(options), (uint8 *)&options };
	MacOSError::check(SecPolicySetValue(policy, &optData));
	return policy.yield();
}

static SecPolicyRef makeOCSPPolicy()
{
	CFRef<SecPolicyRef> policy;
	MacOSError::check(SecPolicyCopy(CSSM_CERT_X_509v3, &CSSMOID_APPLE_TP_REVOCATION_OCSP, &policy.aref()));
	CSSM_APPLE_TP_OCSP_OPTIONS options;
	memset(&options, 0, sizeof(options));
	options.Version = CSSM_APPLE_TP_OCSP_OPTS_VERSION;
	options.Flags = CSSM_TP_ACTION_OCSP_SUFFICIENT;
	CSSM_DATA optData = { sizeof(options), (uint8 *)&options };
	MacOSError::check(SecPolicySetValue(policy, &optData));
	return policy.yield();
}

static CFTypeRef installerPolicy()
{
	CFRef<SecPolicyRef> base = SecPolicyCreateBasicX509();
	CFRef<SecPolicyRef> crl = makeCRLPolicy();
	CFRef<SecPolicyRef> ocsp = makeOCSPPolicy();
	return makeCFArray(3, base.get(), crl.get(), ocsp.get());
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
				addAuthority(result, "_XProtect");
			} else if (FileQuarantine(cfString(path).c_str()).flag(QTN_FLAG_ASSESSMENT_OK)) {
				cfadd(result, "{%O=#T}", kSecAssessmentAssessmentVerdict);
				addAuthority(result, "Prior Assessment");
			} else {
				cfadd(result, "{%O=#F}", kSecAssessmentAssessmentVerdict);
				addAuthority(result, "_XProtect");
			}
			addToAuthority(result, kLSDownloadRiskCategoryKey, riskCategory);
			return;
		}
	}
	// insufficient information from LS - deny by default
	cfadd(result, "{%O=#F}", kSecAssessmentAssessmentVerdict);
	addAuthority(result, "Insufficient Context");
}


//
// Result-creation helpers
//
void PolicyEngine::addAuthority(CFMutableDictionaryRef parent, const char *label, SQLite::int64 row, CFTypeRef cacheInfo)
{
	CFRef<CFMutableDictionaryRef> auth = makeCFMutableDictionary();
	if (label && label[0])
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
bool PolicyEngine::add(CFTypeRef inTarget, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context)
{
	// default type to execution
	if (type == kAuthorityInvalid)
		type = kAuthorityExecute;

	authorizeUpdate(flags, context);
	CFDictionary ctx(context, errSecCSInvalidAttributeValues);
	CFCopyRef<CFTypeRef> target = inTarget;
	
	if (type == kAuthorityOpenDoc) {
		// handle document-open differently: use quarantine flags for whitelisting
		if (!target || CFGetTypeID(target) != CFURLGetTypeID())
			MacOSError::throwMe(errSecCSInvalidObjectRef);
		std::string spath = cfString(target.as<CFURLRef>()).c_str();
		FileQuarantine qtn(spath.c_str());
		qtn.setFlag(QTN_FLAG_ASSESSMENT_OK);
		qtn.applyTo(spath.c_str());
		return true;
	}
	
	if (type == kAuthorityInstall) {
		return cfmake<CFDictionaryRef>("{%O=%O}", kSecAssessmentAssessmentAuthorityOverride, CFSTR("virtual install"));
	}
	
	// resolve URLs to Requirements
	normalizeTarget(target, ctx, true);
	
	// if we now have anything else, we're busted
	if (!target || CFGetTypeID(target) != SecRequirementGetTypeID())
		MacOSError::throwMe(errSecCSInvalidObjectRef);

	double priority = 0;
	string label;
	bool allow = true;
	double expires = never;
	string remarks;
	
	if (CFNumberRef pri = ctx.get<CFNumberRef>(kSecAssessmentUpdateKeyPriority))
		CFNumberGetValue(pri, kCFNumberDoubleType, &priority);
	if (CFStringRef lab = ctx.get<CFStringRef>(kSecAssessmentUpdateKeyLabel))
		label = cfString(lab);
	if (CFDateRef time = ctx.get<CFDateRef>(kSecAssessmentUpdateKeyExpires))
		// we're using Julian dates here; convert from CFDate
		expires = CFDateGetAbsoluteTime(time) / 86400.0 + 2451910.5;
	if (CFBooleanRef allowing = ctx.get<CFBooleanRef>(kSecAssessmentUpdateKeyAllow))
		allow = allowing == kCFBooleanTrue;
	if (CFStringRef rem = ctx.get<CFStringRef>(kSecAssessmentUpdateKeyRemarks))
		remarks = cfString(rem);

	CFRef<CFStringRef> requirementText;
	MacOSError::check(SecRequirementCopyString(target.as<SecRequirementRef>(), kSecCSDefaultFlags, &requirementText.aref()));
	SQLite::Transaction xact(*this, SQLite3::Transaction::deferred, "add_rule");
	SQLite::Statement insert(*this,
		"INSERT INTO authority (type, allow, requirement, priority, label, expires, remarks)"
		"	VALUES (:type, :allow, :requirement, :priority, :label, :expires, :remarks);");
	insert.bind(":type").integer(type);
	insert.bind(":allow").integer(allow);
	insert.bind(":requirement") = requirementText.get();
	insert.bind(":priority") = priority;
	if (!label.empty())
		insert.bind(":label") = label.c_str();
	insert.bind(":expires") = expires;
	if (!remarks.empty())
		insert.bind(":remarks") = remarks.c_str();
	insert.execute();
	this->purgeObjects(priority);
	xact.commit();
	notify_post(kNotifySecAssessmentUpdate);
	return true;
}


//
// Perform an action on existing authority rule(s)
//
bool PolicyEngine::manipulateRules(const std::string &stanza,
	CFTypeRef inTarget, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context)
{
	authorizeUpdate(flags, context);
	CFDictionary ctx(context, errSecCSInvalidAttributeValues);
	CFCopyRef<CFTypeRef> target = inTarget;
	normalizeTarget(target, ctx);

	string label;
	
	if (CFStringRef lab = ctx.get<CFStringRef>(kSecAssessmentUpdateKeyLabel))
		label = cfString(CFStringRef(lab));

	SQLite::Transaction xact(*this, SQLite3::Transaction::deferred, "rule_change");
	SQLite::Statement action(*this);
	if (!target) {
		if (label.empty())	// underspecified
			MacOSError::throwMe(errSecCSInvalidObjectRef);
		if (type == kAuthorityInvalid) {
			action.query(stanza + " WHERE label = :label");
		} else {
			action.query(stanza + " WHERE type = :type AND label = :label");
			action.bind(":type").integer(type);
		}
		action.bind(":label") = label.c_str();
	} else if (CFGetTypeID(target) == CFNumberGetTypeID()) {
		action.query(stanza + " WHERE id = :id");
		action.bind(":id").integer(cfNumber<uint64_t>(target.as<CFNumberRef>()));
	} else if (CFGetTypeID(target) == SecRequirementGetTypeID()) {
		if (type == kAuthorityInvalid)
			type = kAuthorityExecute;
		CFRef<CFStringRef> requirementText;
		MacOSError::check(SecRequirementCopyString(target.as<SecRequirementRef>(), kSecCSDefaultFlags, &requirementText.aref()));
		action.query(stanza + " WHERE type = :type AND requirement = :requirement");
		action.bind(":type").integer(type);
		action.bind(":requirement") = requirementText.get();
	} else
		MacOSError::throwMe(errSecCSInvalidObjectRef);

	action.execute();
	unsigned int changes = this->changes();	// latch change count
	// We MUST purge objects with priority <= MAX(priority of any changed rules);
	// but for now we just get lazy and purge them ALL.
	if (changes) {
		this->purgeObjects(1.0E100);
		xact.commit();
		notify_post(kNotifySecAssessmentUpdate);
		return true;
	}
	// no change; return an error
	MacOSError::throwMe(errSecCSNoMatches);
}


bool PolicyEngine::remove(CFTypeRef target, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context)
{
	if (type == kAuthorityOpenDoc) {
		// handle document-open differently: use quarantine flags for whitelisting
		authorizeUpdate(flags, context);
		if (!target || CFGetTypeID(target) != CFURLGetTypeID())
			MacOSError::throwMe(errSecCSInvalidObjectRef);
		std::string spath = cfString(CFURLRef(target)).c_str();
		FileQuarantine qtn(spath.c_str());
		qtn.clearFlag(QTN_FLAG_ASSESSMENT_OK);
		qtn.applyTo(spath.c_str());
		return true;
	}
	return manipulateRules("DELETE FROM authority", target, type, flags, context);
}

bool PolicyEngine::enable(CFTypeRef target, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context)
{
	return manipulateRules("UPDATE authority SET disabled = 0", target, type, flags, context);
}

bool PolicyEngine::disable(CFTypeRef target, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context)
{
	return manipulateRules("UPDATE authority SET disabled = 1", target, type, flags, context);
}


bool PolicyEngine::update(CFTypeRef target, SecAssessmentFlags flags, CFDictionaryRef context)
{
	AuthorityType type = typeFor(context, kAuthorityInvalid);
	CFStringRef edit = CFStringRef(CFDictionaryGetValue(context, kSecAssessmentContextKeyUpdate));
	if (CFEqual(edit, kSecAssessmentUpdateOperationAdd))
		return this->add(target, type, flags, context);
	else if (CFEqual(edit, kSecAssessmentUpdateOperationRemove))
		return this->remove(target, type, flags, context);
	else if (CFEqual(edit, kSecAssessmentUpdateOperationEnable))
		return this->enable(target, type, flags, context);
	else if (CFEqual(edit, kSecAssessmentUpdateOperationDisable))
		return this->disable(target, type, flags, context);
	else
		MacOSError::throwMe(errSecCSInvalidAttributeValues);
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
void PolicyEngine::recordOutcome(SecStaticCodeRef code, bool allow, AuthorityType type, double expires, int authority)
{
	CFRef<CFDictionaryRef> info;
	MacOSError::check(SecCodeCopySigningInformation(code, kSecCSDefaultFlags, &info.aref()));
	CFDataRef cdHash = CFDataRef(CFDictionaryGetValue(info, kSecCodeInfoUnique));
	assert(cdHash);		// was signed
	CFRef<CFURLRef> path;
	MacOSError::check(SecCodeCopyPath(code, kSecCSDefaultFlags, &path.aref()));
	assert(expires);
	SQLite::Transaction xact(*this, SQLite3::Transaction::deferred, "caching");
	SQLite::Statement insert(*this,
		"INSERT OR REPLACE INTO object (type, allow, hash, expires, path, authority)"
		"	VALUES (:type, :allow, :hash, :expires, :path,"
		"	CASE :authority WHEN 0 THEN (SELECT id FROM authority WHERE label = 'No Matching Rule') ELSE :authority END"
		"	);");
	insert.bind(":type").integer(type);
	insert.bind(":allow").integer(allow);
	insert.bind(":hash") = cdHash;
	insert.bind(":expires") = expires;
	insert.bind(":path") = cfString(path).c_str();
	insert.bind(":authority").integer(authority);
	insert.execute();
	xact.commit();
}


//
// Perform update authorization processing.
// Throws an exception if authorization is denied.
//
static void authorizeUpdate(SecCSFlags flags, CFDictionaryRef context)
{
	AuthorizationRef authorization = NULL;
	
	if (context)
		if (CFTypeRef authkey = CFDictionaryGetValue(context, kSecAssessmentUpdateKeyAuthorization))
			if (CFGetTypeID(authkey) == CFDataGetTypeID()) {
				CFDataRef authdata = CFDataRef(authkey);
				MacOSError::check(AuthorizationCreateFromExternalForm((AuthorizationExternalForm *)CFDataGetBytePtr(authdata), &authorization));
			}
	if (authorization == NULL)
		MacOSError::check(AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authorization));
	
	AuthorizationItem right[] = {
		{ "com.apple.security.assessment.update", 0, NULL, 0 }
	};
	AuthorizationRights rights = { sizeof(right) / sizeof(right[0]), right };
	MacOSError::check(AuthorizationCopyRights(authorization, &rights, NULL,
		kAuthorizationFlagExtendRights | kAuthorizationFlagInteractionAllowed, NULL));
	
	MacOSError::check(AuthorizationFree(authorization, kAuthorizationFlagDefaults));
}


//
// Perform common argument normalizations for update operations
//
static void normalizeTarget(CFRef<CFTypeRef> &target, CFDictionary &context, bool signUnsigned)
{
	// turn CFURLs into (designated) SecRequirements
	if (target && CFGetTypeID(target) == CFURLGetTypeID()) {
		CFRef<SecStaticCodeRef> code;
		MacOSError::check(SecStaticCodeCreateWithPath(target.as<CFURLRef>(), kSecCSDefaultFlags, &code.aref()));
		CFRef<SecRequirementRef> requirement;
		switch (OSStatus rc = SecCodeCopyDesignatedRequirement(code, kSecCSDefaultFlags, (SecRequirementRef *)&target.aref())) {
		case noErr:
			break;
		case errSecCSUnsigned:
			if (signUnsigned) {
				// Ad-hoc sign the code in the system database. This requires root privileges.
				CFRef<SecCodeSignerRef> signer;
				CFTemp<CFDictionaryRef> arguments("{%O=#N, %O=#N}", kSecCodeSignerDetached, kSecCodeSignerIdentity);
				MacOSError::check(SecCodeSignerCreate(arguments, kSecCSDefaultFlags, &signer.aref()));
				MacOSError::check(SecCodeSignerAddSignature(signer, code, kSecCSDefaultFlags));
				MacOSError::check(SecCodeCopyDesignatedRequirement(code, kSecCSDefaultFlags, (SecRequirementRef *)&target.aref()));
				break;
			}
			// fall through
		default:
			MacOSError::check(rc);
		}
		if (context.get(kSecAssessmentUpdateKeyRemarks) == NULL)	{
			// no explicit remarks; add one with the path
			CFRef<CFURLRef> path;
			MacOSError::check(SecCodeCopyPath(code, kSecCSDefaultFlags, &path.aref()));
			CFMutableDictionaryRef dict = makeCFMutableDictionary(context.get());
			CFDictionaryAddValue(dict, kSecAssessmentUpdateKeyRemarks, CFTempString(cfString(path)));
			context.take(dict);
		}
	}
}


} // end namespace CodeSigning
} // end namespace Security
