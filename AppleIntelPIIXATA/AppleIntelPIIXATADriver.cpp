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

#include <sys/systm.h>  // snprintf

#include <IOKit/assert.h>
#include <IOKit/IOMessage.h>
#include <IOKit/ata/IOATATypes.h>
#include <IOKit/ata/IOATAController.h>
#include <IOKit/ata/ATADeviceNub.h>
#include "AppleIntelPIIXATAController.h"
#include "AppleIntelPIIXATADriver.h"
#include "AppleIntelPIIXATAHW.h"
#include "AppleIntelPIIXATAKeys.h"

#define super IOPCIATA
OSDefineMetaClassAndStructors( AppleIntelPIIXATADriver, IOPCIATA )

#ifdef  PIIX_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

// Controller supported modes.
//
#define PIOModes   \
    (_provider->getPIOModes() & ((1 << piixPIOTimingCount) - 1))

#define DMAModes   \
    (_provider->getDMAModes() & ((1 << piixDMATimingCount) - 1))

#define UDMAModes  \
    (_provider->getUltraDMAModes() & ((1 << piixUDMATimingCount) - 1))

//---------------------------------------------------------------------------
//
// Start the single-channel PIIX ATA controller driver.
//

bool 
AppleIntelPIIXATADriver::start( IOService * provider )
{
    DLOG("%s::%s( %p )\n", getName(), __FUNCTION__, provider);

    // Our provider is a 'nub' that represents a single channel
    // PIIX ATA controller. Note that it is not an IOPCIDevice.

	_provider = OSDynamicCast( AppleIntelPIIXATAController, provider );
    if ( _provider == 0 )
        goto fail;

    // Retain and open our provider. The IOPCIDevice object will be
    // returned by the provider if the open was successful.

    _provider->retain();

    if ( ( _provider->open( this, 0, &_pciDevice ) != true ) ||
         ( _pciDevice == 0 ) )
    {
        IOLog("%s: provider open failed\n", getName());
        goto fail;
    }

    // Cache controller properties, and validate them.

    _ioPorts = _provider->getIOBaseAddress();
    _channel = _provider->getChannelNumber();

    if ( ( _ioPorts != kPIIX_P_CMD_ADDR ) && ( _ioPorts != kPIIX_S_CMD_ADDR ) )
    {
        IOLog("%s: invalid ATA port address 0x%x\n", getName(), _ioPorts);
        goto fail;
    }
    if ( _channel > kPIIX_CHANNEL_SECONDARY )
    {
        IOLog("%s: invalid ATA channel number %d\n", getName(), _channel);
        goto fail;
    }

    // Configure the PIIX device.

    if ( configurePCIDevice( _pciDevice, _channel ) != true )
    {
        // IOLog("%s: PIIX PCI configuration failed\n", getName());
        goto fail;
    }

    // Get the base address for the bus master registers in I/O space.

    if ( getBMBaseAddress( _pciDevice, _channel, &_ioBMOffset ) != true )
    {
        IOLog("%s: get bus-master base address failed\n", getName());
        goto fail;
    }

    // Must setup these variables inherited from IOPCIATA before
    // calling its start().

    _bmCommandReg   = IOATAIOReg8::withAddress( _ioBMOffset + kPIIX_IO_BMICX );
    _bmStatusReg    = IOATAIOReg8::withAddress( _ioBMOffset + kPIIX_IO_BMISX );
    _bmPRDAddresReg = IOATAIOReg32::withAddress( _ioBMOffset + kPIIX_IO_BMIDTPX );

    // Reset controller timings for both drives.

    resetTimingsForDevice( kATADevice0DeviceID );
    resetTimingsForDevice( kATADevice1DeviceID );

    // Call super after setting _ioPorts. This is because our
    // configureTFPointers() function will be called by super.

    if ( super::start(_provider) == false )
    {
        IOLog("%s: super start failed\n", getName());
        goto fail;
    }

    // This driver will handle interrupts using a work loop.
    // Create interrupt event source that will signal the
    // work loop (thread) when a device interrupt occurs.
    
    _intSrc = IOInterruptEventSource::interruptEventSource(
              (OSObject *)             this,
	          (IOInterruptEventAction) &interruptOccurred,
                                       _provider, 0 );

    if ( !_intSrc || !_workLoop ||
         (_workLoop->addEventSource(_intSrc) != kIOReturnSuccess) )
    {
        IOLog("%s: interrupt event source error\n", getName());
        goto fail;
    }
    _intSrc->enable();

    // For each device discovered on the ATA bus (by super),
    // create a nub for that device and call registerService() to
    // trigger matching against that device.

    for ( UInt32 i = 0; i < kMaxDrives; i++ )
    {
        if ( _devInfo[i].type != kUnknownATADeviceType )
        {
            ATADeviceNub * nub;

			nub = ATADeviceNub::ataDeviceNub( (IOATAController*) this,
                                              (ataUnitID) i,
                                              _devInfo[i].type );

            if ( nub )
            {
                if ( nub->attach( this ) )
                {
                    _nub[i] = (IOATADevice *) nub;
                    _nub[i]->retain();
                    _nub[i]->registerService();
                }
                nub->release();
            }
		}
	}

    // Successful start, announce our vital properties.

    IOLog("%s: %s (Port 0x%x, IRQ %d, BM 0x%x)\n", getName(),
          _provider->getDeviceName(),
          _ioPorts, _provider->getInterruptLine(), _ioBMOffset);

    return true;

fail:
    
    if ( _provider )
    {
        _provider->close( this );
    }
    
    return false;
}

