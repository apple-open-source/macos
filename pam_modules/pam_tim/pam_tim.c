/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 * Portions Copyright (c) 1998-2000 Luke Howard. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

/*
 * PAM module for TIM authentication. Thanks to Marc
 * Majka for explaining the TIM API.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <netinfo/ni.h>

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define PAM_SM_SESSION
#define PAM_SM_PASSWORD

#include <security/pam_modules.h>
#include <security/_pam_macros.h>
#include <security/pam_mod_misc.h>

#include "tim.h"

#define PASSWORD_PROMPT	"Password:"
#define OLD_PASSWORD_PROMPT "Enter login(AFP) password:"
#define NEW_PASSWORD_PROMPT "New password:"
#define AGAIN_PASSWORD_PROMPT "Retype new password:"

static int      sendConversationMessage(struct pam_conv * aconv, const char *message, int style, struct options *options);
static int	tim2PamStatus(int status);
static int	secure_passwords();

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t * pamh, int flags, int argc,
		    const char **argv)
{
	int             status;
	const char     *user;
	const char     *password;
	struct options 	options;

	pam_std_option(&options, NULL, argc, argv);

	status = pam_get_user(pamh, &user, NULL);
	if (status != PAM_SUCCESS)
	{
		return status;
	}
	status = pam_get_pass(pamh, &password, PASSWORD_PROMPT, &options);
	if (status != PAM_SUCCESS)
	{
		return status;
	}
	status = timAuthenticate2WayRandom(user, password) == 0 ? PAM_SUCCESS : PAM_AUTH_ERR;

	return status;
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_EXTERN
int
pam_sm_acct_mgmt(pam_handle_t * pamh, int flags,
		 int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_EXTERN int
pam_sm_open_session(pam_handle_t * pamh, int flags,
		    int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t * pamh, int flags,
		     int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_EXTERN
int 
pam_sm_chauthtok(pam_handle_t * pamh, int flags,
		 int argc, const char **argv)
{
	char           *oldPassword, *newPassword;
	int             status, tries, canAbort;
	struct options	options;
	struct pam_conv *appconv;
	struct pam_message msg, *pmsg;
	struct pam_response *resp;
	const char     *cmiscptr = NULL;
	char           *user, *agent;
	int		minlen, maxTries, secure;

	canAbort = !(flags & PAM_CHANGE_EXPIRED_AUTHTOK);

	pam_std_option(&options, NULL, argc, argv);

	status = pam_get_item(pamh, PAM_CONV, (void **) &appconv);
	if (status != PAM_SUCCESS)
		return status;

	status = pam_get_item(pamh, PAM_USER, (void **) &user);
	if (status != PAM_SUCCESS)
		return status;

	status = pam_get_item(pamh, PAM_RUSER, (void **) &agent);
	if (status != PAM_SUCCESS)
		agent = user;

	if (agent == NULL)
		return PAM_USER_UNKNOWN;

	status = pam_get_item(pamh, PAM_OLDAUTHTOK, (void **) &oldPassword);
	if (status != PAM_SUCCESS)
		return status;

	secure = secure_passwords();
	maxTries = secure ? 3 : 5;
	minlen = secure ? 8 : 5;

	if (pam_test_option(&options, PAM_OPT_USE_FIRST_PASS, NULL) ||
	    pam_test_option(&options, PAM_OPT_TRY_FIRST_PASS, NULL))
	{
		if (pam_get_item(pamh, PAM_AUTHTOK, (void **) &newPassword) != PAM_SUCCESS)
			newPassword = NULL;

		if (pam_test_option(&options, PAM_OPT_USE_FIRST_PASS, NULL) && newPassword == NULL)
			return PAM_AUTHTOK_RECOVER_ERR;
	}
	if (flags & PAM_PRELIM_CHECK)
	{
		if (oldPassword != NULL &&
		   (pam_test_option(&options, PAM_OPT_USE_FIRST_PASS, NULL) ||
		    pam_test_option(&options, PAM_OPT_TRY_FIRST_PASS, NULL)))
		{
			/* Try the old password. */
			status = timAuthenticate2WayRandom(user, oldPassword) == 0 ? PAM_SUCCESS : PAM_AUTH_ERR;
			if (status != PAM_SUCCESS)
			{
				if (pam_test_option(&options, PAM_OPT_USE_FIRST_PASS, NULL))
					sendConversationMessage(appconv, "AFP password incorrect", PAM_ERROR_MSG, &options);
				else
					sendConversationMessage(appconv, "AFP password incorrect: try again", PAM_ERROR_MSG, &options);
			}
			else
			{
				return PAM_SUCCESS;
			}

			tries = 0;

			while (oldPassword == NULL && tries++ < maxTries)
			{
				pmsg = &msg;
				msg.msg_style = PAM_PROMPT_ECHO_OFF;
				msg.msg = OLD_PASSWORD_PROMPT;
				resp = NULL;

				status = appconv->conv(1, (struct pam_message **) &pmsg, &resp, appconv->appdata_ptr);

				if (status != PAM_SUCCESS)
				{
					return status;
				}
				oldPassword = resp->resp;
				free(resp);

				status = timAuthenticate2WayRandom(user, oldPassword) == 0 ? PAM_SUCCESS : PAM_AUTH_ERR;

				if (status != PAM_SUCCESS)
				{
					int             abortMe = 0;

					if (oldPassword != NULL && oldPassword [0] == '\0')
						abortMe = 1;

					_pam_overwrite(oldPassword);
					_pam_drop(oldPassword);

					if (canAbort && abortMe)
					{
						sendConversationMessage(appconv, "Password change aborted",
									PAM_ERROR_MSG, &options);
						return PAM_AUTHTOK_RECOVER_ERR;
					}
					else
					{
						sendConversationMessage(appconv, "AFP password incorrect: try again",
									PAM_ERROR_MSG, &options);
					}
				}
			}

			if (oldPassword == NULL)
			{
				status = PAM_MAXTRIES;
			}
			pam_set_item(pamh, PAM_OLDAUTHTOK, oldPassword);

			return status;
		}
	}			/* PAM_PRELIM_CHECK */

	status = PAM_ABORT;

	tries = 0;

	while (newPassword == NULL && tries++ < maxTries)
	{
		pmsg = &msg;
		msg.msg_style = PAM_PROMPT_ECHO_OFF;
		msg.msg = NEW_PASSWORD_PROMPT;
		resp = NULL;

		status = appconv->conv(1, &pmsg, &resp, appconv->appdata_ptr);
		if (status != PAM_SUCCESS)
		{
			return status;
		}
		newPassword = resp->resp;
		free(resp);

		if (newPassword[0] == '\0')
		{
			free(newPassword);
			newPassword = NULL;
		}

		if (newPassword != NULL)
		{
			if (oldPassword != NULL && !strcmp(oldPassword, newPassword))
			{
				cmiscptr = "Passwords must differ";
				newPassword = NULL;
			}
			else if (strlen(newPassword) < minlen)
			{
				cmiscptr = "Password too short";
				newPassword = NULL;
			}
			
		}
		else
		{
			return PAM_AUTHTOK_RECOVER_ERR;
		}

		if (cmiscptr == NULL)
		{
			/* get password again */
			char           *miscptr;

			pmsg = &msg;
			msg.msg_style = PAM_PROMPT_ECHO_OFF;
			msg.msg = AGAIN_PASSWORD_PROMPT;
			resp = NULL;

			status = appconv->conv(1, &pmsg, &resp, appconv->appdata_ptr);

			if (status != PAM_SUCCESS)
			{
				return status;
			}
			miscptr = resp->resp;
			free(resp);

			if (miscptr[0] == '\0')
			{
				free(miscptr);
				miscptr = NULL;
			}
			if (miscptr == NULL)
			{
				if (canAbort)
				{
					sendConversationMessage(appconv, "Password change aborted",
							    PAM_ERROR_MSG, &options);
					return PAM_AUTHTOK_RECOVER_ERR;
				}
			}
			else if (!strcmp(newPassword, miscptr))
			{
				miscptr = NULL;
				break;
			}
			sendConversationMessage(appconv, "You must enter the same password",
						  PAM_ERROR_MSG, &options);
			miscptr = NULL;
			newPassword = NULL;
		}
		else
		{
			sendConversationMessage(appconv, cmiscptr, PAM_ERROR_MSG, &options);
			cmiscptr = NULL;
			newPassword = NULL;
		}
	}

	if (cmiscptr != NULL || newPassword == NULL)
	{
		return PAM_MAXTRIES;
	}

	status = timSetPasswordForUser(agent, oldPassword, user, newPassword);
	if (status != TimStatusOK)
	{
		syslog(LOG_ERR, "timSetPasswordForUser: %s", timStatusString(status));
		return tim2PamStatus(status);
	}

	return PAM_SUCCESS;
}

