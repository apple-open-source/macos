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
#include "agentquery.h"
#include "authority.h"
#include "server.h"
#include "session.h"

using namespace SecurityAgent;


//
// The default Mach service name for SecurityAgent
//
const char SecurityAgentQuery::defaultName[] = "com.apple.SecurityAgent";


//
// Construct a query object
//
SecurityAgentQuery::SecurityAgentQuery() :
    SecurityAgent::Client(Server::active().connection().process.uid(),
		Server::active().connection().process.session.bootstrapPort(),
		defaultName),
	mClientSession(Server::active().connection().process.session)
{
}

SecurityAgentQuery::SecurityAgentQuery(uid_t clientUID,
                                       Session &clientSession,
                                       const char *agentName) :
    SecurityAgent::Client(clientUID, clientSession.bootstrapPort(), agentName),
	mClientSession(clientSession)
{
}

SecurityAgentQuery::~SecurityAgentQuery()
{
	terminate();
}

void
SecurityAgentQuery::activate()
{
	if (isActive())
		return;

	// Before popping up an agent: is UI session allowed?
	if (!(mClientSession.attributes() & sessionHasGraphicAccess))
		CssmError::throwMe(CSSM_ERRCODE_NO_USER_INTERACTION);

	// this may take a while
	Server::active().longTermActivity();
        Server::connection().useAgent(this);

	try {
		SecurityAgent::Client::activate();
	} catch (...) {
		Server::connection().useAgent(NULL);	// guess not
		throw;
	}
}

void
SecurityAgentQuery::terminate()
{
	if (!isActive())
		return;

        Server::connection(true).useAgent(NULL);
        
	SecurityAgent::Client::terminate();
}


//
// Perform the "rogue app" access query dialog
//
void QueryKeychainUse::queryUser (const Database *db, const char *database, const char *description,
	AclAuthorization action)
{
    Reason reason;
    int retryCount = 0;
    queryKeychainAccess(Server::connection().process.clientCode(),
        Server::connection().process.pid(),
		database, description, action, needPassphrase, *this);

    CssmData data (passphrase, strlen (passphrase));
    
    
    if (needPassphrase) {
        while (reason = (const_cast<Database*>(db)->decode(data) ? noReason : invalidPassphrase)) {
            if (++retryCount > kMaximumAuthorizationTries) {
                cancelStagedQuery(tooManyTries);
                return;
            }
            else {
                retryQueryKeychainAccess (reason, *this);
                data = CssmData (passphrase, strlen (passphrase));
            }
        }
        
        finishStagedQuery (); // since we are only staged if we needed a passphrase
    }
    
}

QueryKeychainUse::~QueryKeychainUse()
{
	// clear passphrase component (sensitive)
	memset(passphrase, 0, sizeof(passphrase));
}


//
// Perform code signature ACL access adjustment dialogs
//
void QueryCodeCheck::operator () (const char *aclPath)
{
	queryCodeIdentity(Server::connection().process.clientCode(),
        Server::connection().process.pid(), aclPath, *this);
}


//
// Obtain passphrases and submit them to the accept() method until it is accepted
// or we can't get another passphrase. Accept() should consume the passphrase
// if it is accepted. If no passphrase is acceptable, throw out of here.
//
Reason QueryUnlock::query()
{
	CssmAutoData passphrase(CssmAllocator::standard(CssmAllocator::sensitive));
	int retryCount = 0;
	queryInteractive(passphrase);
	while (Reason reason = accept(passphrase)) {
		if (++retryCount > maxTries) {
			cancelStagedQuery(tooManyTries);
			return reason;
		} else {
			retryInteractive(passphrase, reason);
		}
	}
	// accepted
	finishStagedQuery();
	return noReason;
}


//
// Get existing passphrase (unlock) Query
//
Reason QueryUnlock::operator () ()
{
	return query();
}

Reason QueryUnlock::accept(CssmManagedData &passphrase)
{
	return database.decode(passphrase) ? noReason : invalidPassphrase;
}

void QueryUnlock::queryInteractive(CssmOwnedData &passphrase)
{
    char passString[maxPassphraseLength];
	queryUnlockDatabase(Server::connection().process.clientCode(),
        Server::connection().process.pid(),
        database.dbName(), passString);
	passphrase.copy(passString, strlen(passString));
}

void QueryUnlock::retryInteractive(CssmOwnedData &passphrase, Reason reason)
{
    char passString[maxPassphraseLength];
	retryUnlockDatabase(reason, passString);
	passphrase.copy(passString, strlen(passString));
}


