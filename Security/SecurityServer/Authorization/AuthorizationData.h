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
 *  AuthorizationData.h
 *  Authorization
 *
 *    Copyright:  (c) 2000 by Apple Computer, Inc., all rights reserved
 *
 */

#ifndef _H_AUTHORIZATIONDATA
#define _H_AUTHORIZATIONDATA  1

#include <Security/Authorization.h>
#include <Security/AuthorizationPlugin.h>

#include <Security/refcount.h>
#include <Security/cssmalloc.h>

#include <map>
#include <set>
#include <string>

// ptrdiff_t needed, so including STL type closest
#include <vector>

namespace Authorization
{

class AuthValueOverlay : public AuthorizationValue
{
public:
	AuthValueOverlay(const string& stringValue) { length = stringValue.length(); data = const_cast<char *>(stringValue.c_str()); }
	AuthValueOverlay(UInt32 inLength, void *inData) { length = inLength; data = inData; }
};

class AuthValueRef;

class AuthValue : public RefCount
{
	friend class AuthValueRef;
private:
	AuthValue(const AuthValue& value) {}
protected:
	AuthValue(const AuthorizationValue &value);
	AuthValue(UInt32 length, void *data);
public:
    AuthValue &operator = (const AuthValue &other);
    ~AuthValue();
    void fillInAuthorizationValue(AuthorizationValue &value);
    const AuthorizationValue& value() const { return mValue; }
private:
    AuthorizationValue mValue;
    mutable bool mOwnsValue;
};

// AuthValueRef impl
class AuthValueRef : public RefPointer<AuthValue>
{
public:
    AuthValueRef(const AuthValue &value);
    AuthValueRef(const AuthorizationValue &value);
    AuthValueRef(UInt32 length, void *data);
};


// vector should become a member with accessors
class AuthValueVector : public vector<AuthValueRef>
{
    NOCOPY(AuthValueVector)
public:
    AuthValueVector() {}
    ~AuthValueVector() {}

    AuthValueVector &operator = (const AuthorizationValueVector& valueVector);

    void copy(AuthorizationValueVector **data, size_t *length) const;
};



class AuthItemRef;

class AuthItem : public RefCount
{
    friend class AuthItemRef;
private:
    AuthItem(const AuthItem& item);
protected:
    AuthItem(const AuthorizationItem &item);
    AuthItem(AuthorizationString name);
    AuthItem(AuthorizationString name, AuthorizationValue value);
    AuthItem(AuthorizationString name, AuthorizationValue value, AuthorizationFlags flags);

    bool operator < (const AuthItem &other) const;

public:
    AuthItem &operator = (const AuthItem &other);
    ~AuthItem();
    
    void fillInAuthorizationItem(AuthorizationItem &item);
    
    AuthorizationString name() const { return mName; }
    const AuthorizationValue& value() const { return mValue; }
	string stringValue() const { return string(static_cast<char *>(mValue.data), mValue.length); }
    AuthorizationFlags flags() const { return mFlags; }
	void setFlags(AuthorizationFlags inFlags) { mFlags = inFlags; };

private:
    AuthorizationString mName;
    AuthorizationValue mValue;
    AuthorizationFlags mFlags;
    mutable bool mOwnsName;
    mutable bool mOwnsValue;
};

class AuthItemRef : public RefPointer<AuthItem>
{
public:
    AuthItemRef(const AuthorizationItem &item);
    AuthItemRef(AuthorizationString name);
    AuthItemRef(AuthorizationString name, AuthorizationValue value, AuthorizationFlags flags = 0);

    bool operator < (const AuthItemRef &other) const
    {
        return **this < *other;
    }
};

// set should become a member with accessors
class AuthItemSet : public set<AuthItemRef>
{
public:
    AuthItemSet();
    ~AuthItemSet();
    AuthItemSet(const AuthorizationItemSet *item);

    AuthItemSet &operator = (const AuthorizationItemSet& itemSet);

	void copy(AuthorizationItemSet *&data, size_t &length, CssmAllocator &alloc = CssmAllocator::standard()) const;
};

class FindAuthItemByRightName
{
public:
    FindAuthItemByRightName(const char *find_name) : name(find_name) { }

    bool operator()( const AuthItemRef& authitem )
    {
        return (!strcmp(name, authitem->name()));
    }
    bool operator()( const AuthorizationItem* authitem )
    {
        return (!strcmp(name, authitem->name));
    }
    
private:
    const char *name;
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




}; // namespace Authorization

#endif /* ! _H_AUTHORIZATIONDATA */
