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

#include <sys/systm.h>         // snprintf
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMessage.h>
#include "AppleOnboardPCATA.h"

// Increase the PRD table size to one full page or 512 descriptors to allow
// large transfers via the dma engine.

#define kMaxPRDCount           512
#define kMaxPRDSegmentSize     0x10000

// Limited to 2048 ATA sectors per transfer.

#define kMaxATATransferSize   (512 * 2048)

#define CLASS AppleOnboardPCATA
#define super IOPCIATA

OSDefineMetaClassAndAbstractStructors( AppleOnboardPCATA, IOPCIATA )

/*---------------------------------------------------------------------------
 *
 * Start and Stop
 *
 ---------------------------------------------------------------------------*/

bool CLASS::start( IOService * provider )
{
    if (!super::start(provider))
        return false;

    initForPM( provider );

    return true;
}

void CLASS::stop( IOService * provider )
{
    closeATAChannel();
    PMstop();
    super::stop( provider );
}

/*---------------------------------------------------------------------------
 *
 * Open the ATA channel driver.
 *
 ---------------------------------------------------------------------------*/

bool CLASS::openATAChannel( IOService * provider )
{
    // Our provider is a 'nub' that represents a single PCI ATA channel,
    // and not an IOPCIDevice.

    fChannelNub = OSDynamicCast(AppleOnboardPCATAChannel, provider);
    if (fChannelNub == 0)
    {
        DEBUG_LOG("%s: ATA channel type mismatch\n", getName());
        return false;
    }

    // Retain and open our provider.

    fChannelNub->retain();
    if (fChannelNub->open(this) != true)
    {
        DEBUG_LOG("%s: provider open failed\n", getName());
        return false;
    }

    fChannelNumber = fChannelNub->getChannelNumber();

    return true;
}

void CLASS::closeATAChannel( void )
{
    if (fChannelNub)
    {
        fChannelNub->close(this);
    }
}

void CLASS::free( void )
{
    RELEASE( fChannelNub );
    RELEASE( _nub[0]     );
    RELEASE( _nub[1]     );

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

    RELEASE( _bmCommandReg   );
    RELEASE( _bmStatusReg    );
    RELEASE( _bmPRDAddresReg );

    // IOATAController should release this.

    if ( _doubleBuffer.logicalBuffer )
    {
        IOFree((void *)_doubleBuffer.logicalBuffer,
               _doubleBuffer.bufferSize);
        _doubleBuffer.bufferSize     = 0;
        _doubleBuffer.logicalBuffer  = 0;
        _doubleBuffer.physicalBuffer = 0;
    }

    // What about _cmdGate, and _timer in the superclass?

    super::free();
}

/*---------------------------------------------------------------------------
 *
 * Attach a nub for each device on the ATA bus.
 *
 ---------------------------------------------------------------------------*/

void CLASS::attachATADeviceNubs( void )
{
    for (UInt32 i = 0; i < kMaxDriveCount; i++)
    {
        if (_devInfo[i].type != kUnknownATADeviceType)
        {
            ATADeviceNub * nub;

            nub = ATADeviceNub::ataDeviceNub( (IOATAController *) this,
                                              (ataUnitID) i,
                                              _devInfo[i].type );

            if (nub)
            {
                if (_devInfo[i].type == kATAPIDeviceType)
                {
                    // Report less than the full PRD count to handle
                    // any PRD alignment restrictions.

                    nub->setProperty( kIOMaximumSegmentCountReadKey,
                                      kMaxPRDCount / 2, 64 );

                    nub->setProperty( kIOMaximumSegmentCountWriteKey,
                                      kMaxPRDCount / 2, 64 );

                    nub->setProperty( kIOMaximumSegmentByteCountReadKey,
                                      kMaxPRDSegmentSize, 64 );

                    nub->setProperty( kIOMaximumSegmentByteCountWriteKey,
                                      kMaxPRDSegmentSize, 64 );
                }

                if (nub->attach(this))
                {
                    _nub[i] = (IOATADevice *) nub;
                    _nub[i]->retain();
                    _nub[i]->registerService();
                }
                nub->release();
            }
        }
    }
}

/*---------------------------------------------------------------------------
 *
 * Override IOATAController::synchronousIO()
 *
 ---------------------------------------------------------------------------*/

