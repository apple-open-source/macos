//
//  Copyright 2016 Apple. All rights reserved.
//

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
    if (status != errSecSuccess && status != errSecItemNotFound)
        errx(1, "cleanup item: %d", (int)status);

    notify_register_dispatch("com.apple.security.view-change.PCS", &token, queue, ^(int __unused token2) {
        printf("got notification\n");
        got_notification = true;
    });

    /*
     * now check add notification
     */

    status = SecItemAdd((__bridge CFDictionaryRef)query, NULL);
    if (status != errSecSuccess)
        errx(1, "add item: %d", (int)status);

    sleep(3);

    if (!got_notification)
        errx(1, "failed to get notification on add");
    got_notification = false;

    /*
     * clean up and check delete notification too
     */

    status = SecItemDelete((__bridge CFDictionaryRef)query);
    if (status != errSecSuccess)
        errx(1, "cleanup2 item: %d", (int)status);

    sleep(3);

    if (!got_notification)
        errx(1, "failed to get notification on delete");

    return 0;
}
