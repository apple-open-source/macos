//
//  HIDServiceFilter.h
//  HID
//
//  Created by dekom on 9/20/18.
//

#ifndef HIDServiceFilter_h
#define HIDServiceFilter_h

#import <Foundation/Foundation.h>
#import "HIDBase_Private.h"

NS_ASSUME_NONNULL_BEGIN

@class HIDEventService;
@class HIDConnection;
@class HIDEvent;

@protocol HIDServiceFilter <NSObject>

/*!
 * @method matchService
 *
 * @abstract
 * Allows the service filter query a service to determine if it should load.
 *
 * @discussion
 * A service filter will first be passively matched against a service using
 * the "Matching" dictionary provided in the bundle's Info.plist. After passive
 * matching, this method will be called to perform any additional matching
 * and provide a match score if need be.
 *
 * @param service
 * The service that the service filter should load against.
 *
 * @param options
 * An optional dictionary of options that can be queried for matching.
 *
 * @param score
 * An optional match score to return. Service filters are loaded by highest
 * match score first.
 *
 * @result
 * Returns true if the filter should be loaded.
 */
+ (BOOL)matchService:(HIDEventService *)service
             options:(nullable NSDictionary *)options
               score:(NSInteger *)score;

/*!
 * @method initWithService
 *
 * @abstract
 * Creates a HIDServiceFilter object for the corresponding service.
 *
 * @result
 * Returns an instance of a HIDServiceFilter object on success.
 */
- (nullable instancetype)initWithService:(HIDEventService *)service;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Obtains a property from the service filter.
 *
 * @param key
 * The property key.
 *
 * @param client
 * The client requesting the property, if any.
 *
 * @result
 * Returns the property on success.
 */
- (nullable id)propertyForKey:(NSString *)key
                       client:(nullable HIDConnection *)client;

/*!
 * @method setProperty
 *
 * @abstract
 * Sets a property on the service filter.
 *
 * @param value
 * The value of the property.
 *
 * @param key
 * The property key.
 *
 * @param client
 * The client setting the property, if any.
 *
 * @result
 * Returns true on success.
 */
- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key
             client:(nullable HIDConnection *)client;

/*!
 * @method filterEvent
 *
 * @abstract
 * Filters an event for the service.
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
 * Returns a filtered event, or nil if the event should be dropped.
 */
- (nullable HIDEvent *)filterEvent:(HIDEvent *)event;

/*!
 * @method filterCopyEvent
 *
 * @abstract
 * Filters the eventMatching call made on a HIDEventService.
 *
 * @param matching
 * A dictionary of matching criteria that the service filter may use to decide
 * which event to return.
 *
 * @param event
 * An event returned from a service's eventMatching call.
 *
 * @param client
 * The client requesting the event, if any.
 *
 * @result
 * Returns a matching HIDEvent on success, or nil if the event should be
 * dropped.
 */
- (nullable HIDEvent *)filterEventMatching:(nullable NSDictionary *)matching
                                     event:(HIDEvent *)event
                                 forClient:(nullable HIDConnection *)client;

/*!
 * @method setCancelHandler
 *
 * @abstract
 * Sets a cancellation handler on the service filter.
 *
 * @discussion
 * The cancellation handler *must* be invoked after the service filter has been
 * cancelled. This will allow for any asynchronous events to finish. If there
 * are no pending asynchronous events, the filter may call the cancellation
 * handler directly from the cancel method. The cancel handler should be invoked
 * on the dispatch queue provided in the setDispatchQueue method.
 *
 * Please note: The dispatch queue provided here should be used only for filter
 * related work. Kernel calls, or calls that make take some time should be done
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
 * Activates the service filter.
 *
 * @discussion
 * A HIDServiceFilter object is created in an inactive state. The object will
 * be activated after it has been initialized, and the setDispatchQueue,
 * setEventDispatchHandler, and setCancelHandler methods are invoked.
 */
- (void)activate;

/*!
 * @method cancel
 *
 * @abstract
 * Cancels the service filter.
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
 * be handled by the caller if need be.
 *
 * @param queue
 * The dispatch queue object to be used by the service filter.
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
 * The filter should release any strong references to the client when the client
 * is removed.
 *
 * @param client
 * The client that is added/removed.
 *
 * @param added
 * True if the client is added, false if it is being removed.
 */
- (void)clientNotification:(HIDConnection *)client added:(BOOL)added;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDServiceFilter_h */