//
// Obtain passphrases and submit them to the accept() method until it is accepted
// or we can't get another passphrase. Accept() should consume the passphrase
// if it is accepted. If no passphrase is acceptable, throw out of here.
//
Reason QueryNewPassphrase::query()
{
	CssmAutoData passphrase(CssmAllocator::standard(CssmAllocator::sensitive));
	CssmAutoData oldPassphrase(CssmAllocator::standard(CssmAllocator::sensitive));
	int retryCount = 0;
	queryInteractive(passphrase, oldPassphrase);
	while (Reason reason = accept(passphrase,
		(initialReason == changePassphrase) ? &oldPassphrase.get() : NULL)) {
		if (++retryCount > maxTries) {
			cancelStagedQuery(tooManyTries);
			return reason;
		} else {
			retryInteractive(passphrase, oldPassphrase, reason);
		}
	}
	// accepted
	finishStagedQuery();
	return noReason;
}


//
// Get new passphrase Query
//
Reason QueryNewPassphrase::operator () (CssmOwnedData &passphrase)
{
	if (Reason result = query())
		return result;	// failed
	passphrase = mPassphrase;
	return noReason;	// success
}

Reason QueryNewPassphrase::accept(CssmManagedData &passphrase, CssmData *oldPassphrase)
{
	//@@@ acceptance criteria are currently hardwired here
	//@@@ This validation presumes ASCII - UTF8 might be more lenient
	
	// if we have an old passphrase, check it
	if (oldPassphrase && !database.validatePassphrase(*oldPassphrase))
		return oldPassphraseWrong;
	
	// sanity check the new passphrase (but allow user override)
	if (!(mPassphraseValid && passphrase.get() == mPassphrase)) {
		mPassphrase = passphrase;
		mPassphraseValid = true;
		if (mPassphrase.length() == 0)
			return passphraseIsNull;
		if (mPassphrase.length() < 6)
			return passphraseTooSimple;
	}
	
	// accept this
	return noReason;
}

void QueryNewPassphrase::queryInteractive(CssmOwnedData &passphrase, CssmOwnedData &oldPassphrase)
{
    char passString[maxPassphraseLength], oldPassString[maxPassphraseLength];
	queryNewPassphrase(Server::connection().process.clientCode(),
        Server::connection().process.pid(),
		database.dbName(), initialReason, passString, oldPassString);
	passphrase.copy(passString, strlen(passString));
	oldPassphrase.copy(oldPassString, strlen(oldPassString));
}

void QueryNewPassphrase::retryInteractive(CssmOwnedData &passphrase, CssmOwnedData &oldPassphrase, Reason reason)
{
    char passString[maxPassphraseLength], oldPassString[maxPassphraseLength];
	retryNewPassphrase(reason, passString, oldPassString);
	passphrase.copy(passString, strlen(passString));
	oldPassphrase.copy(oldPassString, strlen(oldPassString));
}


//
// Authorize by group membership
//
QueryAuthorizeByGroup::QueryAuthorizeByGroup(uid_t clientUID, const AuthorizationToken &auth) :
  SecurityAgentQuery(Server::active().connection().process.uid(), auth.session),
  authorization(auth), mActive(false) { }


void QueryAuthorizeByGroup::cancel(Reason reason)
{
    if (mActive) {
        cancelStagedQuery(reason);
        mActive = false;
    }
}

void QueryAuthorizeByGroup::done()
{
    if (mActive) {
        finishStagedQuery();
        mActive = false;
    }
}

uid_t QueryAuthorizeByGroup::uid()
{
    return Server::connection().process.uid();
}

bool QueryAuthorizeByGroup::operator () (const char *group, const char *candidateUser,
    char username[maxUsernameLength], char passphrase[maxPassphraseLength], Reason reason)
{
    if (mActive) {
        return retryAuthorizationAuthenticate(reason, username, passphrase);
    } else {
        bool result = authorizationAuthenticate(authorization.creatorCode(),
            Server::connection().process.pid(), group, candidateUser, username, passphrase);
        mActive = true;
        return result;
    }
}

QueryInvokeMechanism::QueryInvokeMechanism(uid_t clientUID, const AuthorizationToken &auth, const char *agentName) :
	SecurityAgentQuery(clientUID, auth.session, agentName) {}

bool QueryInvokeMechanism::operator () (const string &inPluginId, const string &inMechanismId, const AuthValueVector &inArguments, AuthItemSet &inHints, AuthItemSet &inContext, AuthorizationResult *outResult)
{
	bool result = invokeMechanism(inPluginId, inMechanismId, inArguments, inHints, inContext, outResult);
	return result;
}

void QueryInvokeMechanism::terminateAgent()
{
    SecurityAgentQuery::terminateAgent();
}
