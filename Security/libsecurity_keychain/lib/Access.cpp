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
// Access.cpp
//
#include <security_keychain/Access.h>
#include <Security/SecBase.h>
#include "SecBridge.h"
#include <security_utilities/devrandom.h>
#include <security_cdsa_utilities/uniformrandom.h>
#include <security_cdsa_client/aclclient.h>
#include <vector>
#include <SecBase.h>
using namespace KeychainCore;
using namespace CssmClient;


//
// Access static constants
//
const CSSM_ACL_HANDLE Access::ownerHandle;


//
// Create a completely open Access (anyone can do anything)
// Note that this means anyone can *change* the ACL at will, too.
// These ACL entries contain no descriptor names.
//
Access::Access() : mMutex(Mutex::recursive)
{
	SecPointer<ACL> owner = new ACL(*this);
	owner->setAuthorization(CSSM_ACL_AUTHORIZATION_CHANGE_ACL);
	addOwner(owner);
	
	SecPointer<ACL> any = new ACL(*this);
	add(any);
}


//
// Create a default Access object.
// This construct an Access with "default form", whatever that happens to be
// in this release.
//
Access::Access(const string &descriptor, const ACL::ApplicationList &trusted) : mMutex(Mutex::recursive)
{
	makeStandard(descriptor, trusted);
}

Access::Access(const string &descriptor) : mMutex(Mutex::recursive)
{
	ACL::ApplicationList trusted;
	trusted.push_back(new TrustedApplication);
	makeStandard(descriptor, trusted);
}

Access::Access(const string &descriptor, const ACL::ApplicationList &trusted,
	const AclAuthorizationSet &limitedRights, const AclAuthorizationSet &freeRights) : mMutex(Mutex::recursive)
{
	makeStandard(descriptor, trusted, limitedRights, freeRights);
}

void Access::makeStandard(const string &descriptor, const ACL::ApplicationList &trusted,
	const AclAuthorizationSet &limitedRights, const AclAuthorizationSet &freeRights)
{
	StLock<Mutex>_(mMutex);

	// owner "entry"
	SecPointer<ACL> owner = new ACL(*this, descriptor, ACL::defaultSelector);
	owner->setAuthorization(CSSM_ACL_AUTHORIZATION_CHANGE_ACL);
	addOwner(owner);

	// unlimited entry
	SecPointer<ACL> unlimited = new ACL(*this, descriptor, ACL::defaultSelector);
	if (freeRights.empty()) {
		unlimited->authorizations().clear();
		unlimited->authorizations().insert(CSSM_ACL_AUTHORIZATION_ENCRYPT);
	} else
		unlimited->authorizations() = freeRights;
	unlimited->form(ACL::allowAllForm);
	add(unlimited);

	// limited entry
	SecPointer<ACL> limited = new ACL(*this, descriptor, ACL::defaultSelector);
	if (limitedRights.empty()) {
		limited->authorizations().clear();
		limited->authorizations().insert(CSSM_ACL_AUTHORIZATION_DECRYPT);
		limited->authorizations().insert(CSSM_ACL_AUTHORIZATION_SIGN);
		limited->authorizations().insert(CSSM_ACL_AUTHORIZATION_MAC);
		limited->authorizations().insert(CSSM_ACL_AUTHORIZATION_DERIVE);
		limited->authorizations().insert(CSSM_ACL_AUTHORIZATION_EXPORT_CLEAR);
		limited->authorizations().insert(CSSM_ACL_AUTHORIZATION_EXPORT_WRAPPED);
	} else
		limited->authorizations() = limitedRights;
	limited->applications() = trusted;
	add(limited);
}


//
// Create an Access object whose initial value is taken
// from a CSSM ACL bearing object.
//
Access::Access(AclBearer &source) : mMutex(Mutex::recursive)
{
	// retrieve and set
	AutoAclOwnerPrototype owner;
	source.getOwner(owner);
	AutoAclEntryInfoList acls;
	source.getAcl(acls);
	compile(*owner, acls.count(), acls.entries());
}


