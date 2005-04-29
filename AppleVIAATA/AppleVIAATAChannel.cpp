/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/IODeviceTreeSupport.h>
#include "AppleVIAATAChannel.h"
#include "AppleVIAATARoot.h"
#include "AppleVIAATAHardware.h"

#define super IOService
OSDefineMetaClassAndStructors( AppleVIAATAChannel, IOService )

//---------------------------------------------------------------------------

bool AppleVIAATAChannel::mergeProperties( OSDictionary * properties )
{
    bool success = true;
    OSCollectionIterator * propIter =
        OSCollectionIterator::withCollection( properties );

    if ( propIter )
    {
        const OSSymbol * propKey;
        while ((propKey = (const OSSymbol *) propIter->getNextObject()))
        {
            if (setProperty(propKey, properties->getObject(propKey)) == false)
            {
                success = false;
                break;
            }
        }
        propIter->release();
    }
    return success;
}

//---------------------------------------------------------------------------
//
// Initialize the ATA channel nub.
//

bool AppleVIAATAChannel::init( IOService *       provider,
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

    fProvider = OSDynamicCast(AppleVIAATARoot, provider);
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

bool AppleVIAATAChannel::handleOpen( IOService *  client,
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

void AppleVIAATAChannel::handleClose( IOService *  client,
                                      IOOptionBits options )
{
    super::handleClose( client, options );
    if ( fProvider ) fProvider->close( this );
}

//---------------------------------------------------------------------------

UInt16 AppleVIAATAChannel::getCommandBlockAddress( void ) const
{
    UInt16 value = 0xFFF0;
    getNumberValue( kCommandBlockAddressKey, &value, 16 );
    return value;
}

UInt16 AppleVIAATAChannel::getControlBlockAddress( void ) const
{
    UInt16 value = 0xFFF0;
    getNumberValue( kControlBlockAddressKey, &value, 16 );
    return value;
}

UInt32 AppleVIAATAChannel::getInterruptVector( void ) const
{
    UInt32 value = 0xFF;
    getNumberValue( kInterruptVectorKey, &value, 32 );
    return value;
}

UInt32 AppleVIAATAChannel::getChannelNumber( void ) const
{
    UInt32 value = 0xFF;
    getNumberValue( kChannelNumberKey, &value, 32 );
    return value;
}

UInt32 AppleVIAATAChannel::getUltraDMAModeMask( void ) const
{
    return fProvider->getUltraDMAModeMask();
}

UInt32 AppleVIAATAChannel::getHardwareType( void ) const
{
    return fProvider->getHardwareType();
}

UInt32 AppleVIAATAChannel::getHardwareFlags( void ) const
{
    return fProvider->getHardwareFlags();
}

const char * AppleVIAATAChannel::getHardwareName( void ) const
{
    OSString * name;

    name = OSDynamicCast(OSString,
                         fProvider->getProperty(kHardwareNameKey));
    if (name)
        return name->getCStringNoCopy();
    else
        return "UNKNOWN";
}

void AppleVIAATAChannel::pciConfigWrite8( UInt8 offset,
                                          UInt8 data,
                                          UInt8 mask )
{
    fProvider->pciConfigWrite8( offset, data, mask );
}

void AppleVIAATAChannel::pciConfigWrite32( UInt8  offset,
                                           UInt32 data,
                                           UInt32 mask )
{
    fProvider->pciConfigWrite32( offset, data, mask );
}

UInt8 AppleVIAATAChannel::pciConfigRead8( UInt8 offset )
{
    return fProvider->pciConfigRead8( offset );
}

UInt16 AppleVIAATAChannel::pciConfigRead16( UInt8 offset )
{
    return fProvider->pciConfigRead16( offset );
}

UInt32 AppleVIAATAChannel::pciConfigRead32( UInt8 offset )
{
    return fProvider->pciConfigRead32( offset );
}

//---------------------------------------------------------------------------

bool AppleVIAATAChannel::getNumberValue( const char * propKey,
                                         void       * outValue,
                                         UInt32       outBits ) const
{
    OSNumber * num = OSDynamicCast( OSNumber, getProperty( propKey ) );
    bool   success = false;

    if ( num )
    {
        success = true;

        switch ( outBits )
        {
            case 32:
                *(UInt32 *) outValue = num->unsigned32BitValue();
                break;
            
            case 16:
                *(UInt16 *) outValue = num->unsigned16BitValue();
                break;

            case 8:
                *(UInt8 *) outValue = num->unsigned8BitValue();
                break;
            
            default:
                success = false;
                break;
        }
    }
    return success;
}
