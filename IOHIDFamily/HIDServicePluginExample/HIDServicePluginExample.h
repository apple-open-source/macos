//
//  HIDServicePluginExample.h
//  HIDServicePluginExample
//
//  Created by dekom on 10/1/18.
//

#ifndef HIDServicePluginExample_h
#define HIDServicePluginExample_h

#import <HID/HIDBase.h>
#import <HID/HID_Private.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDServicePluginExample : NSObject <HIDServicePlugin>

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

- (void)clientNotification:(HIDConnection *)client added:(BOOL)added;

// should be weak
@property (weak) HIDConnection          *client;
@property (weak) id<HIDEventDispatcher> dispatcher;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDServicePluginExample_h */
