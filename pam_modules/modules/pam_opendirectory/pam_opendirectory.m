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
 * The purpose of this module is to provide a basic password
 * authentication module for Mac OS X.
 ******************************************************************/

#include <pwd.h>
#include <stdio.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <membership.h>
#include <membershipPriv.h>
#include <ConfigurationProfiles/CPBootstrapToken.h>
#include <CoreFoundation/CoreFoundation.h>
#include <OpenDirectory/OpenDirectory.h>
#include <OpenDirectory/OpenDirectoryPriv.h>
#include <DirectoryService/DirectoryService.h>

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT

#include <security/pam_modules.h>
#include <security/pam_appl.h>

#include "Common.h"

#define PM_DISPLAY_NAME "OpenDirectory"
#define PAM_OD_PW_EXP "ODPasswordExpire"

// <rdar://problem/12503092> OD password verification API always leads to user existence timing attacks
// Define our own implementation of AbsoluteToMicroseconds rather than using CoreService ==> CarbonCore
// Nanoseconds AbsoluteToNanoseconds(AbsoluteTime inTime);
//
// Based on Technical Q&A QA1398 - Mach Absolute Time Units
// https://developer.apple.com/library/mac/qa/qa1398/_index.html

uint64_t AbsoluteToMicroseconds(uint64_t t) {
	static double f = 0.0;

	if (f == 0.0) {
		mach_timebase_info_data_t tbinfo;
		(void) mach_timebase_info(&tbinfo);
		f = tbinfo.numer / (1000.0 * tbinfo.denom);
	}

	return t * f;
}


PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	int retval = PAM_PERM_DENIED;
	const char *user = NULL;
	char *homedir = NULL;
	ODRecordRef cfRecord = NULL;
	struct passwd *pwd = NULL;
	struct passwd pwdbuf;
	char pwbuffer[2 * PATH_MAX];
	const char *ttl_str = NULL;
	int ttl = 30 * 60;

	/* get the username */
	retval = pam_get_user(pamh, &user, NULL);
	if (PAM_SUCCESS != retval) {
		goto cleanup;
	}
	if (user == NULL || *user == '\0') {
		retval = PAM_PERM_DENIED;
		goto cleanup;
	}

	/* refresh the membership */
	if (0 != getpwnam_r(user, &pwdbuf, pwbuffer, sizeof(pwbuffer), &pwd) || NULL == pwd) {
		openpam_log(PAM_LOG_ERROR, "%s - Unable to get pwd record.", PM_DISPLAY_NAME);
		retval = PAM_USER_UNKNOWN;
		goto cleanup;
	}
	if (NULL != (ttl_str = openpam_get_option(pamh, "refresh"))) {
		ttl = strtol(ttl_str, NULL, 10) * 60;
	}
	mbr_set_identifier_ttl(ID_TYPE_UID, &pwd->pw_uid, sizeof(pwd->pw_uid), ttl);
	openpam_log(PAM_LOG_DEBUG, "%s - Membership cache TTL set to %d.", PM_DISPLAY_NAME, ttl);

	/* Get user record from OD */
	retval = od_record_create_cstring(pamh, &cfRecord, (const char*)user);
	if (PAM_SUCCESS != retval) {
		openpam_log(PAM_LOG_ERROR, "%s - Unable to get user record: %d.", PM_DISPLAY_NAME, retval);
		goto cleanup;
	}

	/* check if authentication returned password expired */
	if (pam_getenv(pamh, PAM_OD_PW_EXP) != NULL) {
		openpam_log(PAM_LOG_DEBUG, "%s - Password expired.", PM_DISPLAY_NAME);
		retval = PAM_NEW_AUTHTOK_REQD;
		goto cleanup;
	}

	/* check user password policy */
	retval = od_record_check_pwpolicy(cfRecord);
	if (PAM_SUCCESS != retval) {
		goto cleanup;
	}

	/* check user authentication authority */
	retval = od_record_check_authauthority(cfRecord);
	if (PAM_SUCCESS != retval) {
		goto cleanup;
	}

	/* check user home directory */
	if (!openpam_get_option(pamh, "no_check_home")) {
		retval = od_record_check_homedir(cfRecord);
		if (PAM_SUCCESS != retval) {
			goto cleanup;
		}
	}

	/* check user shell */
	if (!openpam_get_option(pamh, "no_check_shell")) {
		retval = od_record_check_shell(cfRecord);
		if (PAM_SUCCESS != retval) {
			goto cleanup;
		}
	}

