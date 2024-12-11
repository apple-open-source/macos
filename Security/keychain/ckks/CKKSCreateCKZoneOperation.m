#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSCreateCKZoneOperation.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OTDefines.h"

#import <KeychainCircle/SecurityAnalyticsConstants.h>
#import <KeychainCircle/SecurityAnalyticsReporterRTC.h>
#import <KeychainCircle/AAFAnalyticsEvent+Security.h>

@interface CKKSCreateCKZoneOperation ()
@property bool allZoneCreationsSucceeded;
@property bool allZoneSubscriptionsSucceeded;
@property bool networkError;

@property NSError* zoneModificationError;
@property NSError* zoneSubscriptionError;

@property CKKSResultOperation* setResultStateOperation;
@end

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
        
        _allZoneCreationsSucceeded = true;
        _allZoneSubscriptionsSucceeded = true;
        _networkError = false;
    }
    return self;
}

- (void)groupStart
{
#if TARGET_OS_TV
    [self.deps.personaAdapter prepareThreadForKeychainAPIUseForPersonaIdentifier: nil];
#endif
    __block NSMutableArray<CKRecordZone*>* zonesNeedingCreation = [NSMutableArray array];
    __block NSMutableArray<CKRecordZoneSubscription*>* subscriptionsToSubscribe = [NSMutableArray array];

    [self.deps.databaseProvider dispatchSyncWithReadOnlySQLTransaction:^{
        for(CKKSKeychainViewState* viewState in self.deps.views) {
            CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry contextID:self.deps.contextID zoneName:viewState.zoneID.zoneName];

            if(ckse.ckzonecreated && ckse.ckzonesubscribed) {
                ckksinfo("ckkskey", viewState.zoneID, "Zone is already created and subscribed");
                viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateInitialized;
            } else {
                CKRecordZone* zone = [[CKRecordZone alloc] initWithZoneID:viewState.zoneID];
                [zonesNeedingCreation addObject:zone];

                CKRecordZoneSubscription* subscription = [[CKRecordZoneSubscription alloc] initWithZoneID:zone.zoneID
                                                                                           subscriptionID:[@"zone:" stringByAppendingString: zone.zoneID.zoneName]];
                [subscriptionsToSubscribe addObject:subscription];

                viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateInitializing;
                [viewState.launch addEvent:@"zone-create"];
            }
        }
    }];

    if(zonesNeedingCreation.count == 0) {
        self.nextState = self.intendedState;
        return;
    }

    AAFAnalyticsEventSecurity *zoneCreationEventS = [[AAFAnalyticsEventSecurity alloc] initWithCKKSMetrics:@{kSecurityRTCFieldNumViews: @(zonesNeedingCreation.count)}
                                                                                                   altDSID:self.deps.activeAccount.altDSID
                                                                                                 eventName:kSecurityRTCEventNameZoneCreation
                                                                                           testsAreEnabled:SecCKKSTestsEnabled()
                                                                                                  category:kSecurityRTCEventCategoryAccountDataAccessRecovery
                                                                                                sendMetric:self.deps.sendMetric];

    ckksnotice_global("ckkszone", "Asking to create and subscribe to CloudKit zones: %@", zonesNeedingCreation);
    [self.deps.overallLaunch addEvent:@"zone-create"];

    WEAKIFY(self);

    ckksnotice_global("ckkszonemodifier", "Attempting to create zones %@", zonesNeedingCreation);

    CKDatabaseOperation<CKKSModifyRecordZonesOperation>* zoneModifyOperation = [[self.deps.cloudKitClassDependencies.modifyRecordZonesOperationClass alloc] initWithRecordZonesToSave:zonesNeedingCreation recordZoneIDsToDelete:nil];

    __block CKDatabaseOperation<CKKSModifySubscriptionsOperation>* zoneSubscriptionOperation = nil;

    zoneModifyOperation.configuration.isCloudKitSupportOperation = YES;
    zoneModifyOperation.database = self.deps.ckdatabase;
    zoneModifyOperation.name = @"zone-creation-operation";
    zoneModifyOperation.group = [CKOperationGroup CKKSGroupWithName:@"zone-creation"];

    // CKKSHighPriorityOperations default enabled
    // This operation might be needed during CKKS/Manatee bringup, which affects the user experience. Bump our priority to get it off-device and unblock Manatee access.
    zoneModifyOperation.qualityOfService = NSQualityOfServiceUserInitiated;
    

    zoneModifyOperation.modifyRecordZonesCompletionBlock = ^(NSArray<CKRecordZone *> *savedRecordZones,
                                                             NSArray<CKRecordZoneID *> *deletedRecordZoneIDs,
                                                             NSError *operationError) {
        STRONGIFY(self);
        if(operationError) {
            ckkserror_global("ckkszonemodifier", "Zone modification failed: %@", operationError);
            [self.deps inspectErrorForRetryAfter:operationError];

            if ([self.deps.reachabilityTracker isNetworkError:operationError]){
                ckksnotice_global("ckkszonemodifier", "Waiting for reachability before issuing zone creation");
                self.networkError = true;
            }
        }
        ckksnotice_global("ckkszonemodifier", "created zones: %@", savedRecordZones);
        self.zoneModificationError = operationError;

        // Exit early if we encountered an error creating zones.
        if (self.zoneModificationError) {
            [self.operationQueue addOperation:self.setResultStateOperation];
            return;
        }
        
        // As zone creation finishes, we create a new zone subscription operation and pass it to cloudKitRetryAfter to add to the CK Database. Its completion block handles CKKSZoneStateEntry creation and kicks off the operation which sets the next state machine state.
        ckksnotice_global("ckkszonemodifier", "Attempting to subscribe to zones %@", subscriptionsToSubscribe);
        zoneSubscriptionOperation = [[self.deps.cloudKitClassDependencies.modifySubscriptionsOperationClass alloc] initWithSubscriptionsToSave:subscriptionsToSubscribe subscriptionIDsToDelete:nil];
        zoneSubscriptionOperation.configuration.isCloudKitSupportOperation = YES;
        zoneSubscriptionOperation.database = self.deps.ckdatabase;
        zoneSubscriptionOperation.name = @"zone-subscription-operation";

        // CKKSHighPriorityOperations default enabled
        // This operation might be needed during CKKS/Manatee bringup, which affects the user experience. Bump our priority to get it off-device and unblock Manatee access.
        zoneSubscriptionOperation.qualityOfService = NSQualityOfServiceUserInitiated;
        
        
        zoneSubscriptionOperation.modifySubscriptionsCompletionBlock = ^(NSArray<CKSubscription *> * _Nullable savedSubscriptions,
                                                                         NSArray<NSString *> * _Nullable deletedSubscriptionIDs,
                                                                         NSError * _Nullable zoneSubscriptionError) {
            STRONGIFY(self);
            self.zoneSubscriptionError = zoneSubscriptionError;
            if(zoneSubscriptionError) {
                ckkserror_global("ckkszonemodifier", "Couldn't create cloudkit zone subscription; keychain syncing is severely degraded: %@", zoneSubscriptionError);
                [self.deps inspectErrorForRetryAfter:zoneSubscriptionError];

                if ([self.deps.reachabilityTracker isNetworkError:zoneSubscriptionError]) {
                    ckksnotice_global("ckkszonemodifier", "Waiting for reachability before issuing zone subscription");
                    self.networkError = true;
                }
            }
            ckksnotice_global("ckkszonemodifier", "Successfully subscribed to %@", savedSubscriptions);
            
            // Create the corresponding CKKSZoneStateEntries based on results of zone creation & zone subscription.
            [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{

                for(CKKSKeychainViewState* viewState in self.deps.views) {
                    CKRecordZone* zone = nil;

                    for(CKRecordZone* possibleZone in zonesNeedingCreation) {
                        if([possibleZone.zoneID isEqual:viewState.zoneID]) {
                            zone = possibleZone;
                            break;
                        }
                    }

                    if(zone == nil) {
                        // We weren't intending to modify this zone. Skip with prejudice!
                        continue;
                    }

                    bool zoneCreated = false;
                    bool zoneSubscribed = false;
                    NSError* localZoneModificationError = nil;

                    if([savedRecordZones containsObject:zone]) {
                        ckksinfo("ckkszone", viewState.zoneID, "Successfully created '%@'", viewState.zoneID);
                        zoneCreated = true;

                        // Since we've successfully created the zone, we should send the ready notification the next time the zone is ready.
                        [viewState armReadyNotification];
                    } else {
                        // Grab the error pertaining to this zone, if there is one.
                        if(self.zoneModificationError &&
                           [self.zoneModificationError.domain isEqualToString:CKErrorDomain] && self.zoneModificationError.code == CKErrorPartialFailure) {
                            localZoneModificationError = self.zoneModificationError.userInfo[CKPartialErrorsByItemIDKey][viewState.zoneID];
                        }
                        
                        ckkserror("ckkszone", viewState.zoneID, "Failed to create '%@' with error %@", viewState.zoneID, localZoneModificationError);
                    }

                    NSString* intendedSubscriptionID = [@"zone:" stringByAppendingString:viewState.zoneID.zoneName];
                    bool createdSubscription = false;
                    for(CKSubscription* subscription in savedSubscriptions) {
                        if([subscription.subscriptionID isEqual:intendedSubscriptionID]) {
                            createdSubscription = true;
                            break;
                        }
                    }

                    // Or, if the subcription error is 'the subscription already exists', that's fine too
                    if(zoneSubscriptionError &&
                       [zoneSubscriptionError.domain isEqualToString:CKErrorDomain] && zoneSubscriptionError.code == CKErrorPartialFailure) {
                        NSError* subscriptionError = zoneSubscriptionError.userInfo[CKPartialErrorsByItemIDKey][intendedSubscriptionID];
                        NSError* thirdLevelError = subscriptionError.userInfo[NSUnderlyingErrorKey];

                        if(subscriptionError && [subscriptionError.domain isEqualToString:CKErrorDomain] && subscriptionError.code == CKErrorServerRejectedRequest &&
                           thirdLevelError && [thirdLevelError.domain isEqualToString:CKErrorDomain] && thirdLevelError.code == CKUnderlyingErrorDuplicateSubscription) {
                            ckkserror("ckks", viewState.zoneID, "zone subscription error appears to say that the zone subscription exists; this is okay!");
                            createdSubscription = true;
                        }
                    }

                    if(createdSubscription) {
                        ckksinfo("ckkszone", viewState.zoneID, "Successfully subscribed '%@'", viewState.zoneID);
                        zoneSubscribed = true;
                    } else {
                        ckkserror("ckkszone", viewState.zoneID, "Failed to subscribe to '%@'", viewState.zoneID);
                    }

                    ckksnotice("ckkszone", viewState.zoneID, "Zone setup progress: created:%d %@ subscribed:%d %@",
                               zoneCreated,
                               localZoneModificationError,
                               zoneSubscribed,
                               zoneSubscriptionError);

                    if(zoneCreated && zoneSubscribed) {
                        // Success! This zone is initialized.
                        viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateInitialized;
                    }

                    NSError* error = nil;
                    CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry contextID:self.deps.contextID zoneName:viewState.zoneID.zoneName];
                    ckse.ckzonecreated = zoneCreated;
                    ckse.ckzonesubscribed = zoneSubscribed;

                    // Although, if the zone subscribed error says there's no zone, mark down that there's no zone
                    if(zoneSubscriptionError &&
                       [zoneSubscriptionError.domain isEqualToString:CKErrorDomain] && zoneSubscriptionError.code == CKErrorPartialFailure) {
                        NSError* subscriptionError = zoneSubscriptionError.userInfo[CKPartialErrorsByItemIDKey][viewState.zoneID];
                        if(subscriptionError && [subscriptionError.domain isEqualToString:CKErrorDomain] && subscriptionError.code == CKErrorZoneNotFound) {
                            ckkserror("ckks", viewState.zoneID, "zone subscription error appears to say the zone doesn't exist, fixing status: %@", zoneSubscriptionError);
                            ckse.ckzonecreated = false;
                        }
                    }

                    [ckse saveToDatabase:&error];
                    if(error) {
                        ckkserror("ckks", viewState.zoneID, "couldn't save zone creation status for %@: %@", viewState.zoneID, error);
                    }

                    self.allZoneCreationsSucceeded = self.allZoneCreationsSucceeded && zoneCreated;
                    self.allZoneSubscriptionsSucceeded = self.allZoneSubscriptionsSucceeded && zoneSubscribed;
                }

                return CKKSDatabaseTransactionCommit;
            }];

            [self.operationQueue addOperation:self.setResultStateOperation];

        };

        [self.deps.ckdatabase addOperation:zoneSubscriptionOperation];
        
    };

    [self.deps.ckdatabase addOperation:zoneModifyOperation];
    
    // Force this thread to wait on zone subscription operation finishing
    self.setResultStateOperation = [CKKSResultOperation named:@"determine-next-state" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
        STRONGIFY(self);
        
        ckksnotice_global("ckkszonemodifier", "Finished creating & subscribing to zones");
        
        if (self.networkError) {
            self.nextState = CKKSStateZoneCreationFailedDueToNetworkError;
            self.error = self.zoneModificationError ?: self.zoneSubscriptionError;
        }
        else if (!self.allZoneCreationsSucceeded || !self.allZoneSubscriptionsSucceeded) {
            // Go into 'zonecreationfailed'
            self.nextState = CKKSStateZoneCreationFailed;
            self.error = self.zoneModificationError ?: self.zoneSubscriptionError;

            [SecurityAnalyticsReporterRTC sendMetricWithEvent:zoneCreationEventS success:NO error:self.error];
        } else {
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:zoneCreationEventS success:YES error:nil];
            self.nextState = self.intendedState;
        }
    }];

    [self.setResultStateOperation addNullableDependency:zoneSubscriptionOperation];
    [self dependOnBeforeGroupFinished:self.setResultStateOperation];

}

@end

#endif // OCTAGON
