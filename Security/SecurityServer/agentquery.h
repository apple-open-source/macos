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

	SecurityAgentQuery(uid_t clientUID, Session &clientSession);
	virtual ~SecurityAgentQuery();

	virtual void activate(const char *bootstrapName = NULL);
	virtual void terminate();

private:
	Session &mClientSession;
};


//
// Specialized for "rogue app" alert queries
//
class QueryKeychainUse : public SecurityAgent::Client::KeychainChoice, public SecurityAgentQuery {
public:
    QueryKeychainUse(uid_t clientUID, Session &clientSession,
		     bool needPass) :
	SecurityAgentQuery(clientUID, clientSession),
	needPassphrase(needPass) { }
    void operator () (const char *database, const char *description, AclAuthorization action);
	
	const bool needPassphrase;
};


//
// Specialized for passphrase-yielding queries based on Credential markers
//
class QueryPassphrase : public SecurityAgentQuery {
protected:
	QueryPassphrase(uid_t clientUID, Session &clientSession,
			unsigned int maxTries) :
	    SecurityAgentQuery(clientUID, clientSession),
	    maxRetries(maxTries) { }
	void query(const AccessCredentials *cred, CSSM_SAMPLE_TYPE relevantSampleType);
	
	virtual void queryInteractive(CssmOwnedData &passphrase) = 0;
	virtual void retryInteractive(CssmOwnedData &passphrase, Reason reason) = 0;

protected:
	virtual Reason accept(CssmManagedData &passphrase, bool canRetry) = 0;
	
private:
	const unsigned int maxRetries;
};


//
// A query for an existing passphrase
//
class QueryUnlock : public QueryPassphrase {
	static const int maxTries = 3;
public:
	QueryUnlock(uid_t clientUID, Session &clientSession,
		    Database &db) :
	    QueryPassphrase(clientUID, clientSession, maxTries),
	    database(db) { }
	
	Database &database;
	
	void operator () (const AccessCredentials *cred);
	
protected:
	void queryInteractive(CssmOwnedData &passphrase);
	void retryInteractive(CssmOwnedData &passphrase, Reason reason);
	Reason accept(CssmManagedData &passphrase, bool canRetry);
};


//
// A query for a new passphrase
//
class QueryNewPassphrase : public QueryPassphrase {
	static const int maxTries = 7;
public:
	QueryNewPassphrase(uid_t clientUID, Session &clientSession,
		    Database::Common &common, Reason reason) :
	    QueryPassphrase(clientUID, clientSession, maxTries),
	    dbCommon(common), initialReason(reason),
	    mPassphrase(CssmAllocator::standard(CssmAllocator::sensitive)),
	    mPassphraseValid(false) { }

	Database::Common &dbCommon;
	
	void operator () (const AccessCredentials *cred, CssmOwnedData &passphrase);
	
protected:
	void queryInteractive(CssmOwnedData &passphrase);
	void retryInteractive(CssmOwnedData &passphrase, Reason reason);
	Reason accept(CssmManagedData &passphrase, bool canRetry);
	
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

class QueryInvokeMechanism : public SecurityAgentQuery {
public:
    QueryInvokeMechanism(uid_t clientUID, const AuthorizationToken &auth);
    bool operator () (const string &inPluginId, const string &inMechanismId, const AuthorizationValueVector *inArguments, const AuthItemSet &inHints, const AuthItemSet &inContext, AuthorizationResult  *outResult, AuthorizationItemSet *&outHintsPtr, AuthorizationItemSet *&outContextPtr);
};

class QueryTerminateAgent : public SecurityAgentQuery {
public:
    QueryTerminateAgent(uid_t clientUID, const AuthorizationToken &auth);
    void operator () ();
};



#endif //_H_AGENTQUERY
