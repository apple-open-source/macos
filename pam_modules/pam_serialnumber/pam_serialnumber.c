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

#include <security/pam_modules.h>
#include <security/pam_appl.h>
#include <CoreFoundation/CoreFoundation.h>
#include <AppleSystemInfo/ASI_SerialNumber.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <syslog.h>

#define PASSWORD_PROMPT	"Password:"
#define UN_LEN	4
#define FE_MAX	8

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	const char *user;
	char *password = NULL;
	char serialnumber[128];
	struct stat buf;
	
	/* When we have the serverinstall option, verify that this is a Server Install */
	if (NULL != openpam_get_option(pamh, "serverinstall")) {
		if ((0 != stat("/System/Installation/Packages/ServerEssentials.pkg", &buf)) && (0 != stat("/System/Installation/Packages/ASRInstall.pkg", &buf))) {
			return PAM_IGNORE;
		}
		if (0 != stat("/System/Library/CoreServices/ServerVersion.plist", &buf)) {
			return PAM_IGNORE;
		}
	}

	/* Verify that the user is root */
	if (pam_get_user(pamh, &user, NULL)) {
		return PAM_AUTH_ERR;
	}	

	if (0 != strncasecmp(user, "root", UN_LEN)) {
		return PAM_AUTH_ERR;
	}

	/* Get the serial number */
	CFStringRef cfSerialNumber = ASI_CopyFormattedSerialNumber();
	CFMutableStringRef cfMutableSerialNumber = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, cfSerialNumber);
	CFRelease(cfSerialNumber);
	if (NULL == cfMutableSerialNumber) {
		return PAM_AUTHINFO_UNAVAIL;
	}
	CFStringUppercase(cfMutableSerialNumber, CFLocaleGetSystem());
	if (!CFStringGetCString(cfMutableSerialNumber, serialnumber, sizeof(serialnumber), kCFStringEncodingMacRoman)) {
		syslog(LOG_ERR, "Authentication error.  The serial number could not be read.");
		CFRelease(cfMutableSerialNumber);
		return PAM_AUTHINFO_UNAVAIL;
	}
	CFRelease(cfMutableSerialNumber);

	/* Cut the serialnumber to 8 characters if we're in legacy mode. */
	if (NULL != openpam_get_option(pamh, "legacy")) {
		serialnumber[FE_MAX] = '\0';
	}

	/* Verify the 'password' */
	if (PAM_SUCCESS != pam_get_item(pamh, PAM_AUTHTOK, (const void **)&password)) {
		syslog(LOG_ERR, "Authentication error.  Unable to get retrieve the password from the PAM context.");
		return PAM_AUTH_ERR;
	}
	if (NULL == password && PAM_SUCCESS != pam_get_authtok(pamh, PAM_AUTHTOK, (const char **)&password, PASSWORD_PROMPT)) {
		syslog(LOG_ERR, "Authentication error.  Unable to get the password from the user.");
		return PAM_AUTH_ERR;
	}
	if (0 != strcmp(password, serialnumber) && FE_MAX <= strlen(serialnumber)) {
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