static int
sendConversationMessage(struct pam_conv * aconv,
			  const char *message, int style, struct options *options)
{
	struct pam_message msg, *pmsg;
	struct pam_response *resp;

	if (pam_test_option(options, PAM_OPT_NO_WARN, NULL))
		return PAM_SUCCESS;

	pmsg = &msg;

	msg.msg_style = style;
	msg.msg = (char *) message;
	resp = NULL;

	return aconv->conv(1, &pmsg, &resp, aconv->appdata_ptr);
}

static int
tim2PamStatus(int timStatus)
{
	switch (timStatus) {
		case TimStatusOK:
			return PAM_SUCCESS;
		case TimStatusUserUnknown:
			return PAM_USER_UNKNOWN;
		case TimStatusUnrecoverablePassword:
			return PAM_AUTHTOK_RECOVER_ERR;
		case TimStatusAuthenticationFailed:
			return PAM_AUTH_ERR;
		case TimStatusInvalidPassword:
			return PAM_AUTHTOK_ERR;
		case TimStatusNotAuthorized:
			return PAM_PERM_DENIED;
		default:
			return PAM_SERVICE_ERR;
	}

	/* NOT REACHED */
	return PAM_SERVICE_ERR;
}

static int
secure_passwords()
{
	void           *d, *d1;
	int             status;
	ni_index        where;
	ni_id           dir;
	ni_namelist     nl;

	status = ni_open(NULL, ".", &d);
	while (status == NI_OK)
	{
		dir.nii_object = 0;
		status = ni_lookupprop(d, &dir, "security_options", &nl);
		if (status == NI_OK)
		{
			where = ni_namelist_match(nl, "secure_passwords");
			if (where != NI_INDEX_NULL)
			{
				ni_free(d);
				return 1;
			}
		}
		d1 = d;
		status = ni_open(d1, "..", &d);
		ni_free(d1);
	}

	return 0;
}

PAM_MODULE_ENTRY("pam_tim");
