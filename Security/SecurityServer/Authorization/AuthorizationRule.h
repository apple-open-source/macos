/*
 *  AuthorizationRule.h
 *  Security
 *
 *  Created by Conrad Sauerwald on Wed Mar 19 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef _H_AUTHORIZATIONRULE
#define _H_AUTHORIZATIONRULE  1

#include <CoreFoundation/CoreFoundation.h>
#include "AuthorizationData.h"

#include "agentquery.h"


namespace Authorization
{

class Rule;

class RuleImpl : public RefCount
{
public:
	RuleImpl();
	RuleImpl(const string &inRightName, CFDictionaryRef cfRight, CFDictionaryRef cfRules);

	OSStatus evaluate(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient,
		AuthorizationFlags flags, CFAbsoluteTime now,
		const CredentialSet *inCredentials, CredentialSet &credentials,
		AuthorizationToken &auth) const;

	string name() const { return mRightName; }

private:
// internal machinery

	// evaluate credential for right
	OSStatus evaluateCredentialForRight(const AuthItemRef &inRight, const Rule &inRule, 
		const AuthItemSet &environment,
		CFAbsoluteTime now, const Credential &credential, bool ignoreShared) const;
		
	// run mechanisms specified for this rule
	OSStatus evaluateMechanism(const AuthItemRef &inRight, const AuthItemSet &environment, AuthorizationToken &auth, CredentialSet &outCredentials) const;

	OSStatus evaluateRules(const AuthItemRef &inRight, const Rule &inRule,
    AuthItemSet &environmentToClient, AuthorizationFlags flags,
	CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials,
	AuthorizationToken &auth) const;

	void setAgentHints(const AuthItemRef &inRight, const Rule &inTopLevelRule, AuthItemSet &environmentToClient, AuthorizationToken &auth) const;

	// perform authorization based on running specified mechanisms (see evaluateMechanism)
	OSStatus evaluateAuthorization(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationFlags flags, CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials, AuthorizationToken &auth) const;

	OSStatus evaluateAuthorizationOld(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationFlags flags, CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials, AuthorizationToken &auth) const;
	OSStatus obtainCredential(QueryAuthorizeByGroup &query, const AuthItemRef &inRight, AuthItemSet &environmentToClient, const char *usernameHint, Credential &outCredential, SecurityAgent::Reason reason) const;

	OSStatus evaluateUser(const AuthItemRef &inRight, const Rule &inRule,
		AuthItemSet &environmentToClient, AuthorizationFlags flags,
		CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials,
		AuthorizationToken &auth) const;

	OSStatus evaluateMechanismOnly(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationToken &auth, CredentialSet &outCredentials) const;

	// find username hint based on session owner
	OSStatus evaluateSessionOwner(const AuthItemRef &inRight, const Rule &inRule, const AuthItemSet &environment, const CFAbsoluteTime now, const AuthorizationToken &auth, string& usernamehint) const;
	

	string agentNameForAuth(const AuthorizationToken &auth) const;
	CredentialSet makeCredentials(const AuthItemSet &context) const;
	
	map<string,string> localizedPrompts() const { return mLocalizedPrompts; }
	
// parsed attributes
private:
	enum Type
	{
		kDeny,
		kAllow,
		kUser,
		kRuleDelegation,
		kKofN,
		kEvaluateMechanisms,
	} mType;

	string mRightName;
	string mGroupName;
	CFTimeInterval mMaxCredentialAge;
	bool mShared;
	bool mAllowRoot;
	vector<string> mEvalDef;
	bool mSessionOwner;
	vector<Rule> mRuleDef;
	uint32_t mKofN;
	mutable uint32_t mTries;
	map<string,string> mLocalizedPrompts;

private:

	class Attribute
	{
	public:
		static bool getBool(CFDictionaryRef config, CFStringRef key, bool required, bool defaultValue);
		static double getDouble(CFDictionaryRef config, CFStringRef key, bool required, double defaultValue);
		static string getString(CFDictionaryRef config, CFStringRef key, bool required, char *defaultValue);
		static vector<string> getVector(CFDictionaryRef config, CFStringRef key, bool required);
		static void setString(CFMutableDictionaryRef config, CFStringRef key, string &value);
		static void setDouble(CFMutableDictionaryRef config, CFStringRef key, double value);
		static void setBool(CFMutableDictionaryRef config, CFStringRef key, bool value);
		static bool getLocalizedPrompts(CFDictionaryRef config, map<string,string> &localizedPrompts);
	};


// keys
	static CFStringRef kUserGroupID;
	static CFStringRef kTimeoutID;
	static CFStringRef kSharedID;
	static CFStringRef kAllowRootID;
	static CFStringRef kMechanismsID;
	static CFStringRef kSessionOwnerID;
	static CFStringRef kKofNID;
	static CFStringRef kPromptID;

	static CFStringRef kRuleClassID;
	static CFStringRef kRuleAllowID;
	static CFStringRef kRuleDenyID;
	static CFStringRef kRuleUserID;
	static CFStringRef kRuleDelegateID;
	static CFStringRef kRuleMechanismsID;

};

class Rule : public RefPointer<RuleImpl>
{
public:
	Rule();
	Rule(const string &inRightName, CFDictionaryRef cfRight, CFDictionaryRef cfRules);
};

}; /* namespace Authorization */

#endif /* ! _H_AUTHORIZATIONRULE */
