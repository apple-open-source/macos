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

#include "IOFWUserClientPhysAddrSpace.h"

#define MIN(a,b) ((a < b) ? a : b)

OSDefineMetaClassAndStructors(IOFWUserClientPhysicalAddressSpace, IOFWPhysicalAddressSpace) ;

bool
IOFWUserClientPhysicalAddressSpace::initWithDesc(
	IOFireWireBus*		bus,
	IOMemoryDescriptor*	mem)
{
	if (!IOFWPhysicalAddressSpace::initWithDesc(bus, mem))
		return false ;
	
	UInt32 currentOffset = 0 ;
	IOByteCount length ;

	mSegmentCount = 0 ;
		
	while (0 != fMem->getPhysicalSegment(currentOffset, & length))
	{
		currentOffset += length ;
		mSegmentCount++ ;
	}
	
	return true ;
}

void
IOFWUserClientPhysicalAddressSpace::free()
{
	IOFWPhysicalAddressSpace::free() ;
}

UInt32
IOFWUserClientPhysicalAddressSpace::getSegmentCount()
{
	return mSegmentCount ;
}

IOReturn
IOFWUserClientPhysicalAddressSpace::getSegments(
	UInt32*				ioSegmentCount,
	IOPhysicalAddress	outPages[],
	IOByteCount			outLength[])
{
	UInt32 segmentCount = MIN(*ioSegmentCount, mSegmentCount) ;
	IOReturn	result = kIOReturnSuccess ;

	IOByteCount currentOffset = 0 ;
	for(UInt32 segment=0; segment < segmentCount; segment++)
	{
		outPages[segment] = fMem->getPhysicalSegment(currentOffset, & outLength[segment]) ;
		currentOffset+= outLength[segment] ;
	}
	
	return result ;
}
