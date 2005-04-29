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

#include "AppleAMDCS5535ATA.h"
#include <i386/proc_reg.h>

#define kPIOModeMask   ((1 << kPIOModeCount) - 1)
#define kDMAModeMask   ((1 << kDMAModeCount) - 1)
#define kUltraModeMask ((1 << kUltraModeCount) - 1)

#define DRIVE_IS_PRESENT(u) \
        (_devInfo[u].type != kUnknownATADeviceType)

#define TIMING_PARAM_IS_VALID(p) \
        ((p) != 0)

#define CLASS AppleAMDCS5535ATA
#define super AppleOnboardPCATA

OSDefineMetaClassAndStructors( AppleAMDCS5535ATA, AppleOnboardPCATA )

/*---------------------------------------------------------------------------
 *
 * Start the single-channel ATA controller driver.
 *
 ---------------------------------------------------------------------------*/

bool CLASS::start( IOService * provider )
{
    bool superStarted = false;

    DEBUG_LOG("%s: %s( %p, %p )\n", getName(), __FUNCTION__, this, provider);

    if (openATAChannel(provider) == false)
        goto fail;

    // Create a work loop.

    fWorkLoop = IOWorkLoop::workLoop();
    if (fWorkLoop == 0)
    {
        DEBUG_LOG("%s: create work loop failed\n", getName());
        goto fail;
    }

    // CS5535 only has a single ATA channel.

    if (fChannelNumber != kPrimaryChannelID)
    {
        DEBUG_LOG("%s: bad ATA channel number %lu\n", getName(),
                  fChannelNumber);
        goto fail;
    }

    // FIXME: Probe for 80-pin conductors.
    // Where can the driver get this info?

    f80PinCable[0] = true;
    f80PinCable[1] = true;

    // Get the base address for the bus master registers in I/O space.

    if (!getBMBaseAddress(fChannelNumber, &fBMBaseAddr))
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
    resetBusTimings();

    // Now ready to call super::start

    if (super::start(provider) == false)
    {
        goto fail;
    }
    superStarted = true;

    // Create interrupt event source that will signal the
    // work loop (thread) when a device interrupt occurs.

    fInterruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(
                       this, &interruptOccurred, &interruptFilter,
                       fChannelNub, 0 );

    if (!fInterruptSource ||
        (fWorkLoop->addEventSource(fInterruptSource) != kIOReturnSuccess))
    {
        DEBUG_LOG("%s: interrupt event source error\n", getName());
        goto fail;
    }
    fInterruptSource->enable();

    attachATADeviceNubs();

    // Successful start, announce hardware settings.

    IOLog("%s: CMD 0x%lx, CTR 0x%lx, IRQ %ld, BM 0x%x\n",
          getName(),
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

bool CLASS::getBMBaseAddress( UInt32   channel,
                              UInt16 * baseAddr )
{
    UInt32 hi, lo;

    DEBUG_LOG("[CH%lu] %s\n", channel, __FUNCTION__);

    rdmsr(ATAC_IO_BAR, lo, hi);
    if ((lo & BM_IDE_BAR_EN) == 0)
    {
        DEBUG_LOG("  BM_IDE_BAR_EN not set\n");
        return false;
    }

    *baseAddr = (UInt16)(lo & BM_IDE_BAR_MASK);
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
    UInt8 status = *(((CLASS *) owner)->_bmStatusReg);

    if (status & BM_STATUS_INT)
    {
        return true;   // signal the work loop
    }
    else
    {
        //kprintf("Spurious ATAC IRQ %02x\n", status);
        return false;  // ignore the spurious interrupt
    }
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

    me->handleDeviceInterrupt();

    // handleDeviceInterrupt() above will stop the bus-master by clearing
    // the BMControl register. For this controller, driver must clear the
    // interrupt flag AFTER the write to the bus-master control register
    // to avoid generating another interrupt (3741986).

    *(me->_bmStatusReg) = BM_STATUS_INT;
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
    infoOut->setUltraModes( kUltraModeMask );
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

    // Note that we need to report the bitmap of each timing mode,
    // not the mode number.

    if (TIMING_PARAM_IS_VALID(fBusTimings[unit].pioTiming))
    {
        configOut->setPIOMode( 1 << fBusTimings[unit].pioModeNumber );
        configOut->setPIOCycleTime( fBusTimings[unit].pioTiming->cycleTimeNS );
        DEBUG_LOG("  PIO mode %lu @ %u ns\n",
                  fBusTimings[unit].pioModeNumber,
                  fBusTimings[unit].pioTiming->cycleTimeNS);
    }

    if (TIMING_PARAM_IS_VALID(fBusTimings[unit].dmaTiming))
    {
        configOut->setDMAMode( 1 << fBusTimings[unit].dmaModeNumber );
        configOut->setDMACycleTime( fBusTimings[unit].dmaTiming->cycleTimeNS );
        DEBUG_LOG("  DMA mode %lu @ %u ns\n",
                  fBusTimings[unit].dmaModeNumber,
                  fBusTimings[unit].dmaTiming->cycleTimeNS);
    }

    if (TIMING_PARAM_IS_VALID(fBusTimings[unit].ultraTiming))
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

    if (configRequest->getUltraMode() & ~kUltraModeMask)
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

    fBusTimings[unit].pioTiming   = &PIOTimingTable[0];
    fBusTimings[unit].dmaTiming   = 0;
    fBusTimings[unit].ultraTiming = 0;

    if (configRequest->getPIOMode())
    {
        UInt32  pioModeNumber;
        UInt32  pioCycleTime;
        UInt32  pioTimingEntry = 0;

        pioModeNumber = bitSigToNumeric(configRequest->getPIOMode());
        pioModeNumber = min(pioModeNumber, kPIOModeCount - 1);
    
        pioCycleTime = configRequest->getPIOCycleTime();
        pioCycleTime = max(pioCycleTime,
                           PIOTimingTable[pioModeNumber].cycleTimeNS);

        // Look for the fastest entry in the PIOTimingTable with a cycle time
        // which is larger than or equal to pioCycleTime.
    
        for (int i = kPIOModeCount - 1; i > 0; i--)
        {
            if (PIOTimingTable[i].cycleTimeNS >= pioCycleTime)
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
                           DMATimingTable[dmaModeNumber].cycleTimeNS);

        // Look for the fastest entry in the DMATimingTable with a cycle time
        // which is larger than or equal to dmaCycleTime.
    
        for (int i = kDMAModeCount - 1; i > 0; i--)
        {
            if (DMATimingTable[i].cycleTimeNS >= dmaCycleTime)
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
        ultraModeNumber = min(ultraModeNumber, kUltraModeCount - 1);

        // For Ultra DMA mode 3 or higher, 80 pin cable must be present.
        // Otherwise, the drive will be limited to UDMA mode 2.

        if (ultraModeNumber > 2)
        {
            if ( f80PinCable[unit] == false )
            {
                DEBUG_LOG("  80-conductor cable not detected\n");
                ultraModeNumber = 2;
            }
        }

        fBusTimings[unit].ultraTiming = &UltraTimingTable[ultraModeNumber];
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

void CLASS::programTimingRegisters( void )
{
    for (int unit = 0; unit < 2; unit++)
    {
        UInt32 dmaTimingValue = 0;

        if (DRIVE_IS_PRESENT(unit) == false)
            continue;

        if (fBusTimings[unit].ultraTiming)
        {
            dmaTimingValue = fBusTimings[unit].ultraTiming->timingValue;
        }
        else if (fBusTimings[unit].dmaTiming)
        {
            dmaTimingValue = fBusTimings[unit].dmaTiming->timingValue;    
        }

        // Program PIO timing

        wrmsr(ATAC_CH0D0_PIO + (unit * 2), 
              fBusTimings[unit].pioTiming->timingValue, 0);

        // Program DMA timing

        if (dmaTimingValue)
        {
            // Turn on PIO format 1
            dmaTimingValue |= (1 << 31);
            wrmsr(ATAC_CH0D0_DMA + (unit * 2), dmaTimingValue, 0);
        }
        else
        {
            UInt32 dmaLo, dmaHi;
            rdmsr(ATAC_CH0D0_DMA + (unit * 2), dmaLo, dmaHi);
            dmaLo |= (1 << 31);
            wrmsr(ATAC_CH0D0_DMA + (unit * 2), dmaLo, dmaHi);
        }
    }

    dumpHardwareRegisters();
}

void CLASS::restoreHardwareState( void )
{
    programTimingRegisters();
}

/*---------------------------------------------------------------------------
 *
 * Dump ATAC Registers
 *
 ---------------------------------------------------------------------------*/

void CLASS::dumpHardwareRegisters( void )
{
    DEBUG_LOG("ATAC_GLD_MSR_CONFIG  0x%016llx\n",
              rdmsr64(ATAC_GLD_MSR_CONFIG));
    DEBUG_LOG("ATAC_GLD_MSR_SMI     0x%016llx\n",
              rdmsr64(ATAC_GLD_MSR_SMI));
    DEBUG_LOG("ATAC_GLD_MSR_ERROR   0x%016llx\n",
              rdmsr64(ATAC_GLD_MSR_ERROR));
    DEBUG_LOG("ATAC_GLD_MSR_PM      0x%016llx\n",
              rdmsr64(ATAC_GLD_MSR_PM));
    DEBUG_LOG("ATAC_GLD_MSR_DIAG    0x%016llx\n",
              rdmsr64(ATAC_GLD_MSR_DIAG));
    DEBUG_LOG("ATAC_IO_BAR          0x%016llx\n",
              rdmsr64(ATAC_IO_BAR));
    DEBUG_LOG("ATAC_RESET           0x%016llx\n",
              rdmsr64(ATAC_RESET));
    DEBUG_LOG("ATAC_CH0D0_PIO       0x%016llx\n",
              rdmsr64(ATAC_CH0D0_PIO));
    DEBUG_LOG("ATAC_CH0D0_DMA       0x%016llx\n",
              rdmsr64(ATAC_CH0D0_DMA));
    DEBUG_LOG("ATAC_CH0D1_PIO       0x%016llx\n",
              rdmsr64(ATAC_CH0D1_PIO));
    DEBUG_LOG("ATAC_CH0D1_DMA       0x%016llx\n",
              rdmsr64(ATAC_CH0D1_DMA));
}
