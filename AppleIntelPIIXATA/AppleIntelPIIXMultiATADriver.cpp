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

#include <IOKit/IOLib.h>
#include <IOKit/pci/IOPCIDevice.h>
#include "AppleIntelPIIXMultiATADriver.h"
#include "AppleIntelPIIXATAController.h"
#include "AppleIntelPIIXATAKeys.h"

#define super IOService
OSDefineMetaClassAndStructors( AppleIntelPIIXMultiATADriver, IOService )

//---------------------------------------------------------------------------
//
// Probe for PCI device and verify that I/O space decoding is enabled.
//

IOService *
AppleIntelPIIXMultiATADriver::probe( IOService * provider,
                                     SInt32 *    score )
{
    IOPCIDevice * pciDevice;

    // Let our superclass probe first.

    if ( super::probe( provider, score ) == 0 )
    {
        return 0;
    }

    // Verify the provider type.

    pciDevice = OSDynamicCast( IOPCIDevice, provider );
    if ( pciDevice == 0 )
    {
        return 0;
    }

    // BIOS did not enable I/O space decoding.
    // For now assume the controller is disabled.

    if ( (pciDevice->configRead16( kIOPCIConfigCommand ) &
          kIOPCICommandIOSpace) == 0 )
    {
        return 0;
    }

    return this;
}

//---------------------------------------------------------------------------
//
// Start the PIIX Multi-ATA controller driver.
//

bool 
AppleIntelPIIXMultiATADriver::start( IOService * provider )
{
    if ( super::start(provider) != true )
    {
        IOLog("%s: super start failed\n", getName());
        return false;
    }

    // Allocate a mutex to serialize access to PCI config space.
    
    _pciConfigLock = IOLockAlloc();
    if ( _pciConfigLock == 0 )
    {
        IOLog("%s: lock allocation failed\n", getName());
        return false;
    }

    _provider = OSDynamicCast( IOPCIDevice, provider );
    if ( _provider == 0 )
    {
        IOLog("%s: provider is not IOPCIDevice\n", getName());
        return false;
    }
    _provider->retain();

    _nubs = createControllerNubs();
    if ( _nubs == 0 )
    {
        IOLog("%s: unable to create controller nubs\n", getName());
        return false;
    }

    _openNubs = OSSet::withCapacity( _nubs->getCount() );
    if ( _openNubs == 0 )
    {
        IOLog("%s: OSSet allocation failed\n", getName());
        return false;
    }

    return true;
}

//---------------------------------------------------------------------------
//
// Release allocated resources before this object is destroyed.
//

void 
AppleIntelPIIXMultiATADriver::free()
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

    if ( _pciConfigLock )
    {
        IOLockFree( _pciConfigLock );
        _pciConfigLock = 0;
    }

    super::free();
}

//---------------------------------------------------------------------------
//
// Create nubs based on the channel information in the driver personality.
//

OSSet *
AppleIntelPIIXMultiATADriver::createControllerNubs()
{
    OSSet *        nubSet;
    OSArray *      channels;
    OSDictionary * channelInfo;
    AppleIntelPIIXATAController * nub;

    do {
        nubSet = OSSet::withCapacity(4);
        if ( nubSet == 0 )
            break;

        // Fetch the ATA channel description array from the property table.

        channels = OSDynamicCast( OSArray, getProperty( kATAChannelsKey ) );
        if ( channels == 0 )
            break;

        for ( UInt32 i = 0; i < 2; i++)
        {
            channelInfo = OSDynamicCast( OSDictionary, channels->getObject(i) );
            if ( channelInfo == 0 )
                break;

            // Create a new controller nub for each ATA channel.
            // Attach this nub as a child.

            nub = new AppleIntelPIIXATAController;

            if ( nub )
            {
                if ( nub->init( this, channelInfo ) &&
                     nub->attach( this )            &&
                     nubSet->setObject( nub )  )
                {
                    nub->registerService();
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
AppleIntelPIIXMultiATADriver::handleOpen( IOService *  client,
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
AppleIntelPIIXMultiATADriver::handleClose( IOService *  client,
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
AppleIntelPIIXMultiATADriver::handleIsOpen( const IOService * client ) const
{
    if ( client )
        return _openNubs->containsObject( client );
    else
        return ( _openNubs->getCount() != 0 );
}

//---------------------------------------------------------------------------
//
// Helpers for non 4-byte aligned PCI config space writes.
// WARNING: These will not work on a big-endian machine.
//

void
AppleIntelPIIXMultiATADriver::pciConfigWrite8( UInt8 offset,
                                               UInt8 data,
                                               UInt8 mask )
{
    union {
        UInt32 b32;
        UInt8  b8[4];
    } u;
 
    UInt8 byte = offset & 0x03;
    // offset &= ~0x03;

    IOLockLock( _pciConfigLock );

    u.b32 = _provider->configRead32( offset );
    u.b8[byte] &= ~mask;
    u.b8[byte] |= (mask & data);
    _provider->configWrite32( offset, u.b32 );

    IOLockUnlock( _pciConfigLock );
}

void
AppleIntelPIIXMultiATADriver::pciConfigWrite16( UInt8  offset,
                                                UInt16 data,
                                                UInt16 mask )
{
    union {
        UInt32 b32;
        UInt16 b16[2];
    } u;
 
    UInt8 word = (offset >> 1) & 0x01;
    // offset &= ~0x03;

    IOLockLock( _pciConfigLock );

    u.b32 = _provider->configRead32( offset );
    u.b16[word] &= ~mask;
    u.b16[word] |= (mask & data);
    _provider->configWrite32( offset, u.b32 );

    IOLockUnlock( _pciConfigLock );
}

bool
AppleIntelPIIXMultiATADriver::serializeProperties( OSSerialize * s ) const
{
    AppleIntelPIIXMultiATADriver * self;
    char timingString[80];

    if ( _provider )
    {
        sprintf( timingString, "0x40=%08lx 0x44=%08lx 0x48=%08lx 0x54=%04x",
                 _provider->configRead32( 0x40 ),
                 _provider->configRead32( 0x44 ),
                 _provider->configRead32( 0x48 ),
                 _provider->configRead16( 0x54 ) );

        self = (AppleIntelPIIXMultiATADriver *) this;
        self->setProperty( "PCI Timing Registers", timingString );
    }

	return super::serializeProperties(s);
}
