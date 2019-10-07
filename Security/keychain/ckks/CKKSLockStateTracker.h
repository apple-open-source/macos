/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>

@protocol CKKSLockStateNotification <NSObject>
- (void)lockStateChangeNotification:(bool)unlocked;
@end

NS_ASSUME_NONNULL_BEGIN

@interface CKKSLockStateTracker : NSObject
@property (nullable) NSOperation* unlockDependency;
@property (readonly) bool isLocked;

@property (readonly,nullable) NSDate* lastUnlockTime;

- (instancetype)init;

// Force a recheck of the keybag lock state
- (void)recheck;

// Check if this error code is related to keybag is locked and we should retry later
- (bool)isLockedError:(NSError*)error;

-(void)addLockStateObserver:(id<CKKSLockStateNotification>)object;

// Ask AKS if the user's keybag is locked
+ (bool)queryAKSLocked;

// Call this to get a CKKSLockStateTracker to use. This tracker will likely be tracking real AKS.
+ (CKKSLockStateTracker*)globalTracker;
@end

NS_ASSUME_NONNULL_END
#endif  // OCTAGON
