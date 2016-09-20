/*
 * Copyright (c) 2000-2010,2012-2013 Apple Inc. All Rights Reserved.
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
#include "authhost.h"
#include <Security/AuthSession.h>
#include <security_utilities/casts.h>
#include <security_utilities/ccaudit.h>
#include <security_cdsa_utilities/handletemplates_defs.h>
#include <security_cdsa_utilities/u32handleobject.h>
#include <security_cdsa_utilities/cssmdb.h>
#include <bsm/audit.h>
#include <bsm/audit_session.h>
#include <sys/event.h>
#include "securityd_service/securityd_service/securityd_service_client.h"

class Key;
class Connection;
class Server;
class AuthHostInstance;

enum {
    session_keybag_locked           = 0,
    session_keybag_unlocked         = 1 << 0,
    session_keybag_check_master_key = 1 << 1,
    session_keybag_loaded           = 1 << 2,
};

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

    SessionAttributeBits attributes() const			{ updateAudit(); return int_cast<au_asflgs_t,SessionAttributeBits>(mAudit.ai_flags); }
    bool attribute(SessionAttributeBits bits) const	{ return attributes() & bits; }
	void setAttributes(SessionAttributeBits bits);
	
    virtual void setupAttributes(SessionCreationFlags flags, SessionAttributeBits attrs);

	virtual uid_t originatorUid();

	static const char kUsername[];
    static const char kRealname[];
    
protected:
	void updateAudit() const;

public:
    void invalidateSessionAuthHosts();      // invalidate auth hosts in this session
    static void invalidateAuthHosts();      // invalidate auth hosts in all sessions
	
	static void processSystemSleep();
	void processLockAll();

	RefPointer<AuthHostInstance> authhost(const bool restart = false);

protected:
 	mutable CommonCriteria::AuditInfo mAudit;
	
	mutable Mutex mAuthHostLock;
	AuthHostInstance *mSecurityAgent;
	
	void kill();

public:
    void verifyKeyStorePassphrase(int32_t retries);
    void changeKeyStorePassphrase();
    void resetKeyStorePassphrase(const CssmData &passphrase);
    service_context_t get_current_service_context();
    void keybagClearState(int state);
    void keybagSetState(int state);
    bool keybagGetState(int state);
private:
    int mKeybagState;

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
};


#endif //_H_SESSION
