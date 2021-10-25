#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import "keychain/ckks/CKKSCreateCKZoneOperation.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OTDefines.h"

@implementation CKKSCreateCKZoneOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
{
    if(self = [super init]) {
        _deps = dependencies;

        _intendedState = intendedState;
        _nextState = errorState;
    }
    return self;
}

- (void)groupStart
{
    __block NSMutableArray<CKRecordZone*>* zonesNeedingCreation = [NSMutableArray array];

    [self.deps.databaseProvider dispatchSyncWithReadOnlySQLTransaction:^{
        for(CKKSKeychainViewState* viewState in self.deps.views) {
            CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:viewState.zoneID.zoneName];

            if(ckse.ckzonecreated && ckse.ckzonesubscribed) {
                ckksinfo("ckkskey", viewState.zoneID, "Zone is already created and subscribed");
            } else {
                CKRecordZone* zone = [[CKRecordZone alloc] initWithZoneID:viewState.zoneID];
                [zonesNeedingCreation addObject:zone];
                viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateInitializing;
            }
        }
    }];

    if(zonesNeedingCreation.count == 0) {
        self.nextState = self.intendedState;
        return;
    }

    ckksnotice_global("ckkszone", "Asking to create and subscribe to CloudKit zones: %@", zonesNeedingCreation);

    CKKSZoneModifyOperations* zoneOps = [self.deps.zoneModifier createZones:zonesNeedingCreation];

    WEAKIFY(self);

    CKKSResultOperation* handleModificationsOperation = [CKKSResultOperation named:@"handle-modification" withBlock:^{
        STRONGIFY(self);

        [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            bool allZoneCreationsSucceeded = true;
            bool allZoneSubscriptionsSucceeded = true;

            for(CKKSKeychainViewState* viewState in self.deps.views) {
                CKRecordZone* zone = nil;

                for(CKRecordZone* possibleZone in zonesNeedingCreation) {
                    if([possibleZone.zoneID isEqual:viewState.zoneID]) {
                        zone = possibleZone;
                        break;
                    }
                }

                if(zone == nil) {
                    // We weren't intending to modify this zone. Skip with predjudice!
                    continue;
                }

                BOOL zoneCreated = NO;
                BOOL zoneSubscribed = NO;

                if([zoneOps.savedRecordZones containsObject:zone]) {
                    ckksinfo("ckkszone", viewState.zoneID, "Successfully created '%@'", viewState.zoneID);
                    zoneCreated = YES;

                    // Since we've successfully created the zone, we should send the ready notification the next time the zone is ready.
                    [viewState armReadyNotification];
                } else {
                    ckkserror("ckkszone", viewState.zoneID, "Failed to create '%@'", viewState.zoneID);
                }

                NSString* intendedSubscriptionID = [@"zone:" stringByAppendingString:viewState.zoneID.zoneName];
                bool createdSubscription = false;
                for(CKSubscription* subscription in zoneOps.savedSubscriptions) {
                    if([subscription.subscriptionID isEqual:intendedSubscriptionID]) {
                        createdSubscription = true;
                        break;
                    }
                }

                // Or, if the subcription error is 'the subscription already exists', that's fine too
                if(zoneOps.zoneSubscriptionOperation.error &&
                   [zoneOps.zoneSubscriptionOperation.error.domain isEqualToString:CKErrorDomain] && zoneOps.zoneSubscriptionOperation.error.code == CKErrorPartialFailure) {
                    NSError* subscriptionError = zoneOps.zoneSubscriptionOperation.error.userInfo[CKPartialErrorsByItemIDKey][intendedSubscriptionID];
                    NSError* thirdLevelError = subscriptionError.userInfo[NSUnderlyingErrorKey];

                    if(subscriptionError && [subscriptionError.domain isEqualToString:CKErrorDomain] && subscriptionError.code == CKErrorServerRejectedRequest &&
                       thirdLevelError && [thirdLevelError.domain isEqualToString:CKErrorDomain] && thirdLevelError.code == CKErrorInternalDuplicateSubscription) {
                        ckkserror("ckks", viewState.zoneID, "zone subscription error appears to say that the zone subscription exists; this is okay!");
                        createdSubscription = true;
                    }
                }

                if(createdSubscription) {
                    ckksinfo("ckkszone", viewState.zoneID, "Successfully subscribed '%@'", viewState.zoneID);
                    zoneSubscribed = YES;
                } else {
                    ckkserror("ckkszone", viewState.zoneID, "Failed to subscribe to '%@'", viewState.zoneID);
                }

                ckksnotice("ckkszone", viewState.zoneID, "Zone setup progress: created:%d %@ subscribed:%d %@",
                           zoneCreated,
                           zoneOps.zoneModificationOperation.error,
                           zoneSubscribed,
                           zoneOps.zoneSubscriptionOperation.error);

                NSError* error = nil;
                CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:viewState.zoneID.zoneName];
                ckse.ckzonecreated = zoneCreated;
                ckse.ckzonesubscribed = zoneSubscribed;

                // Although, if the zone subscribed error says there's no zone, mark down that there's no zone
                if(zoneOps.zoneSubscriptionOperation.error &&
                   [zoneOps.zoneSubscriptionOperation.error.domain isEqualToString:CKErrorDomain] && zoneOps.zoneSubscriptionOperation.error.code == CKErrorPartialFailure) {
                    NSError* subscriptionError = zoneOps.zoneSubscriptionOperation.error.userInfo[CKPartialErrorsByItemIDKey][viewState.zoneID];
                    if(subscriptionError && [subscriptionError.domain isEqualToString:CKErrorDomain] && subscriptionError.code == CKErrorZoneNotFound) {

                        ckkserror("ckks", viewState.zoneID, "zone subscription error appears to say the zone doesn't exist, fixing status: %@", zoneOps.zoneSubscriptionOperation.error);
                        ckse.ckzonecreated = false;
                    }
                }

                [ckse saveToDatabase:&error];
                if(error) {
                    ckkserror("ckks", viewState.zoneID, "couldn't save zone creation status for %@: %@", viewState.zoneID, error);
                }

                allZoneCreationsSucceeded = allZoneCreationsSucceeded && zoneCreated;
                allZoneSubscriptionsSucceeded = allZoneSubscriptionsSucceeded && zoneSubscribed;
            }

            if(!allZoneCreationsSucceeded || !allZoneSubscriptionsSucceeded) {
                // Go into 'zonecreationfailed'
                self.nextState = CKKSStateZoneCreationFailed;
                self.error = zoneOps.zoneModificationOperation.error ?: zoneOps.zoneSubscriptionOperation.error;
            } else {
                self.nextState = self.intendedState;
            }

            return CKKSDatabaseTransactionCommit;
        }];
    }];

    [handleModificationsOperation addNullableDependency:zoneOps.zoneModificationOperation];
    [handleModificationsOperation addNullableDependency:zoneOps.zoneSubscriptionOperation];
    [self runBeforeGroupFinished:handleModificationsOperation];
}

@end

#endif // OCTAGON
