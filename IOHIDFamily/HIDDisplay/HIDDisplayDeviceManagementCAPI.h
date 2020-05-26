//
//  HIDDisplayDeviceManagementCAPI.h
//  IOHIDFamily
//
//  Created by abhishek on 1/6/20.
//

#ifndef HIDDisplayDeviceManagementCAPI_h
#define HIDDisplayDeviceManagementCAPI_h


#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/IOKitLib.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN

typedef enum {
    kHIDDisplayFactoryResetSystem = 0x01,
    kHIDDisplayFactoryResetPresets = 0x02,
    kHIDDisplayFactoryResetUserAdjustments = 0x04,
    kHIDDisplayFactoryResetAll = 0xFF,
} HIDDisplayFactoryResetType __attribute__ ((deprecated));

typedef CFTypeRef HIDDisplayDeviceManagementInterfaceRef;

/*!
 * HIDDisplayCreateDeviceManagementInterfaceWithContainerID
 *
 * @abstract
 * Creates hidDisplayInterface ref with container ID
 *
 * @discussion
 * Create hidDisplayInterface object backed by actual hid device matching
 * given container ID. If no corresponding
 * HID device matches container ID  , this will return NULL.
 * Caller should create only  single instance of HIDDisplayDeviceManagementInterfaceRef  per system for all HIDDisplayDeviceManagementInterface APIs
 * as these APIs are not thread safe.
 *
 * @param containerID
 * Attributes which can uniquely identify display device.
 *
 * @result
 * Returns an instance of a hidDisplayInterface object on success which has to be released by caller.
 */
HIDDisplayDeviceManagementInterfaceRef __nullable HIDDisplayCreateDeviceManagementInterfaceWithContainerID(CFStringRef containerID);

/*!
 * HIDDisplayCreateDeviceManagementInterfaceWithService
 *
 * @abstract
 * Creates hidDisplayInterface ref with  io service object for hid preset device
 *
 * @discussion
 * Create hidDisplayInterface object  for given hid device
 * Caller is expected to create only single instance of HIDDisplayDeviceManagementInterface per system for all HIDDisplayPreset APIs
 * as these APIs are not thread safe.
 *
 * @param service
 * IOKit object for HID preset device.
 *
 * @result
 * Returns an instance of a hidDisplayInterface object on success which has to be released by caller.
 */
HIDDisplayDeviceManagementInterfaceRef __nullable HIDDisplayCreateDeviceManagementInterfaceWithService(io_service_t service);

/*!
* HIDDisplayDeviceManagementFactoryReset
*
* @abstract
* factory reset.
*
* @discussion
* Send factory reset command to device. This api is deprecated , use HIDDisplayDeviceManagementSetFactoryResetData instead.
*
* @param interface
* interface object returned from create api.
*
* @param type
* reset value.
*
* @param securityToken
*  Secure key to uniquely identify display.
*
* @param error
* field which will be filled with  error code on failure.
*
* @result
* Returns true for success. Additionally, if an error occurs and the error parameter is non-NULL, the error parameter will be set to a CFError describing the problem,
*  which the caller must release.
*
*/
__attribute__ ((deprecated)) bool HIDDisplayDeviceManagementFactoryReset(HIDDisplayDeviceManagementInterfaceRef interface, HIDDisplayFactoryResetType type, uint64_t securityToken, CFErrorRef *error);

/*!
* HIDDisplayDeviceManagementSetFactoryResetData
*
* @abstract
* factory reset.
*
* @discussion
* Send factory reset command to device.
*
* @param interface
* interface object returned from create api.
*
* @param data
* reset data.
*
* @param securityToken
*  Secure key to uniquely identify display.
*
* @param error
* field which will be filled with  error code on failure.
*
* @result
* Returns true for success. Additionally, if an error occurs and the error parameter is non-NULL, the error parameter will be set to a CFError describing the problem,
*  which the caller must release.
*
*/
bool HIDDisplayDeviceManagementSetFactoryResetData(HIDDisplayDeviceManagementInterfaceRef interface, uint8_t data, uint64_t securityToken, CFErrorRef *error);

/*!
* HIDDisplayDeviceManagementGetSecurityToken
*
* @abstract
* Get device management security token.
*
* @discussion
* Get Security token from device. This will be used to create security key based on agrement of application and device.
*
* @param interface
* interface object returned from create api.
*
* @param securityToken
* value will be filled with security token from device if call is successful.
*
* @param error
* field which will be filled with  error code on failure.
*
* @result
* Returns true for success.Additionally, if an error occurs and the error parameter is non-NULL, the error parameter will be set to a CFError describing the problem, which the caller must release.
*
*/
bool HIDDisplayDeviceManagementGetSecurityToken(HIDDisplayDeviceManagementInterfaceRef interface, uint64_t* securityToken, CFErrorRef* error);


CF_ASSUME_NONNULL_END

__END_DECLS
#endif /* HIDDisplayDeviceManagementCAPI_h */
