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
#include <AssertMacros.h>
#if KERNEL
#include <IOKit/IOLib.h>
#endif /*KERNEL*/
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#include "IOHIDEventTypes.h"
#pragma clang diagnostic pop
#include "IOHIDEvent.h"
#include "IOHIDEventData.h"
#include "IOHIDUsageTables.h"
#include "IOHIDPrivateKeys.h"
#include <math.h>
#include <os/overflow.h>

#include "ev_keymap.h"

#define EXTERNAL ((unsigned int) -1)

#define super OSObject
#define COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")

OSDefineMetaClassAndStructors(IOHIDEvent, OSObject)

//==============================================================================
// IOHIDEvent::initWithCapacity
//==============================================================================
bool IOHIDEvent::initWithCapacity(IOByteCount capacity)
{
    if (!super::init())
        return false;
    
    if (capacity >= UINT32_MAX)
        return false;

    if (_data && (!capacity || _capacity < capacity) ) {
        // clean out old data's storage if it isn't big enough
        IOFreeData(_data, _capacity);
        _data = NULL;
    }

    _capacity = capacity;

    if ( !_capacity )
        return false;

    if ( !_data && !(_data = (IOHIDEventData *) IOMallocZeroData(_capacity)))
        return false;

    _data->size = (uint32_t)_capacity;
    _children = NULL;
    (void)_parent;
    
    return true;
}

//==============================================================================
// IOHIDEvent::initWithType
//==============================================================================
bool IOHIDEvent::initWithType(IOHIDEventType type, IOByteCount additionalCapacity)
{
    size_t capacity = 0;

    IOHIDEventGetSize(type,capacity);

    if ( !initWithCapacity((IOByteCount)(capacity+additionalCapacity ) ) )
        return false;

    _data->type = type;
    _typeMask   = IOHIDEventTypeMask(type);

    return true;
}

//==============================================================================
// IOHIDEvent::initWithTypeTimeStamp
//==============================================================================
bool IOHIDEvent::initWithTypeTimeStamp(IOHIDEventType type, UInt64 timeStamp, IOOptionBits options, IOByteCount additionalCapacity)
{
    if ( !initWithType(type, additionalCapacity))
        return false;

    _timeStamp  = timeStamp;
    _options    = options;

    _data->options = options;

    return true;
}

//==============================================================================
// IOHIDEvent::free
//==============================================================================
void IOHIDEvent::free()
{
    if (_capacity != EXTERNAL && _data && _capacity) {
        IOFreeData(_data, _capacity);
        _data = NULL;
        _capacity = 0;
    }

    if ( _children ) {
        // We should probably iterate over each child to clear the parent
        _children->release();
        _children = NULL;
    }
    super::free();
}

//==============================================================================
// IOHIDEvent::getTimeStamp
//==============================================================================
AbsoluteTime IOHIDEvent::getTimeStamp()
{
#if KERNEL
    return _options & kIOHIDEventOptionContinuousTime ? continuoustime_to_absolutetime(_timeStamp) : _timeStamp;
#else
    // TODO: The build is failing for IOHIDFamily_test, the conversion API is not available in user space.
    // TODO: Is building for user space just for the TestHIDIOHIDEventSerialization unit test? If so, leave this. If not, fix this to be correct.
    return _timeStamp;
#endif
}

//==============================================================================
// IOHIDEvent::setTimeStamp
//==============================================================================
void IOHIDEvent::setTimeStamp( AbsoluteTime timeStamp )
{
    _timeStamp = timeStamp;
    _options &= ~kIOHIDEventOptionContinuousTime;
}

//==============================================================================
// IOHIDEvent::getTimeStampOfType
//==============================================================================
UInt64 IOHIDEvent::getTimeStampOfType( IOHIDEventTimestampType type )
{
    UInt64 time = _timeStamp;
#if KERNEL
    if (type == kIOHIDEventTimestampTypeAbsolute) {
        time = _options & kIOHIDEventOptionContinuousTime ? continuoustime_to_absolutetime(_timeStamp) : _timeStamp;
    } else if (type == kIOHIDEventTimestampTypeContinuous) {
        time = _options & kIOHIDEventOptionContinuousTime ? _timeStamp : absolutetime_to_continuoustime(_timeStamp);
    }
#endif
    return time;
}

