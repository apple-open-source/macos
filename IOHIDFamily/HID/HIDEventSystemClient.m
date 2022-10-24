//
//  HIDEventSystemClient.m
//  HID
//
//  Created by dekom on 12/20/17.
//

#import "HIDEventSystemClient.h"
#import "HIDEventSystemClientPrivate.h"
#import <IOKit/hid/IOHIDEventSystemClient.h>
#import <IOKit/hid/IOHIDLibPrivate.h>
#import "HIDEvent.h"
#import <os/assumes.h>
#import <os/lock_private.h>

@implementation HIDEventSystemClient {
    IOHIDEventSystemClientRef   _client;
    
    HIDEventHandler            _eventHandler;
    HIDBlock                   _resetHandler;
    HIDEventFilterHandler      _filterHandler;
    HIDServiceHandler          _serviceHandler;
    HIDPropertyChangedHandler  _propertyChangedHandler;
    HIDBlock                   _cancelHandler;
    bool                       _activated;
    os_unfair_recursive_lock   _handlerLock;
}

- (nullable instancetype)initWithType:(HIDEventSystemClientType)type
{
    return [self initWithType:type andAttributes:nil];
}

- (nullable instancetype)initWithType:(HIDEventSystemClientType)type andAttributes:(NSDictionary*) attributes
{
    IOHIDEventSystemClientType clientType = (IOHIDEventSystemClientType)type;
    self = [super init];
    
    if (!self) {
        return self;
    }
    self->_client = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault,
                                                   clientType,
                                                   (__bridge CFDictionaryRef)attributes);
    if (!_client) {
        return nil;
    }
    
    return self;
}

- (void)dealloc
{
    if (_client) {
        CFRelease(_client);
    }
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@", _client];
}

- (id)propertyForKey:(NSString *)key
{
    return (id)CFBridgingRelease(IOHIDEventSystemClientCopyProperty(
                                                    _client,
                                                    (__bridge CFStringRef)key));
}

- (BOOL)setProperty:(id)value forKey:(NSString *)key
{
    return IOHIDEventSystemClientSetProperty(_client,
                                             (__bridge CFStringRef)key,
                                             (__bridge CFTypeRef)value);
}
- (void)setMatching:(id)matching
{
    os_assert([matching isKindOfClass:[NSDictionary class]] ||
              [matching isKindOfClass:[NSArray class]],
              "Unknown matching criteria: %@", matching);
    
    if ([matching isKindOfClass:[NSDictionary class]]) {
        CFDictionaryRef matchDict = (__bridge CFDictionaryRef)matching;
        
        if (((NSDictionary *)matching).count) {
            IOHIDEventSystemClientSetMatching(_client, matchDict);
        } else {
            IOHIDEventSystemClientSetMatching(_client, nil);
        }
    } else if ([matching isKindOfClass:[NSArray class]]) {
        CFArrayRef matchArray = (__bridge CFArrayRef)matching;
        
        if (((NSArray *)matching).count) {
            IOHIDEventSystemClientSetMatchingMultiple(_client, matchArray);
        } else {
            IOHIDEventSystemClientSetMatchingMultiple(_client, nil);
        }
    }
}

- (NSArray<HIDServiceClient *> *)services
{
    return (NSArray *)CFBridgingRelease(
                                IOHIDEventSystemClientCopyServices(_client));
}

- (void)setCancelHandler:(HIDBlock)handler
{
    os_unfair_recursive_lock_lock(&_handlerLock);
    os_assert(!_activated, "Cancel handler set after HIDEventSystemClient was activated");
    _cancelHandler = handler;
    os_unfair_recursive_lock_unlock(&_handlerLock);
}

- (void)setDispatchQueue:(dispatch_queue_t)queue
{
    IOHIDEventSystemClientSetDispatchQueue(_client, queue);
}

static void _eventCallback(void *target,
                           void *refcon __unused,
                           void *sender,
                           IOHIDEventRef event)
{
    HIDEventSystemClient *me = (__bridge HIDEventSystemClient *)target;

    (me->_eventHandler)((__bridge HIDServiceClient *)sender,
                        (__bridge HIDEvent *)event);
}

- (void)setEventHandler:(HIDEventHandler)handler
{
    os_unfair_recursive_lock_lock(&_handlerLock);
    os_assert(_eventHandler == NULL, "Event handler already set");
    _eventHandler = handler;
    os_unfair_recursive_lock_unlock(&_handlerLock);

    IOHIDEventSystemClientRegisterEventCallback(_client,
                                                _eventCallback,
                                                (__bridge void *)self,
                                                nil);
}

