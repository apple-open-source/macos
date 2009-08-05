/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
 * keychain_recode.c
 */

#include "keychain_recode.h"

#include "keychain_utilities.h"
#include "readline.h"
#include "security.h"

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecKeychain.h>

// SecKeychainCopyBlob, SecKeychainRecodeKeychain
#include <Security/SecKeychainPriv.h>


static int
do_recode(const char *keychainName1, const char *keychainName2)
{
	SecKeychainRef keychain1 = NULL, keychain2 = NULL;
	CFMutableArrayRef dbBlobArray = NULL;
	CFDataRef dbBlob = NULL, extraData = NULL;
	OSStatus result;

	if (keychainName1)
	{
		keychain1 = keychain_open(keychainName1);
		if (!keychain1)
		{
			result = 1;
			goto loser;
		}
	}

	keychain2 = keychain_open(keychainName2);
	if (!keychain2)
	{
		result = 1;
		goto loser;
	}

	result = SecKeychainCopyBlob(keychain2, &dbBlob);
	if (result)
	{
		sec_error("SecKeychainCopyBlob %s: %s", keychainName2,
			sec_errstr(result));
		goto loser;
	}

	extraData = CFDataCreate(NULL, NULL, 0);
	
	dbBlobArray = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
	if (dbBlobArray) {
		CFArrayAppendValue(dbBlobArray, dbBlob);
	}

#if !defined MAC_OS_X_VERSION_10_6 || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
	result = SecKeychainRecodeKeychain(keychain1, dbBlob, extraData);
#else
	result = SecKeychainRecodeKeychain(keychain1, dbBlobArray, extraData);
#endif
	if (result)
		sec_error("SecKeychainRecodeKeychain %s, %s: %s", keychainName1,
			keychainName2, sec_errstr(result));

loser:
	if (dbBlobArray)
		CFRelease(dbBlobArray);
	if (dbBlob)
		CFRelease(dbBlob);
	if (extraData)
		CFRelease(extraData);
	if (keychain1)
		CFRelease(keychain1);
	if (keychain2)
		CFRelease(keychain2);

	return result;
}

int
keychain_recode(int argc, char * const *argv)
{
	char *keychainName1 = NULL, *keychainName2 = NULL;
	int ch, result = 0;

	while ((ch = getopt(argc, argv, "h")) != -1)
	{
		switch  (ch)
		{
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 2)
	{
		keychainName1 = argv[0];
		if (*keychainName1 == '\0')
		{
			result = 2;
			goto loser;
		}

		keychainName2 = argv[1];
		if (*keychainName2 == '\0')
		{
			result = 2;
			goto loser;
		}

	}
	else
		return 2;

	result = do_recode(keychainName1, keychainName2);

loser:

	return result;
}
