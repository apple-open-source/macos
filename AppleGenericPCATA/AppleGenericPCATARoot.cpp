/*
 * Copyright (c) 1998-2004 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/IODeviceTreeSupport.h>
#include "AppleGenericPCATARoot.h"
#include "AppleGenericPCATAChannel.h"
#include "AppleGenericPCATAKeys.h"

#define super IOService
OSDefineMetaClassAndStructors( AppleGenericPCATARoot, IOService )

//---------------------------------------------------------------------------
//
// Start the root ATA driver
//

static void registerClientApplier( IOService * service, void * context )
{
    if ( service ) service->registerService();
}

bool  AppleGenericPCATARoot::start( IOService * provider )
{
    if ( super::start(provider) != true )
    {
        return false;
    }

    fProvider = provider;
    fProvider->retain();

    fChannels = createATAChannels();
    if (fChannels == 0)
    {
        return false;
    }

    fOpenChannels = OSSet::withCapacity( fChannels->getCount() );
    if ( fOpenChannels == 0 )
    {
        return false;
    }

    // Register a nub for each ATA channel.

	applyToClients( registerClientApplier, 0 );

    return true;
}

//---------------------------------------------------------------------------
//
// Release allocated resources before this driver is destroyed.
//

void AppleGenericPCATARoot::free( void )
{
    if ( fChannels )
    {
        fChannels->release();
        fChannels = 0;
    }

    if ( fOpenChannels )
    {
        fOpenChannels->release();
        fOpenChannels = 0;
    }

    if ( fProvider )
    {
        fProvider->release();
        fProvider = 0;
    }

    super::free();
}

//---------------------------------------------------------------------------
//
// Locate an entry in the device tree that correspond to the channels
// behind the ATA controller. This allows discovery of the ACPI entry
// for ACPI method evaluation, and also uses the ACPI assigned device
// name for a persistent path to the root device.
//

IORegistryEntry * AppleGenericPCATARoot::getDTChannelEntry( int channelID )
{
    IORegistryEntry * entry = 0;
    const char *      location;

    OSIterator * iter = fProvider->getChildIterator( gIODTPlane );
    if (iter == 0) return 0;

    while (( entry = (IORegistryEntry *) iter->getNextObject() ))
    {
        location = entry->getLocation();
        if ( location && strtol(location, 0, 10) == channelID )
        {
            entry->retain();
            break;
        }
    }

    iter->release();
    
    return entry;  // retain held on the entry
}

//---------------------------------------------------------------------------
//
// Create nubs based on the channel information in the driver personality.
//

OSSet * AppleGenericPCATARoot::createATAChannels( void )
{
    OSSet *           nubSet;
    OSDictionary *    channelInfo;
    IORegistryEntry * dtEntry;

    do {
        nubSet = OSSet::withCapacity(4);
        if ( nubSet == 0 )
            break;
        
        if (fProvider->open(this) != true)
            break;
        
        for ( UInt32 channelID = 0; channelID < 2; channelID++ )
        {
            // FIXME: add native mode support
            channelInfo = createNativeModeChannelInfo( channelID );
            if (channelInfo)
            {
                channelInfo->release();
                channelInfo = 0;
                break;
            }

            channelInfo = createLegacyModeChannelInfo( channelID );
            if (channelInfo == 0)
                continue;

            // Create a new controller nub for each ATA channel.
            // Attach this nub as a child.

            AppleGenericPCATAChannel * nub = new AppleGenericPCATAChannel;

            if ( nub )
            {
                dtEntry = getDTChannelEntry( channelID );

                if (nub->init( this, channelInfo, dtEntry ) &&
                    nub->attach( this ))
                {
                    nubSet->setObject( nub );
                }

                if ( dtEntry )
                {
                    dtEntry->release();
                }
                else
                {
                    // Platform did not create a device tree entry for
                    // this ATA channel. Do it here.

                    char channelName[5] = {'C','H','N','_','\0'};
                    IOService * dtRoot;

                    channelName[3] = '0' + channelID;
                    nub->setName( channelName );

                    dtRoot = fProvider;
                    while (dtRoot && dtRoot->inPlane(gIODTPlane) == false)
                    {
                        dtRoot = dtRoot->getProvider();                    
                    }
                    
                    if (dtRoot)
                        nub->attachToParent( dtRoot, gIODTPlane );
                }

                nub->release();
            } /* if (nub) */
            
            channelInfo->release();
        } /* for (channelID) */

        fProvider->close( this );
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

