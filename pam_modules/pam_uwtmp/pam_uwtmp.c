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

#define DATA_NAME "pam_uwtmp.utmpx"

static void
free_data(pam_handle_t *pamh, void *data, int error_status)
{
	free(data);
}

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	int status;
	char *tty;
	char *user;
	char *remhost;
	struct utmpx *u;

	status = pam_get_item(pamh, PAM_USER, (void *)&user);
	if( status != PAM_SUCCESS )
		return status;

	status = pam_get_item(pamh, PAM_TTY, (void *)&tty);
	if( status != PAM_SUCCESS )
		return status;

	if( (u = calloc(1, sizeof(*u))) == NULL )
		return PAM_BUF_ERR;

	status = pam_set_data(pamh, DATA_NAME, u, free_data);
	if( status != PAM_SUCCESS ) {
		free(u);
		return status;
	}

	status = pam_get_item(pamh, PAM_RHOST, (void *)&remhost);
	if( (status == PAM_SUCCESS) && (remhost != NULL) )
		strncpy(u->ut_host, remhost, sizeof(u->ut_host));
	
	strncpy(u->ut_line, tty, sizeof(u->ut_line));
	strncpy(u->ut_user, user, sizeof(u->ut_user));
	u->ut_pid = getpid();
	u->ut_type = UTMPX_AUTOFILL_MASK | USER_PROCESS;
	gettimeofday(&u->ut_tv, NULL);

	if( pututxline(u) == NULL )
		return PAM_SYSTEM_ERR;

	return PAM_SUCCESS;
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	int status;
	struct utmpx *u;

	status = pam_get_data(pamh, DATA_NAME, (const void **)&u);
	if( status != PAM_SUCCESS )
		return status;

	u->ut_type = UTMPX_AUTOFILL_MASK | DEAD_PROCESS;
	gettimeofday(&u->ut_tv, NULL);
	if( pututxline(u) == NULL )
		return PAM_SYSTEM_ERR;

	return PAM_SUCCESS;
}
