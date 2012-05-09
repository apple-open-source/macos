/*
 *  Copyright (c) 2003-2004,2008-2009 Apple Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 *
 *  AuthorizationRule.cpp
 *  Security
 *
 */

#include "AuthorizationRule.h"
#include <Security/AuthorizationTags.h>
#include <Security/AuthorizationTagsPriv.h>
#include <Security/AuthorizationDB.h>
#include <Security/AuthorizationPriv.h>
#include <security_utilities/logging.h>
#include <bsm/audit_uevents.h>
#include "ccaudit_extensions.h"
#include "authority.h"
#include "server.h"
#include "process.h"
#include "agentquery.h"
#include "AuthorizationMechEval.h"

#include <asl.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <membership.h>

extern "C" {
#include <membershipPriv.h>
}

using namespace CommonCriteria::Securityd;
    
//
// Rule class
//
namespace Authorization {

CFStringRef RuleImpl::kUserGroupID = CFSTR(kAuthorizationRuleParameterGroup);
CFStringRef RuleImpl::kTimeoutID = CFSTR(kAuthorizationRuleParameterCredentialTimeout);
CFStringRef RuleImpl::kSharedID = CFSTR(kAuthorizationRuleParameterCredentialShared);
CFStringRef RuleImpl::kAllowRootID = CFSTR(kAuthorizationRuleParameterAllowRoot);
CFStringRef RuleImpl::kMechanismsID = CFSTR(kAuthorizationRuleParameterMechanisms);
CFStringRef RuleImpl::kSessionOwnerID = CFSTR(kAuthorizationRuleParameterCredentialSessionOwner);
CFStringRef RuleImpl::kKofNID = CFSTR(kAuthorizationRuleParameterKofN);
CFStringRef RuleImpl::kPromptID = CFSTR(kAuthorizationRuleParameterDefaultPrompt);
CFStringRef RuleImpl::kButtonID = CFSTR(kAuthorizationRuleParameterDefaultButton);
CFStringRef RuleImpl::kTriesID = CFSTR("tries"); // XXX/cs move to AuthorizationTagsPriv.h
CFStringRef RuleImpl::kExtractPasswordID = CFSTR(kAuthorizationRuleParameterExtractPassword);

CFStringRef RuleImpl::kRuleClassID = CFSTR(kAuthorizationRuleClass);
CFStringRef RuleImpl::kRuleAllowID = CFSTR(kAuthorizationRuleClassAllow);
CFStringRef RuleImpl::kRuleDenyID = CFSTR(kAuthorizationRuleClassDeny);
CFStringRef RuleImpl::kRuleUserID = CFSTR(kAuthorizationRuleClassUser);
CFStringRef RuleImpl::kRuleDelegateID = CFSTR(kAuthorizationRightRule);
CFStringRef RuleImpl::kRuleMechanismsID = CFSTR(kAuthorizationRuleClassMechanisms);
CFStringRef RuleImpl::kRuleAuthenticateUserID = CFSTR(kAuthorizationRuleParameterAuthenticateUser);


string
RuleImpl::Attribute::getString(CFDictionaryRef config, CFStringRef key, bool required = false, const char *defaultValue = "")
{
	CFTypeRef value = CFDictionaryGetValue(config, key);
	if (value && (CFGetTypeID(value) == CFStringGetTypeID()))
	{
		CFStringRef stringValue = reinterpret_cast<CFStringRef>(value);
		char buffer[512];
		const char *ptr = CFStringGetCStringPtr(stringValue, kCFStringEncodingUTF8);
		if (ptr == NULL)
		{
			if (CFStringGetCString(stringValue, buffer, sizeof(buffer), kCFStringEncodingUTF8))
				ptr = buffer;
			else
			{
				Syslog::alert("Could not convert CFString to C string");
				MacOSError::throwMe(errAuthorizationInternal);
			}
		}

		return string(ptr);
	}
	else
		if (!required)
			return string(defaultValue);
		else
		{
			Syslog::alert("Failed to get rule string");
			MacOSError::throwMe(errAuthorizationInternal);
		}
}			

double
RuleImpl::Attribute::getDouble(CFDictionaryRef config, CFStringRef key, bool required = false, double defaultValue = 0.0)
{
	double doubleValue = 0;

	CFTypeRef value = CFDictionaryGetValue(config, key);
	if (value && (CFGetTypeID(value) == CFNumberGetTypeID()))
	{
		CFNumberGetValue(reinterpret_cast<CFNumberRef>(value), kCFNumberDoubleType, &doubleValue);
	}
	else
		if (!required)
			return defaultValue;
		else
		{
			Syslog::alert("Failed to get rule double value");
			MacOSError::throwMe(errAuthorizationInternal);
		}
			
	return doubleValue;
}

bool
RuleImpl::Attribute::getBool(CFDictionaryRef config, CFStringRef key, bool required = false, bool defaultValue = false)
{
	bool boolValue = false;
	CFTypeRef value = CFDictionaryGetValue(config, key);
	
	if (value && (CFGetTypeID(value) == CFBooleanGetTypeID()))
	{
		boolValue = CFBooleanGetValue(reinterpret_cast<CFBooleanRef>(value));
	}
	else
		if (!required)
			return defaultValue;
		else
		{
			Syslog::alert("Failed to get rule bool value");
			MacOSError::throwMe(errAuthorizationInternal);
		}
	
	return boolValue;
}

vector<string>
RuleImpl::Attribute::getVector(CFDictionaryRef config, CFStringRef key, bool required = false)
{
	vector<string> valueArray;
	
	CFTypeRef value = CFDictionaryGetValue(config, key);
	if (value && (CFGetTypeID(value) == CFArrayGetTypeID()))
	{
		CFArrayRef evalArray = reinterpret_cast<CFArrayRef>(value);

        CFIndex numItems = CFArrayGetCount(evalArray);
		for (CFIndex index=0; index < numItems; index++)
		{
			CFTypeRef arrayValue = CFArrayGetValueAtIndex(evalArray, index);
			if (arrayValue && (CFGetTypeID(arrayValue) == CFStringGetTypeID()))
			{
				CFStringRef stringValue = reinterpret_cast<CFStringRef>(arrayValue);
				char buffer[512];
				const char *ptr = CFStringGetCStringPtr(stringValue, kCFStringEncodingUTF8);
				if (ptr == NULL)
				{
					if (CFStringGetCString(stringValue, buffer, sizeof(buffer), kCFStringEncodingUTF8))
						ptr = buffer;
					else
					{
						Syslog::alert("Failed to convert CFString to C string for item %u in array", index);
						MacOSError::throwMe(errAuthorizationInternal);
					}
				}
				valueArray.push_back(string(ptr));
			}
		}
	}
	else
		if (required)
		{
			Syslog::alert("Value for key either not present or not a CFArray");
			MacOSError::throwMe(errAuthorizationInternal);
		}
			
	return valueArray;
}


bool RuleImpl::Attribute::getLocalizedText(CFDictionaryRef config, map<string,string> &localizedPrompts, CFStringRef dictKey, const char *descriptionKey)
{
	CFIndex numberOfPrompts = 0;
	CFDictionaryRef promptsDict;
	if (CFDictionaryContainsKey(config, dictKey))
	{
		promptsDict = reinterpret_cast<CFDictionaryRef>(CFDictionaryGetValue(config, dictKey));
		if (promptsDict && (CFGetTypeID(promptsDict) == CFDictionaryGetTypeID()))
			numberOfPrompts = CFDictionaryGetCount(promptsDict);
	}
	if (numberOfPrompts == 0)
		return false;

	const void *keys[numberOfPrompts+1];
	const void *values[numberOfPrompts+1];
	CFDictionaryGetKeysAndValues(promptsDict, &keys[0], &values[0]);
	
	while (numberOfPrompts-- > 0)
	{
		CFStringRef keyRef = reinterpret_cast<CFStringRef>(keys[numberOfPrompts]);
		CFStringRef valueRef = reinterpret_cast<CFStringRef>(values[numberOfPrompts]);
		if (!keyRef || (CFGetTypeID(keyRef) != CFStringGetTypeID())) {
			continue;
		}
		if (!valueRef || (CFGetTypeID(valueRef) != CFStringGetTypeID())) {
			continue;
		}
		string key = cfString(keyRef);
		string value = cfString(valueRef);
		localizedPrompts[descriptionKey + key] = value;
	}

	return true;
}


// default rule
RuleImpl::RuleImpl() :
mType(kUser), mGroupName("admin"), mMaxCredentialAge(300.0), mShared(true), mAllowRoot(false), mSessionOwner(false), mTries(0), mAuthenticateUser(true), mExtractPassword(false)
{
	// XXX/cs read default descriptions from somewhere
	// @@@ Default rule is shared admin group with 5 minute timeout
}

// return rule built from rule definition; throw if invalid.
RuleImpl::RuleImpl(const string &inRightName, CFDictionaryRef cfRight, CFDictionaryRef cfRules) : mRightName(inRightName), mExtractPassword(false)
{
	// @@@ make sure cfRight is non mutable and never used that way
	
	if (CFGetTypeID(cfRight) != CFDictionaryGetTypeID())
	{
		Syslog::alert("Invalid rights set");
		MacOSError::throwMe(errAuthorizationInternal);
	}
			
	mTries = 0;

	string classTag = Attribute::getString(cfRight, kRuleClassID, false, "");

	if (classTag.length())
	{
		if (classTag == kAuthorizationRuleClassAllow)
		{
			secdebug("authrule", "%s : rule allow", inRightName.c_str());
			mType = kAllow;
		}
		else if (classTag == kAuthorizationRuleClassDeny)
		{
			secdebug("authrule", "%s : rule deny", inRightName.c_str());
			mType = kDeny;
		}
		else if (classTag == kAuthorizationRuleClassUser)
		{
			mType = kUser;
			mGroupName = Attribute::getString(cfRight, kUserGroupID);
			// grab other user-in-group attributes
			mMaxCredentialAge = Attribute::getDouble(cfRight, kTimeoutID, false, DBL_MAX);
			mShared = Attribute::getBool(cfRight, kSharedID);
			mAllowRoot = Attribute::getBool(cfRight, kAllowRootID);
			mSessionOwner = Attribute::getBool(cfRight, kSessionOwnerID);
			// authorization tags can have eval now too
			mEvalDef = Attribute::getVector(cfRight, kMechanismsID);
			if (mEvalDef.size() == 0 && cfRules /*only rights default see appserver-admin*/)
			{
				CFDictionaryRef cfRuleDef = reinterpret_cast<CFDictionaryRef>(CFDictionaryGetValue(cfRules, CFSTR("authenticate")));
				if (cfRuleDef && CFGetTypeID(cfRuleDef) == CFDictionaryGetTypeID())
					mEvalDef = Attribute::getVector(cfRuleDef, kMechanismsID);
			}
			mTries = int(Attribute::getDouble(cfRight, kTriesID, false, double(kMaximumAuthorizationTries)));
			mAuthenticateUser = Attribute::getBool(cfRight, kRuleAuthenticateUserID, false, true);
			mExtractPassword = Attribute::getBool(cfRight, kExtractPasswordID, false, false);

			secdebug("authrule", "%s : rule user in group \"%s\" timeout %g%s%s",
				inRightName.c_str(),
				mGroupName.c_str(), mMaxCredentialAge, mShared ? " shared" : "",
				mAllowRoot ? " allow-root" : "");

		}
		else if (classTag == kAuthorizationRuleClassMechanisms)
		{
			secdebug("authrule", "%s : rule evaluate mechanisms", inRightName.c_str());
			mType = kEvaluateMechanisms;
			// mechanisms to evaluate
			mEvalDef = Attribute::getVector(cfRight, kMechanismsID, true);
			mTries = int(Attribute::getDouble(cfRight, kTriesID, false, 0.0)); // "forever"
			mShared = Attribute::getBool(cfRight, kSharedID, false, true);
			mExtractPassword = Attribute::getBool(cfRight, kExtractPasswordID, false, false);
		}
		else if (classTag == kAuthorizationRightRule)
		{
			assert(cfRules); // rules can't delegate to other rules
			secdebug("authrule", "%s : rule delegate rule", inRightName.c_str());
			mType = kRuleDelegation;

			// string or 
			string ruleDefString = Attribute::getString(cfRight, kRuleDelegateID, false, "");
			if (ruleDefString.length())
			{
				CFStringRef ruleDefRef = makeCFString(ruleDefString);
				CFDictionaryRef cfRuleDef = reinterpret_cast<CFDictionaryRef>(CFDictionaryGetValue(cfRules, ruleDefRef));
				if (ruleDefRef)
					CFRelease(ruleDefRef);
				if (!cfRuleDef || CFGetTypeID(cfRuleDef) != CFDictionaryGetTypeID())
				{
					Syslog::alert("'%s' does not name a built-in rule", ruleDefString.c_str());
					MacOSError::throwMe(errAuthorizationInternal);
				}
				mRuleDef.push_back(Rule(ruleDefString, cfRuleDef, cfRules));
			}
			else // array
			{
				vector<string> ruleDef = Attribute::getVector(cfRight, kRuleDelegateID, true);
				for (vector<string>::const_iterator it = ruleDef.begin(); it != ruleDef.end(); it++)
				{
					CFStringRef ruleNameRef = makeCFString(*it);
					CFDictionaryRef cfRuleDef = reinterpret_cast<CFDictionaryRef>(CFDictionaryGetValue(cfRules, ruleNameRef));
					if (ruleNameRef)
						CFRelease(ruleNameRef);
					if (!cfRuleDef || (CFGetTypeID(cfRuleDef) != CFDictionaryGetTypeID()))
					{
						Syslog::alert("Invalid rule '%s'in rule set", it->c_str());
						MacOSError::throwMe(errAuthorizationInternal);
					}
					mRuleDef.push_back(Rule(*it, cfRuleDef, cfRules));
				}
			}

			mKofN = int(Attribute::getDouble(cfRight, kKofNID, false, 0.0));
			if (mKofN)
				mType = kKofN;

		}
		else
		{
			secdebug("authrule", "%s : rule class '%s' unknown.", inRightName.c_str(), classTag.c_str());
			Syslog::alert("%s : rule class '%s' unknown", inRightName.c_str(), classTag.c_str());
			MacOSError::throwMe(errAuthorizationInternal);
		}
	}
	else
	{
		// no class tag means, this is the abbreviated specification from the API
		// it _must_ have a definition for "rule" which will be used as a delegate
		// it may have a comment (not extracted here)
		// it may have a default prompt, or a whole dictionary of languages (not extracted here)
		mType = kRuleDelegation;
		string ruleName = Attribute::getString(cfRight, kRuleDelegateID, true);
		secdebug("authrule", "%s : rule delegate rule (1): %s", inRightName.c_str(), ruleName.c_str());
		CFStringRef ruleNameRef = makeCFString(ruleName);
		CFDictionaryRef cfRuleDef = reinterpret_cast<CFDictionaryRef>(CFDictionaryGetValue(cfRules, ruleNameRef));
		if (ruleNameRef)
			CFRelease(ruleNameRef);
		if (!cfRuleDef || CFGetTypeID(cfRuleDef) != CFDictionaryGetTypeID())
		{
			Syslog::alert("Rule '%s' for right '%s' does not exist or is not properly formed", ruleName.c_str(), inRightName.c_str());
			MacOSError::throwMe(errAuthorizationInternal);
		}
		mRuleDef.push_back(Rule(ruleName, cfRuleDef, cfRules));
	}

	Attribute::getLocalizedText(cfRight, mLocalizedPrompts, kPromptID, kAuthorizationRuleParameterDescription);
	Attribute::getLocalizedText(cfRight, mLocalizedButtons, kButtonID, kAuthorizationRuleParameterButton);
}

/*
RuleImpl::~Rule()
{
}
*/

void
RuleImpl::setAgentHints(const AuthItemRef &inRight, const Rule &inTopLevelRule, AuthItemSet &environmentToClient, AuthorizationToken &auth) const
{
	string authorizeString(inRight->name());
	environmentToClient.erase(AuthItemRef(AGENT_HINT_AUTHORIZE_RIGHT)); 
	environmentToClient.insert(AuthItemRef(AGENT_HINT_AUTHORIZE_RIGHT, AuthValueOverlay(authorizeString)));

	pid_t creatorPid = auth.creatorPid();
	environmentToClient.erase(AuthItemRef(AGENT_HINT_CREATOR_PID)); 
	environmentToClient.insert(AuthItemRef(AGENT_HINT_CREATOR_PID, AuthValueOverlay(sizeof(pid_t), &creatorPid)));

	audit_token_t creatorAuditToken = auth.creatorAuditToken().auditToken();
	environmentToClient.erase(AuthItemRef(AGENT_HINT_CREATOR_AUDIT_TOKEN));
	environmentToClient.insert(AuthItemRef(AGENT_HINT_CREATOR_AUDIT_TOKEN, AuthValueOverlay(sizeof(audit_token_t), &creatorAuditToken)));

	Process &thisProcess = Server::process();
	string bundlePath;
	if (SecStaticCodeRef clientCode = auth.creatorCode())
		bundlePath = codePath(clientCode);
	AuthItemSet processHints = SecurityAgent::Client::clientHints(
		SecurityAgent::bundle, bundlePath, thisProcess.pid(), thisProcess.uid());
	environmentToClient.erase(AuthItemRef(AGENT_HINT_CLIENT_TYPE));
	environmentToClient.erase(AuthItemRef(AGENT_HINT_CLIENT_PATH));
	environmentToClient.erase(AuthItemRef(AGENT_HINT_CLIENT_PID));
	environmentToClient.erase(AuthItemRef(AGENT_HINT_CLIENT_UID));
	environmentToClient.insert(processHints.begin(), processHints.end());

	map<string,string> defaultPrompts = inTopLevelRule->localizedPrompts();
	map<string,string> defaultButtons = inTopLevelRule->localizedButtons();

	if (defaultPrompts.empty())
		defaultPrompts = localizedPrompts();
	if (defaultButtons.empty())
		defaultButtons = localizedButtons();
		
	if (!defaultPrompts.empty())
	{
		map<string,string>::const_iterator it;
		for (it = defaultPrompts.begin(); it != defaultPrompts.end(); it++)
		{
			const string &key = it->first;
			const string &value = it->second;
			environmentToClient.insert(AuthItemRef(key.c_str(), AuthValueOverlay(value)));
		}
	}
	if (!defaultButtons.empty())
	{
		map<string,string>::const_iterator it;
		for (it = defaultButtons.begin(); it != defaultButtons.end(); it++)
		{
			const string &key = it->first;
			const string &value = it->second;
			environmentToClient.insert(AuthItemRef(key.c_str(), AuthValueOverlay(value)));
		}
	}	

	// add rulename as a hint
	string ruleName = name();
    environmentToClient.erase(AuthItemRef(AGENT_HINT_AUTHORIZE_RULE));
	environmentToClient.insert(AuthItemRef(AGENT_HINT_AUTHORIZE_RULE, AuthValueOverlay(ruleName)));
}

// If a different evaluation for getting a credential is prescribed,
// we'll run that and validate the credentials from there.
// we fall back on a default configuration from the authenticate rule
OSStatus
RuleImpl::evaluateAuthentication(const AuthItemRef &inRight, const Rule &inRule,AuthItemSet &environmentToClient, AuthorizationFlags flags, CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials, AuthorizationToken &auth, SecurityAgent::Reason &reason, bool savePassword) const
{
	OSStatus status = errAuthorizationDenied;

	Credential hintCredential;
	if (errAuthorizationSuccess == evaluateSessionOwner(inRight, inRule, environmentToClient, now, auth, hintCredential, reason)) {
		if (hintCredential->name().length())
			environmentToClient.insert(AuthItemRef(AGENT_HINT_SUGGESTED_USER, AuthValueOverlay(hintCredential->name())));
		if (hintCredential->realname().length())
			environmentToClient.insert(AuthItemRef(AGENT_HINT_SUGGESTED_USER_LONG, AuthValueOverlay(hintCredential->realname())));
	}

	if ((mType == kUser) && (mGroupName.length()))
		environmentToClient.insert(AuthItemRef(AGENT_HINT_REQUIRE_USER_IN_GROUP, AuthValueOverlay(mGroupName)));

	uint32 tries;
	reason = SecurityAgent::noReason;

	Process &cltProc = Server::process();
	// Authorization preserves creator's UID in setuid processes
    // (which is nice, but cltUid ends up being unused except by the debug
    // message -- AgentMechanismEvaluator ignores it)
	uid_t cltUid = (cltProc.uid() != 0) ? cltProc.uid() : auth.creatorUid();
	secdebug("AuthEvalMech", "Mechanism invocation by process %d (UID %d)", cltProc.pid(), cltUid);
 
    // For auditing within AuthorizationMechEval, pass the right name.  
    size_t rightNameSize = inRight->name() ? strlen(inRight->name()) : 0;
    AuthorizationString rightName = inRight->name() ? inRight->name() : "";
    // @@@  AuthValueRef's ctor ought to take a const void *
    AuthValueRef rightValue(rightNameSize, const_cast<char *>(rightName));
    AuthValueVector authValueVector;
    authValueVector.push_back(rightValue);
    
    RightAuthenticationLogger rightAuthLogger(auth.creatorAuditToken(), AUE_ssauthint);
    rightAuthLogger.setRight(rightName);

	// Just succeed for a continuously active session owner.
	if (auth.session().originatorUid() == auth.creatorUid() && auth.session().attributes() & AU_SESSION_FLAG_HAS_AUTHENTICATED) {
		secdebug("AuthEvalMech", "We are an active session owner.");
		aslmsg m = asl_new(ASL_TYPE_MSG);
		asl_set(m, "com.apple.message.domain", "com.apple.securityd.UserActivity");
		asl_set(m, "com.apple.message.signature", "userIsActive");
		asl_set(m, "com.apple.message.signature2", rightName);
		asl_set(m, "com.apple.message.result", "failure");
		asl_log(NULL, m, ASL_LEVEL_NOTICE, "We are an active session owner.");
		asl_free(m);
//		Credential rightCredential(rightName, auth.creatorUid(), mShared);
//		credentials.erase(rightCredential); credentials.insert(rightCredential);
//		return errAuthorizationSuccess;
	}
	else {
		secdebug("AuthEvalMech", "We are not an active session owner.");
		aslmsg m = asl_new(ASL_TYPE_MSG);
		asl_set(m, "com.apple.message.domain", "com.apple.securityd.UserActivity");
		asl_set(m, "com.apple.message.signature", "userIsNotActive");
		asl_set(m, "com.apple.message.signature2", rightName);
		asl_set(m, "com.apple.message.result", "success");
		asl_log(NULL, m, ASL_LEVEL_NOTICE, "We are not an active session owner.");
		asl_free(m);
	}
	
	AgentMechanismEvaluator eval(cltUid, auth.session(), mEvalDef);

	for (tries = 0; tries < mTries; tries++)
	{
		AuthItemRef retryHint(AGENT_HINT_RETRY_REASON, AuthValueOverlay(sizeof(reason), &reason));
		environmentToClient.erase(retryHint); environmentToClient.insert(retryHint); // replace
		AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(tries), &tries));
		environmentToClient.erase(triesHint); environmentToClient.insert(triesHint); // replace

