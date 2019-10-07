//
//  IOHIDInterfaceElementContainer.cpp
//
//
//  Created by dekom on 12/5/18.
//  Copyright © 2018 Apple Inc. All rights reserved.
//

#include <assert.h>
#include <AssertMacros.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>
#include <HIDDriverKit/HIDDriverKit_Private.h>

#define super IOHIDElementContainer

struct IOHIDInterfaceElementContainer_IVars
{
    IOHIDInterface   *owner;
};

#define _owner  ivars->owner

bool IOHIDInterfaceElementContainer::init(void *descriptor,
                                          IOByteCount length,
                                          IOHIDInterface *owner)
{
    bool result = false;
    
    require(super::init(descriptor, length), exit);
    
    ivars = IONewZero(IOHIDInterfaceElementContainer_IVars, 1);
    require(ivars, exit);
    
    _owner = owner;
    
    result = true;
    
exit:
    return result;
}

IOHIDInterfaceElementContainer *IOHIDInterfaceElementContainer::withDescriptor(
                                                            void *descriptor,
                                                            IOByteCount length,
                                                            IOHIDInterface *owner)
{
    IOHIDInterfaceElementContainer *me = NULL;
    
    me = OSTypeAlloc(IOHIDInterfaceElementContainer);
    
    if (me && !me->init(descriptor, length, owner)) {
        me->release();
        return NULL;
    }
    
    return me;
}

void IOHIDInterfaceElementContainer::free()
{
    IOSafeDeleteNULL(ivars, IOHIDInterfaceElementContainer_IVars, 1);
    
    super::free();
}

IOReturn IOHIDInterfaceElementContainer::updateElementValues(
                                                    IOHIDElementCookie *cookies,
                                                    uint32_t cookieCount)
{
    IOReturn ret = kIOReturnError;
    OSArray *elements = NULL;
    
    require_action(cookies && cookieCount, exit, ret = kIOReturnBadArgument);
    
    elements = OSArray::withCapacity(cookieCount);
    require_action(elements, exit, ret = kIOReturnNoMemory);
    
    for (uint32_t i = 0; i < cookieCount; i++) {
        IOHIDElement *element = (IOHIDElement *)getElements()->getObject(cookies[i]);
        
        if (!element) {
            continue;
        }
        
        elements->setObject(element);
    }
    
    ret = _owner->getElementValues(elements);
    
exit:
    OSSafeReleaseNULL(elements);
    
    return ret;
}

IOReturn IOHIDInterfaceElementContainer::postElementValues(
                                                    IOHIDElementCookie *cookies,
                                                    uint32_t cookieCount)
{
    IOReturn ret = kIOReturnError;
    OSArray *elements = NULL;
    
    require_action(cookies && cookieCount, exit, ret = kIOReturnBadArgument);
    
    elements = OSArray::withCapacity(cookieCount);
    require_action(elements, exit, ret = kIOReturnNoMemory);
    
    for (uint32_t i = 0; i < cookieCount; i++) {
        IOHIDElement *element = (IOHIDElement *)getElements()->getObject(cookies[i]);
        
        if (!element) {
            continue;
        }
        
        elements->setObject(element);
    }
    
    ret = _owner->setElementValues(elements);
    
exit:
    OSSafeReleaseNULL(elements);
    
    return ret;
}
