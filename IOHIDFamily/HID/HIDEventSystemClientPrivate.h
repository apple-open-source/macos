//
//  HIDEventSystemClientPrivate.h
//  IOHIDFamily
//
//  Created by dekom on 12/20/17.
//

#ifndef HIDEventSystemClientPrivate_h
#define HIDEventSystemClientPrivate_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDEventSystemClient (priv)

@property (readonly) IOHIDEventSystemClientRef   client;

/*!
 * @method initWithType:andAttributes:
 *
 * @abstract
 * Creates a HIDEventSystemClient object of the specified type.
 *
 * @param type
 * The client type to initialize.
 *
 * @param attributes
 * Attributes dictionary assoicated with client.
 *
 * @result
 * Returns an instance of a HIDEventSystemClient object on success.
 */
- (nullable instancetype)initWithType:(HIDEventSystemClientType)type andAttributes:(NSDictionary* __nullable) attributes;

@end


NS_ASSUME_NONNULL_END

#endif /* HIDEventSystemClient_h */
