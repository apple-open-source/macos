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
// keyclient 
//
#include <Security/aclclient.h>
#include <Security/keychainacl.h>
#include <Security/cssmwalkers.h>
#include <Security/cssmdata.h>
#include <Security/cssmclient.h>


namespace Security {
namespace CssmClient {


static inline void check(CSSM_RETURN rc)
{
	ObjectImpl::check(rc);
}


//
// AclBearer methods (trivial)
//
AclBearer::~AclBearer()
{ }


//
// Variant forms of AclBearer implemented in terms of its canonical virtual methods
//
void AclBearer::addAcl(const AclEntryInput &input, const CSSM_ACCESS_CREDENTIALS *cred)
{
	changeAcl(AclEdit(input), cred);
}

void AclBearer::changeAcl(CSSM_ACL_HANDLE handle, const AclEntryInput &input,
	const CSSM_ACCESS_CREDENTIALS *cred)
{
	changeAcl(AclEdit(handle, input), cred);
}

void AclBearer::deleteAcl(CSSM_ACL_HANDLE handle, const CSSM_ACCESS_CREDENTIALS *cred)
{
	changeAcl(AclEdit(handle), cred);
}

void AclBearer::deleteAcl(const char *tag, const CSSM_ACCESS_CREDENTIALS *cred)
{
	AutoAclEntryInfoList entries;
	getAcl(entries, tag);
	for (uint32 n = 0; n < entries.count(); n++)
		deleteAcl(entries[n].handle(), cred);
}


//
// KeyAclBearer implementation
//
void KeyAclBearer::getAcl(AutoAclEntryInfoList &aclInfos, const char *selectionTag) const
{
	aclInfos.allocator(allocator);
	check(CSSM_GetKeyAcl(csp, &key, reinterpret_cast<const CSSM_STRING *>(selectionTag), aclInfos, aclInfos));
}

void KeyAclBearer::changeAcl(const CSSM_ACL_EDIT &aclEdit, const CSSM_ACCESS_CREDENTIALS *cred)
{
	check(CSSM_ChangeKeyAcl(csp, AccessCredentials::needed(cred), &aclEdit, &key));
}

void KeyAclBearer::getOwner(AutoAclOwnerPrototype &owner) const
{
	owner.allocator(allocator);
	check(CSSM_GetKeyOwner(csp, &key, owner));
}

void KeyAclBearer::changeOwner(const CSSM_ACL_OWNER_PROTOTYPE &newOwner,
	const CSSM_ACCESS_CREDENTIALS *cred)
{
	check(CSSM_ChangeKeyOwner(csp, AccessCredentials::needed(cred), &key, &newOwner));
}


//
// A single global structure containing pseudo-static data
//
struct Statics {
	Statics();
	CssmAllocator &alloc;

	AutoCredentials nullCred;
	AutoCredentials promptCred;
	AutoCredentials unlockCred;
};

namespace {
	ModuleNexus<Statics> statics;
}


//
// Make pseudo-statics.
// Note: This is an eternal object. It is not currently destroyed
// if the containing code is unloaded. But then, the containing
// code is Security.framework, which never unloads anyway.
//
Statics::Statics()
	: alloc(CssmAllocator::standard()),
	  nullCred(alloc, 1),
	  promptCred(alloc, 2),
	  unlockCred(alloc, 1)
{
	// nullCred: nothing at all
	// contains:
	//  an empty THRESHOLD sample to match threshold subjects with "free" subjects
	nullCred.sample(0) = TypedList(alloc, CSSM_SAMPLE_TYPE_THRESHOLD);

	// promptCred: a credential permitting user prompt confirmations
	// contains:
	//  a KEYCHAIN_PROMPT sample, both by itself and in a THRESHOLD
	promptCred.sample(0) = TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT);
	promptCred.sample(1) = TypedList(alloc, CSSM_SAMPLE_TYPE_THRESHOLD,
		new(alloc) ListElement(TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT)));

	// unlockCred: ???
	unlockCred.sample(0) = TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
		new(alloc) ListElement(CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT));
}


//
// Make and break AclFactories
//
AclFactory::AclFactory()
{ }

AclFactory::~AclFactory()
{ }


//
// Return basic pseudo-static values
//
const AccessCredentials *AclFactory::nullCred() const
{ return &statics().nullCred; }

const AccessCredentials *AclFactory::promptCred() const
{ return &statics().promptCred; }

const AccessCredentials *AclFactory::unlockCred() const
{ return &statics().unlockCred; }


//
// Manage the (pseudo) credentials used to explicitly provide a passphrase to a keychain.
// Use the eternal unlockCred() for normal (protected prompt) unlocking.
//
AclFactory::KeychainCredentials::~KeychainCredentials ()
{
    DataWalkers::chunkFree (mCredentials, allocator);
}

AclFactory::PassphraseUnlockCredentials::PassphraseUnlockCredentials (const CssmData& password,
	CssmAllocator& allocator) : KeychainCredentials(allocator)
{
    mCredentials->sample(0) = TypedList(allocator, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
		new (allocator) ListElement (CSSM_SAMPLE_TYPE_PASSWORD),
		new (allocator) ListElement (CssmAutoData(allocator, password).release()));
}


//
// Manage the (pseudo) credentials used to explicitly change a keychain's passphrase
//
AclFactory::PasswordChangeCredentials::PasswordChangeCredentials (const CssmData& password,
	CssmAllocator& allocator) : KeychainCredentials(allocator)
{
    mCredentials->sample(0) = TypedList(allocator, CSSM_SAMPLE_TYPE_KEYCHAIN_CHANGE_LOCK,
		new (allocator) ListElement (CSSM_SAMPLE_TYPE_PASSWORD),
		new (allocator) ListElement (CssmAutoData(allocator, password).release()));
}


//
// Create an ANY style AclEntryInput.
// This can be used to explicitly request wide-open authorization on a new CSSM object.
//
AclFactory::AnyResourceContext::AnyResourceContext(const CSSM_ACCESS_CREDENTIALS *cred)
	: mAny(CSSM_ACL_SUBJECT_TYPE_ANY), mTag(CSSM_ACL_AUTHORIZATION_ANY)
{
	// set up an ANY/EVERYTHING AclEntryInput
	input().proto().subject() += &mAny;
	AuthorizationGroup &authGroup = input().proto().authorization();
	authGroup.NumberOfAuthTags = 1;
	authGroup.AuthTags = &mTag;
	
	// install the cred (not copied)
	credentials(cred);
}


} // end namespace CssmClient
} // end namespace Security
