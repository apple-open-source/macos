/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
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
#include "cs.h"
#include "SecAssessment.h"
#include "SecAssessmentPriv.h"
#include "policydb.h"
#include "policyengine.h"
#include "xpcengine.h"
#include "csutilities.h"
#include "xar++.h"
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFBundlePriv.h>
#include <security_utilities/globalizer.h>
#include <security_utilities/unix++.h>
#include <security_utilities/cfmunge.h>
#include <notify.h>

using namespace CodeSigning;

//
// CF Objects
//
struct _SecAssessment : private CFRuntimeBase {
public:
	_SecAssessment(CFURLRef p, AuthorityType typ, CFDictionaryRef r) : path(p), type(typ), result(r) { }
	
	CFCopyRef<CFURLRef> path;
	AuthorityType type;
	CFRef<CFDictionaryRef> result;

public:
	static _SecAssessment &ref(SecAssessmentRef r)
		{ return *(_SecAssessment *)r; }

	// CF Boiler-plate
	void *operator new (size_t size)
	{
		return (void *)_CFRuntimeCreateInstance(NULL, SecAssessmentGetTypeID(),
			sizeof(_SecAssessment) - sizeof(CFRuntimeBase), NULL);
	}
	
	static void finalize(CFTypeRef obj)
	{ ((_SecAssessment *)obj)->~_SecAssessment(); }
};

typedef _SecAssessment SecAssessment;


static const CFRuntimeClass assessmentClass = {
	0,								// version
	"SecAssessment",				// name
	NULL,							// init
	NULL,							// copy
	SecAssessment::finalize,		// finalize
	NULL,							// equal
	NULL,							// hash
	NULL,							// formatting
	NULL							// debug string
};


static dispatch_once_t assessmentOnce;
CFTypeID assessmentType = _kCFRuntimeNotATypeID;
	
CFTypeID SecAssessmentGetTypeID()
{
	dispatch_once(&assessmentOnce, ^void() {
		if ((assessmentType = _CFRuntimeRegisterClass(&assessmentClass)) == _kCFRuntimeNotATypeID)
			abort();
	});
	return assessmentType;
}


//
// Common dictionary constants
//
CFStringRef kSecAssessmentContextKeyOperation = CFSTR("operation");
CFStringRef kSecAssessmentOperationTypeExecute = CFSTR("operation:execute");
CFStringRef kSecAssessmentOperationTypeInstall = CFSTR("operation:install");
CFStringRef kSecAssessmentOperationTypeOpenDocument = CFSTR("operation:lsopen");


//
// Read-only in-process access to the policy database
//
class ReadPolicy : public PolicyDatabase {
public:
	ReadPolicy() : PolicyDatabase(policyDatabase) { }
};
ModuleNexus<ReadPolicy> gDatabase;


//
// An on-demand instance of the policy engine
//
ModuleNexus<PolicyEngine> gEngine;

//
// Policy evaluation ("assessment") operations
//
CFStringRef kSecAssessmentContextKeyUTI = CFSTR("context:uti");

CFStringRef kSecAssessmentContextKeyFeedback = CFSTR("context:feedback");
CFStringRef kSecAssessmentFeedbackProgress = CFSTR("feedback:progress");
CFStringRef kSecAssessmentFeedbackInfoCurrent = CFSTR("current");
CFStringRef kSecAssessmentFeedbackInfoTotal = CFSTR("total");

CFStringRef kSecAssessmentContextKeyPrimarySignature = CFSTR("context:primary-signature");

