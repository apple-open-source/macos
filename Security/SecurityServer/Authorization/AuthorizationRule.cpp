/*
 *  AuthorizationRule.cpp
 *  Security
 *
 *  Created by Conrad Sauerwald on Wed Mar 19 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "AuthorizationRule.h"
#include "AuthorizationTags.h"
#include "AuthorizationDB.h"
#include "AuthorizationPriv.h"
#include "authority.h"
#include "server.h"
#include "process.h"


#include <pwd.h>
#include <grp.h>
#include <unistd.h>

#include <bsm/libbsm.h>
#include <bsm/audit_uevents.h>
#include "ccaudit.h"

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

CFStringRef RuleImpl::kRuleClassID = CFSTR(kAuthorizationRuleClass);
CFStringRef RuleImpl::kRuleAllowID = CFSTR(kAuthorizationRuleClassAllow);
CFStringRef RuleImpl::kRuleDenyID = CFSTR(kAuthorizationRuleClassDeny);
CFStringRef RuleImpl::kRuleUserID = CFSTR(kAuthorizationRuleClassUser);
CFStringRef RuleImpl::kRuleDelegateID = CFSTR(kAuthorizationRightRule);
CFStringRef RuleImpl::kRuleMechanismsID = CFSTR(kAuthorizationRuleClassMechanisms);


string
RuleImpl::Attribute::getString(CFDictionaryRef config, CFStringRef key, bool required = false, char *defaultValue = NULL)
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

// add reference to string that we're modifying
void
RuleImpl::Attribute::setString(CFMutableDictionaryRef config, CFStringRef key, string &value)
{
	CFStringRef cfstringValue = CFStringCreateWithCString(NULL /*allocator*/, value.c_str(), kCFStringEncodingUTF8);
	
	if (cfstringValue)
	{
		CFDictionarySetValue(config, key, cfstringValue);
		CFRelease(cfstringValue);
	}
	else
		MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid attribute
}			

void
RuleImpl::Attribute::setDouble(CFMutableDictionaryRef config, CFStringRef key, double value)
{
	CFNumberRef doubleValue = CFNumberCreate(NULL /*allocator*/, kCFNumberDoubleType, doubleValue);
	
	if (doubleValue)
	{
		CFDictionarySetValue(config, key, doubleValue);
		CFRelease(doubleValue);
	}
	else
		MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid attribute
}

void
RuleImpl::Attribute::setBool(CFMutableDictionaryRef config, CFStringRef key, bool value)
{
	if (value)
		CFDictionarySetValue(config, key, kCFBooleanTrue);
	else
		CFDictionarySetValue(config, key, kCFBooleanFalse);
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
		localizedPrompts["description"+key] = value;
	}

	return true;
}


