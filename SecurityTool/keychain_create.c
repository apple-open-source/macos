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
 *  keychain_create.c
 *  security
 *
 *  Created by Michael Brouwer on Tue May 06 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "keychain_create.h"

#include "readline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Security/SecKeychain.h>

static int
do_create(const char *keychain, const char *password, Boolean do_prompt)
{
	SecKeychainRef keychainRef = NULL;
	OSStatus result;

	result = SecKeychainCreate(keychain, password ? strlen(password) : 0, password, do_prompt, NULL, &keychainRef);
	if (keychainRef)
		CFRelease(keychainRef);

	if (result)
		fprintf(stderr, "SecKeychainCreate(%s) returned %ld(0x%lx)\n", keychain, result, result);

	return result;
}

int
keychain_create(int argc, char * const *argv)
{
	int free_keychain = 0, free_password = 0;
	char *password = NULL, *keychain = NULL;
	int ch, result = 0;
	Boolean do_prompt = FALSE;

/* AG: getopts optstring name [args]
    AG: while loop calling getopt is used to extract password from cl from user
    password is the only option to keychain_create
    optstring  is  a  string  containing the legitimate option
    characters.  If such a character is followed by  a  colon,
    the  option  requires  an  argument,  so  getopt  places a
    pointer to the following text in the same argv-element, or
    the  text  of  the following argv-element, in optarg.
*/
	while ((ch = getopt(argc, argv, "hp:P")) != -1)
	{
		switch  (ch)
		{
		case 'p':
			password = optarg;
			break;
		case 'P':
			do_prompt = TRUE;
			break;
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}
/*
    AG:   The external variable optind is  the  index  of  the  next
       array  element  of argv[] to be processed; it communicates
       from one call of getopt() to the  next  which  element  to
       process.
       The variable optind is the index of the next element of the argv[] vector to 	be processed. It shall be initialized to 1 by the system, and getopt() shall 	update it when it finishes with each element of argv[]. When an element of argv[] 	contains multiple option characters, it is unspecified how getopt() determines 	which options have already been processed.

*/
	argc -= optind;
	argv += optind;

	if (argc > 0)
		keychain = *argv;
	else
	{
		fprintf(stderr, "keychain to create: ");
		keychain = readline(NULL, 0);
		if (!keychain)
		{
			result = -1;
			goto loser;
		}

		free_keychain = 1;
		if (*keychain == '\0')
			goto loser;
	}

	if (!password && !do_prompt)
	{
		fprintf(stderr, "password for new keychain: ");
		password = readline(NULL, 0);
		if (!password)
		{
			result = -1;
			goto loser;
		}
		free_password = 1;
	}

	do
	{
		result = do_create(keychain, password, do_prompt);
		if (result)
			goto loser;

		argc--;
		argv++;
		if (!free_keychain)
			keychain = *argv;
	} while (argc > 0);

loser:
	if (free_keychain)
		free(keychain);
	if (free_password)
		free(password);

	return result;
}
