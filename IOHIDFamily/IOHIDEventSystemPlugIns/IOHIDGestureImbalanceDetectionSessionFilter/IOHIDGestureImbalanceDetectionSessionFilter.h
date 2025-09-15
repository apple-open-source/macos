//
//  IOHIDGestureImbalanceDetectionSessionFilter.h
//
//  Created by Austin Erck on 7/9/24.
//

#ifndef IOHIDGestureImbalanceDetectionSessionFilter_h
#define IOHIDGestureImbalanceDetectionSessionFilter_h

#import <HID/HID_Private.h>
@import OSLog;

NS_ASSUME_NONNULL_BEGIN

extern os_log_t logHandle(void);

@interface IOHIDGestureImbalanceDetectionSessionFilter : NSObject <HIDSessionFilter>

- (nullable instancetype)initWithSession:(HIDSession *)session;

- (nullable id)propertyForKey:(NSString *)key;

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                        forService:(HIDEventService *)service;

- (void)activate;

- (void)serviceNotification:(HIDEventService *)service added:(BOOL)added;

- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key;

- (void)setDispatchQueue:(dispatch_queue_t)queue;


@end

NS_ASSUME_NONNULL_END

#endif /* IOHIDGestureImbalanceDetectionSessionFilter_h */
