//
//  HIDSessionFilterExample.h
//  HIDSessionFilterExample
//
//  Created by dekom on 9/26/18.
//

#ifndef HIDSessionFilterExample_h
#define HIDSessionFilterExample_h

#import <HID/HID_Private.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDSessionFilterExample : NSObject <HIDSessionFilter>

- (nullable instancetype)initWithSession:(HIDSession *)session;

- (nullable id)propertyForKey:(NSString *)key;

- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key;

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                        forService:(HIDEventService *)service;

- (void)activate;

- (void)serviceNotification:(HIDEventService *)service added:(BOOL)added;

- (void)setDispatchQueue:(dispatch_queue_t)queue;


// should be weak
@property (weak) HIDSession         *session;
@property (weak) HIDEventService    *keyboard;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDSessionFilterExample_h */