            status = eval.run(authValueVector, environmentToClient, auth);

            if ((status == errAuthorizationSuccess) ||
                (status == errAuthorizationCanceled)) // @@@ can only pass back sideband through context
            {
                secdebug("AuthEvalMech", "storing new context for authorization");
                auth.setInfoSet(eval.context(), savePassword);
            }

            // successfully ran mechanisms to obtain credential
            if (status == errAuthorizationSuccess)
            {
                // deny is the default
                status = errAuthorizationDenied;
                
                CredentialSet newCredentials = makeCredentials(auth);
                // clear context after extracting credentials
                auth.scrubInfoSet(savePassword);
                
                for (CredentialSet::const_iterator it = newCredentials.begin(); it != newCredentials.end(); ++it)
                {
                    const Credential& newCredential = *it;

                    // @@@ we log the uid a process was running under when it created the authref, which is misleading in the case of loginwindow
                    if (newCredential->isValid()) {
                        Syslog::info("UID %u authenticated as user %s (UID %u) for right '%s'", auth.creatorUid(), newCredential->name().c_str(), newCredential->uid(), rightName);
                        rightAuthLogger.logSuccess(auth.creatorUid(), newCredential->uid(), newCredential->name().c_str());
                    } else {
                        // we can't be sure that the user actually exists so inhibit logging of uid
                        Syslog::error("UID %u failed to authenticate as user '%s' for right '%s'", auth.creatorUid(), newCredential->name().c_str(), rightName);
                        rightAuthLogger.logFailure(auth.creatorUid(), newCredential->name().c_str());
                    }
                    
                    if (!newCredential->isValid())
                    {
                        reason = SecurityAgent::invalidPassphrase;
                        continue;
                    }

                    // verify that this credential authorizes right
                    status = evaluateUserCredentialForRight(auth, inRight, inRule, environmentToClient, now, newCredential, true, reason);
                    
                    if (status == errAuthorizationSuccess)
                    {
                        if (auth.operatesAsLeastPrivileged()) {
                            Credential rightCredential(rightName, mShared);
                            credentials.erase(rightCredential); credentials.insert(rightCredential);
                            if (mShared)
                                credentials.insert(Credential(rightName, false));
                        } 

                        // whack an equivalent credential, so it gets updated to a later achieved credential which must have been more stringent
                        credentials.erase(newCredential); credentials.insert(newCredential);
                        // just got a new credential - if it's shared also add a non-shared one that to stick in the authorizationref local cache
                        if (mShared)
                            credentials.insert(Credential(newCredential->uid(), newCredential->name(), newCredential->realname(), false));
                        
                        // use valid credential to set context info
                        // XXX/cs keeping this for now, such that the uid is passed back
                        auth.setCredentialInfo(newCredential, savePassword);
                        secdebug("SSevalMech", "added valid credential for user %s", newCredential->name().c_str());
						// set the sessionHasAuthenticated
						if (newCredential->uid() == auth.session().originatorUid()) {
							secdebug("AuthEvalMech", "We authenticated as the session owner.\n");
							SessionAttributeBits flags = auth.session().attributes();
							flags |= AU_SESSION_FLAG_HAS_AUTHENTICATED;
							auth.session().setAttributes(flags);
						}

                        status = errAuthorizationSuccess;
                        break;
                    }
                }

			if (status == errAuthorizationSuccess)
				break;
		}
		else
			if ((status == errAuthorizationCanceled) || (status == errAuthorizationInternal))
			{
				auth.scrubInfoSet(false);
				break;
			}
			else // last mechanism is now authentication - fail
				if (status == errAuthorizationDenied)
					reason = SecurityAgent::invalidPassphrase;
        }

	// If we fell out of the loop because of too many tries, notify user
	if (tries == mTries)
	{
		reason = SecurityAgent::tooManyTries;
		AuthItemRef retryHint (AGENT_HINT_RETRY_REASON, AuthValueOverlay(sizeof(reason), &reason));
		environmentToClient.erase(retryHint); environmentToClient.insert(retryHint); // replace
		AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(tries), &tries));
		environmentToClient.erase(triesHint); environmentToClient.insert(triesHint); // replace
            eval.run(AuthValueVector(), environmentToClient, auth);
		// XXX/cs is this still necessary?
		auth.scrubInfoSet(false);
		
        rightAuthLogger.logFailure(NULL, CommonCriteria::errTooManyTries);
	}

	return status;
}

