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
// passphrases - canonical code to obtain passphrases
//
#ifndef _H_AGENTQUERY
#define _H_AGENTQUERY

#include <security_agent_client/agentclient.h>
#include <security_cdsa_utilities/AuthorizationData.h>
#include <Security/AuthorizationPlugin.h>
#include "kcdatabase.h"
#include "AuthorizationEngine.h"
#include "authhost.h"
#include "server.h"
#include "session.h"

using Authorization::AuthItemSet;
using Authorization::AuthValueVector;
using Security::OSXCode;

class SecurityAgentQuery : public SecurityAgent::Client {
public:
	typedef SecurityAgent::Reason Reason;
	
	SecurityAgentQuery(const AuthHostType type = securityAgent, Session &session = Server::session());
	
	void inferHints(Process &thisProcess);
    void addHint(const char *name, const void *value = NULL, UInt32 valueLen = 0, UInt32 flags = 0);

	virtual ~SecurityAgentQuery();

	virtual void activate();
	virtual void terminate();
	void create(const char *pluginId, const char *mechanismId, const SessionId inSessionId);

public:
	void readChoice();

	bool allow;
	bool remember;
	AuthHostType mAuthHostType;
	RefPointer<AuthHostInstance> mHostInstance;

protected:
	AuthItemSet mClientHints;
private:
	Port mPort;
    const RefPointer<Connection> mConnection;
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
	QueryOld(Database &db) : database(db) { }
	
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
	static const int maxTries = 7;
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
    Reason operator () (const char *prompt, bool verify, 
                        string &passphrase);
    
protected:
    Reason query(const char *prompt, bool verify, string &passphrase);
};


//
// Generic secret query (not associated with a database)
//
class QueryDBBlobSecret : public SecurityAgentQuery {
	static const int maxTries = kMaximumAuthorizationTries;
public:
    QueryDBBlobSecret()    { }
    Reason operator () (DatabaseCryptoCore &dbCore, const DbBlob *secretsBlob);
    
protected:
    Reason query(DatabaseCryptoCore &dbCore, const DbBlob *secretsBlob);
	Reason accept(CssmManagedData &passphrase, DatabaseCryptoCore &dbCore, const DbBlob *secretsBlob);
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

#endif //_H_AGENTQUERY
