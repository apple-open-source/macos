//
//  PMExtendedBattery.m
//  LowPowerMode-Embedded
//
//  Created by Pablo Pons Bordes on 2/26/25.
//

#import <TargetConditionals.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSPrivate.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <dispatch/dispatch.h>
#import <os/lock.h>
#import <os/feature_private.h>
#import <LowPowerMode/PMExtendedBatteryProtocol.h>

#import "_PMExtendedBatteryClientEventsManager.h"
#import "_PMWeakProxy.h"
#import "PMExtendedBattery.h"

#if WATCH_POWER_RANGER

#pragma mark - Constants & Utils

NSString *const kPMEBSourceCarousel = @"Carousel";
NSString *const kPMEBSourceSettings = @"Settings";
NSString *const kPMEBSourcePMTool   = @"pmtool";

static PMExtendedBatteryFeatureAvailable disabledFeatureAvailable = PMExtendedBatteryFeatureAvailableUnknown;
static PMExtendedBatteryState disabledFeatureState = PMExtendedBatteryStateOff;

typedef NS_OPTIONS(NSUInteger, PMExtendedBatteryInfo) {
    PMExtendedBatteryInfoFeatureAvailable   = 1 << 0,
    PMExtendedBatteryInfoState              = 1 << 1,
    PMExtendedBatteryInfoAll                = (PMExtendedBatteryInfoFeatureAvailable | PMExtendedBatteryInfoState)
};
#define PMExtendedBatteryInfoNone 0


#define CONDITIONAL_NUMBER(__returnNumber, __value) (__returnNumber ? @(__value) : nil)

NS_ASSUME_NONNULL_BEGIN

#pragma mark - Extension

@interface PMExtendedBattery () <PMExtendedBatteryClientProtocol, _PMExtendedBatteryClientEventsManagerObserver>
{
    id<PMExtendedBatteryDelegate> _delegate;
    dispatch_queue_t _delegateQueue;

    os_unfair_lock _lock;

    PMExtendedBatteryFeatureAvailable _featureAvailable;
    PMExtendedBatteryState _state;
    BOOL _dirtyInfo;

    NSXPCConnection *_connection;
    BOOL _connectionInterrupted;
}

@end

#pragma mark - Implementation

@implementation PMExtendedBattery

#pragma mark - Instance Lifecycle

- (instancetype)initWithDelegate:(id<PMExtendedBatteryDelegate> _Nullable)delegate queue:(dispatch_queue_t _Nullable)delegateQueue {
    self = [super init];

    if (self != nil) {
        _delegate = delegate;
        _delegateQueue = delegateQueue ?: dispatch_queue_create("com.apple.powerd.extendedbattery.delegate", NULL);

        _lock = OS_UNFAIR_LOCK_INIT;
        _featureAvailable = PMExtendedBatteryFeatureAvailableUnknown;
        _state = PMExtendedBatteryStateOff;
        _dirtyInfo = YES;
        _connectionInterrupted = NO;

        if (PMExtendedBatteryFeatureFlagEnabled()) {
            // create connection only if the feature is enabled
            _connection = [self _connection];
            [_connection activate];
            [[_PMExtendedBatteryClientEventsManager sharedInstance] registerObserver:self
                                                                            forEvent:PMExtendedBatteryXPCEventUpdatedInfo];
        }
    }

    return self;
}

- (instancetype)init
{
    return [self initWithDelegate:nil queue:nil];
}

- (void)dealloc {
    [[_PMExtendedBatteryClientEventsManager sharedInstance] unregisterObserver:self];
    [_connection invalidate];
    _connection = nil;
}

#pragma mark Delegate

- (id<PMExtendedBatteryDelegate> _Nullable)delegate {
    __block id<PMExtendedBatteryDelegate> delegate;
    dispatch_sync(_delegateQueue, ^{
        delegate = _delegate;
    });
    return delegate;
}

- (void)setDelegate:(id<PMExtendedBatteryDelegate> _Nullable)delegate {
    dispatch_async(_delegateQueue, ^{
        self->_delegate = delegate;
    });
}

#pragma mark - Obtain & set Information

#pragma mark Feature Available

