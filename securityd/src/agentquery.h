/*
 * Copyright (c) 2000-2004,2008-2009 Apple Inc. All Rights Reserved.
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

#include <security_agent_client/agentclient.h>
#include <security_cdsa_utilities/AuthorizationData.h>
#include <security_utilities/ccaudit.h> // some queries do their own authentication
#include <Security/AuthorizationPlugin.h>
#include "kcdatabase.h"
#include "AuthorizationEngine.h"
#include "authhost.h"
#include "server.h"
#include "session.h"

using Authorization::AuthItemSet;
using Authorization::AuthValueVector;
using Security::OSXCode;

//
// base for classes talking to SecurityAgent and authorizationhost
//
class SecurityAgentConnection : public SecurityAgent::Client,
                                public SecurityAgentConnectionInterface
{
public:
    SecurityAgentConnection(const AuthHostType type = securityAgent, Session &session = Server::session());
    virtual ~SecurityAgentConnection();
    virtual void activate();
    virtual void reconnect();
    virtual void disconnect()  { };
    virtual void terminate();
    
    AuthHostType hostType()  { return mAuthHostType; }
    
protected:
    AuthHostType mAuthHostType;
    RefPointer<AuthHostInstance> mHostInstance;
    Port mPort;
    const RefPointer<Connection> mConnection;
    audit_token_t *mAuditToken;
};

//
// Special wrapper around SecurityAgent::Client transaction interfaces.  
// Not currently used because this was intended to support 
// SecurityAgent's/authorizationhost's use of Foundation's enable/disable-sudden-
// termination APIs, but the latter don't work for non-direct children of 
// launchd.  Kept around because securityd might need its own child-transaction 
// semantics one day.  
//
class SecurityAgentTransaction : public SecurityAgentConnection
{
public: 
    SecurityAgentTransaction(const AuthHostType type = securityAgent, Session &session = Server::session(), bool startNow = true);
    ~SecurityAgentTransaction();
    
    void start();
    void end();
    bool started()  { return mStarted; }
    
private:
    bool mStarted;
};

//
// The main SecurityAgent/authorizationhost interaction base class
//
class SecurityAgentQuery : public SecurityAgentConnection
{
public:
	typedef SecurityAgent::Reason Reason;
	
	SecurityAgentQuery(const AuthHostType type = securityAgent, Session &session = Server::session());
	

	void inferHints(Process &thisProcess);
	void addHint(const char *name, const void *value = NULL, UInt32 valueLen = 0, UInt32 flags = 0);

	virtual ~SecurityAgentQuery();

	virtual void disconnect();
	virtual void terminate();
	void create(const char *pluginId, const char *mechanismId, const SessionId inSessionId);

	void readChoice();

	bool allow;
	bool remember;

protected:
	AuthItemSet mClientHints;
};

//
// Specialized for "rogue app" alert queries
//
class QueryKeychainUse : public SecurityAgentQuery {
public:
    QueryKeychainUse(bool needPass, const Database *db);
    Reason queryUser (const char* database, const char *description, AclAuthorization action);

private:
    const KeychainDatabase *mPassphraseCheck; // NULL to not check passphrase
};


//
// Specialized for code signature adjustment queries
//
class QueryCodeCheck : public SecurityAgentQuery {
public:
    bool operator () (const char *aclPath);
};


//
// A query for an existing passphrase
//
class QueryOld : public SecurityAgentQuery {
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
	
protected:
	Reason accept(CssmManagedData &passphrase);
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
class QueryNewPassphrase : public SecurityAgentQuery {
	static const int maxTries = kMaximumAuthorizationTries;
public:
	QueryNewPassphrase(Database &db, Reason reason) :
	    database(db), initialReason(reason),
	    mPassphrase(Allocator::standard(Allocator::sensitive)),
	    mPassphraseValid(false) { }

	Database &database;
	
	Reason operator () (CssmOwnedData &passphrase);
	
protected:
	Reason query();
	virtual Reason accept(CssmManagedData &passphrase, CssmData *oldPassphrase);
	
private:
	Reason initialReason;
	CssmAutoData mPassphrase;
    bool mPassphraseValid;
};


//
// Generic passphrase query (not associated with a database)
//
class QueryGenericPassphrase : public SecurityAgentQuery {
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
class QueryDBBlobSecret : public SecurityAgentQuery {
	static const int maxTries = kMaximumAuthorizationTries;
public:
    QueryDBBlobSecret()    { }
    Reason operator () (DbHandle *dbHandleArray, uint8 dbHandleArrayCount, DbHandle *dbHandleAuthenticated);
    
protected:
    Reason query(DbHandle *dbHandleArray, uint8 dbHandleArrayCount, DbHandle *dbHandleAuthenticated);
	Reason accept(CssmManagedData &passphrase, DbHandle *dbHandlesToAuthenticate, uint8 dbHandleCount, DbHandle *dbHandleAuthenticated);
};

class QueryInvokeMechanism : public SecurityAgentQuery, public RefCount {
public:
	QueryInvokeMechanism(const AuthHostType type, Session &session);
    void initialize(const string &inPluginId, const string &inMechanismId, const AuthValueVector &arguments, const SessionId inSessionId = 0);
    void run(const AuthValueVector &inArguments, AuthItemSet &inHints, AuthItemSet &inContext, AuthorizationResult *outResult);

    bool operator () (const string &inPluginId, const string &inMechanismId, const Authorization::AuthValueVector &inArguments, AuthItemSet &inHints, AuthItemSet &inContext, AuthorizationResult *outResult);
    void terminateAgent();
    //~QueryInvokeMechanism();

    AuthValueVector mArguments;
};

// hybrid of confirm-access and generic authentication queries, for
// securityd's use; keep the Frankenstein references to yourself
// (the alternative is to ask the user to unlock the system keychain,
// and you don't want that, do you?)  
class QueryKeychainAuth : public SecurityAgentQuery {
	static const int maxTries = kMaximumAuthorizationTries;
public:
    QueryKeychainAuth()  { }
    // "prompt" can be NULL
    Reason operator () (const char *database, const char *description, AclAuthorization action, const char *prompt);
    Reason accept(string &username, string &passphrase);
};

#endif //_H_AGENTQUERY
