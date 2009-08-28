/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#if __ppc__

#include <IOKit/IOLib.h>

#include <IOKit/IOBufferMemoryDescriptor.h>
#include "FWDebugging.h"

#include <IOKit/sbp2/IOFireWireSBP2LSIWorkaroundDescriptor.h>
#include <IOKit/sbp2/IOFireWireSBP2ORB.h>

#define kMinPacketSize 16
#define kLowestPacketLimit 256

/////////////////////////////////////////////////////////////////////////////////////////////

// IOFireWireSBP2LSIRange
//
// this class keeps track of physical ranges

//
// The algorithm needs to keep track of a series of physical segments.  This
// is accomplished by keeping a series of IOFireWireSBP2LSIRange instances in
// an OSArray.  The OSArray gives us the ability to dynamically insert and
// remove from the set of segments.  
//
// Each IOFireWireSBP2LSIRange instance keeps track of the segment's physical
// address and length, along with the data's original descriptor and offset
// and potentially a second buffer's address for double buffered segments.
// The class supplies methods to aid in the creation and destruction of buffers
// and the syncronization of the buffer with the original data.
//

class IOFireWireSBP2LSIRange : public OSObject
{
	OSDeclareDefaultStructors(IOFireWireSBP2LSIRange)

protected:
	IOFireWireSBP2LSIWorkaroundDescriptor * parentDesc;

public:

	void * 						buffer;
	
	IOMemoryDescriptor *	memory;
	IOPhysicalAddress		address;
    IOByteCount				length;
	IOByteCount				offset;
	
	virtual void initWithParent( IOFireWireSBP2LSIWorkaroundDescriptor * desc );
	virtual void markForBuffering( void );
	virtual IOReturn allocateBuffer( IODirection direction );
	
	virtual IOReturn syncBufferForInput( void );
	virtual IOReturn syncBufferForOutput( void );
};

OSDefineMetaClassAndStructors(IOFireWireSBP2LSIRange, OSObject)

// initWithParent
//
//

void IOFireWireSBP2LSIRange::initWithParent( IOFireWireSBP2LSIWorkaroundDescriptor * desc )
{
	parentDesc = desc;
	buffer = NULL;
}

// markForBuffering
//
// if we get marked for buffering. we will alocate a
// double buffer when allocateBuffer is called

void IOFireWireSBP2LSIRange::markForBuffering( void )
{
	buffer = (void*)0xffffffff;
	address = NULL;
}

// allocateBuffer
//
// if markForBuffering was previously called, allocate
// a double buffer.

IOReturn IOFireWireSBP2LSIRange::allocateBuffer( IODirection /* direction */ )
{
	IOReturn status = kIOReturnSuccess;
	
	if( buffer == ((void*)0xffffffff) )
	{
		buffer = parentDesc->bufferAllocatorNewBuffer( &address );
		if( !buffer )
			status =  kIOReturnNoMemory;	
	}
		
	return status;
}

// syncBufferForInput
//
// copies data from the buffers to the original descriptor,
// if we are being buffered.

IOReturn IOFireWireSBP2LSIRange::syncBufferForInput( void )
{
	IOReturn status = kIOReturnSuccess;
	
	if( buffer )
	{
		memory->writeBytes( offset, buffer, length );
	}
		
	return status;
}

// syncBufferForOutput
//
// copies the data from the original descriptor to the buffer,
// if we are being buffered.

IOReturn IOFireWireSBP2LSIRange::syncBufferForOutput( void )
{
	IOReturn status = kIOReturnSuccess;

	if( buffer )
	{
		memory->readBytes( offset, buffer, length );
	}
		
	return status;
}

/////////////////////////////////////////////////////////////////////////////////////////////

OSDefineMetaClassAndStructors(IOFireWireSBP2LSIWorkaroundDescriptor, IOGeneralMemoryDescriptor)

OSMetaClassDefineReservedUnused(IOFireWireSBP2LSIWorkaroundDescriptor, 0);
OSMetaClassDefineReservedUnused(IOFireWireSBP2LSIWorkaroundDescriptor, 1);
OSMetaClassDefineReservedUnused(IOFireWireSBP2LSIWorkaroundDescriptor, 2);
OSMetaClassDefineReservedUnused(IOFireWireSBP2LSIWorkaroundDescriptor, 3);
OSMetaClassDefineReservedUnused(IOFireWireSBP2LSIWorkaroundDescriptor, 4);
OSMetaClassDefineReservedUnused(IOFireWireSBP2LSIWorkaroundDescriptor, 5);
OSMetaClassDefineReservedUnused(IOFireWireSBP2LSIWorkaroundDescriptor, 6);
OSMetaClassDefineReservedUnused(IOFireWireSBP2LSIWorkaroundDescriptor, 7);
OSMetaClassDefineReservedUnused(IOFireWireSBP2LSIWorkaroundDescriptor, 8);

/////////////////////////////////////////////////////////////////////////////////////////////
// range allocator
//
// allocates ranges from a permanent pool then by allocating memory if allowed

