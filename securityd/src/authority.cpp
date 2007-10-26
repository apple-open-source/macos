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
#include "authority.h"
#include "server.h"
#include "connection.h"
#include "session.h"
#include "process.h"

#include <security_cdsa_utilities/AuthorizationWalkers.h>

#include <security_utilities/ccaudit.h>		// AuditToken

using Authorization::AuthItemSet;
using Authorization::AuthItemRef;
using Authorization::AuthValue;
using Authorization::AuthValueOverlay;

//
// The global dictionary of extant AuthorizationTokens
//
//AuthorizationToken::AuthMap AuthorizationToken::authMap; // set of extant authorizations
//@@@ Workaround ONLY! Don't destruct this map on termination
AuthorizationToken::AuthMap &AuthorizationToken::authMap = *new AuthMap; // set of extant authorizations
Mutex AuthorizationToken::authMapLock; // lock for mAuthorizations (only)



//
// Create an authorization token.
//
AuthorizationToken::AuthorizationToken(Session &ssn, const CredentialSet &base, 
const audit_token_t &auditToken, bool operateAsLeastPrivileged)
	: mBaseCreds(base), mTransferCount(INT_MAX), 
	mCreatorPid(Server::process().pid()), 
	mCreatorAuditToken(auditToken),
	mOperatesAsLeastPrivileged(operateAsLeastPrivileged)
{
	mCreatorUid = mCreatorAuditToken.euid();
	mCreatorGid = mCreatorAuditToken.egid();
	
	if (SecCodeRef code = Server::process().currentGuest())
		MacOSError::check(SecCodeCopyStaticCode(code, kSecCSDefaultFlags, &mCreatorCode.aref()));
		
	// link to session
	referent(ssn);
	
    // generate our (random) handle
    Server::active().random(mHandle);
    
    // register handle in the global map
    StLock<Mutex> _(authMapLock);
    authMap[mHandle] = this;
	
    // all ready
	secdebug("SSauth", "Authorization %p created using %d credentials; owner=%p",
		this, int(mBaseCreds.size()), mCreatorCode.get());
}

AuthorizationToken::~AuthorizationToken()
{
	// we better be clean
	assert(mUsingProcesses.empty());
    
	secdebug("SSauth", "Authorization %p destroyed", this);
}


Session &AuthorizationToken::session() const
{
	return referent<Session>();
}


//
// Locate an authorization given its blob.
//
AuthorizationToken &AuthorizationToken::find(const AuthorizationBlob &blob)
{
    StLock<Mutex> _(authMapLock);
	AuthMap::iterator it = authMap.find(blob);
	if (it == authMap.end())
		Authorization::Error::throwMe(errAuthorizationInvalidRef);
	return *it->second;
}


//
// Handle atomic deletion of AuthorizationToken objects
//
AuthorizationToken::Deleter::Deleter(const AuthorizationBlob &blob)
    : lock(authMapLock)
{
    AuthMap::iterator it = authMap.find(blob);
    if (it == authMap.end())
        Authorization::Error::throwMe(errAuthorizationInvalidRef);
    mAuth = it->second;
}

void AuthorizationToken::Deleter::remove()
{
    if (mAuth) {
        authMap.erase(mAuth->handle());
        mAuth = NULL;
    }
}


//
// Given a set of credentials, add it to our private credentials and return the result
//
// must hold Session::mCredsLock
CredentialSet AuthorizationToken::effectiveCreds() const
{
    secdebug("SSauth", "Authorization %p grabbing session %p creds %p",
		this, &session(), &session().authCredentials());
    CredentialSet result = session().authCredentials();
	for (CredentialSet::const_iterator it = mBaseCreds.begin(); it != mBaseCreds.end(); it++)
		if (!(*it)->isShared())
			result.insert(*it);
	return result;
}


//
// Add more credential dependencies to an authorization
//
// must hold Session::mCredsLock
void AuthorizationToken::mergeCredentials(const CredentialSet &add)
{
    secdebug("SSauth", "Authorization %p merge creds %p", this, &add);
	for (CredentialSet::const_iterator it = add.begin(); it != add.end(); it++) {
        mBaseCreds.erase(*it);
        mBaseCreds.insert(*it);
    }
    secdebug("SSauth", "Authorization %p merged %d new credentials for %d total",
		this, int(add.size()), int(mBaseCreds.size()));
}


