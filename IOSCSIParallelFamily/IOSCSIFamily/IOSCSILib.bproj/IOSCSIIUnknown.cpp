/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
#include "IOSCSIIUnknown.h"
#include "IOSCSIDeviceClass.h"

int IOSCSIIUnknown::factoryRefCount = 0;

void *IOSCSILibFactory(CFAllocatorRef allocator, CFUUIDRef typeID)
{
    if (CFEqual(typeID, kIOSCSIUserClientTypeID))
        return (void *) IOSCSIDeviceClass::alloc();
    else
        return NULL;
}

void IOSCSIIUnknown::factoryAddRef()
{
    if (0 == factoryRefCount++) {
        CFUUIDRef factoryId = kIOSCSIFactoryID;

        CFRetain(factoryId);
        CFPlugInAddInstanceForFactory(factoryId);
    }
}

void IOSCSIIUnknown::factoryRelease()
{
    if (1 == factoryRefCount--) {
        CFUUIDRef factoryId = kIOSCSIFactoryID;
    
        CFPlugInRemoveInstanceForFactory(factoryId);
        CFRelease(factoryId);
    }
    else if (factoryRefCount < 0)
        factoryRefCount = 0;
}

IOSCSIIUnknown::IOSCSIIUnknown(void *unknownVTable)
: refCount(1)
{
    iunknown.pseudoVTable = (IUnknownVTbl *) unknownVTable;
    iunknown.obj = this;

    factoryAddRef();
};

IOSCSIIUnknown::~IOSCSIIUnknown()
{
    factoryRelease();
}

unsigned long IOSCSIIUnknown::addRef()
{
    refCount += 1;
    return refCount;
}

unsigned long IOSCSIIUnknown::release()
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

HRESULT IOSCSIIUnknown::
genericQueryInterface(void *self, REFIID iid, void **ppv)
{
    IOSCSIIUnknown *me = ((InterfaceMap *) self)->obj;
    return me->queryInterface(iid, ppv);
}

unsigned long IOSCSIIUnknown::genericAddRef(void *self)
{
    IOSCSIIUnknown *me = ((InterfaceMap *) self)->obj;
    return me->addRef();
}

unsigned long IOSCSIIUnknown::genericRelease(void *self)
{
    IOSCSIIUnknown *me = ((InterfaceMap *) self)->obj;
    return me->release();
}