CFStringRef kSecAssessmentAssessmentVerdict = CFSTR("assessment:verdict");
CFStringRef kSecAssessmentAssessmentOriginator = CFSTR("assessment:originator");
CFStringRef kSecAssessmentAssessmentAuthority = CFSTR("assessment:authority");
CFStringRef kSecAssessmentAssessmentSource = CFSTR("assessment:authority:source");
CFStringRef kSecAssessmentAssessmentAuthorityRow = CFSTR("assessment:authority:row");
CFStringRef kSecAssessmentAssessmentAuthorityOverride = CFSTR("assessment:authority:override");
CFStringRef kSecAssessmentAssessmentAuthorityOriginalVerdict = CFSTR("assessment:authority:verdict");
CFStringRef kSecAssessmentAssessmentAuthorityFlags = CFSTR("assessment:authority:flags");
CFStringRef kSecAssessmentAssessmentFromCache = CFSTR("assessment:authority:cached");
CFStringRef kSecAssessmentAssessmentWeakSignature = CFSTR("assessment:authority:weak");
CFStringRef kSecAssessmentAssessmentCodeSigningError = CFSTR("assessment:cserror");
CFStringRef kSecAssessmentAssessmentNotarizationDate = CFSTR("assessment:notarization-date");

CFStringRef kDisabledOverride = CFSTR("security disabled");

SecAssessmentRef SecAssessmentCreate(CFURLRef path,
	SecAssessmentFlags flags,
	CFDictionaryRef context,
	CFErrorRef *errors)
{
	BEGIN_CSAPI
	
	if (flags & kSecAssessmentFlagAsynchronous)
		MacOSError::throwMe(errSecCSUnimplemented);
	
	AuthorityType type = typeFor(context, kAuthorityExecute);
	CFRef<CFMutableDictionaryRef> result = makeCFMutableDictionary();

	SYSPOLICY_ASSESS_API(cfString(path).c_str(), int(type), flags);

	try {
		if (flags & kSecAssessmentFlagDirect) {
			// ask the engine right here to do its thing
			SYSPOLICY_ASSESS_LOCAL();
			gEngine().evaluate(path, type, flags, context, result);
		} else {
			// relay the question to our daemon for consideration
			SYSPOLICY_ASSESS_REMOTE();
			xpcEngineAssess(path, flags, context, result);
		}
	} catch (CommonError &error) {
		switch (error.osStatus()) {
		case CSSMERR_TP_CERT_REVOKED:
			throw;
		default:
			if (!overrideAssessment(flags))
				throw;		// let it go as an error
			break;
		}
		// record the error we would have returned
		cfadd(result, "{%O=#F,'assessment:error'=%d}}", kSecAssessmentAssessmentVerdict, error.osStatus());
	} catch (...) {
		// catch stray errors not conforming to the CommonError scheme
		if (!overrideAssessment(flags))
			throw;		// let it go as an error
		cfadd(result, "{%O=#F}", kSecAssessmentAssessmentVerdict);
	}

	return new SecAssessment(path, type, result.yield());

	END_CSAPI_ERRORS1(NULL)
}


