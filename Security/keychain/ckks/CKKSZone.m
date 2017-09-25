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
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#endif

#import "CKKSKeychainView.h"
#import "CKKSZone.h"

#include <utilities/debugging.h>

@interface CKKSZone()
#if OCTAGON

@property CKDatabaseOperation<CKKSModifyRecordZonesOperation>* zoneCreationOperation;
@property CKDatabaseOperation<CKKSModifyRecordZonesOperation>* zoneDeletionOperation;
@property CKDatabaseOperation<CKKSModifySubscriptionsOperation>* zoneSubscriptionOperation;

@property bool acceptingNewOperations;
@property NSOperationQueue* operationQueue;
@property NSOperation* accountLoggedInDependency;

@property NSHashTable<NSOperation*>* accountOperations;
#endif
@end

@implementation CKKSZone

#if OCTAGON

- (instancetype)initWithContainer:     (CKContainer*) container
                             zoneName: (NSString*) zoneName
                       accountTracker:(CKKSCKAccountStateTracker*) tracker
 fetchRecordZoneChangesOperationClass: (Class<CKKSFetchRecordZoneChangesOperation>) fetchRecordZoneChangesOperationClass
    modifySubscriptionsOperationClass: (Class<CKKSModifySubscriptionsOperation>) modifySubscriptionsOperationClass
      modifyRecordZonesOperationClass: (Class<CKKSModifyRecordZonesOperation>) modifyRecordZonesOperationClass
                   apsConnectionClass: (Class<CKKSAPSConnection>) apsConnectionClass
{
    if(self = [super init]) {
        _container = container;
        _zoneName = zoneName;
        _accountTracker = tracker;

        _database = [_container privateCloudDatabase];
        _zone = [[CKRecordZone alloc] initWithZoneID: [[CKRecordZoneID alloc] initWithZoneName:zoneName ownerName:CKCurrentUserDefaultName]];

        // Every subclass must set up call beginSetup at least once.
        _accountStatus = CKKSAccountStatusUnknown;
        [self resetSetup];

        _accountOperations = [NSHashTable weakObjectsHashTable];

        _fetchRecordZoneChangesOperationClass = fetchRecordZoneChangesOperationClass;
        _modifySubscriptionsOperationClass = modifySubscriptionsOperationClass;
        _modifyRecordZonesOperationClass = modifyRecordZonesOperationClass;
        _apsConnectionClass = apsConnectionClass;

        _queue = dispatch_queue_create([[NSString stringWithFormat:@"CKKSQueue.%@.zone.%@", container.containerIdentifier, zoneName] UTF8String], DISPATCH_QUEUE_SERIAL);
        _operationQueue = [[NSOperationQueue alloc] init];
        _acceptingNewOperations = true;
    }
    return self;
}

