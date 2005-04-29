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

#include <IOKit/IODeviceTreeSupport.h>
#include "AppleGenericPCATAChannel.h"
#include "AppleGenericPCATARoot.h"
#include "AppleGenericPCATAKeys.h"

#define super IOService
OSDefineMetaClassAndStructors( AppleGenericPCATAChannel, IOService )

//---------------------------------------------------------------------------

bool AppleGenericPCATAChannel::mergeProperties( OSDictionary * properties )
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
// Initialize the ATA channel driver.
//

bool AppleGenericPCATAChannel::init( IOService *       provider,
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

    fProvider = OSDynamicCast(AppleGenericPCATARoot, provider);
    if (fProvider == 0)
        return false;

    // Register channel interrupt.

    UInt32 vector = getInterruptVector();
    if (fProvider->callPlatformFunction( "SetDeviceInterrupts",
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
// Handle open and close from our client.
//

bool AppleGenericPCATAChannel::handleOpen( IOService *  client,
                                           IOOptionBits options,
                                           void *       arg )
{
    bool ret = false;

    if ( fProvider && fProvider->open( this, 0, arg ) )
    {
        ret = super::handleOpen( client, options, arg );
        if ( ret == false )
            fProvider->close( this );
    }
    
    return ret;
}

void AppleGenericPCATAChannel::handleClose( IOService *  client,
                                            IOOptionBits options )
{
    super::handleClose( client, options );
	if ( fProvider ) fProvider->close( this );
}

//---------------------------------------------------------------------------
//
// Accessor functions to assist our client.
//

bool AppleGenericPCATAChannel::getNumberValue( const char * propKey,
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

//---------------------------------------------------------------------------

UInt16 AppleGenericPCATAChannel::getCommandBlockAddress( void ) const
{
    UInt16 value = 0xFFF0;
    getNumberValue( kCommandBlockAddressKey, &value, 16 );
    return value;
}

UInt16 AppleGenericPCATAChannel::getControlBlockAddress( void ) const
{
    UInt16 value = 0xFFF0;
    getNumberValue( kControlBlockAddressKey, &value, 16 );
    return value;
}

UInt32 AppleGenericPCATAChannel::getInterruptVector( void ) const
{
    UInt32 value = 0xFF;
    getNumberValue( kInterruptVectorKey, &value, 32 );
    return value;
}

UInt32 AppleGenericPCATAChannel::getChannelNumber( void ) const
{
    UInt32 value = 0xFF;
    getNumberValue( kChannelNumberKey, &value, 32 );
    return value;
}

UInt32 AppleGenericPCATAChannel::getPIOMode( void ) const
{
    UInt32 value = 0;
    getNumberValue( kPIOModeKey, &value, 32 );
    return value;
}
