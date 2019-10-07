
#import <CloudServices/SecureBackup.h>

#import "utilities/debugging.h"
#import "keychain/ckks/CKKSLockStateTracker.h"

#import "keychain/escrowrequest/EscrowRequestController.h"
#import "keychain/escrowrequest/operations/EscrowRequestInformCloudServicesOperation.h"
#import "keychain/escrowrequest/generated_source/SecEscrowPendingRecord.h"
#import "keychain/escrowrequest/SecEscrowPendingRecord+KeychainSupport.h"

@interface EscrowRequestInformCloudServicesOperation()
@property CKKSLockStateTracker* lockStateTracker;
@end

@implementation EscrowRequestInformCloudServicesOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)initWithIntendedState:(OctagonState*)intendedState
                           errorState:(OctagonState*)errorState
                     lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
{
    if((self = [super init])) {
        _intendedState = intendedState;
        _nextState = errorState;
        _lockStateTracker = lockStateTracker;
    }
    return self;
}

- (void)main
{
    secnotice("escrowrequest", "Telling CloudServices about any pending requests");

    NSError* error = nil;
    NSArray<SecEscrowPendingRecord*>* records = [SecEscrowPendingRecord loadAllFromKeychain:&error];
    if(error && !([error.domain isEqualToString:NSOSStatusErrorDomain] && error.code == errSecItemNotFound)) {
        secnotice("escrowrequest", "failed to fetch records from keychain: %@", error);
        if([self.lockStateTracker isLockedError:error]) {
            secnotice("escrowrequest", "Trying again after unlock");
            self.nextState = EscrowRequestStateWaitForUnlock;
        } else {
            self.nextState = EscrowRequestStateNothingToDo;
        }
        self.error = error;
        return;
    }
    error = nil;

    SecEscrowPendingRecord* record = nil;

    for(SecEscrowPendingRecord* existingRecord in records) {
        if(!existingRecord.hasCertCached) {
            record = existingRecord;
            break;
        }
    }

    if(!record) {
        secnotice("escrowrequest", "No pending escrow request needs a certificate");
        self.nextState = EscrowRequestStateNothingToDo;
        return;
    }

    // Next, see if CloudServices can cache a certificate
    NSData* cachedCert = [EscrowRequestInformCloudServicesOperation triggerCloudServicesPasscodeRequest:record.uuid error:&error];
    record.lastCloudServicesTriggerTime = (uint64_t) ([[NSDate date] timeIntervalSince1970] * 1000);

    if(!cachedCert || error) {
        secerror("escrowrequest: cloudservices reports an issue caching the certificate, so we'll have to try again later: %@", error);
        self.error = error;
        // TODO: wait for network?
        self.nextState = EscrowRequestStateNothingToDo;

        NSError* saveCacheTimeError = nil;
        [record saveToKeychain:&saveCacheTimeError];
        if(saveCacheTimeError) {
            secerror("escrowrequest: unable to save the last attempt time: %@", saveCacheTimeError);
        }

        return;
    }

    record.certCached = true;
    [record saveToKeychain:&error];

    if(error) {
        // Ignore this error, since we've successfully triggered the update. We'll probably re-cache the certificate later, but that's okay.
        secerror("escrowrequest: unable to save escrow update request certificate status, so we'll have to try again later: %@", error);
        self.error = error;

        if([self.lockStateTracker isLockedError:error]) {
            secnotice("escrowrequest", "Trying again after unlock");
            self.nextState = EscrowRequestStateWaitForUnlock;
        } else {
            self.nextState = EscrowRequestStateNothingToDo;
        }

        return;
    }

    secnotice("escrowrequest", "CloudService successfully cached a certificate; request is ready for passcode");
    self.nextState = EscrowRequestStateNothingToDo;
}

// Separated into a class method for mocking
// Returns any cert that CS has cached
+ (NSData* _Nullable)triggerCloudServicesPasscodeRequest:(NSString*)uuid error:(NSError**)error
{
    SecureBackup* sb = [[SecureBackup alloc] init];

    NSError* localError = nil;
    SecureBackupBeginPasscodeRequestResults* results = [sb beginHSA2PasscodeRequest:true
                                                                               uuid:uuid
                                                                              error:error];

    if(!results || localError) {
        secerror("escrowrequest: unable to begin passcode request: %@", localError);
        if(error) {
            *error = localError;
        }
        return nil;
    }

    if(!results.cert) {
        secerror("escrowrequest: sbd failed to cache a certificate");
        // TODO fill in error
        return nil;
    }

    return results.cert;
}

@end
