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

#ifndef IOHIDEventServiceKeys_Private_h
#define IOHIDEventServiceKeys_Private_h


/*!
 * @define kIOHIDMaxEventSizeKey
 *
 * @abstract
 * Number property that describes the max event size in bytes that the event
 * service will dispatch. If this property is not provided, the default max
 * size will be set to 4096 bytes.
 */
#define kIOHIDMaxEventSizeKey "MaxEventSize"

/*!
 * @define kIOHIDMaxReportSizeKey
 *
 * @abstract
 * Number property that describes the max report size in bytes that the device
 * will dispatch. If this property is not provided, the
 * kIOHIDMaxInputReportSizeKey key will be used.
 */
#define kIOHIDMaxReportSizeKey "MaxReportSize"

/*!
 * @define kIOHIDReportPoolSizeKey
 *
 * @abstract
 * Number property that describes the amount of report buffers that should
 * be created for handling reports. If this property is not provided, the
 * default value of 1 will be used.
 */
#define kIOHIDReportPoolSizeKey "ReportPoolSize"

/*!
 * @define kIOHIDEventTypeKey
 *
 * @abstract
 * Number property that represents the event type. Acceptable values are defined
 * in the IOHIDEventType enumerator in IOHIDEventTypes.h
 */
#define kIOHIDEventTypeKey "EventType"
#define kIOHIDMatchingEventTypeKey kIOHIDEventTypeKey

/*!
 * @define kIOHIDUsagePageKey
 *
 * @abstract
 * Number property that represents a usage page. Acceptable values are defined
 * in IOHIDUsageTables.h/AppleHIDUsageTables.h
 */
#define kIOHIDUsagePageKey "UsagePage"

/*!
 * @define kIOHIDUsageKey
 *
 * @abstract
 * Number property that represents a usage. Acceptable values are defined in
 * IOHIDUsageTables.h/AppleHIDUsageTables.h
 */
#define kIOHIDUsageKey "Usage"


/*!
 * @define kPrimaryVendorUsages
 *
 * @abstract
 * Number or array of numbers that represents vendor usage pairs.  If multiple event dispatched from the report vendor event that match usage
 * usage pair will be made a parent event.  Usage pair represented by  format UsagePair = (UsagePage << 16) | Usage.
 */
#define kPrimaryVendorUsages "PrimaryVendorUsages"

/*!
 * @define kPrimaryVendorEvents
 *
 * @abstract
 * Array of numbers that represents vendor usage pairs.  If multiple event dispatched from the report vendor event that match
 * usage pair will be made a parent event.  Usage pair represented by  format UsagePair = (UsagePage << 16) | Usage.
 */
#define kPrimaryVendorEvents "PrimaryVendorEvents"


/*!
 * @defined     kIOHIDKeyboardEnabledKey
 * @abstract    Property published by a keyboard service to indicate whether the keyboard is in an
 *              enabled state. This property is used by application software to determine if a software
 *              keyboard should be presented to the user.
 */
#define kIOHIDKeyboardEnabledKey "KeyboardEnabled"

/*!
 * @defined     kIOHIDKeyboardEnabledEventKey
 * @abstract    Property describes the event that is dispatched when there is a change in the keyboard's
 *              state.
 * @discussion  Application software is expected to read the kIOHIDKeyboardEnabledKey value
 *              when the described event is received. The value of this property is a dictionary with keys
 *              kIOHIDKeyboardEnabledEventUsagePageKey, kIOHIDKeyboardEnabledEventUsageKey, and
 *              kIOHIDKeyboardEnabledEventEventTypeKey.
 *
 */
#define kIOHIDKeyboardEnabledEventKey "KeyboardEnabledEvent"

/*!
 * @defined     kIOHIDKeyboardEnabledEventUsagePageKey
 * @abstract    Usage page of event changing keyboard state
 */
#define kIOHIDKeyboardEnabledEventUsagePageKey "UsagePage"

/*!
 * @defined     kIOHIDKeyboardEnabledEventUsageKey
 * @abstract    Usage of event changing keyboard state
 */
