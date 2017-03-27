/*
 * Copyright (c) 2000-2009,2011-2013 Apple Inc. All Rights Reserved.
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
// session - authentication session domains
//
// Security sessions are now by definition congruent to audit subsystem sessions.
// We represent these sessions within securityd as subclasses of class Session,
// but we reach for the kernel's data whenever we're not sure if our data is
// up to date.
//
// Modifications to session state are made from client space using system calls.
// We discover them when we see changes in audit records as they come in with
// new requests. We cannot use system notifications for such changes because
// securityd is fully symmetrically multi-threaded, and thus may process new
// requests from clients before it gets those notifications.
//
#include <pwd.h>
#include <signal.h>                     // SIGTERM
#include <Security/AuthorizationPriv.h> // kAuthorizationFlagLeastPrivileged
#include "session.h"
#include "connection.h"
#include "database.h"
#include "server.h"
#include <security_utilities/logging.h>
#include <agentquery.h>

using namespace CommonCriteria;


//
// The static session map
//
Session::SessionMap Session::mSessions;
Mutex Session::mSessionLock(Mutex::recursive);


const char Session::kUsername[] = "username";
const char Session::kRealname[] = "realname";


//
// Create a Session object from initial parameters (create)
//
Session::Session(const AuditInfo &audit, Server &server)
	: mAudit(audit), mSecurityAgent(NULL), mKeybagState(0)
{
	// link to Server as the global nexus in the object mesh
	parent(server);
	
	// self-register
	StLock<Mutex> _(mSessionLock);
	assert(!mSessions[audit.sessionId()]);
	mSessions[audit.sessionId()] = this;
	
	// log it
    secnotice("SS", "%p Session %d created, uid:%d sessionId:%d", this, this->sessionId(), mAudit.uid(), mAudit.sessionId());
	Syslog::notice("Session %d created", this->sessionId());
}


//
// Destroy a Session
//
Session::~Session()
{
    secnotice("SS", "%p Session %d destroyed", this, this->sessionId());
	Syslog::notice("Session %d destroyed", this->sessionId());
}


Server &Session::server() const
{
	return parent<Server>();
}


//
// Locate a session object by session identifier
//
Session &Session::find(pid_t id, bool create)
{
	if (id == (pid_t)callerSecuritySession)
		return Server::session();
	StLock<Mutex> _(mSessionLock);
	SessionMap::iterator it = mSessions.find(id);
	if (it != mSessions.end())
		return *it->second;

	// new session
	if (!create)
		CssmError::throwMe(errSessionInvalidId);
	AuditInfo info;
	info.get(id);
	assert(info.sessionId() == id);
	RefPointer<Session> session = new Session(info, Server::active());
	mSessions.insert(make_pair(id, session));
	return *session;
}


//
// Act on a death notification for a session's underlying audit session object.
// We may not destroy the Session outright here (due to processes that use it),
// but we do clear out its accumulated wealth.
// Note that we may get spurious death notifications for audit sessions that we
// never learned about. Ignore those.
//
void Session::destroy(SessionId id)
{
    // remove session from session map
    RefPointer<Session> session = NULL;
    {
        StLock<Mutex> _(mSessionLock);
        SessionMap::iterator it = mSessions.find(id);
        if (it != mSessions.end()) {
            session = it->second;
            assert(session->sessionId() == id);
            mSessions.erase(it);
        }
    }

    if (session.get()) {
        session->kill();
    }
}


void Session::kill()
{
    StLock<Mutex> _(*this);     // do we need to take this so early?
    secnotice("SS", "%p killing session %d", this, this->sessionId());
    invalidateSessionAuthHosts();
	
	// base kill processing
	PerSession::kill();
}


//
// Refetch audit session data for the current audit session (to catch outside updates
// to the audit record). This is the price we're paying for not requiring an IPC to
// securityd when audit session data changes (this is desirable for delayering the
// software layer cake).
// If we ever disallow changes to (parts of the) audit session record in the kernel,
// we can loosen up on this continual re-fetching.
//
void Session::updateAudit() const
{
    CommonCriteria::AuditInfo info;
    try {
        info.get(mAudit.sessionId());
    } catch (...) {
        return;
    }
    mAudit = info;
}

void Session::verifyKeyStorePassphrase(int32_t retries)
{
    QueryKeybagPassphrase keybagQuery(*this, retries);
    keybagQuery.inferHints(Server::process());
    if (keybagQuery.query() != SecurityAgent::noReason) {
        CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);
    }
}

void Session::changeKeyStorePassphrase()
{
    service_context_t context = get_current_service_context();
    QueryKeybagNewPassphrase keybagQuery(*this);
    keybagQuery.inferHints(Server::process());
    CssmAutoData pass(Allocator::standard(Allocator::sensitive));
    CssmAutoData oldPass(Allocator::standard(Allocator::sensitive));
    SecurityAgent::Reason queryReason = keybagQuery.query(oldPass, pass);
    if (queryReason == SecurityAgent::noReason) {
        service_client_kb_change_secret(&context, oldPass.data(), (int)oldPass.length(), pass.data(), (int)pass.length());
    } else {
        CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);
    }
}

void Session::resetKeyStorePassphrase(const CssmData &passphrase)
{
    service_context_t context = get_current_service_context();
    service_client_kb_reset(&context, passphrase.data(), (int)passphrase.length());
}

service_context_t Session::get_current_service_context()
{
    service_context_t context = { sessionId(), originatorUid(), *Server::connection().auditToken() };
    return context;
}

void Session::keybagClearState(int state)
{
    mKeybagState &= ~state;
}

void Session::keybagSetState(int state)
{
    mKeybagState |= state;
}

bool Session::keybagGetState(int state)
{
    return mKeybagState & state;
}


//
// Manage authorization client processes
//
void Session::invalidateSessionAuthHosts()
{
    StLock<Mutex> _(mAuthHostLock);
    
    // if you got here, we don't care about pending operations: the auth hosts die
    Syslog::warning("Killing auth hosts");
    if (mSecurityAgent) mSecurityAgent->UnixPlusPlus::Child::kill(SIGTERM);
    mSecurityAgent = NULL;
}

void Session::invalidateAuthHosts()
{
	StLock<Mutex> _(mSessionLock);
	for (SessionMap::const_iterator it = mSessions.begin(); it != mSessions.end(); it++)
        it->second->invalidateSessionAuthHosts();
}

//
// On system sleep, call sleepProcessing on all DbCommons of all Sessions
//
void Session::processSystemSleep()
{
    SecurityAgentXPCQuery::killAllXPCClients();

	StLock<Mutex> _(mSessionLock);
	for (SessionMap::const_iterator it = mSessions.begin(); it != mSessions.end(); it++)
		it->second->allReferences(&DbCommon::sleepProcessing);
}


//
// On "lockAll", call sleepProcessing on all DbCommons of this session (only)
//
void Session::processLockAll()
{
	allReferences(&DbCommon::lockProcessing);
}


//
// The root session corresponds to the audit session that security is running in.
// This is usually the initial system session; but in debug scenarios it may be
// an "ordinary" graphic login session. In such a debug case, we may add attribute
// flags to the session to make our (debugging) life easier.
//
RootSession::RootSession(uint64_t attributes, Server &server)
	: Session(AuditInfo::current(), server)
{
	ref();				// eternalize
	mAudit.ai_flags |= attributes;		// merge imposed attributes
}


// 
// Accessor method for setting audit session flags.
// 
void Session::setAttributes(SessionAttributeBits bits)
{
	StLock<Mutex> _(*this);
	updateAudit();
//	assert((bits & ~settableAttributes) == 0);
	mAudit.ai_flags = bits;
	mAudit.set();
}

//
// The default session setup operation always fails.
// Subclasses can override this to support session setup calls.
//
void Session::setupAttributes(SessionCreationFlags flags, SessionAttributeBits attrs)
{
	MacOSError::throwMe(errSessionAuthorizationDenied);
}

uid_t Session::originatorUid()
{
    if (mAudit.uid() == AU_DEFAUDITID) {
        StLock<Mutex> _(*this);
        updateAudit();
    }
    return mAudit.uid();
}


RefPointer<AuthHostInstance> 
Session::authhost(const bool restart)
{
	StLock<Mutex> _(mAuthHostLock);

	if (restart || !mSecurityAgent || (mSecurityAgent->state() != Security::UnixPlusPlus::Child::alive))
	{
		if (mSecurityAgent)
			PerSession::kill(*mSecurityAgent);
		mSecurityAgent = new AuthHostInstance(*this);
	}
	return mSecurityAgent;
}


//
// Debug dumping
//
#if defined(DEBUGDUMP)

void Session::dumpNode()
{
	PerSession::dumpNode();
	Debug::dump(" auid=%d attrs=%#x securityagent=%p",
		this->sessionId(), uint32_t(this->attributes()), mSecurityAgent);
}

#endif //DEBUGDUMP
