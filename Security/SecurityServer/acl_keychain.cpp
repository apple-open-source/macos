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
// acl_keychain - a subject type for the protected-path
//				  keychain prompt interaction model.
//
// Arguments in list form:
//	list[1] = CssmData: Descriptive String (presented to user in protected dialogs)
//
// Some notes on Acl Update Triggers:
// When the user checks the "don't ask me again" checkbox in the access confirmation
// dialog, we respond by returning the informational error code
// CSSMERR_CSP_APPLE_ADD_APPLICATION_ACL_SUBJECT, and setting a count-down trigger
// in the connection. The caller is entitled to bypass our dialog (it succeeds
// automatically) within the next few (Connection::aclUpdateTriggerLimit == 2)
// requests, in order to update the object's ACL as requested. It must then retry
// the original access operation (which will presumably pass because of that edit).
// These are the rules: for the trigger to apply, the access must be to the same
// object, from the same connection, and within the next two accesses.
// (Currently, these are for a "get acl" and the "change acl" calls.)
// Damage Control Department: The worst this mechanism could do, if subverted, is
// to bypass our confirmation dialog (making it appear to succeed to the ACL validation).
// But that is exactly what the "don't ask me again" checkbox is meant to do, so any
// subversion would be based on a (perhaps intentional) miscommunication between user 
// and client process as to what the user consents not to be asked about (any more).
// The user can always examine the resulting ACL (in Keychain Access or elsewhere), and
// edit it to suit her needs.
//
#ifdef __MWERKS__
#define _CPP_ACL_KEYCHAIN
#endif

#include "acl_keychain.h"
#include "agentquery.h"
#include "acls.h"
#include "connection.h"
#include "xdatabase.h"
#include "server.h"
#include <Security/debugging.h>
#include <algorithm>


//
// Validate a credential set against this subject.
//
bool KeychainPromptAclSubject::validate(const AclValidationContext &context,
    const TypedList &sample) const
{
    SecurityServerEnvironment *env = context.environment<SecurityServerEnvironment>();
    if (env) {
		// check for special ACL-update override
		if (context.authorization() == CSSM_ACL_AUTHORIZATION_CHANGE_ACL
				&& Server::connection().aclWasSetForUpdateTrigger(env->acl)) {
			debug("kcacl", "honoring acl update trigger for %p(%s)", 
                &env->acl, description.c_str());
			return true;
		}

        // ask the user
		QueryKeychainUse query;
		const Database *db = env->database();
		query((db ? db->dbName() : NULL), description.c_str(), context.authorization());
		if (query.continueGrantingToCaller) {
			// mark for special ACL-update override (really soon) later
			Server::connection().setAclUpdateTrigger(env->acl);
			debug("kcacl", "setting acl update trigger for %p(%s)", 
                &env->acl, description.c_str());
			// fail with prejudice (caller will retry)
			CssmError::throwMe(CSSMERR_CSP_APPLE_ADD_APPLICATION_ACL_SUBJECT);
		}
		return query.allowAccess;
    }
	return false;        // default to deny without prejudice
}


//
// Make a copy of this subject in CSSM_LIST form
//
CssmList KeychainPromptAclSubject::toList(CssmAllocator &alloc) const
{
	return TypedList(alloc, CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT,
        new(alloc) ListElement(alloc, description));
}


//
// Create a PasswordAclSubject
//
KeychainPromptAclSubject *KeychainPromptAclSubject::Maker::make(const TypedList &list) const
{
    ListElement *params[1];
	crack(list, 1, params, CSSM_LIST_ELEMENT_DATUM);
	return new KeychainPromptAclSubject(*params[0]);
}

KeychainPromptAclSubject *KeychainPromptAclSubject::Maker::make(Reader &pub, Reader &) const
{
    const char *description; pub(description);
	return new KeychainPromptAclSubject(description);
}

KeychainPromptAclSubject::KeychainPromptAclSubject(string descr)
: SimpleAclSubject(CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT, CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT),
  description(descr)
{
}


//
// Export the subject to a memory blob
//
void KeychainPromptAclSubject::exportBlob(Writer::Counter &pub, Writer::Counter &priv)
{
    pub.insert(description.size() + 1);
}

void KeychainPromptAclSubject::exportBlob(Writer &pub, Writer &priv)
{
    pub(description.c_str());
}


#ifdef DEBUGDUMP

void KeychainPromptAclSubject::debugDump() const
{
	Debug::dump("KeychainPrompt:%s", description.c_str());
}

#endif //DEBUGDUMP
