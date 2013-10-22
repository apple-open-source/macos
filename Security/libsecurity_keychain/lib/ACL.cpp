/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
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
// ACL.cpp
//
#include <security_keychain/ACL.h>
#include <security_keychain/SecCFTypes.h>
#include <security_utilities/osxcode.h>
#include <security_utilities/trackingallocator.h>
#include <security_cdsa_utilities/walkers.h>
#include <security_keychain/TrustedApplication.h>
#include <Security/SecTrustedApplication.h>
#include <security_utilities/devrandom.h>
#include <security_cdsa_utilities/uniformrandom.h>
#include <memory>


using namespace KeychainCore;
using namespace DataWalkers;


//
// The default form of a prompt selector
//
const CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR ACL::defaultSelector = {
	CSSM_ACL_KEYCHAIN_PROMPT_CURRENT_VERSION, 0
};


//
// ACL static constants
//
const CSSM_ACL_HANDLE ACL::ownerHandle;


//
// Create an ACL object from the result of a CSSM ACL query
//
ACL::ACL(Access &acc, const AclEntryInfo &info, Allocator &alloc)
	: allocator(alloc), access(acc), mState(unchanged), mSubjectForm(NULL), mMutex(Mutex::recursive)
{
	// parse the subject
	parse(info.proto().subject());
	
	// fill in AclEntryInfo layer information
	const AclEntryPrototype &proto = info.proto();
	mAuthorizations = proto.authorization();
	mDelegate = proto.delegate();
	mEntryTag = proto.s_tag();

	// take CSSM entry handle from info layer
	mCssmHandle = info.handle();
}

ACL::ACL(Access &acc, const AclOwnerPrototype &owner, Allocator &alloc)
	: allocator(alloc), access(acc), mState(unchanged), mSubjectForm(NULL), mMutex(Mutex::recursive)
{
	// parse subject
	parse(owner.subject());
	
	// for an owner "entry", the next-layer information is fixed (and fake)
	mAuthorizations.insert(CSSM_ACL_AUTHORIZATION_CHANGE_ACL);
	mDelegate = owner.delegate();
	mEntryTag[0] = '\0';

	// use fixed (fake) entry handle
	mCssmHandle = ownerHandle;
}


//
// Create a new ACL that authorizes anyone to do anything.
// This constructor produces a "pure" ANY ACL, without descriptor or selector.
// To generate a "standard" form of ANY, use the appListForm constructor below,
// then change its form to allowAnyForm.
//
ACL::ACL(Access &acc, Allocator &alloc)
	: allocator(alloc), access(acc), mSubjectForm(NULL), mMutex(Mutex::recursive)
{
	mState = inserted;		// new
	mForm = allowAllForm;	// everybody
	mAuthorizations.insert(CSSM_ACL_AUTHORIZATION_ANY);	// anything
	mDelegate = false;
	
	//mPromptDescription stays empty
	mPromptSelector = defaultSelector;
	
	// randomize the CSSM handle
	UniformRandomBlobs<DevRandomGenerator>().random(mCssmHandle);
}


//
// Create a new ACL in standard form.
// As created, it authorizes all activities.
//
ACL::ACL(Access &acc, string description, const CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR &promptSelector,
		Allocator &alloc)
	: allocator(alloc), access(acc), mSubjectForm(NULL), mMutex(Mutex::recursive)
{
	mState = inserted;		// new
	mForm = appListForm;
	mAuthorizations.insert(CSSM_ACL_AUTHORIZATION_ANY);	// anything
	mDelegate = false;
	
	mPromptDescription = description;
	mPromptSelector = promptSelector;
	
	// randomize the CSSM handle
	UniformRandomBlobs<DevRandomGenerator>().random(mCssmHandle);
}


//
// Destroy an ACL
//
ACL::~ACL() 
{
	// release subject form (if any)
	chunkFree(mSubjectForm, allocator);
}


//
// Does this ACL authorize a particular right?
//
bool ACL::authorizes(AclAuthorization right)
{
	StLock<Mutex>_(mMutex);
	return mAuthorizations.find(right) != mAuthorizations.end()
		|| mAuthorizations.find(CSSM_ACL_AUTHORIZATION_ANY) != mAuthorizations.end()
		|| mAuthorizations.empty();
}


