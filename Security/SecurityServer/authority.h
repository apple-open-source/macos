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
// authority - authorization manager
//
#ifndef _H_AUTHORITY
#define _H_AUTHORITY

#include "securityserver.h"
#include "AuthorizationEngine.h"

using Authorization::Credential;
using Authorization::CredentialSet;
using Authorization::AuthItemSet;

class Process;
class Session;


class AuthorizationToken {
public:
	AuthorizationToken(Session &ssn, const CredentialSet &base, const audit_token_t &auditToken);
	~AuthorizationToken();

    Session &session;
	
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
	uid_t creatorGid() const	{ return mCreatorGid; }
    CodeSigning::OSXCode *creatorCode() const { return mCreatorCode; }
	pid_t creatorPid() const	{ return mCreatorPid; }
	
	audit_token_t creatorAuditToken() const {return mCreatorAuditToken; }

	AuthItemSet infoSet(AuthorizationString tag = NULL);
    void setInfoSet(AuthItemSet &newInfoSet);
    void setCredentialInfo(const Credential &inCred);
    void clearInfoSet();

public:
	static AuthorizationToken &find(const AuthorizationBlob &blob);
    
    class Deleter {
    public:
        Deleter(const AuthorizationBlob &blob);
        
        void remove();
        operator AuthorizationToken &() const	{ return *mAuth; }
        
    private:
        AuthorizationToken *mAuth;
        StLock<Mutex> lock;
    };

private:
	Mutex mLock;					// object lock
	AuthorizationBlob mHandle;		// official randomized blob marker
	CredentialSet mBaseCreds;		// credentials we're based on
	
	unsigned int mTransferCount;	// number of internalizations remaining
	
	typedef set<Process *> ProcessSet;
	ProcessSet mUsingProcesses;		// set of process objects using this token

	uid_t mCreatorUid;				// Uid of proccess that created this authorization
	gid_t mCreatorGid;				// Gid of proccess that created this authorization
    RefPointer<OSXCode> mCreatorCode; // code id of creator
	pid_t mCreatorPid;				// Pid of processs that created this authorization

	audit_token_t mCreatorAuditToken;	// Audit token of the process that created this authorization

    AuthItemSet mInfoSet;			// Side band info gathered from evaluations in this session

private:
	typedef map<AuthorizationBlob, AuthorizationToken *> AuthMap;
	static AuthMap authMap;			// set of extant authorizations
    static Mutex authMapLock;		// lock for mAuthorizations (only)
};


//
// The authority itself. You will usually only have one of these.
//
class Authority : public Authorization::Engine {
public:
	Authority(const char *configFile);
	~Authority();
};


#endif //_H_AUTHORITY
