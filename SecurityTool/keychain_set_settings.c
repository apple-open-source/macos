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
 *  keychain_set_settings.c
 *  security
 *
 *  Created by Archana Gottipaty on Wed May 14 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "keychain_set_settings.h"
#include "keychain_utilities.h"
#include "readline.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Security/SecKeychain.h>

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
			goto loser;
		}
	}
	result = SecKeychainSetSettings(keychain, &newKeychainSettings);
	if (result)
	{
		fprintf(stderr, "SecKeychainSetSettings %s returned %ld(0x%lx)\n", keychainName ? keychainName : "<NULL>", result, result);
	}

loser:
	if (keychain)
		CFRelease(keychain);

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

	result = do_keychain_set_settings(keychainName, newKeychainSettings);
	
loser:

	return result;
}
