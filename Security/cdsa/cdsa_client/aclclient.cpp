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
#include <Security/walkers.h>


namespace Security {
namespace CssmClient {


//
// AclBearer methods (trivial)
//
AclBearer::~AclBearer()
{ }


//
// Delete an ACL by handle
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



AclFactory::PasswordChangeCredentials::PasswordChangeCredentials (const CssmData& password, CssmAllocator& allocator) :
    mAllocator (allocator)
{
    mCredentials = new (allocator) AutoCredentials (allocator);;
    mCredentials->sample(0) = TypedList(allocator, CSSM_SAMPLE_TYPE_KEYCHAIN_CHANGE_LOCK, new (allocator) ListElement (CSSM_SAMPLE_TYPE_PASSWORD),
                                        new (allocator) ListElement (password));
}



AclFactory::PasswordChangeCredentials::~PasswordChangeCredentials ()
{
    DataWalkers::chunkFree (mCredentials, mAllocator);
}



} // end namespace CssmClient
} // end namespace Security
