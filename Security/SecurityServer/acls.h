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
// acls - SecurityServer ACL implementation
//
#ifndef _H_ACLS
#define _H_ACLS

#include "securityserver.h"
#include <Security/cssmacl.h>
#include <Security/acl_process.h>
#include <Security/acl_codesigning.h>


class Connection;
class Database;


//
// ACL implementation as used by the SecurityServer
//
class SecurityServerAcl : public ObjectAcl {
public:
	SecurityServerAcl(AclKind k, CssmAllocator &alloc) :ObjectAcl(alloc), mKind(k) { }
	virtual ~SecurityServerAcl();
	
	AclKind kind() const { return mKind; }

    // validation calls restated
	void validate(AclAuthorization auth, const AccessCredentials *cred) const;
	void validate(AclAuthorization auth, const Context &context) const;

	void cssmGetAcl(const char *tag, uint32 &count, AclEntryInfo * &acls);
    void cssmGetOwner(AclOwnerPrototype &owner);
    void cssmChangeAcl(const AclEdit &edit, const AccessCredentials *cred);
    void cssmChangeOwner(const AclOwnerPrototype &newOwner, const AccessCredentials *cred);
	
	virtual void instantiateAcl() = 0;
	virtual void noticeAclChange() = 0;
	virtual const Database *relatedDatabase() const;
	
public:
	static bool getBatchPassphrase(const AccessCredentials *cred,
		CSSM_SAMPLE_TYPE neededSampleType, CssmOwnedData &passphrase);

private:
	AclKind mKind;
};


//
// Our implementation of an ACL validation environment uses information
// derived from a Connection object. It implements context for
// -- ProcessAclSubjects (getuid/getgid)
// -- KeychainPromptAclSubjects (connection link)
//
class SecurityServerEnvironment : public virtual AclValidationEnvironment,
    public virtual ProcessAclSubject::Environment,
	public virtual CodeSignatureAclSubject::Environment {
public:
    SecurityServerEnvironment(const SecurityServerAcl &baseAcl)
    : acl(baseAcl) { }
	
	const SecurityServerAcl &acl;
    
	const Database *database() const		{ return acl.relatedDatabase(); }
    uid_t getuid() const;
    gid_t getgid() const;
	pid_t getpid() const;
	bool verifyCodeSignature(const CodeSigning::Signature *signature);
};


#endif //_H_ACLS