- (void)featureAvailableWithCompletion:(PMGetExtendedBatteryAvailableCompletionHandler)handler {
    os_log_debug(pm_extendedBattery_log() ,"%s, featureAvailable: %@",  __FUNCTION__, NSStringFromPMExtendedBatteryFeatureAvailable(_featureAvailable));
    [self _infoSynchronous:NO
              relevantInfo:PMExtendedBatteryInfoFeatureAvailable
            withCompletion:^(PMExtendedBatteryFeatureAvailable available, PMExtendedBatteryState state, NSError * _Nullable error) {
        handler(available, error);
    }];
}

- (PMExtendedBatteryFeatureAvailable)featureAvailable {
    __block PMExtendedBatteryFeatureAvailable outAvailable;
    [self _infoSynchronous:YES
              relevantInfo:PMExtendedBatteryInfoFeatureAvailable
            withCompletion:^(PMExtendedBatteryFeatureAvailable available, PMExtendedBatteryState state, NSError * _Nullable error) {
        outAvailable = available;
    }];

    return outAvailable;
}

- (void)setFeatureAvailable:(PMExtendedBatteryFeatureAvailable)featureAvailable
                 fromSource:(NSString *)source
             withCompletion:(PMSetExtendedBatteryAvailableCompletionHandler)handler {
    [self _setFeatureAvailable:featureAvailable fromSource:source synchronous:NO withCompletion:handler];
}

- (void)setFeatureAvailable:(PMExtendedBatteryFeatureAvailable)featureAvailable fromSource:(nonnull NSString *)source {
    [self _setFeatureAvailable:featureAvailable fromSource:source synchronous:YES withCompletion:nil];
}

#pragma mark State

- (void)stateWithCompletion:(nonnull PMGetExtendedBatteryStateCompletionHandler)handler {
    [self _infoSynchronous:NO
              relevantInfo:PMExtendedBatteryInfoState
            withCompletion:^(PMExtendedBatteryFeatureAvailable available, PMExtendedBatteryState state, NSError * _Nullable error) {
        handler(state, error);
    }];
}

- (PMExtendedBatteryState)state {
    __block PMExtendedBatteryState outState;
    [self _infoSynchronous:YES
              relevantInfo:PMExtendedBatteryInfoState
            withCompletion:^(PMExtendedBatteryFeatureAvailable available, PMExtendedBatteryState state, NSError * _Nullable error) {
        outState = state;
    }];

    return outState;
}

- (void)setState:(PMExtendedBatteryState)state
      fromSource:(NSString *)source
  withCompletion:(PMSetExtendedBatteryStateCompletionHandler)handler {
    [self _setState:state fromSource:source synchronous:NO withCompletion:handler];
}

- (void)setState:(PMExtendedBatteryState)state fromSource:(nonnull NSString *)source {
    [self _setState:state fromSource:source synchronous:YES withCompletion:nil];
}

#pragma mark - <PMExtendedBatteryClientProtocol>

// Received updated from the remote service
- (void)remoteServiceDidUpdateFeatureAvailable:(PMExtendedBatteryFeatureAvailable)available
                                         state:(PMExtendedBatteryState)state {
    os_log(pm_extendedBattery_log() ,"Did receive update featureAvailable:%@ state: %@",
           NSStringFromPMExtendedBatteryFeatureAvailable(available),
           NSStringFromPMExtendedBatteryState(state));

    [self _updateIfNecessaryAvailable:@(available) sate:@(state) notifyDelegate:PMExtendedBatteryInfoAll];
}

#pragma mark <_PMExtendedBatteryClientEventsManagerObserver>

// handle XPC events from remote service
- (void)extendedBatteryClientEventsManager:(_PMExtendedBatteryClientEventsManager *)manager
                          didReceivedEvent:(NSString *)eventName {

    os_unfair_lock_lock(&_lock);
    BOOL connectionInterrupted = _connectionInterrupted;
    os_unfair_lock_unlock(&_lock);
    if (connectionInterrupted) {
        os_log(pm_extendedBattery_log(), "Have Interrupted connection, requesting updated Info");
        __weak typeof(self) weakSelf = self;
        [self _infoSynchronous:NO
                  relevantInfo:PMExtendedBatteryInfoNone
                withCompletion:^(PMExtendedBatteryFeatureAvailable available,
                                 PMExtendedBatteryState state,
                                 NSError * _Nullable error) {
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (strongSelf == nil) return;

            if (error == nil) {
                os_unfair_lock_lock(&(strongSelf->_lock));
                strongSelf->_connectionInterrupted = NO;
                os_unfair_lock_unlock(&(strongSelf->_lock));
            } else {
                os_log_error(pm_extendedBattery_log(), "Fail to receive updated info with error:%@", error);
            }
        }];
    } else {
        os_log(pm_extendedBattery_log(), "Have valid connection, No need to request updated Info");
    }
}

