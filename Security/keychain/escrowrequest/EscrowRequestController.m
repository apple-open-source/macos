
#import "utilities/debugging.h"

#import "keychain/ot/OctagonStateMachine.h"
#import "keychain/ot/OTStates.h"
#import "keychain/escrowrequest/EscrowRequestController.h"
#import "keychain/escrowrequest/EscrowRequestServer.h"

#import "keychain/ckks/CKKSLockStateTracker.h"

#import "keychain/ot/ObjCImprovements.h"

#import "keychain/escrowrequest/operations/EscrowRequestInformCloudServicesOperation.h"
#import "keychain/escrowrequest/operations/EscrowRequestPerformEscrowEnrollOperation.h"

#import "keychain/escrowrequest/generated_source/SecEscrowPendingRecord.h"
#import "keychain/escrowrequest/SecEscrowPendingRecord+KeychainSupport.h"

OctagonState* const EscrowRequestStateNothingToDo = (OctagonState*)@"nothing_to_do";
OctagonState* const EscrowRequestStateTriggerCloudServices = (OctagonState*)@"trigger_cloudservices";

OctagonState* const EscrowRequestStateAttemptEscrowUpload = (OctagonState*)@"trigger_escrow_upload";
OctagonState* const EscrowRequestStateWaitForUnlock = (OctagonState*)@"wait_for_unlock";

@interface EscrowRequestController ()
@property dispatch_queue_t queue;
@property CKKSLockStateTracker* lockStateTracker;
@property bool haveRecordedDate;
@end

@implementation EscrowRequestController

