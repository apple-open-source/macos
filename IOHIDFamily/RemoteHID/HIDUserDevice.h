//
//  HIDUserDevice.h
//  HID
//
//  Created by dekom on 10/9/17.
//

#ifndef HIDUserDevice_h
#define HIDUserDevice_h

#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDKeys.h>
//#import "HIDBase.h"

/*!
 * @typedef HIDUserDeviceSetReportHandler
 *
 * @abstract
 * The type block used for HIDUserDevice setReport blocks.
 */
typedef IOReturn (^HIDUserDeviceSetReportHandler)(IOHIDReportType type,
                                                  uint32_t reportID,
                                                  uint8_t * _Nullable report,
                                                  NSUInteger reportLength);

/*!
 * @typedef HIDUserDeviceGetReportHandler
 *
 * @abstract
 * The type block used for HIDUserDevice getReport blocks.
 */
typedef IOReturn (^HIDUserDeviceGetReportHandler)(IOHIDReportType type,
                                                  uint32_t reportID,
                                                  uint8_t * _Nullable report,
                                                  NSUInteger * _Nonnull reportLength);

@interface HIDUserDevice : NSObject

- (nullable instancetype)init NS_UNAVAILABLE;

/*!
 * @method initWithProperties
 *
 * @abstract
 * Creates a HIDUserDevice object with the specified properties.
 *
 * @param properties
 * Dictionary containing device properties indexed by keys defined in
 * <IOKit/hid/IOHIDKeys.h>.
 *
 * @result
 * Returns an instance of a HIDUserDevice object on success.
 */
- (nullable instancetype)initWithProperties:(nonnull NSDictionary *)properties;

/*!
 * @method isEqualToHIDUserDevice
 *
 * @abstract
 * Compares two HIDUserDevice objects.
 *
 * @param device
 * The HIDUserDevice to compare against.
 *
 * @result
 * Returns true if the devices are equal.
 */
- (BOOL)isEqualToHIDUserDevice:(nullable HIDUserDevice *)device;

/*!
 * @method setGetReportHandler
 *
 * @abstract
 * Registers a handler to recieve getReport calls.
 *
 * @discussion
 * This call must occur before the device is activated. The device must be
 * activated in order to receive getReport calls.
 *
 * @param handler
 * The handler to receive getReport calls.
 */
- (void)setGetReportHandler:(nonnull HIDUserDeviceGetReportHandler)handler;

/*!
 * @method setSetReportHandler
 *
 * @abstract
 * Registers a handler to recieve setReport calls.
 *
 * @discussion
 * This call must occur before the device is activated. The device must be
 * activated in order to receive setReport calls.
 *
 * @param handler
 * The handler to receive setReport calls.
 */
- (void)setSetReportHandler:(nonnull HIDUserDeviceSetReportHandler)handler;

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
- (void)setCancelHandler:(nonnull dispatch_block_t)handler;

/*!
 * @method setDispatchQueue
 *
 * @abstract
 * Sets the dispatch queue to be associated with the HIDUserDevice.
 * This is necessary in order to receive asynchronous events from the kernel.
 *
 * @discussion
 * A call to setDispatchQueue should only be made once.
 *
 * After a dispatch queue is set, the HIDUserDevice must make a call to activate
 * via activate and cancel via cancel. All handler method calls should be done
 * before activation and not after cancellation.
 *
 * @param queue
 * The dispatch queue to which the event handler block will be submitted.
 */
- (void)setDispatchQueue:(nonnull dispatch_queue_t)queue;

/*!
 * @method activate
 *
 * @abstract
 * Activates the HIDUserDevice object.
 *
 * @discussion
 * A HIDUserDevice object associated with a dispatch queue is created in an
 * inactive state. The object must be activated in order to receive asynchronous
 * events from the kernel.
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
 * Cancels the HIDUserDevice preventing any further invocation of its event
 * handle block.
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
 * Dispatch a report to the HIDUserDevice.
 *
 * @param report
 * The report bytes.
 *
 * @param reportLength
 * The length of the report.
 *
 * @result
 * Returns kIOReturnSuccess on success.
 */
- (IOReturn)handleReport:(nonnull uint8_t *)report
            reportLength:(NSInteger)reportLength;

/*!
 * @property service
 *
 * @abstract
 * The IOService object associated with the HIDUserDevice.
 */
@property (readonly) io_service_t service;

@end

#endif /* HIDUserDevice_h */
