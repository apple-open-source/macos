/*!
 * HIDDevice.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#ifndef HIDDevice_h
#define HIDDevice_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>
#import <IOKit/hidobjc/HIDDeviceBase.h>

NS_ASSUME_NONNULL_BEGIN

/*!
 * @typedef HIDDeviceOptions
 *
 * @abstract
 * Enumeration of options that can be passed in to the openWithOptions method.
 *
 * @field HIDDeviceOptionsNone
 * No special options, the same as calling open directly.
 *
 * @field HIDDeviceOptionsNoDroppedReports
 * Used to prevent reports from getting dropped when the report queue is full.
 *
 * @field HIDDeviceOptionsIgnoreTaskSuspend
 * Used with the HIDDeviceOptionsNoDroppedReports option, even when
 * suspended the device will wait to send any report if the queue is full.
 */
typedef NS_OPTIONS(NSInteger, HIDDeviceOptions) {
    HIDDeviceOptionsNone              = 0,
    HIDDeviceOptionsSeize             = 0x1,
    HIDDeviceOptionsNoDroppedReports  = 0x1 << 16,
    HIDDeviceOptionsIgnoreTaskSuspend = 0x2 << 16,
};

/*!
 * @typedef HIDDeviceCommitDirection
 *
 * @abstract
 * Commit direction passed to the commitElements method.
 */
typedef NS_ENUM(NSInteger, HIDDeviceCommitDirection) {
    HIDDeviceCommitDirectionIn,
    HIDDeviceCommitDirectionOut,
};

/*!
 * @typedef HIDDeviceElementHandler/HIDDeviceBatchElementHandler
 *
 * @abstract
 * The block types used for input element updates.
 */
typedef void (^HIDDeviceElementHandler)(HIDElement * element);
typedef void (^HIDDeviceBatchElementHandler)(NSArray<HIDElement *> * elements);

/*!
 * @typedef HIDDeviceReportCallback/HIDDeviceCommitCallback
 *
 * @abstract
 * The block types used for asynchronous callbacks.
 */
typedef void (^HIDDeviceReportCallback)(IOReturn status, const void * report, NSInteger reportLength, NSInteger reportID);
typedef void (^HIDDeviceCommitCallback)(IOReturn status);

/*!
 * @category HIDDevice
 *
 * @abstract
 * A connection to a HID device on the system.
 *
 * @discussion
 * This connection enables communication with an IOHIDDevice service in the kernel. This is the
 * recommended method to receive input reports, query elements and initiate pull or push
 * requests in the form of set/get reports for a specified HID device on the system.
 *
 * On MacOS, opening certain device types will require one-time TCC approval from the user.
 */
@interface HIDDevice (HIDFramework)

- (instancetype)init NS_UNAVAILABLE;

/*!
 * @method initWithService
 *
 * @abstract
 * Creates a HIDDevice for the specified IOService.
 *
 * @param service
 * The IOService associated with the IOHIDDevice.
 * The service must reference an object in the kernel of type IOHIDDevice.
 * Services can be matched using methods from IOKit/IOKitLib.h.
 *
 * @result
 * A HIDDevice instance on success, nil on failure.
 */
- (nullable instancetype)initWithService:(io_service_t)service;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Obtains a property from the associated HID device service.
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
 * Sets a property on the associated HID device service.
 *
 * @param value
 * The value to set for the property.
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
 * @method conformsToUsagePage
 *
 * @abstract
 * Convenience function that scans the application collection elements to see if
 * the device conforms to the provided usage page and usage.
 *
 * @param usagePage
 * The device usage page to check.
 *
 * @param usage
 * The device usage to check.
 *
 * @result
 * true if the device conforms to the provided usages, false otherwise.
 */
- (BOOL)conformsToUsagePage:(NSInteger)usagePage
                      usage:(NSInteger)usage;

/*!
 * @method elementsMatching
 *
 * @abstract
 * Obtains HID elements that match the criteria contained in the matching
 * dictionary.
 *
 * @discussion
 * Matching keys are prefixed by kIOHIDElement and declared in
 * <IOKit/hid/IOHIDKeys.h>. Passing an empty dictionary will result in all device
 * elements being returned. Note that in order to get/set the element values,
 * the device must be opened.
 *
 * @param matching
 * The dictionary containing element matching criteria.
 *
 * @result
 * An array of the matching HIDElements.
 */
- (NSArray<HIDElement *> *)elementsMatching:(NSDictionary *)matching;

/*!
 * @method setReport
 *
 * @abstract
 * Sends a report synchronously to the device.
 *
 * @discussion
 * The HIDDevice must be open before calling this method. This will block
 * until the device has processed the call or the call fails.
 *
 * @param report
 * The report bytes to send.
 *
 * @param reportLength
 * The length of the report being sent.
 *
 * @param reportID
 * The report ID to send.
 *
 * @param reportType
 * The report type to send.
 *
 * @param outError
 * A reference to an NSError that will be filled with an error object on
 * failure. The reference will be unchanged on success.
 *
 * @result
 * true on success, false on failure.
 */
