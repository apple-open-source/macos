/*
 * Copyright (c) 2003-2004,2012,2014 Apple Inc. All Rights Reserved.
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
 * keychain_lock.c
 */

#include "keychain_lock.h"

#include "keychain_utilities.h"
#include "readline_cssm.h"
#include "security_tool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Security/SecKeychain.h>

static int
do_lock_all(void)
{
	OSStatus result = SecKeychainLockAll();
    if (result)
        sec_perror("SecKeychainLockAll", result);

	return result;
}

static int
do_lock(const char *keychainName)
{
	SecKeychainRef keychain = NULL;
	OSStatus result;

	if (keychainName)
	{
		keychain = keychain_open(keychainName);
		if (!keychain)
		{
			result = 1;
			goto loser;
		}
	}

	result = SecKeychainLock(keychain);
	if (result)
	{
		sec_error("SecKeychainLock %s: %s", keychainName ? keychainName : "<NULL>", sec_errstr(result));
	}

loser:
	if (keychain)
		CFRelease(keychain);

	return result;
}

int
keychain_lock(int argc, char * const *argv)
{
	char *keychainName = NULL;
	int ch, result = 0;
	Boolean lockAll = FALSE;
	while ((ch = getopt(argc, argv, "ah")) != -1)
	{
		switch  (ch)
		{
		case 'a':
			lockAll = TRUE;
			break;
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 1 && !lockAll)
	{
		keychainName = argv[0];
		if (*keychainName == '\0')
		{
			result = 2;
			goto loser;
		}
	}
	else if (argc != 0)
		return 2;

	if (lockAll)
		result = do_lock_all();
	else
		result = do_lock(keychainName);

loser:

	return result;
}
