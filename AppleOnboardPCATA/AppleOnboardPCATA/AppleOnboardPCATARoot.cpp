/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 2.0 (the
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
#include <IOKit/IODeviceTreeSupport.h>
#include "AppleOnboardPCATARoot.h"
#include "AppleOnboardPCATAChannel.h"
#include "AppleOnboardPCATAShared.h"

#define CLASS AppleOnboardPCATARoot
#define super IOService

OSDefineMetaClassAndStructors( AppleOnboardPCATARoot, IOService )

//---------------------------------------------------------------------------
//
// Probe for PCI device and verify that I/O space decoding is enabled.
//

IOService * CLASS::probe( IOService * provider, SInt32 * score )
{
    IOPCIDevice * pciDevice;
    UInt32        enableFlag = 1;

    if (super::probe( provider, score ) == 0)
        return 0;

    // Enter "pcata=0" to disable this driver.
    // Useful during development to load the polled-mode driver instead.

    PE_parse_boot_arg("pcata", &enableFlag);
    if (enableFlag == 0)
        return 0;

    // Verify provider is an IOPCIDevice.

    pciDevice = OSDynamicCast(IOPCIDevice, provider);
    if (pciDevice == 0)
        return 0;

    // Fail if I/O space decoding is disabled.
    // Controller may be disabled in the BIOS.

    if ((pciDevice->configRead16(kIOPCIConfigCommand) &
         kIOPCICommandIOSpace) == 0)
    {
        return 0;
    }

    return this;
}

//---------------------------------------------------------------------------
//
// Start the Root ATA driver. Probe both primary and secondary ATA channels.
//

static void registerClientApplier( IOService * service, void * context )
{
    if (service) service->registerService();
}

bool CLASS::start( IOService * provider )
{
    UInt32 numChannels;

    if (super::start(provider) != true)
        return false;

    fProvider = OSDynamicCast( IOPCIDevice, provider );
    if (fProvider == 0)
        return false;

    fProvider->retain();

    // Enable bus master.

    fProvider->setBusMasterEnable( true );

    // Allocate a mutex to serialize access to PCI config space from
    // the primary and secondary ATA channels.

    fPCILock = IOLockAlloc();
    if (fPCILock == 0)
        return false;

    numChannels = getNumberPropertyValue( kATAChannelCount );
    if (numChannels == 0)
        numChannels = 2;

    fChannels = createATAChannels( numChannels );
    if (fChannels == 0)
        return false;

    fOpenChannels = OSSet::withCapacity( fChannels->getCount() );
    if (fOpenChannels == 0)
        return false;

	// Start ATA channel client matching.

    applyToClients( registerClientApplier, 0 );

    return true;
}

//---------------------------------------------------------------------------
//
// Release allocated resources before this object is destroyed.
//

void CLASS::free( void )
{
    if (fChannels)
    {
        fChannels->release();
        fChannels = 0;
    }

    if (fOpenChannels)
    {
        fOpenChannels->release();
        fOpenChannels = 0;
    }

    if (fProvider)
    {
        fProvider->release();
        fProvider = 0;
    }

    if (fPCILock)
    {
        IOLockFree( fPCILock );
        fPCILock = 0;
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

IORegistryEntry * CLASS::getDTChannelEntry( UInt32 channelID )
{
    IORegistryEntry * entry = 0;
    const char *      location;

    OSIterator * iter = fProvider->getChildIterator( gIODTPlane );
    if (iter == 0) return 0;

    while ((entry = (IORegistryEntry *) iter->getNextObject()))
    {
        location = entry->getLocation();
        if (location && strtol(location, 0, 10) == (int)channelID)
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

OSSet * CLASS::createATAChannels( UInt32 maxChannelCount )
{
    OSSet *           nubSet;
    OSDictionary *    channelInfo;
    IORegistryEntry * dtEntry;

    do {
        nubSet = OSSet::withCapacity(maxChannelCount);
        if (nubSet == 0)
            break;

        if (fProvider->open(this) != true)
            break;

        for ( UInt32 channelID = 0;
              channelID < maxChannelCount; channelID++ )
        {        
            // Create a dictionary for the channel info. Use native mode
            // settings if possible, else default to legacy mode.

            channelInfo = createNativeModeChannelInfo( channelID );
            if (channelInfo == 0)
                channelInfo = createLegacyModeChannelInfo( channelID );
            if (channelInfo == 0)
                continue;

            // Create a nub for each ATA channel.

            AppleOnboardPCATAChannel * channelNub;

            channelNub = new AppleOnboardPCATAChannel;
            if ( channelNub )
            {
                dtEntry = getDTChannelEntry( channelID );

                // Invoke special init method in channel nub.

                if (channelNub->init( this, channelInfo, dtEntry ) &&
                    channelNub->attach( this ))
                {
                    nubSet->setObject( channelNub );
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

                    channelName[3] = '0' + channelID;
                    channelNub->setName( channelName );

                    if (fProvider->inPlane(gIODTPlane))
                    {
                        channelNub->attachToParent( fProvider, gIODTPlane );
                    }
                }

                channelNub->release();
            }

            channelInfo->release();
        }

        fProvider->close( this );
    }
    while ( false );

    // Release and invalidate an empty set.

    if (nubSet && (nubSet->getCount() == 0))
    {
        nubSet->release();
        nubSet = 0;
    }

    return nubSet;
}

//---------------------------------------------------------------------------

OSDictionary * CLASS::createNativeModeChannelInfo( UInt32 channelID )
{
    UInt8  pi = fProvider->configRead8( kIOPCIConfigClassCode );
    UInt16 cmdPort = 0;
    UInt16 ctrPort = 0;

    switch ( channelID )
    {
        case kPrimaryChannelID:
            if ((pi & PCI_ATA_PRI_NATIVE_MASK) == PCI_ATA_PRI_NATIVE_MASK)
            {
                cmdPort = fProvider->configRead16( kIOPCIConfigBaseAddress0 );
                ctrPort = fProvider->configRead16( kIOPCIConfigBaseAddress1 );

                cmdPort &= ~0x1;  // clear IOS
                ctrPort &= ~0x1;

                // Programming interface byte indicate that native mode
                // is supported and active, but the controller has been
                // assigned legacy ranges. Force legacy mode configuration
                // which is safest. PCI INT# interrupts are not wired
                // properly for some machines in this state.

                if ( cmdPort == kPrimaryCommandPort &&
                     ctrPort == kPrimaryControlPort )
                {
                     cmdPort = ctrPort = 0;
                }
            }
            break;

        case kSecondaryChannelID:
            if ((pi & PCI_ATA_SEC_NATIVE_MASK) == PCI_ATA_SEC_NATIVE_MASK)
            {
                cmdPort = fProvider->configRead16( kIOPCIConfigBaseAddress2 );
                ctrPort = fProvider->configRead16( kIOPCIConfigBaseAddress3 );

                cmdPort &= ~0x1;  // clear IOS
                ctrPort &= ~0x1;

                if ( cmdPort == kSecondaryCommandPort &&
                     ctrPort == kSecondaryControlPort )
                {
                     cmdPort = ctrPort = 0;
                }
            }
            break;
    }

    if (cmdPort && ctrPort)
        return createChannelInfo( channelID, cmdPort, ctrPort,
                     fProvider->configRead8(kIOPCIConfigInterruptLine) );
    else
        return 0;
}

//---------------------------------------------------------------------------

OSDictionary * CLASS::createLegacyModeChannelInfo( UInt32 channelID )
{
    UInt16  cmdPort = 0;
    UInt16  ctrPort = 0;
    UInt8   isaIrq  = 0;

    switch ( channelID )
    {
        case kPrimaryChannelID:
            cmdPort = kPrimaryCommandPort;
            ctrPort = kPrimaryControlPort;
            isaIrq  = kPrimaryIRQ;
            break;
        
        case kSecondaryChannelID:
            cmdPort = kSecondaryCommandPort;
            ctrPort = kSecondaryControlPort;
            isaIrq  = kSecondaryIRQ;
            break;
    }

    return createChannelInfo( channelID, cmdPort, ctrPort, isaIrq );
}

//---------------------------------------------------------------------------

OSDictionary * CLASS::createChannelInfo( UInt32 channelID,
                                         UInt32 commandPort,
                                         UInt32 controlPort,
                                         UInt32 interruptVector )
{
    OSDictionary * dict = OSDictionary::withCapacity( 4 );
    OSNumber *     num;

    if ( dict == 0 || commandPort == 0 || controlPort == 0 || 
         interruptVector == 0 || interruptVector == 0xFF )
    {
        if (dict) dict->release();
        return 0;
    }

    num = OSNumber::withNumber( channelID, 32 );
    if (num)
    {
        dict->setObject( kChannelNumberKey, num );
        num->release();
    }

    num = OSNumber::withNumber( commandPort, 32 );
    if (num)
    {
        dict->setObject( kCommandBlockAddressKey, num );
        num->release();
    }

    num = OSNumber::withNumber( controlPort, 32 );
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

    return dict;
}

//---------------------------------------------------------------------------
//
// Handle an open request from a client. Multiple channel nubs can hold
// an open on the root driver.
//

bool CLASS::handleOpen( IOService *  client,
                        IOOptionBits options,
                        void *       arg )
{
    bool ret = true;

    // Reject open request from unknown clients, or if the client
    // already holds an open.

    if ((fChannels->containsObject(client) == false) ||
        (fOpenChannels->containsObject(client) == true))
        return false;

    // First client open will trigger an open to our provider.

    if (fOpenChannels->getCount() == 0)
        ret = fProvider->open(this);

    if (ret == true)
    {
        fOpenChannels->setObject(client);

        // Return the PCI device to the client
        if ( arg ) *((IOService **) arg) = fProvider;
    }

    return ret;
}

//---------------------------------------------------------------------------
//
// Handle a close request from a client.
//

void CLASS::handleClose( IOService *  client,
                         IOOptionBits options )
{
    // Reject close request from clients that do not hold an open.

    if (fOpenChannels->containsObject(client) == false) return;

    fOpenChannels->removeObject(client);

    // Last client close will trigger a close to our provider.

    if (fOpenChannels->getCount() == 0)
        fProvider->close(this);
}

//---------------------------------------------------------------------------
//
// Report if the specified client (or any client) has an open on us.
//

bool CLASS::handleIsOpen( const IOService * client ) const
{
    if (client)
        return fOpenChannels->containsObject(client);
    else
        return (fOpenChannels->getCount() != 0);
}

//---------------------------------------------------------------------------

UInt32 CLASS::getNumberPropertyValue( const char * propKey ) const
{
    OSNumber * num = OSDynamicCast(OSNumber, getProperty(propKey));

    if (num)
        return num->unsigned32BitValue();
    else
        return 0;
}

const char * CLASS::getStringPropertyValue( const char * propKey ) const
{
    OSString * str = OSDynamicCast(OSString, getProperty(propKey));

    if (str)
        return str->getCStringNoCopy();
    else
        return "";
}

const char * CLASS::getHardwareVendorName( void ) const
{
    return getStringPropertyValue( kRootHardwareVendorNameKey );
}

const char * CLASS::getHardwareDeviceName( void ) const
{
    return getStringPropertyValue( kRootHardwareDeviceNameKey );
}

//---------------------------------------------------------------------------

void CLASS::pciConfigWrite8( UInt8 offset, UInt8 data, UInt8 mask )
{
    UInt8 u8;

    IOLockLock( fPCILock );

    u8 = fProvider->configRead8( offset );
    u8 &= ~mask;
    u8 |= (mask & data);
    fProvider->configWrite8( offset, u8 );

    IOLockUnlock( fPCILock );
}

void CLASS::pciConfigWrite16( UInt8 offset, UInt16 data, UInt16 mask )
{
    UInt16 u16;

    IOLockLock( fPCILock );

    u16 = fProvider->configRead16( offset );
    u16 &= ~mask;
    u16 |= (mask & data);
    fProvider->configWrite16( offset, u16 );

    IOLockUnlock( fPCILock );
}

void CLASS::pciConfigWrite32( UInt8 offset, UInt32 data, UInt32 mask )
{
    UInt32 u32;

    IOLockLock( fPCILock );

    u32 = fProvider->configRead32( offset );
    u32 &= ~mask;
    u32 |= (mask & data);
    fProvider->configWrite32( offset, u32 );

    IOLockUnlock( fPCILock );
}

UInt8 CLASS::pciConfigRead8( UInt8 offset )
{
    return fProvider->configRead8( offset );
}

UInt16 CLASS::pciConfigRead16( UInt8 offset )
{
    return fProvider->configRead16( offset );
}

UInt32 CLASS::pciConfigRead32( UInt8 offset )
{
    return fProvider->configRead32( offset );
}
