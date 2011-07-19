/*
 * Copyright (C) 2002-2009 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms of pam_uwtmp, with
 * or without modification, are permitted provided that the following
 * conditions are met:
 * 
 * 1. Redistributions of source code must retain any existing copyright
 * notice, and this entire permission notice in its entirety,
 * including the disclaimer of warranties.
 * 
 * 2. Redistributions in binary form must reproduce all prior and current
 * copyright notices, this list of conditions, and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 * 
 * 3. The name of any author may not be used to endorse or promote
 * products derived from this software without their specific prior
 * written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE. 
 */

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <utmpx.h>
#include <string.h>
#include <stdlib.h>

#define PAM_SM_SESSION
#include <security/pam_modules.h>
#include <security/pam_appl.h>
#include <security/openpam.h>

#define DATA_NAME "pam_uwtmp.utmpx"

PAM_EXTERN int
populate_struct(pam_handle_t *pamh, struct utmpx *u)
{
	int status;
	char *tty;
	char *user;
	char *remhost;

	if (NULL == u)
		return PAM_SYSTEM_ERR;

	if (PAM_SUCCESS != (status = pam_get_item(pamh, PAM_USER, (const void **)&user))) {
		openpam_log(PAM_LOG_DEBUG, "Unable to obtain the username.");
		return status;
	}
	if (NULL != user)
		strlcpy(u->ut_user, user, sizeof(u->ut_user));

	if (PAM_SUCCESS != (status = pam_get_item(pamh, PAM_TTY, (const void **)&tty))) {
		openpam_log(PAM_LOG_DEBUG, "Unable to obtain the tty.");
		return status;
	}
	if (NULL == tty) {
		openpam_log(PAM_LOG_DEBUG, "The tty is NULL.");
		return PAM_IGNORE;
	} else
		strlcpy(u->ut_line, tty, sizeof(u->ut_line));

	if (PAM_SUCCESS != (status = pam_get_item(pamh, PAM_RHOST, (const void **)&remhost))) {
		openpam_log(PAM_LOG_DEBUG, "Unable to obtain the rhost.");
		return status;
	}
	if (NULL != remhost)
		strlcpy(u->ut_host, remhost, sizeof(u->ut_host));

	return status;
}

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	int status;
	struct utmpx *u = NULL;

	if( (u = calloc(1, sizeof(*u))) == NULL ) {
		openpam_log(PAM_LOG_ERROR, "Memory allocation error.");
		return PAM_BUF_ERR;
	}

	if (PAM_SUCCESS != (status = populate_struct(pamh, u))) {
		pam_set_data(pamh, DATA_NAME, NULL, NULL);
		goto err;
	}

	u->ut_pid = getpid();
	u->ut_type = UTMPX_AUTOFILL_MASK | USER_PROCESS;
	gettimeofday(&u->ut_tv, NULL);

	if (PAM_SUCCESS != (status = pam_set_data(pamh, DATA_NAME, u, openpam_free_data))) {
		openpam_log(PAM_LOG_ERROR, "There was an error setting data in the context.");
		goto err;
	}

	if( pututxline(u) == NULL ) {
		openpam_log(PAM_LOG_ERROR, "Unable to write the utmp record.");
		status = PAM_SYSTEM_ERR;
		goto err;
	}

	return PAM_SUCCESS;

err:
	free(u);
	return status;
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	int status;
	struct utmpx *u = NULL;
	int free_u = 0;

	status = pam_get_data(pamh, DATA_NAME, (const void **)&u);
	if( status != PAM_SUCCESS ) {
		openpam_log(PAM_LOG_DEBUG, "Unable to obtain the tmp record from the context.");
	}

	if (NULL == u) {
		if( (u = calloc(1, sizeof(*u))) == NULL ) {
			openpam_log(PAM_LOG_ERROR, "Memory allocation error.");
			return PAM_BUF_ERR;
		}
		free_u = 1;

		if (PAM_SUCCESS != (status = populate_struct(pamh, u)))
			goto fin;
	}

	u->ut_pid = getpid();
	u->ut_type = UTMPX_AUTOFILL_MASK | DEAD_PROCESS;
	gettimeofday(&u->ut_tv, NULL);

	if( pututxline(u) == NULL ) {
		openpam_log(PAM_LOG_ERROR, "Unable to write the utmp record.");
		status = PAM_SYSTEM_ERR;
		goto fin;
	}

	status = PAM_SUCCESS;

fin:
	if (1 == free_u)
		free(u);
	return status;
}
