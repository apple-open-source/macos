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
 * Copyright (c) 1999-2002 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 * 21 May 99 wgulland created.
 *
 */

// public
#import <IOKit/firewire/IOFireWireNub.h>
#import <IOKit/firewire/IOFireWireController.h>
#import <IOKit/firewire/IOConfigDirectory.h>
#import <IOKit/firewire/IOFWSimpleContiguousPhysicalAddressSpace.h>

// protected
#import <IOKit/firewire/IOFireWireLink.h>

// private
#import "IOFireWireUserClient.h"

// system
#import <IOKit/assert.h>
#import <IOKit/IOMessage.h>

OSDefineMetaClassAndStructors(IOFireWireNubAux, OSObject);
OSMetaClassDefineReservedUnused(IOFireWireNubAux, 0);
OSMetaClassDefineReservedUnused(IOFireWireNubAux, 1);
OSMetaClassDefineReservedUnused(IOFireWireNubAux, 2);
OSMetaClassDefineReservedUnused(IOFireWireNubAux, 3);
OSMetaClassDefineReservedUnused(IOFireWireNubAux, 4);
OSMetaClassDefineReservedUnused(IOFireWireNubAux, 5);
OSMetaClassDefineReservedUnused(IOFireWireNubAux, 6);
OSMetaClassDefineReservedUnused(IOFireWireNubAux, 7);
OSMetaClassDefineReservedUnused(IOFireWireNubAux, 8);
OSMetaClassDefineReservedUnused(IOFireWireNubAux, 9);
OSMetaClassDefineReservedUnused(IOFireWireNubAux, 10);
OSMetaClassDefineReservedUnused(IOFireWireNubAux, 11);
OSMetaClassDefineReservedUnused(IOFireWireNubAux, 12);
OSMetaClassDefineReservedUnused(IOFireWireNubAux, 13);
OSMetaClassDefineReservedUnused(IOFireWireNubAux, 14);
OSMetaClassDefineReservedUnused(IOFireWireNubAux, 15);

#pragma mark -

// init
//
//

bool IOFireWireNubAux::init( IOFireWireNub * primary )
{
	bool success = true;		// assume success
	
	// init super
	
    if( !OSObject::init() )
        success = false;
	
	if( success )
	{
		fPrimary = primary;
		fTerminationState = kNotTerminated;
	}
	
	return success;
}

// free
//
//

void IOFireWireNubAux::free()
{	    
	OSObject::free();
}

// hopCount
//
//

UInt32 IOFireWireNubAux::hopCount( IOFireWireNub * nub )
{
	return fPrimary->fControl->hopCount( fPrimary->fNodeID, nub->fNodeID );
}

// hopCount
//
//

UInt32 IOFireWireNubAux::hopCount( void )
{
	return fPrimary->fControl->hopCount( fPrimary->fNodeID );
}

// getTerminationState
//
//

TerminationState IOFireWireNubAux::getTerminationState( void )
{
	return fTerminationState;
}

// setTerminationState
//
//

void IOFireWireNubAux::setTerminationState( TerminationState state )
{
	fTerminationState = state;
}

// isPhysicalAccessEnabled
//
//

bool IOFireWireNubAux::isPhysicalAccessEnabled( void )
{
	return false;
}

// createSimpleContiguousPhysicalAddressSpace
//
//

IOFWSimpleContiguousPhysicalAddressSpace * IOFireWireNubAux::createSimpleContiguousPhysicalAddressSpace( vm_size_t size, IODirection direction )
{
	return fPrimary->fControl->createSimpleContiguousPhysicalAddressSpace( size, direction );
}

// createSimplePhysicalAddressSpace
//
//

IOFWSimplePhysicalAddressSpace * IOFireWireNubAux::createSimplePhysicalAddressSpace( vm_size_t size, IODirection direction )
{
	return fPrimary->fControl->createSimplePhysicalAddressSpace( size, direction );
}

#pragma mark -

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClass( IOFireWireNub, IOService )
OSDefineAbstractStructors(IOFireWireNub, IOService)
//OSMetaClassDefineReservedUnused(IOFireWireNub, 0);
//OSMetaClassDefineReservedUnused(IOFireWireNub, 1);
OSMetaClassDefineReservedUnused(IOFireWireNub, 2);
OSMetaClassDefineReservedUnused(IOFireWireNub, 3);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// init
//
//

bool IOFireWireNub::init( OSDictionary * propTable )
{
	bool success = true;
	
    OSNumber *offset;
    
	if( !IOService::init(propTable) )
	{
		success = false;
	}

	if( success )
	{
		fAuxiliary = createAuxiliary();
		if( fAuxiliary == NULL )
			success = false;
	}

	if( success )
	{
		offset = OSDynamicCast(OSNumber, propTable->getObject("GUID"));
		if( offset )
			fUniqueID = offset->unsigned64BitValue();
	
		fConfigDirectorySet = OSSet::withCapacity(1);
		if( fConfigDirectorySet == NULL )
			success = false;
	}
	
    return success;
}

