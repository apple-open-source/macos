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

#if !defined(__AuthorizationEngine__)
#define __AuthorizationEngine__ 1

#include <Security/Authorization.h>
#include <Security/refcount.h>
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
    virtual CSSM_RETURN cssmError() const;
    virtual OSStatus osStatus() const;
    virtual const char *what () const;
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


class MutableRightSet;
class RightSet;

class Right : protected AuthorizationItem
{
	friend MutableRightSet;
	friend RightSet;
public:
	static Right &overlay(AuthorizationItem &item);
	static Right *overlay(AuthorizationItem *item);
	Right();
	Right(AuthorizationString name, size_t valueLength, const void *value);
	~Right();

	bool operator < (const Right &other) const;
	AuthorizationString rightName() const { return name; }
	size_t argumentLength() const { return valueLength; }
	const void *argument() const { return value; }
};


/* A RightSet is a Container and a Back Insertion Sequence, but it is not a Sequence.  Also it only
   implements the const members of Container and Back Insertion Sequence. */
class RightSet
{
	friend class MutableRightSet;
public:
	// Container required memebers
	typedef Right value_type;
	typedef const Right &const_reference;
	typedef const Right *const_pointer;
	typedef const_pointer const_iterator;
	typedef ptrdiff_t difference_type;
	typedef size_t size_type;

	RightSet(const AuthorizationRights *rights = NULL);
	RightSet(const RightSet &other);
	~RightSet();

	size_type size() const { return mRights->count; }
	size_type max_size() const { return INT_MAX; }
	const_iterator begin() const { return static_cast<const_pointer>(mRights->items); }
	const_iterator end() const { return static_cast<const_pointer>(&mRights->items[mRights->count]); }
	bool empty() const { return size() == 0; }

	// Back Insertion Sequence required memebers
	const_reference back() const;

	// Other convenience members
	operator const AuthorizationRights *() const { return mRights; }
private:
	RightSet &operator = (const RightSet &other);

protected:
	static const AuthorizationRights gEmptyRights;
	AuthorizationRights *mRights;
};


/* A MutableRightSet is a Container and a Back Insertion Sequence, but it is not a Sequence. */
class MutableRightSet : public RightSet
{
public:
	// Container required memebers
	typedef Right &reference;
	typedef Right *pointer;
	typedef pointer iterator;

	MutableRightSet(size_t count = 0, const Right &element = Right());
	MutableRightSet(const RightSet &other);
	~MutableRightSet();

	MutableRightSet &operator = (const RightSet &other);

	iterator begin() { return static_cast<pointer>(mRights->items); }
	iterator end() { return static_cast<pointer>(&mRights->items[mRights->count]); }
	void swap(MutableRightSet &other);

	// Back Insertion Sequence required memebers
	reference back();
	void push_back(const_reference right);
	void pop_back();

	// Other convenience members
	size_type capacity() const { return mCapacity; }
private:
	void grow(size_type min_capacity);

	size_type mCapacity;
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
		const AuthorizationToken &auth);

private:
	OSStatus evaluate(const Right &inRight, const AuthorizationEnvironment *environment,
		CFAbsoluteTime now, const Credential &credential, bool ignoreShared);
	OSStatus obtainCredential(QueryAuthorizeByGroup &client, const Right &inRight,
		const AuthorizationEnvironment *environment, const char *usernameHint,
		Credential &outCredential, SecurityAgent::Reason reason);

	enum Type
	{
		kDeny,
		kAllow,
		kUserInGroup
	} mType;

	string mGroupName;
	CFTimeInterval mMaxCredentialAge;
	bool mShared;
	bool mAllowRoot;

	static CFStringRef kUserInGroupID;
	static CFStringRef kTimeoutID;
	static CFStringRef kSharedID;
	static CFStringRef kAllowRootID;
	static CFStringRef kDenyID;
	static CFStringRef kAllowID;
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
		MutableRightSet *outRights, const AuthorizationToken &auth);
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

	typedef map<Right, Rule> RightMap;
	typedef map<string, Rule> RuleMap;

	RuleMap mRules;
};

}; // namespace Authorization

#endif /* ! __AuthorizationEngine__ */