cleanup:
	CFReleaseSafe(cfRecord);
	free(homedir);
	pam_unsetenv(pamh, PAM_OD_PW_EXP);


	return retval;
}

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	static const char password_prompt[] = "Password:";
	int retval = PAM_SUCCESS;
	const char *user = NULL;
	const char *password = NULL;
	CFStringRef cfAuthAuthority = NULL;
	CFStringRef cfPassword = NULL;
	CFErrorRef odErr = NULL;
	ODRecordRef cfRecord = NULL;
	uint64_t mach_start_time = mach_absolute_time();
	bool should_sleep = 0;

	if (PAM_SUCCESS != (retval = pam_get_user(pamh, &user, NULL))) {
		openpam_log(PAM_LOG_DEBUG, "%s - Unable to obtain the username.", PM_DISPLAY_NAME);
		goto cleanup;
	}
	if (PAM_SUCCESS != (retval = pam_get_authtok(pamh, PAM_AUTHTOK, &password, password_prompt))) {
		openpam_log(PAM_LOG_DEBUG, "%s - Error obtaining the authtok.", PM_DISPLAY_NAME);
		retval = PAM_AUTH_ERR;
		goto cleanup;
	}
	if ((password[0] == '\0') && ((NULL == openpam_get_option(pamh, "nullok")) || (flags & PAM_DISALLOW_NULL_AUTHTOK))) {
		openpam_log(PAM_LOG_DEBUG, "%s - NULL passwords are not allowed.", PM_DISPLAY_NAME);
		retval = PAM_AUTH_ERR;
		goto cleanup;
	}

	/* From this point, all error paths should be constant time */
	should_sleep = 1;

	/* Get user record from OD */
	retval = od_record_create_cstring(pamh, &cfRecord, (const char*)user);
	if (PAM_SUCCESS != retval) {
		openpam_log(PAM_LOG_ERROR, "%s - Unable to get user record.", PM_DISPLAY_NAME);
		goto cleanup;
	}

	if (NULL == cfRecord) {
		openpam_log(PAM_LOG_ERROR, "%s - User record NULL.", PM_DISPLAY_NAME);
		retval = PAM_USER_UNKNOWN;
		goto cleanup;
	}

	/* <rdar://problem/48780154> Adopt new OD SPI and entitlement to use the bootstrap token prior to user authentication */
	if (&CP_SupportsBootstrapToken != NULL) {
		retval = od_record_attribute_create_cfstring(cfRecord, (CFStringRef) kODAttributeTypeAuthenticationAuthority, &cfAuthAuthority);
		NSArray<NSString *> *authAttributes = nil;
		if (retval == PAM_SUCCESS)
			authAttributes = [(NSString *)cfAuthAuthority componentsSeparatedByString:@";"];
		if ([authAttributes containsObject:@(kDSTagAuthAuthorityLocalCachedUser)] &&
			CP_SupportsBootstrapToken()) {
			/* Get token + authenticate. */
			NSString *token = CP_GetBootstrapTokenWithOptions(@{ @"NetworkTimeout" : @20 }, NULL);
			if (token != NULL && token.length > 0) {
				if (ODRecordSetNodeCredentialsWithBootstrapToken(cfRecord, (CFStringRef) token, &odErr)) {
					openpam_log(PAM_LOG_NOTICE, "%s - Authenticated with bootstrap token.", PM_DISPLAY_NAME);
				} else {
					openpam_log(PAM_LOG_ERROR, "%s - Failed to set bootstrap token: %s.", PM_DISPLAY_NAME, [(NSError *)odErr description].UTF8String);
					CFReleaseNull(odErr);
				}
			}
		}
	}

	/* Verify the user's password */
	cfPassword = CFStringCreateWithCString(kCFAllocatorDefault, password, kCFStringEncodingUTF8);
	retval = PAM_USER_UNKNOWN;
	if (!ODRecordVerifyPassword(cfRecord, cfPassword, &odErr)) {
		switch (CFErrorGetCode(odErr)) {
			case kODErrorCredentialsAccountNotFound:
				retval = PAM_USER_UNKNOWN;
				openpam_log(PAM_LOG_DEBUG, "%s - Account not found or invalid.", PM_DISPLAY_NAME);
				break;
			case kODErrorCredentialsAccountDisabled:
			case kODErrorCredentialsAccountInactive:
				openpam_log(PAM_LOG_DEBUG, "%s - The account is disabled or inactive.", PM_DISPLAY_NAME);
				retval = PAM_PERM_DENIED;
				break;
			case kODErrorCredentialsPasswordExpired:
			case kODErrorCredentialsPasswordChangeRequired:
				openpam_log(PAM_LOG_DEBUG, "%s - The authtok is expired or requires updating.", PM_DISPLAY_NAME);
				pam_setenv(pamh, PAM_OD_PW_EXP, "yes", 1);
				retval = PAM_SUCCESS;
				break;
			case kODErrorCredentialsInvalid:
				openpam_log(PAM_LOG_DEBUG, "%s - The authtok is incorrect.", PM_DISPLAY_NAME);
				retval = PAM_AUTH_ERR;
				break;
			case kODErrorCredentialsAccountTemporarilyLocked :
				openpam_log(PAM_LOG_DEBUG, "%s - Account temporarily locked after incorrect password attempt(s).", PM_DISPLAY_NAME);
				retval = PAM_APPLE_ACCT_TEMP_LOCK;
				break;
			case kODErrorCredentialsAccountLocked :
				openpam_log(PAM_LOG_DEBUG, "%s - Account locked after too many incorrect password attempts.", PM_DISPLAY_NAME);
				retval = PAM_APPLE_ACCT_LOCKED;
				break;
			default:
				openpam_log(PAM_LOG_DEBUG, "%s  Unexpected error code from ODRecordVerifyPassword(): %ld.",
					    PM_DISPLAY_NAME, CFErrorGetCode(odErr));
				retval = PAM_AUTH_ERR;
				break;
		}
	} else {
		retval = PAM_SUCCESS;
		should_sleep = 0;
	}

