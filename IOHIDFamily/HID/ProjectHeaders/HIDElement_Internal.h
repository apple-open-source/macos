/*!
 * HIDElement_Internal.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#ifndef HIDElement_Internal_h
#define HIDElement_Internal_h

#import <IOKit/hid/IOHIDElement.h>
#import <IOKit/hid/IOHIDValue.h>
#import <HID/HIDElement.h>

@interface HIDElement (priv)

@property (nullable) IOHIDValueRef valueRef;
@property (readonly) uint32_t cookie;

@end

#endif /* HIDElement_Internal_h */
