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
// session - authentication session domains
//
// A Session is defined by a mach_init bootstrap dictionary. These dictionaries are
// hierarchical and inherited, so they work well for characterization of processes
// that "belong" together. (Of course, if your mach_init is broken, you're in bad shape.)
//
// Sessions are multi-threaded objects.
//
#include "session.h"
#include "connection.h"
#include "server.h"


//
// The static session map
//
Session::SessionMap Session::sessionMap;
Mutex Session::sessionMapLock;


//
// Create a Session object from initial parameters (create)
//
Session::Session(Bootstrap bootstrap, Port servicePort, SessionAttributeBits attrs) 
    : mBootstrap(bootstrap), mServicePort(servicePort),
	  mAttributes(attrs), mProcessCount(0), mAuthCount(0), mDying(false)
{
    secdebug("SSsession", "%p CREATED: handle=0x%lx bootstrap=%d service=%d attrs=0x%lx",
        this, handle(), mBootstrap.port(), mServicePort.port(), mAttributes);
}


void Session::release()
{
	// nothing by default
}


//
// The root session inherits the startup bootstrap and service port
//
RootSession::RootSession(Port servicePort, SessionAttributeBits attrs)
    : Session(Bootstrap(), servicePort, sessionIsRoot | sessionWasInitialized | attrs)
{
    // self-install (no thread safety issues here)
    sessionMap[mServicePort] = this;
}


//
// Dynamic sessions use the given bootstrap and re-register in it
//
DynamicSession::DynamicSession(const Bootstrap &bootstrap)
	: ReceivePort(Server::active().bootstrapName(), bootstrap),
	  Session(bootstrap, *this)
{
	// tell the server to listen to our port
	Server::active().add(*this);
	
	// register for port notifications
    Server::active().notifyIfDead(bootstrapPort());	//@@@??? still needed?
	Server::active().notifyIfUnused(*this);

	// self-register
	StLock<Mutex> _(sessionMapLock);
	sessionMap[*this] = this;
}

DynamicSession::~DynamicSession()
{
	// remove our service port from the server
	Server::active().remove(*this);

	// if this is a (the) graphic login session, lock all databases
	secdebug("session", "%p Locking all %ld databases",
		this, databases().size());
	Database::lockAllDatabases(databases());
}


void DynamicSession::release()
{
	mBootstrap.destroy();
}


//
// Destroy a Session
//
Session::~Session()
{
    assert(mProcessCount == 0);	// can't die with processes still alive
    secdebug("SSsession", "%p DESTROYED: handle=0x%lx bootstrap=%d",
        this, handle(), mBootstrap.port());
}


//
// Locate a session object by service port or (Session API) identifier
//
Session &Session::find(Port servicePort)
{
    StLock<Mutex> _(sessionMapLock);
	SessionMap::const_iterator it = sessionMap.find(servicePort);
	assert(it != sessionMap.end());
	return *it->second;
}

Session &Session::find(SecuritySessionId id)
{
    switch (id) {
    case callerSecuritySession:
        return Server::connection().process.session;
    default:
        return findHandle<Session>(id);
    }
}


//
// Act on a death notification for a session's (sub)bootstrap port.
// We may not destroy the Session outright here (due to processes that use it),
// but we do clear out its accumulated wealth.
//
void Session::eliminate(Port servPort)
{
    // remove session from session map
    StLock<Mutex> _(sessionMapLock);
    SessionMap::iterator it = sessionMap.find(servPort);
    assert(it != sessionMap.end());
    Session *session = it->second;
    sessionMap.erase(it);
	
	// destroy the session service port (this releases mach_init to proceed)
	session->release();

    // clear resources
    if (session->clearResources())
        delete session;
    else
        secdebug("SSsession", "session %p zombified for %d processes and %d auths",
            session, int(session->mProcessCount), int(session->mAuthCount));
}

bool Session::clearResources()
{
    StLock<Mutex> _(mLock);
    
    // this session is now officially dying
    mDying = true;
    
    // invalidate shared credentials
    {
        StLock<Mutex> _(mCredsLock);
        
        IFDEBUG(if (!mSessionCreds.empty()) 
            secdebug("SSauth", "session %p clearing %d shared credentials", 
                this, int(mSessionCreds.size())));
        for (CredentialSet::iterator it = mSessionCreds.begin(); it != mSessionCreds.end(); it++)
            (*it)->invalidate();
    }
    // let the caller know if we are ready to die NOW
    return mProcessCount == 0 && mAuthCount == 0;
}


