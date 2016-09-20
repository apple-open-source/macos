/*
 *  ccaudit_extensions.h
 *  securityd
 *
 *  Created by G H on 3/24/09.
 *  Copyright (c) 2009 Apple Inc. All Rights Reserved.
 *
 *  Extensions to utility classes in Security::CommonCriteria 
 *  (libsecurity_utilities).  Not clear that these are useful enough to be
 *  added there, so for now, they're here.  
 */

#include <string>
#include <stdint.h>
#include <Security/Authorization.h>
#include <bsm/audit_kevents.h>      // AUE_NULL
#include <bsm/libbsm.h>

//
// Regarding message formats in comments, below: 
//
//     <> denotes a string with the indicated information
//     '' denotes a literal string
// 
// Message info is in text tokens unless otherwise indicated.  
//

namespace Security
{

namespace CommonCriteria
{

namespace Securityd 
{

//
// Pure virtual class from which audit log writers should be derived.  
// The assumption about logging is that a "success" case logs certain
// data about what succeeded, while a "failure" case logs that same data
// plus some indication as to why the failure occurred.  
//
// Subclasses minimally need to provide a writeCommon() method.  They may
// override logSuccess(); q.v.  
//
// An AuditLogger is intended to live no longer than the audit trailer of a
// securityd IPC.  
//
// setClientInfo() must be called before logging, or at best, gibberish
// will be logged.  
//
// Nomenclature: 
//     "write" methods only au_write()
//     "log" methods open, write, and close the log
//
class AuditLogger
{
public:
    AuditLogger() : mAuditFd(-1), mEvent(AUE_NULL), mClientInfoSet(false)  { }
    AuditLogger(const audit_token_t *srcToken, short auEvent = AUE_NULL);
    AuditLogger(const AuditToken &srcToken, short auEvent = AUE_NULL);
    virtual ~AuditLogger();
    
    bool open();    // false if auditing disabled; throws on real errors
    void close(bool writeLog = true);   // throws if writeLog true but au_close() failed
    
    void setClientInfo(const audit_token_t *srcToken);
    void setClientInfo(const AuditToken &srcToken);
    void setEvent(short auEvent)  { mEvent = auEvent; }
    short event() const  { return mEvent; }
        
    // common log-writing activities
    void writeToken(token_t *token, const char *name);
    void writeSubject();
    void writeReturn(char status, int reterr);
    virtual void writeCommon() = 0; // should not open or close log
    
    // logSuccess() assumes that all the ancillary information you need is
    // written by writeCommon().  If that's not true, you can either
    // override logSuccess() in your subclass, or use a different method
    // altogether.  Do not call AuditLogger::logSuccess() from the subclass
    // in eiher case.  
    virtual void logSuccess();

    virtual void logFailure(const char *errMsg = NULL, int errcode = errAuthorizationDenied);
    virtual void logFailure(string &errMsg, int errcode = errAuthorizationDenied)  { logFailure(errMsg.c_str(), errcode); }
    
    // @@@  Extra credit: let callers add arbitrary tokens.  Tokens added
    // before a log*() call would be appended to the end of writeCommon()'s
    // standard set.  

protected:
    void logInternalError(const char *fmt, ...);
    
private:
    int mAuditFd;
    short mEvent;
    bool mClientInfoSet;    // disallow resetting client info
    
    uid_t mAuditId;
    uid_t mEuid;
    gid_t mEgid;
    uid_t mRuid;
    gid_t mRgid;
    pid_t mPid;
    au_asid_t mAuditSessionId;
    au_tid_t mOldTerminalId;    // to cache audit_token_to_au32() result
    au_tid_addr_t mTerminalId;  // @@@  AuditInfo still uses ai_tid_t
};

//
// KeychainAuthLogger format:
//     'System keychain authorization'
//     <keychain name>
//     <keychain item name>
//     [optional] <more failure info>
// 
// For QueryKeychainAuth audit logging
//
class KeychainAuthLogger : public AuditLogger
{
    static const char *sysKCAuthStr;
    static const char *unknownKCStr;
    static const char *unknownItemStr;
    
public:
    KeychainAuthLogger() : AuditLogger(), mDatabase(unknownKCStr), mItem(unknownItemStr)  { }
    KeychainAuthLogger(const audit_token_t *srcToken, short auEvent);
    KeychainAuthLogger(const audit_token_t *srcToken, short auEvent, const char *database, const char *item);
    KeychainAuthLogger(const AuditToken &srcToken, short auEvent);
    KeychainAuthLogger(const AuditToken &srcToken, short auEvent, const char *database, const char *item);
    void setDbName(const char *database);
    void setItemName(const char *item);
    virtual void writeCommon();
    
private:
    string mDatabase;
    string mItem;
};

// 
// RightLogger provides basic common data and behavior for rights-based
// logging classes.  @@@  "RightLogger" is a lousy name
//
class RightLogger
{
protected:
    static const char *unknownRightStr;

public:
    RightLogger() : mRight(unknownRightStr)  { }
    virtual ~RightLogger()  { }
    
