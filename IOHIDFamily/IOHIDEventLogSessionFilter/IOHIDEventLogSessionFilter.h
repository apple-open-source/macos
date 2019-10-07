//
//  IOHIDEventLogSessionFilter.h
//  IOHIDFamily
//
//  Created by AB on 10/9/18.
//

#import <HID/HID_Private.h>

NS_ASSUME_NONNULL_BEGIN

@interface IOHIDEventLogSessionFilter : NSObject <HIDSessionFilter>

- (nullable instancetype)initWithSession:(HIDSession *)session;

- (nullable id)propertyForKey:(NSString *)key;

- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key;

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                        forService:(HIDEventService *)service;

- (void)activate;

- (void)serviceNotification:(HIDEventService *)service added:(BOOL)added;

@end

NS_ASSUME_NONNULL_END



