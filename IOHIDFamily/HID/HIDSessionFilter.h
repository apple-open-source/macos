//
//  HIDSessionFilter.h
//  HID
//
//  Created by dekom on 9/20/18.
//

#ifndef HIDSessionFilter_h
#define HIDSessionFilter_h

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class HIDEventService;
@class HIDEvent;
@class HIDSession;

@protocol HIDSessionFilter <NSObject>

/*!
 * @method initWithSession
 *
 * @abstract
 * Creates a HIDSessionFilter object for the corresponding session.
 *
 * @result
 * Returns an instance of a HIDSessionFilter object on success.
 */
- (nullable instancetype)initWithSession:(HIDSession *)session;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Obtains a property from the session filter.
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
 * Sets a property on the session filter.
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
- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key;

/*!
 * @method filterEvent
 *
 * @abstract
 * Filters an event for the provided service.
 *
 * @discussion
 * The filterEvent method provides the session filter with a stream of events
 * from every service. The session filter may observe, modify, or drop the event
 * if it chooses. If the filter is only observing the events, it should return
 * the event unmodified.
 *
 * @param event
 * The event to filter.
 *
 * @param service
 * The service associated with the event.
 *
 * @result
 * Returns a filtered event, or nil if the event should be dropped.
 */
- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                        forService:(HIDEventService *)service;

/*!
 * @method activate
 *
 * @abstract
 * Activates the session filter.
 *
 * @discussion
 * A HIDSessionFilter object is created in an inactive state. The object will
 * be activated after it has been initialized, and the setDispatchQueue and
 * setCancelHandler methods are invoked.
 */
- (void)activate;

@optional

/*!
 * @method serviceNotification
 *
 * @abstract
 * Method invoked when a service is added or removed from the HID event system.
 * The filter should release any strong references to the service when the
 * service is removed.
 *
 * @param service
 * The service that is enumerated/terminated.
 *
 * @param added
 * True if the service is enumerated, false if it is terminating.
 */
- (void)serviceNotification:(HIDEventService *)service added:(BOOL)added;

/*!
 * @method setDispatchQueue
 *
 * @abstract
 * Provides the session filter with a dispatch queue to be used for
 * synchronization and handling asynchronous tasks.
 *
 * Please note: The dispatch queue provided here should be used only for filter
 * related work. Kernel calls, or calls that make take some time should be done
 * on a separate queue, so as not to hold up the whole HID event system.
 *
 * @param queue
 * The dispatch queue object to be used by the session filter.
 */
- (void)setDispatchQueue:(dispatch_queue_t)queue;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDSessionFilter_h */
