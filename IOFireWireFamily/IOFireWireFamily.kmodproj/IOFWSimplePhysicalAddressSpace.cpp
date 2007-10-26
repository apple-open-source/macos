/*
 *
 * Copyright (c) 1998-2006 Apple Computer, Inc. All rights reserved.
 *
 */
 
#ifndef _OPEN_SOURCE_

// public
#import <IOKit/IOLib.h>
#import "IOFireWireController.h"

// private
#import "IOFWSimplePhysicalAddressSpace.h"

// fun with binary compatibility

OSDefineMetaClassAndStructors( IOFWSimplePhysicalAddressSpace, IOFWPhysicalAddressSpace )

OSMetaClassDefineReservedUnused(IOFWSimplePhysicalAddressSpace, 0);
OSMetaClassDefineReservedUnused(IOFWSimplePhysicalAddressSpace, 1);
OSMetaClassDefineReservedUnused(IOFWSimplePhysicalAddressSpace, 2);
OSMetaClassDefineReservedUnused(IOFWSimplePhysicalAddressSpace, 3);
OSMetaClassDefineReservedUnused(IOFWSimplePhysicalAddressSpace, 4);
OSMetaClassDefineReservedUnused(IOFWSimplePhysicalAddressSpace, 5);
OSMetaClassDefineReservedUnused(IOFWSimplePhysicalAddressSpace, 6);
OSMetaClassDefineReservedUnused(IOFWSimplePhysicalAddressSpace, 7);
OSMetaClassDefineReservedUnused(IOFWSimplePhysicalAddressSpace, 8);
OSMetaClassDefineReservedUnused(IOFWSimplePhysicalAddressSpace, 9);

struct MemberVariables
{
	IODirection					fDirection;
	bool						fContiguous;
	
	IOBufferMemoryDescriptor *	fDescriptor;
	bool						fDescriptorPrepared;
	
	IOMemoryMap *				fMap;
	
	IOVirtualAddress			fVirtualAddress;
	
	UInt32						fLength;
};

#define _members ((MemberVariables*)fSimplePhysSpaceMembers)

// init
//
//

bool IOFWSimplePhysicalAddressSpace::init( IOFireWireBus * control, vm_size_t size, IODirection direction, bool contiguous )
{
	bool success = IOFWPhysicalAddressSpace::init( control );
	
	fSimplePhysSpaceMembers = NULL;
		
	if( success )
	{
		success = createMemberVariables();
	}
	
	if( success )
	{
		_members->fLength = size;
		_members->fDirection = direction;
		_members->fContiguous = contiguous;
	}
	
	if( success )
	{
		IOReturn status = allocateMemory();
		if( status != kIOReturnSuccess )
			success = false;
	}
	
	return success;
}

// createMemberVariables
//
//

bool IOFWSimplePhysicalAddressSpace::createMemberVariables( void )
{
	bool success = true;
	
	if( fSimplePhysSpaceMembers == NULL )
	{
		// create member variables
		
		if( success )
		{
			fSimplePhysSpaceMembers = IOMalloc( sizeof(MemberVariables) );
			if( fSimplePhysSpaceMembers == NULL )
				success = false;
		}
		
		// zero member variables
		
		if( success )
		{
			bzero( fSimplePhysSpaceMembers, sizeof(MemberVariables) );
			
			// largely redundant
			_members->fLength = 0;
			_members->fDirection = kIODirectionNone;
			_members->fDescriptor = NULL;
			_members->fDescriptorPrepared = false;
			_members->fMap = NULL;
			_members->fVirtualAddress = 0;
			_members->fContiguous = false;
		}
		
		// clean up on failure
		
		if( !success )
		{
			destroyMemberVariables();
		}
	}
	
	return success;
}

// destroyMemberVariables
//
//

void IOFWSimplePhysicalAddressSpace::destroyMemberVariables( void )
{
	if( fSimplePhysSpaceMembers != NULL )
	{
		IOFree( fSimplePhysSpaceMembers, sizeof(MemberVariables) );
		fSimplePhysSpaceMembers = NULL;
	}
}

// free
//
//

void IOFWSimplePhysicalAddressSpace::free( void )
{
	setDMACommand( NULL );
	
	deallocateMemory();
	
	destroyMemberVariables();
	
	IOFWPhysicalAddressSpace::free();
}

#pragma mark -

// allocateMemory
//
//