/*---------------------------------------------------------------------------
 *
 * Release resources before this object is destroyed.
 *
 ---------------------------------------------------------------------------*/

void
AppleIntelPIIXATADriver::free()
{
#define RELEASE(x) do { if(x) { (x)->release(); (x) = 0; } } while(0)

    DLOG("%s::%s()\n", getName(), __FUNCTION__);

    // Release resources created by start().

    RELEASE( _intSrc         );
    RELEASE( _nub[0]         );
    RELEASE( _nub[1]         );
    RELEASE( _provider       );
    RELEASE( _bmCommandReg   );
    RELEASE( _bmStatusReg    );
    RELEASE( _bmPRDAddresReg );

    // Release registers created by configureTFPointers().

    RELEASE( _tfDataReg      );
    RELEASE( _tfFeatureReg   );
    RELEASE( _tfSCountReg    );
    RELEASE( _tfSectorNReg   );
    RELEASE( _tfCylLoReg     );
    RELEASE( _tfCylHiReg     );
    RELEASE( _tfSDHReg       );
    RELEASE( _tfStatusCmdReg );
    RELEASE( _tfAltSDevCReg  );

    // IOATAController should release this.

	if ( _doubleBuffer.logicalBuffer )
	{
		IOFree( (void *) _doubleBuffer.logicalBuffer,
                         _doubleBuffer.bufferSize );
		_doubleBuffer.bufferSize     = 0;
		_doubleBuffer.logicalBuffer  = 0;
		_doubleBuffer.physicalBuffer = 0;
	}

    // What about _workloop, _cmdGate, and _timer in the superclass?

    super::free();
}

/*---------------------------------------------------------------------------
 *
 * Return a new work loop object, or the one (we) previously created.
 *
 ---------------------------------------------------------------------------*/

IOWorkLoop *
AppleIntelPIIXATADriver::getWorkLoop() const
{
    DLOG("%s::%s()\n", getName(), __FUNCTION__);

    return ( _workLoop ) ? _workLoop :
                           IOWorkLoop::workLoop();
}

/*---------------------------------------------------------------------------
 *
 * Configure the PIIX PCI device.
 *
 ---------------------------------------------------------------------------*/

bool
AppleIntelPIIXATADriver::configurePCIDevice( IOPCIDevice * device,
                                             UInt16        channel )
{
    UInt32 reg;

    DLOG("%s::%s( %p, %d )\n", getName(), __FUNCTION__,
         device, channel);

	// Fetch the corresponding primary/secondary IDETIM register and
	// check the individual channel enable bit. We assume that the
    // master IOSE bit was already checked by our provider.

	reg = device->configRead32( kPIIX_PCI_IDETIM );

	if ( channel == kPIIX_CHANNEL_SECONDARY )
		reg >>= 16;		// kPIIX_PCI_IDETIM + 2 for secondary channel

	if ( (reg & kPIIX_PCI_IDETIM_IDE) == 0 )
    {
		IOLog("%s: %s PCI IDE channel is disabled\n", getName(),
			  (channel == kPIIX_CHANNEL_PRIMARY) ? "Primary" : "Secondary");
		return false;
	}

	// Enable bus-master. The previous state of the bit is returned
    // but ignored.

	device->setBusMasterEnable( true );

    // Read the IDE config register containing the Ultra DMA clock control,
    // and 80-conductor cable reporting bits.

    _ideConfig = device->configRead16( kPIIX_PCI_IDECONFIG );
    DLOG("%s: IDE_CONFIG = %04x\n", getName(), _ideConfig);

    return true;
}

