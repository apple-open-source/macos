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
	DebugLog("+IOFWUserPhysicalAddressSpace::initWithDesc\n" ) ;

	if (!IOFWPhysicalAddressSpace::initWithDesc(bus, mem))
		return false ;
	
	if ( kIOReturnSuccess != mem->prepare() )
	{
		fMemPrepared = false ;
		return false ;
	}
	
	fMemPrepared = true ;
	fSegmentCount = 0 ;

	{ //scope
		UInt32 			currentOffset = 0 ;
		IOByteCount 	length ;
			
		while (0 != fMem->getPhysicalSegment(currentOffset, & length))
		{
			currentOffset += length ;
			++fSegmentCount ;
		}
	}

	DebugLog("new phys addr space - segmentCount=%ld length=0x%lx\n", fSegmentCount, (UInt32)fMem->getLength() ) ;
	
	return true ;
}

void
IOFWUserPhysicalAddressSpace::free()
{
	if (fMemPrepared)
		fMem->complete() ;

	IOFWPhysicalAddressSpace::free() ;
}

// exporterCleanup
//
//

void
IOFWUserPhysicalAddressSpace::exporterCleanup ()
{
	DebugLog("IOFWUserPseudoAddressSpace::exporterCleanup\n");
	
	deactivate();
}

IOReturn
IOFWUserPhysicalAddressSpace::getSegmentCount( UInt32 * outSegmentCount )
{
	*outSegmentCount = fSegmentCount ;
	return kIOReturnSuccess ;
}

IOReturn
IOFWUserPhysicalAddressSpace :: getSegments (
	UInt32*				ioSegmentCount,
	IOMemoryCursor::IOPhysicalSegment	outSegments[] )
{
	unsigned segmentCount = *ioSegmentCount <? fSegmentCount ;	// min
	IOByteCount currentOffset = 0 ;
	
	for( unsigned index = 0; index < segmentCount; ++index )
	{
		outSegments[ index ].location = fMem->getPhysicalSegment( currentOffset, & outSegments[ index ].length ) ;
		currentOffset += outSegments[ index ].length ;
	}

	return kIOReturnSuccess ;
}
