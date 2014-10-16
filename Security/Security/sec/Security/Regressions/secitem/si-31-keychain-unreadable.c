/*
 * Copyright (c) 2008-2010,2012-2014 Apple Inc. All Rights Reserved.
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
 */


#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecInternal.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sqlite3.h>

#include "Security_regressions.h"

#ifdef NO_SERVER
static void ensureKeychainExists(void) {
    CFDictionaryRef query = CFDictionaryCreate(0, &kSecClass, &kSecClassInternetPassword, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFTypeRef results = NULL;
    is_status(SecItemCopyMatching(query, &results), errSecItemNotFound, "expected nothing got %@", results);
    CFReleaseNull(query);
    CFReleaseNull(results);
}
#endif

void kc_dbhandle_reset(void);

/* Create an empty keychain file that can't be read or written and make sure
   securityd can deal with it. */
static void tests(void)
{
#ifndef NO_SERVER
    plan_skip_all("No testing against server.");
#else
    const char *home_dir = getenv("HOME");
    char keychain_dir[1000];
    char keychain_name[1000];
    sprintf(keychain_dir, "%s/Library/Keychains", home_dir);
    sprintf(keychain_name, "%s/keychain-2-debug.db", keychain_dir);

    ensureKeychainExists();
    int fd;
    ok_unix(fd = open(keychain_name, O_RDWR | O_CREAT | O_TRUNC, 0644),
        "create keychain file '%s'", keychain_name);
    ok_unix(fchmod(fd, 0), " keychain file '%s'", keychain_name);
    ok_unix(close(fd), "close keychain file '%s'", keychain_name);

    kc_dbhandle_reset();

    int v_eighty = 80;
    CFNumberRef eighty = CFNumberCreate(NULL, kCFNumberSInt32Type, &v_eighty);
    const char *v_data = "test";
    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)v_data, strlen(v_data));
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(query, kSecClass, kSecClassInternetPassword);
    CFDictionaryAddValue(query, kSecAttrServer, CFSTR("members.spamcop.net"));
    CFDictionaryAddValue(query, kSecAttrAccount, CFSTR("smith"));
    CFDictionaryAddValue(query, kSecAttrPort, eighty);
    CFDictionaryAddValue(query, kSecAttrProtocol, kSecAttrProtocolHTTP);
    CFDictionaryAddValue(query, kSecAttrAuthenticationType, kSecAttrAuthenticationTypeDefault);
    CFDictionaryAddValue(query, kSecValueData, pwdata);
    ok_status(SecItemAdd(query, NULL), "add internet password");
    is_status(SecItemAdd(query, NULL), errSecDuplicateItem,
	"add internet password again");

    ok_status(SecItemCopyMatching(query, NULL), "Found the item we added");

    ok_status(SecItemDelete(query),"Deleted the item we added");

    CFReleaseSafe(eighty);
    CFReleaseSafe(pwdata);
    CFReleaseSafe(query);
#endif
}

int si_31_keychain_unreadable(int argc, char *const *argv)
{
	plan_tests(8);
	tests();

	return 0;
}