IOReturn CLASS::synchronousIO( void )
{
    IOWorkLoop * myWorkLoop;
    IOReturn     ret;

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

    myWorkLoop = getWorkLoop();

    if (myWorkLoop) myWorkLoop->disableAllInterrupts();
    ret = super::synchronousIO();
    if (myWorkLoop) myWorkLoop->enableAllInterrupts();

    return ret;
}

/*---------------------------------------------------------------------------
 *
 * Setup the location of the task file registers.
 *
 ---------------------------------------------------------------------------*/

bool CLASS::configureTFPointers( void )
{
    DEBUG_LOG("[CH%lu] %s\n", fChannelNumber, __FUNCTION__);

    if (!fChannelNub) return false;

    UInt16 cmdBlockAddr = fChannelNub->getCommandBlockAddress();
    UInt16 ctrBlockAddr = fChannelNub->getControlBlockAddress();

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
 * Extend the implementation of scanForDrives() from IOATAController
 * to issue a soft reset before scanning for ATA/ATAPI drive signatures.
 *
 ---------------------------------------------------------------------------*/

UInt32 CLASS::scanForDrives( void )
{
    UInt32 drivesFound;

    DEBUG_LOG("[CH%lu] %s\n", fChannelNumber, __FUNCTION__);

    *_tfAltSDevCReg = mATADCRReset;

    IODelay( 100 );

    *_tfAltSDevCReg = 0x0;

    IOSleep( 10 );

    drivesFound = super::scanForDrives();

    *_tfSDHReg = 0x00;  // Initialize device selection to device 0.

    return drivesFound;
}

/*---------------------------------------------------------------------------
 *
 * Flush the outstanding commands in the command queue.
 * Implementation borrowed from MacIOATA in IOATAFamily.
 *
 ---------------------------------------------------------------------------*/

IOReturn CLASS::handleQueueFlush( void )
{
    UInt32 savedQstate = _queueState;

    DEBUG_LOG("[CH%lu] %s\n", fChannelNumber, __FUNCTION__);

    _queueState = IOATAController::kQueueLocked;

    IOATABusCommand * cmdPtr = 0;

    while (cmdPtr = dequeueFirstCommand())
    {
        cmdPtr->setResult( kIOReturnError );
        cmdPtr->executeCallback();
    }

    _queueState = savedQstate;

    return kATANoErr;
}

/*---------------------------------------------------------------------------
 *
 * Update PRD Table for the current command.
 *
 ---------------------------------------------------------------------------*/

IOReturn CLASS::createChannelCommands( void )
{
    IOMemoryDescriptor * descriptor = _currentCommand->getBuffer();
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
    IOByteCount transferSize   = 0; 

    // There's a unique problem with pci-style controllers, in that each
    // dma transaction is not allowed to cross a 64K boundary. This leaves
    // us with the yucky task of picking apart any descriptor segments that
    // cross such a boundary ourselves.  

    while ( _DMACursor->getPhysicalSegments(
                           /* descriptor */  descriptor,
                           /* position   */  xfrPosition,
                           /* segments   */  &physSegment,
                           /* max segs   */  1,
                           /* max xfer   */  bytesRemaining,
                           /* xfer size  */  &transferSize) )
    {
        xferDataPtr = (UInt8 *) physSegment.location;
        xferCount   = physSegment.length;

        if ((UInt32) xferDataPtr & 0x03)
        {
            IOLog("%s: DMA buffer %p not 4 byte aligned\n",
                  getName(), xferDataPtr);
            //return kIOReturnNotAligned;        
        }

        if (xferCount & 0x03)
        {
            IOLog("%s: DMA buffer length 0x%lx is odd\n",
                  getName(), xferCount);
        }

        // Update bytes remaining count after this pass.
        bytesRemaining -= xferCount;
        xfrPosition += xferCount;

        // Examine the segment to see whether it crosses (a) 64k boundary(s)
        starting64KBlock = (UInt8 *)((UInt32) xferDataPtr & 0xffff0000);
        ptr2EndData  = xferDataPtr + xferCount;
        next64KBlock = starting64KBlock + 0x10000;

        // Loop until this physical segment is fully accounted for.
        // It is not possible to have a memory descriptor which crosses
        // more than one 64K boundary given the max segment size passed
        // to the memory cursor.

        while (xferCount > 0)
        {
            if (index >= kMaxPRDCount)
            {
                IOLog("%s: PRD table exhausted error\n", getName());
                _dmaState = kATADMAError;
                return -1;
            }

            if (ptr2EndData > next64KBlock)
            {
                count2Next64KBlock = next64KBlock - xferDataPtr;

                setPRD(xferDataPtr, (UInt16)count2Next64KBlock,
                       &_prdTable[index], kContinue_PRD);

                xferDataPtr = next64KBlock;
                next64KBlock += 0x10000;
                xferCount -= count2Next64KBlock;
            }
            else
            {
                setPRD(xferDataPtr, (UInt16) xferCount,
                       &_prdTable[index],
                       (bytesRemaining == 0) ? kLast_PRD : kContinue_PRD);

                xferCount = 0;
            }

            index++;
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

bool CLASS::allocDMAChannel( void )
{
    _prdTable = (PRD *) IOMallocContiguous(
                        /* size  */ sizeof(PRD) * kMaxPRDCount, 
                        /* align */ PAGE_SIZE,
                        /* phys  */ &_prdTablePhysical );

    if (!_prdTable)
    {
        IOLog("%s: PRD table allocation failed\n", getName());
        return false;
    }

    _DMACursor = IONaturalMemoryCursor::withSpecification(
                 /* max segment size  */ kMaxPRDSegmentSize,
                 /* max transfer size */ kMaxATATransferSize );

    if (!_DMACursor)
    {
        freeDMAChannel();
        IOLog("%s: Memory cursor allocation failed\n", getName());
        return false;
    }

    // fill the chain with stop commands to initialize it.    
    initATADMAChains(_prdTable);

    return true;
}

bool CLASS::freeDMAChannel( void )
{
    if (_prdTable)
    {
        // make sure the engine is stopped.
        stopDMA();

        // free the descriptor table.
        IOFreeContiguous(_prdTable, sizeof(PRD) * kMaxPRDCount);
    }

    return true;
}

void CLASS::initATADMAChains( PRD * descPtr )
{
    /* Initialize the data-transfer PRD channel command descriptors. */

    for (int i = 0; i < kMaxPRDCount; i++)
    {
        descPtr->bufferPtr = 0;
        descPtr->byteCount = 1;
        descPtr->flags = OSSwapHostToLittleConstInt16( kLast_PRD );
        descPtr++;
    }
}

/*---------------------------------------------------------------------------
 *
 * Handle termination notification from the provider.
 *
 ---------------------------------------------------------------------------*/

IOReturn CLASS::message( UInt32      type,
                         IOService * provider,
                         void *      argument )
{
    if ((provider == fChannelNub) &&
        (type == kIOMessageServiceIsTerminated))
    {
        fChannelNub->close( this );
        return kIOReturnSuccess;
    }

    return super::message( type, provider, argument );
}

/*---------------------------------------------------------------------------
 *
 * Publish a numeric property pertaining to a drive to the registry.
 *
 ---------------------------------------------------------------------------*/

bool CLASS::publishDriveProperty( UInt32       driveUnit,
                                  const char * propKey,
                                  UInt32       value )
{
    char keyString[40];
    snprintf(keyString, 40, "Drive %ld %s", driveUnit, propKey);    
    return super::setProperty( keyString, value, 32 );
}

/*---------------------------------------------------------------------------
 *
 * Power Management
 *
 ---------------------------------------------------------------------------*/

void CLASS::initForPM( IOService * provider )
{
    static IOPMPowerState powerStates[ kPowerStateCount ] =
    {
        { 1, 0, 0,             0,             0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 0, IOPMSoftSleep, IOPMSoftSleep, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 0, IOPMPowerOn,   IOPMPowerOn,   0, 0, 0, 0, 0, 0, 0, 0 }
    };

    PMinit();

    registerPowerDriver(this, powerStates, kPowerStateCount);

    provider->joinPMtree(this);
}

IOReturn CLASS::setPowerState( unsigned long stateIndex,
                               IOService *   whatDevice )
{
    if (stateIndex == kPowerStateOff)
    {
        fHardwareLostPower = true;
    }
    else if (fHardwareLostPower)
    {
        restoreHardwareState();
        fHardwareLostPower = false;
    }

    return IOPMAckImplied;
}

void CLASS::restoreHardwareState( void )
{
}
