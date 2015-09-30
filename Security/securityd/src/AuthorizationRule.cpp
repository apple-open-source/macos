/*
 *  Copyright (c) 2003-2010,2012 Apple Inc. All Rights Reserved.
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
	Syslog::alert("Authorization via securityd no longer supported");
	MacOSError::throwMe(errAuthorizationInternal);
}

double
RuleImpl::Attribute::getDouble(CFDictionaryRef config, CFStringRef key, bool required = false, double defaultValue = 0.0)
{
	Syslog::alert("Authorization via securityd no longer supported");
	MacOSError::throwMe(errAuthorizationInternal);
}

bool
RuleImpl::Attribute::getBool(CFDictionaryRef config, CFStringRef key, bool required = false, bool defaultValue = false)
{
	Syslog::alert("Authorization via securityd no longer supported");
	MacOSError::throwMe(errAuthorizationInternal);
}

vector<string>
RuleImpl::Attribute::getVector(CFDictionaryRef config, CFStringRef key, bool required = false)
{
	Syslog::alert("Authorization via securityd no longer supported");
	MacOSError::throwMe(errAuthorizationInternal);
}


bool RuleImpl::Attribute::getLocalizedText(CFDictionaryRef config, map<string,string> &localizedPrompts, CFStringRef dictKey, const char *descriptionKey)
{
	Syslog::alert("Authorization via securityd no longer supported");
	MacOSError::throwMe(errAuthorizationInternal);
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
	Syslog::alert("Authorization via securityd no longer supported");
	MacOSError::throwMe(errAuthorizationInternal);
}

/*
RuleImpl::~Rule()
{
}
*/

void
RuleImpl::setAgentHints(const AuthItemRef &inRight, const Rule &inTopLevelRule, AuthItemSet &environmentToClient, AuthorizationToken &auth) const
{
	Syslog::alert("Authorization via securityd no longer supported");
	MacOSError::throwMe(errAuthorizationInternal);
}

// If a different evaluation for getting a credential is prescribed,
// we'll run that and validate the credentials from there.
// we fall back on a default configuration from the authenticate rule
OSStatus
RuleImpl::evaluateAuthentication(const AuthItemRef &inRight, const Rule &inRule,AuthItemSet &environmentToClient, AuthorizationFlags flags, CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials, AuthorizationToken &auth, SecurityAgent::Reason &reason, bool savePassword) const
{
	Syslog::alert("Authorization via securityd no longer supported");
	MacOSError::throwMe(errAuthorizationInternal);
}

// create externally verified credentials on the basis of 
// mechanism-provided information
CredentialSet
RuleImpl::makeCredentials(const AuthorizationToken &auth) const
{
	Syslog::alert("Authorization via securityd no longer supported");
	MacOSError::throwMe(errAuthorizationInternal);
}

// evaluate whether a good credential of the current session owner would authorize a right
OSStatus
RuleImpl::evaluateSessionOwner(const AuthItemRef &inRight, const Rule &inRule, const AuthItemSet &environment, const CFAbsoluteTime now, const AuthorizationToken &auth, Credential &credential, SecurityAgent::Reason &reason) const
{
	Syslog::alert("Authorization via securityd no longer supported");
	MacOSError::throwMe(errAuthorizationInternal);
}


OSStatus
RuleImpl::evaluateCredentialForRight(const AuthorizationToken &auth, const AuthItemRef &inRight, const Rule &inRule, const AuthItemSet &environment, CFAbsoluteTime now, const Credential &credential, bool ignoreShared, SecurityAgent::Reason &reason) const
{
	Syslog::alert("Authorization via securityd no longer supported");
	MacOSError::throwMe(errAuthorizationInternal);
}

// Return errAuthorizationSuccess if this rule allows access based on the specified credential,
// return errAuthorizationDenied otherwise.
OSStatus
RuleImpl::evaluateUserCredentialForRight(const AuthorizationToken &auth, const AuthItemRef &inRight, const Rule &inRule, const AuthItemSet &environment, CFAbsoluteTime now, const Credential &credential, bool ignoreShared, SecurityAgent::Reason &reason) const
{
	Syslog::alert("Authorization via securityd no longer supported");
	MacOSError::throwMe(errAuthorizationInternal);
}



OSStatus
RuleImpl::evaluateUser(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationFlags flags, CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials, AuthorizationToken &auth, SecurityAgent::Reason &reason, bool savePassword) const
{
	Syslog::alert("Authorization via securityd no longer supported");
	MacOSError::throwMe(errAuthorizationInternal);
}

OSStatus
RuleImpl::evaluateMechanismOnly(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationToken &auth, CredentialSet &outCredentials, bool savePassword) const
{
	Syslog::alert("Authorization via securityd no longer supported");
	MacOSError::throwMe(errAuthorizationInternal);
}

OSStatus
RuleImpl::evaluateRules(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationFlags flags, CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials, AuthorizationToken &auth, SecurityAgent::Reason &reason, bool savePassword) const
{
	Syslog::alert("Authorization via securityd no longer supported");
	MacOSError::throwMe(errAuthorizationInternal);
}


OSStatus
RuleImpl::evaluate(const AuthItemRef &inRight, const Rule &inRule, AuthItemSet &environmentToClient, AuthorizationFlags flags, CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials, AuthorizationToken &auth, SecurityAgent::Reason &reason, bool savePassword) const
{
	Syslog::alert("Authorization via securityd no longer supported");
	MacOSError::throwMe(errAuthorizationInternal);
}

Rule::Rule() : RefPointer<RuleImpl>(new RuleImpl()) {}
Rule::Rule(const string &inRightName, CFDictionaryRef cfRight, CFDictionaryRef cfRules) : RefPointer<RuleImpl>(new RuleImpl(inRightName, cfRight, cfRules)) {}



} // end namespace Authorization
