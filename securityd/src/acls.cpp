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
// acls - securityd ACL implementation
//
#include "acls.h"
#include "connection.h"
#include "server.h"
#include "agentquery.h"
#include "tokendatabase.h"

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
	ObjectAcl::cssmGetOwner(owner);
}

void SecurityServerAcl::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{
	ObjectAcl::cssmGetAcl(tag, count, acls);
}

void SecurityServerAcl::changeAcl(const AclEdit &edit, const AccessCredentials *cred,
	Database *db)
{
	SecurityServerEnvironment env(*this, db);
	ObjectAcl::cssmChangeAcl(edit, cred, &env);
}

void SecurityServerAcl::changeOwner(const AclOwnerPrototype &newOwner,
	const AccessCredentials *cred, Database *db)
{
	SecurityServerEnvironment env(*this, db);
	ObjectAcl::cssmChangeOwner(newOwner, cred, &env);
}


//
// Modified validate() methods to connect all the conduits...
//
void SecurityServerAcl::validate(AclAuthorization auth, const AccessCredentials *cred, Database *db)
{
    SecurityServerEnvironment env(*this, db);
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
bool SecurityServerEnvironment::verifyCodeSignature(const CodeSigning::Signature *signature,
	const CssmData *comment)
{
	return Server::codeSignatures().verify(Server::process(), signature, comment);
}


//
// PromptedAclSubject personality: Get a secret by prompting through SecurityAgent
//
bool SecurityServerEnvironment::getSecret(CssmOwnedData &secret, const CssmData &prompt) const
{
	//@@@ ignoring prompt - not used right now
	if (database) {
		QueryPIN query(*database);
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
// The default AclSource denies having an ACL at all
//
SecurityServerAcl &AclSource::acl()
{
	CssmError::throwMe(CSSM_ERRCODE_OBJECT_ACL_NOT_SUPPORTED);
}

Database *AclSource::relatedDatabase()
{
	return NULL;
}
