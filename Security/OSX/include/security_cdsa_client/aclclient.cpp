/*
 * Copyright (c) 2000-2001,2007,2011 Apple Inc. All Rights Reserved.
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
// aclclient
//
#include <security_cdsa_client/cssmclient.h>
#include <security_cdsa_client/aclclient.h>
#include <security_cdsa_client/keychainacl.h> 
#include <security_cdsa_utilities/cssmwalkers.h>
#include <security_cdsa_utilities/cssmdata.h>


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
	Allocator &alloc;

	AutoCredentials nullCred;
	AutoCredentials promptCred;
	AutoCredentials unlockCred;
	AutoCredentials cancelCred;
	AutoCredentials promptedPINCred;
	AutoCredentials promptedPINItemCred;
	
	AclOwnerPrototype anyOwner;
	AclEntryInfo anyAcl;
};

namespace {
	ModuleNexus<Statics> statics;
}


//
// Make pseudo-statics.
// Note: This is an eternal object. It is not currently destroyed
// if the containing code is unloaded.
//
Statics::Statics()
	: alloc(Allocator::standard()),
	  nullCred(alloc, 1),
	  promptCred(alloc, 3),
	  unlockCred(alloc, 1),
	  cancelCred(alloc, 1),
	  promptedPINCred(alloc, 1),
	  promptedPINItemCred(alloc, 1),
	  anyOwner(TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_ANY)),
	  anyAcl(AclEntryPrototype(TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_ANY), 1))
{
	// nullCred: nothing at all
	// contains:
	//  an empty THRESHOLD sample to match threshold subjects with "free" subjects
	nullCred.sample(0) = TypedList(alloc, CSSM_SAMPLE_TYPE_THRESHOLD);

	// promptCred: a credential permitting user prompt confirmations
	// contains:
	//  a KEYCHAIN_PROMPT sample, both by itself and in a THRESHOLD
	//  a PROMPTED_PASSWORD sample
	promptCred.sample(0) = TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT);
	promptCred.sample(1) = TypedList(alloc, CSSM_SAMPLE_TYPE_THRESHOLD,
		new(alloc) ListElement(TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT)));
	promptCred.sample(2) = TypedList(alloc, CSSM_SAMPLE_TYPE_PROMPTED_PASSWORD,
		new(alloc) ListElement(alloc, CssmData()));

	// unlockCred: ???
	unlockCred.sample(0) = TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
		new(alloc) ListElement(CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT));
	
	cancelCred.sample(0) = TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
									 new(alloc) ListElement(CSSM_WORDID_CANCELED));
	
	/*
		We don't set this:
	 
			promptedPINCred.tag("PIN1");

		here to avoid triggering code in TokenDatabase::getAcl in securityd that
		would always show a PIN unlock dialog. This credential is used for an
		unlock of the database, i.e. a dbauthenticate call to unlock the card.
	*/
	promptedPINCred.sample(0) = TypedList(alloc, CSSM_SAMPLE_TYPE_PROMPTED_PASSWORD,
										  new(alloc) ListElement(alloc, CssmData()));

	/*
		This credential is used for items like non-repudiation keys that always
		require an explicit entry of the PIN. We set this so that Token::authenticate
		will recognize the number of the PIN we need to unlock.
	*/
	promptedPINItemCred.tag("PIN1");
	promptedPINItemCred.sample(0) = TypedList(alloc, CSSM_SAMPLE_TYPE_PROMPTED_PASSWORD,
										  new(alloc) ListElement(alloc, CssmData()));
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


const AccessCredentials *AclFactory::cancelCred() const
{ return &statics().cancelCred; }

const AccessCredentials *AclFactory::promptedPINCred() const
{ return &statics().promptedPINCred; }

const AccessCredentials *AclFactory::promptedPINItemCred() const
{ return &statics().promptedPINItemCred; }


//
// Manage the (pseudo) credentials used to explicitly provide a passphrase to a keychain.
// Use the eternal unlockCred() for normal (protected prompt) unlocking.
//
AclFactory::KeychainCredentials::~KeychainCredentials ()
{
    DataWalkers::chunkFree(mCredentials, allocator);
}

AclFactory::PassphraseUnlockCredentials::PassphraseUnlockCredentials (const CssmData& password,
	Allocator& allocator) : KeychainCredentials(allocator)
{
    mCredentials->sample(0) = TypedList(allocator, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
		new (allocator) ListElement (CSSM_SAMPLE_TYPE_PASSWORD),
		new (allocator) ListElement (CssmAutoData(allocator, password).release()));
}


//
// Manage the (pseudo) credentials used to explicitly change a keychain's passphrase
//
AclFactory::PasswordChangeCredentials::PasswordChangeCredentials (const CssmData& password,
	Allocator& allocator) : KeychainCredentials(allocator)
{
    mCredentials->sample(0) = TypedList(allocator, CSSM_SAMPLE_TYPE_KEYCHAIN_CHANGE_LOCK,
		new (allocator) ListElement (CSSM_SAMPLE_TYPE_PASSWORD),
		new (allocator) ListElement (CssmAutoData(allocator, password).release()));
}

	
//
// Wide open ("ANY") CSSM forms for owner and ACL entry
//
const AclOwnerPrototype &AclFactory::anyOwner() const
{ return statics().anyOwner; }

const AclEntryInfo &AclFactory::anyAcl() const
{ return statics().anyAcl; }


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


//
// CSSM ACL makers
//
AclFactory::Subject::Subject(Allocator &alloc, CSSM_ACL_SUBJECT_TYPE type)
	: TypedList(alloc, type)
{ }


AclFactory::PWSubject::PWSubject(Allocator &alloc)
	: Subject(alloc, CSSM_ACL_SUBJECT_TYPE_PASSWORD)
{ }

AclFactory::PWSubject::PWSubject(Allocator &alloc, const CssmData &secret)
	: Subject(alloc, CSSM_ACL_SUBJECT_TYPE_PASSWORD)
{
	append(new(alloc) ListElement(alloc, secret));
}

AclFactory::PromptPWSubject::PromptPWSubject(Allocator &alloc, const CssmData &prompt)
	: Subject(alloc, CSSM_ACL_SUBJECT_TYPE_PROMPTED_PASSWORD)
{
	append(new(alloc) ListElement(alloc, prompt));
}

AclFactory::PromptPWSubject::PromptPWSubject(Allocator &alloc, const CssmData &prompt, const CssmData &secret)
	: Subject(alloc, CSSM_ACL_SUBJECT_TYPE_PROMPTED_PASSWORD)
{
	append(new(alloc) ListElement(alloc, prompt));
	append(new(alloc) ListElement(alloc, secret));
}

AclFactory::ProtectedPWSubject::ProtectedPWSubject(Allocator &alloc)
	: Subject(alloc, CSSM_ACL_SUBJECT_TYPE_PROTECTED_PASSWORD)
{ }

AclFactory::PinSubject::PinSubject(Allocator &alloc, uint32 slot)
	: Subject(alloc, CSSM_ACL_SUBJECT_TYPE_PREAUTH)
{
	append(new(alloc) ListElement(CSSM_ACL_AUTHORIZATION_PREAUTH(slot)));
}

AclFactory::PinSourceSubject::PinSourceSubject(Allocator &alloc, const TypedList &form)
	: Subject(alloc, CSSM_ACL_SUBJECT_TYPE_PREAUTH_SOURCE)
{
	append(new(alloc) ListElement(form));
}


} // end namespace CssmClient
} // end namespace Security
