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

#include <TargetConditionals.h>
#if KERNEL
#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSData.h>
#endif /*KERNEL*/
#include <IOKit/IOTypes.h>
#include <IOKit/hid/IOHIDEventTypes.h>

#include <IOKit/hidsystem/IOLLEvent.h>

#define ALIGNED_DATA_SIZE(data_size,align_size) ((((data_size - 1) / align_size) + 1) * align_size)

typedef struct IOHIDEventData IOHIDEventData;

/*!
    @class    IOHIDEvent
    @abstract Create an IOHIDEvent for use inside of the IOHID Event System.
              IOHIDEvents represent an action produced by a HID device or service,
              and can be entered into the IOHID Event System for distrubution to clients
              on the system listening for such events.
              All methods to create an IOHIDEvent take timestamp and options parameters.
              The timestamp can be interpreted in two ways currently, as mach absolute time or
              mach continuous time. The default is mach absolute time. To change this behavior,
              set kIOHIDEventOptionContinuousTime in the options passed to the creation method.
 */
#if defined(KERNEL) && !defined(KERNEL_PRIVATE)
class __deprecated_msg("Use DriverKit") IOHIDEvent : public OSObject
#else
class IOHIDEvent : public OSObject
#endif
{
    OSDeclareAbstractStructors( IOHIDEvent )
    
    IOHIDEventData *    _data;
    OSArray *           _children;
    IOHIDEvent *        _parent;
    size_t              _capacity;
    UInt64              _timeStamp;
    UInt64              _senderID;
    uint64_t            _typeMask;
    IOOptionBits        _options;

    bool initWithCapacity(IOByteCount capacity);
    bool initWithType(IOHIDEventType type, IOByteCount additionalCapacity=0);
    bool initWithTypeTimeStamp(IOHIDEventType type, UInt64 timeStamp, IOOptionBits options = 0, IOByteCount additionalCapacity=0);
    IOByteCount readBytes(void * bytes, IOByteCount withLength, UInt32* outCount);
    
    static IOHIDEvent * _axisEvent (    IOHIDEventType          type,
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOOptionBits            options = 0);

    static IOHIDEvent * _motionEvent (  IOHIDEventType          type,
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        uint32_t                motionType = 0,
                                        uint32_t                motionSubType = 0,
                                        UInt32                  sequence = 0,
                                        IOOptionBits            options = 0);

public:
    static IOHIDEvent *     withBytes(  const void *            bytes,
                                        IOByteCount             size);

    static IOHIDEvent *     withType(   IOHIDEventType          type    = kIOHIDEventTypeNULL,
                                        IOOptionBits            options = 0);

    static IOHIDEvent *     collectionEvent(UInt64          timestamp,
                                            UInt32          usagePage,
                                            UInt32          usage,
                                            bool            ungroupForLegacy,
                                            IOOptionBits    options = 0);
                                        
    static IOHIDEvent *     keyboardEvent(  
                                        UInt64                  timeStamp,
                                        UInt32                  usagePage,
                                        UInt32                  usage,
                                        Boolean                 down,
                                        IOOptionBits            options = 0);
    
    static IOHIDEvent *     keyboardEvent(
                                        UInt64                  timeStamp,
                                        UInt32                  usagePage,
                                        UInt32                  usage,
                                        Boolean                 down,
                                        UInt8                   pressCount,
                                        Boolean                 longPress,
                                        UInt8                   clickSpeed,
                                        IOOptionBits            options = 0);

    
    static IOHIDEvent *     translationEvent (
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOOptionBits            options = 0);

    static IOHIDEvent *     scrollEventWithFixed (
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOOptionBits            options = 0);
  
    static IOHIDEvent *     scrollEvent (
                                        UInt64                  timeStamp,
                                        SInt32                  x,
                                        SInt32                  y,
                                        SInt32                  z,
                                        IOOptionBits            options = 0);
 
    static IOHIDEvent *     zoomEvent (
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOOptionBits            options = 0);
                                       
    static IOHIDEvent *     accelerometerEvent (
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOHIDMotionType         type = 0,
                                        IOHIDMotionPath         subType = 0,
                                        UInt32                  sequence = 0,
                                        IOOptionBits            options = 0);

    static IOHIDEvent *     gyroEvent ( UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOHIDMotionType         type = 0,
                                        IOHIDMotionPath         subType = 0,
                                        UInt32                  sequence = 0,
                                        IOOptionBits            options = 0);

    static IOHIDEvent *     compassEvent (
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOHIDMotionType         type = 0,
                                        IOHIDMotionPath         subType = 0,
                                        UInt32                  sequence = 0,
                                        IOOptionBits            options = 0);
    
    static IOHIDEvent *     buttonEvent (
                                        UInt64                  timeStamp,
                                        UInt32                  mask,
                                        UInt8                   number,
                                        bool                    state,
                                        IOOptionBits            options = 0);
                                        
    static IOHIDEvent *     buttonEvent (
                                        UInt64                  timeStamp,
                                        UInt32                  mask,
                                        UInt8                   number,
                                        IOFixed                 pressure,
                                        IOOptionBits            options = 0);    
                                        
    static IOHIDEvent *     ambientLightSensorEvent (
                                        UInt64                  timeStamp,
                                        UInt32                  level,
                                        UInt32                  channel0    = 0,
                                        UInt32                  channel1    = 0,
                                        UInt32                  channel2    = 0,
                                        UInt32                  channel3    = 0,
                                        IOOptionBits            options     = 0);

    static IOHIDEvent *     ambientLightSensorEvent(
                                        UInt64                  timeStamp,
                                        UInt32                  level,
                                        UInt8                   colorSpace,
                                        IOHIDDouble             colorComponent0,
                                        IOHIDDouble             colorComponent1,
                                        IOHIDDouble             colorComponent2,
                                        IOOptionBits            options);
  
    static IOHIDEvent *     proximityEvent (
                                        UInt64                      timeStamp,
                                        IOHIDProximityDetectionMask mask,
                                        UInt32                      level,
                                        IOOptionBits                options = 0);
    
    static IOHIDEvent *     proximityEventWithProbability (
                                        UInt64                      timeStamp,
                                        IOHIDProximityDetectionMask mask,
                                        UInt32                      probability,
                                        IOOptionBits                options = 0);

    static IOHIDEvent *     temperatureEvent (
                                        UInt64                  timeStamp,
                                        IOFixed                 temperature,
                                        IOOptionBits            options = 0);


    static IOHIDEvent *     relativePointerEventWithFixed(
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        UInt32                  buttonState,
                                        UInt32                  oldButtonState = 0,
                                        IOOptionBits            options = 0);

    static IOHIDEvent *     relativePointerEvent(
                                        UInt64                  timeStamp,
                                        SInt32                  x,
                                        SInt32                  y,
                                        SInt32                  z,
                                        UInt32                  buttonState,
                                        UInt32                  oldButtonState = 0,
                                        IOOptionBits            options = 0);

    static IOHIDEvent *     absolutePointerEvent(
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        UInt32                  buttonState,
                                        UInt32                  oldButtonState = 0,
                                        IOOptionBits            options = 0);
    
    static IOHIDEvent *     multiAxisPointerEvent(
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOFixed                 rX,
                                        IOFixed                 rY,
                                        IOFixed                 rZ,
                                        UInt32                  buttonState,
                                        UInt32                  oldButtonState = 0,
                                        IOOptionBits            options = 0);

    static IOHIDEvent *     digitizerEvent(
                                        UInt64                          timeStamp,
                                        UInt32                          transducerID,
                                        IOHIDDigitizerTransducerType    type,
                                        bool                            inRange,
                                        UInt32                          buttonState,
                                        IOFixed                         x,
                                        IOFixed                         y,
                                        IOFixed                         z           = 0,
                                        IOFixed                         tipPressure = 0,
                                        IOFixed                         auxPressure = 0,
                                        IOFixed                         twist       = 0,
                                        IOOptionBits                    options     = 0 );
    
    static IOHIDEvent *     digitizerEventWithTiltOrientation(
                                        UInt64                          timeStamp,
                                        UInt32                          transducerID,
                                        IOHIDDigitizerTransducerType    type,
                                        bool                            inRange,
                                        UInt32                          buttonState,
                                        IOFixed                         x,
                                        IOFixed                         y,
                                        IOFixed                         z               = 0,
                                        IOFixed                         tipPressure     = 0,
                                        IOFixed                         auxPressure     = 0,
                                        IOFixed                         twist           = 0,
                                        IOFixed                         xTilt           = 0,
                                        IOFixed                         yTilt           = 0,
                                        IOOptionBits                    options         = 0 );

    static IOHIDEvent *     digitizerEventWithPolarOrientation(
                                        UInt64                          timeStamp,
                                        UInt32                          transducerID,
                                        IOHIDDigitizerTransducerType    type,
                                        bool                            inRange,
                                        UInt32                          buttonState,
                                        IOFixed                         x,
                                        IOFixed                         y,
                                        IOFixed                         z               = 0,
                                        IOFixed                         tipPressure     = 0,
                                        IOFixed                         auxPressure     = 0,
                                        IOFixed                         twist           = 0,
                                        IOFixed                         altitude        = 0,
                                        IOFixed                         azimuth         = 0,
                                        IOOptionBits                    options         = 0 );
    
    static IOHIDEvent *     digitizerEventWithPolarOrientation(
                                        UInt64                          timeStamp,
                                        UInt32                          transducerID,
                                        IOHIDDigitizerTransducerType    type,
                                        bool                            inRange,
                                        UInt32                          buttonState,
                                        IOFixed                         x,
                                        IOFixed                         y,
                                        IOFixed                         z               = 0,
                                        IOFixed                         tipPressure     = 0,
                                        IOFixed                         auxPressure     = 0,
                                        IOFixed                         twist           = 0,
                                        IOFixed                         altitude        = 0,
                                        IOFixed                         azimuth         = 0,
                                        IOFixed                         quality         = 0,
                                        IOFixed                         density         = 0,
                                        IOOptionBits                    options         = 0 );

    static IOHIDEvent *     digitizerEventWithPolarOrientation(
                                        UInt64                          timeStamp,
                                        UInt32                          transducerID,
                                        IOHIDDigitizerTransducerType    type,
                                        bool                            inRange,
                                        UInt32                          buttonState,
                                        IOFixed                         x,
                                        IOFixed                         y,
                                        IOFixed                         z               = 0,
                                        IOFixed                         tipPressure     = 0,
                                        IOFixed                         auxPressure     = 0,
                                        IOFixed                         twist           = 0,
                                        IOFixed                         altitude        = 0,
                                        IOFixed                         azimuth         = 0,
                                        IOFixed                         quality         = 0,
                                        IOFixed                         density         = 0,
                                        IOFixed                         majorRadius     = 6<<16,
                                        IOFixed                         minorRadius     = 6<<16,
                                        IOOptionBits                    options         = 0 );

    static IOHIDEvent *     digitizerEventWithQualityOrientation(
                                        UInt64                          timeStamp,
                                        UInt32                          transducerID,
                                        IOHIDDigitizerTransducerType    type,
                                        bool                            inRange,
                                        UInt32                          buttonState,
                                        IOFixed                         x,
                                        IOFixed                         y,
                                        IOFixed                         z               = 0,
                                        IOFixed                         tipPressure     = 0,
                                        IOFixed                         auxPressure     = 0,
                                        IOFixed                         twist           = 0,
                                        IOFixed                         quality         = 0,
                                        IOFixed                         density         = 0,
                                        IOFixed                         irregularity    = 0,
                                        IOFixed                         majorRadius     = 6<<16,
                                        IOFixed                         minorRadius     = 6<<16,
                                        IOOptionBits                    options         = 0 );

    static IOHIDEvent *     powerEvent(
                                        UInt64                  timeStamp,
                                        int64_t                 measurement,
                                        IOHIDPowerType          powerType,
                                        IOHIDPowerSubType       powerSubType    = 0,
                                        IOOptionBits            options         = 0);

    static IOHIDEvent *     vendorDefinedEvent(
                                        UInt64                  timeStamp,
                                        UInt32                  usagePage,
                                        UInt32                  usage,
                                        UInt32                  version,
                                        UInt8 *                 data,
                                        UInt32                  length,
                                        IOOptionBits            options = 0);

    static IOHIDEvent *     biometricEvent(UInt64 timeStamp, IOFixed level, IOHIDBiometricEventType eventType, IOOptionBits options=0);
    
    static IOHIDEvent *     biometricEvent(UInt64 timeStamp, IOFixed level, IOHIDBiometricEventType eventType, UInt32 usagePage, UInt32 usage, UInt8 tapCount, IOOptionBits options=0);
    
    static IOHIDEvent *     atmosphericPressureEvent(UInt64 timeStamp, IOFixed level, UInt32 sequence=0, IOOptionBits options=0);

    static IOHIDEvent *     unicodeEvent(UInt64 timeStamp, UInt8 * payload, UInt32 length, IOHIDUnicodeEncodingType encoding, IOFixed quality, IOOptionBits options);

    static IOHIDEvent *     standardGameControllerEvent(
                                        UInt64                          timeStamp,
                                        IOFixed                         dpadUp,
                                        IOFixed                         dpadDown,
                                        IOFixed                         dpadLeft,
                                        IOFixed                         dpadRight,
                                        IOFixed                         faceX,
                                        IOFixed                         faceY,
                                        IOFixed                         faceA,
                                        IOFixed                         faceB,
                                        IOFixed                         shoulderL,
                                        IOFixed                         shoulderR,
                                        IOOptionBits                    options = 0);

    static IOHIDEvent *     extendedGameControllerEvent(
                                        UInt64                          timeStamp,
                                        IOFixed                         dpadUp,
                                        IOFixed                         dpadDown,
                                        IOFixed                         dpadLeft,
                                        IOFixed                         dpadRight,
                                        IOFixed                         faceX,
                                        IOFixed                         faceY,
                                        IOFixed                         faceA,
                                        IOFixed                         faceB,
                                        IOFixed                         shoulderL1,
                                        IOFixed                         shoulderR1,
                                        IOFixed                         shoulderL2,
                                        IOFixed                         shoulderR2,
                                        IOFixed                         joystickX,
                                        IOFixed                         joystickY,
                                        IOFixed                         joystickZ,
                                        IOFixed                         joystickRz,
                                        IOOptionBits                    options = 0);

    static IOHIDEvent *     extendedGameControllerEventWithOptionalButtons(
                                        UInt64                          timeStamp,
                                        IOFixed                         dpadUp,
                                        IOFixed                         dpadDown,
                                        IOFixed                         dpadLeft,
                                        IOFixed                         dpadRight,
                                        IOFixed                         faceX,
                                        IOFixed                         faceY,
                                        IOFixed                         faceA,
                                        IOFixed                         faceB,
                                        IOFixed                         shoulderL1,
                                        IOFixed                         shoulderR1,
                                        IOFixed                         shoulderL2,
                                        IOFixed                         shoulderR2,
                                        IOFixed                         joystickX,
                                        IOFixed                         joystickY,
                                        IOFixed                         joystickZ,
                                        IOFixed                         joystickRz,
                                        bool                            thumbstickButtonLeft,
                                        bool                            thumbstickButtonRight,
                                        IOFixed                         shoulderL4,
                                        IOFixed                         shoulderR4,
                                        IOFixed                         bottomM1,
                                        IOFixed                         bottomM2,
                                        IOFixed                         bottomM3,
                                        IOFixed                         bottomM4,
                                        IOOptionBits                    options = 0);

    
    static IOHIDEvent *     orientationEvent(UInt64 timeStamp, UInt32 orientationType, IOOptionBits options = 0);

    static IOHIDEvent *     humidityEvent(UInt64 timeStamp, IOFixed rh, UInt32 sequence=0, IOOptionBits options=0);

    static IOHIDEvent *     brightnessEvent(UInt64 timeStamp, IOFixed currentBrightness, IOFixed targetBrightness, UInt64 transitionTime, IOOptionBits options = 0);

    static IOHIDEvent *     genericGestureEvent(UInt64 timeStamp, IOHIDGenericGestureType gestureType, IOOptionBits options = 0);

    static IOHIDEvent * heartRateEvent(UInt64 timeStamp, UInt32 heartRate, IOOptionBits options = 0);
    
    virtual void            appendChild(IOHIDEvent *childEvent);
    OSArray*                getChildren();

    virtual AbsoluteTime    getTimeStamp();
    virtual void            setTimeStamp(AbsoluteTime timeStamp);
    
    virtual IOHIDEventType  getType();
    virtual void            setType(IOHIDEventType type);
    
    virtual IOHIDEventPhaseBits  getPhase();
    virtual void            setPhase(IOHIDEventPhaseBits phase);
    
    virtual IOHIDEvent *    getEvent(IOHIDEventType type, IOOptionBits options = 0);
    
    virtual SInt32          getIntegerValue(
                                        IOHIDEventField key, IOOptionBits options = 0);
    virtual void            setIntegerValue(    
                                        IOHIDEventField key, SInt32 value, IOOptionBits options = 0);
                                        
    virtual IOFixed         getFixedValue(IOHIDEventField key, IOOptionBits options = 0);
    virtual void            setFixedValue(IOHIDEventField key, IOFixed value, IOOptionBits options = 0);
    
    virtual UInt8 *         getDataValue(IOHIDEventField key, IOOptionBits options = 0);

    virtual void            free(void) APPLE_KEXT_OVERRIDE;
    
    virtual size_t          getLength(); 
    virtual IOByteCount     readBytes(void *bytes, IOByteCount withLength);
    
    // Note: this function only serializes one level of child events, use readBytes for nested children.
    virtual OSData          *createBytes();
    
    virtual void            setSenderID(uint64_t senderID);
    
    virtual uint64_t        getLatency(uint32_t scaleFactor);

    virtual IOHIDDouble     getDoubleValue( IOHIDEventField  key,  IOOptionBits  options);
    virtual void            setDoubleValue( IOHIDEventField  key, IOHIDDouble value, IOOptionBits  options);

    virtual UInt64          getTimeStampOfType(IOHIDEventTimestampType type);

    virtual void            setTimeStampOfType(UInt64 timeStamp, IOHIDEventTimestampType type);
    
    inline  IOOptionBits    getOptions() { return _options; };

};

#endif /* _IOKIT_IOHIDEVENT_H */
