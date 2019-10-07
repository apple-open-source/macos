//
//  HIDDisplayPresetCAPI.h
//  IOHIDFamily
//
//  Created by AB on 4/22/19.
//

#ifndef HIDDisplayPresetCAPI_h
#define HIDDisplayPresetCAPI_h

#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/IOKitLib.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN

//preset Fields
/*!
 @defined kHIDDisplayPresetFieldWritableKey
 @abstract preset field to check if data is writable.
 @discussion Check if preset data is writable.Return CFBooleanRef with true as writable.
 */
extern CFStringRef kHIDDisplayPresetFieldWritableKey;

/*!
 @defined kHIDDisplayPresetFieldValidKey
 @abstract preset field to check if it can be set as active preset.
 @discussion Check if preset can be set as active preset .Return CFBooleanRef with true as valid.
 */
extern CFStringRef kHIDDisplayPresetFieldValidKey;

/*!
 @defined kHIDDisplayPresetFieldNameKey
 @abstract preset field to query name.
 @discussion get current preset name. Return CFStringRef for preset name.
 */
extern CFStringRef kHIDDisplayPresetFieldNameKey;

/*!
 @defined kHIDDisplayPresetFieldDescriptionKey
 @abstract preset field to query preset description.
 @discussion get current preset description. Return CFStringRef for preset description.
 */
extern CFStringRef kHIDDisplayPresetFieldDescriptionKey;

/*!
 @defined kHIDDisplayPresetFieldDataBlockOneLengthKey
 @abstract preset field to query preset data block one length.
 @discussion get current preset data block one length. Return CFNumberRef for preset data length.
 */
extern CFStringRef kHIDDisplayPresetFieldDataBlockOneLengthKey;

/*!
 @defined kHIDDisplayPresetFieldDataBlockOneKey
 @abstract preset field to query preset data block one.
 @discussion get current preset data block one. Return CFDataRef for preset data.
 */
extern CFStringRef kHIDDisplayPresetFieldDataBlockOneKey;

/*!
 @defined kHIDDisplayPresetFieldDataBlockTwoLengthKey
 @abstract preset field to query preset data block two length.
 @discussion get current preset data block two length. Return CFNumberRef for preset data length.
 */
extern CFStringRef kHIDDisplayPresetFieldDataBlockTwoLengthKey;

/*!
 @defined kHIDDisplayPresetFieldDataBlockTwoKey
 @abstract preset field to query preset data block two.
 @discussion get current preset data block two. Return CFDataRef for preset data.
 */
extern CFStringRef kHIDDisplayPresetFieldDataBlockTwoKey;


/*!
 @defined kHIDDisplayPresetUniqueIDKey
 @abstract preset field to query preset uniqueID.
 @discussion get current preset uniqueID. Return CFStringRef for preset uniqueID.
 */
extern CFStringRef kHIDDisplayPresetUniqueIDKey;


typedef CFTypeRef HIDDisplayPresetInterfaceRef;

/*** NEED TO REMOVE */
typedef HIDDisplayPresetInterfaceRef HIDDisplayDeviceRef;

HIDDisplayDeviceRef __nullable HIDDisplayCreateDeviceWithContainerID(CFStringRef containerID);

/*** END - NEED TO REMOVE */



/*!
 * HIDDisplayCreatePresetInterfaceWithContainerID
 *
 * @abstract
 * Creates hidDisplayInterface ref with container ID
 *
 * @discussion
 * Create hidDisplayInterface object backed by actual hid device matching
 * given container ID. If no corresponding
 * HID device matches container ID  , this will return NULL.
 * Caller is expected to create only single instance of HIDDisplayPresetIntereface per system for all HIDDisplayPreset APIs
 * as these APIs are not thread safe.
 *
 * @param containerID
 * Attributes which can uniquely identify display device.
 *
 * @result
 * Returns an instance of a hidDisplayInterface object on success which has to be released by caller.
 */
HIDDisplayPresetInterfaceRef __nullable HIDDisplayCreatePresetInterfaceWithContainerID(CFStringRef containerID);


