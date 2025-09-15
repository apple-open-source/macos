/*!
 * HIDEventSystemClient_Internal.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#ifndef HIDEventSystemClient_Internal_h
#define HIDEventSystemClient_Internal_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>
#import <HID/HIDEventSystemClient.h>
#import <IOKit/hidsystem/IOHIDEventSystemClient.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDEventSystemClient (priv)

@property (readonly) IOHIDEventSystemClientRef client;

@end


NS_ASSUME_NONNULL_END

#endif /* HIDEventSystemClient_Internal_h */