//
// Relay lockAllDatabases to all known sessions
//
void Session::lockAllDatabases(bool forSleep)
{
	StLock<Mutex> _(sessionMapLock);
	for (SessionMap::const_iterator it = begin(); it != end(); it++) {
	    secdebug("SSdb", "locking all %d known databases %s in session %p",
			int(it->second->databases().size()), forSleep ? " for sleep" : "", it->second);
		Database::lockAllDatabases(it->second->databases(), forSleep);
	}
}


//
// Process management
//
void Session::addProcess(Process *)
{
    StLock<Mutex> _(mLock);
    mProcessCount++;
}

bool Session::removeProcess(Process *)
{
    StLock<Mutex> _(mLock);
    assert(mProcessCount > 0);
    return --mProcessCount == 0 && mDying && mAuthCount == 0;
}


//
// Authorization retention management.
//
void Session::addAuthorization(AuthorizationToken *)
{
    StLock<Mutex> _(mLock);
    mAuthCount++;
}

bool Session::removeAuthorization(AuthorizationToken *)
{
    StLock<Mutex> _(mLock);
    assert(mAuthCount > 0);
    return --mAuthCount == 0 && mDying && mProcessCount == 0;
}


//
// Authorization operations
//
OSStatus Session::authCreate(const AuthItemSet &rights,
	const AuthItemSet &environment,
	AuthorizationFlags flags,
	AuthorizationBlob &newHandle,
	const security_token_t &securityToken)
{
	// invoke the authorization computation engine
	CredentialSet resultCreds;
	
	// this will acquire mLock, so we delay acquiring it
	auto_ptr<AuthorizationToken> auth(new AuthorizationToken(*this, resultCreds, securityToken));

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
	Server::connection().process.addAuthorization(auth.get());
	auth.release();
	return result;
}

void Session::authFree(const AuthorizationBlob &authBlob, AuthorizationFlags flags)
{
    AuthorizationToken::Deleter deleter(authBlob);
    AuthorizationToken &auth = deleter;
	Process &process = Server::connection().process;
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
    CredentialSet resultCreds;
    AuthorizationToken &auth = authorization(authBlob);
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
		&authorization(authBlob), int(rights.size()), int(grantedRights.size()));
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
	StLock<Mutex> _(mLock);
	if (auth.mayExternalize(Server::connection().process)) {
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
	if (sourceAuth.mayInternalize(Server::connection().process, true)) {
		StLock<Mutex> _(mLock);
		authBlob = extBlob.blob;
        Server::connection().process.addAuthorization(&sourceAuth);
        mAuthCount++;
        secdebug("SSauth", "Authorization %p internalized", &sourceAuth);
		return noErr;
	} else
		return errAuthorizationInternalizeNotAllowed;
}


//
// Set up a (new-ish) Session.
// This call must be made from a process within the session, and it must be the first
// such process to make the call.
//
void Session::setup(SessionCreationFlags flags, SessionAttributeBits attrs)
{
    // check current process object - it may have been cached before the client's bootstrap switch
    Process *process = &Server::connection().process;
    process->session.setupAttributes(attrs);
}


void Session::setupAttributes(SessionAttributeBits attrs)
{
    secdebug("SSsession", "%p setup attrs=0x%lx", this, attrs);
    if (attrs & ~settableAttributes)
        MacOSError::throwMe(errSessionInvalidAttributes);
    if (attribute(sessionWasInitialized))
        MacOSError::throwMe(errSessionAuthorizationDenied);
    setAttributes(attrs | sessionWasInitialized);
}


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

	secdebug("SSauth", "Authorization %p authorizationdbSet %s (result=%ld)",
		&authorization(authBlob), inRightName, result);
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

	secdebug("SSauth", "Authorization %p authorizationdbRemove %s (result=%ld)",
		&authorization(authBlob), inRightName, result);
	return result;
}


//
// Merge a set of credentials into the shared-session credential pool
//
// must hold mCredsLock
void Session::mergeCredentials(CredentialSet &creds)
{
    secdebug("SSsession", "%p merge creds @%p", this, &creds);
	for (CredentialSet::const_iterator it = creds.begin(); it != creds.end(); it++)
		if (((*it)->isShared() && (*it)->isValid())) {
			CredentialSet::iterator old = mSessionCreds.find(*it);
			if (old == mSessionCreds.end()) {
				mSessionCreds.insert(*it);
            } else {
                // replace "new" with "old" in input set to retain synchronization
				(*old)->merge(**it);
                creds.erase(it);
                creds.insert(*old);
            }
		}
}


//
// Locate an AuthorizationToken given a blob
//
AuthorizationToken &Session::authorization(const AuthorizationBlob &blob)
{
    AuthorizationToken &auth = AuthorizationToken::find(blob);
	Server::connection().process.checkAuthorization(&auth);
	return auth;
}
