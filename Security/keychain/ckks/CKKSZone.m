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

#include <AssertMacros.h>

#import <Foundation/Foundation.h>

#if OCTAGON
#import "CloudKitDependencies.h"
#import "keychain/ckks/CKKSCKAccountStateTracker.h"
#import "keychain/ckks/CloudKitCategories.h"
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import "CKKSKeychainView.h"
#import "CKKSZone.h"

#include <utilities/debugging.h>

@interface CKKSZone()

@property CKDatabaseOperation<CKKSModifyRecordZonesOperation>* zoneCreationOperation;
@property CKDatabaseOperation<CKKSModifyRecordZonesOperation>* zoneDeletionOperation;
@property CKDatabaseOperation<CKKSModifySubscriptionsOperation>* zoneSubscriptionOperation;

@property NSOperationQueue* operationQueue;
@property CKKSResultOperation* accountLoggedInDependency;

@property NSHashTable<NSOperation*>* accountOperations;

// Make writable
@property bool halted;
@property bool zoneCreateNetworkFailure;
@property bool zoneSubscriptionNetworkFailure;
@end

@implementation CKKSZone

- (instancetype)initWithContainer:     (CKContainer*) container
                             zoneName: (NSString*) zoneName
                       accountTracker:(CKKSCKAccountStateTracker*) accountTracker
                  reachabilityTracker:(CKKSReachabilityTracker *) reachabilityTracker
 fetchRecordZoneChangesOperationClass: (Class<CKKSFetchRecordZoneChangesOperation>) fetchRecordZoneChangesOperationClass
           fetchRecordsOperationClass: (Class<CKKSFetchRecordsOperation>)fetchRecordsOperationClass
                  queryOperationClass:(Class<CKKSQueryOperation>)queryOperationClass
    modifySubscriptionsOperationClass: (Class<CKKSModifySubscriptionsOperation>) modifySubscriptionsOperationClass
      modifyRecordZonesOperationClass: (Class<CKKSModifyRecordZonesOperation>) modifyRecordZonesOperationClass
                   apsConnectionClass: (Class<CKKSAPSConnection>) apsConnectionClass
{
    if(self = [super init]) {
        _container = container;
        _zoneName = zoneName;
        _accountTracker = accountTracker;
        _reachabilityTracker = reachabilityTracker;

        _halted = false;

        _database = [_container privateCloudDatabase];
        _zone = [[CKRecordZone alloc] initWithZoneID: [[CKRecordZoneID alloc] initWithZoneName:zoneName ownerName:CKCurrentUserDefaultName]];

        _accountStatus = CKKSAccountStatusUnknown;

        _accountLoggedInDependency = [self createAccountLoggedInDependency:@"CloudKit account logged in."];

        _accountOperations = [NSHashTable weakObjectsHashTable];

        _fetchRecordZoneChangesOperationClass = fetchRecordZoneChangesOperationClass;
        _fetchRecordsOperationClass = fetchRecordsOperationClass;
        _queryOperationClass = queryOperationClass;
        _modifySubscriptionsOperationClass = modifySubscriptionsOperationClass;
        _modifyRecordZonesOperationClass = modifyRecordZonesOperationClass;
        _apsConnectionClass = apsConnectionClass;

        _queue = dispatch_queue_create([[NSString stringWithFormat:@"CKKSQueue.%@.zone.%@", container.containerIdentifier, zoneName] UTF8String], DISPATCH_QUEUE_SERIAL);
        _operationQueue = [[NSOperationQueue alloc] init];
    }
    return self;
}

- (CKKSResultOperation*)createAccountLoggedInDependency:(NSString*)message {
    __weak __typeof(self) weakSelf = self;
    CKKSResultOperation* accountLoggedInDependency = [CKKSResultOperation named:@"account-logged-in-dependency" withBlock:^{
        ckksnotice("ckkszone", weakSelf, "%@", message);
    }];
    accountLoggedInDependency.descriptionErrorCode = CKKSResultDescriptionPendingAccountLoggedIn;
    return accountLoggedInDependency;
}

