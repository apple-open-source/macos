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
#import "keychain/ckks/CKKSAccountStateTracker.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import "keychain/ot/ObjCImprovements.h"

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
@end

@implementation CKKSZone

- (instancetype)initWithContainer:(CKContainer*)container
                         zoneName:(NSString*)zoneName
                   accountTracker:(CKKSAccountStateTracker*)accountTracker
              reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
                     zoneModifier:(CKKSZoneModifier*)zoneModifier
        cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies
{
    if(self = [super init]) {
        _container = container;
        _zoneName = zoneName;
        _accountTracker = accountTracker;
        _reachabilityTracker = reachabilityTracker;

        _zoneModifier = zoneModifier;

        _halted = false;

        _database = [_container privateCloudDatabase];
        _zone = [[CKRecordZone alloc] initWithZoneID: [[CKRecordZoneID alloc] initWithZoneName:zoneName ownerName:CKCurrentUserDefaultName]];

        _accountStatus = CKKSAccountStatusUnknown;

        _accountLoggedInDependency = [self createAccountLoggedInDependency:@"CloudKit account logged in."];

        _accountOperations = [NSHashTable weakObjectsHashTable];

        _cloudKitClassDependencies = cloudKitClassDependencies;

        _queue = dispatch_queue_create([[NSString stringWithFormat:@"CKKSQueue.%@.zone.%@", container.containerIdentifier, zoneName] UTF8String], DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _operationQueue = [[NSOperationQueue alloc] init];
    }
    return self;
}

- (CKKSResultOperation*)createAccountLoggedInDependency:(NSString*)message {
    WEAKIFY(self);
    CKKSResultOperation* accountLoggedInDependency = [CKKSResultOperation named:@"account-logged-in-dependency" withBlock:^{
        STRONGIFY(self);
        ckksnotice("ckkszone", self, "%@", message);
    }];
    accountLoggedInDependency.descriptionErrorCode = CKKSResultDescriptionPendingAccountLoggedIn;
    return accountLoggedInDependency;
}

- (void)beginCloudKitOperation {
    [self.accountTracker registerForNotificationsOfCloudKitAccountStatusChange:self];
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

- (CKKSAccountStatus)accountStatusFromCKAccountInfo:(CKAccountInfo*)info
{
    if(!info) {
        return CKKSAccountStatusUnknown;
    }
    if(info.accountStatus == CKAccountStatusAvailable &&
       info.hasValidCredentials) {
        return CKKSAccountStatusAvailable;
    } else {
        return CKKSAccountStatusNoAccount;
    }
}


- (void)cloudkitAccountStateChange:(CKAccountInfo* _Nullable)oldAccountInfo to:(CKAccountInfo*)currentAccountInfo
{
    ckksnotice("ckkszone", self, "%@ Received notification of CloudKit account status change, moving from %@ to %@",
               self.zoneID.zoneName,
               oldAccountInfo,
               currentAccountInfo);

    // Filter for device2device encryption and cloudkit grey mode
    CKKSAccountStatus oldStatus = [self accountStatusFromCKAccountInfo:oldAccountInfo];
    CKKSAccountStatus currentStatus = [self accountStatusFromCKAccountInfo:currentAccountInfo];

    if(oldStatus == currentStatus) {
        ckksnotice("ckkszone", self, "Computed status of new CK account info is same as old status: %@", [CKKSAccountStateTracker stringFromAccountStatus:currentStatus]);
        return;
    }

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

            if(!self.accountLoggedInDependency) {
                self.accountLoggedInDependency = [self createAccountLoggedInDependency:@"CloudKit account logged in again."];
            }

            [self handleCKLogout];
        }
            break;

        case CKKSAccountStatusUnknown: {
            // We really don't expect to receive this as a notification, but, okay!
            ckksnotice("ckkszone", self, "Account status has become undetermined. Pausing for %@", self.zoneID.zoneName);

            if(!self.accountLoggedInDependency) {
                self.accountLoggedInDependency = [self createAccountLoggedInDependency:@"CloudKit account logged in again."];
            }

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

    ckksnotice("ckkszone", self, "Setting up zone %@", self.zoneName);

    WEAKIFY(self);

    // First, check the account status. If it's sufficient, add the necessary CloudKit operations to this operation
    __weak CKKSGroupOperation* weakZoneSetupOperation = self.zoneSetupOperation;
    [self.zoneSetupOperation runBeforeGroupFinished:[CKKSResultOperation named:[NSString stringWithFormat:@"zone-setup-%@", self.zoneName] withBlock:^{
        STRONGIFY(self);
        __strong __typeof(self.zoneSetupOperation) zoneSetupOperation = weakZoneSetupOperation;
        if(!self || !zoneSetupOperation) {
            ckkserror("ckkszone", self, "received callback for released object");
            return;
        }

        if(self.accountStatus != CKKSAccountStatusAvailable) {
            ckkserror("ckkszone", self, "Zone doesn't believe it's logged in; quitting setup");
            return;
        }

        NSBlockOperation* setupCompleteOperation = [NSBlockOperation blockOperationWithBlock:^{
            STRONGIFY(self);
            if(!self) {
                secerror("ckkszone: received callback for released object");
                return;
            }

            ckksnotice("ckkszone", self, "%@: Setup complete", self.zoneName);
        }];
        setupCompleteOperation.name = @"zone-setup-complete-operation";

        // We have an account, so fetch the push environment and bring up APS
        [self.container serverPreferredPushEnvironmentWithCompletionHandler: ^(NSString *apsPushEnvString, NSError *error) {
            STRONGIFY(self);
            if(!self) {
                secerror("ckkszone: received callback for released object");
                return;
            }

            if(error || (apsPushEnvString == nil)) {
                ckkserror("ckkszone", self, "Received error fetching preferred push environment (%@). Keychain syncing is highly degraded: %@", apsPushEnvString, error);
            } else {
                OctagonAPSReceiver* aps = [OctagonAPSReceiver receiverForEnvironment:apsPushEnvString
                                                             namedDelegatePort:SecCKKSAPSNamedPort
                                                            apsConnectionClass:self.cloudKitClassDependencies.apsConnectionClass];
                [aps registerReceiver:self forZoneID:self.zoneID];
            }
        }];

        if(!zoneCreated || !zoneSubscribed) {
            ckksnotice("ckkszone", self, "Asking to create and subscribe to CloudKit zone '%@'", self.zoneName);
            CKKSZoneModifyOperations* zoneOps = [self.zoneModifier createZone:self.zone];

            CKKSResultOperation* handleModificationsOperation = [CKKSResultOperation named:@"handle-modification" withBlock:^{
                STRONGIFY(self);
                if([zoneOps.savedRecordZones containsObject:self.zone]) {
                    ckksnotice("ckkszone", self, "Successfully created '%@'", self.zoneName);
                    self.zoneCreated = true;
                } else {
                    ckksnotice("ckkszone", self, "Failed to create '%@'", self.zoneName);
                    self.zoneCreatedError = zoneOps.zoneModificationOperation.error;
                }

                bool createdSubscription = false;
                for(CKSubscription* subscription in zoneOps.savedSubscriptions) {
                    if([subscription.zoneID isEqual:self.zoneID]) {
                        createdSubscription = true;
                        break;
                    }
                }

                if(createdSubscription) {
                    ckksnotice("ckkszone", self, "Successfully subscribed '%@'", self.zoneName);
                    self.zoneSubscribed = true;
                } else {
                    ckksnotice("ckkszone", self, "Failed to subscribe to '%@'", self.zoneName);
                    self.zoneSubscribedError = zoneOps.zoneSubscriptionOperation.error;
                }
            }];
            [setupCompleteOperation addDependency:zoneOps.zoneModificationOperation];
            [handleModificationsOperation addDependency:zoneOps.zoneModificationOperation];
            [handleModificationsOperation addDependency:zoneOps.zoneSubscriptionOperation];
            [zoneSetupOperation runBeforeGroupFinished:handleModificationsOperation];
        } else {
            ckksnotice("ckkszone", self, "no need to create or subscribe to the zone '%@'", self.zoneName);
        }

        [self.zoneSetupOperation runBeforeGroupFinished:setupCompleteOperation];
    }]];

    [self scheduleAccountStatusOperation:self.zoneSetupOperation];
    return self.zoneSetupOperation;
}


- (CKKSResultOperation*)deleteCloudKitZoneOperation:(CKOperationGroup* _Nullable)ckoperationGroup {
    if(!SecCKKSIsEnabled()) {
        ckksnotice("ckkszone", self, "Skipping CloudKit reset due to disabled CKKS");
        return nil;
    }

    WEAKIFY(self);

    // We want to delete this zone and this subscription from CloudKit.

    // Step 1: cancel setup operations (if they exist)
    [self.accountLoggedInDependency cancel];
    [self.zoneSetupOperation cancel];
    [self.zoneCreationOperation cancel];
    [self.zoneSubscriptionOperation cancel];

    // Step 2: Try to delete the zone

    CKKSZoneModifyOperations* zoneOps = [self.zoneModifier deleteZone:self.zoneID];

    CKKSResultOperation* afterModification = [CKKSResultOperation named:@"after-modification" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
        STRONGIFY(self);

        bool fatalError = false;

        NSError* operationError = zoneOps.zoneModificationOperation.error;
        bool removed = [zoneOps.deletedRecordZoneIDs containsObject:self.zoneID];

        if(!removed && operationError) {
            // Okay, but if this error is either 'ZoneNotFound' or 'UserDeletedZone', that's fine by us: the zone is deleted.
            NSDictionary* partialErrors = operationError.userInfo[CKPartialErrorsByItemIDKey];
            if([operationError.domain isEqualToString:CKErrorDomain] && operationError.code == CKErrorPartialFailure && partialErrors) {
                for(CKRecordZoneID* errorZoneID in partialErrors.allKeys) {
                    NSError* errorZone = partialErrors[errorZoneID];

                    if(errorZone && [errorZone.domain isEqualToString:CKErrorDomain] &&
                       (errorZone.code == CKErrorZoneNotFound || errorZone.code == CKErrorUserDeletedZone)) {
                        ckksnotice("ckkszone", self, "Attempted to delete zone %@, but it's already missing. This is okay: %@", errorZoneID, errorZone);
                    } else {
                        fatalError = true;
                    }
                }

            } else {
                fatalError = true;
            }

            ckksnotice("ckkszone", self, "deletion of record zone %@ completed with error: %@", self.zoneID, operationError);

            if(fatalError) {
                op.error = operationError;
            }
        } else {
            ckksnotice("ckkszone", self, "deletion of record zone %@ completed successfully", self.zoneID);
        }
    }];

    [afterModification addDependency:zoneOps.zoneModificationOperation];
    return afterModification;
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

    [op addNullableDependency:self.accountLoggedInDependency];

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

    // And now, wait for all operations that are running
    for(NSOperation* op in self.operationQueue.operations) {
        if(op.isExecuting) {
            [op waitUntilFinished];
        }
    }
}

@end

#endif /* OCTAGON */