static void traceResult(CFURLRef target, MessageTrace &trace, std::string &sanitized)
{
	static const char *interestingBundles[] = {
		"UNBUNDLED",
		"com.apple.",
		"com.install4j.",
		"com.MindVision.",
		"com.yourcompany.",

		"com.adobe.flashplayer.installmanager",
		"com.adobe.Installers.Setup",
		"com.adobe.PDApp.setup",
		"com.bittorrent.uTorrent",
		"com.divx.divx6formacinstaller",
		"com.getdropbox.dropbox",
		"com.google.Chrome",
		"com.Google.GoogleEarthPlugin.plugin",
		"com.Google.GoogleEarthPlus",
		"com.hp.Installer",
		"com.macpaw.CleanMyMac",
		"com.microsoft.SilverlightInstaller",
		"com.paragon-software.filesystems.NTFS.pkg",
		"com.RealNetworks.RealPlayer",
		"com.skype.skype",
		"it.alfanet.squared5.MPEGStreamclip",
		"org.mozilla.firefox",
		"org.videolan.vlc",
		
		NULL	// sentinel
	};

	string identifier = "UNBUNDLED";
	string version = "UNKNOWN";
	if (CFRef<CFBundleRef> bundle = _CFBundleCreateUnique(NULL, target)) {
		if (CFStringRef ident = CFBundleGetIdentifier(bundle))
			identifier = cfString(ident);
		if (CFStringRef vers = CFStringRef(CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("CFBundleShortVersionString"))))
			version = cfString(vers);
	}
	
	CFRef<CFURLRef> url = CFURLCopyAbsoluteURL(target);
	sanitized = cfString(url);
	string::size_type rslash = sanitized.rfind('/');
	if (rslash != string::npos)
		sanitized = sanitized.substr(rslash+1);
	bool keepFilename = false;
	for (const char **pfx = interestingBundles; *pfx; pfx++) {
		size_t pfxlen = strlen(*pfx);
		if (identifier.compare(0, pfxlen, *pfx, pfxlen) == 0)
			if (pfxlen == identifier.size() || (*pfx)[pfxlen-1] == '.') {
				keepFilename = true;
				break;
			}
	}
	if (!keepFilename) {
		string::size_type dot = sanitized.rfind('.');
		if (dot != string::npos)
			sanitized = sanitized.substr(dot);
		else
			sanitized = "(none)";
	}
	
	trace.add("signature2", "bundle:%s", identifier.c_str());
	trace.add("signature3", "%s", sanitized.c_str());
	trace.add("signature5", "%s", version.c_str());
}
	
static void traceAssessment(SecAssessment &assessment, AuthorityType type, CFDictionaryRef result)
{
	if (CFDictionaryGetValue(result, CFSTR("assessment:remote")))
		return;		// just traced in syspolicyd
	
	string authority = "UNSPECIFIED";
	bool overridden = false;
	bool old_overridden = false;
	if (CFDictionaryRef authdict = CFDictionaryRef(CFDictionaryGetValue(result, kSecAssessmentAssessmentAuthority))) {
		if (CFStringRef auth = CFStringRef(CFDictionaryGetValue(authdict, kSecAssessmentAssessmentSource)))
			authority = cfString(auth);
		else
			authority = "no authority";
		if (CFTypeRef override = CFDictionaryGetValue(authdict, kSecAssessmentAssessmentAuthorityOverride))
			if (CFEqual(override, kDisabledOverride)) {
				old_overridden = true;
				if (CFDictionaryGetValue(authdict, kSecAssessmentAssessmentAuthorityOriginalVerdict) == kCFBooleanFalse)
					overridden = true;
			}
	}

	MessageTrace trace("com.apple.security.assessment.outcome2", NULL);
	std::string sanitized;
	traceResult(assessment.path, trace, sanitized);
	trace.add("signature4", "%d", type);

	if (CFDictionaryGetValue(result, kSecAssessmentAssessmentVerdict) == kCFBooleanFalse) {
		trace.add("signature", "denied:%s", authority.c_str());
		trace.send("assessment denied for %s", sanitized.c_str());
	} else if (overridden) {		// would have failed except for override
		trace.add("signature", "defeated:%s", authority.c_str());
		trace.send("assessment denied for %s but overridden", sanitized.c_str());
	} else if (old_overridden) {	// would have succeeded even without override
		trace.add("signature", "override:%s", authority.c_str());
		trace.send("assessment granted for %s and overridden", sanitized.c_str());
	} else {
		trace.add("signature", "granted:%s", authority.c_str());
		trace.send("assessment granted for %s by %s", sanitized.c_str(), authority.c_str());
	}
}

static void traceUpdate(CFTypeRef target, CFDictionaryRef context, CFDictionaryRef result)
{
	// only trace add operations on URL targets
	if (target == NULL || CFGetTypeID(target) != CFURLGetTypeID())
		return;
	CFStringRef edit = CFStringRef(CFDictionaryGetValue(context, kSecAssessmentContextKeyUpdate));
	if (!CFEqual(edit, kSecAssessmentUpdateOperationAdd))
		return;
	MessageTrace trace("com.apple.security.assessment.update", NULL);
	std::string sanitized;
	traceResult(CFURLRef(target), trace, sanitized);
	trace.send("added rule for %s", sanitized.c_str());
}