// default rule
RuleImpl::RuleImpl() :
mType(kUser), mGroupName("admin"), mMaxCredentialAge(300.0), mShared(true), mAllowRoot(false), mSessionOwner(false), mTries(0)
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
			mTries = 3;
			
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
		}
		else if (classTag == kAuthorizationRightRule)
		{
			assert(cfRules); // this had better not be a rule
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
				mRuleDef.push_back(Rule(ruleDefString, cfRuleDef, NULL));
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
					mRuleDef.push_back(Rule(*it, cfRuleDef, NULL));
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
		assert(cfRules);
		mType = kRuleDelegation;
		string ruleName = Attribute::getString(cfRight, kRuleDelegateID, true);
		secdebug("authrule", "%s : rule delegate rule (1): %s", inRightName.c_str(), ruleName.c_str());
		CFStringRef ruleNameRef = makeCFString(ruleName);
		CFDictionaryRef cfRuleDef = reinterpret_cast<CFDictionaryRef>(CFDictionaryGetValue(cfRules, ruleNameRef));
		if (ruleNameRef)
			CFRelease(ruleNameRef);
		if (!cfRuleDef || CFGetTypeID(cfRuleDef) != CFDictionaryGetTypeID())
			MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule
		mRuleDef.push_back(Rule(ruleName, cfRuleDef, NULL));
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
	environmentToClient.insert(AuthItemRef(AGENT_HINT_AUTHORIZE_RIGHT, AuthValueOverlay(authorizeString)));
	
	// XXX/cs pid/uid/client should only be added when we're ready to call the agent
	pid_t cPid = Server::connection().process.pid();
	environmentToClient.insert(AuthItemRef(AGENT_HINT_CLIENT_PID, AuthValueOverlay(sizeof(pid_t), &cPid)));
	
	uid_t cUid = auth.creatorUid();
	environmentToClient.insert(AuthItemRef(AGENT_HINT_CLIENT_UID, AuthValueOverlay(sizeof(uid_t), &cUid)));

	pid_t creatorPid = auth.creatorPid();
	environmentToClient.insert(AuthItemRef(AGENT_HINT_CREATOR_PID, AuthValueOverlay(sizeof(pid_t), &creatorPid)));
	
	{
		CodeSigning::OSXCode *osxcode = auth.creatorCode();
		if (!osxcode)
			MacOSError::throwMe(errAuthorizationDenied);
			
		string encodedBundle = osxcode->encode();
		char bundleType = (encodedBundle.c_str())[0]; // yay, no accessor
		string bundlePath = osxcode->canonicalPath();
	
		environmentToClient.insert(AuthItemRef(AGENT_HINT_CLIENT_TYPE, AuthValueOverlay(sizeof(bundleType), &bundleType)));
		environmentToClient.insert(AuthItemRef(AGENT_HINT_CLIENT_PATH, AuthValueOverlay(bundlePath)));
	}
	
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
	environmentToClient.insert(AuthItemRef(AGENT_HINT_AUTHORIZE_RULE, AuthValueOverlay(ruleName)));
}

string
RuleImpl::agentNameForAuth(const AuthorizationToken &auth) const
{
	uint8_t hash[20];
	AuthorizationBlob authBlob = auth.handle();
	CssmData hashedData = CssmData::wrap(&hash, sizeof(hash));
	CssmData data = CssmData::wrap(&authBlob, sizeof(authBlob));
	CssmClient::Digest ctx(Server::csp(), CSSM_ALGID_SHA1);
	try {
		ctx.digest(data, hashedData);
	}
	catch (CssmError &e)
	{
		secdebug("auth", "digesting authref failed (%lu)", e.cssmError());
		return string("SecurityAgentMechanism");
	}
	
	uint8_t *point = static_cast<uint8_t*>(hashedData.data());
	for (uint8_t i=0; i < hashedData.length(); point++, i++)
	{
		uint8 value = (*point % 62) + '0';
		if (value > '9') value += 7; 
		if (value > 'Z') value += 6;
		*point = value;
	}
	return string(static_cast<char *>(hashedData.data()), hashedData.length());
}