    void setRight(const string &rightName);
    void setRight(const char *rightName);

protected:
    string mRight;
};

//
// Basic (per-mechanism) AuthMechLogger format:
//     <right name>
//     [optional] 'mechanism' <mechanism name>
//     [optional] <more info>
//
// e.g.:
//     com.foo.bar
//     mechanism FooPlugin:SomeMechanism
//     unknown mechanism; ending rule evaluation
//
class AuthMechLogger : public AuditLogger, public RightLogger
{
    static const char *unknownMechStr;
    static const char *mechStr;
    
public:
    AuthMechLogger() : AuditLogger(), RightLogger(), mEvaluatingMechanism(false), mCurrentMechanism(unknownMechStr)  { }
    AuthMechLogger(const AuditToken &srcToken, short auEvent);
    AuthMechLogger(const audit_token_t *srcToken, short auEvent);
    
    void setCurrentMechanism(const char *mech);    // pass NULL if not running mechs.  
    void setCurrentMechanism(const string &mech)  { setCurrentMechanism(mech.c_str()); }
    virtual void writeCommon();
    
    // Authorization mechanism-evaluation interrupts need to be logged since
    // they cause evaluation to restart, possibly at a different point in the 
    // mechanism chain.  
    void logInterrupt(const char *msg);     // NULL msg okay
    void logInterrupt(string &msg)  { logInterrupt(msg.c_str()); }
    
private:
    bool mEvaluatingMechanism;
    string mCurrentMechanism;
};

//
// Basic RightAuthenticationLogger formats:
//
// Per-credential (newly granted during an evaluation):
//     <right name>
//     UID of user performing the authentication [arg32 token]
//     UID and username of the successfully authenticated user [arg32 token]
// or:
//     <right name>
//     UID of user performing the authentication [arg32 token]
//     Name of the user as whom the first UID was attempting to authenticate
//
// Final (i.e., after all mechanisms) right-granting decision format:
//     <right name>
//     name of process requesting authorization
//     name of process that created the Authorization handle
//
// Least-privilege credential-generating event format:
//     <right name>
//     'least-privilege'
//
// @@@  each format should be its own class
// 
class RightAuthenticationLogger : public AuditLogger, public RightLogger
{
    static const char *unknownUserStr;
    static const char *unknownClientStr;
    static const char *unknownAuthCreatorStr;
    static const char *authenticatorStr;
    static const char *clientStr;
    static const char *authCreatorStr;
    static const char *authenticatedAsStr;
    static const char *leastPrivStr;

public:
    RightAuthenticationLogger() : AuditLogger(), RightLogger()  { }
    RightAuthenticationLogger(const AuditToken &srcToken, short auEvent);
    RightAuthenticationLogger(const audit_token_t *srcToken, short auEvent);
    virtual ~RightAuthenticationLogger()  { }
    
    virtual void writeCommon();
    
    virtual void logSuccess()  { }  // throw?  in any case, don't allow the usual logSuccess() to work
    // @@@  clean up, consolidate Success and AuthorizationResult
    void logSuccess(uid_t authenticator, uid_t target, const char *targetName);
    void logAuthorizationResult(const char *client, const char *authCreator, int errcode);
    void logLeastPrivilege(uid_t userId, bool isAuthorizingUser);
    virtual void logFailure(const char *errMsg, int errcode)  { AuditLogger::logFailure(errMsg, errcode); }
    void logAuthenticatorFailure(uid_t authenticator, const char *targetName);
};


}   // namespace Securityd

}   // namespace CommonCriteria
    
}   // namespace Security
