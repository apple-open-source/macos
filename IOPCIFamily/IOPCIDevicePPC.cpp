/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 * 23 Nov 98 sdouglas created from objc version.
 */

#ifdef __ppc__

#include <IOKit/system.h>

#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pci/IOPCIDevice.h>

#include <IOKit/IOLib.h>
#include <IOKit/assert.h>

#include <libkern/OSByteOrder.h>
#include <libkern/c++/OSContainers.h>


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

UInt32 IOPCIDevice::ioRead32( UInt16 offset, IOMemoryMap * map )
{
    UInt32	value;

    if (0 == map)
    {
        map = ioMap;
        if (0 == map)
            return (0);
    }

    value = OSReadSwapInt32( (volatile void *)map->getVirtualAddress(), offset);
    eieio();

    return (value);
}

UInt16 IOPCIDevice::ioRead16( UInt16 offset, IOMemoryMap * map )
{
    UInt16	value;

    if (0 == map)
    {
        map = ioMap;
        if (0 == map)
            return (0);
    }

    value = OSReadSwapInt16( (volatile void *)map->getVirtualAddress(), offset);
    eieio();

    return (value);
}

UInt8 IOPCIDevice::ioRead8( UInt16 offset, IOMemoryMap * map )
{
    UInt32	value;

    if (0 == map)
    {
        map = ioMap;
        if (0 == map)
            return (0);
    }

    value = ((volatile UInt8 *) map->getVirtualAddress())[ offset ];
    eieio();

    return (value);
}

void IOPCIDevice::ioWrite32( UInt16 offset, UInt32 value,
                             IOMemoryMap * map )
{
    if (0 == map)
    {
        map = ioMap;
        if (0 == map)
            return ;
    }

    OSWriteSwapInt32( (volatile void *)map->getVirtualAddress(), offset, value);
    eieio();
}

void IOPCIDevice::ioWrite16( UInt16 offset, UInt16 value,
                             IOMemoryMap * map )
{
    if (0 == map)
    {
        map = ioMap;
        if (0 == map)
            return ;
    }

    OSWriteSwapInt16( (volatile void *)map->getVirtualAddress(), offset, value);
    eieio();
}

void IOPCIDevice::ioWrite8( UInt16 offset, UInt8 value,
                            IOMemoryMap * map )
{
    if (0 == map)
    {
        map = ioMap;
        if (0 == map)
            return ;
    }

    ((volatile UInt8 *) map->getVirtualAddress())[ offset ] = value;
    eieio();
}

#endif // __ppc__