// create externally verified credentials on the basis of 
// mechanism-provided information
CredentialSet
RuleImpl::makeCredentials(const AuthorizationToken &auth) const
{
	// fetch context and construct a credential to be tested
	const AuthItemSet &context = const_cast<AuthorizationToken &>(auth).infoSet();
	CredentialSet newCredentials;
	
	do {
		AuthItemSet::const_iterator found = find_if(context.begin(), context.end(), FindAuthItemByRightName(kAuthorizationEnvironmentUsername) );
		if (found == context.end())
			break;
		string username = (**found).stringValue();
		secdebug("AuthEvalMech", "found username");
		
		const uid_t *uid = NULL;
		found = find_if(context.begin(), context.end(), FindAuthItemByRightName("uid") );
		if (found != context.end())
		{
			uid = static_cast<const uid_t *>((**found).value().data);
			secdebug("AuthEvalMech", "found uid");
		}

		if (username.length() && uid)
		{
			// credential is valid because mechanism says so
			newCredentials.insert(Credential(*uid, username, "", mShared));
		}
	} while(0);

	return newCredentials;
}

// evaluate whether a good credential of the current session owner would authorize a right
OSStatus
RuleImpl::evaluateSessionOwner(const AuthItemRef &inRight, const Rule &inRule, const AuthItemSet &environment, const CFAbsoluteTime now, const AuthorizationToken &auth, Credential &credential, SecurityAgent::Reason &reason) const
{
	// username hint is taken from the user who created the authorization, unless it's clearly ineligible
	// @@@ we have no access to current requester uid here and the process uid is only taken when the authorization is created
	// meaning that a process like loginwindow that drops privs later is screwed.
	
	Credential sessionCredential;
	uid_t uid = auth.session().originatorUid();
	Server::active().longTermActivity();
	struct passwd *pw = getpwuid(uid);
	if (pw != NULL) {
		// avoid hinting a locked account
		if ( (pw->pw_passwd == NULL) ||
			strcmp(pw->pw_passwd, "*") ) {
			// Check if username will authorize the request and set username to
			// be used as a hint to the user if so
			secdebug("AuthEvalMech", "preflight credential from current user, result follows:");
			sessionCredential = Credential(pw->pw_uid, pw->pw_name, pw->pw_gecos, mShared/*ignored*/);
		} //fi
		endpwent();
	}
	OSStatus status = evaluateUserCredentialForRight(auth, inRight, inRule, environment, now, sessionCredential, true, reason);
	if (errAuthorizationSuccess == status)
		credential = sessionCredential;

	return status;
}


