
#if OCTAGON

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ckks/CKKSLocalResetOperation.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"
#import "keychain/ckks/CKKSMirrorEntry.h"
#import "keychain/ot/OTDefines.h"

@implementation CKKSLocalResetOperation

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

        self.name = @"ckks-local-reset";
    }
    return self;
}

- (void)main {
    [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult {
        [self onqueuePerformLocalReset];
        return CKKSDatabaseTransactionCommit;
    }];
}

- (void)onqueuePerformLocalReset
{
    NSError* localerror = nil;

    for(CKKSKeychainViewState* view in self.deps.zones) {
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:view.zoneID.zoneName];
        ckse.ckzonecreated = false;
        ckse.ckzonesubscribed = false; // I'm actually not sure about this: can you be subscribed to a non-existent zone?
        ckse.changeToken = NULL;
        [ckse saveToDatabase:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't reset zone status: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        [CKKSMirrorEntry deleteAll:view.zoneID error:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't delete all CKKSMirrorEntry: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        [CKKSOutgoingQueueEntry deleteAll:view.zoneID error:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't delete all CKKSOutgoingQueueEntry: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        [CKKSIncomingQueueEntry deleteAll:view.zoneID error:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't delete all CKKSIncomingQueueEntry: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        [CKKSKey deleteAll:view.zoneID error:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't delete all CKKSKey: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        [CKKSTLKShareRecord deleteAll:view.zoneID error:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't delete all CKKSTLKShare: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        [CKKSCurrentKeyPointer deleteAll:view.zoneID error:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't delete all CKKSCurrentKeyPointer: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        [CKKSCurrentItemPointer deleteAll:view.zoneID error:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't delete all CKKSCurrentItemPointer: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        [CKKSDeviceStateEntry deleteAll:view.zoneID error:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't delete all CKKSDeviceStateEntry: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        if(self.error) {
            break;
        }
    }

    if(!self.error) {
        ckksnotice_global("local-reset", "Successfully deleted all local data for zones: %@", self.deps.zones);
        self.nextState = self.intendedState;
    }
}

@end

#endif // OCTAGON

