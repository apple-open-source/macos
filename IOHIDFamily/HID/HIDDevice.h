//
//  HIDDevice.h
//  HID
//
//  Created by dekom on 10/9/17.
//

#ifndef HIDDevice_h
#define HIDDevice_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>
#import <IOKit/hidobjc/HIDDeviceBase.h>

NS_ASSUME_NONNULL_BEGIN

@class HIDElement;

/*!
 * @typedef HIDDeviceCommitDirection
 *
 * @abstract
 * Commit direction passed in to commitElements method.
 */
typedef NS_ENUM(NSInteger, HIDDeviceCommitDirection) {
    HIDDeviceCommitDirectionIn,
    HIDDeviceCommitDirectionOut,
};

/*!
 * @typedef HIDDeviceElementHandler
 *
 * @abstract
 * The type block used for input element updates.
 */
typedef void (^HIDDeviceElementHandler)(HIDElement *element);

typedef void (^HIDDeviceBatchElementHandler)(NSArray<HIDElement *> *elements);

@interface HIDDevice (HIDFramework)

- (instancetype)init NS_UNAVAILABLE;

/*!
 * @method initWithService
 *
 * @abstract
 * Creates a HIDDevice object for the specified IOService object.
 *
 * @discussion
 * The io_service_t passed in this method must reference an object in the kernel
 * of type IOHIDDevice.
 *
 * @param service
 * The IOService object associated with the IOHIDDevice.
 *
 * @result
 * Returns an instance of a HIDDevice object on success.
 */
- (nullable instancetype)initWithService:(io_service_t)service;

/*!
 * @method propertyForKey
 *
 * @abstract
 * Obtains a property from a HIDDevice.
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
 * Sets a property on a HIDDevice.
 *
 * @param value
 * The value of the property
 *
 * @param key
 * The property key.
 *
 * @result
 * Returns true on success.
 */
- (BOOL)setProperty:(nullable id)value forKey:(NSString *)key;

/*!
 * @method conformsToUsagePage
 *
 * @abstract
 * Convenience function that scans the application collection elements to see if
 * the device conforms to the provided usage page and usage.
 *
 * @param usagePage
 * The device usage page.
 *
 * @param usage
 * The device usage.
 *
 * @result
 * Returns true if the device conforms to the provided usages.
 */
- (BOOL)conformsToUsagePage:(NSInteger)usagePage usage:(NSInteger)usage;

/*!
 * @method elementsMatching
 *
 * @abstract
 * Obtains HID elements that match the criteria contained in the matching
 * dictionary.
 *
 * @discussion Matching keys are prefixed by kIOHIDElement and declared in
 * <IOKit/hid/IOHIDKeys.h>. Passing a nil dictionary will result in all device
 * elements being returned. Note that in order to get/set the element values,
 * the device must be opened.
 *
 * @param matching
 * The dictionary containg element matching criteria.
 *
 * @result
 * Returns an array of matching HIDElement objects.
 */
- (NSArray<HIDElement *> *)elementsMatching:(NSDictionary *)matching;

/*!
 * @method setReport
 *
 * @abstract
 * Sends a report to the device.
 *
 * @discussion
 * The HIDDevice must be open before calling this method.
 *
 * @param report
 * The report bytes.
 *
 * @param reportLength
 * The length of the report being passed in.
 *
 * @param reportID
 * The report ID.
 *
 * @param reportType
 * The report type.
 *
 * @param outError
 * An error returned on failure.
 *
 * @result
 * Returns YES on success.
 */
- (BOOL)setReport:(const void *)report
     reportLength:(NSInteger)reportLength
   withIdentifier:(NSInteger)reportID
          forType:(HIDReportType)reportType
            error:(out NSError * _Nullable * _Nullable)outError;

/*!
 * @method getReport
 *
 * @abstract
 * Retrieves a report from the device.
 *
 * @discussion
 * The HIDDevice must be open before calling this method.
 *
 * @param report
 * A buffer to fill with the report bytes.
 *
 * @param reportLength
 * The length of the passed in buffer. The value will be updated to reflect
 * the length of the returned report.
 *
 * @param reportID
 * The report ID.
 *
 * @param reportType
 * The report type.
 *
 * @param outError
 * An error returned on failure.
 *
 * @result
 * Returns YES on success.
 */
- (BOOL)getReport:(void *)report
     reportLength:(NSInteger *)reportLength
   withIdentifier:(NSInteger)reportID
          forType:(HIDReportType)reportType
            error:(out NSError * _Nullable * _Nullable)outError;

/*!
 * @method commitElements
 *
 * @abstract
 * Sets/Gets the array of elements on the device.
 *
 * @discussion
 * The HIDDevice must be open before calling this method.
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
 * An error returned on failure.
 *
 * @result
 * Returns YES on success.
 */
- (BOOL)commitElements:(NSArray<HIDElement *> *)elements
             direction:(HIDDeviceCommitDirection)direction
                 error:(out NSError * _Nullable * _Nullable)outError;

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
 * Registers a handler to be used when an updated element value is issued by the
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
 * Registers a handler to be used when a set of updated element values is issued
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
 * Registers a handler to be used when the HIDDevice is removed.
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
 * Registers a handler to be recieve input reports from the device.
 *
 * @discussion
 * This call must occur before the device is activated. The device must be open
 * and activated in order to receive input reports. Note that internally, a
 * buffer is allocated based on the "MaxInputReportSize" property on the device,
 * so it is imperative that the device's report descriptor adheres to the HID
 * spec when defining input reports so that the buffer size may be allocated
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
 * This is necessary in order to receive asynchronous events from the kernel.
 *
 * @discussion
 * A call to setDispatchQueue should only be made once.
 *
 * If a dispatch queue is set but never used, a call to cancel followed by
 * activate should be performed in that order.
 *
 * After a dispatch queue is set, the HIDDevice must make a call to activate
 * via activate and cancel via cancel. All matching/handler method calls
 * should be done before activation and not after cancellation.
 *
 * @param queue
 * The dispatch queue to which the event handler block will be submitted.
 */
- (void)setDispatchQueue:(dispatch_queue_t)queue;

/*!
 * @method open
 *
 * @abstract
 * Opens a HID device for communication.
 *
 * @discussion
 * Before the client can issue commands that change the state of the device, it
 * must have succeeded in opening the device. This establishes a link between
 * the client's task and the actual device.
 */
- (void)open;

/*!
 * @method close
 *
 * @abstract
 * Closes communication with the HIDDevice.
 *
 * @discussion
 * This closes a link between the client's task and the actual device.
 */
- (void)close;

/*!
 * @method activate
 *
 * @abstract
 * Activates the HIDDevice object.
 *
 * @discussion
 * A HIDDevice object associated with a dispatch queue is created in an inactive
 * state. The object must be activated in order to receive asynchronous events
 * from the kernel.
 *
 * A dispatch queue must be set via setDispatchQueue before activation.
 *
 * An activated device must be cancelled via cancel. All matching/handler method
 * calls should be done before activation and not after cancellation.
 *
 * Calling activate on an active HIDDevice has no effect.
 */
- (void)activate;

/*!
 * @method cancel
 *
 * @abstract
 * Cancels the HIDDevice preventing any further invocation of its event handler
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
 * The IOService object associated with the HIDDevice.
 */
@property (readonly) io_service_t service;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDDevice_h */
