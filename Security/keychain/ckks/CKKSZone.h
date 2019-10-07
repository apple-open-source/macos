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

#import <Foundation/Foundation.h>

#if OCTAGON
#import "keychain/ckks/CKKSAccountStateTracker.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import "keychain/ckks/CloudKitDependencies.h"
#import "keychain/ckks/CKKSCloudKitClassDependencies.h"
#import "keychain/ckks/OctagonAPSReceiver.h"
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSZoneModifier.h"

NS_ASSUME_NONNULL_BEGIN

@interface CKKSZone : NSObject <CKKSZoneUpdateReceiver, CKKSCloudKitAccountStateListener>
{
    CKContainer* _container;
    CKDatabase* _database;
    CKRecordZone* _zone;
}

@property (readonly) NSString* zoneName;

@property CKKSGroupOperation* zoneSetupOperation;
@property (nullable) CKOperationGroup* zoneSetupOperationGroup; // set this if you want zone creates to use a different operation group

@property bool zoneCreated;
@property bool zoneSubscribed;
@property (nullable) NSError* zoneCreatedError;
@property (nullable) NSError* zoneSubscribedError;

// True if this zone object has been halted. Halted zones will never recover.
@property (readonly) bool halted;

@property CKKSAccountStatus accountStatus;

@property (readonly) CKContainer* container;
@property (readonly) CKDatabase* database;

@property (weak) CKKSAccountStateTracker* accountTracker;
@property (weak) CKKSReachabilityTracker* reachabilityTracker;

@property (readonly) CKRecordZone* zone;
@property (readonly) CKRecordZoneID* zoneID;

@property (readonly) CKKSZoneModifier* zoneModifier;

// Dependencies (for injection)
@property (readonly) CKKSCloudKitClassDependencies* cloudKitClassDependencies;

@property dispatch_queue_t queue;

- (instancetype)initWithContainer:(CKContainer*)container
                         zoneName:(NSString*)zoneName
                   accountTracker:(CKKSAccountStateTracker*)accountTracker
              reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
                     zoneModifier:(CKKSZoneModifier*)zoneModifier
        cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies;


- (CKKSResultOperation* _Nullable)deleteCloudKitZoneOperation:(CKOperationGroup* _Nullable)ckoperationGroup;

// Called when CloudKit notifies us that we just logged in.
// That is, if we transition from any state to CKAccountStatusAvailable.
// This will be called under the protection of dispatchSync.
// This is a no-op; you should intercept this call and call handleCKLogin:zoneSubscribed:
// with the appropriate state
- (void)handleCKLogin;

// Actually start a cloudkit login. Pass in whether you believe this zone has been created and if this device has
// subscribed to this zone on the server.
- (CKKSResultOperation* _Nullable)handleCKLogin:(bool)zoneCreated zoneSubscribed:(bool)zoneSubscribed;

// Called when CloudKit notifies us that we just logged out.
// i.e. we transition from CKAccountStatusAvailable to any other state.
// This will be called under the protection of dispatchSync
- (void)handleCKLogout;

// Call this when you're ready for this zone to kick off operations
// based on iCloud account status
- (void)beginCloudKitOperation;

// Cancels all operations (no matter what they are).
- (void)cancelAllOperations;

// Schedules this operation for execution (if the CloudKit account exists)
- (bool)scheduleOperation:(NSOperation*)op;

// Use this to schedule an operation handling account status (cleaning up after logout, etc.).
- (bool)scheduleAccountStatusOperation:(NSOperation*)op;

// Schedules this operation for execution, and doesn't do any dependency magic
// This should _only_ be used if you want to run something even if the CloudKit account is logged out
- (bool)scheduleOperationWithoutDependencies:(NSOperation*)op;

// Use this for testing.
- (void)waitUntilAllOperationsAreFinished;

// Use this for testing, to only wait for a certain type of operation to finish.
- (void)waitForOperationsOfClass:(Class)operationClass;

// If this object wants to do anything that needs synchronization, use this.
// If this object has had -halt called, this block will never fire.
- (void)dispatchSync:(bool (^)(void))block;

// Call this to halt everything this zone is doing. This object will never recover. Use for testing.
- (void)halt;

// Call this to reset this object's setup, so you can call createSetupOperation again.
- (void)resetSetup;

@end

NS_ASSUME_NONNULL_END
#endif  // OCTAGON
