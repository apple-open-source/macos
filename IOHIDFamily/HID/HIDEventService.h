//
//  HIDEventService.h
//  HID
//
//  Created by dekom on 9/13/18.
//

#ifndef HIDEventService_h
#define HIDEventService_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>
#import <IOKit/hidobjc/HIDServiceBase.h>

@class HIDEvent;

NS_ASSUME_NONNULL_BEGIN

@interface HIDEventService (HIDFramework)

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
 * @property serviceID
 *
 * @abstract
 * The serviceID associated with the service.
 */
@property (readonly) uint64_t serviceID;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDEventService_h */