/*---------------------------------------------------------------------------
 *
 * Determine the start of the I/O mapped Bus-Master registers.
 * This range is defined by PCI config space register kPIIX_PCI_BMIBA.
 *
 ---------------------------------------------------------------------------*/

bool AppleIntelPIIXATADriver::getBMBaseAddress( IOPCIDevice * provider,
                                                UInt16        channel,
                                                UInt16 *      addrOut )
{
	UInt32 bmiba;

    DLOG("%s::%s( %p, %d, %p )\n", getName(), __FUNCTION__,
         provider, channel, addrOut);

	bmiba = provider->configRead32( kPIIX_PCI_BMIBA );

	if ( (bmiba & kPIIX_PCI_BMIBA_RTE) == 0 )
    {
        IOLog("%s: PCI memory range 0x%02x (0x%08lx) is not an I/O range\n",
              getName(), kPIIX_PCI_BMIBA, bmiba);
		return false;
	}

	bmiba &= kPIIX_PCI_BMIBA_MASK;	// get the address portion

	// If bmiba is zero, it is likely that the user has elected to
	// turn off PCI IDE support in the BIOS.

	if ( bmiba == 0 )
		return false;

	if ( channel == kPIIX_CHANNEL_SECONDARY )
		bmiba += kPIIX_IO_BM_OFFSET;

	*addrOut = (UInt16) bmiba;

    DLOG("%s::%s ioBMOffset = %04x\n", getName(), __FUNCTION__, *addrOut);

	return true;
}

/*---------------------------------------------------------------------------
 *
 * Reset all timing registers to the slowest (most compatible) timing.
 * UDMA modes are disabled.
 *
 ---------------------------------------------------------------------------*/

void
AppleIntelPIIXATADriver::resetTimingsForDevice( ataUnitID unit )
{
    _pioTiming[unit]  = &piixPIOTiming[ 0 ];  // PIO Mode 0
    _dmaTiming[unit]  = 0;
    _udmaTiming[unit] = 0;

    // Compute the timing register values.

    computeTimingRegisters( unit );
    computeUDMATimingRegisters( unit );

    // Write the timing values to hardware.

    writeTimingRegisters();
}

/*---------------------------------------------------------------------------
 *
 * Setup the location of the task file registers.
 *
 ---------------------------------------------------------------------------*/

bool 
AppleIntelPIIXATADriver::configureTFPointers()
{
    DLOG("%s::%s()\n", getName(), __FUNCTION__);

	_tfDataReg      = IOATAIOReg16::withAddress( _ioPorts + 0 );
	_tfFeatureReg   = IOATAIOReg8::withAddress(  _ioPorts + 1 );
	_tfSCountReg    = IOATAIOReg8::withAddress(  _ioPorts + 2 );
	_tfSectorNReg   = IOATAIOReg8::withAddress(  _ioPorts + 3 );
	_tfCylLoReg     = IOATAIOReg8::withAddress(  _ioPorts + 4 );
	_tfCylHiReg     = IOATAIOReg8::withAddress(  _ioPorts + 5 );
	_tfSDHReg       = IOATAIOReg8::withAddress(  _ioPorts + 6 );
	_tfStatusCmdReg = IOATAIOReg8::withAddress(  _ioPorts + 7 );
	_tfAltSDevCReg  = IOATAIOReg8::withAddress(  _ioPorts + 0x206 );

    if ( !_tfDataReg || !_tfFeatureReg || !_tfSCountReg ||
         !_tfSectorNReg || !_tfCylLoReg || !_tfCylHiReg ||
         !_tfSDHReg || !_tfStatusCmdReg || !_tfAltSDevCReg )
    {
        return false;
    }

	return true;
}