- (void)initializeZone {
    [self.accountTracker notifyOnAccountStatusChange:self];
}

- (void)resetSetup {
    self.zoneCreated = false;
    self.zoneSubscribed = false;
    self.zoneCreatedError = nil;
    self.zoneSubscribedError = nil;

    self.zoneCreationOperation = nil;
    self.zoneSubscriptionOperation = nil;
    self.zoneDeletionOperation = nil;
}

- (CKRecordZoneID*)zoneID {
    return [self.zone zoneID];
}


-(void)ckAccountStatusChange:(CKKSAccountStatus)oldStatus to:(CKKSAccountStatus)currentStatus {
    ckksnotice("ckkszone", self, "%@ Received notification of CloudKit account status change, moving from %@ to %@",
               self.zoneID.zoneName,
               [CKKSCKAccountStateTracker stringFromAccountStatus: oldStatus],
               [CKKSCKAccountStateTracker stringFromAccountStatus: currentStatus]);

    switch(currentStatus) {
        case CKKSAccountStatusAvailable: {
            ckksnotice("ckkszone", self, "Logged into iCloud.");
            [self handleCKLogin];

            if(self.accountLoggedInDependency) {
                [self.operationQueue addOperation:self.accountLoggedInDependency];
                self.accountLoggedInDependency = nil;
            };
        }
            break;

        case CKKSAccountStatusNoAccount: {
            ckksnotice("ckkszone", self, "Logging out of iCloud. Shutting down.");

            self.accountLoggedInDependency = [self createAccountLoggedInDependency:@"CloudKit account logged in again."];

            [self handleCKLogout];
        }
            break;

        case CKKSAccountStatusUnknown: {
            // We really don't expect to receive this as a notification, but, okay!
            ckksnotice("ckkszone", self, "Account status has become undetermined. Pausing for %@", self.zoneID.zoneName);

            self.accountLoggedInDependency = [self createAccountLoggedInDependency:@"CloudKit account return from 'unknown'."];

            [self handleCKLogout];
        }
            break;
    }
}

