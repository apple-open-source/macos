//
//  IOHIDEvent.cpp
//  HIDDriverKit
//
//  Created by dekom on 2/5/19.
//

#include <math.h>
#include <AssertMacros.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>
#include <HIDDriverKit/HIDDriverKit_Private.h>

// Redefines for IOHIDEventData.h macros
#define IOHIDEventRef IOHIDEvent *
#define IOHIDEventGetEventWithOptions(event, type, options) \
(event->getType() == type ? event : NULL)

#ifdef GET_EVENTDATA
#undef GET_EVENTDATA
#endif
#define GET_EVENTDATA(event)    event->_data

struct IOHIDEvent_IVars
{
    IOHIDEventData *data;
    OSArray *children;
    uint64_t timestamp;
    uint64_t senderID;
    IOOptionBits options;
    uint32_t eventCount;
};

#define _data           ivars->data
#define _children       ivars->children
#define _timestamp      ivars->timestamp
#define _senderID       ivars->senderID
#define _options        ivars->options
#define _eventCount     ivars->eventCount

#define super OSContainer

void IOHIDEvent::free()
{
    if (ivars) {
        if (_data) {
            IOFree(_data, _data->size);
        }
        OSSafeReleaseNULL(_children);
    }
    
    IOSafeDeleteNULL(ivars, IOHIDEvent_IVars, 1);
    super::free();
}

bool IOHIDEvent::initWithType(IOHIDEventType type,
                              uint64_t timestamp,
                              uint32_t addCapacity,
                              IOOptionBits options)
{
    bool result = false;
    size_t size = 0;
    
    IOHIDEventGetSize(type, size);
    
    require(super::init(), exit);
    
    ivars = IONewZero(IOHIDEvent_IVars, 1);
    
    _data = (IOHIDEventData *)IOMallocZero(size + addCapacity);
    require(_data, exit);
    
    _data->size = size + addCapacity;
    _data->type = type;
    _data->options = options;
    _timestamp = timestamp;
    _options = options;
    
    result = true;
    
exit:
    return result;
}

uint32_t IOHIDEvent::appendBytes(uint8_t *bytes, uint32_t length)
{
    uint32_t size = 0;
    
    require(_data->size <= length, exit);
    
    size = _data->size;
    bcopy(_data, bytes, size);
    
    require_quiet(_children, exit);
    
    for (unsigned int i = 0; i < _children->getCount(); i++) {
        IOHIDEvent *child = OSDynamicCast(IOHIDEvent, _children->getObject(i));
        
        if (!child) {
            continue;
        }
        
        size += child->appendBytes(bytes + size, length - size);
    }
    
exit:
    return size;
}

IOHIDEvent *IOHIDEvent::withType(IOHIDEventType type,
                                 uint64_t timestamp,
                                 uint32_t addCapacity,
                                 IOOptionBits options)
{
    IOHIDEvent *me = NULL;
    
    me = OSTypeAlloc(IOHIDEvent);
    
    if (me && !me->initWithType(type, timestamp, addCapacity, options)) {
        me->release();
        return NULL;
    }
    
    return me;
}

IOHIDEvent *IOHIDEvent::vendorDefinedEvent(uint64_t timestamp,
                                           uint32_t usagePage,
                                           uint32_t usage,
                                           uint32_t version,
                                           uint8_t *data,
                                           uint32_t length,
                                           IOOptionBits options)
{
    IOHIDEvent *me = NULL;
    
    me = OSTypeAlloc(IOHIDEvent);
    
    if (me && !me->initWithType(kIOHIDEventTypeVendorDefined, timestamp, length, options)) {
        me->release();
        return NULL;
    }
    
    IOHIDVendorDefinedEventData *event = (IOHIDVendorDefinedEventData *)me->_data;
    
    event->usagePage = usagePage;
    event->usage = usage;
    event->version = version;
    event->length = length;
    
    bcopy(data, event->data, length);
    
    return me;
}

uint32_t IOHIDEvent::readBytes(void *bytes, uint32_t length)
{
    IOHIDSystemQueueElement *queueElement = NULL;
    
    if (length < sizeof(IOHIDSystemQueueElement)) {
        return 0;
    }
    
    queueElement = (IOHIDSystemQueueElement *)bytes;
    
    queueElement->timeStamp         = _timestamp;
    queueElement->options           = _options;
    queueElement->eventCount        = _eventCount;
    queueElement->senderID          = _senderID;
    queueElement->attributeLength   = 0;
    
    length -= sizeof(IOHIDSystemQueueElement);
    
    return appendBytes((uint8_t *)queueElement->payload, length);
}

void IOHIDEvent::appendChild(IOHIDEvent *child)
{
    if (!_children) {
        _children = OSArray::withCapacity(1);
        _data->options |= kIOHIDEventOptionIsCollection;
    }
    
    _children->setObject(child);
}

uint64_t IOHIDEvent::getIntegerValue(IOHIDEventField key)
{
    uint64_t value = 0;
    
    GET_EVENT_VALUE(this, key, value, 0, Integer);
    
    return value;
}

void IOHIDEvent::setIntegerValue(IOHIDEventField key, uint64_t value)
{
    SET_EVENT_VALUE(this, key, value, 0, Integer);
}

IOFixed IOHIDEvent::getFixedValue(IOHIDEventField key)
{
    IOFixed value = 0;
    
    GET_EVENT_VALUE(this, key, value, 0, Fixed);
    
    return value;
}

void IOHIDEvent::setFixedValue(IOHIDEventField key, IOFixed value)
{
    SET_EVENT_VALUE(this, key, value, 0, Fixed);
}

uint8_t *IOHIDEvent::getDataValue(IOHIDEventField key)
{
    uint8_t *value = NULL;
    
    GET_EVENT_DATA(this, key, dataValue, options);
    
    return value;
}

IOHIDEventType IOHIDEvent::getType()
{
    return _data->type;
}

uint32_t IOHIDEvent::getLength()
{
    _eventCount = 0;
    return getLength(&_eventCount);
}

uint32_t IOHIDEvent::getLength(uint32_t *eventCount)
{
    uint32_t length = _data->size;
    
    length += sizeof(IOHIDSystemQueueElement);
    
    require_quiet(_children, exit);
    
    for (unsigned int i = 0; i < _children->getCount(); i++) {
        IOHIDEvent *child = OSDynamicCast(IOHIDEvent, _children->getObject(i));
        
        if (!child) {
            continue;
        }
        
        length += child->getLength(eventCount) - sizeof(IOHIDSystemQueueElement);
    }
    
exit:
    if (eventCount) {
        *eventCount = *eventCount + 1;
    }
    
    return length;
}

uint64_t IOHIDEvent::getTimestamp()
{
    return _timestamp;
}
