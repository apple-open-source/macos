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
#ifndef _H_POLICYENGINE
#define _H_POLICYENGINE

#include "SecAssessment.h"
#include "opaqueallowlist.h"
#include "evaluationmanager.h"
#include "policydb.h"
#include <security_utilities/globalizer.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/hashing.h>
#include <security_utilities/sqlite++.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/CodeSigning.h>

namespace Security {
namespace CodeSigning {


typedef uint EngineOperation;
enum {
	opInvalid = 0,
	opEvaluate,
	opAddAuthority,
	opRemoveAuthority,
};


class PolicyEngine : public PolicyDatabase {
public:
	PolicyEngine();
	PolicyEngine(const char *path);
	virtual ~PolicyEngine();

public:
	void evaluate(CFURLRef path, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context, CFMutableDictionaryRef result);

	CFDictionaryRef update(CFTypeRef target, SecAssessmentFlags flags, CFDictionaryRef context);
	CFDictionaryRef add(CFTypeRef target, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context);
	CFDictionaryRef remove(CFTypeRef target, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context);
	CFDictionaryRef enable(CFTypeRef target, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context, bool authorize);
	CFDictionaryRef disable(CFTypeRef target, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context, bool authorize);
	CFDictionaryRef find(CFTypeRef target, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context);

	void recordFailure(CFDictionaryRef info);

public:
	static void addAuthority(SecAssessmentFlags flags, CFMutableDictionaryRef parent, const char *label, SQLite::int64 row = 0, CFTypeRef cacheInfo = NULL, bool weak = false, uint64_t ruleFlags = 0);
	static void addToAuthority(CFMutableDictionaryRef parent, CFStringRef key, CFTypeRef value);

private:
	void evaluateCode(CFURLRef path, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context, CFMutableDictionaryRef result, bool handleUnsigned);
	void evaluateInstall(CFURLRef path, SecAssessmentFlags flags, CFDictionaryRef context, CFMutableDictionaryRef result);
	void evaluateDocOpen(CFURLRef path, SecAssessmentFlags flags, CFDictionaryRef context, CFMutableDictionaryRef result);
	
	void evaluateCodeItem(SecStaticCodeRef code, CFURLRef path, AuthorityType type, SecAssessmentFlags flags, bool nested, CFMutableDictionaryRef result);
	void adjustValidation(SecStaticCodeRef code);
	bool temporarySigning(SecStaticCodeRef code, AuthorityType type, CFURLRef path, SecAssessmentFlags matchFlags);
	void normalizeTarget(CFRef<CFTypeRef> &target, AuthorityType type, CFDictionary &context, std::string *signUnsigned);
	
	void selectRules(SQLite::Statement &action, std::string stanza, std::string table,
		CFTypeRef inTarget, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context, std::string suffix = "");
	CFDictionaryRef manipulateRules(const std::string &stanza,
		CFTypeRef target, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context, bool authorize);

	void setOrigin(CFArrayRef chain, CFMutableDictionaryRef result);

	void recordOutcome(SecStaticCodeRef code, bool allow, AuthorityType type, double expires, SQLite::int64 authority);

private:
	OpaqueAllowlist* mOpaqueAllowlist;
	CFDictionaryRef opaqueAllowlistValidationConditionsFor(SecStaticCodeRef code);
	bool opaqueAllowlistContains(SecStaticCodeRef code, SecAssessmentFeedback feedback, OSStatus reason);
	void opaqueAllowlistAdd(SecStaticCodeRef code);

    friend class EvaluationManager;
    friend class EvaluationTask;
};


} // end namespace CodeSigning
} // end namespace Security

#endif //_H_POLICYENGINE
