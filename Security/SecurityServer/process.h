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
// process - track a single client process and its belongings
//
#ifndef _H_PROCESS
#define _H_PROCESS

#include "securityserver.h"
#include "SecurityAgentClient.h"
#include <Security/refcount.h>
#include "key.h"
#include "codesigdb.h"
#include "notifications.h"
#include <string>

using MachPlusPlus::Port;
using MachPlusPlus::TaskPort;

class Session;
class AuthorizationToken;


//
// A Process object represents a UNIX process (and associated Mach Task) that has
// had contact with us and may have some state associated with it.
//
class Process : public CodeSignatures::Identity {
public:
	Process(Port servicePort, TaskPort tPort,
		const ClientSetupInfo *info, const char *identity,
		uid_t uid, gid_t gid);
	virtual ~Process();
    
    uid_t uid() const			{ return mUid; }
    gid_t gid() const			{ return mGid; }
    pid_t pid() const			{ return mPid; }
    TaskPort taskPort() const	{ return mTaskPort; }
	bool byteFlipped() const	{ return mByteFlipped; }
	
	CodeSigning::OSXCode *clientCode() const { return (mClientIdent == unknown) ? NULL : mClientCode; }
	
	void addAuthorization(AuthorizationToken *auth);
	void checkAuthorization(AuthorizationToken *auth);
	bool removeAuthorization(AuthorizationToken *auth);
	
	void beginConnection(Connection &);
	bool endConnection(Connection &);
	bool kill(bool keepTaskPort = false);
    
    void addDatabase(Database *database);
    void removeDatabase(Database *database);
    
    void requestNotifications(Port port, Listener::Domain domain, Listener::EventMask events);
    void stopNotifications(Port port);
    void postNotification(Listener::Domain domain, Listener::Event event, const CssmData &data);
    
    Session &session;

	// aclSequence is taken to serialize ACL validations to pick up mutual changes
	Mutex aclSequence;
	
protected:
	std::string getPath() const;
	const CssmData getHash(CodeSigning::OSXSigner &signer) const;
	
private:
	Mutex mLock;						// object lock
	uint32 mBusyCount;					// number of Connection references
	bool mDying;						// process is dead; waiting for Connections to drain

	// peer state: established during connection startup; fixed thereafter
    TaskPort mTaskPort;					// task port
	bool mByteFlipped;					// client's byte order is reverse of ours
    pid_t mPid;							// process id
    uid_t mUid;							// UNIX uid credential
    gid_t mGid;							// primary UNIX gid credential
	
	RefPointer<CodeSigning::OSXCode> mClientCode; // code object for client (NULL if unknown)
	mutable enum { deferred, known, unknown } mClientIdent; // state of client identity
	mutable auto_ptr<CodeSigning::Signature> mCachedSignature; // cached signature (if already known)
	
	// authorization dictionary
	typedef multiset<AuthorizationToken *> AuthorizationSet;
	AuthorizationSet mAuthorizations;	// set of valid authorizations for process
    
    // database dictionary
    typedef set<Database *> DatabaseSet;
    DatabaseSet mDatabases;				// set of valid database handles
};


#endif //_H_PROCESS