// Initialize this object so that we can call beginSetup again
- (void)resetSetup {
    self.setupStarted = false;
    self.setupComplete = false;

    if([self.zoneSetupOperation isPending]) {
        // Nothing to do here: there's already an existing zoneSetupOperation
    } else {
        self.zoneSetupOperation = [[CKKSGroupOperation alloc] init];
        self.zoneSetupOperation.name = @"zone-setup-operation";
    }

    if([self.accountLoggedInDependency isPending]) {
        // Nothing to do here: there's already an existing accountLoggedInDependency
    } else {
        __weak __typeof(self) weakSelf = self;
        self.accountLoggedInDependency = [NSBlockOperation blockOperationWithBlock:^{
            ckksnotice("ckkszone", weakSelf, "CloudKit account logged in.");
        }];
        self.accountLoggedInDependency.name = @"account-logged-in-dependency";
    }

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


-(void)ckAccountStatusChange: (CKKSAccountStatus)oldStatus to:(CKKSAccountStatus)currentStatus {

    // dispatch this on a serial queue, so we get each transition in order
    [self dispatchSync: ^bool {
        ckksnotice("ckkszone", self, "%@ Received notification of CloudKit account status change, moving from %@ to %@",
                   self.zoneID.zoneName,
                   [CKKSCKAccountStateTracker stringFromAccountStatus: self.accountStatus],
                   [CKKSCKAccountStateTracker stringFromAccountStatus: currentStatus]);
        CKKSAccountStatus oldStatus = self.accountStatus;
        self.accountStatus = currentStatus;

        switch(currentStatus) {
            case CKKSAccountStatusAvailable: {

                ckksinfo("ckkszone", self, "logging in while setup started: %d and complete: %d", self.setupStarted, self.setupComplete);

                // This is only a login if we're not in the middle of setup, and the previous state was not logged in
                if(!(self.setupStarted ^ self.setupComplete) && oldStatus != CKKSAccountStatusAvailable) {
                    [self resetSetup];
                    [self handleCKLogin];
                }

                if(self.accountLoggedInDependency) {
                    [self.operationQueue addOperation:self.accountLoggedInDependency];
                    self.accountLoggedInDependency = nil;
                };
            }
            break;

            case CKKSAccountStatusNoAccount: {
                ckksnotice("ckkszone", self, "Logging out of iCloud. Shutting down.");

                self.accountLoggedInDependency = [NSBlockOperation blockOperationWithBlock:^{
                    ckksnotice("ckkszone", self, "CloudKit account logged in again.");
                }];
                self.accountLoggedInDependency.name = @"account-logged-in-dependency";

                [self.operationQueue cancelAllOperations];
                [self handleCKLogout];

                // now we're in a logged out state. Optimistically prepare for a log in!
                [self resetSetup];
            }
            break;

            case CKKSAccountStatusUnknown: {
                // We really don't expect to receive this as a notification, but, okay!
                ckksnotice("ckkszone", self, "Account status has become undetermined. Pausing for %@", self.zoneID.zoneName);

                self.accountLoggedInDependency = [NSBlockOperation blockOperationWithBlock:^{
                    ckksnotice("ckkszone", self, "CloudKit account restored from 'unknown'.");
                }];
                self.accountLoggedInDependency.name = @"account-logged-in-dependency";

                [self.operationQueue cancelAllOperations];
                [self resetSetup];
            }
            break;
        }

        return true;
    }];
}

- (NSOperation*) createSetupOperation: (bool) zoneCreated zoneSubscribed: (bool) zoneSubscribed {
    if(!SecCKKSIsEnabled()) {
        ckksinfo("ckkszone", self, "Skipping CloudKit registration due to disabled CKKS");
        return nil;
    }

    // If we've already started set up, skip doing it again.
    if(self.setupStarted) {
        ckksinfo("ckkszone", self, "skipping startup: it's already started");
        return self.zoneSetupOperation;
    }

    if(self.zoneSetupOperation == nil) {
        ckkserror("ckkszone", self, "trying to set up but the setup operation is gone; what happened?");
        return nil;
    }

    self.zoneCreated = zoneCreated;
    self.zoneSubscribed = zoneSubscribed;

    // Zone setups and teardowns are due to either 1) first CKKS launch or 2) the user logging in to iCloud.
    // Therefore, they're QoS UserInitiated.
    self.zoneSetupOperation.queuePriority = NSOperationQueuePriorityNormal;
    self.zoneSetupOperation.qualityOfService = NSQualityOfServiceUserInitiated;

    ckksnotice("ckkszone", self, "Setting up zone %@", self.zoneName);
    self.setupStarted = true;

    __weak __typeof(self) weakSelf = self;

    // First, check the account status. If it's sufficient, add the necessary CloudKit operations to this operation
    NSBlockOperation* doSetup = [NSBlockOperation blockOperationWithBlock:^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if(!strongSelf) {
            ckkserror("ckkszone", strongSelf, "received callback for released object");
            return;
        }

        __block bool ret = false;
        [strongSelf dispatchSync: ^bool {
            strongSelf.accountStatus = [strongSelf.accountTracker currentCKAccountStatusAndNotifyOnChange:strongSelf];

            switch(strongSelf.accountStatus) {
                case CKKSAccountStatusNoAccount:
                    ckkserror("ckkszone", strongSelf, "No CloudKit account; quitting setup for %@", strongSelf.zoneID.zoneName);
                    [strongSelf handleCKLogout];
                    ret = true;
                    break;
                case CKKSAccountStatusAvailable:
                    if(strongSelf.accountLoggedInDependency) {
                        [strongSelf.operationQueue addOperation: strongSelf.accountLoggedInDependency];
                        strongSelf.accountLoggedInDependency = nil;
                    }
                    break;
                case CKKSAccountStatusUnknown:
                    ckkserror("ckkszone", strongSelf, "CloudKit account status currently unknown; stopping setup for %@", strongSelf.zoneID.zoneName);
                    ret = true;
                    break;
            }

            return true;
        }];

        NSBlockOperation* setupCompleteOperation = [NSBlockOperation blockOperationWithBlock:^{
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            if(!strongSelf) {
                secerror("ckkszone: received callback for released object");
                return;
            }

            ckksinfo("ckkszone", strongSelf, "%@: Setup complete", strongSelf.zoneName);
            strongSelf.setupComplete = true;
        }];
        setupCompleteOperation.name = @"zone-setup-complete-operation";

        // If we don't have an CloudKit account, don't bother continuing
        if(ret) {
            [strongSelf.zoneSetupOperation runBeforeGroupFinished:setupCompleteOperation];
            return;
        }

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
                [aps register:strongSelf forZoneID:strongSelf.zoneID];
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
                } else {
                    ckkserror("ckkszone", strongSubSelf, "Couldn't create zone %@; %@", strongSubSelf.zoneName, operationError);
                }
                strongSubSelf.zoneCreatedError = operationError;

                [strongSubSelf.operationQueue addOperation: modifyRecordZonesCompleteOperation];
            };

            ckksnotice("ckkszone", strongSelf, "Adding CKKSModifyRecordZonesOperation: %@ %@", zoneCreationOperation, zoneCreationOperation.dependencies);
            strongSelf.zoneCreationOperation = zoneCreationOperation;
            [setupCompleteOperation addDependency: modifyRecordZonesCompleteOperation];
            [strongSelf.zoneSetupOperation runBeforeGroupFinished: zoneCreationOperation];
            [strongSelf.zoneSetupOperation dependOnBeforeGroupFinished: modifyRecordZonesCompleteOperation];
        } else {
            ckksinfo("ckkszone", strongSelf, "no need to create the zone '%@'", strongSelf.zoneName);
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

                [strongSubSelf.operationQueue addOperation: zoneSubscriptionCompleteOperation];
            };

            if(modifyRecordZonesCompleteOperation) {
                [zoneSubscriptionOperation addDependency:modifyRecordZonesCompleteOperation];
            }
            strongSelf.zoneSubscriptionOperation = zoneSubscriptionOperation;
            [setupCompleteOperation addDependency: zoneSubscriptionCompleteOperation];
            [strongSelf.zoneSetupOperation runBeforeGroupFinished:zoneSubscriptionOperation];
            [strongSelf.zoneSetupOperation dependOnBeforeGroupFinished: zoneSubscriptionCompleteOperation];
        } else {
            ckksinfo("ckkszone", strongSelf, "no need to create database subscription");
        }

        [strongSelf.zoneSetupOperation runBeforeGroupFinished:setupCompleteOperation];
    }];
    doSetup.name = @"begin-zone-setup";

    [self.zoneSetupOperation runBeforeGroupFinished:doSetup];

    return self.zoneSetupOperation;
}