- (CKKSResultOperation*)handleCKLogin:(bool)zoneCreated zoneSubscribed:(bool)zoneSubscribed {
    if(!SecCKKSIsEnabled()) {
        ckksinfo("ckkszone", self, "Skipping CloudKit registration due to disabled CKKS");
        return nil;
    }

    // If we've already started set up and that hasn't finished, complain
    if([self.zoneSetupOperation isPending] || [self.zoneSetupOperation isExecuting]) {
        ckksnotice("ckkszone", self, "Asked to handleCKLogin, but zoneSetupOperation appears to not be complete? %@ Continuing anyway", self.zoneSetupOperation);
    }

    self.zoneSetupOperation = [[CKKSGroupOperation alloc] init];
    self.zoneSetupOperation.name = [NSString stringWithFormat:@"zone-setup-operation-%@", self.zoneName];

    self.zoneCreated = zoneCreated;
    self.zoneSubscribed = zoneSubscribed;

    // Zone setups and teardowns are due to either 1) first CKKS launch or 2) the user logging in to iCloud.
    // Therefore, they're QoS UserInitiated.
    self.zoneSetupOperation.queuePriority = NSOperationQueuePriorityNormal;
    self.zoneSetupOperation.qualityOfService = NSQualityOfServiceUserInitiated;

    ckksnotice("ckkszone", self, "Setting up zone %@", self.zoneName);

    __weak __typeof(self) weakSelf = self;

    // First, check the account status. If it's sufficient, add the necessary CloudKit operations to this operation
    __weak CKKSGroupOperation* weakZoneSetupOperation = self.zoneSetupOperation;
    [self.zoneSetupOperation runBeforeGroupFinished:[CKKSResultOperation named:[NSString stringWithFormat:@"zone-setup-%@", self.zoneName] withBlock:^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        __strong __typeof(self.zoneSetupOperation) zoneSetupOperation = weakZoneSetupOperation;
        __strong __typeof(self.reachabilityTracker) reachabilityTracker = self.reachabilityTracker;
        if(!strongSelf || !zoneSetupOperation) {
            ckkserror("ckkszone", strongSelf, "received callback for released object");
            return;
        }

        if(strongSelf.accountStatus != CKKSAccountStatusAvailable) {
            ckkserror("ckkszone", strongSelf, "Zone doesn't believe it's logged in; quitting setup");
            return;
        }

        NSBlockOperation* setupCompleteOperation = [NSBlockOperation blockOperationWithBlock:^{
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            if(!strongSelf) {
                secerror("ckkszone: received callback for released object");
                return;
            }

            ckksnotice("ckkszone", strongSelf, "%@: Setup complete", strongSelf.zoneName);
        }];
        setupCompleteOperation.name = @"zone-setup-complete-operation";

        // We have an account, so fetch the push environment and bring up APS
        [strongSelf.container serverPreferredPushEnvironmentWithCompletionHandler: ^(NSString *apsPushEnvString, NSError *error) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            if(!strongSelf) {
                secerror("ckkszone: received callback for released object");
                return;
            }

            if(error || (apsPushEnvString == nil)) {
                ckkserror("ckkszone", strongSelf, "Received error fetching preferred push environment (%@). Keychain syncing is highly degraded: %@", apsPushEnvString, error);
            } else {
                CKKSAPSReceiver* aps = [CKKSAPSReceiver receiverForEnvironment:apsPushEnvString
                                                             namedDelegatePort:SecCKKSAPSNamedPort
                                                            apsConnectionClass:strongSelf.apsConnectionClass];
                [aps registerReceiver:strongSelf forZoneID:strongSelf.zoneID];
            }
        }];

        NSBlockOperation* modifyRecordZonesCompleteOperation = nil;
        if(!zoneCreated) {
            ckksnotice("ckkszone", strongSelf, "Creating CloudKit zone '%@'", strongSelf.zoneName);
            CKDatabaseOperation<CKKSModifyRecordZonesOperation>* zoneCreationOperation = [[strongSelf.modifyRecordZonesOperationClass alloc] initWithRecordZonesToSave: @[strongSelf.zone] recordZoneIDsToDelete: nil];
            zoneCreationOperation.queuePriority = NSOperationQueuePriorityNormal;
            zoneCreationOperation.qualityOfService = NSQualityOfServiceUserInitiated;
            zoneCreationOperation.database = strongSelf.database;
            zoneCreationOperation.name = @"zone-creation-operation";
            zoneCreationOperation.group = strongSelf.zoneSetupOperationGroup ?: [CKOperationGroup CKKSGroupWithName:@"zone-creation"];;

            // Completion blocks don't count for dependencies. Use this intermediate operation hack instead.
            modifyRecordZonesCompleteOperation = [[NSBlockOperation alloc] init];
            modifyRecordZonesCompleteOperation.name = @"zone-creation-finished";

            zoneCreationOperation.modifyRecordZonesCompletionBlock = ^(NSArray<CKRecordZone *> *savedRecordZones, NSArray<CKRecordZoneID *> *deletedRecordZoneIDs, NSError *operationError) {
                __strong __typeof(weakSelf) strongSelf = weakSelf;
                if(!strongSelf) {
                    secerror("ckkszone: received callback for released object");
                    return;
                }

                __strong __typeof(weakSelf) strongSubSelf = weakSelf;

                if(!operationError) {
                    ckksnotice("ckkszone", strongSubSelf, "Successfully created zone %@", strongSubSelf.zoneName);
                    strongSubSelf.zoneCreated = true;
                    strongSubSelf.zoneSetupOperationGroup = nil;
                } else {
                    ckkserror("ckkszone", strongSubSelf, "Couldn't create zone %@; %@", strongSubSelf.zoneName, operationError);
                }
                strongSubSelf.zoneCreatedError = operationError;
                if ([reachabilityTracker isNetworkError:operationError]){
                    strongSelf.zoneCreateNetworkFailure = true;
                }
                [strongSubSelf.operationQueue addOperation: modifyRecordZonesCompleteOperation];
            };

            if (strongSelf.zoneCreateNetworkFailure) {
                [zoneCreationOperation addNullableDependency:reachabilityTracker.reachablityDependency];
                strongSelf.zoneCreateNetworkFailure = false;
            }
            ckksnotice("ckkszone", strongSelf, "Adding CKKSModifyRecordZonesOperation: %@ %@", zoneCreationOperation, zoneCreationOperation.dependencies);
            strongSelf.zoneCreationOperation = zoneCreationOperation;
            [setupCompleteOperation addDependency: modifyRecordZonesCompleteOperation];
            [zoneSetupOperation runBeforeGroupFinished: zoneCreationOperation];
            [zoneSetupOperation dependOnBeforeGroupFinished: modifyRecordZonesCompleteOperation];
        } else {
            ckksnotice("ckkszone", strongSelf, "no need to create the zone '%@'", strongSelf.zoneName);
        }

        if(!zoneSubscribed) {
            ckksnotice("ckkszone", strongSelf, "Creating CloudKit record zone subscription for %@", strongSelf.zoneName);
            CKRecordZoneSubscription* subscription = [[CKRecordZoneSubscription alloc] initWithZoneID: strongSelf.zoneID subscriptionID:[@"zone:" stringByAppendingString: strongSelf.zoneName]];
            CKNotificationInfo* notificationInfo = [[CKNotificationInfo alloc] init];

            notificationInfo.shouldSendContentAvailable = false;
            subscription.notificationInfo = notificationInfo;

            CKDatabaseOperation<CKKSModifySubscriptionsOperation>* zoneSubscriptionOperation = [[strongSelf.modifySubscriptionsOperationClass alloc] initWithSubscriptionsToSave: @[subscription] subscriptionIDsToDelete: nil];

            zoneSubscriptionOperation.queuePriority = NSOperationQueuePriorityNormal;
            zoneSubscriptionOperation.qualityOfService = NSQualityOfServiceUserInitiated;
            zoneSubscriptionOperation.database = strongSelf.database;
            zoneSubscriptionOperation.name = @"zone-subscription-operation";

            // Completion blocks don't count for dependencies. Use this intermediate operation hack instead.
            NSBlockOperation* zoneSubscriptionCompleteOperation = [[NSBlockOperation alloc] init];
            zoneSubscriptionCompleteOperation.name = @"zone-subscription-complete";
            zoneSubscriptionOperation.modifySubscriptionsCompletionBlock = ^(NSArray<CKSubscription *> * _Nullable savedSubscriptions, NSArray<NSString *> * _Nullable deletedSubscriptionIDs, NSError * _Nullable operationError) {
                __strong __typeof(weakSelf) strongSubSelf = weakSelf;
                if(!strongSubSelf) {
                    ckkserror("ckkszone", strongSubSelf, "received callback for released object");
                    return;
                }

                if(!operationError) {
                    ckksnotice("ckkszone", strongSubSelf, "Successfully subscribed to %@", savedSubscriptions);

                    // Success; write that down. TODO: actually ensure that the saved subscription matches what we asked for
                    for(CKSubscription* sub in savedSubscriptions) {
                        ckksnotice("ckkszone", strongSubSelf, "Successfully subscribed to %@", sub.subscriptionID);
                        strongSubSelf.zoneSubscribed = true;
                    }
                } else {
                    ckkserror("ckkszone", strongSubSelf, "Couldn't create cloudkit zone subscription; keychain syncing is severely degraded: %@", operationError);
                }

                strongSubSelf.zoneSubscribedError = operationError;
                strongSubSelf.zoneSubscriptionOperation = nil;
                if ([reachabilityTracker isNetworkError:operationError]){
                    strongSelf.zoneSubscriptionNetworkFailure = true;
                }

                [strongSubSelf.operationQueue addOperation: zoneSubscriptionCompleteOperation];
            };

            if (strongSelf.zoneSubscriptionNetworkFailure) {
                [zoneSubscriptionOperation addNullableDependency:reachabilityTracker.reachablityDependency];
                strongSelf.zoneSubscriptionNetworkFailure = false;
            }
            [zoneSubscriptionOperation addNullableDependency:modifyRecordZonesCompleteOperation];
            strongSelf.zoneSubscriptionOperation = zoneSubscriptionOperation;
            [setupCompleteOperation addDependency: zoneSubscriptionCompleteOperation];
            [zoneSetupOperation runBeforeGroupFinished:zoneSubscriptionOperation];
            [zoneSetupOperation dependOnBeforeGroupFinished: zoneSubscriptionCompleteOperation];
        } else {
            ckksnotice("ckkszone", strongSelf, "no need to create database subscription");
        }

        [strongSelf.zoneSetupOperation runBeforeGroupFinished:setupCompleteOperation];
    }]];

    [self scheduleAccountStatusOperation:self.zoneSetupOperation];
    return self.zoneSetupOperation;
}


