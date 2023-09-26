/*!
 * HIDVirtualEventService.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#ifndef HIDVirtualEventService_h
#define HIDVirtualEventService_h

#import <Foundation/Foundation.h>
#import <HID/HIDEvent.h>

NS_ASSUME_NONNULL_BEGIN

/*!
 * @enum HIDVirtualServiceNotificationType
 *
 * @discussion
 * The HIDVirtualServiceNotificationType enum refers to the current state of the
 * virtual service from the perspective of the clients. Specifically, these notification types establish if
 * the service is in a state where it can properly interact with clients.
 *
 * @constant HIDVirtualServiceNotificationTypeEnumerated
 * Notification on the delegate with this type indicates that service can recieve events
 * from any active clients, prior to this notification there is no guarantee events will be recieved.
 * It is possible to send events in the handler for this notification or any
 * time after until the termination notification, as the virtual service is guaranteed to be registered.
 *
 * @constant HIDVirtualServiceNotificationTypeTerminated
 * Notification on the delegate with this type indicates that the service is terminated/stale.
 * Service will need to be re-instantiated in order to be actively used by clients. It will not be
 * possible to send or recieve events in the handler for this notification; by that point
 * there will be no event system to handle events.
 */
typedef NS_ENUM(NSInteger, HIDVirtualServiceNotificationType) {
    HIDVirtualServiceNotificationTypeEnumerated = 10,
    HIDVirtualServiceNotificationTypeTerminated
};


/*!
 * @protocol HIDVirtualEventServiceDelegate
 *
 * @abstract
 * A protocol used for objects that will handle HIDVirtualEventService functionality via delegate callbacks.
 *
 * @discussion
 * A HIDVirtualEventServices can receive several types of requests, such as setting a property, querying a
 * property, querying events, etc.. These requests are piped out to a delegate that can be set on the service.
 * This delegate must conform to the HIDVirtualEventServiceDelegate protocol.
 */
@protocol HIDVirtualEventServiceDelegate <NSObject>

/*!
 * @method setProperty
 *
 * @abstract
 * Handle a request to set a property on the service.
 *
 * @param value
 * The value of the property to set.
 *
 * @param key
 * The property key to set.
 *
 * @param service
 * Currently always set to the ID of the HIDVirtualEventService.
 *
 * @result
 * true on success, false on failure.
 */
- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key
         forService:(id)service;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Handle a request to query a property from the service.
 *
 * @param key
 * The key of the requested property.
 *
 * @param service
 * Currently always set to the ID of the HIDVirtualEventService.
 *
 * @result
 * The requested property on success, nil on failure.
 */
- (nullable id)propertyForKey:(NSString *)key
                   forService:(id)service;

/*!
 * @method copyEventMatching
 *
 * @abstract
 * Handle a request to query for an event matching the criteria in the provided dictionary.
 *
 * @param matching
 * Optional matching criteria for the event.
 *
 * @param service
 * Currently always set to the ID of the HIDVirtualEventService.
 *
 * @result
 * The matching HIDEvent on success, nil on failure.
 */
- (nullable HIDEvent *)copyEventMatching:(nullable NSDictionary *)matching
                              forService:(id)service;

/*!
 * @method setOutputEvent
 *
 * @abstract
 * Handle a request to set an output event on the service.
 *
 * @param event
 * The output event received.
 *
 * @param service
 * Currently always set to the ID of the HIDVirtualEventService.
 *
 * @result
 * true on success, false on failure.
 */
- (BOOL)setOutputEvent:(nullable HIDEvent *)event
            forService:(id)service;

/*!
 * @method notification
 *
 * @abstract
 * Handle a notification sent to the service.
 *
 * @param type
 * The type of notification received.
 * The possible notification types are enumerated above.
 *
 * @param property
 * Notification property. Currently always nil.
 *
 * @param service
 * Currently always set to the ID of the HIDVirtualEventService.
 */
- (void)notification:(HIDVirtualServiceNotificationType)type
        withProperty:(nullable NSDictionary *)property
          forService:(id)service;

@end

/*!
 * @interface HIDVirtualEventService
 *
 * @abstract
 * A virtual HID service used to create a corresponding HID service in the HID event system.
 *
 * @discussion
 * HID services typically correspond to physically connected HID compliant input devices. This
 * interface is one method to virtually represent a HID device without physical hardware. The virtual
 * service will create a corresponding HID service in the HID event system that clients can interact with.
 * The virtual service can also interact with the HID event system by dispatching events.
 *
 * For a process to use this interface, it must possess the com.apple.private.hid.client.event-dispatch
 * entitlement.
 */
@interface HIDVirtualEventService : NSObject

/*!
 * @method setCancelHandler
 *
 * @abstract
 * Sets a cancellation handler for the dispatch queue associated with the
 * virtual event service.
 *
 * @discussion
 * The cancellation handler (if specified) will be submitted to the virtual event
 * service dispatch queue in response to a call to cancel after all the events have
 * been handled.
 *
 * @param handler
 * The cancellation handler block to be associated with the dispatch queue.
 */
- (void)setCancelHandler:(HIDBlock)handler;

/*!
 * @method setDispatchQueue
 *
 * @abstract
 * Sets the dispatch queue to be associated with the HIDVirtualEventService.
 *
 * @discussion
 * This is necessary in order to receive asynchronous events from the kernel.
 * A call to setDispatchQueue should only be made once.
 *
 * After a dispatch queue is set, the HIDVirtualEventService must make a call to activate
 * before use and later to cancel before releasing. A call to setCancelHandler should be made
 * before activation and not after cancellation.
 *
 * @param queue
 * The dispatch queue to which the event handler block will be submitted.
 */
- (void)setDispatchQueue:(dispatch_queue_t)queue;

/*!
 * @method activate
 *
 * @abstract
 * Activates the HIDVirtualEventService object.
 *
 * @discussion
 * A HIDVirtualEventService associated with a dispatch queue is created in an
 * inactive state. The service must be activated in order to receive delegate interface
 * calls from HID event system.
 *
 * A dispatch queue must be set via setDispatchQueue before activation.
 *
 * An activated device must be cancelled via cancel. A call to setCancelHandler
 * should be made before activation and not after cancellation.
 *
 * Calling activate on an active HIDVirtualEventService has no effect.
 */
- (void)activate;

/*!
 * @method cancel
 *
 * @abstract
 * Cancels the HIDVirtualEventService, preventing any further invocation of its event
 * handle block.
 *
 * @discussion
 * Cancelling prevents any further invocation of the event handler block for the
 * specified dispatch queue, but does not interrupt an event handler block that
 * is already in progress.
 *
 * Explicit cancellation of the HIDVirtualEventService is required, no implicit
 * cancellation takes place.
 *
 * Calling cancel on an inactive HIDVirtualEventService has no effect.
 */
- (void)cancel;

/*!
 * @method dispatchEvent
 *
 * @abstract
 * Dispatches an event into the HID event system.
 *
 * @param event
 * The event to be dispatched into the HID event system.
 *
 *@result
 * true if the event was successfully dispatched, false otherwise.
 */
- (BOOL)dispatchEvent:(HIDEvent *)event;

/*!
 * @property serviceID
 *
 * @abstract
 * The service ID of the HIDVirtualEventService.
 */
@property (readonly) uint64_t serviceID;

/*!
 * @property delegate
 *
 * @abstract
 * Delegate for the HIDVirtualEventService, requests to the service will be forwarded here.
 */
@property (weak) id <HIDVirtualEventServiceDelegate> delegate;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDUserDevice_h */
