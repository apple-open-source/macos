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

#import "IOFireWireUserClient.h"
#import "IOFireWireLink.h"
#import "IOFireWireNub.h"
#import "IOFireWireController.h"
#import "IOConfigDirectory.h"

#import <IOKit/assert.h>
#import <IOKit/IOMessage.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClass( IOFireWireNub, IOService )
OSDefineAbstractStructors(IOFireWireNub, IOService)
//OSMetaClassDefineReservedUnused(IOFireWireNub, 0);
OSMetaClassDefineReservedUnused(IOFireWireNub, 1);
OSMetaClassDefineReservedUnused(IOFireWireNub, 2);
OSMetaClassDefineReservedUnused(IOFireWireNub, 3);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOFireWireNub::init(OSDictionary * propTable)
{
    OSNumber *offset;
    if( !IOService::init(propTable))
        return false;

    offset = OSDynamicCast(OSNumber, propTable->getObject("GUID"));
    if(offset)
        fUniqueID = offset->unsigned64BitValue();

	fConfigDirectorySet = OSSet::withCapacity(1);
	if( fConfigDirectorySet == NULL )
		return false;

    return true;
}

void IOFireWireNub::free()
{
    if(fDirectory)
        fDirectory->release();
	
	if( fConfigDirectorySet )
		fConfigDirectorySet->release();
		
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

IOFWSpeed IOFireWireNub::FWSpeed() const
{
    return fControl->FWSpeed(fNodeID);
}

// How fast can this node talk to another node?
IOFWSpeed IOFireWireNub::FWSpeed(const IOFireWireNub *dst) const
{
    return fControl->FWSpeed(fNodeID, dst->fNodeID);
}

// How big (as a power of two) can packets sent to the node be?
int IOFireWireNub::maxPackLog(bool forSend) const
{
    int log = fControl->maxPackLog(forSend, fNodeID);
    return log;
}

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

void IOFireWireNub::setMaxPackLog(bool forSend, bool forROM, int maxPackLog)
{
    if(forSend)
        fMaxWritePackLog = maxPackLog;
    else if(forROM)
        fMaxReadROMPackLog = maxPackLog;
    else
        fMaxReadPackLog = maxPackLog;
}

IOFWReadCommand *IOFireWireNub::createReadCommand(FWAddress devAddress, IOMemoryDescriptor *hostMem,
				FWDeviceCallback completion, void *refcon,
 				bool failOnReset)
{
    IOFWReadCommand * cmd;
    cmd = new IOFWReadCommand;
    if(cmd) {
        if(!cmd->initAll(this, devAddress,
                         hostMem, completion, refcon, failOnReset)) {
            cmd->release();
            cmd = NULL;
	}
    }
    return cmd;
}

IOFWReadQuadCommand *IOFireWireNub::createReadQuadCommand(FWAddress devAddress, UInt32 *quads, int numQuads,
				FWDeviceCallback completion, void *refcon,
 				bool failOnReset)
{
    IOFWReadQuadCommand * cmd;
    cmd = new IOFWReadQuadCommand;
    if(cmd) {
        if(!cmd->initAll(this, devAddress, quads, numQuads, 
		completion, refcon, failOnReset)) {
            cmd->release();
            cmd = NULL;
	}
    }
    return cmd;
}

IOFWWriteCommand *IOFireWireNub::createWriteCommand(FWAddress devAddress, IOMemoryDescriptor *hostMem,
				FWDeviceCallback completion, void *refcon,
 				bool failOnReset)
{
    IOFWWriteCommand * cmd;
    cmd = new IOFWWriteCommand;
    if(cmd) {
        if(!cmd->initAll(this, devAddress, hostMem,
                         completion, refcon, failOnReset)) {
            cmd->release();
            cmd = NULL;
	}
    }
    return cmd;
}

IOFWWriteQuadCommand *IOFireWireNub::createWriteQuadCommand(FWAddress devAddress, 
				UInt32 *quads, int numQuads,
				FWDeviceCallback completion, void *refcon,
 				bool failOnReset)
{
    IOFWWriteQuadCommand * cmd;
    cmd = new IOFWWriteQuadCommand;
    if(cmd) {
        if(!cmd->initAll(this, devAddress, quads, numQuads,
		completion, refcon, failOnReset)) {
            cmd->release();
            cmd = NULL;
	}
    }
    return cmd;
}

IOFWCompareAndSwapCommand *
IOFireWireNub::createCompareAndSwapCommand(FWAddress devAddress, const UInt32 *cmpVal, const UInt32 *newVal,
		int size, FWDeviceCallback completion, void *refcon, bool failOnReset)
{
    IOFWCompareAndSwapCommand * cmd;
    cmd = new IOFWCompareAndSwapCommand;
    if(cmd) {
        if(!cmd->initAll(this, devAddress, cmpVal, newVal, size, completion, refcon, failOnReset)) {
            cmd->release();
            cmd = NULL;
	}
    }
    return cmd;
}

/*
 * Create local FireWire address spaces for the device to access
 */

IOFWPhysicalAddressSpace *IOFireWireNub::createPhysicalAddressSpace(IOMemoryDescriptor *mem)
{
    return fControl->createPhysicalAddressSpace(mem);
}

IOFWPseudoAddressSpace *IOFireWireNub::createPseudoAddressSpace(FWAddress *addr, UInt32 len, 
				FWReadCallback reader, FWWriteCallback writer, void *refcon)
{
    return fControl->createPseudoAddressSpace(addr, len, reader, writer, refcon);
}

IOReturn IOFireWireNub::getConfigDirectory(IOConfigDirectory *&dir)
{
    dir = fDirectory;
	fConfigDirectorySet->setObject( fDirectory );
    
	return kIOReturnSuccess;    
}

IOReturn IOFireWireNub::getConfigDirectoryRef( IOConfigDirectory *&dir )
{
    dir = fDirectory;
	fDirectory->retain();
	
    return kIOReturnSuccess;    
}

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

/**
 ** IOUserClient methods
 **/

IOReturn IOFireWireNub::newUserClient(task_t		owningTask,
                                      void * 		/* security_id */,
                                      UInt32  		type,
                                      IOUserClient **	handler )

{
    IOReturn			err = kIOReturnSuccess;
    IOFireWireUserClient *	client;

    if( type != 11 )
        return( kIOReturnBadArgument);

    client = IOFireWireUserClient::withTask(owningTask);

    if( !client || (false == client->attach( this )) ||
        (false == client->start( this )) ) {
        if(client) {
            client->detach( this );
            client->release();
        }
        err = kIOReturnNoMemory;
    }

    *handler = client;
    return( err );
}

void IOFireWireNub::setNodeFlags( UInt32 flags )
{
}

void IOFireWireNub::clearNodeFlags( UInt32 flags )
{
}

UInt32 IOFireWireNub::getNodeFlags( void )
{
	return 0;
}