- (BOOL)setReport:(const void *)report
     reportLength:(NSInteger)reportLength
   withIdentifier:(NSInteger)reportID
          forType:(HIDReportType)reportType
            error:(out NSError * _Nullable * _Nullable)outError;

/*!
 * @method setReport
 *
 * @abstract
 * Sends a report asynchronously to the device.
 *
 * @discussion
 * The HIDDevice must be open and a dispatch queue must be set before calling
 * this method. If a callback is provided, this call will return once the request has been sent
 * to the device or the request fails. If true is returned, the callback will be triggered after the
 * device has processed the call or the timeout is hit. If false is returned,
 * the callback will not be triggered.
 *
 * @param report
 * A buffer containing the report bytes to set. If a callback is provided and the original call
 * to setReport does not fail, this buffer must not be freed until the callback is called.
 *
 * @param reportLength
 * The length of the report being sent.
 *
 * @param reportID
 * The report ID to send.
 *
 * @param reportType
 * The report type to send.
 *
 * @param outError
 * A reference to an NSError that will be filled with an error object on
 * failure. The reference will be unchanged on success.
 *
 * @param timeout
 * The maximum amount of time in milliseconds to wait for a device response before
 * the callback is triggered with an error.
 *
 * @param callback
 * The callback to invoke upon completion of an asynchronous call. If this is null
 * the call will run synchronously. If the initial call to setReport returns false, the
 * callback will not be invoked.
 *
 * @result
 * true on success, false on failure.
 */
- (BOOL)setReport:(const void *)report
     reportLength:(NSInteger)reportLength
   withIdentifier:(NSInteger)reportID
          forType:(HIDReportType)reportType
            error:(out NSError * _Nullable * _Nullable)outError
          timeout:(NSInteger)timeout
         callback:(HIDDeviceReportCallback _Nullable)callback;

/*!
 * @method getReport
 *
 * @abstract
 * Requests a report synchronously from the device.
 *
 * @discussion
 * The HIDDevice must be open before calling this method. This will block
 * until the device has processed the call or the call fails.
 *
 * @param report
 * A buffer to fill with the requested report bytes.
 *
 * @param reportLength
 * The length of the passed in buffer. The value will be updated to reflect
 * the length of the returned report.
 *
 * @param reportID
 * The report ID to request.
 *
 * @param reportType
 * The report type to request.
 *
 * @param outError
 * A reference to an NSError that will be filled with an error object on
 * failure. The reference will be unchanged on success.
 *
 * @result
 * true on success, false on failure.
 */
- (BOOL)getReport:(void *)report
     reportLength:(NSInteger *)reportLength
   withIdentifier:(NSInteger)reportID
          forType:(HIDReportType)reportType
            error:(out NSError * _Nullable * _Nullable)outError;

/*!
 * @method getReport
 *
 * @abstract
 * Retrieves a report asynchronously from the device.
 *
 * @discussion
 * The HIDDevice must be open and a dispatch queue must be set before calling
 * this method. If a callback is provided, this call will return once the request has been sent
 * to the device or the request fails. If true is returned, the callback will be triggered after the
 * device has processed the call or the timeout is hit. If false is returned,
 * the callback will not be triggered.
 *
 * @param report
 * A buffer to fill with the report bytes. If a callback is provided and the original call
 * to getReport does not fail, this buffer must not be freed until the callback is called.
 *
 * @param reportLength
 * The length of the report buffer. If a callback is not provided, the value will be updated
 * to reflect the length of the report. The pointer memory does not need to be maintained,
 * the original value will be copied.
 *
 * @param reportID
 * The report ID to request.
 *
 * @param reportType
 * The report type to request.
 *
 * @param outError
 * A reference to an NSError that will be filled with an error object on
 * failure. The reference will be unchanged on success.
 *
 * @param timeout
 * The maximum amount of time in milliseconds to wait for a device response before
 * the callback is triggered with an error.
 *
 * @param callback
 * The callback to invoke upon completion of an asynchronous call. If this is null
 * the call will run synchronously. If the initial call to getReport returns false, the
 * callback will not be invoked.
 *
 * @result
 * Returns YES on success.
 */
- (BOOL)getReport:(void *)report
     reportLength:(NSInteger *)reportLength
   withIdentifier:(NSInteger)reportID
          forType:(HIDReportType)reportType
            error:(out NSError * _Nullable * _Nullable)outError
          timeout:(NSInteger)timeout
         callback:(HIDDeviceReportCallback _Nullable)callback;