/*---------------------------------------------------------------------------
 *
 * The work loop based interrupt handler called by our interrupt event
 * source.
 *
 ---------------------------------------------------------------------------*/

void
AppleIntelPIIXATADriver::interruptOccurred( OSObject *               owner,
                                            IOInterruptEventSource * src,
                                            int                      count )
{
	AppleIntelPIIXATADriver * self = (AppleIntelPIIXATADriver *) owner;

    // Let our superclass handle the interrupt to advance to the next state
    // in its internal state machine.
    
	self->handleDeviceInterrupt();
}

/*---------------------------------------------------------------------------
 *
 * Extend the implementation of scanForDrives() from IOATAController
 * to issue a soft reset before scanning for ATA/ATAPI drive signatures.
 *
 ---------------------------------------------------------------------------*/

UInt32 
AppleIntelPIIXATADriver::scanForDrives()
{
    UInt32 unitsFound;

    DLOG("%s::%s()\n", getName(), __FUNCTION__);

    *_tfAltSDevCReg = mATADCRReset;

    IODelay( 100 );

    *_tfAltSDevCReg = 0x0;

    IOSleep( 10 );

    unitsFound = super::scanForDrives();
    
    // FIXME: Hack for Darwin/x86 on VPC compatibility.
    // VPC 4.0 will set the error bit (bit 0) in the status register
    // following an assertion of SRST. The scanForDrives() code in
    // IOATAController will see that this bit is set, and will not
    // recognize the ATA device. The following code will re-scan the
    // bus and ignore the error bit.

    for ( int unit = 0; ((unit < 2) && (unitsFound < 2)); unit++ )
    {
        if ( _devInfo[unit].type == kUnknownATADeviceType )
        {
            UInt32 milsSpent;

            // Select unit and wait for a not busy bus.

            for ( milsSpent = 0; milsSpent < 10000; )
            {
                *_tfSDHReg	= ( unit << 4 );
                IODelay( 10 );

                if ( (*_tfStatusCmdReg & mATABusy) == 0x00 ) break;

                IOSleep( 10 );
                milsSpent += 10;
            }
            if ( milsSpent >= 10000 ) break;

            // Ignore the error bit in the status register, and check
            // for a ATA device signature.

			if ( (*_tfCylLoReg == 0x00) && (*_tfCylHiReg == 0x00) &&
                 (*_tfSCountReg == 0x01) && (*_tfSectorNReg == 0x01) &&
                 ( (*_tfAltSDevCReg & 0x50) == 0x50) )
            {
                _devInfo[unit].type = kATADeviceType;
                _devInfo[unit].packetSend = kATAPIUnknown;
                unitsFound++;
            }
        }
    }

	*_tfSDHReg = 0x00; 	// Initialize device selection to device 0.

    return unitsFound;
}

/*---------------------------------------------------------------------------
 *
 * Provide information on the bus capability.
 *
 ---------------------------------------------------------------------------*/

IOReturn
AppleIntelPIIXATADriver::provideBusInfo( IOATABusInfo * infoOut )
{
    DLOG("%s::%s( %p )\n", getName(), __FUNCTION__, infoOut);

	if ( infoOut == 0 )
	{
		IOLog("%s::%s bad argument\n", getName(), __FUNCTION__);
		return -1;
	}

    infoOut->zeroData();
    infoOut->setSocketType( kInternalATASocket );

	infoOut->setPIOModes( PIOModes );
	infoOut->setDMAModes( DMAModes );
	infoOut->setUltraModes( UDMAModes );

	UInt8 units = 0;
	if ( _devInfo[0].type != kUnknownATADeviceType ) units++;
	if ( _devInfo[1].type != kUnknownATADeviceType ) units++;

	infoOut->setUnits( units );

	return kATANoErr;
}

/*---------------------------------------------------------------------------
 *
 * Return the currently configured timing for the drive unit.
 *
 ---------------------------------------------------------------------------*/

