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
    __block CKKSZoneStateEntry* ckseOriginal = nil;
    [self.deps.databaseProvider dispatchSyncWithReadOnlySQLTransaction:^{
        ckseOriginal = [CKKSZoneStateEntry state:self.deps.zoneID.zoneName];
    }];

    if(ckseOriginal.ckzonecreated && ckseOriginal.ckzonesubscribed) {
        ckksinfo("ckkskey", self.deps.zoneID, "Zone is already created and subscribed");
        self.nextState = self.intendedState;
        return;
    }

    ckksnotice("ckkszone", self.deps.zoneID, "Asking to create and subscribe to CloudKit zone '%@'", self.deps.zoneID.zoneName);
    CKRecordZone* zone = [[CKRecordZone alloc] initWithZoneID:self.deps.zoneID];
    CKKSZoneModifyOperations* zoneOps = [self.deps.zoneModifier createZone:zone];

    WEAKIFY(self);

    CKKSResultOperation* handleModificationsOperation = [CKKSResultOperation named:@"handle-modification" withBlock:^{
        STRONGIFY(self);
        BOOL zoneCreated = NO;
        BOOL zoneSubscribed = NO;
        if([zoneOps.savedRecordZones containsObject:zone]) {
            ckksinfo("ckkszone", self.deps.zoneID, "Successfully created '%@'", self.deps.zoneID);
            zoneCreated = YES;
        } else {
            ckkserror("ckkszone", self.deps.zoneID, "Failed to create '%@'", self.deps.zoneID);
        }

        bool createdSubscription = false;
        for(CKSubscription* subscription in zoneOps.savedSubscriptions) {
            if([subscription.subscriptionID isEqual:[@"zone:" stringByAppendingString: self.deps.zoneID.zoneName]]) {
                zoneSubscribed = true;
                break;
            }
        }

        if(createdSubscription) {
            ckksinfo("ckkszone", self.deps.zoneID, "Successfully subscribed '%@'", self.deps.zoneID);
            zoneSubscribed = YES;
        } else {
            ckkserror("ckkszone", self.deps.zoneID, "Failed to subscribe to '%@'", self.deps.zoneID);
        }

        [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            ckksnotice("ckkszone", self.deps.zoneID, "Zone setup progress: created:%d %@ subscribed:%d %@",
                       zoneCreated,
                       zoneOps.zoneModificationOperation.error,
                       zoneSubscribed,
                       zoneOps.zoneSubscriptionOperation.error);

            NSError* error = nil;
            CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:self.deps.zoneID.zoneName];
            ckse.ckzonecreated = zoneCreated;
            ckse.ckzonesubscribed = zoneSubscribed;

            // Although, if the zone subscribed error says there's no zone, mark down that there's no zone
            if(zoneOps.zoneSubscriptionOperation.error &&
               [zoneOps.zoneSubscriptionOperation.error.domain isEqualToString:CKErrorDomain] && zoneOps.zoneSubscriptionOperation.error.code == CKErrorPartialFailure) {
                NSError* subscriptionError = zoneOps.zoneSubscriptionOperation.error.userInfo[CKPartialErrorsByItemIDKey][self.deps.zoneID];
                if(subscriptionError && [subscriptionError.domain isEqualToString:CKErrorDomain] && subscriptionError.code == CKErrorZoneNotFound) {

                    ckkserror("ckks", self.deps.zoneID, "zone subscription error appears to say the zone doesn't exist, fixing status: %@", zoneOps.zoneSubscriptionOperation.error);
                    ckse.ckzonecreated = false;
                }
            }

            [ckse saveToDatabase:&error];
            if(error) {
                ckkserror("ckks", self.deps.zoneID, "couldn't save zone creation status for %@: %@", self.deps.zoneID, error);
            }

            if(!zoneCreated || !zoneSubscribed) {
                // Go into 'zonecreationfailed'
                self.nextState = SecCKKSZoneKeyStateZoneCreationFailed;
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
