/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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
 * limitations under the xLicense.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import <Security/Security.h>

#include "keychain_regressions.h"
#include "kc-helpers.h"
#include "kc-item-helpers.h"

#include <dispatch/private.h>
#include <Security/Security.h>

#include <stdlib.h>

#define BLOCKS 30

static OSStatus callbackFunction(SecKeychainEvent keychainEvent,
                                 SecKeychainCallbackInfo *info, void *context)
{
    CFRetainSafe(info->item);
    //printf("received a callback: %d %p\n", keychainEvent, info->item);
    CFReleaseNull(info->item);

    return 0;
}

static void tests() {
    SecKeychainRef kc = getEmptyTestKeychain();

    static dispatch_once_t onceToken = 0;
    static dispatch_queue_t process_queue = NULL;
    dispatch_once(&onceToken, ^{
        process_queue = dispatch_queue_create("com.apple.security.item-add-queue", DISPATCH_QUEUE_CONCURRENT);

        dispatch_queue_set_width(process_queue, 40);
    });
    dispatch_group_t g = dispatch_group_create();


    // Run the CFRunLoop to clear out existing notifications
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);

    UInt32 didGetNotification = 0;
    ok_status(SecKeychainAddCallback(callbackFunction, kSecAddEventMask | kSecDeleteEventMask | kSecDataAccessEventMask, &didGetNotification), "add callback");

    // Run the CFRunLoop to mark this run loop as "pumped"
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);

    for(int i = 0; i < BLOCKS; i++) {
        dispatch_group_async(g, process_queue, ^() {
            SecKeychainItemRef blockItem = NULL;
            CFStringRef itemclass = kSecClassInternetPassword;

            CFStringRef label = CFStringCreateWithFormat(NULL, NULL, CFSTR("testItem%05d"), i);
            CFStringRef account = CFSTR("testAccount");
            CFStringRef service = CFStringCreateWithFormat(NULL, NULL, CFSTR("testService%05d"), i);
            char * name;
            asprintf(&name, "%s (item %d)", testName, i);

            // add the item
            blockItem = createCustomItem(name, kc, createAddCustomItemDictionaryWithService(kc, itemclass, label, account, service));

            ok_status(SecKeychainItemDelete(blockItem), "%s: SecKeychainItemDelete", name);
            usleep(100 * arc4random_uniform(10000));
            CFReleaseNull(blockItem);

            free(name);
            CFReleaseNull(label);
            CFReleaseNull(service);
        });
    }

    // Process run loop until every block has run
    while(dispatch_group_wait(g, DISPATCH_TIME_NOW) != 0) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    }

    // One last hurrah
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", testName);
    CFReleaseNull(kc);
}

int kc_20_item_delete_stress(int argc, char *const *argv)
{
    plan_tests(getEmptyTestKeychainTests + 1 + (createCustomItemTests + 1)*BLOCKS + 1);
    initializeKeychainTests(__FUNCTION__);
    
    tests();
    
    deleteTestFiles();
    return 0;
}
