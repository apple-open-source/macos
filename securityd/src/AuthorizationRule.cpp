/*
 *  Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
#include <security_utilities/ccaudit.h>
#include <bsm/audit_uevents.h>
#include "authority.h"
#include "server.h"
#include "process.h"
#include "agentquery.h"
#include "AuthorizationMechEval.h"

#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <membership.h>

extern "C" {
#include <membershipPriv.h>
}

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
CFStringRef RuleImpl::kTriesID = CFSTR("tries"); // XXX/cs move to AuthorizationTagsPriv.h

CFStringRef RuleImpl::kRuleClassID = CFSTR(kAuthorizationRuleClass);
CFStringRef RuleImpl::kRuleAllowID = CFSTR(kAuthorizationRuleClassAllow);
CFStringRef RuleImpl::kRuleDenyID = CFSTR(kAuthorizationRuleClassDeny);
CFStringRef RuleImpl::kRuleUserID = CFSTR(kAuthorizationRuleClassUser);
CFStringRef RuleImpl::kRuleDelegateID = CFSTR(kAuthorizationRightRule);
CFStringRef RuleImpl::kRuleMechanismsID = CFSTR(kAuthorizationRuleClassMechanisms);
CFStringRef RuleImpl::kRuleAuthenticateUserID = CFSTR(kAuthorizationRuleParameterAuthenticateUser);


string
RuleImpl::Attribute::getString(CFDictionaryRef config, CFStringRef key, bool required = false, char *defaultValue = "")
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
				MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule
		}

		return string(ptr);
	}
	else
		if (!required)
			return string(defaultValue);
		else
			MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule
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
			MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule
			
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
			MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule
	
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

		for (int index=0; index < CFArrayGetCount(evalArray); index++)
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
						MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule
				}
				valueArray.push_back(string(ptr));
			}
		}
	}
	else
		if (required)
			MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule
			
	return valueArray;
}


bool RuleImpl::Attribute::getLocalizedPrompts(CFDictionaryRef config, map<string,string> &localizedPrompts)
{
	CFIndex numberOfPrompts = 0;
	CFDictionaryRef promptsDict;
	if (CFDictionaryContainsKey(config, kPromptID))
	{
		promptsDict = reinterpret_cast<CFDictionaryRef>(CFDictionaryGetValue(config, kPromptID));
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
		if (!keyRef || (CFGetTypeID(keyRef) != CFStringGetTypeID()))
			continue;
		if (!valueRef || (CFGetTypeID(valueRef) != CFStringGetTypeID()))
			continue;
		string key = cfString(keyRef);
		string value = cfString(valueRef);
		localizedPrompts[kAuthorizationRuleParameterDescription+key] = value;
	}

	return true;
}


// default rule
RuleImpl::RuleImpl() :
mType(kUser), mGroupName("admin"), mMaxCredentialAge(300.0), mShared(true), mAllowRoot(false), mSessionOwner(false), mTries(0), mAuthenticateUser(true)
{
	// XXX/cs read default descriptions from somewhere
	// @@@ Default rule is shared admin group with 5 minute timeout
}

// return rule built from rule definition; throw if invalid.
RuleImpl::RuleImpl(const string &inRightName, CFDictionaryRef cfRight, CFDictionaryRef cfRules) : mRightName(inRightName)
{
	// @@@ make sure cfRight is non mutable and never used that way
	
	if (CFGetTypeID(cfRight) != CFDictionaryGetTypeID())
		MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule
			
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
			mTries = int(Attribute::getDouble(cfRight, kTriesID, false, 3.0)); // XXX/cs double(kAuthorizationMaxTries)
			mAuthenticateUser = Attribute::getBool(cfRight, kRuleAuthenticateUserID, false, true);

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
					MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule
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
						MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule
					mRuleDef.push_back(Rule(*it, cfRuleDef, cfRules));
				}
			}

			mKofN = int(Attribute::getDouble(cfRight, kKofNID, false, 0.0));
			if (mKofN)
				mType = kKofN;

		}
		else
		{
			secdebug("authrule", "%s : rule class unknown %s.", inRightName.c_str(), classTag.c_str());
			MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule
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
			MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule
		mRuleDef.push_back(Rule(ruleName, cfRuleDef, cfRules));
	}

	Attribute::getLocalizedPrompts(cfRight, mLocalizedPrompts);
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

	if (defaultPrompts.empty())
		defaultPrompts = localizedPrompts();
		
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

	// add rulename as a hint
	string ruleName = name();
    environmentToClient.erase(AuthItemRef(AGENT_HINT_AUTHORIZE_RULE));
	environmentToClient.insert(AuthItemRef(AGENT_HINT_AUTHORIZE_RULE, AuthValueOverlay(ruleName)));
}

// If a different evaluation for getting a credential is prescribed,
// we'll run that and validate the credentials from there.
// we fall back on a default configuration from the authenticate rule
OSStatus
RuleImpl::evaluateAuthentication(const AuthItemRef &inRight, const Rule &inRule,AuthItemSet &environmentToClient, AuthorizationFlags flags, CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials, AuthorizationToken &auth) const
{
	OSStatus status = errAuthorizationDenied;

	Credential hintCredential;
	if (errAuthorizationSuccess == evaluateSessionOwner(inRight, inRule, environmentToClient, now, auth, hintCredential)) {
		if (hintCredential->name().length())
			environmentToClient.insert(AuthItemRef(AGENT_HINT_SUGGESTED_USER, AuthValueOverlay(hintCredential->name())));
		if (hintCredential->realname().length())
			environmentToClient.insert(AuthItemRef(AGENT_HINT_SUGGESTED_USER_LONG, AuthValueOverlay(hintCredential->realname())));
	}

	if ((mType == kUser) && (mGroupName.length()))
		environmentToClient.insert(AuthItemRef(AGENT_HINT_REQUIRE_USER_IN_GROUP, AuthValueOverlay(mGroupName)));

	uint32 tries;
	SecurityAgent::Reason reason = SecurityAgent::noReason;

	Process &cltProc = Server::process();
	// Authorization preserves creator's UID in setuid processes
	uid_t cltUid = (cltProc.uid() != 0) ? cltProc.uid() : auth.creatorUid();
	secdebug("AuthEvalMech", "Mechanism invocation by process %d (UID %d)", cltProc.pid(), cltUid);
 
	AgentMechanismEvaluator eval(cltUid, auth.session(), mEvalDef);

	for (tries = 0; tries < mTries; tries++)
	{
		AuthItemRef retryHint(AGENT_HINT_RETRY_REASON, AuthValueOverlay(sizeof(reason), &reason));
		environmentToClient.erase(retryHint); environmentToClient.insert(retryHint); // replace
		AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(tries), &tries));
		environmentToClient.erase(triesHint); environmentToClient.insert(triesHint); // replace

		status = eval.run(AuthValueVector(), environmentToClient, auth);

		if ((status == errAuthorizationSuccess) ||
			(status == errAuthorizationCanceled)) // @@@ can only pass back sideband through context
		{
			secdebug("AuthEvalMech", "storing new context for authorization");
			auth.setInfoSet(eval.context());
		}

		// successfully ran mechanisms to obtain credential
		if (status == errAuthorizationSuccess)
		{
			// deny is the default
			status = errAuthorizationDenied;
			
			CredentialSet newCredentials = makeCredentials(auth);
			// clear context after extracting credentials
			auth.scrubInfoSet(); 
			
			CommonCriteria::AuditRecord auditrec(auth.creatorAuditToken());
			for (CredentialSet::const_iterator it = newCredentials.begin(); it != newCredentials.end(); ++it)
			{
				const Credential& newCredential = *it;

				// @@@ we log the uid a process was running under when it created the authref, which is misleading in the case of loginwindow
				if (newCredential->isValid()) {
					Syslog::info("uid %lu succeeded authenticating as user %s (uid %lu) for right %s.", auth.creatorUid(), newCredential->name().c_str(), newCredential->uid(), inRight->name());
					auditrec.submit(AUE_ssauthint, CommonCriteria::errNone, inRight->name());
				} else {
					// we can't be sure that the user actually exists so inhibit logging of uid
					Syslog::error("uid %lu failed to authenticate as user %s for right %s.", auth.creatorUid(), newCredential->name().c_str(), inRight->name());
					auditrec.submit(AUE_ssauthint, CommonCriteria::errInvalidCredential, inRight->name());
				}
				
				if (!newCredential->isValid())
				{
					reason = SecurityAgent::invalidPassphrase; //invalidPassphrase;
					continue;
				}

				// verify that this credential authorizes right
				status = evaluateUserCredentialForRight(auth, inRight, inRule, environmentToClient, now, newCredential, true);
				
				if (status == errAuthorizationSuccess)
				{
					if (auth.operatesAsLeastPrivileged()) {
						Credential rightCredential(inRight->name(), mShared);
						credentials.erase(rightCredential); credentials.insert(rightCredential);
						if (mShared)
							credentials.insert(Credential(inRight->name(), false));
					} else {
						// whack an equivalent credential, so it gets updated to a later achieved credential which must have been more stringent
						credentials.erase(newCredential); credentials.insert(newCredential);
					   // just got a new credential - if it's shared also add a non-shared one that to stick in the authorizationref local cache
					   if (mShared)
							   credentials.insert(Credential(newCredential->uid(), newCredential->name(), newCredential->realname(), false));
					}
					
					// use valid credential to set context info
					// XXX/cs keeping this for now, such that the uid is passed back
					auth.setCredentialInfo(newCredential);
					secdebug("SSevalMech", "added valid credential for user %s", newCredential->name().c_str());
					status = errAuthorizationSuccess;
					break;
				}
				else
					reason = SecurityAgent::userNotInGroup; //unacceptableUser; // userNotInGroup
			}

			if (status == errAuthorizationSuccess)
				break;
		}
		else
			if ((status == errAuthorizationCanceled) ||
		(status == errAuthorizationInternal))
			{
				auth.scrubInfoSet();
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
		auth.scrubInfoSet();
		
		CommonCriteria::AuditRecord auditrec(auth.creatorAuditToken());
		auditrec.submit(AUE_ssauthorize, CommonCriteria::errTooManyTries, inRight->name());
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
RuleImpl::evaluateSessionOwner(const AuthItemRef &inRight, const Rule &inRule, const AuthItemSet &environment, const CFAbsoluteTime now, const AuthorizationToken &auth, Credential &credential) const
{
	// username hint is taken from the user who created the authorization, unless it's clearly ineligible
	// @@@ we have no access to current requester uid here and the process uid is only taken when the authorization is created
	// meaning that a process like loginwindow that drops privs later is screwed.
	
	uid_t uid;
	Session &session = auth.session();
	Credential sessionCredential;
	if (session.haveOriginatorUid()) {
		// preflight session credential as if it were a fresh copy
		const Credential &cred = session.originatorCredential();
		sessionCredential = Credential(cred->uid(), cred->name(), cred->realname(), mShared/*ignored*/);
	} else {
		uid = auth.creatorUid();
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
	}
	OSStatus status = evaluateUserCredentialForRight(auth, inRight, inRule, environment, now, sessionCredential, true);
	if (errAuthorizationSuccess == status)
		credential = sessionCredential;

	return status;
}


