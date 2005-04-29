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
// process - track a single client process and its belongings
//
#include "process.h"
#include "server.h"
#include "session.h"
#include "tempdatabase.h"
#include "authority.h"
#include "flippers.h"


//
// Construct a Process object.
//
Process::Process(Port servicePort, TaskPort taskPort,
	const ClientSetupInfo *info, const char *identity, const CommonCriteria::AuditToken &audit)
 :  mTaskPort(taskPort), mByteFlipped(false), mPid(audit.pid()), mUid(audit.euid()), mGid(audit.egid())
{
	// set parent session
	parent(Session::find(servicePort));

    // let's take a look at our wannabe client...
	if (mTaskPort.pid() != mPid) {
		secdebug("SS", "Task/pid setup mismatch pid=%d task=%d(%d) for %s",
			mPid, mTaskPort.port(), mTaskPort.pid(),
			(identity && identity[0]) ? identity : "(unknown)");
		CssmError::throwMe(CSSMERR_CSSM_ADDIN_AUTHENTICATE_FAILED);	// you lied!
	}

	setup(info, identity);
	
	secdebug("SS", "New process %p(%d) uid=%d gid=%d session=%p TP=%d %sfor %s",
		this, mPid, mUid, mGid, &session(),
        mTaskPort.port(),
		mByteFlipped ? "FLIP " : "",
		(identity && identity[0]) ? identity : "(unknown)");
}


//
// Screen a process setup request for an existing process.
// This usually means the client has called exec(2) and forgotten all about itself.
// Though it could be a nefarious attempt to fool us...
//
void Process::reset(Port servicePort, TaskPort taskPort,
	const ClientSetupInfo *info, const char *identity, const CommonCriteria::AuditToken &audit)
{
	if (servicePort != session().servicePort() || taskPort != mTaskPort) {
		secdebug("SS", "Process %p(%d) reset mismatch (sp %d-%d, tp %d-%d) for %s",
			this, pid(), servicePort.port(), session().servicePort().port(), taskPort.port(), mTaskPort.port(),
			(identity && identity[0]) ? identity : "(unknown)");
		CssmError::throwMe(CSSMERR_CSSM_ADDIN_AUTHENTICATE_FAILED);		// liar
	}

	setup(info, identity);
	
	secdebug("SS", "process %p(%d) has reset; now %sfor %s",
		this, mPid, mByteFlipped ? "FLIP " : "",
		(identity && identity[0]) ? identity : "(unknown)");
}


//
// Common set processing
//
void Process::setup(const ClientSetupInfo *info, const char *identity)
{
	// process setup info
	assert(info);
	uint32 pversion;
	if (info->order == 0x1234) {	// right side up
		pversion = info->version;
	} else if (info->order == 0x34120000) { // flip side up
		pversion = ntohl(info->version);
		mByteFlipped = true;
	} else // non comprende
		CssmError::throwMe(CSSM_ERRCODE_INCOMPATIBLE_VERSION);

	// check wire protocol version
	if (pversion != SSPROTOVERSION)
		CssmError::throwMe(CSSM_ERRCODE_INCOMPATIBLE_VERSION);

	// process identity (if given)
	try {
		mClientCode = OSXCode::decode(identity);
		mClientIdent = deferred;	// will calculate code identity when needed
	} catch (...) {
		secdebug("SS", "process %p(%d) identity decode threw exception", this, pid());
		mClientCode = NULL;
		mClientIdent = unknown;		// no chance to squeeze a code identity from this
		secdebug("SS", "process %p(%d) no clientCode - marked anonymous", this, pid());
	}
}


//
// Clean up a Process object
//
Process::~Process()
{
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

	// no need to lock here; the client process has no more active threads
	secdebug("SS", "Process %p(%d) has died", this, mPid);
	
    // release our name for the process's task port
	if (mTaskPort)
        mTaskPort.destroy();
}

void Process::kill()
{
	StLock<Mutex> _(*this);
	
	// release local temp store
	mLocalStore = NULL;

	// standard kill processing
	PerProcess::kill();
}


Session& Process::session() const
{
	return parent<Session>();
}


LocalDatabase &Process::localStore()
{
	StLock<Mutex> _(*this);
	if (!mLocalStore)
		mLocalStore = new TempDatabase(*this);
	return *mLocalStore;
}

Key *Process::makeTemporaryKey(const CssmKey &key, CSSM_KEYATTR_FLAGS moreAttributes,
	const AclEntryPrototype *owner)
{
	return safer_cast<TempDatabase&>(localStore()).makeKey(key, moreAttributes, owner);
}


//
// Change the session of a process.
// This is the result of SessionCreate from a known process client.
//
void Process::changeSession(Port servicePort)
{
	// re-parent
	parent(Session::find(servicePort));
	
	secdebug("SS", "process %p(%d) changed session to %p", this, pid(), &session());
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
	StLock<Mutex> _(*this);
	mAuthorizations.insert(auth);
	auth->addProcess(*this);
}

void Process::checkAuthorization(AuthorizationToken *auth)
{
	assert(auth);
	StLock<Mutex> _(*this);
	if (mAuthorizations.find(auth) == mAuthorizations.end())
		MacOSError::throwMe(errAuthorizationInvalidRef);
}

bool Process::removeAuthorization(AuthorizationToken *auth)
{
	assert(auth);
	StLock<Mutex> _(*this);
	// we do everything with a single set lookup call...
	typedef AuthorizationSet::iterator Iter;
	Iter it = mAuthorizations.lower_bound(auth);
	bool isLast;
	if (it == mAuthorizations.end() || auth != *it) {
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
void Process::requestNotifications(Port port, SecurityServer::NotificationDomain domain, SecurityServer::NotificationMask events)
{
    new ProcessListener(*this, port, domain, events);
}

void Process::stopNotifications(Port port)
{
    if (!Listener::remove(port))
        CssmError::throwMe(CSSMERR_CSSM_INVALID_HANDLE_USAGE);	//@@@ bad name (should be "no such callback")
}


//
// Debug dump support
//
#if defined(DEBUGDUMP)

void Process::dumpNode()
{
	PerProcess::dumpNode();
	if (mByteFlipped)
		Debug::dump(" FLIPPED");
	Debug::dump(" task=%d pid=%d uid/gid=%d/%d",
		mTaskPort.port(), mPid, mUid, mGid);
	if (mClientCode) {
		Debug::dump(" client=%s", mClientCode->canonicalPath().c_str());
		switch (mClientIdent) {
		case deferred:
			break;
		case known:
			Debug::dump("[OK]");
			break;
		case unknown:
			Debug::dump("[UNKNOWN]");
			break;
		}
	} else {
		Debug::dump(" NO CLIENT ID");
	}
}

#endif //DEBUGDUMP