OSDictionary *
AppleGenericPCATARoot::createNativeModeChannelInfo( UInt32 ataChannel )
{
    IOPCIDevice * pciDevice;
    UInt16        cmdPort = 0;
    UInt16        ctrPort = 0;

    pciDevice = OSDynamicCast(IOPCIDevice, fProvider);
    if (pciDevice == 0)
        return 0;

    switch ( ataChannel )
    {
        case PRI_CHANNEL_ID:
            cmdPort = pciDevice->configRead16( kIOPCIConfigBaseAddress0 );
            ctrPort = pciDevice->configRead16( kIOPCIConfigBaseAddress1 );

            // Both address ranges must reside in I/O space.

            if (((cmdPort & 0x1) == 0) || ((ctrPort & 0x1) == 0))
            {
                cmdPort = ctrPort = 0;
                break;
            }

            cmdPort &= ~0x1;  // clear PCI I/O space indicator bit
            ctrPort &= ~0x1;

            if ((cmdPort > 0xFFF8) || (ctrPort > 0xFFF8) ||
                (cmdPort < 0x100)  || (ctrPort < 0x100)  ||
                ((cmdPort == PRI_CMD_ADDR) && (ctrPort == PRI_CTR_ADDR)))
            {
                cmdPort = ctrPort = 0;
            }
            break;

        case SEC_CHANNEL_ID:
            cmdPort = pciDevice->configRead16( kIOPCIConfigBaseAddress2 );
            ctrPort = pciDevice->configRead16( kIOPCIConfigBaseAddress3 );

            // Both address ranges must reside in I/O space.

            if (((cmdPort & 0x1) == 0) || ((ctrPort & 0x1) == 0))
            {
                cmdPort = ctrPort = 0;
                break;
            }

            if ((cmdPort > 0xFFF8) || (ctrPort > 0xFFF8) ||
                (cmdPort < 0x100)  || (ctrPort < 0x100)  ||
                ((cmdPort == SEC_CMD_ADDR) && (ctrPort == SEC_CTR_ADDR)))
            {
                cmdPort = ctrPort = 0;
            }
            break;
    }

    if (cmdPort && ctrPort)
        return createChannelInfo( ataChannel, cmdPort, ctrPort,
                     pciDevice->configRead8(kIOPCIConfigInterruptLine) );
    else
        return 0;
}

//---------------------------------------------------------------------------

OSDictionary *
AppleGenericPCATARoot::createLegacyModeChannelInfo( UInt32 ataChannel )
{
    UInt16  cmdPort = 0;
    UInt16  ctrPort = 0;
    UInt8   isaIrq  = 0;

    switch ( ataChannel )
    {
        case PRI_CHANNEL_ID:
            cmdPort = PRI_CMD_ADDR;
            ctrPort = PRI_CTR_ADDR;
            isaIrq  = PRI_ISA_IRQ;
            break;
        
        case SEC_CHANNEL_ID:
            cmdPort = SEC_CMD_ADDR;
            ctrPort = SEC_CTR_ADDR;
            isaIrq  = SEC_ISA_IRQ;
            break;
    }

    return createChannelInfo( ataChannel, cmdPort, ctrPort, isaIrq );
}

//---------------------------------------------------------------------------

OSDictionary *
AppleGenericPCATARoot::createChannelInfo( UInt32 ataChannel,
                                    UInt16 commandPort,
                                    UInt16 controlPort,
                                    UInt8 interruptVector )
{
    OSDictionary * dict = OSDictionary::withCapacity( 5 );
    OSNumber *     num;

    if ( dict == 0 || commandPort == 0 || controlPort == 0 || 
         interruptVector == 0 || interruptVector == 0xFF )
    {
        if ( dict ) dict->release();
        return 0;
    }

    num = OSNumber::withNumber( ataChannel, 32 );
    if (num)
    {
        dict->setObject( kChannelNumberKey, num );
        num->release();
    }
    
    num = OSNumber::withNumber( commandPort, 16 );
    if (num)
    {
        dict->setObject( kCommandBlockAddressKey, num );
        num->release();
    }

    num = OSNumber::withNumber( controlPort, 16 );
    if (num)
    {
        dict->setObject( kControlBlockAddressKey, num );
        num->release();
    }

    num = OSNumber::withNumber( interruptVector, 32 );
    if (num)
    {
        dict->setObject( kInterruptVectorKey, num );
        num->release();
    }

    dict->setObject( kPIOModeKey, getProperty(kPIOModeKey) );

    return dict;
}

//---------------------------------------------------------------------------
//
// Handle an open request from a client. Several clients can call this
// function and hold an open on this controller.
//

bool AppleGenericPCATARoot::handleOpen( IOService *  client,
                                        IOOptionBits options,
                                        void *       arg )
{
    bool ret = true;

    // Reject open request from unknown clients, or if the client
    // already holds an open.

    if ( ( fChannels->containsObject( client ) == false ) ||
         ( fOpenChannels->containsObject( client ) == true ) )
        return false;

    // First client open will trigger an open to our provider.

    if ( fOpenChannels->getCount() == 0 )
        ret = fProvider->open( this );

    if ( ret )
    {
        fOpenChannels->setObject( client );
        if ( arg ) *((IOService **) arg) = fProvider;
    }

    return ret;
}

//---------------------------------------------------------------------------
//
// Handle a close request from a client.
//

void AppleGenericPCATARoot::handleClose( IOService *  client,
                                         IOOptionBits options )
{
    // Reject close request from clients that do not hold an open.

    if ( fOpenChannels->containsObject( client ) == false ) return;

    fOpenChannels->removeObject( client );

    // Last client close will trigger a close to our provider.

    if ( fOpenChannels->getCount() == 0 )
        fProvider->close( this );
}

//---------------------------------------------------------------------------
//
// Report if the specified client has an open on us.
//

bool AppleGenericPCATARoot::handleIsOpen( const IOService * client ) const
{
    if ( client )
        return fOpenChannels->containsObject( client );
    else
        return ( fOpenChannels->getCount() != 0 );
}

//---------------------------------------------------------------------------
//
// PCI specific ATA root driver.
//

#undef  super
#define super AppleGenericPCATARoot
OSDefineMetaClassAndStructors( AppleGenericPCATAPCIRoot,
                               AppleGenericPCATARoot )

IOService * AppleGenericPCATAPCIRoot::probe( IOService * provider,
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
    
    return this;
}
