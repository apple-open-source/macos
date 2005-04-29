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


/* Turn off debugging for live systems. */
/* #define DEBUG */

#define PAM_SM_AUTH
#define PAM_SM_SESSION
#define _PAM_EXTERN_FUNCTIONS
#include <pam/pam_modules.h>
#include <pam/pam_mod_misc.h>
#include <pam/_pam_macros.h>

#define AFP_DATA_TAG 		"pam_afpmount_authtok"
#define PASSWORD_PROMPT     "Password:"
#define AFP_END_OF_PASS 	"\x0A\x04"

#define MNTHOME_PATH		"/usr/bin/mnthome"
/* AFP_PASS_BUFFER = _PASSWORD_LEN + strlen(AFP_END_OF_PASS) +  1 */
#define AFP_PASS_BUFFER 	(_PASSWORD_LEN+3)


void cleanup(pam_handle_t *pamh, void *data, int pam_end_status)
{
	if (data != NULL) {
		bzero(data, strlen(data));
		free(data);
	}
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	int retval;
	const char *authtok = NULL;
	char *password = NULL;
	
	/* grab PAM password. */
	if ((retval = pam_get_pass(pamh, &authtok, PASSWORD_PROMPT, PAM_OPT_TRY_FIRST_PASS)) != PAM_SUCCESS) {
		D(("pam_get_pass [%s]", pam_strerror(pamh, retval)));
		return retval;
	}

	if (authtok == NULL) {
		return PAM_SERVICE_ERR;
	}
	password = strdup(authtok);

	if (password == NULL) {
		return PAM_BUF_ERR;		
	}

	/* Save password to pipe to mnthome */
	if ((retval = pam_set_data(pamh, AFP_DATA_TAG, password, cleanup)) != PAM_SUCCESS) {
		D(("pam_set_data [%s]", pam_strerror(pamh, retval)));
		return retval;
	}
	return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	int retval, master, slave, pid, plen;
	uid_t	 uid;
	const char *user = NULL;
	const char *authtok = NULL;
	struct stat	statbuf;
	struct passwd *pwd;
	char *password;
	char tmp[strlen(PASSWORD_PROMPT)+1];
	
	if ((retval = pam_get_user(pamh, &user, NULL)) != PAM_SUCCESS) {
			D(("pam_get_user [%s]", pam_strerror(pamh, retval)));
		return retval;
	}
	
	if (user == NULL) {
		return PAM_SERVICE_ERR;
	}
	
	if ((retval = pam_get_data(pamh, AFP_DATA_TAG, (const void **)&authtok)) != PAM_SUCCESS) {
			D(("pam_get_data [%s]", pam_strerror(pamh, retval)));
		return retval;
	}

	pwd = getpwnam(user);

	if (pwd == NULL) {
		return PAM_SERVICE_ERR;
	}
	uid = pwd->pw_uid;

	/* stat mnthome */
	if (stat(MNTHOME_PATH,&statbuf) < 0) {
				D(("stat of mnthome failed [%s]", strerror(errno)));
		return PAM_SERVICE_ERR;
	}

	if (openpty(&master, &slave, NULL, NULL, NULL) == -1) {
				D(("openpty failed [%s]", strerror(errno)));
		return PAM_SERVICE_ERR;
	}


	switch (pid = fork()) {
		
		case -1:
				/* fork failure */
				D(("fork failed [%s]", strerror(errno)));
				return PAM_SERVICE_ERR;
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
					D(("setuid(%d) failed [%s]",uid, strerror(errno)));
					_exit(1);
				}

				execl(MNTHOME_PATH, "mnthome", NULL);
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
					return PAM_SERVICE_ERR;
				}

				if (strncmp(PASSWORD_PROMPT, tmp, strlen(PASSWORD_PROMPT))!=0) {
					D(("getpass prompt failed: [%s]", tmp));
					tcflush(master, TCIOFLUSH);
					close(master);
					waitpid(pid, NULL, 0);
					return PAM_SERVICE_ERR;					
				}
				D(("Master read: '%s'",tmp));

				password = calloc(AFP_PASS_BUFFER, sizeof(char));
				strlcpy(password, authtok, _PASSWORD_LEN+1);
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
					if (tmp[0] == '\n' && read(master, tmp, strlen(PASSWORD_PROMPT)) != -1) {
						D(("read progress '%s'",tmp));
						retval = PAM_SUCCESS;
					}
					else {
						retval = PAM_SERVICE_ERR;
						D(("progress read failed"));											
					}
				}
				else {
					retval = PAM_SERVICE_ERR;
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
	return retval;
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

#ifdef PAM_STATIC
PAM_MODULE_ENTRY("pam_afpmount");
#endif