OSStatus
RuleImpl::evaluateCredentialForRight(const AuthorizationToken &auth, const AuthItemRef &inRight, const Rule &inRule, const AuthItemSet &environment, CFAbsoluteTime now, const Credential &credential, bool ignoreShared, SecurityAgent::Reason &reason) const
{
	if (auth.operatesAsLeastPrivileged()) {
        if (credential->isRight() && credential->isValid() && (inRight->name() == credential->name())) 
        {
            if (!ignoreShared && !mShared && credential->isShared())
            {
                // @@@  no proper SA::Reason
                reason = SecurityAgent::unknownReason;
                secdebug("autheval", "shared credential cannot be used, denying right %s", inRight->name());
                return errAuthorizationDenied;
            } else {
                return errAuthorizationSuccess;
            }
        } else {
            // @@@  no proper SA::Reason
            reason = SecurityAgent::unknownReason;
            return errAuthorizationDenied;
        }
	} else
		return evaluateUserCredentialForRight(auth, inRight, inRule, environment, now, credential, false, reason);
}

// Return errAuthorizationSuccess if this rule allows access based on the specified credential,
// return errAuthorizationDenied otherwise.
OSStatus
RuleImpl::evaluateUserCredentialForRight(const AuthorizationToken &auth, const AuthItemRef &inRight, const Rule &inRule, const AuthItemSet &environment, CFAbsoluteTime now, const Credential &credential, bool ignoreShared, SecurityAgent::Reason &reason) const
{
	assert(mType == kUser);

    // Ideally we'd set the AGENT_HINT_RETRY_REASON hint in this method, but
    // evaluateAuthentication() overwrites it before 
    // AgentMechanismEvaluator::run().  That's what led to passing "reason"
    // everywhere, from RuleImpl::evaluate() on down.  

	// Get the username from the credential
	const char *user = credential->name().c_str();

	// If the credential is not valid or its age is more than the allowed maximum age
	// for a credential, deny.
	if (!credential->isValid())
	{
        // @@@  it could be the username, not password, was invalid
        reason = SecurityAgent::invalidPassphrase;
		secdebug("autheval", "credential for user %s is invalid, denying right %s", user, inRight->name());
		return errAuthorizationDenied;
	}

	if (now - credential->creationTime() > mMaxCredentialAge)
	{
        // @@@  no proper SA::Reason
        reason = SecurityAgent::unknownReason;
		secdebug("autheval", "credential for user %s has expired, denying right %s", user, inRight->name());
		return errAuthorizationDenied;
	}

	if (!ignoreShared && !mShared && credential->isShared())
	{
        // @@@  no proper SA::Reason
        reason = SecurityAgent::unknownReason;
		secdebug("autheval", "shared credential for user %s cannot be used, denying right %s", user, inRight->name());
		return errAuthorizationDenied;
	}

	// A root (uid == 0) user can do anything
	if (credential->uid() == 0)
	{
		secdebug("autheval", "user %s has uid 0, granting right %s", user, inRight->name());
		return errAuthorizationSuccess;
	}

	if (mSessionOwner)
	{
		Session &session = auth.session();
		uid_t console_user = session.originatorUid();

		if (credential->uid() == console_user)
		{
			secdebug("autheval", "user %s is session-owner(uid: %d), granting right %s", user, console_user, inRight->name());
			return errAuthorizationSuccess;
		}
		// set "reason" in this case?  not that a proper SA::Reason exists
	}
	else
	{
		// @@@  no proper SA::Reason
		reason = SecurityAgent::unknownReason;
		secdebug("autheval", "session-owner check failed.");
	}
	
	if (mGroupName.length())
	{
		const char *groupname = mGroupName.c_str();
		Server::active().longTermActivity();

		if (!groupname)
			return errAuthorizationDenied;
			
		do 
		{
			uuid_t group_uuid, user_uuid;
			int is_member;

            // @@@  it'd be nice to have SA::Reason codes for the failures
            // associated with the pre-check-membership mbr_*() functions, 
            // but userNotInGroup will do
			if (mbr_group_name_to_uuid(groupname, group_uuid))
				break;
				
			if (mbr_uid_to_uuid(credential->uid(), user_uuid))
			{
				struct passwd *pwd;
				if (NULL == (pwd = getpwnam(user)))
					break;
				if (mbr_uid_to_uuid(pwd->pw_uid, user_uuid))
					break;				
			}

			if (mbr_check_membership(user_uuid, group_uuid, &is_member))
				break;
				
			if (is_member)
			{
				secdebug("autheval", "user %s is a member of group %s, granting right %s",
					user, groupname, inRight->name());
				return errAuthorizationSuccess;
			}
				
		}
		while (0);
        
        reason = SecurityAgent::userNotInGroup;
		secdebug("autheval", "user %s is not a member of group %s, denying right %s",
			user, groupname, inRight->name());
	}
    else if (mSessionOwner) // rule asks only if user is the session owner
    {
        reason = SecurityAgent::unacceptableUser;
    }
	
	return errAuthorizationDenied;
}



