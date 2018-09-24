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

#import <Foundation/Foundation.h>

#if OCTAGON
#import <CloudKit/CKContainer_Private.h>
#import <CloudKit/CloudKit.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#import "keychain/ckks/CKKSCondition.h"
#import "keychain/ckks/CloudKitDependencies.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * Implements a 'debouncer' to store the current CK account and circle state, and receive updates to it.
 *
 * Will only be considered "logged in" if we both have a CK account and are 'in circle'.
 *
 * It will notify listeners on account state changes, so multiple repeated account state notifications with the same state are filtered by this class.
 * Listeners can also get the 'current' state, no matter what it is. They will also then be atomically added to the notification queue, and so will
 * always receive the next update, preventing them from getting a stale state and missing an immediate update.
 */

typedef NS_ENUM(NSInteger, CKKSAccountStatus) {
    /* Set at initialization. This means we haven't figured out what the account state is. */
    CKKSAccountStatusUnknown = 0,
    /* We have an iCloud account and are in-circle */
    CKKSAccountStatusAvailable = 1,
    /* No iCloud account is logged in on this device, or we're out of circle */
    CKKSAccountStatusNoAccount = 3,
};

@interface SOSAccountStatus : NSObject
@property SOSCCStatus status;
@property (nullable) NSError* error;
- (instancetype)init:(SOSCCStatus)status error:error;
@end

@protocol CKKSAccountStateListener <NSObject>
- (void)ckAccountStatusChange:(CKKSAccountStatus)oldStatus to:(CKKSAccountStatus)currentStatus;
@end

@interface CKKSCKAccountStateTracker : NSObject
@property CKKSCondition* finishedInitialDispatches;

// If you use these, please be aware they could change out from under you at any time
@property (nullable) CKAccountInfo* currentCKAccountInfo;
@property (nullable) SOSAccountStatus* currentCircleStatus;

@property (readonly,atomic) CKKSAccountStatus currentComputedAccountStatus;
@property (nullable,readonly,atomic) NSError* currentAccountError;
@property CKKSCondition* currentComputedAccountStatusValid;

// Fetched and memoized from CloudKit; we can't afford deadlocks with their callbacks
@property (nullable, copy) NSString* ckdeviceID;
@property (nullable) NSError* ckdeviceIDError;
@property CKKSCondition* ckdeviceIDInitialized;

// Fetched and memoized from the Account when we're in-circle; our threading model is strange
@property (nullable) NSString* accountCirclePeerID;
@property (nullable) NSError* accountCirclePeerIDError;
@property CKKSCondition* accountCirclePeerIDInitialized;

- (instancetype)init:(CKContainer*)container nsnotificationCenterClass:(Class<CKKSNSNotificationCenter>)nsnotificationCenterClass;

- (dispatch_semaphore_t)notifyOnAccountStatusChange:(id<CKKSAccountStateListener>)listener;

// Methods useful for testing:

// Call this to simulate a notification (and pause the calling thread until all notifications are delivered)
- (void)notifyCKAccountStatusChangeAndWaitForSignal;
- (void)notifyCircleStatusChangeAndWaitForSignal;

- (dispatch_group_t _Nullable)checkForAllDeliveries;

+ (SOSAccountStatus*)getCircleStatus;
+ (void)fetchCirclePeerID:(void (^)(NSString* _Nullable peerID, NSError* _Nullable error))callback;
+ (NSString*)stringFromAccountStatus:(CKKSAccountStatus)status;

@end

NS_ASSUME_NONNULL_END
#endif  // OCTAGON
