/*!
 * HIDBasicTimeSync.h
 * HID
 *
 * Copyright © 2024-2025 Apple Inc. All rights reserved.
 */

#ifndef HIDBasicTimeSync_h
#define HIDBasicTimeSync_h

#import <HID/HIDTimeSync_Internal.h>

#if TIMESYNC_BASIC_AVAILABLE

__attribute__((visibility("hidden"))) API_AVAILABLE(ios(19.0))
@interface HIDBasicTimeSync : HIDTimeSync

- (instancetype)init;

@end

#endif

#endif /* HIDBasicTimeSync_h */
