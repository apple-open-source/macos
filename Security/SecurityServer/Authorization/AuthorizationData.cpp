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
 *  AuthorizationData.cpp
 *  Authorization
 *
 *  Created by Michael Brouwer on Thu Oct 12 2000.
 *  Copyright (c) 2000 Apple Computer Inc. All rights reserved.
 *
 */

#include "AuthorizationData.h"

#include <grp.h>
#include <pwd.h>
#include <Security/checkpw.h>

#include "server.h"


// checkpw() that uses provided struct passwd
extern "C"
{
int checkpw_internal( const struct passwd *pw, const char* password );
}


namespace Authorization {


AuthValueRef::AuthValueRef(const AuthValue &value) : 
	RefPointer<AuthValue>(new AuthValue(value)) {}

AuthValueRef::AuthValueRef(const AuthorizationValue &value) : 
	RefPointer<AuthValue>(new AuthValue(value)) {}

AuthValue::AuthValue(const AuthorizationValue &value) :
    mOwnsValue(false)
{
    mValue.length = value.length;
    mValue.data = value.data;
}

AuthValueRef::AuthValueRef(UInt32 length, void *data) : 
	RefPointer<AuthValue>(new AuthValue(length, data)) {}

AuthValue::AuthValue(UInt32 length, void *data) :
    mOwnsValue(true)
{
    mValue.length = length;
    mValue.data = new uint8_t[length];
    if (length)
        memcpy(mValue.data, data, length);
}

AuthValue::~AuthValue()
{
    if (mOwnsValue)
        delete[] reinterpret_cast<uint8_t*>(mValue.data);
}

AuthValue &
AuthValue::operator = (const AuthValue &other)
{
    if (mOwnsValue)
        delete[] reinterpret_cast<uint8_t*>(mValue.data);

    mValue = other.mValue;
    mOwnsValue = other.mOwnsValue;
    other.mOwnsValue = false;
    return *this;
}

void
AuthValue::fillInAuthorizationValue(AuthorizationValue &value)
{
    value.length = mValue.length;
    value.data = mValue.data;
}

AuthValueVector &
AuthValueVector::operator = (const AuthorizationValueVector& valueVector)
{
    clear();
    for (unsigned int i=0; i < valueVector.count; i++)
        push_back(AuthValueRef(valueVector.values[i]));
    return *this;
}

void
AuthValueVector::copy(AuthorizationValueVector **data, size_t *length) const
{
    AuthorizationValueVector valueVector;
    valueVector.count = size();
    valueVector.values = new AuthorizationValue[valueVector.count];
    int i = 0;
	for (const_iterator it = begin(); it != end(); ++it, ++i)
    {
		(*it)->fillInAuthorizationValue(valueVector.values[i]);
	}

	Copier<AuthorizationValueVector> flatValueVector(&valueVector);
	*length = flatValueVector.length();
	*data = flatValueVector.keep();

	delete[] valueVector.values;
}

AuthItem::AuthItem(const AuthorizationItem &item) :
    mFlags(item.flags),
    mOwnsName(true),
    mOwnsValue(true)
{
	if (!item.name)
		MacOSError::throwMe(errAuthorizationInternal);
	size_t nameLen = strlen(item.name) + 1;
	mName = new char[nameLen];
	memcpy(const_cast<char *>(mName), item.name, nameLen);

	mValue.length = item.valueLength;
	mValue.data = new uint8_t[item.valueLength];
	if (mValue.length)
		memcpy(mValue.data, item.value, item.valueLength);
}


AuthItem::AuthItem(AuthorizationString name) :
    mName(name),
    mFlags(0),
    mOwnsName(false),
    mOwnsValue(false)
{
    mValue.length = 0;
    mValue.data = NULL;
}

AuthItem::AuthItem(AuthorizationString name, AuthorizationValue value, AuthorizationFlags flags) :
    mFlags(flags),
    mOwnsName(true),
    mOwnsValue(true)
{
	if (!name)
		MacOSError::throwMe(errAuthorizationInternal);
	size_t nameLen = strlen(name) + 1;
	mName = new char[nameLen];
	memcpy(const_cast<char *>(mName), name, nameLen);

	mValue.length = value.length;
	mValue.data = new uint8_t[value.length];
	if (mValue.length)
		memcpy(mValue.data, value.data, value.length);
}

AuthItem::~AuthItem()
{
    if (mOwnsName)
        delete[] mName;
    if (mOwnsValue)
        delete[] reinterpret_cast<uint8_t*>(mValue.data);
}

bool
AuthItem::operator < (const AuthItem &other) const
{
    return strcmp(mName, other.mName) < 0;
}

AuthItem &
AuthItem::operator = (const AuthItem &other)
{
    if (mOwnsName)
        delete[] mName;
    if (mOwnsValue)
        delete[] reinterpret_cast<uint8_t*>(mValue.data);

    mName = other.mName;
    mValue = other.mValue;
    mFlags = other.mFlags;
    mOwnsName = other.mOwnsName;
    other.mOwnsName = false;
    mOwnsValue = other.mOwnsValue;
    other.mOwnsValue = false;
    return *this;
}

void
AuthItem::fillInAuthorizationItem(AuthorizationItem &item)
{
    item.name = mName;
    item.valueLength = mValue.length;
    item.value = mValue.data;
    item.flags = mFlags;
}


AuthItemRef::AuthItemRef(const AuthorizationItem &item) : RefPointer<AuthItem>(new AuthItem(item)) {}

AuthItemRef::AuthItemRef(AuthorizationString name) : RefPointer<AuthItem>(new AuthItem(name)) {}

AuthItemRef::AuthItemRef(AuthorizationString name, AuthorizationValue value, AuthorizationFlags flags) : RefPointer<AuthItem>(new AuthItem(name, value, flags)) {}


//
// AuthItemSet
//
AuthItemSet::AuthItemSet()
{
}

AuthItemSet::~AuthItemSet()
{
}

AuthItemSet &
AuthItemSet::operator = (const AuthorizationItemSet& itemSet)
{
    clear();

    for (unsigned int i=0; i < itemSet.count; i++)
        insert(AuthItemRef(itemSet.items[i]));

    return *this;
}

AuthItemSet::AuthItemSet(const AuthorizationItemSet *itemSet)
{
	if (itemSet)
	{
		for (unsigned int i=0; i < itemSet->count; i++)
			insert(AuthItemRef(itemSet->items[i]));
	}
}

void
AuthItemSet::copy(AuthorizationItemSet *&data, size_t &length, CssmAllocator &alloc) const
{
    AuthorizationItemSet itemSet;
    itemSet.count = size();
    itemSet.items = new AuthorizationItem[itemSet.count];
    int i = 0;
    for (const_iterator it = begin(); it != end(); ++it, ++i)
    {
        (*it)->fillInAuthorizationItem(itemSet.items[i]);
    }

	Copier<AuthorizationItemSet> flatItemSet(&itemSet, alloc);
	length = flatItemSet.length();
		
	data = flatItemSet.keep();
	// else flatItemSet disappears again

    delete[] itemSet.items;
}

//
// CredentialImpl class
//

// only for testing whether this credential is usable
CredentialImpl::CredentialImpl(const string &username, const uid_t uid, const gid_t gid, bool shared) :
mUsername(username), mShared(shared), mUid(uid), mGid(gid), mCreationTime(CFAbsoluteTimeGetCurrent()), mValid(true)
{
}

// credential with validity based on username/password combination.
CredentialImpl::CredentialImpl(const string &username, const string &password, bool shared) :
mUsername(username), mShared(shared), mCreationTime(CFAbsoluteTimeGetCurrent()), mValid(false)
{
	// Calling into DirectoryServices can be a long term operation
	Server::active().longTermActivity();

	// try short name first
	const char *user = username.c_str();
	struct passwd *pw = getpwnam(user);

    do {

        if (!pw)
        {
            secdebug("autheval", "user %s not found, creating invalid credential", user);
            break;
        }

		mUsername = string ( pw->pw_name );
		mUid = pw->pw_uid;
		mGid = pw->pw_gid;

        const char *passwd = password.c_str();
        int checkpw_status = checkpw_internal(pw, passwd);

        if (checkpw_status != CHECKPW_SUCCESS)
        {
				secdebug("autheval", "checkpw() for user %s failed with error %d, creating invalid credential", user, checkpw_status);
				break;
        }

		secdebug("autheval", "checkpw() for user %s succeeded, creating%s credential",
			user, mShared ? " shared" : "");

		mValid = true;

        endpwent();
    }
	while (0);
}


CredentialImpl::~CredentialImpl()
{
}

bool
CredentialImpl::operator < (const CredentialImpl &other) const
{
	if (!mShared && other.mShared)
		return true;
	if (!other.mShared && mShared)
		return false;

	return mUsername < other.mUsername;
}

// Returns true if this CredentialImpl should be shared.
bool
CredentialImpl::isShared() const
{
	return mShared;
}

// Merge with other
void
CredentialImpl::merge(const CredentialImpl &other)
{
	assert(mUsername == other.mUsername);

	if (other.mValid && (!mValid || mCreationTime < other.mCreationTime))
	{
		mCreationTime = other.mCreationTime;
		mUid = other.mUid;
		mGid = other.mGid;
		mValid = true;
	}
}

// The time at which this credential was obtained.
CFAbsoluteTime
CredentialImpl::creationTime() const
{
	return mCreationTime;
}

// Return true iff this credential is valid.
bool
CredentialImpl::isValid() const
{
	return mValid;
}

void
CredentialImpl::invalidate()
{
	mValid = false;
}

//
// Credential class
//
Credential::Credential() :
RefPointer<CredentialImpl>(NULL)
{
}

Credential::Credential(CredentialImpl *impl) :
RefPointer<CredentialImpl>(impl)
{
}

Credential::Credential(const string &username, const uid_t uid, const gid_t gid, bool shared) :
RefPointer<CredentialImpl>(new CredentialImpl(username, uid, gid, shared))
{
}

Credential::Credential(const string &username, const string &password, bool shared) :
RefPointer<CredentialImpl>(new CredentialImpl(username, password, shared))
{
}

Credential::~Credential()
{
}

bool
Credential::operator < (const Credential &other) const
{
	if (!*this)
		return other;

	if (!other)
		return false;

	return (**this) < (*other);
}



}	// end namespace Authorization
