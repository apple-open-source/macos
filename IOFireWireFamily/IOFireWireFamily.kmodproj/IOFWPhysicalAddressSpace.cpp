/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/firewire/IOFWAddressSpace.h>

#include "FWDebugging.h"

/*
 * Direct physical memory <-> FireWire address.
 * Accesses to these addresses will be handled automatically by the
 * hardware without notification.
 *
 * The 64 bit FireWire address of (32 bit) physical addr xxxx:xxxx is hostNode:0000:xxxx:xxxx
 */

OSDefineMetaClassAndStructors(IOFWPhysicalAddressSpaceAux, IOFWAddressSpaceAux);

OSMetaClassDefineReservedUnused(IOFWPhysicalAddressSpaceAux, 0);
OSMetaClassDefineReservedUnused(IOFWPhysicalAddressSpaceAux, 1);
OSMetaClassDefineReservedUnused(IOFWPhysicalAddressSpaceAux, 2);
OSMetaClassDefineReservedUnused(IOFWPhysicalAddressSpaceAux, 3);
OSMetaClassDefineReservedUnused(IOFWPhysicalAddressSpaceAux, 4);
OSMetaClassDefineReservedUnused(IOFWPhysicalAddressSpaceAux, 5);
OSMetaClassDefineReservedUnused(IOFWPhysicalAddressSpaceAux, 6);
OSMetaClassDefineReservedUnused(IOFWPhysicalAddressSpaceAux, 7);
OSMetaClassDefineReservedUnused(IOFWPhysicalAddressSpaceAux, 8);
OSMetaClassDefineReservedUnused(IOFWPhysicalAddressSpaceAux, 9);

#pragma mark -

OSDefineMetaClassAndStructors(IOFWPhysicalAddressSpace, IOFWAddressSpace)

// initWithDesc
//
//

bool IOFWPhysicalAddressSpace::initWithDesc(IOFireWireBus *control,
                                            IOMemoryDescriptor *mem)
{
    if(!IOFWAddressSpace::init(control))
        return false;

    fMem = mem;
    fMem->retain();
    fLen = mem->getLength();

    return true;
}

// createAuxiliary
//
// virtual method for creating auxiliary object.  subclasses needing to subclass 
// the auxiliary object can override this.

IOFWAddressSpaceAux * IOFWPhysicalAddressSpace::createAuxiliary( void )
{
	IOFWPhysicalAddressSpaceAux * auxiliary;
    
	auxiliary = new IOFWPhysicalAddressSpaceAux;

    if( auxiliary != NULL && !auxiliary->init(this) ) 
	{
        auxiliary->release();
        auxiliary = NULL;
    }
	
    return auxiliary;
}

// free
//
//

void IOFWPhysicalAddressSpace::free()
{
    if(fMem)
        fMem->release();

    IOFWAddressSpace::free();
}

// doRead
//
//

UInt32 IOFWPhysicalAddressSpace::doRead(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 len, 
					IOMemoryDescriptor **buf, IOByteCount * offset, IOFWRequestRefCon refcon)
{
    UInt32 res = kFWResponseAddressError;
    vm_size_t pos;
    IOPhysicalAddress phys;
	
	if( !isTrustedNode( nodeID ) )
		return kFWResponseAddressError;
		
    if(addr.addressHi != 0)
		return kFWResponseAddressError;

    pos = 0;
    while(pos < fLen) 
	{
        IOPhysicalLength lengthOfSegment;
        phys = fMem->getPhysicalSegment(pos, &lengthOfSegment);
        if(addr.addressLo >= phys && addr.addressLo+len <= phys+lengthOfSegment) 
		{
            // OK, block is in space and is within one VM page
			// Set position to exact start
			*offset = pos + addr.addressLo - phys;
            *buf = fMem;
            res = kFWResponseComplete;
            break;
        }
		
        pos += lengthOfSegment;
    }

    return res;
}

// doWrite
//
//

UInt32 IOFWPhysicalAddressSpace::doWrite(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 len,
                                         const void *buf, IOFWRequestRefCon refcon)
{
    UInt32 res = kFWResponseAddressError;

    vm_size_t pos;
    IOPhysicalAddress phys;

	if( !isTrustedNode( nodeID ) )
		return kFWResponseAddressError;
	
    if(addr.addressHi != 0)
		return kFWResponseAddressError;

    pos = 0;
    while(pos < fLen) 
	{
        IOPhysicalLength lengthOfSegment;
        phys = fMem->getPhysicalSegment(pos, &lengthOfSegment);
        if(addr.addressLo >= phys && addr.addressLo+len <= phys+lengthOfSegment) 
		{
            // OK, block is in space and is within one VM page
			// Set position to exact start
            
			fMem->writeBytes(pos + addr.addressLo - phys, buf, len);
            res = kFWResponseComplete;
            
			break;
        }
        pos += lengthOfSegment;
    }
	
    return res;
}