OSStatus
RuleImpl::evaluateCredentialForRight(const AuthorizationToken &auth, const AuthItemRef &inRight, const Rule &inRule, const AuthItemSet &environment, CFAbsoluteTime now, const Credential &credential, bool ignoreShared) const
{
	if (auth.operatesAsLeastPrivileged()) {
		if (credential->isRight() && credential->isValid() && (inRight->name() == credential->name()))
			return errAuthorizationSuccess;
		else
			return errAuthorizationDenied;
	} else
		return evaluateUserCredentialForRight(auth, inRight, inRule, environment, now, credential, false);
}

// Return errAuthorizationSuccess if this rule allows access based on the specified credential,
// return errAuthorizationDenied otherwise.
OSStatus
RuleImpl::evaluateUserCredentialForRight(const AuthorizationToken &auth, const AuthItemRef &inRight, const Rule &inRule, const AuthItemSet &environment, CFAbsoluteTime now, const Credential &credential, bool ignoreShared) const
{
	assert(mType == kUser);

	// Get the username from the credential
	const char *user = credential->name().c_str();

	// If the credential is not valid or it's age is more than the allowed maximum age
	// for a credential, deny.
	if (!credential->isValid())
	{
		secdebug("autheval", "credential for user %s is invalid, denying right %s", user, inRight->name());
		return errAuthorizationDenied;
	}

	if (now - credential->creationTime() > mMaxCredentialAge)
	{
		secdebug("autheval", "credential for user %s has expired, denying right %s", user, inRight->name());
		return errAuthorizationDenied;
	}

	if (!ignoreShared && !mShared && credential->isShared())
	{
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
		if (session.haveOriginatorUid())
		{
			uid_t console_user = session.originatorUid();

			if (credential->uid() == console_user)
			{
				secdebug("autheval", "user %s is session-owner(uid: %d), granting right %s", user, console_user, inRight->name());
				return errAuthorizationSuccess;
			}
		}
		else
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
			
			if (mbr_group_name_to_uuid(groupname, group_uuid))
				break;
				
			if (mbr_uid_to_uuid(credential->uid(), user_uuid))
				break;

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

		secdebug("autheval", "user %s is not a member of group %s, denying right %s",
			user, groupname, inRight->name());
	}
	
	return errAuthorizationDenied;
}



