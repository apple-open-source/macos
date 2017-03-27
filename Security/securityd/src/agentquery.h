/*
 * Copyright (c) 2000-2005,2007-2010,2012-2013 Apple Inc. All Rights Reserved.
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
// passphrases - canonical code to obtain passphrases
//
#ifndef _H_AGENTQUERY
#define _H_AGENTQUERY

#include <security_cdsa_utilities/AuthorizationData.h>
#include <security_utilities/ccaudit.h> // some queries do their own authentication
#include <Security/AuthorizationPlugin.h>
#include "kcdatabase.h"
#include "authhost.h"
#include "server.h"
#include "session.h"
#include <xpc/xpc.h>

using Authorization::AuthItemSet;
using Authorization::AuthValueVector;
using Security::OSXCode;

#define kMaximumAuthorizationTries (10000)

//
// base for classes talking to com.apple.security.agent and com.apple.security.authhost 
//
class SecurityAgentXPCConnection
{
public:
    SecurityAgentXPCConnection(Session &session = Server::session());
    virtual ~SecurityAgentXPCConnection();
    virtual void activate(bool ignoreUid);
    virtual void disconnect()  { };
    virtual void terminate();
        
protected:
    RefPointer<AuthHostInstance> mHostInstance;
    Session &mSession;
    xpc_connection_t mXPCConnection;
    const RefPointer<Connection> mConnection;
    audit_token_t *mAuditToken;
    uid_t mNobodyUID;

    bool inDarkWake();

};


//
// The main com.apple.security.agent interaction base class
//
class SecurityAgentXPCQuery : public SecurityAgentXPCConnection
{
public:
    static void killAllXPCClients();
	
    typedef SecurityAgent::Reason Reason;
	
	SecurityAgentXPCQuery(Session &session = Server::session());
	
    
	void inferHints(Process &thisProcess);
	void addHint(const char *name, const void *value = NULL, UInt32 valueLen = 0, UInt32 flags = 0);
    
	virtual ~SecurityAgentXPCQuery();
    
	virtual void disconnect();
	virtual void terminate();
	void create(const char *pluginId, const char *mechanismId);
    OSStatus invoke();
    void setTerminateOnSleep(bool terminateOnSleep) {mTerminateOnSleep = terminateOnSleep;}
    bool getTerminateOnSleep() {return mTerminateOnSleep;}
    void setInput(const AuthItemSet& inHints, const AuthItemSet& inContext) { mInHints = inHints; mInContext = inContext; }
    void checkResult();

	void readChoice();
    
	bool allow;
	bool remember;
    
protected:
	AuthItemSet mClientHints;
    AuthItemSet mImmutableHints;
    AuthItemSet mInHints;
    AuthItemSet mInContext;
    AuthItemSet mOutHints;
    AuthItemSet mOutContext;
    bool mAgentConnected;
    uint64_t mLastResult;
    bool mTerminateOnSleep;
};

//
// Specialized for "rogue app" alert queries
//
class QueryKeychainUse : public SecurityAgentXPCQuery {
public:
    QueryKeychainUse(bool needPass, const Database *db);
    Reason queryUser (const char* database, const char *description, AclAuthorization action);

private:
    const KeychainDatabase *mPassphraseCheck; // NULL to not check passphrase
};


//
// A query for an existing passphrase
//
class QueryOld : public SecurityAgentXPCQuery {
	static const int maxTries = kMaximumAuthorizationTries;
public:
	QueryOld(Database &db) : database(db) {setTerminateOnSleep(true);}
	
	Database &database;
	
	Reason operator () ();
	
protected:
	Reason query();
	virtual Reason accept(CssmManagedData &) = 0;
};


class QueryUnlock : public QueryOld {
public:
	QueryUnlock(KeychainDatabase &db) : QueryOld(db) { }
    Reason retrievePassword(CssmOwnedData &passphrase);
	
protected:
	Reason accept(CssmManagedData &passphrase);
};


class QueryKeybagPassphrase : public SecurityAgentXPCQuery {
public:
    QueryKeybagPassphrase(Session &session, int32_t retries = kMaximumAuthorizationTries);

    Reason query();
    Reason accept(CssmManagedData &passphrase);
protected:
    Session &mSession;
    service_context_t mContext;
    int32_t mRetries;
};

class QueryKeybagNewPassphrase : public QueryKeybagPassphrase {
public:
    QueryKeybagNewPassphrase(Session &session);

    Reason query(CssmOwnedData &oldPassphrase, CssmOwnedData &passphrase);
};

//
// Repurpose QueryUnlock for PIN prompting
// Not very clean - but this stuff is an outdated hack as it is...
//
class QueryPIN : public QueryOld {
public:
	QueryPIN(Database &db);
	
	const CssmData &pin() const { return mPin; }

protected:
	Reason accept(CssmManagedData &pin);
	
private:
	CssmAutoData mPin;		// PIN obtained
};


//
// A query for a new passphrase
//
class QueryNewPassphrase : public SecurityAgentXPCQuery {
	static const int maxTries = kMaximumAuthorizationTries;
public:
	QueryNewPassphrase(Database &db, Reason reason) :
	    database(db), initialReason(reason),
	    mPassphrase(Allocator::standard(Allocator::sensitive)),
        mOldPassphrase(Allocator::standard(Allocator::sensitive)),
	    mPassphraseValid(false) { }

	Database &database;
	
	Reason operator () (CssmOwnedData &oldPassphrase, CssmOwnedData &passphrase);
	
protected:
	Reason query();
	virtual Reason accept(CssmManagedData &passphrase, CssmData *oldPassphrase);
	
private:
	Reason initialReason;
	CssmAutoData mPassphrase;
    CssmAutoData mOldPassphrase;
    bool mPassphraseValid;
};


//
// Generic passphrase query (not associated with a database)
//
class QueryGenericPassphrase : public SecurityAgentXPCQuery {
public:
    QueryGenericPassphrase()    { }
    Reason operator () (const CssmData *prompt, bool verify,
                        string &passphrase);
    
protected:
    Reason query(const CssmData *prompt, bool verify, string &passphrase);
};


//
// Generic secret query (not associated with a database)
//
class QueryDBBlobSecret : public SecurityAgentXPCQuery {
	static const int maxTries = kMaximumAuthorizationTries;
public:
    QueryDBBlobSecret()    { }
    Reason operator () (DbHandle *dbHandleArray, uint8 dbHandleArrayCount, DbHandle *dbHandleAuthenticated);
    
protected:
    Reason query(DbHandle *dbHandleArray, uint8 dbHandleArrayCount, DbHandle *dbHandleAuthenticated);
	Reason accept(CssmManagedData &passphrase, DbHandle *dbHandlesToAuthenticate, uint8 dbHandleCount, DbHandle *dbHandleAuthenticated);
};

// hybrid of confirm-access and generic authentication queries, for
// securityd's use; keep the Frankenstein references to yourself
// (the alternative is to ask the user to unlock the system keychain,
// and you don't want that, do you?)  
class QueryKeychainAuth : public SecurityAgentXPCQuery {
	static const int maxTries = kMaximumAuthorizationTries;
public:
    QueryKeychainAuth()  { }
    // "prompt" can be NULL
    Reason operator () (const char *database, const char *description, AclAuthorization action, const char *prompt);
    Reason accept(string &username, string &passphrase);
};

#endif //_H_AGENTQUERY
