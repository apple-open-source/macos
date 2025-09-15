//
//  PMPowerMitigations.m
//  LowPowerMode-Embedded
//
//  Created by Prateek Malhotra on 11/20/24.
//

#import <os/log.h>
#import <os/transaction_private.h>
#import <notify.h>
#import <Foundation/Foundation.h>

#import "PMPowerMitigations.h"
#import "PMPowerMitigationsServiceProtocol.h"
#import "PMPowerMitigationsServiceCallbackProtocol.h"

NSString *const kPowerMitigationsManagerService = @"com.apple.powerexperienced.powermitigationsmanager.service";

NSString *const kPowerMitigationsManagerServiceStartupNotify = @"com.apple.powerexperienced.powermitigationsmanager.restart";

@interface PMPowerMitigations ()

@property NSMutableDictionary<ClientIdentifier *, NSMutableSet<id<PMPowerMitigationsObserver>> *> *observersByClientId;
@property dispatch_queue_t queue;
@property os_log_t log;

@property (nonatomic, retain) NSXPCConnection *connection;
@property BOOL connectionInterrupted;

@property (retain) PMPowerMitigationSession *currentMitigationSession;

@end

@implementation PMPowerMitigations

+ (instancetype)sharedInstance {
    static PMPowerMitigations *instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [[PMPowerMitigations alloc] init];
    });
    return instance;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        _observersByClientId = [NSMutableDictionary dictionary];
        _queue = dispatch_queue_create("com.apple.lowpowermode.pmpowermitigations.queue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _log = os_log_create("com.apple.lowpowermode", "PMPowerMitigations");
        _currentMitigationSession = nil;

        _connectionInterrupted = NO;
        [self initConnectionToService];

        if (_connection) {
            [self configureConnectionInterruptionHandler];
            [self configureConnectionInvalidationHandler];
            [self configureConnectionReconnectionOnServiceRestart];
            [_connection activate];
            [self registerForMitigationUpdates];
        }
    }
    return self;
}

#pragma mark NSXPCConnection
- (void)initConnectionToService {
    _connection = [[NSXPCConnection alloc] initWithMachServiceName:kPowerMitigationsManagerService
                                                           options:NSXPCConnectionPrivileged];
    if (!_connection) {
        os_log_error(_log, "Failed to connect to %@", kPowerMitigationsManagerService);
    }
    os_log_info(_log, "established connection to %@", kPowerMitigationsManagerService);

    _connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(PMPowerMitigationsServiceProtocol)];
    _connection.exportedObject = self;
    _connection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(PMPowerMitigationsServiceCallbackProtocol)];
}

- (void)configureConnectionInterruptionHandler {
    __weak typeof(self) welf = self;
    [_connection setInterruptionHandler:^{
        typeof(self) strongSelf = welf;
        if (!strongSelf) {
            return;
        }
        os_log_info(strongSelf.log, "Connection to %@ interrupted", kPowerMitigationsManagerService);
        strongSelf.connectionInterrupted = YES;
    }];

}

- (void)configureConnectionInvalidationHandler {
    __weak typeof(self) welf = self;
    [_connection setInvalidationHandler:^{
        typeof(self) strongSelf = welf;
        if (!strongSelf) {
            return;
        }
        os_log_error(strongSelf.log, "Connection to %@ invalidated", kPowerMitigationsManagerService);
    }];
}

- (void)configureConnectionReconnectionOnServiceRestart {
    __weak typeof(self) welf = self;
    static int syncToken;
    int status = notify_register_dispatch("com.apple.powerexperienced.restart", &syncToken, _queue, ^(int token __unused) {
        typeof(self) strongSelf = welf;
        if (!strongSelf) {
            return;
        }
        if (strongSelf.connectionInterrupted) {
            os_log(strongSelf.log, "%@ has restarted", kPowerMitigationsManagerService);
            [strongSelf registerForMitigationUpdates];
            strongSelf.connectionInterrupted = NO;
        }
    });
    if (status != NOTIFY_STATUS_OK) {
        os_log_error(_log, "Failed to register for reconnections with service with status:0x%x", status);
    }
}

