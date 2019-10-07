//
//  HIDServicePluginExample.m
//  HIDServicePluginExample
//
//  Created by dekom on 10/1/18.
//

#import <Foundation/Foundation.h>
#import "HIDServicePluginExample.h"
#import <IOKit/hid/IOHIDUsageTables.h>
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/hid/IOHIDServiceKeys.h>
#import <IOKit/hid/IOHIDEventTypes.h>
#import <IOKit/IOKitLib.h>
#import <AssertMacros.h>
#import <os/log.h>
#import "IOHIDDebug.h"

#ifdef DEBUG_ASSERT_MESSAGE
#undef DEBUG_ASSERT_MESSAGE
#endif

#define DEBUG_ASSERT_MESSAGE(name, assertion, label, message, file, line, value) \
    HIDLog("AssertMacros: %s, %s", assertion, (message!=0) ? message : "");

@implementation HIDServicePluginExample {
    HIDBlock _cancelHandler;
    dispatch_queue_t _queue;
    HIDDevice *_device;
    NSNumber *_prop;
    bool _activated;
    bool _clientAdded;
}

+ (BOOL)matchService:(io_service_t)service
             options:(NSDictionary *)options
               score:(NSInteger *)score
{
    if(IOObjectConformsTo(service, "IOHIDUserDevice")) {
        *score = 100;
        
        HIDLog("HIDServicePluginExample::matchService: %d options: %@ score: %ld",
               service, options, (long)*score);
        
        return true;
    }
    
    return false;
}

- (instancetype)initWithService:(io_service_t)service
{
    self = [super init];
    if (!self) {
        return self;
    }
    
    _device = [[HIDDevice alloc] initWithService:service];
    if (!_device) {
        return nil;
    }
    
    [_device open];
    
    __weak HIDServicePluginExample *weakSelf = self;
    [_device setInputElementHandler:^(HIDElement *element) {
        __strong HIDServicePluginExample *strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        
        if (element.usage < kHIDUsage_KeyboardA ||
            element.usage > kHIDUsage_KeyboardRightGUI) {
            return;
        }
        
        HIDLog("HIDServicePluginExample::recevied element: %@", element);
        
        HIDEvent *event = [[HIDEvent alloc] initWithType:kIOHIDEventTypeKeyboard
                                               timestamp:mach_absolute_time()
                                                senderID:1234];
        
        [event setIntegerValue:element.usagePage forField:kIOHIDEventFieldKeyboardUsagePage];
        [event setIntegerValue:element.usage forField:kIOHIDEventFieldKeyboardUsage];
        [event setIntegerValue:element.integerValue forField:kIOHIDEventFieldKeyboardDown];
        
        [strongSelf->_dispatcher dispatchEvent:event];
    }];
    
    HIDLog("HIDServicePluginExample::initWithService: %d", service);
    
    return self;
}

- (void)dealloc
{
    HIDLog("HIDServicePluginExample dealloc");
}

- (NSString *)description
{
    return @"HIDServicePluginExample";
}

- (nullable id)propertyForKey:(NSString *)key
                       client:(HIDConnection *)client
{
    id result = nil;
    
    if ([key isEqualToString:@"TestHIDServicePluginGetProperty"]) {
        result = _prop;
        HIDLog("HIDServicePluginExample::propertyForKey %@ value: %@ client: %@", key, result, client);
    } else if ([key isEqualToString:@(kIOHIDServicePluginDebugKey)]) {
        // debug dictionary that gets captured by hidutil
        NSMutableDictionary *debug = [NSMutableDictionary new];
        
        debug[@"PluginName"] = @"HIDServicePluginExample";
        debug[@"cancelHandler"] = _cancelHandler ? @YES : @NO;
        debug[@"dispatchQueue"] = _queue ? @YES : @NO;
        debug[@"activated"] = @(_activated);
        debug[@"clientAdded"] = @(_clientAdded);
        
        result = debug;
    } else {
        result = [_device propertyForKey:key];
    }
    
    return result;
}

- (BOOL)setProperty:(id)value
             forKey:(NSString *)key
             client:(HIDConnection *)client
{
    bool result = false;
    
    if ([key isEqualToString:@"TestHIDServicePluginSetProperty"] &&
        [value isKindOfClass:[NSNumber class]]) {
        _prop = value;
        result = true;
        
        HIDLog("HIDServiceFilterExample::setProperty: %@ forKey: %@ client: %@", value, key, client);
    } else {
        result = [_device setProperty:value forKey:key];
    }
    
    return result;
}

- (HIDEvent *)eventMatching:(NSDictionary *)matching
                  forClient:(HIDConnection *)client
{
    HIDEvent *event = nil;
    NSNumber *type = nil;
    
    require(matching, exit);
    
    HIDLog("HIDServicePluginExample::eventMatching: %@ client: %@", matching, client);
    
    type = matching[@("EventType")];
    
    if (type && type.unsignedIntValue == kIOHIDEventTypeKeyboard) {
        event = [[HIDEvent alloc] initWithType:kIOHIDEventTypeKeyboard
                                     timestamp:1234
                                      senderID:5678];
    }
    
exit:
    return event;
}

- (void)setEventDispatcher:(id<HIDEventDispatcher>)dispatcher
{
    HIDLog("HIDServicePluginExample::setEventDispatcher %@", dispatcher);
    _dispatcher = dispatcher;
}

- (void)setCancelHandler:(HIDBlock)handler
{
    HIDLog("HIDServicePluginExample::setCancelHandler %p", handler);
    _cancelHandler = handler;
    
    __weak HIDServicePluginExample *weakSelf = self;
    [_device setCancelHandler:^{
        __strong HIDServicePluginExample *strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        
        HIDLog("HIDServicePluginExample::cancel calling cancel handler");
        
        strongSelf->_cancelHandler();
        strongSelf->_cancelHandler = nil;
    }];
}

- (void)activate
{
    HIDLog("HIDServicePluginExample::activate");
    [_device activate];
    _activated = true;
}

- (void)cancel
{
    HIDLog("HIDServicePluginExample::cancel");
    [_device cancel];
    [_device close];
}

- (void)setDispatchQueue:(dispatch_queue_t)queue
{
    HIDLog("HIDServicePluginExample::setDispatchQueue %p", queue);
    _queue = queue;
    [_device setDispatchQueue:_queue];
}

- (void)clientNotification:(HIDConnection *)client added:(BOOL)added
{
    HIDLog("HIDServicePluginExample::clientNotification %@ added: %d", client, added);
    
    if (added) {
        _clientAdded = true;
        _client = client;
    } else {
        _clientAdded = false;
        _client = nil;
    }
}

@end
