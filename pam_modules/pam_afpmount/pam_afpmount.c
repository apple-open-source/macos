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
 * The purpose of this module is to automount afp home directories
 * during interactive(password) ssh login.
 *
 * This code is reasonably dependent on the consistency of 
 * /usr/bin/mnthome's output. 
 ******************************************************************/

#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>
#include <Security/AuthSession.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/errno.h>
#include <unistd.h>
#include <pwd.h>
#include <util.h>
#include <string.h>
#include <stdarg.h>


/* Turn off debugging for live systems. */
#define D(...) //

#define PAM_SM_AUTH
#define _PAM_EXTERN_FUNCTIONS
#include <pam/pam_modules.h>
#include <pam/pam_mod_misc.h>

#define PASSWORD_PROMPT     "Password:"
#define AFP_END_OF_PASS 	"\x0A\x04"

#define MNTHOME_PATH		"/usr/bin/mnthome"
/* AFP_PASS_BUFFER = _PASSWORD_LEN + strlen(AFP_END_OF_PASS) +  1 */
#define AFP_PASS_BUFFER 	(_PASSWORD_LEN+3)

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	AuthorizationRef authorizationRef = NULL;
	AuthorizationFlags authzflags;
	AuthorizationEnvironment env;
	OSStatus        err;
	AuthorizationRights rights;
	AuthorizationItem envItems[2];
	int             options = 0;
	int             status;
	int             i;
	struct passwd *pwd;
	struct stat	statbuf;
	int retval, master, slave, pid, plen;
	uid_t	 uid;
	char tmp[strlen(PASSWORD_PROMPT)+1];
	char *password = NULL;

	for(i = 0; (i < argc) && argv[i]; i++)
		pam_std_option(&options, argv[i]);
	options |= PAM_OPT_TRY_FIRST_PASS;

	rights.count = 0;
	rights.items = NULL;

	envItems[0].name = kAuthorizationEnvironmentUsername;
	status = pam_get_item(pamh, PAM_USER, (void *)&envItems[0].value);
	if (status != PAM_SUCCESS) {
		return PAM_IGNORE;
	}
	if( envItems[0].value == NULL ) {
		status = pam_get_user(pamh, (void *)&(envItems[0].value), NULL);
		if( status != PAM_SUCCESS )
			return PAM_IGNORE;
		if( envItems[0].value == NULL )
			return PAM_IGNORE;
	} 
	envItems[0].valueLength = strlen(envItems[0].value);
	envItems[0].flags = 0;

	envItems[1].name = kAuthorizationEnvironmentPassword;
	status = pam_get_pass(pamh, (void *)&envItems[1].value,PASSWORD_PROMPT,
		options);
	if (status != PAM_SUCCESS) {
		return PAM_IGNORE;
	}
	if( envItems[1].value == NULL ) {
		envItems[1].valueLength = 0;
		/* no password can't mount, just return */
		return PAM_IGNORE;
	}
	else
		envItems[1].valueLength = strlen(envItems[1].value);
	envItems[1].flags = 0;

	env.count = 2;
	env.items = envItems;

	authzflags = kAuthorizationFlagDefaults;
	err = AuthorizationCreate(&rights, &env, authzflags, &authorizationRef);
	if (err != errAuthorizationSuccess) {
		return PAM_IGNORE;
	}
	AuthorizationFree(authorizationRef, 0);

	pwd = getpwnam(envItems[0].value);

	if (pwd == NULL) {
		return PAM_IGNORE;
	}
	uid = pwd->pw_uid;

	/* stat mnthome */
	if (stat(MNTHOME_PATH, &statbuf) < 0) {
		D(("stat of mnthome failed [%s]", strerror(errno)));
		return PAM_IGNORE;
	}

	if (openpty(&master, &slave, NULL, NULL, NULL) == -1) {
		D(("openpty failed [%s]", strerror(errno)));
		return PAM_IGNORE;
	}

	switch (pid = fork()) {
		case -1:
				/* fork failure */
				D(("fork failed [%s]", strerror(errno)));
				return PAM_IGNORE;
				break;
		case  0:
				/* child */
				(void) close(master);
				if (login_tty(slave) == -1) {
					D(("login_tty failed [%s]", strerror(errno)));
					_exit(1);
				}

				/* change to appropriate user...*/
				if (setuid(uid) == -1) {
					D(("setuid(%d) failed [%s]", uid, strerror(errno)));
					_exit(1);
				}

				(void)execl(MNTHOME_PATH, "mnthome", NULL);
				_exit(1);
				break;
		default: 
				/* parent */
				(void) close(slave);
				
				/* block on master until getpass() prompt arrives */
				if (read(master, tmp, strlen(PASSWORD_PROMPT)) == -1) {
					D(("read failed '%s' [%s]", tmp, strerror(errno)));
					tcflush(master, TCIOFLUSH);
					close(master);
					/* Wait for zombie mnthomes */
					waitpid(pid, NULL, WNOHANG);
					return PAM_IGNORE;
				}

				if (strncmp(PASSWORD_PROMPT, tmp, strlen(PASSWORD_PROMPT))!=0) {
					D(("getpass prompt failed: [%s]", tmp));
					tcflush(master, TCIOFLUSH);
					close(master);
					waitpid(pid, NULL, 0);
					return PAM_IGNORE;
				}
				D(("Master read: '%s'",tmp));

				password = calloc(AFP_PASS_BUFFER, sizeof(char));
				strlcpy(password, envItems[1].value, _PASSWORD_LEN+1);
				/* add end of password marker */
				strlcat(password, AFP_END_OF_PASS, AFP_PASS_BUFFER);
				plen = strlen(password);

				if (write(master, password, plen) != plen) {
					D(("write failed [%s]", strerror(errno)));
					retval = PAM_SERVICE_ERR;
				}
				else {
					retval = PAM_SUCCESS;
				}

				/* give child a chance to catch up */
				sleep(1);
				bzero(tmp, sizeof(tmp));

				/* grab newline thrown by getpass() */
				if (retval == PAM_SUCCESS && read(master, tmp, strlen(PASSWORD_PROMPT)) != -1) {
					D(("read newline '%s'",tmp));
					/* check newline was received, grab progress text */
					//if (tmp[0] == '\n' && read(master, tmp, strlen(PASSWORD_PROMPT)) != -1) {
					if (read(master, tmp, strlen(PASSWORD_PROMPT)) != -1) {
						D(("read progress '%s'",tmp));
						retval = PAM_IGNORE;
					}
					else {
						retval = PAM_IGNORE;
						D(("progress read failed"));											
					}
				} else {
					retval = PAM_IGNORE;
					D(("read newline failed"));					
				}

				tcflush(master, TCIOFLUSH);
				close(master);

				/* clean up password */
				bzero(password, AFP_PASS_BUFFER);
				free(password);
				
				/* wait safely,  for child to be done. */
				waitpid(pid, NULL, 0);
	}
	return PAM_IGNORE;
}

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_IGNORE;
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	return PAM_IGNORE;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_IGNORE;
}

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_IGNORE;
}

#ifdef PAM_STATIC
PAM_MODULE_ENTRY("pam_afpmount");
#endif