IOReturn IOFireWireSBP2LSIWorkaroundDescriptor::rangeAllocatorInitialize( UInt32 rangeCount )
{
	IOReturn status = kIOReturnSuccess;
	
	FWLSILOGALLOC( ("LSILOG : allocating %ld permanent ranges\n", rangeCount) );

	// init fields
	
	if( status == kIOReturnSuccess )
	{
		fAllocatedRangesCount = 0; // unnecessary
	
		fPermanentRanges = OSArray::withCapacity( rangeCount ? rangeCount : 1 );
		if( !fPermanentRanges )
			status = kIOReturnNoMemory;
	}
	
	// create permanent ranges
	
	if( status == kIOReturnSuccess )
	{
		UInt32 i;
		
		for( i = 0; 
			 status == kIOReturnSuccess && i < rangeCount; 
			 i++ )
		{
			IOFireWireSBP2LSIRange * range = NULL;
			
			if( status == kIOReturnSuccess )
			{
				range = new IOFireWireSBP2LSIRange;
				if( !range )
					status = kIOReturnNoMemory;
			}
			
			if( status == kIOReturnSuccess )
			{
				if( !fPermanentRanges->setObject( i, range ) )
					status = kIOReturnError;
			}
			
			if( range )
				range->release();
		}
		
		FWKLOGASSERT( rangeCount == i );				
	}
	
	FWLSILOGALLOC( ("LSILOG : successfully allocated %ld permanent ranges, status = 0x%08lx\n",
													fPermanentRanges->getCount(), status) );

	return status;
}

void IOFireWireSBP2LSIWorkaroundDescriptor::rangeAllocatorDeallocateAllRanges( void )
{
	FWLSILOGALLOC( ("LSILOG : reset allocated ranges count. new allocCount = 0\n") );

	fAllocatedRangesCount = 0;
}

IOFireWireSBP2LSIRange * IOFireWireSBP2LSIWorkaroundDescriptor::rangeAllocatorNewRange( void )
{
	IOFireWireSBP2LSIRange * range = NULL;
	
	// use a preallocated range if possible
	if( fAllocatedRangesCount < fPermanentRanges->getCount() )
	{		
		range = (IOFireWireSBP2LSIRange *) 
					fPermanentRanges->getObject( fAllocatedRangesCount );
	
		range->retain();	// 1 ref for us and 1 ref for them
		
		fAllocatedRangesCount++;
		
		FWLSILOGALLOC( ("LSILOG : allocating from permanent ranges.  new allocCount = %ld\n", 
						fAllocatedRangesCount ) );
	} 
	else if( !fFixedCapacity ) // create one if we can
	{
		FWLSILOGALLOC( ("LSILOG : creating new range\n") );
		range = new IOFireWireSBP2LSIRange; 
	}
	
	return range;
}

void IOFireWireSBP2LSIWorkaroundDescriptor::rangeAllocatorFree( void )
{
	FWLSILOGALLOC( ("LSILOG : free all ranges\n") );
	if( fPermanentRanges )
		fPermanentRanges->release();  // releases all the ranges, too
}

/////////////////////////////////////////////////////////////////////////////////////////////
// buffer allocator
//
// allocates buffers from a permanent pool then by allocating memory if allowed.
// dishes out buffers in kMinPacketSize*2 increments

IOReturn IOFireWireSBP2LSIWorkaroundDescriptor::bufferAllocatorInitialize
															( IOByteCount requestedBufferSize )
{
	IOReturn status = kIOReturnSuccess;
	
	// calc page count
	IOByteCount bufferSize = ((requestedBufferSize + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1));
	UInt32 pageCount = bufferSize / PAGE_SIZE;
	
	FWKLOGASSERT( (PAGE_SIZE % (kMinPacketSize*2)) == 0 );

	FWLSILOGALLOC( ("LSILOG : allocating %ld permanent pages of buffer\n", pageCount) );

	if( status == kIOReturnSuccess )
	{
		fAllocatedBytesCount = 0; // unnecessary
		
		fBufferDescriptors = OSArray::withCapacity( pageCount ? pageCount : 1 );
		if( !fBufferDescriptors )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		UInt32 i;
		
		for( i = 0; 
			 status == kIOReturnSuccess && i < pageCount; 
			 i++ )
		{
			::IOBufferMemoryDescriptor *	bufferDesc = NULL;
			
			if( status == kIOReturnSuccess )
			{
                bufferDesc = ::IOBufferMemoryDescriptor::withOptions( kIODirectionOutIn | kIOMemoryUnshared, PAGE_SIZE, PAGE_SIZE ); 
				if( !bufferDesc )
					status = kIOReturnNoMemory;
			}
			
			if( status == kIOReturnSuccess )
			{
				status = bufferDesc->prepare();
			}
			
			if( status == kIOReturnSuccess )
			{
				if( !fBufferDescriptors->setObject( i, bufferDesc ) )
					status = kIOReturnError;
			}
			
			if( bufferDesc )
				bufferDesc->release();
		}
		
		FWKLOGASSERT( pageCount == i );
		
		// we use i instead of rangeCount so we free correctly on error
		fPermanentPages = i;				
	}
	
	FWLSILOGALLOC( ("LSILOG : successfully allocated %ld permanent pages, status = 0x%08lx\n", 
								fPermanentPages, status) );

	return status;
}

