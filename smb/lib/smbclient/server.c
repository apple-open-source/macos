/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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

#include "smbclient.h"

#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include <readpassphrase.h>
#include <libkern/OSAtomic.h>
#include <netsmb/smb.h>
#include <netsmb/smb_lib.h>
#include <netsmb/smb_conn.h>
#include <CoreFoundation/CoreFoundation.h>
#include <NetFS/NetFS.h>
#include <NetFS/NetFSPrivate.h>

typedef int32_t refcount_t;

struct smb_server_handle
{
    volatile refcount_t refcount;
    struct smb_ctx * context;
};

static NTSTATUS
SMBLibraryInit(void)
{
    if (smb_load_library() != 0) {
        return make_nterror(NT_STATUS_NO_SUCH_DEVICE);
    }

    return NT_STATUS_SUCCESS;
}

/* Allocate a new SMBHANDLE. */
static NTSTATUS
SMBAllocateServer(
    SMBHANDLE * phConnection)
{
    SMBHANDLE hConnection;

    hConnection = calloc(1, sizeof(struct smb_server_handle));
    if (hConnection == NULL) {
        return make_nterror(NT_STATUS_NO_MEMORY);
    }

    hConnection->context = smb_create_ctx();
    if (hConnection->context == NULL) {
        return make_nterror(NT_STATUS_NO_MEMORY);
    }

    SMBRetainServer(hConnection);
    *phConnection = hConnection;
    return NT_STATUS_SUCCESS;
}

static NTSTATUS
SMBCreateURLFromString(
    const char * pTarget,
    CFURLRef *   pTargetURL)
{
    CFStringRef cfstr;
    CFURLRef    cfurl;

    if (*pTarget == '\\' && *(pTarget + 1) == '\\') {
        /* We have a UNC path that we need to break down into a SMB URL. */

        /* Until we write some UNC parsing code, just fail ... */
        return make_nterror(NT_STATUS_OBJECT_PATH_SYNTAX_BAD);
    }

    cfstr = CFStringCreateWithCString(kCFAllocatorDefault, pTarget,
                                            kCFStringEncodingUTF8);
    if (cfstr == NULL) {
        return make_nterror(NT_STATUS_NO_MEMORY);
    }

    cfurl = CFURLCreateWithString(kCFAllocatorDefault, cfstr, NULL);
    CFRelease(cfstr);

    if (cfurl == NULL) {
        return make_nterror(NT_STATUS_OBJECT_PATH_SYNTAX_BAD);
    }

    *pTargetURL = cfurl;
    return NT_STATUS_SUCCESS;
}

static CFMutableDictionaryRef
SMBCreateDefaultOptions(void)
{
    CFMutableDictionaryRef options;

    options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0 /* capacity */,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!options) {
        return NULL;
    }

    /* Share an existing VC if possible. */
    CFDictionarySetValue(options, kNetFSForceNewSessionKey, kCFBooleanFalse);

    return options;
}

/* Prompt for a password and set it on the SMB context. */
static NTSTATUS
SMBPasswordPrompt(
    SMBHANDLE hConnection)
{
    void *      hContext = NULL;
    NTSTATUS    status;
    char *      passwd;
    char        passbuf[SMB_MAXPASSWORDLEN + 1];
    char        prompt[128];

    status = SMBServerContext(hConnection, &hContext);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* If the password is already set, don't prompt. We don't want
     * to prompt on every retry ...
     */
    if (((struct smb_ctx *)hContext)->ct_setup.ioc_password[0] != '\0') {
        return NT_STATUS_SUCCESS;
    }

    snprintf(prompt, sizeof(prompt), "Password for %s: ",
            ((struct smb_ctx *)hContext)->serverName);

    /* If we have a TTY, read a password and retry ... */
    passwd = readpassphrase(prompt, passbuf, sizeof(passbuf),
            RPP_REQUIRE_TTY);
    if (passwd) {
        smb_ctx_setpassword(hContext, passwd);
        memset(passbuf, 0, sizeof(passbuf));
        return NT_STATUS_SUCCESS;
    } else {
        /* XXX Map errno to NTSTATUS */
        return NT_STATUS_ILL_FORMED_PASSWORD;
    }
}

