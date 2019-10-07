//
//  HIDEventSystemClient.h
//  IOHIDFamily
//
//  Created by dekom on 12/20/17.
//

#ifndef HIDEventSystemClient_h
#define HIDEventSystemClient_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>

NS_ASSUME_NONNULL_BEGIN

@class HIDServiceClient;
@class HIDEvent;

/*!
 * @typedef HIDEventSystemClientType
 *
 * @abstract
 * Enumerator of HIDEventSystemClient types.
 *
 * @field HIDEventSystemClientTypeAdmin
 * Admin client will receive blanket access to all HIDEventSystemClient API,
 * and will receive events before monitor/rate controlled clients. This client
 * type requires entitlement 'com.apple.private.hid.client.admin', and in
 * general should not be used.
 *
 * @field HIDEventSystemClientTypeMonitor
 * Client type used for receiving HID events from the HID event system. Requires
 * entitlement 'com.apple.private.hid.client.event-monitor'.
 *
 * @field HIDEventSystemClientTypePassive
 * Client type that does not require any entitlements, but may not receive HID
 * events. Passive clients can be used for querying/setting properties on
 * HID services.
 *
 * @field HIDEventSystemClientTypeRateControlled
 * Client type used for receiving HID events from the HID event system. This is
 * similar to the monitor client, except rate controlled clients have the
 * ability to set the report and batch interval for the services they are
 * monitoring. Requires entitlement 'com.apple.private.hid.client.event-monitor'.
 *
 * @field HIDEventSystemClientTypeSimple
 * Public client type used by third parties. Simple clients do not have the
 * ability to monitor events, and have a restricted set of properties on which
 * they can query/set on a HID service.
 */
typedef NS_ENUM(NSInteger, HIDEventSystemClientType) {
    HIDEventSystemClientTypeAdmin,
    HIDEventSystemClientTypeMonitor,
    HIDEventSystemClientTypePassive,
    HIDEventSystemClientTypeRateControlled,
    HIDEventSystemClientTypeSimple
};

/*!
 * @typedef HIDEventHandler
 *
 * @abstract
 * The type of block used for receiving HID events.
 */
typedef void (^HIDEventHandler)(HIDServiceClient * _Nullable service,
                                HIDEvent *event);

/*!
 * @typedef HIDEventFilterHandler
 *
 * @abstract
 * The type of block used for event filtering. See setEventFilterHandler method
 * for more information.
 */
typedef BOOL (^HIDEventFilterHandler)(HIDServiceClient * _Nullable service,
                                      HIDEvent *event);

/*!
 * @typedef HIDServiceHandler
 *
 * @abstract
 * The type of block used for receiving service added notifications.
 */
typedef void (^HIDServiceHandler)(HIDServiceClient *service);

/*!
 * @typedef HIDPropertyChangedHandler
 *
 * @abstract
 * The type of block used for property change notifications.
 */
typedef void (^HIDPropertyChangedHandler)(NSString *property,
                                          id value);

@interface HIDEventSystemClient : NSObject

- (instancetype)init NS_UNAVAILABLE;

/*!
 * @method initWithType
 *
 * @abstract
 * Creates a HIDEventSystemClient object of the specified type.
 *
 * @param type
 * The client type to initialize.
 *
 *
 * @result
 * Returns an instance of a HIDEventSystemClient object on success.
 */
- (nullable instancetype)initWithType:(HIDEventSystemClientType)type;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Obtains a property for the client.
 *
 * @discussion
 * Iterates through the HID session's properties, starting first with the
 * currently loaded session filters.
 *
 * If properties from specific services are desired, the HIDServiceClient's
 * copyProperty method should be used.
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
 * Sets a property on behalf of the client.
 *
 * @discussion
 * The property will be applied to the HID session, all loaded session
 * session filters, along with all services and service filters.
 *
 * If you would like to set a property on a specific service, the
 * HIDServiceClient's setProperty method should be used.
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
 * @method setMatching
 *
 * @abstract
 * Sets matching criteria for interested services.
 *
 * @discussion
 * Matching keys should be based off of the desired service's properties, some
 * of which are defined in <IOKit/hid/IOHIDKeys.h>. Passing an empty dictionary
 * or array will result in all services being matched. If interested in multiple,
 * specific services, an NSArray of NSDictionaries may be passed in. This call
 * must occur before the client is activated.
 *
 * Example matching NSDictionary for mouse:
 * matching = @{ @kIOHIDPrimaryUsagePageKey : @(kHIDPage_GenericDesktop),
 *               @kIOHIDPrimaryUsageKey : @(kHIDUsage_GD_Mouse) };
 *
 * Example matching NSArray for keyboard and mouse:
 * matching = @[ @{@kIOHIDPrimaryUsagePageKey: @(kHIDPage_GenericDesktop),
 *                  @kIOHIDPrimaryUsageKey: @(kHIDUsage_GD_Mouse)},
 *               @{@kIOHIDPrimaryUsagePageKey: @(kHIDPage_GenericDesktop),
 *                  @kIOHIDPrimaryUsageKey: @(kHIDUsage_GD_Keyboard)} ];
 *
 * @param matching
 * An NSArray or NSDictionary containing matching criteria.
 */
- (void)setMatching:(id)matching;

