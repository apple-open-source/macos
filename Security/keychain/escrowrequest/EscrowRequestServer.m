
#import "utilities/debugging.h"

#import "keychain/escrowrequest/EscrowRequestController.h"
#import "keychain/escrowrequest/EscrowRequestServer.h"
#import "keychain/escrowrequest/generated_source/SecEscrowPendingRecord.h"
#import "keychain/escrowrequest/SecEscrowPendingRecord+KeychainSupport.h"

#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSAnalytics.h"

NSString* ESRPendingSince = @"ERSPending";

@implementation EscrowRequestServer

- (instancetype)initWithLockStateTracker:(CKKSLockStateTracker*)lockStateTracker
{
    if((self = [super init])) {
        _controller = [[EscrowRequestController alloc] initWithLockStateTracker:lockStateTracker];
    }
    return self;
}

+ (EscrowRequestServer*)server
{
    static EscrowRequestServer* server;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        server = [[EscrowRequestServer alloc] initWithLockStateTracker:[CKKSLockStateTracker globalTracker]];
        [server setupAnalytics];
    });
    return server;
}


+ (id<SecEscrowRequestable> _Nullable)request:(NSError *__autoreleasing  _Nullable * _Nullable)error {
    return [EscrowRequestServer server];
}

- (BOOL)triggerEscrowUpdate:(nonnull NSString *)reason
                      error:(NSError * _Nullable __autoreleasing * _Nullable)error
{
    // Magical async code style to sync conversion only happens with NSXPC.
    // Use a semaphore here, since we don't have any other option.
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    __block NSError* updateError = nil;
    [self triggerEscrowUpdate:reason reply:^(NSError * _Nullable operationError) {
        updateError = operationError;
        dispatch_semaphore_signal(sema);
    }];
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    if(error && updateError) {
        *error = updateError;
    }
    return updateError == nil ? YES : NO;
}

- (NSDictionary *)fetchStatuses:(NSError **)error
{
    __block NSDictionary *status = nil;

    __block NSError* updateError = nil;
    [self fetchRequestStatuses:^(NSDictionary<NSString *,NSString *> * _Nullable requestUUID, NSError * _Nullable blockError) {
        status = requestUUID;
        updateError = blockError;
    }];

    if(error && updateError) {
        *error = updateError;
    }
    return status;
}

- (bool)pendingEscrowUpload:(NSError *__autoreleasing  _Nullable * _Nullable)error {
    __block NSDictionary *result = nil;
    __block NSError* updateError = nil;

    [self fetchRequestStatuses:^(NSDictionary<NSString *,NSString *> * requestUUID, NSError *blockError) {
        result = requestUUID;
        updateError = blockError;
    }];

    if(updateError) {
        secnotice("escrow", "failed to fetch escrow statuses: %@", updateError);
        if(error) {
            *error = updateError;
        }
        return NO;
    }
    if(result == nil || (result && [result count] == 0)) {
        return NO;
    }

    BOOL inProgress = NO;
    for(NSString* status in result.allValues) {
        if([status isEqualToString:SecEscrowRequestHavePrecord] ||
           [status isEqualToString:SecEscrowRequestPendingPasscode] ||
           [status isEqualToString:SecEscrowRequestPendingCertificate]) {
            inProgress = YES;
        }
    }

    return inProgress;
}

- (void)cachePrerecord:(NSString*)uuid
   serializedPrerecord:(nonnull NSData *)prerecord
                 reply:(nonnull void (^)(NSError * _Nullable))reply
{
    NSError* error = nil;
    SecEscrowPendingRecord* record = [SecEscrowPendingRecord loadFromKeychain:uuid error:&error];

    if(error) {
        secerror("escrowrequest: unable to load uuid %@: %@", uuid, error);
        reply(error);
        return;
    }

    record.serializedPrerecord = prerecord;

    [record saveToKeychain:&error];
    if(error) {
        secerror("escrowrequest: unable to save new prerecord for uuid %@: %@", uuid, error);
        reply(error);
        return;
    }

    secnotice("escrowrequest", "saved new prerecord for uuid %@", uuid);

    // Poke the EscrowRequestController, so it will upload the record
    [self.controller.stateMachine pokeStateMachine];

    reply(nil);
}

- (void)fetchPrerecord:(nonnull NSString *)prerecordUUID
                 reply:(nonnull void (^)(NSData * _Nullable, NSError * _Nullable))reply
{
    NSError* error = nil;
    SecEscrowPendingRecord* record = [SecEscrowPendingRecord loadFromKeychain:prerecordUUID error:&error];

    if(error) {
        secerror("escrowrequest: unable to load prerecord with uuid %@: %@", prerecordUUID, error);
        reply(nil, error);
        return;
    }

    if(record.hasSerializedPrerecord) {
        secnotice("escrowrequest", "fetched prerecord for uuid %@", prerecordUUID);
        reply(record.serializedPrerecord, nil);
    } else {
        secerror("escrowrequest: no prerecord for uuid %@: %@", prerecordUUID, error);
        // TODO: fill in error
        reply(nil, error);
    }
}

