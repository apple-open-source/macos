/*
 * Copyright (c) 2000-2004,2007-2008 Apple Inc. All Rights Reserved.
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
// string), while Version 1 contains both selector and description. We are now always
// writing version-1 data, but will continue to recognize version-0 data indefinitely
// for really, really old keychain items.
//
#include "acl_keychain.h"
#include "agentquery.h"
#include "acls.h"
#include "connection.h"
#include "database.h"
#include "server.h"
#include <security_utilities/debugging.h>
#include <security_utilities/logging.h>
#include <security_cdsa_utilities/osxverifier.h>
#include <algorithm>


#define ACCEPT_LEGACY_FORM 1


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
		Process &process = Server::process();
		secdebug("kcacl", "Keychain query for process %d (UID %d)", process.pid(), process.uid());

		// assemble the effective validity mode mask
		uint32_t mode = Maker::defaultMode;
		const uint16_t &flags = selector.flags;
		if (flags & CSSM_ACL_KEYCHAIN_PROMPT_UNSIGNED_ACT)
			mode = (mode & ~CSSM_ACL_KEYCHAIN_PROMPT_UNSIGNED) | (flags & CSSM_ACL_KEYCHAIN_PROMPT_UNSIGNED);
		if (flags & CSSM_ACL_KEYCHAIN_PROMPT_INVALID_ACT)
			mode = (mode & ~CSSM_ACL_KEYCHAIN_PROMPT_INVALID) | (flags & CSSM_ACL_KEYCHAIN_PROMPT_INVALID);
		
		// determine signed/validity status of client, without reference to any particular Code Requirement
		SecCodeRef clientCode = process.currentGuest();
		Server::active().longTermActivity();
		OSStatus validation = clientCode ? SecCodeCheckValidity(clientCode, kSecCSDefaultFlags, NULL) : errSecCSStaticCodeNotFound;
		switch (validation) {
		case noErr:							// client is signed and valid
			secdebug("kcacl", "client is valid, proceeding");
			break;
		case errSecCSUnsigned:				// client is not signed
			if (!(mode & CSSM_ACL_KEYCHAIN_PROMPT_UNSIGNED)) {
				secdebug("kcacl", "client is unsigned, suppressing prompt");
				return false;
			}
			break;
		case errSecCSSignatureFailed:		// client signed but signature is broken
		case errSecCSGuestInvalid:			// client signed but dynamically invalid
		case errSecCSStaticCodeNotFound:	// client not on disk (or unreadable)
			if (!(mode & CSSM_ACL_KEYCHAIN_PROMPT_INVALID)) {
				secdebug("kcacl", "client is invalid, suppressing prompt");
				Syslog::info("suppressing keychain prompt for invalidly signed client %s(%d)",
					process.getPath().c_str(), process.pid());
				return false;
			}
			Syslog::info("attempting keychain prompt for invalidly signed client %s(%d)",
				process.getPath().c_str(), process.pid());
			break;
		default:							// something else went wrong
			secdebug("kcacl", "client validation failed rc=%d, suppressing prompt", int32_t(validation));
			return false;
		}
		
		// At this point, we're committed to try to Pop The Question. Now, how?
		
		// does the user need to type in the passphrase?
        const Database *db = env->database;
        bool needPassphrase = db && (selector.flags & CSSM_ACL_KEYCHAIN_PROMPT_REQUIRE_PASSPHRASE);

		// an application (i.e. Keychain Access.app :-) can force this option
		if (clientCode && validation == noErr) {
			CFRef<CFDictionaryRef> dict;
			if (SecCodeCopySigningInformation(clientCode, kSecCSDefaultFlags, &dict.aref()) == noErr)
				if (CFDictionaryRef info = CFDictionaryRef(CFDictionaryGetValue(dict, kSecCodeInfoPList)))
					needPassphrase |=
						(CFDictionaryGetValue(info, CFSTR("SecForcePassphrasePrompt")) != NULL);
		}

		// pop The Question
		if (db && db->belongsToSystem() && !hasAuthorizedForSystemKeychain()) {
			QueryKeychainAuth query;
			query.inferHints(Server::process());
			if (query(db ? db->dbName() : NULL, description.c_str(), context.authorization(), NULL) != SecurityAgent::noReason)
				return false;
			return true;
		} else {
			QueryKeychainUse query(needPassphrase, db);
			query.inferHints(Server::process());
			query.addHint(AGENT_HINT_CLIENT_VALIDITY, &validation, sizeof(validation));
			if (query.queryUser(db ? db->dbName() : NULL, 
				description.c_str(), context.authorization()) != SecurityAgent::noReason)
				return false;

			// process an "always allow..." response
			if (query.remember && clientCode) {
				RefPointer<OSXCode> clientXCode = new OSXCodeWrap(clientCode);
				RefPointer<AclSubject> subject = new CodeSignatureAclSubject(OSXVerifier(clientXCode));
				SecurityServerAcl::addToStandardACL(context, subject);
			}

			// finally, return the actual user response
			return query.allow;
		}
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
// Has the caller recently authorized in such a way as to render unnecessary
// the usual QueryKeychainAuth dialog?  (The right is specific to Keychain 
// Access' way of editing a system keychain.)  
//
bool KeychainPromptAclSubject::hasAuthorizedForSystemKeychain() const
{
    string rightString = "system.keychain.modify";
    return Server::session().isRightAuthorized(rightString, Server::connection(), false/*no UI*/);
}



//
// Create a KeychainPromptAclSubject
//
uint32_t KeychainPromptAclSubject::Maker::defaultMode;

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
	default:
		CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
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

	// always use the latest binary version
	version(currentVersion);
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


#ifdef DEBUGDUMP

void KeychainPromptAclSubject::debugDump() const
{
	Debug::dump("KeychainPrompt:%s(%s)",
		description.c_str(),
		(selector.flags & CSSM_ACL_KEYCHAIN_PROMPT_REQUIRE_PASSPHRASE) ? "passphrase" : "standard");
}

#endif //DEBUGDUMP