- (instancetype)initWithLockStateTracker:(CKKSLockStateTracker*)lockStateTracker
{
    if((self = [super init])) {
        _queue = dispatch_queue_create("EscrowRequestControllerQueue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _lockStateTracker = lockStateTracker;

        _stateMachine = [[OctagonStateMachine alloc] initWithName:@"escrowrequest"
                                                           states:[NSSet setWithArray:@[EscrowRequestStateNothingToDo,
                                                                                          EscrowRequestStateTriggerCloudServices,
                                                                                          EscrowRequestStateAttemptEscrowUpload,
                                                                                          EscrowRequestStateWaitForUnlock]]
                                                     initialState:EscrowRequestStateNothingToDo
                                                            queue:_queue
                                                      stateEngine:self
                                                 lockStateTracker:lockStateTracker];

        _forceIgnoreCloudServicesRateLimiting = false;
    }

    return self;
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol> * _Nullable)_onqueueNextStateMachineTransition:(nonnull OctagonState *)currentState
                                                                                                         flags:(nonnull OctagonFlags *)flags
                                                                                                  pendingFlags:(nonnull id<OctagonStateOnqueuePendingFlagHandler>)pendingFlagHandler
{
    if([flags _onqueueContains:OctagonFlagEscrowRequestInformCloudServicesOperation]) {
        [flags _onqueueRemoveFlag:OctagonFlagEscrowRequestInformCloudServicesOperation];
        return [[EscrowRequestInformCloudServicesOperation alloc] initWithIntendedState:EscrowRequestStateNothingToDo
                                                                             errorState:EscrowRequestStateNothingToDo
                                                                       lockStateTracker:self.lockStateTracker];
    }

    if([currentState isEqualToString:EscrowRequestStateTriggerCloudServices]) {
        return [[EscrowRequestInformCloudServicesOperation alloc] initWithIntendedState:EscrowRequestStateNothingToDo
                                                                             errorState:EscrowRequestStateNothingToDo
                                                                       lockStateTracker:self.lockStateTracker];
    }

    if([currentState isEqualToString:EscrowRequestStateAttemptEscrowUpload]) {
        return [[EscrowRequestPerformEscrowEnrollOperation alloc] initWithIntendedState:EscrowRequestStateNothingToDo
                                                                             errorState:EscrowRequestStateNothingToDo
                                                                    enforceRateLimiting:true
                                                                       lockStateTracker:self.lockStateTracker];
    }

    if([currentState isEqualToString:EscrowRequestStateWaitForUnlock]) {
        secnotice("escrowrequest", "waiting for unlock before continuing with state machine");
        OctagonStateTransitionOperation* op = [OctagonStateTransitionOperation named:@"wait-for-unlock"
                                                                            entering:EscrowRequestStateNothingToDo];
        [op addNullableDependency:self.lockStateTracker.unlockDependency];
        return op;
    }

    NSError* error = nil;
    NSArray<SecEscrowPendingRecord*>* records = [SecEscrowPendingRecord loadAllFromKeychain:&error];
    if(error) {
        if([self.lockStateTracker isLockedError:error]) {
            return [OctagonStateTransitionOperation named:@"wait-for-unlock"
                                                 entering:EscrowRequestStateWaitForUnlock];
        }
        secnotice("escrowrequest", "failed to fetch records from keychain, nothing to do: %@", error);
        return nil;
    }

    // First, do we need to poke CloudServices?
    for(SecEscrowPendingRecord* record in records) {
        // Completed records don't need anything.
        if(record.hasUploadCompleted && record.uploadCompleted) {
            continue;
        }

        if (!self.haveRecordedDate) {
            NSDate *date = [[CKKSAnalytics logger] datePropertyForKey:ESRPendingSince];
            if (date == NULL) {
                [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:ESRPendingSince];
            }
            self.haveRecordedDate = true;
        }

        uint64_t fiveMinutesAgo = ((uint64_t)[[NSDate date] timeIntervalSince1970] * 1000) - (1000*60*5);

        if(!record.certCached) {
            if(!self.forceIgnoreCloudServicesRateLimiting && (record.hasLastCloudServicesTriggerTime && record.lastCloudServicesTriggerTime >= fiveMinutesAgo)) {
                secnotice("escrowrequest", "Request %@ needs to cache a certificate, but that has been attempted recently. Holding off...", record.uuid);
                continue;
            }

            secnotice("escrowrequest", "Request %@ needs a cached certififcate", record.uuid);

            return [OctagonStateTransitionOperation named:@"escrow-request-cache-cert"
                                                 entering:EscrowRequestStateTriggerCloudServices];
        }

        if(record.hasSerializedPrerecord) {
            if([record escrowAttemptedWithinLastSeconds:5*60]) {
                secnotice("escrowrequest", "Request %@ needs to be stored, but has been attempted recently. Holding off...", record.uuid);
                continue;
            }

            secnotice("escrowrequest", "Request %@ needs to be stored!", record.uuid);

            return [OctagonStateTransitionOperation named:@"escrow-request-attempt-escrow-upload"
                                                 entering:EscrowRequestStateAttemptEscrowUpload];
        }
    }


    return nil;
}

- (void)triggerEscrowUpdateRPC:(nonnull NSString *)reason
                         reply:(nonnull void (^)(NSError * _Nullable))reply
{
    [self.stateMachine startOperation];

    NSError* error = nil;
    NSArray<SecEscrowPendingRecord*>* records = [SecEscrowPendingRecord loadAllFromKeychain:&error];
    if(error && !([error.domain isEqualToString:NSOSStatusErrorDomain] && error.code == errSecItemNotFound)) {
        secnotice("escrowrequest", "failed to fetch records from keychain: %@", error);
        reply(error);
        return;
    }
    error = nil;

    secnotice("escrowrequest", "Investigating a new escrow request");

    BOOL escrowRequestExists = NO;
    for(SecEscrowPendingRecord* existingRecord in records) {
        if(existingRecord.uploadCompleted) {
            continue;
        }

        if (existingRecord.hasAltDSID) {
            continue;
        }

        secnotice("escrowrequest", "Retriggering an existing escrow request: %@", existingRecord);
        existingRecord.hasCertCached = false;
        existingRecord.serializedPrerecord = nil;

        [existingRecord saveToKeychain:&error];
        if(error) {
            secerror("escrowrequest: Unable to save modified request to keychain: %@", error);
            reply(error);
            return;
        }

        secnotice("escrowrequest", "Retriggering an existing escrow request complete");
        escrowRequestExists = YES;
    }

    if(escrowRequestExists == NO){
        secnotice("escrowrequest", "Creating a new escrow request");

        SecEscrowPendingRecord* record = [[SecEscrowPendingRecord alloc] init];
        record.uuid = [[NSUUID UUID] UUIDString];
        record.altDSID = nil;
        record.triggerRequestTime = ((uint64_t)[[NSDate date] timeIntervalSince1970] * 1000);
        secnotice("escrowrequest", "beginning a new escrow request (%@)", record.uuid);

        [record saveToKeychain:&error];

        if(error) {
            secerror("escrowrequest: unable to save escrow update request: %@", error);
            reply(error);
            return;
        }
    }

    [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:ESRPendingSince];
    self.haveRecordedDate = true;

    [self.stateMachine handleFlag:OctagonFlagEscrowRequestInformCloudServicesOperation];

    reply(nil);
}

- (void)storePrerecordsInEscrowRPC:(void (^)(uint64_t count, NSError* _Nullable error))reply
{
    EscrowRequestPerformEscrowEnrollOperation* op = [[EscrowRequestPerformEscrowEnrollOperation alloc] initWithIntendedState:EscrowRequestStateNothingToDo
                                                                                                                  errorState:EscrowRequestStateNothingToDo
                                                                                                         enforceRateLimiting:false
                                                                                                            lockStateTracker:self.lockStateTracker];
    [self.stateMachine startOperation];
    [self.stateMachine doSimpleStateMachineRPC:@"trigger-escrow-store"
                                            op:op
                                  sourceStates:[NSSet setWithObject:EscrowRequestStateNothingToDo]
                                         reply:^(NSError * _Nullable error) {
                                             secnotice("escrowrequest", "Uploaded %d records with error %@", (int)op.numberOfRecordsUploaded, error);
                                             reply(op.numberOfRecordsUploaded, error);
                                         }];
}

@end
