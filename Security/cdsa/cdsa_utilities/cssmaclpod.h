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


//
// cssmaclpod - enhanced PodWrappers for ACL-related CSSM data structures
//
#ifndef _CSSMACLPOD
#define _CSSMACLPOD

#include <Security/utilities.h>
#include <Security/cssmlist.h>
#include <Security/cssmalloc.h>

namespace Security {

// a nicer name for an authorization tag
typedef CSSM_ACL_AUTHORIZATION_TAG AclAuthorization;
typedef std::set<AclAuthorization> AclAuthorizationSet;


//
// Enhanced POD Wrappers for the public ACL-related CSSM structures
//
class AuthorizationGroup : public PodWrapper<AuthorizationGroup, CSSM_AUTHORIZATIONGROUP> {
public:
	AuthorizationGroup() { NumberOfAuthTags = 0; }
	AuthorizationGroup(AclAuthorization auth);
	
	explicit AuthorizationGroup(const AclAuthorizationSet &, CssmAllocator &alloc);
	void destroy(CssmAllocator &alloc);
	
    bool empty() const			{ return NumberOfAuthTags == 0; }
	unsigned int count() const	{ return NumberOfAuthTags; }
	CSSM_ACL_AUTHORIZATION_TAG operator [] (unsigned ix) const
	{ assert(ix < count()); return AuthTags[ix]; }
	
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
	char *tag() { return EntryTag; }
	const char *tag() const { return EntryTag; }
	void tag(const char *tagString);
	AuthorizationGroup &authorization() { return AuthorizationGroup::overlay(Authorization); }
	const AuthorizationGroup &authorization() const
	{ return AuthorizationGroup::overlay(Authorization); }
};

class AclOwnerPrototype : public PodWrapper<AclOwnerPrototype, CSSM_ACL_OWNER_PROTOTYPE> {
public:
	AclOwnerPrototype() { clearPod(); }
	explicit AclOwnerPrototype(const AclEntryPrototype &proto)
	{ TypedSubject = proto.subject(); Delegate = proto.delegate(); }
	AclOwnerPrototype(const CSSM_LIST &subj, bool delegate = false)
	{ TypedSubject = subj; Delegate = delegate; }
	
	TypedList &subject() { return TypedList::overlay(TypedSubject); }
	const TypedList &subject() const { return TypedList::overlay(TypedSubject); }
	bool delegate() const { return Delegate; }
};

class AclEntryInfo : public PodWrapper<AclEntryInfo, CSSM_ACL_ENTRY_INFO> {
public:
	AclEntryPrototype &proto() { return AclEntryPrototype::overlay(EntryPublicInfo); }
	const AclEntryPrototype &proto() const
	{ return AclEntryPrototype::overlay(EntryPublicInfo); }
	
	operator AclEntryPrototype &() { return proto(); }
	operator const AclEntryPrototype &() const { return proto(); }

	CSSM_ACL_HANDLE &handle() { return EntryHandle; }
	const CSSM_ACL_HANDLE &handle() const { return EntryHandle; }
};

class AclEntryInput : public PodWrapper<AclEntryInput, CSSM_ACL_ENTRY_INPUT> {
public:
	AclEntryInput() { clearPod(); }
	AclEntryInput(const AclEntryPrototype &prot)
	{ Prototype = prot; Callback = NULL; CallerContext = NULL; }

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
	AutoAclOwnerPrototype(CssmAllocator *allocator = NULL)
		: mAclOwnerPrototype(NULL), mAllocator(allocator) { }
	~AutoAclOwnerPrototype();
	
	operator CSSM_ACL_OWNER_PROTOTYPE *()	{ return make(); }
	AclOwnerPrototype &operator * ()		{ return *make(); }

	void allocator(CssmAllocator &allocator);

private:
	AclOwnerPrototype *mAclOwnerPrototype;
	CssmAllocator *mAllocator;
	
	AclOwnerPrototype *make();
};


class AutoAclEntryInfoList {
	NOCOPY(AutoAclEntryInfoList)
public:
	// allocator can be set after construction
	AutoAclEntryInfoList(CssmAllocator *allocator = NULL)
    : mAclEntryInfo(NULL), mNumberOfAclEntries(0), mAllocator(allocator) { }
	~AutoAclEntryInfoList();

	operator CSSM_ACL_ENTRY_INFO_PTR *()
	{ return reinterpret_cast<CSSM_ACL_ENTRY_INFO_PTR *>(&mAclEntryInfo); }
	operator uint32 *() { return &mNumberOfAclEntries; }

	void allocator(CssmAllocator &allocator);

	const AclEntryInfo &at(uint32 ix) const { return mAclEntryInfo[ix]; }
	const AclEntryInfo &operator[](uint32 ix) const
	{ assert(ix < mNumberOfAclEntries); return mAclEntryInfo[ix]; }
	AclEntryInfo &operator[](uint32 ix)
	{ assert(ix < mNumberOfAclEntries); return mAclEntryInfo[ix]; }

	uint32 size() const { return mNumberOfAclEntries; }	// obsolete
	uint32 count() const { return mNumberOfAclEntries; }
	AclEntryInfo *entries() const { return mAclEntryInfo; }

private:
	AclEntryInfo *mAclEntryInfo;
	uint32 mNumberOfAclEntries;
	CssmAllocator *mAllocator;
};

class AutoAuthorizationGroup : public AuthorizationGroup {
public:
	AutoAuthorizationGroup(CssmAllocator &alloc) : allocator(alloc) { }
	explicit AutoAuthorizationGroup(const AclAuthorizationSet &set,
		CssmAllocator &alloc) : AuthorizationGroup(set, alloc), allocator(alloc) { }
	~AutoAuthorizationGroup()	{ destroy(allocator); }

	CssmAllocator &allocator;
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

template <class Action>
void walk(Action &operate, const AclEntryInfo &info)
{ walk(operate, const_cast<AclEntryInfo &>(info)); }

// AuthorizationGroup
template <class Action>
void walk(Action &operate, AuthorizationGroup &auth)
{
	operate(auth);
	uint32 count = auth.count();
	operate.blob(auth.AuthTags, count * sizeof(AclAuthorization));
	for (uint32 n = 0; n < count; n++)
		walk(operate, auth.AuthTags[n]);
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
