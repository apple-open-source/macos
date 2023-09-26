/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * This document is the property of Apple Inc.
 * It is considered confidential and proprietary.
 *
 * This document may not be reproduced or transmitted in any form,
 * in whole or in part, without the express written permission of
 * Apple Inc.
 */

#ifndef IOHIDSensorPowerLoggingFilter_h
#define IOHIDSensorPowerLoggingFilter_h

#import <HID/HID_Private.h>

NS_ASSUME_NONNULL_BEGIN

@interface IOHIDSensorPowerLoggingFilter : NSObject <HIDServiceFilter>

+ (BOOL)matchService:(HIDEventService *)service
             options:(nullable NSDictionary *)options
               score:(NSInteger *)score;

- (nullable instancetype)initWithService:(HIDEventService *)service;

- (nullable id)propertyForKey:(NSString *)key
                       client:(nullable HIDConnection *)client;

- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key
             client:(nullable HIDConnection *)client;

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event;

- (nullable HIDEvent *)filterEventMatching:(nullable NSDictionary *)matching
                                     event:(HIDEvent *)event
                                 forClient:(nullable HIDConnection *)client;

- (void)setCancelHandler:(HIDBlock)handler;

- (void)activate;

- (void)cancel;

- (void)clientNotification:(HIDConnection *)client added:(BOOL)added;

// should be weak
@property (weak) HIDEventService        * service;

@end

NS_ASSUME_NONNULL_END

#endif /* IOHIDSensorPowerLoggingFilter_h */
