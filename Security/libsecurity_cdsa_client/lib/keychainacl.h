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
#ifndef _KEYCHAINACL
#define _KEYCHAINACL

#include <Security/cssm.h>
#include <security_cdsa_utilities/cssmaclpod.h>
#include <security_cdsa_utilities/cssmcred.h>
#include <security_cdsa_utilities/cssmalloc.h>

#ifdef _CPP_KEYCHAINACL
# pragma export on
#endif


namespace Security
{

namespace CssmClient
{

class KeychainAclFactory
{
public:
	KeychainAclFactory(Allocator &alloc);
	~KeychainAclFactory();
	
	Allocator &allocator;
	
public:
	//
	// Create credentials. These functions return AccessCredentials pointers.
	//
	const AccessCredentials *nullCredentials();
	const AccessCredentials *keychainPromptCredentials();
	const AccessCredentials *keychainPromptUnlockCredentials();
	const AutoCredentials *passwordChangeCredentials(const CssmData &password);
	const AutoCredentials *passwordUnlockCredentials(const CssmData &password);

public:
	//
	// Create initial ACLs. Pass those to resource creation functions.
	//
	AclEntryInput *keychainPromptOwner(const CssmData &description);
	AclEntryInput *anyOwner();
	void release(AclEntryInput *input);
	
public:
	//
	// Edit ACLs (in external form, as TypedLists)
	//
	void comment(TypedList &subject);
	void uncomment(TypedList &subject);
	
private:
	AutoCredentials nullCred;
	AutoCredentials kcCred;
	AutoCredentials kcUnlockCred;
};


} // end namespace CssmClient

} // end namespace Security

#ifdef _CPP_KEYCHAINACL
# pragma export off
#endif

#endif //_KEYCHAINACL
