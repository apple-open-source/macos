/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 */
/*
 *  key_create.c
 *  security
 *
 *  Created by Michael Brouwer on Fri May 23 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "key_create.h"

#include "keychain_utilities.h"

#include <Security/SecKey.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int
do_key_create_pair(const char *keychainName, CSSM_ALGORITHMS algorithm, uint32 keySizeInBits)
{
	SecKeychainRef keychain = NULL;
	OSStatus status;
	int result = 0;
	CSSM_CC_HANDLE contextHandle = 0;
	CSSM_KEYUSE publicKeyUsage = CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_WRAP | CSSM_KEYUSE_DERIVE;
	uint32 publicKeyAttr = CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE;
	CSSM_KEYUSE privateKeyUsage = CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_SIGN | CSSM_KEYUSE_UNWRAP | CSSM_KEYUSE_DERIVE;
	uint32 privateKeyAttr = CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_SENSITIVE | CSSM_KEYATTR_EXTRACTABLE;
	SecAccessRef initialAccess = NULL;
	SecKeyRef publicKey = NULL;
	SecKeyRef privateKey = NULL;

	if (keychainName)
	{
		keychain = keychain_open(keychainName);
		if (!keychain)
		{
			result = 1;
			goto loser;
		}
	}

	status = SecKeyCreatePair(keychain, algorithm, keySizeInBits, contextHandle,
        publicKeyUsage,
        publicKeyAttr,
        privateKeyUsage,
        privateKeyAttr,
        initialAccess,
        &publicKey, 
        &privateKey);
	if (status)
	{
		fprintf(stderr, "SecKeyCreatePair %s returned %ld(0x%lx)\n", keychainName ? keychainName : "<NULL>", status, status);
		result = 1;
	}

loser:
	if (keychain)
		CFRelease(keychain);

	return result;
}

static int
parse_algorithm(const char *name, CSSM_ALGORITHMS *algorithm)
{
	size_t len = strlen(name);

	if (!strncmp("rsa", name, len))
		*algorithm = CSSM_ALGID_RSA;
	else if (!strncmp("dsa", name, len))
		*algorithm = CSSM_ALGID_DSA;
	else if (!strncmp("dh", name, len))
		*algorithm = CSSM_ALGID_DH;
	else if (!strncmp("fee", name, len))
		*algorithm = CSSM_ALGID_FEE;
	else
	{
		fprintf(stderr, "Invalid algorithm: %s\n", name);
		return 2;
	}

	return 0;
}

int
key_create_pair(int argc, char * const *argv)
{
	const char *keychainName = NULL;
	CSSM_ALGORITHMS algorithm = CSSM_ALGID_RSA;
	uint32 keySizeInBits = 512;
	int ch, result = 0;

/*
    { "create-keypair", key_create_pair,
	  "[-a alg] [-s size] [-f date] [-t date] [-v days] [-k keychain] [-n name] [-A|-T app1:app2:...]\n"
	  "    -a  Use alg as the algorithm, can be rsa, dh, dsa or fee (default rsa)\n"
	  "    -s  Specify the keysize in bits (default 512)\n"
	  "    -f  Make a key valid from the specified date\n"
	  "    -t  Make a key valid to the specified date\n"
	  "    -v  Make a key valid for the number of days specified from today\n"
	  "    -k  Use the specified keychain rather than the default\n"
	  "    -A  Allow any application to access without warning.\n"
	  "    -T  Allow the applications specified to access without warning.\n"
	  "If no options are provided ask the user interactively",
*/

    while ((ch = getopt(argc, argv, "a:s:f:t:v:k:AT:h")) != -1)
	{
		switch  (ch)
		{
        case 'a':
			result = parse_algorithm(optarg, &algorithm);
			if (result)
				goto loser;
			break;
        case 's':
			keySizeInBits = atoi(optarg);
			break;

		case 'f':
		case 't':
		case 'v':
		case 'k':
		case 'A':
		case 'T':
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

	result = do_key_create_pair(keychainName, algorithm, keySizeInBits);	

loser:
	return result;
}