//
// Add an application to the trusted-app list of this ACL.
// Will fail unless this is a standard "simple" form ACL.
//
void ACL::addApplication(TrustedApplication *app)
{
	StLock<Mutex>_(mMutex);
	switch (mForm) {
	case appListForm:	// simple...
		mAppList.push_back(app);
		modify();
		break;
	case allowAllForm:	// hmm...
		if (!mPromptDescription.empty()) {
			// verbose "any" form (has description, "any" override)
			mAppList.push_back(app);
			modify();
			break;
		}
		// pure "any" form without description. Cannot convert to appListForm	
	default:
		MacOSError::throwMe(errSecACLNotSimple);
	}
}


//
// Mark an ACL as modified.
//
void ACL::modify()
{
	StLock<Mutex>_(mMutex);
	if (mState == unchanged) {
		secdebug("SecAccess", "ACL %p marked modified", this);
		mState = modified;
	}
}


//
// Mark an ACL as "removed"
// Removed ACLs have no valid contents (they are invalid on their face).
// When "updated" to the originating item, they will cause the corresponding
// ACL entry to be deleted. Otherwise, they are irrelevant.
// Note: Removing an ACL does not actually remove it from its Access's map.
//
void ACL::remove()
{
	StLock<Mutex>_(mMutex);
	mAppList.clear();
	mForm = invalidForm;
	mState = deleted;
}


//
// Produce CSSM-layer form (ACL prototype) copies of our content.
// Note that the result is chunk-allocated, and becomes owned by the caller.
//
void ACL::copyAclEntry(AclEntryPrototype &proto, Allocator &alloc)
{
	StLock<Mutex>_(mMutex);
	proto.clearPod();	// preset
	
	// carefully copy the subject
	makeSubject();
	assert(mSubjectForm);
	proto = AclEntryPrototype(*mSubjectForm, mDelegate);	// shares subject
	ChunkCopyWalker w(alloc);
	walk(w, proto.subject());	// copy subject in-place
	
	// the rest of a prototype
	proto.tag(mEntryTag);
	AuthorizationGroup tags(mAuthorizations, allocator);
	proto.authorization() = tags;
}

void ACL::copyAclOwner(AclOwnerPrototype &proto, Allocator &alloc)
{
	StLock<Mutex>_(mMutex);
	proto.clearPod();
	
	makeSubject();
	assert(mSubjectForm);
	proto = AclOwnerPrototype(*mSubjectForm, mDelegate);	// shares subject
	ChunkCopyWalker w(alloc);
	walk(w, proto.subject());	// copy subject in-place
}


//
// (Re)place this ACL's setting into the AclBearer specified.
// If update, assume this is an update operation and the ACL was
// originally derived from this object; specifically, assume the
// CSSM handle is valid. If not update, assume this is a different
// object that has no related ACL entry (yet).
//
void ACL::setAccess(AclBearer &target, bool update,
	const AccessCredentials *cred)
{
	StLock<Mutex>_(mMutex);
	// determine what action we need to perform
	State action = state();
	if (!update)
		action = (action == deleted) ? unchanged : inserted;
	
	// the owner acl (pseudo) "entry" is a special case
	if (isOwner()) {
		switch (action) {
		case unchanged:
			secdebug("SecAccess", "ACL %p owner unchanged", this);
			return;
		case inserted:		// means modify the initial owner
		case modified:
			{
				secdebug("SecAccess", "ACL %p owner modified", this);
				makeSubject();
				assert(mSubjectForm);
				AclOwnerPrototype proto(*mSubjectForm, mDelegate);
				target.changeOwner(proto, cred);
				return;
			}
		default:
			assert(false);
			return;
		}
	}

	// simple cases
	switch (action) {
	case unchanged:	// ignore
		secdebug("SecAccess", "ACL %p handle 0x%lx unchanged", this, entryHandle());
		return;
	case deleted:	// delete
		secdebug("SecAccess", "ACL %p handle 0x%lx deleted", this, entryHandle());
		target.deleteAcl(entryHandle(), cred);
		return;
	default:
		break;
	}
	
	// build the byzantine data structures that CSSM loves so much
	makeSubject();
	assert(mSubjectForm);
	AclEntryPrototype proto(*mSubjectForm, mDelegate);
	proto.tag(mEntryTag);
	AutoAuthorizationGroup tags(mAuthorizations, allocator);
	proto.authorization() = tags;
	AclEntryInput input(proto);
	switch (action) {
	case inserted:	// insert
		secdebug("SecAccess", "ACL %p inserted", this);
		target.addAcl(input, cred);
		break;
	case modified:	// update
		secdebug("SecAccess", "ACL %p handle 0x%lx modified", this, entryHandle());
		target.changeAcl(entryHandle(), input, cred);
		break;
	default:
		assert(false);
	}
}


