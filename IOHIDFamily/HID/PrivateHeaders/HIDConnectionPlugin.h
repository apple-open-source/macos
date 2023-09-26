/*!
 * HIDConnectionPlugin.h
 * HID
 *
 * Copyright © 2023 Apple Inc. All rights reserved.
 */

#ifndef HIDConnectionPlugin_h
#define HIDConnectionPlugin_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase_Private.h>

NS_ASSUME_NONNULL_BEGIN

// Forward declarations
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
@protocol HIDConnectionPlugin <NSObject>

/*!
 * @method matchService
 *
 * @abstract
 * Allows the conneciton plugin to query a connection to determine if it should load
 * against that connection.
 *
 * @param connection
 * The connection under inspection.
 *
 * @result
 * true if the plugin should be loaded, false otherwise.
 */
+ (BOOL)matchConnection:(HIDConnection *)connection;

/*!
 * @method initWithConnection
 *
 * @abstract
 * Create a HIDConnectionPlugin for the corresponding connection.
 *
 * @result
 * A HIDConnectionPlugin instance on success, nil on failure.
 */
- (nullable instancetype)initWithConnection:(HIDConnection *)connection;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Obtain a property from the service plugin.
 *
 * @param key
 * The property key being queried.
 *
 * @result
 * The property on success, nil on failure.
 */
- (nullable id)propertyForKey:(NSString *)key;

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
 **
 * @result
 * true on success, false on failure.
 */
- (BOOL)setProperty:(nullable id)value forKey:(NSString *)key;

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

/*!
 * @method filterEvent
 *
 * @abstract
 * Filter an event for the provided service.
 *
 * @discussion
 * The filterEvent method provides the connection filter with a stream of events
 * from every service. The connection filter may observe, modify, or drop the event
 * if it chooses. If the filter is only observing the events, it should return
 * the event unmodified.
 *
 * @param event
 * The event to filter.
 *
 *
 * @result
 * A filtered event, or nil if the event should be dropped.
 */
- (nullable HIDEvent *)filterEvent:(HIDEvent *)event;

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


@end

NS_ASSUME_NONNULL_END

#endif /* HIDConnectionPlugin_h */
