

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

    ckksnotice("ckkszone", self.deps.zoneID, "Deleting CloudKit zone '%@'", self.deps.zoneID);
    CKKSZoneModifyOperations* zoneOps = [self.deps.zoneModifier deleteZone:self.deps.zoneID];


    WEAKIFY(self);

    CKKSResultOperation* handleModificationsOperation = [CKKSResultOperation named:@"handle-modification" withBlock:^{
        STRONGIFY(self);

        bool fatalError = false;

        NSError* operationError = zoneOps.zoneModificationOperation.error;
        bool removed = [zoneOps.deletedRecordZoneIDs containsObject:self.deps.zoneID];

        if(!removed && operationError) {
            // Okay, but if this error is either 'ZoneNotFound' or 'UserDeletedZone', that's fine by us: the zone is deleted.
            NSDictionary* partialErrors = operationError.userInfo[CKPartialErrorsByItemIDKey];
            if([operationError.domain isEqualToString:CKErrorDomain] && operationError.code == CKErrorPartialFailure && partialErrors) {
                for(CKRecordZoneID* errorZoneID in partialErrors.allKeys) {
                    NSError* errorZone = partialErrors[errorZoneID];

                    if(errorZone && [errorZone.domain isEqualToString:CKErrorDomain] &&
                       (errorZone.code == CKErrorZoneNotFound || errorZone.code == CKErrorUserDeletedZone)) {
                        ckksnotice("ckkszone", self.deps.zoneID, "Attempted to delete zone %@, but it's already missing. This is okay: %@", errorZoneID, errorZone);
                    } else {
                        fatalError = true;
                    }
                }

            } else {
                fatalError = true;
            }

            ckksnotice("ckkszone", self.deps.zoneID, "deletion of record zone %@ completed with error: %@", self.deps.zoneID, operationError);

            if(fatalError) {
                // Early-exit
                self.error = operationError;
                return;
            }
        }

        ckksnotice("ckkszone", self.deps.zoneID, "deletion of record zone %@ completed successfully", self.deps.zoneID);

        [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            NSError* error = nil;
            CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:self.deps.zoneID.zoneName];
            ckse.ckzonecreated = NO;
            ckse.ckzonesubscribed = NO;

            [ckse saveToDatabase:&error];
            if(error) {
                ckkserror("ckks", self.deps.zoneID, "couldn't save zone creation status for %@: %@", self.deps.zoneID, error);
            }

            self.nextState = self.intendedState;
            return CKKSDatabaseTransactionCommit;
        }];
    }];

    [handleModificationsOperation addNullableDependency:zoneOps.zoneModificationOperation];
    [handleModificationsOperation addNullableDependency:zoneOps.zoneSubscriptionOperation];
    [self runBeforeGroupFinished:handleModificationsOperation];
}

@end

#endif // OCTAGON
