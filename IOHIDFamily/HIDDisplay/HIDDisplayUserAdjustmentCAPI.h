//
//  HIDDisplayUserAdjustmentCAPI.h
//  IOHIDFamily
//
//  Created by abhishek on 1/15/20.
//

#ifndef HIDDisplayUserAdjustmentCAPI_h
#define HIDDisplayUserAdjustmentCAPI_h


#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/IOKitLib.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN

//user adjustment Fields
// *********** Note ***********
// All float / double values are represented as IOFixed, application
// should make neccessary conversion before getting / setting value to HID Interface
//****************************
/*!
 @defined kHIDDisplayUserAdjustmentDescriptionKey
 @abstract user adjustment description.
 @discussion expected value type CFStringRef.
 */
extern CFStringRef kHIDDisplayUserAdjustmentDescriptionKey;

/*!
 @defined kHIDDisplayUserAdjustmentInformationKey
 @abstract user adjustment information
 @discussion expected value type CFDataRef.
 */
extern CFStringRef kHIDDisplayUserAdjustmentInformationKey;

typedef CFTypeRef HIDDisplayUserAdjustmentInterfaceRef;

/*!
 * HIDDisplayCreateUserAdjustmentInterfaceWithContainerID
 *
 * @abstract
 * Creates hidDisplayInterface ref with container ID
 *
 * @discussion
 * Create hidDisplayInterface object backed by actual hid device matching
 * given container ID. If no corresponding
 * HID device matches container ID  , this will return NULL.
 * Caller should create only  single instance of HIDDisplayCreateUserAdjustmentInterfaceWithContainerID  per system for all
 * HIDDisplayCreateUserAdjustmentInterfaceWithContainerID APIs as these APIs are not thread safe.  This api shouldn't be used
 * in case underlying service is expected to terminate / add while api is in process. Calling this for terminated or in progress
 * termination service containerID may have undesirable results. Use HIDDisplayCreateUserAdjustmentInterfaceWithService instead.
 *
 * @param containerID
 * Attributes which can uniquely identify display device.
 *
 * @result
 * Returns an instance of a hidDisplayInterface object on success which has to be released by caller.
 */
HIDDisplayUserAdjustmentInterfaceRef __nullable HIDDisplayCreateUserAdjustmentInterfaceWithContainerID(CFStringRef containerID);

/*!
 * HIDDisplayCreateUserAdjustmentInterfaceWithService
 *
 * @abstract
 * Creates hidDisplayInterface ref with  io service object for hid preset device
 *
 * @discussion
 * Create hidDisplayInterface object  for given hid device
 * Caller is expected to create only single instance of HIDDisplayCreateUserAdjustmentInterfaceWithService per system for all HIDDisplayPreset APIs
 * as these APIs are not thread safe.
 *
 * @param service
 * IOKit object for HID preset device.
 *
 * @result
 * Returns an instance of a hidDisplayInterface object on success which has to be released by caller.
 */
HIDDisplayUserAdjustmentInterfaceRef __nullable HIDDisplayCreateUserAdjustmentInterfaceWithService(io_service_t service);

/*!
* HIDDisplayUserAdjustmentSetData
*
* @abstract
* Set user adjustment data for keys defined above.
*
* @discussion
* set user adjustment .
*
* @param interface
* interface object returned from create api.
*
* @param data
* data to set
*
* @param error
* field which will be filled with  error code on failure.
*
* @result
* Returns true for success. Additionally, if an error occurs and the error parameter is non-NULL, the error parameter will be set to a CFError describing the problem,
*  which the caller must release.
*
*/
bool HIDDisplayUserAdjustmentSetData(HIDDisplayUserAdjustmentInterfaceRef interface, CFDictionaryRef data, CFErrorRef* error);

/*!
* HIDDisplayUserAdjustmentCopyData
*
* @abstract
* Get user adjustment data for keys defined above.
*
* @param interface
* interface object returned from create api.
*
* @discussion
* get user adjustment . Caller is responsible for releasing data if call is successfull.
*
* @param error
* field which will be filled with  error code on failure.
*
* @result
* Returns true for success. Additionally, if an error occurs and the error parameter is non-NULL, the error parameter will be set to a CFError describing the problem,
*  which the caller must release.
*
*/
CFDictionaryRef _Nullable HIDDisplayUserAdjustmentCopyData(HIDDisplayUserAdjustmentInterfaceRef interface, CFErrorRef* error);


/*!
* HIDDisplayUserAdjustmentIsValid
*
* @abstract
* Check if current user adjustment is valid or not.
*
* @param interface
* interface object returned from create api.
*
* @result
* Returns true for valid.
*
*/
bool HIDDisplayUserAdjustmentIsValid(HIDDisplayUserAdjustmentInterfaceRef interface);

/*!
* HIDDisplayUserAdjustmentInvalidate
*
* @abstract
* Invalidate current user adjustment.
*
* @param interface
* interface object returned from create api.
*
* @param error
* field which will be filled with  error code on failure.
*
* @result
* Returns true if success. Additionally, if an error occurs and the error parameter is non-NULL, the error parameter will be set to a CFError describing the problem,
*  which the caller must release.
*
*/
bool HIDDisplayUserAdjustmentInvalidate(HIDDisplayUserAdjustmentInterfaceRef interface, CFErrorRef* error);


CF_ASSUME_NONNULL_END

__END_DECLS

#endif /* HIDDisplayUserAdjustmentCAPI_h */
