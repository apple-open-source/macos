/*
 *  ccaudit_extensions.cpp
 *  securityd
 *
 *  Created by G H on 3/24/09.
 *  Copyright (c) 2009 Apple Inc. All Rights Reserved.
 *
 */

#include <errno.h>
#include <assert.h>
#include <stdio.h>                  // vsnprintf()
#include <stdarg.h>                 // va_start(), et al.
#include <syslog.h>
#include <string.h>                 // memcpy()
#include <bsm/audit_uevents.h>      // AUE_ssauth*
#include <bsm/libbsm.h>
#include <security_utilities/errors.h>
#include <security_utilities/ccaudit.h>
#include "ccaudit_extensions.h"

namespace Security
{
    
namespace CommonCriteria
{

namespace Securityd 
{

//
// AuditLogger
//
AuditLogger::AuditLogger(const audit_token_t *srcToken, short auEvent)
    : mAuditFd(-1), mEvent(auEvent), mClientInfoSet(false)
{
    setClientInfo(srcToken); 
}

AuditLogger::AuditLogger(const AuditToken &srcToken, short auEvent)
    : mAuditFd(-1), mEvent(auEvent), mClientInfoSet(false)
{
    setClientInfo(srcToken); 
}
    
AuditLogger::~AuditLogger()
{
    close();
}

bool 
AuditLogger::open()
{
    if (-1 != mAuditFd)
        return true;
    
    // @@@  use audit_get_cond() when it's available
    int acond = au_get_state();
    switch (acond)
    {
        case AUC_NOAUDIT:
            return false;
        case AUC_AUDITING:
            break;
        default:
            logInternalError("error checking auditing status (%d)", acond);
            UnixError::throwMe(acond);  // assume it's a Unix error
    }
    if ((mAuditFd = au_open()) < 0)
    {
        logInternalError("au_open() failed (%s)", strerror(errno));
        UnixError::throwMe(errno);
    }
    return true;
}

void 
AuditLogger::close(bool writeLog/* = true*/)
{
    if (-1 != mAuditFd)
    {
        int keep = writeLog == true ?  AU_TO_WRITE : AU_TO_NO_WRITE;
        int error = au_close(mAuditFd, keep, mEvent);
        mAuditFd = -1;
        if (writeLog == true && error < 0)
        {
            logInternalError("au_close() failed; record not committed");
            UnixError::throwMe(error);
        }
    }
}

void 
AuditLogger::setClientInfo(const audit_token_t *srcToken)
{
    assert(srcToken);
    audit_token_to_au32(*srcToken, &mAuditId, &mEuid, &mEgid, &mRuid, &mRgid, &mPid, &mAuditSessionId, &mOldTerminalId);

    mTerminalId.at_type = AU_IPv4;
    mTerminalId.at_addr[0] = mOldTerminalId.machine;
    mTerminalId.at_port = mOldTerminalId.port;
    
    mClientInfoSet = true;
}

void 
AuditLogger::setClientInfo(const AuditToken &srcToken)
{
    mAuditId = srcToken.auditId();
    mEuid = srcToken.euid();
    mEgid = srcToken.egid();
    mRuid = srcToken.ruid();
    mRgid = srcToken.rgid();
    mPid = srcToken.pid();
    mAuditSessionId = srcToken.sessionId();
    memcpy(&mOldTerminalId, &(srcToken.terminalId()), sizeof(mOldTerminalId));
    
    mTerminalId.at_type = AU_IPv4;
    mTerminalId.at_addr[0] = mOldTerminalId.machine;
    mTerminalId.at_port = mOldTerminalId.port;
    
    mClientInfoSet = true;
}

void
AuditLogger::writeToken(token_t *token, const char *name)
{
    const char *tokenName = name ? name : "<unidentified>";
    if (NULL == token)
    {
        logInternalError("Invalid '%s' token", tokenName);
        close();
        UnixError::throwMe(EPERM);      // per audit_submit()
    }
    if (au_write(mAuditFd, token) < 0)
    {
        logInternalError("Error writing '%s' token (%s)", tokenName, strerror(errno));
        close();
        UnixError::throwMe(errno);
    }
}

void 
AuditLogger::writeSubject()
{
    assert(mClientInfoSet);

    token_t *token;

    // @@@  terminal ID is not carried in the audit trailer nowadays, but 
    // this code should be harmless: it replicates the current logic in 
    // audit_submit()
    if (AU_IPv4 == mTerminalId.at_type)
        token = au_to_subject32(mAuditId, mEuid, mEgid, mRuid, mRgid, mPid, mAuditSessionId, &mOldTerminalId);
    else 
        token = au_to_subject_ex(mAuditId, mEuid, mEgid, mRuid, mRgid, mPid, mAuditSessionId, &mTerminalId);
    writeToken(token, "subject");
}

void 
AuditLogger::writeReturn(char status, int reterr)
{
    writeToken(au_to_return32(status, reterr), "return");
}

void 
AuditLogger::logSuccess()
{
    if (false == open())
        return;
    writeCommon();
    writeReturn(0, 0);
    close();
}

void
AuditLogger::logFailure(const char *errMsg, int errcode)
{
    if (false == open())
        return;
    writeCommon();
    if (errMsg)
        writeToken(au_to_text(errMsg), "evaluation error");
    writeReturn(EPERM, errcode);
    close();
}

// cribbed from audit_submit()
void
AuditLogger::logInternalError(const char *fmt, ...)
{
    va_list ap;
    char text[MAX_AUDITSTRING_LEN];
    
    if (fmt != NULL)
    {
        int error = errno;
        va_start(ap, fmt);
        (void)vsnprintf(text, MAX_AUDITSTRING_LEN, fmt, ap);
        va_end(ap);
        syslog(LOG_AUTH | LOG_ERR, "%s", text);
        errno = error;
    }
}

//
// KeychainAuthLogger
//
const char *KeychainAuthLogger::sysKCAuthStr = "System keychain authorization";
const char *KeychainAuthLogger::unknownKCStr = "<unknown keychain>";
const char *KeychainAuthLogger::unknownItemStr = "<unknown item>";

KeychainAuthLogger::KeychainAuthLogger(const audit_token_t *srcToken, short auEvent)
    : AuditLogger(srcToken, auEvent), mDatabase(unknownKCStr), 
      mItem(unknownItemStr)
{
}

KeychainAuthLogger::KeychainAuthLogger(const AuditToken &srcToken, short auEvent)
    : AuditLogger(srcToken, auEvent), mDatabase(unknownKCStr), 
      mItem(unknownItemStr)
{
}
    
KeychainAuthLogger::KeychainAuthLogger(const audit_token_t *srcToken, short auEvent, const char *database, const char *item)
    : AuditLogger(srcToken, auEvent)
{
    setDbName(database);
    setItemName(item);
}

KeychainAuthLogger::KeychainAuthLogger(const AuditToken &srcToken, short auEvent, const char *database, const char *item)
    : AuditLogger(srcToken, auEvent)
{
    setDbName(database);
    setItemName(item);
}

void
KeychainAuthLogger::setDbName(const char *database)
{
    mDatabase = database ? database : unknownKCStr;
}

void
KeychainAuthLogger::setItemName(const char *item)
{
    mItem = item ? item : unknownItemStr;
}

void 
KeychainAuthLogger::writeCommon()
{
    writeSubject();
    writeToken(au_to_text(sysKCAuthStr), sysKCAuthStr);
    writeToken(au_to_text(mDatabase.c_str()), "keychain");
    writeToken(au_to_text(mItem.c_str()), "keychain item");
}


//
// RightLogger
//
const char *RightLogger::unknownRightStr = "<unknown right>";

void 
RightLogger::setRight(const string &rightName)  
{
    mRight.clear(); 
    mRight = rightName;
}

void 
RightLogger::setRight(const char *rightName)
{
    if (rightName)      // NULL bad for string class and au_to_text()
    {
        string tmpStr(rightName);   // setRight() takes a string&
        setRight(tmpStr);
    }
}
    

//
// AuthMechLogger
//
const char *AuthMechLogger::unknownMechStr = "<unknown mechanism>";
const char *AuthMechLogger::mechStr = "mechanism ";

AuthMechLogger::AuthMechLogger(const AuditToken &srcToken, short auEvent)
    : AuditLogger(srcToken, auEvent), RightLogger(), 
      mEvaluatingMechanism(false), mCurrentMechanism(unknownMechStr)
{
}

AuthMechLogger::AuthMechLogger(const audit_token_t *srcToken, short auEvent)
    : AuditLogger(srcToken, auEvent), RightLogger(), 
      mEvaluatingMechanism(false), mCurrentMechanism(unknownMechStr)
{
}

void 
AuthMechLogger::setCurrentMechanism(const char *mech)
{ 
    mCurrentMechanism.clear();
    if (NULL == mech)
    {
        mEvaluatingMechanism = false;
    }
    else 
    {
        mCurrentMechanism = mech; 
        mEvaluatingMechanism = true; 
    }
}

void 
AuthMechLogger::writeCommon()
{
    writeSubject();
    writeToken(au_to_text(mRight.c_str()), "right");
    if (true == mEvaluatingMechanism)
    {
        string tmpStr = mechStr;    // mechStr includes a trailing space
        tmpStr += mCurrentMechanism;
        writeToken(au_to_text(tmpStr.c_str()), "mechanism");
    }
}

void 
AuthMechLogger::logInterrupt(const char *msg)
{
    if (false == open())
        return;
    writeCommon();
    if (msg)
        writeToken(au_to_text(msg), "interrupt");
    writeReturn(0, 0);
    close();
}

//
// RightAuthenticationLogger
//
const char *RightAuthenticationLogger::unknownUserStr = "<unknown user>";
const char *RightAuthenticationLogger::unknownClientStr = "<unknown client>";
const char *RightAuthenticationLogger::unknownAuthCreatorStr = "<unknown creator>";
const char *RightAuthenticationLogger::authenticatorStr = "known UID ";
const char *RightAuthenticationLogger::clientStr = "client ";
const char *RightAuthenticationLogger::authCreatorStr = "creator ";
const char *RightAuthenticationLogger::authenticatedAsStr = "authenticated as ";
const char *RightAuthenticationLogger::leastPrivStr = "least-privilege";

RightAuthenticationLogger::RightAuthenticationLogger(const AuditToken &srcToken, short auEvent)
    : AuditLogger(srcToken, auEvent), RightLogger()
{
}

RightAuthenticationLogger::RightAuthenticationLogger(const audit_token_t *srcToken, short auEvent)
    : AuditLogger(srcToken, auEvent), RightLogger()
{
}

void 
RightAuthenticationLogger::writeCommon()
{
    writeSubject();
    writeToken(au_to_text(mRight.c_str()), "right");
}

void
RightAuthenticationLogger::logSuccess(uid_t authenticator, uid_t target, const char *targetName)
{
    if (false == open())
        return;
    writeCommon();
    
    // au_to_arg32() is really meant for auditing syscall arguments; 
    // we're slightly abusing it to get descriptive strings for free.  
    writeToken(au_to_arg32(1, authenticatorStr, authenticator), "authenticator");
    string tmpStr(authenticatedAsStr);
    // targetName shouldn't be NULL on a successful authentication, but allow
    // for programmer screwups
    tmpStr += targetName ? targetName : unknownUserStr;
    writeToken(au_to_arg32(2, tmpStr.c_str(), target), "target");
    writeReturn(0, 0);
    close();
}

void 
RightAuthenticationLogger::logAuthorizationResult(const char *client, const char *authCreator, int errcode)
{
    if (false == open())
        return;
    writeCommon();
    string tmpStr(clientStr);
    tmpStr += client ? client : unknownClientStr;
    writeToken(au_to_text(tmpStr.c_str()), "Authorization client");
    tmpStr.clear();
    tmpStr = authCreatorStr;
    tmpStr += authCreator ? authCreator : unknownAuthCreatorStr;
    writeToken(au_to_text(tmpStr.c_str()), "Authorization creator");
    if (errAuthorizationSuccess == errcode)
        writeReturn(0, 0);
    else
        writeReturn(EPERM, errcode);
    close();
}

void 
RightAuthenticationLogger::logLeastPrivilege(uid_t userId, bool isAuthorizingUser)
{
    if (false == open())
        return;
    writeCommon();
    writeToken(au_to_text(leastPrivStr), leastPrivStr);
    writeReturn(0, 0);
    close();
}

void
RightAuthenticationLogger::logAuthenticatorFailure(uid_t authenticator, const char *targetName)
{
    if (false == open())
        return;
    writeCommon();
    writeToken(au_to_arg32(1, authenticatorStr, authenticator), "authenticator");
    if (NULL == targetName)
        writeToken(au_to_text(unknownUserStr), "target username");
    else
        writeToken(au_to_text(targetName), "target username");
    // @@@  EAUTH more appropriate, but !defined for _POSIX_C_SOURCE
    writeReturn(EPERM, errAuthorizationDenied);
    close();
}

}   // namespace Securityd
    
}   // namespace CommonCriteria

}   // namespace Security
