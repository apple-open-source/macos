/*
 * Copyright © 1998-2012 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#include "IOUSBIUnknown.h"
#include "IOUSBDeviceClass.h"
#include "IOUSBInterfaceClass.h"
#include <IOKit/usb/IOUSBLib.h>

int IOUSBIUnknown::factoryRefCount = 0;

void 
*IOUSBLibFactory(CFAllocatorRef allocator, CFUUIDRef typeID)
{
#pragma unused (allocator)
   if (CFEqual(typeID, kIOUSBDeviceUserClientTypeID))
        return (void *) IOUSBDeviceClass::alloc();
    else if (CFEqual(typeID, kIOUSBInterfaceUserClientTypeID))
        return (void *) IOUSBInterfaceClass::alloc();
    return NULL;
}



void 
IOUSBIUnknown::factoryAddRef()
{
    if (0 == factoryRefCount++) 
    {
        CFUUIDRef factoryId = kIOUSBFactoryID;

        CFRetain(factoryId);
        CFPlugInAddInstanceForFactory(factoryId);
    }
}



void 
IOUSBIUnknown::factoryRelease()
{
    if (1 == factoryRefCount--) 
	{
        CFUUIDRef factoryId = kIOUSBFactoryID;
    
        CFPlugInRemoveInstanceForFactory(factoryId);
        CFRelease(factoryId);
    }
    else if (factoryRefCount < 0)
        factoryRefCount = 0;
}



IOUSBIUnknown::IOUSBIUnknown(void *unknownVTable)
: refCount(1)
{
    iunknown.pseudoVTable = (IUnknownVTbl *) unknownVTable;
    iunknown.obj = this;

    factoryAddRef();
};



IOUSBIUnknown::~IOUSBIUnknown()
{
    factoryRelease();
}



UInt32 
IOUSBIUnknown::addRef()
{
    refCount += 1;
    return refCount;
}



UInt32 
IOUSBIUnknown::release()
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



HRESULT IOUSBIUnknown::
genericQueryInterface(void *self, REFIID iid, void **ppv)
{
    IOUSBIUnknown *me = ((InterfaceMap *) self)->obj;
    return me->queryInterface(iid, ppv);
}



UInt32 
IOUSBIUnknown::genericAddRef(void *self)
{
    IOUSBIUnknown *me = ((InterfaceMap *) self)->obj;
    return me->addRef();
}



UInt32 
IOUSBIUnknown::genericRelease(void *self)
{
    IOUSBIUnknown *me = ((InterfaceMap *) self)->obj;
    return me->release();
}