#pragma mark - Utilities

#pragma mark - Communicate with remote Service

- (void)_infoSynchronous:(BOOL)synchronous
            relevantInfo:(PMExtendedBatteryInfo)relevantInfo
          withCompletion:(nonnull PMExtendedBatteryCompletionHandler)handler {
    if (PMExtendedBatteryFeatureFlagEnabled() == NO) {
        os_log_error(pm_extendedBattery_log() ,"Feature not enabled");
        handler(disabledFeatureAvailable, disabledFeatureState, nil);
        return;
    }

    // If we have value already cache we return it immediately
    os_unfair_lock_lock(&_lock);
    BOOL dirtyInfo = _dirtyInfo;
    PMExtendedBatteryFeatureAvailable available = _featureAvailable;
    PMExtendedBatteryState state = _state;
    os_unfair_lock_unlock(&_lock);
    if (dirtyInfo == NO) {
        os_log(pm_extendedBattery_log(), "Cached Info, featureAvailable: %@, state: %@",
               NSStringFromPMExtendedBatteryFeatureAvailable(available),
               NSStringFromPMExtendedBatteryState(state));
        handler(available, state, nil);
        return;
    }

    // Otherwise we go to the remove process to fetch it
    __weak typeof(self) weakSelf = self;
    __auto_type wrapperHandler = ^(PMExtendedBatteryFeatureAvailable available,
                                   PMExtendedBatteryState state,
                                   NSError *_Nullable error) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf == nil) return;

        if (error == nil) {
            // We notify over the delegate the data that is not relevant to the caller.
            PMExtendedBatteryInfo delegateRelevantInfo = (PMExtendedBatteryInfoAll ^ relevantInfo);
            [strongSelf _updateIfNecessaryAvailable:@(available) sate:@(state) notifyDelegate:delegateRelevantInfo];
        }
        handler(available, state, error);
    };

    __auto_type proxy = [self _synchronousRemoteProxyObject:synchronous withErrorHandler:^(NSError * _Nonnull error) {
        NSError *outError = error;
        if ([outError.domain isEqualToString:PMExtendedBatteryErrorDomain] == NO) {
            outError = PMExtendedBatteryError(PMExtendedBatteryErrorCodeConnection,
                                              @"fail to connect with remote service",
                                              error);
        }
        os_unfair_lock_lock(&(self->_lock));
        self->_dirtyInfo = YES;
        PMExtendedBatteryFeatureAvailable available = self->_featureAvailable;
        PMExtendedBatteryState state = self->_state;
        os_unfair_lock_unlock(&(self->_lock));

        os_log_error(pm_extendedBattery_log(), "Fail getting current info. forwarding current, featureAvailable: %@, state: %@, error: %@",
                     NSStringFromPMExtendedBatteryFeatureAvailable(available),
                     NSStringFromPMExtendedBatteryState(state),
                     outError);
        wrapperHandler(available, state, outError);

        (void)self; // retain self to ensure XPC connection is open until we receives a respond from the remote service.
    }];
    [proxy extendedBatteryInfoWithCompletion:wrapperHandler];
}

