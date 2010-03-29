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
#include <IOKit/IOLib.h>
#include "ev_keymap.h"
#include "IOHIDEvent.h"
#include "IOHIDEventData.h"
#include "IOHIDUsageTables.h"

#define EXTERNAL ((unsigned int) -1)

#define super OSObject

OSDefineMetaClassAndStructors(IOHIDEvent, super)

extern unsigned int hid_adb_2_usb_keymap[];

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

    return true;
}

//==============================================================================
// IOHIDEvent::initWithType
//==============================================================================
bool IOHIDEvent::initWithType(IOHIDEventType type)
{
    size_t capacity = 0;
    
    IOHIDEventGetSize(type,capacity);

    if ( !initWithCapacity(capacity) )
        return false;
    
    _data->type = type;
    _typeMask   = IOHIDEventTypeMask(type);
    
    return true;
}

//==============================================================================
// IOHIDEvent::initWithTypeTimeStamp
//==============================================================================
bool IOHIDEvent::initWithTypeTimeStamp(IOHIDEventType type, AbsoluteTime timeStamp, IOOptionBits options)
{
    if ( !initWithType(type))
        return false;
        
    _timeStamp  = timeStamp;
    _options    = options;

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

//==============================================================================
// IOHIDEvent::withType
//==============================================================================
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

//==============================================================================
// IOHIDEvent::withEventData
//==============================================================================
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
            me = IOHIDEvent::translationEvent(timeStamp, data->mouseMove.dx<<16 + data->mouseMove.subx<<8, data->mouseMove.dy<<16 + data->mouseMove.suby<<8, 0, options);
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

//==============================================================================
// IOHIDEvent::keyboardEvent
//==============================================================================
IOHIDEvent * IOHIDEvent::keyboardEvent( AbsoluteTime            timeStamp, 
                                        UInt32                  usagePage,
                                        UInt32                  usage,
                                        Boolean                 down,
                                        IOOptionBits            options)
{
    IOHIDEvent *me = new IOHIDEvent;

    if (me && !me->initWithTypeTimeStamp(kIOHIDEventTypeKeyboard, timeStamp, options)) {
        me->release();
        return 0;
    }
        
    IOHIDKeyboardEventData * keyboard = (IOHIDKeyboardEventData *)me->_data;
    keyboard->usagePage = usagePage;
    keyboard->usage = usage;
    keyboard->down = down;
    
    return me;
}

//==============================================================================
// IOHIDEvent::_axisEvent
//==============================================================================
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

//==============================================================================
// IOHIDEvent::translationEvent
//==============================================================================
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

//==============================================================================
// IOHIDEvent::scrollEvet
//==============================================================================
IOHIDEvent * IOHIDEvent::scrollEvent(
                                        AbsoluteTime            timeStamp,
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

//==============================================================================
// IOHIDEvent::zoomEvent
//==============================================================================
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


//==============================================================================
// IOHIDEvent::accelerometerEvent
//==============================================================================
IOHIDEvent * IOHIDEvent::accelerometerEvent(
                                        AbsoluteTime            timeStamp,
                                        IOFixed                 x,
                                        IOFixed                 y,
                                        IOFixed                 z,
                                        IOHIDAccelerometerType  type,
										IOHIDAccelerometerSubType  subType,
                                        IOOptionBits            options)
{
    IOHIDEvent *                    event;
    IOHIDAccelerometerEventData *   data;    
    
    event = IOHIDEvent::_axisEvent( kIOHIDEventTypeAccelerometer, 
                                    timeStamp, 
                                    x,
                                    y,
                                    z,
                                    options);

    if ( event ) {
        data = (IOHIDAccelerometerEventData *)event->_data;
        data->acclType = type;
		data->acclSubType = subType;
    }
    
    return event;
}

//==============================================================================
// IOHIDEvent::gyroEvent
//==============================================================================
IOHIDEvent * IOHIDEvent::gyroEvent(
											AbsoluteTime            timeStamp,
											IOFixed                 x,
											IOFixed                 y,
											IOFixed                 z,
											IOHIDGyroType		type,
											IOHIDGyroSubType		subType,
											IOOptionBits            options)
{
    IOHIDEvent *                    event;
    IOHIDGyroEventData *		data;    
    
    event = IOHIDEvent::_axisEvent( kIOHIDEventTypeGyro, 
								   timeStamp, 
								   x,
								   y,
								   z,
								   options);
	
    if ( event ) {
        data = (IOHIDGyroEventData *)event->_data;
        data->gyroType = type;
        data->gyroSubType = subType;
    }
    
    return event;
}

//==============================================================================
// IOHIDEvent::ambientLightSensorEvent
//==============================================================================
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

//==============================================================================
// IOHIDEvent::proximityEvent
//==============================================================================
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



//==============================================================================
// IOHIDEvent::temperatureEvent
//==============================================================================
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

//==============================================================================
// IOHIDEvent::buttonEvent
//==============================================================================
IOHIDEvent * IOHIDEvent::buttonEvent(
                                        AbsoluteTime            timeStamp,
                                        UInt32                  buttonMask,
                                        IOOptionBits            options)
{
    return IOHIDEvent::buttonEvent(timeStamp, buttonMask, 0, options);
}

//==============================================================================
// IOHIDEvent::buttonEvent
//==============================================================================
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
                                        IOOptionBits            options)
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
    
    #define HIDEVENTFIXED 1
    GET_EVENT_VALUE(this, key, value, options);
    #undef HIDEVENTFIXED
    
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
    #define HIDEVENTFIXED 1
    SET_EVENT_VALUE(this, key, value, options);
    #undef HIDEVENTFIXED
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
        
        for(i=0 ;i<childCount; i++)
            if ( child = (IOHIDEvent *)_children->getObject(i) )
                length += child->getLength(count) - sizeof(IOHIDSystemQueueElement);
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
            if ( child = (IOHIDEvent *)_children->getObject(i) )
                size += child->appendBytes(bytes + size, withLength - size);
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
        IOHIDEventData *    eventData   = queueElement->events + offset;
        IOHIDEvent *        event       = IOHIDEvent::withType(eventData->type);
    
        if ( !event ) 
            continue;
            
        bcopy(eventData, event->_data, event->_data->size);
        *((uint64_t *)&(event->_timeStamp)) = queueElement->timeStamp;
        event->_options = queueElement->options;
                
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
    
    queueElement->timeStamp     = *((uint64_t *)&_timeStamp);
    queueElement->options       = _options;
    queueElement->eventCount    = _eventCount;
    
    //bytes += sizeof(IOHIDSystemQueueElement);
    withLength -= sizeof(IOHIDSystemQueueElement);
    
    return appendBytes((UInt8 *)queueElement->events, withLength);        
}

