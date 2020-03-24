
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ot/categories/OTAccountMetadataClassC+KeychainSupport.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTCuttlefishAccountStateHolder ()
@property dispatch_queue_t queue;
@property dispatch_queue_t notifyQueue;

@property NSString* containerName;
@property NSString* contextID;

@property NSMutableSet<id<OTCuttlefishAccountStateHolderNotifier>>* monitors;
@end

@implementation OTCuttlefishAccountStateHolder

- (instancetype)initWithQueue:(dispatch_queue_t)queue
                    container:(NSString*)containerName
                      context:(NSString*)contextID
{
    if((self = [super init])) {
        _queue = queue;
        _notifyQueue = dispatch_queue_create("OTCuttlefishAccountStateHolderNotifier", NULL);
        _containerName = containerName;
        _contextID = contextID;
        _monitors = [NSMutableSet set];
    }
    return self;
}

- (void)registerNotification:(id<OTCuttlefishAccountStateHolderNotifier>)notifier
{
    [self.monitors addObject:notifier];
}

- (OTAccountMetadataClassC* _Nullable)loadOrCreateAccountMetadata:(NSError**)error
{
    __block OTAccountMetadataClassC* metadata = nil;
    __block NSError* localError = nil;
    dispatch_sync(self.queue, ^{
        metadata = [self _onqueueLoadOrCreateAccountMetadata:&localError];
    });

    if(error && localError) {
        *error = localError;
    }
    return metadata;
}

- (OTAccountMetadataClassC* _Nullable)_onqueueLoadOrCreateAccountMetadata:(NSError**)error
{
    dispatch_assert_queue(self.queue);

    NSError* localError = nil;
    OTAccountMetadataClassC* current = [OTAccountMetadataClassC loadFromKeychainForContainer:self.containerName contextID:self.contextID error:&localError];

    if(!current || localError) {
        if([localError.domain isEqualToString:NSOSStatusErrorDomain] && localError.code == errSecItemNotFound) {
            // That's okay, this is the first time we're saving this.
            current = [[OTAccountMetadataClassC alloc] init];
            current.attemptedJoin = OTAccountMetadataClassC_AttemptedAJoinState_NOTATTEMPTED;
        } else {
            // No good.
            if(error) {
                *error = localError;
            }
            return nil;
        }
    }

    return current;
}

- (NSString * _Nullable)getEgoPeerID:(NSError * _Nullable *)error {
    NSError* localError = nil;

    OTAccountMetadataClassC* current = [self loadOrCreateAccountMetadata:&localError];

    if(localError || !current) {
        if(error) {
            *error = localError;
        }
        return nil;
    }

    if(!current.peerID) {
        if(error) {
            *error = [NSError errorWithDomain:OTCuttlefishContextErrorDomain
                                         code:OTCCNoExistingPeerID
                                  description:@"No existing ego peer ID"];
        }

        return nil;
    }

    return current.peerID;
}

- (OTAccountMetadataClassC_AttemptedAJoinState)fetchPersistedJoinAttempt:(NSError * _Nullable *)error {
    NSError* localError = nil;
    OTAccountMetadataClassC* current = [self loadOrCreateAccountMetadata:&localError];

    if(localError || !current) {
        if(error) {
            *error = localError;
        }
        return OTAccountMetadataClassC_AttemptedAJoinState_UNKNOWN;
    }
    return current.attemptedJoin;
}

- (NSDate *)lastHealthCheckupDate:(NSError * _Nullable *)error {
    NSError* localError = nil;

    OTAccountMetadataClassC* current = [self loadOrCreateAccountMetadata:&localError];

    if(localError || !current) {
        if(error) {
            *error = localError;
        }
        return NULL;
    }
    
    return [NSDate dateWithTimeIntervalSince1970: ((NSTimeInterval)current.lastHealthCheckup) / 1000.0];
}


- (BOOL)persistNewEgoPeerID:(NSString*)peerID error:(NSError**)error {
    return [self persistAccountChanges:^OTAccountMetadataClassC * _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
        metadata.peerID = peerID;
        return metadata;
    } error:error];
}