//
// Create an Access object from CSSM-layer access controls
//
Access::Access(const CSSM_ACL_OWNER_PROTOTYPE &owner,
	uint32 aclCount, const CSSM_ACL_ENTRY_INFO *acls) : mMutex(Mutex::recursive)
{
	compile(owner, aclCount, acls);
}


Access::~Access() 
{
}


// Convert a SecPointer to a SecACLRef.
static SecACLRef
convert(const SecPointer<ACL> &acl)
{
	return *acl;
}

//
// Return all ACL components in a newly-made CFArray.
//
CFArrayRef Access::copySecACLs() const
{
	return makeCFArray(convert, mAcls);
}

CFArrayRef Access::copySecACLs(CSSM_ACL_AUTHORIZATION_TAG action) const
{
	list<ACL *> choices;
	for (Map::const_iterator it = mAcls.begin(); it != mAcls.end(); it++)
		if (it->second->authorizes(action))
			choices.push_back(it->second);
	return choices.empty() ? NULL : makeCFArray(convert, choices);
}


//
// Enter the complete access configuration into a AclBearer.
// If update, skip any part marked unchanged. (If not update, skip
// any part marked deleted.)
//
void Access::setAccess(AclBearer &target, bool update /* = false */)
{
	StLock<Mutex>_(mMutex);
	AclFactory factory;
	editAccess(target, update, factory.promptCred());
}

void Access::setAccess(AclBearer &target, Maker &maker)
{
	StLock<Mutex>_(mMutex);
	if (maker.makerType() == Maker::kStandardMakerType)
	{
		// remove initial-setup ACL
		target.deleteAcl(Maker::creationEntryTag, maker.cred());
		
		// insert our own ACL entries
		editAccess(target, false, maker.cred());
	}
}

void Access::editAccess(AclBearer &target, bool update, const AccessCredentials *cred)
{
	StLock<Mutex>_(mMutex);
	assert(mAcls[ownerHandle]);	// have owner
	
	// apply all non-owner ACLs first
	for (Map::iterator it = mAcls.begin(); it != mAcls.end(); it++)
		if (!it->second->isOwner())
			it->second->setAccess(target, update, cred);
	
	// finally, apply owner
	mAcls[ownerHandle]->setAccess(target, update, cred);
}


//
// A convenience function to add one application to a standard ("simple") form
// ACL entry. This will only work if
//  -- there is exactly one ACL entry authorizing the right
//  -- that entry is in simple form
//
void Access::addApplicationToRight(AclAuthorization right, TrustedApplication *app)
{
	StLock<Mutex>_(mMutex);
	vector<ACL *> acls;
	findAclsForRight(right, acls);
	if (acls.size() != 1)
		MacOSError::throwMe(errSecACLNotSimple);	// let's not guess here...
	(*acls.begin())->addApplication(app);
}


//
// Yield new (copied) CSSM level owner and acls values, presumably
// for use at CSSM layer operations.
// Caller is responsible for releasing the beasties when done.
//
void Access::copyOwnerAndAcl(CSSM_ACL_OWNER_PROTOTYPE * &ownerResult,
	uint32 &aclCount, CSSM_ACL_ENTRY_INFO * &aclsResult)
{
	StLock<Mutex>_(mMutex);
	Allocator& alloc = Allocator::standard();
	unsigned long count = mAcls.size() - 1;	// one will be owner, others are acls
	AclOwnerPrototype owner;
	CssmAutoPtr<AclEntryInfo> acls = new(alloc) AclEntryInfo[count];
	AclEntryInfo *aclp = acls;	// -> next unfilled acl element
	for (Map::const_iterator it = mAcls.begin(); it != mAcls.end(); it++) {
		SecPointer<ACL> acl = it->second;
		if (acl->isOwner()) {
			acl->copyAclOwner(owner, alloc);
		} else {
			aclp->handle() = acl->entryHandle();
			acl->copyAclEntry(*aclp, alloc);
			++aclp;
		}
	}
	assert((aclp - acls) == count);	// all ACL elements filled

	// commit output
	ownerResult = new(alloc) AclOwnerPrototype(owner);
	aclCount = (uint32)count;
	aclsResult = acls.release();
}


