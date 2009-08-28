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

#include <stdio.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <OpenDirectory/OpenDirectory.h>
#include <OpenDirectory/OpenDirectoryPriv.h>
#include <DirectoryService/DirectoryService.h>

#define PAM_SM_AUTH 
#define PAM_SM_ACCOUNT 

#include <security/pam_modules.h>
#include <security/pam_appl.h>

static int
get_boolean_value(const void *p)
{
	int value;

	if (CFBooleanGetTypeID() == CFGetTypeID(p))
		return CFBooleanGetValue(p);
	else if (CFNumberGetTypeID() == CFGetTypeID(p) && CFNumberGetValue(p, kCFNumberIntType, &value))
		return 0 != value;
	else
		return 0;
}

static int
check_pwpolicy(ODRecordRef record)
{
	CFDictionaryRef policy = NULL;
	const void *isDisabled;
	const void *newPasswordRequired;
	int retval;
	
	if (NULL == (policy = ODRecordCopyPasswordPolicy(kCFAllocatorDefault, record, NULL)) ||
	    NULL == (isDisabled = CFDictionaryGetValue(policy, CFSTR("isDisabled"))) ||
	    !get_boolean_value(isDisabled))
		retval = PAM_SUCCESS;
	else
		retval = PAM_PERM_DENIED;
	if (NULL != policy &&
		NULL != (newPasswordRequired = CFDictionaryGetValue(policy, CFSTR("newPasswordRequired"))) &&
	    get_boolean_value(newPasswordRequired))
		retval = PAM_NEW_AUTHTOK_REQD;
	if (NULL != policy)
		CFRelease(policy);
	return retval;
}

static int
check_authauthority(ODRecordRef record)
{
	int retval = PAM_SUCCESS;

	CFArrayRef vals = ODRecordCopyValues(record, CFSTR(kDSNAttrAuthenticationAuthority), NULL);
	if (vals != NULL) {
		CFIndex count = CFArrayGetCount(vals);
		CFIndex i;
		for (i = 0; i < count; ++i) {
			const void *val = CFArrayGetValueAtIndex(vals, i);
			if (val == NULL || CFGetTypeID(val) != CFStringGetTypeID() || CFStringHasPrefix(val, CFSTR(kDSValueAuthAuthorityDisabledUser))) {
				retval = PAM_PERM_DENIED;
				break;
			}
		}
		CFRelease(vals);
	}

	return retval;
}

static int
check_shell(ODRecordRef record)
{
	int retval = PAM_SUCCESS;

	CFArrayRef vals = ODRecordCopyValues(record, CFSTR(kDS1AttrUserShell), NULL);
	if (vals != NULL) {
		CFIndex count = CFArrayGetCount(vals);
		CFIndex i;
		for (i = 0; i < count; ++i) {
			const void *val = CFArrayGetValueAtIndex(vals, i);
			if (val == NULL || CFGetTypeID(val) != CFStringGetTypeID() || CFStringCompare(val, CFSTR("/usr/bin/false"), 0) == kCFCompareEqualTo) {
				retval = PAM_PERM_DENIED;
				break;
			}
		}
		CFRelease(vals);
	}

	return retval;
}


PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	int retval;
	const char *user = NULL;

	/* get the username */
	retval = pam_get_user(pamh, &user, NULL);
	if (retval != PAM_SUCCESS) {
		return retval;
	}
	if (user == NULL || *user == '\0') {
		return PAM_PERM_DENIED;
	}

	/* check if the user's account is disabled */
	ODNodeRef cfNodeRef = ODNodeCreateWithNodeType(kCFAllocatorDefault, kODSessionDefault, eDSAuthenticationSearchNodeName, NULL);
	if (cfNodeRef != NULL) {
		CFStringRef cfUser = CFStringCreateWithCString(NULL, user, kCFStringEncodingUTF8);
		if (cfUser != NULL) {
			ODRecordRef cfRecord = ODNodeCopyRecord(cfNodeRef, CFSTR(kDSStdRecordTypeUsers), cfUser, NULL, NULL);
			if (cfRecord != NULL) {
				if (retval == PAM_SUCCESS) {
					retval = check_pwpolicy(cfRecord);
				}
				if (retval == PAM_SUCCESS) {
					retval = check_authauthority(cfRecord);
				}
				if (retval == PAM_SUCCESS && !openpam_get_option(pamh, "no_check_shell")) {
					retval = check_shell(cfRecord);
				}
				CFRelease(cfRecord);
			}
			CFRelease(cfUser);
		}
		CFRelease(cfNodeRef);
	}

	return retval;
}