OSStatus
RuleImpl::evaluateUser(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationFlags flags, CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials, AuthorizationToken &auth, SecurityAgent::Reason &reason, bool savePassword) const
{
    // If we got here, this is a kUser type rule, let's start looking for a
	// credential that is satisfactory

	// Zeroth -- Here is an extra special saucy ugly hack to allow authorizations
	// created by a proccess running as root to automatically get a right.
	if (mAllowRoot && auth.creatorUid() == 0)
	{
        SECURITYD_AUTH_USER_ALLOWROOT(&auth);
        
		secdebug("autheval", "creator of authorization has uid == 0 granting right %s",
			inRight->name());
		return errAuthorizationSuccess;
	}
	
	// if we're not supposed to authenticate evaluate the session-owner against the group
	if (!mAuthenticateUser)
	{
		Credential hintCredential;
		OSStatus status = evaluateSessionOwner(inRight, inRule, environmentToClient, now, auth, hintCredential, reason);

		if (!status)
        {
            SECURITYD_AUTH_USER_ALLOWSESSIONOWNER(&auth);
			return errAuthorizationSuccess;
        }

		return errAuthorizationDenied;
	}

	// First -- go though the credentials we either already used or obtained during this authorize operation.
	for (CredentialSet::const_iterator it = credentials.begin(); it != credentials.end(); ++it)
	{
		// Passed-in user credentials are allowed for least-privileged mode
		if (auth.operatesAsLeastPrivileged() && !(*it)->isRight() && (*it)->isValid()) 
		{
			OSStatus status = evaluateUserCredentialForRight(auth, inRight, inRule, environmentToClient, now, *it, false, reason);
			if (errAuthorizationSuccess == status) {
				Credential rightCredential(inRight->name(), mShared);
				credentials.erase(rightCredential); credentials.insert(rightCredential);
				if (mShared)
					credentials.insert(Credential(inRight->name(), false));
				return status;
			}
		}

		// if this is least privileged, this will function differently: match credential to requested right
		OSStatus status = evaluateCredentialForRight(auth, inRight, inRule, environmentToClient, now, *it, false, reason);
			
		if (status != errAuthorizationDenied) {
			// add credential to authinfo
			auth.setCredentialInfo(*it, savePassword);
			return status;
		}

	}

	// Second -- go though the credentials passed in to this authorize operation by the state management layer.
	if (inCredentials)
	{
		for (CredentialSet::const_iterator it = inCredentials->begin(); it != inCredentials->end(); ++it)
		{
			// if this is least privileged, this will function differently: match credential to requested right
			OSStatus status = evaluateCredentialForRight(auth, inRight, inRule, environmentToClient, now, *it, false, reason);

			if (status == errAuthorizationSuccess)
			{
				// Add the credential we used to the output set.
				// whack an equivalent credential, so it gets updated to a later achieved credential which must have been more stringent
				credentials.erase(*it); credentials.insert(*it);
				// add credential to authinfo
				auth.setCredentialInfo(*it, savePassword);

				return status;
			}
			else if (status != errAuthorizationDenied)
				return status;
		}
	}

	// Finally -- We didn't find the credential in our passed in credential lists.  Obtain a new credential if our flags let us do so.
	if (!(flags & kAuthorizationFlagExtendRights))
		return errAuthorizationDenied;
	
	// authorizations that timeout immediately cannot be preauthorized
	if ((flags & kAuthorizationFlagPreAuthorize) && 
		(mMaxCredentialAge == 0.0))
	{
		inRight->setFlags(inRight->flags() | kAuthorizationFlagCanNotPreAuthorize);
		return errAuthorizationSuccess;
	}

	if (!(flags & kAuthorizationFlagInteractionAllowed))
		return errAuthorizationInteractionNotAllowed;

	setAgentHints(inRight, inRule, environmentToClient, auth);

	return evaluateAuthentication(inRight, inRule, environmentToClient, flags, now, inCredentials, credentials, auth, reason, savePassword);
}

