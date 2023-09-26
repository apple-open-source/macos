/*!
 * HIDServiceClient.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#ifndef HIDServiceClient_h
#define HIDServiceClient_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>
#import <IOKit/hidobjc/HIDServiceClientBase.h>

NS_ASSUME_NONNULL_BEGIN

/*!
 * @category HIDServiceClient
 *
 * @abstract
 * A client of a HID service in the HID event system.
 *
 * @discussion
 * HID services represent a HID compliant entity on the system
 * that is capable of dispatching HID events. Clients of a HID service
 * are able to interact with the service by setting or getting properties
 * from it and querying for matching events.
 *
 * To subscribe to all of the events a service generates,
 * use a HIDEventSystemClient that matches the service.
 *
 * HIDServiceClients should not be created, but received using other APIs.
 */
@interface HIDServiceClient (HIDFramework)

- (instancetype)init NS_UNAVAILABLE;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Obtains a property from the service.
 *
 * @param key
 * The property key to query.
 *
 * @result
 * The property on success, nil on failure.
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
 * A dictionary of properties on success, nil on failure.
 */
- (nullable NSDictionary *)propertiesForKeys:(NSArray<NSString *> *)keys;

/*!
 * @method setProperty
 *
 * @abstract
 * Sets a property on the service.
 *
 * @param value
 * The value of the property to set.
 *
 * @param key
 * The property key to set.
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
 * The device usage page to check.
 *
 * @param usage
 * The device usage to check.
 *
 * @result
 * true if the service conforms to the provided usages, false otherwise.
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
 * The matching HIDEvent on success, nil otherwise.
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
 * The service ID associated with the service.
 */
@property (readonly) uint64_t serviceID;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDServiceClient_h */
