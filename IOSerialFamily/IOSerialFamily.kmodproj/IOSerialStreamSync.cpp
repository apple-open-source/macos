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
/*
 * IOSerialStreamSync.cpp
 *
 * 2000-10-21	gvdl	Initial real change to IOKit serial family.
 *
 */

#include "IOSerialDriverSync.h"

#include "IOSerialStreamSync.h"
#include "IORS232SerialStreamSync.h"
#include "IOModemSerialStreamSync.h"

OSDefineMetaClassAndAbstractStructors(IOSerialDriverSync, IOService);
OSMetaClassDefineReservedUnused(IOSerialDriverSync,  0);
OSMetaClassDefineReservedUnused(IOSerialDriverSync,  1);
OSMetaClassDefineReservedUnused(IOSerialDriverSync,  2);
OSMetaClassDefineReservedUnused(IOSerialDriverSync,  3);
OSMetaClassDefineReservedUnused(IOSerialDriverSync,  4);
OSMetaClassDefineReservedUnused(IOSerialDriverSync,  5);
OSMetaClassDefineReservedUnused(IOSerialDriverSync,  6);
OSMetaClassDefineReservedUnused(IOSerialDriverSync,  7);
OSMetaClassDefineReservedUnused(IOSerialDriverSync,  8);
OSMetaClassDefineReservedUnused(IOSerialDriverSync,  9);
OSMetaClassDefineReservedUnused(IOSerialDriverSync, 10);
OSMetaClassDefineReservedUnused(IOSerialDriverSync, 11);
OSMetaClassDefineReservedUnused(IOSerialDriverSync, 12);
OSMetaClassDefineReservedUnused(IOSerialDriverSync, 13);
OSMetaClassDefineReservedUnused(IOSerialDriverSync, 14);
OSMetaClassDefineReservedUnused(IOSerialDriverSync, 15);

OSDefineMetaClassAndStructors(IOSerialStreamSync, IOService);
OSMetaClassDefineReservedUnused(IOSerialStreamSync,  0);
OSMetaClassDefineReservedUnused(IOSerialStreamSync,  1);
OSMetaClassDefineReservedUnused(IOSerialStreamSync,  2);
OSMetaClassDefineReservedUnused(IOSerialStreamSync,  3);
OSMetaClassDefineReservedUnused(IOSerialStreamSync,  4);
OSMetaClassDefineReservedUnused(IOSerialStreamSync,  5);
OSMetaClassDefineReservedUnused(IOSerialStreamSync,  6);
OSMetaClassDefineReservedUnused(IOSerialStreamSync,  7);
OSMetaClassDefineReservedUnused(IOSerialStreamSync,  8);
OSMetaClassDefineReservedUnused(IOSerialStreamSync,  9);
OSMetaClassDefineReservedUnused(IOSerialStreamSync, 10);
OSMetaClassDefineReservedUnused(IOSerialStreamSync, 11);
OSMetaClassDefineReservedUnused(IOSerialStreamSync, 12);
OSMetaClassDefineReservedUnused(IOSerialStreamSync, 13);
OSMetaClassDefineReservedUnused(IOSerialStreamSync, 14);
OSMetaClassDefineReservedUnused(IOSerialStreamSync, 15);

OSDefineMetaClassAndStructors(IORS232SerialStreamSync, IOSerialStreamSync);
OSDefineMetaClassAndStructors(IOModemSerialStreamSync, IOSerialStreamSync);

#define super IOService

bool IOSerialStreamSync::init(OSDictionary *dictionary, void *refCon)
{
    if (!super::init(dictionary))
        return false;

    fRefCon = refCon;
    return true;
}

bool IOSerialStreamSync::attach(IOService *provider)
{    
    if (!super::attach(provider))
        return false;
    
    fProvider = OSDynamicCast(IOSerialDriverSync, provider);
    if (fProvider) 
        return true;
    else {
        super::detach(provider);
        return false;
    }
}

IOReturn IOSerialStreamSync::
acquirePort(bool sleep)
    { return fProvider->acquirePort(sleep, fRefCon); }

IOReturn IOSerialStreamSync::
releasePort()
    { return fProvider->releasePort(fRefCon); }

IOReturn IOSerialStreamSync::
setState(UInt32 state, UInt32 mask)
    { return fProvider->setState(state, mask, fRefCon); }

UInt32 IOSerialStreamSync::
getState()
    { return fProvider->getState(fRefCon); }

IOReturn IOSerialStreamSync::
watchState(UInt32 *state, UInt32 mask)
    { return fProvider->watchState(state, mask, fRefCon); }

UInt32 IOSerialStreamSync::
nextEvent()
    { return fProvider->nextEvent(fRefCon); }

IOReturn IOSerialStreamSync::
executeEvent(UInt32 event, UInt32 data)
    { return fProvider->executeEvent(event, data, fRefCon); }

IOReturn IOSerialStreamSync::
requestEvent(UInt32 event, UInt32 *data)
    { return fProvider->requestEvent(event, data, fRefCon); }

IOReturn IOSerialStreamSync::
enqueueEvent(UInt32 event, UInt32 data, bool sleep)
    { return fProvider->enqueueEvent(event, data, sleep, fRefCon); }

IOReturn IOSerialStreamSync::
dequeueEvent(UInt32 *event, UInt32 *data, bool sleep)
    { return fProvider->dequeueEvent(event, data, sleep, fRefCon); }

IOReturn IOSerialStreamSync::
enqueueData(UInt8 *buffer,  UInt32 size, UInt32 *count, bool sleep)
    { return fProvider->enqueueData(buffer, size, count, sleep, fRefCon); }

IOReturn IOSerialStreamSync::
dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min)
    { return fProvider->dequeueData(buffer, size, count, min, fRefCon); }
