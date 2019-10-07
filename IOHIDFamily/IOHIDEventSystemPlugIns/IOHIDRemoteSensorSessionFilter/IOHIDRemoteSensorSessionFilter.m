//
//  HIDSessionFilterExample.m
//  HIDSessionFilterExample
//
//  Created by dekom on 9/26/18.
//

#import <Foundation/Foundation.h>
#import <HID/HID_Private.h>
#import "IOHIDDebug.h"
#import <IOKit/hid/IOHIDUsageTables.h>
#import <IOKit/hid/IOHIDEventSystemKeys.h>
#import <RemoteHID/RemoteHID.h>

@interface IOHIDRemoteSensorSessionFilter : NSObject <HIDSessionFilter>

- (nullable instancetype)initWithSession:(HIDSession *)session;

- (nullable id)propertyForKey:(NSString *)key;

- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key;

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                        forService:(HIDEventService *)service;

- (void)activate;

- (void)serviceNotification:(HIDEventService *)service added:(BOOL)added;

- (void)setDispatchQueue:(dispatch_queue_t)queue;

@property  dispatch_queue_t              queue;
@property  HIDRemoteDeviceAACPServer *   server;

@end


@implementation IOHIDRemoteSensorSessionFilter {

}

- (nullable instancetype)initWithSession:(HIDSession * __unused)session
{
    self = [super init];
    if (!self) {
        return self;
    }
    
    self.queue = dispatch_queue_create("com.apple.hidrc", DISPATCH_QUEUE_SERIAL);
    self.server = [[HIDRemoteDeviceAACPServer alloc] initWithQueue:self.queue];
    
    return self;
}

- (void)dealloc
{

}

- (nullable id)propertyForKey:(NSString *)key
{
    id result = nil;
    
    if ([key isEqualToString:@(kIOHIDSessionFilterDebugKey)]) {
        NSMutableDictionary * debug = [NSMutableDictionary new];
        debug[@"Class"] = @"IOHIDRemoteSensorSessionFilter";
        
        result = debug;
    }
    
    return result;
}

- (BOOL)setProperty:(nullable id __unused)value
             forKey:(NSString * __unused)key
{
    return NO;
}

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                        forService:(HIDEventService * __unused)service
{
    return event;
}

- (void)activate
{
    [self.server activate];
}

- (void)serviceNotification:(HIDEventService * __unused)service added:(BOOL __unused)added
{

}

- (void)setDispatchQueue:(dispatch_queue_t __unused)queue
{

}

@end

