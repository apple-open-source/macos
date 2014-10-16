/*
 * Copyright (c) 2004-2007,2013 Apple Inc. All Rights Reserved.
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
// tokenacl - Token-based ACL implementation
//
#include "tokenacl.h"
#include "tokend.h"
#include "token.h"
#include "tokendatabase.h"
#include "agentquery.h"
#include <security_utilities/trackingallocator.h>
#include <security_cdsa_utilities/cssmbridge.h>


//
// A TokenAcl initializes to "invalid, needs update".
// Note how our Token will start its ResetGeneration at 1, while we start at zero.
//
TokenAcl::TokenAcl()
	: mLastReset(0)
{
}


//
// Instantiate is called (by the ACL machinery core) before this ACL object's
// contents are used in any way. Here is where we fetch the ACL data from tokend
// (if we haven't got it yet).
//
void TokenAcl::instantiateAcl()
{
	if (token().resetGeneration(mLastReset))
		return;
	
	secdebug("tokenacl", "%p loading ACLs from tokend", this);
		
	// read owner
	AclOwnerPrototype *owner = NULL;
	token().tokend().getOwner(aclKind(), tokenHandle(), owner);
	assert(owner);
	
	// read entries
	uint32 count;
	AclEntryInfo *infos;
	token().tokend().getAcl(aclKind(), tokenHandle(), NULL, count, infos);
	
	// commit to setting the ACL data
	ObjectAcl::owner(*owner);
	ObjectAcl::entries(count, infos);
	
	// and if we actually made it to here...
	mLastReset = token().resetGeneration();
}


//
// The ACL machinery core calls this after successfully making changes to our ACL.
//
void TokenAcl::changedAcl()
{
}


//
// CSSM-layer read gates. This accesses a cached version prepared in our instantiateAcl().
//
void TokenAcl::getOwner(AclOwnerPrototype &owner)
{
	ObjectAcl::cssmGetOwner(owner);
}

void TokenAcl::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{
	ObjectAcl::cssmGetAcl(tag, count, acls);
}


//
// CSSM-layer write gates.
// This doesn't directly write to the local ObjectAcl at all. The call is relayed to
// tokend, and the resulting ACL is being re-read when next needed.
//
void TokenAcl::changeAcl(const AclEdit &edit, const AccessCredentials *cred, Database *db)
{
	// changeAcl from/to a PIN (source) ACL has special UI handling here
	// @@@ this is an ad-hoc hack; general solution awaits the ACL machinery rebuild later
	instantiateAcl();		// (redundant except in error cases)
	if (TokenDatabase *tokenDb = dynamic_cast<TokenDatabase *>(db))
		if (edit.mode() == CSSM_ACL_EDIT_MODE_REPLACE)
			if (const AclEntryInput *input = edit.newEntry()) {
				if (unsigned pin = pinFromAclTag(input->proto().tag())) {
					// assume this is a PIN change request
					pinChange(pin, edit.handle(), *tokenDb);
					invalidateAcl();
					return;
				}
			}			

	// hand the request off to tokend to do as it will
	token().tokend().changeAcl(aclKind(), tokenHandle(), Required(cred), edit);
	invalidateAcl();
}

void TokenAcl::changeOwner(const AclOwnerPrototype &newOwner,
	const AccessCredentials *cred, Database *db)
{
	token().tokend().changeOwner(aclKind(), tokenHandle(), Required(cred), newOwner);
	invalidateAcl();
}


//
// Ad-hoc PIN change processing.
// This cooks a suitable changeAcl call to tokend, ad hoc.
// It does NOT complete the originating request; it replaces it completely.
// (Completion processing requires not-yet-implemented ACL machine UI coalescing features.)
//
class QueryNewPin : public QueryNewPassphrase {
public:
	QueryNewPin(unsigned int pinn, CSSM_ACL_HANDLE h, TokenDatabase &db, Reason reason)
		: QueryNewPassphrase(db, reason), pin(pinn), handle(h) { }
		
	const unsigned int pin;
	const CSSM_ACL_HANDLE handle;

	Reason accept(CssmManagedData &passphrase, CssmData *oldPassphrase);
};

SecurityAgent::Reason QueryNewPin::accept(CssmManagedData &passphrase, CssmData *oldPassphrase)
{
	assert(oldPassphrase);		// we don't handle the new-pin case (yet)
	
	// form a changeAcl query and send it to tokend
	try {
		TrackingAllocator alloc(Allocator::standard());
		AclEntryPrototype proto(TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_PROMPTED_PASSWORD,
			new(alloc) ListElement(passphrase)
			));
		proto.authorization() = AuthorizationGroup(CSSM_ACL_AUTHORIZATION_PREAUTH(pin), alloc);
		char pintag[20]; sprintf(pintag, "PIN%d", pin);
		proto.tag(pintag);
		AclEntryInput input(proto);
		AclEdit edit(CSSM_ACL_EDIT_MODE_REPLACE, handle, &input);
		AutoCredentials cred(alloc);
		cred += TypedList(alloc, CSSM_SAMPLE_TYPE_PROMPTED_PASSWORD,
			new(alloc) ListElement(*oldPassphrase));
		safer_cast<TokenDatabase &>(database).token().tokend().changeAcl(dbAcl, noDb, cred, edit);
		return SecurityAgent::noReason;
	} catch (const CssmError &err) {
		switch (err.error) {
		default:
			return SecurityAgent::unknownReason;
		}
	} catch (...) {
		return SecurityAgent::unknownReason;
	}
}

void TokenAcl::pinChange(unsigned int pin, CSSM_ACL_HANDLE handle, TokenDatabase &database)
{
	QueryNewPin query(pin, handle, database, SecurityAgent::changePassphrase);
	query.inferHints(Server::process());
	CssmAutoData newPin(Allocator::standard(Allocator::sensitive));
    CssmAutoData oldPin(Allocator::standard(Allocator::sensitive));
	switch (query(oldPin, newPin)) {
	case SecurityAgent::noReason:		// worked
		return;
	default:
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	}
}
