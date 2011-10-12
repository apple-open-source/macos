/*
 * Copyright (c) 1998-2011 Apple Inc. All rights reserved.
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
#include <IOKit/storage/IODVDBlockStorageDevice.h>

#define	super	IOCDBlockStorageDevice
OSDefineMetaClassAndAbstractStructors(IODVDBlockStorageDevice,IOCDBlockStorageDevice)

bool
IODVDBlockStorageDevice::init(OSDictionary * properties)
{
    bool result;

    result = super::init(properties);
    if (result) {
        setProperty(kIOBlockStorageDeviceTypeKey,kIOBlockStorageDeviceTypeDVD);
    }

    return(result);
}

#ifndef __LP64__
IOReturn
IODVDBlockStorageDevice::readDVDStructure(IOMemoryDescriptor *buffer,const DVDStructureFormat format,
                                        const UInt32 address,const UInt8 layer,const UInt8 agid)
{
    return(kIOReturnUnsupported);
}
#endif /* !__LP64__ */

#ifdef __LP64__
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice,  0);
#else /* !__LP64__ */
OSMetaClassDefineReservedUsed(IODVDBlockStorageDevice,  0);
#endif /* !__LP64__ */
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice,  1);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice,  2);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice,  3);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice,  4);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice,  5);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice,  6);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice,  7);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice,  8);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice,  9);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 10);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 11);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 12);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 13);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 14);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 15);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 16);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 17);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 18);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 19);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 20);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 21);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 22);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 23);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 24);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 25);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 26);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 27);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 28);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 29);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 30);
OSMetaClassDefineReservedUnused(IODVDBlockStorageDevice, 31);
