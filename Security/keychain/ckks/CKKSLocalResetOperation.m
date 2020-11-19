
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

    CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:self.deps.zoneID.zoneName];
    ckse.ckzonecreated = false;
    ckse.ckzonesubscribed = false; // I'm actually not sure about this: can you be subscribed to a non-existent zone?
    ckse.changeToken = NULL;
    [ckse saveToDatabase:&localerror];
    if(localerror && self.error == nil) {
        ckkserror("local-reset", self.deps.zoneID, "couldn't reset zone status: %@", localerror);
        self.error = localerror;
        localerror = nil;
    }

    [CKKSMirrorEntry deleteAll:self.deps.zoneID error:&localerror];
    if(localerror && self.error == nil) {
        ckkserror("local-reset", self.deps.zoneID, "couldn't delete all CKKSMirrorEntry: %@", localerror);
        self.error = localerror;
        localerror = nil;
    }

    [CKKSOutgoingQueueEntry deleteAll:self.deps.zoneID error:&localerror];
    if(localerror && self.error == nil) {
        ckkserror("local-reset", self.deps.zoneID, "couldn't delete all CKKSOutgoingQueueEntry: %@", localerror);
        self.error = localerror;
        localerror = nil;
    }

    [CKKSIncomingQueueEntry deleteAll:self.deps.zoneID error:&localerror];
    if(localerror && self.error == nil) {
        ckkserror("local-reset", self.deps.zoneID, "couldn't delete all CKKSIncomingQueueEntry: %@", localerror);
        self.error = localerror;
        localerror = nil;
    }

    [CKKSKey deleteAll:self.deps.zoneID error:&localerror];
    if(localerror && self.error == nil) {
        ckkserror("local-reset", self.deps.zoneID, "couldn't delete all CKKSKey: %@", localerror);
        self.error = localerror;
        localerror = nil;
    }

    [CKKSTLKShareRecord deleteAll:self.deps.zoneID error:&localerror];
    if(localerror && self.error == nil) {
        ckkserror("local-reset", self.deps.zoneID, "couldn't delete all CKKSTLKShare: %@", localerror);
        self.error = localerror;
        localerror = nil;
    }

    [CKKSCurrentKeyPointer deleteAll:self.deps.zoneID error:&localerror];
    if(localerror && self.error == nil) {
        ckkserror("local-reset", self.deps.zoneID, "couldn't delete all CKKSCurrentKeyPointer: %@", localerror);
        self.error = localerror;
        localerror = nil;
    }

    [CKKSCurrentItemPointer deleteAll:self.deps.zoneID error:&localerror];
    if(localerror && self.error == nil) {
        ckkserror("local-reset", self.deps.zoneID, "couldn't delete all CKKSCurrentItemPointer: %@", localerror);
        self.error = localerror;
        localerror = nil;
    }

    [CKKSDeviceStateEntry deleteAll:self.deps.zoneID error:&localerror];
    if(localerror && self.error == nil) {
        ckkserror("local-reset", self.deps.zoneID, "couldn't delete all CKKSDeviceStateEntry: %@", localerror);
        self.error = localerror;
        localerror = nil;
    }

    if(!self.error) {
        ckksnotice("local-reset", self.deps.zoneID, "Successfully deleted all local data");
        self.nextState = self.intendedState;
    }
}

@end

#endif // OCTAGON

