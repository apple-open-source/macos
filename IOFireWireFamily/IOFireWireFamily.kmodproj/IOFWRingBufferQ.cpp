/*
 * Copyright (c) 1998-2009 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").	You may not use this file except in compliance with the
 * License.	 Please obtain a copy of the License at
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
 *  IOFWBufferQ.cpp
 *  IOFireWireFamily
 *
 *  Created by calderon on 9/17/09.
 *  Copyright 2009 Apple. All rights reserved.
 *
 */

#import "IOFWRingBufferQ.h"
#import "FWDebugging.h"

// system
#import <IOKit/IOTypes.h>
//#import <IOKit/firewire/FireLog.h>

// IOFWRingBufferQ class
// *** This class is not multithread safe ***
// Its usage must be lock protected to ensure only one consumer at a time

#define super OSObject
OSDefineMetaClassAndStructors( IOFWRingBufferQ, OSObject ) ;

//withAddressRange
//

IOFWRingBufferQ * IOFWRingBufferQ::withAddressRange( mach_vm_address_t address, mach_vm_size_t length, IOOptionBits options, task_t task)
{
	DebugLog("IOFWRingBufferQ::withAddressRange\n");
	IOFWRingBufferQ * that = OSTypeAlloc( IOFWRingBufferQ );
	
	if ( that )
	{
		if ( that->initQ( address, length, options, task ) )
			return that;
		
		DebugLog("IOFWRingBufferQ::withAddressRange failed initQ\n");
		that->release();
	}
	
	return 0;
}

// initQ
// inits class specific variables

bool IOFWRingBufferQ::initQ( mach_vm_address_t address, mach_vm_size_t length, IOOptionBits options, task_t task )
{
	DebugLog("IOFWRingBufferQ::initQ\n");
	fMemDescriptor = IOMemoryDescriptor::withAddressRange( address, length, options, task );
	if ( !fMemDescriptor )
		return false;
	
	if ( fMemDescriptor->prepare( (IODirection)(options & kIOMemoryDirectionMask) ) != kIOReturnSuccess )
		return false;
	
	fMemDescriptorPrepared = true;
	fFrontOffset = NULL;
	fQueueLength = 0;
	fBufferSize = fMemDescriptor->getLength();
	
	return true;
}

// free
//
void IOFWRingBufferQ::free()
{
	if ( fMemDescriptorPrepared )
		fMemDescriptor->complete();
	
	if ( fMemDescriptor )
		fMemDescriptor->release();
	
	super::free();
}

// isEmpty
//

bool IOFWRingBufferQ::isEmpty( void )
{
	return (fQueueLength == 0);
}

// dequeueBytes
// Remove the next n bytes in queue given by 'size', but do not return bytes

bool IOFWRingBufferQ::dequeueBytes( IOByteCount size )
{
	return dequeueBytesWithCopy(NULL, size);
}

// dequeueBytesWithCopy
// Remove the next n bytes in queue given by 'size' and write bytes into 'copy'

bool IOFWRingBufferQ::dequeueBytesWithCopy( void * copy, IOByteCount size )
{
	// zzz is there a guarantee that we will dequeue in first-out order? Maybe it's First-In, Any-Out
	
	bool success = true;
	IOByteCount paddingBytes = 0;
	
	if ( copy )
	{
		// get first entry (of size 'size') in queue
		if ( front(copy, size, &paddingBytes) )
		{
			fFrontOffset = ( fFrontOffset + paddingBytes + size ) % fBufferSize; // advance front pointer by padding and by this dequeue
			fQueueLength = fQueueLength - paddingBytes - size;	// remove padding and this dequeue size
		}
		else
			success = false;
	}
	else
	{
		// copy not necessary, simply make bytes available in queue
		
		IOByteCount theRealFrontOffset = frontEntryOffset( size, &paddingBytes );	// get front offset that corresponds to 'size'
		fFrontOffset = theRealFrontOffset + size;	// advance front pointer by this dequeue
		fQueueLength = fQueueLength - paddingBytes - size;	// remove padding and this dequeue size
		
		// should have checks to make sure numbers are sensible, if not success=false
	}
	
	DebugLog("<<< IOFWRingBufferQ::dequeueBytesWithCopy BSize: %u Length: %u Front: %u Size: %u Copy: %p\n", fBufferSize, fQueueLength, fFrontOffset, size, copy);

	return success;
}

