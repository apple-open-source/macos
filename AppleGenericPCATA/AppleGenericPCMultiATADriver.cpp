/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/IOLib.h>
#include "AppleGenericPCMultiATADriver.h"
#include "AppleGenericPCATAController.h"
#include "AppleGenericPCATAKeys.h"

#define super IOService
OSDefineMetaClassAndStructors( AppleGenericPCMultiATADriver, IOService )

//---------------------------------------------------------------------------
//
// Start the Multi-ATA controller driver.
//

static void registerClientApplier( IOService * service, void * context )
{
    if ( service ) service->registerService();
}

bool 
AppleGenericPCMultiATADriver::start( IOService * provider )
{
    if ( super::start(provider) != true )
    {
        return false;
    }

    _provider = provider;
    _provider->retain();

    _nubs = createControllerNubs();
    if ( _nubs == 0 )
    {
        return false;
    }

    _openNubs = OSSet::withCapacity( _nubs->getCount() );
    if ( _openNubs == 0 )
    {
        return false;
    }

    // Register a controller for each ATA channel.

	applyToClients( registerClientApplier, 0 );

    return true;
}

//---------------------------------------------------------------------------
//
// Release allocated resources before this object is destroyed.
//

void 
AppleGenericPCMultiATADriver::free()
{
    if ( _nubs )
    {
        _nubs->release();
        _nubs = 0;
    }

    if ( _openNubs )
    {
        _openNubs->release();
        _openNubs = 0;
    }

    if ( _provider )
    {
        _provider->release();
        _provider = 0;
    }

    super::free();
}

//---------------------------------------------------------------------------
//
// Create nubs based on the channel information in the driver personality.
//

OSSet *
AppleGenericPCMultiATADriver::createControllerNubs()
{
    OSSet *        nubSet;
    OSArray *      channels;
    OSDictionary * channelInfo;
    AppleGenericPCATAController * nub;

    do {
        nubSet = OSSet::withCapacity(4);
        if ( nubSet == 0 )
            break;

        // Fetch the ATA channel description array from the property table.

        channels = OSDynamicCast( OSArray, getProperty( kATAChannelsKey ) );
        if ( channels == 0 )
            break;

        for ( UInt32 i = 0; ; i++)
        {
            channelInfo = OSDynamicCast( OSDictionary, channels->getObject(i) );
            if ( channelInfo == 0 )
                break;

            // Create a new controller nub for each ATA channel.
            // Attach this nub as a child.

            nub = new AppleGenericPCATAController;

            if ( nub )
            {
                if ( nub->init( channelInfo ) &&
                     nub->attach( this ) )
                {
                    nubSet->setObject( nub );
                }
                nub->release();
            }
        }
    }
    while ( false );

    // Release and invalidate an empty set.

    if ( nubSet && (nubSet->getCount() == 0) )
    {
        nubSet->release();
        nubSet = 0;
    }

    return nubSet;
}

//---------------------------------------------------------------------------
//
// Handle an open request from a client. Several clients can call this
// function and hold an open on this controller.
//

bool
AppleGenericPCMultiATADriver::handleOpen( IOService *  client,
                                          IOOptionBits options,
                                          void *       arg )
{
    bool ret = true;

    // Reject open request from unknown clients, or if the client
    // already holds an open.

    if ( ( _nubs->containsObject( client ) == false ) ||
         ( _openNubs->containsObject( client ) == true ) )
        return false;

    // First client open will trigger an open to our provider.

    if ( _openNubs->getCount() == 0 )
        ret = _provider->open( this );

    if ( ret )
    {
        _openNubs->setObject( client );
        if ( arg ) *((IOService **) arg) = _provider;
    }

    return ret;
}

//---------------------------------------------------------------------------
//
// Handle a close request from a client.
//

void
AppleGenericPCMultiATADriver::handleClose( IOService *  client,
                                           IOOptionBits options )
{
    // Reject close request from clients that do not hold an open.

    if ( _openNubs->containsObject( client ) == false ) return;

    _openNubs->removeObject( client );

    // Last client close will trigger a close to our provider.

    if ( _openNubs->getCount() == 0 )
        _provider->close( this );
}

//---------------------------------------------------------------------------
//
// Report if the specified client has an open on us.
//

bool
AppleGenericPCMultiATADriver::handleIsOpen( const IOService * client ) const
{
    if ( client )
        return _openNubs->containsObject( client );
    else
        return ( _openNubs->getCount() != 0 );
}

//---------------------------------------------------------------------------
//
// PCI specific Multi-ATA controller driver.
//

#include <IOKit/pci/IOPCIDevice.h>

#undef  super
#define super AppleGenericPCMultiATADriver
OSDefineMetaClassAndStructors( AppleGenericPCMultiPCIATADriver,
                               AppleGenericPCMultiATADriver )

IOService *
AppleGenericPCMultiPCIATADriver::probe( IOService * provider,
                                        SInt32 *    score )
{
    IOPCIDevice * pciDevice;

    // Let our superclass probe first.

    if ( super::probe( provider, score ) == 0 )
        return 0;

    // Verify the provider type.

    pciDevice = OSDynamicCast( IOPCIDevice, provider );
    if ( pciDevice == 0 )
        return 0;

    // BIOS did not enable I/O space decoding.
    // For now assume the channel is disabled.

    if ( (pciDevice->configRead16( kIOPCIConfigCommand ) &
          kIOPCICommandIOSpace) == 0 )
    {
        return 0;
    }

    // Perhaps a check to read class code register to make
    // sure it is an IDE controller?
    
    return this;
}
