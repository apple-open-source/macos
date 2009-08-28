/*
 * Copyright (c) 2000-2004,2008 Apple Inc. All Rights Reserved.
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
#ifndef _H_SESSION
#define _H_SESSION

#include "structure.h"
#include "acls.h"
#include "authority.h"
#include "authhost.h"
#include <Security/AuthSession.h>
#include <security_cdsa_utilities/handletemplates_defs.h>
#include <security_cdsa_utilities/u32handleobject.h>
#include <security_cdsa_utilities/cssmdb.h>

#if __GNUC__ > 2
#include <ext/hash_map>
using __gnu_cxx::hash_map;
#else
#include <hash_map>
#endif


class Key;
class Connection;
class Server;
class AuthHostInstance;

//
// A Session object represents one or more Connections that are known to
// belong to the same authentication domain. Informally this means just
// about "the same user", for the right definition of "user." The upshot
// is that global credentials can be shared by Connections of one Session
// with a modicum of security, and so Sessions are the natural nexus of
// single-sign-on functionality.
//
class Session : public U32HandleObject, public PerSession {
public:
    typedef MachPlusPlus::Bootstrap Bootstrap;

    Session(Bootstrap bootstrap, Port servicePort, SessionAttributeBits attrs = 0);
	virtual ~Session();
    
    Bootstrap bootstrapPort() const		{ return mBootstrap; }
	Port servicePort() const			{ return mServicePort; }
    
	IFDUMP(virtual void dumpNode());
    
public:
    static const SessionAttributeBits settableAttributes =
        sessionHasGraphicAccess | sessionHasTTY | sessionIsRemote;

    SessionAttributeBits attributes() const			{ return mAttributes; }
    bool attribute(SessionAttributeBits bits) const	{ return mAttributes & bits; }
	
    virtual void setupAttributes(SessionCreationFlags flags, SessionAttributeBits attrs);

	virtual bool haveOriginatorUid() const = 0;
	virtual uid_t originatorUid() const = 0;
    Credential originatorCredential() const { return mOriginatorCredential; }

	virtual CFDataRef copyUserPrefs() = 0;

	static std::string kUsername;
    static std::string kRealname;
    
protected:
    void setAttributes(SessionAttributeBits attrs)	{ mAttributes |= attrs; }
    
public:
	const CredentialSet &authCredentials() const	{ return mSessionCreds; }

    //
    // For external Authorization clients
    //
	OSStatus authCreate(const AuthItemSet &rights, const AuthItemSet &environment,
		AuthorizationFlags flags, AuthorizationBlob &newHandle, const audit_token_t &auditToken);
	void authFree(const AuthorizationBlob &auth, AuthorizationFlags flags);
	static OSStatus authGetRights(const AuthorizationBlob &auth,
		const AuthItemSet &requestedRights, const AuthItemSet &environment,
		AuthorizationFlags flags, AuthItemSet &grantedRights);
	OSStatus authGetInfo(const AuthorizationBlob &auth, const char *tag, AuthItemSet &contextInfo);
    
	OSStatus authExternalize(const AuthorizationBlob &auth, AuthorizationExternalForm &extForm);
	OSStatus authInternalize(const AuthorizationExternalForm &extForm, AuthorizationBlob &auth);

	OSStatus authorizationdbGet(AuthorizationString inRightName, CFDictionaryRef *rightDict);
	OSStatus authorizationdbSet(const AuthorizationBlob &authBlob, AuthorizationString inRightName, CFDictionaryRef rightDict);
	OSStatus authorizationdbRemove(const AuthorizationBlob &authBlob, AuthorizationString inRightName);
    
    //
    // Authorization methods for securityd's internal use
    //
    OSStatus authCheckRight(string &rightName, Connection &connection, bool allowUI);
    // authCheckRight() with exception-handling and Boolean return semantics
    bool isRightAuthorized(string &rightName, Connection &connection, bool allowUI);

private:
    struct AuthorizationExternalBlob {
        AuthorizationBlob blob;
        mach_port_t session;
    };
	
protected:
    static AuthorizationToken &authorization(const AuthorizationBlob &blob);
	OSStatus authGetRights(AuthorizationToken &auth,
		const AuthItemSet &requestedRights, const AuthItemSet &environment,
		AuthorizationFlags flags, AuthItemSet &grantedRights);
	void mergeCredentials(CredentialSet &creds);

public:
    static Session &find(Port servPort);
    static Session &find(SecuritySessionId id);
	template <class SessionType> static SessionType &find(SecuritySessionId id);
    static void destroy(Port servPort);
    void invalidateSessionAuthHosts();      // invalidate auth hosts in this session
    static void invalidateAuthHosts();      // invalidate auth hosts in all sessions
	
	static void processSystemSleep();
	void processLockAll();

	RefPointer<AuthHostInstance> authhost(const AuthHostType hostType = securityAgent, const bool restart = false);

protected:
    Bootstrap mBootstrap;			// session bootstrap port
	Port mServicePort;				// SecurityServer service port for this session
    SessionAttributeBits mAttributes; // attribute bits (see AuthSession.h)

    mutable Mutex mCredsLock;	// lock for mSessionCreds
	CredentialSet mSessionCreds;	// shared session authorization credentials

	mutable Mutex mAuthHostLock;
	AuthHostInstance *mSecurityAgent;
	AuthHostInstance *mAuthHost;
    
	CFRef<CFDataRef> mSessionAgentPrefs;
    Credential mOriginatorCredential;
	
	void kill();
	
protected:
	static PortMap<Session> mSessions;
};

template <class SessionType>
SessionType &Session::find(SecuritySessionId id)
{
	if (SessionType *ssn = dynamic_cast<SessionType *>(&find(id)))
		return *ssn;
	else
		MacOSError::throwMe(errSessionInvalidId);
}


//
// The RootSession is the session (i.e. bootstrap dictionary) of system daemons that are
// started early and don't belong to anything more restrictive. The RootSession is considered
// immortal.
// Currently, telnet sessions et al also default into this session, but this will change
// (we hope).
//
class RootSession : public Session {
public:
    RootSession(Server &server, SessionAttributeBits attrs = 0);
	
	bool haveOriginatorUid() const		{ return true; }
	uid_t originatorUid() const         { return 0; }
	CFDataRef copyUserPrefs()           { return NULL; }
};


//
// A DynamicSession is the default type of session object. We create one when a new
// Connection initializes whose bootstrap port we haven't seen before. These Sessions
// are torn down when their bootstrap object disappears (which happens when mach_init
// destroys it due to its requestor referent vanishing).
//
class DynamicSession : private ReceivePort, public Session {
public:
    DynamicSession(TaskPort taskPort);
	~DynamicSession();
	
	void setupAttributes(SessionCreationFlags flags, SessionAttributeBits attrs);

	bool haveOriginatorUid() const					{ return mHaveOriginatorUid; }
	uid_t originatorUid() const;
	void originatorUid(uid_t uid);
	void setUserPrefs(CFDataRef userPrefsDict);
	CFDataRef copyUserPrefs();
	
protected:
	void checkOriginator();			// fail unless current process is originator
	void kill();					// augment parent's kill

private:
	Port mOriginatorTask;			// originating process's task port
	bool mHaveOriginatorUid;		// originator uid was set by session originator
	uid_t mOriginatorUid;			// uid as set by session originator
};


#endif //_H_SESSION
