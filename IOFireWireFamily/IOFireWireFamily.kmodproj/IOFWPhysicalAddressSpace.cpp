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
#include <IOKit/firewire/IOFireWireController.h>

#include "FWDebugging.h"

#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IODMACommand.h>

/*
 * Direct physical memory <-> FireWire address.
 * Accesses to these addresses will be handled automatically by the
 * hardware without notification.
 *
 * The following is currently true, though the code no longer makes such assumptions :
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

// init
//
//

bool IOFWPhysicalAddressSpace::init( IOFireWireBus * bus )
{
	bool success = true;		// assume success
	
	// init super
	
    if( !IOFWAddressSpace::init( bus ) )
        success = false;
		
	return success;
}

// createAuxiliary
//
// virtual method for creating auxiliary object.  subclasses needing to subclass 
// the auxiliary object can override this.

IOFWAddressSpaceAux * IOFWPhysicalAddressSpace::createAuxiliary( void )
{
	IOFWPhysicalAddressSpaceAux * auxiliary;
    
	auxiliary = OSTypeAlloc( IOFWPhysicalAddressSpaceAux );

    if( auxiliary != NULL && !auxiliary->init(this) ) 
	{
        auxiliary->release();
        auxiliary = NULL;
    }
	
    return (IOFWAddressSpaceAux*)auxiliary;
}

// checkMemoryInRange
//
//

IOReturn IOFWPhysicalAddressSpace::checkMemoryInRange( IOMemoryDescriptor * memory )
{
	IOReturn status = kIOReturnSuccess;

	if( memory == NULL )
	{
		status = kIOReturnBadArgument;
	}
	
	//
	// setup
	//
	
	bool memory_prepared = false;
	if( status == kIOReturnSuccess )
	{
		status = memory->prepare( kIODirectionInOut );
	}
	
	if( status == kIOReturnSuccess )
	{
		memory_prepared = true;
	}
	
	UInt64 length = 0;
	if( status == kIOReturnSuccess )
	{
		length = memory->getLength();
		if( length == 0 )
		{
			status = kIOReturnError;
		}
	}
	
	//
	// create IODMACommand
	//
	
	IODMACommand * dma_command = NULL;
	if( status == kIOReturnSuccess )
	{
		dma_command = IODMACommand::withSpecification( 
												kIODMACommandOutputHost64,		// segment function
												64,								// max address bits
												length,							// max segment size
												(IODMACommand::MappingOptions)(IODMACommand::kMapped | IODMACommand::kIterateOnly),		// IO mapped & don't bounce buffer
												length,							// max transfer size
												0,								// page alignment
												NULL,							// mapper
												NULL );							// refcon
		if( dma_command == NULL )
			status = kIOReturnError;
		
	}
	
	if( status == kIOReturnSuccess )
	{
		// set memory descriptor and don't prepare it
		status = dma_command->setMemoryDescriptor( memory, false ); 
	}	

	bool dma_command_prepared = false;
	if( status == kIOReturnSuccess )
	{
		status = dma_command->prepare( 0, length, true );
	}

	if( status == kIOReturnSuccess )
	{
		dma_command_prepared = true;
	}
	
	//
	// check ranges
	//

	if( status == kIOReturnSuccess )
	{
		UInt64 offset = 0;
		UInt64 mask = fControl->getFireWirePhysicalAddressMask();
		while( (offset < length) && (status == kIOReturnSuccess) )
		{
			IODMACommand::Segment64 segments[10];
			UInt32 num_segments = 10;
			status = dma_command->gen64IOVMSegments( &offset, segments, &num_segments );
			if( status == kIOReturnSuccess )
			{
				for( UInt32 i = 0; i < num_segments; i++ )
				{
				//	IOLog( "checkSegments - segments[%d].fIOVMAddr = 0x%016llx, fLength = %d\n", i, segments[i].fIOVMAddr, segments[i].fLength  );
						
					if( (segments[i].fIOVMAddr & (~mask)) )
					{
				//		IOLog( "checkSegmentsFailed - 0x%016llx & 0x%016llx\n", segments[i].fIOVMAddr, mask );
						status = kIOReturnNotPermitted;
						break;
					}
				}
			}
		}
	}
	
	//
	// clean up
	//
	
	if( dma_command_prepared )
	{
		dma_command->complete();
		dma_command_prepared = false;
	}
		
	if( dma_command )
	{
		dma_command->clearMemoryDescriptor(); 
		dma_command->release();
		dma_command = NULL;
	}
	
	if( memory_prepared )
	{
		memory->complete();
		memory_prepared = false;
	}
	
	return status;

}

// initWithDesc
//
//

bool IOFWPhysicalAddressSpace::initWithDesc( IOFireWireBus *control,
                                             IOMemoryDescriptor * mem )
{
    if(!IOFWAddressSpace::init(control))
		return false;
		
//	IOLog( "IOFWPhysicalAddressSpace::initWithDesc\n" );
	
	IOReturn status = kIOReturnSuccess;
	
	if( status == kIOReturnSuccess )
	{
		if( mem != NULL )
		{
			status = checkMemoryInRange( mem );
		}
	}

//	IOLog( "IOFWPhysicalAddressSpace::initWithDesc (1) - status = 0x%08lx\n", status );
	
	IODMACommand * dma_command = NULL;
	if( status == kIOReturnSuccess )
	{
		UInt32 address_bits = fControl->getFireWirePhysicalAddressBits();
		dma_command = IODMACommand::withSpecification( 
												kIODMACommandOutputHost64,		// segment function
												address_bits,					// max address bits
												0,								// max segment size
												IODMACommand::kMapped,			// I/O mapped
												0,								// max transfer size
												0,								// no alignment
												NULL,							// mapper
												NULL );							// refcon
		if( dma_command == NULL )
			status = kIOReturnError;
		
	}

	if( status == kIOReturnSuccess )
	{
		setDMACommand( dma_command );
		dma_command->release();
		status = setMemoryDescriptor( mem );
	} 

//	IOLog( "IOFWPhysicalAddressSpace::initWithDesc (2) - status = 0x%08lx\n", status );
	
    return (status == kIOReturnSuccess);
}

// initWithDesc
//
//

bool IOFWPhysicalAddressSpace::initWithDMACommand(	IOFireWireBus * control,
													IODMACommand * command )
{
    if( !IOFWAddressSpace::init(control) )
        return false;

	setDMACommand( command );

    return true;
}

// free
//
//

void IOFWPhysicalAddressSpace::free()
{	
//	IOLog( "IOFWPhysicalAddressSpace::free\n" );
	
    IOFWAddressSpace::free();
}

// doRead
//
//

UInt32 IOFWPhysicalAddressSpace::doRead(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 len, 
					IOMemoryDescriptor **buf, IOByteCount * offset, IOFWRequestRefCon refcon)
{
    UInt32 res = kFWResponseAddressError;
    UInt64 pos;
    UInt64 phys;
	
	if( !isTrustedNode( nodeID ) )
		return kFWResponseAddressError;
	
	if( !isPrepared() )
		return kFWResponseAddressError;
		
	UInt64 address = ((UInt64)addr.addressHi << 32) | (UInt64)addr.addressLo;	
	UInt64 desc_length = getLength();
	
    pos = 0;
    while( pos < desc_length ) 
	{
		bool found = false;
		UInt64 lengthOfSegment;
        phys = getPhysicalSegment( pos, &lengthOfSegment );
		
		if( (address >= phys) && (address < (phys+lengthOfSegment)) )
		{
			UInt32 union_length = (lengthOfSegment - (address - phys));
			
			// check if the request extends beyond this physical segment
			if( len <= union_length )
			{
				found = true;
			}
			else
			{
				// look ahead for contiguous ranges
				
				UInt64 contiguous_address = (phys + lengthOfSegment);
				UInt64 contiguous_pos = (pos + lengthOfSegment);
				UInt64 contiguous_length = len - union_length;
				UInt64 contig_phys;
				
				while( contiguous_pos < desc_length )
				{
					contig_phys = getPhysicalSegment( contiguous_pos, &lengthOfSegment );
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
			*offset = (pos + address - phys);
            *buf = getMemoryDescriptor();
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
    UInt64 pos;
    UInt64 phys;

//	IOLog( "IOFWPhysicalAddressSpace::doWrite\n" );
	
	if( !isTrustedNode( nodeID ) )
		return kFWResponseAddressError;

	if( !isPrepared() )
	{
		return kFWResponseAddressError;
	}

	UInt64 address = ((UInt64)addr.addressHi << 32) | (UInt64)addr.addressLo;

	UInt64 desc_length = getLength();

    pos = 0;
    while(pos < desc_length) 
	{
		bool found = false;
		UInt64 lengthOfSegment;
        phys = getPhysicalSegment(pos, &lengthOfSegment);

//		IOLog( "IOFWPhysicalAddressSpace::doWrite - address = 0x%016llx phys = 0x%016llx\n", address, phys );
		
		if( (address >= phys) && (address < (phys+lengthOfSegment)) )
		{
			UInt32 union_length = (lengthOfSegment - (address - phys));
			
			// check if the request extends beyond this physical segment
			if( len <= union_length )
			{
				found = true;
			}
			else
			{
				// look ahead for contiguous ranges
				
				UInt64 contiguous_address = (phys + lengthOfSegment);
				UInt64 contiguous_pos = (pos + lengthOfSegment);
				UInt64 contiguous_length = len - union_length;
				UInt64 contig_phys;
				
				while( contiguous_pos < desc_length )
				{
					contig_phys = getPhysicalSegment( contiguous_pos, &lengthOfSegment );
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

			getMemoryDescriptor()->writeBytes( pos + (address - phys), buf, len);
			getDMACommand()->writeBytes( pos + (address - phys), buf, len );
				
			// make sure any bounce buffers have the new data
		//	synchronize( kIODirectionOut );

			res = kFWResponseComplete;
            break;
        }
		
        pos += lengthOfSegment;
    }

    return res;
}

// getMemoryDescriptor
//
//

IOMemoryDescriptor * IOFWPhysicalAddressSpace::getMemoryDescriptor( void )
{
	IOMemoryDescriptor * desc = NULL;
	
	IODMACommand * dma_command = getDMACommand();
	if( dma_command )
	{
		desc = (IOMemoryDescriptor*)dma_command->getMemoryDescriptor();
	}
	
	return desc;
}

// setMemoryDescriptor
//
//

IOReturn IOFWPhysicalAddressSpace::setMemoryDescriptor( IOMemoryDescriptor * descriptor )
{
	IOReturn status = kIOReturnSuccess;
		
	if( isPrepared() )
	{
		complete();
	}
	
	IODMACommand * dma_command = getDMACommand();
	if( dma_command == NULL )
		status = kIOReturnError;

	if( status == kIOReturnSuccess )
	{
		if( descriptor == NULL )
		{
			dma_command->clearMemoryDescriptor();
		}
		else
		{
			dma_command->clearMemoryDescriptor(); 
			status = dma_command->setMemoryDescriptor( descriptor, false );
			if( status == kIOReturnSuccess )
			{
				prepare();
			}
		}
	}
	
	return status;
}

// getLength
//
//

UInt64 IOFWPhysicalAddressSpace::getLength( void )
{
	UInt64 length = 0;
	
	IOMemoryDescriptor * desc = getMemoryDescriptor();
	if( desc )
	{
		length = desc->getLength();
	}
	
	return length;
}

/////////////////////////////////////////////////////////////////////////////////////
#pragma mark -

// init
//
//

bool IOFWPhysicalAddressSpaceAux::init( IOFWAddressSpace * primary )
{
	bool success = true;		// assume success
	
	// init super
	
    if( !IOFWAddressSpaceAux::init( primary ) )
        success = false;

//	IOLog( "IOFWPhysicalAddressSpaceAux::init\n" );
	
	if( success )
	{
		fDMACommand = NULL;
	}
	
	if( !success )
	{
	}
	
	return success;
}

// free
//
//

void IOFWPhysicalAddressSpaceAux::free()
{	
//	IOLog( "IOFWPhysicalAddressSpaceAux::free\n" );

	if( isPrepared() )
	{
		complete();
	}

	if( fDMACommand )
	{
		fDMACommand->clearMemoryDescriptor();
		fDMACommand->release();
		fDMACommand = NULL;
	}
	
	IOFWAddressSpaceAux::free();
}

// setDMACommand
//
//

void IOFWPhysicalAddressSpaceAux::setDMACommand( IODMACommand * dma_command ) 
{
	if( fDMACommandPrepared )
	{
		complete();
	}

	IODMACommand * old = fDMACommand;
	fDMACommand = dma_command;
	
	if( fDMACommand )
	{
		fDMACommand->retain();
	}
	
	if( old )
	{
		old->release();
	}
}

// setDMACommand
//
//

IODMACommand * IOFWPhysicalAddressSpaceAux::getDMACommand( void ) 
{
	return fDMACommand;
}

// isPrepared
//
//

bool IOFWPhysicalAddressSpaceAux::isPrepared( void )
{
	return fDMACommandPrepared;
}

// getPhysicalSegment
//
//

UInt64 IOFWPhysicalAddressSpaceAux::getPhysicalSegment( UInt64 offset, UInt64 * length ) 
{
	IOReturn status = kIOReturnSuccess;
	
	UInt64 	phys = 0;

	IODMACommand::Segment64 segment;
	UInt32 numSegments = 1;
	UInt64 pos = offset;
	if( status == kIOReturnSuccess )
	{
		status = fDMACommand->gen64IOVMSegments( &pos, &segment, &numSegments );
	}
	
	if( status == kIOReturnSuccess )
	{
		if( numSegments != 1 )
		{
			status = kIOReturnNoMemory;
		}
	}

	if( status == kIOReturnSuccess )
	{
		phys = segment.fIOVMAddr;
		*length = segment.fLength;
	}

	return phys;
}

// prepare
//
//

IOReturn IOFWPhysicalAddressSpaceAux::prepare( void )
{
	IOReturn status = kIOReturnSuccess;

//	IOLog( "IOFWPhysicalAddressSpaceAux::prepare\n" );

	if( !fDMACommandPrepared )
	{
		UInt64 desc_length = ((IOFWPhysicalAddressSpace*)fPrimary)->getLength();
		status = fDMACommand->prepare( 0, desc_length );
		if( status == kIOReturnSuccess )
		{
			fDMACommandPrepared = true;
		}
	}
	
	return status;
}

// complete
//
//

IOReturn IOFWPhysicalAddressSpaceAux::complete( void )
{
	IOReturn status = kIOReturnSuccess;

//	IOLog( "IOFWPhysicalAddressSpaceAux::complete\n" );

	if( !fDMACommandPrepared )
	{
		status = kIOReturnNotReady;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = fDMACommand->complete();
		if( status == kIOReturnSuccess )
		{
			fDMACommandPrepared = false;
		}
	}
	
	return status;	
}

// synchronize
//
//

IOReturn IOFWPhysicalAddressSpaceAux::synchronize( IOOptionBits options )
{
	IOReturn status = kIOReturnSuccess;

//	IOLog( "IOFWPhysicalAddressSpaceAux::synchronize - direction = %d\n", direction );

	if( !fDMACommandPrepared )
	{
		status = kIOReturnNotReady;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = fDMACommand->synchronize( options );
	}
	
	return status;	
}

// getSegments
//
//

IOReturn IOFWPhysicalAddressSpaceAux::getSegments( UInt64 * offset, FWSegment * fw_segments, UInt32 * num_segments )
{
	IOReturn status = kIOReturnSuccess;
	
	IODMACommand::Segment64	* vm_segments = NULL;
	UInt32 vm_segments_size = 0;
	
	if( (offset == NULL) || (fw_segments == NULL) || (num_segments == NULL) )
	{
		status = kIOReturnBadArgument;
	}
	
	if( status == kIOReturnSuccess )
	{
		vm_segments_size = sizeof(IODMACommand::Segment64) * (*num_segments);
		vm_segments = (IODMACommand::Segment64*)IOMalloc( vm_segments_size );
		if( vm_segments == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		IODMACommand * dma_command = getDMACommand();
		status = dma_command->gen64IOVMSegments( offset, vm_segments, num_segments );
	}
	
	if( status == kIOReturnSuccess )
	{
		for( UInt32 i = 0; i < *num_segments; i++ )
		{
			fw_segments[i].length = vm_segments[i].fLength;
			fw_segments[i].address.nodeID = 0x0000;		// invalid node id
			fw_segments[i].address.addressHi = (vm_segments[i].fIOVMAddr >> 32) & 0x000000000000ffffULL;
			fw_segments[i].address.addressLo = vm_segments[i].fIOVMAddr & 0x00000000ffffffffULL;
		}
	}

	if( fw_segments != NULL )
	{
		IOFree( vm_segments, vm_segments_size );
		vm_segments = NULL;
	}
	
	return status;
}