//
// Retrieve the description from a randomly chosen ACL within this Access.
// In the conventional case where all ACLs have the same descriptor, this
// is deterministic. But you have been warned.
//
string Access::promptDescription() const
{
	for (Map::const_iterator it = mAcls.begin(); it != mAcls.end(); it++) {
		ACL *acl = it->second;
		switch (acl->form()) {
		case ACL::allowAllForm:
		case ACL::appListForm:
			{
				string descr = acl->promptDescription();
				if (!descr.empty())
					return descr;
			}
		default:
			break;
		}
	}
	// couldn't find suitable ACL (no description anywhere)
	CssmError::throwMe(errSecACLNotSimple);
}


//
// Add a new ACL to the resident set. The ACL must have been
// newly made for this Access.
//
void Access::add(ACL *newAcl)
{
	StLock<Mutex>_(mMutex);
	if (&newAcl->access != this)
		MacOSError::throwMe(errSecParam);
	assert(!mAcls[newAcl->entryHandle()]);
	mAcls[newAcl->entryHandle()] = newAcl;
}


//
// Add the owner ACL to the resident set. The ACL must have been
// newly made for this Access.
// Since an Access must have exactly one owner ACL, this call
// should only be made (exactly once) for a newly created Access.
//
void Access::addOwner(ACL *newAcl)
{
	StLock<Mutex>_(mMutex);
	newAcl->makeOwner();
	assert(mAcls.find(ownerHandle) == mAcls.end());	// no owner yet
	add(newAcl);
}


//
// Compile a set of ACL entries and owner into internal form.
//
void Access::compile(const CSSM_ACL_OWNER_PROTOTYPE &owner,
	uint32 aclCount, const CSSM_ACL_ENTRY_INFO *acls)
{
	StLock<Mutex>_(mMutex);
	// add owner acl
	mAcls[ownerHandle] = new ACL(*this, AclOwnerPrototype::overlay(owner));
	
	// add acl entries
	const AclEntryInfo *acl = AclEntryInfo::overlay(acls);
	for (uint32 n = 0; n < aclCount; n++) {
		secdebug("SecAccess", "%p compiling entry %ld", this, acl[n].handle());
		mAcls[acl[n].handle()] = new ACL(*this, acl[n]);
	}
	secdebug("SecAccess", "%p %ld entries compiled", this, mAcls.size());
}


//
// Creation helper objects
//
const char Access::Maker::creationEntryTag[] = "___setup___";

Access::Maker::Maker(Allocator &alloc, MakerType makerType)
	: allocator(alloc), mKey(alloc), mCreds(allocator), mMakerType(makerType)
{
	if (makerType == kStandardMakerType)
	{
		// generate random key
		mKey.malloc(keySize);
		UniformRandomBlobs<DevRandomGenerator>().random(mKey.get());
		
		// create entry info for resource creation
		mInput = AclEntryPrototype(TypedList(allocator, CSSM_ACL_SUBJECT_TYPE_PASSWORD,
			new(allocator) ListElement(mKey.get())));
		mInput.proto().tag(creationEntryTag);

		// create credential sample for access
		mCreds += TypedList(allocator, CSSM_SAMPLE_TYPE_PASSWORD, new(allocator) ListElement(mKey.get()));
	}
	else
	{
		// just make it an CSSM_ACL_SUBJECT_TYPE_ANY list
		mInput = AclEntryPrototype(TypedList(allocator, CSSM_ACL_SUBJECT_TYPE_ANY));
	}
}

void Access::Maker::initialOwner(ResourceControlContext &ctx, const AccessCredentials *creds)
{
	//@@@ make up ctx.entry-info
	ctx.input() = mInput;
	ctx.credentials(creds);
}

const AccessCredentials *Access::Maker::cred()
{
	return &mCreds;
}
