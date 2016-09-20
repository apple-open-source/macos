/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

#define __KEYCHAINCORE__ 1

#include <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecItemPriv.h>
#include <utilities/array_size.h>
#include <utilities/SecCFRelease.h>
#include <stdlib.h>
#include <unistd.h>

#include "Security_regressions.h"


/* Test add api in all it's variants. */
static void tests(void)
{
    NSDictionary *item, *query;
    CFTypeRef result = NULL;
    NSDictionary *whoami = NULL;

    whoami = CFBridgingRelease(_SecSecuritydCopyWhoAmI(NULL));

    NSLog(@"whoami: %@", whoami);

    /*
     * first clean some
     */

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
    };
    SecItemDelete((CFDictionaryRef)query);

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychain : @YES,
    };
    SecItemDelete((CFDictionaryRef)query);

    /*
     * Add entry
     */

    item = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychain : @YES,
    };

    ok_status(SecItemAdd((CFDictionaryRef)item, NULL), "SecItemAdd");

    /*
     * Check for multi user mode and its expected behavior (since its different)
     */

    bool multiUser = (whoami[@"musr"]) ? true : false;

    /*
     * Check we can't find it in our keychain
     */

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
    };

    is(SecItemCopyMatching((CFTypeRef)query, &result), multiUser ? errSecItemNotFound : noErr, "SecItemCopyMatching");
    CFReleaseNull(result);
    if (multiUser) {
        is(SecItemDelete((CFTypeRef)query), errSecItemNotFound, "SecItemDelete");
    } else {
        ok(true, "dummy");
    }

    /*
     * Check we can find it in system keychain
     */

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychain : @YES,
    };

    ok_status(SecItemCopyMatching((CFTypeRef)query, &result), "SecItemCopyMatching(system)");

    ok_status(SecItemDelete((CFTypeRef)query), "SecItemDelete(system)");
}

int si_13_item_system(int argc, char *const *argv)
{
    plan_tests(5);

    @autoreleasepool {
        tests();
    }
    
    return 0;
}