- (CKKSResultOperation*)deleteCloudKitZoneOperation:(CKOperationGroup* _Nullable)ckoperationGroup {
    if(!SecCKKSIsEnabled()) {
        ckksnotice("ckkszone", self, "Skipping CloudKit reset due to disabled CKKS");
        return nil;
    }

    // We want to delete this zone and this subscription from CloudKit.

    // Step 1: cancel setup operations (if they exist)
    [self.accountLoggedInDependency cancel];
    [self.zoneSetupOperation cancel];
    [self.zoneCreationOperation cancel];
    [self.zoneSubscriptionOperation cancel];

    // Step 2: Try to delete the zone

    CKDatabaseOperation<CKKSModifyRecordZonesOperation>* zoneDeletionOperation = [[self.modifyRecordZonesOperationClass alloc] initWithRecordZonesToSave: nil recordZoneIDsToDelete: @[self.zoneID]];
    zoneDeletionOperation.queuePriority = NSOperationQueuePriorityNormal;
    zoneDeletionOperation.qualityOfService = NSQualityOfServiceUserInitiated;
    zoneDeletionOperation.database = self.database;
    zoneDeletionOperation.group = ckoperationGroup;

    CKKSGroupOperation* zoneDeletionGroupOperation = [[CKKSGroupOperation alloc] init];
    zoneDeletionGroupOperation.name = [NSString stringWithFormat:@"cloudkit-zone-delete-%@", self.zoneName];

    CKKSResultOperation* doneOp = [CKKSResultOperation named:@"zone-reset-watcher" withBlock:^{}];
    [zoneDeletionGroupOperation dependOnBeforeGroupFinished:doneOp];

    __weak __typeof(self) weakSelf = self;

    zoneDeletionOperation.modifyRecordZonesCompletionBlock = ^(NSArray<CKRecordZone *> *savedRecordZones, NSArray<CKRecordZoneID *> *deletedRecordZoneIDs, NSError *operationError) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if(!strongSelf) {
            ckkserror("ckkszone", strongSelf, "received callback for released object");
            return;
        }

        bool fatalError = false;
        if(operationError) {
            // Okay, but if this error is either 'ZoneNotFound' or 'UserDeletedZone', that's fine by us: the zone is deleted.
            NSDictionary* partialErrors = operationError.userInfo[CKPartialErrorsByItemIDKey];
            if([operationError.domain isEqualToString:CKErrorDomain] && operationError.code == CKErrorPartialFailure && partialErrors) {
                for(CKRecordZoneID* errorZoneID in partialErrors.allKeys) {
                    NSError* errorZone = partialErrors[errorZoneID];

                    if(errorZone && [errorZone.domain isEqualToString:CKErrorDomain] &&
                       (errorZone.code == CKErrorZoneNotFound || errorZone.code == CKErrorUserDeletedZone)) {
                        ckksnotice("ckkszone", strongSelf, "Attempted to delete zone %@, but it's already missing. This is okay: %@", errorZoneID, errorZone);
                    } else {
                        fatalError = true;
                    }
                }

            } else {
                fatalError = true;
            }
        }

        if(operationError) {
            ckksnotice("ckkszone", strongSelf, "deletion of record zones  %@ completed with error: %@", deletedRecordZoneIDs, operationError);
        } else {
            ckksnotice("ckkszone", strongSelf, "deletion of record zones  %@ completed successfully", deletedRecordZoneIDs);
        }

        if(operationError && fatalError) {
            // If the error wasn't actually a problem, don't report it upward.
            doneOp.error = operationError;
        }
        [zoneDeletionGroupOperation runBeforeGroupFinished:doneOp];
    };

    // If the zone creation operation is still pending, wait for it to complete before attempting zone deletion
    [zoneDeletionOperation addNullableDependency: self.zoneCreationOperation];
    [zoneDeletionGroupOperation runBeforeGroupFinished:zoneDeletionOperation];

    [zoneDeletionGroupOperation runBeforeGroupFinished:[CKKSResultOperation named:@"print-log-message" withBlock:^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        ckksnotice("ckkszone", strongSelf, "deleting zones %@ with dependencies %@", zoneDeletionOperation.recordZoneIDsToDelete, zoneDeletionOperation.dependencies);
    }]];
    return zoneDeletionGroupOperation;
}

