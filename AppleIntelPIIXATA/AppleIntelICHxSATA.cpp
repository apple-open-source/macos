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

#include "AppleIntelICHxSATA.h"

#define super AppleIntelPIIXPATA
OSDefineMetaClassAndStructors( AppleIntelICHxSATA, AppleIntelPIIXPATA )

//---------------------------------------------------------------------------

IOReturn AppleIntelICHxSATA::provideBusInfo( IOATABusInfo * infoOut )
{
    if ( super::provideBusInfo( infoOut ) != kATANoErr )
    {
        return -1;
    }

    // Override the socket type reported by the PATA driver

    infoOut->setSocketType( kInternalSATA );

    return kATANoErr;
}

//---------------------------------------------------------------------------

UInt32 AppleIntelICHxSATA::scanForDrives( void )
{
    UInt32 unitsFound;

    // Try real hard to reset the port(s) and attached devices.

	for ( int loopMs = 0; loopMs <= 3000; loopMs += 10 )
	{
        if ( (loopMs % 1000) == 0 )
        {
            for ( UInt32 i = 0; i < _provider->getMaxDriveUnits(); i++ )
                setSATAPortEnable( i, false );
        
            IOSleep( 20 );
        
            for ( UInt32 i = 0; i < _provider->getMaxDriveUnits(); i++ )
                setSATAPortEnable( i, true );
    
            IOSleep( 20 );

            *_tfAltSDevCReg = mATADCRReset;  // ATA reset
    
            IODelay( 100 );
    
            *_tfAltSDevCReg = 0x0;            
        }

		if ( (*_tfStatusCmdReg & mATABusy) == 0x00 )
			break;

		IOSleep( 10 );
	}

    // ICH5 does offer a device present flag for each SATA port. This
    // information can be used to speed up boot by reducing unnecessary
    // bus scanning when no devices are present. In addition, the port
    // can be disabled to reduce power usage. For now we still use the
    // standard bus scanning implementation in IOATAController.

	unitsFound = IOPCIATA::scanForDrives();

    // Fixup discrepancies between the results from ATA bus scanning,
    // and the SATA device present status.

    for ( UInt32 unit = 0; unit < kMaxDrives; unit++ )
    {
        if ( _devInfo[unit].type != kUnknownATADeviceType &&
             ( unit >= _provider->getMaxDriveUnits() ||
               getSATAPortPresentStatus( unit ) == false ) )
        {
            // Detected a device, but SATA reports that no device are
            // present on the port. Trust SATA since if the device was
            // detected then surely the port present bit would be set.

            _devInfo[unit].type = kUnknownATADeviceType;
        }
    }

    // Turn off unused SATA ports.
    
    for ( UInt32 unit = 0; unit < _provider->getMaxDriveUnits(); unit++ )
    {
        if ( _devInfo[unit].type == kUnknownATADeviceType )
        {
            setSATAPortEnable( unit, false );
        }
    }

    return unitsFound;
}

//---------------------------------------------------------------------------

void AppleIntelICHxSATA::setSATAPortEnable( UInt32 driveUnit, bool enable )
{
    int port = _provider->getSerialATAPortForDrive( driveUnit );

    switch ( port )
    {
        case kSerialATAPort0:
            _provider->pciConfigWrite8( kPIIX_PCI_PCS,
                                        enable ? kPIIX_PCI_PCS_P0E : 0,
                                        kPIIX_PCI_PCS_P0E );
            break;
        
        case kSerialATAPort1:
            _provider->pciConfigWrite8( kPIIX_PCI_PCS,
                                        enable ? kPIIX_PCI_PCS_P1E : 0,
                                        kPIIX_PCI_PCS_P1E );
            break;
    }
}

//---------------------------------------------------------------------------

bool AppleIntelICHxSATA::getSATAPortPresentStatus( UInt32 driveUnit )
{
    int   port = _provider->getSerialATAPortForDrive( driveUnit );
    UInt8 pcs;
    UInt8 mask;

    pcs = _pciDevice->configRead8( kPIIX_PCI_PCS );

    switch ( port )
    {
        case kSerialATAPort0: mask = kPIIX_PCI_PCS_P0P; break;
        case kSerialATAPort1: mask = kPIIX_PCI_PCS_P1P; break;
        default:              mask = 0;
    }

    return (( pcs & mask ) == mask);
}