IOReturn 
AppleIntelPIIXATADriver::getConfig( IOATADevConfig * configOut,
                                    UInt32           unit )
{
    DLOG("%s::%s( %p, %ld )\n", getName(), __FUNCTION__,
         configOut, unit);

	if ( (configOut == 0) || (unit > kATADevice1DeviceID) )
	{
		IOLog("%s::%s bad argument\n", getName(), __FUNCTION__);
		return -1;	
	}

    configOut->setPIOMode( 0 );
    configOut->setDMAMode( 0 );
    configOut->setUltraMode( 0 );

    if ( _pioTiming[unit] )
    {
        configOut->setPIOMode( 1 << _pioTiming[unit]->mode );
        configOut->setPIOCycleTime( _pioTiming[unit]->cycleTime );
    }

    if ( _dmaTiming[unit] )
    {
        configOut->setDMAMode( 1 << _dmaTiming[unit]->mode );
        configOut->setDMACycleTime( _dmaTiming[unit]->cycleTime );
    }

    if ( _udmaTiming[unit] )
    {
        configOut->setUltraMode( 1 << _udmaTiming[unit]->mode );
    }

	configOut->setPacketConfig( _devInfo[unit].packetSend );

    return kATANoErr;
}

/*---------------------------------------------------------------------------
 *
 * Select the bus timings for a given drive.
 *
 ---------------------------------------------------------------------------*/

IOReturn 
AppleIntelPIIXATADriver::selectConfig( IOATADevConfig * config,
                                       UInt32           unit )
{
    const PIIXTiming *     pioTiming  = 0;
    const PIIXTiming *     dmaTiming  = 0;
    const PIIXUDMATiming * udmaTiming = 0;

    DLOG("%s::%s( %p, %ld )\n", getName(), __FUNCTION__,
         config, unit);

	if ( (config == 0) || (unit > kATADevice1DeviceID) )
	{
		IOLog("%s::%s bad argument\n", getName(), __FUNCTION__);
		return -1;
	}

    removeProperty( kSelectedPIOModeKey );
    removeProperty( kSelectedDMAModeKey );
    removeProperty( kSelectedUltraDMAModeKey );

    // Verify that the selected PIO mode is supported.

    if ( ( config->getPIOMode() & PIOModes ) == 0 )
    {
        DLOG("%s: Unsupported PIO mode %x\n", getName(), config->getPIOMode());
        pioTiming = &piixPIOTiming[0];
    }
    else
    {
        UInt8  pioModeNumber;

        // Convert a bit-significant indicators to a numeric values.

        pioModeNumber = bitSigToNumeric( config->getPIOMode() );
        DLOG("%s: pioModeNumber = %d\n", getName(), pioModeNumber);

        // Even though the drive supports the given PIO mode, it is
        // necessary to check the cycle time reported by the drive,
        // and downgrade the controller timings if the drive cannot
        // support the cycle time for the target PIO mode.

        for ( ; pioModeNumber > 0; pioModeNumber-- )
        {
            // If the mode is supported by the controller, and the
            // drive supported cycle time is less than or equal to
            // the mode's cycle time, then select the mode.

            if ( ( ( (1 << pioModeNumber) & PIOModes ) ) &&
                 ( config->getPIOCycleTime() <=
                   piixPIOTiming[ pioModeNumber ].cycleTime ) )
            {
                break;
            }

            DLOG("%s: pioModeNumber = %d\n", getName(), pioModeNumber - 1);
        }

        setDriveProperty( unit, kSelectedPIOModeKey, pioModeNumber, 8 );
        pioTiming = &piixPIOTiming[ pioModeNumber ];
    }

    // Look at the selected Multi-Word DMA mode.

    if ( config->getDMAMode() )
	{
        UInt8  dmaModeNumber;

        // Is the selected DMA mode supported?

        if ( ( config->getDMAMode() & DMAModes ) == 0 )
        {
            DLOG("%s: Unsupported DMA mode\n", getName());
            return kATAModeNotSupported;
        }

        dmaModeNumber = bitSigToNumeric( config->getDMAMode() );
        DLOG("%s: dmaModeNumber = %d\n", getName(), dmaModeNumber);

        // Even though the drive supports the given DMA mode, it is
        // necessary to check the cycle time reported by the drive,
        // and downgrade the controller timings if the drive cannot
        // support the cycle time for the target DMA mode.

        for ( ; dmaModeNumber > 0; dmaModeNumber-- )
        {
            // If the mode is supported by the controller, and the
            // drive supported cycle time is less than or equal to
            // the mode's cycle time, then select the mode.

            if ( ( ( (1 << dmaModeNumber) & DMAModes ) ) &&
                 ( config->getDMACycleTime() <=
                   piixDMATiming[ dmaModeNumber ].cycleTime ) )
            {
                break;
            }

            DLOG("%s: dmaModeNumber = %d\n", getName(), dmaModeNumber - 1);
        }

        if ( (1 << dmaModeNumber) & DMAModes )
        {
            setDriveProperty( unit, kSelectedDMAModeKey, dmaModeNumber, 8 );
            dmaTiming = &piixDMATiming[ dmaModeNumber ];
        }
        else
            dmaTiming = 0;  // No Multi-Word DMA mode selected.
    }

    // Look at the selected U-DMA mode.

    if ( config->getUltraMode() )
    {
        UInt8 udmaModeNumber;
    
        // Is the selected Ultra-DMA mode supported?

        if ( ( config->getUltraMode() & UDMAModes ) == 0 )
        {
            DLOG("%s: Unsupported U-DMA mode\n", getName());
            return kATAModeNotSupported;
        }

        udmaModeNumber = bitSigToNumeric( config->getUltraMode() );

        // For Ultra DMA mode 3 or higher, a 80-conductor cable must
        // be present. Otherwise, the drive will be limited to mode 2.

        if ( udmaModeNumber > 2 )
        {
            UInt16 cableMask = kPIIX_PCI_IDECONFIG_PCR0;
            if ( unit == kATADevice1DeviceID )         cableMask <<= 1;
            if ( _channel == kPIIX_CHANNEL_SECONDARY ) cableMask <<= 2;

            if ( ( cableMask & _ideConfig ) == 0 )
            {
                DLOG("%s: 80-conductor cable not detected\n", getName());
                udmaModeNumber = 2;   // limited to mode 2
            }
        }

        setDriveProperty( unit, kSelectedUltraDMAModeKey, udmaModeNumber, 8 );
        DLOG("%s: udmaModeNumber = %d\n", getName(), udmaModeNumber);
        udmaTiming = &piixUDMATiming[ udmaModeNumber ];
    }

    // If DMA and PIO require different timings, we use fast
    // timing for DMA only, and revert to compatible timing
    // for PIO data register access.

    if ( dmaTiming && (pioTiming->cycleTime != dmaTiming->cycleTime) )
    {
        DLOG("%s: forcing PIO compatible timing\n", getName());
        pioTiming = &piixPIOTiming[0];
    }

    // Cache the selected timings.

    _pioTiming[unit]  = pioTiming;
    _dmaTiming[unit]  = dmaTiming;
    _udmaTiming[unit] = udmaTiming;

    // Compute the timing register values.

    computeTimingRegisters( (ataUnitID) unit );
    computeUDMATimingRegisters( (ataUnitID) unit );

    // Write the timing values to hardware.

    writeTimingRegisters();

	_devInfo[unit].packetSend = config->getPacketConfig();

	return getConfig( config, unit );
}

