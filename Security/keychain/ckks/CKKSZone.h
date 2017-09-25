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
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef CKKSZone_h
#define CKKSZone_h

#import <Foundation/Foundation.h>

#if OCTAGON
#import "keychain/ckks/CloudKitDependencies.h"
#import "keychain/ckks/CKKSCKAccountStateTracker.h"
#endif

#if OCTAGON
@interface CKKSZone : NSObject<CKKSZoneUpdateReceiver, CKKSAccountStateListener> {
    CKContainer* _container;
    CKDatabase* _database;
    CKRecordZone* _zone;
}
#else
@interface CKKSZone : NSObject {
}
#endif

@property (readonly) NSString* zoneName;

@property bool setupStarted;
@property bool setupComplete;
@property CKKSGroupOperation* zoneSetupOperation;

@property bool zoneCreated;
@property bool zoneSubscribed;
@property NSError* zoneCreatedError;
@property NSError* zoneSubscribedError;

#if OCTAGON
@property CKKSAccountStatus accountStatus;

@property (readonly) CKContainer* container;
@property (readonly) CKDatabase* database;

@property (weak) CKKSCKAccountStateTracker* accountTracker;

@property (readonly) CKRecordZone* zone;
@property (readonly) CKRecordZoneID* zoneID;

// Dependencies (for injection)
@property (readonly) Class<CKKSFetchRecordZoneChangesOperation> fetchRecordZoneChangesOperationClass;
@property (readonly) Class<CKKSModifySubscriptionsOperation> modifySubscriptionsOperationClass;
@property (readonly) Class<CKKSModifyRecordZonesOperation> modifyRecordZonesOperationClass;
@property (readonly) Class<CKKSAPSConnection> apsConnectionClass;

@property dispatch_queue_t queue;

- (instancetype)initWithContainer:     (CKContainer*) container
                             zoneName: (NSString*) zoneName
                       accountTracker:(CKKSCKAccountStateTracker*) tracker
 fetchRecordZoneChangesOperationClass: (Class<CKKSFetchRecordZoneChangesOperation>) fetchRecordZoneChangesOperationClass
    modifySubscriptionsOperationClass: (Class<CKKSModifySubscriptionsOperation>) modifySubscriptionsOperationClass
      modifyRecordZonesOperationClass: (Class<CKKSModifyRecordZonesOperation>) modifyRecordZonesOperationClass
                   apsConnectionClass: (Class<CKKSAPSConnection>) apsConnectionClass;


- (NSOperation*) createSetupOperation: (bool) zoneCreated zoneSubscribed: (bool) zoneSubscribed;

- (CKKSResultOperation*) beginResetCloudKitZoneOperation;

// Called when CloudKit notifies us that we just logged in.
// That is, if we transition from any state to CKAccountStatusAvailable.
// This will be called under the protection of dispatchSync
- (void)handleCKLogin;

// Called when CloudKit notifies us that we just logged out.
// i.e. we transition from CKAccountStatusAvailable to any other state.
// This will be called under the protection of dispatchSync
- (void)handleCKLogout;

// Cancels all operations (no matter what they are).
- (void)cancelAllOperations;

// Schedules this operation for execution (if the CloudKit account exists)
- (bool)scheduleOperation: (NSOperation*) op;

// Use this to schedule an operation handling account status (cleaning up after logout, etc.).
- (bool)scheduleAccountStatusOperation: (NSOperation*) op;

// Schedules this operation for execution, and doesn't do any dependency magic
// This should _only_ be used if you want to run something even if the CloudKit account is logged out
- (bool)scheduleOperationWithoutDependencies:(NSOperation*)op;

// Use this for testing.
- (void)waitUntilAllOperationsAreFinished;

// Use this for testing, to only wait for a certain type of operation to finish.
- (void)waitForOperationsOfClass:(Class) operationClass;

// If this object wants to do anything that needs synchronization, use this.
- (void) dispatchSync: (bool (^)(void)) block;

// Call this to reset this object's setup, so you can call createSetupOperation again.
- (void)resetSetup;

#endif
@end

#endif /* CKKSZone_h */
