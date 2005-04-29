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

#include <IOKit/pci/IOPCIDevice.h>
#include "AppleNVIDIAnForceATA.h"

#define kPIOModeMask     ((1 << kPIOModeCount) - 1)
#define kDMAModeMask     ((1 << kDMAModeCount) - 1)

#define DRIVE_IS_PRESENT(u) \
        (_devInfo[u].type != kUnknownATADeviceType)

#define TIMING_PARAM_IS_VALID(p) \
        ((p) != 0)

#define CLASS AppleNVIDIAnForceATA
#define super AppleOnboardPCATA

OSDefineMetaClassAndStructors( AppleNVIDIAnForceATA, AppleOnboardPCATA )

static const HardwareInfo * getHardwareInfo( UInt32 pciID )
{
    const HardwareInfo * info = 0;

    static const HardwareInfo hardwareTable[] =
    {
        { 0x01bc10de, 5, "NVIDIA nForce"  },
        { 0x006510de, 6, "NVIDIA nForce2" },
        { 0x00d510de, 6, "NVIDIA nForce3" },
    };

    for (UInt i = 0; i < sizeof(hardwareTable)/sizeof(hardwareTable[0]); i++)
    {
        if (hardwareTable[i].pciDeviceID == pciID)
        {
            info = &hardwareTable[i];
            break;
        }
    }

    return info;
}

/*---------------------------------------------------------------------------
 *
 * Start the single-channel nForce ATA controller driver.
 *
 ---------------------------------------------------------------------------*/

bool CLASS::start( IOService * provider )
{
    bool	superStarted = false;
    UInt8	cableDetect;
    UInt32	udmaTiming;

    DEBUG_LOG("%s: %s( %p, %p )\n", getName(), __FUNCTION__, this, provider);

    if (openATAChannel(provider) == false)
        goto fail;

    // Create a work loop.

    fWorkLoop = IOWorkLoop::workLoop();
    if (fWorkLoop == 0)
    {
        DEBUG_LOG("%s: new work loop failed\n", getName());
        goto fail;
    }

    if (fChannelNumber > kSecondaryChannelID)
    {
        DEBUG_LOG("%s: bad ATA channel number %lu\n", getName(),
                  fChannelNumber);
        goto fail;
    }

    // Determine the type of hardware which will then tell us the highest
    // Ultra-DMA mode supported.

    fHWInfo = getHardwareInfo(
              fChannelNub->pciConfigRead32(kIOPCIConfigVendorID));
    if (!fHWInfo)
    {
        DEBUG_LOG("%s: unsupported hardware\n", getName());
        goto fail;
    }

    fUltraModeMask = (1 << (fHWInfo->maxUltraMode + 1)) - 1;

    // Probe for 80-pin conductors on drive 0 and 1.
    
    cableDetect = fChannelNub->pciConfigRead8(PCI_CABLE_DETECT);
    udmaTiming  = fChannelNub->pciConfigRead32(PCI_ULTRA_TIMING);

    if (cableDetect & (0x3 << (fChannelNumber * 2)))
        f80PinCablePresent = true;

    // If system booted with UDMA modes that require a 80-pin cable,
    // then BIOS must know something we don't.

    if (udmaTiming & (0x0404 << ((1 - fChannelNumber) * 16)))
        f80PinCablePresent = true;

    // Get the base address for the bus master registers in I/O space.

    if (!getBMBaseAddress(&fBMBaseAddr))
    {
        DEBUG_LOG("%s: get bus-master base address failed\n", getName());
        goto fail;
    }

    // Must setup these variables inherited from IOPCIATA before super::start

    _bmCommandReg   = IOATAIOReg8::withAddress(  fBMBaseAddr + BM_COMMAND );
    _bmStatusReg    = IOATAIOReg8::withAddress(  fBMBaseAddr + BM_STATUS );
    _bmPRDAddresReg = IOATAIOReg32::withAddress( fBMBaseAddr + BM_PRD_PTR );

    // Reset bus timings for both drives.

    dumpHardwareRegisters();
    initializeHardware();
    resetBusTimings();

    // Now we are ready to call super::start

    if (super::start(provider) == false)
    {
        goto fail;
    }
    superStarted = true;

    // Create interrupt event source that will signal the
    // work loop (thread) when a device interrupt occurs.

    if ( fChannelNub->getInterruptVector() == 14 ||
         fChannelNub->getInterruptVector() == 15 )
    {
        // ISA IRQ are never shared, no need for an interrupt filter.
        fInterruptSource = IOInterruptEventSource::interruptEventSource(
                           this, &interruptOccurred,
                           fChannelNub, 0 );
    }
    else
    {
        fInterruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(
                           this, &interruptOccurred, &interruptFilter,
                           fChannelNub, 0 );
    }

    if (!fInterruptSource ||
        (fWorkLoop->addEventSource(fInterruptSource) != kIOReturnSuccess))
    {
        DEBUG_LOG("%s: interrupt event source error\n", getName());
        goto fail;
    }
    fInterruptSource->enable();

    attachATADeviceNubs();

    // Successful start, announce hardware settings.

    IOLog("%s: %s (CMD 0x%lx, CTR 0x%lx, IRQ %ld, BM 0x%x)\n",
          getName(),
          fHWInfo->deviceName,
          fChannelNub->getCommandBlockAddress(),
          fChannelNub->getControlBlockAddress(),
          fChannelNub->getInterruptVector(),
          fBMBaseAddr);

    return true;

fail:
    if (fChannelNub)
        closeATAChannel();

    if (superStarted)
        super::stop( provider );

    return false;
}

