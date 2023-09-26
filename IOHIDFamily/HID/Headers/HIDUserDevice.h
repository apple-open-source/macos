/*!
 * HIDUserDevice.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#ifndef HIDUserDevice_h
#define HIDUserDevice_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>

NS_ASSUME_NONNULL_BEGIN

/*!
 * @constant kHIDUserDeviceCreateInactiveKey
 *
 * @abstract
 * HIDUserDevices initialized with this property will wait until activation to register the device.
 *
 * @discussion
 * With this key set to @true in the dictionary provided to [HIDUserDevice initWithProperties:],
 * registration of the device service will be deffered until [HIDUserDevice activate].
 * It is recommended to use this key when intending to support the set/get report handlers.
 */
HID_EXPORT NSString * const kHIDUserDevicePropertyCreateInactiveKey;

/*!
 * @typedef HIDUserDeviceSetReportHandler/HIDUserDeviceGetReportHandler
 *
 * @abstract
 * The block types used for handling set and get reports.
 */
typedef IOReturn (^HIDUserDeviceSetReportHandler)(HIDReportType type, NSInteger reportID, const void * report, NSInteger reportLength);
typedef IOReturn (^HIDUserDeviceGetReportHandler)(HIDReportType type, NSInteger reportID, void * report, NSInteger * reportLength);

/*!
 * @interface HIDUserDevice
 *
 * @abstract
 * A virtual HID device.
 *
 * @discussion
 * Typically, HID devices are representations of phyiscal hardware connected to the system
 * that a user would interact with, such as a keyboard. However, there are many situations where a
 * process may want to interact with the HID event system as a HID device without human
 * interaction with physical hardware. HIDUserDevices fill this gap by allowing a process to
 * create a virtual representation of a HID device that can perform many of the actions of a
 * true HID device, such as sending input reports, receiving set and get reports, etc..
 *
 * For a process to use this interface, it must possess the com.apple.developer.hid.virtual.device
 * entitlement.
 *
 * On MacOS, certain actions will require one-time TCC approval from the user.
 */
@interface HIDUserDevice : NSObject

- (instancetype)init NS_UNAVAILABLE;

/*!
 * @method initWithProperties
 *
 * @abstract
 * Creates a HIDUserDevice with the specified properties.
 *
 * @param properties
 * Dictionary containing device properties from the keys defined in
 * <IOKit/hid/IOHIDKeys.h>, see also <code>kHIDUserDeviceCreateInactiveKey</code>
 *
 * @result
 * A HIDUserDevice instance on success, nil on failure.
 */
- (nullable instancetype)initWithProperties:(NSDictionary *)properties;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Obtains a property from the device.
 *
 * @param key
 * The property key to query.
 *
 * @result
 * The property on success, nil on failure.
 */
- (nullable id)propertyForKey:(NSString *)key;

/*!
 * @method setProperty
 *
 * @abstract
 * Sets a property on the device.
 *
 * @param value
 * The value of the property to set.
 *
 * @param key
 * The property key to set.
 *
 * @result
 * true on success, false on failure.
 */
- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key;

/*!
 * @method setGetReportHandler
 *
 * @abstract
 * Registers a handler to receive getReport calls.
 *
 * @discussion
 * This call must occur before the device is activated. The device must be
 * activated in order to receive getReport calls.
 *
 * @param handler
 * The handler to receive getReport calls.
 */
- (void)setGetReportHandler:(HIDUserDeviceGetReportHandler)handler;

/*!
 * @method setSetReportHandler
 *
 * @abstract
 * Registers a handler to receive setReport calls.
 *
 * @discussion
 * This call must occur before the device is activated. The device must be
 * activated in order to receive setReport calls.
 *
 * @param handler
 * The handler to receive setReport calls.
 */
- (void)setSetReportHandler:(HIDUserDeviceSetReportHandler)handler;

/*!
 * @method setCancelHandler
 *
 * @abstract
 * Sets a cancellation handler for the dispatch queue associated with the
 * device.
 *
 * @discussion
 * The cancellation handler (if specified) will be submitted to the device's
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
 * Sets the dispatch queue to be associated with the HIDUserDevice.
 *
 * @discussion
 * This is necessary in order to receive asynchronous events from the kernel.
 *
 * A call to setDispatchQueue should only be made once.
 *
 * After a dispatch queue is set, the HIDUserDevice must make a call to activate
 * via activate and cancel via cancel. All handler method calls should be made
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
 * Activates the HIDUserDevice.
 *
 * @discussion
 * A HIDUserDevice associated with a dispatch queue is created in an
 * inactive state. The device must be activated in order to receive asynchronous
 * events from the kernel.
 *
 * A dispatch queue must be set via setDispatchQueue before activation.
 *
 * An activated device must be cancelled via cancel. All handler method calls
 * should be made before activation and not after cancellation.
 *
 * Calling activate on an active HIDUserDevice has no effect.
 */
- (void)activate;

/*!
 * @method cancel
 *
 * @abstract
 * Cancels the HIDUserDevice preventing any further invocation of its event
 * handler block.
 *
 * @discussion
 * Cancelling prevents any further invocation of the event handler block for the
 * specified dispatch queue, but does not interrupt an event handler block that
 * is already in progress.
 *
 * Explicit cancellation of the HIDUserDevice is required, no implicit
 * cancellation takes place.
 *
 * Calling cancel on an already cancelled device has no effect.
 */
- (void)cancel;

/*!
 * @method handleReport
 *
 * @abstract
 * Dispatch an input report into the system.
 *
 * @param report
 * The report bytes to dispatch.
 *
 * @param outError
 * A reference to an NSError that will be filled with an error object on
 * failure. The reference will be unchanged on success.
 *
 * @result
 * true on success, false on failure.
 */
- (BOOL)handleReport:(NSData *)report
               error:(out NSError * _Nullable * _Nullable)outError;

/*!
 * @method handleReport:withTimestamp:
 *
 * @abstract
 * Dispatch an input report into the system.
 *
 * @param report
 * The report bytes to dispatch.
 *
 * @param timestamp
 * Report timestamp in mach absolute time.
 *
 * @param outError
 * A reference to an NSError that will be filled with an error object on
 * failure. The reference will be unchanged on success.
 *
 * @result
 * true on success, false on failure.
 */
- (BOOL)handleReport:(NSData *)report
       withTimestamp:(uint64_t)timestamp
               error:(out NSError * _Nullable * _Nullable)outError;

/*!
 * @property service
 *
 * @abstract
 * The IOService associated with the HIDUserDevice.
 */
@property (readonly) io_service_t service;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDUserDevice_h */
