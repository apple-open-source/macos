/*
 * Copyright (c) 2011-2016 Apple Inc. All Rights Reserved.
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
#include "quarantine++.h"
#include "codesigning_dtrace.h"
#include <security_utilities/cfmunge.h>
#include <Security/Security.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecCodePriv.h>
#include <Security/SecRequirementPriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecCodeSigner.h>
#include <Security/cssmapplePriv.h>
#include <security_utilities/unix++.h>
#include <notify.h>

#include "diskrep.h"
#include "codedirectory.h"
#include "csutilities.h"
#include "notarization.h"
#include "StaticCode.h"

#include <CoreServices/CoreServicesPriv.h>
#include "SecCodePriv.h"
#undef check // Macro! Yech.

namespace Security {
namespace CodeSigning {

static const double NEGATIVE_HOLD = 60.0/86400;	// 60 seconds to cache negative outcomes

static const char RECORDER_DIR[] = "/tmp/gke-";		// recorder mode destination for detached signatures
enum {
	recorder_code_untrusted = 0,		// signed but untrusted
	recorder_code_adhoc = 1,			// unsigned; signature recorded
	recorder_code_unable = 2,			// unsigned; unable to record signature
};


static void authorizeUpdate(SecAssessmentFlags flags, CFDictionaryRef context);
static CFTypeRef installerPolicy() CF_RETURNS_RETAINED;


//
// Core structure
//
PolicyEngine::PolicyEngine()
	: PolicyDatabase(NULL, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOFOLLOW)
{
	try {
		mOpaqueAllowlist = new OpaqueAllowlist();
	} catch (...) {
		mOpaqueAllowlist = NULL;
		secerror("Failed opening the gkopaque database.");
	}
}

PolicyEngine::PolicyEngine(const char *path)
	: PolicyDatabase(path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOFOLLOW)
{
	try {
		mOpaqueAllowlist = new OpaqueAllowlist();
	} catch (...) {
		mOpaqueAllowlist = NULL;
		secerror("Failed opening the gkopaque database.");
	}
}

PolicyEngine::~PolicyEngine()
{
	delete mOpaqueAllowlist;
}


//
// Top-level evaluation driver
//
void PolicyEngine::evaluate(CFURLRef path, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context, CFMutableDictionaryRef result)
{
    // update GKE
    installExplicitSet(gkeAuthFile, gkeSigsFile);

    // find the global evaluation manager
    EvaluationManager *evaluationManager = EvaluationManager::globalManager();

    // perform the evaluation
    EvaluationTask *evaluationTask = evaluationManager->evaluationTask(this, path, type, flags, context, result);
    evaluationManager->finalizeTask(evaluationTask, flags, result);

    // if rejected, reset the automatic rearm timer
    if (CFDictionaryGetValue(result, kSecAssessmentAssessmentVerdict) == kCFBooleanFalse)
        resetRearmTimer("reject");
}


//
// Create GKE allowlist filter screens.
// These are strings that are used to determine quickly whether unsigned code may
// have a GKE-style allowlist entry in the authority database. The idea is to make
// up a decent hash quickly.
//
// Note: We continue to use SHA1 here for compatibility of existing GKE entries.
// These are a prescreen, backed up by code signature checks later on. Use of SHA1 here is not a security problem.
//
static std::string createAllowlistScreen(char type, const Byte *digest, size_t length)
{
        size_t cap = 2*length + 2;
	char buffer[cap];
	buffer[0] = type;
	for (size_t n = 0; n < length; n++) {
                snprintf(buffer + 1 + 2*n, cap - (1 + 2*n), "%02.2x", digest[n]);
        }
	return buffer;
}

static std::string createAllowlistScreen(SecStaticCodeRef code)
{
	DiskRep *rep = SecStaticCode::requiredStatic(code)->diskRep();
	std::string screen;
	if (CFRef<CFDataRef> info = rep->component(cdInfoSlot)) {
		// has an Info.plist - hash it
		SHA1 hash;
		hash.update(CFDataGetBytePtr(info), CFDataGetLength(info));
		SHA1::Digest digest;
		hash.finish(digest);
		return createAllowlistScreen('I', digest, sizeof(digest));
	} else if (CFRef<CFDataRef> repSpecific = rep->component(cdRepSpecificSlot)) {
		// has a rep-specific slot - hash that (this catches disk images cheaply)
		// got invented after SHA-1 deprecation, so we'll use SHA256, which is the new default
		CCHashInstance hash(kCCDigestSHA256);
		hash.update(CFDataGetBytePtr(repSpecific), CFDataGetLength(repSpecific));
		Byte digest[256/8];
		hash.finish(digest);
		return createAllowlistScreen('R', digest, sizeof(digest));
	} else if (rep->mainExecutableImage()) {
		// stand-alone Mach-O executables are always candidates
		return "N";
	} else {
		// if everything else fails, hash the (single) file
		SHA1 hash;
		hashFileData(rep->mainExecutablePath().c_str(), &hash);
		SHA1::Digest digest;
		hash.finish(digest);
		return createAllowlistScreen('M', digest, sizeof(digest));
	}
}


void PolicyEngine::evaluateCodeItem(SecStaticCodeRef code, CFURLRef path, AuthorityType type, SecAssessmentFlags flags, bool nested, CFMutableDictionaryRef result)
{
	
	SQLite::Statement query(*this,
		"SELECT allow, requirement, id, label, expires, flags, disabled, filter_unsigned, remarks FROM scan_authority"
		" WHERE type = :type"
		" ORDER BY priority DESC;");
	query.bind(":type").integer(type);
	
	SQLite3::int64 latentID = 0;		// first (highest priority) disabled matching ID
	std::string latentLabel;			// ... and associated label, if any

    secdebug("gk", "evaluateCodeItem type=%d flags=0x%x nested=%d path=%s", type, int(flags), nested, cfString(path).c_str());
	while (query.nextRow()) {
		bool allow = int(query[0]);
		const char *reqString = query[1];
		SQLite3::int64 id = query[2];
		const char *label = query[3];
		double expires = query[4];
		sqlite3_int64 ruleFlags = query[5];
		SQLite3::int64 disabled = query[6];
//		const char *filter = query[7];
//		const char *remarks = query[8];

		secdebug("gk", "considering rule %d(%s) requirement %s", int(id), label ? label : "UNLABELED", reqString);
		CFRef<SecRequirementRef> requirement;
		MacOSError::check(SecRequirementCreateWithString(CFTempString(reqString), kSecCSDefaultFlags, &requirement.aref()));
		switch (OSStatus rc = SecStaticCodeCheckValidity(code, kSecCSBasicValidateOnly | kSecCSCheckGatekeeperArchitectures, requirement)) {
		case errSecSuccess:
			break;						// rule match; process below
		case errSecCSReqFailed:
			continue;					// rule does not apply
		case errSecCSVetoed:
			return;						// nested code has failed to pass
		default:
			MacOSError::throwMe(rc);	// general error; pass to caller
		}
		
		// If this rule is disabled, do not continue any further and just continue iterating
		// until we find one that is enabled.
		if (disabled) {
			// ...but always record the first matching rule for informational purposes.
			if (latentID == 0) {
				latentID = id;
				latentLabel = label ? label : "";
			}
			continue;
		}

		// current rule is first rule (in priority order) that matched. Apply it
        secnotice("gk", "rule %d applies - allow=%d", int(id), allow);
		if (nested && allow)			// success, nothing to record
			return;

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
					this->recordOutcome(code, allow, type, min(expires, dateToJulian(limit)), id);
				}
			}
		}
		if (allow) {
			if (SYSPOLICY_ASSESS_OUTCOME_ACCEPT_ENABLED()) {
				if (!info)
					MacOSError::check(SecCodeCopySigningInformation(code, kSecCSSigningInformation, &info.aref()));
				CFDataRef cdhash = CFDataRef(CFDictionaryGetValue(info, kSecCodeInfoUnique));
				SYSPOLICY_ASSESS_OUTCOME_ACCEPT(cfString(path).c_str(), type, label, cdhash ? CFDataGetBytePtr(cdhash) : NULL);
			}
		} else {
			if (SYSPOLICY_ASSESS_OUTCOME_DENY_ENABLED() || SYSPOLICY_RECORDER_MODE_ENABLED()) {
				if (!info)
					MacOSError::check(SecCodeCopySigningInformation(code, kSecCSSigningInformation, &info.aref()));
				CFDataRef cdhash = CFDataRef(CFDictionaryGetValue(info, kSecCodeInfoUnique));
				std::string cpath = cfString(path);
				const void *hashp = cdhash ? CFDataGetBytePtr(cdhash) : NULL;
				SYSPOLICY_ASSESS_OUTCOME_DENY(cpath.c_str(), type, label, hashp);
				SYSPOLICY_RECORDER_MODE(cpath.c_str(), type, label, hashp, recorder_code_untrusted);
			}
		}
		cfadd(result, "{%O=%B}", kSecAssessmentAssessmentVerdict, allow);
		addAuthority(flags, result, label, id, NULL, false, ruleFlags);
		return;
	}
	
	// no applicable authority (but signed, perhaps temporarily). Deny by default
    secnotice("gk", "rejecting due to lack of matching active rule");
	CFRef<CFDictionaryRef> info;
	MacOSError::check(SecCodeCopySigningInformation(code, kSecCSSigningInformation, &info.aref()));
	if (flags & kSecAssessmentFlagRequestOrigin) {
		if (CFArrayRef chain = CFArrayRef(CFDictionaryGetValue(info, kSecCodeInfoCertificates)))
			setOrigin(chain, result);
	}
	if (SYSPOLICY_ASSESS_OUTCOME_DEFAULT_ENABLED() || SYSPOLICY_RECORDER_MODE_ENABLED()) {
		CFDataRef cdhash = CFDataRef(CFDictionaryGetValue(info, kSecCodeInfoUnique));
		const void *hashp = cdhash ? CFDataGetBytePtr(cdhash) : NULL;
		std::string cpath = cfString(path);
		SYSPOLICY_ASSESS_OUTCOME_DEFAULT(cpath.c_str(), type, latentLabel.c_str(), hashp);
		SYSPOLICY_RECORDER_MODE(cpath.c_str(), type, latentLabel.c_str(), hashp, 0);
	}
	if (!(flags & kSecAssessmentFlagNoCache))
		this->recordOutcome(code, false, type, this->julianNow() + NEGATIVE_HOLD, latentID);
	cfadd(result, "{%O=%B}", kSecAssessmentAssessmentVerdict, false);
	addAuthority(flags, result, latentLabel.c_str(), latentID);
}

CFDictionaryRef PolicyEngine::opaqueAllowlistValidationConditionsFor(SecStaticCodeRef code)
{
	 return (mOpaqueAllowlist != NULL) ? mOpaqueAllowlist->validationConditionsFor(code) : NULL;
}

bool PolicyEngine::opaqueAllowlistContains(SecStaticCodeRef code, SecAssessmentFeedback feedback, OSStatus reason)
{
	return (mOpaqueAllowlist != NULL) ? mOpaqueAllowlist->contains(code, feedback, reason) : false;
}

void PolicyEngine::opaqueAllowlistAdd(SecStaticCodeRef code)
{
	if (mOpaqueAllowlist) {
		mOpaqueAllowlist->add(code);
	}
}

void PolicyEngine::adjustValidation(SecStaticCodeRef code)
{
	CFRef<CFDictionaryRef> conditions = opaqueAllowlistValidationConditionsFor(code);
	SecStaticCodeSetValidationConditions(code, conditions);
}


bool PolicyEngine::temporarySigning(SecStaticCodeRef code, AuthorityType type, CFURLRef path, SecAssessmentFlags matchFlags)
{
    secnotice("gk", "temporarySigning type=%d matchFlags=0x%x path=%s", type, int(matchFlags), cfString(path).c_str());

    // see if we have a screened record to take matchFlags from
    std::string screen = createAllowlistScreen(code);
    SQLite::Statement query(*this,
        "SELECT flags FROM authority "
        "WHERE type = :type"
        " AND NOT flags & :flag"
        " AND CASE WHEN filter_unsigned IS NULL THEN remarks = :remarks ELSE filter_unsigned = :screen END");
    query.bind(":type").integer(type);
    query.bind(":flag").integer(kAuthorityFlagDefault);
    query.bind(":screen") = screen;
    query.bind(":remarks") = cfString(path);
    secdebug("gk", "match screen=%s", screen.c_str());
    if (query.nextRow())	// got a matching rule
        matchFlags = SQLite3::int64(query[0]);
    else if (matchFlags == 0)  // lazy and no match
        return false;
    secdebug("gk", "matchFlags found=0x%x", int(matchFlags));

	try {
		// ad-hoc sign the code and attach the signature
		CFRef<CFDataRef> signature = CFDataCreateMutable(NULL, 0);
		CFTemp<CFMutableDictionaryRef> arguments("{%O=%O, %O=#N, %O=%d}", kSecCodeSignerDetached, signature.get(), kSecCodeSignerIdentity,
			kSecCodeSignerDigestAlgorithm, (matchFlags & kAuthorityFlagAllowlistSHA256) ? kSecCodeSignatureHashSHA256 : kSecCodeSignatureHashSHA1);
		// for modern allowlist entries, neuter the identifier since it may be derived from the filename
		if (matchFlags & kAuthorityFlagAllowlistSHA256)
			CFDictionaryAddValue(arguments, kSecCodeSignerIdentifier, CFSTR("ADHOC"));
		CFRef<SecCodeSignerRef> signer;
		MacOSError::check(SecCodeSignerCreate(arguments, (matchFlags & kAuthorityFlagAllowlistV2) ? kSecCSSignOpaque : kSecCSSignV1, &signer.aref()));
		MacOSError::check(SecCodeSignerAddSignature(signer, code, kSecCSDefaultFlags));
		MacOSError::check(SecCodeSetDetachedSignature(code, signature, kSecCSDefaultFlags));
		
		SecRequirementRef dr = NULL;
		SecCodeCopyDesignatedRequirement(code, kSecCSDefaultFlags, &dr);
		CFStringRef drs = NULL;
		SecRequirementCopyString(dr, kSecCSDefaultFlags, &drs);
        secnotice("gk", "successfully created temporary signature - requirement=%s", cfString(drs).c_str());
        
		// if we're in GKE recording mode, save that signature and report its location
		if (SYSPOLICY_RECORDER_MODE_ENABLED()) {
			int status = recorder_code_unable;	// ephemeral signature (not recorded)
			if (geteuid() == 0) {
				CFRef<CFUUIDRef> uuid = CFUUIDCreate(NULL);
				std::string sigfile = RECORDER_DIR + cfStringRelease(CFUUIDCreateString(NULL, uuid)) + ".tsig";
				try {
					UnixPlusPlus::AutoFileDesc fd(sigfile, O_WRONLY | O_CREAT);
					fd.write(CFDataGetBytePtr(signature), CFDataGetLength(signature));
					status = recorder_code_adhoc;	// recorded signature
					SYSPOLICY_RECORDER_MODE_ADHOC_PATH(cfString(path).c_str(), type, sigfile.c_str());
				} catch (...) { }
			}

			// now report the D probe itself
			CFRef<CFDictionaryRef> info;
			MacOSError::check(SecCodeCopySigningInformation(code, kSecCSDefaultFlags, &info.aref()));
			CFDataRef cdhash = CFDataRef(CFDictionaryGetValue(info, kSecCodeInfoUnique));
			SYSPOLICY_RECORDER_MODE(cfString(path).c_str(), type, "",
				cdhash ? CFDataGetBytePtr(cdhash) : NULL, status);
		}
		
		return true;	// it worked; we're now (well) signed
	} catch (...) { }
	
	return false;
}


//
// Executable code.
// Read from disk, evaluate properly, cache as indicated.
//
void PolicyEngine::evaluateCode(CFURLRef path, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context, CFMutableDictionaryRef result, bool handleUnsigned)
{
	// not really a Gatekeeper function... but reject all "hard quarantined" files because they were made from sandboxed sources without download privilege
    if (type == kAuthorityExecute) {
        FileQuarantine qtn(cfString(path).c_str());
        if (qtn.flag(QTN_FLAG_HARD))
            MacOSError::throwMe(errSecCSFileHardQuarantined);
    }
	
	// hack: if caller passed a UTI, use that to turn off app-only checks for some well-known ones
	bool appOk = false;
	if (CFStringRef uti = CFStringRef(CFDictionaryGetValue(context, kSecAssessmentContextKeyUTI))) {
		appOk = CFEqual(uti, CFSTR("com.apple.systempreference.prefpane"))
			|| CFEqual(uti, CFSTR("com.apple.systempreference.screen-saver"))
			|| CFEqual(uti, CFSTR("com.apple.systempreference.screen-slide-saver"))
			|| CFEqual(uti, CFSTR("com.apple.menu-extra"));
	}
	
	CFCopyRef<SecStaticCodeRef> code;
	MacOSError::check(SecStaticCodeCreateWithPath(path, kSecCSDefaultFlags | kSecCSForceOnlineNotarizationCheck, &code.aref()));
	
	SecCSFlags validationFlags = kSecCSEnforceRevocationChecks | kSecCSCheckAllArchitectures | kSecCSAllowNetworkAccess;
	if (!(flags & kSecAssessmentFlagAllowWeak))
		validationFlags |= kSecCSStrictValidate;
	adjustValidation(code);
	
	// deal with a very special case (broken 10.6/10.7 Applet bundles)
	OSStatus rc = SecStaticCodeCheckValidity(code, validationFlags | kSecCSBasicValidateOnly, NULL);
	if (rc == errSecCSSignatureFailed) {
		MacOSError::throwMe(rc);
	}

	// ad-hoc sign unsigned code
    bool wasAdhocSigned = false;
	if (rc == errSecCSUnsigned && handleUnsigned && (!overrideAssessment(flags) || SYSPOLICY_RECORDER_MODE_ENABLED())) {
		if (temporarySigning(code, type, path, 0)) {
            wasAdhocSigned = true;
			rc = errSecSuccess;		// clear unsigned; we are now well-signed
			validationFlags |= kSecCSBasicValidateOnly;	// no need to re-validate deep contents
		}
	}
	
	// prepare for deep traversal of (hopefully) good signatures
	SecAssessmentFeedback feedback = SecAssessmentFeedback(CFDictionaryGetValue(context, kSecAssessmentContextKeyFeedback));
	__block CFRef<CFMutableDictionaryRef> nestedFailure = NULL;	// save a nested failure for later
	MacOSError::check(SecStaticCodeSetCallback(code, kSecCSDefaultFlags, NULL, ^CFTypeRef (SecStaticCodeRef item, CFStringRef cfStage, CFDictionaryRef info) {
		string stage = cfString(cfStage);
		if (stage == "prepared") {
			if (!CFEqual(item, code))	// genuine nested (not top) code
				adjustValidation(item);
		} else if (stage == "progress") {
			if (feedback && CFEqual(item, code)) {	// top level progress
				bool proceed = feedback(kSecAssessmentFeedbackProgress, info);
				if (!proceed)
					SecStaticCodeCancelValidation(code, kSecCSDefaultFlags);
			}
		} else if (stage == "validated") {
			SecStaticCodeSetCallback(item, kSecCSDefaultFlags, NULL, NULL);		// clear callback to avoid unwanted recursion
			evaluateCodeItem(item, path, type, flags, item != code, result);
			if (CFTypeRef verdict = CFDictionaryGetValue(result, kSecAssessmentAssessmentVerdict))
				if (CFEqual(verdict, kCFBooleanFalse)) {
					if (item == code)
						return makeCFNumber(OSStatus(errSecCSVetoed));  // (signal nested-code policy failure, picked up below)
					// nested code policy failure; save, reset, and continue
					if (!nestedFailure)
						nestedFailure = CFMutableDictionaryRef(CFDictionaryGetValue(result, kSecAssessmentAssessmentAuthority));
					CFDictionaryRemoveValue(result, kSecAssessmentAssessmentAuthority);
					CFDictionaryRemoveValue(result, kSecAssessmentAssessmentVerdict);
				}
		}
		return NULL;
	}));

	// go for it!
	SecCSFlags topFlags = validationFlags | kSecCSCheckNestedCode | kSecCSRestrictSymlinks | kSecCSReportProgress;
	if (type == kAuthorityExecute && !appOk)
		topFlags |= kSecCSRestrictToAppLike;
	switch (rc = SecStaticCodeCheckValidity(code, topFlags, NULL)) {
	case errSecSuccess:		// continue below
		break;
	case errSecCSUnsigned:
		cfadd(result, "{%O=#F}", kSecAssessmentAssessmentVerdict);
		addAuthority(flags, result, "no usable signature");
		return;
	case errSecCSVetoed:		// nested code rejected by rule book; result was filled out there
        if (wasAdhocSigned)
            addToAuthority(result, kSecAssessmentAssessmentSource, CFSTR("no usable signature"));   // ad-hoc signature proved useless
		return;
	case errSecCSWeakResourceRules:
	case errSecCSWeakResourceEnvelope:
	case errSecCSResourceNotSupported:
	case errSecCSAmbiguousBundleFormat:
	case errSecCSSignatureNotVerifiable:
	case errSecCSRegularFile:
	case errSecCSBadMainExecutable:
	case errSecCSBadFrameworkVersion:
	case errSecCSUnsealedAppRoot:
	case errSecCSUnsealedFrameworkRoot:
	case errSecCSInvalidSymlink:
	case errSecCSNotAppLike:
	{
		// consult the allowlist
		bool allow = false;
		const char *label;
		// we've bypassed evaluateCodeItem before we failed validation. Explicitly apply it now
		SecStaticCodeSetCallback(code, kSecCSDefaultFlags, NULL, NULL);
		evaluateCodeItem(code, path, type, flags | kSecAssessmentFlagNoCache, false, result);
		if (CFTypeRef verdict = CFDictionaryGetValue(result, kSecAssessmentAssessmentVerdict)) {
			// verdict rendered from a nested component - signature not acceptable to Gatekeeper
			if (CFEqual(verdict, kCFBooleanFalse))	// nested code rejected by rule book; result was filled out there
				return;
			if (CFEqual(verdict, kCFBooleanTrue) && !(flags & kSecAssessmentFlagIgnoreAllowlist))
				if (opaqueAllowlistContains(code, feedback, rc)) {
					allow = true;
				}
		}
		if (allow) {
			label = "allowed cdhash";
		} else {
			CFDictionaryReplaceValue(result, kSecAssessmentAssessmentVerdict, kCFBooleanFalse);
			label = "obsolete resource envelope";
		}
		cfadd(result, "{%O=%d}", kSecAssessmentAssessmentCodeSigningError, rc);
		addAuthority(flags, result, label, 0, NULL, true);
		return;
	}
	default:
		MacOSError::throwMe(rc);
	}

	// Copy notarization date, if present, from code signing information
	CFRef<CFDictionaryRef> info;
	OSStatus status = SecCodeCopySigningInformation(code, kSecCSInternalInformation, &info.aref());
	if (status == 0 && info) {
		CFDateRef date = (CFDateRef)CFDictionaryGetValue(info, kSecCodeInfoNotarizationDate);
		if (date) {
			cfadd(result, "{%O=%O}", kSecAssessmentAssessmentNotarizationDate, date);
		}
	} else {
		secerror("Unable to copy signing information: %d", (int)status);
	}
	
	if (nestedFailure && CFEqual(CFDictionaryGetValue(result, kSecAssessmentAssessmentVerdict), kCFBooleanTrue)) {
		// structure intact, top level approved, nested code failed policy
		CFMutableDictionaryRef authority = CFMutableDictionaryRef(CFDictionaryGetValue(result, kSecAssessmentAssessmentAuthority));
		uint64_t ruleFlags = cfNumber<uint64_t>(CFNumberRef(CFDictionaryGetValue(authority, kSecAssessmentAssessmentAuthorityFlags)));
		if (ruleFlags & kAuthorityFlagDefault) {
			// default rule requires positive match at each nested code - reinstate failure
			CFDictionaryReplaceValue(result, kSecAssessmentAssessmentVerdict, kCFBooleanFalse);
			CFDictionaryReplaceValue(result, kSecAssessmentAssessmentAuthority, nestedFailure);
		}
	}
}


//
// Installer archive.
// Hybrid policy: If we detect an installer signature, use and validate that.
// If we don't, check for a code signature instead.
//
void PolicyEngine::evaluateInstall(CFURLRef path, SecAssessmentFlags flags, CFDictionaryRef context, CFMutableDictionaryRef result)
{
	const AuthorityType type = kAuthorityInstall;

	// check for recent explicit approval, using a bookmark's FileResourceIdentifierKey
	if (CFRef<CFDataRef> bookmark = cfLoadFile(lastApprovedFile)) {
		Boolean stale;
		if (CFRef<CFURLRef> url = CFURLCreateByResolvingBookmarkData(NULL, bookmark,
			kCFBookmarkResolutionWithoutUIMask | kCFBookmarkResolutionWithoutMountingMask, NULL, NULL, &stale, NULL))
			if (CFRef<CFDataRef> savedIdent = CFDataRef(CFURLCreateResourcePropertyForKeyFromBookmarkData(NULL, kCFURLFileResourceIdentifierKey, bookmark)))
				if (CFRef<CFDateRef> savedMod = CFDateRef(CFURLCreateResourcePropertyForKeyFromBookmarkData(NULL, kCFURLContentModificationDateKey, bookmark))) {
					CFRef<CFDataRef> currentIdent;
					CFRef<CFDateRef> currentMod;
					if (CFURLCopyResourcePropertyForKey(path, kCFURLFileResourceIdentifierKey, &currentIdent.aref(), NULL))
						if (CFURLCopyResourcePropertyForKey(path, kCFURLContentModificationDateKey, &currentMod.aref(), NULL))
							if (CFEqual(savedIdent, currentIdent) && CFEqual(savedMod, currentMod)) {
								cfadd(result, "{%O=#T}", kSecAssessmentAssessmentVerdict);
								addAuthority(flags, result, "explicit preference");
								return;
							}
				}
	}
	
	Xar xar(cfString(path).c_str());
	if (!xar) {
		// follow the code signing path
		evaluateCode(path, type, flags, context, result, true);
		return;
	}

	SQLite3::int64 latentID = 0;		// first (highest priority) disabled matching ID
	std::string latentLabel;			// ... and associated label, if any
	if (!xar.isSigned()) {
		// unsigned xar
		if (SYSPOLICY_ASSESS_OUTCOME_UNSIGNED_ENABLED())
			SYSPOLICY_ASSESS_OUTCOME_UNSIGNED(cfString(path).c_str(), type);
		cfadd(result, "{%O=#F}", kSecAssessmentAssessmentVerdict);
		addAuthority(flags, result, "no usable signature");
		return;
	}
	if (CFRef<CFArrayRef> certs = xar.copyCertChain()) {
		CFRef<CFTypeRef> policy = installerPolicy();
		CFRef<SecTrustRef> trust;
		CFRef<CFDataRef> checksum;
		CFRef<CFStringRef> teamID;
		CFRef<CFMutableDictionaryRef> requirementContext = makeCFMutableDictionary();
		MacOSError::check(SecTrustCreateWithCertificates(certs, policy, &trust.aref()));
//		MacOSError::check(SecTrustSetAnchorCertificates(trust, cfEmptyArray())); // no anchors
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
		case kSecTrustResultUnspecified:
			break;
		default:
			{
				OSStatus rc;
				MacOSError::check(SecTrustGetCssmResultCode(trust, &rc));
				MacOSError::throwMe(rc);
			}
		}

		xar.registerStapledNotarization();
		checksum.take(xar.createPackageChecksum());
		if (checksum) {
			double notarizationDate = NAN;

			// Force a single online check for the checksum, which is always SHA1.
			bool is_revoked = checkNotarizationServiceForRevocation(checksum, kSecCodeSignatureHashSHA1, &notarizationDate);
			if (is_revoked) {
				MacOSError::throwMe(errSecCSRevokedNotarization);
			}

			// Extract a team identifier from the certificates.  This isn't validated and could be spoofed,
			// but since the 'legacy' keyword is only used in addition to the Developer ID requirement,
			// this is still stafe for now.
			SecCertificateRef leaf = SecCertificateRef(CFArrayGetValueAtIndex(certs, 0));
			CFRef<CFArrayRef> orgUnits = SecCertificateCopyOrganizationalUnit(leaf);
			if (orgUnits.get() && CFArrayGetCount(orgUnits) == 1) {
				teamID = (CFStringRef)CFArrayGetValueAtIndex(orgUnits, 0);
			}

			// Create the appropriate requirement context entry to allow notarized requirement check.
			CFRef<CFNumberRef> algorithm = makeCFNumber((uint32_t)xar.checksumDigestAlgorithm());
			cfadd(requirementContext, "{%O=%O}", kSecRequirementKeyPackageChecksum, checksum.get());
			cfadd(requirementContext, "{%O=%O}", kSecRequirementKeyChecksumAlgorithm, algorithm.get());
			if (teamID.get()) {
				cfadd(requirementContext, "{%O=%O}", kSecRequirementKeyTeamIdentifier, teamID.get());
			}

			if (!isnan(notarizationDate)) {
				CFRef<CFDateRef> date = CFDateCreate(NULL, notarizationDate);
				if (date) {
					cfadd(result, "{%O=%O}", kSecAssessmentAssessmentNotarizationDate, date.get());
				}
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
			switch (OSStatus rc = SecRequirementEvaluate(requirement, chain, requirementContext.get(), kSecCSDefaultFlags)) {
			case errSecSuccess: // success
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

			if (SYSPOLICY_ASSESS_OUTCOME_ACCEPT_ENABLED() || SYSPOLICY_ASSESS_OUTCOME_DENY_ENABLED()) {
				if (allow)
					SYSPOLICY_ASSESS_OUTCOME_ACCEPT(cfString(path).c_str(), type, label, NULL);
				else
					SYSPOLICY_ASSESS_OUTCOME_DENY(cfString(path).c_str(), type, label, NULL);
			}

			// not adding to the object cache - we could, but it's not likely to be worth it
			cfadd(result, "{%O=%B}", kSecAssessmentAssessmentVerdict, allow);
			addAuthority(flags, result, label, id);
			return;
		}
	}
	if (SYSPOLICY_ASSESS_OUTCOME_DEFAULT_ENABLED())
		SYSPOLICY_ASSESS_OUTCOME_DEFAULT(cfString(path).c_str(), type, latentLabel.c_str(), NULL);
	
	// no applicable authority. Deny by default
	cfadd(result, "{%O=#F}", kSecAssessmentAssessmentVerdict);
	addAuthority(flags, result, latentLabel.c_str(), latentID);
}


//
// Create a suitable policy array for verification of installer signatures.
//
static SecPolicyRef makeRevocationPolicy()
{
	CFRef<SecPolicyRef> policy(SecPolicyCreateRevocation(kSecRevocationUseAnyAvailableMethod));
	return policy.yield();
}

static CFTypeRef installerPolicy()
{
	CFRef<SecPolicyRef> base = SecPolicyCreateBasicX509();
	CFRef<SecPolicyRef> revoc = makeRevocationPolicy();
	return makeCFArray(2, base.get(), revoc.get());
}


//
// LaunchServices-layer document open.
// We don't cache those at present. If we ever do, we need to authenticate CoreServicesUIAgent as the source of its risk assessment.
//
void PolicyEngine::evaluateDocOpen(CFURLRef path, SecAssessmentFlags flags, CFDictionaryRef context, CFMutableDictionaryRef result)
{
	if (context) {
		FileQuarantine qtn(cfString(path).c_str());
		if (CFDictionaryGetValue(context, kSecAssessmentContextKeyPrimarySignature) == kCFBooleanTrue) {
			// Client requests that we focus on the code signature on this document and report on that.
			// On this path, we care about the (code) signature on the document, not its risk assessment,
			// and any exception is reported as a primary error.
			if (qtn.flag(QTN_FLAG_ASSESSMENT_OK)) {
				// previously added by user - hacked to say no/no usable signature to trigger proper DMG processing in XProtect
				cfadd(result, "{%O=#F}", kSecAssessmentAssessmentVerdict);
                addAuthority(flags, result, "no usable signature");
				return;
			}
			evaluateCode(path, kAuthorityOpenDoc, flags, context, result, true);
			return;
		}
		if (CFStringRef riskCategory = CFStringRef(CFDictionaryGetValue(context, kLSDownloadRiskCategoryKey))) {

			if (CFEqual(riskCategory, kLSRiskCategorySafe)
				|| CFEqual(riskCategory, kLSRiskCategoryNeutral)
				|| CFEqual(riskCategory, kLSRiskCategoryUnknown)
				|| CFEqual(riskCategory, kLSRiskCategoryMayContainUnsafeExecutable)) {
				cfadd(result, "{%O=#T}", kSecAssessmentAssessmentVerdict);
				addAuthority(flags, result, "_XProtect");
			} else if (qtn.flag(QTN_FLAG_HARD)) {
				MacOSError::throwMe(errSecCSFileHardQuarantined);
			} else if (qtn.flag(QTN_FLAG_ASSESSMENT_OK)) {
				// previously added by user
				cfadd(result, "{%O=#T}", kSecAssessmentAssessmentVerdict);
				addAuthority(flags, result, "Prior Assessment");
			} else if (!overrideAssessment(flags)) {		// no need to do more work if we're off
				try {
					evaluateCode(path, kAuthorityOpenDoc, flags, context, result, true);
				} catch (...) {
					// some documents can't be code signed, so this may be quite benign
				}
			}
			if (CFDictionaryGetValue(result, kSecAssessmentAssessmentVerdict) == NULL) {	// no code signature to help us out
			   cfadd(result, "{%O=#F}", kSecAssessmentAssessmentVerdict);
			   addAuthority(flags, result, "_XProtect");
			}
			addToAuthority(result, kLSDownloadRiskCategoryKey, riskCategory);
			return;
		}
	}
	// insufficient information from LS - deny by default
	cfadd(result, "{%O=#F}", kSecAssessmentAssessmentVerdict);
	addAuthority(flags, result, "Insufficient Context");
}


//
// Result-creation helpers
//
void PolicyEngine::addAuthority(SecAssessmentFlags flags, CFMutableDictionaryRef parent, const char *label, SQLite::int64 row, CFTypeRef cacheInfo, bool weak, uint64_t ruleFlags)
{
	CFRef<CFMutableDictionaryRef> auth = makeCFMutableDictionary();
	if (label && label[0])
		cfadd(auth, "{%O=%s}", kSecAssessmentAssessmentSource, label);
	if (row)
		CFDictionaryAddValue(auth, kSecAssessmentAssessmentAuthorityRow, CFTempNumber(row));
	if (overrideAssessment(flags))
		CFDictionaryAddValue(auth, kSecAssessmentAssessmentAuthorityOverride, kDisabledOverride);
	if (cacheInfo)
		CFDictionaryAddValue(auth, kSecAssessmentAssessmentFromCache, cacheInfo);
	CFDictionaryAddValue(auth, kSecAssessmentAssessmentAuthorityFlags, CFTempNumber(ruleFlags));
	if (weak) {
		CFDictionaryAddValue(auth, kSecAssessmentAssessmentWeakSignature, kCFBooleanTrue);
		CFDictionaryReplaceValue(parent, kSecAssessmentAssessmentAuthority, auth);
	} else {
		CFDictionaryAddValue(parent, kSecAssessmentAssessmentAuthority, auth);
	}
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
CFDictionaryRef PolicyEngine::add(CFTypeRef inTarget, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context)
{
	// default type to execution
	if (type == kAuthorityInvalid)
		type = kAuthorityExecute;

	authorizeUpdate(flags, context);
	CFDictionary ctx(context, errSecCSInvalidAttributeValues);
	CFCopyRef<CFTypeRef> target = inTarget;
	CFRef<CFDataRef> bookmark = NULL;
	std::string filter_unsigned;

	switch (type) {
	case kAuthorityExecute:
		normalizeTarget(target, type, ctx, &filter_unsigned);
		// bookmarks are untrusted and just a hint to callers
		bookmark = ctx.get<CFDataRef>(kSecAssessmentRuleKeyBookmark);
		break;
	case kAuthorityInstall:
		if (inTarget && CFGetTypeID(inTarget) == CFURLGetTypeID()) {
			// no good way to turn an installer file into a requirement. Pretend to succeeed so caller proceeds
			CFRef<CFArrayRef> properties = makeCFArray(2, kCFURLFileResourceIdentifierKey, kCFURLContentModificationDateKey);
			CFRef<CFErrorRef> error;
			CFURLBookmarkCreationOptions options = kCFURLBookmarkCreationDoNotIncludeSandboxExtensionsMask | kCFURLBookmarkCreationMinimalBookmarkMask;
			if (CFRef<CFDataRef> bookmark = CFURLCreateBookmarkData(NULL, CFURLRef(inTarget), options, properties, NULL, &error.aref())) {
				UnixPlusPlus::AutoFileDesc fd(lastApprovedFile, O_WRONLY | O_CREAT | O_TRUNC);
				fd.write(CFDataGetBytePtr(bookmark), CFDataGetLength(bookmark));
				return NULL;
			}
		}
		break;
	case kAuthorityOpenDoc:
		// handle document-open differently: use quarantine flags for allowlisting
		if (!target || CFGetTypeID(target) != CFURLGetTypeID())	// can only "add" file paths
			MacOSError::throwMe(errSecCSInvalidObjectRef);
		try {
			std::string spath = cfString(target.as<CFURLRef>());
			FileQuarantine qtn(spath.c_str());
			qtn.setFlag(QTN_FLAG_ASSESSMENT_OK);
			qtn.applyTo(spath.c_str());
		} catch (const CommonError &error) {
			// could not set quarantine flag - report qualified success
			return cfmake<CFDictionaryRef>("{%O=%O,'assessment:error'=%d}",
				kSecAssessmentAssessmentAuthorityOverride, CFSTR("error setting quarantine"), error.osStatus());
		} catch (...) {
			return cfmake<CFDictionaryRef>("{%O=%O}", kSecAssessmentAssessmentAuthorityOverride, CFSTR("unable to set quarantine"));
		}
		return NULL;
	}
	
	// if we now have anything else, we're busted
	if (!target || CFGetTypeID(target) != SecRequirementGetTypeID())
		MacOSError::throwMe(errSecCSInvalidObjectRef);

	double priority = 0;
	string label;
	bool allow = true;
	double expires = never;
	string remarks;
	SQLite::uint64 dbFlags = kAuthorityFlagAllowlistV2 | kAuthorityFlagAllowlistSHA256;
	
	if (CFNumberRef pri = ctx.get<CFNumberRef>(kSecAssessmentUpdateKeyPriority))
		CFNumberGetValue(pri, kCFNumberDoubleType, &priority);
	if (CFStringRef lab = ctx.get<CFStringRef>(kSecAssessmentUpdateKeyLabel))
		label = cfString(lab);
	if (CFDateRef time = ctx.get<CFDateRef>(kSecAssessmentUpdateKeyExpires))
		// we're using Julian dates here; convert from CFDate
		expires = dateToJulian(time);
	if (CFBooleanRef allowing = ctx.get<CFBooleanRef>(kSecAssessmentUpdateKeyAllow))
		allow = allowing == kCFBooleanTrue;
	if (CFStringRef rem = ctx.get<CFStringRef>(kSecAssessmentUpdateKeyRemarks))
		remarks = cfString(rem);

	CFRef<CFStringRef> requirementText;
	MacOSError::check(SecRequirementCopyString(target.as<SecRequirementRef>(), kSecCSDefaultFlags, &requirementText.aref()));
	SQLite::Transaction xact(*this, SQLite3::Transaction::deferred, "add_rule");
	SQLite::Statement insert(*this,
		"INSERT INTO authority (type, allow, requirement, priority, label, expires, filter_unsigned, remarks, flags)"
		"	VALUES (:type, :allow, :requirement, :priority, :label, :expires, :filter_unsigned, :remarks, :flags);");
	insert.bind(":type").integer(type);
	insert.bind(":allow").integer(allow);
	insert.bind(":requirement") = requirementText.get();
	insert.bind(":priority") = priority;
	if (!label.empty())
		insert.bind(":label") = label;
	insert.bind(":expires") = expires;
	insert.bind(":filter_unsigned") = filter_unsigned.empty() ? NULL : filter_unsigned.c_str();
	if (!remarks.empty())
		insert.bind(":remarks") = remarks;
	insert.bind(":flags").integer(dbFlags);
	insert.execute();
	SQLite::int64 newRow = this->lastInsert();
	if (bookmark) {
		SQLite::Statement bi(*this, "INSERT INTO bookmarkhints (bookmark, authority) VALUES (:bookmark, :authority)");
		bi.bind(":bookmark") = CFDataRef(bookmark);
		bi.bind(":authority").integer(newRow);
		bi.execute();
	}
	this->purgeObjects(priority);
	xact.commit();
	notify_post(kNotifySecAssessmentUpdate);
	return cfmake<CFDictionaryRef>("{%O=%d}", kSecAssessmentUpdateKeyRow, newRow);
}


CFDictionaryRef PolicyEngine::remove(CFTypeRef target, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context)
{
	if (type == kAuthorityOpenDoc) {
		// handle document-open differently: use quarantine flags for allowlisting
		authorizeUpdate(flags, context);
		if (!target || CFGetTypeID(target) != CFURLGetTypeID())
			MacOSError::throwMe(errSecCSInvalidObjectRef);
		std::string spath = cfString(CFURLRef(target)).c_str();
		FileQuarantine qtn(spath.c_str());
		qtn.clearFlag(QTN_FLAG_ASSESSMENT_OK);
		qtn.applyTo(spath.c_str());
		return NULL;
	}
	return manipulateRules("DELETE FROM authority", target, type, flags, context, true);
}

CFDictionaryRef PolicyEngine::enable(CFTypeRef target, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context, bool authorize)
{
	return manipulateRules("UPDATE authority SET disabled = 0", target, type, flags, context, authorize);
}

CFDictionaryRef PolicyEngine::disable(CFTypeRef target, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context, bool authorize)
{
	return manipulateRules("UPDATE authority SET disabled = 1", target, type, flags, context, authorize);
}

CFDictionaryRef PolicyEngine::find(CFTypeRef target, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context)
{
    //for privacy reasons we only want to allow the admin to list the database
    authorizeUpdate(flags, context);

	SQLite::Statement query(*this);
	selectRules(query, "SELECT scan_authority.id, scan_authority.type, scan_authority.requirement, scan_authority.allow, scan_authority.label, scan_authority.priority, scan_authority.remarks, scan_authority.expires, scan_authority.disabled, bookmarkhints.bookmark FROM scan_authority LEFT OUTER JOIN bookmarkhints ON scan_authority.id = bookmarkhints.authority",
		"scan_authority", target, type, flags, context,
		" ORDER BY priority DESC");
	CFRef<CFMutableArrayRef> found = makeCFMutableArray(0);
	while (query.nextRow()) {
		SQLite::int64 id = query[0];
		int type = int(query[1]);
		const char *requirement = query[2];
		int allow = int(query[3]);
		const char *label = query[4];
		double priority = query[5];
		const char *remarks = query[6];
		double expires = query[7];
		int disabled = int(query[8]);
		CFRef<CFDataRef> bookmark = query[9].data();
		CFRef<CFMutableDictionaryRef> rule = makeCFMutableDictionary(5,
			kSecAssessmentRuleKeyID, CFTempNumber(id).get(),
			kSecAssessmentRuleKeyType, CFRef<CFStringRef>(typeNameFor(type)).get(),
			kSecAssessmentRuleKeyRequirement, CFTempString(requirement).get(),
			kSecAssessmentRuleKeyAllow, allow ? kCFBooleanTrue : kCFBooleanFalse,
			kSecAssessmentRuleKeyPriority, CFTempNumber(priority).get()
			);
		if (label)
			CFDictionaryAddValue(rule, kSecAssessmentRuleKeyLabel, CFTempString(label));
		if (remarks)
			CFDictionaryAddValue(rule, kSecAssessmentRuleKeyRemarks, CFTempString(remarks));
		if (expires != never)
			CFDictionaryAddValue(rule, kSecAssessmentRuleKeyExpires, CFRef<CFDateRef>(julianToDate(expires)));
		if (disabled)
			CFDictionaryAddValue(rule, kSecAssessmentRuleKeyDisabled, CFTempNumber(disabled));
		if (bookmark)
			CFDictionaryAddValue(rule, kSecAssessmentRuleKeyBookmark, bookmark);
		CFArrayAppendValue(found, rule);
	}
	if (CFArrayGetCount(found) == 0)
		MacOSError::throwMe(errSecCSNoMatches);
	return cfmake<CFDictionaryRef>("{%O=%O}", kSecAssessmentUpdateKeyFound, found.get());
}


CFDictionaryRef PolicyEngine::update(CFTypeRef target, SecAssessmentFlags flags, CFDictionaryRef context)
{
	// update GKE
	installExplicitSet(gkeAuthFile, gkeSigsFile);

	AuthorityType type = typeFor(context, kAuthorityInvalid);
	CFStringRef edit = CFStringRef(CFDictionaryGetValue(context, kSecAssessmentContextKeyUpdate));
	CFDictionaryRef result;
	if (CFEqual(edit, kSecAssessmentUpdateOperationAdd))
		result = this->add(target, type, flags, context);
	else if (CFEqual(edit, kSecAssessmentUpdateOperationRemove))
		result = this->remove(target, type, flags, context);
	else if (CFEqual(edit, kSecAssessmentUpdateOperationEnable))
		result = this->enable(target, type, flags, context, true);
	else if (CFEqual(edit, kSecAssessmentUpdateOperationDisable))
		result = this->disable(target, type, flags, context, true);
	else if (CFEqual(edit, kSecAssessmentUpdateOperationFind))
		result = this->find(target, type, flags, context);
	else
		MacOSError::throwMe(errSecCSInvalidAttributeValues);
	if (result == NULL)
		result = makeCFDictionary(0);		// success, no details
	return result;
}


//
// Construct and prepare an SQL query on the authority table, operating on some set of existing authority records.
// In essence, this appends a suitable WHERE clause to the stanza passed and prepares it on the statement given.
//
void PolicyEngine::selectRules(SQLite::Statement &action, std::string phrase, std::string table,
	CFTypeRef inTarget, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context, std::string suffix /* = "" */)
{
	CFDictionary ctx(context, errSecCSInvalidAttributeValues);
	CFCopyRef<CFTypeRef> target = inTarget;
	std::string filter_unsigned;	// ignored; used just to trigger ad-hoc signing
	normalizeTarget(target, type, ctx, &filter_unsigned);

	string label;
	if (CFStringRef lab = ctx.get<CFStringRef>(kSecAssessmentUpdateKeyLabel))
		label = cfString(CFStringRef(lab));

	if (!target) {
		if (label.empty()) {
			if (type == kAuthorityInvalid) {
				action.query(phrase + suffix);
			} else {
				action.query(phrase + " WHERE " + table + ".type = :type" + suffix);
				action.bind(":type").integer(type);
			}
		} else {	// have label
			if (type == kAuthorityInvalid) {
				action.query(phrase + " WHERE " + table + ".label = :label" + suffix);
			} else {
				action.query(phrase + " WHERE " + table + ".type = :type AND " + table + ".label = :label" + suffix);
				action.bind(":type").integer(type);
			}
			action.bind(":label") = label;
		}
	} else if (CFGetTypeID(target) == CFNumberGetTypeID()) {
		action.query(phrase + " WHERE " + table + ".id = :id" + suffix);
		action.bind(":id").integer(cfNumber<uint64_t>(target.as<CFNumberRef>()));
	} else if (CFGetTypeID(target) == SecRequirementGetTypeID()) {
		if (type == kAuthorityInvalid)
			type = kAuthorityExecute;
		CFRef<CFStringRef> requirementText;
		MacOSError::check(SecRequirementCopyString(target.as<SecRequirementRef>(), kSecCSDefaultFlags, &requirementText.aref()));
		action.query(phrase + " WHERE " + table + ".type = :type AND " + table + ".requirement = :requirement" + suffix);
		action.bind(":type").integer(type);
		action.bind(":requirement") = requirementText.get();
	} else
		MacOSError::throwMe(errSecCSInvalidObjectRef);
}


