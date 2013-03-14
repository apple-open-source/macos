/*
 * Copyright (c) 2000-2008 Apple Inc. All Rights Reserved.
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
// acls - securityd ACL implementation
//
#include "acls.h"
#include "connection.h"
#include "server.h"
#include "agentquery.h"
#include "tokendatabase.h"
#include "acl_keychain.h"

// ACL subjects whose Environments we implement
#include <security_cdsa_utilities/acl_any.h>
#include <security_cdsa_utilities/acl_password.h>
#include <security_cdsa_utilities/acl_threshold.h>


//
// SecurityServerAcl is virtual
//
SecurityServerAcl::~SecurityServerAcl()
{ }


//
// The default implementation of the ACL interface simply uses the local ObjectAcl
// data. You can customize this by implementing instantiateAcl() [from ObjectAcl]
// or by overriding these methods as desired.
// Note: While you can completely ignore the ObjectAcl personality if you wish, it's
// usually smarter to adapt it.
//
void SecurityServerAcl::getOwner(AclOwnerPrototype &owner)
{
	StLock<Mutex> _(aclSequence);
	ObjectAcl::cssmGetOwner(owner);
}

void SecurityServerAcl::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{
	StLock<Mutex> _(aclSequence);
	ObjectAcl::cssmGetAcl(tag, count, acls);
}

void SecurityServerAcl::changeAcl(const AclEdit &edit, const AccessCredentials *cred,
	Database *db)
{
	StLock<Mutex> _(aclSequence);
	SecurityServerEnvironment env(*this, db);
	ObjectAcl::cssmChangeAcl(edit, cred, &env);
}

void SecurityServerAcl::changeOwner(const AclOwnerPrototype &newOwner,
	const AccessCredentials *cred, Database *db)
{
	StLock<Mutex> _(aclSequence);
	SecurityServerEnvironment env(*this, db);
	ObjectAcl::cssmChangeOwner(newOwner, cred, &env);
}


//
// Modified validate() methods to connect all the conduits...
//
void SecurityServerAcl::validate(AclAuthorization auth, const AccessCredentials *cred, Database *db)
{
    SecurityServerEnvironment env(*this, db);

	{
		// Migrator gets a free ride
		Process &thisProcess = Server::process();
		StLock<Mutex> _(thisProcess);
		SecCodeRef clientRef = thisProcess.currentGuest();
		if (clientRef) {
			std::string clientPath = codePath(clientRef);
			if (clientPath == std::string("/usr/libexec/KeychainMigrator"))
				return;
		}
	}
	
	StLock<Mutex> objectSequence(aclSequence);
	StLock<Mutex> processSequence(Server::process().aclSequence);
    ObjectAcl::validate(auth, cred, &env);
}

void SecurityServerAcl::validate(AclAuthorization auth, const Context &context, Database *db)
{
	validate(auth,
		context.get<AccessCredentials>(CSSM_ATTRIBUTE_ACCESS_CREDENTIALS), db);
}


//
// This helper tries to add the (new) subject given to the ACL
// whose validation is currently proceeding through context.
// This will succeed if the ACL is in standard form, which means
// a ThresholdAclSubject.
// The new subject will be added at the front (so it is checked first
// from now on), and as a side effect we'll notify the client side to
// re-encode the object.
// Returns true if the edit could be done, or false if the ACL wasn't
// standard enough. May throw if the ACL is malformed or otherwise messed up.
//
// This is a self-contained helper that is here merely because it's "about"
// ACLs and has no better home.
//
bool SecurityServerAcl::addToStandardACL(const AclValidationContext &context, AclSubject *subject)
{
	if (SecurityServerEnvironment *env = context.environment<SecurityServerEnvironment>())
		if (ThresholdAclSubject *threshold = env->standardSubject(context)) {
			unsigned size = threshold->count();
			if (dynamic_cast<KeychainPromptAclSubject *>(threshold->subject(size-1))) {
				// looks standard enough
				secdebug("acl", "adding new subject %p to from of threshold ACL", subject);
				threshold->add(subject, 0);
				
				// tell the ACL it's been modified
				context.acl()->changedAcl();

				// trigger a special notification code on (otherwise successful) return
				Server::connection().overrideReturn(CSSMERR_CSP_APPLE_ADD_APPLICATION_ACL_SUBJECT);
				return true;
			}
		}
	secdebug("acl", "ACL is not standard form; cannot edit");
	return false;
}


//
// Look at the ACL whose validation is currently proceeding through context.
// If it LOOKS like a plausible version of a legacy "dot mac item" ACL.
// We don't have access to the database attributes of the item up here in the
// securityd sky, so we have to apply a heuristic based on which applications (by path)
// are given access to the item.
// So this is strictly a heuristic. The potential downside is that we may inadvertently
// give access to new .Mac authorized Apple (only) applications when the user only intended
// a limited set of extremely popular Apple (only) applications that just happen to all be
// .Mac authorized today. We can live with that.
//
bool SecurityServerAcl::looksLikeLegacyDotMac(const AclValidationContext &context)
{
	static const char * const prototypicalDotMacPath[] = {
		"/Applications/Mail.app",
		"/Applications/Safari.app",
		"/Applications/iSync.app",
		"/Applications/System Preferences.app",
		"/Applications/iCal.app",
		"/Applications/iChat.app",
		"/Applications/iTunes.app",
		"/Applications/Address Book.app",
		"/Applications/iSync.app",
		NULL	// sentinel
	};
	
	static const unsigned threshold = 6;
	
	if (SecurityServerEnvironment *env = context.environment<SecurityServerEnvironment>()) {
		if (ThresholdAclSubject *list = env->standardSubject(context)) {
			unsigned count = list->count();
			unsigned matches = 0;
			for (unsigned n = 0; n < count; ++n) {
				if (CodeSignatureAclSubject *app = dynamic_cast<CodeSignatureAclSubject *>(list->subject(n))) {
					for (const char * const *p = prototypicalDotMacPath; *p; p++)
						if (app->path() == *p)
							matches++;
				}
			}
			secdebug("codesign", "matched %d of %zd candididates (threshold=%d)",
				matches, sizeof(prototypicalDotMacPath) / sizeof(char *) - 1, threshold);
			return matches >= threshold;
		}
	}
	return false;
}


//
// External storage interface
//
Adornable &SecurityServerEnvironment::store(const AclSubject *subject)
{
	switch (subject->type()) {
	case CSSM_ACL_SUBJECT_TYPE_PREAUTH:
		{
			if (TokenDatabase *tokenDb = dynamic_cast<TokenDatabase *>(database))
				return tokenDb->common().store();
		}
		break;
	default:
		break;
	}
	CssmError::throwMe(CSSM_ERRCODE_ACL_SUBJECT_TYPE_NOT_SUPPORTED);
}


//
// ProcessAclSubject personality: uid/gid/pid come from the active Process object
//
uid_t SecurityServerEnvironment::getuid() const
{
    return Server::process().uid();
}

gid_t SecurityServerEnvironment::getgid() const
{
    return Server::process().gid();
}

pid_t SecurityServerEnvironment::getpid() const
{
    return Server::process().pid();
}


//
// CodeSignatureAclSubject personality: take code signature from active Process object
//
bool SecurityServerEnvironment::verifyCodeSignature(const OSXVerifier &verifier,
	const AclValidationContext &context)
{
	return Server::codeSignatures().verify(Server::process(), verifier, context);
}


//
// PromptedAclSubject personality: Get a secret by prompting through SecurityAgent
//
bool SecurityServerEnvironment::getSecret(CssmOwnedData &secret, const CssmData &prompt) const
{
	//@@@ ignoring prompt - not used right now
	if (database) {
		QueryPIN query(*database);
		query.inferHints(Server::process());
		if (!query()) {	// success
			secret = query.pin();
			return true;
		}
	}
	return false;
}


//
// SecretAclSubject personality: externally validate a secret (passphrase etc.)
// Right now, this always goes to the (Token)Database object, because that's where
// the PIN ACL entries are. We could direct this at the ObjectAcl (database or key)
// instead and rely on tokend to perform the PIN mapping, but the generic tokend
// wrappers do not (currently) perform any ACL validation, so every tokend would have
// to re-implement that. Perhaps in the next ACL revamp cycle...
//
bool SecurityServerEnvironment::validateSecret(const SecretAclSubject *me,
	const AccessCredentials *cred)
{
	return database && database->validateSecret(me, cred);
}


//
// PreAuthenticationAclSubject personality - refer to database (ObjectAcl)
//
ObjectAcl *SecurityServerEnvironment::preAuthSource()
{
	return database ? &database->acl() : NULL;
}


//
// Autonomous ACL editing support
//
ThresholdAclSubject *SecurityServerEnvironment::standardSubject(const AclValidationContext &context)
{
	return dynamic_cast<ThresholdAclSubject *>(context.subject());
}


//
// The default AclSource denies having an ACL at all
//
AclSource::~AclSource()
{ /* virtual */ }

SecurityServerAcl &AclSource::acl()
{
	CssmError::throwMe(CSSM_ERRCODE_OBJECT_ACL_NOT_SUPPORTED);
}

Database *AclSource::relatedDatabase()
{
	return NULL;
}