#define kIOHIDKeyboardEnabledEventUsageKey "Usage"

/*!
 * @defined     kIOHIDKeyboardEnabledEventEventTypeKey
 * @abstract    Event type of event changing keyboard state
 */
#define kIOHIDKeyboardEnabledEventEventTypeKey "EventType"

/*!
 * @defined     kIOHIDKeyboardEnabledByEventKey
 * @abstract    A keyboard service that supports keyboard enablement states should publish this property
 *              and set its value to true.
 */
#define kIOHIDKeyboardEnabledByEventKey "KeyboardEnabledByEvent"

/*!
 * @defined     kIOHIDSuppressKeyboardEnabledEventKey
 * @abstract    When setting a keyboard enabled property a keyboard event will not be dispatched for this property set.
 */
#define kIOHIDSuppressKeyboardEnabledEventKey "SuppressKeyboardEnabledEvent"

/*!
 * @defined     kIOHIDSuppressKeyboardEnabledSetReportKey
 * @abstract    When setting a keyboard enabled property a keyboard set report will not be dispatched
 *              for to the device during this property set.
 */
#define kIOHIDSuppressKeyboardEnabledSetReportKey "SuppressKeyboardEnabledSetReport"

/*!
 * @defined     kIOHIDSupportsGlobeKeyKey
 * @abstract    A keyboard service that supports keyboard layout changes should by keyboard event
 *              publish this property and set its value to true.
 */
#define kIOHIDSupportsGlobeKeyKey "SupportsGlobeKey"

/*!
 * @defined     kIOHIDAppleVendorSupported
 * @abstract    If true, Apple Vendor Usages should be supported by this device.
 */
#define kIOHIDAppleVendorSupported "AppleVendorSupported"

/*!
 * @defined     kIOHIDElementKey
 * @abstract    Keys that represents an element property.
 * @discussion  Property for a HID Device or element dictionary.
 *              Elements can be heirarchical, so they can contain other elements.
 */
#define kIOHIDElementKey                    "Elements"

/*!
 * @defined     kIOHIDElementParentCollectionKey
 * @abstract    Keys that represents an element parent property.
 * @discussion  Property for a HID Device or element dictionary.
 *              Elements can be heirarchical, so they can contain other elements.
 */
#define kIOHIDElementParentCollectionKey            "ParentCollection"

/*!
 * Unsigned integer value represent bitmap of sensor properties supported by service (see  kSensorProperty...)
 */
#define kIOHIDEventServiceSensorPropertySupportedKey  "SensorPropertySupported"

enum {
    /*!
     @defined    kSensorPropertyReportInterval
     @abstract   Sensor service supports report interval property
     @discussion See kIOHIDSensorPropertyReportIntervalKey
     */
    kSensorPropertyReportInterval   = (1 << 0),

    /*!
     @defined    kSensorPropertyReportLatency
     @abstract   Sensor service supports report latency (AKA batch interval) property
     @discussion See kIOHIDSensorPropertyReportLatencyKey, kIOHIDSensorPropertyBatchIntervalKey
     */
    kSensorPropertyReportLatency    = (1 << 1),

    /*!
     @defined    kSensorPropertySniffControl
     @abstract   Sensor service supports BT sniff mode control
     @discussion See kIOHIDSensorPropertySniffControlKey
     */
    kSensorPropertySniffControl     = (1 << 2),

    /*!
     @defined    kSensorPropertySampleInterval
     @abstract   Sensor service supports sample interval property
     @discussion See kIOHIDSensorPropertySampleIntervalKey
     */
    kSensorPropertySampleInterval   = (1 << 3),

    /*!
     @defined    kSensorPropertyMaxFIFOEvents
     @abstract   Sensor service supports maximum FIFO event queue size / max batch size
     @discussion See kIOHIDSensorPropertyMaxFIFOEventsKey
     */
    kSensorPropertyMaxFIFOEvents    = (1 << 4),
};