static NTSTATUS
SMBServerConnect(
        SMBHANDLE   hConnection,
        CFURLRef    targetUrl,
        CFMutableDictionaryRef netfsOptions,
        SMBAuthType authType)
{
    void * hContext;
    NTSTATUS status;
    int err;

    status = SMBServerContext(hConnection, &hContext);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* Reset all the authentication options. This puts us into the
     * state of requiring user authentication (ie. forces NTLM).
     */
    CFDictionarySetValue(netfsOptions, kNetFSUseKerberosKey, kCFBooleanFalse);
    CFDictionarySetValue(netfsOptions, kNetFSUseGuestKey, kCFBooleanFalse);
    CFDictionarySetValue(netfsOptions, kNetFSUseAnonymousKey, kCFBooleanFalse);

    switch (authType) {
        case kSMBAuthTypeKerberos:
            CFDictionarySetValue(netfsOptions, kNetFSUseKerberosKey,
                    kCFBooleanTrue);
            break;
        case kSMBAuthTypeUser:
            /* If no authentication options are set, this is the default. */
            break;
        case kSMBAuthTypeGuest:
            CFDictionarySetValue(netfsOptions, kNetFSUseGuestKey,
                    kCFBooleanTrue);
            break;
        case kSMBAuthTypeAnonymous:
            CFDictionarySetValue(netfsOptions, kNetFSUseAnonymousKey,
                    kCFBooleanTrue);
            break;
        default:
            return make_nterror(NT_STATUS_INVALID_PARAMETER);
    }

    err = smb_open_session(hContext, targetUrl, netfsOptions,
            NULL /* [OUT] session_info */);
    if (err == EAUTH) {
        return make_nterror(NT_STATUS_LOGON_FAILURE);
    }

    if (err) {
        /* XXX map real NTSTATUS code */
        return make_nterror(NT_STATUS_CONNECTION_REFUSED);
    }

    return NT_STATUS_SUCCESS;
}

NTSTATUS
SMBServerContext(
    SMBHANDLE hConnection,
    void ** phContext)
{
    if (hConnection == NULL || hConnection->context == NULL) {
        return make_nterror(NT_STATUS_INVALID_HANDLE);
    }

    *phContext = hConnection->context;
    return NT_STATUS_SUCCESS;
}

NTSTATUS
SMBOpenServer(
    const char * pTargetServer,
    SMBHANDLE * phConnection)
{
    return SMBOpenServerEx(pTargetServer, phConnection, 0);
}

