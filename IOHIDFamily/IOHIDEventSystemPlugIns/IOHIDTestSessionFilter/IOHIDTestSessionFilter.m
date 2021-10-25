//
//  IOHIDTestServiceFilter.m
//  IOHIDTestServiceFilter
//
//  Created by dekom on 9/28/18.
//

#import <Foundation/Foundation.h>
#import "IOHIDTestSessionFilter.h"
#import "IOHIDDebug.h"
#import <IOKit/hid/IOHIDUsageTables.h>
#import <IOKit/hid/IOHIDServiceKeys.h>
#import <IOKit/hid/IOHIDEventTypes.h>
#include <xpc/private.h>

@implementation IOHIDTestSessionFilter {
    NSNumber *_prop;
    HIDBlock _cancelHandler;
    dispatch_queue_t _queue;
    bool _activated;
    bool _clientAdded;
    bool _audit;
}

- (instancetype)initWithSession:(HIDSession *)session
{
    self = [super init];
    if (!self) {
        return self;
    }
        
    HIDLog("HIDServiceFilterExample::initWithSession: %@", session);
    
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

- (id)propertyForKey:(NSString *)key
{
    id result = nil;
    
    if ([key isEqualToString:@"TestHIDServiceFilterGetProperty"]) {
        result = _prop;
        HIDLog("HIDServiceFilterExample::propertyForKey %@ value: %@", key, result);
    } else if ([key isEqualToString:@"TestHIDServiceFilterEnableAudit"]) {
        result = [NSNumber numberWithBool:_audit];
        HIDLog("HIDServiceFilterExample::propertyForKey %@ value: %@", key, result);
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
{
    bool result = false;
    
    if ([key isEqualToString:@"TestHIDServiceFilterSetProperty"] &&
        [value isKindOfClass:[NSNumber class]]) {
        _prop = value;
        result = true;
        
        HIDLog("HIDServiceFilterExample::setProperty: %@ forKey: %@", value, key);
    } else if ([key isEqualToString:@"TestHIDServiceFilterEnableAudit"]) {
        _audit = [[NSNumber numberWithBool:YES] isEqual:value];
        result = true;
        HIDLog("HIDServiceFilterExample::setProperty: %@ forKey: %@", value, key);
    }
    
    return result;
}

- (HIDEvent *)filterEvent:(HIDEvent *)event
               forService:(HIDEventService *)service
{
    HIDLog("HIDServiceFilterExample::filterEvent %@ forService: %@", event, service);
    return event;
}

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                      toConnection:(HIDConnection *)connection
                       fromService:(HIDEventService *)service
{
    if (connection) {
        audit_token_t conn_token = {0};
        xpc_object_t entitlements = nil;

        [connection getAuditToken:&conn_token];

        entitlements = xpc_copy_entitlement_for_token(NULL, &conn_token);
        
        if (entitlements) {
            if (xpc_dictionary_get_value(entitlements, "com.apple.private.hid.testconnection.audit") != XPC_BOOL_TRUE) {
                event = nil;
            }
            HIDLog("HIDServiceFilterExample::filterEvent entitlements: %s",  xpc_copy_description(entitlements));
        }
    }

    HIDLog("HIDServiceFilterExample::filterEvent: %@ toConnection: %@ fromService: %@", event, connection, service);

    return event;
}

- (void)activate
{
    HIDLog("HIDServiceFilterExample::activate");
    _activated = true;
}

- (void)setDispatchQueue:(dispatch_queue_t)queue
{
    HIDLog("HIDServiceFilterExample::setDispatchQueue %p", queue);
    _queue = queue;
}

@end
