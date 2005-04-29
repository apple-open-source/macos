/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
// acl_keychain - a subject type for the protected-path
//				  keychain prompt interaction model.
//
// Arguments in CSSM_LIST form:
//	list[1] = CssmData: CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR structure
//	list[2] = CssmData: Descriptive String (presented to user in protected dialogs)
// For legacy compatibility, we accept a single-entry form
//	list[1] = CssmData: Descriptive String
// which defaults to a particular CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR structure value.
// This is never produced by current code, and is considered purely a legacy feature.
//
// On-disk (flattened) representation:
// In order to accommodate legacy formats nicely, we use the binary-versioning feature
// of the ACL machinery. Version 0 is the legacy format (storing only the description
// string), while Version 1 contains both selector and description. To allow for
// maximum backward compatibility, legacy-compatible forms are written out as version 0.
// See isLegacyCompatible().
//
// Some notes on Acl Update Triggers:
// When the user checks the "don't ask me again" checkbox in the access confirmation
// dialog, we respond by returning the informational error code
// CSSMERR_CSP_APPLE_ADD_APPLICATION_ACL_SUBJECT, and setting a count-down trigger
// in the connection. The caller is entitled to bypass our dialog (it succeeds
// automatically) within the next few (Connection::aclUpdateTriggerLimit == 3)
// requests, in order to update the object's ACL as requested. It must then retry
// the original access operation (which will presumably pass because of that edit).
// These are the rules: for the trigger to apply, the access must be to the same
// object, from the same connection, and within the next aclUpdateTriggerLimit accesses.
// (Currently, these are for a "get acl", "get owner", and the "change acl" calls.)
// Damage Control Department: The worst this mechanism could do, if subverted, is
// to bypass our confirmation dialog (making it appear to succeed to the ACL validation).
// But that is exactly what the "don't ask me again" checkbox is meant to do, so any
// subversion would be based on a (perhaps intentional) miscommunication between user 
// and client process as to what the user consents not to be asked about (any more).
// The user can always examine the resulting ACL (in Keychain Access or elsewhere), and
// edit it to suit her needs.
//
#include "acl_keychain.h"
#include "agentquery.h"
#include "acls.h"
#include "connection.h"
#include "database.h"
#include "server.h"
#include <security_utilities/debugging.h>
#include <algorithm>


#define ACCEPT_LEGACY_FORM 1
#define FECKLESS_KEYCHAIN_ACCESS_EXCEPTION 1


//
// The default for the selector structure.
//
CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR KeychainPromptAclSubject::defaultSelector = {
	CSSM_ACL_KEYCHAIN_PROMPT_CURRENT_VERSION,	// version
	0											// flags
};


//
// Validate a credential set against this subject.
//
bool KeychainPromptAclSubject::validate(const AclValidationContext &context,
    const TypedList &sample) const
{
    if (SecurityServerEnvironment *env = context.environment<SecurityServerEnvironment>()) {
		// check for special ACL-update override
		if (context.authorization() == CSSM_ACL_AUTHORIZATION_CHANGE_ACL
				&& Server::connection().aclWasSetForUpdateTrigger(env->acl)) {
			secdebug("kcacl", "honoring acl update trigger for %p(%s)", 
                &env->acl, description.c_str());
			return true;
		}
		
		// does the user need to type in the passphrase?
        const Database *db = env->database;
        bool needPassphrase = db && (selector.flags & CSSM_ACL_KEYCHAIN_PROMPT_REQUIRE_PASSPHRASE);

		// ask the user
#if FECKLESS_KEYCHAIN_ACCESS_EXCEPTION
		Process &process = Server::process();
		secdebug("kcacl", "Keychain query from process %d (UID %d)", process.pid(), process.uid());
		if (process.clientCode())
			needPassphrase |=
				process.clientCode()->canonicalPath() == "/Applications/Utilities/Keychain Access.app";
#endif
		QueryKeychainUse query(needPassphrase, db);
        query.inferHints(Server::process());
		
        if (query.queryUser(db ? db->dbName() : NULL, 
                            description.c_str(), context.authorization())
            != SecurityAgent::noReason)
                return false;

		// process "always allow..." response
		if (query.remember) {
			// mark for special ACL-update override (really soon) later
			Server::connection().setAclUpdateTrigger(env->acl);
			secdebug("kcacl", "setting acl update trigger for %p(%s)", 
				&env->acl, description.c_str());
			// fail with prejudice (caller will retry)
			CssmError::throwMe(CSSMERR_CSP_APPLE_ADD_APPLICATION_ACL_SUBJECT);
		}

		// finally, return the actual user response
		return query.allow;
    }
	return false;        // default to deny without prejudice
}