/*!
 * HIDDisplayCreatePresetInterfaceWithService
 *
 * @abstract
 * Creates hidDisplayInterface ref with  io service object for hid preset device
 *
 * @discussion
 * Create hidDisplayInterface object  for given hid device
 * Caller is expected to create only single instance of HIDDisplayPresetIntereface per system for all HIDDisplayPreset APIs
 * as these APIs are not thread safe.
 *
 * @param service
 * IOKit object for HID preset device.
 *
 * @result
 * Returns an instance of a hidDisplayInterface object on success which has to be released by caller.
 */
HIDDisplayPresetInterfaceRef __nullable HIDDisplayCreatePresetInterfaceWithService(io_service_t service);


/*!
 * HIDDisplayGetContainerID
 *
 * @abstract
 * Get ContainerID for hid preset device
 *
 * @discussion
 * get container id published on device interface or any of it's parent
 *
 * @result
 * CFStringRef for containerID. Caller shouldn't release any returned CFString
 */
CFStringRef __nullable HIDDisplayGetContainerID(HIDDisplayPresetInterfaceRef hidDisplayInterface);

/*!
 * HIDDisplayGetPresetCount
 *
 * @abstract
 * Get preset count for given display. This is readonly property
 *
 * @discussion
 * Get preset count for given hidDisplayInterface object
 *
 * @param hidDisplayInterface
 * hidDisplayInterface object returned from HIDDisplayCreatePresetInterfaceWithContainerID
 *
 * @result
 * Returns total number of display preset supported by device.
 */
CFIndex HIDDisplayGetPresetCount(HIDDisplayPresetInterfaceRef hidDisplayInterface);

/*!
 * HIDDisplayGetFactoryDefaultPresetIndex
 *
 * @abstract
 * Get factory default preset index set for display
 *
 * @discussion
 * Get factory default preset index for given hidDisplayInterface object. This is readonly property
 *
 * @param hidDisplayInterface
 * hidDisplayInterface object returned from HIDDisplayCreatePresetInterfaceWithContainerID
 *
 * @param error
 * field which will be filled with  error code on failure.
 *
 * @result
 * Returns factory default preset index of display device on success and -1 on failure.
 */
CFIndex HIDDisplayGetFactoryDefaultPresetIndex(HIDDisplayPresetInterfaceRef hidDisplayInterface, CFErrorRef* error);

/*!
 * HIDDisplayGetPresetCapabilities
 *
 * @abstract
 * Get capabilites supported by preset.
 *
 * @discussion
 * Get CFStringRef array of preset capabilites supported by given presets supported by device.
 * Keys returned from this API call is subset of keys returned from HIDDisplayCopyPreset
 *
 * @param hidDisplayInterface
 * hidDisplayInterface object returned from HIDDisplayCreatePresetInterfaceWithContainerID
 *
 * @result
 * Returns array of CFStringRef on success.
 */
CFArrayRef __nullable HIDDisplayGetPresetCapabilities(HIDDisplayPresetInterfaceRef hidDisplayInterface);

/*!
 * HIDDisplayGetActivePresetIndex
 *
 * @abstract
 * Get active preset index for display
 *
 * @discussion
 * Get active preset index for given hidDisplayInterface object.
 *
 * @param hidDisplayInterface
 * hidDisplayInterface object returned from HIDDisplayCreatePresetInterfaceWithContainerID
 *
 * @param error
 * field which will be filled with  error code on failure.
 *
 * @result
 * Returns active preset index of display device on success and -1 on failure.
 */
CFIndex HIDDisplayGetActivePresetIndex(HIDDisplayPresetInterfaceRef hidDisplayInterface, CFErrorRef* error);

/*!
 * HIDDisplaySetActivePresetIndex
 *
 * @abstract
 * Set active preset index for display
 *
 * @discussion
 * Set active preset index for given hidDisplayInterface object.
 *
 * @param hidDisplayInterface
 * hidDisplayInterface object returned from HIDDisplayCreatePresetInterfaceWithContainerID
 *
 * @param presetIndex
 * preset index which you want to set as active index
 *
 * @param error
 * field which will be filled with  error code on failure.
 *
 * @result
 * Returns true on success.
 */
bool HIDDisplaySetActivePresetIndex(HIDDisplayPresetInterfaceRef hidDisplayInterface, CFIndex presetIndex, CFErrorRef* error);

