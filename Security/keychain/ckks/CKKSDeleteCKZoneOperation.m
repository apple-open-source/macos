

#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKSDeleteCKZoneOperation.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OTDefines.h"

@interface CKKSDeleteCKZoneOperation ()
@property bool networkError;

@property CKKSResultOperation* setResultStateOperation;
@end

@implementation CKKSDeleteCKZoneOperation
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
        
        _networkError = false;
    }
    return self;
}

- (void)groupStart
{
#if TARGET_OS_TV
    [self.deps.personaAdapter prepareThreadForKeychainAPIUseForPersonaIdentifier: nil];
#endif
    __block NSMutableArray<CKRecordZoneID*>* zonesNeedingDeletion = [NSMutableArray array];

    for(CKKSKeychainViewState* viewState in self.deps.views) {
        [zonesNeedingDeletion addObject:viewState.zoneID];
    }

    if(zonesNeedingDeletion.count == 0) {
        ckksnotice_global("ckkszone", "No zones to delete");
        self.nextState = self.intendedState;
        return;
    }

    ckksnotice_global("ckkszone", "Deleting CloudKit zones: %@", zonesNeedingDeletion);

    WEAKIFY(self);

    CKDatabaseOperation<CKKSModifyRecordZonesOperation>* zoneDeletionOperation = [[self.deps.cloudKitClassDependencies.modifyRecordZonesOperationClass alloc] initWithRecordZonesToSave:nil recordZoneIDsToDelete:zonesNeedingDeletion];

    zoneDeletionOperation.configuration.isCloudKitSupportOperation = YES;
    zoneDeletionOperation.database = self.deps.ckdatabase;
    zoneDeletionOperation.name = @"zone-creation-operation";
    zoneDeletionOperation.group = [CKOperationGroup CKKSGroupWithName:@"zone-creation"];

    // CKKSSetHighPriorityOperations is default enabled
    zoneDeletionOperation.qualityOfService = NSQualityOfServiceUserInitiated;
    
    
    zoneDeletionOperation.modifyRecordZonesCompletionBlock = ^(NSArray<CKRecordZone *> *savedRecordZones,
                                                             NSArray<CKRecordZoneID *> *deletedRecordZoneIDs,
                                                             NSError *operationError) {
        STRONGIFY(self);
        if(operationError) {
            ckkserror_global("ckkszonemodifier", "Zone modification failed: %@", operationError);
            [self.deps inspectErrorForRetryAfter:operationError];

            if ([self.deps.reachabilityTracker isNetworkError:operationError]) {
                ckksnotice_global("ckkszonemodifier", "Waiting for reachability before issuing zone deletion");
                self.networkError = true;
            }
        }
        ckksnotice_global("ckkszonemodifier", "deleted zones: %@", deletedRecordZoneIDs);
        
        [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            for(CKKSKeychainViewState* viewState in self.deps.views) {
                CKRecordZoneID* zoneID = nil;

                for(CKRecordZoneID* possibleZoneID in zonesNeedingDeletion) {
                    if([possibleZoneID isEqual:viewState.zoneID]) {
                        zoneID = possibleZoneID;
                        break;
                    }
                }

                if(zoneID == nil) {
                    // We weren't intending to delete this zone! skip!
                    continue;
                }

                bool removed = [deletedRecordZoneIDs containsObject:viewState.zoneID];

                // Hang onto operationError if we deem it fatal.
                if(!removed && operationError) {
                    // Okay, but if this error is either 'ZoneNotFound' or 'UserDeletedZone', that's fine by us: the zone is deleted.
                    NSDictionary* partialErrors = operationError.userInfo[CKPartialErrorsByItemIDKey];
                    if([operationError.domain isEqualToString:CKErrorDomain] && operationError.code == CKErrorPartialFailure && partialErrors) {
                        for(CKRecordZoneID* errorZoneID in partialErrors.allKeys) {
                            NSError* errorZone = partialErrors[errorZoneID];

                            if(errorZone && [errorZone.domain isEqualToString:CKErrorDomain] &&
                               (errorZone.code == CKErrorZoneNotFound || errorZone.code == CKErrorUserDeletedZone)) {
                                ckksnotice("ckkszone", viewState.zoneID, "Attempted to delete zone %@, but it's already missing. This is okay: %@", errorZoneID, errorZone);
                            } else {
                                self.error = operationError;
                            }
                        }

                    } else {
                        self.error = operationError;
                    }

                    ckksnotice("ckkszone", viewState.zoneID, "deletion of record zone %@ completed with error: %@", viewState.zoneID, operationError);

                    if(self.error) {
                        // Early-exit
                        ckkserror("ckkszone", viewState.zoneID, "we hit a fatal error!!!");
                        continue;
                    }
                }

                ckksnotice("ckkszone", viewState.zoneID, "deletion of record zone %@ completed successfully", viewState.zoneID);

                NSError* error = nil;
                CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry contextID:self.deps.contextID zoneName:viewState.zoneID.zoneName];
                ckse.ckzonecreated = NO;
                ckse.ckzonesubscribed = NO;

                [ckse saveToDatabase:&error];
                if(error) {
                    ckkserror("ckks", viewState.zoneID, "couldn't save zone deletion status for %@: %@", viewState.zoneID, error);
                }
            }

            [self.operationQueue addOperation:self.setResultStateOperation];
            return CKKSDatabaseTransactionCommit;

        }];

    };

    [self.deps.ckdatabase addOperation:zoneDeletionOperation];

    // Force this thread to wait on zone deletion operation finishing
    self.setResultStateOperation = [CKKSResultOperation named:@"determine-next-state" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
        STRONGIFY(self);
        ckksnotice_global("ckkszonemodifier", "Finished deleting zones");
        
        if (self.networkError) {
            self.nextState = CKKSStateZoneDeletionFailedDueToNetworkError;
        }

        else if (!self.error) {
            self.nextState = self.intendedState;
            ckksnotice_global("ckkszonemodifier", "no fatal errors discovered!");
        }
    }];

    [self.setResultStateOperation addDependency:zoneDeletionOperation];
    [self dependOnBeforeGroupFinished:self.setResultStateOperation];
}

@end

#endif // OCTAGON