- (void)_setFeatureAvailable:(PMExtendedBatteryFeatureAvailable)newAvailable
                  fromSource:(NSString *)source
                 synchronous:(BOOL)synchronous
              withCompletion:(PMSetExtendedBatteryAvailableCompletionHandler _Nullable)handler {
    if (PMExtendedBatteryFeatureFlagEnabled() == NO) {
        os_log_error(pm_extendedBattery_log() ,"Feature not enabled");
        handler(disabledFeatureAvailable, disabledFeatureState, nil);
        return;
    }

    os_log_debug(pm_extendedBattery_log() ,"Setting new featureAvailable: %@, source:%@ synchronous:%{BOOL}u",
                 NSStringFromPMExtendedBatteryFeatureAvailable(newAvailable),
                 source,
                 synchronous);

    // Handler Wrapper
    __weak typeof(self) weakSelf = self;
    __auto_type wrapperHandler = ^(PMExtendedBatteryFeatureAvailable available, PMExtendedBatteryState state, NSError * _Nullable error) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf == nil) return;

        if (error == nil) {
            os_log(pm_extendedBattery_log() ,"Set new featureAvailable:%@ state: %@",
                   NSStringFromPMExtendedBatteryFeatureAvailable(available),
                   NSStringFromPMExtendedBatteryState(state));
        } else {
            os_log_error(pm_extendedBattery_log(), "Failed setting new featureAvailable: %@, current featureAvailable: %@ state:%@, error:%@",
                         NSStringFromPMExtendedBatteryFeatureAvailable(newAvailable),
                         NSStringFromPMExtendedBatteryFeatureAvailable(available),
                         NSStringFromPMExtendedBatteryState(state),
                         error);
        }

        BOOL expectedErrorDomain = ([error.domain isEqualToString:PMExtendedBatteryErrorDomain]);
        BOOL isConnectionError = (error.code == PMExtendedBatteryErrorCodeConnection);
        BOOL shouldUpdateInfo = ((error == nil)  || (expectedErrorDomain && (isConnectionError == NO)));

        // Update with new values if necessary
        os_unfair_lock_lock(&(self->_lock));
        PMExtendedBatteryState currentState = strongSelf->_state;
        if (shouldUpdateInfo) {
            strongSelf->_featureAvailable = available;
            strongSelf->_state = state;
            strongSelf->_dirtyInfo = NO;
        } else {
            strongSelf->_dirtyInfo = YES;
        }
        os_unfair_lock_unlock(&(self->_lock));

        // call back handler
        if (handler != nil) handler(available, state, error);

        // Notify delegate if necessary
        BOOL shouldNotifyFeatureAvailableUpdate = (newAvailable != available);
        BOOL shouldNotifyStateUpdate = (currentState != state);
        [strongSelf _notifyDelegateAvailable:CONDITIONAL_NUMBER(shouldNotifyFeatureAvailableUpdate, available)
                                        sate:CONDITIONAL_NUMBER(shouldNotifyStateUpdate, state)];
    };

    // Call remote service
    __auto_type proxy = [self _synchronousRemoteProxyObject:synchronous withErrorHandler:^(NSError * _Nonnull error) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf == nil) return;
        NSError *outError = PMExtendedBatteryError(PMExtendedBatteryErrorCodeConnection,
                                                   @"fail to connect with remote service",
                                                   error);
        // Th connection was broken we have nothing better than the current state.
        os_unfair_lock_lock(&(strongSelf->_lock));
        PMExtendedBatteryFeatureAvailable currentFeatureAvailable = strongSelf->_featureAvailable;
        PMExtendedBatteryState currentState = strongSelf->_state;
        os_unfair_lock_unlock(&(strongSelf->_lock));

        wrapperHandler(currentFeatureAvailable, currentState, outError);
    }];
    [proxy setFeatureAvailable:newAvailable fromSource:source withCompletion:wrapperHandler];
}