//
// Execute an atomic change to existing records in the authority table.
//
CFDictionaryRef PolicyEngine::manipulateRules(const std::string &stanza,
	CFTypeRef inTarget, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context, bool authorize)
{
	SQLite::Transaction xact(*this, SQLite3::Transaction::deferred, "rule_change");
	SQLite::Statement action(*this);
	if (authorize)
		authorizeUpdate(flags, context);
	selectRules(action, stanza, "authority", inTarget, type, flags, context);
	action.execute();
	unsigned int changes = this->changes();	// latch change count
	// We MUST purge objects with priority <= MAX(priority of any changed rules);
	// but for now we just get lazy and purge them ALL.
	if (changes) {
		this->purgeObjects(1.0E100);
		xact.commit();
		notify_post(kNotifySecAssessmentUpdate);
		return cfmake<CFDictionaryRef>("{%O=%d}", kSecAssessmentUpdateKeyCount, changes);
	}
	// no change; return an error
	MacOSError::throwMe(errSecCSNoMatches);
}


//
// Fill in extra information about the originator of cryptographic credentials found - if any
//
void PolicyEngine::setOrigin(CFArrayRef chain, CFMutableDictionaryRef result)
{
	if (chain)
		if (CFArrayGetCount(chain) > 0)
			if (SecCertificateRef leaf = SecCertificateRef(CFArrayGetValueAtIndex(chain, 0)))
				if (CFStringRef summary = SecCertificateCopyLongDescription(NULL, leaf, NULL)) {
					CFDictionarySetValue(result, kSecAssessmentAssessmentOriginator, summary);
					CFRelease(summary);
				}
}


