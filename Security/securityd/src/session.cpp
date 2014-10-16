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
	: mAudit(audit), mSecurityAgent(NULL), mAuthHost(NULL), mKeybagState(0)
{
	// link to Server as the global nexus in the object mesh
	parent(server);
	
	// self-register
	StLock<Mutex> _(mSessionLock);
	assert(!mSessions[audit.sessionId()]);
	mSessions[audit.sessionId()] = this;
	
	// log it
	SECURITYD_SESSION_CREATE(this, this->sessionId(), &mAudit, sizeof(mAudit));
	Syslog::notice("Session %d created", this->sessionId());
}


//
// Destroy a Session
//
Session::~Session()
{
	SECURITYD_SESSION_DESTROY(this, this->sessionId());
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
	if (id == callerSecuritySession)
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
	RefPointer<Session> session = new DynamicSession(info);
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
    bool unlocked = false;
    RefPointer<Session> session = NULL;
    {
        StLock<Mutex> _(mSessionLock);
        SessionMap::iterator it = mSessions.find(id);
        if (it != mSessions.end()) {
            session = it->second;
            assert(session->sessionId() == id);
            mSessions.erase(it);

            for (SessionMap::iterator kb_it = mSessions.begin(); kb_it != mSessions.end(); kb_it++) {
                RefPointer<Session> kb_session = kb_it->second;
                if (kb_session->originatorUid() == session->originatorUid()) {
                    if (kb_session->keybagGetState(session_keybag_unlocked)) unlocked = true;
                }
            }
        }
    }

    if (session.get()) {
        if (!unlocked) {
            service_context_t context = session->get_current_service_context();
            service_client_kb_lock(&context);
        }
        session->kill();
    }
}