/*!
 * @method setCancelHandler
 *
 * @abstract
 * Sets a cancellation handler for the dispatch queue associated with the
 * client.
 *
 * @discussion
 * The cancellation handler (if specified) will be submitted to the client's
 * dispatch queue in response to a call to cancel after all the events have been
 * handled.
 *
 * @param handler
 * The cancellation handler block to be associated with the dispatch queue.
 */
- (void)setCancelHandler:(HIDBlock)handler;

/*!
 * @method setDispatchQueue
 *
 * @abstract
 * Sets the dispatch queue to be associated with the client.
 * This is necessary in order to receive asynchronous events.
 *
 * @discussion
 * A call to setDispatchQueue should only be made once.
 *
 * If a dispatch queue is set but never used, a call to cancel followed by
 * activate should be performed in that order.
 *
 * After a dispatch queue is set, the client must make a call to activate
 * via activate and cancel via cancel. All matching/handler method calls
 * should be done before activation and not after cancellation.
 *
 * @param queue
 * The dispatch queue to which the event handler block will be submitted.
 */
- (void)setDispatchQueue:(dispatch_queue_t)queue;

/*!
 * @method setEventHandler
 *
 * @abstract
 * Registers a handler to recieve HID events from matched services.
 *
 * @discussion
 * This call must occur before the client is activated. The client must be
 * activated in order to receive events.
 *
 * The client must have the kIOHIDEventSystemClientEventMonitorEntitlement
 * entitlement in order to receive keyboard, digitizer, or ponter events,
 * unless it is of type kHIDEventSystemClientTypeAdmin (which requires the
 * kIOHIDEventSystemClientAdminEntitlement entitlement).
 *
 * @param handler
 * The handler to receive events.
 */
- (void)setEventHandler:(HIDEventHandler)handler;

/*!
 * @method setResetHandler
 *
 * @abstract
 * Registers a handler to receive reset notifications from the HID server.
 *
 * @discussion
 * This block will be invoked when the HID server resets. This can occur when
 * the process running the server crashes. No action on the client is necessary,
 * the connection will be re-established internally.
 *
 * This call must occur before the client is activated. The client must be
 * activated in order to receive reset notifications.
 *
 * @param handler
 * The handler to receive reset notifications.
 */
- (void)setResetHandler:(HIDBlock)handler;

/*!
 * @method setEventFilterHandler
 *
 * @abstract
 * Registers a handler to filter events.
 *
 * @discussion
 * A client may register an event filter handler with the event system to filter
 * specific events from being dispatched. The client should return true if the
 * event should not be dispatched, and false otherwise.
 *
 * This call must occur before the client is activated. The client must be
 * activated in order to receive events. The client must have the
 * kIOHIDEventSystemClientEventFilterEntitlement entitlement in order to receive
 * event filter calls.
 *
 * @param handler
 * The handler to receive events.
 */
- (void)setEventFilterHandler:(HIDEventFilterHandler)handler;

/*!
 * @method setServiceNotificationHandler
 *
 * @abstract
 * Registers a handler to receive service added notifications.
 *
 * @discussion
 * If a client is interested in receiving service removal notifications, the
 * HIDServiceClient's setRemovalHandler method may be used. The client should
 * set this handler within the context of this service notification handler in
 * order to be gauranteed delivery of the notification.
 *
 * This call must occur before the client is activated. The client must be
 * activated in order to receive events.
 *
 * @param handler
 * The handler to receive events.
 */
- (void)setServiceNotificationHandler:(HIDServiceHandler)handler;

/*!
 * @method setPropertyChangedHandler
 *
 * @abstract
 * Registers a handler to be recieve notifications when a property changes.
 *
 * @discussion
 * This call must occur before the client is activated. The client must be
 * activated in order to receive events.
 *
 * @param handler
 * The handler to receive property changed notifications.
 *
 * @param matching
 * An NSString or NSArray of NSStrings containing matching properties.
 *
 */
- (void)setPropertyChangedHandler:(HIDPropertyChangedHandler)handler
                         matching:(id)matching;

/*!
 * @method activate
 *
 * @abstract
 * Activates the HIDEventSystemClient object.
 *
 * @discussion
 * A client object associated with a dispatch queue is created in an inactive
 * state. The object must be activated in order to receive asynchronous events.
 *
 * A dispatch queue must be set via setDispatchQueue before activation.
 *
 * An activated client must be cancelled via cancel. All matching/handler method
 * calls should be done before activation and not after cancellation.
 *
 * Calling activate on an active client has no effect.
 */
- (void)activate;

/*!
 * @method cancel
 *
 * @abstract
 * Cancels the client preventing any further invocation of its event handler
 * block.
 *
 * @discussion
 * Cancelling prevents any further invocation of the event handler block for the
 * specified dispatch queue, but does not interrupt an event handler block that
 * is already in progress.
 *
 * Explicit cancellation of the client is required, no implicit cancellation
 * takes place.
 *
 * Calling cancel on an already cancelled client has no effect.
 */
- (void)cancel;

/*!
 * @property services
 *
 * @abstract
 * Returns an array of HID services matching the client's criteria.
 *
 * @discussion
 * The client should set matching service critera in the setMatching method. If
 * no matching criteria is provided, all currently enumerated services will be
 * returned.
 */
@property (readonly) NSArray<HIDServiceClient *> *services;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDEventSystemClient_h */
