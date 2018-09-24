/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2017 Apple Computer, Inc.  All Rights Reserved.
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

#include "IOHIDEventServiceFastPathClass.h"
#include <IOKit/hid/IOHIDEvent.h>
#include <IOKit/IOCFSerialize.h>
#include "IOHIDEventServiceFastPathUserClient.h"
#include "IOHIDEventData.h"
#include "IOHIDPrivateKeys.h"
#include "IOHIDDebug.h"

//===========================================================================
// CFPlugIn Static Assignments
//===========================================================================
IOCFPlugInInterface IOHIDEventServiceFastPathClass::sIOCFPlugInInterfaceV1 =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    1, 0,	// version/revision
    &IOHIDEventServiceFastPathClass::_probe,
    &IOHIDEventServiceFastPathClass::_start,
    &IOHIDEventServiceFastPathClass::_stop
};

IOHIDServiceFastPathInterface IOHIDEventServiceFastPathClass::sIOHIDServiceFastPathInterface =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    &IOHIDEventServiceFastPathClass::_open,
    &IOHIDEventServiceFastPathClass::_close,
    &IOHIDEventServiceFastPathClass::_copyProperty,
    &IOHIDEventServiceFastPathClass::_setProperty,
    &IOHIDEventServiceFastPathClass::_copyEvent,
};


//===========================================================================
// CONSTRUCTOR / DESTRUCTOR methods
//===========================================================================
//---------------------------------------------------------------------------
// IOHIDEventServiceClass
//---------------------------------------------------------------------------
IOHIDEventServiceFastPathClass::IOHIDEventServiceFastPathClass() : IOHIDIUnknown(&sIOCFPlugInInterfaceV1)
{
    _hidService.pseudoVTable    = NULL;
    _hidService.obj             = this;
    
    _service                    = MACH_PORT_NULL;
    _connect                    = MACH_PORT_NULL;
    _client                     = MACH_PORT_NULL;
    _sharedMemory               = NULL;
    _sharedMemorySize           = 0;

}

//---------------------------------------------------------------------------
// ~IOHIDEventServiceClass
//---------------------------------------------------------------------------
IOHIDEventServiceFastPathClass::~IOHIDEventServiceFastPathClass()
{
    // finished with the shared memory
    if (_sharedMemory)
    {
#if !__LP64__
        vm_address_t        mappedMem = (vm_address_t)_sharedMemory;
#else
        mach_vm_address_t   mappedMem = (mach_vm_address_t)_sharedMemorySize;
#endif
        IOConnectUnmapMemory (_connect,
                              0,
                              mach_task_self(),
                              mappedMem);
        _sharedMemory       = NULL;
        _sharedMemorySize   = 0;
    }
    
    if (_service) {
        IOObjectRelease(_service);
        _service = MACH_PORT_NULL;
    }
    if (_client) {
        IOObjectRelease(_client);
        _client = MACH_PORT_NULL;
    }
    if (_connect) {
        IOServiceClose(_connect);
        _connect = MACH_PORT_NULL;
    }
}

//===========================================================================
// IOCFPlugInInterface methods
//===========================================================================
IOReturn IOHIDEventServiceFastPathClass::_probe(void *self, CFDictionaryRef propertyTable, io_service_t service, SInt32 *order)
{
    return getThis(self)->probe(propertyTable, service, order);
}

IOReturn IOHIDEventServiceFastPathClass::_start(void *self, CFDictionaryRef propertyTable, io_service_t service)
{
    return getThis(self)->start(propertyTable, service);
}

IOReturn IOHIDEventServiceFastPathClass::_stop(void *self)
{
    return getThis(self)->stop();
}

boolean_t IOHIDEventServiceFastPathClass::_open(void * self, IOOptionBits options, CFDictionaryRef property)
{
    return getThis(self)->open(options, property);
}

void IOHIDEventServiceFastPathClass::_close(void * self, IOOptionBits options)
{
    getThis(self)->close(options);
}

CFTypeRef IOHIDEventServiceFastPathClass::_copyProperty(void * self, CFStringRef key)
{
    return getThis(self)->copyProperty(key);
}

