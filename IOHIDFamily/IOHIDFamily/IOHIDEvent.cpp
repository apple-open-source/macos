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
#include <IOKit/IOLib.h>
#include "IOHIDEventTypes.h"
#include "IOHIDEvent.h"
#include "IOHIDEventData.h"
#include "IOHIDUsageTables.h"

#if !TARGET_OS_EMBEDDED
#include "ev_keymap.h"
#endif /* TARGET_OS_EMBEDDED */

#define EXTERNAL ((unsigned int) -1)

#define super OSObject

OSDefineMetaClassAndStructors(IOHIDEvent, OSObject)

//==============================================================================
// IOHIDEvent::initWithCapacity
//==============================================================================
bool IOHIDEvent::initWithCapacity(IOByteCount capacity)
{
    if (!super::init())
        return false;

    if (_data && (!capacity || _capacity < capacity) ) {
        // clean out old data's storage if it isn't big enough
        IOFree(_data, _capacity);
        _data = 0;
    }

    _capacity = capacity;

    if ( !_capacity )
        return false;

    if ( !_data && !(_data = (IOHIDEventData *) IOMalloc(_capacity)))
        return false;

    bzero(_data, _capacity);
    _data->size = _capacity;
    _children = NULL;
    
    clock_get_uptime(&_creationTimeStamp);

    return true;
}

//==============================================================================
// IOHIDEvent::initWithType
//==============================================================================
bool IOHIDEvent::initWithType(IOHIDEventType type, IOByteCount additionalCapacity)
{
    size_t capacity = 0;

    IOHIDEventGetSize(type,capacity);

    if ( !initWithCapacity(capacity+additionalCapacity) )
        return false;

    _data->type = type;
    _typeMask   = IOHIDEventTypeMask(type);

    return true;
}

//==============================================================================
// IOHIDEvent::initWithTypeTimeStamp
//==============================================================================
bool IOHIDEvent::initWithTypeTimeStamp(IOHIDEventType type, AbsoluteTime timeStamp, IOOptionBits options, IOByteCount additionalCapacity)
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
        IOFree(_data, _capacity);
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
    return _timeStamp;
}

