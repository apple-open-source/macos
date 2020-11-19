/*
 * Copyright (c) 2016,2020 Apple Inc. All Rights Reserved.
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

#include <Foundation/Foundation.h>
#include <Security/Security.h>
#include <Security/SecItemPriv.h>
#include <notify.h>
#include <err.h>
#import <TargetConditionals.h>

int
main(int argc, const char ** argv)
{
    dispatch_queue_t queue = dispatch_queue_create("notifications-queue", NULL);
    __block int got_notification = false;
    OSStatus status;
    int token;

    NSDictionary *query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : @"keychain-test1",
        (id)kSecAttrSyncViewHint : @"PCS-Master",
        (id)kSecAttrAccount : @"account-delete-me",
        (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleAfterFirstUnlock,
    };
    status = SecItemDelete((__bridge CFDictionaryRef)query);
    if (status != errSecSuccess && status != errSecItemNotFound) {
        errx(1, "cleanup item: %d", (int)status);
    }

    notify_register_dispatch("com.apple.security.view-change.PCS", &token, queue, ^(int __unused token2) {
        printf("got notification\n");
        got_notification = true;
    });

    /*
     * now check add notification
     */

    status = SecItemAdd((__bridge CFDictionaryRef)query, NULL);
    if (status != errSecSuccess) {
        errx(1, "add item: %d", (int)status);
    }

    sleep(3);

// Bridge explicitly disables notify phase, no PCS, octagon or sos on this platform
#if !TARGET_OS_BRIDGE
    if (!got_notification) {
        errx(1, "failed to get notification on add");
    }
#else
    if (got_notification) {
        errx(1, "received unexpected notification on add");
    }
#endif
    got_notification = false;

    /*
     * clean up and check delete notification too
     */

    status = SecItemDelete((__bridge CFDictionaryRef)query);
    if (status != errSecSuccess) {
        errx(1, "cleanup2 item: %d", (int)status);
    }

    sleep(3);

#if !TARGET_OS_BRIDGE
    if (!got_notification) {
        errx(1, "failed to get notification on delete");
    }
#else
    if (got_notification) {
        errx(1, "received unexpected notification on delete");
    }
#endif

    return 0;
}
