/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

// FIXME
// #include <IOKit/hid/IOHIDLib.h>
#include "IOHIDLib.h"

#include "IOHIDIUnknown.h"
#include "IOHIDDeviceClass.h"

int IOHIDIUnknown::factoryRefCount = 0;

void *IOHIDLibFactory(CFAllocatorRef allocator, CFUUIDRef typeID)
{
    if (CFEqual(typeID, kIOHIDDeviceUserClientTypeID))
        return (void *) IOHIDDeviceClass::alloc();
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

unsigned long IOHIDIUnknown::addRef()
{
    refCount += 1;
    return refCount;
}

unsigned long IOHIDIUnknown::release()
{
    unsigned long retVal = refCount - 1;

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

unsigned long IOHIDIUnknown::genericAddRef(void *self)
{
    IOHIDIUnknown *me = ((InterfaceMap *) self)->obj;
    return me->addRef();
}

unsigned long IOHIDIUnknown::genericRelease(void *self)
{
    IOHIDIUnknown *me = ((InterfaceMap *) self)->obj;
    return me->release();
}
