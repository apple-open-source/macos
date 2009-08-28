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
 
/*
 *
 *	IOFWAddressSpace.cpp
 *
 * Classes which describe addresses in the local node which are accessable to other nodes
 * via firewire asynchronous read/write/lock requests.
 *
 * HISTORY
 *
 */
 
#include <IOKit/firewire/IOFWAddressSpace.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFireWireDevice.h>

#include "FWDebugging.h"

OSDefineMetaClassAndStructors(IOFWAddressSpaceAux, OSObject);

OSMetaClassDefineReservedUsed(IOFWAddressSpaceAux, 0);			// intersects
OSMetaClassDefineReservedUnused(IOFWAddressSpaceAux, 1);
OSMetaClassDefineReservedUnused(IOFWAddressSpaceAux, 2);
OSMetaClassDefineReservedUnused(IOFWAddressSpaceAux, 3);
OSMetaClassDefineReservedUnused(IOFWAddressSpaceAux, 4);
OSMetaClassDefineReservedUnused(IOFWAddressSpaceAux, 5);
OSMetaClassDefineReservedUnused(IOFWAddressSpaceAux, 6);
OSMetaClassDefineReservedUnused(IOFWAddressSpaceAux, 7);
OSMetaClassDefineReservedUnused(IOFWAddressSpaceAux, 8);
OSMetaClassDefineReservedUnused(IOFWAddressSpaceAux, 9);

#pragma mark -

// init
//
//

bool IOFWAddressSpaceAux::init( IOFWAddressSpace * primary )
{
	bool success = true;		// assume success
	
	// init super
	
    if( !OSObject::init() )
        success = false;
	
	fExclusive = false;
	
	if( success )
	{
		fPrimary = primary;
		fControl = fPrimary->fControl;
		
		//
		// create node set
		//
		
		fTrustedNodeSet = OSSet::withCapacity(1);
		if( fTrustedNodeSet == NULL )
			success = false;
	}
	
	if( success )
	{
		fTrustedNodeSetIterator = OSCollectionIterator::withCollection( fTrustedNodeSet );
		if( fTrustedNodeSetIterator == NULL )
			success = false;
	}
	
	if( !success )
	{
		if( fTrustedNodeSet != NULL )
		{
			fTrustedNodeSet->release();
			fTrustedNodeSet = NULL;
		}
		
		if( fTrustedNodeSetIterator != NULL )
		{
			fTrustedNodeSetIterator->release();
			fTrustedNodeSetIterator = NULL;
		}
	}
	
	return success;
}

// free
//
//

void IOFWAddressSpaceAux::free()
{	
	if( fTrustedNodeSet != NULL )
	{
		fTrustedNodeSet->release();
		fTrustedNodeSet = NULL;
	}
	
	if( fTrustedNodeSetIterator != NULL )
	{
		fTrustedNodeSetIterator->release();
		fTrustedNodeSetIterator = NULL;
	}
    
	OSObject::free();
}

// isTrustedNode
//
//

bool IOFWAddressSpaceAux::isTrustedNode( UInt16 nodeID )
{
	bool	trusted = false;

#if 1	
	//
	// trusted if not in secure mode
	//
	
	if( fControl->getSecurityMode() == kIOFWSecurityModeNormal )
		trusted = true;
#endif
		
	//
	// trusted if the local node
	// trusted if no nodes in set (no source node ID verification)
	//
	
	if( !trusted )
	{
		UInt16 localNodeID = fControl->getLocalNodeID();
		
//		FWKLOG(( "IOFWAddressSpaceAux::isTrustedNode - localNodeID = 0x%x\n", localNodeID ));

		if( nodeID == localNodeID || fTrustedNodeSet->getCount() == 0 )
			trusted = true;
	}
	
	//
	// check node id of all devices in set
	//
	
	IOFireWireDevice * item = NULL;
	fTrustedNodeSetIterator->reset();
	while( !trusted && (item = (IOFireWireDevice *) fTrustedNodeSetIterator->getNextObject()) )
	{
		UInt32 generation = 0;
		UInt16 deviceNodeID = 0;

		item->getNodeIDGeneration( generation, deviceNodeID ); 

	//	FWKLOG(( "IOFWAddressSpaceAux::isTrustedNode - deviceNodeID = 0x%x\n", deviceNodeID ));

		if( deviceNodeID == nodeID )
			trusted = true;
	}

//	FWKLOG(( "IOFWAddressSpaceAux::isTrustedNode - nodeID = 0x%x, trusted = %d\n", nodeID, trusted ));

	return trusted;
}

// addTrustedNode
//
//

void IOFWAddressSpaceAux::addTrustedNode( IOFireWireDevice * device )
{
	fTrustedNodeSet->setObject( device );
}

// removeTrustedNode
//
//

void IOFWAddressSpaceAux::removeTrustedNode( IOFireWireDevice * device )
{
	fTrustedNodeSet->removeObject( device );
}

// removeAllTrustedNodes
//
//