cleanup:
	if (should_sleep) {
		// <rdar://problem/12503092> OD password verification API always leads to user existence timing attacks
		uint64_t elapsed      = mach_absolute_time() - mach_start_time;
		uint64_t microseconds = AbsoluteToMicroseconds(elapsed);

		const uint64_t response_delay = 2000000;    // 2000000 us == 2s
		openpam_log(PAM_LOG_DEBUG, "%s - auth %lld µs", PM_DISPLAY_NAME, microseconds);
		if (microseconds < response_delay)
			usleep(response_delay - microseconds);

		elapsed      = mach_absolute_time() - mach_start_time;
		microseconds = AbsoluteToMicroseconds(elapsed);
		openpam_log(PAM_LOG_DEBUG, "%s - auth %lld µs (blinding)", PM_DISPLAY_NAME, microseconds);
	}

	CFReleaseSafe(cfAuthAuthority);
	CFReleaseSafe(cfRecord);
	CFReleaseSafe(cfPassword);
	CFReleaseSafe(odErr);

	return retval;
}


PAM_EXTERN int
pam_sm_setcred(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}


PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	static const char old_password_prompt[] = "Old Password:";
	static const char new_password_prompt[] = "New Password:";
	int retval = PAM_SUCCESS;
	const char *user = NULL;
	const char *new_password = NULL;
	const char *old_password = NULL;
	CFErrorRef odErr = NULL;
	ODRecordRef cfRecord = NULL;
	CFStringRef cfOldPassword = NULL;
	CFStringRef cfNewPassword = NULL;

	if (flags & PAM_PRELIM_CHECK) {
		retval = PAM_SUCCESS;
		goto cleanup;
	}

	if ((retval = pam_get_user(pamh, &user, NULL)) != PAM_SUCCESS) {
		openpam_log(PAM_LOG_DEBUG, "%s - Error obtaining the username.", PM_DISPLAY_NAME);
		goto cleanup;
	}

	if (PAM_SUCCESS != (retval = pam_get_authtok(pamh, PAM_OLDAUTHTOK, &old_password, old_password_prompt))) {
		openpam_log(PAM_LOG_DEBUG, "%s - Error obtaining the old password.", PM_DISPLAY_NAME);
		goto cleanup;
	}
	if (PAM_SUCCESS != (retval = pam_get_authtok(pamh, PAM_AUTHTOK, &new_password, new_password_prompt))) {
		openpam_log(PAM_LOG_DEBUG, "%s - Error obtaining the new password.", PM_DISPLAY_NAME);
		goto cleanup;
	}

	/* Get user record from OD */
	retval = od_record_create_cstring(pamh, &cfRecord, (const char*)user);
	if (PAM_SUCCESS != retval) {
		openpam_log(PAM_LOG_ERROR, "%s - Unable to get user record.", PM_DISPLAY_NAME);
		goto cleanup;
	}

	/* reset the user's password */
	cfOldPassword = CFStringCreateWithCString(kCFAllocatorDefault, old_password, kCFStringEncodingUTF8);
	cfNewPassword = CFStringCreateWithCString(kCFAllocatorDefault, new_password, kCFStringEncodingUTF8);

	retval = PAM_SYSTEM_ERR;
	if (!ODRecordChangePassword(cfRecord, cfOldPassword, cfNewPassword, &odErr)) {
		switch (CFErrorGetCode(odErr)) {
			case kODErrorCredentialsInvalid:
			case kODErrorCredentialsPasswordQualityFailed:
				openpam_log(PAM_LOG_DEBUG, "%s - The authtok is invaild or of low quality.", PM_DISPLAY_NAME);
				retval = PAM_AUTHTOK_ERR;
				break;
			case kODErrorCredentialsNotAuthorized:
			case kODErrorCredentialsAccountDisabled:
			case kODErrorCredentialsAccountInactive:
				openpam_log(PAM_LOG_DEBUG, "%s - The account not authorized, disabled or inactive.", PM_DISPLAY_NAME);
				retval = PAM_PERM_DENIED;
				break;
			case kODErrorCredentialsPasswordUnrecoverable:
				openpam_log(PAM_LOG_DEBUG, "%s - The authtok us unrecoverable.", PM_DISPLAY_NAME);
				retval = PAM_AUTHTOK_RECOVERY_ERR;
				break;
			case kODErrorCredentialsAccountTemporarilyLocked :
				openpam_log(PAM_LOG_DEBUG, "%s - Account temporarily locked after incorrect password attempt(s).", PM_DISPLAY_NAME);
				retval = PAM_APPLE_ACCT_TEMP_LOCK;
				break;
			case kODErrorCredentialsAccountLocked :
				openpam_log(PAM_LOG_DEBUG, "%s - Account locked after too many incorrect password attempts.", PM_DISPLAY_NAME);
				retval = PAM_APPLE_ACCT_LOCKED;
				break;
			default:
				openpam_log(PAM_LOG_DEBUG, "%s - There was an unexpected error while changing the password.", PM_DISPLAY_NAME);
				retval = PAM_ABORT;
				break;
		}
	} else {
		retval = PAM_SUCCESS;
	}

cleanup:
	CFReleaseSafe(odErr);
	CFReleaseSafe(cfRecord);
	CFReleaseSafe(cfOldPassword);
	CFReleaseSafe(cfNewPassword);

	return retval;
}
