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
#include "child.h"          // ServerChild (really UnixPlusPlus::Child)::find()

#include <security_utilities/logging.h>	//@@@ debug only
#include "agentquery.h"


//
// Construct a Process object.
//
Process::Process(TaskPort taskPort,	const ClientSetupInfo *info, const CommonCriteria::AuditToken &audit)
 :  mTaskPort(taskPort), mByteFlipped(false), mPid(audit.pid()), mUid(audit.euid()), mGid(audit.egid())
{
	StLock<Mutex> _(*this);
	
	// set parent session
	parent(Session::find(audit.sessionId(), true));

    // let's take a look at our wannabe client...
	if (mTaskPort.pid() != mPid) {
		secdebug("SS", "Task/pid setup mismatch pid=%d task=%d(%d)",
			mPid, mTaskPort.port(), mTaskPort.pid());
		CssmError::throwMe(CSSMERR_CSSM_ADDIN_AUTHENTICATE_FAILED);	// you lied!
	}

	setup(info);
	ClientIdentification::setup(this->pid());

    // NB: ServerChild::find() should only be used to determine
    // *existence*.  Don't use the returned Child object for anything else, 
    // as it is not protected against its underlying process's destruction.  
	if (this->pid() == getpid() // called ourselves (through some API). Do NOT record this as a "dirty" transaction
        || ServerChild::find<ServerChild>(this->pid()))   // securityd's child; do not mark this txn dirty
		VProc::Transaction::deactivate();

	if (SECURITYD_CLIENT_NEW_ENABLED())
		SECURITYD_CLIENT_NEW(this, this->pid(), &this->session(),			
			(char *)codePath(this->processCode()).c_str(), taskPort, mUid, mGid, mByteFlipped);
}


//
// Screen a process setup request for an existing process.
// This means the client has requested intialization even though we remember having
// talked to it in the past. This could either be an exec(2), or the client could just
// have forgotten all about its securityd client state. Or it could be an attack...
//
void Process::reset(TaskPort taskPort, const ClientSetupInfo *info, const CommonCriteria::AuditToken &audit)
{
	StLock<Mutex> _(*this);
	if (taskPort != mTaskPort) {
		secdebug("SS", "Process %p(%d) reset mismatch (tp %d-%d)",
			this, pid(), taskPort.port(), mTaskPort.port());
		//@@@ CssmError::throwMe(CSSM_ERRCODE_VERIFICATION_FAILURE);		// liar
	}
	setup(info);
	CFCopyRef<SecCodeRef> oldCode = processCode();

	ClientIdentification::setup(this->pid());	// re-constructs processCode()
	if (CFEqual(oldCode, processCode())) {
		SECURITYD_CLIENT_RESET_AMNESIA(this);
	} else {
		SECURITYD_CLIENT_RESET_FULL(this);
		CodeSigningHost::reset();
	}
}


//
// Common set processing
//
void Process::setup(const ClientSetupInfo *info)
{
	// process setup info
	assert(info);
	uint32 pversion;
	if (info->order == 0x1234) {	// right side up
		pversion = info->version;
		mByteFlipped = false;
	} else if (info->order == 0x34120000) { // flip side up
		pversion = flip(info->version);
		mByteFlipped = true;
	} else // non comprende
		CssmError::throwMe(CSSM_ERRCODE_INCOMPATIBLE_VERSION);

	// check wire protocol version
	if (pversion != SSPROTOVERSION)
		CssmError::throwMe(CSSM_ERRCODE_INCOMPATIBLE_VERSION);
}


//
// Clean up a Process object
//
Process::~Process()
{
	SECURITYD_CLIENT_RELEASE(this, this->pid());

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


void Process::checkSession(const audit_token_t &auditToken)
{
	AuditToken audit(auditToken);
	if (audit.sessionId() != this->session().sessionId())
		this->changeSession(audit.sessionId());
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
void Process::changeSession(Session::SessionId sessionId)
{
	// re-parent
	parent(Session::find(sessionId, true));
	SECURITYD_CLIENT_CHANGE_SESSION(this, &this->session());
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
	CodeSigningHost::dump();
	ClientIdentification::dump();
}

#endif //DEBUGDUMP
