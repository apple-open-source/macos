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
 *  AuthorizationRule.h
 *  Security
 *
 */

#ifndef _H_AUTHORIZATIONRULE
#define _H_AUTHORIZATIONRULE  1

#include <CoreFoundation/CoreFoundation.h>
#include <security_cdsa_utilities/AuthorizationData.h>
#include "authority.h"

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
		AuthorizationToken &auth, SecurityAgent::Reason &reason, bool savePassword) const;

	string name() const { return mRightName; }
	bool extractPassword() const { return mExtractPassword; }

private:
// internal machinery

	// evaluate credential for right
	OSStatus evaluateCredentialForRight(const AuthorizationToken &auth, const AuthItemRef &inRight, const Rule &inRule, 
                                        const AuthItemSet &environment, CFAbsoluteTime now, const Credential &credential, bool ignoreShared, SecurityAgent::Reason &reason) const;
	// evaluate user credential (authentication) for right
	OSStatus evaluateUserCredentialForRight(const AuthorizationToken &auth, const AuthItemRef &inRight, const Rule &inRule, const AuthItemSet &environment, CFAbsoluteTime now, const Credential &credential, bool ignoreShared, SecurityAgent::Reason &reason) const;

	OSStatus evaluateRules(const AuthItemRef &inRight, const Rule &inRule,
    AuthItemSet &environmentToClient, AuthorizationFlags flags,
	CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials,
	AuthorizationToken &auth, SecurityAgent::Reason &reason, bool savePassword) const;

	void setAgentHints(const AuthItemRef &inRight, const Rule &inTopLevelRule, AuthItemSet &environmentToClient, AuthorizationToken &auth) const;

	// perform authorization based on running specified mechanisms (see evaluateMechanism)
	OSStatus evaluateAuthentication(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationFlags flags, CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials, AuthorizationToken &auth, SecurityAgent::Reason &reason, bool savePassword) const;

	OSStatus evaluateUser(const AuthItemRef &inRight, const Rule &inRule,
		AuthItemSet &environmentToClient, AuthorizationFlags flags,
		CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials,
		AuthorizationToken &auth, SecurityAgent::Reason &reason, bool savePassword) const;

	OSStatus evaluateMechanismOnly(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationToken &auth, CredentialSet &outCredentials, bool savePassword) const;

	// find username hint based on session owner
	OSStatus evaluateSessionOwner(const AuthItemRef &inRight, const Rule &inRule, const AuthItemSet &environment, const CFAbsoluteTime now, const AuthorizationToken &auth, Credential &credential, SecurityAgent::Reason &reason) const;

	CredentialSet makeCredentials(const AuthorizationToken &auth) const;
	
	map<string,string> localizedPrompts() const { return mLocalizedPrompts; }
	map<string,string> localizedButtons() const { return mLocalizedButtons; }
	
    
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
	bool mExtractPassword;
	bool mAuthenticateUser;
	map<string,string> mLocalizedPrompts;
	map<string,string> mLocalizedButtons;

private:

	class Attribute
	{
	public:
		static bool getBool(CFDictionaryRef config, CFStringRef key, bool required, bool defaultValue);
		static double getDouble(CFDictionaryRef config, CFStringRef key, bool required, double defaultValue);
		static string getString(CFDictionaryRef config, CFStringRef key, bool required, const char *defaultValue);
		static vector<string> getVector(CFDictionaryRef config, CFStringRef key, bool required);
		static bool getLocalizedText(CFDictionaryRef config, map<string,string> &localizedPrompts, CFStringRef dictKey, const char *descriptionKey);
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
	static CFStringRef kButtonID;
    static CFStringRef kTriesID;
	static CFStringRef kExtractPasswordID;
    
	static CFStringRef kRuleClassID;
	static CFStringRef kRuleAllowID;
	static CFStringRef kRuleDenyID;
	static CFStringRef kRuleUserID;
	static CFStringRef kRuleDelegateID;
	static CFStringRef kRuleMechanismsID;
	static CFStringRef kRuleAuthenticateUserID;
};

class Rule : public RefPointer<RuleImpl>
{
public:
	Rule();
	Rule(const string &inRightName, CFDictionaryRef cfRight, CFDictionaryRef cfRules);
};

}; /* namespace Authorization */

#endif /* ! _H_AUTHORIZATIONRULE */