// createAuxiliary
//
// virtual method for creating auxiliary object.  subclasses needing to subclass 
// the auxiliary object can override this.

IOFireWireNubAux * IOFireWireNub::createAuxiliary( void )
{
	IOFireWireNubAux * auxiliary;
    
	auxiliary = OSTypeAlloc( IOFireWireNubAux );

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

void IOFireWireNub::free()
{
    if( fDirectory != NULL )
	{
        fDirectory->release();
		fDirectory = NULL;
	}
	
	if( fConfigDirectorySet != NULL )
	{
		fConfigDirectorySet->release();
		fConfigDirectorySet = NULL;
	}
		
	if( fAuxiliary != NULL )
	{
		fAuxiliary->release();
		fAuxiliary = NULL;
	}

    if( fControl != NULL )
	{
        fControl->release();
		fControl = NULL;
	}
		
    IOService::free();
}

// getNodeIDGeneration
//
//

IOReturn IOFireWireNub::getNodeIDGeneration(UInt32 &generation, UInt16 &nodeID, UInt16 &localID) const
{ 
	generation = fGeneration; 
	nodeID = fNodeID; 
	localID = fLocalNodeID; 
	return kIOReturnSuccess;
}

// getNodeIDGeneration
//
//

IOReturn IOFireWireNub::getNodeIDGeneration(UInt32 &generation, UInt16 &nodeID) const
{
	generation = fGeneration; 
	nodeID = fNodeID; 
	return kIOReturnSuccess;
}

// FWSpeed
//
//

IOFWSpeed IOFireWireNub::FWSpeed() const
{
    return fControl->FWSpeed(fNodeID);
}

// FWSpeed
//
// How fast can this node talk to another node?

IOFWSpeed IOFireWireNub::FWSpeed(const IOFireWireNub *dst) const
{
    return fControl->FWSpeed(fNodeID, dst->fNodeID);
}

// maxPackLog
//
// How big (as a power of two) can packets sent to the node be?

int IOFireWireNub::maxPackLog(bool forSend) const
{
    int log = fControl->maxPackLog(forSend, fNodeID);
    return log;
}

// maxPackLog
//
// How big (as a power of two) can packets sent to an address in the node be?

int IOFireWireNub::maxPackLog(bool forSend, FWAddress address) const
{
    int log = fControl->maxPackLog(forSend, fNodeID);
    if(forSend) {
        if(log > fMaxWritePackLog)
            log = fMaxWritePackLog;
    }
    else if(address.addressHi == kCSRRegisterSpaceBaseAddressHi &&
            address.addressLo > kConfigROMBaseAddress &&
            address.addressLo < kConfigROMBaseAddress + 1024) {
        if(log > fMaxReadROMPackLog)
            log = fMaxReadROMPackLog;
    }

    else if(log > fMaxReadPackLog)
        log = fMaxReadPackLog;
    return log;
}

// maxPackLog
//
// How big (as a power of two) can packets sent between nodes be?

int IOFireWireNub::maxPackLog(bool forSend, const IOFireWireNub *dst) const
{
    int log;
    if(forSend) {
        log = fControl->maxPackLog(fNodeID, dst->fNodeID);
    }
    else {
        log = fControl->maxPackLog(dst->fNodeID, fNodeID);
    }

    return log;
}

// setMaxPackLog
//
//

void IOFireWireNub::setMaxPackLog(bool forSend, bool forROM, int maxPackLog)
{
    if(forSend)
        fMaxWritePackLog = maxPackLog;
    else if(forROM)
        fMaxReadROMPackLog = maxPackLog;
    else
        fMaxReadPackLog = maxPackLog;
}

// createReadCommand
//
//

IOFWReadCommand *IOFireWireNub::createReadCommand(FWAddress devAddress, IOMemoryDescriptor *hostMem,
				FWDeviceCallback completion, void *refcon,
 				bool failOnReset)
{
    IOFWReadCommand * cmd;
    cmd = OSTypeAlloc( IOFWReadCommand );
    if(cmd) {
        if(!cmd->initAll(this, devAddress,
                         hostMem, completion, refcon, failOnReset)) {
            cmd->release();
            cmd = NULL;
	}
    }
    return cmd;
}

// createReadQuadCommand
//
//

IOFWReadQuadCommand *IOFireWireNub::createReadQuadCommand(FWAddress devAddress, UInt32 *quads, int numQuads,
				FWDeviceCallback completion, void *refcon,
 				bool failOnReset)
{
    IOFWReadQuadCommand * cmd;
    cmd = OSTypeAlloc( IOFWReadQuadCommand );
    if(cmd) { 
        if(!cmd->initAll(this, devAddress, quads, numQuads, 
		completion, refcon, failOnReset)) {
            cmd->release();
            cmd = NULL;
	}
    }
    return cmd;
}

// createWriteCommand
//
//

IOFWWriteCommand *IOFireWireNub::createWriteCommand(FWAddress devAddress, IOMemoryDescriptor *hostMem,
				FWDeviceCallback completion, void *refcon,
 				bool failOnReset)
{
    IOFWWriteCommand * cmd;
    cmd = OSTypeAlloc( IOFWWriteCommand );
    if(cmd) 
	{
        if(!cmd->initAll(this, devAddress, hostMem,
                         completion, refcon, failOnReset)) 
		{
            cmd->release();
            cmd = NULL;
		}
    }
	
    return cmd;
}

// createWriteQuadCommand
//
//

IOFWWriteQuadCommand *IOFireWireNub::createWriteQuadCommand(FWAddress devAddress, 
				UInt32 *quads, int numQuads,
				FWDeviceCallback completion, void *refcon,
 				bool failOnReset)
{
    IOFWWriteQuadCommand * cmd;
    cmd = OSTypeAlloc( IOFWWriteQuadCommand );
    if(cmd) 
	{
        if(!cmd->initAll(this, devAddress, quads, numQuads,
		completion, refcon, failOnReset)) 
		{
            cmd->release();
            cmd = NULL;
		}
    }
	
    return cmd;
}

// createCompareAndSwapCommand
//
//

IOFWCompareAndSwapCommand *
IOFireWireNub::createCompareAndSwapCommand(FWAddress devAddress, const UInt32 *cmpVal, const UInt32 *newVal,
		int size, FWDeviceCallback completion, void *refcon, bool failOnReset)
{
    IOFWCompareAndSwapCommand * cmd;
    cmd = OSTypeAlloc( IOFWCompareAndSwapCommand );
    if(cmd) 
	{
        if(!cmd->initAll(this, devAddress, cmpVal, newVal, size, completion, refcon, failOnReset)) 
		{
            cmd->release();
            cmd = NULL;
		}
    }
	
    return cmd;
}

/*
 * Create local FireWire address spaces for the device to access
 */

// createPhysicalAddressSpace
//
//

IOFWPhysicalAddressSpace *IOFireWireNub::createPhysicalAddressSpace(IOMemoryDescriptor *mem)
{
    return fControl->createPhysicalAddressSpace(mem);
}

// createPseudoAddressSpace
//
//

IOFWPseudoAddressSpace *IOFireWireNub::createPseudoAddressSpace(FWAddress *addr, UInt32 len, 
				FWReadCallback reader, FWWriteCallback writer, void *refcon)
{
    return fControl->createPseudoAddressSpace(addr, len, reader, writer, refcon);
}

// getConfigDirectory
//
//

IOReturn IOFireWireNub::getConfigDirectory(IOConfigDirectory *&dir)
{
	fControl->closeGate();
	
    dir = fDirectory;
	fConfigDirectorySet->setObject( fDirectory );
    
	fControl->openGate();
	
	return kIOReturnSuccess;    
}

// getConfigDirectoryRef
//
//

IOReturn IOFireWireNub::getConfigDirectoryRef( IOConfigDirectory *&dir )
{
    dir = fDirectory;
	fDirectory->retain();
	
    return kIOReturnSuccess;    
}

// setConfigDirectory
//
//

IOReturn IOFireWireNub::setConfigDirectory( IOConfigDirectory *directory )
{
	IOConfigDirectory * oldDirectory = fDirectory;
	
	directory->retain();
	fDirectory = directory;
	
	if( oldDirectory )
		oldDirectory->release();
	
	return kIOReturnSuccess;
}

// getBus
//
//

IOFireWireBus * IOFireWireNub::getBus() const
{ 
	return (IOFireWireBus *)fControl; 
}

// getController
//
//

IOFireWireController * IOFireWireNub::getController() const
{ 
	return fControl;
}

// getUniqueID
//
//

const CSRNodeUniqueID& IOFireWireNub::getUniqueID() const
{ 
	return fUniqueID; 
}

// setNodeFlags
//
//

void IOFireWireNub::setNodeFlags( UInt32 flags )
{
}

// clearNodeFlags
//
//

void IOFireWireNub::clearNodeFlags( UInt32 flags )
{
}

// getNodeFlags
//
//

UInt32 IOFireWireNub::getNodeFlags( void )
{
	return 0;
}
