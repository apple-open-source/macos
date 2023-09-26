/*!
 * HIDServiceFilter.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#ifndef HIDServiceFilter_h
#define HIDServiceFilter_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase_Private.h>

NS_ASSUME_NONNULL_BEGIN

// Forward declarations
@class HIDEventService;
@class HIDConnection;

/*!
 * @protocol HIDServiceFilter
 *
 * @abstract
 * A protocol for creating HID service filters.
 *
 * @discussion
 * HID service filters match to HID services and are able to filter calls that the
 * service receives.
 */
@protocol HIDServiceFilter <NSObject>

/*!
 * @method matchService
 *
 * @abstract
 * Allows the service filter to query a service to determine if it should load
 * against that service.
 *
 * @discussion
 * A service filter will first be passively matched against a service using
 * the "Matching" dictionary provided in the bundle's Info.plist. After passive
 * matching, this method will be called to perform any additional matching
 * and update the match score.
 *
 * @param service
 * The service under inspection.
 *
 * @param options
 * An optional dictionary of options that can be queried for matching.
 *
 * @param score
 * An optional match score to return. Service filters are loaded by highest
 * match score first.
 *
 * @result
 * true if the filter should be loaded, false otherwise.
 */
+ (BOOL)matchService:(HIDEventService *)service
             options:(nullable NSDictionary *)options
               score:(NSInteger *)score;

/*!
 * @method initWithService
 *
 * @abstract
 * Create a HIDServiceFilter for the corresponding service.
 *
 * @param service
 * The service that is associated with the filter.
 *
 * @result
 * A HIDServiceFilter instance on success.
 */
- (nullable instancetype)initWithService:(HIDEventService *)service;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Obtain a property from the service filter.
 *
 * @param key
 * The property key being queried.
 *
 * @param client
 * The client requesting the property, if any.
 *
 * @result
 * The property on success, nil on failure.
 */
- (nullable id)propertyForKey:(NSString *)key
                       client:(nullable HIDConnection *)client;

/*!
 * @method setProperty
 *
 * @abstract
 * Set a property on the service filter.
 *
 * @param value
 * The value of the property to set.
 *
 * @param key
 * The property key to set.
 *
 * @param client
 * The client setting the property, if any.
 *
 * @result
 * true on success, false on failure.
 */
- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key
             client:(nullable HIDConnection *)client;

/*!
 * @method filterEvent
 *
 * @abstract
 * Filter an event for the service.
 *
 * @discussion
 * The filter method provides the service filter with a stream of events from
 * the service. The service filter may observe, modify, or drop the event if
 * it chooses. If the filter is only observing the events, it should return the
 * event unmodified.
 *
 * @param event
 * The event to filter.
 *
 * @result
 * A filtered event, or nil if the event should be dropped.
 */
- (nullable HIDEvent *)filterEvent:(HIDEvent *)event;

/*!
 * @method filterCopyEvent
 *
 * @abstract
 * Filter the eventMatching call made on a HIDEventService.
 *
 * @param matching
 * A dictionary of matching criteria that the service filter may use to decide
 * which event to return.
 *
 * @param event
 * The event returned from a service's eventMatching call.
 *
 * @param client
 * The client requesting the event, if any.
 *
 * @result
 * A matching HIDEvent, or nil if the event should be dropped.
 */
- (nullable HIDEvent *)filterEventMatching:(nullable NSDictionary *)matching
                                     event:(HIDEvent *)event
                                 forClient:(nullable HIDConnection *)client;

/*!
 * @method setCancelHandler
 *
 * @abstract
 * Set a cancellation handler on the service filter.
 *
 * @discussion
 * The cancellation handler *must* be invoked after the service filter has been
 * cancelled. This will allow for any asynchronous events to finish. If there
 * are no pending asynchronous events, the filter may call the cancellation
 * handler directly from the cancel method. The cancel handler should be invoked
 * on the dispatch queue provided in the setDispatchQueue method.
 *
 * Please note: The dispatch queue provided here should be used only for filter
 * related work. Kernel calls, or calls that make take some time should be made
 * on a separate queue, so as not to hold up the whole HID event system.
 *
 * @param handler
 * The cancellation handler block to be invoked after the plugin is cancelled.
 */
- (void)setCancelHandler:(HIDBlock)handler;

/*!
 * @method activate
 *
 * @abstract
 * Activate the service filter.
 *
 * @discussion
 * A HIDServiceFilter is created in an inactive state. The filter will
 * be activated after it has been initialized, and the setDispatchQueue,
 * setEventDispatchHandler, and setCancelHandler methods are invoked.
 */
- (void)activate;

/*!
 * @method cancel
 *
 * @abstract
 * Cancel the service filter.
 *
 * @discussion
 * A HIDServiceFilter will be cancelled when the service associated with the
 * filter terminates. The filter is responsible for calling the cancel handler
 * provided in the setCancelHandler method once all asynchronous activity is
 * finished and the filter is ready to be released.
 */
- (void)cancel;

@optional

/*!
 * @method setDispatchQueue
 *
 * @abstract
 * Provides the service filter with a dispatch queue to be used for
 * synchronization and handling asynchronous tasks.
 *
 * @discussion
 * All calls made to the filter will be synchronized against the dispatch queue
 * provided here. Calls made by the filter are not gauaranteed to be
 * synchronized against this dispatch queue, so proper synchronization should
 * be handled by the caller if needed.
 *
 * @param queue
 * The dispatch queue to be used by the service filter.
 */
- (void)setDispatchQueue:(dispatch_queue_t)queue;

/*!
 * @method setEventDispatcher
 *
 * @abstract
 * Provides the plugin with a delegate that may be used for dispatching events.
 *
 * @param dispatcher
 * The delegate that the plugin should use to dispatch an event.
 */
- (void)setEventDispatcher:(id<HIDEventDispatcher>)dispatcher;

/*!
 * @method clientNotification
 *
 * @abstract
 * Method invoked when a client is added or removed from the HID event system.
 *
 * @discussion
 * The filter should release any strong references to the client when the client
 * is removed.
 *
 * @param client
 * The client that is being added/removed.
 *
 * @param added
 * True if the client is added, false if it is being removed.
 */
- (void)clientNotification:(HIDConnection *)client added:(BOOL)added;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDServiceFilter_h */
