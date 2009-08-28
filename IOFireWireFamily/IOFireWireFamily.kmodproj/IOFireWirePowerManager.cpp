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

// public 
#include <IOKit/firewire/IOFireWirePowerManager.h>
// protected
#include <IOKit/firewire/IOFireWireController.h>

// private
#import "FWDebugging.h"

OSDefineMetaClassAndStructors(IOFireWirePowerManager, OSObject)
OSMetaClassDefineReservedUnused(IOFireWirePowerManager, 0);
OSMetaClassDefineReservedUnused(IOFireWirePowerManager, 1);
OSMetaClassDefineReservedUnused(IOFireWirePowerManager, 2);
OSMetaClassDefineReservedUnused(IOFireWirePowerManager, 3);
OSMetaClassDefineReservedUnused(IOFireWirePowerManager, 4);
OSMetaClassDefineReservedUnused(IOFireWirePowerManager, 5);
OSMetaClassDefineReservedUnused(IOFireWirePowerManager, 6);
OSMetaClassDefineReservedUnused(IOFireWirePowerManager, 7);
OSMetaClassDefineReservedUnused(IOFireWirePowerManager, 8);
OSMetaClassDefineReservedUnused(IOFireWirePowerManager, 9);

#pragma mark -

/////////////////////////////////////////////////////////////////////////////

// createWithController
//
//

IOFireWirePowerManager * IOFireWirePowerManager::createWithController( IOFireWireController * controller )
{
    IOFireWirePowerManager * me = OSTypeAlloc( IOFireWirePowerManager );
	if( me != NULL )
	{
		if( !me->initWithController(controller) ) 
		{
            me->release();
            me = NULL;
        }
	}

    return me;
}

// initWithController
//
//

bool IOFireWirePowerManager::initWithController( IOFireWireController * controller )
{
	bool success = true;		// assume success
	
	// init super
	
    if( !OSObject::init() )
        success = false;
	
	if( success )
	{
		fControl = controller;
		fMaximumDeciwatts = 0;
		fAllocatedDeciwatts = 0;
	}
	
	return success;
}

#pragma mark -

/////////////////////////////////////////////////////////////////////////////

// setMaximumDeciwatts
//
//

void IOFireWirePowerManager::setMaximumDeciwatts( UInt32 deciwatts )
{
	FWKLOG(( "IOFireWirePowerManager::setMaximumDeciwatts - setting maximum milliwats to %d\n", deciwatts ));
	
	fMaximumDeciwatts = deciwatts;
}

// allocateDeciwatts
//
//

IOReturn IOFireWirePowerManager::allocateDeciwatts( UInt32 deciwatts )
{
	IOReturn status = kIOReturnSuccess;
	
	fControl->closeGate();
	
	FWKLOG(( "IOFireWirePowerManager::allocateDeciwatts - allocating %d deciwatts\n", deciwatts ));
	
	if( fAllocatedDeciwatts + deciwatts <= fMaximumDeciwatts )
	{
		fAllocatedDeciwatts += deciwatts;
	}
	else
	{
		status = kIOReturnNoResources;
	}
	
	fControl->openGate();
	
	return status;
}

// deallocateDeciwatts
//
//

void IOFireWirePowerManager::deallocateDeciwatts( UInt32 deciwatts )
{
	fControl->closeGate();
	
	FWKLOG(( "IOFireWirePowerManager::deallocateDeciwatts - freeing %d deciwatts\n", deciwatts ));
	
	if( deciwatts <= fAllocatedDeciwatts )
	{
		fAllocatedDeciwatts -= deciwatts;
	}
	else
	{
		IOLog( "IOFireWirePowerManager::deallocateDeciwatts - freed deciwatts %d > allocated deciwatts %d!\n", (uint32_t)deciwatts, (uint32_t)fAllocatedDeciwatts );
		fAllocatedDeciwatts = 0;
	}
	
	// notify clients that more power has been made available
	if( deciwatts != 0 )
	{
		fControl->messageClients( kIOFWMessagePowerStateChanged );
	}

	fControl->openGate();
}
