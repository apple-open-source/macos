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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOFireWireUnit, IOFireWireNub)
OSMetaClassDefineReservedUnused(IOFireWireUnit, 0);
OSMetaClassDefineReservedUnused(IOFireWireUnit, 1);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOFireWireUnit::init(OSDictionary *propTable, IOConfigDirectory *directory)
{
    if(!IOFireWireNub::init(propTable))
        return false;

    directory->retain();
    fDirectory = directory;
    return true;
}

bool IOFireWireUnit::attach(IOService *provider)
{
    fDevice = OSDynamicCast(IOFireWireDevice, provider);
    if(!fDevice)
	return false;
    if( !IOFireWireNub::attach(provider))
        return (false);
    fControl = fDevice->getController();

    fDevice->getNodeIDGeneration(fGeneration, fNodeID, fLocalNodeID);
    
    return(true);
}

IOReturn IOFireWireUnit::message( UInt32 mess, IOService * provider,
                                void * argument )
{
    // Propagate bus reset start/end messages
    if(provider == fDevice &&
       (kIOMessageServiceIsResumed == mess ||
        kIOMessageServiceIsSuspended == mess ||
        kIOMessageServiceIsRequestingClose == mess ||
        kIOFWMessageServiceIsRequestingClose == mess)) {
        fDevice->getNodeIDGeneration(fGeneration, fNodeID, fLocalNodeID);
	messageClients( mess );
        return kIOReturnSuccess;
    }
    return IOService::message(mess, provider, argument );
}

/**
 ** Matching methods
 **/
bool IOFireWireUnit::matchPropertyTable(OSDictionary * table)
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

bool IOFireWireUnit::handleOpen( 	IOService *	  forClient,
                            IOOptionBits	  options,
                            void *		  arg )
{
    bool ok;
    ok = fDevice->open(this, options, arg);
    if(ok)
        ok = IOFireWireNub::handleOpen(forClient, options, arg);
    return ok;
}

void IOFireWireUnit::handleClose(   IOService *	  forClient,
                            IOOptionBits	  options )
{
    IOFireWireNub::handleClose(forClient, options);
    fDevice->close(this, options);
}

