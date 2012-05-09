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
#include <security_utilities/ccaudit.h>
#include <security_cdsa_utilities/handletemplates_defs.h>
#include <security_cdsa_utilities/u32handleobject.h>
#include <security_cdsa_utilities/cssmdb.h>
#include <bsm/audit.h>
#include <bsm/audit_session.h>
#include <sys/event.h>

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
class Session : public PerSession {
public:
	typedef au_asid_t SessionId;			// internal session identifier (audit session id)

    Session(const CommonCriteria::AuditInfo &audit, Server &server);
	virtual ~Session();
    
	Server &server() const;

	SessionId sessionId() const { return mAudit.sessionId(); }
	CommonCriteria::AuditInfo &auditInfo() { return mAudit; }
    
	IFDUMP(virtual void dumpNode());
    
public:
    static const SessionAttributeBits settableAttributes =
        sessionHasGraphicAccess | sessionHasTTY | sessionIsRemote | AU_SESSION_FLAG_HAS_AUTHENTICATED;

    SessionAttributeBits attributes() const			{ updateAudit(); return mAudit.ai_flags; }
    bool attribute(SessionAttributeBits bits) const	{ return attributes() & bits; }
	void setAttributes(SessionAttributeBits bits);
	
    virtual void setupAttributes(SessionCreationFlags flags, SessionAttributeBits attrs);

	virtual uid_t originatorUid() const		{ updateAudit(); return mAudit.uid(); }

	virtual CFDataRef copyUserPrefs() = 0;

	static const char kUsername[];
    static const char kRealname[];
    
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

protected:
	void updateAudit() const;

private:
    struct AuthorizationExternalBlob {
        AuthorizationBlob blob;
		uint32_t session;
    };
	
protected:
    static AuthorizationToken &authorization(const AuthorizationBlob &blob);
	OSStatus authGetRights(AuthorizationToken &auth,
		const AuthItemSet &requestedRights, const AuthItemSet &environment,
		AuthorizationFlags flags, AuthItemSet &grantedRights);
	void mergeCredentials(CredentialSet &creds);

public:
    void invalidateSessionAuthHosts();      // invalidate auth hosts in this session
    static void invalidateAuthHosts();      // invalidate auth hosts in all sessions
	
	static void processSystemSleep();
	void processLockAll();

	RefPointer<AuthHostInstance> authhost(const AuthHostType hostType = securityAgent, const bool restart = false);

protected:
 	mutable CommonCriteria::AuditInfo mAudit;
	
	mutable Mutex mCredsLock;				// lock for mSessionCreds
	CredentialSet mSessionCreds;			// shared session authorization credentials

	mutable Mutex mAuthHostLock;
	AuthHostInstance *mSecurityAgent;
	AuthHostInstance *mAuthHost;
    
	CFRef<CFDataRef> mSessionAgentPrefs;
    Credential mOriginatorCredential;
	
	void kill();

public:
	static Session &find(SessionId id, bool create);	// find and optionally create
    template <class SessionType> static SessionType &find(SecuritySessionId id);
	static void destroy(SessionId id);

protected:
	typedef std::map<SessionId, RefPointer<Session> > SessionMap;
	static SessionMap mSessions;
	static Mutex mSessionLock;
};


template <class SessionType>
SessionType &Session::find(SecuritySessionId id)
{
	if (SessionType *ssn = dynamic_cast<SessionType *>(&find(id, false)))
		return *ssn;
	else
		MacOSError::throwMe(errSessionInvalidId);
}


//
// The RootSession is the session of all code that originates from system startup processing
// and does not belong to any particular login origin. (Or, if you prefer, whose login origin
// is the system itself.)
//
class RootSession : public Session {
public:
    RootSession(uint64_t attributes, Server &server);
	
	CFDataRef copyUserPrefs()           { return NULL; }
};


//
// A DynamicSession object represents a session that is dynamically constructed
// when we first encounter it. These sessions are actually created in client
// space using the audit session APIs.
// We tear down a DynamicSession when the system reports (via kevents) that the
// kernel audit session object has been destroyed.
//
class DynamicSession : private ReceivePort, public Session {
public:
    DynamicSession(const CommonCriteria::AuditInfo &audit);

	void setUserPrefs(CFDataRef userPrefsDict);
	CFDataRef copyUserPrefs();
};


#endif //_H_SESSION
