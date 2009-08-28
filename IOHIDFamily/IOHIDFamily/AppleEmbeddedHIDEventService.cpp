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

#include "AppleEmbeddedHIDEventService.h"
#include "IOHIDEvent.h"


//===========================================================================
// AppleEmbeddedHIDEventService class
#define super IOHIDEventService

OSDefineMetaClassAndAbstractStructors( AppleEmbeddedHIDEventService, super )


//====================================================================================================
// AppleEmbeddedHIDEventService::dispatchAccelerometerEvent
//====================================================================================================
void AppleEmbeddedHIDEventService::dispatchAccelerometerEvent(AbsoluteTime timestamp, IOFixed x, IOFixed y, IOFixed z, IOHIDAccelerometerType type, IOOptionBits options)
{
    IOHIDEvent * event = IOHIDEvent::accelerometerEvent(timestamp, x, y, z, type, options);
    
    if ( event ) {
        dispatchEvent(event);
        event->release();
    }
}

//====================================================================================================
// AppleEmbeddedHIDEventService::dispatchProximityEvent
//====================================================================================================
void AppleEmbeddedHIDEventService::dispatchProximityEvent(AbsoluteTime timestamp, IOFixed level, IOOptionBits options)
{

}

//====================================================================================================
// AppleEmbeddedHIDEventService::dispatchAmbientLightSensorEvent
//====================================================================================================
void AppleEmbeddedHIDEventService::dispatchAmbientLightSensorEvent(AbsoluteTime timestamp, UInt32 level, UInt32 channel0, UInt32 channel1, IOOptionBits options)
{
    IOHIDEvent * event = IOHIDEvent::ambientLightSensorEvent(timestamp, level, channel0, channel1, options);
    
    if ( event ) {
        dispatchEvent(event);
        event->release();
    }
}

//====================================================================================================
// AppleEmbeddedHIDEventService::dispatchTemperatureEvent
//====================================================================================================
void AppleEmbeddedHIDEventService::dispatchTemperatureEvent(AbsoluteTime timestamp, IOFixed temperature, IOOptionBits options)
{
    IOHIDEvent * event = IOHIDEvent::temperatureEvent(timestamp, temperature, options);
    
    if ( event ) {
        dispatchEvent(event);
        event->release();
    }
}