PAM_EXTERN int
pam_sm_authenticate(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	static const char password_prompt[] = "Password:";
	int retval = PAM_SUCCESS;
	const char *user;
	const char *password = NULL;
	CFErrorRef odErr;

	if ((retval = pam_get_user(pamh, &user, NULL)) != PAM_SUCCESS)
		return retval;
	if (PAM_SUCCESS != (retval = pam_get_item(pamh, PAM_AUTHTOK, (void *)&password)))
		return retval;
	if (NULL == password) {
		if (PAM_SUCCESS != (retval = pam_get_authtok(pamh, PAM_AUTHTOK, &password, password_prompt)))
			return PAM_AUTH_ERR;
	}
	if ((password[0] == '\0') && ((NULL == openpam_get_option(pamh, "nullok")) || (flags & PAM_DISALLOW_NULL_AUTHTOK)))
		return PAM_AUTH_ERR;

	/* verify the user's password */
	retval = PAM_USER_UNKNOWN;
	ODNodeRef cfNodeRef = ODNodeCreateWithNodeType(kCFAllocatorDefault, kODSessionDefault, eDSAuthenticationSearchNodeName, NULL);
	if (cfNodeRef != NULL) {
		CFStringRef cfUser = CFStringCreateWithCString(NULL, user, kCFStringEncodingUTF8);
		CFStringRef cfPassword = CFStringCreateWithCString(NULL, password, kCFStringEncodingUTF8);
		if ((cfUser != NULL) && (cfPassword != NULL)) {
			ODRecordRef cfRecord = ODNodeCopyRecord(cfNodeRef, CFSTR(kDSStdRecordTypeUsers), cfUser, NULL, NULL);
			if (cfRecord != NULL) {
				if (!ODRecordVerifyPassword(cfRecord, cfPassword, &odErr)) {
					switch (CFErrorGetCode(odErr)) {
						case kODErrorCredentialsAccountNotFound:
							retval = PAM_USER_UNKNOWN;
							break;
						case kODErrorCredentialsAccountDisabled:
						case kODErrorCredentialsAccountInactive:
							retval = PAM_PERM_DENIED;
							break;
						case kODErrorCredentialsPasswordExpired:
						case kODErrorCredentialsPasswordChangeRequired:
							retval = PAM_SUCCESS;
							break;
						default:
							retval = PAM_AUTH_ERR;
							break;
					}
				}
				else {
					retval = PAM_SUCCESS;
				}
				CFRelease(cfRecord);
				if (odErr) {
					CFRelease(odErr);
				}
			}
			else {
				retval = PAM_AUTH_ERR;
			}
			CFRelease(cfUser);
			CFRelease(cfPassword);
		}
		CFRelease(cfNodeRef);
	}

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
	const char *user;
	const char *new_password = NULL;
	const char *old_password = NULL;
	CFErrorRef odErr;

	if (flags & PAM_PRELIM_CHECK)
		return PAM_SUCCESS;

	if ((retval = pam_get_user(pamh, &user, NULL)) != PAM_SUCCESS)
		return retval;
	if (PAM_SUCCESS != (retval = pam_get_item(pamh, PAM_OLDAUTHTOK, (void *)&old_password)))
		return retval;
	if (NULL == old_password &&
	    PAM_SUCCESS != (retval = pam_get_authtok(pamh, PAM_OLDAUTHTOK, &old_password, old_password_prompt)))
		return retval;
	if (PAM_SUCCESS != (retval = pam_get_item(pamh, PAM_AUTHTOK, (void *)&new_password)))
		return retval;
	if (NULL == new_password &&
	    PAM_SUCCESS != (retval = pam_get_authtok(pamh, PAM_AUTHTOK, &new_password, new_password_prompt)))
		return retval;

	/* reset the user's password */
	retval = PAM_SYSTEM_ERR;
	ODNodeRef cfNodeRef = ODNodeCreateWithNodeType(kCFAllocatorDefault, kODSessionDefault, eDSAuthenticationSearchNodeName, NULL);
	if (cfNodeRef != NULL) {
		CFStringRef cfUser = CFStringCreateWithCString(NULL, user, kCFStringEncodingUTF8);
		CFStringRef cfOldPassword = CFStringCreateWithCString(NULL, old_password, kCFStringEncodingUTF8);
		CFStringRef cfNewPassword = CFStringCreateWithCString(NULL, new_password, kCFStringEncodingUTF8);
		if ((cfUser != NULL) && (cfOldPassword != NULL) && (cfNewPassword != NULL)) {
			ODRecordRef cfRecord = ODNodeCopyRecord(cfNodeRef, CFSTR(kDSStdRecordTypeUsers), cfUser, NULL, NULL);
			if (cfRecord != NULL) {
				if (!ODRecordChangePassword(cfRecord, cfOldPassword, cfNewPassword, &odErr)) {
					switch (CFErrorGetCode(odErr)) {
						case kODErrorCredentialsInvalid:
						case kODErrorCredentialsPasswordQualityFailed:
							retval = PAM_AUTHTOK_ERR;
							break;
						case kODErrorCredentialsNotAuthorized:
						case kODErrorCredentialsAccountDisabled:
						case kODErrorCredentialsAccountInactive:
							retval = PAM_PERM_DENIED;
							break;
						case kODErrorCredentialsPasswordUnrecoverable:
							retval = PAM_AUTHTOK_RECOVERY_ERR;
							break;
						default:
							retval = PAM_ABORT;
							break;
					}
				}
				else {
					retval = PAM_SUCCESS;
				}
				CFRelease(cfRecord);
			}
			else {
				retval = PAM_SERVICE_ERR;
			}
			CFRelease(cfUser);
			CFRelease(cfOldPassword);
			CFRelease(cfNewPassword);
		}
		CFRelease(cfNodeRef);
	}
	return retval;
}
