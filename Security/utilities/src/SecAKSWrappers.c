/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
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

//
//  SecAKSWrappers.c
//  utilities
//

#include <utilities/SecAKSWrappers.h>
#include <utilities/SecCFWrappers.h>

#if TARGET_IPHONE_SIMULATOR
#  define change_notification "com.apple.will.never.happen"
#elif TARGET_OS_IPHONE
#  include <MobileKeyBag/MobileKeyBag.h>
#  define change_notification kMobileKeyBagLockStatusNotificationID
#elif TARGET_OS_MAC
#  include <AppleKeyStoreEvents.h>
#  define change_notification kAppleKeyStoreLockStatusNotificationID
#else
#  error "unsupported target platform"
#endif

const char * const kUserKeybagStateChangeNotification = change_notification;

bool SecAKSDoWhileUserBagLocked(CFErrorRef *error, dispatch_block_t action)
{
#if TARGET_IPHONE_SIMULATOR
    action();
    return true;
#else
    // Acquire lock assertion, ref count?
    
    __block kern_return_t status = kIOReturnSuccess;
    static dispatch_once_t queue_once;
    static dispatch_queue_t assertion_queue;
    
#if TARGET_OS_MAC && !TARGET_OS_EMBEDDED                            // OS X
    AKSAssertionType_t lockAssertType = kAKSAssertTypeOther;
    keybag_handle_t keybagHandle = session_keybag_handle;
#else                                                               // iOS, but not simulator
    AKSAssertionType_t lockAssertType = kAKSAssertTypeProfile;      // Profile supports timeouts, but only available on iOS
    keybag_handle_t keybagHandle = device_keybag_handle;
#endif
    
    dispatch_once(&queue_once, ^{
        assertion_queue = dispatch_queue_create("AKS Lock Assertion Queue", NULL);
    });
    
    static uint32_t count = 0;
    
    dispatch_sync(assertion_queue, ^{
        if (count == 0) {
            uint64_t timeout = 60ull;
            secnotice("lockassertions", "Requesting lock assertion for %lld seconds", timeout);
            status = aks_assert_hold(keybagHandle, lockAssertType, timeout);
        }
        
        if (status == kIOReturnSuccess)
            ++count;
    });
    
    if (status == kIOReturnSuccess) {
        action();
        dispatch_sync(assertion_queue, ^{
            if (count && (--count == 0)) {
                secnotice("lockassertions", "Dropping lock assertion");
                status = aks_assert_drop(keybagHandle, lockAssertType);
            }
        });
    }
    return SecKernError(status, error, CFSTR("Kern return error"));
#endif  /* !TARGET_IPHONE_SIMULATOR */
}
