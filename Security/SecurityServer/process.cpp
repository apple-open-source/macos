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
#include "process.h"
#include "server.h"
#include "session.h"
#include "authority.h"
#include "flippers.h"


//
// Construct a Process object.
//
Process::Process(Port servicePort, TaskPort taskPort,
	const ClientSetupInfo *info, const char *identity, uid_t uid, gid_t gid)
 :  session(Session::find(servicePort)), mBusyCount(0), mDying(false),
	mTaskPort(taskPort), mByteFlipped(false), mUid(uid), mGid(gid),
	mClientIdent(deferred)
{
	// examine info passed
	assert(info);
	uint32 pversion = info->version;
	if (pversion == SSPROTOVERSION) {
		// correct protocol, same byte order, cool
	} else {
		Flippers::flip(pversion);
		if (pversion == SSPROTOVERSION) {
			// correct protocol, reversed byte order
			mByteFlipped = true;
		} else {
			// unsupported protocol version
			CssmError::throwMe(CSSM_ERRCODE_INCOMPATIBLE_VERSION);
		}
	}

    // let's take a look at our wannabe client...
    mPid = mTaskPort.pid();
    
    // register with the session
    session.addProcess(this);
	
	secdebug("SS", "New process %p(%d) uid=%d gid=%d session=%p TP=%d %sfor %s",
		this, mPid, mUid, mGid, &session,
        mTaskPort.port(),
		mByteFlipped ? "FLIP " : "",
		(identity && identity[0]) ? identity : "(unknown)");

	try {
		mClientCode = CodeSigning::OSXCode::decode(identity);
	} catch (...) {
		secdebug("SS", "process %p(%d) identity decode threw exception", this, pid());
	}
	if (!mClientCode) {
		mClientIdent = unknown;		// no chance to squeeze a code identity from this
		secdebug("SS", "process %p(%d) no clientCode - marked anonymous", this, pid());
	}
}


Process::~Process()
{
	assert(mBusyCount == 0);	// mustn't die with Connections referencing us

	// tell all our authorizations that we're gone
	IFDEBUG(if (!mAuthorizations.empty()) 
		secdebug("SS", "Process %p(%d) clearing %d authorizations", 
			this, mPid, int(mAuthorizations.size())));
	for (AuthorizationSet::iterator it = mAuthorizations.begin();
			it != mAuthorizations.end(); ) {
        AuthorizationToken *auth = *it;
        while (++it != mAuthorizations.end() && *it == auth) ;	// Skip duplicates
		if (auth->endProcess(*this))
			delete auth;
    }

    // remove all database handles that belong to this process
    IFDEBUG(if (!mDatabases.empty())
        secdebug("SS", "Process %p(%d) clearing %d database handles",
            this, mPid, int(mDatabases.size())));
    for (DatabaseSet::iterator it = mDatabases.begin();
            it != mDatabases.end(); it++)
        delete *it;

	// no need to lock here; the client process has no more active threads
	secdebug("SS", "Process %p(%d) has died", this, mPid);
	
    // release our name for the process's task port
	if (mTaskPort)
        mTaskPort.destroy();
    
    // deregister from session
    if (session.removeProcess(this))
        delete &session;
}

bool Process::kill(bool keepTaskPort)
{
	StLock<Mutex> _(mLock);
	if (keepTaskPort)
		mTaskPort = Port();	// clear port so we don't destroy it later
	if (mBusyCount == 0) {
		return true;	// destroy me now
	} else {
		secdebug("SS", "Process %p(%d) destruction deferred for %d busy connections",
			this, mPid, int(mBusyCount));
		mDying = true;
		return false;	// destroy me later
	}
}


//
// Connection management
//
void Process::beginConnection(Connection &)
{
	StLock<Mutex> _(mLock);
	mBusyCount++;
}

bool Process::endConnection(Connection &)
{
	StLock<Mutex> _(mLock);
	return --mBusyCount == 0 && mDying;
}


//
// Database management
//
void Process::addDatabase(Database *database)
{
    StLock<Mutex> _(mLock);
    mDatabases.insert(database);
}

void Process::removeDatabase(Database *database)
{
    StLock<Mutex> _(mLock);
    assert(mDatabases.find(database) != mDatabases.end());
    mDatabases.erase(database);
}


//
// CodeSignatures implementation of Identity.
// The caller must make sure we have a valid (not necessarily hash-able) clientCode().
//
string Process::getPath() const
{
	assert(mClientCode);
	return mClientCode->canonicalPath();
}

const CssmData Process::getHash(CodeSigning::OSXSigner &signer) const
{
	switch (mClientIdent) {
	case deferred:
		try {
			// try to calculate our signature hash (first time use)
			mCachedSignature.reset(mClientCode->sign(signer));
			assert(mCachedSignature.get());
			mClientIdent = known;
			secdebug("SS", "process %p(%d) code signature computed", this, pid());
			break;
		} catch (...) {
			// couldn't get client signature (unreadable, gone, hack attack, ...)
			mClientIdent = unknown;
			secdebug("SS", "process %p(%d) no code signature - anonymous", this, pid());
			CssmError::throwMe(CSSM_ERRCODE_INSUFFICIENT_CLIENT_IDENTIFICATION);
		}
	case known:
		assert(mCachedSignature.get());
		break;
	case unknown:
		CssmError::throwMe(CSSM_ERRCODE_INSUFFICIENT_CLIENT_IDENTIFICATION);
	}
	return CssmData(*mCachedSignature);
}


//
// Authorization set maintainance
//
void Process::addAuthorization(AuthorizationToken *auth)
{
	assert(auth);
	StLock<Mutex> _(mLock);
	mAuthorizations.insert(auth);
	auth->addProcess(*this);
}

void Process::checkAuthorization(AuthorizationToken *auth)
{
	assert(auth);
	StLock<Mutex> _(mLock);
	if (mAuthorizations.find(auth) == mAuthorizations.end())
		MacOSError::throwMe(errAuthorizationInvalidRef);
}

bool Process::removeAuthorization(AuthorizationToken *auth)
{
	assert(auth);
	StLock<Mutex> _(mLock);
	// we do everything with a single set lookup call...
	typedef AuthorizationSet::iterator Iter;
	Iter it = mAuthorizations.lower_bound(auth);
	bool isLast;
	if (it == mAuthorizations.end() || auth != *it) {
		Syslog::error("process is missing authorization to remove");	// temp. diagnostic
		isLast = true;
	} else {
		Iter next = it; ++next;			// following element
		isLast = (next == mAuthorizations.end()) || auth != *next;
		mAuthorizations.erase(it);		// remove first match
	}
	if (isLast) {
		if (auth->endProcess(*this))	// ... tell it to remove us,
			return true;				// ... and tell the caller
	}
	return false;						// keep the auth; it's still in use
}


//
// Notification client maintainance
//
void Process::requestNotifications(Port port, Listener::Domain domain, Listener::EventMask events)
{
    new Listener(*this, port, domain, events);
}

void Process::stopNotifications(Port port)
{
    if (!Listener::remove(port))
        CssmError::throwMe(CSSMERR_CSSM_INVALID_HANDLE_USAGE);	//@@@ bad name (should be "no such callback")
}

void Process::postNotification(Listener::Domain domain, Listener::Event event, const CssmData &data)
{
    Listener::notify(domain, event, data);
}