/*---------------------------------------------------------------------------
 *
 * Release resources before the driver is unloaded.
 *
 ---------------------------------------------------------------------------*/

void CLASS::free( void )
{
    DEBUG_LOG("%s::%s( %p )\n", getName(), __FUNCTION__, this);

    if (fInterruptSource && fWorkLoop)
    {
        fWorkLoop->removeEventSource(fInterruptSource);
    }

    RELEASE( fInterruptSource );
    RELEASE( fWorkLoop        );

    super::free();
}

/*---------------------------------------------------------------------------
 *
 * Return the driver's work loop
 *
 ---------------------------------------------------------------------------*/

IOWorkLoop * CLASS::getWorkLoop( void ) const
{
    return fWorkLoop;
}

/*---------------------------------------------------------------------------
 *
 * Determine the start of the I/O mapped Bus-Master registers.
 *
 ---------------------------------------------------------------------------*/

bool CLASS::getBMBaseAddress( UInt16 * baseAddr )
{
    UInt32 bmiba;

    DEBUG_LOG("[CH%lu] %s\n", fChannelNumber, __FUNCTION__);

    bmiba = fChannelNub->pciConfigRead32( kIOPCIConfigBaseAddress4 );

    if ((bmiba & 0x01) == 0)
    {
        DEBUG_LOG("  PCI BAR 0x20 (0x%08lx) is not an I/O range\n", bmiba);
        return false;
    }

    bmiba &= BM_ADDR_MASK;  // get the address portion
    if (bmiba == 0)
    {
        DEBUG_LOG("  BMIBA is zero\n");
        return false;
    }

    if (fChannelNumber == kSecondaryChannelID)
        bmiba += BM_SEC_OFFSET;

    *baseAddr = (UInt16) bmiba;
    DEBUG_LOG("  BMBaseAddr = %04x\n", *baseAddr);
    return true;
}

/*---------------------------------------------------------------------------
 *
 * Reset all timing registers to the slowest (most compatible) timing.
 * DMA modes are disabled.
 *
 ---------------------------------------------------------------------------*/