/*---------------------------------------------------------------------------
 *
 * Write the timing values to the PIIX timing registers.
 *
 ---------------------------------------------------------------------------*/

void
AppleIntelPIIXATADriver::writeTimingRegisters( ataUnitID unit )
{
    UInt8 idetimOffset;
    UInt8  sidetimMask;
    UInt8  udmactlMask;
    UInt16 udmatimMask;
    UInt16 ideConfigMask;

    DLOG("%s::%s()\n", getName(), __FUNCTION__);

    if ( _channel == kPIIX_CHANNEL_PRIMARY )
    {
        idetimOffset = kPIIX_PCI_IDETIM;
    
        sidetimMask  = kPIIX_PCI_SIDETIM_PRTC1_MASK |
                       kPIIX_PCI_SIDETIM_PISP1_MASK;
        
        udmactlMask  = kPIIX_PCI_UDMACTL_PSDE0 |
                       kPIIX_PCI_UDMACTL_PSDE1;
        
        udmatimMask  = kPIIX_PCI_UDMATIM_PCT0_MASK |
                       kPIIX_PCI_UDMATIM_PCT1_MASK;
    
        ideConfigMask = kPIIX_PCI_IDECONFIG_PCB0      |
                        kPIIX_PCI_IDECONFIG_PCB1      |
                        kPIIX_PCI_IDECONFIG_FAST_PCB0 |
                        kPIIX_PCI_IDECONFIG_FAST_PCB1 |
                        kPIIX_PCI_IDECONFIG_WR_PP_EN;
    }
    else
    {
        idetimOffset = kPIIX_PCI_IDETIM_S;

        sidetimMask  = kPIIX_PCI_SIDETIM_SRTC1_MASK |
                       kPIIX_PCI_SIDETIM_SISP1_MASK;

        udmactlMask  = kPIIX_PCI_UDMACTL_SSDE0 |
                       kPIIX_PCI_UDMACTL_SSDE1;

        udmatimMask  = kPIIX_PCI_UDMATIM_SCT0_MASK |
                       kPIIX_PCI_UDMATIM_SCT1_MASK;

        ideConfigMask = kPIIX_PCI_IDECONFIG_SCB0      |
                        kPIIX_PCI_IDECONFIG_SCB1      |
                        kPIIX_PCI_IDECONFIG_FAST_SCB0 |
                        kPIIX_PCI_IDECONFIG_FAST_SCB1 |
                        kPIIX_PCI_IDECONFIG_WR_PP_EN;
    }

    // Timing registers are shared between primary and secondary ATA
    // channels. Call the PCI config space write functions in our
    // provider to serialize changes in those timing registers.

    if ( _provider->hasPerChannelTimingSupport() == false )
    {
        _provider->pciConfigWrite16( idetimOffset, _idetim[unit] );
        
        DLOG("%s: IDETIM[%d] = %04x\n", getName(), unit, _idetim[unit]);
    }
    else
    {
        _provider->pciConfigWrite16( idetimOffset, _idetim[0] | _idetim[1]);

        _provider->pciConfigWrite8( kPIIX_PCI_SIDETIM, _sidetim, sidetimMask );

        _provider->pciConfigWrite8( kPIIX_PCI_UDMACTL, _udmactl, udmactlMask );

        _provider->pciConfigWrite16( kPIIX_PCI_UDMATIM, _udmatim, udmatimMask );

        _provider->pciConfigWrite16( kPIIX_PCI_IDECONFIG,
                                     _ideConfig, ideConfigMask );

        DLOG("%s: IDETIM = %04x SIDETIM = %02x UDMACTL = %02x UDMATIM = %04x\n",
             getName(),
             _idetim[0] | _idetim[1], _sidetim, _udmactl, _udmatim);
    }

#if 0
    IOLog("%s: CH %d PCI 0x40: %08lx\n", getName(),
          _channel, _pciDevice->configRead32( 0x40 ));
    IOLog("%s: CH %d PCI 0x44: %08lx\n", getName(),
          _channel, _pciDevice->configRead32( 0x44 ));
    IOLog("%s: CH %d PCI 0x48: %08lx\n", getName(),
          _channel, _pciDevice->configRead32( 0x48 ));
#endif
}

