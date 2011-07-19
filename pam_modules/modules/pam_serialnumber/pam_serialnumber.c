/*-
 * Portions Copyright (c) 2006-2009 Apple Computer, Inc.
 * All rights reserved.
 *
 * This module is based on a pam_module with the following copyright:
 * Copyright 2001 Mark R V Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define	PAM_SM_AUTH

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <security/pam_modules.h>
#include <security/pam_appl.h>

#include <CoreFoundation/CoreFoundation.h>
#include <AppleSystemInfo/AppleSystemInfo.h>
#include <sys/mount.h>

#include "Common.h"

#define PASSWORD_PROMPT	"Password:"
#define UN_LEN	4
#define FE_MAX	11

#define AUTH_FAILURE_LOG_PATH  "/var/run/pam_serialnumber_failure_log"
#define AUTH_FAILURE_MAX_TRIES 3

int auth_failure_log_failure(unsigned);
int auth_failure_max_reached(unsigned,unsigned*);

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	const char *user, *max_attempt_str;
	char *password = NULL;
	char serialnumber[128];
	unsigned current_attempt = 0, max_attempts = AUTH_FAILURE_MAX_TRIES;

	/* When we have the serverinstall option, verify that this is a ServerInstall */
	if (NULL != openpam_get_option(pamh, "serverinstall") && false == IsServerInstall()) {
		openpam_log(PAM_LOG_DEBUG, "The serverinstall option requires that we be a server install.");
		return PAM_IGNORE;
	}

	/* Verify that the user is root */
	if (pam_get_user(pamh, &user, NULL)) {
		openpam_log(PAM_LOG_DEBUG, "Unable to obtain the username.");
		return PAM_AUTH_ERR;
	}

	if (0 != strncasecmp(user, "root", UN_LEN)) {
		openpam_log(PAM_LOG_DEBUG, "Only root can authenticate using the serial number.");
		return PAM_AUTH_ERR;
	}

	/* Get the serial number */
	CFStringRef cfSerialNumber = ASI_CopyFormattedSerialNumber();
	if (!CFStringGetCString(cfSerialNumber, serialnumber, sizeof(serialnumber), kCFStringEncodingMacRoman)) {
		openpam_log(PAM_LOG_ERROR, "Authentication error.  The serial number could not be read.");
		CFRelease(cfSerialNumber);
		return PAM_AUTHINFO_UNAVAIL;
	}
	CFRelease(cfSerialNumber);

	/* Cut the serialnumber to 11 characters if we're in legacy mode. */
	if (NULL != openpam_get_option(pamh, "legacy")) {
		serialnumber[FE_MAX] = '\0';
	}

	if (NULL != (max_attempt_str = (openpam_get_option(pamh, "max_auth_failures")))) {
		max_attempts = (int)strtoul(max_attempt_str, (char **)NULL, 10);
		if (1 > max_attempts) {
			openpam_log(PAM_LOG_DEBUG, "Unable to determine \"max_auth_failures\" argument.");
			max_attempts = AUTH_FAILURE_MAX_TRIES;
		}
	}
	openpam_log(PAM_LOG_DEBUG, "Maximum authentication failures set to %d.", max_attempts);

	if (PAM_SUCCESS != auth_failure_max_reached(max_attempts, &current_attempt)) {
		openpam_log(PAM_LOG_ERROR, "Authentication prohibited: Too many authentication failures.");
		return PAM_PERM_DENIED;
	}

	/* Verify the 'password' */
	if (PAM_SUCCESS != pam_get_authtok(pamh, PAM_AUTHTOK, (const char **)&password, PASSWORD_PROMPT)) {
		openpam_log(PAM_LOG_ERROR, "Authentication error.  Unable to get the password.");
		return PAM_AUTH_ERR;
	}
	if (0 != strcasecmp(password, serialnumber) && FE_MAX <= strlen(serialnumber)) {
		auth_failure_log_failure(++current_attempt);
		return PAM_AUTH_ERR;
	}

	/* Just to be on the safe side... */
	memset(password, 0, strlen(password));

	return PAM_SUCCESS;
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

int
auth_failure_max_reached(unsigned num_attempts_max, unsigned *num_attempts_out)
{
	FILE *fp = NULL;
	int retval = PAM_SERVICE_ERR;

	*num_attempts_out = 0;

	fp = fopen(AUTH_FAILURE_LOG_PATH, "r");
	if (NULL == fp) {
		openpam_log(PAM_LOG_DEBUG, "Error opening authorization log.");
	} else {
		if (1 != fscanf(fp, "%d", num_attempts_out)) {
			openpam_log(PAM_LOG_DEBUG, "Error parsing authorization log.");
		}
		fclose(fp);
	}

	if (*num_attempts_out < num_attempts_max) {
		openpam_log(PAM_LOG_DEBUG, "Attempted %d of %d authorizations.", *num_attempts_out, num_attempts_max);
		retval = PAM_SUCCESS;
	} else {
		openpam_log(PAM_LOG_DEBUG, "Maximum failure attempts of %d reached.", num_attempts_max);
		retval = PAM_PERM_DENIED;
	}

	return retval;
}

int
auth_failure_log_failure(unsigned auth_cur)
{
	FILE *fp = NULL;

	fp = fopen(AUTH_FAILURE_LOG_PATH,"w");
	if (NULL == fp) {
		openpam_log(PAM_LOG_DEBUG, "Error opening authorization log: %s.", strerror(errno));
	} else {
		if (1 < fprintf(fp, "%d", auth_cur)) {
			openpam_log(PAM_LOG_DEBUG, "Error writing authorization log.");
			goto cleanup;
		}
		openpam_log(PAM_LOG_DEBUG, "Logging failure number %d.", auth_cur);
		fclose(fp);
	}

cleanup:
	return PAM_SUCCESS;
}
