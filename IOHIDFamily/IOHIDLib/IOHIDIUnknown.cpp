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

#include <TargetConditionals.h>

#include <IOKit/hid/IOHIDDevicePlugIn.h>
#include <IOKit/hid/IOHIDServicePlugIn.h>
#include "IOHIDIUnknown.h"
#include "IOHIDDeviceClass.h"
#include "IOHIDUPSClass.h"

#if TARGET_OS_EMBEDDED
    #include "IOHIDEventServiceClass.h"
#endif


int IOHIDIUnknown::factoryRefCount = 0;

void *IOHIDLibFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeID)
{
    if (CFEqual(typeID, kIOHIDDeviceUserClientTypeID))
        return (void *) IOHIDObsoleteDeviceClass::alloc();
    else if (CFEqual(typeID, kIOHIDDeviceTypeID))
        return (void *) IOHIDDeviceClass::alloc();
#if TARGET_OS_EMBEDDED
    else if (CFEqual(typeID, kIOHIDServicePlugInTypeID))
        return (void *) IOHIDEventServiceClass::alloc();
#endif
    else if (CFEqual(typeID, kIOUPSPlugInTypeID))
        return (void *) IOHIDUPSClass::alloc();
    else
        return NULL;
}

void IOHIDIUnknown::factoryAddRef()
{
    if (0 == factoryRefCount++) {
        CFUUIDRef factoryId = kIOHIDDeviceFactoryID;

        CFRetain(factoryId);
        CFPlugInAddInstanceForFactory(factoryId);
    }
}

void IOHIDIUnknown::factoryRelease()
{
    if (1 == factoryRefCount--) {
        CFUUIDRef factoryId = kIOHIDDeviceFactoryID;
    
        CFPlugInRemoveInstanceForFactory(factoryId);
        CFRelease(factoryId);
    }
    else if (factoryRefCount < 0)
        factoryRefCount = 0;
}

IOHIDIUnknown::IOHIDIUnknown(void *unknownVTable)
: refCount(1)
{
    iunknown.pseudoVTable = (IUnknownVTbl *) unknownVTable;
    iunknown.obj = this;

    factoryAddRef();
};

IOHIDIUnknown::~IOHIDIUnknown()
{
    factoryRelease();
}

UInt32 IOHIDIUnknown::addRef()
{
    refCount += 1;
    return refCount;
}

UInt32 IOHIDIUnknown::release()
{
    UInt32 retVal = refCount - 1;

    if (retVal > 0)
        refCount = retVal;
    else if (retVal == 0) {
        refCount = retVal;
        delete this;
    }
    else
        retVal = 0;

    return retVal;
}

HRESULT IOHIDIUnknown::
genericQueryInterface(void *self, REFIID iid, void **ppv)
{
    IOHIDIUnknown *me = ((InterfaceMap *) self)->obj;
    return me->queryInterface(iid, ppv);
}

UInt32 IOHIDIUnknown::genericAddRef(void *self)
{
    IOHIDIUnknown *me = ((InterfaceMap *) self)->obj;
    return me->addRef();
}

UInt32 IOHIDIUnknown::genericRelease(void *self)
{
    IOHIDIUnknown *me = ((InterfaceMap *) self)->obj;
    return me->release();
}