/*---------------------------------------------------------------------------
 *
 * Compute the U-DMA timing register values.
 *
 ---------------------------------------------------------------------------*/

void
AppleIntelPIIXATADriver::computeUDMATimingRegisters( ataUnitID unit )
{
    UInt8  udmaEnableBit   = kPIIX_PCI_UDMACTL_PSDE0;
    UInt8  udmatimShifts   = kPIIX_PCI_UDMATIM_PCT0_SHIFT;
    UInt8  udmaClockShifts = 0;

    DLOG("%s::%s( %d )\n", getName(), __FUNCTION__, unit);

    if ( _channel == kPIIX_CHANNEL_SECONDARY )
    {
        udmaEnableBit  <<= 2;
        udmatimShifts   += 8;
        udmaClockShifts += 2;
    }
    if ( unit == kATADevice1DeviceID )
    {
        udmaEnableBit  <<= 1;
        udmatimShifts   += 4;
        udmaClockShifts += 1;
    }

    // Disable U-DMA for this drive unit.

    _udmactl &= ~udmaEnableBit;

    // If U-DMA is enabled for this unit, update timing register
    // and enable U-DMA.

    if ( _udmaTiming[unit] )
    {
        _udmatim &= ~( kPIIX_PCI_UDMATIM_PCT0_MASK << udmatimShifts );
        _udmatim |= (( _udmaTiming[unit]->udmatim << udmatimShifts ) &
                     ( kPIIX_PCI_UDMATIM_PCT0_MASK << udmatimShifts ));

        _ideConfig &= ~(( kPIIX_PCI_IDECONFIG_PCB0 |
                          kPIIX_PCI_IDECONFIG_FAST_PCB0 ) << udmaClockShifts);
        _ideConfig |= ( _udmaTiming[unit]->udmaClock << udmaClockShifts );

        _udmactl |= udmaEnableBit;
    }
}