OSStatus
RuleImpl::evaluateMechanism(const AuthItemRef &inRight, const AuthItemSet &environment, AuthorizationToken &auth, CredentialSet &outCredentials) const
{
    string agentName = agentNameForAuth(auth);

    // @@@ configuration does not support arguments
    AuthValueVector arguments;
	// XXX/cs Move this up - we shouldn't know how to retrieve the ingoing context
	AuthItemSet context = auth.infoSet();
	AuthItemSet hints = environment;
    
    CommonCriteria::AuditRecord auditrec(auth.creatorAuditToken());

    AuthorizationResult result = kAuthorizationResultAllow;
    vector<string>::const_iterator currentMechanism = mEvalDef.begin();
    
    while ( (result == kAuthorizationResultAllow)  &&
            (currentMechanism != mEvalDef.end()) ) // iterate mechanisms
    {
        string::size_type extPlugin = currentMechanism->find(':');
        if (extPlugin != string::npos)
        {
            // no whitespace removal
            string pluginIn(currentMechanism->substr(0, extPlugin));
            string mechanismIn(currentMechanism->substr(extPlugin + 1));
            secdebug("SSevalMech", "external mech %s:%s", pluginIn.c_str(), mechanismIn.c_str());

            bool mechExecOk = false; // successfully ran a mechanism
                
			Process &cltProc = Server::active().connection().process;
			// Authorization preserves creator's UID in setuid processes
			uid_t cltUid = (cltProc.uid() != 0) ? cltProc.uid() : auth.creatorUid();
			secdebug("SSevalMech", "Mechanism invocation by process %d (UID %d)", cltProc.pid(), cltUid);
			QueryInvokeMechanism client(cltUid, auth, agentName.c_str());
            try
            {
                mechExecOk = client(pluginIn, mechanismIn, arguments, hints, context, &result);
            }
            catch (...) {
                secdebug("SSevalMech", "exception from mech eval or client death");
                // various server problems, but only if it really failed
                if (mechExecOk != true)
                    result = kAuthorizationResultUndefined;
            }
                
            secdebug("SSevalMech", "evaluate(plugin: %s, mechanism: %s) %s, result: %lu.", pluginIn.c_str(), mechanismIn.c_str(), (mechExecOk == true) ? "succeeded" : "failed", result);
		}
        else
        {
            // internal mechanisms - no glue
            if (*currentMechanism == "authinternal")
            {
                secdebug("SSevalMech", "evaluate authinternal");
                result = kAuthorizationResultDeny;
                do {
                    AuthItemSet::iterator found = find_if(context.begin(), context.end(), FindAuthItemByRightName(kAuthorizationEnvironmentUsername) );
                    if (found == context.end())
                        break;
                    string username(static_cast<const char *>((*found)->value().data), (*found)->value().length);
                    secdebug("SSevalMech", "found username");
                    found = find_if(context.begin(), context.end(), FindAuthItemByRightName(kAuthorizationEnvironmentPassword) );
                    if (found == context.end())
                        break;
                    string password(static_cast<const char *>((*found)->value().data), (*found)->value().length);
                    secdebug("SSevalMech", "found password");
                    Credential newCredential(username, password, true); // create a new shared credential
					
					if (newCredential->isValid())
					{
						Syslog::info("authinternal authenticated user %s (uid %lu) for right %s.", newCredential->username().c_str(), newCredential->uid(), inRight->name());
						auditrec.submit(AUE_ssauthint, CommonCriteria::errNone, inRight->name());
					}
					else
					{
						// we can't be sure that the user actually exists so inhibit logging of uid
						Syslog::error("authinternal failed to authenticate user %s for right %s.", newCredential->username().c_str(), inRight->name());

						auditrec.submit(AUE_ssauthint, CommonCriteria::errInvalidCredential, inRight->name());
					}

                    if (newCredential->isValid())
                    {
                        outCredentials.clear(); // only keep last one
                        secdebug("SSevalMech", "inserting new credential");
                        outCredentials.insert(newCredential);
                        result = kAuthorizationResultAllow;
                    } else
                        result = kAuthorizationResultDeny;
                } while (0);
            }
            else
            if (*currentMechanism == "push_hints_to_context")
            {
                secdebug("SSevalMech", "evaluate push_hints_to_context");
				mTries = 1; // XXX/cs this should be set in authorization config
                result = kAuthorizationResultAllow; // snarfcredential doesn't block evaluation, ever, it may restart
                // create out context from input hints, no merge
                // @@@ global copy template not being invoked...
				context = hints;
            }
            else
            if (*currentMechanism == "switch_to_user")
            {
				Process &cltProc = Server::active().connection().process;
				// Authorization preserves creator's UID in setuid processes
				uid_t cltUid = (cltProc.uid() != 0) ? cltProc.uid() : auth.creatorUid();
				secdebug("SSevalMech", "terminating agent at request of process %d (UID %d)\n", cltProc.pid(), cltUid);
				QueryInvokeMechanism client(cltUid, auth, agentName.c_str());

                try {
                    client.terminateAgent();
                } catch (...) {
                    // Not our agent
                }
                result = kAuthorizationResultAllow;
            }
        }
        
        // we own outHints and outContext
        switch(result)
        {
            case kAuthorizationResultAllow:
                secdebug("SSevalMech", "result allow");
                currentMechanism++;
                break;
            case kAuthorizationResultDeny:
                secdebug("SSevalMech", "result deny");
                break;
            case kAuthorizationResultUndefined:
                secdebug("SSevalMech", "result undefined");
                break; // abort evaluation
            case kAuthorizationResultUserCanceled:
                secdebug("SSevalMech", "result canceled");
                break; // stop evaluation, return some sideband
            default:
                break; // abort evaluation
        }
    }

    // End of evaluation, if last step produced meaningful data, incorporate
    if ((result == kAuthorizationResultAllow) ||
        (result == kAuthorizationResultUserCanceled)) // @@@ can only pass back sideband through context
    {
        secdebug("SSevalMech", "storing new context for authorization");
        auth.setInfoSet(context);
    }

    switch(result)
    {
        case kAuthorizationResultDeny:
            return errAuthorizationDenied;
        case kAuthorizationResultUserCanceled:
            return errAuthorizationCanceled;
		case kAuthorizationResultAllow:
            return errAuthorizationSuccess;
        default:
            return errAuthorizationInternal;
    }
}