/*!
 * @method commitElements
 *
 * @abstract
 * Sets or gets an array of elements on the device.
 *
 * The HIDDevice must be open before calling this method.
 *
 * @param elements
 * An array of elements to commit to the device.
 *
 * @param direction
 * The direction of the commit.
 *
 * @discussion
 * Passing in HIDDeviceCommitDirectionIn will issue a getReport call to the
 * device, and the elements in the array will be updated with the value
 * retrieved by the device. The value can be accessed via the integerValue or
 * dataValue property on the HIDElement.
 *
 * Passing in HIDDeviceCommitDirectionOut will issue a setReport call to the
 * device. Before issuing this call, the desired value should be set on the
 * element by calling:
 *     element.integerValue = (value) or
 *     element.dataValue = (value)
 *
 * Input elements should not be used with HIDDeviceCommitDirectionOutput.
 *
 * @param outError
 * A reference to an NSError that will be filled with an error object on
 * failure. The reference will be unchanged on success.
 *
 * @result
 * true on success, false on failure.
 */
- (BOOL)commitElements:(NSArray<HIDElement *> *)elements
             direction:(HIDDeviceCommitDirection)direction
                 error:(out NSError * _Nullable * _Nullable)outError;

/*!
 * @method commitElements
 *
 * @abstract
 * Asynchronously sets or gets an array of elements on the device.
 *
 * @discussion
 * The HIDDevice must be open and a dispatch queue must be set before calling
 * this method. If a callback is provided, this call will return once the request has been sent
 * to the device or the request fails. If true is returned, the callback will be triggered after the
 * device has processed the call or the timeout is hit. If false is returned,
 * the callback will not be triggered.
 *
 * @param elements
 * An array of elements to commit to the device.
 *
 * @param direction
 * The direction of the commit.
 *
 * Passing in HIDDeviceCommitDirectionIn will issue a getReport call to the
 * device, and the elements in the array will be updated with the value
 * retrieved by the device. The value can be accessed via the integerValue or
 * dataValue property on the HIDElement.
 *
 * Passing in HIDDeviceCommitDirectionOut will issue a setReport call to the
 * device. Before issuing this call, the desired value should be set on the
 * element by calling:
 *     element.integerValue = (value) or
 *     element.dataValue = (value)
 *
 * Input elements should not be used with HIDDeviceCommitDirectionOutput.
 *
 * @param outError
 * A reference to an NSError that will be filled with an error object on
 * failure. The reference will be unchanged on success.
 *
 * @param timeout
 * If asynchronous, the time in milliseconds before the call will fail and the
 * callback invoked with a timeout error code.
 *
 * @param callback
 * The callback to invoke upon completion of an asynchronous call. If this is null
 * the call will run synchronously. If the initial call to commitElements returns false, the
 * callback will not be invoked.
 *
 * @result
 * true on success, false on failure.
 */
- (BOOL)commitElements:(NSArray<HIDElement *> *)elements
             direction:(HIDDeviceCommitDirection)direction
                 error:(out NSError * _Nullable * _Nullable)outError
               timeout:(NSInteger)timeout
              callback:(HIDDeviceCommitCallback _Nullable)callback;

/*!
 * @method setInputElementMatching
 *
 * @abstract
 * Sets matching criteria for element values received via setInputElementHandler
 * method.
 *
 * @discussion
 * Matching keys are prefixed by kIOHIDElement and declared in
 * <IOKit/hid/IOHIDKeys.h>. Passing an empty dictionary/array will result in all
 * elements being matched. If interested in multiple, specific device elements,
 * an NSArray of NSDictionaries may be passed in. This call must occur before
 * the device is activated.
 *
 * @param matching
 * An NSArray or NSDictionary containing matching criteria.
 */
- (void)setInputElementMatching:(id)matching;

/*!
 * @method setInputElementHandler
 *
 * @abstract
 * Registers a handler to be invoked when an updated element value is issued by the
 * device.
 *
 * @discussion
 * An input element refers to any element of type kHIDElementTypeInput and is
 * usually issued by interrupt driven reports.
 *
 * If more specific element values are desired, you may specify matching
 * criteria via the setInputElementMatching method.
 *
 * This call must occur before the device is activated. The device must be open
 * and activated in order to receive element updates.
 *
 * @param handler
 * The handler to receive input elements.
 */
- (void)setInputElementHandler:(HIDDeviceElementHandler)handler;