void IOFireWireSBP2LSIWorkaroundDescriptor::bufferAllocatorDeallocateAllBuffers( void )
{
	// it's all free now
	fAllocatedBytesCount = 0;
	
	FWLSILOGALLOC( ("LSILOG : reset allocated bytes count. new allocByteCount = 0\n") );

	FWLSILOGALLOC( ("LSILOG : freeing %ld pages of non-permanent buffer\n", 
									fBufferDescriptors->getCount() - fPermanentPages) );
	
	// remove all but the permanent pages
	UInt32 i = 0;
	while( (i = fBufferDescriptors->getCount()) > fPermanentPages )
	{
		fBufferDescriptors->removeObject(i-1);
	}
}

void * IOFireWireSBP2LSIWorkaroundDescriptor::bufferAllocatorNewBuffer(
											IOPhysicalAddress * address )
{
	IOReturn status = kIOReturnSuccess;
	void * buffer = NULL;
	
	::IOBufferMemoryDescriptor *	bufferDesc = NULL;

	UInt32 aligned_page = fAllocatedBytesCount & ~(PAGE_SIZE-1);
	UInt32 page = aligned_page / PAGE_SIZE;
	UInt32 offset = fAllocatedBytesCount - aligned_page;
	
	fAllocatedBytesCount += kMinPacketSize*2;   // max possible buffer size
	
	FWKLOGASSERT( page == ((fAllocatedBytesCount) & ~(PAGE_SIZE-1)) );
	
	if( page < fBufferDescriptors->getCount() )
	{
		bufferDesc = (::IOBufferMemoryDescriptor *) 
										fBufferDescriptors->getObject( page );
										
		FWLSILOGALLOC( ("LSILOG : allocating from permanent pages. new allocByteCount = %ld\n", 
														fAllocatedBytesCount) );
	}
	else if( !fFixedCapacity )
	{
		FWLSILOGALLOC( ("LSILOG : creating new page. new allocByteCount = %ld\n", 
														fAllocatedBytesCount) );
														
		bufferDesc = ::IOBufferMemoryDescriptor::withOptions( kIODirectionOutIn | kIOMemoryUnshared, PAGE_SIZE, PAGE_SIZE ); 
		if( !bufferDesc )
			status = kIOReturnNoMemory;
		
		if( status == kIOReturnSuccess )
		{
			bufferDesc->prepare();
		}
		
		if( status == kIOReturnSuccess )
		{
			if( !fBufferDescriptors->setObject( page, bufferDesc ) )
				status = kIOReturnError;
		}
		
		if( bufferDesc )
			bufferDesc->release();
	
	}
	
	if( status == kIOReturnSuccess )
	{
		IOByteCount seg_size = 0;
		*address = bufferDesc->getPhysicalSegment( 0, &seg_size ) + offset;
		buffer = (void*)((UInt32)bufferDesc->getBytesNoCopy() + offset);		
	}
	
	FWLSILOGALLOC( ("LSILOG : return buffer = 0x%lx\n", buffer) );

	return buffer;
}

void IOFireWireSBP2LSIWorkaroundDescriptor::bufferAllocatorFree( void )
{
	FWLSILOGALLOC( ("LSILOG : free all pages\n") );

	if( fBufferDescriptors )
		fBufferDescriptors->release();  // releases all in collection
}

/////////////////////////////////////////////////////////////////////////////////////////////
// rangeTable allocator
//
// allocates memory for the range table, potentially reallocating 
// a new block of memory if allowed.

IOReturn IOFireWireSBP2LSIWorkaroundDescriptor::rangeTableAllocatorInitialize
													( UInt32 entries )
{
	IOReturn status = kIOReturnSuccess;
		
	if( status == kIOReturnSuccess )
	{
		fRangeTableSize = sizeof( IOPhysicalRange ) * entries;
		fRangeTable = (IOPhysicalRange*) IOMalloc( fRangeTableSize );
		if( fRangeTable == NULL )
			status = kIOReturnError;
	}

	FWLSILOGALLOC( ("LSILOG : created %d entry range table. status = 0x%08lx\n", 
																	entries, status) );

	return status;
}

IOPhysicalRange * IOFireWireSBP2LSIWorkaroundDescriptor::rangeTableAllocatorNewTable
																	( UInt32 entries )
{
	IOReturn status = kIOReturnSuccess;
	
	IOPhysicalRange *	buffer = NULL;
	
	UInt32 requestedSize = sizeof( IOPhysicalRange ) * entries;

	if( requestedSize <= fRangeTableSize )
	{
		FWLSILOGALLOC( ("LSILOG : requested entry count = %ld. using existing range table\n", 
																	entries) );

		buffer = fRangeTable;
	}
	else if( !fFixedCapacity )
	{
		FWLSILOGALLOC( ("LSILOG : requested entry count = %ld. creating new range table\n", 
																	entries) );

		if( fRangeTable )
		{
			IOFree( fRangeTable, fRangeTableSize  );
			fRangeTable = NULL;
		}
		
		fRangeTable = (IOPhysicalRange*) IOMalloc( requestedSize );
		if( !fRangeTable )
			status = kIOReturnNoMemory;
			
		if( status == kIOReturnSuccess )
		{
			buffer = fRangeTable;
		}
	}
	
	FWLSILOGALLOC( ("LSILOG : return rangeTable = 0x%08lx\n", buffer) );

	return buffer;
}

