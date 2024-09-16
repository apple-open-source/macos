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

#pragma once

#import <HID/HID_Private.h>

os_log_t _IOHIDButtonLog(void);

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, HIDSuppressionStateType) {
    kHIDSuppressionStateTypeUnknown = 0,
    kHIDSuppressionStateTypeSuppressed = 1,
    kHIDSuppressionStateTypeUnsuppressed = 2,
};

@interface HIDButtonLoggingServiceFilter: NSObject <HIDServiceFilter>

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

// should be weak
@property (weak) HIDEventService        *service;

@end

NS_ASSUME_NONNULL_END
