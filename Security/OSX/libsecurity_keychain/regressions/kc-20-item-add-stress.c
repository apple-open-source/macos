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

#include <Security/Security.h>

#define BLOCKS 300

static void tests() {
    SecKeychainRef kc = getPopulatedTestKeychain();

    static dispatch_once_t onceToken = 0;
    static dispatch_queue_t process_queue = NULL;
    dispatch_once(&onceToken, ^{
        process_queue = dispatch_queue_create("com.apple.security.item-add-queue", DISPATCH_QUEUE_CONCURRENT);
    });
    dispatch_group_t g = dispatch_group_create();

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
            CFReleaseNull(blockItem);

            // find the item
            blockItem = checkNCopyFirst(name, createQueryCustomItemDictionaryWithService(kc, itemclass, label, service), 1);
            readPasswordContents(blockItem, CFSTR("data"));

            // update the item
            CFMutableDictionaryRef newData = CFDictionaryCreateMutable(NULL, 0,
                                                                     &kCFTypeDictionaryKeyCallBacks,
                                                                     &kCFTypeDictionaryValueCallBacks);
            CFDataRef differentdata = CFDataCreate(NULL, (void*)"differentdata", strlen("differentdata"));
            CFDictionarySetValue(newData, kSecValueData, differentdata);
            CFReleaseNull(differentdata);

            CFDictionaryRef query = createQueryCustomItemDictionaryWithService(kc, itemclass, label, service);
            ok_status(SecItemUpdate(query, newData), "%s: SecItemUpdate", name);
            CFReleaseNull(query);
            readPasswordContents(blockItem, CFSTR("differentdata"));

            // delete the item
            ok_status(SecKeychainItemDelete(blockItem), "%s: SecKeychainItemDelete", name);
            CFReleaseNull(blockItem);
            blockItem = checkNCopyFirst(name, createQueryCustomItemDictionaryWithService(kc, itemclass, label, service), 0);

            free(name);
            CFReleaseNull(label);
            CFReleaseNull(service);
            CFReleaseNull(blockItem);
        });
    }

    dispatch_group_wait(g, DISPATCH_TIME_FOREVER);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", testName);
    CFReleaseNull(kc);
}

int kc_20_item_add_stress(int argc, char *const *argv)
{
    plan_tests(( makeItemTests + checkNTests + readPasswordContentsTests + 1 + readPasswordContentsTests + 1 + checkNTests )*BLOCKS + getPopulatedTestKeychainTests + 1);
    initializeKeychainTests(__FUNCTION__);

    tests();

    deleteTestFiles();
    return 0;
}
