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
// passphrases - canonical code to obtain passphrases
//
#ifndef _H_AGENTQUERY
#define _H_AGENTQUERY

#include "securityserver.h"
#include "xdatabase.h"
#include <Security/utilities.h>
#include "SecurityAgentClient.h"
#include "AuthorizationData.h"

using Authorization::AuthItemSet;

//
// The common machinery of retryable SecurityAgent queries
//
class Session;

class SecurityAgentQuery : protected SecurityAgent::Client {
public:
	typedef SecurityAgent::Reason Reason;
	
	static const char defaultName[];

	SecurityAgentQuery();
	SecurityAgentQuery(uid_t clientUID, Session &clientSession, const char *agentName = defaultName);
	virtual ~SecurityAgentQuery();

	virtual void activate();
	virtual void terminate();

private:
	Session &mClientSession;
};


//
// Specialized for "rogue app" alert queries
//
class QueryKeychainUse : public SecurityAgent::Client::KeychainChoice, public SecurityAgentQuery {
public:
    QueryKeychainUse(bool needPass)	: needPassphrase(needPass) { }
    void queryUser (const Database *db, const char* database, const char *description, AclAuthorization action);
	~QueryKeychainUse();
	
	const bool needPassphrase;
};


//
// Specialized for code signature adjustment queries
//
class QueryCodeCheck : public SecurityAgent::Client::KeychainChoice, public SecurityAgentQuery {
public:
    void operator () (const char *aclPath);
};


//
// A query for an existing passphrase
//
class QueryUnlock : public SecurityAgentQuery {
	static const int maxTries = kMaximumAuthorizationTries;
public:
	QueryUnlock(Database &db) : database(db) { }
	
	Database &database;
	
	Reason operator () ();
	
protected:
	Reason query();
	void queryInteractive(CssmOwnedData &passphrase);
	void retryInteractive(CssmOwnedData &passphrase, Reason reason);
	Reason accept(CssmManagedData &passphrase);
};


//
// A query for a new passphrase
//
class QueryNewPassphrase : public SecurityAgentQuery {
	static const int maxTries = 7;
public:
	QueryNewPassphrase(Database &db, Reason reason) :
	    database(db), initialReason(reason),
	    mPassphrase(CssmAllocator::standard(CssmAllocator::sensitive)),
	    mPassphraseValid(false) { }

	Database &database;
	
	Reason operator () (CssmOwnedData &passphrase);
	
protected:
	Reason query();
	void queryInteractive(CssmOwnedData &passphrase, CssmOwnedData &oldPassphrase);
	void retryInteractive(CssmOwnedData &passphrase, CssmOwnedData &oldPassphrase, Reason reason);
	Reason accept(CssmManagedData &passphrase, CssmData *oldPassphrase);
	
private:
	Reason initialReason;
	CssmAutoData mPassphrase;
    bool mPassphraseValid;
};


//
// The "give user/passphrase in group" authorization dialog.
// This class is not self-contained, since the AuthorizationEngine wants
// to micro-manage the retry process.
//
class AuthorizationToken;

class QueryAuthorizeByGroup : public SecurityAgentQuery {
public:
    QueryAuthorizeByGroup(uid_t clientUID, const AuthorizationToken &auth);

    bool operator () (const char *group, const char *candidateUser, char username[SecurityAgent::maxUsernameLength], char passphrase[SecurityAgent::maxPassphraseLength], Reason reason = SecurityAgent::userNotInGroup);
    void cancel(Reason reason);
    void done();
    
    uid_t uid();
    
    const AuthorizationToken &authorization;

private:
    bool mActive;
};


using Authorization::AuthValueVector;

class QueryInvokeMechanism : public SecurityAgentQuery {
public:
    QueryInvokeMechanism(uid_t clientUID, const AuthorizationToken &auth, const char *agentName);
    bool operator () (const string &inPluginId, const string &inMechanismId, const AuthValueVector &inArguments, AuthItemSet &inHints, AuthItemSet &inContext, AuthorizationResult *outResult);
    void terminateAgent();
};

#endif //_H_AGENTQUERY
