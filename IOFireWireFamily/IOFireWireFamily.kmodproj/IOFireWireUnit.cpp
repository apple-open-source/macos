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
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 * 21 May 99 wgulland created.
 *
 */

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme
#define DEBUGLOG kprintf
#include <IOKit/assert.h>

#include <IOKit/IOMessage.h>
#include <IOKit/firewire/IOFireWireUnit.h>
#include <IOKit/firewire/IOFireWireDevice.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOConfigDirectory.h>

OSDefineMetaClassAndStructors(IOFireWireUnitAux, IOFireWireNubAux);
OSMetaClassDefineReservedUnused(IOFireWireUnitAux, 0);
OSMetaClassDefineReservedUnused(IOFireWireUnitAux, 1);
OSMetaClassDefineReservedUnused(IOFireWireUnitAux, 2);
OSMetaClassDefineReservedUnused(IOFireWireUnitAux, 3);

#pragma mark -

// init
//
//

bool IOFireWireUnitAux::init( IOFireWireUnit * primary )
{
	bool success = true;		// assume success
	
	// init super
	
    if( !IOFireWireNubAux::init( primary ) )
        success = false;
	
	if( success )
	{
	}
	
	return success;
}

// free
//
//

void IOFireWireUnitAux::free()
{	    
	IOFireWireNubAux::free();
}

#pragma mark -

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOFireWireUnit, IOFireWireNub)
OSMetaClassDefineReservedUnused(IOFireWireUnit, 0);
OSMetaClassDefineReservedUnused(IOFireWireUnit, 1);

#pragma mark -
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// init
//
//

bool IOFireWireUnit::init( OSDictionary *propTable, IOConfigDirectory *directory )
{
    if(!IOFireWireNub::init(propTable))
        return false;

    directory->retain();
    fDirectory = directory;
    return true;
}

// createAuxiliary
//
// virtual method for creating auxiliary object.  subclasses needing to subclass 
// the auxiliary object can override this.

IOFireWireNubAux * IOFireWireUnit::createAuxiliary( void )
{
	IOFireWireUnitAux * auxiliary;
    
	auxiliary = new IOFireWireUnitAux;

    if( auxiliary != NULL && !auxiliary->init(this) ) 
	{
        auxiliary->release();
        auxiliary = NULL;
    }
	
    return auxiliary;
}

// attach
//
//

bool IOFireWireUnit::attach( IOService *provider )
{
    fDevice = OSDynamicCast(IOFireWireDevice, provider);
    if(!fDevice)
		return false;
	fDevice->retain();
    if( !IOFireWireNub::attach(provider))
        return (false);
    fControl = fDevice->getController();
    fControl->retain();
    fDevice->getNodeIDGeneration(fGeneration, fNodeID, fLocalNodeID);
    
    return(true);
}

// free
//
//

void IOFireWireUnit::free()
{
	if( fDevice != NULL )
	{
		fDevice->release();
		fDevice = NULL;
	}
	
    IOFireWireNub::free();
}

// message
//
//

IOReturn IOFireWireUnit::message( 	UInt32 		mess, 
									IOService *	provider,
									void * 		argument )
{
    // Propagate bus reset start/end messages
    if(provider == fDevice &&
       (kIOMessageServiceIsResumed == mess ||
        kIOMessageServiceIsSuspended == mess ||
        kIOMessageServiceIsRequestingClose == mess ||
        kIOFWMessageServiceIsRequestingClose == mess)) 
	{
        fDevice->getNodeIDGeneration(fGeneration, fNodeID, fLocalNodeID);
		messageClients( mess );
        return kIOReturnSuccess;
    }
	
	if( kIOFWMessagePowerStateChanged == mess )
	{
		messageClients( mess );
		return kIOReturnSuccess;
	}

	if( kIOFWMessageTopologyChanged == mess )
	{
		messageClients( mess );
		return kIOReturnSuccess;
	}
	
    return IOService::message(mess, provider, argument );
}

/**
 ** Matching methods
 **/

// matchPropertyTable
//
//

bool IOFireWireUnit::matchPropertyTable( OSDictionary * table )
{
    //
    // If the service object wishes to compare some of its properties in its
    // property table against the supplied matching dictionary,
    // it should do so in this method and return truth on success.
    //
    if (!IOFireWireNub::matchPropertyTable(table))  return false;

    // We return success if the following expression is true -- individual
    // comparisions evaluate to truth if the named property is not present
    // in the supplied matching dictionary.

    bool res = compareProperty(table, gFireWireUnit_Spec_ID) &&
        compareProperty(table, gFireWireUnit_SW_Version) &&
        compareProperty(table, gFireWireVendor_ID) &&
        compareProperty(table, gFireWire_GUID);
    return res;
}

#pragma mark -

/////////////////////////////////////////////////////////////////////////////
// open / close
//

// handleOpen
//
//

bool IOFireWireUnit::handleOpen(	IOService *	  	forClient,
									IOOptionBits	options,
									void *		  	arg )
{
	if ( isOpen() )
		return false ;

    bool ok;
    ok = fDevice->open(this, options, arg);
    if(ok)
        ok = IOFireWireNub::handleOpen(forClient, options, arg);
    return ok;
}

// handleClose
//
//

void IOFireWireUnit::handleClose(   IOService *	  	forClient,
									IOOptionBits	options )
{
    IOFireWireNub::handleClose(forClient, options);
    fDevice->close(this, options);
}

#pragma mark -

/////////////////////////////////////////////////////////////////////////////
// node flags
//

// setNodeFlags
//
//

void IOFireWireUnit::setNodeFlags( UInt32 flags )
{
	if( fDevice )
		fDevice->setNodeFlags( flags );
}

// clearNodeFlags
//
//

void IOFireWireUnit::clearNodeFlags( UInt32 flags )
{
	if( fDevice )
		fDevice->clearNodeFlags( flags );
}

// getNodeFlags
//
//

UInt32 IOFireWireUnit::getNodeFlags( void )
{
	if( fDevice )
		return fDevice->getNodeFlags();
	else
		return 0;
}

// setMaxSpeed
//
//

void IOFireWireUnit::setMaxSpeed( IOFWSpeed speed )
{
	if( fDevice )
		fDevice->setMaxSpeed( speed );
}

#pragma mark -

/////////////////////////////////////////////////////////////////////////////
// address spaces
//

/*
 * Create local FireWire address spaces for the device to access
 */

// createPhysicalAddressSpace
//
//

IOFWPhysicalAddressSpace * IOFireWireUnit::createPhysicalAddressSpace( IOMemoryDescriptor *mem )
{
    IOFWPhysicalAddressSpace * space = fControl->createPhysicalAddressSpace(mem);
	
	if( space != NULL )
	{
		space->addTrustedNode( fDevice );
	}
	
	return space;
}

// createPseudoAddressSpace
//
//

IOFWPseudoAddressSpace * IOFireWireUnit::createPseudoAddressSpace( 	FWAddress *		addr, 
																	UInt32 			len, 
																	FWReadCallback 	reader, 
																	FWWriteCallback writer, 
																	void *			refcon )
{
    IOFWPseudoAddressSpace * space = fControl->createPseudoAddressSpace(addr, len, reader, writer, refcon);

	if( space != NULL )
	{
		space->addTrustedNode( fDevice );
	}
	
	return space;

}