- (void)_setState:(PMExtendedBatteryState)newState
       fromSource:(NSString *)source
      synchronous:(BOOL)synchronous
   withCompletion:(PMSetExtendedBatteryStateCompletionHandler _Nullable)handler {
    if (PMExtendedBatteryFeatureFlagEnabled() == NO) {
        os_log_error(pm_extendedBattery_log() ,"Feature not enabled");
        handler(disabledFeatureState, nil);
        return;
    }

    os_log(pm_extendedBattery_log() ,"Setting new state: %@, source:%@ synchronous:%{BOOL}u",
           NSStringFromPMExtendedBatteryState(newState),
           source,
           synchronous);

    // Handler Wrapper
    __weak typeof(self) weakSelf = self;
    __auto_type wrapperHandler = ^(PMExtendedBatteryFeatureAvailable available, PMExtendedBatteryState state, NSError * _Nullable error) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf == nil) return;

        if (error == nil) {
            os_log(pm_extendedBattery_log() ,"Set new state: %@", NSStringFromPMExtendedBatteryState(state));
        } else {
            os_log_error(pm_extendedBattery_log(), "Failed setting new state:%@, current featureAvailable: %@ state:%@, error:%@",
                         NSStringFromPMExtendedBatteryState(newState),
                         NSStringFromPMExtendedBatteryFeatureAvailable(available),
                         NSStringFromPMExtendedBatteryState(state),
                         error);
        }

        BOOL expectedErrorDomain = ([error.domain isEqualToString:PMExtendedBatteryErrorDomain]);
        BOOL isConnectionError = (error.code == PMExtendedBatteryErrorCodeConnection);
        BOOL shouldUpdateInfo = ((error == nil)  || (expectedErrorDomain && (isConnectionError == NO)));

        // Update with new values if necessary
        os_unfair_lock_lock(&(self->_lock));
        PMExtendedBatteryFeatureAvailable currentFeatureAvailable = strongSelf->_featureAvailable;
        if (shouldUpdateInfo) {
            strongSelf->_featureAvailable = available;
            strongSelf->_state = state;
            strongSelf->_dirtyInfo = NO;
        } else {
            strongSelf->_dirtyInfo = YES;
        }
        os_unfair_lock_unlock(&(self->_lock));

        // call back handler
        if (handler != nil) handler(state, error);

        // Notify delegate if necessary
        BOOL shouldNotifyFeatureAvailableUpdate = (currentFeatureAvailable != available);
        BOOL shouldNotifyStateUpdate = (newState != state);
        [strongSelf _notifyDelegateAvailable:CONDITIONAL_NUMBER(shouldNotifyFeatureAvailableUpdate, available)
                                        sate:CONDITIONAL_NUMBER(shouldNotifyStateUpdate, state)];
    };

    // Call remote service
    __auto_type proxy = [self _synchronousRemoteProxyObject:synchronous withErrorHandler:^(NSError * _Nonnull error) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf == nil) return;
        NSError *outError = PMExtendedBatteryError(PMExtendedBatteryErrorCodeConnection,
                                                   @"fail to connect with remote service",
                                                   error);
        // Th connection was broken we have nothing better than the current state.
        os_unfair_lock_lock(&(strongSelf->_lock));
        PMExtendedBatteryFeatureAvailable currentFeatureAvailable = strongSelf->_featureAvailable;
        PMExtendedBatteryState currentState = strongSelf->_state;
        os_unfair_lock_unlock(&(strongSelf->_lock));

        wrapperHandler(currentFeatureAvailable, currentState, outError);
    }];
    [proxy setState:newState fromSource:source withCompletion:wrapperHandler];
}

#pragma mark Update Status

/// Update the given values. if nil it will be ignored
- (void)_updateIfNecessaryAvailable:(NSNumber *_Nullable)availableNumber
                               sate:(NSNumber *_Nullable)stateNumber
                     notifyDelegate:(PMExtendedBatteryInfo)infoToNotify {
    BOOL featureAvailableDidChange = NO;
    BOOL stateDidChange = NO;
    PMExtendedBatteryFeatureAvailable available = PMExtendedBatteryFeatureAvailableFromNSInteger(availableNumber.integerValue);
    PMExtendedBatteryState state = PMExtendedBatteryStateFromNSInteger(stateNumber.intValue);

    // Update internal variables
    os_unfair_lock_lock(&_lock);
    featureAvailableDidChange = ((availableNumber != nil ) && (_featureAvailable != available));
    _featureAvailable = available;
    stateDidChange = ((stateNumber != nil) && (_state != state));
    _state = state;
    // If both values were provided we can mark as not dirty
    if ((_dirtyInfo == YES) && (availableNumber != nil) && (stateNumber != nil)) {
        _dirtyInfo = NO;
    }
    os_unfair_lock_unlock(&_lock);

    if (featureAvailableDidChange && stateDidChange) {
        os_log(pm_extendedBattery_log() ,"Did update featureAvailable: %@, state: %@",
               NSStringFromPMExtendedBatteryFeatureAvailable(available),
               NSStringFromPMExtendedBatteryState(state));
    } else if (featureAvailableDidChange) {
        os_log(pm_extendedBattery_log() ,"Did update featureAvailable: %@",
               NSStringFromPMExtendedBatteryFeatureAvailable(available));
    } else if (stateDidChange) {
        os_log(pm_extendedBattery_log() ,"Did update state: %@",
               NSStringFromPMExtendedBatteryState(state));
    }

    // Notify delegate what it change if necessary
    if (infoToNotify & PMExtendedBatteryInfoAll) {
        BOOL shouldNotifyFeatureAvailable = featureAvailableDidChange && (infoToNotify & PMExtendedBatteryInfoFeatureAvailable);
        BOOL shouldNotifyState = stateDidChange && (infoToNotify & PMExtendedBatteryInfoState);

        [self _notifyDelegateAvailable:CONDITIONAL_NUMBER(shouldNotifyFeatureAvailable, available)
                                  sate:CONDITIONAL_NUMBER(shouldNotifyState, state)];
    }
}

