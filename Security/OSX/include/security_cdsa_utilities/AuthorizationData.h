/*
 * Copyright (c) 2000,2002-2006,2011,2014 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


/*
 *  AuthorizationData.h
 *  Authorization
 */

#ifndef _H_AUTHORIZATIONDATA
#define _H_AUTHORIZATIONDATA  1

#include <Security/Authorization.h>
#include <Security/AuthorizationPlugin.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <CoreFoundation/CFDate.h>

#include <security_utilities/refcount.h>
#include <security_utilities/alloc.h>

#include <map>
#include <set>
#include <string>

// ptrdiff_t needed, so including STL type closest
#include <vector>

// @@@ Should consider making the various types better citizens by taking an Allocator, for now values are wiped.

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
public:
    AuthValueVector() {}
    ~AuthValueVector() {}

    AuthValueVector &operator = (const AuthorizationValueVector& valueVector);
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
	
public:
	bool getString(string &value);
	bool getCssmData(CssmAutoData &value);
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
    AuthItemSet(const AuthItemSet& itemSet);

    AuthItemSet &operator = (const AuthorizationItemSet& itemSet);
    AuthItemSet &operator = (const AuthItemSet& itemSet);

public:
	AuthItem *find(const char *name);
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

}; // namespace Authorization

#endif /* ! _H_AUTHORIZATIONDATA */
