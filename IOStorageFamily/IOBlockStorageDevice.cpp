/*
 * Copyright (c) 1998-2007 Apple Inc.  All Rights Reserved.
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
#include <IOKit/IOLib.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/storage/IOBlockStorageDevice.h>

#define	super	IOService
OSDefineMetaClassAndAbstractStructors(IOBlockStorageDevice,IOService)

bool
IOBlockStorageDevice::init(OSDictionary * properties)
{
    bool result;

    result = super::init(properties);
    if (result) {
        result = setProperty(kIOBlockStorageDeviceTypeKey,
                             kIOBlockStorageDeviceTypeGeneric);
    }
    
    return(result);
}

/* Route a user-client getter request.  We should make a full-
 * fledged user-client for IOBlockStorageDevice at some point.
 */
OSObject *
IOBlockStorageDevice::getProperty(const OSSymbol * key) const
{
    OSObject *obj = NULL;

    if (key->isEqualTo(kIOBlockStorageDeviceWriteCacheStateKey)) {
        bool enabled;
        IOReturn result;

        result = ((IOBlockStorageDevice *)this)->getWriteCacheState(&enabled);
        if (result == kIOReturnSuccess) {
            obj = (enabled) ? kOSBooleanTrue : kOSBooleanFalse;
        }
/*  } else if (key->isEqualTo(...)) {
 *      obj = ...;
 */
    } else {
        obj = super::getProperty(key);
    }

    return(obj);
}

/* Route a user-client setter request.  We should make a full-
 * fledged user-client for IOBlockStorageDevice at some point.
 */
IOReturn
IOBlockStorageDevice::setProperties(OSObject * properties)
{
    OSDictionary *dict;
    OSObject *obj;
    IOReturn result;

    result = super::setProperties(properties);
    if (result != kIOReturnUnsupported) {
        return(result);
    }

    result = IOUserClient::clientHasPrivilege(current_task(),kIOClientPrivilegeAdministrator);
    if (result != kIOReturnSuccess) {
        return(result);
    }

    dict = OSDynamicCast(OSDictionary,properties);
    if (!dict) {
        return(kIOReturnBadArgument);
    }

    obj = dict->getObject(kIOBlockStorageDeviceWriteCacheStateKey);
    if (obj) {
        if (OSDynamicCast(OSBoolean,obj)) {
            result = setWriteCacheState((obj == kOSBooleanTrue));
        } else {
            result = kIOReturnBadArgument;
        }
        return(result);
    }

/*  obj = dict->getObject(...);
 *  if (obj) {
 *      result = ...;
 *      return(result);
 *  }
 */

    return(kIOReturnUnsupported);
}

IOReturn
IOBlockStorageDevice::doSyncReadWrite(IOMemoryDescriptor *buffer,
                                      UInt32 block,UInt32 nblks)
{
    return(kIOReturnUnsupported);
}

IOReturn
IOBlockStorageDevice::doAsyncReadWrite(IOMemoryDescriptor *buffer,
                                       UInt32 block, UInt32 nblks,
                                       IOStorageCompletion completion)
{
    return(kIOReturnUnsupported);
}

IOReturn
IOBlockStorageDevice::doAsyncReadWrite(IOMemoryDescriptor *buffer,
                                       UInt64 block,UInt64 nblks,
                                       IOStorageCompletion completion)
{
    if ((block >> 32) || (nblks >> 32)) {
        return(kIOReturnUnsupported);
    } else {
        return(doAsyncReadWrite(buffer,(UInt32)block,(UInt32)nblks,completion));
    }
}

OSMetaClassDefineReservedUsed(IOBlockStorageDevice, 0);

IOReturn
IOBlockStorageDevice::getWriteCacheState(bool *enabled)
{
    return(kIOReturnUnsupported);
}

OSMetaClassDefineReservedUsed(IOBlockStorageDevice, 1);

IOReturn
IOBlockStorageDevice::setWriteCacheState(bool enabled)
{
    return(kIOReturnUnsupported);
}

OSMetaClassDefineReservedUsed(IOBlockStorageDevice, 2);

IOReturn
IOBlockStorageDevice::doAsyncReadWrite(IOMemoryDescriptor *buffer,
                                       UInt64 block,UInt64 nblks,
                                       IOStorageAttributes *attributes,
                                       IOStorageCompletion *completion)
{
    if (attributes && attributes->options) {
        return(kIOReturnUnsupported);
    } else {
        return(doAsyncReadWrite(buffer,block,nblks,completion ? *completion : (IOStorageCompletion) { 0 }));
    }
}

OSMetaClassDefineReservedUsed(IOBlockStorageDevice, 3);

IOReturn
IOBlockStorageDevice::requestIdle(void)
{
    return(kIOReturnUnsupported);
}

OSMetaClassDefineReservedUsed(IOBlockStorageDevice, 4);

IOReturn
IOBlockStorageDevice::doDiscard(UInt64 block, UInt64 nblks)
{
    return(kIOReturnUnsupported);
}

OSMetaClassDefineReservedUsed(IOBlockStorageDevice, 5);

OSMetaClassDefineReservedUnused(IOBlockStorageDevice,  6);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice,  7);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice,  8);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice,  9);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 10);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 11);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 12);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 13);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 14);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 15);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 16);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 17);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 18);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 19);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 20);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 21);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 22);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 23);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 24);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 25);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 26);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 27);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 28);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 29);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 30);
OSMetaClassDefineReservedUnused(IOBlockStorageDevice, 31);
