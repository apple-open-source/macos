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

/*
 * This is to fool os services to not provide the Keychain manager
 * interface tht doens't work since we don't have unified headers
 * between iOS and OS X. rdar://23405418/
 */
#define __KEYCHAINCORE__ 1


#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <Security/SecBase.h>
#import <Security/SecItem.h>
#import <Security/SecItemPriv.h>
#import <Security/SecInternal.h>
#import <utilities/SecCFWrappers.h>
#import <utilities/SecFileLocations.h>
#import <securityd/SecItemServer.h>

#import <stdlib.h>

#include "secd_regressions.h"
#include "SecdTestKeychainUtilities.h"

void SecAccessGroupsSetCurrent(CFArrayRef accessGroups);
CFArrayRef SecAccessGroupsGetCurrent();

int
secd_21_transmogrify(int argc, char *const *argv)
{
    plan_tests(kSecdTestSetupTestCount + 14);

#if TARGET_OS_IOS
    CFErrorRef error = NULL;
    CFDictionaryRef result = NULL;
    OSStatus res;

    CFArrayRef currentACL = SecAccessGroupsGetCurrent();

    NSMutableArray *newACL = [NSMutableArray arrayWithArray:(__bridge NSArray *)currentACL];
    [newACL addObjectsFromArray:@[
                                  @"com.apple.private.system-keychain",
                                  @"com.apple.private.syncbubble-keychain",
                                  @"com.apple.private.migrate-musr-system-keychain",
                                  @"com.apple.ProtectedCloudStorage",
                                  ]];
    
    SecAccessGroupsSetCurrent((__bridge CFArrayRef)newACL);


    secd_test_setup_temp_keychain("secd_21_transmogrify", NULL);

    /*
     * Add to user keychain
     */

    res = SecItemAdd((CFDictionaryRef)@{
        (id)kSecClass :  (id)kSecClassGenericPassword,
        (id)kSecAttrAccount :  @"user-label-me",
    }, NULL);
    is(res, 0, "SecItemAdd(user)");

    SecurityClient client = {
        .task = NULL,
        .accessGroups = (__bridge CFArrayRef)@[
            @"com.apple.ProtectedCloudStorage"
        ],
        .allowSystemKeychain = true,
        .allowSyncBubbleKeychain = true,
        .uid = 502,
        .inMultiUser = false,
        .activeUser = 502,
    };

    is(_SecServerTransmogrifyToSystemKeychain(&client, &error), true, "_SecServerTransmogrifyToSystemKeychain: %@", error);

    CFDataRef musr = SecMUSRCreateActiveUserUUID(502);

    client.inMultiUser = true;
    client.musr = musr;

    SecSecuritySetMusrMode(true, 502, 502);

    res = SecItemCopyMatching((CFDictionaryRef)@{
        (id)kSecClass :  (id)kSecClassGenericPassword,
        (id)kSecAttrAccount :  @"user-label-me",
        (id)kSecUseSystemKeychain : (id)kCFBooleanTrue,
        (id)kSecReturnAttributes : (id)kCFBooleanTrue,
    }, (CFTypeRef *)&result);
    is(res, 0, "SecItemCopyMatching(system)");

    ok(isDictionary(result), "found item");
    if (isDictionary(result)) {
        NSData *data = ((__bridge NSDictionary *)result)[@"musr"];
        ok([data isEqual:(__bridge id)SecMUSRGetSystemKeychainUUID()], "item is system keychain");
    } else {
        ok(0, "returned item is: %@", result);
    }
    CFReleaseNull(result);

    /*
     * Check sync bubble
     */

    res = _SecItemAdd((__bridge CFDictionaryRef)@{
        (id)kSecClass :  (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : @"com.apple.ProtectedCloudStorage",
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecAttrAccount :  @"pcs-label-me",
    }, &client, NULL, NULL);
    is(res, true, "SecItemAdd(user)");

    res = _SecItemCopyMatching((__bridge CFDictionaryRef)@{
         (id)kSecClass :  (id)kSecClassGenericPassword,
         (id)kSecAttrAccount :  @"pcs-label-me",
         (id)kSecReturnAttributes : (id)kCFBooleanTrue,
     }, &client, (CFTypeRef *)&result, &error);
    is(res, true, "SecItemCopyMatching(system): %@", error);

    ok(isDictionary(result), "result is dictionary");

    /* Check that data are in 502 active user keychain */
    ok (CFEqualSafe(((__bridge CFDataRef)((__bridge NSDictionary *)result)[@"musr"]), musr), "not in msr 502");

    CFReleaseNull(result);


    ok(_SecServerTransmogrifyToSyncBubble((__bridge CFArrayRef)@[@"com.apple.mailq.sync.xpc" ], client.uid, &client, &error),
       "_SecServerTransmogrifyToSyncBubble: %@", error);

    CFReleaseNull(error);

    /*
     * first check normal keychain
     */

    res = _SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecClass :  (id)kSecClassGenericPassword,
        (id)kSecAttrAccount :  @"pcs-label-me",
        (id)kSecReturnAttributes : (id)kCFBooleanTrue,
    }, &client, (CFTypeRef *)&result, &error);
    is(res, true, "SecItemCopyMatching(active): %@", error);

    ok(isDictionary(result), "result is dictionary");
    CFReleaseNull(result);

    SecSecuritySetMusrMode(true, 503, 503);

    /*
     * then syncbubble keychain
     */

    res = _SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecClass :  (id)kSecClassGenericPassword,
        (id)kSecAttrAccount :  @"pcs-label-me",
        (id)kSecReturnAttributes : (id)kCFBooleanTrue,
        (id)kSecUseSyncBubbleKeychain : @502,
    }, &client, (CFTypeRef *)&result, &error);
    is(res, true, "SecItemCopyMatching(syncbubble): %@", error);
    ok(isDictionary(result), "result is dictionary");

    CFReleaseNull(result);

    SecSecuritySetMusrMode(false, 501, -1);

    SecAccessGroupsSetCurrent(currentACL);

    CFRelease(musr);
#else
    plan_skip_all("not support on non TARGET_OS_IOS");
#endif
    return 0;
}
