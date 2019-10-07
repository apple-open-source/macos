//
//  IOHIDInterface.cpp
//  HIDDriverKit
//
//  Created by dekom on 1/16/19.
//

#include <assert.h>
#include <AssertMacros.h>
#include <stdio.h>
#include <stdlib.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>
#include <DriverKit/IOBufferMemoryDescriptor.h>
#include <HIDDriverKit/HIDDriverKit_Private.h>

struct IOHIDInterface_IVars
{
    OSDictionaryPtr                 properties;
    IOHIDInterfaceElementContainer  *container;
    OSArray                         *elements;
};

#define _properties ivars->properties
#define _container  ivars->container
#define _elements   ivars->elements

#undef super
#define super IOService

bool IOHIDInterface::init()
{
    bool ret;
    
    ret = super::init();
    require_action(ret, exit, HIDLogError("Init:%x", ret));
    
    assert(IOService::ivars);
    
    ivars = IONewZero(IOHIDInterface_IVars, 1);
    
    ret = true;
    
exit:
    return ret;
}

void IOHIDInterface::free()
{
    if (ivars) {
        OSSafeReleaseNULL(_properties);
        OSSafeReleaseNULL(_container);
        OSSafeReleaseNULL(_elements);
    }
    
    IOSafeDeleteNULL(ivars, IOHIDInterface_IVars, 1);
    super::free();
}

OSArray *IOHIDInterface::getElements()
{
    if (!_elements) {
        createElements();
    }
    
    return _elements;
}


kern_return_t IOHIDInterface::commitElements(OSArray *elements,
                                             IOHIDElementCommitDirection direction)
{
    IOReturn ret = kIOReturnError;
    
    if (direction == kIOHIDElementCommitDirectionIn) {
        ret = getElementValues(elements);
    } else {
        ret = setElementValues(elements);
    }
    
    if (ret != kIOReturnSuccess) {
        HIDServiceLogError("IOHIDInterface::commitElements failed %d: 0x%x", direction, ret);
    }
    
    return ret;
}

kern_return_t IOHIDInterface::setElementValues(OSArray *elements)
{
    kern_return_t ret = kIOReturnError;
    IOBufferMemoryDescriptor *values = NULL;
    uint32_t totalSize = 0;
    uint8_t *buffer = NULL;
    uint8_t *buffPtr = NULL;
    
    require_action(elements && elements->getCount(), exit, ret = kIOReturnBadArgument);
    
    // first get the total size of all of the element values
    for (unsigned int i = 0; i < elements->getCount(); i++) {
        IOHIDElementPrivate *element;
        
        element = (IOHIDElementPrivate *)elements->getObject(i);
        if (!element) {
            continue;
        }
        
        totalSize += sizeof(IOHIDElementValueHeader) + element->getByteSize();
    }
    
    require_action(totalSize, exit, ret = kIOReturnBadArgument);
    
    ret = IOBufferMemoryDescriptor::Create(kIOMemoryDirectionInOut, totalSize, 0, &values);
    require_noerr_action(ret, exit, ret = kIOReturnNoMemory);
    
    {
        uint64_t address;
        uint64_t length;
        ret = values->Map(0, 0, 0, IOVMPageSize, &address, &length);
        require_noerr_action(ret, exit, HIDServiceLogError("map failed: 0x%x", ret));
        
        buffer = (typeof(buffer))address;
    }
    
    require_action(buffer, exit, ret = kIOReturnNoMemory);
    
    buffPtr = buffer;
    
    // Now fill up the buffer
    for (unsigned int i = 0; i < elements->getCount(); i++) {
        IOHIDElementPrivate *element;
        IOHIDElementValueHeader *header;
        uint32_t valueSize = 0;
        
        element = (IOHIDElementPrivate *)elements->getObject(i);
        if (!element) {
            continue;
        }
        
        valueSize = element->getByteSize();
        
        header = (IOHIDElementValueHeader *)buffPtr;
        header->cookie = element->getCookie();
        
        bcopy(element->getDataValue()->getBytesNoCopy(), header->value, valueSize);
        
        buffPtr += sizeof(IOHIDElementValueHeader) + valueSize;
    }
    
    ret = SetElementValues(elements->getCount(), values);
    require_noerr_action(ret, exit, HIDServiceLogError("SetElementValues failed: 0x%x", ret));
    
exit:
    OSSafeReleaseNULL(values);
    
    return ret;
}