OSStatus
RuleImpl::evaluateAuthorization(const AuthItemRef &inRight, const Rule &inRule,
		AuthItemSet &environmentToClient, 
        AuthorizationFlags flags, CFAbsoluteTime now, 
        const CredentialSet *inCredentials, 
        CredentialSet &credentials, AuthorizationToken &auth) const
{
    OSStatus status = errAuthorizationDenied;

    string usernamehint;
	evaluateSessionOwner(inRight, inRule, environmentToClient, now, auth, usernamehint);
    if (usernamehint.length())
		environmentToClient.insert(AuthItemRef(AGENT_HINT_SUGGESTED_USER, AuthValueOverlay(usernamehint)));

    if ((mType == kUser) && (mGroupName.length()))
        environmentToClient.insert(AuthItemRef(AGENT_HINT_REQUIRE_USER_IN_GROUP, AuthValueOverlay(mGroupName)));

    uint32 tries;
    SecurityAgent::Reason reason = SecurityAgent::noReason;

    for (tries = 0; tries < mTries; tries++)
    {
		AuthItemRef retryHint(AGENT_HINT_RETRY_REASON, AuthValueOverlay(sizeof(reason), &reason));
		environmentToClient.erase(retryHint); environmentToClient.insert(retryHint); // replace
		AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(tries), &tries));
		environmentToClient.erase(triesHint); environmentToClient.insert(triesHint); // replace

        status = evaluateMechanism(inRight, environmentToClient, auth, credentials);

        // successfully ran mechanisms to obtain credential
        if (status == errAuthorizationSuccess)
        {
            // deny is the default
            status = errAuthorizationDenied;
            
            // fetch context and construct a credential to be tested
			AuthItemSet inContext = auth.infoSet();
            CredentialSet newCredentials = makeCredentials(inContext);
			// clear context after extracting credentials
			auth.clearInfoSet(); 
            
            for (CredentialSet::const_iterator it = newCredentials.begin(); it != newCredentials.end(); ++it)
            {
                const Credential& newCredential = *it;
				CommonCriteria::AuditRecord auditrec(auth.creatorAuditToken());

				// @@@ we log the uid a process was running under when it created the authref, which is misleading in the case of loginwindow
				if (newCredential->isValid())
				{
					Syslog::info("uid %lu succeeded authenticating as user %s (uid %lu) for right %s.", auth.creatorUid(), newCredential->username().c_str(), newCredential->uid(), inRight->name());
					auditrec.submit(AUE_ssauthorize, CommonCriteria::errNone, inRight->name());
				}
				else
				{
					// we can't be sure that the user actually exists so inhibit logging of uid
					Syslog::error("uid %lu failed to authenticate as user %s for right %s.", auth.creatorUid(), newCredential->username().c_str(), inRight->name());
					auditrec.submit(AUE_ssauthorize, CommonCriteria::errInvalidCredential, inRight->name());
				}
                
                if (!newCredential->isValid())
                {
                    reason = SecurityAgent::invalidPassphrase; //invalidPassphrase;
                    continue;
                }
            
                // verify that this credential authorizes right
                status = evaluateCredentialForRight(inRight, inRule, environmentToClient, now, newCredential, true);

                if (status == errAuthorizationSuccess)
                {
					// whack an equivalent credential, so it gets updated to a later achieved credential which must have been more stringent
                    credentials.erase(newCredential); credentials.insert(newCredential);
 					// use valid credential to set context info
					auth.setCredentialInfo(newCredential);
                    secdebug("SSevalMech", "added valid credential for user %s", newCredential->username().c_str());
                    status = errAuthorizationSuccess;
                    break;
                }
                else
				{
                    reason = SecurityAgent::userNotInGroup; //unacceptableUser; // userNotInGroup
					// don't audit: we denied on the basis of something
					// other than a bad user or password
				}
            }
            
            if (status == errAuthorizationSuccess)
                break;
        }
        else
            if ((status == errAuthorizationCanceled) ||
		(status == errAuthorizationInternal))
			{
				auth.clearInfoSet();
				break;
			}
    }

    // If we fell out of the loop because of too many tries, notify user
    if (tries == mTries)
    {
        reason = SecurityAgent::tooManyTries;
		AuthItemRef retryHint (AGENT_HINT_RETRY_REASON, AuthValueOverlay(sizeof(reason), &reason));
		environmentToClient.erase(retryHint); environmentToClient.insert(retryHint); // replace
		AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(tries), &tries));
		environmentToClient.erase(triesHint); environmentToClient.insert(triesHint); // replace
        evaluateMechanism(inRight, environmentToClient, auth, credentials);
		auth.clearInfoSet();
    }

	Process &cltProc = Server::active().connection().process;
	// Authorization preserves creator's UID in setuid processes
	uid_t cltUid = (cltProc.uid() != 0) ? cltProc.uid() : auth.creatorUid();
	secdebug("SSevalMech", "terminating agent at request of process %d (UID %d)\n", cltProc.pid(), cltUid);
	string agentName = agentNameForAuth(auth);
	QueryInvokeMechanism client(cltUid, auth, agentName.c_str());
	
	try {
		client.terminateAgent();
	} catch (...) {
		// Not our agent
	}
	
    return status;
}

