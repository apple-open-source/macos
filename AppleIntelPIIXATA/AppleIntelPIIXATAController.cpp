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

#include "AppleIntelPIIXATAController.h"
#include "AppleIntelPIIXMultiATADriver.h"
#include "AppleIntelPIIXATAKeys.h"

#define super IOService
OSDefineMetaClassAndStructors( AppleIntelPIIXATAController, IOService )

#define CastOSNumber(x) OSDynamicCast( OSNumber, x )
#define CastOSString(x) OSDynamicCast( OSString, x )

static bool
getOSNumberValue( const OSDictionary * dict,
                  const char         * key,
                  UInt16             * outValue )
{
    OSNumber * num = CastOSNumber( dict->getObject( key ) );
    if ( num )
    {
        *outValue = num->unsigned16BitValue();
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------
//
// Create the interrupt properties.
//

bool
AppleIntelPIIXATAController::setupInterrupt( UInt32 line )
{
	OSArray *         controller = 0;
	OSArray *         specifier  = 0;
	OSData *          tmpData    = 0;
    bool              ret        = false;
	extern OSSymbol * gIntelPICName;

	do {
		// Create the interrupt specifer array.
        // This specifies the interrupt line.

		specifier = OSArray::withCapacity(1);
		if ( !specifier ) break;

        tmpData = OSData::withBytes( &line, sizeof(line) );
        if ( !tmpData ) break;

        specifier->setObject( tmpData );

        // Next the interrupt controller array.

        controller = OSArray::withCapacity(1);
        if ( !controller ) break;

        controller->setObject( gIntelPICName );

        // Put the two arrays into our property table.
        
        ret = setProperty( gIOInterruptControllersKey, controller )
           && setProperty( gIOInterruptSpecifiersKey,  specifier );
    }
    while( false );
    
    if ( controller ) controller->release();
    if ( specifier  ) specifier->release();
    if ( tmpData    ) tmpData->release();
    
    return ret;
}

//---------------------------------------------------------------------------
//
// Initialize the ATA controller object.
//

bool
AppleIntelPIIXATAController::init( IOService *    provider,
                                   OSDictionary * dictionary )
{
    if ( super::init(dictionary) == false )
        return false;

    _provider = provider;

    // Fetch the port address and interrupt line properties from
    // the dictionary provided.

	if ( !getOSNumberValue( dictionary, kPortAddressKey, &_ioPorts )
      || !getOSNumberValue( dictionary, kInterruptLineKey, &_irq )
      || !getOSNumberValue( dictionary, kChannelNumberKey, &_channel )
       ) return false;

    if ( provider )
    {
        OSNumber *  num;
        OSString *  str;

        num = CastOSNumber( provider->getProperty( kPIOModesKey ) );
        if ( num )
            _pioModes = num->unsigned8BitValue();

        num = CastOSNumber( provider->getProperty( kDMAModesKey ) );
        if ( num )
            _dmaModes = num->unsigned8BitValue();

        num = CastOSNumber( provider->getProperty( kUltraDMAModesKey ) );
        if ( num )
            _udmaModes = num->unsigned8BitValue();

        str = CastOSString( provider->getProperty( kDeviceNameKey ) );
        if ( str )
            _deviceName = str->getCStringNoCopy();

        _perChannelTimings = true;
        if ( provider->getProperty(kPerChannelTimingsKey) == kOSBooleanFalse )
            _perChannelTimings = false;
    }

    if ( !setupInterrupt( _irq ) )
        return false;

    return true;
}

//---------------------------------------------------------------------------
//
// Handle open and close from our client.
//

bool
AppleIntelPIIXATAController::handleOpen( IOService *  client,
                                         IOOptionBits options,
                                         void *       arg )
{
    bool ret = false;

    if ( _provider && _provider->open( this, options, arg ) )
    {
        ret = super::handleOpen( client, options, arg );
        if ( ret == false )
            _provider->close( this );
    }
    
    return ret;
}

void
AppleIntelPIIXATAController::handleClose( IOService *  client,
                                          IOOptionBits options )
{
    super::handleClose( client, options );
	if ( _provider ) _provider->close( this );
}

//---------------------------------------------------------------------------
//
// Accessor functions to assist our client.
//

UInt16
AppleIntelPIIXATAController::getIOBaseAddress() const
{
    return _ioPorts;
}

UInt16
AppleIntelPIIXATAController::getInterruptLine() const
{
    return _irq;
}

UInt16
AppleIntelPIIXATAController::getChannelNumber() const
{
    return _channel;
}

void
AppleIntelPIIXATAController::pciConfigWrite8( UInt8 offset,
                                              UInt8 data,
                                              UInt8 mask )
{
    if ( _provider )
         ((AppleIntelPIIXMultiATADriver *) _provider)->pciConfigWrite8(
            offset, data, mask );
}

void
AppleIntelPIIXATAController::pciConfigWrite16( UInt8  offset,
                                               UInt16 data,
                                               UInt16 mask )
{
    if ( _provider )
         ((AppleIntelPIIXMultiATADriver *) _provider)->pciConfigWrite16(
            offset, data, mask );
}

UInt8
AppleIntelPIIXATAController::getPIOModes() const
{
	return _pioModes;
}

UInt8
AppleIntelPIIXATAController::getDMAModes() const
{
	return _dmaModes;
}

UInt8
AppleIntelPIIXATAController::getUltraDMAModes() const
{
	return _udmaModes;
}

const char *
AppleIntelPIIXATAController::getDeviceName() const
{
	return _deviceName ? _deviceName : "Unknown Device";
}

bool
AppleIntelPIIXATAController::hasPerChannelTimingSupport() const
{
    return _perChannelTimings;
}
