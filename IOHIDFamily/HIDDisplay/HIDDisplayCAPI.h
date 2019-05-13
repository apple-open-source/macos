//
//  HIDDisplayCAPI.h
//  IOHIDFamily
//
//  Created by AB on 1/10/19.
//

#ifndef HIDDisplayCAPI_h
#define HIDDisplayCAPI_h


#import <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

#if defined(__cplusplus)
extern "C" {
#endif
    
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

    
typedef CFTypeRef HIDDisplayDeviceRef;

/*!
 * HIDDisplayCreateDeviceWithContainerID
 *
 * @abstract
 * Creates HIDDisplayDevice ref with container ID
 *
 * @discussion
 * Create HIDDisplayDevice object backed by actual hid device matching
 * given container ID. If no corresponding
 * HID device matches container ID  , this will return NULL.
 *
 * @param containerID
 * Attributes which can uniquely identify display device.
 *
 * @result
 * Returns an instance of a HIDDisplayDevice object on success which has to be released by caller.
 */
HIDDisplayDeviceRef __nullable HIDDisplayCreateDeviceWithContainerID(CFStringRef containerID);

/*!
 * HIDDisplayGetPresetCount
 *
 * @abstract
 * Get preset count for given display. This is readonly property
 *
 * @discussion
 * Get preset count for given HIDDisplayDevice object
 *
 * @param hidDisplayDevice
 * HIDDisplayDevice object returned from HIDDisplayCreateDeviceWithContainerID
 *
 * @result
 * Returns total number of display preset supported by device on success and -1 on failure.
 */
CFIndex HIDDisplayGetPresetCount(HIDDisplayDeviceRef hidDisplayDevice);

/*!
 * HIDDisplayGetFactoryDefaultPresetIndex
 *
 * @abstract
 * Get factory default preset index set for display
 *
 * @discussion
 * Get factory default preset index for given HIDDisplayDevice object. This is readonly property
 *
 * @param hidDisplayDevice
 * HIDDisplayDevice object returned from HIDDisplayCreateDeviceWithContainerID
 *
 * @param error
 * field which will be filled with  error code on failure.
 *
 * @result
 * Returns factory default preset index of display device on success and -1 on failure.
 */
CFIndex HIDDisplayGetFactoryDefaultPresetIndex(HIDDisplayDeviceRef hidDisplayDevice, CFErrorRef *error);

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
 * @param hidDisplayDevice
 * HIDDisplayDevice object returned from HIDDisplayCreateDeviceWithContainerID
 *
 * @result
 * Returns array of CFStringRef on success.
 */
CFArrayRef __nullable HIDDisplayGetPresetCapabilities(HIDDisplayDeviceRef hidDisplayDevice);
    
/*!
 * HIDDisplayGetActivePresetIndex
 *
 * @abstract
 * Get active preset index for display
 *
 * @discussion
 * Get active preset index for given HIDDisplayDevice object.
 *
 * @param hidDisplayDevice
 * HIDDisplayDevice object returned from HIDDisplayCreateDeviceWithContainerID
 *
 * @param error
 * field which will be filled with  error code on failure.
 *
 * @result
 * Returns active preset index of display device on success and -1 on failure.
 */
CFIndex HIDDisplayGetActivePresetIndex(HIDDisplayDeviceRef hidDisplayDevice, CFErrorRef* error);

/*!
 * HIDDisplaySetActivePresetIndex
 *
 * @abstract
 * Set active preset index for display
 *
 * @discussion
 * Set active preset index for given HIDDisplayDevice object.
 *
 * @param hidDisplayDevice
 * HIDDisplayDevice object returned from HIDDisplayCreateDeviceWithContainerID
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
bool HIDDisplaySetActivePresetIndex(HIDDisplayDeviceRef hidDisplayDevice, CFIndex presetIndex, CFErrorRef* error);

/*!
 * HIDDisplayCopyPreset
 *
 * @abstract
 * Copy info for given preset index.
 *
 * @discussion
 * Copy info for given preset index. Index has to be range [0 - (presetCount-1)].
 * Caller is responsible for releasing return object if nonnull
 *
 * @param hidDisplayDevice
 * HIDDisplayDevice object returned from HIDDisplayCreateDeviceWithContainerID
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
CFDictionaryRef __nullable HIDDisplayCopyPreset(HIDDisplayDeviceRef hidDisplayDevice, CFIndex presetIndex, CFErrorRef *error);

/*!
 * HIDDisplaySetPreset
 *
 * @abstract
 * Set info for given preset index.
 *
 * @discussion
 * Set info for given preset index. Index has to be range [0 - (presetCount-1)].
 * Returns false if info is readonly.
 *
 * @param hidDisplayDevice
 * HIDDisplayDevice object returned from HIDDisplayCreateDeviceWithContainerID
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
bool HIDDisplaySetPreset(HIDDisplayDeviceRef hidDisplayDevice, CFIndex presetIndex, CFDictionaryRef info, CFErrorRef *error);

/*!
 * HIDDisplayIsPresetValid
 *
 * @abstract
 * Check if given preset is valid.
 *
 * @discussion
 * Get preset valid info. Index has to be range [0 - (presetCount-1)].
 * This imformation can be used to determine if given preset can be set as active preset.
 *
 * @param hidDisplayDevice
 * HIDDisplayDevice object returned from HIDDisplayCreateDeviceWithContainerID
 *
 * @param presetIndex
 * preset index for which you want to set property
 *
 *
 * @result
 * Returns true if preset is valid.
 */
bool HIDDisplayIsPresetValid(HIDDisplayDeviceRef hidDisplayDevice, CFIndex presetIndex);

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
 *
 * @param hidDisplayDevice
 * HIDDisplayDevice object returned from HIDDisplayCreateDeviceWithContainerID
 *
 * @param presetIndex
 * preset index for which you want to set property
 *
 * @result
 * Returns true if preset is writable.
 */
bool HIDDisplayIsPresetWritable(HIDDisplayDeviceRef hidDisplayDevice, CFIndex presetIndex);

/*!
 * HIDDisplayCopyPresetUniqueID
 *
 * @abstract
 * Get preset unique id.
 *
 * @discussion
 * Get CFStringRef representing uniqueID for given preset index. Index has to be range [0 - (presetCount-1)].
 * UniqueID is string  representation of 128 byte UUID. Caller need to release returned string if nonnull
 *
 * @param hidDisplayDevice
 * HIDDisplayDevice object returned from HIDDisplayCreateDeviceWithContainerID
 *
 * @param presetIndex
 * preset index for which you want to set property
 *
 * @result
 * Returns CFStringRef for uniqueID on success.
 */
CFStringRef __nullable HIDDisplayCopyPresetUniqueID(HIDDisplayDeviceRef hidDisplayDevice, CFIndex presetIndex);
    
CF_ASSUME_NONNULL_END
    
#ifdef __cplusplus
}
#endif

__END_DECLS

#endif /* HIDDisplayCAPI_h */