kern_return_t IOHIDInterface::getElementValues(OSArray *elements)
{
    kern_return_t ret = kIOReturnError;
    IOBufferMemoryDescriptor *values = NULL;
    uint32_t totalSize = 0;
    uint8_t *buffer = NULL;
    uint8_t *buffPtr = NULL;
    
    require_action(elements && elements->getCount(), exit, ret = kIOReturnBadArgument);
    
    // first get the total size of all of the element values
    for (unsigned int i = 0; i < elements->getCount(); i++) {
        IOHIDElementPrivate *element;
        
        element = (IOHIDElementPrivate *)elements->getObject(i);
        if (!element) {
            continue;
        }
        
        totalSize += sizeof(IOHIDElementValueHeader) + element->getByteSize();
    }
    
    require_action(totalSize, exit, ret = kIOReturnBadArgument);
    
    ret = IOBufferMemoryDescriptor::Create(kIOMemoryDirectionInOut, totalSize, 0, &values);
    require_noerr_action(ret, exit, ret = kIOReturnNoMemory);
    
    {
        uint64_t address;
        uint64_t length;
        ret = values->Map(0, 0, 0, IOVMPageSize, &address, &length);
        require_noerr_action(ret, exit, HIDServiceLogError("map failed: 0x%x", ret));
        
        buffer = (typeof(buffer))address;
    }
    
    buffPtr = buffer;
    
    // Next put the cookies in the buffer
    for (unsigned int i = 0; i < elements->getCount(); i++) {
        IOHIDElementPrivate *element;
        IOHIDElementValueHeader *header;
        uint32_t valueSize = 0;
        
        element = (IOHIDElementPrivate *)elements->getObject(i);
        if (!element) {
            continue;
        }
        
        valueSize = element->getByteSize();
        
        header = (IOHIDElementValueHeader *)buffPtr;
        header->cookie = element->getCookie();
        
        buffPtr += sizeof(IOHIDElementValueHeader) + valueSize;
    }
    
    // Get the updated elements from the device
    ret = GetElementValues(elements->getCount(), values);
    require_noerr_action(ret, exit, HIDServiceLogError("GetElementValues failed: 0x%x", ret));
    
    buffPtr = buffer;
    
    // Update our elements with the values returned from the device
    for (unsigned int i = 0; i < elements->getCount(); i++) {
        IOHIDElementPrivate *element;
        IOHIDElementValueHeader *header;
        uint32_t valueSize = 0;
        
        element = (IOHIDElementPrivate *)elements->getObject(i);
        if (!element) {
            continue;
        }
        
        valueSize = element->getByteSize();
        
        header = (IOHIDElementValueHeader *)buffPtr;
        
        if (header->cookie != element->getCookie()) {
            HIDLog("coookie mismatch hdr: %d element: %d", header->cookie, element->getCookie());
            continue;
        }
        
        OSData *data = OSData::withBytes(header->value, valueSize);
        if (data) {
            // update our element value (will not post to device)
            element->setDataBits(data);
            data->release();
        }
        
        buffPtr += sizeof(IOHIDElementValueHeader) + valueSize;
    }
    
exit:
    OSSafeReleaseNULL(values);
    
    return ret;
}

void IOHIDInterface::processReport(uint64_t timestamp,
                                   uint8_t *report,
                                   uint32_t reportLength,
                                   IOHIDReportType type,
                                   uint32_t reportID)
{
    _container->processReport(type,
                              reportID,
                              report,
                              reportLength,
                              timestamp,
                              NULL,
                              0);
}

bool IOHIDInterface::createElements()
{
    bool result = false;
    OSData *descriptor = NULL;
    unsigned int descLength = 0;
    IOReturn ret = kIOReturnError;
    IOBufferMemoryDescriptor *md = NULL;
    uint64_t address;
    uint64_t length;
    
    ret = CopyProperties(&_properties);
    require_noerr_action(ret, exit, HIDServiceLogError("provider->CopyProperties:%x\n", ret));
    
    descriptor = OSDynamicCast(OSData, OSDictionaryGetValue(_properties, kIOHIDReportDescriptorKey));
    require_action(descriptor, exit, HIDServiceLogError("no descriptor"));
    
    descLength = descriptor->getLength();
    
    _container = IOHIDInterfaceElementContainer::withDescriptor(
                        (void *)OSDataGetBytesPtr(descriptor, 0, descLength),
                        descLength,
                        this);
    require_action(_container, exit, HIDServiceLogError("failed to create descriptor container"));
    
    ret = GetSupportedCookies(&md);
    require_noerr_action(ret, exit, HIDServiceLogError("Failed to get supported cookies: 0x%x", ret));
    
    ret = md->Map(0, 0, 0, IOVMPageSize, &address, &length);
    require_noerr(ret, exit);
    
    _elements = OSArray::withCapacity(length/sizeof(uint32_t));
    require(_elements, exit);
    
    for (uint32_t i = 0; i < length/sizeof(uint32_t); i++) {
        IOHIDElement *element = NULL;
        uint32_t cookie = ((uint32_t *)address)[i];
        
        element = (IOHIDElement *)_container->getElements()->getObject(cookie);
        
        if (!element) {
            HIDServiceLogError("Failed to find element for cookie %d", cookie);
            continue;
        }
        
        _elements->setObject(element);
    }
    
    result = true;
    
exit:
    if (!result) {
        HIDServiceLogFault("Failed to create elements");
    }
    
    OSSafeReleaseNULL(md);
    
    return result;
}
