/*!
 * HIDSession.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#ifndef HIDSession_h
#define HIDSession_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>
#import <IOKit/hidobjc/HIDSessionBase.h>

NS_ASSUME_NONNULL_BEGIN

/*!
 * @category HIDSession
 *
 * @abstract
 * Direct interaction with the HID session.
 *
 * @discussion
 * Should only be used by system code.
 */
@interface HIDSession (HIDFramework)

- (instancetype)init NS_UNAVAILABLE;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Obtains a property from the session.
 *
 * @param key
 * The property key to query.
 *
 * @result
 * The property on success, nil on failure.
 */
- (nullable id)propertyForKey:(NSString *)key;

/*!
 * @method setProperty
 *
 * @abstract
 * Sets a property on the session.
 *
 * @param value
 * The value of the property to set.
 *
 * @param key
 * The property key to set.
 *
 * @result
 * true on success, nil on failure.
 */
- (BOOL)setProperty:(nullable id)value forKey:(NSString *)key;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDSession_h */
