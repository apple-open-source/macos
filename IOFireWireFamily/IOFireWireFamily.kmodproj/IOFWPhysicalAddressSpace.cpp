/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
		bool found = false;
		IOPhysicalLength lengthOfSegment;
        phys = fMem->getPhysicalSegment(pos, &lengthOfSegment);
		
		if( (addr.addressLo >= phys) && (addr.addressLo < (phys+lengthOfSegment)) )
		{
			UInt32 union_length = (lengthOfSegment - (addr.addressLo - phys));
			
			// check if the request extends beyond this physical segment
			if( len <= union_length )
			{
				found = true;
			}
			else
			{
				// look ahead for contiguous ranges
				
				IOPhysicalAddress contiguous_address = (phys + lengthOfSegment);
				vm_size_t contiguous_pos = (pos + lengthOfSegment);
				UInt32 contiguous_length = len - union_length;
				IOPhysicalAddress contig_phys;
				
				while( contiguous_pos < fLen )
				{
					contig_phys = fMem->getPhysicalSegment( contiguous_pos, &lengthOfSegment );
					if( contiguous_address != contig_phys )
					{	
						// not contiguous, bail
						break;
					}
					
					if( contiguous_length <= lengthOfSegment )
					{
						// fits in this segment - success
						found = true;
						break;
					}

					contiguous_length -= lengthOfSegment;
					contiguous_pos += lengthOfSegment;
					contiguous_address += lengthOfSegment;
				}
				
			}
		}

		if( found )
		{
            // OK, block is in space
			// Set position to exact start
			*offset = (pos + addr.addressLo - phys);
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
		bool found = false;
		IOPhysicalLength lengthOfSegment;
        phys = fMem->getPhysicalSegment(pos, &lengthOfSegment);
		
		if( (addr.addressLo >= phys) && (addr.addressLo < (phys+lengthOfSegment)) )
		{
			UInt32 union_length = (lengthOfSegment - (addr.addressLo - phys));
			
			// check if the request extends beyond this physical segment
			if( len <= union_length )
			{
				found = true;
			}
			else
			{
				// look ahead for contiguous ranges
				
				IOPhysicalAddress contiguous_address = (phys + lengthOfSegment);
				vm_size_t contiguous_pos = (pos + lengthOfSegment);
				UInt32 contiguous_length = len - union_length;
				IOPhysicalAddress contig_phys;
				
				while( contiguous_pos < fLen )
				{
					contig_phys = fMem->getPhysicalSegment( contiguous_pos, &lengthOfSegment );
					if( contiguous_address != contig_phys )
					{	
						// not contiguous, bail
						break;
					}
					
					if( contiguous_length <= lengthOfSegment )
					{
						// fits in this segment - success
						found = true;
						break;
					}

					contiguous_length -= lengthOfSegment;
					contiguous_pos += lengthOfSegment;
					contiguous_address += lengthOfSegment;
				}
				
			}
		}

		if( found )
		{
            // OK, block is in space

			fMem->writeBytes( pos + (addr.addressLo - phys), buf, len);
            res = kFWResponseComplete;
            break;
        }
		
        pos += lengthOfSegment;
    }

    return res;
}