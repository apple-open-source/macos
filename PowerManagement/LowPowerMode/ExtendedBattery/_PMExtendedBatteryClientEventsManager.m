//
//  _PMExtendedBatteryClientEventsManager.m
//  LowPowerMode-Embedded
//
//  Created by Pablo Pons Bordes on 3/6/25.
//

#import "_PMExtendedBatteryClientEventsManager.h"
#import <xpc/private.h>

#if WATCH_POWER_RANGER

NS_ASSUME_NONNULL_BEGIN
typedef NSHashTable<id<_PMExtendedBatteryClientEventsManagerObserver> >  PMObserversHashTable;

@interface _PMExtendedBatteryClientEventsManager () {
    dispatch_queue_t _internalQueue;
    NSMutableDictionary<NSString *, PMObserversHashTable *> *_allObservers;
}

@end

@implementation _PMExtendedBatteryClientEventsManager

- (instancetype)init {
    self = [super init];
    if (self != nil) {
        _internalQueue = dispatch_queue_create("com.apple.lowpowermode.extendedbattery.clienteventmanager",
                                               DISPATCH_QUEUE_SERIAL);
        _allObservers = [NSMutableDictionary new];
        [self _registerEventsHandler];
    }

    return self;
}

+ (instancetype)sharedInstance {
    static _PMExtendedBatteryClientEventsManager *observer;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        observer = [[_PMExtendedBatteryClientEventsManager alloc] init];
    });

    return observer;
}

- (void)registerObserver:(id<_PMExtendedBatteryClientEventsManagerObserver>)anObserver
                forEvent:(NSString *)eventName {
    dispatch_sync(_internalQueue, ^{
        os_log(pm_extendedBattery_log(), "register observer %@ for event:%@", anObserver, eventName);
        PMObserversHashTable *observers = self->_allObservers[eventName];
        if (observers == nil) {
            NSPointerFunctionsOptions options = (NSPointerFunctionsWeakMemory | NSPointerFunctionsObjectPointerPersonality);
            observers = [[NSHashTable alloc] initWithOptions:options capacity:1];
            self->_allObservers[eventName] = observers;
        }
        [observers addObject:anObserver];
        [self _registerToReceiverEventsWithName:eventName];
    });
}

- (void)unregisterObserver:(id<_PMExtendedBatteryClientEventsManagerObserver>)anObserver {
    dispatch_sync(_internalQueue, ^{
        [self->_allObservers enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull eventName,
                                                                 PMObserversHashTable * _Nonnull observers,
                                                                 BOOL * _Nonnull stop) {
            [observers removeObject:anObserver];
            if (observers.count == 0) {
                [self _unregisterFromReceiveEventsWithName:eventName];
            }
        }];
    });
}

- (void)_handleEvent:(NSString *)eventName {
    dispatch_assert_queue(_internalQueue);
    PMObserversHashTable *observers = self->_allObservers[eventName];
    if (observers.count == 0) {
        os_log(pm_extendedBattery_log(), "don't have observers for eventName: '%@'", eventName);
        // It seems the objects were all deallocated, we can safely unregister for this even
        [self _unregisterFromReceiveEventsWithName:eventName];
        return;
    }

    for (id<_PMExtendedBatteryClientEventsManagerObserver> anObserver in observers) {
        [anObserver extendedBatteryClientEventsManager:self didReceivedEvent:eventName];
    }
}

- (void)_registerToReceiverEventsWithName:(NSString *)eventName {
    // register the process to receive events
    xpc_object_t eventDict = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(eventDict, eventName.UTF8String, eventName.UTF8String);
    xpc_set_event(PMExtendedBatteryXPCEventStream.UTF8String, eventName.UTF8String, eventDict);
    os_log(pm_extendedBattery_log(), "Register to receive event: '%@'", eventName);
}

- (void)_unregisterFromReceiveEventsWithName:(NSString *)eventName {
    xpc_set_event(PMExtendedBatteryXPCEventStream.UTF8String, eventName.UTF8String, NULL);
    os_log(pm_extendedBattery_log(), "Unregister from receive event: '%@'", eventName);
}

- (void)_registerEventsHandler {
    __weak typeof(self) weakSelf = self;
    xpc_set_event_stream_handler(PMExtendedBatteryXPCEventStream.UTF8String, _internalQueue, ^(xpc_object_t  _Nonnull xpcEvent) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf == nil) return;

        const char *eventNameCString = xpc_dictionary_get_string(xpcEvent, XPC_EVENT_KEY_NAME);
        NSString *eventName = [NSString stringWithUTF8String:eventNameCString];
        os_log(pm_extendedBattery_log(), "received XPCEvent '%@'", eventName);
        [strongSelf _handleEvent:eventName];
    });
}

@end

NS_ASSUME_NONNULL_END

#endif // WATCH_POWER_RANGER
