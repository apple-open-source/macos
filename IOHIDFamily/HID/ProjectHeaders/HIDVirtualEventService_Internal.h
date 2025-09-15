/*!
 * HIDVirtualEventService_Internal.h
 * HID
 *
 * Copyright © 2025 Apple Inc. All rights reserved.
 */

#ifndef HIDVirtualEventService_Internal_h
#define HIDVirtualEventService_Internal_h

#import <HID/HIDVirtualEventService.h>
#import <stdatomic.h>
#import <dispatch/dispatch.h>

@class HIDEventSystemClient;
@class HIDServiceClient;

/// Internal instance variables/properties of the `HIDVirtualEventService` class.
///
@interface HIDVirtualEventService () {
    _Atomic int _state;
}

@property  HIDEventSystemClient *   client;
@property  HIDServiceClient *       serviceClient;
@property  dispatch_queue_t         queue;

@end

#endif /* HIDVirtualEventService_Internal_h */