- (BOOL)persistNewTrustState:(OTAccountMetadataClassC_TrustState)newState
                         error:(NSError**)error
{
    return [self persistAccountChanges:^(OTAccountMetadataClassC *metadata) {
        metadata.trustState = newState;
        return metadata;
    } error:error];
}

- (BOOL)persistNewAccountState:(OTAccountMetadataClassC_AccountState)newState
               optionalAltDSID:(NSString* _Nullable)altDSID
                         error:(NSError**)error
{
    return [self persistAccountChanges:^(OTAccountMetadataClassC *metadata) {
        metadata.icloudAccountState = newState;
        metadata.altDSID = altDSID;
        return metadata;
    } error:error];
}

- (BOOL)persistNewEpoch:(uint64_t)epoch
                  error:(NSError**)error
{
    return [self persistAccountChanges:^(OTAccountMetadataClassC *metadata) {
        metadata.epoch = epoch;
        return metadata;
    } error:error];
}

- (BOOL)persistAccountChanges:(OTAccountMetadataClassC* _Nullable (^)(OTAccountMetadataClassC*))makeChanges
                        error:(NSError**)error
{
    __block NSError* localError = nil;
    __block OTAccountMetadataClassC* newState = nil;
    __block OTAccountMetadataClassC* oldState = nil;

    dispatch_sync(self.queue, ^void {
        oldState = [self _onqueueLoadOrCreateAccountMetadata:&localError];
        if(!oldState) {
            return;
        }

        newState = makeChanges([oldState copy]);
        if(newState && ![newState saveToKeychainForContainer:self.containerName contextID:self.contextID error:&localError]) {
            newState = nil;
        }
    });

    if(localError && error) {
        *error = localError;
    }

    if (newState) {
        [self asyncNotifyAccountStateChanges:newState from:oldState];
        return YES;
    } else {
        return NO;
    }
}

- (BOOL)persistLastHealthCheck:(NSDate*)lastCheck
                         error:(NSError**)error
{
    return [self persistAccountChanges:^(OTAccountMetadataClassC *metadata) {
        metadata.lastHealthCheckup = (uint64_t) ([lastCheck timeIntervalSince1970] * 1000);
        return metadata;
    } error:error];
}

- (BOOL)persistOctagonJoinAttempt:(OTAccountMetadataClassC_AttemptedAJoinState)attempt
                            error:(NSError**)error
{
    return [self persistAccountChanges:^(OTAccountMetadataClassC *metadata) {
        metadata.attemptedJoin = attempt;
        return metadata;
    } error:error];
}

- (BOOL)_onqueuePersistAccountChanges:(OTAccountMetadataClassC* _Nullable (^)(OTAccountMetadataClassC* metadata))makeChanges
                                error:(NSError**)error
{
    __block NSError* localError = nil;
    dispatch_assert_queue(self.queue);

    OTAccountMetadataClassC *oldState = nil;
    OTAccountMetadataClassC *newState = nil;

    oldState = [self _onqueueLoadOrCreateAccountMetadata:&localError];
    if(oldState) {
        newState = makeChanges([oldState copy]);

        if(![newState saveToKeychainForContainer:self.containerName contextID:self.contextID error:&localError]) {
            newState = nil;
        }
    }

    if (error && localError) {
        *error = localError;
    }

    if (newState) {
        [self asyncNotifyAccountStateChanges:newState from:oldState];
        return YES;
    } else {
        return NO;
    }
}

- (void)asyncNotifyAccountStateChanges:(OTAccountMetadataClassC *)newState from:(OTAccountMetadataClassC *)oldState
{
    WEAKIFY(self);

    dispatch_async(self.notifyQueue, ^{
        STRONGIFY(self);

        for (id<OTCuttlefishAccountStateHolderNotifier> monitor in self.monitors) {
            [monitor accountStateUpdated:newState from:oldState];
        }
    });
}


@end
