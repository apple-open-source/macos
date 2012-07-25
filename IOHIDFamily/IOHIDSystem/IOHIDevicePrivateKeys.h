/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2009 Apple Computer, Inc.  All Rights Reserved.
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
#ifndef _IOHIDEVICE_PRIVATE_KEYS_H
#define _IOHIDEVICE_PRIVATE_KEYS_H

enum {
    kAccelMouse                 = 0x0001,
    kAccelScroll                = 0x0002,
    kAbsoluteConvertMouse       = 0x0004,
    kAccelScrollConvertPointer  = 0x0008,
    kAccelNoScrollAcceleration  = 0x0010
};

enum {
    kScrollTypeContinuous       = 0x0001,
    kScrollTypeZoom             = 0x0002,
    kScrollTypeMomentumContinue = 0x0004,
    kScrollTypeTouch            = 0x0008,
    kScrollTypeMomentumStart    = 0x0010,
    kScrollTypeMomentumEnd      = 0x0020,    
    kScrollTypeMomentumAny      = kScrollTypeMomentumContinue | kScrollTypeMomentumStart | kScrollTypeMomentumEnd,
    
    kScrollTypeOptionPhaseAny           = 0xff00,
    kScrollTypeOptionPhaseBegan         = 0x0100,
    kScrollTypeOptionPhaseChanged       = 0x0200,
    kScrollTypeOptionPhaseEnded         = 0x0400,
    kScrollTypeOptionPhaseCanceled      = 0x0800,    
    kScrollTypeOptionPhaseMayBegin      = 0x8000,    
};

#define kIOHIDEventServicePropertiesKey "HIDEventServiceProperties"
#define kIOHIDTemporaryParametersKey    "HIDTemporaryParameters"
#define kIOHIDDefaultParametersKey      "HIDDefaultParameters"
#define kIOHIDDeviceParametersKey       "HIDDeviceParameters"
#define kIOHIDDeviceEventIDKey			"HIDDeviceEventID"
#define kIOHIDDeviceScrollWithTrackpadKey "TrackpadScroll" // really should be "HIDDeviceScrollWithTrackpad"
#define kIOHIDDeviceScrollDisableKey    "HIDDeviceScrollDisable"

#endif /* !_IOHIDEVICE_PRIVATE_KEYS_H */

