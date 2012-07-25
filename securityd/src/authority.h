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
// authority - authorization manager
//
#ifndef _H_AUTHORITY
#define _H_AUTHORITY

#include <security_utilities/osxcode.h>
#include <security_utilities/ccaudit.h>
#include "database.h"
#include "credential.h"
#include <security_cdsa_utilities/AuthorizationData.h>

using Authorization::AuthItemSet;
using Authorization::Credential;
using Authorization::CredentialSet;
using Security::CommonCriteria::AuditToken;

class Process;
class Session;

class AuthorizationToken : public PerSession {
public:
	AuthorizationToken(Session &ssn, const CredentialSet &base, const audit_token_t &auditToken, bool operateAsLeastPrivileged = false);
	~AuthorizationToken();

    Session &session() const;
	
	const AuthorizationBlob &handle() const		{ return mHandle; }
	const CredentialSet &baseCreds() const		{ return mBaseCreds; }
	CredentialSet effectiveCreds() const;
	
	typedef CredentialSet::iterator iterator;
	iterator begin()		{ return mBaseCreds.begin(); }
	iterator end()			{ return mBaseCreds.end(); }
	
	// add more credential dependencies
	void mergeCredentials(const CredentialSet &more);
	
	// maintain process-owning links
	void addProcess(Process &proc);
	bool endProcess(Process &proc);
	
	// access control for external representations
	bool mayExternalize(Process &proc) const;
	bool mayInternalize(Process &proc, bool countIt = true);

	uid_t creatorUid() const	{ return mCreatorUid; }
	gid_t creatorGid() const	{ return mCreatorGid; }
    SecStaticCodeRef creatorCode() const { return mCreatorCode; }
	pid_t creatorPid() const	{ return mCreatorPid; }
	bool creatorSandboxed() const { return mCreatorSandboxed; }
	
	const AuditToken &creatorAuditToken() const { return mCreatorAuditToken; }
	
	AuthItemSet infoSet(AuthorizationString tag = NULL);
    void setInfoSet(AuthItemSet &newInfoSet, bool savePassword);
    void setCredentialInfo(const Credential &inCred, bool savePassword);
    void clearInfoSet();
	void scrubInfoSet(bool savePassword);
	bool operatesAsLeastPrivileged() const { return mOperatesAsLeastPrivileged; }

public:
	static AuthorizationToken &find(const AuthorizationBlob &blob);
    
    class Deleter {
    public:
        Deleter(const AuthorizationBlob &blob);
        
        void remove();
        operator AuthorizationToken &() const	{ return *mAuth; }
        
    private:
        RefPointer<AuthorizationToken> mAuth;
        StLock<Mutex> lock;
    };

private:
	Mutex mLock;					// object lock
	AuthorizationBlob mHandle;		// official randomized blob marker
	CredentialSet mBaseCreds;		// credentials we're based on
	
	unsigned int mTransferCount;	// number of internalizations remaining
	
	typedef set<Process *> ProcessSet;
	ProcessSet mUsingProcesses;		// set of process objects using this token

	uid_t mCreatorUid;				// Uid of process that created this authorization
	gid_t mCreatorGid;				// Gid of process that created this authorization
	CFCopyRef<SecStaticCodeRef> mCreatorCode; // code reference to creator
	pid_t mCreatorPid;				// Pid of processs that created this authorization
	bool mCreatorSandboxed;         // A record of whether or not the creator was Sandboxed
	
	AuditToken mCreatorAuditToken;	// Audit token of the process that created this authorization

    AuthItemSet mInfoSet;			// Side band info gathered from evaluations in this session

	bool mOperatesAsLeastPrivileged;

	AuthItemSet mSavedPassword;

private:
	typedef map<AuthorizationBlob, RefPointer<AuthorizationToken> > AuthMap;
	static AuthMap &authMap;			// set of extant authorizations
    static Mutex authMapLock;		// lock for mAuthorizations (only)
};

#endif //_H_AUTHORITY
