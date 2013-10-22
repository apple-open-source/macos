/*
 * Copyright (c) 2000-2004,2006-2007 Apple Inc. All Rights Reserved.
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
#include <security_cdsa_utilities/cssmaclpod.h>
#include <security_cdsa_utilities/cssmwalkers.h>
#include <cstdarg>


namespace Security {


//
// AclAuthorizationSets
//
AclAuthorizationSet::AclAuthorizationSet(AclAuthorization auth0, AclAuthorization auth, ...)
{
	insert(auth0);

	va_list args;
	va_start(args, auth);
	while (auth) {
		insert(auth);
		auth = va_arg(args, AclAuthorization);
	}
	va_end(args);
}


//
// AclAuthorizationGroups
//
AuthorizationGroup::AuthorizationGroup(const AclAuthorizationSet &auths, Allocator &alloc)
{
	NumberOfAuthTags = (uint32)auths.size();
	AuthTags = alloc.alloc<CSSM_ACL_AUTHORIZATION_TAG>(NumberOfAuthTags);
	copy(auths.begin(), auths.end(), AuthTags);	// happens to be sorted
}

AuthorizationGroup::AuthorizationGroup(CSSM_ACL_AUTHORIZATION_TAG tag, Allocator &alloc)
{
	AuthTags = alloc.alloc<CSSM_ACL_AUTHORIZATION_TAG>(1);
	AuthTags[0] = tag;
	NumberOfAuthTags = 1;
}

void AuthorizationGroup::destroy(Allocator &alloc)
{
	alloc.free(AuthTags);
}

bool AuthorizationGroup::contains(CSSM_ACL_AUTHORIZATION_TAG tag) const
{
	return find(AuthTags, &AuthTags[NumberOfAuthTags], tag) != &AuthTags[NumberOfAuthTags];
}


AuthorizationGroup::operator AclAuthorizationSet() const
{
	return AclAuthorizationSet(AuthTags, &AuthTags[NumberOfAuthTags]);
}

AclEntryPrototype::AclEntryPrototype(const AclOwnerPrototype &proto)
{
	memset(this, 0, sizeof(*this));
	TypedSubject = proto.subject(); Delegate = proto.delegate();
	//@@@ set authorization to "is owner" pseudo-auth? See cssmacl.h
}

void AclEntryPrototype::tag(const char *tagString)
{
	if (tagString == NULL)
		EntryTag[0] = '\0';
	else if (strlen(tagString) > CSSM_MODULE_STRING_SIZE)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_ENTRY_TAG);
	else
		strcpy(EntryTag, tagString);
}

void AclEntryPrototype::tag(const string &tagString)
{
	if (tagString.length() > CSSM_MODULE_STRING_SIZE)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_ENTRY_TAG);
	else
		memcpy(EntryTag, tagString.c_str(), tagString.length() + 1);
}


AclOwnerPrototype *AutoAclOwnerPrototype::make()
{
	if (!mAclOwnerPrototype) {
		mAclOwnerPrototype = (AclOwnerPrototype*) mAllocator->malloc(sizeof(AclOwnerPrototype));
        new (mAclOwnerPrototype) AclOwnerPrototype;
		mAclOwnerPrototype->clearPod();
	}
	return mAclOwnerPrototype;
}

AutoAclOwnerPrototype::~AutoAclOwnerPrototype()
{
	if (mAllocator)
		DataWalkers::chunkFree(mAclOwnerPrototype, *mAllocator);
}

void
AutoAclOwnerPrototype::allocator(Allocator &allocator)
{
	mAllocator = &allocator;
}


void AutoAclEntryInfoList::size(uint32 newSize)
{
	assert(mAllocator);
	mEntries = mAllocator->alloc<AclEntryInfo>(mEntries, newSize);
	for (uint32 n = mCount; n < newSize; n++)
		mEntries[n].clearPod();
	mCount = newSize;
}


AclEntryInfo &AutoAclEntryInfoList::at(uint32 ix)
{
	if (ix >= mCount)
		size(ix + 1);	// expand vector
	return mEntries[ix];
}


void AutoAclEntryInfoList::clear()
{
	if (mAllocator)
	{
		DataWalkers::ChunkFreeWalker w(*mAllocator);
		for (uint32 ix = 0; ix < mCount; ix++)
			walk(w, mEntries[ix]);
		mAllocator->free(mEntries);
		mEntries = NULL;
		mCount = 0;
	}
}

void AutoAclEntryInfoList::allocator(Allocator &allocator)
{
	mAllocator = &allocator;
}


void AutoAclEntryInfoList::add(const TypedList &subj, const AclAuthorizationSet &auths, const char *tag /* = NULL */)
{
	AclEntryInfo &info = at(size());
	info.proto() = AclEntryPrototype(subj);
	info.proto().authorization() = AuthorizationGroup(auths, allocator());
	info.proto().tag(tag);
	info.handle(size());
}

void AutoAclEntryInfoList::addPin(const TypedList &subj, uint32 slot)
{
	char tag[20];
	snprintf(tag, sizeof(tag), "PIN%d", slot);
	add(subj, CSSM_ACL_AUTHORIZATION_PREAUTH(slot), tag);
}

void AutoAclEntryInfoList::addPinState(uint32 slot, uint32 status)
{
	char tag[20];
	snprintf(tag, sizeof(tag), "PIN%d?", slot);
	TypedList subj(allocator(), CSSM_WORDID_PIN,
		new(allocator()) ListElement(slot),
		new(allocator()) ListElement(status));
	add(subj, CSSM_WORDID_PIN, tag);
}

void AutoAclEntryInfoList::addPinState(uint32 slot, uint32 status, uint32 count)
{
	char tag[20];
	snprintf(tag, sizeof(tag), "PIN%d?", slot);
	TypedList subj(allocator(), CSSM_WORDID_PIN,
		new(allocator()) ListElement(slot),
		new(allocator()) ListElement(status),
		new(allocator()) ListElement(count));
	add(subj, CSSM_WORDID_PIN, tag);
}

uint32 pinFromAclTag(const char *tag, const char *suffix /* = NULL */)
{
	if (tag) {
		char format[20];
		snprintf(format, sizeof(format), "PIN%%d%s%%n", suffix ? suffix : "");
		uint32 pin;
		unsigned consumed;
		sscanf(tag, format, &pin, &consumed);
		if (consumed == strlen(tag))	// complete and sufficient
			return pin;
	}
	return 0;
}

}	// namespace Security