void CLASS::resetBusTimings( void )
{
    DEBUG_LOG("[CH%lu] %s\n", fChannelNumber, __FUNCTION__);

    memset(&fBusTimings[0], 0, sizeof(fBusTimings));

    fBusTimings[0].pioTiming = &PIOTimingTable[0];
    fBusTimings[1].pioTiming = &PIOTimingTable[0];

    programTimingRegisters();
}

/*---------------------------------------------------------------------------
 *
 * Filter interrupts that are not originated by our hardware. This will help
 * prevent waking up our work loop thread when sharing a interrupt line with
 * another driver.
 *
 ---------------------------------------------------------------------------*/

bool CLASS::interruptFilter( OSObject * owner,
                             IOFilterInterruptEventSource * src )
{
    CLASS * driver = (CLASS *) owner;

    if (*(driver->_bmStatusReg) & BM_STATUS_INT)
        return true;   // signal the work loop
    else
        return false;  // ignore the spurious interrupt
}

/*---------------------------------------------------------------------------
 *
 * The work loop based interrupt handler called by our interrupt event
 * source.
 *
 ---------------------------------------------------------------------------*/

void CLASS::interruptOccurred( OSObject *               owner,
                               IOInterruptEventSource * source,
                               int                      count )
{
    CLASS * me = (CLASS *) owner;

    // Clear interrupt latch

    *(me->_bmStatusReg) = BM_STATUS_INT;

    me->handleDeviceInterrupt();
}

/*---------------------------------------------------------------------------
 *
 * Provide information on the ATA bus capability.
 *
 ---------------------------------------------------------------------------*/

