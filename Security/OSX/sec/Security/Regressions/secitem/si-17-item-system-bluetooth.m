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

// If we're running no server this test isn't valid
#if defined(NO_SERVER) && NO_SERVER
#define DO_TESTS false
#else
#define DO_TESTS true
#endif


static NSString* const kBlueToothServiceName = @"BluetoothGlobal";

/* Test presence of properly migrated items into system keychain. */
static void tests(void)
{
    NSDictionary *query;
    NSDictionary *whoami = NULL;

    whoami = CFBridgingRelease(_SecSecuritydCopyWhoAmI(NULL));

    NSLog(@"whoami: %@", whoami);

    /*
     * Check for multi user mode and its expected behavior (since its different)
     */

    bool multiUser = (whoami[@"musr"]) ? true : false;

    /*
     * Check we can't find it in our keychain
     */
    SKIP: {
        skip("No Server mode, test not valid", 2, DO_TESTS)
        query = @{
                  (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
                  (__bridge id)kSecAttrService : kBlueToothServiceName,
                  };

        is(SecItemCopyMatching((CFTypeRef)query, NULL), multiUser ? errSecItemNotFound : noErr, "Blue tooth item - user keychain");

        query = @{
                  (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
                  (__bridge id)kSecAttrService : kBlueToothServiceName,
                  (__bridge id)kSecUseSystemKeychain : @YES,
                  };
        
        is(SecItemCopyMatching((CFTypeRef)query, NULL), noErr, "Blue tooth item - system keychain");
    }
}

int si_17_item_system_bluetooth(int argc, char *const *argv)
{
    plan_tests(2);

    @autoreleasepool {
        tests();
    }
    
    return 0;
}
