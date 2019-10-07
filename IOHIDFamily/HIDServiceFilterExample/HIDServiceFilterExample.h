//
//  HIDServiceFilterExample.h
//  HIDServiceFilterExample
//
//  Created by dekom on 9/28/18.
//

#ifndef HIDServiceFilterExample_h
#define HIDServiceFilterExample_h

#import <HID/HID_Private.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDServiceFilterExample : NSObject <HIDServiceFilter>

- (nullable instancetype)initWithService:(HIDEventService *)service;

- (nullable id)propertyForKey:(NSString *)key
                       client:(nullable HIDConnection *)client;

- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key
             client:(nullable HIDConnection *)client;

+ (BOOL)matchService:(HIDEventService *)service
             options:(nullable NSDictionary *)options
               score:(NSInteger *)score;

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event;

- (nullable HIDEvent *)filterEventMatching:(nullable NSDictionary *)matching
                                     event:(HIDEvent *)event
                                 forClient:(nullable HIDConnection *)client;

- (void)setCancelHandler:(HIDBlock)handler;

- (void)activate;

- (void)cancel;

- (void)setDispatchQueue:(dispatch_queue_t)queue;

- (void)setEventDispatcher:(id<HIDEventDispatcher>)dispatcher;

- (void)clientNotification:(HIDConnection *)client added:(BOOL)added;

// should be weak
@property (weak) HIDEventService        *service;
@property (weak) HIDConnection          *client;
@property (weak) id<HIDEventDispatcher> dispatcher;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDServiceFilterExample_h */