static void _resetCallback(void *target, void *context __unused)
{
    HIDEventSystemClient *me = (__bridge HIDEventSystemClient *)target;
    
    (me->_resetHandler)();
}

- (void)setResetHandler:(HIDBlock)handler
{
    os_unfair_recursive_lock_lock(&_handlerLock);
    os_assert(_resetHandler == NULL, "Reset handler already set");
    _resetHandler = handler;
    os_unfair_recursive_lock_unlock(&_handlerLock);

    IOHIDEventSystemClientRegisterResetCallback(_client,
                                                _resetCallback,
                                                (__bridge void *)self,
                                                nil);
}

static boolean_t _eventFilterCallback(void *target,
                                 void *refcon __unused,
                                 void *sender,
                                 IOHIDEventRef event)
{
    HIDEventSystemClient *me = (__bridge HIDEventSystemClient *)target;
    boolean_t result;
    
    result = (me->_filterHandler)((__bridge HIDServiceClient *)sender,
                                (__bridge HIDEvent *)event);
    return result;
}

- (void)setEventFilterHandler:(HIDEventFilterHandler)handler
{
    os_unfair_recursive_lock_lock(&_handlerLock);
    os_assert(_filterHandler == NULL, "Filter handler already set");
    _filterHandler = handler;
    os_unfair_recursive_lock_unlock(&_handlerLock);

    IOHIDEventSystemClientRegisterEventFilterCallback(_client,
                                                      _eventFilterCallback,
                                                      (__bridge void *)self,
                                                      nil);
}

static void _serviceCallback(void *target,
                             void *refcon __unused,
                             IOHIDServiceClientRef service)
{
    HIDEventSystemClient *me = (__bridge HIDEventSystemClient *)target;
    
    (me->_serviceHandler)((__bridge HIDServiceClient *)service);
}

- (void)setServiceNotificationHandler:(HIDServiceHandler)handler
{
    os_unfair_recursive_lock_lock(&_handlerLock);
    os_assert(_serviceHandler == NULL, "Service notification handler already set");
    _serviceHandler = handler;
    os_unfair_recursive_lock_unlock(&_handlerLock);

    IOHIDEventSystemClientRegisterDeviceMatchingCallback(_client,
                                                         _serviceCallback,
                                                         (__bridge void *)self,
                                                         nil);
}

static void _propertiesChangedCallback(void *target,
                                       void *context __unused,
                                       CFStringRef property,
                                       CFTypeRef value)
{
    HIDEventSystemClient *me = (__bridge HIDEventSystemClient *)target;
    
    (me->_propertyChangedHandler)((__bridge NSString *)property,
                                  (__bridge id)value);
}

- (void)setPropertyChangedHandler:(HIDPropertyChangedHandler)handler
                         matching:(id)matching
{
    os_assert([matching isKindOfClass:[NSString class]] ||
              [matching isKindOfClass:[NSArray class]],
              "Unknown matching criteria: %@", matching);
    
    os_unfair_recursive_lock_lock(&_handlerLock);
    os_assert(_propertyChangedHandler == NULL, "Property changed notification handler already set");

    _propertyChangedHandler = handler;
    os_unfair_recursive_lock_unlock(&_handlerLock);

    if ([matching isKindOfClass:[NSString class]]) {
        CFStringRef matchString = (__bridge CFStringRef)matching;
        IOHIDEventSystemClientRegisterPropertyChangedCallback(
                                                    _client,
                                                    matchString,
                                                    _propertiesChangedCallback,
                                                    (__bridge void *)self,
                                                    nil);
    } else if ([matching isKindOfClass:[NSArray class]]) {
        for (NSString *match in matching) {
            CFStringRef matchString = (__bridge CFStringRef)match;
            
            IOHIDEventSystemClientRegisterPropertyChangedCallback(
                                                    _client,
                                                    matchString,
                                                    _propertiesChangedCallback,
                                                    (__bridge void *)self,
                                                    nil);
        }
    }
}

- (void)activate
{
    os_unfair_recursive_lock_lock(&_handlerLock);
    _activated = true;
    os_unfair_recursive_lock_unlock(&_handlerLock);

    IOHIDEventSystemClientSetCancelHandler(_client, ^{
        // Block captures reference to self while cancellation hasn't completed.
        if (self->_cancelHandler) {
            self->_cancelHandler();
            self->_cancelHandler = nil;
        }
    });

    IOHIDEventSystemClientActivate(_client);
}

- (void)cancel
{
    IOHIDEventSystemClientCancel(_client);
}

- (IOHIDEventSystemClientRef) client
{
    return _client;
}

@end
