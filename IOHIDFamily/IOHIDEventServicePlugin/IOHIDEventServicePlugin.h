//
//  IOHIDEventServicePlugin.h
//  IOHIDEventServicePlugin
//
//  Created by dekom on 10/10/18.
//

#ifndef IOHIDEventServicePlugin_h
#define IOHIDEventServicePlugin_h

#import <HID/HID_Private.h>

NS_ASSUME_NONNULL_BEGIN

@interface IOHIDEventServicePlugin : NSObject <HIDServicePlugin>

+ (BOOL)matchService:(io_service_t)service
             options:(nullable NSDictionary *)options
               score:(NSInteger *)score;

- (nullable instancetype)initWithService:(io_service_t)service;

- (nullable id)propertyForKey:(NSString *)key
                       client:(nullable HIDConnection *)client;

- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key
             client:(nullable HIDConnection *)client;

- (nullable HIDEvent *)eventMatching:(nullable NSDictionary *)matching
                           forClient:(nullable HIDConnection *)client;

- (void)setEventDispatcher:(id<HIDEventDispatcher>)dispatcher;

- (void)setCancelHandler:(HIDBlock)handler;

- (void)activate;

- (void)cancel;

- (void)setDispatchQueue:(dispatch_queue_t)queue;

// backwards compatibility for copyEvent/setOutputEvent
- (nullable HIDEvent *)copyEvent:(IOHIDEventType)type
                        matching:(nullable HIDEvent *)matching
                         options:(IOOptionBits)options;

- (IOReturn)setOutputEvent:(HIDEvent *)event;

// should be weak
@property (weak) id<HIDEventDispatcher> dispatcher;

@end

NS_ASSUME_NONNULL_END

#endif /* IOHIDEventServicePlugin_h */
