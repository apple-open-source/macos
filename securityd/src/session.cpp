/*
 * Copyright (c) 2000-2004,2008-2009 Apple Inc. All Rights Reserved.
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
// A Session is defined by a mach_init bootstrap dictionary. These dictionaries are
// hierarchical and inherited, so they work well for characterization of processes
// that "belong" together. (Of course, if your mach_init is broken, you're in bad shape.)
//
// Sessions are multi-threaded objects.
//
#include <pwd.h>
#include <signal.h>                     // SIGTERM
#include <Security/AuthorizationPriv.h> // kAuthorizationFlagLeastPrivileged
#include "session.h"
#include "connection.h"
#include "database.h"
#include "server.h"
#include <security_utilities/logging.h>

//
// The static session map
//
PortMap<Session> Session::mSessions;

std::string Session::kUsername = "username";
std::string Session::kRealname = "realname";

//
// Create a Session object from initial parameters (create)
//
Session::Session(Bootstrap bootstrap, Port servicePort, SessionAttributeBits attrs) 
    : mBootstrap(bootstrap), mServicePort(servicePort),
	  mAttributes(attrs), mSecurityAgent(NULL), mAuthHost(NULL)
{
    secdebug("SSsession", "%p CREATED: handle=%#x bootstrap=%d service=%d attrs=%#x",
        this, handle(), mBootstrap.port(), mServicePort.port(), uint32_t(mAttributes));
	SECURITYD_SESSION_CREATE(this, attrs, servicePort);
	Syslog::notice("Session 0x%lx created", this->handle());
}


//
// Destroy a Session
//
Session::~Session()
{
    secdebug("SSsession", "%p DESTROYED: handle=%#x bootstrap=%d",
        this, handle(), mBootstrap.port());
	Syslog::notice("Session 0x%lx destroyed", this->handle());
}


//
// Locate a session object by service port or (Session API) identifier
//
Session &Session::find(Port servicePort)
{
    StLock<Mutex> _(mSessions);
	PortMap<Session>::const_iterator it = mSessions.find(servicePort);
	assert(it != mSessions.end());
	return *it->second;
}

Session &Session::find(SecuritySessionId id)
{
    switch (id) {
    case callerSecuritySession:
        return Server::session();
    default:
		try {
			return U32HandleObject::find<Session>(id, CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
		} catch (const CommonError &err) {
			Syslog::warning("Session::find(%#x) failed rcode=%d", id, err.osStatus());
			for (PortMap<Session>::const_iterator it = mSessions.begin(); it != mSessions.end(); ++it)
				Syslog::notice(" Valid sessions include %#x attrs=%#x",
					it->second->handle(), it->second->attributes());
			throw;
		}
    }
}


//
// Act on a death notification for a session's (sub)bootstrap port.
// We may not destroy the Session outright here (due to processes that use it),
// but we do clear out its accumulated wealth.
//
void Session::destroy(Port servPort)
{
    // remove session from session map
    StLock<Mutex> _(mSessions);
    PortMap<Session>::iterator it = mSessions.find(servPort);
    assert(it != mSessions.end());
	RefPointer<Session> session = it->second;
	SECURITYD_SESSION_DESTROY(session);
	Syslog::notice("Session 0x%lx dead", session->handle());
    mSessions.erase(it);
	session->kill();
}

void Session::kill()
{
    StLock<Mutex> _(*this);     // do we need to take this so early?
	
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
	StLock<Mutex> _(mSessions);
	for (PortMap<Session>::const_iterator it = mSessions.begin(); it != mSessions.end(); it++)
        it->second->invalidateSessionAuthHosts();
}

//
// On system sleep, call sleepProcessing on all DbCommons of all Sessions
//
void Session::processSystemSleep()
{
	StLock<Mutex> _(mSessions);
	for (PortMap<Session>::const_iterator it = mSessions.begin(); it != mSessions.end(); it++)
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
// The root session inherits the startup bootstrap and service port
//
RootSession::RootSession(Server &server, SessionAttributeBits attrs)
    : Session(Bootstrap(), server.primaryServicePort(),
		sessionIsRoot | sessionWasInitialized | attrs)
{
	parent(server);		// the Server is our parent
	ref();				// eternalize

    // self-install (no thread safety issues here)
	mSessions[mServicePort] = this;
}

//
// Dynamic sessions use the given bootstrap and re-register in it
//
DynamicSession::DynamicSession(TaskPort taskPort)
	: ReceivePort(Server::active().bootstrapName(), taskPort.bootstrap(), false),
	  Session(taskPort.bootstrap(), *this),
	  mOriginatorTask(taskPort), mHaveOriginatorUid(false)
{
	// link to Server as the global nexus in the object mesh
	parent(Server::active());
	
	// tell the server to listen to our port
	Server::active().add(*this);
	
	// register for port notifications
    Server::active().notifyIfDead(bootstrapPort());	//@@@??? still needed?
	Server::active().notifyIfUnused(*this);

	// self-register
	StLock<Mutex> _(mSessions);
	assert(!mSessions[*this]);  // can't be registered already (we just made it)
	mSessions[*this] = this;
	
	secdebug("SSsession", "%p dynamic session originator=%d (pid=%d)",
		this, mOriginatorTask.port(), taskPort.pid());
}

DynamicSession::~DynamicSession()
{
	// remove our service port from the server
	Server::active().remove(*this);
}


void DynamicSession::kill()
{
	StLock<Mutex> _(*this);
	mBootstrap.destroy();		// release our bootstrap port
	Session::kill();			// continue with parent kill
}


//
// Set up a DynamicSession.
// This call must be made from a process within the session, and it must be the first
// such process to make the call.
//
void DynamicSession::setupAttributes(SessionCreationFlags flags, SessionAttributeBits attrs)
{
	StLock<Mutex> _(*this);
	SECURITYD_SESSION_SETATTR(this, attrs);
	Syslog::notice("Session 0x%lx attributes 0x%x", this->handle(), attrs);
    secdebug("SSsession", "%p setup flags=%#x attrs=%#x", this, uint32_t(flags), uint32_t(attrs));
    if (attrs & ~settableAttributes)
        MacOSError::throwMe(errSessionInvalidAttributes);
	checkOriginator();
    if (attribute(sessionWasInitialized))
        MacOSError::throwMe(errSessionAuthorizationDenied);
    setAttributes(attrs | sessionWasInitialized);
}


//
// Check whether the calling process is the session originator.
// If it's not, throw.
//
void DynamicSession::checkOriginator()
{
	if (mOriginatorTask != Server::process().taskPort())
		MacOSError::throwMe(errSessionAuthorizationDenied);
}


//
// The "originator uid" is a uid value that can be provided by the session originator
// and retrieved by anyone. Securityd places no semantic meaning on this value.
//
uid_t DynamicSession::originatorUid() const
{
	if (mHaveOriginatorUid)
		return mOriginatorUid;
	else
		MacOSError::throwMe(errSessionValueNotSet);
}


void DynamicSession::originatorUid(uid_t uid)
{
	checkOriginator();
	if (mHaveOriginatorUid)		// must not re-set this
		MacOSError::throwMe(errSessionAuthorizationDenied);
	mHaveOriginatorUid = true;
	mOriginatorUid = uid;

	Server::active().longTermActivity();
	struct passwd *pw = getpwuid(uid);

	if (pw != NULL) {

        mOriginatorCredential = Credential(uid, pw->pw_name ? pw->pw_name : "", pw->pw_gecos ? pw->pw_gecos : "", "", true/*shared*/);
        endpwent();
	}

	secdebug("SSsession", "%p session uid set to %d", this, uid);
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
        extBlob.session = bootstrapPort();
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
// The default session setup operation always fails.
// Subclasses can override this to support session setup calls.
//
void Session::setupAttributes(SessionCreationFlags flags, SessionAttributeBits attrs)
{
	MacOSError::throwMe(errSessionAuthorizationDenied);
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
	checkOriginator();
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
	Debug::dump(" boot=%d service=%d attrs=%#x authhost=%p securityagent=%p",
		mBootstrap.port(), mServicePort.port(), uint32_t(mAttributes), mAuthHost, mSecurityAgent);
}

#endif //DEBUGDUMP