//
// At present, CopyResult simply retrieves the result already formed by Create.
// In the future, this will be more lazy.
//
CFDictionaryRef SecAssessmentCopyResult(SecAssessmentRef assessmentRef,
	SecAssessmentFlags flags,
	CFErrorRef *errors)
{
	BEGIN_CSAPI

	SecAssessment &assessment = SecAssessment::ref(assessmentRef);
	CFCopyRef<CFDictionaryRef> result = assessment.result;
	if (overrideAssessment(flags)) {
		// turn rejections into approvals, but note that we did that
		CFTypeRef verdict = CFDictionaryGetValue(result, kSecAssessmentAssessmentVerdict);
		if (verdict == kCFBooleanFalse) {
			CFRef<CFMutableDictionaryRef> adulterated = makeCFMutableDictionary(result.get());
			CFDictionarySetValue(adulterated, kSecAssessmentAssessmentVerdict, kCFBooleanTrue);
			if (CFDictionaryRef authority = CFDictionaryRef(CFDictionaryGetValue(adulterated, kSecAssessmentAssessmentAuthority))) {
				CFRef<CFMutableDictionaryRef> authority2 = makeCFMutableDictionary(authority);
				CFDictionarySetValue(authority2, kSecAssessmentAssessmentAuthorityOverride, kDisabledOverride);
				CFDictionarySetValue(authority2, kSecAssessmentAssessmentAuthorityOriginalVerdict, verdict);
				CFDictionarySetValue(adulterated, kSecAssessmentAssessmentAuthority, authority2);
			} else {
				cfadd(adulterated, "{%O={%O=%O}}",
					kSecAssessmentAssessmentAuthority, kSecAssessmentAssessmentAuthorityOverride, kDisabledOverride);
			}
			result = adulterated.get();
		}
	}
	traceAssessment(assessment, assessment.type, result);
	return result.yield();

	END_CSAPI_ERRORS1(NULL)
}


//
// Policy editing operations.
// These all make permanent changes to the system-wide authority records.
//
CFStringRef kSecAssessmentContextKeyUpdate = CFSTR("update");
CFStringRef kSecAssessmentUpdateOperationAdd = CFSTR("update:add");
CFStringRef kSecAssessmentUpdateOperationRemove = CFSTR("update:remove");
CFStringRef kSecAssessmentUpdateOperationEnable = CFSTR("update:enable");
CFStringRef kSecAssessmentUpdateOperationDisable = CFSTR("update:disable");
CFStringRef kSecAssessmentUpdateOperationFind = CFSTR("update:find");

CFStringRef kSecAssessmentUpdateKeyAuthorization = CFSTR("update:authorization");
CFStringRef kSecAssessmentUpdateKeyPriority = CFSTR("update:priority");
CFStringRef kSecAssessmentUpdateKeyLabel = CFSTR("update:label");
CFStringRef kSecAssessmentUpdateKeyExpires = CFSTR("update:expires");
CFStringRef kSecAssessmentUpdateKeyAllow = CFSTR("update:allow");
CFStringRef kSecAssessmentUpdateKeyRemarks = CFSTR("update:remarks");

CFStringRef kSecAssessmentUpdateKeyRow = CFSTR("update:row");
CFStringRef kSecAssessmentUpdateKeyCount = CFSTR("update:count");
CFStringRef kSecAssessmentUpdateKeyFound = CFSTR("update:found");