void Session::kill()
{
    StLock<Mutex> _(*this);     // do we need to take this so early?
	SECURITYD_SESSION_KILL(this, this->sessionId());
    invalidateSessionAuthHosts();
	
    // invalidate shared credentials
    {
        StLock<Mutex> _(mCredsLock);
        
        IFDEBUG(if (!mSessionCreds.empty()) 
            secdebug("SSauth", "session %p clearing %d shared credentials", 
                this, int(mSessionCreds.size())));
        for (CredentialSet::iterator it = mSessionCreds.begin(); it != mSessionCreds.end(); it++)
            (*it)->invalidate();
    }
	
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
    // if this gets called from a timer there is no connection() object.
    // need to check for valid connection object and pass the audit token along
    service_context_t context = { sessionId(), originatorUid(), {} }; //*Server::connection().auditToken()
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
    if (mAuthHost) mAuthHost->UnixPlusPlus::Child::kill(SIGTERM);
    mSecurityAgent = NULL;
    mAuthHost = NULL;
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
// Dynamic sessions use the audit session context of the first-contact client caller.
//
DynamicSession::DynamicSession(const AuditInfo &audit)
	: Session(audit, Server::active())
{
}


//
// Authorization operations
//
OSStatus Session::authCreate(const AuthItemSet &rights,
	const AuthItemSet &environment,
	AuthorizationFlags flags,
	AuthorizationBlob &newHandle,
	const audit_token_t &auditToken)
{
	// invoke the authorization computation engine
	CredentialSet resultCreds;
	
	// this will acquire the object lock, so we delay acquiring it (@@@ no longer needed)
	auto_ptr<AuthorizationToken> auth(new AuthorizationToken(*this, resultCreds, auditToken, (flags&kAuthorizationFlagLeastPrivileged)));

	SECURITYD_AUTH_CREATE(this, auth.get());
    
    // Make a copy of the mSessionCreds
    CredentialSet sessionCreds;
    {
        StLock<Mutex> _(mCredsLock);
        sessionCreds = mSessionCreds;
    }
	
	AuthItemSet outRights;
	OSStatus result = Server::authority().authorize(rights, environment, flags,
        &sessionCreds, &resultCreds, outRights, *auth);
	newHandle = auth->handle();

    // merge resulting creds into shared pool
    if ((flags & kAuthorizationFlagExtendRights) && 
        !(flags & kAuthorizationFlagDestroyRights))
    {
        StLock<Mutex> _(mCredsLock);
        mergeCredentials(resultCreds);
        auth->mergeCredentials(resultCreds);
    }

	// Make sure that this isn't done until the auth(AuthorizationToken) is guaranteed to 
	// not be destroyed anymore since it's destructor asserts it has no processes
	Server::process().addAuthorization(auth.get());
	auth.release();
	return result;
}

void Session::authFree(const AuthorizationBlob &authBlob, AuthorizationFlags flags)
{
    AuthorizationToken::Deleter deleter(authBlob);
    AuthorizationToken &auth = deleter;
	Process &process = Server::process();
	process.checkAuthorization(&auth);

	if (flags & kAuthorizationFlagDestroyRights) {
		// explicitly invalidate all shared credentials and remove them from the session
		for (CredentialSet::const_iterator it = auth.begin(); it != auth.end(); it++)
			if ((*it)->isShared())
				(*it)->invalidate();
	}

	// now get rid of the authorization itself
	if (process.removeAuthorization(&auth))
        deleter.remove();
}

OSStatus Session::authGetRights(const AuthorizationBlob &authBlob,
	const AuthItemSet &rights, const AuthItemSet &environment,
	AuthorizationFlags flags,
	AuthItemSet &grantedRights)
{
	AuthorizationToken &auth = authorization(authBlob);
	return auth.session().authGetRights(auth, rights, environment, flags, grantedRights);
}

OSStatus Session::authGetRights(AuthorizationToken &auth,
	const AuthItemSet &rights, const AuthItemSet &environment,
	AuthorizationFlags flags,
	AuthItemSet &grantedRights)
{
    CredentialSet resultCreds;
    CredentialSet effective;
    {
        StLock<Mutex> _(mCredsLock);
        effective	 = auth.effectiveCreds();
    }
	OSStatus result = Server::authority().authorize(rights, environment, flags, 
        &effective, &resultCreds, grantedRights, auth);

	// merge resulting creds into shared pool
	if ((flags & kAuthorizationFlagExtendRights) && !(flags & kAuthorizationFlagDestroyRights))
    {
        StLock<Mutex> _(mCredsLock);
        mergeCredentials(resultCreds);
        auth.mergeCredentials(resultCreds);
	}

	secdebug("SSauth", "Authorization %p copyRights asked for %d got %d",
		&auth, int(rights.size()), int(grantedRights.size()));
	return result;
}

OSStatus Session::authGetInfo(const AuthorizationBlob &authBlob,
	const char *tag,
	AuthItemSet &contextInfo)
{
	AuthorizationToken &auth = authorization(authBlob);
	secdebug("SSauth", "Authorization %p get-info", &auth);
	contextInfo = auth.infoSet(tag);
    return noErr;
}

OSStatus Session::authExternalize(const AuthorizationBlob &authBlob, 
	AuthorizationExternalForm &extForm)
{
	const AuthorizationToken &auth = authorization(authBlob);
	StLock<Mutex> _(*this);
	if (auth.mayExternalize(Server::process())) {
		memset(&extForm, 0, sizeof(extForm));
        AuthorizationExternalBlob &extBlob =
            reinterpret_cast<AuthorizationExternalBlob &>(extForm);
        extBlob.blob = auth.handle();
        extBlob.session = this->sessionId();
		secdebug("SSauth", "Authorization %p externalized", &auth);
		return noErr;
	} else
		return errAuthorizationExternalizeNotAllowed;
}

OSStatus Session::authInternalize(const AuthorizationExternalForm &extForm, 
	AuthorizationBlob &authBlob)
{
	// interpret the external form
    const AuthorizationExternalBlob &extBlob = 
        reinterpret_cast<const AuthorizationExternalBlob &>(extForm);
	
    // locate source authorization
    AuthorizationToken &sourceAuth = AuthorizationToken::find(extBlob.blob);
    
	// check for permission and do it
	if (sourceAuth.mayInternalize(Server::process(), true)) {
		StLock<Mutex> _(*this);
		authBlob = extBlob.blob;
        Server::process().addAuthorization(&sourceAuth);
        secdebug("SSauth", "Authorization %p internalized", &sourceAuth);
		return noErr;
	} else
		return errAuthorizationInternalizeNotAllowed;
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

//
// Authorization database I/O
//
OSStatus Session::authorizationdbGet(AuthorizationString inRightName, CFDictionaryRef *rightDict)
{
	string rightName(inRightName);
	return Server::authority().getRule(rightName, rightDict);
}


OSStatus Session::authorizationdbSet(const AuthorizationBlob &authBlob, AuthorizationString inRightName, CFDictionaryRef rightDict)
{
	CredentialSet resultCreds;
    AuthorizationToken &auth = authorization(authBlob);
    CredentialSet effective;

    {
        StLock<Mutex> _(mCredsLock);
        effective	 = auth.effectiveCreds();
    }

	OSStatus result = Server::authority().setRule(inRightName, rightDict, &effective, &resultCreds, auth);

    {
        StLock<Mutex> _(mCredsLock);
        mergeCredentials(resultCreds);
        auth.mergeCredentials(resultCreds);
	}

	secdebug("SSauth", "Authorization %p authorizationdbSet %s (result=%d)",
		&authorization(authBlob), inRightName, int32_t(result));
	return result;
}


OSStatus Session::authorizationdbRemove(const AuthorizationBlob &authBlob, AuthorizationString inRightName)
{
	CredentialSet resultCreds;
    AuthorizationToken &auth = authorization(authBlob);
    CredentialSet effective;

    {
        StLock<Mutex> _(mCredsLock);
        effective	 = auth.effectiveCreds();
    }

	OSStatus result = Server::authority().removeRule(inRightName, &effective, &resultCreds, auth);

    {
        StLock<Mutex> _(mCredsLock);
        mergeCredentials(resultCreds);
        auth.mergeCredentials(resultCreds);
	}

	secdebug("SSauth", "Authorization %p authorizationdbRemove %s (result=%d)",
		&authorization(authBlob), inRightName, int32_t(result));
	return result;
}


//
// Merge a set of credentials into the shared-session credential pool
//
// must hold mCredsLock
void Session::mergeCredentials(CredentialSet &creds)
{
    secdebug("SSsession", "%p merge creds @%p", this, &creds);
	CredentialSet updatedCredentials = creds;
	for (CredentialSet::const_iterator it = creds.begin(); it != creds.end(); it++)
		if ((*it)->isShared() && (*it)->isValid()) {
			CredentialSet::iterator old = mSessionCreds.find(*it);
			if (old == mSessionCreds.end()) {
				mSessionCreds.insert(*it);
            } else {
                // replace "new" with "old" in input set to retain synchronization
				(*old)->merge(**it);
                updatedCredentials.erase(*it);
                updatedCredentials.insert(*old);
            }
		}
	creds.swap(updatedCredentials);
}


//
// Locate an AuthorizationToken given a blob
//
AuthorizationToken &Session::authorization(const AuthorizationBlob &blob)
{
    AuthorizationToken &auth = AuthorizationToken::find(blob);
	Server::process().checkAuthorization(&auth);
	return auth;
}

//
// Run the Authorization engine to check if a given right has been authorized,
// independent of an external client request.  
//
OSStatus Session::authCheckRight(string &rightName, Connection &connection, bool allowUI)
{
    // dummy up the arguments for authCreate()
    AuthorizationItem rightItem = { rightName.c_str(), 0, NULL, 0 };
    AuthorizationItemSet rightItemSet = { 1, &rightItem };
    AuthItemSet rightAuthItemSet(&rightItemSet);
    AuthItemSet envAuthItemSet(kAuthorizationEmptyEnvironment);
    AuthorizationFlags flags = kAuthorizationFlagDefaults | kAuthorizationFlagExtendRights;
    if (true == allowUI)
        flags |= kAuthorizationFlagInteractionAllowed;
    AuthorizationBlob dummyHandle;
    const audit_token_t *at = connection.auditToken();
    
    return authCreate(rightAuthItemSet, envAuthItemSet, flags, dummyHandle, *at);
}

// for places within securityd that don't want to #include
// <libsecurity_authorization/Authorization.h> or to fuss about exceptions
bool Session::isRightAuthorized(string &rightName, Connection &connection, bool allowUI)
{
    bool isAuthorized = false;
    
    try {
        OSStatus status = authCheckRight(rightName, connection, allowUI);
        if (errAuthorizationSuccess == status)
            isAuthorized = true;
    }
    catch (...) { 
    }
    return isAuthorized;
}

RefPointer<AuthHostInstance> 
Session::authhost(const AuthHostType hostType, const bool restart)
{
	StLock<Mutex> _(mAuthHostLock);

	if (hostType == privilegedAuthHost)
	{
		if (restart || !mAuthHost || (mAuthHost->state() != Security::UnixPlusPlus::Child::alive))
		{
			if (mAuthHost)
				PerSession::kill(*mAuthHost);
			mAuthHost = new AuthHostInstance(*this, hostType);	
		}
		return mAuthHost;
	}
	else /* if (hostType == securityAgent) */
	{
		if (restart || !mSecurityAgent || (mSecurityAgent->state() != Security::UnixPlusPlus::Child::alive))
		{
			if (mSecurityAgent)
				PerSession::kill(*mSecurityAgent);
			mSecurityAgent = new AuthHostInstance(*this, hostType);
		}
		return mSecurityAgent;
	}
}

void DynamicSession::setUserPrefs(CFDataRef userPrefsDict)
{
	if (Server::process().uid() != 0)
		MacOSError::throwMe(errSessionAuthorizationDenied);
	StLock<Mutex> _(*this);
	mSessionAgentPrefs = userPrefsDict;
}

CFDataRef DynamicSession::copyUserPrefs()
{
	StLock<Mutex> _(*this);
	if (mSessionAgentPrefs)
		CFRetain(mSessionAgentPrefs);
	return mSessionAgentPrefs;
}


//
// Debug dumping
//
#if defined(DEBUGDUMP)

void Session::dumpNode()
{
	PerSession::dumpNode();
	Debug::dump(" auid=%d attrs=%#x authhost=%p securityagent=%p",
		this->sessionId(), uint32_t(this->attributes()), mAuthHost, mSecurityAgent);
}

#endif //DEBUGDUMP