- (void)registerForMitigationUpdates {
    os_log_info(self.log, "Registering for mitigation updates with %@", kPowerMitigationsManagerService);
    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            os_log_error(self.log, "Failed to connect to service. Error: %@", error);
    }] registerForPowerMitigations];
}

#pragma mark PMPowerMitigationsObservable
- (void)addObserver:(id<PMPowerMitigationsObserver>)observer forClientIdentifier:(ClientIdentifier *)clientIdentifier {
    dispatch_async(_queue, ^{
        if (![self.observersByClientId objectForKey:clientIdentifier]) {
            self.observersByClientId[clientIdentifier] = [NSMutableSet set];
        }
        [self.observersByClientId[clientIdentifier] addObject:observer];
    });
}

- (void)removeObserver:(id<PMPowerMitigationsObserver>)observer forClientIdentifier:(ClientIdentifier *)clientIdentifier {
    dispatch_async(_queue, ^{
        if ([self.observersByClientId objectForKey:clientIdentifier]) {
            [self.observersByClientId[clientIdentifier] removeObject:observer];
        }
    });

}

- (PMPowerMitigationInfo *)newMitigationInfoForClientIdentifier:(ClientIdentifier *)clientIdentifier {
    // For now, all clients get the systemWideMitigationLevel
    PMPowerMitigationInfo *mitigationInfoForClient = nil;
    PMMitigationLevel mitigationLevelForClient;

    if (!_currentMitigationSession) {
        mitigationLevelForClient = PMMitigationLevelNone;
    }
    else {
        mitigationLevelForClient = _currentMitigationSession.systemWideMitigationLevel;
    }
    mitigationInfoForClient =  [[PMPowerMitigationInfo alloc] initWithMitigationLevel:mitigationLevelForClient
                                                                     clientIdentifier:clientIdentifier];
    return mitigationInfoForClient;
}

- (PMPowerMitigationInfo *)copyCurrentMitigationInfoForClientIdentifier:(ClientIdentifier *)clientIdentifier {
    __block PMPowerMitigationInfo *mitigationInfoForClient = nil;
    dispatch_sync(_queue, ^() {
        mitigationInfoForClient = [self newMitigationInfoForClientIdentifier:clientIdentifier];
    });
    return mitigationInfoForClient;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-implementations"
- (PMPowerMitigationInfo *)getCurrentMitigationInfoForClientIdentifier:(ClientIdentifier *)clientIdentifier {
    return [self copyCurrentMitigationInfoForClientIdentifier:clientIdentifier];
}
#pragma clang diagnostic pop


#pragma mark PMPowerMitigationsServiceCallbackProtocol
- (void)didUpdateToMitigation:(PMPowerMitigationSession *)newMitigation
               fromMitigation:(PMPowerMitigationSession *)oldMitigation {
    NS_VALID_UNTIL_END_OF_SCOPE os_transaction_t transaction = os_transaction_create("com.apple.pmpowermitigations.update");

    dispatch_async(_queue, ^{
        (void)transaction;
        os_log_info(self.log, "Received updated PowerMitigationSession info with level: %ld triggered by source %@.", (long)newMitigation.systemWideMitigationLevel, newMitigation.sessionSource);

        BOOL levelHasChanged = NO;

        if (!self.currentMitigationSession || (self.currentMitigationSession.systemWideMitigationLevel != newMitigation.systemWideMitigationLevel)) {
            self.currentMitigationSession = newMitigation;
            levelHasChanged = YES;
        }

        // Only notify observers if systemWideMitigationLevel has changed
        // Assumes client-specific levels won't change without a systemwide level change
        if (levelHasChanged) {
            os_log_info(self.log, "Updating observers as mitigation levels have changed.");
            for (ClientIdentifier *key in self.observersByClientId) {
                os_log_debug(self.log, "Updating observers for clientId: %@", key);
                for (id<PMPowerMitigationsObserver> observer in self.observersByClientId[key]) {
                    PMPowerMitigationInfo *mitigationInfo = [self newMitigationInfoForClientIdentifier:key];
                    [observer didChangeToMitigations:mitigationInfo withSessionInfo:newMitigation];
                }
            }
        }
    });
}

@end
