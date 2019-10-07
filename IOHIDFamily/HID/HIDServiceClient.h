//
//  HIDServiceClient.h
//  IOHIDFamily
//
//  Created by dekom on 12/22/17.
//

#ifndef HIDServiceClient_h
#define HIDServiceClient_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>
#import <IOKit/hidobjc/HIDServiceClientBase.h>

NS_ASSUME_NONNULL_BEGIN

@class HIDEvent;

@interface HIDServiceClient (HIDFramework)

- (instancetype)init NS_UNAVAILABLE;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Obtains a property from the service.
 *
 * @param key
 * The property key.
 *
 * @result
 * Returns the property on success.
 */
- (nullable id)propertyForKey:(NSString *)key;

/*!
 * @method propertiesForKeys
 *
 * @abstract
 * Obtains multiple properties from the service.
 *
 * @param keys
 * The property keys to query.
 *
 * @result
 * Returns a dictionary of properties on success.
 */
- (nullable NSDictionary *)propertiesForKeys:(NSArray<NSString *> *)keys;

/*!
 * @method setProperty
 *
 * @abstract
 * Sets a property on the service.
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

/*!
 * @method conformsToUsagePage
 *
 * @abstract
 * Iterates through the service's usage pairs to see if the service conforms to
 * the provided usage page and usage.
 *
 * @param usagePage
 * The device usage page.
 *
 * @param usage
 * The device usage.
 *
 * @result
 * Returns true if the service conforms to the provided usages.
 */
- (BOOL)conformsToUsagePage:(NSInteger)usagePage usage:(NSInteger)usage;

/*!
 * @method eventMatching
 *
 * @abstract
 * Queries the service for an event matching the criteria in the provided
 * dictionary.
 *
 * @param matching
 * Optional matching criteria that can be passed to the service.
 *
 * @result
 * Returns a HIDEvent on success.
 */
- (nullable HIDEvent *)eventMatching:(nullable NSDictionary *)matching;

/*!
 * @method setRemovalHandler
 *
 * @abstract
 * Registers a handler to be invoked when the service is removed.
 *
 * @param handler
 * The handler to receive the removal notification.
 */
- (void)setRemovalHandler:(HIDBlock)handler;

/*!
 * @property serviceID
 *
 * @abstract
 * The serviceID associated with the service.
 */
@property (readonly) uint64_t serviceID;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDServiceClient_h */