//
// Parse an AclEntryPrototype (presumably from a CSSM "Get" ACL operation
// into internal form.
//
void ACL::parse(const TypedList &subject)
{
	StLock<Mutex>_(mMutex);
	try {
		switch (subject.type()) {
		case CSSM_ACL_SUBJECT_TYPE_ANY:
			// subsume an "any" as a standard form
			mForm = allowAllForm;
			return;
		case CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT:
			// pure keychain prompt - interpret as applist form with no apps
			parsePrompt(subject);
			mForm = appListForm;
			return;
		case CSSM_ACL_SUBJECT_TYPE_THRESHOLD:
			{
				// app-list format: THRESHOLD(1, n): sign(1), ..., sign(n), PROMPT
				if (subject[1] != 1)
					throw ParseError();
				uint32 count = subject[2];
				
				// parse final (PROMPT) element
				TypedList &end = subject[count + 2];	// last choice
				if (end.type() != CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT)
					throw ParseError();	// not PROMPT at end
				parsePrompt(end);
				
				// check for leading ANY
				TypedList &first = subject[3];
				if (first.type() == CSSM_ACL_SUBJECT_TYPE_ANY) {
					mForm = allowAllForm;
					return;
				}
				
				// parse other (code signing) elements
				for (uint32 n = 0; n < count - 1; n++)
					mAppList.push_back(new TrustedApplication(TypedList(subject[n + 3].list())));
			}
			mForm = appListForm;
			return;
		default:
			mForm = customForm;
			mSubjectForm = chunkCopy(&subject);
			return;
		}
	} catch (const ParseError &) {
		secdebug("SecAccess", "acl compile failed; marking custom");
		mForm = customForm;
		mSubjectForm = chunkCopy(&subject);
		mAppList.clear();
	}
}

void ACL::parsePrompt(const TypedList &subject)
{
	StLock<Mutex>_(mMutex);
	assert(subject.length() == 3);
	mPromptSelector =
		*subject[1].data().interpretedAs<CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR>(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
	mPromptDescription = subject[2].toString();
}


//
// Take this ACL and produce its meaning as a CSSM ACL subject in mSubjectForm
//
void ACL::makeSubject()
{
	StLock<Mutex>_(mMutex);
	switch (form()) {
	case allowAllForm:
		chunkFree(mSubjectForm, allocator);	// release previous
		if (mPromptDescription.empty()) {
			// no description -> pure ANY
			mSubjectForm = new(allocator) TypedList(allocator, CSSM_ACL_SUBJECT_TYPE_ANY);
		} else {
			// have description -> threshold(1 of 2) of { ANY, PROMPT }
			mSubjectForm = new(allocator) TypedList(allocator, CSSM_ACL_SUBJECT_TYPE_THRESHOLD,
				new(allocator) ListElement(1),
				new(allocator) ListElement(2));
			*mSubjectForm += new(allocator) ListElement(TypedList(allocator, CSSM_ACL_SUBJECT_TYPE_ANY));
			TypedList prompt(allocator, CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT,
				new(allocator) ListElement(allocator, CssmData::wrap(mPromptSelector)),
				new(allocator) ListElement(allocator, mPromptDescription));
			*mSubjectForm += new(allocator) ListElement(prompt);
		}
		return;
	case appListForm: {
		// threshold(1 of n+1) of { app1, ..., appn, PROMPT }
		chunkFree(mSubjectForm, allocator);	// release previous
		uint32 appCount = (uint32)mAppList.size();
		mSubjectForm = new(allocator) TypedList(allocator, CSSM_ACL_SUBJECT_TYPE_THRESHOLD,
			new(allocator) ListElement(1),
			new(allocator) ListElement(appCount + 1));
		for (uint32 n = 0; n < appCount; n++)
			*mSubjectForm +=
				new(allocator) ListElement(mAppList[n]->makeSubject(allocator));
		TypedList prompt(allocator, CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT,
			new(allocator) ListElement(allocator, CssmData::wrap(mPromptSelector)),
			new(allocator) ListElement(allocator, mPromptDescription));
		*mSubjectForm += new(allocator) ListElement(prompt);
		}
		return;
	case customForm:
		assert(mSubjectForm);	// already set; keep it
		return;
	default:
		assert(false);	// unexpected
	}
}
