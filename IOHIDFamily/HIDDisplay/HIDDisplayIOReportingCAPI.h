//
//  HIDDisplayIOReportingCAPI.h
//  IOHIDFamily
//
//  Created by AB on 4/22/19.
//

#ifndef HIDDisplayIOReportingCAPI_h
#define HIDDisplayIOReportingCAPI_h

#import <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN


typedef CFTypeRef HIDDisplayIOReportingInterfaceRef;

/*!
 * @param inputData
 * input data. Application must create copy if they need to use data after callback.
 *
 */
typedef void (^IOReportingInputDataHandler)(CFDataRef inputData);

/*!
 * HIDDisplayCreateIOReportingInterfaceWithContainerID
 *
 * @abstract
 * Creates hidDisplayInterface ref with container ID
 *
 * @discussion
 * Create hidDisplayInterface object backed by actual hid device matching
 * given container ID. If no corresponding
 * HID device matches container ID  , this will return NULL.
 * Caller should create only  single instance of HIDDisplayIOReportingInterface  per system for all HIDDisplayIOReporting APIs
 * as these APIs are not thread safe.
 *
 * @param containerID
 * Attributes which can uniquely identify display device.
 *
 * @result
 * Returns an instance of a hidDisplayInterface object on success which has to be released by caller.
 */
HIDDisplayIOReportingInterfaceRef __nullable HIDDisplayCreateIOReportingInterfaceWithContainerID(CFStringRef containerID);

/*!
 * HIDDisplayIOReportingSetDispatchQueue
 *
 * @abstract
 * Set dispatch queue for input data.
 *
 * @discussion
 * All asynchronous communication from device like input reports would be posted on given dispatch queue.
 *
 * @param hidDisplayInterface
 * hidDisplayInterface object returned from HIDDisplayCreateIOReportingInterfaceWithContainerID.
 *
 * @param queue
 * dispatch queue to recieve input data.
 *
 */
void HIDDisplayIOReportingSetDispatchQueue(HIDDisplayIOReportingInterfaceRef hidDisplayInterface, dispatch_queue_t queue);

/*!
 * HIDDisplayIOReportingSetInputDataHandler
 *
 * @abstract
 * Set Data handler which will be called for input reports from device.
 *
 * @discussion
 * Device can communicate with host through input report. Handler set by caller
 * would be called for input reports on dispatch queue set in HIDDisplayIOReportingSetDispatchQueue.
 *
 * @param hidDisplayInterface
 * hidDisplayInterface object returned from HIDDisplayCreateIOReportingInterfaceWithContainerID.
 *
 * @param handler
 * handler which is expecte to be called for input report.
 *
 */
void HIDDisplayIOReportingSetInputDataHandler(HIDDisplayIOReportingInterfaceRef hidDisplayInterface, IOReportingInputDataHandler handler);

/*!
 * HIDDisplayIOReportingSetCancelHandler
 *
 * @abstract
 * Set cancel handler for object.
 *
 * @discussion
 * Cancel handler would be called on dispatch queue once object is  cancelled.
 * Reference to object must be released after cancel handler has been called.
 
 * @param hidDisplayInterface
 * hidDisplayInterface object returned from HIDDisplayCreateIOReportingInterfaceWithContainerID.
 *
 *
 */
void HIDDisplayIOReportingSetCancelHandler(HIDDisplayIOReportingInterfaceRef hidDisplayInterface, dispatch_block_t handler);

/*!
 * HIDDisplayIOReportingActivate
 *
 * @abstract
 * Activate code path to recieve input reports on set handler.
 *
 * @discussion
 * Caller should activate interface before he can expect any input reports from it.
 *
 * Expected call path for setup
 * 1.HIDDisplayCreateIOReportingInterfaceWithContainerID
 * 2.HIDDisplayIOReportingSetInputDataHandler
 * 3.HIDDisplayIOReportingSetDispatchQueue
 * 4.HIDDisplayIOReportingActivate
 *
 * Expected call path for teardown
 * 1. HIDDisplayIOReportingCancel
 * 2. CFRelease(InterfaceObject)
 *
 * If caller is only interested in setting output reports and don't care about input data
 * then he can skip 2,3,4 steps from setup and 1 step from teardown
 *
 * @param hidDisplayInterface
 * hidDisplayInterface object returned from HIDDisplayCreateIOReportingInterfaceWithContainerID.
 *
 *
 */
void HIDDisplayIOReportingActivate(HIDDisplayIOReportingInterfaceRef hidDisplayInterface);

/*!
 * HIDDisplayIOReportingCancel
 *
 * @abstract
 * Teardown code path for input report handling set in activation.
 *
 * @discussion
 * Caller should call cancellation of object if activated before release.
 * Activation without cancel will cause assert.
 *
 * @param hidDisplayInterface
 * hidDisplayInterface object returned from HIDDisplayCreateIOReportingInterfaceWithContainerID.
 *
 */
void HIDDisplayIOReportingCancel(HIDDisplayIOReportingInterfaceRef hidDisplayInterface);

/*!
 * HIDDisplayIOReportingSetOutputData
 *
 * @abstract
 * Send data to device.
 *
 * @discussion
 * Send data to device. Data will be dispatched to device in form of output report.
 *
 * @param hidDisplayInterface
 * hidDisplayInterface object returned from HIDDisplayCreateIOReportingInterfaceWithContainerID.
 *
 * @param data
 * data to send.
 *
 * @param error
 * field which will be filled with  error code on failure.
 *
 * @result
 * Returns true if data send is successful.Additionally, if an error occurs and the error parameter is non-NULL, the error parameter will be set to a CFError describing the problem, which the caller must release.
 *
 */
bool HIDDisplayIOReportingSetOutputData(HIDDisplayIOReportingInterfaceRef hidDisplayInterface, CFDataRef data, CFErrorRef* error);

CF_ASSUME_NONNULL_END

__END_DECLS


#endif /* HIDDisplayIOReportingCAPI_h */