//
// Register a new process that uses this authorization token.
// This is an idempotent operation.
//
void AuthorizationToken::addProcess(Process &proc)
{
	StLock<Mutex> _(mLock);
	mUsingProcesses.insert(&proc);
	secdebug("SSauth", "Authorization %p added process %p(%d)", this, &proc, proc.pid());
}


//
// Completely unregister client process.
// It does not matter how often it was registered with addProcess before.
// This returns true if no more processes use this token. Presumably you
// would then want to clean up, though that's up to you.
//
bool AuthorizationToken::endProcess(Process &proc)
{
	StLock<Mutex> _(mLock);
	assert(mUsingProcesses.find(&proc) != mUsingProcesses.end());
	mUsingProcesses.erase(&proc);
	secdebug("SSauth", "Authorization %p removed process %p(%d)%s",
		this, &proc, proc.pid(), mUsingProcesses.empty() ? " FINAL" : "");
	return mUsingProcesses.empty();
}


//
// Check whether internalization/externalization is allowed
//
bool AuthorizationToken::mayExternalize(Process &) const
{
	return mTransferCount > 0;
}

bool AuthorizationToken::mayInternalize(Process &, bool countIt)
{
	StLock<Mutex> _(mLock);
	if (mTransferCount > 0) {
		if (countIt) {
			mTransferCount--;
			secdebug("SSauth", "Authorization %p decrement intcount to %d", this, mTransferCount);
		}
		return true;
	}
	return false;
}

AuthItemSet
AuthorizationToken::infoSet(AuthorizationString tag)
{
    StLock<Mutex> _(mLock); // consider a separate lock
 	
	AuthItemSet tempSet;
 	
	if (tag)
	{
		AuthItemSet::iterator found = find_if(mInfoSet.begin(), mInfoSet.end(), 
					Authorization::FindAuthItemByRightName(tag));
		if (found != mInfoSet.end())
			tempSet.insert(AuthItemRef(*found));

	}
	else
		tempSet = mInfoSet;

	secdebug("SSauth", "Authorization %p returning copy of context %s%s.", this, tag ? "for tag " : "", tag ? "" : tag);
	
	return tempSet;
}

void
AuthorizationToken::setInfoSet(AuthItemSet &newInfoSet)
{
	StLock<Mutex> _(mLock); // consider a separate lock
    secdebug("SSauth", "Authorization %p setting new context", this);
    mInfoSet = newInfoSet;
}

// This is destructive (non-merging)
void
AuthorizationToken::setCredentialInfo(const Credential &inCred)
{
    AuthItemSet dstInfoSet;
    char uid_string[16]; // fit a uid_t(u_int32_t)
 	
    if (snprintf(uid_string, sizeof(uid_string), "%u", inCred->uid()) >=
		int(sizeof(uid_string)))
        uid_string[0] = '\0';
    AuthItemRef uidHint("uid", AuthValueOverlay(uid_string ? strlen(uid_string) + 1 : 0, uid_string), 0);
    dstInfoSet.insert(uidHint);
 
    AuthItemRef userHint("username", AuthValueOverlay(inCred->name()), 0);
    dstInfoSet.insert(userHint);
 
	setInfoSet(dstInfoSet);
}

void
AuthorizationToken::clearInfoSet()
{
    AuthItemSet dstInfoSet;
    secdebug("SSauth", "Authorization %p clearing context", this);
    setInfoSet(dstInfoSet);
}

void
AuthorizationToken::scrubInfoSet()
{
	AuthItemSet srcInfoSet = infoSet(), dstInfoSet;
	AuthItemSet::const_iterator end = srcInfoSet.end();
	for (AuthItemSet::const_iterator it = srcInfoSet.begin(); it != end; ++it)
	{
		const AuthItemRef &item = *it;
		if (item->flags() == kAuthorizationContextFlagExtractable)
			dstInfoSet.insert(item);
	}
    secdebug("SSauth", "Authorization %p scrubbing context", this);
    setInfoSet(dstInfoSet);
}