CFStringRef kSecAssessmentRuleKeyID = CFSTR("rule:id");
CFStringRef kSecAssessmentRuleKeyPriority = CFSTR("rule:priority");
CFStringRef kSecAssessmentRuleKeyAllow = CFSTR("rule:allow");
CFStringRef kSecAssessmentRuleKeyLabel = CFSTR("rule:label");
CFStringRef kSecAssessmentRuleKeyRemarks = CFSTR("rule:remarks");
CFStringRef kSecAssessmentRuleKeyRequirement = CFSTR("rule:requirement");
CFStringRef kSecAssessmentRuleKeyType = CFSTR("rule:type");
CFStringRef kSecAssessmentRuleKeyExpires = CFSTR("rule:expires");
CFStringRef kSecAssessmentRuleKeyDisabled = CFSTR("rule:disabled");
CFStringRef kSecAssessmentRuleKeyBookmark = CFSTR("rule:bookmark");


Boolean SecAssessmentUpdate(CFTypeRef target,
	SecAssessmentFlags flags,
	CFDictionaryRef context,
	CFErrorRef *errors)
{
	if (CFDictionaryRef outcome = SecAssessmentCopyUpdate(target, flags, context, errors)) {
		CFRelease(outcome);
		return true;
	} else {
		return false;
	}
}

CFDictionaryRef SecAssessmentCopyUpdate(CFTypeRef target,
	SecAssessmentFlags flags,
	CFDictionaryRef context,
	CFErrorRef *errors)
{
	BEGIN_CSAPI

	CFDictionary ctx(context, errSecCSInvalidAttributeValues);
	CFRef<CFDictionaryRef> result;

	// make context exist and writable
	CFRef<CFMutableDictionaryRef> mcontext = context ? makeCFMutableDictionary(context) : makeCFMutableDictionary();
	
	if (CFDictionaryGetValue(mcontext, kSecAssessmentUpdateKeyAuthorization) == NULL) {
		// no authorization passed in. Make an empty one in this context
		AuthorizationRef authorization;
		MacOSError::check(AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authorization));
		AuthorizationExternalForm extform;
		MacOSError::check(AuthorizationMakeExternalForm(authorization, &extform));
		CFDictionaryAddValue(mcontext, kSecAssessmentUpdateKeyAuthorization, CFTempData(&extform, sizeof(extform)));
		if (!(flags & kSecAssessmentFlagDirect))
			AuthorizationFree(authorization, kAuthorizationFlagDefaults);
	}

	if (flags & kSecAssessmentFlagDirect) {
		// ask the engine right here to do its thing
		result.take(gEngine().update(target, flags, ctx));
	} else {
		// relay the question to our daemon for consideration
		result.take(xpcEngineUpdate(target, flags, ctx));
	}

	traceUpdate(target, context, result);
	return result.yield();

	END_CSAPI_ERRORS1(NULL)
}

static Boolean
updateAuthority(const char *authority, bool enable, CFErrorRef *errors)
{
	CFStringRef updateValue = enable ? kSecAssessmentUpdateOperationEnable : kSecAssessmentUpdateOperationDisable;
	CFTemp<CFDictionaryRef> ctx("{%O=%s, %O=%O}", kSecAssessmentUpdateKeyLabel, authority, kSecAssessmentContextKeyUpdate, updateValue);
	return SecAssessmentUpdate(NULL, kSecCSDefaultFlags, ctx, errors);
}

static Boolean
updateAuthorityLocal(const char *authority, bool enable, CFErrorRef *errors)
{
	CFDictionaryRef result = NULL;
	CFTemp<CFDictionaryRef> ctx("{%O=%s}", kSecAssessmentUpdateKeyLabel, authority);
	if (enable) {
		result = gEngine().enable(NULL, kAuthorityInvalid, kSecCSDefaultFlags, ctx, false);
	} else {
		result = gEngine().disable(NULL, kAuthorityInvalid, kSecCSDefaultFlags, ctx, false);
	}
	if (result) {
		return true;
	} else {
		return false;
	}
}

