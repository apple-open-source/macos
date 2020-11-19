//
//  HIDDisplayCAPI.h
//  IOHIDFamily
//
//  Created by AB on 1/10/19.
//

#ifndef HIDDisplayCAPI_h
#define HIDDisplayCAPI_h


#import <HIDDisplay/HIDDisplayPresetCAPI.h>
#import <HIDDisplay/HIDDisplayIOReportingCAPI.h>
#import <HIDDisplay/HIDDisplayDeviceManagementCAPI.h>
#import <HIDDisplay/HIDDisplayUserAdjustmentCAPI.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN

/*!
 * HIDDisplayGetContainerID
 *
 * @abstract
 * Get ContainerID for hid display device
 *
 * @discussion
 * get container id published on device interface or any of it's parent
 *
 * @result
 * CFStringRef for containerID. Caller shouldn't release any returned CFString
 */
CFStringRef __nullable HIDDisplayGetContainerID(CFTypeRef __nonnull hidDisplayInterface);

CF_ASSUME_NONNULL_END

__END_DECLS

#endif /* HIDDisplayCAPI_h */
