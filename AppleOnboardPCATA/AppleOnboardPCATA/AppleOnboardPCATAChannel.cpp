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

#include <IOKit/IODeviceTreeSupport.h>
#include "AppleOnboardPCATARoot.h"
#include "AppleOnboardPCATAChannel.h"
#include "AppleOnboardPCATAShared.h"

#define CLASS AppleOnboardPCATAChannel
#define super IOService

OSDefineMetaClassAndStructors( AppleOnboardPCATAChannel, IOService )

//---------------------------------------------------------------------------

bool CLASS::mergeProperties( OSDictionary * dict )
{
    bool success = true;
    OSCollectionIterator * iter = OSCollectionIterator::withCollection(dict);

    if ( iter )
    {
        const OSSymbol * key;
        while ((key = (const OSSymbol *) iter->getNextObject()))
        {
            if (setProperty(key, dict->getObject(key)) == false)
            {
                success = false;
                break;
            }
        }
        iter->release();
    }
    return success;
}

//---------------------------------------------------------------------------
//
// Initialize the ATA channel nub.
//

bool CLASS::init( IOService *       provider,
                  OSDictionary *    properties,
                  IORegistryEntry * dtEntry )
{
    if ( dtEntry )
    {
        if ( super::init( dtEntry, gIODTPlane ) == false ||
             mergeProperties( properties ) == false )
             return false;
    }
    else
    {
        if ( super::init( properties ) == false )
             return false;
    }

    fProvider = OSDynamicCast(AppleOnboardPCATARoot, provider);
    if (fProvider == 0)
        return false;

    // Call platform to register the interrupt assigned to each ATA
    // channel. For PCI interrupts (native mode), each channel will
    // share the same interrupt vector assigned to the PCI device.
    // Legacy mode channels will attempt to register IRQ 14 and 15.

    UInt32 vector = getInterruptVector();
    if (provider->callPlatformFunction( "SetDeviceInterrupts",
                   /* waitForFunction */ false,
                   /* nub             */ this,
                   /* vectors         */ (void *) &vector,
                   /* vectorCount     */ (void *) 1,
                   /* exclusive       */ (void *) false )
                                         != kIOReturnSuccess)
    {
        return false;
    }

    setLocation( getChannelNumber() ? "1" : "0" );

    return true;
}

//---------------------------------------------------------------------------
//
// Handle open and close from our exclusive client.
//

bool CLASS::handleOpen( IOService *  client,
                        IOOptionBits options,
                        void *       arg )
{
    bool ret = false;

    if ( fProvider && fProvider->open( this, options, arg ) )
    {
        ret = super::handleOpen( client, options, arg );
        if ( ret == false )
            fProvider->close( this );
    }
    
    return ret;
}

void CLASS::handleClose( IOService *  client,
                         IOOptionBits options )
{
    super::handleClose( client, options );
    if ( fProvider ) fProvider->close( this );
}

//---------------------------------------------------------------------------

UInt32 CLASS::getCommandBlockAddress( void ) const
{
    return getNumberPropertyValue( kCommandBlockAddressKey );
}

UInt32 CLASS::getControlBlockAddress( void ) const
{
    return getNumberPropertyValue( kControlBlockAddressKey );
}

UInt32 CLASS::getInterruptVector( void ) const
{
    return getNumberPropertyValue( kInterruptVectorKey );
}

UInt32 CLASS::getChannelNumber( void ) const
{
    return getNumberPropertyValue( kChannelNumberKey );
}

const char * CLASS::getHardwareVendorName( void ) const
{
    return fProvider->getHardwareVendorName();
}

const char * CLASS::getHardwareDeviceName( void ) const
{
    return fProvider->getHardwareDeviceName();
}

void CLASS::pciConfigWrite8( UInt8 offset, UInt8 data,  UInt8 mask )
{
    fProvider->pciConfigWrite8( offset, data, mask );
}

void CLASS::pciConfigWrite16( UInt8 offset, UInt16 data, UInt16 mask )
{
    fProvider->pciConfigWrite16( offset, data, mask );
}

void CLASS::pciConfigWrite32( UInt8 offset, UInt32 data, UInt32 mask )
{
    fProvider->pciConfigWrite32( offset, data, mask );
}

UInt8 CLASS::pciConfigRead8( UInt8 offset )
{
    return fProvider->pciConfigRead8( offset );
}

UInt16 CLASS::pciConfigRead16( UInt8 offset )
{
    return fProvider->pciConfigRead16( offset );
}

UInt32 CLASS::pciConfigRead32( UInt8 offset )
{
    return fProvider->pciConfigRead32( offset );
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

//---------------------------------------------------------------------------

bool CLASS::matchPropertyTable( OSDictionary * table,
                                SInt32 *       score )
{
    OSString * name1;
    OSString * name2;

    if (!table || !score || !super::matchPropertyTable(table, score))
        return false;

    name1 = OSDynamicCast(OSString,
                          table->getObject(kRootHardwareVendorNameKey));
    name2 = OSDynamicCast(OSString,
                          fProvider->getProperty(kRootHardwareVendorNameKey));

    if (!name1 || !name2 || !name1->isEqualTo(name2))
    {
        return false;
    }

    return true;
}