IOReturn IOFWSimplePhysicalAddressSpace::allocateMemory( void )
{
	IOReturn status = kIOReturnSuccess;
	
	if( _members->fLength == 0 )
	{
		status = kIOReturnBadArgument;
	}
	
	//
	// create memory descriptor
	//
	
	if( status == kIOReturnSuccess )
	{
		IOOptionBits options = 0;

		if( _members->fContiguous )
		{
			options |= kIOMemoryPhysicallyContiguous;
		}
		
		UInt64 mask = fControl->getFireWirePhysicalAddressMask();
		
	//	IOLog( "IOFWSimplePhysicalAddressSpace::allocateMemory - options = 0x%08lx mask = 0x%016llx\n", options, mask );
		
		_members->fDescriptor = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(	
																kernel_task,		// kernel task
																options,			// options
																_members->fLength,	// size
																mask );				// mask for physically addressable memory
		if( _members->fDescriptor == NULL )
			status = kIOReturnNoMemory;
	}

//	IOLog( "IOFWSimplePhysicalAddressSpace::allocateMemory - _members->fDescriptor = 0x%08lx status = 0x%08lx\n", (UInt32)_members->fDescriptor, (UInt32)status  );

	//
	// wire the memory
	//
	
	if( status == kIOReturnSuccess )
	{
	//	IOLog( "IOFWSimplePhysicalAddressSpace::allocateMemory - addr = 0x%08lx\n", (UInt32)((IOBufferMemoryDescriptor*)_members->fDescriptor)->getBytesNoCopy() );
		// in 10.0 you had to manually set the length. I don't know if that's still true.
		((IOBufferMemoryDescriptor*)_members->fDescriptor)->setLength( _members->fLength );
		
		status = _members->fDescriptor->prepare( _members->fDirection );
	}

	if( status == kIOReturnSuccess )
	{
		_members->fDescriptorPrepared = true;
	}
	
	//
	// get the physical address
	//
	
	IODMACommand * dma_command = NULL;
	if( status == kIOReturnSuccess )
	{
		UInt32 address_bits = fControl->getFireWirePhysicalAddressBits();
		dma_command = IODMACommand::withSpecification( 
												kIODMACommandOutputHost64,		// segment function
												address_bits,					// max address bits
												_members->fLength,				// max segment size
												(IODMACommand::MappingOptions)(IODMACommand::kMapped | IODMACommand::kIterateOnly),	 // IO mapped & don't bounce buffer
												_members->fLength,				// max transfer size
												0,								// page alignment
												NULL,							// mapper
												NULL );							// refcon
		if( dma_command == NULL )
			status = kIOReturnNoMemory;
	}

	if( status == kIOReturnSuccess )
	{
		setDMACommand( dma_command );
		dma_command->release();
		status = setMemoryDescriptor( _members->fDescriptor );
	}

	//
	// get the virtual address
	//
	
	if( status == kIOReturnSuccess )
	{
		_members->fMap = _members->fDescriptor->map();
		if( _members->fMap == NULL )
		{
			status = kIOReturnNoMemory;
		}
	}
	
	if( status == kIOReturnSuccess )
	{
		_members->fVirtualAddress = _members->fMap->getVirtualAddress();
		if( _members->fVirtualAddress == NULL )
		{
			status = kIOReturnNoMemory;
		}
	}

 //IOLog( "IOFWSimplePhysicalAddressSpace::allocateMemory - fVirtualAddress = 0x%08lx status = 0x%08lx\n", (UInt32)_members->fVirtualAddress, (UInt32)status  );
	
	//
	// zero the buffer
	//
	
	if( status == kIOReturnSuccess )
	{
		bzero( (void *)_members->fVirtualAddress, _members->fLength );
	}
	
	return status;
}

// deallocateMemory
//
//

void IOFWSimplePhysicalAddressSpace::deallocateMemory( void )
{
	if( _members->fMap != NULL )
	{
		_members->fMap->release();
		_members->fMap = NULL;
	}
	
	if( _members->fDescriptorPrepared )
	{
		_members->fDescriptor->complete();
		_members->fDescriptorPrepared = false;
	}

	if( _members->fDescriptor )
	{
		_members->fDescriptor->release();
		_members->fDescriptor = NULL;
	}
	
	_members->fVirtualAddress = 0;
}

#pragma mark -

// getVirtualAddress
//
//

IOVirtualAddress IOFWSimplePhysicalAddressSpace::getVirtualAddress( void )
{	
	return _members->fVirtualAddress;
}

#endif
