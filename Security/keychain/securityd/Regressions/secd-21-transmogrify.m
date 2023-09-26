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

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <Security/SecBase.h>
#import <Security/SecItem.h>
#import <Security/SecItemPriv.h>
#import <Security/SecInternal.h>
#import <utilities/SecCFWrappers.h>
#import <utilities/SecFileLocations.h>
#import "keychain/securityd/SecItemServer.h"

#import <stdlib.h>

#include "secd_regressions.h"
#include "SecdTestKeychainUtilities.h"
#include "server_security_helpers.h"

int
secd_21_transmogrify(int argc, char *const *argv)
{
    plan_tests(kSecdTestSetupTestCount + 18);

#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
    CFErrorRef error = NULL;
    CFDictionaryRef result = NULL;
    OSStatus res;

    CFArrayRef currentACL = CFRetainSafe(SecAccessGroupsGetCurrent());

    NSMutableArray *newACL = [NSMutableArray arrayWithArray:(__bridge NSArray *)currentACL];
    [newACL addObjectsFromArray:@[
                                  @"com.apple.ProtectedCloudStorage",
                                  ]];
    
    SecAccessGroupsSetCurrent((__bridge CFArrayRef)newACL);


    secd_test_setup_temp_keychain("secd_21_transmogrify", NULL);

    // Add to user keychain
    // This is before going into "edu mode"

    res = SecItemAdd((CFDictionaryRef)@{
        (id)kSecClass :  (id)kSecClassGenericPassword,
        (id)kSecAttrAccount :  @"user-label-me",
        (id)kSecValueData : [NSData dataWithBytes:"password" length:8],
        (id)kSecAttrAccessGroup : @"com.apple.ProtectedCloudStorage"
    }, NULL);
    is(res, 0, "SecItemAdd(user)");

    SecurityClient client = {
        .task = NULL,
        .accessGroups = (__bridge CFArrayRef)@[
            @"com.apple.ProtectedCloudStorage"
        ],
#if KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
        .allowSystemKeychain = true,
#endif
        .allowSyncBubbleKeychain = true,
        .uid = 502,
        .inEduMode = false,
        .activeUser = 502,
    };

    // Move things to the system keychain, going into "edu mode'
    is(_SecServerTransmogrifyToSystemKeychain(&client, &error), true, "_SecServerTransmogrifyToSystemKeychain: %@", error);

    CFDataRef musr = SecMUSRCreateActiveUserUUID(502);

    client.inEduMode = true;
    client.musr = musr;

    // Check that the item is in the system keychain
    res = _SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecClass :  (id)kSecClassGenericPassword,
        (id)kSecAttrAccount :  @"user-label-me",
        (id)kSecUseSystemKeychain : (id)kCFBooleanTrue,
        (id)kSecReturnAttributes : (id)kCFBooleanTrue,
        (id)kSecReturnData : @(YES)
    }, &client, (CFTypeRef *)&result, &error);
    is(res, true, "_SecItemCopyMatching(system)");

    ok(isDictionary(result), "found item");
    if (isDictionary(result)) {
        NSData *data = ((__bridge NSDictionary *)result)[@"musr"];
        ok([data isEqual:(__bridge id)SecMUSRGetSystemKeychainUUID()], "item is system keychain");

        NSData* passwordData = [(__bridge NSDictionary*)result valueForKey:(id)kSecValueData];
        ok([passwordData isEqual:[NSData dataWithBytes:"password" length:8]], "no data found in transmogrified item");
    } else {
        ok(0, "returned item is: %@", result);
        ok(0, "returned error is: %@", error);
    }
    CFReleaseNull(result);

    // Check sync bubble
    // Note that we are already in "edu mode", because we called _SecServerTransmogrifyToSystemKeychain above.
    // This means the DB is (most likely) protected by the system keybag.

    // Add an item to the 502 active user keychain
    res = _SecItemAdd((__bridge CFDictionaryRef)@{
        (id)kSecClass :  (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : @"com.apple.ProtectedCloudStorage",
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecAttrAccount :  @"pcs-label-me",
        (id)kSecValueData : [NSData dataWithBytes:"some data" length:9],
    }, &client, NULL, NULL);
    is(res, true, "SecItemAdd(userforsyncbubble)");

    // Check that the item is in the 502 active user keychain
    res = _SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecClass :  (id)kSecClassGenericPassword,
        (id)kSecAttrAccount :  @"pcs-label-me",
        (id)kSecReturnAttributes : (id)kCFBooleanTrue,
        (id)kSecReturnData : @(YES),
    }, &client, (CFTypeRef *)&result, &error);
    is(res, true, "SecItemCopyMatching(userforsyncbubble): %@", error);

    ok(isDictionary(result), "result is dictionary");
    ok([[(__bridge NSDictionary*)result valueForKey:(__bridge id)kSecValueData] isEqual:[NSData dataWithBytes:"some data" length:9]], "retrieved data matches stored data");

    ok (CFEqualSafe(((__bridge CFDataRef)((__bridge NSDictionary *)result)[@"musr"]), musr), "should match musr for active user 502");

    CFReleaseNull(result);

    // Now copy things to the sync bubble keychain
    ok(_SecServerTransmogrifyToSyncBubble((__bridge CFArrayRef)@[@"com.apple.mailq.sync.xpc" ], client.uid, &client, &error),
       "_SecServerTransmogrifyToSyncBubble: %@", error);

    CFReleaseNull(error);

    // Check the 502 active user keychain again, since the item should have been copied, not moved.
    res = _SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecClass :  (id)kSecClassGenericPassword,
        (id)kSecAttrAccount :  @"pcs-label-me",
        (id)kSecReturnAttributes : (id)kCFBooleanTrue,
        (id)kSecReturnData : @(YES),
    }, &client, (CFTypeRef *)&result, &error);
    is(res, true, "SecItemCopyMatching(active): %@", error);

    ok(isDictionary(result), "result is dictionary");
    ok([[(__bridge NSDictionary*)result valueForKey:(__bridge id)kSecValueData] isEqual:[NSData dataWithBytes:"some data" length:9]], "retrieved data matches stored data");
    ok (CFEqualSafe(((__bridge CFDataRef)((__bridge NSDictionary *)result)[@"musr"]), musr), "should still match musr for active user 502");
    CFReleaseNull(result);

    SecSecuritySetMusrMode(true, 503, 503);

    // Now check that the item exists in the syncbubble keychain for user 502
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
    CFReleaseNull(currentACL);

    CFRelease(musr);

    secd_test_teardown_delete_temp_keychain("secd_21_transmogrify");
#else
    plan_skip_all("not support on non KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER");
#endif
    return 0;
}