- (void)notifyZoneChange: (CKRecordZoneNotification*) notification {
    ckksnotice("ckkszone", self, "received a notification for CK zone change, ignoring");
}

- (void)handleCKLogin {
    ckksinfo("ckkszone", self, "received a notification of CK login");
    self.accountStatus = CKKSAccountStatusAvailable;
}

- (void)handleCKLogout {
    ckksinfo("ckkszone", self, "received a notification of CK logout");
    self.accountStatus = CKKSAccountStatusNoAccount;
    [self resetSetup];
}

- (bool)scheduleOperation: (NSOperation*) op {
    if(self.halted) {
        ckkserror("ckkszone", self, "attempted to schedule an operation on a halted zone, ignoring");
        return false;
    }

    if(self.accountLoggedInDependency) {
        [op addDependency: self.accountLoggedInDependency];
    }

    [self.operationQueue addOperation: op];
    return true;
}

- (void)cancelAllOperations {
    [self.operationQueue cancelAllOperations];
}

- (void)waitUntilAllOperationsAreFinished {
    [self.operationQueue waitUntilAllOperationsAreFinished];
}

- (void)waitForOperationsOfClass:(Class) operationClass {
    NSArray* operations = [self.operationQueue.operations copy];
    for(NSOperation* op in operations) {
        if([op isKindOfClass:operationClass]) {
            [op waitUntilFinished];
        }
    }
}

