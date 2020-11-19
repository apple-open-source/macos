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
#import <CloudKit/CloudKit.h>
#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKSResultOperation.h"
#import "keychain/ckks/CKKSFetchAllRecordZoneChangesOperation.h"
#import "keychain/ckks/OctagonAPSReceiver.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * This class implements a CloudKit-fetch-with-retry.
 * In the case of network or other failures, it'll issue retries.
 * Only in the case of a clean fetch will its operation dependency resolve.
 */

@class CKKSKeychainView;
@class CKKSReachabilityTracker;
@class CKKSNearFutureScheduler;

@interface CKKSZoneChangeFetcher : NSObject <CKKSZoneUpdateReceiverProtocol>
@property (readonly) Class<CKKSFetchRecordZoneChangesOperation> fetchRecordZoneChangesOperationClass;
@property (readonly) CKContainer* container;
@property CKKSReachabilityTracker* reachabilityTracker;

@property (readonly) NSError* lastCKFetchError;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithContainer:(CKContainer*)container
                       fetchClass:(Class<CKKSFetchRecordZoneChangesOperation>)fetchRecordZoneChangesOperationClass
              reachabilityTracker:(CKKSReachabilityTracker *)reachabilityTracker;

- (void)registerClient:(id<CKKSChangeFetcherClient>)client;

- (CKKSResultOperation*)requestSuccessfulFetch:(CKKSFetchBecause*)why;
- (CKKSResultOperation*)requestSuccessfulFetchForManyReasons:(NSSet<CKKSFetchBecause*>*)why;

// Returns the next fetch, if one is scheduled, or the last/currently executing fetch if not.
- (CKKSResultOperation* _Nullable)inflightFetch;

// CKKSZoneUpdateReceiverProtocol
- (void)notifyZoneChange:(CKRecordZoneNotification* _Nullable)notification;

// We don't particularly care what this does, as long as it finishes
- (void)holdFetchesUntil:(CKKSResultOperation* _Nullable)holdOperation;

- (void)cancel;

// I don't recommend using these unless you're a test.
@property CKKSNearFutureScheduler* fetchScheduler;
@end

NS_ASSUME_NONNULL_END
#endif
