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
#include "authority.h"
#include "server.h"
#include "connection.h"
#include "session.h"
#include "process.h"

#include "AuthorizationWalkers.h"

using Authorization::Right;

//
// The global dictionary of extant AuthorizationTokens
//
AuthorizationToken::AuthMap AuthorizationToken::authMap; // set of extant authorizations
Mutex AuthorizationToken::authMapLock; // lock for mAuthorizations (only)


//
// Construct an Authority
//
Authority::Authority(const char *configFile)
: Authorization::Engine(configFile)
{
}

Authority::~Authority()
{
}


//
// Create an authorization token.
//
AuthorizationToken::AuthorizationToken(Session &ssn, const CredentialSet &base)
	: session(ssn), mBaseCreds(base), mTransferCount(INT_MAX), 
	mCreatorUid(Server::connection().process.uid()),
    mCreatorCode(Server::connection().process.clientCode()), mInfoSet(NULL)
{
    // generate our (random) handle
    Server::active().random(mHandle);
    
    // register handle in the global map
    StLock<Mutex> _(authMapLock);
    authMap[mHandle] = this;
    
	// register with parent session
	session.addAuthorization(this);
	
    // all ready
	IFDEBUG(debug("SSauth", "Authorization %p created using %d credentials; owner=%s",
		this, int(mBaseCreds.size()),
        mCreatorCode ? mCreatorCode->encode().c_str() : "unknown"));
}

AuthorizationToken::~AuthorizationToken()
{
	// we better be clean
	assert(mUsingProcesses.empty());
    
    // deregister from parent session
    if (session.removeAuthorization(this))
        delete &session;

    // remove stored context
    if (mInfoSet)
    {
        debug("SSauth", "Authorization %p destroying context @%p", this, mInfoSet);
        CssmAllocator::standard().free(mInfoSet); // @@@ switch to sensitive allocator
    }
    
	debug("SSauth", "Authorization %p destroyed", this);
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
        delete mAuth;
        mAuth = NULL;
    }
}


//
// Given a set of credentials, add it to our private credentials and return the result
//
// must hold Session::mCredsLock
CredentialSet AuthorizationToken::effectiveCreds() const
{
    IFDEBUG(debug("SSauth", "Authorization %p grabbing session %p creds %p", this, &session, &session.authCredentials()));
    CredentialSet result = session.authCredentials();
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
    debug("SSauth", "Authorization %p merge creds %p", this, &add);
	for (CredentialSet::const_iterator it = add.begin(); it != add.end(); it++) {
        mBaseCreds.erase(*it);
        mBaseCreds.insert(*it);
    }
    debug("SSauth", "Authorization %p merged %d new credentials for %d total",
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
	debug("SSauth", "Authorization %p added process %p(%d)", this, &proc, proc.pid());
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
	IFDEBUG(debug("SSauth", "Authorization %p removed process %p(%d)%s",
		this, &proc, proc.pid(), mUsingProcesses.empty() ? " FINAL" : ""));
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
			debug("SSauth", "Authorization %p decrement intcount to %d", this, mTransferCount);
		}
		return true;
	}
	return false;
}

AuthorizationItemSet &
AuthorizationToken::infoSet()
{
    StLock<Mutex> _(mLock); // consider a separate lock
    MutableRightSet tempInfoSet(mInfoSet); // turn no info into empty set

    AuthorizationItemSet *returnSet = Copier<AuthorizationItemSet>(tempInfoSet, CssmAllocator::standard()).keep();
    debug("SSauth", "Authorization %p returning context %p", this, returnSet);
    return *returnSet;
}

void
AuthorizationToken::setInfoSet(AuthorizationItemSet &newInfoSet)
{
    StLock<Mutex> _(mLock); // consider a separate lock
    if (mInfoSet)
        CssmAllocator::standard().free(mInfoSet); // @@@ move to sensitive allocator
    debug("SSauth", "Authorization %p context %p -> %p", this, mInfoSet, &newInfoSet);
    mInfoSet = &newInfoSet;
}

// This is destructive (non-merging)
void
AuthorizationToken::setCredentialInfo(const Credential &inCred)
{
    StLock<Mutex> _(mLock);

    MutableRightSet dstInfoSet;
    char uid_string[16]; // fit a uid_t(u_int32_t)
	
    if (snprintf(uid_string, sizeof(uid_string), "%u", inCred->uid()) >=
		sizeof(uid_string))
        uid_string[0] = '\0';
    Right uidHint("uid", uid_string ? strlen(uid_string) + 1 : 0, uid_string );
    dstInfoSet.push_back(uidHint);

    const char *user = inCred->username().c_str();
    Right userHint("username", user ? strlen(user) + 1 : 0, user );
    dstInfoSet.push_back(userHint);

    AuthorizationItemSet *newInfoSet = Copier<AuthorizationItemSet>(dstInfoSet, CssmAllocator::standard()).keep();
    CssmAllocator::standard().free(mInfoSet); // @@@ move to sensitive allocator
    mInfoSet = newInfoSet;
}

