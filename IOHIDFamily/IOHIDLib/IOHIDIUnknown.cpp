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
#include <stdatomic.h>
#include <os/log.h>

int IOHIDIUnknown::factoryRefCount = 0;


void IOHIDIUnknown::factoryAddRef()
{
    CFUUIDRef factoryId = kIOHIDDeviceFactoryID;
    CFPlugInAddInstanceForFactory(factoryId);
}

void IOHIDIUnknown::factoryRelease()
{
    CFUUIDRef factoryId = kIOHIDDeviceFactoryID;
    CFPlugInRemoveInstanceForFactory(factoryId);

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
    return atomic_fetch_add((_Atomic UInt32*)&refCount, 1) + 1;
}

UInt32 IOHIDIUnknown::release()
{
    UInt32 retVal = atomic_fetch_sub((_Atomic UInt32*)&refCount, 1);
    
    if (retVal < 1) {
        os_log_fault(OS_LOG_DEFAULT, "Over Release IOHIDIUnknown Reference");
    } else if (retVal == 1) {
        delete this;
    }
    
    return retVal - 1;
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
