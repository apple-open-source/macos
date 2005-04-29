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
 * The purpose of this module is to provide an interface between
 * the PAM account and session management SPIs and the Apple
 * security framework.
 ******************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>
#include <Security/AuthSession.h>

#define PAM_SM_AUTH 

#include <pam/pam_modules.h>
#include <pam/_pam_macros.h>
#include <pam/pam_mod_misc.h>

#define PASSWORD_PROMPT          "Password:"

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	AuthorizationRef authorizationRef = NULL;
	AuthorizationFlags authzflags;
	AuthorizationEnvironment env;
	OSStatus        err;
	AuthorizationRights rights;
	AuthorizationItem rightItems[1], envItems[2];
	int             options = 0;
	int             status;
	int             i;

	for(i = 0; (i < argc) && argv[i]; i++)
		pam_std_option(&options, argv[i]);
	options |= PAM_OPT_TRY_FIRST_PASS;

	rights.count = 0;
	rights.items = NULL;

	envItems[0].name = kAuthorizationEnvironmentUsername;
	status = pam_get_item(pamh, PAM_USER, (void *)&envItems[0].value);
	if (status != PAM_SUCCESS) {
		return status;
	}
	if( envItems[0].value == NULL ) {
		status = pam_get_user(pamh, (void *)&(envItems[0].value), NULL);
		if( status != PAM_SUCCESS )
			return status;
		if( envItems[0].value == NULL )
			return PAM_AUTH_ERR;
	} 
	envItems[0].valueLength = strlen(envItems[0].value);
	envItems[0].flags = 0;

	envItems[1].name = kAuthorizationEnvironmentPassword;
	status = pam_get_pass(pamh, (void *)&envItems[1].value,PASSWORD_PROMPT,
		options);
	if (status != PAM_SUCCESS) {
		return status;
	}
	if( envItems[1].value == NULL )
		envItems[1].valueLength = 0;
	else
		envItems[1].valueLength = strlen(envItems[1].value);
	envItems[1].flags = 0;

	env.count = 2;
	env.items = envItems;

	authzflags = kAuthorizationFlagDefaults;
	err = AuthorizationCreate(&rights, &env, authzflags, &authorizationRef);
	if (err != errAuthorizationSuccess) {
		return PAM_AUTH_ERR;
	}
	rightItems[0].name = "system.login.tty";
	rightItems[0].value = NULL;
	rightItems[0].valueLength = 0;
	rightItems[0].flags = 0;

	rights.count = 1;
	rights.items = rightItems;

	authzflags = kAuthorizationFlagExtendRights | kAuthorizationFlagPreAuthorize;
	err = AuthorizationCopyRights(authorizationRef, &rights, &env, authzflags, NULL);
	if (err != errAuthorizationSuccess) {
		return PAM_AUTH_ERR;
	}
	AuthorizationFree(authorizationRef, 0);

	return PAM_SUCCESS;
}

/* pam_sm_setcred:
 * The purpose of this function is to set the user's "credentials".
 * This function is invoked only *after* the user has been authenticated.
 * AFAIK, PAM's ideas of "credentials" is taking any information the
 * authentication token has and making that available to the calling
 * program through pam_set_data().  This can also be an opportunity
 * to make additional information available to the authentication token.
 *
 * Since our SecurityServer token doesn't know anything more about
 * the user or environment than the calling program does, this essentially
 * checks the authentication token to ensure that it is still valid.
 * We could also give some additional information to the SecurityServer,
 * but apparently the SecurityServer doesn't actually use or allow the
 * retrieval of that information at this time.
 */
PAM_EXTERN int
pam_sm_setcred(pam_handle_t * pamh, int flags,
		    int argc, const char **argv)
{
	return PAM_SUCCESS;
}
