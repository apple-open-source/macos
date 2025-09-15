/*!
 * HIDEventService.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#ifndef HIDEventService_h
#define HIDEventService_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>
#import <HID/HIDConnection.h>
#import <IOKit/hidobjc/HIDServiceBase.h>

NS_ASSUME_NONNULL_BEGIN

/*!
 * @category HIDEventService
 *
 * @abstract
 * Direct interaction with a HID service.
 *
 * @discussion
 * This should only be used by system code.
 */
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
 * @method registerWithSystem
 *
 * @abstract
 *  Triggers matching for a service with the current set of active clients
 *  Intended for use with "unregistered" services (through the kIOHIDServiceUnregisteredKey)
 */
- (void)registerWithSystem;


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
 * @param client
 * Connection that sets property
 *
 * @result
 * Returns true on success.
 */
- (BOOL)setProperty:(id)value forKey:(NSString *)key forClient:(HIDConnection *)client;

/*!
 * @method workIntervalStart
 *
 * @abstract
 * Starts a libdispatch os_workgroup_interval_t which allows CLPC to provide an appropriate level of CPU performance
 * to complete the work in the necessary deadline.
 *
 * @discussion
 * To be called at the start of a performance critical piece of work(ex: event dispatch).
 * Must be called on the workloop and not yielded until IOHIDServiceWorkIntervalFinish(or IOHIDServiceWorkIntervalCancel) is called.
 * A work interval cannot be started until the previous one was finished.
 * 
 * @param start
 * Time in mach absolute time when the work was started, can be now or a time in the past.
 * 
 * @param deadline
 * Time in mach absolute time when the work needs to be completed by.
 *
 * @param complexity
 * Signal the complexity of work, predefined application-specific values.
 *
 * @result
 * Returns the bsd error code produced by libdispatch.
 * EINVAL returned if work interval already started.
 * EBUSY returned if someone else is concurrently in a start, update or finish.
 * ENOTSUP returned if workgroup intervals are not supported on this device.
 */
- (int)workIntervalStart:(uint64_t)start deadline:(uint64_t)deadline complexity:(uint64_t)complexity;

/*!
 * @method   IOHIDServiceWorkIntervalUpdate
 * 
 * @abstract
 * Updates the deadline for a previously started workgroup interval.
 * 
 * @param deadline
 * Updated time in mach absolute time when the work needs to be completed by.
 * 
 * @param complexity
 * Signal the complexity of work, predefined application-specific values.
 * 
 * @result
 * Returns the bsd error code produced by libdispatch.
 * EINVAL is returned if interval is not yet started.
 * EBUSY is returned if someone else is concurrently in a start, update or finish.
 * ENOTSUP returned if workgroup intervals are not supported on this device.
 */
- (int)workIntervalUpdate:(uint64_t)deadline complexity:(uint64_t)complexity;

/*!
 * @method IOHIDServiceWorkIntervalFinish
 * 
 * @abstract
 * Finishes the workgroup interval.
 * 
 * @discussion
 * To be called when the performance critical piece of work is finished.
 * Must be called before dropping the workloop.
 * 
 * @result
 * Returns the bsd error code produced by libdispatch.
 * EINVAL is returned if interval is not started.
 * EBUSY is returned if someone else is concurrently in a start, update or finish.
 * ENOTSUP returned if workgroup intervals are not supported on this device.
 */
- (int)workIntervalFinish;

/*!
 * @method IOHIDServiceWorkIntervalCancel
 *
 * @abstract
 * Cancels an in-progress workgroup interval.
 * 
 * @discussion
 * To be used if the work is no longer needed or performance critical.
 * 
 * @result
 * Returns the bsd error code produced by libdispatch.
 * EINVAL is returned if interval is not started.
 * EBUSY is returned if someone else is concurrently in a start, update or finish.
 * ENOTSUP returned if workgroup intervals are not supported on this device.
 */
- (int)workIntervalCancel;

/*!
 * @method eventStatistics
 *
 * @abstract
 * return dictionary that represent statistics of dispatched events
 *
 * @result
 * NSDictionary of key value pair with key represent event name and value represent event count
 */
- (nullable NSDictionary *)eventStatistics;


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
