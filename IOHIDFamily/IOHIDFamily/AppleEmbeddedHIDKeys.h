/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef _IOKIT_HID_EMBEDDEDHIDKEYS_H_
#define _IOKIT_HID_EMBEDDEDHIDKEYS_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

#define kIOHIDThresholdXKey                     "ThresholdX"
#define kIOHIDThresholdYKey                     "ThresholdY"
#define kIOHIDThresholdZKey                     "ThresholdZ"
#define kIOHIDThresholdPeriodKey                "ThresholdPeriod"


#define kIOHIDAccelerometerShakeKey             "Shake"
#define kIOHIDGyroShakeKey						"Shake"

#define kIOHIDOrientationKey					"Orientation"

/*!
 @typedef IOHIDOrientationType
 @abstract Orientation of event triggered.
 @discussion
 @constant kIOHIDOrientationTypeUndefined
 @constant kIOHIDOrientationTypeNorth
 @constant kIOHIDOrientationTypeSouth
 @constant kIOHIDOrientationTypeEast
 @constant kIOHIDOrientationTypeWest
 @constant kIOHIDOrientationTypeNorthEast
 @constant kIOHIDOrientationTypeNorthWest
 @constant kIOHIDOrientationTypeSoutEast
 @constant kIOHIDOrientationTypeSouthWest
 */
enum {
    kIOHIDOrientationTypeUndefined  = 0,
    kIOHIDOrientationTypeNorth      = 1,
    kIOHIDOrientationTypeSouth      = 2,
    kIOHIDOrientationTypeEast       = 3,
    kIOHIDOrientationTypeWest       = 4,
    kIOHIDOrientationTypeNorthEast  = 5,
    kIOHIDOrientationTypeNorthWest  = 6,
    kIOHIDOrientationTypeSouthEast  = 7,
    kIOHIDOrientationTypeSouthWest  = 8
};
typedef uint32_t IOHIDOrientationType;

#define kIOHIDPlacementKey						"Placement"
/*!
 @typedef IOHIDPlacementType
 @abstract Placement of event triggered.
 @discussion
 @constant kIOHIDPlacementTypeUndefined
 @constant kIOHIDPlacementTypeTop
 @constant kIOHIDPlacementTypeBottom
 */
enum {
    kIOHIDPlacementTypeUndefined = 0,
    kIOHIDPlacementTypeTop       = 1,
    kIOHIDPlacementTypeBottom    = 2
};
typedef uint32_t IOHIDPlacementType;



__END_DECLS

#endif /* !_IOKIT_HID_EMBEDDEDHIDKEYS_H_ */
