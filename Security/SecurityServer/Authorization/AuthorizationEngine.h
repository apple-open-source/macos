/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 *  AuthorizationEngine.h
 *  Authorization
 *
 *    Copyright:  (c) 2000 by Apple Computer, Inc., all rights reserved
 *
 */

#ifndef _H_AUTHORIZATIONENGINE
#define _H_AUTHORIZATIONENGINE  1

#include <Security/Authorization.h>
#include <Security/AuthorizationPlugin.h>
#include "AuthorizationData.h"

#include <Security/refcount.h>
#include <Security/threading.h>
#include <Security/osxsigning.h>
#include "agentquery.h"

#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFDictionary.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <map>
#include <set>
#include <string>

class AuthorizationToken;

namespace Authorization
{

class Error : public CssmCommonError {
protected:
    Error(int err);
public:
    const int error;
    virtual CSSM_RETURN cssmError() const throw();
    virtual OSStatus osStatus() const throw();
    virtual const char *what () const throw();
	// @@@ Default value should be internal error.
    static void throwMe(int err = -1) __attribute((noreturn));
};


/* Credentials are less than comparable so they can be put in sets or maps. */
class CredentialImpl : public RefCount
{
public:
	CredentialImpl(const string &username, const uid_t uid, gid_t gid, bool shared);
	CredentialImpl(const string &username, const string &password, bool shared);
	~CredentialImpl();

	bool operator < (const CredentialImpl &other) const;

	// Returns true if this credential should be shared.
	bool isShared() const;

	// Merge with other
	void merge(const CredentialImpl &other);

	// The time at which this credential was obtained.
	CFAbsoluteTime creationTime() const;

	// Return true iff this credential is valid.
	bool isValid() const;

	// Make this credential invalid.
	void invalidate();

	// We could make Rule a friend but instead we just expose this for now
	inline const string& username() const { return mUsername; }
	inline const uid_t uid() const { return mUid; }
	inline const gid_t gid() const { return mGid; }


private:
	// The username of the user that provided his password.
	// This and mShared are what make this credential unique.
	// @@@ We do not deal with the domain as of yet.
	string mUsername;

	// True iff this credential is shared.
	bool mShared;

	// Fields below are not used by less than operator

	// cached pw-data as returned by getpwnam(mUsername)
	uid_t mUid;
	gid_t mGid;

	CFAbsoluteTime mCreationTime;
	bool mValid;
};


/* Credentials are less than comparable so they can be put in sets or maps. */
class Credential : public RefPointer<CredentialImpl>
{
public:
	Credential();
	Credential(CredentialImpl *impl);
	Credential(const string &username, const uid_t uid, gid_t gid, bool shared);
	Credential(const string &username, const string &password, bool shared);
	~Credential();

	bool operator < (const Credential &other) const;
};


typedef set<Credential> CredentialSet;


class Rule
{
public:
	Rule();
	Rule(CFTypeRef cfRule);
	Rule(const Rule &other);
	Rule &operator = (const Rule &other);
	~Rule();

	OSStatus evaluate(const Right &inRight, const AuthorizationEnvironment *environment,
		AuthorizationFlags flags, CFAbsoluteTime now,
		const CredentialSet *inCredentials, CredentialSet &credentials,
		AuthorizationToken &auth);

private:
	OSStatus evaluate(const Right &inRight, const AuthorizationEnvironment *environment,
		CFAbsoluteTime now, const Credential &credential, bool ignoreShared);
	OSStatus obtainCredential(QueryAuthorizeByGroup &client, const Right &inRight,
		const AuthorizationEnvironment *environment, const char *usernameHint,
		Credential &outCredential, SecurityAgent::Reason reason);
    OSStatus evaluateMechanism(const AuthorizationEnvironment *environment, AuthorizationToken &auth, CredentialSet &outCredentials);


	enum Type
	{
		kDeny,
		kAllow,
		kUserInGroup,
        kEvalMech
	} mType;

	string mGroupName;
	CFTimeInterval mMaxCredentialAge;
	bool mShared;
	bool mAllowRoot;
	string mEvalDef;

	static CFStringRef kUserInGroupID;
	static CFStringRef kTimeoutID;
	static CFStringRef kSharedID;
	static CFStringRef kAllowRootID;
	static CFStringRef kDenyID;
	static CFStringRef kAllowID;
	static CFStringRef kEvalMechID;

};


/* The engine which performs the actual authentication and authorization computations.

	The implementation of a typical call to AuthorizationCreate would look like:
	
	Get the current shared CredentialSet for this session.
	Call authorizedRights() with inRights and the shared CredentialSet.
	Compute the difference set between the rights requested and the rights returned from authorizedRights().  
	Call credentialIds() with the rights computed above (for which we have no credentials yet).
	Call aquireCredentials() for the credentialIds returned from credentialIds()
	For each credential returned place it in the session (replacing when needed) if shared() returns true.
	The authorization returned to the user should now refer to the credentials in the session and the non shared ones returned by aquireCredentials().

	When a call to AuthorizationCopyRights() is made, just call authorizedRights() using the union of the session credentials and the credentials tied to the authorization specified.

	When a call to AuthorizationCopyInfo() is made, ask the Credential specified by tag for it info and return it.

	When a call to AuthorizationFree() is made, delete all the non-shared credentials ascociated with the authorization specified.  If the kAuthorizationFreeFlagDestroy is set.  Also delete the shared credentials ascociated with the authorization specified.
 */
class Engine
{
public:
	Engine(const char *configFile);
	~Engine();

	OSStatus authorize(const RightSet &inRights, const AuthorizationEnvironment *environment,
		AuthorizationFlags flags, const CredentialSet *inCredentials, CredentialSet *outCredentials,
		MutableRightSet *outRights, AuthorizationToken &auth);
private:
	void updateRules(CFAbsoluteTime now);
	void readRules();
	void parseRules(CFDictionaryRef rules);
	static void parseRuleCallback(const void *key, const void *value, void *context);
	void parseRule(CFStringRef right, CFTypeRef rule);

	Rule getRule(const Right &inRight) const;

	char *mRulesFileName;
	CFAbsoluteTime mLastChecked;
	struct timespec mRulesFileMtimespec;

	typedef map<string, Rule> RuleMap;

	RuleMap mRules;
    mutable Mutex mLock;
};

}; // namespace Authorization

#endif /* ! _H_AUTHORIZATIONENGINE */
