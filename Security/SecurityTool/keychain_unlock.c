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
 * keychain_unlock.c
 */

#include "keychain_unlock.h"
#include "readline.h"
#include "keychain_utilities.h"
#include "security.h"

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int
do_unlock(const char *keychainName, char *password, Boolean use_password)
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

	result = SecKeychainUnlock(keychain, password ? strlen(password) : 0, password, use_password);
	if (result)
	{
		sec_error("SecKeychainUnlock %s: %s", keychainName ? keychainName : "<NULL>", sec_errstr(result));
	}

loser:
	if (keychain)
		CFRelease(keychain);

	return result;
}

int
keychain_unlock(int argc, char * const *argv)
{
	int zero_password = 0;
	char *password = NULL;
	int ch, result = 0;
	Boolean use_password = TRUE;
	const char *keychainName = NULL;

	while ((ch = getopt(argc, argv, "hp:u")) != -1)
	{
		switch  (ch)
		{
		case 'p':
			password = optarg;
			break;
        case 'u':
            use_password = FALSE;
			break;
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
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
			goto loser;
		}
	}
	else if (argc != 0)
		return 2;

    if (!password && use_password)
    {
		const char *fmt = "password to unlock %s: ";
		const char *name = keychainName ? keychainName : "default";
		char *prompt = malloc(strlen(fmt) + strlen(name));
		sprintf(prompt, fmt, name);
        password = getpass(prompt);
		free(prompt);
		if (!password)
		{
			result = -1;
			goto loser;
		}
		zero_password = 1;
    }

	result = do_unlock(keychainName, password, use_password);
	if (result)
		goto loser;

loser:
	if (zero_password)
		memset(password, 0, strlen(password));

	return result;
}
