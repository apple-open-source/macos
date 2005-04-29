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

#include <sys/systm.h>    // snprintf
#include <IOKit/assert.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include "AppleVIAATADriver.h"

#define super IOPCIATA
OSDefineMetaClassAndStructors( AppleVIAATADriver, IOPCIATA )

#if     0
#define DEBUG_LOG(fmt, args...) kprintf(fmt, ## args)
#define ERROR_LOG(fmt, args...) kprintf(fmt, ## args)
#else
#define DEBUG_LOG(fmt, args...)
#define ERROR_LOG(fmt, args...) IOLog(fmt, ## args)
#endif

#define kPIOModeMask   ((1 << kPIOModeCount) - 1)
#define kDMAModeMask   ((1 << kDMAModeCount) - 1)
#define kUDMAModeMask  (fProvider->getUltraDMAModeMask())

#define DRIVE_IS_PRESENT(u) \
        (_devInfo[u].type != kUnknownATADeviceType)

#define TIMING_PARAM_IS_VALID(p) \
        ((p) != 0)

// Increase the PRD table size to one full page or 4096 descriptors for
// large transfers via DMA.  2048 are required for 1 megabyte transfers
// assuming no fragmentation and no alignment issues on the buffer.  We
// allocate twice that since there are more issues than simple alignment
// for this DMA engine.

#define kATAXferDMADesc  512
#define kATAMaxDMADesc   kATAXferDMADesc

// up to 2048 ATA sectors per transfer

#define kMaxATAXfer      512 * 2048

/*---------------------------------------------------------------------------
 *
 * Start the single-channel VIA ATA controller driver.
 *
 ---------------------------------------------------------------------------*/