/*!
 * HIDDisplayCopyPreset
 *
 * @abstract
 * Copy info for given preset index.
 *
 * @discussion
 * Copy info for given preset index. Index has to be range [0 - (presetCount-1)].
 * Caller is responsible for releasing return object if nonnull
 * Caller should use  single instance of HIDDisplayPresetIntereface per system as these APIs are
 * not thread safe.
 *
 * @param hidDisplayInterface
 * hidDisplayInterface object returned from HIDDisplayCreatePresetInterfaceWithContainerID
 *
 * @param presetIndex
 * preset index for which you want to copy property
 *
 * @param error
 * field which will be filled with  error code on failure.
 *
 * @result
 * Returns preset info on success. preset info is dictionary containing preset fields and value
 */
CFDictionaryRef __nullable HIDDisplayCopyPreset(HIDDisplayPresetInterfaceRef hidDisplayInterface, CFIndex presetIndex, CFErrorRef *error);

/*!
 * HIDDisplaySetPreset
 *
 * @abstract
 * Set info for given preset index.
 *
 * @discussion
 * Set info for given preset index. Index has to be range [0 - (presetCount-1)].
 * Returns false if info is readonly.
 * Caller should use  single instance of HIDDisplayPresetIntereface per system as these APIs are
 * not thread safe.
 *
 * @param hidDisplayInterface
 * hidDisplayInterface object returned from HIDDisplayCreatePresetInterfaceWithContainerID
 *
 * @param presetIndex
 * preset index for which you want to set property
 *
 * @param info
 * Dictionary of preset fields and values
 *
 * @param error
 * field which will be filled with  error code on failure.
 *
 * @result
 * Returns true on success.
 */
bool HIDDisplaySetPreset(HIDDisplayPresetInterfaceRef hidDisplayInterface, CFIndex presetIndex, CFDictionaryRef info, CFErrorRef *error);

/*!
 * HIDDisplayIsPresetValid
 *
 * @abstract
 * Check if given preset is valid.
 *
 * @discussion
 * Get preset valid info. Index has to be range [0 - (presetCount-1)].
 * This imformation can be used to determine if given preset can be set as active preset.
 * Caller should use  single instance of HIDDisplayPresetIntereface per system as these APIs are
 * not thread safe.
 *
 * @param hidDisplayInterface
 * hidDisplayInterface object returned from HIDDisplayCreatePresetInterfaceWithContainerID
 *
 * @param presetIndex
 * preset index for which you want to set property
 *
 *
 * @result
 * Returns true if preset is valid.
 */
bool HIDDisplayIsPresetValid(HIDDisplayPresetInterfaceRef hidDisplayInterface, CFIndex presetIndex);

/*!
 * HIDDisplayIsPresetWritable
 *
 * @abstract
 * Check if given preset is writable.
 *
 * @discussion
 * Get preset writable info. Index has to be range [0 - (presetCount-1)].
 * This imformation can be used to determine if given preset support data write.
 * If preset is not writable , call to HIDDisplaySetPreset will fail with unsupported return
 * Caller should use  single instance of HIDDisplayPresetIntereface per system as these APIs are
 * not thread safe.
 *
 * @param hidDisplayInterface
 * hidDisplayInterface object returned from HIDDisplayCreatePresetInterfaceWithContainerID
 *
 * @param presetIndex
 * preset index for which you want to set property
 *
 * @result
 * Returns true if preset is writable.
 */
bool HIDDisplayIsPresetWritable(HIDDisplayPresetInterfaceRef hidDisplayInterface, CFIndex presetIndex);

/*!
 * HIDDisplayCopyPresetUniqueID
 *
 * @abstract
 * Get preset unique id.
 *
 * @discussion
 * Get CFDataRef representing uniqueID for given preset index. Index has to be range [0 - (presetCount-1)].
 * UniqueID is raw 16 byte UUID. Caller need to release returned CFData if nonnull
 * Caller should use  single instance of HIDDisplayPresetIntereface per system as these APIs are
 * not thread safe.
 *
 * @param hidDisplayInterface
 * hidDisplayInterface object returned from HIDDisplayCreatePresetInterfaceWithContainerID
 *
 * @param presetIndex
 * preset index for which you want to set property
 *
 * @result
 * Returns CFDataRef for uniqueID on success.
 */
CFDataRef __nullable HIDDisplayCopyPresetUniqueID(HIDDisplayPresetInterfaceRef hidDisplayInterface, CFIndex presetIndex);

CF_ASSUME_NONNULL_END

__END_DECLS

#endif /* HIDDisplayPresetCAPI_h */
