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


//
// Construct a Process object.
//
Process::Process(Port servicePort, TaskPort taskPort, const char *identity, uid_t uid, gid_t gid)
 :  session(Session::find(servicePort)), mBusyCount(0), mDying(false),
	mTaskPort(taskPort), mUid(uid), mGid(gid)
{
    // let's take a look at our wannabe client...
    mPid = mTaskPort.pid();
    
    // register with the session
    session.addProcess(this);
	
	// identify the client-on-disk
	// @@@ do this lazily on first use?
	// @@@ note that the paradigm will shift here when kernel-supported id happens
	mClientCode = CodeSigning::OSXCode::decode(identity);
	
	debug("SS", "New process %p(%d) uid=%d gid=%d session=%p TP=%d for %s",
		this, mPid, mUid, mGid, &session,
        mTaskPort.port(), identity ? identity : "(unknown)");
}

#if 0
Process::Process(Process &prior)
 :	session(Session::find(prior.mTaskPort.bootstrap())), mBusyCount(0), mDying(false),
    mTaskPort(prior.mTaskPort), mUid(prior.mUid), mGid(prior.mGid)
{
    // copy more
    mPid = prior.mPid;

    // register with the session
    session.addProcess(this);
    
    // copy the client-code id (and clear it in the prior so it doesn't get destroyed there)
    mClientCode = prior.mClientCode;
    prior.mTaskPort = Port();

    debug("SS", "Process %p(%d) recloned uid=%d gid=%d session=%p",
        this, mPid, mUid, mGid, &session);
}
#endif


Process::~Process()
{
	assert(mBusyCount == 0);	// mustn't die with Connections referencing us

	// tell all our authorizations that we're gone
	IFDEBUG(if (!mAuthorizations.empty()) 
		debug("SS", "Process %p(%d) clearing %d authorizations", 
			this, mPid, int(mAuthorizations.size())));
	for (AuthorizationSet::iterator it = mAuthorizations.begin();
			it != mAuthorizations.end(); it++) {
        AuthorizationToken *auth = *it;
        if (removeAuthorization(auth))
            delete auth;
    }
        
    // remove all database handles that belong to this process
    IFDEBUG(if (!mDatabases.empty())
        debug("SS", "Process %p(%d) clearing %d database handles",
            this, mPid, int(mDatabases.size())));
    for (DatabaseSet::iterator it = mDatabases.begin();
            it != mDatabases.end(); it++)
        delete *it;

	// no need to lock here; the client process has no more active threads
	debug("SS", "Process %p(%d) has died", this, mPid);
	
    // release our name for the process's task port
	if (mTaskPort)
        mTaskPort.destroy();	// either dead or taken by reclone
    
    // deregister from session
    if (session.removeProcess(this))
        delete &session;
}

bool Process::kill()
{
	if (mBusyCount == 0) {
		return true;	// destroy me now
	} else {
		debug("SS", "Process %p(%d) destruction deferred for %d busy connections",
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
// Verify the code signature of the a process's on-disk source.
// @@@ In a truly secure solution, we would ask the OS to verify this.
// @@@ Only the OS knows for sure what disk file (if any) originated a process.
// @@@ In the meantime, we fake it.
//
bool Process::verifyCodeSignature(const CodeSigning::Signature *signature)
{
	if (mClientCode)
		return Server::signer().verify(*mClientCode, signature);
	else
		return false;	// identity not known; can't verify
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

bool Process::removeAuthorization(AuthorizationToken *auth)
{
	assert(auth);
	StLock<Mutex> _(mLock);
	// we do everything with a single set lookup call...
	typedef AuthorizationSet::iterator Iter;
	pair<Iter, Iter> range = mAuthorizations.equal_range(auth);
	assert(range.first != mAuthorizations.end());
	Iter next = range.first; next++;	// next element after first hit
	mAuthorizations.erase(range.first);	// erase first hit
	if (next == range.second) {			// if no more hits...
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

