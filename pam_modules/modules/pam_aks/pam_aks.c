/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 * Portions Copyright (c) 2001 PADL Software Pty Ltd. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Portions Copyright (c) 2000 Apple Computer, Inc.  All Rights
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

/******************************************************************
 * The purpose of this module is to provide an aks based 
 * authentication module for Mac OS X.
 ******************************************************************/

#include <CoreFoundation/CoreFoundation.h>
#include <spawn.h>
#include <unistd.h>
#include <pwd.h>
#include <os/log.h>
#include <libproc.h>
#include "Logging.h"

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT

#include <security/pam_modules.h>
#include <security/pam_appl.h>


PAM_DEFINE_LOG(AKS)
#define AKS_LOG PAM_LOG_AKS()

extern char **environ;

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
    os_log_debug(AKS_LOG, "pam_sm_authenticate");

    int retval = PAM_AUTH_ERR;

    const char *user = NULL;
    struct passwd *pwd = NULL;
    struct passwd pwdbuf;
    char buffer[2 * PATH_MAX];

    /* get information about user to authenticate for */
    if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS || !user ||
        getpwnam_r(user, &pwdbuf, buffer, sizeof(buffer), &pwd) != 0 || !pwd) {
        os_log_error(AKS_LOG, "unable to obtain the username.");
        retval = PAM_AUTHINFO_UNAVAIL;
        goto cleanup;
    }

	char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
	int status = proc_pidpath(getpid(), pathbuf, sizeof(pathbuf));
	if (status <= 0) {
		os_log_error(AKS_LOG, "unable to get the path.");
		goto cleanup;
	}

	snprintf(buffer, sizeof(buffer), "%d", pwd->pw_uid);
	pid_t spawn_pid;
	char *args[] = {pathbuf, buffer, NULL};
	status = posix_spawn(&spawn_pid, "/System/Library/Frameworks/LocalAuthentication.framework/Support/lastatus", NULL, NULL, args, environ);
	if (status == 0) {
		os_log_debug(AKS_LOG, "helper pid %d", spawn_pid);
		if (waitpid(spawn_pid, &status, 0) != -1) {
			os_log_debug(AKS_LOG, "helper return value %d", status);
			if (status == 0)
				retval = PAM_SUCCESS;
		} else {
			os_log_debug(AKS_LOG, "wait failed %d", status);
		}
	} else {
		os_log_debug(AKS_LOG, "launch failed %d", status);
	}

cleanup:
    os_log_debug(AKS_LOG, "pam_sm_authenticate returned %d", retval);
    return retval;
}


PAM_EXTERN int 
pam_sm_setcred(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}


PAM_EXTERN int 
pam_sm_acct_mgmt(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
    return PAM_SUCCESS;
}
