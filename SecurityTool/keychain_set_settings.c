/*
 * Copyright (c) 2003-2009 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
 *
 * keychain_set_settings.c
 */

#include "keychain_set_settings.h"
#include "keychain_utilities.h"
#include "readline.h"
#include "security.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainPriv.h>

#define PW_BUF_SIZE 512				/* size of buffer to alloc for password */


static int
do_keychain_set_settings(const char *keychainName, SecKeychainSettings newKeychainSettings)
{
	SecKeychainRef keychain = NULL;
	OSStatus result;

	if (keychainName)
	{
		keychain = keychain_open(keychainName);
		if (!keychain)
		{
			result = 1;
			goto cleanup;
		}
	}
	result = SecKeychainSetSettings(keychain, &newKeychainSettings);
	if (result)
	{
		sec_error("SecKeychainSetSettings %s: %s", keychainName ? keychainName : "<NULL>", sec_errstr(result));
	}

cleanup:
	if (keychain)
		CFRelease(keychain);

	return result;
}


static int
do_keychain_set_password(const char *keychainName, const char* oldPassword, const char* newPassword)
{
	SecKeychainRef keychain = NULL;
	OSStatus result = 1;
	UInt32 oldLen = (oldPassword) ? strlen(oldPassword) : 0;
	UInt32 newLen = (newPassword) ? strlen(newPassword) : 0;
	char *oldPass = (oldPassword) ? (char*)oldPassword : NULL;
	char *newPass = (newPassword) ? (char*)newPassword : NULL;
	char *oldBuf = NULL;
	char *newBuf = NULL;

	if (keychainName)
	{
		keychain = keychain_open(keychainName);
		if (!keychain)
		{
			result = 1;
			goto cleanup;
		}
	}

	if (!oldPass) {
		/* prompt for old password */
		char *pBuf = getpass("Old Password: ");
		if (pBuf) {
			oldBuf = (char*) calloc(PW_BUF_SIZE, 1);
			oldLen = strlen(pBuf);
			memcpy(oldBuf, pBuf, oldLen);
			bzero(pBuf, oldLen);
			oldPass = oldBuf;
		}
	}

	if (!newPass) {
		/* prompt for new password */
		char *pBuf = getpass("New Password: ");
		if (pBuf) {
			newBuf = (char*) calloc(PW_BUF_SIZE, 1);
			newLen = strlen(pBuf);
			memcpy(newBuf, pBuf, newLen);
			bzero(pBuf, newLen);
		}
		/* confirm new password */
		pBuf = getpass("Retype New Password: ");
		if (pBuf) {
			UInt32 confirmLen = strlen(pBuf);
			if (confirmLen == newLen && newBuf &&
				!memcmp(pBuf, newBuf, newLen)) {
				newPass = newBuf;
			}
			bzero(pBuf, confirmLen);
		}
	}

	if (!oldPass || !newPass) {
		sec_error("try again");
		goto cleanup;
	}

	/* lock keychain first to remove existing credentials */
	(void)SecKeychainLock(keychain);

	/* change the password */
	result = SecKeychainChangePassword(keychain, oldLen, oldPass, newLen, newPass);
	if (result)
	{
		sec_error("error changing password for \"%s\": %s",
			keychainName ? keychainName : "<NULL>", sec_errstr(result));
	}

cleanup:
	/* if we allocated password buffers, zero and free them */
	if (oldBuf) {
		bzero(oldBuf, PW_BUF_SIZE);
		free(oldBuf);
	}
	if (newBuf) {
		bzero(newBuf, PW_BUF_SIZE);
		free(newBuf);
	}
	if (keychain) {
		CFRelease(keychain);
	}
	return result;
}


int
keychain_set_settings(int argc, char * const *argv)
{
	char *keychainName = NULL;
	int ch, result = 0;
    SecKeychainSettings newKeychainSettings =
		{ SEC_KEYCHAIN_SETTINGS_VERS1, FALSE, FALSE, INT_MAX };

    while ((ch = getopt(argc, argv, "hlt:u")) != -1)
	{
		switch  (ch)
		{
        case 'l':
            newKeychainSettings.lockOnSleep = TRUE;
			break;
		case 't':
            newKeychainSettings.lockInterval = atoi(optarg);
			break;
		case 'u':
            newKeychainSettings.useLockInterval = TRUE;
			break;
		case '?':
		default:
			result = 2; /* @@@ Return 2 triggers usage message. */
			goto cleanup;
		}
	}

	if (newKeychainSettings.lockInterval != INT_MAX) {
		// -t was specified, which implies -u
		newKeychainSettings.useLockInterval = TRUE;
	} else {
		// -t was unspecified, so revert to no timeout
		newKeychainSettings.useLockInterval = FALSE;
	}

	argc -= optind;
	argv += optind;

	if (argc == 1)
	{
		keychainName = argv[0];
		if (*keychainName == '\0')
		{
			result = 2;
			goto cleanup;
		}
	}
	else if (argc != 0)
	{
		result = 2;
		goto cleanup;
	}

	result = do_keychain_set_settings(keychainName, newKeychainSettings);

cleanup:

	return result;
}

int
keychain_set_password(int argc, char * const *argv)
{
	char *keychainName = NULL;
	char *oldPassword = NULL;
	char *newPassword = NULL;
	int ch, result = 0;

    while ((ch = getopt(argc, argv, "ho:p:")) != -1)
	{
		switch  (ch)
		{
        case 'o':
            oldPassword = optarg;
			break;
        case 'p':
            newPassword = optarg;
			break;
		case '?':
		default:
			result = 2; /* @@@ Return 2 triggers usage message. */
			goto cleanup;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 1)
	{
		keychainName = argv[0];
		if (*keychainName == '\0')
		{
			result = 2;
			goto cleanup;
		}
	}
	else if (argc != 0)
	{
		result = 2;
		goto cleanup;
	}

	result = do_keychain_set_password(keychainName, oldPassword, newPassword);

cleanup:

	return result;
}