void IOFireWireSBP2LSIWorkaroundDescriptor::rangeTableAllocatorFree( void )
{
	FWLSILOGALLOC( ("LSILOG : free range table\n") );

	if( fRangeTable )
	{
		IOFree( fRangeTable, fRangeTableSize  );
		fRangeTable = NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////

//
// disable other initialization methods
//

bool IOFireWireSBP2LSIWorkaroundDescriptor::initWithAddress( 
								   void *      /* address       */ ,
                                   IOByteCount /* withLength    */ ,
                                   IODirection /* withDirection */ )
{
    return false;
}

bool IOFireWireSBP2LSIWorkaroundDescriptor::initWithAddress( 
								   vm_address_t /* address       */ ,
                                   IOByteCount  /* withLength    */ ,
                                   IODirection  /* withDirection */ ,
                                   task_t       /* withTask      */ )
{
    return false;
}

bool IOFireWireSBP2LSIWorkaroundDescriptor::initWithPhysicalAddress(
                                   IOPhysicalAddress /* address       */ ,
                                   IOByteCount       /* withLength    */ ,
                                   IODirection       /* withDirection */ )
{
    return false;
}


bool IOFireWireSBP2LSIWorkaroundDescriptor::initWithPhysicalRanges(
                                   IOPhysicalRange * /* ranges        */ ,
                                   UInt32            /* withCount     */ ,
                                   IODirection       /* withDirection */ ,
                                   bool              /* asReference   */ )
{
    return false;
}

bool IOFireWireSBP2LSIWorkaroundDescriptor::initWithRanges
								( IOVirtualRange *  ranges,
                                  UInt32           withCount,
                                  IODirection      withDirection,
                                  task_t           withTask,
                                  bool             asReference )
{
	// this method is now protected so clients can't call it
	// but initWithPhysicalRanges calls this so we route it to the superclass
    return IOGeneralMemoryDescriptor::initWithRanges( ranges, withCount, withDirection, withTask, asReference );
}


// withCapacity
//
// create a new IOFireWireSBP2LSIWorkaroundDescriptor with a initial capacity
//
// creates a descriptor with a set amount of storage which can always be obtained
// without an allocation.  The decriptor will grow dynamically to handle larger requests
// unless fixedCapacity is true.
//

IOFireWireSBP2LSIWorkaroundDescriptor * IOFireWireSBP2LSIWorkaroundDescriptor::withCapacity
						( UInt32 maxElements, IOByteCount permanentBufferSpace, bool fixedCapacity )
{
	IOFireWireSBP2LSIWorkaroundDescriptor *me = new IOFireWireSBP2LSIWorkaroundDescriptor;
    
    if( me && !me->initWithCapacity( maxElements, permanentBufferSpace, fixedCapacity ) )
	{
		me->release();
		me = NULL;
    }
	
    return me;
}

// initWithCapacity
//
//
	
bool IOFireWireSBP2LSIWorkaroundDescriptor::initWithCapacity
					( UInt32 permanentRanges, IOByteCount permanentBufferSpace, bool fixedCapacity )
{
	IOReturn status = kIOReturnSuccess;

	if( status == kIOReturnSuccess )
	{
		fFixedCapacity = fixedCapacity;
		
		status = rangeAllocatorInitialize( permanentRanges );
	}
	
	if( status == kIOReturnSuccess )
	{
		status = bufferAllocatorInitialize( permanentBufferSpace );
	}
	
	if( status == kIOReturnSuccess )
	{
		fRanges = OSArray::withCapacity( permanentRanges ? permanentRanges : 1 );
		if( !fRanges )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = rangeTableAllocatorInitialize( permanentRanges );
	}
	
	return ( status == kIOReturnSuccess );
}

// resetToInitialCapacity
//
// We allow the descriptor to be reinited. We also allow the a a minimum capacity
// which we will never need memory allocations for.  This routine returns the memory
// descriptor to its original minimimum capacity state.

IOReturn IOFireWireSBP2LSIWorkaroundDescriptor::resetToInitialCapacity( void )
{
	IOReturn status = kIOReturnSuccess;
	
	//
	// release the referenced descriptor
	//
	
	if( fOriginalDesc )
		fOriginalDesc->release();

	//
	// free buffers
	//

	bufferAllocatorDeallocateAllBuffers();
		
	//
	// reset ranges
	//

	if( fRanges )
		fRanges->flushCollection();   // release ranges
	
	rangeAllocatorDeallocateAllRanges(); 

	return status;
}

// withDescriptor
//
// create a new IOFireWireSBP2LSIWorkaroundDescriptor
//
// len and offset are the length of and offset into the segment in desc which 
// will be used for the transfer.  The new descriptor will perform
// the workaround only on this segment.  The resulting descriptor will encompass only this
// segment and this segment will begin at offset zero not its former offset in the orginal
// descriptor.
//

IOFireWireSBP2LSIWorkaroundDescriptor * IOFireWireSBP2LSIWorkaroundDescriptor::withDescriptor
					( IOMemoryDescriptor * desc, IOByteCount offset,  IOByteCount len,
					  IODirection direction )
{
	IOReturn	status = kIOReturnSuccess;
	IOFireWireSBP2LSIWorkaroundDescriptor *me = NULL;
		
	if( status == kIOReturnSuccess )
	{
		me = new IOFireWireSBP2LSIWorkaroundDescriptor;
		if( !me )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		if( !me->initWithCapacity( 0, 0, false ) )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		if( !me->initWithDescriptor( desc, offset, len, direction ) )
			status = kIOReturnError;
	}
	
    if( me && status != kIOReturnSuccess )
	{
		me->release();
		me = NULL;
    }
	
    return me;
}

// initWithDescriptor
//
//

bool IOFireWireSBP2LSIWorkaroundDescriptor::initWithDescriptor
	( IOMemoryDescriptor * desc, IOByteCount offset,  IOByteCount length, 
	  IODirection direction )
{
	IOReturn status = kIOReturnSuccess;
	
	UInt32 count = 0;
	IOPhysicalRange * rangeTable = NULL;

	//
	// return descriptor to its initial state if needed
	//
	
	if( status == kIOReturnSuccess )
	{
		status = resetToInitialCapacity();
	}
	
	//
	// setup vars and verify args
	//
	
	if( status == kIOReturnSuccess )
	{
		// store source descriptor, offset, ...
		
		fDirection = direction;
		fOriginalDesc = desc;
		fOffset = offset;

		if( !desc )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		fOriginalDesc->retain();
	}
	
	if( status == kIOReturnSuccess )
	{
		// ... and length
		
		fLength = length;
		if( fLength == 0 )
			fLength = desc->getLength();
	}
	
	if( status == kIOReturnSuccess )
	{
		// there is nothing we can do if the whole 
		// transfer is smaller than 16 bytes

		if( fLength < kMinPacketSize )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		// creates the fRanges array based on the physical segments 
		// of the original descriptor
		
		status = initializeRangesArray();
	}		
	
#if LSILOGGING
	{
		FWLSILOG( ("LSILOG : original descriptor\n") );
		for( UInt32 i = 0; i < fRanges->getCount(); i++ )
		{
			IOFireWireSBP2LSIRange * range = (IOFireWireSBP2LSIRange*)fRanges->getObject(i);
			FWLSILOG( ("LSILOG : range #%ld addr = 0x%08lx len = %d buffered = %s\n", 
					i, range->address, range->length, range->buffer == NULL ? "no" : "yes") );
		
			#if 0	
			if( i % 10 == 0 )
				IOSleep(1000);
			#endif
		}
	}
#endif

	//
	// run _the eric algorithm_ now !
	//
	
	if( status == kIOReturnSuccess )
	{
		// step one :
		// Fix up individual segments smaller than 16 bytes by double-buffering
		
		status = recalculateSmallSegments();
	}
	
#if 0
	{
		FWLSILOG( ("LSILOG : descriptor after recalculateSmallSegments\n") );
		for( UInt32 i = 0; i < fRanges->getCount(); i++ )
		{
			IOFireWireSBP2LSIRange * range = (IOFireWireSBP2LSIRange*)fRanges->getObject(i);
			FWLSILOG( ("LSILOG : range #%ld addr = 0x%08lx len = %d buffered = %s\n", 
					i, range->address, range->length, range->buffer == NULL ? "no" : "yes") );
			#if 0
			if( i % 10 == 0 )
				IOSleep(1000);
			#endif
		}
	}
#endif

	if( status == kIOReturnSuccess )
	{
		// step two :
		// prevent > 60k segments

		status = splitLargeSegments();
	}

#if 0
	{
		FWLSILOG( ("LSILOG : descriptor after splitLargeSegments\n") );
		for( UInt32 i = 0; i < fRanges->getCount(); i++ )
		{
			IOFireWireSBP2LSIRange * range = (IOFireWireSBP2LSIRange*)fRanges->getObject(i);
			FWLSILOG( ("LSILOG : range #%ld addr = 0x%08lx len = %d buffered = %s\n", 
					i, range->address, range->length, range->buffer == NULL ? "no" : "yes") );
			#if 0
			if( i % 10 == 0 )
				IOSleep(1000);
			#endif
		}
	}
#endif
	
	if( status == kIOReturnSuccess )
	{
		// step three :
		// prevent < 16 byte packets
		
		status = resegmentOddLengthSegments();
	}

#if 0
	{
		FWLSILOG( ("LSILOG : descriptor after resegmentOddLengthSegments\n") );
		for( UInt32 i = 0; i < fRanges->getCount(); i++ )
		{
			IOFireWireSBP2LSIRange * range = (IOFireWireSBP2LSIRange*)fRanges->getObject(i);
			FWLSILOG( ("LSILOG : range #%ld addr = 0x%08lx len = %d buffered = %s\n", 
					i, range->address, range->length, range->buffer == NULL ? "no" : "yes") );
			#if 0
			if( i % 10 == 0 )
				IOSleep(1000);
			#endif
		}
	}
#endif
	
	//
	// initialize buffers
	//
	
	if( status == kIOReturnSuccess )
	{
		// setup double buffering

		status = initializeBuffers();
		
		FWLSILOGALLOC( ("LSILOG : attempted to initialize buffers. status = 0x%08lx\n", status) );
	}

#if LSILOGGING
	{
		FWLSILOG( ("LSILOG : fixed descriptor\n") );
		for( UInt32 i = 0; i < fRanges->getCount(); i++ )
		{
			IOFireWireSBP2LSIRange * range = (IOFireWireSBP2LSIRange*)fRanges->getObject(i);
			FWLSILOG( ("LSILOG : range #%ld addr = 0x%08lx len = %d buffered = %s\n", 
					i, range->address, range->length, range->buffer == NULL ? "no" : "yes") );
			#if 0
			if( i % 10 == 0 )
				IOSleep(1000);
			#endif
		}
	}
#endif
	
	//
	// create range table
	//
	
	if( status == kIOReturnSuccess )
	{
		count = fRanges->getCount();
		rangeTable = rangeTableAllocatorNewTable( count );
		if( !rangeTable )
			status = kIOReturnError;
			
		FWLSILOGALLOC( ("LSILOG : attempted to allocate range table. status = 0x%08lx\n", status) );
	}
	
	//
	// fill out range table
	//

	if( status == kIOReturnSuccess )
	{
		for( UInt32 i = 0; i < count; i++ )
		{
			IOFireWireSBP2LSIRange * range = (IOFireWireSBP2LSIRange*)fRanges->getObject( i );
			if( !range )
			{
				status = kIOReturnError;
				break;
			}
			
			rangeTable[i].address = range->address;
			rangeTable[i].length = range->length;
		}
		
		FWLSILOGALLOC( ("LSILOG : attempted to fillout range table. status = 0x%08lx\n", status) );
	}
	
	//
	// init super
	//
	
	if( status == kIOReturnSuccess )
	{
		if( !IOGeneralMemoryDescriptor::initWithPhysicalRanges
										( rangeTable, count, fDirection, true ) )
			status = kIOReturnError;
		
		FWLSILOGALLOC( ("LSILOG : attempted to initialize decriptor. status = 0x%08lx\n", status) );
	}
	
	return ( status == kIOReturnSuccess );
}

/////////////////////////////////////////////////////////////////////////////////////////////

// initializeRangesArray
//
// creates the fRanges array based on the physical segments of the original
// descriptor

IOReturn IOFireWireSBP2LSIWorkaroundDescriptor::initializeRangesArray( void )
{
	IOReturn status = kIOReturnSuccess;
	
	IOPhysicalAddress 	phys = NULL;
	IOByteCount 		seg_length = 0;
	IOByteCount 		i;
	UInt32				seg_count;
	
	//
	// add each segment to the array
	//
	
	i = fOffset;
	seg_count = 0;
	while( (status == kIOReturnSuccess) && 
		   (phys = fOriginalDesc->getPhysicalSegment( i, &seg_length )) ) 
	{
		FWLSILOGALLOC( ("LSILOG : creating range for segment %ld. offset = 0x%08lx, len = %ld\n",
						seg_count, i, seg_length) );

		// clip to length
		if( i + seg_length > fLength )
			seg_length = fLength - i;
				
		// create a new range
		IOFireWireSBP2LSIRange * range = rangeAllocatorNewRange();
		if( !range )
			status = kIOReturnNoMemory;
		
		if( status == kIOReturnSuccess )
		{
			range->memory		= fOriginalDesc;
			range->address 		= phys;
			range->length 		= seg_length;
			range->offset 		= i;
			
			range->initWithParent( this );
			
			// this won't allocate memory because if we've used up 
			// our range space the range allocation would have failed
			
			if( !fRanges->setObject( seg_count, range ) )
				status = kIOReturnError;
		}
		
		if( range )
		{
			range->release();
			range = NULL;
		}

		FWLSILOGALLOC( ("LSILOG : finished creating range for segment %ld. status = 0x%08lx\n",
						seg_count, status) );
		
		i += seg_length;
		seg_count++;
		
	}

	return status;
}

// recalculateSmallSegments
//
// Fix up individual segments smaller than 16 bytes by double-buffering
//
// Assume: Total I/O must be 16 bytes or more (otherwise bridge just can't
// do it safely)  
//
// If we make a segment double-buffered we will set its pointer
// to 0xffffffff to denote this.  We will create the actual buffer later once
// we're done tweaking everything.  It is quite possible that we will merge this
// with another buffered segment and this buffer will not need to be allocated

IOReturn IOFireWireSBP2LSIWorkaroundDescriptor::recalculateSmallSegments( void )
{
	IOReturn status = kIOReturnSuccess;

	UInt32 i = 0;
	while( i < fRanges->getCount() )
	{
		IOFireWireSBP2LSIRange * range = (IOFireWireSBP2LSIRange*)fRanges->getObject(i);

		if( range->length < kMinPacketSize  )
		{
			IOByteCount bytesNeeded = (kMinPacketSize - range->length);

			// must double buffer this part, and must make segment at least 16 bytes
			// must combine with part of an adjacent segment to reach 16 byte size
			
			if( i == (fRanges->getCount() - 1) )
			{
				// this is the final segment. we know that the previous 
				// segment plus this segment must be at least 16 bytes.
				// we are going to either steal from the previous segment
				// or merge with it.
				
				IOFireWireSBP2LSIRange * prevRange =
									(IOFireWireSBP2LSIRange*)fRanges->getObject(i-1);
				
				if( prevRange->length < (kMinPacketSize*2) )
				{
					// previous segment is too small to steal from, so merge with it
					
					prevRange->markForBuffering();				// double buffer
					prevRange->length += range->length;			// add our part
					
					FWLSILOGALLOC( ("LSILOG : range #%ld - added %ld bytes to previous range\n", i, range->length) );

					fRanges->removeObject(i);					// remove ourselves
				}
				else
				{
					// previous segment is large - steal what we need from the end
					
					prevRange->length -= bytesNeeded;  	// get what we need 
														//    to be exactly 16 bytes
					
					range->markForBuffering();			// double buffer
					range->length = kMinPacketSize;		// make us 16
					
					FWLSILOGALLOC( ("LSILOG : range #%ld - stole %ld bytes from previous range\n", i, bytesNeeded) );
				}
				
				i++; // next loop iteration
			}
			else
			{
				// not the final segment. steal from the next segment
				
				IOFireWireSBP2LSIRange * nextRange =
							(IOFireWireSBP2LSIRange*)fRanges->getObject(i+1);
				
				if( range->length + nextRange->length >= kMinPacketSize )
				{
					// we can steal enough bytes (16 - range.length) from nextRange.length
					// to solve this
					
					// Note: nextRange might be left with fewer than 16 bytes.  If so, it will
					// be fixed on the next iteration. 
					
					nextRange->length -= bytesNeeded;		// get what we need
					nextRange->address += bytesNeeded;		// bump his pointer by what we stole
					
					range->length = kMinPacketSize;			// make us 16
					range->markForBuffering();				// double buffer it
					
					FWLSILOGALLOC( ("LSILOG : range #%ld - stole %ld bytes from next range\n", i, bytesNeeded) );
					
					i++;  // next loop iteration
				}
				else
				{
					// the yuck case
					// nextRange.length is too small to completely solve our problem
					// combine with it and delete the next range, but then reprocess
					// so we can steal yet more bytes to make it okay
					
					range->length += nextRange->length;		// get all his bytes
					range->markForBuffering();				// double buffer it
					
					FWLSILOGALLOC( ("LSILOG : range #%ld - stole all %ld bytes from next range\n", i, nextRange->length) );

					fRanges->removeObject(i+1);				// leave no evidence
					
					// don't do i++ because we want to reprocess the same range
				}
			}
		}
		else
		{
			FWLSILOGALLOC( ("LSILOG : range #%ld - range has enough bytes\n", i) );

			i++;
		}
		
		#if 0
		IOSleep(500);
		#endif 
	}
	
	return status;
}

// splitLargeSegments
//
// prevent > 60k segments
//
// Segments might be larger than 60k.  If so, the SBP-2 layer might break them up.
// We don't know how SBP-2 will do that; it might create a segment that triggers
// the bug!  So we break up all segments to 60k or less so that the SBP-2 layer
// won't mess with them

IOReturn IOFireWireSBP2LSIWorkaroundDescriptor::splitLargeSegments( void )
{
	IOReturn status = kIOReturnSuccess;

	UInt32 i = 0;
	while( i < fRanges->getCount() )
	{
		IOFireWireSBP2LSIRange * range = (IOFireWireSBP2LSIRange*)fRanges->getObject(i);
		
		if( range->length > kFWSBP2MaxPageClusterSize )
		{		
			IOFireWireSBP2LSIRange * nextRange = NULL;
			
			if( status == kIOReturnSuccess )
			{
				// create a new range
				nextRange = rangeAllocatorNewRange();
				if( nextRange )
					status = kIOReturnSuccess;
			}
			
			if( status == kIOReturnSuccess )
			{
				// trim down to exactly 60k; put the remainder in nextRange
				// Note: ok if nextRange is an unfortunate size (say, < 16 bytes)
				// we will fix that in step 3
				
				nextRange->memory		= fOriginalDesc;
				nextRange->address 		= range->address + kFWSBP2MaxPageClusterSize;	
												// start at the end of range
				nextRange->length 		= range->length - kFWSBP2MaxPageClusterSize;	
												// remainder
				
				range->length			= kFWSBP2MaxPageClusterSize;	// just the first 60k

				nextRange->initWithParent( this );

				if( !fRanges->setObject( i+1, nextRange ) )
					status = kIOReturnError;
			}
		
			if( nextRange )
			{
				nextRange->release();
				nextRange = NULL;
			}
		}
		
		i++;
	}
	
	return status;
}

// resegmentOddLengthSegments
//
// prevent < 16 byte packets
//
// All segments are now at least 16 bytes.  None will be broken up by SBP-2, but
// an odd-length segment (not multiple of packet size) could enf in a < 16 byte
// packet.  For example, a segment of 2056 bytes would be transferred by the 
// bridge as one 2048 byte packet and one 8 byte packet.
//
// For each segment find out if this is possible.  If so, the segment can be
// broken into (probably unequal) halves to prevent it.  For example, break
// the 2056 byte segment into 2032 and 24 byte degments.  Bridge must use
// 2032 byte packet (or 1024 and 1008, or 512, 512, 512 and 496) followed 
// by a 24 byte packet.  One extra PTE will be needed in SBP-2.
// 
// Assume: Extra PTE is better than double-buffering entire segment.  If memory
// is mostly contiguous, most segments will be about 60k.  One extra PTE per
// 60k may double the PTE count, but it was low to begin with. If memory is
// mostly discontiguous, most PTEs are already perfect 4k pages and won't
// need to be broken up - probably only the first and the final PTE will be
// broken up.  PTE increase is small.
//
// We specify packet size in the ORB, but SBP-2 can override it with a smaller
// value.  Assume smallest size ever kLowestPacketLimit is 256.  It's okay if
// we assume too low of a kLowestPacketLimit (just mildly inefficient sometimes.)
 
IOReturn IOFireWireSBP2LSIWorkaroundDescriptor::resegmentOddLengthSegments( void )
{
	IOReturn status = kIOReturnSuccess;

	UInt32 i = 0;
	while( i < fRanges->getCount() )
	{
		IOFireWireSBP2LSIRange * range = (IOFireWireSBP2LSIRange*)fRanges->getObject(i);
		UInt32 finalPacketSize = range->length % kLowestPacketLimit;
		
		// might target end up with < 16 bytes in the final packet
		if( finalPacketSize != 0 && finalPacketSize < kMinPacketSize )
		{
			IOFireWireSBP2LSIRange * nextRange = NULL;
			
			// final packet would have been less than 16 bytes.  But total segment is
			// at least 257 bytes. Why? We removed all < 16 byte segments, so this
			// segment must be at least 257 bytes for (range->length % kLowestPacketLimit)
			// < kMinPacketSize.  So we can steal 16 bytes to form a new segment, leaving
			// a safe (big) segment
			
			if( status == kIOReturnSuccess )
			{
				// create a new range
				nextRange = rangeAllocatorNewRange();
				if( nextRange )
					status = kIOReturnSuccess;
			}
			
			if( status == kIOReturnSuccess )
			{
				range->length			-= kMinPacketSize;		// 16 is always safe
	
				nextRange->memory		= fOriginalDesc;
				nextRange->address 		= range->address + range->length;	
																// old starting address
				nextRange->length 		= kMinPacketSize; 		// new size
				
				nextRange->initWithParent( this );
		
				if( !fRanges->setObject( i+1, nextRange ) )
					status = kIOReturnError;
			}
		
			if( nextRange )
			{
				nextRange->release();
				nextRange = NULL;
			}

		}
		
		i++; 
	}
	
	return status;
}

// intializeBuffers
//
// setup double buffering
//
// go through and recalculate the offset for each segment and allocate
// a buffer if necessary.  we calculate the offset here and let the 
// instances decide if they wish to allocate a buffer

IOReturn IOFireWireSBP2LSIWorkaroundDescriptor::initializeBuffers( void )
{
	IOReturn status = kIOReturnSuccess;
	
	UInt32 count = fRanges->getCount();
	UInt32 offset = fOffset;
	for( UInt32 i = 0; i < count; i++ )
	{
		IOFireWireSBP2LSIRange * range = (IOFireWireSBP2LSIRange*)fRanges->getObject(i);
		range->offset = offset;
		offset += range->length;
		status = range->allocateBuffer( fDirection );
		if( status != kIOReturnSuccess )
			break;
	}
	
	return status;
}

/////////////////////////////////////////////////////////////////////////////////////////////

// syncBuffersForOutput
//
// copies data from the original source descriptor to the buffers.  we just
// tell all instances to do it and they decided if they really need to.
//
// this should be called before an output transfer is initiated.

IOReturn IOFireWireSBP2LSIWorkaroundDescriptor::syncBuffersForOutput( void )
{
	IOReturn status = kIOReturnSuccess;
		
	//
	// sync buffers for output
	//
	
	UInt32 count = fRanges->getCount();
	for( UInt32 i = 0; i < count; i++ )
	{
		IOFireWireSBP2LSIRange * range = (IOFireWireSBP2LSIRange*)fRanges->getObject( i );
		status = range->syncBufferForOutput();
		if( status != kIOReturnSuccess )
			break;
	}
	
	return status;
}

// syncBuffersForInput
//
// copies data from the buffers to the original source descriptor.  we just
// tell all instances to do it and they decided if they really need to.
//
// this should be called after an input transfer is complete.

IOReturn IOFireWireSBP2LSIWorkaroundDescriptor::syncBuffersForInput( void )
{
	IOReturn status = kIOReturnSuccess;
	
	//
	// sync buffers for input
	//

	UInt32 count = fRanges->getCount();
	for( UInt32 i = 0; i < count; i++ )
	{
		IOFireWireSBP2LSIRange * range = (IOFireWireSBP2LSIRange*)fRanges->getObject( i );
		status = range->syncBufferForInput();
		if( status != kIOReturnSuccess )
			break;
	}
	
	return status;
}

// free
//
//

void IOFireWireSBP2LSIWorkaroundDescriptor::free( void )
{	
	//
	// release the original
	//
	
	if( fOriginalDesc )
		fOriginalDesc->release();
	
	//		
	// free the table, now...
	//
	
	rangeTableAllocatorFree();
	
	//
	// free the buffers
	//
	
	bufferAllocatorFree();

	//
	// free range allocator
	//

	if( fRanges )
		fRanges->release();
	
	rangeAllocatorFree();


	IOGeneralMemoryDescriptor::free();
}

#endif