void IOFWAddressSpaceAux::removeAllTrustedNodes( void )
{
	fTrustedNodeSet->flushCollection();
}

// isExclusive
//
//

bool IOFWAddressSpaceAux::isExclusive( void )
{
	return fExclusive;
}

// setExclusive
//
//

void IOFWAddressSpaceAux::setExclusive( bool exclusive )
{
	fExclusive = exclusive;
}

// intersects
//
//

bool IOFWAddressSpaceAux::intersects( IOFWAddressSpace * space )
{
	return false;
}

#pragma mark -

/*
 * Base class for FireWire address space objects
 */

OSDefineMetaClass( IOFWAddressSpace, OSObject )
OSDefineAbstractStructors(IOFWAddressSpace, OSObject)

//OSMetaClassDefineReservedUnused(IOFWAddressSpace, 0);
//OSMetaClassDefineReservedUnused(IOFWAddressSpace, 1);

// init
//
//

bool IOFWAddressSpace::init(IOFireWireBus *bus)
{
	bool success = true;		// assume success
	
	// init super
	
    if( !OSObject::init() )
        success = false;

	// get controller
	
	if( success )
	{
		fControl = OSDynamicCast(IOFireWireController, bus);
		if( fControl == NULL )
			success = false;
	}
	
	// create expansion data
	
	if( success )
	{
		fIOFWAddressSpaceExpansion = (ExpansionData*) IOMalloc( sizeof(ExpansionData) );
		if( fIOFWAddressSpaceExpansion == NULL )
			success = false;
	}
	
	// zero expansion data
	
	if( success )
	{
		bzero( fIOFWAddressSpaceExpansion, sizeof(ExpansionData) );
		fIOFWAddressSpaceExpansion->fAuxiliary = createAuxiliary();
		if( fIOFWAddressSpaceExpansion->fAuxiliary == NULL )
			success = false;
	}

	// clean up on failure
	
	if( !success )
	{
		if( fIOFWAddressSpaceExpansion->fAuxiliary != NULL )
		{
			fIOFWAddressSpaceExpansion->fAuxiliary->release();
			fIOFWAddressSpaceExpansion->fAuxiliary = NULL;
		}
		
		if( fIOFWAddressSpaceExpansion != NULL )
		{
			IOFree ( fIOFWAddressSpaceExpansion, sizeof(ExpansionData) );
			fIOFWAddressSpaceExpansion = NULL;
		}
	}
	
    return success;
}

// createAuxiliary
//
// virtual method for creating auxiliary object.  subclasses needing to subclass 
// the auxiliary object can override this.

IOFWAddressSpaceAux * IOFWAddressSpace::createAuxiliary( void )
{
	IOFWAddressSpaceAux * auxiliary;
    
	auxiliary = OSTypeAlloc( IOFWAddressSpaceAux );

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

void IOFWAddressSpace::free()
{
	if( fIOFWAddressSpaceExpansion != NULL )
	{
		// release auxiliary object
		
		if( fIOFWAddressSpaceExpansion->fAuxiliary != NULL )
		{
			fIOFWAddressSpaceExpansion->fAuxiliary->release();
			fIOFWAddressSpaceExpansion->fAuxiliary = NULL;
		}
		
		// free expansion data
		
		IOFree ( fIOFWAddressSpaceExpansion, sizeof(ExpansionData) );
		fIOFWAddressSpaceExpansion = NULL;
	}
	
    OSObject::free();
}

// doLock
//
//

UInt32 IOFWAddressSpace::doLock(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 inLen,
                        const UInt32 *newVal, UInt32 &outLen, UInt32 *oldVal, UInt32 type,
                          IOFWRequestRefCon refcon)
{
    UInt32 ret = kFWResponseAddressError;
    bool ok;
    int size;
    int i;
    IOMemoryDescriptor *desc = NULL;
    IOByteCount offset;

    size = inLen/8;	// Depends on type, right for 'compare and swap'
    outLen = inLen/2;	// right for 'compare and swap'
    
	ret = doRead(nodeID, speed, addr, size*4, &desc, &offset, refcon);
	if(ret != kFWResponseComplete)
		return ret;

    desc->readBytes(offset, oldVal, size*4);
    
    switch (type) 
	{
        case kFWExtendedTCodeCompareSwap:
            ok = true;
			for(i=0; i<size; i++)
                ok = ok && oldVal[i] == newVal[i];
			if(ok)
                ret = doWrite(nodeID, speed, addr, size*4, newVal+size, refcon);
			break;

        default:
            ret = kFWResponseTypeError;
    }
    return ret;
}

// activate
//
//

IOReturn IOFWAddressSpace::activate()
{
    return fControl->allocAddress(this);
}

// deactivate
//
//

void IOFWAddressSpace::deactivate()
{
    fControl->freeAddress(this);
}

// contains
//
//

UInt32 IOFWAddressSpace::contains(FWAddress addr)
{
    return 0;
}