// create externally verified credentials on the basis of 
// mechanism-provided information
CredentialSet
RuleImpl::makeCredentials(const AuthItemSet &context) const
{
    CredentialSet newCredentials;

    do {    
        AuthItemSet::const_iterator found = find_if(context.begin(), context.end(), FindAuthItemByRightName(kAuthorizationEnvironmentUsername) );
        if (found == context.end())
            break;
        string username = (**found).stringValue();
        secdebug("SSevalMech", "found username");

        const uid_t *uid = NULL;
        found = find_if(context.begin(), context.end(), FindAuthItemByRightName("uid") );
        if (found != context.end())
        {
            uid = static_cast<const uid_t *>((**found).value().data);
            secdebug("SSevalMech", "found uid");
        }

        const gid_t *gid = NULL;
        found = find_if(context.begin(), context.end(), FindAuthItemByRightName("gid") );
        if (found != context.end())
        {
            gid = static_cast<const gid_t *>((**found).value().data);
            secdebug("SSevalMech", "found gid");
        }

        if (username.length() && uid && gid)
        {
            // credential is valid because mechanism says so
            newCredentials.insert(Credential(username, *uid, *gid, mShared));
        }
        else
        {
            found = find_if(context.begin(), context.end(), FindAuthItemByRightName(kAuthorizationEnvironmentPassword) );
            if (found != context.end())
            {
                secdebug("SSevalMech", "found password");
                string password = (**found).stringValue();
                secdebug("SSevalMech", "falling back on username/password credential if valid");
                newCredentials.insert(Credential(username, password, mShared));
            }
        }
    } while(0);

    return newCredentials;
}