OSStatus
RuleImpl::evaluateUser(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationFlags flags, CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials, AuthorizationToken &auth) const
{
	// If we got here, this is a kUser type rule, let's start looking for a
	// credential that is satisfactory

	// Zeroth -- Here is an extra special saucy ugly hack to allow authorizations
	// created by a proccess running as root to automatically get a right.
	if (mAllowRoot && auth.creatorUid() == 0)
	{
		secdebug("autheval", "creator of authorization has uid == 0 granting right %s",
			inRight->name());
		return errAuthorizationSuccess;
	}
	
	// if we're not supposed to authenticate evaluate the session-owner against the group
	if (!mAuthenticateUser)
	{
		Credential hintCredential;
		OSStatus status = evaluateSessionOwner(inRight, inRule, environmentToClient, now, auth, hintCredential);

		if (!status)
			return errAuthorizationSuccess;

		return errAuthorizationDenied;
	}

	// First -- go though the credentials we either already used or obtained during this authorize operation.
	for (CredentialSet::const_iterator it = credentials.begin(); it != credentials.end(); ++it)
	{
		// Passed in user credentials are allowed for least privileged mode
		if (auth.operatesAsLeastPrivileged() && !(*it)->isRight() && (*it)->isValid()) 
		{
			OSStatus status = evaluateUserCredentialForRight(auth, inRight, inRule, environmentToClient, now, *it, false);
			if (errAuthorizationSuccess == status) {
				Credential rightCredential(inRight->name(), mShared);
				credentials.erase(rightCredential); credentials.insert(rightCredential);
				if (mShared)
					credentials.insert(Credential(inRight->name(), false));
				return status;
			}
		}

		// if this is least privileged, this will function differently: match credential to requested right
		OSStatus status = evaluateCredentialForRight(auth, inRight, inRule, environmentToClient, now, *it, false);
			
		if (status != errAuthorizationDenied) {
			// add credential to authinfo
			auth.setCredentialInfo(*it);
			return status;
		}

	}

	// Second -- go though the credentials passed in to this authorize operation by the state management layer.
	if (inCredentials)
	{
		for (CredentialSet::const_iterator it = inCredentials->begin(); it != inCredentials->end(); ++it)
		{
			// if this is least privileged, this will function differently: match credential to requested right
			OSStatus status = evaluateCredentialForRight(auth, inRight, inRule, environmentToClient, now, *it, false);

			if (status == errAuthorizationSuccess)
			{
				// Add the credential we used to the output set.
				// whack an equivalent credential, so it gets updated to a later achieved credential which must have been more stringent
				credentials.erase(*it); credentials.insert(*it);
				// add credential to authinfo
				auth.setCredentialInfo(*it);

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

	return evaluateAuthentication(inRight, inRule, environmentToClient, flags, now, inCredentials, credentials, auth);
}

OSStatus
RuleImpl::evaluateMechanismOnly(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationToken &auth, CredentialSet &outCredentials) const
{
	uint32 tries = 0; 
	OSStatus status;

	Process &cltProc = Server::process();
	// Authorization preserves creator's UID in setuid processes
	uid_t cltUid = (cltProc.uid() != 0) ? cltProc.uid() : auth.creatorUid();
	secdebug("AuthEvalMech", "Mechanism invocation by process %d (UID %d)", cltProc.pid(), cltUid);

	{
		AgentMechanismEvaluator eval(cltUid, auth.session(), mEvalDef);

		do
		{
			setAgentHints(inRight, inRule, environmentToClient, auth);
			AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(tries), &tries));
			environmentToClient.erase(triesHint); environmentToClient.insert(triesHint); // replace
			    
			status = eval.run(AuthValueVector(), environmentToClient, auth);
			
			if ((status == errAuthorizationSuccess) ||
				(status == errAuthorizationCanceled)) // @@@ can only pass back sideband through context
			{
				secdebug("AuthEvalMech", "storing new context for authorization");
				auth.setInfoSet(eval.context());
				if (status == errAuthorizationSuccess)
				{
					if (auth.operatesAsLeastPrivileged())
						outCredentials.insert(Credential(inRight->name(), mShared));
					else
						outCredentials = makeCredentials(auth);
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
	if (name() == "system.login.done")
	{
		QueryInvokeMechanism query(securityAgent, auth.session());
		query.terminateAgent();
		QueryInvokeMechanism query2(privilegedAuthHost, auth.session());
		query2.terminateAgent();
	}

	return status;
}

OSStatus
RuleImpl::evaluateRules(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationFlags flags, CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials, AuthorizationToken &auth) const
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
		status = (*it)->evaluate(inRight, inRule, environmentToClient, flags, now, inCredentials, credentials, auth);

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
	
	return status; // return the last failure
}


OSStatus
RuleImpl::evaluate(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationFlags flags, CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials, AuthorizationToken &auth) const
{
	switch (mType)
	{
	case kAllow:
		secdebug("autheval", "rule is always allow");
		return errAuthorizationSuccess;
	case kDeny:
		secdebug("autheval", "rule is always deny");
		return errAuthorizationDenied;
	case kUser:
		secdebug("autheval", "rule is user");
		return evaluateUser(inRight, inRule, environmentToClient, flags, now, inCredentials, credentials, auth);
	case kRuleDelegation:
		secdebug("autheval", "rule evaluates rules");
		return evaluateRules(inRight, inRule, environmentToClient, flags, now, inCredentials, credentials, auth);
	case kKofN:
		secdebug("autheval", "rule evaluates k-of-n rules");
		return evaluateRules(inRight, inRule, environmentToClient, flags, now, inCredentials, credentials, auth);
	case kEvaluateMechanisms:
		secdebug("autheval", "rule evaluates mechanisms");
		return evaluateMechanismOnly(inRight, inRule, environmentToClient, auth, credentials);
	default:
		MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule
	}
}

Rule::Rule() : RefPointer<RuleImpl>(new RuleImpl()) {}
Rule::Rule(const string &inRightName, CFDictionaryRef cfRight, CFDictionaryRef cfRules) : RefPointer<RuleImpl>(new RuleImpl(inRightName, cfRight, cfRules)) {}



} // end namespace Authorization