OSStatus
RuleImpl::evaluateMechanismOnly(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationToken &auth, CredentialSet &outCredentials, bool savePassword) const
{
	uint32 tries = 0; 
	OSStatus status;

	Process &cltProc = Server::process();
	// Authorization preserves creator's UID in setuid processes
	uid_t cltUid = (cltProc.uid() != 0) ? cltProc.uid() : auth.creatorUid();
	secdebug("AuthEvalMech", "Mechanism invocation by process %d (UID %d)", cltProc.pid(), cltUid);

	{
		AgentMechanismEvaluator eval(cltUid, auth.session(), mEvalDef);
        // For auditing within AuthorizationMechEval, pass the right name.  
        size_t rightNameSize = inRight->name() ? strlen(inRight->name()) : 0;
        AuthorizationString rightName = inRight->name() ? inRight->name() : "";
        // @@@  AuthValueRef's ctor ought to take a const void *
        AuthValueRef rightValue(rightNameSize, const_cast<char *>(rightName));
        AuthValueVector authValueVector;
        authValueVector.push_back(rightValue);
        
		do
		{
			setAgentHints(inRight, inRule, environmentToClient, auth);
			AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(tries), &tries));
			environmentToClient.erase(triesHint); environmentToClient.insert(triesHint); // replace
            
            status = eval.run(authValueVector, environmentToClient, auth);
			if ((status == errAuthorizationSuccess) ||
				(status == errAuthorizationCanceled)) // @@@ can only pass back sideband through context
			{
				secdebug("AuthEvalMech", "storing new context for authorization");
				auth.setInfoSet(eval.context(), savePassword);
				if (status == errAuthorizationSuccess)
				{
                    // (try to) attach the authorizing UID to the least-priv cred
					if (auth.operatesAsLeastPrivileged())
                    {
                        outCredentials.insert(Credential(rightName, mShared));
                        if (mShared) 
                            outCredentials.insert(Credential(rightName, false));
                        
                        RightAuthenticationLogger logger(auth.creatorAuditToken(), AUE_ssauthint);
                        logger.setRight(rightName);

                        AuthItem *uidItem = eval.context().find(AGENT_CONTEXT_UID);
                        if (uidItem)
                        {
                            uid_t authorizedUid;
                            memcpy(&authorizedUid, uidItem->value().data, sizeof(authorizedUid));
                            secdebug("AuthEvalMech", "generating least-privilege cred for '%s' authorized by UID %u", inRight->name(), authorizedUid);
                            logger.logLeastPrivilege(authorizedUid, true);
                        }
                        else    // cltUid is better than nothing
                        {
                            secdebug("AuthEvalMech", "generating least-privilege cred for '%s' with process- or auth-UID %u", inRight->name(), cltUid);
                            logger.logLeastPrivilege(cltUid, false);
                        }
                    }

                    if (0 == strcmp(rightName, "system.login.console") && NULL == eval.context().find(AGENT_CONTEXT_AUTO_LOGIN)) {
                        secdebug("AuthEvalMech", "We logged in as the session owner.\n");
                        SessionAttributeBits flags = auth.session().attributes();
                        flags |= AU_SESSION_FLAG_HAS_AUTHENTICATED;
                        auth.session().setAttributes(flags);							
                    }
                    CredentialSet newCredentials = makeCredentials(auth);
                    outCredentials.insert(newCredentials.begin(), newCredentials.end());
				}
			}

			tries++;
		}
		while ((status == errAuthorizationDenied) // only if we have an expected failure we continue
					&& ((mTries == 0) 				// mTries == 0 means we try forever
					|| ((mTries > 0) 			// mTries > 0 means we try up to mTries times
					&& (tries < mTries))));
	}
	
	// HACK kill all hosts to free pages for low memory systems
    // (XXX/gh  there should be a #define for this right)
	if (name() == "system.login.done")
	{
        // one case where we don't want to mark the agents as "busy"
		QueryInvokeMechanism query(securityAgent, auth.session());
		query.terminateAgent();
		QueryInvokeMechanism query2(privilegedAuthHost, auth.session());
		query2.terminateAgent();
	}

	return status;
}

