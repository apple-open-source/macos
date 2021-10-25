

#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import "keychain/ckks/CKKSDeleteCKZoneOperation.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OTDefines.h"

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
    }
    return self;
}

- (void)groupStart
{
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
    CKKSZoneModifyOperations* zoneOps = [self.deps.zoneModifier deleteZones:zonesNeedingDeletion];

    WEAKIFY(self);

    CKKSResultOperation* handleModificationsOperation = [CKKSResultOperation named:@"handle-modification" withBlock:^{
        STRONGIFY(self);

        __block bool anyFatalError = NO;

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

                bool fatalError = false;

                NSError* operationError = zoneOps.zoneModificationOperation.error;
                bool removed = [zoneOps.deletedRecordZoneIDs containsObject:viewState.zoneID];

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
                                fatalError = true;
                            }
                        }

                    } else {
                        fatalError = true;
                    }

                    ckksnotice("ckkszone", viewState.zoneID, "deletion of record zone %@ completed with error: %@", viewState.zoneID, operationError);

                    if(fatalError) {
                        // Early-exit
                        self.error = operationError;
                        anyFatalError |= true;
                        continue;
                    }
                }

                ckksnotice("ckkszone", viewState.zoneID, "deletion of record zone %@ completed successfully", viewState.zoneID);

                NSError* error = nil;
                CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:viewState.zoneID.zoneName];
                ckse.ckzonecreated = NO;
                ckse.ckzonesubscribed = NO;

                [ckse saveToDatabase:&error];
                if(error) {
                    ckkserror("ckks", viewState.zoneID, "couldn't save zone creation status for %@: %@", viewState.zoneID, error);
                }

            }
            return CKKSDatabaseTransactionCommit;

        }];

        if(!anyFatalError) {
            self.nextState = self.intendedState;
        }
        // Otherwise, stay in the error state
    }];

    [handleModificationsOperation addNullableDependency:zoneOps.zoneModificationOperation];
    [handleModificationsOperation addNullableDependency:zoneOps.zoneSubscriptionOperation];
    [self runBeforeGroupFinished:handleModificationsOperation];
}

@end

#endif // OCTAGON