// evaluate whether a good credential of the current session owner would authorize a right
OSStatus
RuleImpl::evaluateSessionOwner(const AuthItemRef &inRight, const Rule &inRule,
                           const AuthItemSet &environment,
                           const CFAbsoluteTime now,
                           const AuthorizationToken &auth,
                           string& usernamehint) const
{
    // username hint is taken from the user who created the authorization, unless it's clearly ineligible
	OSStatus status = noErr;
	// @@@ we have no access to current requester uid here and the process uid is only taken when the authorization is created
	// meaning that a process like loginwindow that drops privs later is screwed.
	uid_t uid = auth.creatorUid();
	
	Server::active().longTermActivity();
	struct passwd *pw = getpwuid(uid);
	if (pw != NULL)
	{
		// avoid hinting a locked account (ie. root)
		if ( (pw->pw_passwd == NULL) ||
				strcmp(pw->pw_passwd, "*") ) {
			// Check if username will authorize the request and set username to
			// be used as a hint to the user if so
			status = evaluateCredentialForRight(inRight, inRule, environment, now, Credential(pw->pw_name, pw->pw_uid, pw->pw_gid, mShared), true);
			
			if (status == errAuthorizationSuccess) 
				usernamehint = pw->pw_name;
		} //fi
		endpwent();
	}
	return status;
}