//==============================================================================
// IOHIDEvent::setTimeStampOfType
//==============================================================================
void IOHIDEvent::setTimeStampOfType( UInt64                  timeStamp,
                                     IOHIDEventTimestampType type)
{
    _timeStamp = timeStamp;
    if (type == kIOHIDEventTimestampTypeAbsolute) {
        _options &= ~kIOHIDEventOptionContinuousTime;
    } else if (type == kIOHIDEventTimestampTypeContinuous) {
        _options |= kIOHIDEventOptionContinuousTime;
    }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::withType
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::withType(      IOHIDEventType          type,
                                        IOOptionBits            options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithType(type)) {
        me->release();
        return 0;
    }

    me->_options = options;

    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::keyboardEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::keyboardEvent( UInt64                  timeStamp,
                                        UInt32                  usagePage,
                                        UInt32                  usage,
                                        Boolean                 down,
                                        IOOptionBits            options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeKeyboard, timeStamp, options | kIOHIDEventOptionIsAbsolute)) {
        me->release();
        return 0;
    }

    IOHIDKeyboardEventData * keyboard = (IOHIDKeyboardEventData *)me->_data;
    keyboard->usagePage = usagePage;
    keyboard->usage = usage;
    keyboard->down = down;
    keyboard->pressCount = 1;

    return me;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::keyboardEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::keyboardEvent( UInt64                  timeStamp,
                                        UInt32                  usagePage,
                                        UInt32                  usage,
                                        Boolean                 down,
                                        UInt8                   pressCount,
                                        Boolean                 longPress,
                                        UInt8                   clickSpeed,
                                        IOOptionBits            options)
{
    IOHIDEvent * me = NULL;
    
    me = IOHIDEvent::keyboardEvent(timeStamp, usagePage, usage, down, options);
    
    if (me) {
        IOHIDKeyboardEventData * keyboard = (IOHIDKeyboardEventData *)me->_data;
        keyboard->pressCount = pressCount;
        keyboard->longPress = longPress;
        keyboard->clickSpeed = clickSpeed;
    }
    
    return me;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::unicodeEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::unicodeEvent(UInt64 timeStamp, UInt8 * payload, UInt32 length, IOHIDUnicodeEncodingType encoding, IOFixed quality, IOOptionBits options)
{
    IOHIDEvent *            event   = new IOHIDEvent;
    IOHIDEvent *            result  = NULL;
    IOHIDUnicodeEventData * data    = NULL;
    
    require(payload && length, exit);
    require(event, exit);
    require(event->initWithTypeTimeStamp(kIOHIDEventTypeUnicode, timeStamp, options|kIOHIDEventOptionIsAbsolute, length), exit);
    
    data = (IOHIDUnicodeEventData *)event->_data;
    
    data->encoding  = encoding;
    data->quality   = quality;
    data->length    = length;
    bcopy(payload, data->payload, length);
    
    result = event;
    result->retain();

exit:
    if ( event )
        event->release();
    
    return result;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::_axisEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::_axisEvent(    IOHIDEventType          type,
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOOptionBits            options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithTypeTimeStamp(type, timeStamp, options)) {
        me->release();
        return 0;
    }

    IOHIDAxisEventData * event = (IOHIDAxisEventData *)me->_data;
    event->position.x = x;
    event->position.y = y;
    event->position.z = z;

    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::_motionEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::_motionEvent ( IOHIDEventType          type,
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        uint32_t                motionType,
                                        uint32_t                motionSubType,
                                        UInt32                  sequence,
                                        IOOptionBits            options)
{
    IOHIDEvent *            event;
    IOHIDMotionEventData *  data;

    event = IOHIDEvent::_axisEvent( type,
                                    timeStamp,
                                    x,
                                    y,
                                    z,
                                    options);

    if ( event ) {
        data = (IOHIDMotionEventData *)event->_data;
        data->motionType = motionType;
        data->motionSubType = motionSubType;
        data->motionSequence = sequence;
    }

    return event;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::translationEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::translationEvent(
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOOptionBits            options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeTranslation, timeStamp, options)) {
        me->release();
        return 0;
    }

    IOHIDTranslationEventData * event = (IOHIDTranslationEventData *)me->_data;
#if TARGET_OS_VISION
    event->position.x = CAST_FIXED_TO_DOUBLE(x);
    event->position.y = CAST_FIXED_TO_DOUBLE(y);
    event->position.z = CAST_FIXED_TO_DOUBLE(z);
#else
    event->position.x = x;
    event->position.y = y;
    event->position.z = z;
#endif
    
    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::scrollEvet
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::scrollEventWithFixed(
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOOptionBits            options)
{
    return IOHIDEvent::_axisEvent(      kIOHIDEventTypeScroll,
                                        timeStamp,
                                        x,
                                        y,
                                        z,
                                        options);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::scrollEvet
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::scrollEvent(
                                        UInt64                  timeStamp,
                                        SInt32                  x,
                                        SInt32                  y,
                                        SInt32                  z,
                                        IOOptionBits            options)
{
    return IOHIDEvent::_axisEvent(      kIOHIDEventTypeScroll,
                                        timeStamp,
                                        x<<16,
                                        y<<16,
                                        z<<16,
                                        options);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::zoomEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::zoomEvent(
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOOptionBits            options)
{
    return IOHIDEvent::_axisEvent(      kIOHIDEventTypeZoom,
                                        timeStamp,
                                        x,
                                        y,
                                        z,
                                        options);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::accelerometerEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::accelerometerEvent(
                                    UInt64                  timeStamp,
                                    IOFixed                 x,
                                    IOFixed                 y,
                                    IOFixed                 z,
                                    IOHIDMotionType         type,
                                    IOHIDMotionPath         subType,
                                    UInt32                  sequence,
                                    IOOptionBits            options)
{
    return IOHIDEvent::_motionEvent( kIOHIDEventTypeAccelerometer,
                                    timeStamp,
                                    x,
                                    y,
                                    z,
                                    type,
                                    subType,
                                    sequence,
                                    options);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::gyroEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::gyroEvent(
                                    UInt64                  timeStamp,
                                    IOFixed                 x,
                                    IOFixed                 y,
                                    IOFixed                 z,
                                    IOHIDMotionType         type,
                                    IOHIDMotionPath         subType,
                                    UInt32                  sequence,
                                    IOOptionBits            options)
{
    return IOHIDEvent::_motionEvent( kIOHIDEventTypeGyro,
                                    timeStamp,
                                    x,
                                    y,
                                    z,
                                    type,
                                    subType,
                                    sequence,
                                    options);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::compassEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::compassEvent(
                                   UInt64                   timeStamp,
                                   IOFixed                  x,
                                   IOFixed                  y,
                                   IOFixed                  z,
                                   IOHIDMotionType          type,
                                   IOHIDMotionPath          subType,
                                   UInt32                   sequence,
                                   IOOptionBits             options)
{
    return IOHIDEvent::_motionEvent( kIOHIDEventTypeCompass,
                                    timeStamp,
                                    x,
                                    y,
                                    z,
                                    type,
                                    subType,
                                    sequence,
                                    options);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::ambientLightSensorEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::ambientLightSensorEvent(
                                        UInt64                  timeStamp,
                                        UInt32                  level,
                                        UInt32                  channel0,
                                        UInt32                  channel1,
										UInt32                  channel2,
										UInt32                  channel3,
                                        IOOptionBits            options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeAmbientLightSensor, timeStamp, options)) {
        me->release();
        return 0;
    }

    IOHIDAmbientLightSensorEventData * event = (IOHIDAmbientLightSensorEventData *)me->_data;
    event->level = level;
    event->ch0 = channel0;
    event->ch1 = channel1;
    event->ch2 = channel2;
    event->ch3 = channel3;

    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::ambientLightSensorEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::ambientLightSensorEvent(
                                        UInt64                  timeStamp,
                                        UInt32                  level,
                                        IOHIDEventColorSpace    colorSpace,
                                        IOHIDDouble             colorComponent0,
                                        IOHIDDouble             colorComponent1,
                                        IOHIDDouble             colorComponent2,
                                        IOOptionBits            options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeAmbientLightSensor, timeStamp, options)) {
        me->release();
        return 0;
    }

    IOHIDAmbientLightSensorEventData * event = (IOHIDAmbientLightSensorEventData *)me->_data;
    event->level = level;
    event->colorSpace = colorSpace;
    event->colorComponent0 = colorComponent0;
    event->colorComponent1 = colorComponent1;
    event->colorComponent2 = colorComponent2;
    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::proximityEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::proximityEvent (
                                        UInt64      				timeStamp,
                                        IOHIDProximityDetectionMask	mask,
										UInt32						level,
                                        IOOptionBits				options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeProximity, timeStamp, options)) {
        me->release();
        return 0;
    }

    IOHIDProximityEventData * event = (IOHIDProximityEventData *)me->_data;
    event->detectionMask	= mask;
    event->proximityType    = kIOHIDProximityProximityTypeLevel;
    event->proximity.level  = level;

    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::proximityEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::proximityEventWithProbability(
                                            UInt64                      timeStamp,
                                            IOHIDProximityDetectionMask mask,
                                            UInt32                      probability,
                                            IOOptionBits                options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeProximity, timeStamp, options)) {
        me->release();
        return 0;
    }

    IOHIDProximityEventData * event = (IOHIDProximityEventData *)me->_data;
    event->detectionMask    = mask;
    event->proximityType    = kIOHIDProximityProximityTypeProbability;
    event->proximity.probability  = probability;

    return me;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::temperatureEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::temperatureEvent(
                                        UInt64                  timeStamp,
                                        IOFixed                 temperature,
                                        IOOptionBits            options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeTemperature, timeStamp, options)) {
        me->release();
        return 0;
    }

    IOHIDTemperatureEventData * event = (IOHIDTemperatureEventData *)me->_data;
    event->level = temperature;

    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::buttonEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::buttonEvent(
                                        UInt64                  timeStamp,
                                        UInt32                  mask,
                                        UInt8                   number,
                                        bool                    state,
                                        IOOptionBits            options)
{
    IOHIDEvent *            event   = new IOHIDEvent;
    IOHIDButtonEventData *  data    = NULL;
    
    require(event, exit);
    
    require(event->initWithTypeTimeStamp(kIOHIDEventTypeButton, timeStamp, options | kIOHIDEventOptionIsAbsolute), exit);
    
    data = (IOHIDButtonEventData *)event->_data;
    require(data, exit);
        
    data->mask          = mask;
    data->number        = number;
    data->state         = state;
    data->pressure      = state ? 0x10000 : 0x00000;
    data->clickCount    = 1;

exit:
    if ( !data ) {
        if ( event ) {
            event->release();
            event = NULL;
        }
    }
    return event;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::buttonEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::buttonEvent(
                                        UInt64                  timeStamp,
                                        UInt32                  mask,
                                        UInt8                   number,
                                        IOFixed                 pressure,
                                        IOOptionBits            options)
{
    IOHIDEvent *            event   = NULL;
    IOHIDButtonEventData *  data    = NULL;
    bool                    state   = (pressure>>16) & 0x1;

    event = buttonEvent(timeStamp, mask, number, state, options);
    require(event, exit);
    
    data = (IOHIDButtonEventData *)event->_data;
    require(data, exit);
    
    data->pressure = pressure;

exit:
    return event;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::relativePointerEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::absolutePointerEvent(
                                              UInt64                  timeStamp,
                                              IOFixed                 x,
                                              IOFixed                 y,
                                              IOFixed                 z,
                                              UInt32                  buttonState,
                                              UInt32                  oldButtonState,
                                              IOOptionBits            options)
{
    IOHIDEvent *            event   = NULL;
    IOHIDPointerEventData * data    = NULL;
    UInt32                  index, delta;
    
    event = new IOHIDEvent;
    require(event, exit);
    
    require(event->initWithTypeTimeStamp(kIOHIDEventTypePointer, timeStamp, options | kIOHIDEventOptionIsAbsolute), exit);
    
    data = (IOHIDPointerEventData *)event->_data;
    require(data, exit);
    
#if TARGET_OS_VISION
    data->position.x = CAST_FIXED_TO_DOUBLE(x);
    data->position.y = CAST_FIXED_TO_DOUBLE(y);
    data->position.z = CAST_FIXED_TO_DOUBLE(z);
#else
    data->position.x = x;
    data->position.y = y;
    data->position.z = z;
#endif
    data->button.mask = buttonState;
    
    
    delta = buttonState ^ oldButtonState;
    for ( index=0; delta; delta>>=1, buttonState>>=1) {
        IOHIDEvent * subEvent;
        
        if ( (delta & 0x1) == 0 )
            continue;
        
        subEvent = buttonEvent(timeStamp, data->button.mask, index+1, (bool)(buttonState&0x1));
        if ( !subEvent )
            continue;
        
        event->appendChild(subEvent);
        
        subEvent->release();
    }
    
exit:
    if ( !data ) {
        if ( event ) {
            event->release();
            event = NULL;
        }
    }
    return event;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::relativePointerEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::relativePointerEventWithFixed(
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        UInt32                  buttonState,
                                        UInt32                  oldButtonState,
                                        IOOptionBits            options)

{

    IOHIDEvent *            event   = NULL;
    IOHIDPointerEventData * data    = NULL;
    UInt32                  index, delta;
    
    event = new IOHIDEvent;
    require(event, exit);

    require(event->initWithTypeTimeStamp(kIOHIDEventTypePointer, timeStamp, options), exit);

    data = (IOHIDPointerEventData *)event->_data;
    require(data, exit);

#if TARGET_OS_VISION
    data->position.x = CAST_FIXED_TO_DOUBLE(x);
    data->position.y = CAST_FIXED_TO_DOUBLE(y);
    data->position.z = CAST_FIXED_TO_DOUBLE(z);
#else
    data->position.x = x;
    data->position.y = y;
    data->position.z = z;
#endif
    data->button.mask = buttonState;
    
    
    delta = buttonState ^ oldButtonState;
    for ( index=0; delta; delta>>=1, buttonState>>=1) {
        IOHIDEvent * subEvent;
        
        if ( (delta & 0x1) == 0 )
            continue;
        
        subEvent = buttonEvent(timeStamp, data->button.mask, index+1, (bool)(buttonState&0x1));
        if ( !subEvent )
            continue;
        
        event->appendChild(subEvent);
        
        subEvent->release();
    }

exit:
    if ( !data ) {
        if ( event ) {
            event->release();
            event = NULL;
        }
    }
    return event;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::relativePointerEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::relativePointerEvent(
                                        UInt64                  timeStamp,
                                        SInt32                  x,
                                        SInt32                  y,
                                        SInt32                  z,
                                        UInt32                  buttonState,
                                        UInt32                  oldButtonState,
                                        IOOptionBits            options)
{
     return relativePointerEventWithFixed (timeStamp, x << 16, y << 16, z << 16, buttonState, oldButtonState, options);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::multiAxisPointerEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::multiAxisPointerEvent(
                                        UInt64                  timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOFixed                 rX,
                                        IOFixed                 rY,
                                        IOFixed                 rZ,
                                        UInt32                  buttonState,
                                        UInt32                  oldButtonState,
                                        IOOptionBits            options)
{
    IOHIDEvent *                        event   = NULL;
    IOHIDMultiAxisPointerEventData *    data    = NULL;
    UInt32                              index, delta;
    
    event = new IOHIDEvent;
    require(event, exit);
    
    require(event->initWithTypeTimeStamp(kIOHIDEventTypeMultiAxisPointer, timeStamp, options | kIOHIDEventOptionIsAbsolute | kIOHIDEventOptionIsCenterOrigin), exit);
    
    data = (IOHIDMultiAxisPointerEventData *)event->_data;
    require(data, exit);

    data->position.x    = x;
    data->position.y    = y;
    data->position.z    = z;
    data->rotation.x    = rX;
    data->rotation.y    = rY;
    data->rotation.z    = rZ;
    data->button.mask   = buttonState;
    
    delta = buttonState ^ oldButtonState;
    for ( index=0; delta; delta>>=1, buttonState>>=1) {
        IOHIDEvent * subEvent;
        
        if ( (delta & 0x1) == 0 )
            continue;
        
        subEvent = buttonEvent(timeStamp, data->button.mask, index+1, (bool)(buttonState&0x1));
        if ( !subEvent )
            continue;
        
        event->appendChild(subEvent);
        
        subEvent->release();
    }
    
exit:
    if ( !data ) {
        if ( event ) {
            event->release();
            event = NULL;
        }
    }
    return event;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::digitizerEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::digitizerEvent(
                                        UInt64                          timeStamp,
                                        UInt32                          transducerID,
                                        IOHIDDigitizerTransducerType    type,
                                        bool                            inRange,
                                        UInt32                          buttonState,
                                        IOFixed                         x,
                                        IOFixed                         y,
                                        IOFixed                         z,
                                        IOFixed                         tipPressure,
                                        IOFixed                         auxPressure,
                                        IOFixed                         twist,
                                        IOOptionBits                    options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeDigitizer, timeStamp, options | kIOHIDEventOptionIsAbsolute)) {
        me->release();
        return NULL;
    }

    IOHIDDigitizerEventData *event = (IOHIDDigitizerEventData *)me->_data;

    if ( inRange ) {
        event->options.range = 1;
    }

    event->eventMask        = 0;       // todo:
    event->transducerIndex  = transducerID; // Multitouch uses this as a path ID
    event->transducerType   = type;
    event->buttonMask       = buttonState;
#if TARGET_OS_VISION
    event->position.x       = CAST_FIXED_TO_DOUBLE(x);
    event->position.y       = CAST_FIXED_TO_DOUBLE(y);
    event->position.z       = CAST_FIXED_TO_DOUBLE(z);
    event->pressure         = CAST_FIXED_TO_DOUBLE(tipPressure);
    event->auxPressure      = CAST_FIXED_TO_DOUBLE(auxPressure);
    event->angle.twist      = CAST_FIXED_TO_DOUBLE(twist);

#else
    event->position.x       = x;
    event->position.y       = y;
    event->position.z       = z;
    event->pressure         = tipPressure;
    event->auxPressure      = auxPressure;
    event->angle.twist      = twist;
#endif

    // Let's assume no tip pressure means finger
    switch ( event->transducerType ) {
        case kIOHIDDigitizerTransducerTypeFinger:
            event->identity = 2;        // Multitouch interprets this as 'finger', hard code to 2 or index finger
            event->orientationType = kIOHIDDigitizerOrientationTypeQuality;
#if TARGET_OS_VISION
            event->orientation.quality.majorRadius = CAST_FIXED_TO_DOUBLE(5<<16);
            event->orientation.quality.minorRadius = CAST_FIXED_TO_DOUBLE(5<<16);
            event->orientation.polar.majorRadius = CAST_FIXED_TO_DOUBLE(5<<16);
            event->orientation.polar.minorRadius = CAST_FIXED_TO_DOUBLE(5<<16);
#else
            event->orientation.quality.majorRadius = 5<<16;
            event->orientation.quality.minorRadius = 5<<16;
            event->orientation.polar.majorRadius = 5<<16;
            event->orientation.polar.minorRadius = 5<<16;
#endif
            break;
        default:
#if TARGET_OS_VISION
            event->orientation.quality.majorRadius = CAST_FIXED_TO_DOUBLE(3<<16);
            event->orientation.quality.minorRadius = CAST_FIXED_TO_DOUBLE(3<<16);
            event->orientation.polar.majorRadius = CAST_FIXED_TO_DOUBLE(3<<16);
            event->orientation.polar.minorRadius = CAST_FIXED_TO_DOUBLE(3<<16);
#else
            event->orientation.quality.majorRadius = 3<<16;
            event->orientation.quality.minorRadius = 3<<16;
            event->orientation.polar.majorRadius = 3<<16;
            event->orientation.polar.minorRadius = 3<<16;
#endif
            break;
    }

    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::digitizerEventWithTiltOrientation
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::digitizerEventWithTiltOrientation(
                                        UInt64                          timeStamp,
                                        UInt32                          transducerID,
                                        IOHIDDigitizerTransducerType    type,
                                        bool                            inRange,
                                        UInt32                          buttonState,
                                        IOFixed                         x,
                                        IOFixed                         y,
                                        IOFixed                         z __unused,
                                        IOFixed                         tipPressure,
                                        IOFixed                         auxPressure,
                                        IOFixed                         twist,
                                        IOFixed                         xTilt,
                                        IOFixed                         yTilt __unused,
                                        IOOptionBits                    options)
{
    IOHIDEvent * me                 = NULL;
    IOHIDDigitizerEventData * event = NULL;

    me = IOHIDEvent::digitizerEvent(timeStamp, transducerID, type, inRange, buttonState, x, y, tipPressure, auxPressure, twist, options);
    require(me, exit);

    event = (IOHIDDigitizerEventData *)me->_data;
    require(event, exit);

    event->orientationType = kIOHIDDigitizerOrientationTypeTilt;
    
#if TARGET_OS_VISION
    event->orientation.tilt.x = CAST_FIXED_TO_DOUBLE(xTilt);
    event->orientation.tilt.y = CAST_FIXED_TO_DOUBLE(xTilt);
#else
    event->orientation.tilt.x = xTilt;
    event->orientation.tilt.y = xTilt;
#endif

exit:
    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::digitizerEventWithPolarOrientation
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::digitizerEventWithPolarOrientation(
                                        UInt64                          timeStamp,
                                        UInt32                          transducerID,
                                        IOHIDDigitizerTransducerType    type,
                                        bool                            inRange,
                                        UInt32                          buttonState,
                                        IOFixed                         x,
                                        IOFixed                         y,
                                        IOFixed                         z __unused,
                                        IOFixed                         tipPressure,
                                        IOFixed                         auxPressure,
                                        IOFixed                         twist,
                                        IOFixed                         altitude,
                                        IOFixed                         azimuth,
                                        IOOptionBits                    options)
{
    IOHIDEvent * me                 = NULL;
    IOHIDDigitizerEventData * event = NULL;

    me = IOHIDEvent::digitizerEvent(timeStamp, transducerID, type, inRange, buttonState, x, y, tipPressure, auxPressure, twist, options);
    require(me, exit);

    event = (IOHIDDigitizerEventData *)me->_data;
    require(event, exit);

    event->orientationType = kIOHIDDigitizerOrientationTypePolar;

#if TARGET_OS_VISION
    event->orientation.polar.altitude   = CAST_FIXED_TO_DOUBLE(altitude);
    event->orientation.polar.azimuth    = CAST_FIXED_TO_DOUBLE(azimuth);
#else
    event->orientation.polar.altitude   = altitude;
    event->orientation.polar.azimuth    = azimuth;
#endif
exit:
    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::digitizerEventWithPolarOrientation
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::digitizerEventWithPolarOrientation(
                                        UInt64                          timeStamp,
                                        UInt32                          transducerID,
                                        IOHIDDigitizerTransducerType    type,
                                        bool                            inRange,
                                        UInt32                          buttonState,
                                        IOFixed                         x,
                                        IOFixed                         y,
                                        IOFixed                         z,
                                        IOFixed                         tipPressure,
                                        IOFixed                         auxPressure,
                                        IOFixed                         twist,
                                        IOFixed                         altitude,
                                        IOFixed                         azimuth,
                                        IOFixed                         quality,
                                        IOFixed                         density,
                                        IOOptionBits                    options)
{
    IOHIDDigitizerEventData * event = NULL;

    IOHIDEvent * me = IOHIDEvent::digitizerEventWithPolarOrientation(timeStamp, transducerID, type, inRange, buttonState, x, y, z, tipPressure, auxPressure, twist, altitude, azimuth, options);
    require(me, exit);
 
    event = (IOHIDDigitizerEventData *)me->_data;
 
#if TARGET_OS_VISION
    event->orientation.polar.quality = CAST_FIXED_TO_DOUBLE(quality);
    event->orientation.polar.density = CAST_FIXED_TO_DOUBLE(density);
#else
    event->orientation.polar.quality = quality;
    event->orientation.polar.density = density;
#endif
    
exit:
    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::digitizerEventWithPolarOrientation
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::digitizerEventWithPolarOrientation(
                                        UInt64                          timeStamp,
                                        UInt32                          transducerID,
                                        IOHIDDigitizerTransducerType    type,
                                        bool                            inRange,
                                        UInt32                          buttonState,
                                        IOFixed                         x,
                                        IOFixed                         y,
                                        IOFixed                         z,
                                        IOFixed                         tipPressure,
                                        IOFixed                         auxPressure,
                                        IOFixed                         twist,
                                        IOFixed                         altitude,
                                        IOFixed                         azimuth,
                                        IOFixed                         quality,
                                        IOFixed                         density,
                                        IOFixed                         majorRadius,
                                        IOFixed                         minorRadius,
                                        IOOptionBits                    options)
{
    IOHIDDigitizerEventData * event = NULL;

    IOHIDEvent * me = IOHIDEvent::digitizerEventWithPolarOrientation(timeStamp, transducerID, type, inRange, buttonState, x, y, z, tipPressure, auxPressure, twist, altitude, azimuth, quality, density, options);
    require(me, exit);

    event = (IOHIDDigitizerEventData *)me->_data;
    
#if TARGET_OS_VISION
    event->orientation.polar.majorRadius = CAST_FIXED_TO_DOUBLE(majorRadius);
    event->orientation.polar.minorRadius = CAST_FIXED_TO_DOUBLE(minorRadius);
#else
    event->orientation.polar.majorRadius = majorRadius;
    event->orientation.polar.minorRadius = minorRadius;
#endif

exit:
    return me;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::digitizerEventWithQualityOrientation
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::digitizerEventWithQualityOrientation(
                                        UInt64                          timeStamp,
                                        UInt32                          transducerID,
                                        IOHIDDigitizerTransducerType    type,
                                        bool                            inRange,
                                        UInt32                          buttonState,
                                        IOFixed                         x,
                                        IOFixed                         y,
                                        IOFixed                         z __unused,
                                        IOFixed                         tipPressure,
                                        IOFixed                         auxPressure,
                                        IOFixed                         twist,
                                        IOFixed                         quality,
                                        IOFixed                         density,
                                        IOFixed                         irregularity,
                                        IOFixed                         majorRadius,
                                        IOFixed                         minorRadius,
                                        IOOptionBits                    options)
{
    IOHIDEvent * me                 = NULL;
    IOHIDDigitizerEventData * event = NULL;

    me = IOHIDEvent::digitizerEvent(timeStamp, transducerID, type, inRange, buttonState, x, y, tipPressure, auxPressure, twist, options);
    require(me, exit);

    event = (IOHIDDigitizerEventData *)me->_data;
    require(event, exit);

    event->orientationType = kIOHIDDigitizerOrientationTypeQuality;
    
#if TARGET_OS_VISION
    event->orientation.quality.quality          = CAST_FIXED_TO_DOUBLE(quality);
    event->orientation.quality.density          = CAST_FIXED_TO_DOUBLE(density);
    event->orientation.quality.irregularity     = CAST_FIXED_TO_DOUBLE(irregularity);
    event->orientation.quality.majorRadius      = CAST_FIXED_TO_DOUBLE(majorRadius);
    event->orientation.quality.minorRadius      = CAST_FIXED_TO_DOUBLE(minorRadius);
#else
    event->orientation.quality.quality          = quality;
    event->orientation.quality.density          = density;
    event->orientation.quality.irregularity     = irregularity;
    event->orientation.quality.majorRadius      = majorRadius;
    event->orientation.quality.minorRadius      = minorRadius;
#endif

exit:
    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::powerEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::powerEvent(
                                   UInt64                  timeStamp,
                                   int64_t                 measurement,
                                   IOHIDPowerType          powerType,
                                   IOHIDPowerSubType       powerSubType,
                                   IOOptionBits            options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypePower, timeStamp, options)) {
        me->release();
        return 0;
    }

    IOHIDPowerEventData *event = (IOHIDPowerEventData *)me->_data;

    event->measurement = (IOFixed)measurement;
    event->powerType = powerType;
    event->powerSubType = powerSubType;

    return me;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::vendorDefinedEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::vendorDefinedEvent(
                                           UInt64                  timeStamp,
                                           UInt32                  usagePage,
                                           UInt32                  usage,
                                           UInt32                  version,
                                           UInt8 *                 data,
                                           UInt32                  length,
                                           IOOptionBits            options)
{
    UInt32      dataLength  = length;
    IOHIDEvent * me         = new IOHIDEvent;
 
    //dataLength = ALIGNED_DATA_SIZE(dataLength, sizeof(uint32_t));
  
    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeVendorDefined, timeStamp, options, dataLength)) {
        me->release();
        return 0;
    }

    IOHIDVendorDefinedEventData *event = (IOHIDVendorDefinedEventData *)me->_data;

    event->usagePage    = usagePage;
    event->usage        = usage;
    event->version      = version;
    event->length       = dataLength;

    bcopy(data, event->data, length);

    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::biometricEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::biometricEvent(UInt64 timeStamp, IOFixed level, IOHIDBiometricEventType eventType, IOOptionBits options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeBiometric, timeStamp, options | kIOHIDEventOptionIsAbsolute)) {
        me->release();
        return 0;
    }

    IOHIDBiometricEventData *event = (IOHIDBiometricEventData *)me->_data;

    event->eventType    = eventType;
    event->level        = level;
    event->tapCount     = 0;
    event->usagePage    = 0;
    event->usage        = 0;

    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::biometricEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::biometricEvent(UInt64 timeStamp, IOFixed level, IOHIDBiometricEventType eventType, UInt32 usagePage, UInt32 usage, UInt8 tapCount, IOOptionBits options)
{
    IOHIDEvent *me = IOHIDEvent::biometricEvent(timeStamp, level, eventType, options);
    
    if (!me)
        return 0;
    
    IOHIDBiometricEventData *event = (IOHIDBiometricEventData *)me->_data;
    
    event->usagePage = usagePage;
    event->usage = usage;
    event->tapCount = tapCount;
    
    return me;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::atmosphericPressureEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::atmosphericPressureEvent(UInt64 timeStamp, IOFixed level, UInt32 sequence, IOOptionBits options)
{
    IOHIDEvent *me = new IOHIDEvent;
    
    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeAtmosphericPressure, timeStamp, options | kIOHIDEventOptionIsAbsolute)) {
        me->release();
        return 0;
    }
    
    IOHIDAtmosphericPressureEventData *event = (IOHIDAtmosphericPressureEventData *)me->_data;
    
    event->level = level;
    event->sequence = sequence;
    
    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::standardGameControllerEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::standardGameControllerEvent(UInt64                          timeStamp,
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
                                                     IOOptionBits                    options)
{
    IOHIDEvent *me = new IOHIDEvent;
    IOHIDGameControllerEventData *event = NULL;
    
    require(me, exit);
    
    require_action(me->initWithTypeTimeStamp(kIOHIDEventTypeGameController, timeStamp, options | kIOHIDEventOptionIsAbsolute), exit, me->release());
    
    event = (IOHIDGameControllerEventData *)me->_data;
 
    event->dpad.up      = dpadUp;
    event->dpad.down    = dpadDown;
    event->dpad.left    = dpadLeft;
    event->dpad.right   = dpadRight;
    event->face.x       = faceX;
    event->face.y       = faceY;
    event->face.a       = faceA;
    event->face.b       = faceB;
    event->shoulder.l1  = shoulderL;
    event->shoulder.r1  = shoulderR;

exit:
    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::extendedGameControllerEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::extendedGameControllerEvent(UInt64                          timeStamp,
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
                                                     IOOptionBits                    options)
{
    IOHIDEvent *me = standardGameControllerEvent(timeStamp, dpadUp, dpadDown, dpadLeft, dpadRight, faceX, faceY, faceA, faceB, shoulderL1, shoulderR1, options);
    IOHIDGameControllerEventData *event = NULL;
    require(me, exit);
    
    event = (IOHIDGameControllerEventData *)me->_data;
    
    event->controllerType = kIOHIDGameControllerTypeExtended;
    event->shoulder.l2  = shoulderL2;
    event->shoulder.r2  = shoulderR2;
    event->joystick.x   = joystickX;
    event->joystick.y   = joystickY;
    event->joystick.z   = joystickZ;
    event->joystick.rz  = joystickRz;

exit:
    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::extendedGameControllerEventWithOptionalButtons
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::extendedGameControllerEventWithOptionalButtons(
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
        IOOptionBits                    options)
{
    IOHIDEvent *me = standardGameControllerEvent(timeStamp, dpadUp, dpadDown, dpadLeft, dpadRight, faceX, faceY, faceA, faceB, shoulderL1, shoulderR1, options);
    IOHIDGameControllerEventData *event = NULL;
    __IOHIDGameControllerEventData *extData = NULL;
    require(me, exit);

    event = (IOHIDGameControllerEventData *)me->_data;
    extData = (__IOHIDGameControllerEventData *)me->_data;

    event->controllerType = kIOHIDGameControllerTypeExtended;
    event->shoulder.l2  = shoulderL2;
    event->shoulder.r2  = shoulderR2;
    event->joystick.x   = joystickX;
    event->joystick.y   = joystickY;
    event->joystick.z   = joystickZ;
    event->joystick.rz  = joystickRz;

    extData->bottom.m1 = bottomM1;
    extData->bottom.m2 = bottomM2;
    extData->bottom.m3 = bottomM3;
    extData->bottom.m4 = bottomM4;

    extData->thumbstickButtonLeft = thumbstickButtonLeft;
    extData->thumbstickButtonRight = thumbstickButtonRight;
    extData->l4  = shoulderL4;
    extData->r4  = shoulderR4;

exit:
    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::humidityEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::humidityEvent(UInt64 timeStamp, IOFixed rh, UInt32 sequence, IOOptionBits options)
{
    IOHIDEvent *me = new IOHIDEvent;
    
    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeHumidity, timeStamp, options | kIOHIDEventOptionIsAbsolute)) {
        me->release();
        return 0;
    }
    
    IOHIDHumidityEventData *event = (IOHIDHumidityEventData *)me->_data;
    
    event->rh = rh;
    event->sequence = sequence;
    
    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::brightnessEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::brightnessEvent(UInt64 timeStamp, IOFixed currentBrightness, IOFixed targetBrightness, UInt64 transitionTime, IOOptionBits options)
{
    IOHIDEvent *me = new IOHIDEvent;
    
    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeBrightness, timeStamp, options)) {
        me->release();
        return 0;
    }
    
    IOHIDBrightnessEventData *event = (IOHIDBrightnessEventData *)me->_data;
    
    event->currentBrightness = currentBrightness;
    event->targetBrightness  = targetBrightness;
    event->transitionTime    = transitionTime;
  
    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::orienrationEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent *   IOHIDEvent::orientationEvent(UInt64 timeStamp, UInt32 orientationType, IOOptionBits options)
{
    IOHIDEvent *me = new IOHIDEvent;
    
    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeOrientation, timeStamp, options)) {
        me->release();
        return 0;
    }
    
    __IOHIDOrientationEventData *event = (__IOHIDOrientationEventData *) me->_data;
    
    event->orientationType = orientationType;
	
	return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::genericGestureEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::genericGestureEvent(UInt64 timeStamp, IOHIDGenericGestureType gestureType, IOOptionBits options)
{
    IOHIDEvent *me = new IOHIDEvent;
    
    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeGenericGesture, timeStamp, options)) {
        me->release();
        return NULL;
    }
    
    __IOHIDGenericGestureEventData *event = (__IOHIDGenericGestureEventData *) me->_data;
    
    event->gestureType = gestureType;
    
    return me;
}


//==============================================================================
// IOHIDEvent::appendChild
//==============================================================================
void IOHIDEvent::appendChild(IOHIDEvent *childEvent)
{
    if (!_children) {
        const OSObject *events[] = { childEvent };

        _children = OSArray::withObjects(events, 1);
        
        _data->options |= kIOHIDEventOptionIsCollection;
    } else {
        _children->setObject(childEvent);
    }
}

//==============================================================================
// IOHIDEvent::getType
//==============================================================================
IOHIDEventType IOHIDEvent::getType()
{
    return _data->type;
}

//==============================================================================
// IOHIDEvent::setType
//==============================================================================
void IOHIDEvent::setType(IOHIDEventType type)
{
    initWithType(type);
}

//==============================================================================
// IOHIDEvent::getEvent
//==============================================================================
IOHIDEvent * IOHIDEvent::getEvent(      IOHIDEventType          type,
                                        IOOptionBits            options __unused)
{
    return (_data->type == type) ? this : NULL;
}

//==============================================================================
// IOHIDEvent::getIntegerValue
//==============================================================================
SInt32 IOHIDEvent::getIntegerValue(     IOHIDEventField         key,
                                        IOOptionBits            options)
{
    SInt32 value = 0;

    GET_EVENT_VALUE(this, key, value, options, Integer);
    
    return value;
}

//==============================================================================
// IOHIDEvent::getDoubleValue
//==============================================================================
IOHIDDouble IOHIDEvent::getDoubleValue( IOHIDEventField         key,
                                        IOOptionBits            options)
{
    IOHIDDouble value = 0;

    GET_EVENT_VALUE(this, key, value, options, Double);
  
    return value;
}




//==============================================================================
// IOHIDEvent::getFixedValue
//==============================================================================
IOFixed IOHIDEvent::getFixedValue(      IOHIDEventField         key,
                                        IOOptionBits            options)
{
    IOFixed value = 0;

    GET_EVENT_VALUE(this, key, value, options, Fixed);

    return value;
}

//==============================================================================
// IOHIDEvent::getDataValue
//==============================================================================
UInt8 * IOHIDEvent::getDataValue(IOHIDEventField key, IOOptionBits options)
{
    UInt8 * value = NULL;

    GET_EVENT_DATA(this, key, value, options)
    
    return value;
}


//==============================================================================
// IOHIDEvent::setIntegerValue
//==============================================================================
void IOHIDEvent::setIntegerValue(       IOHIDEventField         key,
                                        SInt32                  value,
                                        IOOptionBits            options)
{
    SET_EVENT_VALUE(this, key, value, options, Integer);
}

//==============================================================================
// IOHIDEvent::setFixedValue
//==============================================================================
void IOHIDEvent::setFixedValue(         IOHIDEventField         key,
                                        IOFixed                 value,
                                        IOOptionBits            options)
{
    SET_EVENT_VALUE(this, key, value, options, Fixed);
}

//==============================================================================
// IOHIDEvent::setDoubleValue
//==============================================================================
void IOHIDEvent::setDoubleValue( IOHIDEventField         key,
                                 IOHIDDouble             value,
                                 IOOptionBits            options)
{
    SET_EVENT_VALUE(this, key, value, options, Double);
}

//==============================================================================
// IOHIDEvent::getLength
//==============================================================================
size_t IOHIDEvent::getLength()
{
    _eventCount = 0;
    return getLength(&_eventCount);
}

//==============================================================================
// IOHIDEvent::getLength
//==============================================================================
IOByteCount IOHIDEvent::getLength(UInt32 * count)
{
    IOByteCount length = _data->size;

    length += sizeof(IOHIDSystemQueueElement);

    if ( _children )
    {
        UInt32          i, childCount;
        IOHIDEvent *    child;

        childCount = _children->getCount();

        for(i=0 ;i<childCount; i++) {
            if ( (child = (IOHIDEvent *)_children->getObject(i)) ) {
                length += child->getLength(count) - sizeof(IOHIDSystemQueueElement);
            }
        }
    }

    if ( count )
        *count = *count + 1;

    return length;
}

//==============================================================================
// IOHIDEvent::appendBytes
//==============================================================================
IOByteCount IOHIDEvent::appendBytes(UInt8 * bytes, IOByteCount withLength)
{
    IOByteCount size = 0;

    size = _data->size;

    if ( size > withLength )
        return 0;

    bcopy(_data, bytes, size);

    if ( _children )
    {
        UInt32          i, childCount;
        IOHIDEvent *    child;

        childCount = _children->getCount();

        for(i=0 ;i<childCount; i++) {
            if ( (child = (IOHIDEvent *)_children->getObject(i)) ) {
                size += child->appendBytes(bytes + size, withLength - size);
            }
        }
    }

    return size;
}

//==============================================================================
// IOHIDEvent::withBytes
//==============================================================================
IOHIDEvent * IOHIDEvent::withBytes(     const void *            bytes,
                                        IOByteCount             size)
{
    IOHIDSystemQueueElement *   queueElement= NULL;
    IOHIDEvent *                parent      = NULL;
    UInt32                      index       = 0;
    UInt32                      total       = 0;
    UInt32                      offset      = 0;
    UInt32                      sz          = 0;
    UInt32                      attributeLength = 0;
    UInt32                      eventCount = 0;
    UInt32                      eventDataSize = 0;
    UInt32                      eventDataType = 0;

    if ( !bytes || !size || ( sizeof(IOHIDSystemQueueElement) > size ) ) {
        return NULL;
    }

    queueElement    = (IOHIDSystemQueueElement *)bytes;
    total           = (UInt32)size - sizeof(IOHIDSystemQueueElement);


    eventCount = queueElement->eventCount;
    attributeLength = queueElement->attributeLength;
    // to prevent complier reordering
    // local assignment should be enforced
    // before validation (55894984)
    COMPILER_BARRIER();

    for (index=0; index < eventCount && offset<total; index++)
    {
        if (os_add_overflow(attributeLength, offset, &sz)) {
            break;
        }
        
        // Can bad value of offset cause overflow ??
        // we already checked that above
        
        if ( (attributeLength + offset) > total ) {
            break;
        }
        
        if ((total - (attributeLength + offset)) < sizeof(IOHIDEventData) ) {
            break;
        }
        

        IOHIDEventData *eventData = (IOHIDEventData *)(queueElement->payload + attributeLength + offset);
        
        if (!eventData) {
            break;
        }
        
        eventDataType = eventData->type;
        eventDataSize = eventData->size;
        COMPILER_BARRIER();
        
        if ( eventDataType >= kIOHIDEventTypeCount ) {
            break;
        }

        // Back calculate additional capacity needed
        IOHIDEventGetSize(eventDataType, sz);
        if (sz > eventDataSize) {
            break;
        }
        
        IOHIDEvent *event = new IOHIDEvent;

        if ( !event ) {
            break;
        }

        if (!event->initWithType(eventDataType, eventDataSize - sz) ||
            eventDataSize != event->_data->size ||
            (total - (attributeLength + offset)) < eventDataSize) {
            event->release();
            break;
        }
        
        bcopy(eventData, event->_data, event->_data->size);
        event->_timeStamp = queueElement->timeStamp;
        event->_options = queueElement->options;
        event->_senderID = queueElement->senderID;

        if ( !parent ) {
            parent = event;
        }
        else {
            //Append event here;
            parent->appendChild(event);
            event->release();
        }
        
        if ( ((UInt32) (offset + eventDataSize) ) <= offset ) {
            break;
        }
        offset += eventDataSize;
    }

    return parent;
}

//==============================================================================
// IOHIDEvent::getChildren
//==============================================================================
OSArray* IOHIDEvent::getChildren()
{
    return _children;
}
//==============================================================================
// IOHIDEvent::readBytes
//==============================================================================
IOByteCount IOHIDEvent::readBytes(void * bytes, IOByteCount withLength)
{
    IOHIDSystemQueueElement *   queueElement= NULL;
    
    if ( withLength < sizeof(IOHIDSystemQueueElement) )
        return 0;
    
    queueElement    = (IOHIDSystemQueueElement *)bytes;

    queueElement->timeStamp         = _timeStamp;
    queueElement->options           = _options;
    queueElement->eventCount        = _eventCount;
    queueElement->senderID          = _senderID;
    queueElement->attributeLength   = 0;

    //bytes += sizeof(IOHIDSystemQueueElement);
    withLength -= sizeof(IOHIDSystemQueueElement);

    return appendBytes((UInt8 *)queueElement->payload, withLength);
}

OSData *IOHIDEvent::createBytes()
{
    OSData *result = NULL;
    IOHIDSystemQueueElement queueElement = { 0 };
    
    result = OSData::withCapacity((unsigned int)getLength());
    require(result, exit);
    
    queueElement.timeStamp         = _timeStamp;
    queueElement.options           = _options;
    queueElement.eventCount        = _eventCount;
    queueElement.senderID          = _senderID;
    queueElement.attributeLength   = 0;
    
    result->appendBytes(&queueElement, sizeof(queueElement));
    result->appendBytes(_data, _data->size);
    
    require_quiet(_children, exit);
    
    for (unsigned int i = 0; i < _children->getCount(); i++) {
        IOHIDEvent *child = (IOHIDEvent *)_children->getObject(i);
        if (!child) {
            continue;
        }
        
        result->appendBytes(child->_data, child->_data->size);
    }
    
exit:
    return result;
}

//==============================================================================
// IOHIDEvent::getPhase
//==============================================================================
IOHIDEventPhaseBits IOHIDEvent::getPhase()
{
    return (_data->options >> kIOHIDEventEventOptionPhaseShift) & kIOHIDEventEventPhaseMask;
}

//==============================================================================
// IOHIDEvent::setPhase
//==============================================================================
void IOHIDEvent::setPhase(IOHIDEventPhaseBits phase)
{
    _data->options &= ~(kIOHIDEventEventPhaseMask << kIOHIDEventEventOptionPhaseShift);
    _data->options |= ((phase & kIOHIDEventEventPhaseMask) << kIOHIDEventEventOptionPhaseShift);
}

//==============================================================================
// IOHIDEvent::setSenderID
//==============================================================================
void IOHIDEvent::setSenderID(uint64_t senderID)
{
    _senderID = senderID;
}

//==============================================================================
// IOHIDEvent::getLatency
//==============================================================================
uint64_t IOHIDEvent::getLatency(uint32_t scaleFactor)
{
    AbsoluteTime    delta = mach_absolute_time();
    AbsoluteTime    ts = getTimeStamp();
    uint64_t        ns;
    
    SUB_ABSOLUTETIME(&delta, &ts);
    
    absolutetime_to_nanoseconds(delta, &ns);

    return ns / scaleFactor;
}
