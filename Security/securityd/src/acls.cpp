/*
 * Copyright (c) 2000-2009,2012-2016 Apple Inc. All Rights Reserved.
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
#include "acl_partition.h"

// ACL subjects whose Environments we implement
#include <security_cdsa_utilities/acl_any.h>
#include <security_cdsa_utilities/acl_password.h>
#include "acl_keychain.h"

#include <sys/sysctl.h>
#include <security_utilities/logging.h>
#include <security_utilities/cfmunge.h>

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

    // if we're setting the INTEGRITY entry, check if you're in the partition list.
    if (const AclEntryInput* input = edit.newEntry()) {
        if (input->proto().authorization().containsOnly(CSSM_ACL_AUTHORIZATION_INTEGRITY)) {
            // Only prompt the user if these creds allow UI.
            bool ui = (!!cred) && cred->authorizesUI();
            validatePartition(env, ui); // throws if fail

            // If you passed partition validation, bypass the owner ACL check entirely.
            env.forceSuccess = true;
        }
    }

    // If these access credentials, by themselves, protect this database, force success and don't
    // restrict changing PARTITION_ID
    if(db && db->checkCredentials(cred)) {
        env.forceSuccess = true;
        ObjectAcl::cssmChangeAcl(edit, cred, &env, NULL);
    } else {
        ObjectAcl::cssmChangeAcl(edit, cred, &env, CSSM_APPLE_ACL_TAG_PARTITION_ID);
    }
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

	StLock<Mutex> objectSequence(aclSequence);
	StLock<Mutex> processSequence(Server::process().aclSequence);
	ObjectAcl::validate(auth, cred, &env);

    // partition validation happens outside the normal acl validation flow, in addition
    bool ui = (!!cred) && cred->authorizesUI();

    // we should only offer the chance to extend the partition ID list on a "read" operation, so check the AclAuthorization
    bool readOperation =
        (auth == CSSM_ACL_AUTHORIZATION_CHANGE_ACL)     ||
        (auth == CSSM_ACL_AUTHORIZATION_DECRYPT)        ||
        (auth == CSSM_ACL_AUTHORIZATION_GENKEY)         ||
        (auth == CSSM_ACL_AUTHORIZATION_EXPORT_WRAPPED) ||
        (auth == CSSM_ACL_AUTHORIZATION_EXPORT_CLEAR)   ||
        (auth == CSSM_ACL_AUTHORIZATION_IMPORT_WRAPPED) ||
        (auth == CSSM_ACL_AUTHORIZATION_IMPORT_CLEAR)   ||
        (auth == CSSM_ACL_AUTHORIZATION_SIGN)           ||
        (auth == CSSM_ACL_AUTHORIZATION_DECRYPT)        ||
        (auth == CSSM_ACL_AUTHORIZATION_MAC)            ||
        (auth == CSSM_ACL_AUTHORIZATION_DERIVE);

    validatePartition(env, ui && readOperation);
}

void SecurityServerAcl::validate(AclAuthorization auth, const Context &context, Database *db)
{
	validate(auth,
		context.get<AccessCredentials>(CSSM_ATTRIBUTE_ACCESS_CREDENTIALS), db);
}


//
// Partitioning support
//
void SecurityServerAcl::validatePartition(SecurityServerEnvironment& env, bool prompt)
{
    // Calling checkAppleSigned() early at boot on a clean system install
    // will end up trying to create the system keychain and causes a hang.
    // Avoid this by checking for the presence of the db first.
    if((!env.database) || env.database->dbVersion() < SecurityServer::CommonBlob::version_partition) {
        secinfo("integrity", "no db or old db version, skipping");
        return;
    }

    // For the Keychain Migrator, don't even check the partition list
    Process &process = Server::process();
    if (process.checkAppleSigned() && process.hasEntitlement(migrationEntitlement)) {
        secnotice("integrity", "bypassing partition check for keychain migrator");
        return;   // migrator client -> automatic win
    }

	if (CFRef<CFDictionaryRef> partition = this->createPartitionPayload()) {
		CFArrayRef partitionList;
		if (cfscan(partition, "{Partitions=%AO}", &partitionList)) {
			CFRef<CFStringRef> partitionDebug = CFCopyDescription(partitionList);	// for debugging only
			secnotice("integrity", "ACL partitionID = %s", cfString(partitionDebug).c_str());
			if (env.database) {
				CFRef<CFStringRef> clientPartitionID = makeCFString(env.database->process().partitionId());
				if (CFArrayContainsValue(partitionList, CFRangeMake(0, CFArrayGetCount(partitionList)), clientPartitionID)) {
					secnotice("integrity", "ACL partitions match: %s", cfString(clientPartitionID).c_str());
					return;
				} else {
					secnotice("integrity", "ACL partition mismatch: client %s ACL %s", cfString(clientPartitionID).c_str(), cfString(partitionDebug).c_str());
					if (prompt && extendPartition(env))
						return;
					MacOSError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);
				}
			}
		}
		secnotice("integrity", "failed to parse partition payload");
		MacOSError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
    } else {
        // There's no partition list. This keychain is recently upgraded.
        Server::connection().overrideReturn(CSSMERR_CSP_APPLE_ADD_APPLICATION_ACL_SUBJECT);
        if(env.database->isRecoding()) {
            secnotice("integrity", "no partition ACL - database is recoding; skipping add");
            // let this pass as well
        } else {
            secnotice("integrity", "no partition ACL - adding");
            env.acl.instantiateAcl();
            this->createClientPartitionID(env.database->process());
            env.acl.changedAcl();
            Server::connection().overrideReturn(CSSMERR_CSP_APPLE_ADD_APPLICATION_ACL_SUBJECT);
        }
    }
}


bool SecurityServerAcl::extendPartition(SecurityServerEnvironment& env)
{
	// brute-force find the KeychainAclSubject in the ACL
	KeychainPromptAclSubject *kcSubject = NULL;
	SecurityServerAcl& acl = env.acl;
	for (EntryMap::const_iterator it = acl.begin(); it != acl.end(); ++it) {
		AclSubjectPointer subject = it->second.subject;
		if (ThresholdAclSubject *threshold = dynamic_cast<ThresholdAclSubject *>(subject.get())) {
			unsigned size = threshold->count();
			if (KeychainPromptAclSubject* last = dynamic_cast<KeychainPromptAclSubject *>(threshold->subject(size-1))) {
				// looks standard enough
				kcSubject = last;
				break;
			}
		}
	}

	if (kcSubject) {
		BaseValidationContext ctx(NULL, CSSM_ACL_AUTHORIZATION_PARTITION_ID, &env);
        kcSubject->addPromptAttempt();
		return kcSubject->validateExplicitly(ctx, ^{
            secnotice("integrity", "adding partition to list");
            env.acl.instantiateAcl();
			this->addClientPartitionID(env.database->process());
			env.acl.changedAcl();
			// trigger a special notification code on (otherwise successful) return
			Server::connection().overrideReturn(CSSMERR_CSP_APPLE_ADD_APPLICATION_ACL_SUBJECT);
		});
	}
    secnotice("integrity", "failure extending partition");
	return false;
}


PartitionAclSubject* SecurityServerAcl::findPartitionSubject()
{
	pair<EntryMap::const_iterator, EntryMap::const_iterator> range;
	switch (this->getRange(CSSM_APPLE_ACL_TAG_PARTITION_ID, range, true)) {
		case 0:
			secnotice("integrity", "no partition tag on ACL");
			return NULL;
		default:
			secnotice("integrity", "multiple partition ACL entries");
			MacOSError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
		case 1:
			break;
	}
	const AclEntry& entry = range.first->second;
	if (!entry.authorizes(CSSM_ACL_AUTHORIZATION_PARTITION_ID)) {
		secnotice("integrity", "partition entry does not authorize CSSM_ACL_AUTHORIZATION_PARTITION_ID");
		MacOSError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
	}
	if (PartitionAclSubject* partition = dynamic_cast<PartitionAclSubject*>(entry.subject.get())) {
		return partition;
	} else {
		secnotice("integrity", "partition entry is not PartitionAclSubject");
		MacOSError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
	}
}


CFDictionaryRef SecurityServerAcl::createPartitionPayload()
{
	if (PartitionAclSubject* subject = this->findPartitionSubject()) {
		if (CFDictionaryRef result = subject->createDictionaryPayload()) {
			return result;
		} else {
			secnotice("integrity", "partition entry is malformed XML");
			MacOSError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
		}
	} else {
		return NULL;
	}
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
				secinfo("acl", "adding new subject %p to from of threshold ACL", subject);
				threshold->add(subject, 0);

				// tell the ACL it's been modified
				context.acl()->changedAcl();

				// trigger a special notification code on (otherwise successful) return
				Server::connection().overrideReturn(CSSMERR_CSP_APPLE_ADD_APPLICATION_ACL_SUBJECT);
				return true;
			}
		}
	secinfo("acl", "ACL is not standard form; cannot edit");
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
			secinfo("codesign", "matched %d of %zd candididates (threshold=%d)",
				matches, sizeof(prototypicalDotMacPath) / sizeof(char *) - 1, threshold);
			return matches >= threshold;
		}
	}
	return false;
}


//
// ACL manipulations related to keychain partitions
//
bool SecurityServerAcl::createClientPartitionID(Process& process)
{
    // Make sure the ACL is ready for edits
    instantiateAcl();

	// create partition payload
	std::string partitionID = process.partitionId();
	CFTemp<CFDictionaryRef> payload("{Partitions=[%s]}", partitionID.c_str());
	ObjectAcl::AclSubjectPointer subject = new PartitionAclSubject();
	static_cast<PartitionAclSubject*>(subject.get())->setDictionaryPayload(Allocator::standard(), payload);
	ObjectAcl::AclEntry partition(subject);
	partition.addAuthorization(CSSM_ACL_AUTHORIZATION_PARTITION_ID);
	this->add(CSSM_APPLE_ACL_TAG_PARTITION_ID, partition);
	secnotice("integrity", "added partition %s to new key", partitionID.c_str());
	return true;
}


bool SecurityServerAcl::addClientPartitionID(Process& process)
{
	if (PartitionAclSubject* subject = this->findPartitionSubject()) {
		std::string partitionID = process.partitionId();
		if (CFRef<CFDictionaryRef> payload = subject->createDictionaryPayload()) {
			CFArrayRef partitionList;
			if (cfscan(payload, "{Partitions=%AO}", &partitionList)) {
				CFTemp<CFDictionaryRef> newPayload("{Partitions=[+%O,%s]}", partitionList, partitionID.c_str());
				subject->setDictionaryPayload(Allocator::standard(), newPayload);
			}
			return true;
		} else {
			MacOSError::throwMe(CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE);
		}
	} else {
		return createClientPartitionID(process);
	}
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
