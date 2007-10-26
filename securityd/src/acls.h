/*
 * Copyright (c) 2000-2001,2003-2007 Apple Inc. All Rights Reserved.
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
// These classes implement securityd's local ACL machine in terms of the generic
// ObjectAcl model. In particular, they define securityd's AclValidationEnvironment,
// which hooks the real-world state into the abstract AclSubject submachines.
//
// Note that these classes are *complete* but *extendable*. The default implementation
// uses unmodified local ObjectAcl state. Subclasses (and certain AclSubjects) may delegate
// validation to outside agents (such as a tokend) and thus act as caching forwarding agents.
// Don't assume.
//
#ifndef _H_ACLS
#define _H_ACLS

#include <securityd_server/sscommon.h>
#include <security_cdsa_utilities/cssmacl.h>
#include <security_cdsa_utilities/context.h>
#include <security_cdsa_utilities/acl_process.h>
#include <security_cdsa_utilities/acl_codesigning.h>
#include <security_cdsa_utilities/acl_secret.h>
#include <security_cdsa_utilities/acl_preauth.h>
#include <security_cdsa_utilities/acl_prompted.h>
#include <security_cdsa_utilities/acl_threshold.h>

using namespace SecurityServer;


class Connection;
class Database;


//
// ACL implementation as used by the SecurityServer
//
class SecurityServerAcl : public ObjectAcl {
public:
	SecurityServerAcl() : ObjectAcl(Allocator::standard()), aclSequence(Mutex::recursive) { }
	virtual ~SecurityServerAcl();

    // validation calls restated
	virtual void validate(AclAuthorization auth, const AccessCredentials *cred, Database *relatedDatabase);
	void validate(AclAuthorization auth, const Context &context, Database *relatedDatabase);

	// CSSM layer ACL calls
	virtual void getOwner(AclOwnerPrototype &owner);
	virtual void getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls);
    virtual void changeAcl(const AclEdit &edit, const AccessCredentials *cred,
		Database *relatedDatabase);
	virtual void changeOwner(const AclOwnerPrototype &newOwner, const AccessCredentials *cred,
		Database *relatedDatabase);
	
	// to be provided by implementations
	virtual AclKind aclKind() const = 0;
	
	// a helper to (try to) add an ACL to a "standard form" item ACL
	static bool addToStandardACL(const AclValidationContext &context, AclSubject *subject);
	static bool looksLikeLegacyDotMac(const AclValidationContext &context);

	// aclSequence is taken to serialize ACL validations to pick up mutual changes
	Mutex aclSequence;
};


//
// Our implementation of an ACL validation environment uses information
// derived from a Connection object. It implements context for a fair number
// of subject types (see the inheritance list below).
//
class SecurityServerEnvironment : public virtual AclValidationEnvironment,
    public virtual ProcessAclSubject::Environment,
	public virtual CodeSignatureAclSubject::Environment,
	public virtual SecretAclSubject::Environment,
	public virtual PromptedAclSubject::Environment,
	public virtual PreAuthorizationAcls::Environment {
public:
    SecurityServerEnvironment(SecurityServerAcl &baseAcl, Database *db)
    : acl(baseAcl), database(db) { }
	
	SecurityServerAcl &acl;
	Database * const database;
    
	// personalities
    uid_t getuid() const;
    gid_t getgid() const;
	pid_t getpid() const;
	bool verifyCodeSignature(const OSXVerifier &verifier, const AclValidationContext &context);
	bool validateSecret(const SecretAclSubject *me, const AccessCredentials *cred);
	bool getSecret(CssmOwnedData &secret, const CssmData &prompt) const;
	ObjectAcl *preAuthSource();
	Adornable &store(const AclSubject *subject);
	
	// subject editing
	ThresholdAclSubject *standardSubject(const AclValidationContext &context);
};


//
// An abstract source of a SecurityServerAcl.
// There is a default implementation, which throws OBJECT_ACL_NOT_SUPPORTED.
//
class AclSource {
protected:
	AclSource() { }
	virtual ~AclSource();
	
public:
	virtual SecurityServerAcl &acl();	// defaults to "no ACL; throw exception"
	virtual Database *relatedDatabase(); // optionally, a Database related to me

	//
    // Forward ACL calls, passing some locally obtained stuff along.
	// These are virtual so an AclSource can override them. Such overrides
	// should enhance/post-process rather than replace functionality.
	//
	virtual void getOwner(AclOwnerPrototype &owner)
	{ return acl().getOwner(owner); }
	virtual void getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
	{ return acl().getAcl(tag, count, acls); }
	virtual void changeAcl(const AclEdit &edit, const AccessCredentials *cred)
	{ return acl().changeAcl(edit, cred, relatedDatabase()); }
	virtual void changeOwner(const AclOwnerPrototype &newOwner, const AccessCredentials *cred)
	{ return acl().changeOwner(newOwner, cred, relatedDatabase()); }
	virtual void validate(AclAuthorization auth, const AccessCredentials *cred)
	{ acl().validate(auth, cred, relatedDatabase()); }
	virtual void validate(AclAuthorization auth, const Context &context)
	{ acl().validate(auth, context, relatedDatabase()); }
};


#endif //_H_ACLS
