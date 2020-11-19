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
/*
 *  IOFireWirePhysicalAddressSpace.cpp
 *  IOFireWireFamily
 *
 *  Created by NWG on Fri Dec 08 2000.
 *  Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 */

#import "IOFWUserPhysicalAddressSpace.h"
#import "FWDebugging.h"

OSDefineMetaClassAndStructors(IOFWUserPhysicalAddressSpace, IOFWPhysicalAddressSpace) ;

bool
IOFWUserPhysicalAddressSpace::initWithDesc(
	IOFireWireBus*		bus,
	IOMemoryDescriptor*	mem)
{
	DebugLog("// *** TEST: IOFWUserPhysicalAddressSpace<%p>::initWithDesc\n", this );
	
	// *** MEM: this kIODirectionPrepareToPhys32 mem desc was passed in from IOFireWireUserClient::physicalAddressSpace_Create
	
	if (!IOFWPhysicalAddressSpace::initWithDesc(bus, mem))
		return false ;
	
	fDescriptor = mem;
	fDescriptor->retain();
	
	fSegmentCount = 0 ;

	{ //scope
		UInt32 			currentOffset = 0 ;
		IOByteCount 	length ;
		
		// *** VERIFY: this is presumably still the best way to count segments?
		while (0 != fDescriptor->getPhysicalSegment(currentOffset, & length))
		{
			currentOffset += length ;
			++fSegmentCount ;
		}
	}
	
	DebugLog("IOFWUserPhysicalAddressSpace<%p>::initWithDesc - fSegmentCount = %d, length = %u\n", this, fSegmentCount, (unsigned int)fDescriptor->getLength() ) ;
	
	return true ;
}

void
IOFWUserPhysicalAddressSpace::free()
{
	if( fDescriptor )
	{
		fDescriptor->release();
		fDescriptor = NULL;
	}

	IOFWPhysicalAddressSpace::free();
}

// exporterCleanup
//
//

void
IOFWUserPhysicalAddressSpace::exporterCleanup( const OSObject * self )
{
	IOFWUserPhysicalAddressSpace * me = (IOFWUserPhysicalAddressSpace*)self;
	
	DebugLog("IOFWUserPseudoAddressSpace::exporterCleanup\n");
	
	me->deactivate();
}

IOReturn
IOFWUserPhysicalAddressSpace::getSegmentCount( UInt32 * outSegmentCount )
{
	DebugLog("// *** TEST: IOFWUserPhysicalAddressSpace<%p>::getSegmentCount\n", this ) ;
	
	*outSegmentCount = fSegmentCount ;
	return kIOReturnSuccess ;
}

IOReturn
IOFWUserPhysicalAddressSpace::getSegments (
	UInt32*				ioSegmentCount,
	IOFireWireLib::FWPhysicalSegment32	outSegments[] )
{
	// *** VERIFY: IOFWUserPhysicalAddressSpace::getSegments - need a way to verify this code path
	DebugLog("// *** TEST: IOFWUserPhysicalAddressSpace<%p>::getSegments\n", this ) ;
	
	unsigned segmentCount = *ioSegmentCount;
	if( fSegmentCount < segmentCount )
	{
		segmentCount = fSegmentCount;
	}
 	
#if 1
	// *** ISOCH FIX: IOFWUserPhysicalAddressSpace::getSegments - outSegments is now populated from fDMACommand rather than fDescriptor

	IOReturn status = kIOReturnSuccess;
	IODMACommand * dma_command = getDMACommand();
	IODMACommand::Segment32	segs[ segmentCount ];
	UInt32 numSegs = 0;
	
	if( status == kIOReturnSuccess )
	{
		numSegs = segmentCount;
		
		// DebugLog("IOFWUserPhysicalAddressSpace<%p>::getSegments - gen32IOVMSegments( numSegs = %d)\n", this, numSegs );
		status = dma_command->gen32IOVMSegments( 0, segs, &numSegs );
		if( status != kIOReturnSuccess )
		{
			DebugLog("IOFWUserPhysicalAddressSpace<%p>::getSegments - ERROR: gen32IOVMSegments failed with 0x%08x\n", this, status );
		}
	}
	
	if( status == kIOReturnSuccess )
	{
		if( numSegs > segmentCount || numSegs == 0 )
		{
			DebugLog("IOFWUserPhysicalAddressSpace<%p>::getSegments - ERROR: numSegs = %u\n", this, numSegs );
			status = kIOReturnNoResources;
		}
	}
	
	if( status == kIOReturnSuccess )
	{
		*ioSegmentCount = numSegs;
		
		for( unsigned i = 0; i < numSegs; i++ )
		{
			outSegments[i].location = segs[i].fIOVMAddr;
			outSegments[i].length = segs[i].fLength;
			
			DebugLog("IOFWUserPhysicalAddressSpace<%p>::getSegments - outSegments[%d].location = 0x%08x\n", this, i, outSegments[i].location );
			DebugLog("IOFWUserPhysicalAddressSpace<%p>::getSegments - outSegments[%d].length = 0x%08x\n", this, i, outSegments[i].length );
		}
	}
#else
	IOByteCount currentOffset = 0 ;
	
	for( unsigned index = 0; index < segmentCount; ++index )
	{
		IOByteCount length = 0;
		outSegments[ index ].location = (IOPhysicalAddress32)fDescriptor->getPhysicalSegment( currentOffset, &length ) ;
		outSegments[ index ].length = (IOPhysicalLength32)length;
		currentOffset += length ;
	}
#endif
	
	return kIOReturnSuccess ;
}