NTSTATUS
SMBOpenServerEx(
    const char * pTargetServer,
    SMBHANDLE * phConnection,
    uint64_t    options)
{
    NTSTATUS    status;
    int         err;
    void *      hContext;

    CFURLRef    targetUrl = NULL;
    CFMutableDictionaryRef netfsOptions = NULL;

    *phConnection = NULL;

    status = SMBLibraryInit();
    if (!NT_SUCCESS(status)) {
        goto done;
    }

    netfsOptions = SMBCreateDefaultOptions();
    if (netfsOptions == NULL) {
        status = make_nterror(NT_STATUS_NO_MEMORY);
        goto done;
    }

    status = SMBCreateURLFromString(pTargetServer, &targetUrl);
    if (!NT_SUCCESS(status)) {
        goto done;
    }

    status = SMBAllocateServer(phConnection);
    if (!NT_SUCCESS(status)) {
        goto done;
    }

    status = SMBServerContext(*phConnection, &hContext);
    if (!NT_SUCCESS(status)) {
        goto done;
    }

    /* If the target hasn't set a user name, let's assume that we should use
     * the current username. This is almost always what the caller wants.
     */
    if (((struct smb_ctx *)hContext)->ct_setup.ioc_user[0] == '\0') {
        struct passwd * pwent;

        pwent = getpwuid(geteuid());
        if (pwent) {
            smb_ctx_setuser(hContext, pwent->pw_name);
        }
    }

    /* If the target doesn't contain a share name, let's assume that the
     * caller means IPC$.
     */
    if (!((struct smb_ctx *)hContext)->ct_origshare) {
        err = smb_ctx_setshare(hContext, "IPC$", SMB_ST_ANY);
        if (err) {
            status = make_nterror(NT_STATUS_NO_MEMORY);
            goto done;
        }
    }

    /* XXX We really should port this code to use the lower-level API so that
     * we can give up immediately if the TCP connection fails..
     */

    /* Our default connection strategy is to always use the name of the
     * logged-in user. We try Kerberos and then fall back to NTLM, and then
     * guest and anonymous if we are allowed.
     */

    status = SMBServerConnect(*phConnection, targetUrl,
            netfsOptions, kSMBAuthTypeKerberos);
    if (status == make_nterror(NT_STATUS_LOGON_FAILURE) &&
        !(options & kSMBOptionNoPrompt)) {
        SMBPasswordPrompt(*phConnection);
    }

    if (!NT_SUCCESS(status)) {
        status = SMBServerConnect(*phConnection, targetUrl,
                netfsOptions, kSMBAuthTypeUser);
    }

    if (!(options & kSMBOptionRequireAuth)) {
        if (!NT_SUCCESS(status)) {
            status = SMBServerConnect(*phConnection, targetUrl,
                    netfsOptions, kSMBAuthTypeGuest);
        }

        if (!NT_SUCCESS(status)) {
            status = SMBServerConnect(*phConnection, targetUrl,
                    netfsOptions, kSMBAuthTypeAnonymous);
        }
    }

    /* OK, now we have a virtual circuit but no tree connection yet. */
    
    err = smb_share_connect((*phConnection)->context);
    if (err) {
        status = make_nterror(NT_STATUS_BAD_NETWORK_NAME);
        goto done;
    }
    
    status = NT_STATUS_SUCCESS;

done:
    if (netfsOptions) {
        CFRelease(netfsOptions);
    }

    if (!NT_SUCCESS(status) && *phConnection) {
        SMBReleaseServer(*phConnection);
        *phConnection = NULL;
    }

    if (targetUrl) {
        CFRelease(targetUrl);
    }

    return status;
}

NTSTATUS
SMBServerGetProperties(
        SMBHANDLE hConnection,
        SMBServerProperties * pProperties)
{
    NTSTATUS    status;
    void *      hContext;
    uint32_t    vc_flags;

    status = SMBServerContext(hConnection, &hContext);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    memset(pProperties, 0, sizeof(*pProperties));

    /* Since we don't ever create a SMBHANDLE without a valid SMB connection,
     * the vc_flags must always be set. If this assumption changes, we ought
     * to call the SMBIOC_GET_VC_FLAGS ioctl here.
     */
    vc_flags = ((struct smb_ctx *)hContext)->ct_vc_flags;

    if (vc_flags & SMBV_GUEST_ACCESS) {
        pProperties->authType = kSMBAuthTypeGuest;
    } else if (vc_flags & SMBV_ANONYMOUS_ACCESS) {
        pProperties->authType = kSMBAuthTypeAnonymous;
    } else if (vc_flags & SMBV_KERBEROS_ACCESS) {
        pProperties->authType = kSMBAuthTypeKerberos;
    } else {
        pProperties->authType = kSMBAuthTypeUser;
    }

    pProperties->dialect = kSMBDialectSMB;

    pProperties->capabilities = ((struct smb_ctx *)hContext)->ct_vc_caps;

    /* XXX We don't have a way of getting the following info: */
    pProperties->maxReadBytes = getpagesize();
    pProperties->maxWriteBytes = getpagesize();
    pProperties->maxTransactBytes = getpagesize();
    pProperties->tconFlags = 0;

    return NT_STATUS_SUCCESS;
}

NTSTATUS
SMBRetainServer(
    SMBHANDLE hConnection)
{
    OSAtomicIncrement32(&hConnection->refcount);
    return NT_STATUS_SUCCESS;
}

NTSTATUS
SMBReleaseServer(
    SMBHANDLE hConnection)
{
    refcount_t refcount;

    refcount = OSAtomicDecrement32(&hConnection->refcount);
    if (refcount == 0) {
        smb_ctx_done(hConnection->context);
        free(hConnection);
    }

    return NT_STATUS_SUCCESS;
}

/* vim: set sw=4 ts=4 tw=79 et: */
