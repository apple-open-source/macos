//
//  HIDManager.h
//  HID
//
//  Created by dekom on 10/31/17.
//

#ifndef HIDManager_h
#define HIDManager_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>

NS_ASSUME_NONNULL_BEGIN

/*!
 * @typedef HIDManagerOptions
 *
 * @abstract
 * Enumerator of options to be passed in to the initWithOptions method.
 *
 * @field HIDManagerIndependentDevices
 * Devices maintained by the manager will act independently from calls to the
 * manager. This allows for devices to be scheduled on separate queues, and
 * their lifetime can persist after the manager is gone.
 *
 * The following calls will not be propagated to the devices:
 *     setInputElementMatching, setInputElementHandler, setInputReportHandler,
 *     setCancelHandler, setDispatchQueue, open, close, activate, cancel.
 *
 * This also means that the manager will not be able to receive input reports or
 * input elements, since the devices may or may not be scheduled.
 */
typedef NS_OPTIONS(NSInteger, HIDManagerOptions) {
    HIDManagerIndependentDevices = 1 << 3
};

@class HIDDevice;

/*!
 * @typedef HIDDeviceHandler
 *
 * @abstract
 * The type of block used for HIDDevice notifications.
 */
typedef void (^HIDDeviceHandler)(HIDDevice *device, BOOL added);

/*!
 * @typedef HIDManagerElementHandler
 *
 * @abstract
 * The type block used for input element updates.
 */
typedef void (^HIDManagerElementHandler)(HIDDevice *sender,
                                         HIDElement *element);

@interface HIDManager : NSObject

/*!
 * @method initWithOptions
 *
 * @abstract
 * Creates a HIDManager object with the specified options.
 *
 * @param options
 * Options defined by the HIDManagerOptions enumerator.
 *
 * @result
 * Returns a HIDManager instance.
 */
- (instancetype)initWithOptions:(HIDManagerOptions)options;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Obtains a property from the HIDManager.
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
 * Sets a property on the HIDManager. The property will be applied to all
 * devices owned by the HIDManager.
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
 * @method setInputElementMatching
 *
 * @abstract
 * Sets matching criteria for element values received via setInputElementHandler
 * method.
 *
 * @discussion
 * Matching keys are prefixed by kIOHIDElement and declared in
 * <IOKit/hid/IOHIDKeys.h>. Passing an empty dictionary or array will result in
 * all elements being matched. If interested in multiple, specific device
 * elements, an NSArray of NSDictionaries may be passed in. This call must occur
 * before the manager is activated.
 *
 * Please note: This call is propagated to all devices owned by the HIDManager.
 * If this behavior is not desired, pass in the HIDManagerIndependentDevices
 * option to the initWithOptions method when creating the HIDManager.
 *
 * @param matching
 * An NSArray or NSDictionary containing matching criteria.
 */
- (void)setInputElementMatching:(id)matching;

/*!
 * @method setInputElementHandler
 *
 * @abstract
 * Registers a handler to be used when an updated element value is issued by the
 * device.
 *
 * @discussion
 * An input element refers to any element of type kIOHIDElementTypeInput and is
 * usually issued by interrupt driven reports.
 *
 * If more specific element values are desired, you may specify matching
 * criteria via the setInputElementMatching method.
 *
 * This call must occur before the manager is activated. The manager must be
 * open and activated in order to receive element updates.
 *
 * Please note: This call is propagated to all devices owned by the HIDManager.
 * If this behavior is not desired, pass in the HIDManagerIndependentDevices
 * option to the initWithOptions method when creating the HIDManager.
 *
 * @param handler
 * The handler to receive input elements.
 */
- (void)setInputElementHandler:(HIDManagerElementHandler)handler;

/*!
 * @method setDeviceMatching
 *
 * @abstract
 * Sets matching criteria for device enumeration.
 *
 * @discussion
 * Matching keys are declared in <IOKit/hid/IOHIDKeys.h>. Passing an empty
 * dictionary or array will result in all devices being enumerated. If
 * interested in multiple, specific devices, an NSArray of NSDictionaries may be
 * passed in. This call must occur before the manager is activated.
 *
 * @param matching
 * An NSArray or NSDictionary containing matching criteria.
 */
- (void)setDeviceMatching:(id)matching;

/*!
 * @method setDeviceNotificationHandler
 *
 * @abstract
 * Registers a handler to be used when a HIDDevice is enumerated or removed.
 *
 * @discussion
 * This call must occur before the manager is activated. The manager must be
 * activated in order to receive enumeration notifications.
 *
 * @param handler
 * The handler to receive enumeration/removal notifications.
 */
- (void)setDeviceNotificationHandler:(HIDDeviceHandler)handler;