// Return errAuthorizationSuccess if this rule allows access based on the specified credential,
// return errAuthorizationDenied otherwise.
OSStatus
RuleImpl::evaluateCredentialForRight(const AuthItemRef &inRight, const Rule &inRule, const AuthItemSet &environment, CFAbsoluteTime now, const Credential &credential, bool ignoreShared) const
{
	assert(mType == kUser);

	// Get the username from the credential
	const char *user = credential->username().c_str();

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

    // XXX/cs replace with remembered session-owner once that functionality is added to SecurityServer
    if (mSessionOwner)
    {
        uid_t console_user;
        struct stat console_stat;
        if (!lstat("/dev/console", &console_stat))
        {
            console_user = console_stat.st_uid;
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
		struct group *gr = getgrnam(groupname);
		if (!gr)
			return errAuthorizationDenied;
	
		// Is this the default group of this user?
		// PR-2875126 <grp.h> declares gr_gid int, as opposed to advertised (getgrent(3)) gid_t
		// When this is fixed this warning should go away.
		if (credential->gid() == gr->gr_gid)
		{
			secdebug("autheval", "user %s has group %s(%d) as default group, granting right %s",
				user, groupname, gr->gr_gid, inRight->name());
			endgrent();
			return errAuthorizationSuccess;
		}
	
		for (char **group = gr->gr_mem; *group; ++group)
		{
			if (!strcmp(*group, user))
			{
				secdebug("autheval", "user %s is a member of group %s, granting right %s",
					user, groupname, inRight->name());
				endgrent();
				return errAuthorizationSuccess;
			}
		}
	
		secdebug("autheval", "user %s is not a member of group %s, denying right %s",
			user, groupname, inRight->name());
		endgrent();
	}
	
	return errAuthorizationDenied;
}

OSStatus
RuleImpl::evaluateUser(const AuthItemRef &inRight, const Rule &inRule,
    AuthItemSet &environmentToClient, AuthorizationFlags flags,
	CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials,
	AuthorizationToken &auth) const
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
	
	// if this is a "is-admin" rule check that and return
	// XXX/cs add way to specify is-admin class of rule: if (mNoVerify)
	if (name() == kAuthorizationRuleIsAdmin)
	{
		string username;
		if (!evaluateSessionOwner(inRight, inRule, environmentToClient, now, auth, username))
			return errAuthorizationSuccess;
	}

	// First -- go though the credentials we either already used or obtained during this authorize operation.
	for (CredentialSet::const_iterator it = credentials.begin(); it != credentials.end(); ++it)
	{
		OSStatus status = evaluateCredentialForRight(inRight, inRule, environmentToClient, now, *it, true);
		if (status != errAuthorizationDenied)
		{
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
			OSStatus status = evaluateCredentialForRight(inRight, inRule, environmentToClient, now, *it, false);
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

	// Finally -- We didn't find the credential in our passed in credential lists.  Obtain a new credential if
	// our flags let us do so.
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

    // If a different evaluation is prescribed,
    // we'll run that and validate the credentials from there
    // we fall back on a default configuration
	if (mEvalDef.size() == 0)
		return evaluateAuthorizationOld(inRight, inRule, environmentToClient, flags, now, inCredentials, credentials, auth);
	else
		return evaluateAuthorization(inRight, inRule, environmentToClient, flags, now, inCredentials, credentials, auth);
}

// XXX/cs insert a mechanism that let's the agent live (keep-alive) only in loginwindow's case
OSStatus
RuleImpl::evaluateMechanismOnly(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationToken &auth, CredentialSet &outCredentials) const
{
	uint32 tries = 0; 
	OSStatus status;

	do
	{
		setAgentHints(inRight, inRule, environmentToClient, auth);
		AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(tries), &tries));
		environmentToClient.erase(triesHint); environmentToClient.insert(triesHint); // replace

		status = evaluateMechanism(inRight, environmentToClient, auth, outCredentials);
		tries++;
	}
	while ((status == errAuthorizationDenied) // only if we have an expected failure we continue
				&& ((mTries == 0) 				// mTries == 0 means we try forever
					|| ((mTries > 0) 			// mTries > 0 means we try up to mTries times
						&& (tries < mTries))));

	if (name() != "system.login.console")
	{
		// terminate agent
		string agentName = agentNameForAuth(auth);
		Process &cltProc = Server::active().connection().process;
		// Authorization preserves creator's UID in setuid processes
		uid_t cltUid = (cltProc.uid() != 0) ? cltProc.uid() : auth.creatorUid();
		secdebug("SSevalMech", "terminating agent at request of process %d (UID %d)\n", cltProc.pid(), cltUid);

		QueryInvokeMechanism client(cltUid, auth, agentName.c_str());

		try 
		{
			client.terminateAgent();
		} catch (...) {
			// Not our agent
		}
	}
	return status;
}

OSStatus
RuleImpl::evaluateRules(const AuthItemRef &inRight, const Rule &inRule,
    AuthItemSet &environmentToClient, AuthorizationFlags flags,
	CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials,
	AuthorizationToken &auth) const
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
RuleImpl::evaluate(const AuthItemRef &inRight, const Rule &inRule,
    AuthItemSet &environmentToClient, AuthorizationFlags flags,
	CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials,
	AuthorizationToken &auth) const
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




