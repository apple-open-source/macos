/* Copyright (c) 2012 Apple Inc. All rights reserved. */

#include "ccaudit.h"
#include "debugging.h"
#include "process.h"
#include "authtoken.h"

#include <Security/Authorization.h>
#include <Security/AuthorizationPlugin.h>
#include <bsm/libbsm.h>


struct _ccaudit_s {
    __AUTH_BASE_STRUCT_HEADER__;
    
    int fd;
    int32_t event;

    auth_token_t auth;
    process_t proc;
    audit_info_s auditInfo;
    au_tid_t tid;
};

static void
_ccaudit_finalizer(CFTypeRef value)
{
    ccaudit_t ccaudit = (ccaudit_t)value;

    CFReleaseSafe(ccaudit->auth);
    CFReleaseSafe(ccaudit->proc);
}

AUTH_TYPE_INSTANCE(ccaudit,
                   .init = NULL,
                   .copy = NULL,
                   .finalize = _ccaudit_finalizer,
                   .equal = NULL,
                   .hash = NULL,
                   .copyFormattingDesc = NULL,
                   .copyDebugDesc = NULL
                   );

static CFTypeID ccaudit_get_type_id() {
    static CFTypeID type_id = _kCFRuntimeNotATypeID;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        type_id = _CFRuntimeRegisterClass(&_auth_type_ccaudit);
    });
    
    return type_id;
}

ccaudit_t
ccaudit_create(process_t proc, auth_token_t auth, int32_t event)
{
    ccaudit_t ccaudit = NULL;

    require(auth != NULL, done);
    
    ccaudit = (ccaudit_t)_CFRuntimeCreateInstance(kCFAllocatorDefault, ccaudit_get_type_id(), AUTH_CLASS_SIZE(ccaudit), NULL);
    require(ccaudit != NULL, done);
    
    ccaudit->auth = (auth_token_t)CFRetain(auth);
    ccaudit->proc = (process_t)CFRetain(proc);
    ccaudit->fd = -1;
    ccaudit->event = event;
    
    ccaudit->auditInfo = *auth_token_get_audit_info(auth);
    ccaudit->tid.port = ccaudit->auditInfo.tid;
    
done:
    return ccaudit;
}

static bool _enabled()
{
    static dispatch_once_t onceToken;
    static bool enabled = false;
    
    dispatch_once(&onceToken, ^{
        int acond = au_get_state();
        switch (acond) {
            case AUC_NOAUDIT:
                break;
            case AUC_AUDITING:
                enabled = true;
                break;
            default:
                LOGE("ccaudit: error checking auditing status (%d)", acond);
        }
    });

    return enabled;
}

static bool _open(ccaudit_t ccaudit)
{
    if (!_enabled()) {
        return false;
    }
    
    if (-1 != ccaudit->fd)
        return true;

    if ((ccaudit->fd = au_open()) < 0) {
        LOGE("ccaudit: au_open() failed (%s)", strerror(errno));
        return false;
    }

    return true;
}

static void _close(ccaudit_t ccaudit)
{
    if (-1 != ccaudit->fd) {
        int err = au_close(ccaudit->fd, AU_TO_WRITE, (short)ccaudit->event);
        ccaudit->fd = -1;
        if (err < 0) {
            LOGE("ccaudit: au_close() failed; record not committed");
        }
    }
}

static bool _write(ccaudit_t ccaudit, token_t * token, const char * name)
{
    const char *tokenName = name ?  name : "<unidentified>";
    if (NULL == token)
    {
        LOGE("ccaudit: invalid '%s' token", tokenName);
        return false;
    }
    if (au_write(ccaudit->fd, token) < 0) {
        LOGE("ccaudit: error writing '%s' token (%s)", tokenName, strerror(errno));
        return false;
    }
    return true;
}

static bool _subject(ccaudit_t ccaudit)
{
    token_t * token = au_to_subject32(ccaudit->auditInfo.auid, ccaudit->auditInfo.euid, ccaudit->auditInfo.egid,
                                      ccaudit->auditInfo.ruid, ccaudit->auditInfo.rgid, ccaudit->auditInfo.pid, ccaudit->auditInfo.asid, &ccaudit->tid);
    return _write(ccaudit, token, "subject");
}