bool AppleVIAATADriver::start( IOService * provider )
{
    bool superStarted = false;

    DEBUG_LOG("%s: %s( %p, %p )\n", getName(), __FUNCTION__, this, provider);

    // Our provider is a 'nub' that represents a single channel PCI ATA
    // controller, and not an IOPCIDevice.

    fProvider = OSDynamicCast( AppleVIAATAChannel, provider );
    if ( fProvider == 0 )
        goto fail;

    // Retain and open our provider.

    fProvider->retain();

    if ( fProvider->open( this ) != true )
    {
        DEBUG_LOG("%s: provider open failed\n", getName());
        goto fail;
    }

    // Create a work loop.

    fWorkLoop = IOWorkLoop::workLoop();
    if ( fWorkLoop == 0 )
    {
        DEBUG_LOG("%s: new work loop failed\n", getName());
        goto fail;
    }

    // Cache static controller properties.

    fChannelNumber = fProvider->getChannelNumber();
    if ( fChannelNumber > SEC_CHANNEL_ID )
    {
        DEBUG_LOG("%s: bad ATA channel number %ld\n", getName(),
                  fChannelNumber);
        goto fail;
    }

    // Probe for 80-pin conductors on drive 0 and 1.

    f80PinCable[0] = ((readTimingRegister(kVIATimingRegUltra, 0) & 0x10) != 0);
    f80PinCable[1] = ((readTimingRegister(kVIATimingRegUltra, 1) & 0x10) != 0);

    // Get the base address for the bus master registers in I/O space.

    if ( getBMBaseAddress( fChannelNumber, &fBMBaseAddr ) != true )
    {
        DEBUG_LOG("%s: invalid bus-master base address\n", getName());
        goto fail;
    }

    // Must setup these variables inherited from IOPCIATA before it is started.

    _bmCommandReg   = IOATAIOReg8::withAddress( fBMBaseAddr + BM_COMMAND );
    _bmStatusReg    = IOATAIOReg8::withAddress( fBMBaseAddr + BM_STATUS );
    _bmPRDAddresReg = IOATAIOReg32::withAddress( fBMBaseAddr + BM_PRD_TABLE );

    // Reset bus timings for both drives.

    initializeHardware();
    resetBusTimings();

    // Override P-ATA reporting in IOATAController::start()
    // for SystemProfiler.

    if (fProvider->getHardwareType() == VIA_HW_SATA)
    {
        setProperty( kIOPropertyPhysicalInterconnectTypeKey,
                     kIOPropertyPhysicalInterconnectTypeSerialATA );
    }


    // Now we are ready to call super::start

    if ( super::start(_provider) == false )
    {
        goto fail;
    }
    superStarted = true;

    // This driver will handle interrupts using a work loop.
    // Create interrupt event source that will signal the
    // work loop (thread) when a device interrupt occurs.

    if ( fProvider->getInterruptVector() == 14 ||
         fProvider->getInterruptVector() == 15 )
    {
        // Legacy IRQ are never shared, no need for an interrupt filter.

        fInterruptSource = IOInterruptEventSource::interruptEventSource(
                           this, &interruptOccurred,
                           fProvider, 0 );
    }
    else
    {
        fInterruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(
                           this, &interruptOccurred, &interruptFilter,
                           fProvider, 0 );
    }

    if ( !fInterruptSource ||
         (fWorkLoop->addEventSource(fInterruptSource) != kIOReturnSuccess) )
    {
        DEBUG_LOG("%s: interrupt registration error\n", getName());
        goto fail;
    }
    fInterruptSource->enable();

    // Attach to power management.

    initForPM( provider );

    // For each device discovered on the ATA bus (by super),
    // create a nub for that device and call registerService() to
    // trigger matching against that device.

    for ( UInt32 i = 0; i < kMaxDriveCount; i++ )
    {
        if ( _devInfo[i].type != kUnknownATADeviceType )
        {
            ATADeviceNub * nub;

            nub = ATADeviceNub::ataDeviceNub( (IOATAController*) this,
                                              (ataUnitID) i,
                                              _devInfo[i].type );

            if ( nub )
            {
                if ( _devInfo[i].type == kATAPIDeviceType )
                {
                    nub->setProperty( kIOMaximumSegmentCountReadKey,
                                      kATAMaxDMADesc / 2, 64 );

                    nub->setProperty( kIOMaximumSegmentCountWriteKey,
                                      kATAMaxDMADesc / 2, 64 );

                    nub->setProperty( kIOMaximumSegmentByteCountReadKey,
                                      0x10000, 64 );

                    nub->setProperty( kIOMaximumSegmentByteCountWriteKey,
                                      0x10000, 64 );
                }

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

    // Successful start, announce useful properties.

    IOLog("%s: VIA %s (CMD 0x%x, CTR 0x%x, IRQ %ld, BM 0x%x)\n", getName(),
          fProvider->getHardwareName(),
          fProvider->getCommandBlockAddress(),
          fProvider->getControlBlockAddress(),
          fProvider->getInterruptVector(),
          fBMBaseAddr);

    return true;

fail:
    if ( fProvider )
        fProvider->close( this );

    if ( superStarted )
        super::stop( provider );

    return false;
}

/*---------------------------------------------------------------------------
 *
 * Stop the single-channel VIA ATA controller driver.
 *
 ---------------------------------------------------------------------------*/

void AppleVIAATADriver::stop( IOService * provider )
{
    PMstop();
    super::stop( provider );
}

/*---------------------------------------------------------------------------
 *
 * Release resources before this driver is destroyed.
 *
 ---------------------------------------------------------------------------*/

void AppleVIAATADriver::free( void )
{
#define RELEASE(x) do { if(x) { (x)->release(); (x) = 0; } } while(0)

    DEBUG_LOG("%s::%s( %p )\n", getName(), __FUNCTION__, this);

    // Release resources created by start().

    if (fInterruptSource && fWorkLoop)
    {
        fWorkLoop->removeEventSource(fInterruptSource);
    }

    RELEASE( fProvider        );
    RELEASE( fInterruptSource );
    RELEASE( fWorkLoop        );
    RELEASE( _nub[0]          );
    RELEASE( _nub[1]          );
    RELEASE( _bmCommandReg    );
    RELEASE( _bmStatusReg     );
    RELEASE( _bmPRDAddresReg  );

    // Release registers created by configureTFPointers().

    RELEASE( _tfDataReg       );
    RELEASE( _tfFeatureReg    );
    RELEASE( _tfSCountReg     );
    RELEASE( _tfSectorNReg    );
    RELEASE( _tfCylLoReg      );
    RELEASE( _tfCylHiReg      );
    RELEASE( _tfSDHReg        );
    RELEASE( _tfStatusCmdReg  );
    RELEASE( _tfAltSDevCReg   );

    // IOATAController should release this.

    if ( _doubleBuffer.logicalBuffer )
    {
        IOFree( (void *) _doubleBuffer.logicalBuffer,
                         _doubleBuffer.bufferSize );
        _doubleBuffer.bufferSize     = 0;
        _doubleBuffer.logicalBuffer  = 0;
        _doubleBuffer.physicalBuffer = 0;
    }

    // What about _cmdGate, and _timer in the superclass?

    super::free();
}

/*---------------------------------------------------------------------------
 *
 * Return the driver's work loop
 *
 ---------------------------------------------------------------------------*/

IOWorkLoop * AppleVIAATADriver::getWorkLoop( void ) const
{
    return fWorkLoop;
}

/*---------------------------------------------------------------------------
 *
 * Override IOATAController::synchronousIO()
 *
 ---------------------------------------------------------------------------*/

IOReturn AppleVIAATADriver::synchronousIO( void )
{
    IOReturn ret;
    
    // IOATAController::synchronousIO() asserts nIEN bit in order to disable
    // drive interrupts during polled mode command execution. The problem is
    // that this will float the INTRQ line and put it in high impedance state,
    // which on certain systems has the undesirable effect of latching a false
    // interrupt on the interrupt controller. Perhaps those systems lack a
    // strong pull down resistor on the INTRQ line. Experiment shows that the
    // interrupt event source is signalled, and its producerCount incremented
    // after every synchronousIO() call. This false interrupt can become
    // catastrophic after reverting to async operations since software can
    // issue a command, handle the false interrupt, and issue another command
    // to the drive before the actual completion of the first command, leading
    // to a irrecoverable bus hang. This function is called after an ATA bus
    // reset. Waking from system sleep will exercise this path.
    // The workaround is to mask the interrupt line while the INTRQ line is
    // floating (or bouncing).

    if (fInterruptSource) fInterruptSource->disable();
    ret = super::synchronousIO();
    if (fInterruptSource) fInterruptSource->enable();

    return ret;
}

/*---------------------------------------------------------------------------
 *
 * Determine the start of the I/O mapped Bus-Master registers.
 *
 ---------------------------------------------------------------------------*/

bool AppleVIAATADriver::getBMBaseAddress( UInt32   channel,
                                          UInt16 * baseAddr )
{
    UInt32 bmiba;

    DEBUG_LOG("%s::%s( %p, %ld, %p )\n", getName(), __FUNCTION__,
              this, channel, baseAddr);

    bmiba = fProvider->pciConfigRead32( PCI_BMIBA );

    if ((bmiba & PCI_BMIBA_RTE) == 0)
    {
        DEBUG_LOG("%s: PCI BAR 0x%02x (0x%08lx) is not an I/O range\n",
                  getName(), PCI_BMIBA, bmiba);
        return false;
    }

    bmiba &= PCI_BMIBA_MASK;  // get the address portion
    if (bmiba == 0)
    {
        DEBUG_LOG("%s: BMIBA is zero\n", getName());
        return false;
    }

    if (channel == SEC_CHANNEL_ID)
        bmiba += BM_SEC_OFFSET;

    *baseAddr = (UInt16) bmiba;
    DEBUG_LOG("%s: BMBaseAddr = %04x\n", getName(), *baseAddr);

    return true;
}

/*---------------------------------------------------------------------------
 *
 * Reset all timing registers to the slowest (most compatible) timing.
 * DMA modes are disabled.
 *
 ---------------------------------------------------------------------------*/

void AppleVIAATADriver::resetBusTimings( void )
{
    DEBUG_LOG("%s::%s( %p )\n", getName(), __FUNCTION__, this);

    memset(&fBusTimings[0], 0, sizeof(fBusTimings));

    fBusTimings[0].pioTiming = &PIOTimingTable[0];
    fBusTimings[1].pioTiming = &PIOTimingTable[0];

    programTimingRegisters();
}

/*---------------------------------------------------------------------------
 *
 * Setup the location of the task file registers.
 *
 ---------------------------------------------------------------------------*/

bool AppleVIAATADriver::configureTFPointers( void )
{
    DEBUG_LOG("%s::%s( %p )\n", getName(), __FUNCTION__, this);

    UInt16 cmdBlockAddr = fProvider->getCommandBlockAddress();
    UInt16 ctrBlockAddr = fProvider->getControlBlockAddress();

    _tfDataReg      = IOATAIOReg16::withAddress( cmdBlockAddr + 0 );
    _tfFeatureReg   = IOATAIOReg8::withAddress(  cmdBlockAddr + 1 );
    _tfSCountReg    = IOATAIOReg8::withAddress(  cmdBlockAddr + 2 );
    _tfSectorNReg   = IOATAIOReg8::withAddress(  cmdBlockAddr + 3 );
    _tfCylLoReg     = IOATAIOReg8::withAddress(  cmdBlockAddr + 4 );
    _tfCylHiReg     = IOATAIOReg8::withAddress(  cmdBlockAddr + 5 );
    _tfSDHReg       = IOATAIOReg8::withAddress(  cmdBlockAddr + 6 );
    _tfStatusCmdReg = IOATAIOReg8::withAddress(  cmdBlockAddr + 7 );
    _tfAltSDevCReg  = IOATAIOReg8::withAddress(  ctrBlockAddr + 2 );

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
 * Filter interrupts that are not originated by our hardware. This will help
 * prevent waking up our work loop thread when sharing a interrupt line with
 * another driver.
 *
 ---------------------------------------------------------------------------*/

bool AppleVIAATADriver::interruptFilter( OSObject * owner,
                                         IOFilterInterruptEventSource * src )
{
    AppleVIAATADriver * self = (AppleVIAATADriver *) owner;

    if ( *(self->_bmStatusReg) & BM_STATUS_INT )
        return true;   // signal the work loop
    else
        return false;  // ignore this spurious interrupt
}

/*---------------------------------------------------------------------------
 *
 * The work loop based interrupt handler called by our interrupt event
 * source.
 *
 ---------------------------------------------------------------------------*/

void AppleVIAATADriver::interruptOccurred( OSObject *               owner,
                                           IOInterruptEventSource * source,
                                           int                      count )
{
    AppleVIAATADriver * self = (AppleVIAATADriver *) owner;

    // Clear interrupt latch

    *(self->_bmStatusReg) = BM_STATUS_INT;

    // Let our superclass handle the interrupt to advance to the next state
    // in the state machine.

    self->handleDeviceInterrupt();
}

/*---------------------------------------------------------------------------
 *
 * Extend the implementation of scanForDrives() from IOATAController
 * to issue a soft reset before scanning for ATA/ATAPI drive signatures.
 *
 ---------------------------------------------------------------------------*/

UInt32 AppleVIAATADriver::scanForDrives( void )
{
    UInt32 unitsFound;

    DEBUG_LOG("%s::%s( %p )\n", getName(), __FUNCTION__, this);

    *_tfAltSDevCReg = mATADCRReset;

    IODelay( 100 );

    *_tfAltSDevCReg = 0x0;

    IOSleep( 10 );

    unitsFound = super::scanForDrives();

    *_tfSDHReg = 0x00;  // Initialize device selection to device 0.

    return unitsFound;
}

/*---------------------------------------------------------------------------
 *
 * Provide information on the ATA bus capability.
 *
 ---------------------------------------------------------------------------*/

IOReturn AppleVIAATADriver::provideBusInfo( IOATABusInfo * infoOut )
{
    DEBUG_LOG("%s::%s( %p, %p )\n", getName(), __FUNCTION__, this, infoOut);

    if ( infoOut == 0 )
    {
        DEBUG_LOG("%s: %s bad argument\n", getName(), __FUNCTION__);
        return -1;
    }

    infoOut->zeroData();

    if (fProvider->getHardwareType() == VIA_HW_SATA)
        infoOut->setSocketType( kInternalSATA );
    else
        infoOut->setSocketType( kInternalATASocket );

    infoOut->setPIOModes( kPIOModeMask );
    infoOut->setDMAModes( kDMAModeMask );
    infoOut->setUltraModes( kUDMAModeMask );
    infoOut->setExtendedLBA( true );
    infoOut->setMaxBlocksExtended( 0x0800 );  // 2048 sectors for ext LBA

    UInt8 units = 0;
    if ( _devInfo[0].type != kUnknownATADeviceType ) units++;
    if ( _devInfo[1].type != kUnknownATADeviceType ) units++;
    infoOut->setUnits( units );

    return kATANoErr;
}

/*---------------------------------------------------------------------------
 *
 * Returns the currently configured timings for the drive unit.
 *
 ---------------------------------------------------------------------------*/

IOReturn AppleVIAATADriver::getConfig( IOATADevConfig * configOut,
                                       UInt32           unit )
{
    DEBUG_LOG("%s::%s( %p, %p, %ld )\n", getName(), __FUNCTION__,
              this, configOut, unit);

    if ((configOut == 0) || (unit > kATADevice1DeviceID))
    {
        DEBUG_LOG("%s: %s bad argument\n", getName(), __FUNCTION__);
        return -1;
    }

    configOut->setPIOMode( 0 );
    configOut->setDMAMode( 0 );
    configOut->setUltraMode( 0 );

    // Note that we need to report the bitmap of each mode,
    // not its mode number.

    if (TIMING_PARAM_IS_VALID(fBusTimings[unit].pioTiming))
    {
        configOut->setPIOMode( 1 << fBusTimings[unit].pioModeNumber );
        configOut->setPIOCycleTime( fBusTimings[unit].pioTiming->cycle );
    }

    if (TIMING_PARAM_IS_VALID(fBusTimings[unit].dmaTiming))
    {
        configOut->setDMAMode( 1 << fBusTimings[unit].dmaModeNumber );
        configOut->setDMACycleTime( fBusTimings[unit].dmaTiming->cycle );
    }

    if (fBusTimings[unit].ultraEnabled)
    {
        configOut->setUltraMode( 1 << fBusTimings[unit].ultraModeNumber );
    }

    configOut->setPacketConfig( _devInfo[unit].packetSend );

    return kATANoErr;
}

/*---------------------------------------------------------------------------
 *
 * Select the bus timings for a given drive unit.
 *
 ---------------------------------------------------------------------------*/

IOReturn AppleVIAATADriver::selectConfig( IOATADevConfig * configRequest,
                                          UInt32           unit )
{
    DEBUG_LOG("%s::%s( %p, %p, %ld )\n", getName(), __FUNCTION__,
              this, configRequest, unit);

    if ((configRequest == 0) || (unit > kATADevice1DeviceID))
    {
        DEBUG_LOG("%s: %s bad argument\n", getName(), __FUNCTION__);
        return -1;
    }

    // All config requests must include a supported PIO mode

    if ((configRequest->getPIOMode() & kPIOModeMask) == 0)
    {
        DEBUG_LOG("%s: PIO mode unsupported\n", getName());
        return kATAModeNotSupported;
    }

    if (configRequest->getDMAMode() & ~kDMAModeMask)
    {
        DEBUG_LOG("%s: DMA mode unsupported (0x%x)\n",
                  getName(), configRequest->getDMAMode());
        return kATAModeNotSupported;
    }

    if (configRequest->getUltraMode() & ~kUDMAModeMask)
    {
        DEBUG_LOG("%s: UDMA mode unsupported (0x%x)\n",
                  getName(), configRequest->getUltraMode());
        return kATAModeNotSupported;
    }

    if (configRequest->getDMAMode() && configRequest->getUltraMode())
    {
        DEBUG_LOG("%s: multiple DMA mode selection error\n", getName());
        return kATAModeNotSupported;
    }

    _devInfo[unit].packetSend = configRequest->getPacketConfig();

    selectTimingParameter( configRequest, unit );

    return getConfig( configRequest, unit );
}

/*---------------------------------------------------------------------------
 *
 * Select timing parameters based on config request.
 *
 ---------------------------------------------------------------------------*/

void AppleVIAATADriver::selectTimingParameter( IOATADevConfig * configRequest,
                                               UInt32           unit )
{
    DEBUG_LOG("%s::%s( %p, %d )\n", getName(), __FUNCTION__, this, unit);

    // Reset existing parameters for this unit.

    fBusTimings[unit].pioTiming = 0;
    fBusTimings[unit].dmaTiming = 0;
    fBusTimings[unit].ultraEnabled = false;

    if ( configRequest->getPIOMode() )
    {
        UInt32  pioModeNumber;
        UInt32  pioCycleTime;
        UInt32  pioTimingEntry = 0;

        pioModeNumber = bitSigToNumeric( configRequest->getPIOMode() );
        pioModeNumber = min(pioModeNumber, kPIOModeCount - 1);

        // Use a default cycle time if the device didn't report a time to use.
    
        pioCycleTime = configRequest->getPIOCycleTime();
        pioCycleTime = max(pioCycleTime, PIOMinCycleTime[pioModeNumber]);

        // Look for the fastest entry in the PIOTimingTable with a cycle time
        // which is larger than or equal to pioCycleTime.
    
        for (int i = kPIOTimingCount - 1; i > 0; i--)
        {
            if ( PIOTimingTable[i].cycle >= pioCycleTime )
            {
                pioTimingEntry = i;
                break;
            }
        }

        fBusTimings[unit].pioTiming = &PIOTimingTable[pioTimingEntry];
        fBusTimings[unit].pioModeNumber = pioModeNumber;
        DEBUG_LOG("%s: selected PIO mode %d\n", getName(), pioModeNumber);
        setDriveProperty(unit, kSelectedPIOModeKey, pioModeNumber, 8);
    }

    if ( configRequest->getDMAMode() )
    {
        UInt32  dmaModeNumber;
        UInt32  dmaCycleTime;
        UInt32  dmaTimingEntry = 0;

        dmaModeNumber = bitSigToNumeric( configRequest->getDMAMode() );
        dmaModeNumber = min(dmaModeNumber, kDMAModeCount - 1);

        dmaCycleTime = configRequest->getDMACycleTime();
        dmaCycleTime = max(dmaCycleTime, DMAMinCycleTime[dmaModeNumber]);

        // Look for the fastest entry in the DMATimingTable with a cycle time
        // which is larger than or equal to dmaCycleTime.
    
        for (int i = kDMATimingCount - 1; i > 0; i--)
        {
            if ( DMATimingTable[i].cycle >= dmaCycleTime )
            {
                dmaTimingEntry = i;
                break;
            }
        }
        
        fBusTimings[unit].dmaTiming = &DMATimingTable[dmaTimingEntry];
        fBusTimings[unit].dmaModeNumber = dmaModeNumber;
        DEBUG_LOG("%s: selected DMA mode %d\n", getName(), dmaModeNumber);
        setDriveProperty(unit, kSelectedDMAModeKey, dmaModeNumber, 8);
    }

    if ( configRequest->getUltraMode() )
    {
        UInt32  ultraModeNumber;

        ultraModeNumber = bitSigToNumeric( configRequest->getUltraMode() );
        ultraModeNumber = min(ultraModeNumber, kUDMAModeCount - 1);

        // For Ultra DMA mode 3 or higher, 80 pin cable must be present.
        // Otherwise, the drive will be limited to UDMA mode 2.

        if ( fProvider->getHardwareType() != VIA_HW_SATA &&
             ultraModeNumber > 2 )
        {
            if ( f80PinCable[unit] == false )
            {
                DEBUG_LOG("%s: 80-conductor cable not detected\n", getName());
                ultraModeNumber = 2;
            }
        }

        fBusTimings[unit].ultraEnabled = true;
        fBusTimings[unit].ultraModeNumber = ultraModeNumber;
        DEBUG_LOG("%s: selected Ultra mode %d\n", getName(), ultraModeNumber);
        setDriveProperty(unit, kSelectedUltraDMAModeKey, ultraModeNumber, 8);
    }

    programTimingRegisters();
}

/*---------------------------------------------------------------------------
 *
 * Program timing registers for both drives.
 *
 ---------------------------------------------------------------------------*/

static void mergeTimings( VIATimingParameter *       dst,
                          const VIATimingParameter * src )
{
    if (TIMING_PARAM_IS_VALID(dst) == false ||
        TIMING_PARAM_IS_VALID(src) == false)
        return;

    dst->cycle    = max(dst->cycle, src->cycle);
    dst->setup    = max(dst->setup, src->setup);
    dst->active   = max(dst->active, src->active);
    dst->recovery = max(dst->recovery, src->recovery);
}

void AppleVIAATADriver::programTimingRegisters( void )
{
    if (fProvider->getHardwareType() != VIA_HW_SATA)
    {
        VIATimingParameter  timingCommand;  // shared between both drives
        VIATimingParameter  timingData[2];

        memset(&timingCommand, 0, sizeof(timingCommand));
        memset(&timingData[0], 0, sizeof(timingData));

        for (int unit = 0; unit < 2; unit++)
        {
            if (DRIVE_IS_PRESENT(unit) == false)
                continue;

            mergeTimings( &timingCommand,    fBusTimings[unit].pioTiming );
            mergeTimings( &timingData[unit], fBusTimings[unit].pioTiming );
            mergeTimings( &timingData[unit], fBusTimings[unit].dmaTiming );
        }

        // We now have all the information need to program the registers.

        for (int unit = 0; unit < 2; unit++)
        {
            if (DRIVE_IS_PRESENT(unit) == false)
                continue;

            writeTimingIntervalNS( kVIATimingRegCommandActive,
                                unit, timingCommand.active );

            writeTimingIntervalNS( kVIATimingRegCommandRecovery,
                                unit, timingCommand.recovery );

#if 0   // FIXME - is this necessary?
            writeTimingIntervalNS( kVIATimingRegAddressSetup,
                                unit, timingData[unit].setup );
#endif

            writeTimingIntervalNS( kVIATimingRegDataActive,
                                unit, timingData[unit].active );

            writeTimingIntervalNS( kVIATimingRegDataRecovery,
                                unit, timingData[unit].recovery );

            if (fBusTimings[unit].ultraEnabled)
            {
                UInt8 mode = fBusTimings[unit].ultraModeNumber;
                UInt8 type = fProvider->getHardwareType();
                writeTimingRegister( kVIATimingRegUltra, unit,
                                    UltraTimingTable[type][mode]); 
            }
            else
            {
                writeTimingRegister( kVIATimingRegUltra, unit, 0x8b ); 
            }        
        }
    }

    dumpVIARegisters();
}

/*---------------------------------------------------------------------------
 *
 * Read and write timing registers.
 *
 ---------------------------------------------------------------------------*/

void AppleVIAATADriver::writeTimingIntervalNS( VIATimingReg reg,
                                               UInt32       unit,
                                               UInt32       timeNS )
{
    const UInt32 clockPeriodPS = 30000; // 30ns @ 33MHz PCI
    UInt32 periods = ((timeNS * 1000) + clockPeriodPS - 1) / clockPeriodPS;

    periods =  min(periods, VIATimingRegInfo[reg].maxValue);
    periods =  max(periods, VIATimingRegInfo[reg].minValue);
    periods -= VIATimingRegInfo[reg].minValue;
    periods <<= VIATimingRegInfo[reg].shift;
    periods &= VIATimingRegInfo[reg].mask;

    writeTimingRegister( reg, unit, periods );
}

void AppleVIAATADriver::writeTimingRegister( VIATimingReg reg,
                                             UInt32       unit,
                                             UInt8        periods )
{
    fProvider->pciConfigWrite8( VIATimingRegOffset[reg][fChannelNumber][unit],
                                periods,
                                VIATimingRegInfo[reg].mask);

    DEBUG_LOG("%s: CH%d DRV%d wrote 0x%02x to offset 0x%02x\n",
              getName(), fChannelNumber, unit, periods,
              VIATimingRegOffset[reg][fChannelNumber][unit]);
}

UInt32 AppleVIAATADriver::readTimingIntervalNS( VIATimingReg reg, UInt32 unit )
{
    UInt32 time;

    time =   readTimingRegister( reg, unit );
    time &=  VIATimingRegInfo[reg].mask;
    time >>= VIATimingRegInfo[reg].shift;
    time +=  VIATimingRegInfo[reg].minValue;
    time *=  30;

    return time;
}

UInt8 AppleVIAATADriver::readTimingRegister( VIATimingReg reg, UInt32 unit )
{
    return fProvider->pciConfigRead8(
                      VIATimingRegOffset[reg][fChannelNumber][unit]);
}

/*---------------------------------------------------------------------------
 *
 * Hardware initialization.
 *
 ---------------------------------------------------------------------------*/

void AppleVIAATADriver::initializeHardware( void )
{
    // Turn on prefetch and post write buffers for both primary and
    // secondary channels.

    fProvider->pciConfigWrite8( VIA_IDE_CONFIG, 0xF0, 0xF0 );

    if (fProvider->getHardwareType() == VIA_HW_UDMA_66)
    {
        // Setup ATA-66 clock.
        fProvider->pciConfigWrite32( VIA_ULTRA_TIMING, 0x00080008,
                                     0x00080008 );
    }
}

/*---------------------------------------------------------------------------
 *
 * Dynamically select the bus timings for a drive unit.
 *
 ---------------------------------------------------------------------------*/

void AppleVIAATADriver::selectIOTiming( ataUnitID unit )
{
    /* Timings was already applied by selectConfig() */
}

/*---------------------------------------------------------------------------
 *
 * Flush the outstanding commands in the command queue.
 * Implementation borrowed from MacIOATA in IOATAFamily.
 *
 ---------------------------------------------------------------------------*/

IOReturn AppleVIAATADriver::handleQueueFlush( void )
{
    UInt32 savedQstate = _queueState;

    DEBUG_LOG("%s::%s()\n", getName(), __FUNCTION__);

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

IOReturn AppleVIAATADriver::message( UInt32      type,
                                      IOService * provider,
                                      void *      argument )
{
    if ( ( provider == fProvider ) &&
         ( type == kIOMessageServiceIsTerminated ) )
    {
        fProvider->close( this );
        return kIOReturnSuccess;
    }

    return super::message( type, provider, argument );
}

/*---------------------------------------------------------------------------
 *
 * Publish a numeric property pertaining to a drive to the registry.
 *
 ---------------------------------------------------------------------------*/

bool AppleVIAATADriver::setDriveProperty( UInt32       driveUnit,
                                          const char * key,
                                          UInt32       value,
                                          UInt32       numberOfBits)
{
    char keyString[40];
    
    snprintf(keyString, 40, "Drive %ld %s", driveUnit, key);
    
    return super::setProperty( keyString, value, numberOfBits );
}

//---------------------------------------------------------------------------

IOReturn AppleVIAATADriver::createChannelCommands( void )
{
    IOMemoryDescriptor* descriptor = _currentCommand->getBuffer();
    IOMemoryCursor::PhysicalSegment physSegment;
    UInt32 index = 0;
    UInt8  *xferDataPtr, *ptr2EndData, *next64KBlock, *starting64KBlock;
    UInt32 xferCount, count2Next64KBlock;
    
    if ( !descriptor )
    {
        return -1;
    }

    // This form of DMA engine can only do 1 pass.
    // It cannot execute multiple chains.

    IOByteCount bytesRemaining = _currentCommand->getByteCount() ;
    IOByteCount xfrPosition    = _currentCommand->getPosition() ;
    IOByteCount  transferSize  = 0; 

    // There's a unique problem with pci-style controllers, in that each
    // dma transaction is not allowed to cross a 64K boundary. This leaves
    // us with the yucky task of picking apart any descriptor segments that
    // cross such a boundary ourselves.  

    while ( _DMACursor->getPhysicalSegments(
                           /* descriptor */ descriptor,
                           /* position   */ xfrPosition,
                           /* segments   */ &physSegment,
                           /* max segs   */ 1,
                           /* max xfer   */ bytesRemaining,
                           /* xfer size  */ &transferSize) )
    {
        xferDataPtr = (UInt8 *) physSegment.location;
        xferCount   = physSegment.length;

        if ( (UInt32) xferDataPtr & 0x01 )
        {
            IOLog("%s: DMA buffer %p not 2 byte aligned\n",
                  getName(), xferDataPtr);
            return kIOReturnNotAligned;        
        }

        if ( xferCount & 0x01 )
        {
            IOLog("%s: DMA buffer length %ld is odd\n",
                  getName(), xferCount);
        }

        // Update bytes remaining count after this pass.
        bytesRemaining -= xferCount;
        xfrPosition += xferCount;
            
        // Examine the segment to see whether it crosses (a) 64k boundary(s)
        starting64KBlock = (UInt8*) ( (UInt32) xferDataPtr & 0xffff0000);
        ptr2EndData  = xferDataPtr + xferCount;
        next64KBlock = starting64KBlock + 0x10000;

        // Loop until this physical segment is fully accounted for.
        // It is possible to have a memory descriptor which crosses more
        // than one 64K boundary in a single span.
        
        while ( xferCount > 0 )
        {
            if (ptr2EndData > next64KBlock)
            {
                count2Next64KBlock = next64KBlock - xferDataPtr;
                if ( index < kATAMaxDMADesc )
                {
                    setPRD( xferDataPtr, (UInt16)count2Next64KBlock,
                            &_prdTable[index], kContinue_PRD);
                    
                    xferDataPtr = next64KBlock;
                    next64KBlock += 0x10000;
                    xferCount -= count2Next64KBlock;
                    index++;
                }
                else
                {
                    IOLog("%s: PRD table exhausted error 1\n", getName());
                    _dmaState = kATADMAError;
                    return -1;
                }
            }
            else
            {
                if (index < kATAMaxDMADesc)
                {
                    setPRD( xferDataPtr, (UInt16) xferCount,
                            &_prdTable[index],
                            (bytesRemaining == 0) ? kLast_PRD : kContinue_PRD);
                    xferCount = 0;
                    index++;
                }
                else
                {
                    IOLog("%s: PRD table exhausted error 2\n", getName());
                    _dmaState = kATADMAError;
                    return -1;
                }
            }
        }
    } // end of segment counting loop.

    if (index == 0)
    {
        IOLog("%s: rejected command with zero PRD count (0x%lx bytes)\n",
              getName(), _currentCommand->getByteCount());
        return kATADeviceError;
    }

    // Transfer is satisfied and only need to check status on interrupt.
    _dmaState = kATADMAStatus;
    
    // Chain is now ready for execution.
    return kATANoErr;
}

//---------------------------------------------------------------------------

bool AppleVIAATADriver::allocDMAChannel( void )
{
    _prdTable = (PRD *) IOMallocContiguous(
                        /* size  */ sizeof(PRD) * kATAMaxDMADesc, 
                        /* align */ 0x10000, 
                        /* phys  */ &_prdTablePhysical );

    if ( !_prdTable )
    {
        IOLog("%s: PRD table allocation failed\n", getName());
        return false;
    }

    _DMACursor = IONaturalMemoryCursor::withSpecification(
                          /* max segment size  */ 0x10000,
                          /* max transfer size */ kMaxATAXfer );
    
    if ( !_DMACursor )
    {
        freeDMAChannel();
        IOLog("%s: Memory cursor allocation failed\n", getName());
        return false;
    }

    // fill the chain with stop commands to initialize it.    
    initATADMAChains( _prdTable );

    return true;
}

//---------------------------------------------------------------------------

bool AppleVIAATADriver::freeDMAChannel( void )
{
    if ( _prdTable )
    {
        // make sure the engine is stopped.
        stopDMA();

        // free the descriptor table.
        IOFreeContiguous(_prdTable, sizeof(PRD) * kATAMaxDMADesc);
    }

    return true;
}

//---------------------------------------------------------------------------

void AppleVIAATADriver::initATADMAChains( PRD * descPtr )
{
    UInt32 i;

    /* Initialize the data-transfer PRD channel command descriptors. */

    for (i = 0; i < kATAMaxDMADesc; i++)
    {
        descPtr->bufferPtr = 0;
        descPtr->byteCount = 1;
        descPtr->flags = OSSwapHostToLittleConstInt16( kLast_PRD );
        descPtr++;
    }
}

//---------------------------------------------------------------------------

enum {
    kVIAPowerStateOff = 0,
    kVIAPowerStateDoze,
    kVIAPowerStateOn,
    kVIAPowerStateCount
};

void AppleVIAATADriver::initForPM( IOService * provider )
{
    static const IOPMPowerState powerStates[ kVIAPowerStateCount ] =
    {
        { 1, 0, 0,             0,             0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 0, IOPMSoftSleep, IOPMSoftSleep, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 0, IOPMPowerOn,   IOPMPowerOn,   0, 0, 0, 0, 0, 0, 0, 0 }
    };

    PMinit();

    registerPowerDriver( this, (IOPMPowerState *) powerStates,
                         kVIAPowerStateCount );

    provider->joinPMtree( this );
}

//---------------------------------------------------------------------------

IOReturn AppleVIAATADriver::setPowerState( unsigned long stateIndex,
                                           IOService *   whatDevice )
{
    if ( stateIndex == kVIAPowerStateOff )
    {
        fHardwareLostContext = true;
    }
    else if ( fHardwareLostContext )
    {
        initializeHardware();
        programTimingRegisters();
        fHardwareLostContext = false;
    }

    return IOPMAckImplied;
}

//---------------------------------------------------------------------------

void AppleVIAATADriver::dumpVIARegisters( void )
{
    DEBUG_LOG("VIA_IDE_ENABLE   0x%02x\n",
              fProvider->pciConfigRead8(VIA_IDE_ENABLE));
    DEBUG_LOG("VIA_IDE_CONFIG   0x%02x\n",
              fProvider->pciConfigRead8(VIA_IDE_CONFIG));
    DEBUG_LOG("VIA_ULTRA_TIMING 0x%08x\n",
              fProvider->pciConfigRead32(VIA_ULTRA_TIMING));
    DEBUG_LOG("VIA_FIFO_CONFIG  0x%02x\n",
              fProvider->pciConfigRead8(VIA_FIFO_CONFIG));
    DEBUG_LOG("VIA_MISC_1       0x%02x\n",
              fProvider->pciConfigRead8(VIA_MISC_1));
    DEBUG_LOG("VIA_MISC_2       0x%02x\n",
              fProvider->pciConfigRead8(VIA_MISC_2));
    DEBUG_LOG("VIA_MISC_3       0x%02x\n",
              fProvider->pciConfigRead8(VIA_MISC_3));

    for (int unit = 0; unit < kMaxDriveCount; unit++)
    {
        if (DRIVE_IS_PRESENT(unit) == false) continue;

        DEBUG_LOG("[ %s Ch%ld Drive%ld ]\n",
                  fProvider->getHardwareName(), fChannelNumber, unit);
        DEBUG_LOG("Command Active   %ld ns\n",
                  readTimingIntervalNS(kVIATimingRegCommandActive, unit));
        DEBUG_LOG("Command Recovery %ld ns\n",
                  readTimingIntervalNS(kVIATimingRegCommandRecovery, unit));
        DEBUG_LOG("Address Setup    %ld ns\n",
                  readTimingIntervalNS(kVIATimingRegAddressSetup, unit));

        DEBUG_LOG("Data Active      %ld ns\n",
                  readTimingIntervalNS(kVIATimingRegDataActive, unit));
        DEBUG_LOG("Data Recovery    %ld ns\n",
                  readTimingIntervalNS(kVIATimingRegDataRecovery, unit));

        if (fBusTimings[unit].ultraEnabled)
        {
            DEBUG_LOG("UDMA Timing      0x%02x\n",
                      readTimingRegister(kVIATimingRegUltra, unit));
        }
    }
}