// This is slated to be removed when the new auth panel is fixed up
OSStatus
RuleImpl::evaluateAuthorizationOld(const AuthItemRef &inRight, const Rule &inRule,
		AuthItemSet &environmentToClient, 
        AuthorizationFlags flags, CFAbsoluteTime now, 
        const CredentialSet *inCredentials, 
        CredentialSet &credentials, AuthorizationToken &auth) const
{
	Process &cltProc = Server::active().connection().process;
	// Authorization preserves creator's UID in setuid processes
	uid_t cltUid = (cltProc.uid() != 0) ? cltProc.uid() : auth.creatorUid();
	secdebug("autheval", "Auth query from process %d (UID %d)", cltProc.pid(), cltUid);
	QueryAuthorizeByGroup query(cltUid, auth);

	string usernamehint;

	evaluateSessionOwner(inRight, inRule, environmentToClient, now, auth, usernamehint);

	Credential newCredential;
	// @@@ Keep the default reason the same, so the agent only gets userNotInGroup or invalidPassphrase
	SecurityAgent::Reason reason = SecurityAgent::userNotInGroup;

	CommonCriteria::AuditRecord auditrec(auth.creatorAuditToken());

	// @@@ Hardcoded 3 tries to avoid infinite loops.
	for (uint32_t tryCount = 0; tryCount < mTries; ++tryCount)
	{
		// Obtain a new credential.  Anything but success is considered an error.
		OSStatus status = obtainCredential(query, inRight, environmentToClient, usernamehint.c_str(), newCredential, reason);
		if (status)
			return status;

		// Now we have successfully obtained a credential we need to make sure it authorizes the requested right
		if (!newCredential->isValid())
		{
			reason = SecurityAgent::invalidPassphrase;
			auditrec.submit(AUE_ssauthorize, CommonCriteria::errInvalidCredential, inRight->name());
		}
		else {
			status = evaluateCredentialForRight(inRight, inRule, environmentToClient, now, newCredential, true);
			if (status == errAuthorizationSuccess)
			{
				// Add the new credential we obtained to the output set.
				// whack an equivalent credential, so it gets updated to a later achieved credential which must have been more stringent
				credentials.erase(newCredential); credentials.insert(newCredential);
				query.done();
						
				// add credential to authinfo
				auth.setCredentialInfo(newCredential);
								
				auditrec.submit(AUE_ssauthorize, CommonCriteria::errNone, inRight->name());
				return errAuthorizationSuccess;
			}
			else if (status != errAuthorizationDenied)
			{
				if (status == errAuthorizationCanceled)
					auditrec.submit(AUE_ssauthorize, CommonCriteria::errUserCanceled, inRight->name());
				// else don't audit--error not due to bad
				// username or password
				return status;
			}
		}
		reason = SecurityAgent::userNotInGroup;
	}
	query.cancel(SecurityAgent::tooManyTries);

	auditrec.submit(AUE_ssauthorize, CommonCriteria::errTooManyTries, inRight->name());
	return errAuthorizationDenied;
}

OSStatus
RuleImpl::obtainCredential(QueryAuthorizeByGroup &query, const AuthItemRef &inRight, 
    AuthItemSet &environmentToClient, const char *usernameHint, Credential &outCredential, SecurityAgent::Reason reason) const
{
	char nameBuffer[SecurityAgent::maxUsernameLength];
	char passphraseBuffer[SecurityAgent::maxPassphraseLength];
	OSStatus status = errAuthorizationDenied;

	try {
		if (query(mGroupName.c_str(), usernameHint, nameBuffer, passphraseBuffer, reason))
			status = noErr;
	} catch (const CssmCommonError &err) {
		status = err.osStatus();
	} catch (...) {
		status = errAuthorizationInternal;
	}
	if (status == CSSM_ERRCODE_USER_CANCELED)
	{
		secdebug("auth", "canceled obtaining credential for user in group %s", mGroupName.c_str());
		return errAuthorizationCanceled;
	}
	if (status == CSSM_ERRCODE_NO_USER_INTERACTION)
	{
		secdebug("auth", "user interaction not possible obtaining credential for user in group %s", mGroupName.c_str());
		return errAuthorizationInteractionNotAllowed;
	}

	if (status != noErr)
	{
		secdebug("auth", "failed obtaining credential for user in group %s", mGroupName.c_str());
		return status;
	}

	secdebug("auth", "obtained credential for user %s", nameBuffer);
	string username(nameBuffer);
	string password(passphraseBuffer);
	outCredential = Credential(username, password, mShared);
	return errAuthorizationSuccess;
}


Rule::Rule() : RefPointer<RuleImpl>(new RuleImpl()) {}
Rule::Rule(const string &inRightName, CFDictionaryRef cfRight, CFDictionaryRef cfRules) : RefPointer<RuleImpl>(new RuleImpl(inRightName, cfRight, cfRules)) {}


} // end namespace Authorization