// getBytes
//

IOByteCount IOFWRingBufferQ::readBytes(IOByteCount offset, void * bytes, IOByteCount withLength)
{
	return fMemDescriptor->readBytes( offset, bytes, withLength );
}

// enqueueBytes
// Insert 'bytes' into queue contiguously

bool IOFWRingBufferQ::enqueueBytes( void * bytes, IOByteCount size )
{
	bool success = true;
	
	IOByteCount offset = 0;
	IOByteCount paddingBytes = 0;
	
	// determine if 'bytes' will fit in queue and get the appropriate insertion offset
	if ( success = willFitAtEnd(size, &offset, &paddingBytes) )
	{
		if ( bytes )
		{
			fQueueLength = fQueueLength + size + paddingBytes;	// grow queue appropriately
			if ( fMemDescriptor->writeBytes(offset, bytes, size) == 0 )			// write 'bytes' into queue at insertion offset
				success = false;
		}
		else
		{
			success = false;
		}
	}
	
	DebugLog(">>> IOFWRingBufferQ::enqueueBytes BSize: %u Length: %u Front: %u Insert: %u/%u\n", fBufferSize, fQueueLength, fFrontOffset, offset, paddingBytes);
	return success;
}

//isSpaceAvailable
//

bool IOFWRingBufferQ::isSpaceAvailable( IOByteCount size, IOByteCount * offset )
{
#if 0
	FireLog("[");
	UInt16 i;
	UInt16 drawBufferWidth = 64;
	UInt32 drawBufferSize = fBufferSize;
	UInt32 drawBufAvail = drawBufferWidth * (fBufferSize-fQueueLength) / drawBufferSize;
	UInt32 drawStartOff = drawBufferWidth * fFrontOffset / drawBufferSize;
	UInt32 drawDestOff = drawBufferWidth * endOffset / drawBufferSize;

	if ( drawStartOff != 0 ) drawStartOff--;
	if ( drawDestOff != 0 ) drawDestOff--;

	bool drawFilled = drawStartOff > drawDestOff ? true : false;
	for ( i=0; i < drawBufferWidth; i++ )
	{
		if ( i == drawStartOff )
			FireLog("s");
		else if ( i == drawDestOff )
			FireLog("d");
		else if ( drawFilled )
			FireLog("o");
		else if ( drawBufAvail <= (drawBufferWidth/2) )
			FireLog("?");
		else
			FireLog(".");

		if ( i == drawStartOff )
			drawFilled = true;
		if ( i == drawDestOff )
			drawFilled = false;
	}
	
	FireLog("]\n");
	FireLog("-- Units Available: %lu  StartPtr: %lu  DestinationPtr: %lu --\n", drawBufAvail, drawStartOff, drawDestOff );
#endif
	
	return willFitAtEnd(size, offset, NULL);
}

// front
// Return a copy of first entry, of size 'size'

bool IOFWRingBufferQ::front( void * copy, IOByteCount size, IOByteCount * paddingBytes )
{
	//FireLog("IOFWRingBufferQ::front\n");
	bool success = true;
	
	if ( isEmpty() )
	{
		success = false;
	}
	else
	{
		if ( copy )
		{
			IOByteCount frontOffset = frontEntryOffset( size, paddingBytes );	// get proper offset for first entry
			if ( fMemDescriptor->readBytes(frontOffset, copy, size) == 0 )
				success = false;
		}
		else
		{
			success = false;
		}
	}

	return success;
}

// It's a beautiful day.

// spaceAvailable
//

IOByteCount IOFWRingBufferQ::spaceAvailable( void )
{
	return fBufferSize - fQueueLength;
}

