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
// keychainacl - Keychain-related ACL and credential forms
//
#ifdef __MWERKS__
#define _CPP_KEYCHAINACL
#endif

#include "keychainacl.h"
#include <security_cdsa_utilities/cssmwalkers.h>

using namespace CssmClient;


//
// Construct the factory.
// @@@ Leaks.
//
KeychainAclFactory::KeychainAclFactory(Allocator &alloc)
: allocator(alloc), nullCred(alloc, 1), kcCred(alloc, 2), kcUnlockCred(alloc, 1)
{
	// the credential objects self-initialize to empty
	nullCred.sample(0) = TypedList(alloc, CSSM_SAMPLE_TYPE_THRESHOLD);
	
	kcCred.sample(0) = TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT);
	kcCred.sample(1) = TypedList(alloc, CSSM_SAMPLE_TYPE_THRESHOLD,
		new(alloc) ListElement(TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT)));

	// @@@ This leaks a ListElement(CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT)
	kcUnlockCred.sample(0) = TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
									  new(alloc) ListElement(CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT));
}

KeychainAclFactory::~KeychainAclFactory()
{
}


//
// Produce credentials.
// These are constants that don't need to be allocated per use.
//
const AccessCredentials *KeychainAclFactory::nullCredentials()
{
	return &nullCred;
}

const AccessCredentials *KeychainAclFactory::keychainPromptCredentials()
{
	return &kcCred;
}

const AccessCredentials *KeychainAclFactory::keychainPromptUnlockCredentials()
{
	return &kcUnlockCred;
}

const AutoCredentials *KeychainAclFactory::passwordChangeCredentials(const CssmData &password)
{
	AutoCredentials *cred = new AutoCredentials(allocator, 1);
	// @@@ This leaks a ListElement(CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT) and ListElement(password)
	cred->sample(0) = TypedList(allocator, CSSM_SAMPLE_TYPE_KEYCHAIN_CHANGE_LOCK,
								new(allocator) ListElement(CSSM_SAMPLE_TYPE_PASSWORD),
								new(allocator) ListElement(password));
	return cred;
}

const AutoCredentials *KeychainAclFactory::passwordUnlockCredentials(const CssmData &password)
{
	AutoCredentials *cred = new AutoCredentials(allocator, 1);
	// @@@ This leaks a ListElement(CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT) and ListElement(password)
	cred->sample(0) = TypedList(allocator, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
								new(allocator) ListElement(CSSM_SAMPLE_TYPE_PASSWORD),
								new(allocator) ListElement(password));
	return cred;
}


//
// 
AclEntryInput *KeychainAclFactory::keychainPromptOwner(const CssmData &description)
{
	// @@@ Make sure this works for a NULL description
	AclEntryPrototype proto(TypedList(allocator, CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT,
		new(allocator) ListElement(allocator, description)));
	return new(allocator) AclEntryInput(proto);
}

AclEntryInput *KeychainAclFactory::anyOwner()
{
	AclEntryPrototype proto(TypedList(allocator, CSSM_ACL_SUBJECT_TYPE_ANY));
	return new(allocator) AclEntryInput(proto);
}

void KeychainAclFactory::release(AclEntryInput *input)
{
	DataWalkers::chunkFree(input, allocator);
}


//
// ACL editing
//
void KeychainAclFactory::comment(TypedList &subject)
{
	subject.insert(new(allocator) ListElement(CSSM_ACL_SUBJECT_TYPE_COMMENT),
		subject.first());
}

void KeychainAclFactory::uncomment(TypedList &subject)
{
	ListElement *first = subject.first();
	assert(*first == CSSM_ACL_SUBJECT_TYPE_COMMENT);
	subject -= first;
	destroy(first, allocator);
}
