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

#ifndef _IOKIT_HID_APPLEEMBEDDEDHIDEVENTSERVICE_H
#define _IOKIT_HID_APPLEEMBEDDEDHIDEVENTSERVICE_H

#include <IOKit/hidevent/IOHIDEventService.h>
#include <IOKit/hid/AppleEmbeddedHIDKeys.h>

class AppleEmbeddedHIDEventService: public IOHIDEventService
{
    OSDeclareAbstractStructors( AppleEmbeddedHIDEventService )
	
public:
	virtual bool			handleStart(IOService * provider);


protected:
    virtual void            dispatchAccelerometerEvent(AbsoluteTime timestamp, IOFixed x, IOFixed y, IOFixed z, IOHIDAccelerometerType type = 0, IOHIDAccelerometerSubType subType = 0, IOOptionBits options=0);
	
    virtual void            dispatchGyroEvent(AbsoluteTime timestamp, IOFixed x, IOFixed y, IOFixed z, IOHIDGyroType type = 0, IOHIDGyroSubType subType = 0, IOOptionBits options=0);
        
    virtual void            dispatchProximityEvent(AbsoluteTime timestamp, IOHIDProximityDetectionMask mask, UInt32 level = 0, IOOptionBits options=0);

    virtual void            dispatchAmbientLightSensorEvent(AbsoluteTime timestamp, UInt32 level, UInt32 channel0 = 0, UInt32 channel1 = 0, UInt32 channel2 = 0, UInt32 channel3 = 0, IOOptionBits options=0);

    virtual void            dispatchTemperatureEvent(AbsoluteTime timestamp, IOFixed temperature, IOOptionBits options=0);
	
	virtual IOHIDOrientationType	getOrientation();
	
	virtual IOHIDPlacementType		getPlacement();
};

#endif /* _IOKIT_HID_APPLEEMBEDDEDHIDEVENTSERVICE_H */