IOReturn CLASS::provideBusInfo( IOATABusInfo * infoOut )
{
    DEBUG_LOG("[CH%lu] %s( %p )\n", fChannelNumber, __FUNCTION__, infoOut);

    if (infoOut == 0)
    {
        return -1;
    }

    infoOut->zeroData();
    infoOut->setSocketType( kInternalATASocket );
    infoOut->setPIOModes( kPIOModeMask );
    infoOut->setDMAModes( kDMAModeMask );
    infoOut->setUltraModes( fUltraModeMask );
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

IOReturn CLASS::getConfig( IOATADevConfig * configOut,
                           UInt32           unit )
{
    DEBUG_LOG("[CH%lu D%lu] %s( %p )\n", fChannelNumber, unit, __FUNCTION__,
              configOut);

    if ((configOut == 0) || (unit > kATADevice1DeviceID))
    {
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
        configOut->setPIOCycleTime( fBusTimings[unit].pioTiming->cycleTime );
        DEBUG_LOG("  PIO mode %lu @ %u ns\n",
                  fBusTimings[unit].pioModeNumber,
                  fBusTimings[unit].pioTiming->cycleTime);
    }

    if (TIMING_PARAM_IS_VALID(fBusTimings[unit].dmaTiming))
    {
        configOut->setDMAMode( 1 << fBusTimings[unit].dmaModeNumber );
        configOut->setDMACycleTime( fBusTimings[unit].dmaTiming->cycleTime );
        DEBUG_LOG("  DMA mode %lu @ %u ns\n",
                  fBusTimings[unit].dmaModeNumber,
                  fBusTimings[unit].dmaTiming->cycleTime);
    }

    if (fBusTimings[unit].ultraEnabled)
    {
        configOut->setUltraMode( 1 << fBusTimings[unit].ultraModeNumber );
        DEBUG_LOG("  Ultra mode %lu\n", fBusTimings[unit].ultraModeNumber);
    }

    configOut->setPacketConfig( _devInfo[unit].packetSend );

    return kATANoErr;
}

/*---------------------------------------------------------------------------
 *
 * Select the bus timings for a given drive unit.
 *
 ---------------------------------------------------------------------------*/

IOReturn CLASS::selectConfig( IOATADevConfig * configRequest,
                              UInt32           unit )
{
    DEBUG_LOG("[CH%lu D%lu] %s( %p )\n", fChannelNumber, unit, __FUNCTION__,
              configRequest);

    if ((configRequest == 0) || (unit > kATADevice1DeviceID))
    {
        return -1;
    }

    // All config requests must include a supported PIO mode

    if ((configRequest->getPIOMode() & kPIOModeMask) == 0)
    {
        DEBUG_LOG("  missing PIO mode\n");
        return kATAModeNotSupported;
    }

    if (configRequest->getDMAMode() & ~kDMAModeMask)
    {
        DEBUG_LOG("  DMA mode not supported\n");
        return kATAModeNotSupported;
    }

    if (configRequest->getUltraMode() & ~fUltraModeMask)
    {
        DEBUG_LOG("  Ultra DMA mode not supported\n");
        return kATAModeNotSupported;
    }

    if (configRequest->getDMAMode() && configRequest->getUltraMode())
    {
        DEBUG_LOG("  multiple DMA mode selection error\n");
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

void CLASS::selectTimingParameter( IOATADevConfig * configRequest,
                                   UInt32           unit )
{
    DEBUG_LOG("[CH%lu D%lu] %s( %p )\n", fChannelNumber, unit, __FUNCTION__,
              configRequest);

    // Reset existing parameters for this unit.

    fBusTimings[unit].pioTiming = &PIOTimingTable[0];
    fBusTimings[unit].dmaTiming = 0;
    fBusTimings[unit].ultraEnabled = false;

    if (configRequest->getPIOMode())
    {
        UInt32  pioModeNumber;
        UInt32  pioCycleTime;
        UInt32  pioTimingEntry = 0;

        pioModeNumber = bitSigToNumeric( configRequest->getPIOMode() );
        pioModeNumber = min(pioModeNumber, kPIOModeCount - 1);

        // Use a default cycle time if the device didn't report a time to use.

        pioCycleTime = configRequest->getPIOCycleTime();
        pioCycleTime = max(pioCycleTime,
                           PIOTimingTable[pioModeNumber].cycleTime);

        // Look for the fastest entry in the PIOMinCycleTime with a cycle time
        // which is larger than or equal to pioCycleTime.

        for (int i = kPIOModeCount - 1; i > 0; i--)
        {
            if (PIOTimingTable[i].cycleTime >= pioCycleTime)
            {
                pioTimingEntry = i;
                break;
            }
        }

        fBusTimings[unit].pioTiming = &PIOTimingTable[pioTimingEntry];
        fBusTimings[unit].pioModeNumber = pioModeNumber;
        DEBUG_LOG("  selected PIO timing entry %d\n", pioTimingEntry);
        publishDriveProperty(unit, kSelectedPIOModeKey, pioModeNumber);
    }

    if (configRequest->getDMAMode())
    {
        UInt32  dmaModeNumber;
        UInt32  dmaCycleTime;
        UInt32  dmaTimingEntry = 0;

        dmaModeNumber = bitSigToNumeric(configRequest->getDMAMode());
        dmaModeNumber = min(dmaModeNumber, kDMAModeCount - 1);

        dmaCycleTime = configRequest->getDMACycleTime();
        dmaCycleTime = max(dmaCycleTime,
                           DMATimingTable[dmaModeNumber].cycleTime);

        // Look for the fastest entry in the DMAMinCycleTime with a cycle time
        // which is larger than or equal to dmaCycleTime.
    
        for (int i = kDMAModeCount - 1; i > 0; i--)
        {
            if (DMATimingTable[i].cycleTime >= dmaCycleTime)
            {
                dmaTimingEntry = i;
                break;
            }
        }
        
        fBusTimings[unit].dmaTiming = &DMATimingTable[dmaTimingEntry];
        fBusTimings[unit].dmaModeNumber = dmaModeNumber;
        DEBUG_LOG("  selected DMA timing entry %d\n", dmaTimingEntry);
        publishDriveProperty(unit, kSelectedDMAModeKey, dmaModeNumber);
    }

    if (configRequest->getUltraMode())
    {
        UInt32  ultraModeNumber;

        ultraModeNumber = bitSigToNumeric(configRequest->getUltraMode());
        ultraModeNumber = min(ultraModeNumber, fHWInfo->maxUltraMode);

        // For Ultra DMA mode 3 or higher, 80 pin cable must be present.
        // Otherwise, the drive will be limited to UDMA mode 2.

        if (ultraModeNumber > 2)
        {
            if ( f80PinCablePresent == false )
            {
                ERROR_LOG("%s: 80-conductor cable not detected on channel %u\n",
                    getName(), fChannelNumber);
                ultraModeNumber = 2;
            }
        }

        fBusTimings[unit].ultraEnabled = true;
        fBusTimings[unit].ultraModeNumber = ultraModeNumber;
        DEBUG_LOG("  selected Ultra mode %d\n", ultraModeNumber);
        publishDriveProperty(unit, kSelectedUltraDMAModeKey, ultraModeNumber);
    }

    programTimingRegisters();
}

/*---------------------------------------------------------------------------
 *
 * Program timing registers for both drives.
 *
 ---------------------------------------------------------------------------*/


static void mergeTimings( TimingParameter *       dst,
                          const TimingParameter * src )
{
    if (TIMING_PARAM_IS_VALID(dst) == false ||
        TIMING_PARAM_IS_VALID(src) == false)
        return;

    dst->cycleTime    = max(dst->cycleTime, src->cycleTime);
    dst->setupTime    = max(dst->setupTime, src->setupTime);
    dst->activeTime   = max(dst->activeTime, src->activeTime);
    dst->recoveryTime = max(dst->recoveryTime, src->recoveryTime);
}

void CLASS::programTimingRegisters( void )
{
    TimingParameter  timingCommand;  // shared between both drives
    TimingParameter  timingData[2];

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

        writeTimingIntervalNS( kTimingRegCommandActive,
                               unit, timingCommand.activeTime );

        writeTimingIntervalNS( kTimingRegCommandRecovery,
                               unit, timingCommand.recoveryTime );

        writeTimingIntervalNS( kTimingRegAddressSetup,
                               unit, timingData[unit].setupTime );

        writeTimingIntervalNS( kTimingRegDataActive,
                               unit, timingData[unit].activeTime );

        writeTimingIntervalNS( kTimingRegDataRecovery,
                               unit, timingData[unit].recoveryTime );

        if (fBusTimings[unit].ultraEnabled)
        {
            UInt8 mode = fBusTimings[unit].ultraModeNumber;
            writeTimingRegister( kTimingRegUltra, unit,
                                 UltraTimingTable[mode]); 
        }
        else
        {
            writeTimingRegister( kTimingRegUltra, unit, 0x8b ); 
        }
    }

    dumpHardwareRegisters();
}

/*---------------------------------------------------------------------------
 *
 * Read and write timing registers.
 *
 ---------------------------------------------------------------------------*/

void CLASS::writeTimingIntervalNS( TimingReg reg, UInt32 drive, UInt32 timeNS )
{
    const UInt32 clockPeriodPS = 30000; // 30ns @ 33MHz PCI
    UInt32 clocks = ((timeNS * 1000) + clockPeriodPS - 1) / clockPeriodPS;
    UInt32 shifts = TimingRegInfo[reg].bitnum;

    if (reg == kTimingRegAddressSetup)
        shifts -= ((drive << 1) + (fChannelNumber << 2));

    clocks =  min(clocks, TimingRegInfo[reg].maxValue);
    clocks =  max(clocks, TimingRegInfo[reg].minValue);
    clocks -= TimingRegInfo[reg].minValue;
    clocks &= TimingRegInfo[reg].bitmask;
    clocks <<= shifts;

    fChannelNub->pciConfigWrite8(TimingRegOffset[reg][fChannelNumber][drive],
                                 clocks, TimingRegInfo[reg].bitmask << shifts);

    DEBUG_LOG("%s: CH%d DRV%d wrote 0x%02x to offset 0x%02x\n",
              getName(), fChannelNumber, drive, clocks,
              TimingRegOffset[reg][fChannelNumber][drive]);
}

void CLASS::writeTimingRegister( TimingReg reg, UInt32 drive, UInt8 value )
{
    fChannelNub->pciConfigWrite8(TimingRegOffset[reg][fChannelNumber][drive],
                                 value, 0xFF);

    DEBUG_LOG("%s: CH%d DRV%d wrote 0x%02x to offset 0x%02x\n",
              getName(), fChannelNumber, drive, value,
              TimingRegOffset[reg][fChannelNumber][drive]);
}

UInt32 CLASS::readTimingIntervalNS( TimingReg reg, UInt32 drive )
{
    UInt32 timeNS;
    UInt32 shifts = TimingRegInfo[reg].bitnum;

    if (reg == kTimingRegAddressSetup)
        shifts -= ((drive << 1) + (fChannelNumber << 2));

    timeNS = readTimingRegister( reg, drive );
    timeNS >>= shifts;
    timeNS &= TimingRegInfo[reg].bitmask;
    timeNS += TimingRegInfo[reg].minValue;
    timeNS *= 30;

    return timeNS;
}

UInt8 CLASS::readTimingRegister( TimingReg reg, UInt32 drive )
{
    return fChannelNub->pciConfigRead8(
                        TimingRegOffset[reg][fChannelNumber][drive]);
}

/*---------------------------------------------------------------------------
 *
 * Hardware initialization.
 *
 ---------------------------------------------------------------------------*/

void CLASS::initializeHardware( void )
{
    // Turn on prefetch and post write buffers for both primary and
    // secondary channels.

    fChannelNub->pciConfigWrite8( PCI_IDE_CONFIG, 0xF0, 0xF0 );
}

//---------------------------------------------------------------------------

void CLASS::restoreHardwareState( void )
{
    initializeHardware();
    programTimingRegisters();
}

//---------------------------------------------------------------------------

void CLASS::dumpHardwareRegisters( void )
{
    DEBUG_LOG("PCI_IDE_ENABLE   0x%02x\n",
              fChannelNub->pciConfigRead8(PCI_IDE_ENABLE));
    DEBUG_LOG("PCI_IDE_CONFIG   0x%02x\n",
              fChannelNub->pciConfigRead8(PCI_IDE_CONFIG));
    DEBUG_LOG("PCI_CABLE_DETECT 0x%02x\n",
              fChannelNub->pciConfigRead8(PCI_CABLE_DETECT));
    DEBUG_LOG("PCI_FIFO_CONFIG  0x%02x\n",
              fChannelNub->pciConfigRead8(PCI_FIFO_CONFIG));

    for (int unit = 0; unit < kMaxDriveCount; unit++)
    {
        if (DRIVE_IS_PRESENT(unit) == false) continue;

        DEBUG_LOG("[ Ch%ld Drive%ld ]\n",
                  fChannelNumber, unit);
        DEBUG_LOG("Command Active   %ld ns\n",
                  readTimingIntervalNS(kTimingRegCommandActive, unit));
        DEBUG_LOG("Command Recovery %ld ns\n",
                  readTimingIntervalNS(kTimingRegCommandRecovery, unit));
        DEBUG_LOG("Address Setup    %ld ns\n",
                  readTimingIntervalNS(kTimingRegAddressSetup, unit));
        DEBUG_LOG("Data Active      %ld ns\n",
                  readTimingIntervalNS(kTimingRegDataActive, unit));
        DEBUG_LOG("Data Recovery    %ld ns\n",
                  readTimingIntervalNS(kTimingRegDataRecovery, unit));

        if (fBusTimings[unit].ultraEnabled)
        {
            DEBUG_LOG("UDMA Timing      0x%02x\n",
                      readTimingRegister(kTimingRegUltra, unit));
        }
    }
}
