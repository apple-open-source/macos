//
//  IOHIDDeviceElementContainer.cpp
//  IOHIDFamily
//
//  Created by dekom on 10/23/18.
//

#include "IOHIDDeviceElementContainer.h"
#include <AssertMacros.h>
#include "IOHIDDevice.h"

#define super IOHIDElementContainer
OSDefineMetaClassAndStructors(IOHIDDeviceElementContainer, IOHIDElementContainer)

#define _owner  _reserved->owner

bool IOHIDDeviceElementContainer::init(void *descriptor,
                                       IOByteCount length,
                                       IOHIDDevice *owner)
{
    bool result = false;
    
    require(super::init(descriptor, length), exit);
    
    _reserved = IONew(ExpansionData, 1);
    require(_reserved, exit);
    
    bzero(_reserved, sizeof(ExpansionData));
    
    _owner = owner;
    
    result = true;
    
exit:
    return result;
}

IOHIDDeviceElementContainer *IOHIDDeviceElementContainer::withDescriptor(
                                                            void *descriptor,
                                                            IOByteCount length,
                                                            IOHIDDevice *owner)
{
    IOHIDDeviceElementContainer *me = new IOHIDDeviceElementContainer;
    
    if (me && !me->init(descriptor, length, owner)) {
        me->release();
        return NULL;
    }
    
    return me;
}

void IOHIDDeviceElementContainer::free()
{
    if (_reserved) {
        IODelete(_reserved, ExpansionData, 1);
    }
    
    super::free();
}

IOReturn IOHIDDeviceElementContainer::updateElementValues(
                                                    IOHIDElementCookie *cookies,
                                                    UInt32 cookieCount)
{
    return _owner->updateElementValues(cookies, cookieCount);
}

IOReturn IOHIDDeviceElementContainer::postElementValues(
                                                    IOHIDElementCookie *cookies,
                                                    UInt32 cookieCount)
{
    return _owner->postElementValues(cookies, cookieCount);
}