- (void)fetchRequestWaitingOnPasscode:(nonnull void (^)(NSString * _Nullable, NSError * _Nullable))reply
{
    NSError* error = nil;

    NSArray<SecEscrowPendingRecord*>* records = [SecEscrowPendingRecord loadAllFromKeychain:&error];

    if(error && [error.domain isEqualToString:NSOSStatusErrorDomain] && error.code == errSecItemNotFound) {
        // Fair enough! There are no requests waiting for a passcode.
        [[CKKSAnalytics logger] setDateProperty:nil forKey:ESRPendingSince];
        reply(nil, nil);
        return;
    }

    if(!records || error) {
        reply(nil, error);
        return;
    }

    // Are any of these requests pending?
    for(SecEscrowPendingRecord* record in records) {
        if(!record.certCached) {
            secnotice("escrowrequest", "Escrow request %@ doesn't yet have a certificate cached", record.uuid);
            continue;
        }

        if(record.hasSerializedPrerecord) {
            secnotice("escrowrequest", "Escrow request %@ already has a prerecord; no passcode needed", record.uuid);
            continue;
        }

        secnotice("escrowrequest", "Escrow request %@ is pending a passcode", record.uuid);
        reply(record.uuid, nil);
        return;
    }

    secnotice("escrowrequest", "No escrow requests need a passcode");
    reply(nil, nil);
}

- (void)triggerEscrowUpdate:(nonnull NSString *)reason
                      reply:(nonnull void (^)(NSError * _Nullable))reply
{
    secnotice("escrowrequest", "Triggering an escrow update request due to '%@'", reason);

    [self.controller triggerEscrowUpdateRPC:reason
                                      reply:reply];
}

- (void)fetchRequestStatuses:(void (^)(NSDictionary<NSString*, NSString*>* _Nullable requestUUID, NSError* _Nullable error))reply
{
    NSError* error = nil;
    NSArray<SecEscrowPendingRecord*>* records = [SecEscrowPendingRecord loadAllFromKeychain:&error];

    if(error && [error.domain isEqualToString:NSOSStatusErrorDomain] && error.code == errSecItemNotFound) {
        // Fair enough! There are no requests waiting for a passcode.
        secnotice("escrowrequest", "no extant requests");
        reply(@{}, nil);
        return;
    }

    if(error) {
        secerror("escrowrequest: failed to load requests: %@", error);
        reply(nil, error);
    }

    secnotice("escrowrequest", "found requests: %@", records);

    NSMutableDictionary<NSString*, NSString*>* d = [NSMutableDictionary dictionary];
    for(SecEscrowPendingRecord* record in records) {
        if(record.uploadCompleted) {
            d[record.uuid] = @"complete";
        } else if(record.hasSerializedPrerecord) {
            d[record.uuid] = SecEscrowRequestHavePrecord;
        } else if(record.certCached) {
            d[record.uuid] = SecEscrowRequestPendingPasscode;
        } else {
            d[record.uuid] = SecEscrowRequestPendingCertificate;
        }
    }

    reply(d, nil);
}

- (void)resetAllRequests:(void (^)(NSError* _Nullable error))reply
{
    secnotice("escrowrequest", "deleting all requests");

    NSError* error = nil;
    NSArray<SecEscrowPendingRecord*>* records = [SecEscrowPendingRecord loadAllFromKeychain:&error];

    if(error && [error.domain isEqualToString:NSOSStatusErrorDomain] && error.code == errSecItemNotFound) {
        // Fair enough! There are no requests waiting for a passcode.
        secnotice("escrowrequest", "no extant requests; nothing to delete");
        reply(nil);
        return;
    }

    if(error) {
        secnotice("escrowrequest", "error fetching records: %@", error);
        reply(error);
        return;
    }

    for(SecEscrowPendingRecord* record in records) {
        [record deleteFromKeychain:&error];
        if(error) {
            secnotice("escrowrequest", "Unable to delete %@: %@", record, error);
        }
    }

    // Report the last error, if any.
    reply(error);
}

- (void)storePrerecordsInEscrow:(void (^)(uint64_t count, NSError* _Nullable error))reply
{
    secnotice("escrowrequest", "attempting to store a prerecord in escrow");

    [self.controller storePrerecordsInEscrowRPC:reply];
}

- (void)setupAnalytics
{
    [[CKKSAnalytics logger] AddMultiSamplerForName:@"escorwrequest-healthSummary" withTimeInterval:SFAnalyticsSamplerIntervalOncePerReport block:^NSDictionary<NSString *,NSNumber *> *{
        NSMutableDictionary<NSString *,NSNumber*>* values = [NSMutableDictionary dictionary];

        NSDate *date = [[CKKSAnalytics logger] datePropertyForKey:ESRPendingSince];
        if (date) {
            values[ESRPendingSince] = @([CKKSAnalytics fuzzyDaysSinceDate:date]);
        }

        return values;
    }];
}

@end
