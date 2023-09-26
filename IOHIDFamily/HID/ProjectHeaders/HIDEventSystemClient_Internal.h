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

/*!
 * @method initWithType:andAttributes
 *
 * @abstract
 * Creates a HIDEventSystemClient of the specified type.
 *
 * @discussion
 * A HIDEventSystem client is limited in its permitted functionality by the type
 * provided. A restriction due to lack of entitlement may not be immediately or
 * easily noticable, confer the HIDEventSystemClientType documentation above
 * for guidelines.
 *
 * @param type
 * The desired type of the client.
 *
 * @param attributes
 * Attributes to associate with the client.
 *
 * @result
 * A HIDEventSystemClient instance on success, nil on failure.
 */
- (nullable instancetype)initWithType:(HIDEventSystemClientType)type andAttributes:(NSDictionary * __nullable)attributes;

@end


NS_ASSUME_NONNULL_END

#endif /* HIDEventSystemClient_Internal_h */