// willFitAtEnd
// Checks to see if an entry of 'sizeOfEntry' bytes will fit in queue. If so, it will return the offset to which the entry should be written and any padding bytes that should be added to the queue length to compensate for entries that don't fit at the end of the memory range and should be written at beginning of memory range.

bool IOFWRingBufferQ::willFitAtEnd( IOByteCount sizeOfEntry, IOByteCount * offset, IOByteCount * paddingBytes )
{
	bool success = true;
	
	if ( paddingBytes )
		*paddingBytes = 0;
	
	IOByteCount endOffset = (fFrontOffset + fQueueLength) % fBufferSize;	// pointer that designates end of queue
	
	if ( fQueueLength < fBufferSize ) // is not full; has space available - avoids empty vs. full confusion
	{
		if ( endOffset >= fFrontOffset )	// [__f....e__]
		{
			if ( sizeOfEntry > (fBufferSize - endOffset) )
			{
				// cannot fit at end
				IOByteCount padding = fBufferSize - endOffset;	// the number of bytes to get insertion offset to wrap
				
				if ( paddingBytes )
					*paddingBytes = padding;
				
				endOffset = (fFrontOffset + fQueueLength + padding) % fBufferSize;	// advance insertion offset past end of memory range; should be zero
				
				if ( sizeOfEntry > fFrontOffset )
					success = false;	// cannot fit at start either
			}
		}
		else	// [..e____f..]
		{
			if ( sizeOfEntry > (fBufferSize - fQueueLength) )
				success = false;	// cannot fit in space available
		}
	}
	else if ( fQueueLength > fBufferSize )
	{
		//FireLog("IOFWRingBufferQ::willFitAtEnd queue has grown larger (%u) than buffer size (%u)!\n", fQueueLength, fBufferSize);
		success = false;
	}
	else
	{
		// queue is full
		success = false;
	}
	
	if ( offset )
		*offset = endOffset;
	
	DebugLog("IOFWRingBufferQ::willFitAtEnd BSize: %u Length: %u Front: %u Insert: %u EntrySize: %u\n", fBufferSize, fQueueLength, fFrontOffset, endOffset, sizeOfEntry);
	
	return success;
}

// frontEntryOffset
// Get the first entry in the queue. First entry is determined by collecting 'sizeOfEntry' bytes of front of queue. If first entry cannot have been fit between the front queue offset and the actual end of the memory range of the buffer, then it probably exists at the beginning of memory range - paddingBytes represents the number of bytes skipped at the end of memory range to get back to beginning of memory range.

IOByteCount IOFWRingBufferQ::frontEntryOffset( IOByteCount sizeOfEntry, IOByteCount * paddingBytes )
{
	IOByteCount frontEntryOffset = fFrontOffset;
	
	IOByteCount endOffset = (fFrontOffset + fQueueLength) % fBufferSize;	// pointer that designates end of queue
	
	if ( paddingBytes )
		*paddingBytes = 0;
	
	if ( !isEmpty() )	// avoid empty vs. full confusion
	{
		if ( fFrontOffset >= endOffset )	// [..e____f..]
		{
			if ( sizeOfEntry > (fBufferSize - fFrontOffset) )
			{
				// cannot fit at end
				IOByteCount padding = fBufferSize - fFrontOffset;	// the number of bytes to get insertion offset to wrap
				
				if ( paddingBytes )
					*paddingBytes = padding;
				
				frontEntryOffset = (fFrontOffset + padding) % fBufferSize;	// should be zero, since padding made it wrap
				
				DebugLogCond( (sizeOfEntry > endOffset), "IOFWRingBufferQ::frontEntryOffset Front entry cannot occur within queue starting at buffer index zero!\n");
			}
			// else - normal front offset will work (no padding), do nothing
		}
		else	// [__f....e__]
		{
			DebugLogCond( (sizeOfEntry > fQueueLength), "IOFWRingBufferQ::frontEntryOffset Front entry cannot occur within queue!\n");
		}
	}
	else
	{
		DebugLog("IOFWRingBufferQ::frontEntryOffset Queue is empty!\n");
	}
	
	return frontEntryOffset;
}
