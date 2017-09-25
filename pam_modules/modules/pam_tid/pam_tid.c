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
 * The purpose of this module is to provide a Touch ID
 * based authentication module for Mac OS X.
 ******************************************************************/

#include <CoreFoundation/CoreFoundation.h>
#include <coreauthd_spi.h>
#include <pwd.h>
#include <LocalAuthentication/LAPrivateDefines.h>

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT

#include <security/pam_modules.h>
#include <security/pam_appl.h>
#include <Security/Authorization.h>
#include <vproc_priv.h>
#include "Logging.h"
#include "Common.h"

PAM_DEFINE_LOG(touchid)
#define PAM_LOG PAM_LOG_touchid()

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
    os_log_debug(PAM_LOG, "pam_tid: pam_sm_authenticate");

    int retval = PAM_AUTH_ERR;
    CFTypeRef context = NULL;
    CFErrorRef error = NULL;
    CFMutableDictionaryRef options = NULL;
    CFNumberRef key = NULL;
    CFNumberRef value = NULL;
	CFNumberRef key2 = NULL;
	CFNumberRef value2 = NULL;
	AuthorizationRef authorizationRef = NULL;

    int tmp;

    const char *user = NULL;
    struct passwd *pwd = NULL;
    struct passwd pwdbuf;

    /* determine the required bufsize for getpwnam_r */
    int bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufsize == -1) {
        bufsize = 2 * PATH_MAX;
    }
    
    /* get information about user to authenticate for */
    char *buffer = malloc(bufsize);
    if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS || !user ||
        getpwnam_r(user, &pwdbuf, buffer, bufsize, &pwd) != 0 || !pwd) {
        os_log_error(PAM_LOG, "unable to obtain the username.");
        retval = PAM_AUTHINFO_UNAVAIL;
        goto cleanup;
    }

	// check if we are running under Aqua session
	char *manager;
	if (vproc_swap_string(NULL, VPROC_GSK_MGR_NAME, NULL, &manager) != NULL) {
		os_log_error(PAM_LOG, "unable to determine session.");
		retval = PAM_AUTH_ERR;
		goto cleanup;
	}
	bool runningInAquaSession = manager ? !strcmp(manager, VPROCMGR_SESSION_AQUA) : FALSE;
	free(manager);
	if (!runningInAquaSession) {
		os_log_debug(PAM_LOG, "UI not available.");
		retval = PAM_AUTH_ERR;
		goto cleanup;
	}

	// check if user is eligible to use Touch ID. If not, fail.
    /* prepare the options dictionary, aka rewrite @{ @(LAOptionNotInteractive) : @YES } without Foundation */
    tmp = kLAOptionNotInteractive;
    key = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &tmp);

    tmp = 1;
    value = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &tmp);

	tmp = kLAOptionUserId;
	key2 = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &tmp);

	tmp = pwd->pw_uid;
	value2 = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &tmp);

	if (! (key && value && key2 && value2)) {
		os_log_error(PAM_LOG, "unable to create data structures.");
		retval = PAM_AUTH_ERR;
		goto cleanup;
	}

    options = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(options, key, value);
	CFDictionarySetValue(options, key2, value2);

	context = LACreateNewContextWithACMContext(NULL, &error);
	if (!context) {
		os_log_error(PAM_LOG, "unable to create context.");
		retval = PAM_AUTH_ERR;
		goto cleanup;
	}

    /* evaluate policy */
    if (!LAEvaluatePolicy(context, kLAPolicyDeviceOwnerAuthenticationWithBiometrics, options, &error)) {
		// error is intended as failure means Touch ID is not usable which is in fact not an error but the state we need to handle
		if (CFErrorGetCode(error) != kLAErrorNotInteractive) {
			os_log_debug(PAM_LOG, "policy evaluation failed: %ld", CFErrorGetCode(error));
			retval = PAM_AUTH_ERR;
			goto cleanup;
		}
    }

	OSStatus status = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authorizationRef);
	if (status == errAuthorizationSuccess) {
		AuthorizationItem myItems = {"com.apple.security.sudo", 0, NULL, 0};
		AuthorizationRights myRights = {1, &myItems};
		AuthorizationRights *authorizedRights = NULL;
		AuthorizationFlags flags = kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights;
		status = AuthorizationCopyRights(authorizationRef, &myRights, kAuthorizationEmptyEnvironment, flags, &authorizedRights);
		os_log_debug(PAM_LOG, "Authorization result: %d", (int)status);
		if (authorizedRights)
			AuthorizationFreeItemSet(authorizedRights);
		AuthorizationFree(authorizationRef, kAuthorizationFlagDefaults);
	}

    /* we passed the Touch ID authentication successfully */
	if (status == errAuthorizationSuccess) {
		retval = PAM_SUCCESS;
	}

cleanup:
	CFReleaseSafe(context);
	CFReleaseSafe(key);
	CFReleaseSafe(value);
	CFReleaseSafe(key2);
	CFReleaseSafe(value2);
	CFReleaseSafe(options);
	CFReleaseSafe(error);
    free(buffer);
	os_log_debug(PAM_LOG, "pam_tid: pam_sm_authenticate returned %d", retval);
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