- (bool)scheduleAccountStatusOperation: (NSOperation*) op {
    if(self.halted) {
        ckkserror("ckkszone", self, "attempted to schedule an account operation on a halted zone, ignoring");
        return false;
    }

    // Always succeed. But, account status operations should always proceed in-order.
    [op linearDependencies:self.accountOperations];
    [self.operationQueue addOperation: op];
    return true;
}

// to be used rarely, if at all
- (bool)scheduleOperationWithoutDependencies:(NSOperation*)op {
    if(self.halted) {
        ckkserror("ckkszone", self, "attempted to schedule an non-dependent operation on a halted zone, ignoring");
        return false;
    }

    [self.operationQueue addOperation: op];
    return true;
}

- (void) dispatchSync: (bool (^)(void)) block {
    // important enough to block this thread.
    __block bool ok = false;
    dispatch_sync(self.queue, ^{
        if(self.halted) {
            ckkserror("ckkszone", self, "CKKSZone not dispatchSyncing a block (due to being halted)");
            return;
        }

        ok = block();
        if(!ok) {
            ckkserror("ckkszone", self, "CKKSZone block returned false");
        }
    });
}

- (void)halt {
    // Synchronously set the 'halted' bit
    dispatch_sync(self.queue, ^{
        self.halted = true;
    });

    // Bring all operations down, too
    [self cancelAllOperations];
}

@end

#endif /* OCTAGON */