/*!
 * @method setInputReportHandler
 *
 * @abstract
 * Registers a handler to be recieve input reports from enumerated devices.
 *
 * @discussion
 * This call must occur before the manager is activated. The manager must be
 * open and activated in order to receive input reports.
 *
 * Please note: This call is propagated to all devices owned by the HIDManager.
 * If this behavior is not desired, pass in the HIDManagerIndependentDevices
 * option to the initWithOptions method when creating the HIDManager.
 *
 * @param handler
 * The handler to receive input reports.
 */
- (void)setInputReportHandler:(HIDReportHandler)handler;

/*!
 * @method setCancelHandler
 *
 * @abstract
 * Sets a cancellation handler for the dispatch queue associated with the
 * manager.
 *
 * @discussion
 * The cancellation handler (if specified) will be submitted to the manager's
 * dispatch queue in response to a call to cancel after all the events have been
 * handled.
 *
 * Please note: This call is propagated to all devices owned by the HIDManager.
 * If this behavior is not desired, pass in the HIDManagerIndependentDevices
 * option to the initWithOptions method when creating the HIDManager.
 *
 * @param handler
 * The cancellation handler block to be associated with the dispatch queue.
 */
- (void)setCancelHandler:(HIDBlock)handler;

/*!
 * @method setDispatchQueue
 *
 * @abstract
 * Sets the dispatch queue to be associated with the HIDManager.
 * This is necessary in order to receive asynchronous events from the kernel.
 *
 * @discussion
 * A call to setDispatchQueue should only be made once.
 *
 * If a dispatch queue is set but never used, a call to cancel followed by
 * activate should be performed in that order.
 *
 * After a dispatch queue is set, the HIDManager must make a call to activate
 * via activate and cancel via cancel. All matching/handler method calls
 * should be done before activation and not after cancellation.
 *
 * Please note: This call is propagated to all devices owned by the HIDManager.
 * If this behavior is not desired, pass in the HIDManagerIndependentDevices
 * option to the initWithOptions method when creating the HIDManager.
 *
 * @param queue
 * The dispatch queue to which the event handler block will be submitted.
 */
- (void)setDispatchQueue:(dispatch_queue_t)queue;

/*!
 * @method open
 *
 * @abstract
 * Opens the HIDManager for communication with the devices.
 *
 * @discussion
 * Before the client can issue commands that change the state of the devices, it
 * must have succeeded in opening the devices. This establishes a link between
 * the client's task and the actual devices.
 *
 * Please note: This call is propagated to all devices owned by the HIDManager.
 * If this behavior is not desired, pass in the HIDManagerIndependentDevices
 * option to the initWithOptions method when creating the HIDManager.
 */
- (void)open;

/*!
 * @method close
 *
 * @abstract
 * Closes communication with the HIDManager and associated devices.
 *
 * Please note: This call is propagated to all devices owned by the HIDManager.
 * If this behavior is not desired, pass in the HIDManagerIndependentDevices
 * option to the initWithOptions method when creating the HIDManager.
 *
 * @discussion
 * This closes a link between the client's task and the actual devices.
 */
- (void)close;

/*!
 * @method activate
 *
 * @abstract
 * Activates the HIDManager object.
 *
 * @discussion
 * A HIDManager object associated with a dispatch queue is created in an
 * inactive state. The object must be activated in order to receive asynchronous
 * events from the kernel.
 *
 * A dispatch queue must be set via setDispatchQueue before activation.
 *
 * An activated device must be cancelled via cancel. All matching/handler method
 * calls should be done before activation and not after cancellation.
 *
 * Calling activate on an active HIDManager has no effect.
 *
 * Please note: This call is propagated to all devices owned by the HIDManager.
 * If this behavior is not desired, pass in the HIDManagerIndependentDevices
 * option to the initWithOptions method when creating the HIDManager.
 */
- (void)activate;

/*!
 * @method cancel
 *
 * @abstract
 * Cancels the HIDManager preventing any further invocation of its event handler
 * block.
 *
 * @discussion
 * Cancelling prevents any further invocation of the event handler block for the
 * specified dispatch queue, but does not interrupt an event handler block that
 * is already in progress.
 *
 * Explicit cancellation of the HIDManager is required, no implicit cancellation
 * takes place.
 *
 * Calling cancel on an already cancelled manager has no effect.
 *
 * Please note: This call is propagated to all devices owned by the HIDManager.
 * If this behavior is not desired, pass in the HIDManagerIndependentDevices
 * option to the initWithOptions method when creating the HIDManager.
 */
- (void)cancel;

/*!
 * @property devices
 *
 * @abstract
 * Returns an array of enumerated HID devices.
 *
 * @discussion
 * The manager should set matching device critera in the setDeviceMatching
 * method. If no matching criteria is provided, all currently enumerated devices
 * will be returned.
 */
@property (readonly) NSArray<HIDDevice *> *devices;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDManager_h */