- (CKKSResultOperation*)beginResetCloudKitZoneOperation {
    if(!SecCKKSIsEnabled()) {
        ckksinfo("ckkszone", self, "Skipping CloudKit reset due to disabled CKKS");
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

    CKKSResultOperation* doneOp = [CKKSResultOperation named:@"zone-reset-watcher" withBlock:^{}];

    __weak __typeof(self) weakSelf = self;

    zoneDeletionOperation.modifyRecordZonesCompletionBlock = ^(NSArray<CKRecordZone *> *savedRecordZones, NSArray<CKRecordZoneID *> *deletedRecordZoneIDs, NSError *operationError) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if(!strongSelf) {
            ckkserror("ckkszone", strongSelf, "received callback for released object");
            return;
        }

        ckksinfo("ckkszone", strongSelf, "record zones deletion %@ completed with error: %@", deletedRecordZoneIDs, operationError);
        [strongSelf resetSetup];

        doneOp.error = operationError;
        [strongSelf.operationQueue addOperation: doneOp];
    };

    // If the zone creation operation is still pending, wait for it to complete before attempting zone deletion
    [zoneDeletionOperation addNullableDependency: self.zoneCreationOperation];

    ckksinfo("ckkszone", self, "deleting zone with %@ %@", zoneDeletionOperation, zoneDeletionOperation.dependencies);
    // Don't use scheduleOperation: zone deletions should be attempted even if we're "logged out"
    [self.operationQueue addOperation: zoneDeletionOperation];
    self.zoneDeletionOperation = zoneDeletionOperation;
    return doneOp;
}

- (void)notifyZoneChange: (CKRecordZoneNotification*) notification {
    ckksnotice("ckkszone", self, "received a notification for CK zone change, ignoring");
}

- (void)handleCKLogin {
    ckksinfo("ckkszone", self, "received a notification of CK login, ignoring");
}

- (void)handleCKLogout {
    ckksinfo("ckkszone", self, "received a notification of CK logout, ignoring");
}

- (bool)scheduleOperation: (NSOperation*) op {
    if(!self.acceptingNewOperations) {
        ckksdebug("ckkszone", self, "attempted to schedule an operation on a cancelled zone, ignoring");
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
    // Always succeed. But, account status operations should always proceed in-order.
    [op linearDependencies:self.accountOperations];
    [self.operationQueue addOperation: op];
    return true;
}

// to be used rarely, if at all
- (bool)scheduleOperationWithoutDependencies:(NSOperation*)op {
    [self.operationQueue addOperation: op];
    return true;
}

- (void) dispatchSync: (bool (^)(void)) block {
    // important enough to block this thread.
    __block bool ok = false;
    dispatch_sync(self.queue, ^{
        ok = block();
        if(!ok) {
            ckkserror("ckkszone", self, "CKKSZone block returned false");
        }
    });
}


#endif /* OCTAGON */
@end



