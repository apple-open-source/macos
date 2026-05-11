/*
 * Copyright (c) 2026 Apple Inc. All Rights Reserved.
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

#if OCTAGON

#import <TargetConditionals.h>
#import <Foundation/Foundation.h>

#if !TARGET_OS_SIMULATOR
#import <AppleKeyStore/AppleKeyStore.h>
#else
typedef enum {
    kAKSFirstUnlockEvent,
    kAKSLockStateChangeEvent,
    kAKSMementoEffacedEvent,
    kAKSBackgroundPoliciesInvalidated,
    kAKSPasscodeThresholdMessage,
    kAKSInactivityReboot,
    kAKSCacheFlowEnabled,
} AKSEventType;
extern const CFStringRef kAKSInfoCacheFlowContext;
typedef struct _AKSEvent AKSEvent;
#endif

@protocol OTAppleKeyStoreAdapter

- (AKSEvent*)eventsRegister:(dispatch_queue_t)queue callback:(void (^)(AKSEventType, CFDictionaryRef))callback;
- (void)eventsUnregister:(AKSEvent*)ref;

@end

@interface OTAppleKeyStoreActualAdapter : NSObject <OTAppleKeyStoreAdapter>
@end

#endif // OCTAGON
