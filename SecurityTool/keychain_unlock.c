/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  keychain_unlock.c
 *  security
 *
 *  Created by Archana Gottipaty on Sun May 11 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "keychain_unlock.h"
#include "readline.h"
#include "keychain_utilities.h"
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
		fprintf(stderr, "SecKeychainUnlock %s returned %ld(0x%lx)\n", keychainName ? keychainName : "<NULL>", result, result);
	}

loser:
	if (keychain)
		CFRelease(keychain);

	return result;
}

int
keychain_unlock(int argc, char * const *argv)
{
	int free_password = 0;
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
        fprintf(stderr, "password to unlock %s: ", keychainName ? keychainName : "default");
        password = readline(NULL, 0);
		if (!password)
		{
			result = -1;
			goto loser;
		}
		free_password = 1;
    }

	result = do_unlock(keychainName, password, use_password);
	if (result)
		goto loser;

loser:
	if (free_password)
		free(password);

	return result;
}
