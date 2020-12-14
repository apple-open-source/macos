
#import <CoreCDP/CDPError.h>
#import <CoreCDP/CDPStateController.h>
#import <CloudServices/CloudServices.h>

#import "utilities/debugging.h"

#import "keychain/ot/ObjCImprovements.h"

#import "keychain/escrowrequest/EscrowRequestController.h"
#import "keychain/escrowrequest/operations/EscrowRequestPerformEscrowEnrollOperation.h"
#import "keychain/escrowrequest/generated_source/SecEscrowPendingRecord.h"
#import "keychain/escrowrequest/SecEscrowPendingRecord+KeychainSupport.h"

#import "keychain/ckks/CKKSLockStateTracker.h"

@interface EscrowRequestPerformEscrowEnrollOperation ()
@property bool enforceRateLimiting;
@property CKKSLockStateTracker* lockStateTracker;
@end

@implementation EscrowRequestPerformEscrowEnrollOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)initWithIntendedState:(OctagonState*)intendedState
                           errorState:(OctagonState*)errorState
                     enforceRateLimiting:(bool)enforceRateLimiting
                     lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
{
    if((self = [super init])) {
        _intendedState = intendedState;
        _nextState = errorState;
        _enforceRateLimiting = enforceRateLimiting;
        _lockStateTracker = lockStateTracker;
    }
    return self;
}

- (BOOL)checkFatalError:(NSError *)error
{
    if (error == nil) {
        return NO;
    }

    if (error.code == kSecureBackupInternalError && [error.domain isEqualToString:kSecureBackupErrorDomain]) { // SOS peer ID mismatch!!!, the error code is wrong though
        return YES;
    }

    if ([error.domain isEqualToString:kSecureBackupErrorDomain] && error.code == kSecureBackupNotInSyncCircleError) {
        // One or more peers is missing (likely the SOS peer)
        return YES;
    }

    if( [error.domain isEqualToString:CDPStateErrorDomain] && error.code == CDPStateErrorNoPeerIdFound) {
        // CDP is unhappy about the self peer. I don't understand why we get both this and kSecureBackupNotInSyncCircleError
        return YES;
    }

    return NO;
}

- (void)groupStart
{
    secnotice("escrowrequest", "Attempting to escrow any pending prerecords");

    NSError* error = nil;
    NSArray<SecEscrowPendingRecord*>* records = [SecEscrowPendingRecord loadAllFromKeychain:&error];
    if(error && !([error.domain isEqualToString:NSOSStatusErrorDomain] && error.code == errSecItemNotFound)) {
        secnotice("escrowrequest", "failed to fetch records from keychain: %@", error);
        self.error = error;

        if([self.lockStateTracker isLockedError: error]) {
            secnotice("escrowrequest", "Will retry after unlock");
            self.nextState = EscrowRequestStateWaitForUnlock;
        } else {
            self.nextState = EscrowRequestStateNothingToDo;
        }
        return;
    }
    error = nil;

    SecEscrowPendingRecord* record = nil;

    for(SecEscrowPendingRecord* existingRecord in records) {
        if(existingRecord.uploadCompleted) {
            secnotice("escrowrequest", "Skipping completed escrow request (%@)", existingRecord);
            continue;
        }

        if(self.enforceRateLimiting && [existingRecord escrowAttemptedWithinLastSeconds:5*60]) {
            secnotice("escrowrequest", "Skipping pending escrow request (%@); it's rate limited", existingRecord);
            continue;
        }

        if(existingRecord.hasSerializedPrerecord) {
            record = existingRecord;
            break;
        }
    }

    if(record == nil && record.uuid == nil) {
        secnotice("escrowrequest", "No pending escrow request has a prerecord");
        self.nextState = EscrowRequestStateNothingToDo;
        return;
    }

    secnotice("escrowrequest", "escrow request have pre-record uploading: %@", record.uuid);

    // Ask CDP to escrow the escrow-record. Use the "finish operation" trick to wait
    CKKSResultOperation* finishOp = [CKKSResultOperation named:@"cdp-finish" withBlock:^{}];
    [self dependOnBeforeGroupFinished: finishOp];

    /*
     * Update and save the preRecord an extra time (before we crash)
     */

    record.lastEscrowAttemptTime = (uint64_t) ([[NSDate date] timeIntervalSince1970] * 1000);
    record.uploadRetries += 1;

    // Save the last escrow attempt time to keychain
    NSError* saveError = nil;
    [record saveToKeychain:&saveError];
    if(saveError) {
        secerror("escrowrequest: unable to save last escrow time: %@", error);
    }

    WEAKIFY(self);

    [EscrowRequestPerformEscrowEnrollOperation cdpUploadPrerecord:record
                                                       secretType:CDPComplexDeviceSecretType
                                                            reply:^(BOOL didUpdate, NSError * _Nullable error) {
                                                                STRONGIFY(self);

                                                                //* check for fatal errors that definatly should make us give up
                                                                if ([self checkFatalError:error]) {
                                                                    secerror("escrowrequest: fatal error for record: %@, dropping: %@", record.uuid, error);
                                                                    NSError* deleteError = nil;
                                                                    [record deleteFromKeychain:&deleteError];
                                                                    if(saveError) {
                                                                        secerror("escrowrequest: unable to delete last escrow time: %@", deleteError);
                                                                    }

                                                                    self.error = error;
                                                                    [self.operationQueue addOperation:finishOp];
                                                                    return;
                                                                }

                                                                if(error || !didUpdate) {
                                                                    secerror("escrowrequest: prerecord %@ upload failed: %@", record.uuid, error);

                                                                    self.error = error;
                                                                    [self.operationQueue addOperation:finishOp];
                                                                    return;
                                                                }

                                                                self.numberOfRecordsUploaded = 1;
                                                                secerror("escrowrequest: prerecord %@ upload succeeded", record.uuid);

                                                                record.uploadCompleted = true;
                                                                NSError* saveError = nil;
                                                                [record saveToKeychain:&saveError];
                                                                if(saveError) {
                                                                    secerror("escrowrequest: unable to save last escrow time: %@", error);
                                                                }

                                                                if(saveError) {
                                                                    secerror("escrowrequest: unable to save completion of prerecord %@ in keychain", record.uuid);
                                                                }

                                                                self.nextState = EscrowRequestStateNothingToDo;
                                                                [self.operationQueue addOperation:finishOp];
                                                            }];
}

+ (void)cdpUploadPrerecord:(SecEscrowPendingRecord*)recordToSend
                secretType:(CDPDeviceSecretType)secretType
                     reply:(void (^)(BOOL didUpdate, NSError* _Nullable error))reply
{
    CDPStateController *controller = [[CDPStateController alloc] initWithContext:nil];
    [controller attemptToEscrowPreRecord:@"unknown-local-passcode"
                           preRecordUUID:recordToSend.uuid
                              secretType:secretType
                              completion:^(BOOL didUpdate, NSError *error) {
                                  reply(didUpdate, error);
                              }];
}

@end