static Boolean
migrateSystemPolicy(void)
{
	CFErrorRef *errors = NULL;
	
	if (pathExists(oldPolicyDatabase)) {
		PolicyEngine oldEngine = PolicyEngine(oldPolicyDatabase);
		bool isDevIDAllowed = !(oldEngine.value<int>("SELECT disabled FROM authority WHERE label = 'Developer ID';", true));
		bool isNotarizedAllowed = !(oldEngine.value<int>("SELECT disabled FROM authority WHERE label = 'Notarized Developer ID';", true));
		
		updateAuthorityLocal("Developer ID", isDevIDAllowed, errors);
		updateAuthorityLocal("Testflight", isDevIDAllowed, errors);
		if (isDevIDAllowed) {
			updateAuthorityLocal("Notarized Developer ID", true, errors);
		}
		if (errors) {
			secerror("Unable to migrate developer ID app policy");
			return false;
		}
		
		updateAuthorityLocal("Notarized Developer ID", isNotarizedAllowed, errors);
		updateAuthorityLocal("Unnotarized Developer ID", isNotarizedAllowed, errors);
		if (errors) {
			secerror("Failed to migrate notarized app policy");
			return false;
		}
		
		oldEngine.close();
		// Perform best-effort cleanup
		unlink(oldPolicyDatabase);
	}
	
	// Perform best-effort cleanup
	if (pathExists(oldDefaultPolicyDatabase)) {
		unlink(oldDefaultPolicyDatabase);
	}

	if (pathExists(oldPrefsFile)) {
		if (copyfile(oldPrefsFile, prefsFile, NULL, COPYFILE_ALL) != 0) {
			secerror("Failed to copy %s to %s to reset system policy: errno=%d", oldPrefsFile, prefsFile, errno);
			return false;
		}
		if (chown(prefsFile, 0, 0)) {
			secerror("Failed to set root ownership on %s: %d", prefsFile, errno);
			return false;
		}
		if (chmod(prefsFile, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) {
			secerror("Failed to set set permissions on %s: %d", prefsFile, errno);
			return false;
		}
		
		// Perform best-effort cleanup
		unlink(oldPrefsFile);
	}

	return true;
}

//
// The fcntl of System Policies.
// For those very special requests.
//
Boolean SecAssessmentControl(CFStringRef control, void *arguments, CFErrorRef *errors)
{
	BEGIN_CSAPI
	
	if (CFEqual(control, CFSTR("ui-enable"))) {
		xpcEngineEnable();
		return true;
	} else if (CFEqual(control, CFSTR("ui-enable-local"))) {
		setAssessment(true);
		MessageTrace trace("com.apple.security.assessment.state", "enable");
		trace.send("enable assessment outcomes");
		return true;
	} else if (CFEqual(control, CFSTR("ui-disable"))) {
		xpcEngineDisable();
		return true;
	} else if (CFEqual(control, CFSTR("ui-disable-local"))) {
		setAssessment(false);
		MessageTrace trace("com.apple.security.assessment.state", "disable");
		trace.send("disable assessment outcomes");
		return true;
	} else if (CFEqual(control, CFSTR("ui-status"))) {
		CFBooleanRef &result = *(CFBooleanRef*)(arguments);
		if (overrideAssessment())
			result = kCFBooleanFalse;
		else
			result = kCFBooleanTrue;
		return true;
	} else if (CFEqual(control, CFSTR("ui-enable-devid"))) {
		updateAuthority("Developer ID", true, errors);
		updateAuthority("Notarized Developer ID", true, errors);
		updateAuthority("Testflight", true, errors);
		MessageTrace trace("com.apple.security.assessment.state", "enable-devid");
		trace.send("enable Developer ID approval");
		return true;
	} else if (CFEqual(control, CFSTR("ui-disable-devid"))) {
		updateAuthority("Developer ID", false, errors);
		updateAuthority("Testflight", false, errors);
		MessageTrace trace("com.apple.security.assessment.state", "disable-devid");
		trace.send("disable Developer ID approval");
		return true;
    } else if (CFEqual(control, CFSTR("ui-get-devid"))) {
        xpcEngineCheckDevID((CFBooleanRef*)(arguments));
        return true;
    } else if (CFEqual(control, CFSTR("ui-get-devid-local"))) {
		CFBooleanRef &result = *(CFBooleanRef*)(arguments);
		if (gEngine().value<int>("SELECT disabled FROM authority WHERE label = 'Developer ID';", true))
			result = kCFBooleanFalse;
		else
			result = kCFBooleanTrue;
		return true;
	} else if (CFEqual(control, CFSTR("ui-enable-notarized"))) {
		updateAuthority("Notarized Developer ID", true, errors);
		updateAuthority("Unnotarized Developer ID", true, errors);
		MessageTrace trace("com.apple.security.assessment.state", "enable-notarized");
		trace.send("enable Notarized Developer ID approval");
		return true;
	} else if (CFEqual(control, CFSTR("ui-disable-notarized"))) {
		updateAuthority("Notarized Developer ID", false, errors);
		updateAuthority("Unnotarized Developer ID", false, errors);
		MessageTrace trace("com.apple.security.assessment.state", "disable-notarized");
		trace.send("disable Notarized Developer ID approval");
		return true;
	} else if (CFEqual(control, CFSTR("ui-get-notarized"))) {
		xpcEngineCheckNotarized((CFBooleanRef*)(arguments));
		return true;
	} else if (CFEqual(control, CFSTR("ui-get-notarized-local"))) {
		CFBooleanRef &result = *(CFBooleanRef*)(arguments);
		if (gEngine().value<int>("SELECT disabled FROM authority WHERE label = 'Notarized Developer ID';", true))
			result = kCFBooleanFalse;
		else
			result = kCFBooleanTrue;
		return true;
	} else if (CFEqual(control, CFSTR("ui-record-reject"))) {
		// send this through syspolicyd for update validation
		xpcEngineRecord(CFDictionaryRef(arguments));
		return true;
	} else if (CFEqual(control, CFSTR("ui-record-reject-local"))) {
		// perform the local operation (requires root)
		gEngine().recordFailure(CFDictionaryRef(arguments));
		return true;
	} else if (CFEqual(control, CFSTR("ui-recall-reject"))) {
		// no special privileges required for this, so read directly
		CFDictionaryRef &result = *(CFDictionaryRef*)(arguments);
		CFRef<CFDataRef> infoData = cfLoadFile(lastRejectFile);
		if (infoData)
			result = makeCFDictionaryFrom(infoData);
		else
			result = NULL;
		return true;
	} else if (CFEqual(control, CFSTR("rearm-status"))) {
		CFTimeInterval &result = *(CFTimeInterval*)(arguments);
		if (!queryRearmTimer(result))
			result = 0;
		return true;
	} else if (CFEqual(control, CFSTR("migrate"))) {
		return migrateSystemPolicy();
	} else
		MacOSError::throwMe(errSecCSInvalidAttributeValues);

	END_CSAPI_ERRORS1(false)
}

//
// The standard set of ticket functions that use XPC to talk to syspolicyd.
//
static Boolean SecAssessmentTicketRegisterXPC(CFDataRef ticketData, CFErrorRef *errors)
{
	BEGIN_CSAPI

	xpcEngineTicketRegister(ticketData);
	return true;

	END_CSAPI_ERRORS1(false)
}

static Boolean SecAssessmentTicketLookupXPC(CFDataRef hash, SecCSDigestAlgorithm hashType, SecAssessmentTicketFlags flags, double *date, CFErrorRef *errors)
{
	BEGIN_CSAPI

	xpcEngineTicketLookup(hash, hashType, flags, date);
	return true;

	END_CSAPI_ERRORS1(false)
}

static Boolean SecAssessmentLegacyCheckXPC(CFDataRef hash, SecCSDigestAlgorithm hashType, CFStringRef teamID, CFErrorRef *errors)
{
	BEGIN_CSAPI

	xpcEngineLegacyCheck(hash, hashType, teamID);
	return true;

	END_CSAPI_ERRORS1(false)
}


// A small set of SecAssessment ticket functions can be overridden within syspolicyd to improve performance by skipping the XPC layer.
// A global structure holds PAC'd pointers for all 3 functions, which are initialized to the normal XPC functions for all processes.
// A single registration function can be used to override these, but only by syspolicyd.

#if __has_feature(ptrauth_calls)
#define SECASC_PTRAUTH(op) __ptrauth(ptrauth_key_process_independent_code, true, ptrauth_string_discriminator("secasc." #op)) op
#else
#define SECASC_PTRAUTH(op) op
#endif

typedef struct SecAssessmentLocalFunctions {
	Boolean (* SECASC_PTRAUTH(ticketRegistration))(CFDataRef, CFErrorRef *);
	Boolean (* SECASC_PTRAUTH(ticketLookup))(CFDataRef, SecCSDigestAlgorithm, SecAssessmentTicketFlags, double *, CFErrorRef *);
	Boolean (* SECASC_PTRAUTH(legacyCheck))(CFDataRef, SecCSDigestAlgorithm, CFStringRef, CFErrorRef *);
} SecAssessmentLocalFunctions_t;

static SecAssessmentFunctions_t sImplementation = {
	.ticketRegistration = SecAssessmentTicketRegisterXPC,
	.ticketLookup = SecAssessmentTicketLookupXPC,
	.legacyCheck = SecAssessmentLegacyCheckXPC,
};

Boolean
SecAssessmentRegisterFunctions(SecAssessmentFunctions_t *overrides)
{
	// This is only intended to be used from syspolicyd, so prevent all other callers here.
	CFRef<SecTaskRef> task = SecTaskCreateFromSelf(NULL);
	if (task.get() == NULL) {
		return false;
	}

	uint32_t flags = SecTaskGetCodeSignStatus(task);
	if ((flags & kSecCodeStatusPlatform) != kSecCodeStatusPlatform) {
		return false;
	}

	CFRef<CFStringRef> signingID = SecTaskCopySigningIdentifier(task, NULL);
	if (signingID.get() == NULL) {
		return false;
	}

	if (CFEqual(signingID, CFSTR("com.apple.syspolicyd"))) {
		sImplementation.ticketLookup = overrides->ticketLookup;
		sImplementation.ticketRegistration = overrides->ticketRegistration;
		sImplementation.legacyCheck = overrides->legacyCheck;
		return true;
	}

	return false;
}

Boolean SecAssessmentRegisterPackageTicket(CFURLRef packageURL, CFErrorRef* errors)
{
	BEGIN_CSAPI
	
	string path = cfString(packageURL);
	Xar xar(path.c_str());
	
	if (!xar) {
		MacOSError::throwMe(errSecParam);
	}
	
	xar.registerStapledNotarization();
	return true;
	
	END_CSAPI_ERRORS1(false)
}

Boolean SecAssessmentTicketRegister(CFDataRef ticketData, CFErrorRef *errors)
{
	return sImplementation.ticketRegistration(ticketData, errors);
}

Boolean SecAssessmentTicketLookup(CFDataRef hash, SecCSDigestAlgorithm hashType, SecAssessmentTicketFlags flags, double *date, CFErrorRef *errors)
{
	return sImplementation.ticketLookup(hash, hashType, flags, date, errors);
}

Boolean SecAssessmentLegacyCheck(CFDataRef hash, SecCSDigestAlgorithm hashType, CFStringRef teamID, CFErrorRef *errors)
{
	return sImplementation.legacyCheck(hash, hashType, teamID, errors);
}
