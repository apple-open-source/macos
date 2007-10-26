/*-
 * Portions Copyright (c) Apple Computer, Inc.
 * All rights reserved. 
 *
 * This module is based on a pam_module with the following copyright:
 * Copyright 2001 Mark R V Murray
 * All rights reserved.
 *
 * Portions Copyright 2006 Apple Computer, Inc.
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

#include <pam/pam_modules.h>
#include <pam/_pam_macros.h>
#include <pam/pam_mod_misc.h>
#include <CoreFoundation/CoreFoundation.h>
#include <AppleSystemInfo/ASI_SerialNumber.h>
#include <sys/mount.h>
#include <syslog.h>

#define PASSWORD_PROMPT	"Password:"
#define UN_LEN	4
#define FE_MAX	8
#define FE_BUF	FE_MAX+1

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	int i;
	int options;
	const char *user;
	const char *password;
	size_t pass_len;
	CFStringRef cfs;
	char *snbuffer = NULL;
	size_t snbuffsz;
	char firsteight[FE_BUF];
	struct statfs fs;
	
	/* Mind the options */
	if (NULL != argv) {
		for(i = 0; (i < argc) && argv[i]; i++) {
			pam_std_option(&options, argv[i]);
		}
	}
	
	options |= PAM_OPT_TRY_FIRST_PASS;

	/* Verify that this is a read-only mount*/
	if (0 != statfs("/", &fs)) {
		return PAM_IGNORE;
	}

	if (~fs.f_flags & MNT_RDONLY) {
		return PAM_IGNORE;
	}

	/* Verify that the user is root */
	if (pam_get_user(pamh, &user, NULL)) {
		return PAM_AUTH_ERR;
	}	

	if (0 != strncasecmp(user, "root", UN_LEN)) {
		return PAM_AUTH_ERR;
	}
	
	/* Get the first 8 characters of the serial number */
	cfs = ASI_CopyFormattedSerialNumber();
	if (0 < CFStringGetLength(cfs)) {
		snbuffsz = CFStringGetLength(cfs)+1;
		if ((2 > snbuffsz) && (128 < snbuffsz)) {
			return PAM_AUTH_ERR;
		}
		snbuffer = malloc(snbuffsz);
		if (NULL == snbuffer) {
			return PAM_AUTH_ERR;
		}
		CFStringGetCString(cfs, snbuffer, snbuffsz, kCFStringEncodingMacRoman);
		strlcpy(firsteight, snbuffer, FE_BUF);
		free(snbuffer);
	} else {
		strlcpy(firsteight, "12345678", FE_BUF);
	}

	/* Verify the 'password' */
	if (pam_get_pass(pamh, &password, PASSWORD_PROMPT, options)) {
		return PAM_AUTH_ERR;
	}
	
	pass_len = strlen(password);
	if (strlen(firsteight) != pass_len) {
		return PAM_AUTH_ERR;
	}
	
	if (0 != strncasecmp(password, firsteight, pass_len)) {
		return PAM_AUTH_ERR;
	}
	
	/* Just to be on the safe side... */
	bzero((void *)password, pass_len);

	return PAM_SUCCESS;
}

#ifdef PAM_STATIC
PAM_MODULE_ENTRY("pam_serverinstallonly");
#endif