/*---------------------------------------------------------------------------
 *
 * Compute the PIO/DMA timing register values.
 *
 ---------------------------------------------------------------------------*/

void
AppleIntelPIIXATADriver::computeTimingRegisters( ataUnitID unit )
{
    const PIIXTiming * timing;
    UInt8              index;
    bool               slowPIO = false;

    DLOG("%s::%s( %d )\n", getName(), __FUNCTION__, unit);

	assert( _pioTiming[unit] != 0 );

    timing = _dmaTiming[unit] ? _dmaTiming[unit] : _pioTiming[unit];

	if ( timing->cycleTime != _pioTiming[unit]->cycleTime )
    {
        DLOG("%s: using PIO compatible timing\n", getName());
        slowPIO = true;
    }

    // Get register programming index.

    index = timing->registerIndex;

    if ( _provider->hasPerChannelTimingSupport() == false )
    {
        _idetim[unit] = piixIDETIM[index][unit];
    }
    else /* PIIX3 or better */
    {
        _idetim[unit] = piix3IDETIM[index][unit];

        /* Update SIDETIM register for Drive 1 */

        if ( unit == kATADevice1DeviceID )
        {
            _sidetim &= ~piix3SIDETIM[0][_channel];
            _sidetim |=  piix3SIDETIM[index][_channel];
        }

        // Always enable the PIO performance feature on
        // ICH controllers.
    
        _ideConfig |= kPIIX_PCI_IDECONFIG_WR_PP_EN;
    }

    if ( slowPIO )
    {
        if ( unit == kATADevice1DeviceID )
            _idetim[unit] &= ~kPIIX_PCI_IDETIM_DTE1;
        else
            _idetim[unit] &= ~kPIIX_PCI_IDETIM_DTE0;
    }
}

/*---------------------------------------------------------------------------
 *
 * Select the bus timing configuration for a particular device.
 *
 ---------------------------------------------------------------------------*/

void 
AppleIntelPIIXATADriver::selectIOTiming( ataUnitID unit )
{
    if ( _provider->hasPerChannelTimingSupport() == false )
    {
        DLOG("%s::%s( %d )\n", getName(), __FUNCTION__, unit);
        writeTimingRegisters( unit );
    }
}

/*---------------------------------------------------------------------------
 *
 * Flush the outstanding commands in the command queue.
 * Implementation borrowed from MacIOATA in IOATAFamily.
 *
 ---------------------------------------------------------------------------*/

IOReturn
AppleIntelPIIXATADriver::handleQueueFlush()
{
	UInt32 savedQstate = _queueState;

    DLOG("%s::%s()\n", getName(), __FUNCTION__);

	_queueState = IOATAController::kQueueLocked;

	IOATABusCommand * cmdPtr = 0;

	while ( cmdPtr = dequeueFirstCommand() )
	{
		cmdPtr->setResult( kIOReturnError );
		cmdPtr->executeCallback();
	}

	_queueState = savedQstate;

	return kATANoErr;
}

/*---------------------------------------------------------------------------
 *
 * Handle termination notification from the provider.
 *
 ---------------------------------------------------------------------------*/

IOReturn
AppleIntelPIIXATADriver::message( UInt32      type,
                                  IOService * provider,
                                  void *      argument )
{
    if ( ( provider == _provider ) &&
         ( type == kIOMessageServiceIsTerminated ) )
    {
        _provider->close( this );
        return kIOReturnSuccess;
    }

    return super::message( type, provider, argument );
}

/*---------------------------------------------------------------------------
 *
 * Publish a numeric property pertaining to a drive to the registry.
 *
 ---------------------------------------------------------------------------*/

bool
AppleIntelPIIXATADriver::setDriveProperty( UInt8        driveUnit,
                                           const char * key,
                                           UInt64       value,
                                           UInt         numberOfBits)
{
    char keyString[40];
    
    snprintf(keyString, 40, "Drive %d %s", driveUnit, key);
    
    return super::setProperty( keyString, value, numberOfBits );
}
