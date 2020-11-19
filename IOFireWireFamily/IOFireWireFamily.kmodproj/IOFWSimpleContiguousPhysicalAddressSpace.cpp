/*
 * Copyright (c) 1998-2006 Apple Computer, Inc. All rights reserved.
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


// public
#import <IOKit/IOLib.h>

// private
#include "IOFWSimpleContiguousPhysicalAddressSpace.h"
#include "FWDebugging.h"

// fun with binary compatibility

OSDefineMetaClassAndStructors( IOFWSimpleContiguousPhysicalAddressSpace, IOFWSimplePhysicalAddressSpace )

OSMetaClassDefineReservedUnused(IOFWSimpleContiguousPhysicalAddressSpace, 0);
OSMetaClassDefineReservedUnused(IOFWSimpleContiguousPhysicalAddressSpace, 1);
OSMetaClassDefineReservedUnused(IOFWSimpleContiguousPhysicalAddressSpace, 2);
OSMetaClassDefineReservedUnused(IOFWSimpleContiguousPhysicalAddressSpace, 3);
OSMetaClassDefineReservedUnused(IOFWSimpleContiguousPhysicalAddressSpace, 4);
OSMetaClassDefineReservedUnused(IOFWSimpleContiguousPhysicalAddressSpace, 5);
OSMetaClassDefineReservedUnused(IOFWSimpleContiguousPhysicalAddressSpace, 6);
OSMetaClassDefineReservedUnused(IOFWSimpleContiguousPhysicalAddressSpace, 7);
OSMetaClassDefineReservedUnused(IOFWSimpleContiguousPhysicalAddressSpace, 8);
OSMetaClassDefineReservedUnused(IOFWSimpleContiguousPhysicalAddressSpace, 9);

struct MemberVariables
{
	FWAddress	fFWPhysicalAddress;
};

#define _members ((MemberVariables*)fSimpleContigPhysSpaceMembers)

// init
//
//

bool IOFWSimpleContiguousPhysicalAddressSpace::init( IOFireWireBus * control, vm_size_t size, IODirection direction )
{
	DebugLog("IOFWSimpleContiguousPhysicalAddressSpace<%p>::init\n", this );
	
	fSimpleContigPhysSpaceMembers = NULL;
	
	bool success = IOFWSimplePhysicalAddressSpace::init( control, size, direction, true );
		
	if( success )
	{
		IOReturn status = cachePhysicalAddress();
		if( status != kIOReturnSuccess )
			success = false;
	}
	
	return success;
}

// free
//
//

void IOFWSimpleContiguousPhysicalAddressSpace::free( void )
{
	IOFWSimplePhysicalAddressSpace::free();
}

// createMemberVariables
//
//

bool IOFWSimpleContiguousPhysicalAddressSpace::createMemberVariables( void )
{
	bool success = true;
	
	success = IOFWSimplePhysicalAddressSpace::createMemberVariables();
	
	if( success && (fSimpleContigPhysSpaceMembers == NULL) )
	{
		// create member variables
		
		if( success )
		{
			fSimpleContigPhysSpaceMembers = IOMalloc( sizeof(MemberVariables) );
			if( fSimpleContigPhysSpaceMembers == NULL )
				success = false;
		}
		
		// zero member variables
		
		if( success )
		{
			bzero( fSimpleContigPhysSpaceMembers, sizeof(MemberVariables) );
			
			// largely redundant
			_members->fFWPhysicalAddress.nodeID = 0x0000;
			_members->fFWPhysicalAddress.addressHi = 0x0000;
			_members->fFWPhysicalAddress.addressLo = 0x00000000;
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

void IOFWSimpleContiguousPhysicalAddressSpace::destroyMemberVariables( void )
{
	IOFWSimplePhysicalAddressSpace::destroyMemberVariables();
	
	if( fSimpleContigPhysSpaceMembers != NULL )
	{
		IOFree( fSimpleContigPhysSpaceMembers, sizeof(MemberVariables) );
		fSimpleContigPhysSpaceMembers = NULL;
	}
}

#pragma mark -

// cachePhysicalAddress
//
// 

IOReturn IOFWSimpleContiguousPhysicalAddressSpace::cachePhysicalAddress( void )
{
	IOReturn status = kIOReturnSuccess;

	UInt32 segment_count = 0;
	FWSegment	segments[ 2 ];
	if( status == kIOReturnSuccess )
	{
		UInt64 offset_64 = 0;					
		segment_count = 2;
		status = getSegments( &offset_64, segments, &segment_count );
	}
	
	// sanity checks for contiguous allocation
	if( status == kIOReturnSuccess )
	{
		if( segment_count > 2 || segment_count == 0 )
		{
			status = kIOReturnNoResources;
		}
	}		
	
	if( status == kIOReturnSuccess )
	{
		if(  segments[0].length < getLength() )
		{
			status = kIOReturnNoResources;
		}
	}
	
	if( status == kIOReturnSuccess )
	{
		_members->fFWPhysicalAddress = segments[0].address;
	}
	
//	IOLog( "IOFWSimpleContiguousPhysicalAddressSpace::cachePhysicalAddress - 0x%04x %08lx\n", 
//					_members->fFWPhysicalAddress.addressHi, _members->fFWPhysicalAddress.addressLo );
					
	return status;
}

// getPhysicalAddress
//
//

FWAddress IOFWSimpleContiguousPhysicalAddressSpace::getFWAddress( void )
{
	return _members->fFWPhysicalAddress;
}
