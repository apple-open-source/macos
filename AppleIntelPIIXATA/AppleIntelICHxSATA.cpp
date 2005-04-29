/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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

#include<IOKit/storage/IOStorageProtocolCharacteristics.h>
#include "AppleIntelICHxSATA.h"

#define super AppleIntelPIIXPATA
OSDefineMetaClassAndStructors( AppleIntelICHxSATA, AppleIntelPIIXPATA )

//---------------------------------------------------------------------------

bool AppleIntelICHxSATA::start( IOService * provider )
{
    // Override P-ATA reporting in IOATAController::start()
    // for SystemProfiler.

    setProperty( kIOPropertyPhysicalInterconnectTypeKey,
                 kIOPropertyPhysicalInterconnectTypeSerialATA );

    return super::start(provider);
}

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
    int   port = _provider->getSerialATAPortForDrive( driveUnit );
    UInt8 mask = 0;

    switch ( port )
    {
        case kSerialATAPort0:
            mask = kPIIX_PCI_PCS_P0E;
            break;
        case kSerialATAPort1:
            mask = kPIIX_PCI_PCS_P1E;
            break;
        case kSerialATAPort2:
            mask = kPIIX_PCI_PCS_P2E;
            break;
        case kSerialATAPort3:
            mask = kPIIX_PCI_PCS_P3E;
            break;
    }

    _provider->pciConfigWrite8( kPIIX_PCI_PCS,
                                enable ? mask : 0, mask );
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
        case kSerialATAPort2: mask = kPIIX_PCI_PCS_P2P; break;
        case kSerialATAPort3: mask = kPIIX_PCI_PCS_P3P; break;
        default:              mask = 0;
    }

    return (( pcs & mask ) == mask);
}

//---------------------------------------------------------------------------

IOReturn AppleIntelICHxSATA::selectDevice( ataUnitID unit )
{
    // This override is needed when a single device is connected to
    // the second port (virtual slave) of an ICH6 SATA channel. The
    // status register will float (0x7f) following an ATA reset, at
    // boot time or waking from sleep, and also to recover from bus
    // errors and super::selectDevice() will time out while waiting
    // for DRQ to clear. We just accelerate the device selection to
    // the only possible drive unit on the bus. Check _selectedUnit
    // to eliminate unnecessary taskfile accesses.

    if (_selectedUnit    != kATADevice1DeviceID   &&
        _devInfo[0].type == kUnknownATADeviceType &&
        _devInfo[1].type != kUnknownATADeviceType)
    {
        *_tfSDHReg = (1 << 4);  // force device selection to unit 1
    }

    return super::selectDevice( unit );
}

//---------------------------------------------------------------------------

IOReturn AppleIntelICHxSATA::setPowerState( unsigned long stateIndex,
                                            IOService *   whatDevice )
{
    if ( stateIndex == kPIIXPowerStateOff )
    {
        // Record the fact that the driver should initialize the port
        // enable bits when power is raised.

        _initPortEnable = true;
    }
    else if ( _initPortEnable )
    {
        // Power state was OFF, refresh the port enable bits in case
        // the controller lost hardware context.

        for ( UInt32 unit = 0; unit < _provider->getMaxDriveUnits(); unit++ )
        {
            setSATAPortEnable( unit,
                              _devInfo[unit].type != kUnknownATADeviceType );
        }
        _initPortEnable = false;
    }

    return super::setPowerState( stateIndex, whatDevice );
}

#if 0
static void dumpRegsICH6( IOPCIDevice * pci )
{
    kprintf("INT_LN      %x\n", pci->configRead8(0x3c));
    kprintf("INT_PN      %x\n", pci->configRead8(0x3d));
    kprintf("IDE_TIMP    %x\n", pci->configRead16(0x40));
    kprintf("IDE_TIMS    %x\n", pci->configRead16(0x42));
    kprintf("IDE_SIDETIM %x\n", pci->configRead8(0x44));
    kprintf("SDMA_CNT    %x\n", pci->configRead8(0x48));
    kprintf("SDMA_TIM    %x\n", pci->configRead16(0x4a));
    kprintf("IDE_CONFIG  %x\n", pci->configRead32(0x54));
    kprintf("PID         %x\n", pci->configRead16(0x70));
    kprintf("PC          %x\n", pci->configRead16(0x72));
    kprintf("PMCS        %x\n", pci->configRead16(0x74));
    kprintf("MAP         %x\n", pci->configRead8(0x90));
    kprintf("PCS         %x\n", pci->configRead16(0x92));
    kprintf("SIR         %x\n", pci->configRead32(0x94));
    kprintf("ATC         %x\n", pci->configRead8(0xc0));
    kprintf("ATS         %x\n", pci->configRead8(0xc4));
    kprintf("BFCS        %x\n", pci->configRead32(0xe0));

    pci->configWrite32(0xa0, 0);
    kprintf("Index 0x00  %0x\n", pci->configRead32(0xa4));
    pci->configWrite32(0xa0, 0x18);
    kprintf("Index 0x18  %0x\n", pci->configRead32(0xa4));
    pci->configWrite32(0xa0, 0x1c);
    kprintf("Index 0x1c  %0x\n", pci->configRead32(0xa4));
    pci->configWrite32(0xa0, 0x28);
    kprintf("Index 0x28  %0x\n", pci->configRead32(0xa4));
    pci->configWrite32(0xa0, 0x54);
    kprintf("Index 0x54  %0x\n", pci->configRead32(0xa4));
    pci->configWrite32(0xa0, 0x64);
    kprintf("Index 0x64  %0x\n", pci->configRead32(0xa4));
    pci->configWrite32(0xa0, 0x74);
    kprintf("Index 0x74  %0x\n", pci->configRead32(0xa4));
    pci->configWrite32(0xa0, 0x84);
    kprintf("Index 0x84  %0x\n", pci->configRead32(0xa4));
}
#endif
