/*
 * Copyright (c) 2002-2014 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#ifndef _IOPSKEYSPRIVATE_H_
#define _IOPSKEYSPRIVATE_H_

/*!
 * @define      kAppleRawCurrentCapacityKey
 * @abstract    CFDictionary key for the current power source's raw capacity, unaltered by any smoothing algorithms.
 *
 * @discussion
 *              <ul>
 *              <li> Apple-defined power sources will publish this key in units of percent or mAh.
 *              <li> The power source's software may specify the units for this key.
 *                   The units must be consistent for all capacities reported by this power source.
 *                   The power source will usually define this number in units of percent, or mAh.
 *              <li> Clients may derive a raw percentage of power source battery remaining by dividing "AppleRawCurrentCapacity" by "Max Capacity"
 *              <li> Type CFNumber kCFNumberIntType (signed integer)
 *              </ul>
 */
#ifndef kAppleRawCurrentCapacityKey
#define kAppleRawCurrentCapacityKey "AppleRawCurrentCapacity"
#endif

/*
 * kIOPSVendorIDSourceKey holds CFNumberRef data. Used to differentiate
 * between various vendor id sources.
 */
#define kIOPSVendorIDSourceKey              "Vendor ID Source"

/* Internal values for kIOPSTypeKey */
#define kIOPSAccessoryType                  "Accessory Source"


#if TARGET_OS_IPHONE

/* kIOPSRawExternalConnectivityKey specifies if device is connected to
 * any external power source. In some cases, kIOPSPowerSourceStateKey may not
 * show the external power source, if that external source is a battery
 */
#define kIOPSRawExternalConnectivityKey     "Raw External Connected"


/*
 * Additional identifiable information to relate multiple accessory power sources.
 * Holds a CFStringRef
 */
#define kIOPSAccessoryIdentifierKey         "Accessory Identifier"


/* Internal transport types */
#define kIOPSAIDTransportType                   "AID"
#define kIOPSTransportTypeBluetooth             "Bluetooth"
#define kIOPSTransportTypeBluetoothLowEnergy    "Bluetooth LE"

/*
 * Invalid ProductId & VendorId values are used in cases when there are no
 * product/vendor ids assigned. In such cases, kIOPSNameKey can be used to
 * identify the power source.
 */
#define kIOPSInvalidProductID                   0xffff
#define kIOPSInvalidVendorID                    0xffff
#endif

/*
 * Power adapter related internal keys
 */

/*!
 * @define      kIOPSPowerAdapterSerialStringKey
 *
 * @abstract    The power adapter's serial string.
 *              The value associated with this key is a CFString value
 *
 * @discussion  This key may be present in the dictionary returned from
 *              @link //apple_ref/c/func/IOPSCopyExternalPowerAdapterDetails IOPSCopyExternalPowerAdapterDetails @/link
 *              This key might not be defined in the adapter details dictionary.
 */
#define kIOPSPowerAdapterSerialStringKey    "SerialString"

/*!
 * @define      kIOPSPowerAdapterNameKey
 *
 * @abstract    The power adapter's name.
 *              The value associated with this key is a CFString value
 *
 * @discussion  This key may be present in the dictionary returned from
 *              @link //apple_ref/c/func/IOPSCopyExternalPowerAdapterDetails IOPSCopyExternalPowerAdapterDetails @/link
 *              This key might not be defined in the adapter details dictionary.
 */

#define kIOPSPowerAdapterNameKey            "Name"

/*!
 * @define      kIOPSPowerAdapterNameKey
 *
 * @abstract    The power adapter's manufacturer's id.
 *              The value associated with this key is a CFNumber kCFNumberIntType integer value
 *
 * @discussion  This key may be present in the dictionary returned from
 *              @link //apple_ref/c/func/IOPSCopyExternalPowerAdapterDetails IOPSCopyExternalPowerAdapterDetails @/link
 *              This key might not be defined in the adapter details dictionary.
 */
#define kIOPSPowerAdapterManufacturerIDKey  "Manufacturer"

/*!
 * @define      kIOPSPowerAdapterHardwareVersionKey
 *
 * @abstract    The power adapter's hardware version.
 *              The value associated with this key is a CFString value
 *
 * @discussion  This key may be present in the dictionary returned from
 *              @link //apple_ref/c/func/IOPSCopyExternalPowerAdapterDetails IOPSCopyExternalPowerAdapterDetails @/link
 *              This key might not be defined in the adapter details dictionary.
 */
#define kIOPSPowerAdapterHardwareVersionKey       "HwVersion"

/*!
 * @define      kIOPSPowerAdapterFirmwareVersionKey
 *
 * @abstract    The power adapter's firmware version.
 *              The value associated with this key is a CFString value
 *
 * @discussion  This key may be present in the dictionary returned from
 *              @link //apple_ref/c/func/IOPSCopyExternalPowerAdapterDetails IOPSCopyExternalPowerAdapterDetails @/link
 *              This key might not be defined in the adapter details dictionary.
 */
#define kIOPSPowerAdapterFirmwareVersionKey       "FwVersion"


/*
 * Battery case related internal keys
 */

/*!
 * @define      kIOPSAppleBatteryCaseCumulativeCurrentKey
 * @abstract    CFDictionary key for a battery case's cumulative current flow
 *              through its battery since its last reset.
 *
 * @discussion
 *              <ul>
 *              <li> Apple-defined power sources may publish this key in units amp-seconds.
 *              <li> Type CFNumber kCFNumberIntType (signed integer)
 *              </ul>
 */
#define kIOPSAppleBatteryCaseCumulativeCurrentKey "Battery Case Cumulative Current"

/*!
 * @define      kAppleBatteryCaseAvailableCurrentKey
 * @abstract    CFDictionary key for a battery case's available current for host.
 *
 * @discussion
 *              <ul>
 *              <li> Apple-defined power sources will publish this key in units mA.
 *              <li> Type CFNumber kCFNumberIntType (signed integer)
 *              </ul>
 */
#define kIOPSAppleBatteryCaseAvailableCurrentKey "Battery Case Available Current"

/*!
 * @define      kIOPSAppleBatteryCaseChemIDKey
 * @abstract    CFDictionary key for a battery case's battery's Chem ID.
 *
 * @discussion
 *              <ul>
 *              <li> Apple-defined power sources may publish this key.
 *              <li> Type CFNumber kCFNumberIntType (integer)
 *              <li> Note that this value does not have any physical unit.
 *              </ul>
 */
#define kIOPSAppleBatteryCaseChemIDKey "Battery Case Chem ID"

/*!
 * @define      kAppleBatteryCommandSetCurrentLimitBackOffKey
 *
 * @abstract    Tell the battery case of a PMU imposed back off in current limit.
 * @discussion
 *              <ul>
 *              <li> The matching argument should be a CFNumber of kCFNumberIntType
 *              <li> specifying the amount the PMU has reduced incoming current limit in mA.
 */
#define kAppleBatteryCommandSetCurrentLimitBackOffKey "Current Limit Back Off"

#endif /* defined(_IOPSKEYSPRIVATE_H_) */