#pragma mark Notify Delegate

/// Notify delegate updated from given values. if nil it will be ignored
- (void)_notifyDelegateAvailable:(NSNumber *)availableNumber sate:(NSNumber *)stateNumber {
    // Notify delegate if necessary
    if ((availableNumber != nil) || (stateNumber != nil)) {
        dispatch_async(_delegateQueue, ^{
            id<PMExtendedBatteryDelegate> delegate = self->_delegate;
            if ((availableNumber != nil) &&
                [delegate respondsToSelector:@selector(extendedBattery:didChangeAvailable:)]) {
                PMExtendedBatteryFeatureAvailable available = PMExtendedBatteryFeatureAvailableFromNSInteger(availableNumber.integerValue);
                os_log_debug(pm_extendedBattery_log(), "Notifying delegate:%@, extended battery did change available:%@",
                             delegate, NSStringFromPMExtendedBatteryFeatureAvailable(available));
                [delegate extendedBattery:self didChangeAvailable:available];
            }

            if ((stateNumber != nil) &&
                [delegate respondsToSelector:@selector(extendedBattery:didChangeState:)]) {
                PMExtendedBatteryState state = PMExtendedBatteryStateFromNSInteger(stateNumber.intValue);
                os_log_debug(pm_extendedBattery_log(), "Notifying delegate:%@, extended battery did change state:%@",
                             delegate, NSStringFromPMExtendedBatteryState(state));
                [delegate extendedBattery:self didChangeState:state];
            }
        });
    };
}

#pragma mark XPCConnection

- (NSXPCConnection *)_connection {
    NSXPCConnection *connection = [[NSXPCConnection alloc] initWithMachServiceName:PMExtendedBatteryMachServiceName
                                                                           options:NSXPCConnectionPrivileged];
    connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(PMExtendedBatteryServiceProtocol)];
    connection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(PMExtendedBatteryClientProtocol)];
    connection.exportedObject =  [_PMWeakProxy proxyWithTarget:self];
    connection.invalidationHandler = ^{
        os_log_debug(pm_extendedBattery_log(), "PMExtendedBattery invalidationHandler called");
    };
    __weak typeof(self) weakSelf = self;
    connection.interruptionHandler = ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf == nil) return;
        // Mark status as stale to force fetch it from the remote service next time
        os_unfair_lock_lock(&(strongSelf->_lock));
        strongSelf->_dirtyInfo = YES;
        strongSelf->_connectionInterrupted = YES;
        os_unfair_lock_unlock(&(strongSelf->_lock));
        os_log_error(pm_extendedBattery_log(), "PMExtendedBattery interruptionHandler called");
    };

    return connection;
}

- (id<PMExtendedBatteryServiceProtocol>)_synchronousRemoteProxyObject:(BOOL)synchronous
                                                     withErrorHandler:(void (^)(NSError *_Nonnull error))handler {
    if (synchronous) {
        return [_connection synchronousRemoteObjectProxyWithErrorHandler:handler];
    } else {
        return [_connection remoteObjectProxyWithErrorHandler:handler];
    }
}

@end

NS_ASSUME_NONNULL_END

#endif // WATCH_POWER_RANGER
