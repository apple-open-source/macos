/*
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2018-2022 Apple Computer, Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef HIDVirtualEventService_h
#define HIDVirtualEventService_h

#import <Foundation/Foundation.h>
#import <HID/HIDEvent.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * @typedef HIDVirtualServiceNotificationType
 *
 * @abstract
 * Enumerator of notification types.
 */
typedef NS_ENUM(NSInteger, HIDVirtualServiceNotificationType) {
    HIDVirtualServiceNotificationTypeEnumerated = 10,
    HIDVirtualServiceNotificationTypeTerminated
};


/*!
 * @protocol HIDVirtualEventServiceDelegate
 *
 * @abstract
 * A protocol used for objects that handle HIDVirtualEventService functionality.
 */
@protocol HIDVirtualEventServiceDelegate <NSObject>

/*!
 * @method setProperty
 *
 * @abstract
 * Handle setting a property on the service.
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
- (BOOL) setProperty:(nullable id)value forKey:(NSString *)key forService:(id) service;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Handle obtaining a property from the service.
 *
 * @param key
 * The property key.
 *
 * @result
 * Returns the property on success.
 */
- (nullable id) propertyForKey:(NSString *)key forService:(id) service;

/*!
 * @method copyEventMatching
 *
 * @abstract
 * Handle service queries for an event matching the criteria in the provided
 * dictionary.
 *
 * @param matching
 * Optional matching criteria that can be passed to the service.
 *
 * @result
 * Returns a HIDEvent on success.
 */
- (nullable HIDEvent *) copyEventMatching:(nullable NSDictionary *)matching forService:(id) service;

/*!
 * @method setOutputEvent
 *
 * @abstract
 * Handle service interface for output events
 * dictionary.
 *
 * @param event
 * Event received by service.
 *
 * @result
 * Returns a true on success.
 */
- (BOOL) setOutputEvent:(nullable HIDEvent *) event forService:(id) service;

/*!
 * @method notification
 *
 * @abstract
 * Queries the service for an event matching the criteria in the provided
 * dictionary.
 *
 * @param type
 * Notification type.
 *
 * @param property
 * Notification property.
 */
- (void) notification:(HIDVirtualServiceNotificationType) type withProperty:(nullable NSDictionary *) property forService:(id) service;

@end


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
 * This is necessary in order to receive asynchronous events from the kernel.
 *
 * @discussion
 * A call to setDispatchQueue should only be made once.
 *
 * After a dispatch queue is set, the HIDVirtualEventService must make a call to activate
 * via activate and cancel via cancel. All handler method calls should be done
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
 * A HIDVirtualEventService object associated with a dispatch queue is created in an
 * inactive state. The object must be activated in order to receive delegate interface
 * calls from HID event system.
 *
 * A dispatch queue must be set via setDispatchQueue before activation.
 *
 * An activated device must be cancelled via cancel. All handler method calls
 * should be done before activation and not after cancellation.
 *
 * Calling activate on an active HIDUserDevice has no effect.
 */
- (void)activate;

/*!
 * @method cancel
 *
 * @abstract
 * Cancels the HIDVirtualEventService preventing any further invocation of its event
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
 * Calling cancel on an already cancelled device has no effect.
 */
- (void)cancel;

/*!
 * @method dispatchEvent
 *
 * @abstract
 * Dispatch event to event system
 *
 * @param event
 * The event to be dispatch to event  system.
 *
 *@result true if event was successfully dispatched.
 */
- (BOOL) dispatchEvent: (HIDEvent *) event;


/*!
 * @property serviceID
 *
 * @abstract
 * The service ID object  with the virtual service.
 */
@property (readonly) uint64_t serviceID;

/*!
 * @property delegate
 *
 * @abstract
 *  delegate handler for the virtual HID service interface
 */
@property (weak) id <HIDVirtualEventServiceDelegate> delegate;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDUserDevice_h */
