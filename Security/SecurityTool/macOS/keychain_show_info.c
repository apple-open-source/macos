/*
 * Copyright (c) 2003-2004,2008-2009,2012,2014 Apple Inc. All Rights Reserved.
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
 * keychain_show_info.c
 */

#include "keychain_show_info.h"
#include "keychain_utilities.h"
#include "readline_cssm.h"
#include "security_tool.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainPriv.h>
#include <Security/SecCoreAnalytics.h>
#include "utilities/SecCFRelease.h"

static int
do_keychain_show_info(const char *keychainName, Boolean showSalt)
{
	SecKeychainRef keychain = NULL;
    SecKeychainSettings keychainSettings = { SEC_KEYCHAIN_SETTINGS_VERS1 };
	OSStatus result;
    CFDataRef salt = NULL;

	if (keychainName)
	{
		keychain = keychain_open(keychainName);
		if (!keychain)
		{
			result = 1;
			goto loser;
		}
	}

	result = SecKeychainCopySettings(keychain, &keychainSettings);
	if (result)
	{
		sec_error("SecKeychainCopySettings %s: %s", keychainName ? keychainName : "<NULL>", sec_errstr(result));
		goto loser;
	}

    if (showSalt) {
        result = SecKeychainCopySalt(keychain, &salt);
        if (result)
        {
            sec_error("SecKeychainCopySalt %s: %s", keychainName ? keychainName : "<NULL>", sec_errstr(result));
            goto loser;
        }
    }

    fprintf(stderr,"Keychain \"%s\"%s%s",
		keychainName ? keychainName : "<NULL>",
		keychainSettings.lockOnSleep ? " lock-on-sleep" : "",
		keychainSettings.useLockInterval ? " use-lock-interval" : "");
	if (keychainSettings.lockInterval == INT_MAX) {
		fprintf(stderr," no-timeout");
	} else {
		fprintf(stderr," timeout=%ds", (int)keychainSettings.lockInterval);
	}

    if (salt) {
        fprintf(stderr, " salt=");
        CFIndex end = CFDataGetLength(salt);
        const UInt8 * ptr = CFDataGetBytePtr(salt);
        for (CFIndex i=0;i<end;i++) {
            fprintf(stderr,"%02X", ptr[i]);
        }
    }

    fprintf(stderr,"\n");

loser:
	CFReleaseNull(keychain);
	CFReleaseNull(salt);
	return result;
}

int
keychain_show_info(int argc, char * const *argv)
{
    // ensure that use of `security show-keychain-info` during early boot doesn't hang waiting to send CA events at shutdown (rdar://146406899)
    SecCoreAnalyticsSetEnabledForProcess(false);

	char *keychainName = NULL;
    int ch, result = 0;
    Boolean showSalt = FALSE;

    while ((ch = getopt(argc, argv, "s")) != -1)
    {
        switch  (ch)
        {
        case 's':
            showSalt = TRUE;
            break;
        case '?':
        default:
            return SHOW_USAGE_MESSAGE;
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
		return SHOW_USAGE_MESSAGE;

	result = do_keychain_show_info(keychainName, showSalt);

loser:
	return result;
}
