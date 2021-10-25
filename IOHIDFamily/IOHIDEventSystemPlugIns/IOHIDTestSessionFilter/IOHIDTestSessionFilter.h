//
//  IOHIDTestServiceFilter.h
//  IOHIDTestServiceFilter
//
//  Created by jkergan on 6/29/21.
//

#ifndef IOHIDTestSessionFilter_h
#define IOHIDTestSessionFilter_h

#import <HID/HID_Private.h>

NS_ASSUME_NONNULL_BEGIN

@interface IOHIDTestSessionFilter : NSObject <HIDSessionFilter>

- (nullable instancetype)initWithSession:(HIDSession *)session;

- (nullable id)propertyForKey:(NSString *)key;

- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key;

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                        forService:(HIDEventService *)service;

- (void)activate;

- (void)setDispatchQueue:(dispatch_queue_t)queue;

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                      toConnection:(HIDConnection *)connection
                       fromService:(HIDEventService *)service;

@end

NS_ASSUME_NONNULL_END

#endif /* IOHIDTestSessionFilter_h */