void ccaudit_log_authorization(ccaudit_t ccaudit, const char * right, OSStatus err)
{

    if (!_open(ccaudit)) {
        return;
    }
    char buf[PATH_MAX+1];
    
    _subject(ccaudit);
    _write(ccaudit, au_to_text(right), "right");
    snprintf(buf, sizeof(buf), "client %s", process_get_code_url(ccaudit->proc));
    _write(ccaudit, au_to_text(buf), "Authorization client");
    snprintf(buf, sizeof(buf), "creator %s", auth_token_get_code_url(ccaudit->auth));
    _write(ccaudit, au_to_text(buf), "Authorization creator");
    
    if (auth_token_least_privileged(ccaudit->auth)) {
        _write(ccaudit, au_to_text("least-privilege"), "least-privilege");
    }
    
    if (err == errAuthorizationSuccess) {
        _write(ccaudit, au_to_return32(0, 0), "return");
    } else {
        _write(ccaudit, au_to_return32(EPERM, (uint32_t)err), "return");
    }
    
    _close(ccaudit);
}

void ccaudit_log_success(ccaudit_t ccaudit, credential_t cred, const char * right)
{

    if (!_open(ccaudit)) {
        return;
    }
    char buf[PATH_MAX+1];
    
    _subject(ccaudit);
    _write(ccaudit, au_to_text(right), "right");
    _write(ccaudit, au_to_arg32(1, "known UID ", auth_token_get_uid(ccaudit->auth)), "authenticator");
    snprintf(buf, sizeof(buf), "authenticated as %s", credential_get_name(cred));
    _write(ccaudit, au_to_arg32(2, buf, credential_get_uid(cred)), "target");
    _write(ccaudit, au_to_return32(0, 0), "return");
    
    _close(ccaudit);
}

void ccaudit_log_failure(ccaudit_t ccaudit, const char * credName, const char * right)
{

    if (!_open(ccaudit)) {
        return;
    }
    _subject(ccaudit);
    _write(ccaudit, au_to_text(right), "right");
    _write(ccaudit, au_to_arg32(1, "authenticated as ", auth_token_get_uid(ccaudit->auth)), "authenticator");
    
    if (NULL == credName) {
        _write(ccaudit, au_to_text("<unknown user>"), "target username");
    } else {
        _write(ccaudit, au_to_text(credName), "target username");
    }
    _write(ccaudit, au_to_return32(EPERM, (uint32_t)errAuthorizationDenied), "return");
    
    _close(ccaudit);
}

void ccaudit_log_mechanism(ccaudit_t ccaudit, const char * right, const char * mech, uint32_t status, const char * interrupted)
{

    if (!_open(ccaudit)) {
        return;
    }
    char buf[PATH_MAX+1];
    
    _subject(ccaudit);
    _write(ccaudit, au_to_text(right), "right");
    snprintf(buf, sizeof(buf), "mechanism %s", mech);
    _write(ccaudit, au_to_text(buf), "mechanism");
    
    if (interrupted) {
        _write(ccaudit, au_to_text(interrupted), "interrupt");
    }
    
    if (status == kAuthorizationResultAllow) {
        _write(ccaudit, au_to_return32(0, 0), "return");
    } else {
        _write(ccaudit, au_to_return32(EPERM, (uint32_t)status), "return");
    }
    
    _close(ccaudit);
}

void ccaudit_log(ccaudit_t ccaudit, const char * right, const char * msg, OSStatus err)
{
    if (!_open(ccaudit)) {
        return;
    }
    
    _subject(ccaudit);
    _write(ccaudit, au_to_text(right), "right");
    
    if (msg) {
        _write(ccaudit, au_to_text(msg), "evaluation error");
    }
    
    if (err == errAuthorizationSuccess) {
        _write(ccaudit, au_to_return32(0, 0), "return");
    } else {
        _write(ccaudit, au_to_return32(EPERM, (uint32_t)err), "return");
    }
    
    _close(ccaudit);
}
