/*
*
* @APPLE_LICENSE_HEADER_START@
*
* Copyright (c) 2019 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef HIDUserDevice_h
#define HIDUserDevice_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>

NS_ASSUME_NONNULL_BEGIN

/*!
    @constant    kHIDUserDeviceCreateInactiveKey
    @abstract
        HID user devices created with property  will create  representation of the device once
        object activated.
    @discussion
        With key set in to @YES in property dictionary provided in [HIDUserDevice initWithProperties:]
        creation of device representation will be deffered until  [HIDUserDevice activate].
        Its recommended to use key for HIDUserDevice which intend to support set/get  report handler
        <code>HIDUserDeviceGetReportHandler</code>, <code>HIDUserDeviceGetReportHandler</code>
 */

HID_EXPORT NSString * const kHIDUserDevicePropertyCreateInactiveKey;


/*!
 * @typedef HIDUserDeviceSetReportHandler
 *
 * @abstract
 * The type block used for HIDUserDevice setReport blocks.
 */
typedef IOReturn (^HIDUserDeviceSetReportHandler)(HIDReportType type,
                                                  NSInteger reportID,
                                                  const void *report,
                                                  NSInteger reportLength);

/*!
 * @typedef HIDUserDeviceGetReportHandler
 *
 * @abstract
 * The type block used for HIDUserDevice getReport blocks.
 *
 * @discussion
 * The implementor of this block may update the reportLength variable to reflect
 * the actual length of the returned report.
 */
typedef IOReturn (^HIDUserDeviceGetReportHandler)(HIDReportType type,
                                                  NSInteger reportID,
                                                  void *report,
                                                  NSInteger *reportLength);

@interface HIDUserDevice : NSObject

- (instancetype)init NS_UNAVAILABLE;

/*!
 * @method initWithProperties
 *
 * @abstract
 * Creates a HIDUserDevice object with the specified properties.
 *
 * @param properties
 * Dictionary containing device properties indexed by keys defined in
 * <IOKit/hid/IOHIDKeys.h>, see also <code>kHIDUserDeviceCreateInactiveKey</code>
 *
 * @result
 * Returns an instance of a HIDUserDevice object on success.
 */
- (nullable instancetype)initWithProperties:(NSDictionary *)properties;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Obtains a property from a HIDUserDevice.
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
 * Sets a property on a HIDUserDevice.
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
- (void)setGetReportHandler:(HIDUserDeviceGetReportHandler)handler;

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
- (void)setDispatchQueue:(dispatch_queue_t)queue;

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
 * @param outError
 * An error returned on failure.
 *
 * @result
 * Returns YES on success.
 */
- (BOOL)handleReport:(NSData *)report
               error:(out NSError * _Nullable * _Nullable)outError;

/*!
 * @method handleReport:withTimestamp:
 *
 * @abstract
 * Dispatch a report to the HIDUserDevice.
 *
 * @param report
 * The report bytes.
 *
 * @param timestamp
 * Report timestamp in mach absolute time
 *
 * @param outError
 * An error returned on failure.
 *
 * @result
 * Returns YES on success.
 */
- (BOOL)handleReport:(NSData *)report
       withTimestamp:(uint64_t)timestamp
               error:(out NSError * _Nullable * _Nullable)outError;

/*!
 * @property service
 *
 * @abstract
 * The IOService object associated with the HIDUserDevice.
 */
@property (readonly) io_service_t service;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDUserDevice_h */