boolean_t IOHIDEventServiceFastPathClass::_setProperty(void * self, CFStringRef key, CFTypeRef property)
{
    return getThis(self)->setProperty(key, property);
}

IOHIDEventRef IOHIDEventServiceFastPathClass::_copyEvent(void *self, CFTypeRef copySpec, IOOptionBits options)
{
    return getThis(self)->copyEvent(copySpec, options);
}


// Public Methods
//---------------------------------------------------------------------------
// IOHIDEventServiceFastPathClass::alloc
//---------------------------------------------------------------------------
IOCFPlugInInterface ** IOHIDEventServiceFastPathClass::alloc()
{
    IOHIDEventServiceFastPathClass * self = new IOHIDEventServiceFastPathClass;
    
    return self ? (IOCFPlugInInterface **) &self->iunknown.pseudoVTable : NULL;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceFastPathClass::queryInterface
//---------------------------------------------------------------------------
HRESULT IOHIDEventServiceFastPathClass::queryInterface(REFIID iid, void **ppv)
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT res = S_OK;
    
    if (CFEqual(uuid, IUnknownUUID) || CFEqual(uuid, kIOCFPlugInInterfaceID))
    {
        *ppv = &iunknown;
        addRef();
    } else if (CFEqual(uuid, kIOHIDServiceFastPathInterfaceID))
    {
        _hidService.pseudoVTable    = (IUnknownVTbl *)  &sIOHIDServiceFastPathInterface;
        _hidService.obj             = this;
        *ppv = &_hidService;
        addRef();
    } else {
        *ppv = 0;
    }
    
    if (!*ppv) {
        res = E_NOINTERFACE;
    }
    CFRelease(uuid);
    return res;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceFastPathClass::probe
//---------------------------------------------------------------------------
IOReturn IOHIDEventServiceFastPathClass::probe(CFDictionaryRef propertyTable __unused, io_service_t service, SInt32 * order __unused)
{
    if (!service || !IOObjectConformsTo(service, "IOHIDEventService"))
        return kIOReturnBadArgument;
    
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceFastPathClass::start
//---------------------------------------------------------------------------
IOReturn IOHIDEventServiceFastPathClass::start(CFDictionaryRef propertyTable __unused, io_service_t service)
{
    IOReturn   ret;
    
    _service = service;
    IOObjectRetain(service);
    
    ret = IOServiceOpen(_service, mach_task_self(), kIOHIDEventServiceFastPathUserClientType, &_connect);
    if (ret) {
        HIDLogError("IOServiceOpen(kIOHIDEventServiceFastPathUserClientType): 0x%x", ret);
    }
    ret = IOConnectGetService(_connect, &_client);
    if (ret) {
        HIDLogError("IOConnectGetService(kIOHIDEventServiceFastPathUserClientType): 0x%x", ret);
    }
    return ret;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceFastPathClass::stop
//---------------------------------------------------------------------------
IOReturn IOHIDEventServiceFastPathClass::stop()
{
    return true;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceFastPathClass::open
//---------------------------------------------------------------------------
boolean_t IOHIDEventServiceFastPathClass::open(IOOptionBits options, CFDictionaryRef property)
{
    void *      inputData     = NULL;
    size_t      inputDataSize = 0;
    IOReturn    ret;
    CFDataRef   data = IOCFSerialize (property , kIOCFSerializeToBinary);
    if (data) {
        inputData = (void*)CFDataGetBytePtr(data);
        inputDataSize = CFDataGetLength(data);
    }
    uint64_t input = options;
    ret = IOConnectCallMethod(_connect, kIOHIDEventServiceFastPathUserClientOpen, &input, 1, inputData, inputDataSize, NULL, NULL, NULL, NULL);
    if (data) {
        CFRelease(data);
    }
    if (ret) {
        HIDLogError("IOConnectCallMethod (kIOHIDEventServiceFastPathUserClientOpen): 0x%x", ret);
        return  false;
    }
    uint32_t sharedMemorySize = 0;
    CFNumberRef value = (CFNumberRef)copyProperty(CFSTR(kIOHIDEventServiceQueueSize));
    if (value) {
        CFNumberGetValue (value, kCFNumberSInt32Type, &sharedMemorySize);
        CFRelease(value);
    }
    if (sharedMemorySize) {
        // allocate the memory
#if !__LP64__
        vm_address_t        address = static_cast<vm_address_t>(0);
        vm_size_t           size    = 0;
#else
        mach_vm_address_t   address = static_cast<mach_vm_address_t>(0);
        mach_vm_size_t      size    = 0;
#endif
        ret = IOConnectMapMemory (_connect, 0, mach_task_self(), &address, &size, kIOMapAnywhere);
        _sharedMemory = (void *) address;
        _sharedMemorySize = size;
        if (ret != kIOReturnSuccess || !_sharedMemory || !_sharedMemorySize) {
             HIDLogError("IOConnectMapMemory (sharedMemorySize:%d): 0x%x", sharedMemorySize, ret);
            return false;
        }
    }
    
    return true;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceFastPathClass::close
//---------------------------------------------------------------------------
void IOHIDEventServiceFastPathClass::close(IOOptionBits options)
{
    uint64_t input = options;
    IOReturn ret = IOConnectCallScalarMethod(_connect, kIOHIDEventServiceFastPathUserClientClose, &input, 1, NULL, NULL);
    if (ret) {
        HIDLogError("IOConnectCallMethod (kIOHIDEventServiceFastPathUserClientClose): 0x%x", ret);
    }
}

//---------------------------------------------------------------------------
// IOHIDEventServiceFastPathClass::copyProperty
//---------------------------------------------------------------------------
CFTypeRef IOHIDEventServiceFastPathClass::copyProperty(CFStringRef key)
{
    CFTypeRef value = IORegistryEntryCreateCFProperty (_client, key, kCFAllocatorDefault, 0);
    return value;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceFastPathClass::setProperty
//---------------------------------------------------------------------------
boolean_t IOHIDEventServiceFastPathClass::setProperty(CFStringRef key, CFTypeRef property)
{
    IOReturn ret;
    
    ret = IOConnectSetCFProperty(_connect, key, property);
    if (ret) {
        HIDLogError("IOConnectSetCFProperty: 0x%x", ret);
    }
    return (ret == kIOReturnSuccess);
}




//---------------------------------------------------------------------------
// IOHIDEventServiceFastPathClass::copyEvent
//---------------------------------------------------------------------------
IOHIDEventRef IOHIDEventServiceFastPathClass::copyEvent(CFTypeRef copySpec, IOOptionBits options)
{
    uint64_t            input[2]        = {options, kIOHIDEventServiceFastPathCopySpecSerializedType};
    const UInt8 *       inputData       = NULL;
    size_t              inputDataSize   = 0;
    IOHIDEventRef       event           = NULL;
    IOReturn            ret             = kIOReturnSuccess;
    CFDataRef           data            = NULL;
    
    if (copySpec) {
        if (CFGetTypeID(copySpec) == CFDataGetTypeID()) {
            data = (CFDataRef) CFRetain(copySpec);
            input[1] = kIOHIDEventServiceFastPathCopySpecDataType;
        } else {
            data = IOCFSerialize(copySpec, kIOCFSerializeToBinary);
        }
        if (data) {
            inputData = CFDataGetBytePtr(data);
            inputDataSize = CFDataGetLength(data);
        }
    }
    
    do {
        if (!_sharedMemory) {
            HIDLogError("No shared memory");
            break;
        }
        
        *(uint32_t *)_sharedMemory = 0;
        
        ret = IOConnectCallMethod(_connect, kIOHIDEventServiceFastPathUserClientCopyEvent, input, 2, inputData, inputDataSize, NULL, NULL, NULL, NULL);
        if ( ret != kIOReturnSuccess ) {
            HIDLogError("IOConnectCallMethod (kIOHIDEventServiceFastPathUserClientCopyEvent): 0x%x (copySpec = %@)", ret, copySpec);
            break;
        }

        if (*(uint32_t *)_sharedMemory) {
            event = IOHIDEventCreateWithBytes(kCFAllocatorDefault, (const UInt8*)_sharedMemory + sizeof(uint32_t), *(uint32_t *)_sharedMemory);
        }
    } while (0);
   
    
    if (data) {
        CFRelease(data);
    }
    return event;
}