OSStatus
RuleImpl::evaluateRules(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationFlags flags, CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials, AuthorizationToken &auth, SecurityAgent::Reason &reason, bool savePassword) const
{
	// line up the rules to try
	if (!mRuleDef.size())
		return errAuthorizationSuccess;
		
	uint32_t count = 0;
	OSStatus status = errAuthorizationSuccess;
	vector<Rule>::const_iterator it;
	
	for (it = mRuleDef.begin();it != mRuleDef.end(); it++)
	{
		// are we at k yet?
		if ((mType == kKofN) && (count == mKofN))
			return errAuthorizationSuccess;

		// get a rule and try it
		status = (*it)->evaluate(inRight, inRule, environmentToClient, flags, now, inCredentials, credentials, auth, reason, savePassword);

		// if status is cancel/internal error abort
		if ((status == errAuthorizationCanceled) || (status == errAuthorizationInternal))
			return status;

		if (status != errAuthorizationSuccess)
		{
			// continue if we're only looking for k of n
			if (mType == kKofN)
				continue;
			
			break;
		}
		else
			count++;
	}

	if ((mType == kKofN) && (status == errAuthorizationSuccess) && (count < mKofN))
		status = errAuthorizationDenied;
	
	return status; // return the last failure
}


OSStatus
RuleImpl::evaluate(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationFlags flags, CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials, AuthorizationToken &auth, SecurityAgent::Reason &reason, bool savePassword) const
{
	switch (mType)
	{
	case kAllow:
        SECURITYD_AUTH_ALLOW(&auth, (char *)name().c_str());
		return errAuthorizationSuccess;
	case kDeny:
        SECURITYD_AUTH_DENY(&auth, (char *)name().c_str());
		return errAuthorizationDenied;
	case kUser:
        SECURITYD_AUTH_USER(&auth, (char *)name().c_str());
		return evaluateUser(inRight, inRule, environmentToClient, flags, now, inCredentials, credentials, auth, reason, savePassword);
	case kRuleDelegation:
        SECURITYD_AUTH_RULES(&auth, (char *)name().c_str());
		return evaluateRules(inRight, inRule, environmentToClient, flags, now, inCredentials, credentials, auth, reason, savePassword);
	case kKofN:
        SECURITYD_AUTH_KOFN(&auth, (char *)name().c_str());
		return evaluateRules(inRight, inRule, environmentToClient, flags, now, inCredentials, credentials, auth, reason, savePassword);
	case kEvaluateMechanisms:
        SECURITYD_AUTH_MECHRULE(&auth, (char *)name().c_str());
            // if we had a SecurityAgent::Reason code for "mechanism denied,"
            // it would make sense to pass down "reason"
		return evaluateMechanismOnly(inRight, inRule, environmentToClient, auth, credentials, savePassword);
	default:
		Syslog::alert("Unrecognized rule type %d", mType);
		MacOSError::throwMe(errAuthorizationInternal); // invalid rule
	}
}

Rule::Rule() : RefPointer<RuleImpl>(new RuleImpl()) {}
Rule::Rule(const string &inRightName, CFDictionaryRef cfRight, CFDictionaryRef cfRules) : RefPointer<RuleImpl>(new RuleImpl(inRightName, cfRight, cfRules)) {}



} // end namespace Authorization
