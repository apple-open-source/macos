/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "AppleGenericPCATAController.h"
#include "AppleGenericPCATAKeys.h"

#define super IOService
OSDefineMetaClassAndStructors( AppleGenericPCATAController, IOService )

static bool
getOSNumberValue( const OSDictionary * dict,
                  const char         * key,
                  UInt32             * outValue )
{
    OSNumber * num = OSDynamicCast( OSNumber, dict->getObject( key ) );
    if ( num )
    {
        *outValue = num->unsigned32BitValue();
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------
//
// Create the interrupt properties.
//

bool
AppleGenericPCATAController::setupInterrupt( IOService *provider, UInt32 line )
{
    if (_irqSet) return true;

    IOReturn ret = provider->callPlatformFunction( "SetDeviceInterrupts",
              /* waitForFunction */ false,
              /* nub             */ this,
              /* vectors         */ (void *) &line,
              /* vectorCount     */ (void *) 1,
              /* exclusive       */ (void *) false );     
    if (ret == kIOReturnSuccess) {
        _irqSet = true;
        return true;
    } else {
        return false;
    }
}

//---------------------------------------------------------------------------
//
// Initialize the ATA controller object.
//

bool
AppleGenericPCATAController::init( OSDictionary * dictionary )
{
    if ( super::init(dictionary) == false )
        return false;

    // Fetch the port address and interrupt line properties from
    // the dictionary provided.

	if ( !getOSNumberValue( dictionary, kPortAddressKey, &_ioPorts )
      || !getOSNumberValue( dictionary, kInterruptLineKey, &_irq )
      || !getOSNumberValue( dictionary, kPIOModeKey, &_pioMode )
       ) return false;

    return true;
}

//---------------------------------------------------------------------------
//
// Handle open and close from our client.
//

bool
AppleGenericPCATAController::handleOpen( IOService *  client,
                                         IOOptionBits options,
                                         void *       arg )
{
    bool ret = false;

    if ( _provider && _provider->open( this, 0, arg ) )
    {
        ret = super::handleOpen( client, options, arg );
        if ( ret == false )
            _provider->close( this );
    }
    
    return ret;
}

void
AppleGenericPCATAController::handleClose( IOService *  client,
                                          IOOptionBits options )
{
    super::handleClose( client, options );
	if ( _provider ) _provider->close( this );
}

//---------------------------------------------------------------------------
//
// Set and clear our provider reference when attached and detached
// to a parent in the service plane.
//

bool
AppleGenericPCATAController::attachToParent( IORegistryEntry *       parent,
                                             const IORegistryPlane * plane )
{
	bool ret = super::attachToParent( parent, plane );
	if ( ret && (plane == gIOServicePlane) ) {
            _provider = (IOService *) parent;
            if ( !setupInterrupt( parent, _irq ) )
                return false;
        }

	return ret;
}

void
AppleGenericPCATAController::detachFromParent( IORegistryEntry *       parent,
                                               const IORegistryPlane * plane )
{
    if ( plane == gIOServicePlane ) _provider = 0;
    return super::detachFromParent( parent, plane );
}

//---------------------------------------------------------------------------
//
// Accessor functions to assist our client.
//

UInt32
AppleGenericPCATAController::getIOPorts() const
{
    return _ioPorts;
}

UInt32
AppleGenericPCATAController::getInterruptLine() const
{
    return _irq;
}

UInt32
AppleGenericPCATAController::getPIOMode() const
{
    return _pioMode;
}
