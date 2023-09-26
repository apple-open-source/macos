/*!
 * HIDEventService_Internal.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#ifndef HIDEventService_Internal_h
#define HIDEventService_Internal_h

#import <HID/HIDEventService.h>

@interface HIDEventService (HIDFrameworkPrivate)
- (void)dispatchEvent:(HIDEvent *)event;
@end

#endif /* HIDEventService_Internal_h */