//
// Make a copy of this subject in CSSM_LIST form
//
CssmList KeychainPromptAclSubject::toList(Allocator &alloc) const
{
	// always issue new (non-legacy) form
	return TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT,
		new(alloc) ListElement(alloc, CssmData::wrap(selector)),
        new(alloc) ListElement(alloc, description));
}


//
// Create a KeychainPromptAclSubject
//
KeychainPromptAclSubject *KeychainPromptAclSubject::Maker::make(const TypedList &list) const
{
	switch (list.length()) {
#if ACCEPT_LEGACY_FORM
	case 2:	// legacy case: just description
		{
			ListElement *params[1];
			crack(list, 1, params, CSSM_LIST_ELEMENT_DATUM);
			return new KeychainPromptAclSubject(*params[0], defaultSelector);
		}
#endif //ACCEPT_LEGACY_FORM
	case 3:	// standard case: selector + description
		{
			ListElement *params[2];
			crack(list, 2, params, CSSM_LIST_ELEMENT_DATUM, CSSM_LIST_ELEMENT_DATUM);
			return new KeychainPromptAclSubject(*params[1],
				*params[0]->data().interpretedAs<CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR>(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE));
		}
	default:
		CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
	}
}

KeychainPromptAclSubject *KeychainPromptAclSubject::Maker::make(Version version,
	Reader &pub, Reader &) const
{
	CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR selector;
	const char *description;
	switch (version) {
	case pumaVersion:
		selector = defaultSelector;
		pub(description);
		break;
	case jaguarVersion:
		pub(selector);
		selector.version = n2h(selector.version);
		selector.flags = n2h(selector.flags);
		pub(description);
		break;
	}
	return new KeychainPromptAclSubject(description, selector);
}

KeychainPromptAclSubject::KeychainPromptAclSubject(string descr,
	const CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR &sel)
	: SimpleAclSubject(CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT),
	selector(sel), description(descr)
{
	// check selector version
	if (selector.version != CSSM_ACL_KEYCHAIN_PROMPT_CURRENT_VERSION)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);

	// determine binary compatibility version
	if (selector.flags == 0)	// compatible with old form
		version(pumaVersion);
	else
		version(jaguarVersion);
}


//
// Export the subject to a memory blob
//
void KeychainPromptAclSubject::exportBlob(Writer::Counter &pub, Writer::Counter &priv)
{
	if (version() != 0) {
		selector.version = h2n (selector.version);
		selector.flags = h2n (selector.flags);
		pub(selector);
	}
	
    pub.insert(description.size() + 1);
}

void KeychainPromptAclSubject::exportBlob(Writer &pub, Writer &priv)
{
	if (version() != 0) {
		selector.version = h2n (selector.version);
		selector.flags = h2n (selector.flags);
		pub(selector);
	}
    pub(description.c_str());
}


//
// Determine whether this ACL subject is in "legacy compatible" form.
// Legacy (<10.2) form contained no selector.
//
bool KeychainPromptAclSubject::isLegacyCompatible() const
{
	return selector.flags == 0;
}


#ifdef DEBUGDUMP

void KeychainPromptAclSubject::debugDump() const
{
	Debug::dump("KeychainPrompt:%s(%s)",
		description.c_str(),
		(selector.flags & CSSM_ACL_KEYCHAIN_PROMPT_REQUIRE_PASSPHRASE) ? "passphrase" : "standard");
}

#endif //DEBUGDUMP
