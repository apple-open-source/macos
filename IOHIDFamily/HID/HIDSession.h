//
//  HIDSession.h
//  IOHIDFamily
//
//  Created by dekom on 9/13/18.
//

#ifndef HIDSession_h
#define HIDSession_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>
#import <IOKit/hidobjc/HIDSessionBase.h>

@class HIDEvent;

NS_ASSUME_NONNULL_BEGIN

@interface HIDSession (HIDFramework)

- (instancetype)init NS_UNAVAILABLE;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Obtains a property from the sesion.
 *
 * @param key
 * The property key.
 *
 * @result
 * Returns the property on success.
 */
- (nullable id)propertyForKey:(NSString *)key;

/*!
 * @method setProperty
 *
 * @abstract
 * Sets a property on the sesion.
 *
 * @param value
 * The value of the property.
 *
 * @param key
 * The property key.
 *
 * @result
 * Returns true on success.
 */
- (BOOL)setProperty:(nullable id)value forKey:(NSString *)key;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDSession_h */
