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
Session::Session(Bootstrap bootstrap, SessionAttributeBits attrs) 
    : mBootstrap(bootstrap), mAttributes(attrs), mProcessCount(0), mAuthCount(0), mDying(false)
{
    debug("SSsession", "%p CREATED: handle=0x%lx bootstrap=%d attrs=0x%lx",
        this, handle(), mBootstrap.port(), mAttributes);
}

RootSession::RootSession()
    : Session(Bootstrap(), sessionIsRoot | sessionWasInitialized)
{
    // self-install
    sessionMap[mBootstrap] = this;
}

DynamicSession::DynamicSession(Bootstrap bootstrap) : Session(bootstrap)
{
    Server::active().notifyIfDead(bootstrapPort());
}


//
// Destroy a Session
//
Session::~Session()
{
    assert(mProcessCount == 0);	// can't die with processes still alive
    Database::lockAllDatabases();
    debug("SSsession", "%p DESTROYED: handle=0x%lx bootstrap=%d",
        this, handle(), mBootstrap.port());
}


//
// Retrieve or create a session object
//
Session &Session::find(Bootstrap bootstrap, bool makeNew)
{
    StLock<Mutex> _(sessionMapLock);
    Session * &slot = sessionMap[bootstrap];
    if (slot == NULL)
        if (makeNew)
            slot = new DynamicSession(bootstrap);
        else
            Authorization::Error::throwMe(errAuthorizationInvalidRef);
    return *slot;
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
void Session::eliminate(Bootstrap bootstrap)
{
    // remove session from session map
    StLock<Mutex> _(sessionMapLock);
    SessionMap::iterator it = sessionMap.find(bootstrap);
    assert(it != sessionMap.end());
    Session *session = it->second;
    sessionMap.erase(it);

    // clear resources
    if (session->clearResources())
        delete session;
    else
        debug("SSsession", "session %p zombified for %d processes and %d auths",
            session, int(session->mProcessCount), int(session->mAuthCount));
}

bool Session::clearResources()
{
    StLock<Mutex> _(mLock);
    
    // this session is now officially dying
    mDying = true;
    
    // invalidate shared credentials
    IFDEBUG(if (!mSessionCreds.empty()) 
        debug("SSauth", "session %p clearing %d shared credentials", 
            this, int(mSessionCreds.size())));
    for (CredentialSet::iterator it = mSessionCreds.begin(); it != mSessionCreds.end(); it++)
        (*it)->invalidate();
    
    // let the caller know if we are ready to die NOW
    return mProcessCount == 0 && mAuthCount == 0;
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
OSStatus Session::authCreate(const RightSet &rights,
	const AuthorizationEnvironment *environment,
	AuthorizationFlags flags,
	AuthorizationBlob &newHandle)
{
	// invoke the authorization computation engine
	CredentialSet resultCreds;
	
	// this will acquire mLock, so we delay acquiring it
	auto_ptr<AuthorizationToken> auth(new AuthorizationToken(*this, resultCreds));
	
	OSStatus result = Server::authority().authorize(rights, environment, flags,
        &mSessionCreds, &resultCreds, NULL, *auth);
	newHandle = auth->handle();

	{
		StLock<Mutex> _(mLock);

		// merge resulting creds into shared pool
		if ((flags & kAuthorizationFlagExtendRights) && 
			!(flags & kAuthorizationFlagDestroyRights)) {
			mergeCredentials(resultCreds);
			auth->mergeCredentials(resultCreds);
		}
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

	if (flags & kAuthorizationFlagDestroyRights) {
		// explicitly invalidate all shared credentials and remove them from the session
		for (CredentialSet::const_iterator it = auth.begin(); it != auth.end(); it++)
			if ((*it)->isShared())
				(*it)->invalidate();
	}

	// now get rid of the authorization itself
	if (Server::connection().process.removeAuthorization(&auth))
        deleter.remove();
}

OSStatus Session::authGetRights(const AuthorizationBlob &authBlob,
	const RightSet &rights, const AuthorizationEnvironment *environment,
	AuthorizationFlags flags,
	MutableRightSet &grantedRights)
{
	StLock<Mutex> _(mLock);
	CredentialSet resultCreds;
	AuthorizationToken &auth = authorization(authBlob);
	CredentialSet effective = auth.effectiveCreds();
	OSStatus result = Server::authority().authorize(rights, environment, flags, 
        &effective, &resultCreds, &grantedRights, auth);

	// merge resulting creds into shared pool
	if ((flags & kAuthorizationFlagExtendRights) && !(flags & kAuthorizationFlagDestroyRights)) {
		mergeCredentials(resultCreds);
		auth.mergeCredentials(resultCreds);
	}

	IFDEBUG(debug("SSauth", "Authorization %p copyRights asked for %d got %d",
		&authorization(authBlob), int(rights.size()), int(grantedRights.size())));
	return result;
}

OSStatus Session::authGetInfo(const AuthorizationBlob &authBlob,
	const char *tag,
	MutableRightSet &grantedRights)
{
	StLock<Mutex> _(mLock);
	AuthorizationToken &auth = authorization(authBlob);
	debug("SSauth", "Authorization %p get-info not implemented", &auth);
    if (tag) {	// no such tag (no info support)
        return errAuthorizationInvalidTag;
    } else {	// return no tags (no info support)
        grantedRights = RightSet();	// return no entries
        return noErr;
    }
}

OSStatus Session::authExternalize(const AuthorizationBlob &authBlob, 
	AuthorizationExternalForm &extForm)
{
	StLock<Mutex> _(mLock);
	const AuthorizationToken &auth = authorization(authBlob);
	if (auth.mayExternalize(Server::connection().process)) {
		memset(&extForm, 0, sizeof(extForm));
        AuthorizationExternalBlob &extBlob =
            reinterpret_cast<AuthorizationExternalBlob &>(extForm);
        extBlob.blob = auth.handle();
        extBlob.session = bootstrapPort();
		debug("SSauth", "Authorization %p externalized", &auth);
		return noErr;
	} else
		return errAuthorizationExternalizeNotAllowed;
}

OSStatus Session::authInternalize(const AuthorizationExternalForm &extForm, 
	AuthorizationBlob &authBlob)
{
	StLock<Mutex> _(mLock);
	
	// interpret the external form
    const AuthorizationExternalBlob &extBlob = 
        reinterpret_cast<const AuthorizationExternalBlob &>(extForm);
	
    // locate source authorization
    AuthorizationToken &sourceAuth = AuthorizationToken::find(extBlob.blob);
    
	// check for permission and do it
	if (sourceAuth.mayInternalize(Server::connection().process, true)) {
		authBlob = extBlob.blob;
        Server::connection().process.addAuthorization(&sourceAuth);
        mAuthCount++;
        debug("SSauth", "Authorization %p internalized", &sourceAuth);
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
    if (process->taskPort().bootstrap() != process->session.bootstrapPort())
        process = Server::active().resetConnection();
    process->session.setupAttributes(attrs);
}


void Session::setupAttributes(SessionAttributeBits attrs)
{
    debug("SSsession", "%p setup attrs=0x%lx", this, attrs);
    if (attrs & ~settableAttributes)
        MacOSError::throwMe(errSessionInvalidAttributes);
    if (attribute(sessionWasInitialized))
        MacOSError::throwMe(errSessionAuthorizationDenied);
    setAttributes(attrs | sessionWasInitialized);
}


//
// Merge a set of credentials into the shared-session credential pool
//
void Session::mergeCredentials(CredentialSet &creds)
{
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
    return AuthorizationToken::find(blob);
}