//
// Take an assessment outcome and record it in the object cache
//
void PolicyEngine::recordOutcome(SecStaticCodeRef code, bool allow, AuthorityType type, double expires, SQLite::int64 authority)
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
	insert.bind(":path") = cfString(path);
	insert.bind(":authority").integer(authority);
	insert.execute();
	xact.commit();
}


//
// Record a UI failure record after proper validation of the caller
//
void PolicyEngine::recordFailure(CFDictionaryRef info)
{
	CFRef<CFDataRef> infoData = makeCFData(info);
	UnixPlusPlus::AutoFileDesc fd(lastRejectFile, O_WRONLY | O_CREAT | O_TRUNC);
	fd.write(CFDataGetBytePtr(infoData), CFDataGetLength(infoData));
	notify_post(kNotifySecAssessmentRecordingChange);
}


//
// Perform update authorization processing.
// Throws an exception if authorization is denied.
//
static void authorizeUpdate(SecAssessmentFlags flags, CFDictionaryRef context)
{
	AuthorizationRef authorization = NULL;
	
	if (context)
		if (CFTypeRef authkey = CFDictionaryGetValue(context, kSecAssessmentUpdateKeyAuthorization))
			if (CFGetTypeID(authkey) == CFDataGetTypeID()) {
				CFDataRef authdata = CFDataRef(authkey);
				if (CFDataGetLength(authdata) != sizeof(AuthorizationExternalForm))
					MacOSError::throwMe(errSecCSInvalidObjectRef);
				MacOSError::check(AuthorizationCreateFromExternalForm((AuthorizationExternalForm *)CFDataGetBytePtr(authdata), &authorization));
			}
	if (authorization == NULL)
		MacOSError::throwMe(errSecCSDBDenied);
	
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
void PolicyEngine::normalizeTarget(CFRef<CFTypeRef> &target, AuthorityType type, CFDictionary &context, std::string *signUnsigned)
{
	// turn CFURLs into (designated) SecRequirements
	if (target && CFGetTypeID(target) == CFURLGetTypeID()) {
		CFRef<SecStaticCodeRef> code;
		CFURLRef path = target.as<CFURLRef>();
		MacOSError::check(SecStaticCodeCreateWithPath(path, kSecCSDefaultFlags, &code.aref()));
		switch (OSStatus rc = SecCodeCopyDesignatedRequirement(code, kSecCSDefaultFlags, (SecRequirementRef *)&target.aref())) {
		case errSecSuccess: {
			// use the *default* DR to avoid unreasonably wide DRs opening up Gatekeeper to attack
			CFRef<CFDictionaryRef> info;
			MacOSError::check(SecCodeCopySigningInformation(code, kSecCSRequirementInformation, &info.aref()));
			target = CFDictionaryGetValue(info, kSecCodeInfoImplicitDesignatedRequirement);
			}
			break;
		case errSecCSUnsigned:
			if (signUnsigned && temporarySigning(code, type, path, kAuthorityFlagAllowlistV2 | kAuthorityFlagAllowlistSHA256)) {	// ad-hoc sign the code temporarily
				MacOSError::check(SecCodeCopyDesignatedRequirement(code, kSecCSDefaultFlags, (SecRequirementRef *)&target.aref()));
				*signUnsigned = createAllowlistScreen(code);
				break;
			}
			MacOSError::check(rc);
			break;
		case errSecCSSignatureFailed:
		default:
			MacOSError::check(rc);
			break;
		}
		if (context.get(kSecAssessmentUpdateKeyRemarks) == NULL)	{
			// no explicit remarks; add one with the path
			CFRef<CFURLRef> path;
			MacOSError::check(SecCodeCopyPath(code, kSecCSDefaultFlags, &path.aref()));
			CFMutableDictionaryRef dict = makeCFMutableDictionary(context.get());
			CFDictionaryAddValue(dict, kSecAssessmentUpdateKeyRemarks, CFTempString(cfString(path)));
			context.take(dict);
		}
		CFStringRef edit = CFStringRef(context.get(kSecAssessmentContextKeyUpdate));
		if (type == kAuthorityExecute && CFEqual(edit, kSecAssessmentUpdateOperationAdd)) {
			// implicitly allowlist the code
			opaqueAllowlistAdd(code);
		}
	}
}

} // end namespace CodeSigning
} // end namespace Security
