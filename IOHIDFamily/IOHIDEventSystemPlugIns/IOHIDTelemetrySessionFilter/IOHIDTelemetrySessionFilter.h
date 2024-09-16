//
//  IOHIDTelemetrySessionFilter.h
//  IOHIDTelemetrySessionFilter
//
//

#ifndef IOHIDTelemetrySessionFilter_h
#define IOHIDTelemetrySessionFilter_h

#import <HID/HID_Private.h>

NS_ASSUME_NONNULL_BEGIN

@interface IOHIDTelemetrySessionFilter : NSObject <HIDSessionFilter>

- (nullable instancetype)initWithSession:(HIDSession *)session;

- (nullable id)propertyForKey:(NSString *)key;

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                        forService:(HIDEventService *)service;

- (void)activate;

- (void)serviceNotification:(HIDEventService *)service added:(BOOL)added;

- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key;

- (void)setDispatchQueue:(dispatch_queue_t)queue;

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                      toConnection:(HIDConnection *)connection
                       fromService:(HIDEventService *)service;


@end

NS_ASSUME_NONNULL_END

#endif /* IOHIDTelemetrySessionFilter_h */
