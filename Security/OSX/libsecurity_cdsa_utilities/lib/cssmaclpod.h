/*
 * Copyright (c) 2000-2007,2011 Apple Inc. All Rights Reserved.
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


//
// cssmaclpod - enhanced PodWrappers for ACL-related CSSM data structures
//
#ifndef _CSSMACLPOD
#define _CSSMACLPOD

#include <security_utilities/utilities.h>
#include <security_cdsa_utilities/cssmlist.h>
#include <security_cdsa_utilities/cssmalloc.h>

namespace Security {

// a nicer name for an authorization tag
typedef CSSM_ACL_AUTHORIZATION_TAG AclAuthorization;


//
// An STL set of authorization tags, with some convenience features
//
class AclAuthorizationSet : public std::set<AclAuthorization> {
public:
	AclAuthorizationSet() { }
	AclAuthorizationSet(AclAuthorization auth) { insert(auth); }
	AclAuthorizationSet(AclAuthorization *authBegin, AclAuthorization *authEnd)
		: set<AclAuthorization>(authBegin, authEnd) { }
	AclAuthorizationSet(AclAuthorization a1, AclAuthorization a2, ...);	// list of auths, end with zero
};


//
// Enhanced POD Wrappers for the public ACL-related CSSM structures
//
class AuthorizationGroup : public PodWrapper<AuthorizationGroup, CSSM_AUTHORIZATIONGROUP> {
public:
	AuthorizationGroup() { NumberOfAuthTags = 0; }	
	AuthorizationGroup(const AclAuthorizationSet &, Allocator &alloc);
	AuthorizationGroup(AclAuthorization tag, Allocator &alloc);
	void destroy(Allocator &alloc);
	
    bool empty() const			{ return NumberOfAuthTags == 0; }
	unsigned int size() const	{ return NumberOfAuthTags; }
	unsigned int count() const	{ return NumberOfAuthTags; }
	CSSM_ACL_AUTHORIZATION_TAG operator [] (unsigned ix) const
	{ assert(ix < size()); return AuthTags[ix]; }
	
	bool contains(CSSM_ACL_AUTHORIZATION_TAG tag) const;
	operator AclAuthorizationSet () const;
};

class AclOwnerPrototype;

class AclEntryPrototype : public PodWrapper<AclEntryPrototype, CSSM_ACL_ENTRY_PROTOTYPE> {
public:
	AclEntryPrototype() { clearPod(); }
	explicit AclEntryPrototype(const AclOwnerPrototype &proto);
	AclEntryPrototype(const CSSM_LIST &subj, bool delegate = false)
	{ clearPod(); TypedSubject = subj; Delegate = delegate; }
	
	TypedList &subject() { return TypedList::overlay(TypedSubject); }
	const TypedList &subject() const { return TypedList::overlay(TypedSubject); }

	bool delegate() const { return Delegate; }
	void delegate(bool d) { Delegate = d; }

	char *tag() { return EntryTag[0] ? EntryTag : NULL; }
	void tag(const char *tagString);
	void tag(const std::string &tagString);
	const char *tag() const { return EntryTag[0] ? EntryTag : NULL; }
	std::string s_tag() const { return EntryTag; }

	AuthorizationGroup &authorization() { return AuthorizationGroup::overlay(Authorization); }
	const AuthorizationGroup &authorization() const
	{ return AuthorizationGroup::overlay(Authorization); }
};

class AclOwnerPrototype : public PodWrapper<AclOwnerPrototype, CSSM_ACL_OWNER_PROTOTYPE> {
public:
	AclOwnerPrototype() { clearPod(); }
	explicit AclOwnerPrototype(const AclEntryPrototype &proto)
	{ TypedSubject = proto.subject(); delegate(proto.delegate()); }
	AclOwnerPrototype(const CSSM_LIST &subj, bool del = false)
	{ TypedSubject = subj; delegate(del); }
	
	TypedList &subject() { return TypedList::overlay(TypedSubject); }
	const TypedList &subject() const { return TypedList::overlay(TypedSubject); }
	bool delegate() const { return Delegate; }
	void delegate(bool d) { Delegate = d; }
};

class AclEntryInfo : public PodWrapper<AclEntryInfo, CSSM_ACL_ENTRY_INFO> {
public:
	AclEntryInfo() { clearPod(); }
	AclEntryInfo(const AclEntryPrototype &prot, CSSM_ACL_HANDLE h = 0)
	{ proto() = prot; handle() = h; }

	AclEntryPrototype &proto() { return AclEntryPrototype::overlay(EntryPublicInfo); }
	const AclEntryPrototype &proto() const
	{ return AclEntryPrototype::overlay(EntryPublicInfo); }
	
	operator AclEntryPrototype &() { return proto(); }
	operator const AclEntryPrototype &() const { return proto(); }

	CSSM_ACL_HANDLE &handle() { return EntryHandle; }
	const CSSM_ACL_HANDLE &handle() const { return EntryHandle; }
	void handle(CSSM_ACL_HANDLE h) { EntryHandle = h; }
};

class AclEntryInput : public PodWrapper<AclEntryInput, CSSM_ACL_ENTRY_INPUT> {
public:
	AclEntryInput() { clearPod(); }
	AclEntryInput(const CSSM_ACL_ENTRY_PROTOTYPE &prot)
	{ Prototype = prot; Callback = NULL; CallerContext = NULL; }

	AclEntryInput &operator = (const CSSM_ACL_ENTRY_PROTOTYPE &prot)
	{ Prototype = prot; Callback = NULL; CallerContext = NULL; return *this; }

	AclEntryPrototype &proto() { return AclEntryPrototype::overlay(Prototype); }
	const AclEntryPrototype &proto() const { return AclEntryPrototype::overlay(Prototype); }
	//@@@ not supporting callback features (yet)
};

class AclEdit : public PodWrapper<AclEdit, CSSM_ACL_EDIT> {
public:
	AclEdit(CSSM_ACL_EDIT_MODE m, CSSM_ACL_HANDLE h, const AclEntryInput *data)
	{ EditMode = m; OldEntryHandle = h; NewEntry = data; }
	AclEdit(const AclEntryInput &add)
	{ EditMode = CSSM_ACL_EDIT_MODE_ADD; OldEntryHandle = CSSM_INVALID_HANDLE; NewEntry = &add; }
	AclEdit(CSSM_ACL_HANDLE h, const AclEntryInput &modify)
	{ EditMode = CSSM_ACL_EDIT_MODE_REPLACE; OldEntryHandle = h; NewEntry = &modify; }
	AclEdit(CSSM_ACL_HANDLE h)
	{ EditMode = CSSM_ACL_EDIT_MODE_DELETE; OldEntryHandle = h; NewEntry = NULL; }
	
	CSSM_ACL_EDIT_MODE mode() const { return EditMode; }
	CSSM_ACL_HANDLE handle() const { return OldEntryHandle; }
	const AclEntryInput *newEntry() const { return AclEntryInput::overlay(NewEntry); }
};


//
// Allocating versions of Acl structures
//
class AutoAclOwnerPrototype {
	NOCOPY(AutoAclOwnerPrototype)
public:
	// allocator can be set after construction
	AutoAclOwnerPrototype(Allocator *allocator = NULL)
		: mAclOwnerPrototype(NULL), mAllocator(allocator) { }
	~AutoAclOwnerPrototype();
	
	operator bool () const					{ return mAllocator; }
	bool operator ! () const				{ return !mAllocator; }
	
	operator AclOwnerPrototype * ()			{ return make(); }
	operator AclOwnerPrototype & ()			{ return *make(); }
	AclOwnerPrototype &operator * ()		{ return *make(); }
	
	TypedList &subject()					{ return make()->subject(); }
	TypedList &subject() const
	{ assert(mAclOwnerPrototype); return mAclOwnerPrototype->subject(); }
	bool delegate() const
	{ assert(mAclOwnerPrototype); return mAclOwnerPrototype->delegate(); }
	void delegate(bool d)					{ make()->delegate(d); }

	void allocator(Allocator &allocator);
	Allocator &allocator() const { assert(mAllocator); return *mAllocator; }
	
	AclOwnerPrototype &operator = (const TypedList &subj)
	{ make()->subject() = subj; make()->delegate(false); return *mAclOwnerPrototype; }
	
	const AclOwnerPrototype *release()
	{ AclOwnerPrototype *r = mAclOwnerPrototype; mAclOwnerPrototype = NULL; return r; }
	
private:
	AclOwnerPrototype *mAclOwnerPrototype;
	Allocator *mAllocator;
	
	AclOwnerPrototype *make();
};


class AutoAclEntryInfoList {
	NOCOPY(AutoAclEntryInfoList)
public:
	// allocator can be set after construction
	AutoAclEntryInfoList(Allocator *allocator = NULL)
    : mEntries(NULL), mCount(0), mAllocator(allocator) { }
	~AutoAclEntryInfoList()							{ clear(); }

	operator bool () const							{ return mAllocator; }
	bool operator ! () const						{ return !mAllocator; }
	operator uint32 *()								{ return &mCount; }
	operator CSSM_ACL_ENTRY_INFO ** ()				{ return reinterpret_cast<CSSM_ACL_ENTRY_INFO **>(&mEntries); }

	void allocator(Allocator &allocator);
	Allocator &allocator() const { assert(mAllocator); return *mAllocator; }

	const AclEntryInfo &at(uint32 ix) const
	{ assert(ix < mCount); return mEntries[ix]; }
	const AclEntryInfo &operator [] (uint32 ix) const		{ return at(ix); }
	AclEntryInfo &at(uint32 ix);
	AclEntryInfo &operator[] (uint32 ix)					{ return at(ix); }

	uint32 size() const { return mCount; }
	uint32 count() const { return mCount; }
	AclEntryInfo *entries() const { return mEntries; }
	
	void clear();
	void size(uint32 newSize);
	
	// structured adders. Inputs must be chunk-allocated with our Allocator
	void add(const TypedList &subj, const AclAuthorizationSet &auths, const char *tag = NULL);
	void addPin(const TypedList &subj, uint32 slot);
	void addPinState(uint32 slot, uint32 state);
	void addPinState(uint32 slot, uint32 state, uint32 count);
	
	void release()		{ mAllocator = NULL; }

private:
	AclEntryInfo *mEntries;
	uint32 mCount;
	Allocator *mAllocator;
};

//
// Extract the pin number from a "PIN%d?" tag.
// Returns 0 if the tag isn't of that form.
//
uint32 pinFromAclTag(const char *tag, const char *suffix = NULL);


class AutoAuthorizationGroup : public AuthorizationGroup {
public:
	AutoAuthorizationGroup(Allocator &alloc) : allocator(alloc) { }
	explicit AutoAuthorizationGroup(const AclAuthorizationSet &set,
		Allocator &alloc) : AuthorizationGroup(set, alloc), allocator(alloc) { }
	~AutoAuthorizationGroup()	{ destroy(allocator); }

	Allocator &allocator;
};


//
// Walkers for the CSSM API structure types
//
namespace DataWalkers {

// AclEntryInput
template <class Action>
AclEntryInput *walk(Action &operate, AclEntryInput * &input)
{
	operate(input);
	walk(operate, input->proto());
	return input;
}

template <class Action>
void walk(Action &operate, AclEntryInput &input)
{
	operate(input);
	walk(operate, input.proto());
}

// AclEntryInfo
template <class Action>
void walk(Action &operate, AclEntryInfo &info)
{
	operate(info);
	walk(operate, info.proto());
}

// AuthorizationGroup
template <class Action>
void walk(Action &operate, AuthorizationGroup &auth)
{
	operate(auth);
	uint32 count = auth.count();
	operate.blob(auth.AuthTags, count * sizeof(auth.AuthTags[0]));
	for (uint32 n = 0; n < count; n++)
		operate(auth.AuthTags[n]);
}

template <class Action>
void walk(Action &operate, CSSM_AUTHORIZATIONGROUP &auth)
{ walk(operate, static_cast<CSSM_AUTHORIZATIONGROUP &>(auth)); }

// AclEntryPrototype
template <class Action>
void enumerate(Action &operate, AclEntryPrototype &proto)
{
	walk(operate, proto.subject());
	walk(operate, proto.authorization());
	//@@@ ignoring validity period
}

template <class Action>
void walk(Action &operate, AclEntryPrototype &proto)
{
	operate(proto);
	enumerate(operate, proto);
}

template <class Action>
AclEntryPrototype *walk(Action &operate, AclEntryPrototype * &proto)
{
	operate(proto);
	enumerate(operate, *proto);
	return proto;
}

// AclOwnerPrototype
template <class Action>
void walk(Action &operate, AclOwnerPrototype &proto)
{
	operate(proto);
	walk(operate, proto.subject());
}

template <class Action>
AclOwnerPrototype *walk(Action &operate, AclOwnerPrototype * &proto)
{
	operate(proto);
	walk(operate, proto->subject());
	return proto;
}


} // end namespace DataWalkers

} // end namespace Security


#endif //_CSSMACLPOD