/*!
 * @method setBatchInputElementHandler
 *
 * @abstract
 * Registers a handler to be invoked when a set of updated element values is issued
 * by the device.
 *
 * @discussion
 * An input element refers to any element of type kHIDElementTypeInput and is
 * usually issued by interrupt driven reports.
 *
 * If more specific element values are desired, you may specify matching
 * criteria via the setInputElementMatching method.
 *
 * This handler groups elements together by timestamp, in an effort to dispatch
 * all updated elements on a per-report basis. This method must not be used
 * with the setInputElementHandler method, you must pick one.
 *
 * This call must occur before the device is activated. The device must be open
 * and activated in order to receive element updates.
 *
 * @param handler
 * The handler to receive input elements.
 */
- (void)setBatchInputElementHandler:(HIDDeviceBatchElementHandler)handler;

/*!
 * @method setRemovalHandler
 *
 * @abstract
 * Registers a handler to be invoked when the HIDDevice is removed.
 *
 * @discussion
 * This call must occur before the device is activated. The device must be
 * activated in order to receive removal notifications.
 *
 * @param handler
 * The handler to receive removal notifications.
 */
- (void)setRemovalHandler:(HIDBlock)handler;

/*!
 * @method setInputReportHandler
 *
 * @abstract
 * Registers a handler to be invoked when input reports are received from the device.
 *
 * @discussion
 * This call must occur before the device is activated. The device must be open
 * and activated in order to receive input reports. Note that internally, a
 * buffer is allocated based on the "MaxInputReportSize" property on the device,
 * so it is imperative that the device's report descriptor adheres to the HID
 * specification when defining input reports so that the buffer size may be allocated
 * correctly.
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
 * Sets the dispatch queue to be associated with the HIDDevice.
 *
 * @discussion
 * This is necessary in order to receive asynchronous events from the kernel.
 *
 * A call to setDispatchQueue should only be made once.
 *
 * If a dispatch queue is set but never used, a call to cancel followed by
 * activate should be performed in that order.
 *
 * After a dispatch queue is set, the HIDDevice must make a call to activate
 * via activate and cancel via cancel. All matching/handler method calls
 * should be made before activation and not after cancellation.
 *
 * @param queue
 * The dispatch queue to which the event handler block will be submitted.
 */
- (void)setDispatchQueue:(dispatch_queue_t)queue;

/*!
 * @method open
 *
 * @abstract
 * Opens the HID device for communication.
 *
 * It is recommended to use the openWithOptions method instead, to check
 * the return value.
 *
 * @discussion
 * Before the client can issue commands that change the state of the device, it
 * must have succeeded in opening the device. This establishes a link between
 * the client's task and the actual device.
 */
- (void)open;

/*!
 * @method open
 *
 * @abstract
 * Opens the HID device for communication.
 *
 * @discussion
 * Before the client can issue commands that change the state of the device, it
 * must have succeeded in opening the device. This establishes a link between
 * the client's task and the actual device.
 *
 * @param options
 * Options for the open request. See the documentation for HIDDeviceOptions
 * above.
 *
 * @param outError
 * A reference to an NSError that will be filled with an error object on
 * failure. The reference will be unchanged on success.
 * A value of kIOReturnExclusiveAccess indicates the device is currently
 * seized by another client.
 *
 * @return
 * true for success. If false with an error code of kIOReturnExclusiveAccess,
 * the open was successful but the device is temporarily seized. Communication
 * with the device will be blocked until the seizing client closes the device.
 */
- (BOOL)openWithOptions:(HIDDeviceOptions)options
       error:(out NSError * _Nullable * _Nullable)outError;

/*!
 * @method close
 *
 * @abstract
 * Closes the HID device for communication.
 *
 * @discussion
 * This closes the link between the client's task and the actual device.
 */
- (void)close;

/*!
 * @method activate
 *
 * @abstract
 * Activates the HIDDevice.
 *
 * @discussion
 * A HIDDevice associated with a dispatch queue is created in an inactive
 * state. The HIDDevice must be activated in order to receive asynchronous events
 * from the kernel.
 *
 * A dispatch queue must be set via setDispatchQueue before activation.
 *
 * An activated device must be cancelled via cancel. All matching/handler method
 * calls should be made before activation and not after cancellation.
 *
 * Calling activate on an active HIDDevice has no effect.
 */
- (void)activate;

/*!
 * @method cancel
 *
 * @abstract
 * Cancels the HIDDevice, preventing any further invocation of its event handler
 * block.
 *
 * @discussion
 * Cancelling prevents any further invocation of the event handler block for the
 * specified dispatch queue, but does not interrupt an event handler block that
 * is already in progress.
 *
 * Explicit cancellation of the HIDDevice is required, no implicit cancellation
 * takes place.
 *
 * Calling cancel on an already cancelled device has no effect.
 */
- (void)cancel;

/*!
 * @property service
 *
 * @abstract
 * The IOService associated with the HIDDevice.
 *
 * @discussion
 * This is a reference to an IOHIDDevice in the kernel.
 */
@property (readonly) io_service_t service;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDDevice_h */
