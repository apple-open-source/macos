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
#ifndef _IOKIT_IOHIDEVENT_H
#define _IOKIT_IOHIDEVENT_H

#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSArray.h>
#include <IOKit/IOTypes.h>
#include <IOKit/hid/IOHIDEventTypes.h>
#include <IOKit/hidsystem/IOLLEvent.h>

typedef struct IOHIDEventData IOHIDEventData;

class IOHIDEvent: public OSObject
{
    OSDeclareAbstractStructors( IOHIDEvent )
    
    IOHIDEventData *    _data;
    OSArray *           _children;
    IOHIDEvent *        _parent;
    size_t              _capacity;
    AbsoluteTime        _timeStamp;
    uint32_t            _typeMask;
    IOOptionBits        _options;
    UInt32              _eventCount;

    bool initWithCapacity(IOByteCount capacity);
    bool initWithType(IOHIDEventType type);
    bool initWithTypeTimeStamp(IOHIDEventType type, AbsoluteTime timeStamp, IOOptionBits options = 0);
    IOByteCount getLength(UInt32 * eventCount);
    IOByteCount appendBytes(UInt8 * bytes, IOByteCount withLength);
    
    static IOHIDEvent * _axisEvent (    IOHIDEventType          type,
                                        AbsoluteTime            timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOOptionBits            options = 0);
    
public:
    static IOHIDEvent *     withBytes(  const void *            bytes,
                                        IOByteCount             size);

    static IOHIDEvent *     withType(   IOHIDEventType          type    = kIOHIDEventTypeNULL,
                                        IOOptionBits            options = 0);
                                        
    static IOHIDEvent *     withEventData (
                                        AbsoluteTime            timeStamp, 
                                        UInt32                  type,
                                        NXEventData *           data, 
                                        IOOptionBits            options = 0);
                                        
    static IOHIDEvent *     keyboardEvent(  
                                        AbsoluteTime            timeStamp, 
                                        UInt32                  usagePage,
                                        UInt32                  usage,
                                        Boolean                 down,
                                        IOOptionBits            options = 0);
                                        
    static IOHIDEvent *     translationEvent (
                                        AbsoluteTime            timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOOptionBits            options = 0);
                                        
    static IOHIDEvent *     scrollEvent (
                                        AbsoluteTime            timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOOptionBits            options = 0);
 
    static IOHIDEvent *     zoomEvent (
                                        AbsoluteTime            timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOOptionBits            options = 0);
                                       
    static IOHIDEvent *     accelerometerEvent (
                                        AbsoluteTime            timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOHIDAccelerometerType  type = 0,
                                        IOHIDAccelerometerSubType  subType = 0,
                                        IOOptionBits            options = 0);

    static IOHIDEvent *     gyroEvent ( AbsoluteTime            timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOHIDGyroType           type = 0,
                                        IOHIDGyroSubType        subType = 0,
                                        IOFixed                 qx=0,
                                        IOFixed                 qy=0,												
                                        IOFixed                 qz=0,												
                                        IOFixed                 qw=0,												
                                        IOOptionBits            options = 0);

    static IOHIDEvent *     compassEvent (
                                        AbsoluteTime            timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOHIDCompassType        type = 0,
                                        IOOptionBits            options = 0);    
	
    static IOHIDEvent *     buttonEvent (
                                        AbsoluteTime            timeStamp,
                                        UInt32                  buttonMask,
                                        IOOptionBits            options = 0);
                                        
    static IOHIDEvent *     buttonEvent (
                                        AbsoluteTime            timeStamp,
                                        UInt32                  buttonMask,
                                        IOFixed                 pressure,
                                        IOOptionBits            options = 0);    
                                        
    static IOHIDEvent *     ambientLightSensorEvent (
                                        AbsoluteTime            timeStamp,
                                        UInt32                  level,
                                        UInt32                  channel0    = 0,
                                        UInt32                  channel1    = 0,
										UInt32                  channel2    = 0,
										UInt32                  channel3    = 0,
                                        IOOptionBits            options     = 0);
                                        
	static IOHIDEvent *     proximityEvent (
                                        AbsoluteTime				timeStamp,
                                        IOHIDProximityDetectionMask	mask,
										UInt32						level,
                                        IOOptionBits				options = 0);
	
    static IOHIDEvent *     temperatureEvent (
                                        AbsoluteTime            timeStamp,
                                        IOFixed                 temperature,
                                        IOOptionBits            options     = 0);

    static IOHIDEvent *     absolutePointerEvent(
                                        AbsoluteTime                timeStamp,
                                        SInt32                      x,
                                        SInt32                      y,
                                        IOGBounds *                 bounds,
                                        UInt32                      buttonState,
                                        bool                        inRange,
                                        SInt32                      tipPressure,
                                        SInt32                      tipPressureMin,
                                        SInt32                      tipPressureMax,
                                        IOOptionBits                options);
    
    virtual void            appendChild(IOHIDEvent *childEvent);

    virtual AbsoluteTime    getTimeStamp();
    virtual void            setTimeStamp(AbsoluteTime timeStamp);
    
    virtual IOHIDEventType  getType();
    virtual void            setType(IOHIDEventType type);
    
    virtual IOHIDEventPhaseBits  getPhase();
    virtual void            setPhase(IOHIDEventPhaseBits phase);
    
    virtual IOHIDEvent *    getEvent(   IOHIDEventType          type, 
                                        IOOptionBits            options = 0);
    
    virtual SInt32          getIntegerValue(
                                        IOHIDEventField         key, 
                                        IOOptionBits            options = 0);
    virtual void            setIntegerValue(    
                                        IOHIDEventField         key, 
                                        SInt32                  value, 
                                        IOOptionBits            options = 0);
                                        
    virtual IOFixed         getFixedValue(IOHIDEventField       key,
                                        IOOptionBits            options = 0);
    virtual void            setFixedValue(
                                        IOHIDEventField         key, 
                                        IOFixed                 value,
                                        IOOptionBits            options = 0);

    virtual void free();
    
    virtual size_t getLength(); 
    virtual IOByteCount readBytes(void * bytes, IOByteCount withLength);

};

#endif /* _IOKIT_IOHIDEVENT_H */
