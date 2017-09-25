/*
 * Copyright (c) 2017 Apple Inc. All rights reserved.
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


#include <Foundation/Foundation.h>
#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecInternal.h>
#include <utilities/SecFileLocations.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecItemBackup.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "secd_regressions.h"

#include <securityd/SecItemServer.h>

#include "SecdTestKeychainUtilities.h"

void SecAccessGroupsSetCurrent(CFArrayRef accessGroups);
CFArrayRef SecAccessGroupsGetCurrent();


static void AddItem(NSDictionary *attr)
{
    NSMutableDictionary *mattr = [attr mutableCopy];
    mattr[(__bridge id)kSecValueData] = [NSData dataWithBytes:"foo" length:3];
    mattr[(__bridge id)kSecAttrAccessible] = (__bridge id)kSecAttrAccessibleAfterFirstUnlock;
    ok_status(SecItemAdd((__bridge CFDictionaryRef)mattr, NULL));
}

int secd_37_pairing_initial_sync(int argc, char *const *argv)
{
    CFErrorRef error = NULL;
    CFTypeRef stuff = NULL;
    OSStatus res = 0;

    plan_tests(16);

    /* custom keychain dir */
    secd_test_setup_temp_keychain("secd_37_pairing_initial_sync", NULL);

    CFArrayRef currentACL = SecAccessGroupsGetCurrent();

    NSMutableArray *newACL = [NSMutableArray arrayWithArray:(__bridge NSArray *)currentACL];
    [newACL addObjectsFromArray:@[
        @"com.apple.ProtectedCloudStorage",
    ]];

    SecAccessGroupsSetCurrent((__bridge CFArrayRef)newACL);


    NSDictionary *pcsinetattrs = @{
        (__bridge id)kSecClass : (__bridge id)kSecClassInternetPassword,
        (__bridge id)kSecAttrAccessGroup : @"com.apple.ProtectedCloudStorage",
        (__bridge id)kSecAttrAccount : @"1",
        (__bridge id)kSecAttrServer : @"current",
        (__bridge id)kSecAttrType : @(0x10001),
        (__bridge id)kSecAttrSynchronizable : @YES,
        (__bridge id)kSecAttrSyncViewHint :  (__bridge id)kSecAttrViewHintPCSMasterKey,
    };
    NSDictionary *pcsinetattrsNotCurrent = @{
        (__bridge id)kSecClass : (__bridge id)kSecClassInternetPassword,
        (__bridge id)kSecAttrAccessGroup : @"com.apple.ProtectedCloudStorage",
        (__bridge id)kSecAttrAccount : @"1",
        (__bridge id)kSecAttrServer : @"noncurrent",
        (__bridge id)kSecAttrType : @(0x00001),
        (__bridge id)kSecAttrSynchronizable : @YES,
        (__bridge id)kSecAttrSyncViewHint :  (__bridge id)kSecAttrViewHintPCSMasterKey,
    };
    NSDictionary *pcsgenpattrs = @{
       (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
       (__bridge id)kSecAttrAccessGroup : @"com.apple.ProtectedCloudStorage",
       (__bridge id)kSecAttrAccount : @"2",
       (__bridge id)kSecAttrSynchronizable : @YES,
       (__bridge id)kSecAttrSyncViewHint :  (__bridge id)kSecAttrViewHintPCSMasterKey,
    };
    NSDictionary *ckksattrs = @{
        (__bridge id)kSecClass : (__bridge id)kSecClassInternetPassword,
        (__bridge id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (__bridge id)kSecAttrAccount : @"2",
        (__bridge id)kSecAttrSynchronizable : @YES,
        (__bridge id)kSecAttrSyncViewHint :  (__bridge id)kSecAttrViewHintPCSMasterKey,
    };
    AddItem(pcsinetattrs);
    AddItem(pcsinetattrsNotCurrent);
    AddItem(pcsgenpattrs);
    AddItem(ckksattrs);

    CFArrayRef items = _SecServerCopyInitialSyncCredentials(SecServerInitialSyncCredentialFlagTLK | SecServerInitialSyncCredentialFlagPCS, &error);
    ok(items, "_SecServerCopyInitialSyncCredentials: %@", error);
    CFReleaseNull(error);

    ok_status((res = SecItemCopyMatching((__bridge CFDictionaryRef)pcsinetattrs, &stuff)),
              "SecItemCopyMatching: %d", (int)res);
    CFReleaseNull(stuff);
    ok_status((res = SecItemCopyMatching((__bridge CFDictionaryRef)pcsinetattrsNotCurrent, &stuff)),
              "SecItemCopyMatching: %d", (int)res);
    CFReleaseNull(stuff);
    ok_status((res = SecItemCopyMatching((__bridge CFDictionaryRef)pcsgenpattrs, &stuff)),
              "SecItemCopyMatching: %d", (int)res);
    CFReleaseNull(stuff);
    ok_status((res = SecItemCopyMatching((__bridge CFDictionaryRef)ckksattrs, &stuff)),
              "SecItemCopyMatching: %d", (int)res);
    CFReleaseNull(stuff);


    ok(_SecItemDeleteAll(&error), "SecItemServerDeleteAll: %@", error);
    CFReleaseNull(error);

    ok(_SecServerImportInitialSyncCredentials(items, &error), "_SecServerImportInitialSyncCredentials: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(items);

    ok_status((res = SecItemCopyMatching((__bridge CFDictionaryRef)pcsinetattrs, &stuff)),
              "SecItemCopyMatching: %d", (int)res);
    CFReleaseNull(stuff);
    is_status((res = SecItemCopyMatching((__bridge CFDictionaryRef)pcsinetattrsNotCurrent, &stuff)), errSecItemNotFound,
              "SecItemCopyMatching: %d", (int)res);
    CFReleaseNull(stuff);
    ok_status((res = SecItemCopyMatching((__bridge CFDictionaryRef)pcsgenpattrs, &stuff)),
              "SecItemCopyMatching: %d", (int)res);
    CFReleaseNull(stuff);
    ok_status((res = SecItemCopyMatching((__bridge CFDictionaryRef)ckksattrs, &stuff)),
              "SecItemCopyMatching: %d", (int)res);
    CFReleaseNull(stuff);

    SecAccessGroupsSetCurrent(currentACL);


    return 0;
}