//==============================================================================
// IOHIDEvent::setTimeStamp
//==============================================================================
void IOHIDEvent::setTimeStamp( AbsoluteTime timeStamp )
{
    _timeStamp = timeStamp;
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

#if !TARGET_OS_EMBEDDED

extern unsigned int hid_adb_2_usb_keymap[];

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::withEventData
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::withEventData (
                                        AbsoluteTime            timeStamp,
                                        UInt32                  type,
                                        NXEventData *           data,
                                        IOOptionBits            options)
{
    IOHIDEvent *me = NULL;

    switch ( type ) {
        case NX_LMOUSEDOWN:
        case NX_LMOUSEUP:
        case NX_RMOUSEDOWN:
        case NX_RMOUSEUP:
        case NX_OMOUSEDOWN:
        case NX_OMOUSEUP:
            break;
        case NX_MOUSEMOVED:
        case NX_LMOUSEDRAGGED:
        case NX_RMOUSEDRAGGED:
        case NX_OMOUSEDRAGGED:
            me = IOHIDEvent::translationEvent(timeStamp, (data->mouseMove.dx<<16) + (data->mouseMove.subx<<8), (data->mouseMove.dy<<16) + (data->mouseMove.suby<<8), 0, options);
            break;

        case NX_KEYDOWN:
        case NX_KEYUP:
            {
            uint32_t usage = hid_adb_2_usb_keymap[data->key.keyCode];

            if ( usage < kHIDUsage_KeyboardLeftControl || usage > kHIDUsage_KeyboardRightGUI || !data->key.repeat )
                me = IOHIDEvent::keyboardEvent(timeStamp, kHIDPage_KeyboardOrKeypad, hid_adb_2_usb_keymap[data->key.keyCode], type==NX_KEYDOWN, options | (data->key.repeat ? kIOHIDKeyboardIsRepeat : 0));
            }
            break;
        case NX_SCROLLWHEELMOVED:
            me = IOHIDEvent::scrollEvent(timeStamp, data->scrollWheel.pointDeltaAxis2<<16, data->scrollWheel.pointDeltaAxis1<<16, data->scrollWheel.pointDeltaAxis3<<16, options);
            break;
        case NX_ZOOM:
            me = IOHIDEvent::zoomEvent(timeStamp, data->scrollWheel.pointDeltaAxis2<<16, data->scrollWheel.pointDeltaAxis1<<16, data->scrollWheel.pointDeltaAxis3<<16, options);
            break;
        case NX_SYSDEFINED:
            switch (data->compound.subType) {
                case NX_SUBTYPE_AUX_MOUSE_BUTTONS:
                    me = IOHIDEvent::buttonEvent(timeStamp, data->compound.misc.L[1], options);
                    break;
                case NX_SUBTYPE_AUX_CONTROL_BUTTONS:
                    uint16_t flavor = data->compound.misc.L[0] >> 16;
                    switch ( flavor ) {
                        case NX_KEYTYPE_SOUND_UP:
                        case NX_KEYTYPE_SOUND_DOWN:
                        case NX_KEYTYPE_BRIGHTNESS_UP:
                        case NX_KEYTYPE_BRIGHTNESS_DOWN:
                        case NX_KEYTYPE_HELP:
                        case NX_POWER_KEY:
                        case NX_KEYTYPE_MUTE:
                        case NX_KEYTYPE_NUM_LOCK:

                        case NX_KEYTYPE_EJECT:
                        case NX_KEYTYPE_VIDMIRROR:

                        case NX_KEYTYPE_PLAY:
                        case NX_KEYTYPE_NEXT:
                        case NX_KEYTYPE_PREVIOUS:
                        case NX_KEYTYPE_FAST:
                        case NX_KEYTYPE_REWIND:

                        case NX_KEYTYPE_ILLUMINATION_UP:
                        case NX_KEYTYPE_ILLUMINATION_DOWN:
                        case NX_KEYTYPE_ILLUMINATION_TOGGLE:
                            break;
                    }
            }
            break;
    }

    return me;
}

#endif /* TARGET_OS_EMBEDDED */


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::keyboardEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::keyboardEvent( AbsoluteTime            timeStamp,
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

    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::_axisEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::_axisEvent(    IOHIDEventType          type,
                                        AbsoluteTime            timeStamp,
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
                                        AbsoluteTime            timeStamp,
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
                                        AbsoluteTime            timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOOptionBits            options)
{
    return IOHIDEvent::_axisEvent(      kIOHIDEventTypeTranslation,
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
                                        AbsoluteTime            timeStamp,
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
                                        AbsoluteTime            timeStamp,
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
                                    AbsoluteTime            timeStamp,
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
                                    AbsoluteTime            timeStamp,
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
                                   AbsoluteTime             timeStamp,
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
                                        AbsoluteTime            timeStamp,
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
// IOHIDEvent::proximityEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::proximityEvent (
                                        AbsoluteTime				timeStamp,
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
	event->level			= level;

    return me;
}



//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::temperatureEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::temperatureEvent(
                                        AbsoluteTime            timeStamp,
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
                                        AbsoluteTime            timeStamp,
                                        UInt32                  buttonMask,
                                        IOOptionBits            options)
{
    return IOHIDEvent::buttonEvent(timeStamp, buttonMask, 0, options);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::buttonEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::buttonEvent(
                                        AbsoluteTime            timeStamp,
                                        UInt32                  buttonMask,
                                        IOFixed                 pressure,
                                        IOOptionBits            options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeButton, timeStamp, options)) {
        me->release();
        return 0;
    }

    IOHIDButtonEventData * event = (IOHIDButtonEventData *)me->_data;
    event->button.buttonMask = buttonMask;
    event->button.pressure   = pressure;

    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::relativePointerEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::relativePointerEvent(
                                        AbsoluteTime            timeStamp,
                                        SInt32                  x,
                                        SInt32                  y,
                                        SInt32                  z,
                                        UInt32                  buttonState,
                                        IOOptionBits            options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypePointer, timeStamp, options)) {
        me->release();
        return NULL;
    }

    IOHIDPointerEventData *event = (IOHIDPointerEventData *)me->_data;

    event->position.x = x<<16;
    event->position.y = y<<16;
    event->position.z = z<<16;
    event->button.buttonMask = buttonState;

    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::multiAxisPointerEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::multiAxisPointerEvent(
                                        AbsoluteTime            timeStamp,
                                        UInt32                  buttonState,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOFixed                 rX,
                                        IOFixed                 rY,
                                        IOFixed                 rZ,
                                        IOOptionBits            options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeMultiAxisPointer, timeStamp, options | kIOHIDEventOptionIsAbsolute | kIOHIDEventOptionIsCenterOrigin)) {
        me->release();
        return NULL;
    }

    IOHIDMultiAxisPointerEventData *event = (IOHIDMultiAxisPointerEventData *)me->_data;

    event->position.x = x;
    event->position.y = y;
    event->position.z = z;
    event->rotation.x = rX;
    event->rotation.y = rY;
    event->rotation.z = rZ;
    event->button.buttonMask = buttonState;

    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::digitizerEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::digitizerEvent(
                                        AbsoluteTime                    timeStamp,
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
        event->options |= kIOHIDTransducerRange;
    }

    event->eventMask        = 0;       // todo:
    event->transducerIndex  = transducerID; // Multitouch uses this as a path ID
    event->transducerType   = type;
    event->buttonMask       = buttonState;
    event->position.x       = x;
    event->position.y       = y;
    event->position.z       = z;
    event->pressure         = tipPressure;
    event->auxPressure      = auxPressure;
    event->twist            = twist;


    // Let's assume no tip pressure means finger
    switch ( event->transducerType ) {
        case kIOHIDDigitizerTransducerTypeFinger:
            event->identity = 2;        // Multitouch interprets this as 'finger', hard code to 2 or index finger
            event->orientationType = kIOHIDDigitizerOrientationTypeQuality;
            event->orientation.quality.quality = 0;
            event->orientation.quality.density = 0;
            event->orientation.quality.irregularity = 0;
            event->orientation.quality.majorRadius = 6<<16;
            event->orientation.quality.minorRadius = 6<<16;
            break;
    }

    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::digitizerEventWithTiltOrientation
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::digitizerEventWithTiltOrientation(
                                        AbsoluteTime                    timeStamp,
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

    event->orientation.tilt.x = xTilt;
    event->orientation.tilt.y = xTilt;

exit:
    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::digitizerEventWithPolarOrientation
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::digitizerEventWithPolarOrientation(
                                        AbsoluteTime                    timeStamp,
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

    event->orientation.polar.altitude   = altitude;
    event->orientation.polar.azimuth    = azimuth;

exit:
    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::digitizerEventWithQualityOrientation
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::digitizerEventWithQualityOrientation(
                                        AbsoluteTime                    timeStamp,
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

    event->orientation.quality.quality          = quality;
    event->orientation.quality.density          = density;
    event->orientation.quality.irregularity     = irregularity;
    event->orientation.quality.majorRadius      = majorRadius;
    event->orientation.quality.minorRadius      = minorRadius;

exit:
    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::powerEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::powerEvent(
                                   AbsoluteTime            timeStamp,
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

    event->measurement = measurement;
    event->powerType = powerType;
    event->powerSubType = powerSubType;

    return me;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::vendorDefinedEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::vendorDefinedEvent(
                                           AbsoluteTime            timeStamp,
                                           UInt32                  usagePage,
                                           UInt32                  usage,
                                           UInt32                  version,
                                           UInt8 *                 data,
                                           UInt32                  length,
                                           IOOptionBits            options)
{
    UInt32      dataLength  = length;
    IOHIDEvent * me         = new IOHIDEvent;

    if ( dataLength < sizeof(natural_t))
        dataLength = sizeof(natural_t);

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

#if TARGET_OS_EMBEDDED
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEvent::biometricEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEvent * IOHIDEvent::biometricEvent(AbsoluteTime timeStamp, IOFixed level, IOHIDBiometricEventType eventType, IOOptionBits options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeBiometric, timeStamp, options)) {
        me->release();
        return 0;
    }

    IOHIDBiometricEventData *event = (IOHIDBiometricEventData *)me->_data;

    event->eventType    = eventType;
    event->level        = level;

    return me;
}
#endif /* TARGET_OS_EMBEDDED */

//==============================================================================
// IOHIDEvent::appendChild
//==============================================================================
void IOHIDEvent::appendChild(IOHIDEvent *childEvent)
{
    if (!_children) {
        const OSObject *events[] = { childEvent };

        _children = OSArray::withObjects(events, 1);
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

    GET_EVENT_VALUE(this, key, value, options);

    return value;
}

//==============================================================================
// IOHIDEvent::getFixedValue
//==============================================================================
IOFixed IOHIDEvent::getFixedValue(      IOHIDEventField         key,
                                        IOOptionBits            options)
{
    IOFixed value = 0;

    GET_EVENT_VALUE_FIXED(this, key, value, options);

    return value;
}

//==============================================================================
// IOHIDEvent::setIntegerValue
//==============================================================================
void IOHIDEvent::setIntegerValue(       IOHIDEventField         key,
                                        SInt32                  value,
                                        IOOptionBits            options)
{
    SET_EVENT_VALUE(this, key, value, options);
}

//==============================================================================
// IOHIDEvent::setFixedValue
//==============================================================================
void IOHIDEvent::setFixedValue(         IOHIDEventField         key,
                                        IOFixed                 value,
                                        IOOptionBits            options)
{
    SET_EVENT_VALUE_FIXED(this, key, value, options);
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

    if ( !bytes || !size )
        return NULL;

    queueElement    = (IOHIDSystemQueueElement *)bytes;
    total           = size - sizeof(IOHIDSystemQueueElement);

    for (index=0; index<queueElement->eventCount && offset<total; index++)
    {
        IOHIDEventData *    eventData   = (IOHIDEventData *)(queueElement->payload + queueElement->attributeLength + offset);
        IOHIDEvent *        event       = IOHIDEvent::withType(eventData->type);

        if ( !event )
            continue;

        bcopy(eventData, event->_data, event->_data->size);
        AbsoluteTime_to_scalar(&(event->_timeStamp)) = queueElement->timeStamp;
        AbsoluteTime_to_scalar(&(event->_creationTimeStamp)) = queueElement->creationTimeStamp;
        event->_options = queueElement->options;
        event->_senderID = queueElement->senderID;

        if ( !parent )
            parent = event;
        else {
            //Append event here;
            event->release();
        }
        offset += eventData->size;
    }

    return parent;
}

//==============================================================================
// IOHIDEvent::readBytes
//==============================================================================
IOByteCount IOHIDEvent::readBytes(void * bytes, IOByteCount withLength)
{
    IOHIDSystemQueueElement *   queueElement= NULL;

    queueElement    = (IOHIDSystemQueueElement *)bytes;

    queueElement->timeStamp         = *((uint64_t *)&_timeStamp);
    queueElement->creationTimeStamp = *((uint64_t *)&_creationTimeStamp);
    queueElement->options           = _options;
    queueElement->eventCount        = _eventCount;
    queueElement->senderID          = _senderID;
    queueElement->attributeLength   = 0;

    //bytes += sizeof(IOHIDSystemQueueElement);
    withLength -= sizeof(IOHIDSystemQueueElement);

    return appendBytes((UInt8 *)queueElement->payload, withLength);
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
    uint64_t        ns;
    
    SUB_ABSOLUTETIME(&delta, &_timeStamp);
    
    absolutetime_to_nanoseconds(delta, &ns);

    return ns / scaleFactor;
}

