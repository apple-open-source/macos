//
//  HIDServiceFilterExample.m
//  HIDServiceFilterExample
//
//  Created by dekom on 9/28/18.
//

#import <Foundation/Foundation.h>
#import "HIDServiceFilterExample.h"
#import "IOHIDDebug.h"
#import <IOKit/hid/IOHIDUsageTables.h>
#import <IOKit/hid/IOHIDServiceKeys.h>
#import <IOKit/hid/IOHIDEventTypes.h>

@implementation HIDServiceFilterExample {
    NSNumber *_prop;
    HIDBlock _cancelHandler;
    dispatch_queue_t _queue;
    bool _activated;
    bool _clientAdded;
}

- (instancetype)initWithService:(HIDEventService *)service
{
    self = [super init];
    if (!self) {
        return self;
    }
    
    _service = service;
    
    HIDLog("HIDServiceFilterExample::initWithService: %@", _service);
    
    return self;
}

- (void)dealloc
{
    HIDLog("HIDServiceFilterExample dealloc");
}

- (NSString *)description
{
    return @"HIDServiceFilterExample";
}

- (id)propertyForKey:(NSString *)key client:(HIDConnection *)client
{
    id result = nil;
    
    if ([key isEqualToString:@"TestHIDServiceFilterGetProperty"]) {
        result = _prop;
        HIDLog("HIDServiceFilterExample::propertyForKey %@ value: %@ client: %@", key, result, client);
    } else if ([key isEqualToString:@(kIOHIDServiceFilterDebugKey)]) {
        // debug dictionary that gets captured by hidutil
        NSMutableDictionary *debug = [NSMutableDictionary new];
        
        debug[@"FilterName"] = @"HIDServiceFilterExample";
        debug[@"cancelHandler"] = _cancelHandler ? @YES : @NO;
        debug[@"dispatchQueue"] = _queue ? @YES : @NO;
        debug[@"activated"] = @(_activated);
        debug[@"clientAdded"] = @(_clientAdded);
        
        result = debug;
    }
    
    return result;
}

- (BOOL)setProperty:(id)value
             forKey:(NSString *)key
             client:(HIDConnection *)client
{
    bool result = false;
    
    if ([key isEqualToString:@"TestHIDServiceFilterSetProperty"] &&
        [value isKindOfClass:[NSNumber class]]) {
        _prop = value;
        result = true;
        
        HIDLog("HIDServiceFilterExample::setProperty: %@ forKey: %@ client: %@", value, key, client);
    } else if ([key isEqualToString:@"TestEventDispatch"]) {
        HIDEvent *event = [[HIDEvent alloc] initWithType:kIOHIDEventTypeKeyboard
                                               timestamp:1234
                                                senderID:5678];
        
        [event setIntegerValue:kHIDPage_KeyboardOrKeypad forField:kIOHIDEventFieldKeyboardUsagePage];
        [event setIntegerValue:kHIDUsage_KeyboardLeftShift forField:kIOHIDEventFieldKeyboardUsage];
        [event setIntegerValue:1 forField:kIOHIDEventFieldKeyboardDown];
        
        [_dispatcher dispatchEvent:event];
        result = true;
    }
    
    return result;
}

+ (BOOL)matchService:(HIDEventService *)service
             options:(NSDictionary *)options
               score:(NSInteger *)score
{
    HIDLog("HIDServiceFilterExample::matchService %@ options: %@ score: %ld", service, options, (long)*score);
    
    if ([service conformsToUsagePage:kHIDPage_GenericDesktop usage:kHIDUsage_GD_Keyboard]) {
        *score = 500;
        return true;
    } else {
        return false;
    }
}

- (HIDEvent *)filterEvent:(HIDEvent *)event
{
    HIDLog("HIDServiceFilterExample::filterEvent %@", event);
    return event;
}

- (HIDEvent *)filterEventMatching:(NSDictionary *)matching
                            event:(HIDEvent *)event
                        forClient:(HIDConnection *)client
{
    HIDEvent *child = [[HIDEvent alloc] initWithType:kIOHIDEventTypeKeyboard
                                           timestamp:1234
                                            senderID:5678];
    
    [event appendEvent:child];
    
    HIDLog("HIDServiceFilterExample::filterEventMatching %@ event: %@ client: %@", matching, event, client);
    
    return event;
}

- (void)setCancelHandler:(HIDBlock)handler
{
    HIDLog("HIDServiceFilterExample::setCancelHandler %p", handler);
    _cancelHandler = handler;
}

- (void)activate
{
    HIDLog("HIDServiceFilterExample::activate");
    _activated = true;
}

- (void)cancel
{
    HIDLog("HIDServiceFilterExample::cancel");
    
    // This call does not have to be asynchronous, this was done to demonstrate
    // that the cancel handler may be called asynchronously. If there is no
    // async work to be done, you can call the cancel handler directly in cancel.
    dispatch_async(_queue, ^{
        HIDLog("HIDServiceFilterExample::cancel calling cancel handler");
        self->_cancelHandler();
        self->_cancelHandler = nil;
    });
}

- (void)setDispatchQueue:(dispatch_queue_t)queue
{
    HIDLog("HIDServiceFilterExample::setDispatchQueue %p", queue);
    _queue = queue;
}

- (void)setEventDispatcher:(id<HIDEventDispatcher>)dispatcher
{
    HIDLog("HIDServiceFilterExample::setEventDispatcher %@", dispatcher);
    _dispatcher = dispatcher;
}

- (void)clientNotification:(HIDConnection *)client added:(BOOL)added
{
    HIDLog("HIDServiceFilterExample::clientNotification %@ added: %d", client, added);
    
    if (added) {
        _clientAdded = true;
        _client = client;
    } else {
        _clientAdded = false;
        _client = nil;
    }
}

@end
