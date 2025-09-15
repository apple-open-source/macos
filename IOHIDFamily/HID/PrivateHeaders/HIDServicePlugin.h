/*!
 * HIDServicePlugin.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#ifndef HIDServicePlugin_h
#define HIDServicePlugin_h

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
 * A protocol for creating HID service plugins.
 *
 * @discussion
 * A HID service plugin matches to a HID service and is able to handle calls that the
 * service receives.
 */
@protocol HIDServicePlugin <NSObject>

/*!
 * @method matchService
 *
 * @abstract
 * Allows the service plugin to query a service to determine if it should load
 * against that service.
 *
 * @param service
 * The service under inspection.
 *
 * @param options
 * An optional dictionary of options that can be queried for matching.
 *
 * @param score
 * An optional match score to return. The service plugin with the highest
 * match score will be loaded.
 *
 * @result
 * true if the plugin should be loaded, false otherwise.
 */
+ (BOOL)matchService:(io_service_t)service
             options:(nullable NSDictionary *)options
               score:(NSInteger *)score;

/*!
 * @method initWithService
 *
 * @abstract
 * Create a HIDServicePlugin for the corresponding service.
 *
 * @result
 * A HIDServicePlugin instance on success, nil on failure.
 */
- (nullable instancetype)initWithService:(io_service_t)service;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Obtain a property from the service plugin.
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
 * Set a property on the service plugin.
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
 * @method eventMatching
 *
 * @abstract
 * Query the service plugin for an event matching the criteria in the provided
 * dictionary.
 *
 * @param matching
 * A dictionary of matching criteria that the service may use to decide which
 * event to return.
 *
 * @param client
 * The client requesting the event, if any.
 *
 * @result
 * A matching HIDEvent on success, nil on failure.
 */
- (nullable HIDEvent *)eventMatching:(nullable NSDictionary *)matching
                           forClient:(nullable HIDConnection *)client;

/*!
 * @method setEventDispatcher
 *
 * @abstract
 * Provides the plugin with a delegate that may be used for dispatching events.
 *
 * @param dispatcher
 * The delegate that the filter should use to dispatch an event.
 */
- (void)setEventDispatcher:(id<HIDEventDispatcher>)dispatcher;

/*!
 * @method setCancelHandler
 *
 * @abstract
 * Set a cancellation handler on the plugin.
 *
 * @discussion
 * The cancellation handler *must* be invoked after the service plugin has been
 * cancelled. This will allow for any asynchronous events to finish. If there
 * are no pending asynchronous events, the plugin may call the cancellation
 * handler directly from the cancel method. The cancel handler should be invoked
 * on the dispatch queue provided in the setDispatchQueue method.
 *
 * @param handler
 * The cancellation handler block to be invoked after the plugin is cancelled.
 */
- (void)setCancelHandler:(HIDBlock)handler;

/*!
 * @method activate
 *
 * @abstract
 * Activate the service plugin.
 *
 * @discussion
 * A HIDServicePlugin is created in an inactive state. The plugin will
 * be activated after it has been initialized, and the setDispatchQueue,
 * setEventDispatchHandler, and setCancelHandler methods are invoked.
 */
- (void)activate;

/*!
 * @method cancel
 *
 * @abstract
 * Cancel the service plugin.
 *
 * @discussion
 * A HIDServicePlugin will be cancelled when the service associated with the
 * plugin terminates. The plugin is responsible for calling the cancel handler
 * provided in the setCancelHandler method once all asynchronous activity is
 * finished and the plugin is ready to be released.
 */
- (void)cancel;

@optional

/*!
 * @method setDispatchQueue
 *
 * @abstract
 * Provides the plugin with a dispatch queue to be used for synchronization and
 * handling asynchronous tasks.
 *
 * @discussion
 * All calls made to the plugin will be synchronized against the dispatch queue
 * provided here. Calls made by the plugin are not gauaranteed to be
 * synchronized against this dispatch queue, so proper synchronization should
 * be handled by the caller if needed.
 *
 * Please note: The dispatch queue provided here should be used only for plugin
 * related work. Kernel calls, or calls that make take some time should be made
 * on a separate queue, so as not to hold up the whole HID event system.
 *
 * @param queue
 * The dispatch queue to be used by the plugin.
 */
- (void)setDispatchQueue:(dispatch_queue_t)queue;

/*!
 * @method clientNotification
 *
 * @abstract
 * Method invoked when a client is added or removed from the HID event system.
 *
 * @discussion
 * The plugin should release any strong references to the client when the client
 * is removed.
 *
 * @param client
 * The client that is being added/removed.
 *
 * @param added
 * True if the client is added, false if it is being removed.
 */
- (void)clientNotification:(HIDConnection *)client added:(BOOL)added;

/*!
 * @method setHIDEventService
 *
 * @abstract
 * Provides the plugin with the HIDEventService to make calls directly.
 *
 * @param service
 * The service that can be used to make HIDEventService and IOHIDService calls.
 */
- (void)setHIDEventService:(HIDEventService *)service;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDServicePlugin_h */
