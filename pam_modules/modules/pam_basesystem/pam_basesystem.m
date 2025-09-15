/*
 * Copyright (c) 2025 Apple Computer, Inc. All rights reserved.
 * Portions Copyright (c) 2001 PADL Software Pty Ltd. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Portions Copyright (c) 2025 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import <CoreFoundation/CoreFoundation.h>
#import <unistd.h>
#import <pwd.h>
#import <spawn.h>
#import <os/log.h>
#import <sysexits.h>
#import "Logging.h"
#import <reboot2.h>

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT

#import <security/pam_modules.h>
#import <security/pam_appl.h>

#ifdef PAM_USE_OS_LOG
PAM_DEFINE_LOG(aks)
#define PAM_LOG PAM_LOG_aks()
#endif

extern char **environ;

#define LIBEXEC_SSHD_FVUNLOCK "/usr/libexec/sshd-fvunlock"
#define PIVOT_MOUNT_PATH "/System/Volumes/macOS"

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
    _LOG_DEBUG("pam_sm_authenticate");

    static const char password_prompt[] = "Password:";
    int retval = PAM_AUTH_ERR;

    const char *user = NULL;
    const char *password = NULL;
    char *resp = NULL;
    
    int rd = -1;
    int wr = -1;
    pid_t pid = 0;
    posix_spawn_file_actions_t file_actions = NULL;
    posix_spawnattr_t spawn_attr = NULL;

    /* get information about user to authenticate for */
    if ((retval = pam_get_user(pamh, &user, NULL)) != PAM_SUCCESS || !user) {
        _LOG_ERROR("basesystem: unable to obtain the username.");
        if (retval == PAM_SUCCESS && !user) {
            retval = PAM_AUTHINFO_UNAVAIL;
        }
        goto cleanup;
    }
    _LOG_DEBUG("basesystem: obtained username %s", user);

    if ((retval = pam_get_authtok(pamh, PAM_AUTHTOK, &password, password_prompt)) != PAM_SUCCESS) {
        _LOG_ERROR("basesystem: unable to obtain authtok.");
        goto cleanup;
    }
    _LOG_DEBUG("basesystem: got password");

    const char *const child_argv[] = {
        LIBEXEC_SSHD_FVUNLOCK,
        "--no-reboot",
        user,
        NULL
    };
    const char *const envp[] = {
        NULL
    };

    int rc = 0;
    if ((rc = posix_spawn_file_actions_init(&file_actions)) != 0) {
        _LOG_ERROR("posix_spawn_file_actions_init() failed: %d", rc);
        retval = PAM_SYSTEM_ERR;
        goto cleanup;
    }

    if ((rc = posix_spawnattr_init(&spawn_attr)) != 0) {
        _LOG_ERROR("posix_spawnattr_init() failed; %d", rc);
        retval = PAM_SYSTEM_ERR;
        goto cleanup;
    }

    int pipefd[2];
    if ((rc = pipe(pipefd)) != 0) {
        _LOG_ERROR("pipe() failed: %d", rc);
        retval = PAM_SYSTEM_ERR;
        goto cleanup;
    }
    
    rd = pipefd[0];
    wr = pipefd[1];

    if ((rc = posix_spawn_file_actions_adddup2(&file_actions, rd, STDIN_FILENO)) != 0) {
        _LOG_ERROR("posix_spawn_file_actions_adddup2() failed: %d", rc);
        retval = PAM_SYSTEM_ERR;
        goto cleanup;
    }
    
    if ((rc = posix_spawn_file_actions_addclose(&file_actions, wr))) {
        _LOG_ERROR("posix_spawn_file_actions_addclose() for write fd failed: %d", rc);
        retval = PAM_SYSTEM_ERR;
        goto cleanup;
    }

    if ((rc = posix_spawn_file_actions_addclose(&file_actions, rd))) {
        _LOG_ERROR("posix_spawn_file_actions_addclose(rd) for read fd failed: %d", rc);
        retval = PAM_SYSTEM_ERR;
        goto cleanup;
    }

    rc = posix_spawn(&pid, LIBEXEC_SSHD_FVUNLOCK, &file_actions, &spawn_attr, (char *const *)child_argv, (char *const *)envp);
    if (rc != 0) {
        _LOG_ERROR("posix_spawn() failed: %d", rc);
        retval = PAM_SYSTEM_ERR;
        goto cleanup;
    }
    close(rd);
    rd = -1;
    
    // write password over pipe, with null terminator
    size_t datalen = strlen(password)+1;
    size_t written = write(wr, password, datalen);
    if (written != datalen) {
        _LOG_ERROR("short write");
        retval = PAM_SYSTEM_ERR;
        goto cleanup;
    }
    
    close(wr);
    wr = -1;
    
    int status = 0;
    if ((rc = waitpid(pid, &status, 0)) == -1) {
        _LOG_ERROR("waitpid() failed: %d (status: %d)", rc, status);
        retval = PAM_SYSTEM_ERR;
        goto cleanup;
    }
    
    // check status to see if we exited normally.
    if (!WIFEXITED(status)) {
        // child exited in an abnormal way
        _LOG_ERROR(LIBEXEC_SSHD_FVUNLOCK " exited abnormally (status: %d)", status);
        retval = PAM_SYSTEM_ERR;
        goto cleanup;
    }
    
    int exitcode = WEXITSTATUS(status);
    if (exitcode != 0) {
        _LOG_ERROR(LIBEXEC_SSHD_FVUNLOCK " exited normally with code: %d", exitcode);
        switch (exitcode) {
            case EX_NOPERM:
                _LOG_ERROR(LIBEXEC_SSHD_FVUNLOCK " failed due to incorrect password");
                retval = PAM_AUTH_ERR;
                break;
            case EX_TEMPFAIL:
                _LOG_ERROR(LIBEXEC_SSHD_FVUNLOCK " failed because %s is temporarily unavailable", user);
                pam_prompt(pamh, PAM_ERROR_MSG, &resp, "Account %s is temporarily unavailable.", user);
                retval = PAM_MAXTRIES;
                break;
            case EX_SOFTWARE:
            default:
                _LOG_INFO(LIBEXEC_SSHD_FVUNLOCK " failed due to a system error");
                retval = PAM_SYSTEM_ERR;
                break;
        }
        goto cleanup;
    }
    
    // Prompt for reboot
    if (pam_prompt(pamh, PAM_TEXT_INFO, &resp, "System successfully unlocked.\nYou may now use SSH to authenticate normally.") != PAM_SUCCESS) {
        _LOG_ERROR("failed to message for userspace reboot, continuing anyway");
    }
    
    int pivotcode = 0;
    if ((pivotcode = reboot3(RB3_PIVOTROOT, PIVOT_MOUNT_PATH)) != 0) {
        _LOG_ERROR("userspace reboot failed: %d", pivotcode);
        retval = PAM_SYSTEM_ERR;
        goto cleanup;
    }

    // may not ever be reached since we userspace reobot.
    retval = PAM_SUCCESS;
cleanup:
    if (spawn_attr) {
        posix_spawnattr_destroy(&spawn_attr);
    }
    if (file_actions) {
        posix_spawn_file_actions_destroy(&file_actions);
    }
    
    if (rd != -1) {
        close(rd);
    }
    if (wr != -1) {
        close(wr);
    }
    
    if (resp != NULL) {
        free(resp);
    }
    
    if (retval != PAM_SUCCESS) {
        _LOG_ERROR("basesystem: pam_sm_authenticate for %s returned %d", user, retval);
    }
    return retval;
}


PAM_EXTERN int 
pam_sm_setcred(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
    _LOG_DEBUG("pam_sm_setcred");
	return PAM_SUCCESS;
}


PAM_EXTERN int 
pam_sm_acct_mgmt(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
    _LOG_DEBUG("pam_sm_acct_mgmt");
    return PAM_SUCCESS;
}
