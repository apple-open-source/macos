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
// Construct a query object
//
SecurityAgentQuery::SecurityAgentQuery(uid_t clientUID,
                                       Session &clientSession) :
    SecurityAgent::Client(clientUID, clientSession.bootstrapPort()),
	mClientSession(clientSession)
{
}

SecurityAgentQuery::~SecurityAgentQuery()
{
	// SecurityAgent::Client::~SecurityAgent already calls terminate().
}

void
SecurityAgentQuery::activate(const char *bootstrapName = NULL)
{
	if (isActive())
		return;

	// Before popping up an agent: is UI session allowed?
	if (!(mClientSession.attributes() & sessionHasGraphicAccess))
		CssmError::throwMe(CSSM_ERRCODE_NO_USER_INTERACTION);

	// this may take a while
	Server::active().longTermActivity();
	Server::connection().useAgent(this);

	SecurityAgent::Client::activate(bootstrapName);
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
void QueryKeychainUse::operator () (const char *database, const char *description,
	AclAuthorization action)
{
	queryKeychainAccess(Server::connection().process.clientCode(),
        Server::connection().process.pid(),
		database, description, action, needPassphrase, *this);
}


//
// Obtain passphrases and submit them to the accept() method until it is accepted
// or we can't get another passphrase. Accept() should consume the passphrase
// if it is accepted. If no passphrase is acceptable, throw out of here.
//
void QueryPassphrase::query(const AccessCredentials *cred, CSSM_SAMPLE_TYPE sampleType)
{
	CssmAutoData passphrase(CssmAllocator::standard(CssmAllocator::sensitive));
	if (SecurityServerAcl::getBatchPassphrase(cred, sampleType, passphrase)) {
		// batch use - try the one and only, fail if unacceptable
		if (accept(passphrase, false) == noReason)
			return;
		else
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PASSPHRASE);	//@@@ not ideal
	} else {
		// interactive use - run a try/retry loop
		unsigned int retryCount = 0;
		queryInteractive(passphrase);
		while (Reason reason = accept(passphrase, true)) {
			if (++retryCount > maxRetries) {
				cancelStagedQuery(tooManyTries);
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PASSPHRASE);	//@@@ not ideal
			} else {
				retryInteractive(passphrase, reason);
			}
		}
		// accepted
		finishStagedQuery();
	}
}


//
// Get existing passphrase (unlock) Query
//
void QueryUnlock::operator () (const AccessCredentials *cred)
{
	query(cred, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK);
}

Reason QueryUnlock::accept(CssmManagedData &passphrase, bool)
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
// Get new passphrase Query
//
void QueryNewPassphrase::operator () (const AccessCredentials *cred, CssmOwnedData &passphrase)
{
	query(cred, CSSM_SAMPLE_TYPE_KEYCHAIN_CHANGE_LOCK);
	passphrase = mPassphrase;
}

Reason QueryNewPassphrase::accept(CssmManagedData &passphrase, bool canRetry)
{
	//@@@ acceptance criteria are currently hardwired here
	//@@@ This validation presumes ASCII - UTF8 might be more lenient
	
	// if we can't retry (i.e. batch environment), accept it rather than fail terminally
	if (!canRetry) {
		mPassphrase = passphrase;
		return noReason;
	}
	
	// if the user insists (re-enters the same passphrase), allow it
	if (mPassphraseValid && passphrase.get() == mPassphrase)
		return noReason;

	// check simple criteria
	mPassphrase = passphrase;
    mPassphraseValid = true;
	if (mPassphrase.length() == 0)
		return passphraseIsNull;
	const char *passString = mPassphrase;
	if (strlen(passString) < 6)
		return passphraseTooSimple;
	
	// accept this
	return noReason;
}

void QueryNewPassphrase::queryInteractive(CssmOwnedData &passphrase)
{
    char passString[maxPassphraseLength];
	queryNewPassphrase(Server::connection().process.clientCode(),
        Server::connection().process.pid(),
		dbCommon.dbName(), initialReason, passString);
	passphrase.copy(passString, strlen(passString));
}

void QueryNewPassphrase::retryInteractive(CssmOwnedData &passphrase, Reason reason)
{
    char passString[maxPassphraseLength];
	retryNewPassphrase(reason, passString);
	passphrase.copy(passString, strlen(passString));
}


//
// Authorize by group membership
//
QueryAuthorizeByGroup::QueryAuthorizeByGroup(uid_t clientUID, const AuthorizationToken &auth) :
  SecurityAgentQuery(clientUID, auth.session),
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

QueryInvokeMechanism::QueryInvokeMechanism(uid_t clientUID, const AuthorizationToken &auth) :
	SecurityAgentQuery(clientUID, auth.session) {}

bool QueryInvokeMechanism::operator () (const string &inPluginId, const string &inMechanismId, const AuthorizationValueVector *inArguments, const AuthItemSet &inHints, const AuthItemSet &inContext, AuthorizationResult *outResult, AuthorizationItemSet *&outHintsPtr, AuthorizationItemSet *&outContextPtr)
{
    bool result = invokeMechanism(inPluginId, inMechanismId, inArguments, inHints, inContext, outResult, outHintsPtr, outContextPtr);
        return result;
}

QueryTerminateAgent::QueryTerminateAgent(uid_t clientUID, const AuthorizationToken &auth) :
  SecurityAgentQuery(clientUID, auth.session) {}

void QueryTerminateAgent::operator () ()
{
    terminateAgent(); 
}