/*!
 * @defined   kIOHIDSensorPropertySniffControlKey
 * @abstract  Property to control BT sniff mode for sensor
 * @discussion The following values are supported:
 *      0 - enable sniff mode while streaming
 *      1 - disable sniff mode while streaming
 *
 *  property associated with IOHIDDevice element with usagePage/usage:
 *   kHIDPage_AppleVendorSensor / kHIDUsage_AppleVendorSensor_BTSniffOff
 *
 */
#define kIOHIDSensorPropertySniffControlKey      "SniffControl"

/*!
 @defined    kIOHIDSensorPropertyMaxFIFOEventsKey
 @abstract   Property to get or set the maximum FIFO event queue size of supported sensor devices
 @discussion Corresponds to kHIDUsage_Snsr_Property_MaxFIFOEvents in a sensor device's
 descriptor.
 */
#define kIOHIDSensorPropertyMaxFIFOEventsKey    "MaxFIFOEvents"

/*!
 @defined    kIOHIDAuthenticatedDeviceKey
 @abstract   Property to indicate that a game controller is authenticated for embedded trains
 @discussion Set to true for a device to allow support on embedded trains, set to false to block support.
 descriptor.
 */
#define kIOHIDAuthenticatedDeviceKey        "Authenticated"

/*!
 @typedef IOHIDGameControllerType
 @abstract Describes support game controller types.
 @constant kIOHIDGameControllerTypeStandard Standard game controller.
 @constant kIOHIDGameControllerTypeExtended Extended game controller.
 */
enum {
    kIOHIDGameControllerTypeStandard,
    kIOHIDGameControllerTypeExtended
};
typedef uint32_t IOHIDGameControllerType;

#define kIOHIDGameControllerTypeKey         "GameControllerType"

#define kIOHIDGameControllerFormFittingKey  "GameControllerFormFitting"

/*!
   @defined    kIOHIDDigitizerSurfaceSwitchKey
   @abstract   Property to turn on / of surface digitizer contact reporting
   @discussion To allow for better power management, a host may wish to indicate what it would like a touchpad digitizer to not report surface digitizer contacts by clearing this
               flag. By default, upon cold‐boot/power cycle, touchpads that support reporting surface
               contacts shall do so by default.
*/
#define kIOHIDDigitizerSurfaceSwitchKey "DigitizerSurfaceSwitch"

#define kIOHIDDigitizerGestureCharacterStateKey "DigitizerCharacterGestureState"

/*!
 @typedef IOHIDElementMatchOptions
 @abstract Options to control the functionality of getting matching elements.
 @constant kIOHIDSearchDeviceElements Search the full list of device elements instead of the interface list
 */
enum {
    kIOHIDSearchDeviceElements = (1 << 0),
};
typedef uint32_t IOHIDElementMatchOptions;

/*!
 * @defined    kIOHIDUnifiedKeyMappingKey
 * @abstract   Property to enable/disable unified key mappings based on using ctrl instead of the Fn key.
 * @discussion To unify the key remapping across iPadOS and macOS ctrl is the preferred modifier for shortcuts that were originally tied to the Fn key.
 *             When the ctrl key is held then the arrow keys become Up -> PageUp, Down -> PageDown, Left -> Home, Right -> End
 *             Defaults to false
 */
#define kIOHIDUnifiedKeyMappingKey "UnifiedKeyMapping"

/*!
 * @defined    kCtrlKeyboardUsageMapKey
 * @abstract   Remapping for when Ctrl Key modifier is held
 * @discussion string of comma separated uint64_t values representing (usagePage<<32) | usage pairs
 */
#define kCtrlKeyboardUsageMapKey "CtrlKeyboardUsageMap"

/*!
 * @defined    kIOHIDUnifiedKeyModifierMapKey
 * @abstract   Debug key for printing the current Unified Key Remappings
 */
#define kIOHIDUnifiedKeyModifierMapKey "UnifiedKeyMaps"

/*!
 * @defined    kIOHIDLegacyKeyModifierMapKey
 * @abstract   Debug key for printing the current Legacy Key Remappings
 */
#define kIOHIDLegacyKeyModifierMapKey "LegacyKeyMaps"

#endif /* IOHIDEventServiceKeys_Private_h */
