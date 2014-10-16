/*
 * Copyright (c) 2008,2010,2012-2014 Apple Inc. All Rights Reserved.
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

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sqlite3.h>

#include "Security_regressions.h"

const uint8_t keychain_data[] = {
    0x62, 0x70, 0x6c, 0x69, 0x73, 0x74, 0x30, 0x30, 0xd2, 0x01, 0x02, 0x03,
    0x04, 0x5f, 0x10, 0x1b, 0x4e, 0x53, 0x57, 0x69, 0x6e, 0x64, 0x6f, 0x77,
    0x20, 0x46, 0x72, 0x61, 0x6d, 0x65, 0x20, 0x50, 0x72, 0x6f, 0x63, 0x65,
    0x73, 0x73, 0x50, 0x61, 0x6e, 0x65, 0x6c, 0x5f, 0x10, 0x1d, 0x4e, 0x53,
    0x57, 0x69, 0x6e, 0x64, 0x6f, 0x77, 0x20, 0x46, 0x72, 0x61, 0x6d, 0x65,
    0x20, 0x41, 0x62, 0x6f, 0x75, 0x74, 0x20, 0x54, 0x68, 0x69, 0x73, 0x20,
    0x4d, 0x61, 0x63, 0x5f, 0x10, 0x1c, 0x32, 0x38, 0x20, 0x33, 0x37, 0x33,
    0x20, 0x33, 0x34, 0x36, 0x20, 0x32, 0x39, 0x30, 0x20, 0x30, 0x20, 0x30,
    0x20, 0x31, 0x34, 0x34, 0x30, 0x20, 0x38, 0x37, 0x38, 0x20, 0x5f, 0x10,
    0x1d, 0x35, 0x36, 0x38, 0x20, 0x33, 0x39, 0x35, 0x20, 0x33, 0x30, 0x37,
    0x20, 0x33, 0x37, 0x39, 0x20, 0x30, 0x20, 0x30, 0x20, 0x31, 0x34, 0x34,
    0x30, 0x20, 0x38, 0x37, 0x38, 0x20, 0x08, 0x0d, 0x2b, 0x4b, 0x6a, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8a
};

void kc_dbhandle_reset(void);

/* Test basic add delete update copy matching stuff. */
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
    int fd;
    ok_unix(fd = open(keychain_name, O_RDWR | O_CREAT | O_TRUNC, 0644),
        "create keychain file");
    is(write(fd, keychain_data, sizeof(keychain_data)),
        (ssize_t)sizeof(keychain_data), "write garbage to keychain file");
    ok_unix(close(fd), "close keychain file");

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

    CFRelease(query);
    CFRelease(eighty);
    CFRelease(pwdata);
#endif
}

int si_31_keychain_bad(int argc, char *const *argv)
{
	plan_tests(7);

	tests();

	return 0;
}
