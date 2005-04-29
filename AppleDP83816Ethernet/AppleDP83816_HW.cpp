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

#include "AppleDP83816.h"

#define CLASS AppleDP83816Ethernet
#define super IOEthernetController

#pragma mark -
#pragma mark ••• Hardware Probe •••
#pragma mark -

//---------------------------------------------------------------------------

bool CLASS::probeHardware( void )
{
    // Cache the initial hardware settings auto-loaded from
    // the EEPROM on hardware reset.

    fRegSRR  = ReadReg(SRR) & SRR_REV_MASK;
    fRegCFG  = ReadReg(CFG);
    DEBUG_LOG("Initial SRR = %lx, CFG = %lx\n", fRegSRR, fRegCFG);

    getMacAddress();

    return true;
}

//---------------------------------------------------------------------------

void CLASS::getMacAddress( void )
{
    union {
        UInt8  bytes[4];
        UInt32 dword;
    } data;

    WriteRegFlush( RFCR, RFCR_RFADDR_PM_1_0 );
    data.dword = ReadReg( RFDR );
    fMacAddress.bytes[0] = data.bytes[0];
    fMacAddress.bytes[1] = data.bytes[1];

    WriteRegFlush( RFCR, RFCR_RFADDR_PM_3_2 );
    data.dword = ReadReg( RFDR );
    fMacAddress.bytes[2] = data.bytes[0];
    fMacAddress.bytes[3] = data.bytes[1];

    WriteRegFlush( RFCR, RFCR_RFADDR_PM_5_4 );
    data.dword = ReadReg( RFDR );
    fMacAddress.bytes[4] = data.bytes[0];
    fMacAddress.bytes[5] = data.bytes[1];

    DEBUG_LOG("MAC address = %02x:%02x:%02x:%02x:%02x:%02x\n",
              fMacAddress.bytes[0], fMacAddress.bytes[1],
              fMacAddress.bytes[2], fMacAddress.bytes[3],
              fMacAddress.bytes[4], fMacAddress.bytes[5]);
}

#pragma mark -
#pragma mark ••• Initialization •••
#pragma mark -

//---------------------------------------------------------------------------

void CLASS::setMacAddress( void )
{
    union {
        UInt8  bytes[4];
        UInt32 dword;
    } data;

    data.dword = 0;

    WriteRegFlush( RFCR, RFCR_RFADDR_PM_1_0 );
    data.bytes[0] = fMacAddress.bytes[0];
    data.bytes[1] = fMacAddress.bytes[1];
    WriteRegFlush( RFDR, data.dword );

    WriteRegFlush( RFCR, RFCR_RFADDR_PM_3_2 );
    data.bytes[0] = fMacAddress.bytes[2];
    data.bytes[1] = fMacAddress.bytes[3];
    WriteRegFlush( RFDR, data.dword );

    WriteRegFlush( RFCR, RFCR_RFADDR_PM_5_4 );
    data.bytes[0] = fMacAddress.bytes[4];
    data.bytes[1] = fMacAddress.bytes[5];
    WriteRegFlush( RFDR, data.dword );
}

//---------------------------------------------------------------------------

bool CLASS::swInit( void )
{
    if (!allocateRxMemory())
        return false;

    if (!allocateTxMemory())
        return false;

    if (!initRxRing())
        return false;

    if (!initTxRing())
        return false;

    return true;
}

//---------------------------------------------------------------------------

void CLASS::swFree( void )
{
    releaseRxMemory();
    releaseTxMemory();
}

//---------------------------------------------------------------------------

bool CLASS::hwReset( void )
{
    int wait;

    WriteReg(CR, CR_RST);
    for (wait = kHardwareWaitTimeout; wait; wait--)
    {
        IOSleep(kHardwareWaitValue);
        if ((ReadReg(CR) & CR_RST) == 0)
        {
            DEBUG_LOG("reset complete\n");
            break;
        }
    }

    if (!wait)
    {
        ERROR_LOG("%s: %s TIMEOUT\n", getName(), __FUNCTION__);
        return false;
    }

    return true;
}

//---------------------------------------------------------------------------

void CLASS::hwInit( void )
{
    fFullDuplexMode = false;

    hwStop();

    // Disable all wakeup sources

    WriteReg(WCSR, 0);

    // Program CFG

    fRegCFG |= CFG_PINT_ACEN;
    fRegCFG &= ~(CFG_EUPHCOMP | CFG_PESEL | CFG_BEM | CFG_PHY_DIS);
    WriteRegFlush(CFG, fRegCFG);

    // Reset internal PHY

    WriteRegFlush(CFG, fRegCFG | CFG_PHY_RST);
    IOSleep(100);
    WriteRegFlush(CFG, fRegCFG);

    // Program descriptor pointers

    WriteReg( RXDP, fRxDescPhysAddr );
    WriteReg( TXDP, fTxDescPhysAddr );

    // MAC Tx and Rx configuration

    fRegTXCFG = TXCFG_ATP | TXCFG_MXDMA_256
              | ((512/32) << TXCFG_FLTH_SHIFT)
              | (( 64/32) << TXCFG_DRTH_SHIFT);
    WriteReg(TXCFG, fRegTXCFG);

    fRegRXCFG = RXCFG_MXDMA_256 | ((128/8) << RXCFG_DRTH_SHIFT);
    WriteReg(RXCFG, fRegRXCFG);

    // Clear all statistics counters

    WriteReg(MIBC, MIBC_ACLR);

    // Program receive filters

    setMacAddress();

    fRegRFCR = RFCR_RFEN | RFCR_AAB | RFCR_APM;
    WriteReg(RFCR, fRegRFCR);

    // Program interrupt holdoff (100us units)

    WriteReg(IHR, 3);
}

//---------------------------------------------------------------------------

void CLASS::updateMACForDuplexModeChange( void )
{
    fRegTXCFG &= ~(TXCFG_CSI | TXCFG_HBI);
    fRegRXCFG &= ~RXCFG_ATX;

    if (fFullDuplexMode)
    {
        fRegTXCFG |= (TXCFG_CSI | TXCFG_HBI);
        fRegRXCFG |= RXCFG_ATX;
        DEBUG_LOG("MAC full-duplex mode\n");
    }

    WriteReg(TXCFG, fRegTXCFG);
    WriteReg(RXCFG, fRegRXCFG);
}

//---------------------------------------------------------------------------

void CLASS::hwStop( void )
{
    int wait;

    disableHardwareInterrupts();

    WriteReg(CR, CR_RXD | CR_TXD);
    for (wait = kHardwareWaitTimeout; wait; wait--)
    {
        IOSleep(kHardwareWaitValue);
        if ((ReadReg(CR) & (CR_RXE | CR_TXE)) == 0)
            break;
    }

    if (!wait)
    {
        ERROR_LOG("%s: %s TIMEOUT\n", getName(), __FUNCTION__);
    }
}

//---------------------------------------------------------------------------

void CLASS::hwSetMagicPacketEnable( bool enable )
{
    if (enable)
    {
        // Enable Magic Packet support by placing receiver in
        // "silent mode" as described in the DP83816 doc.
        // We assume the receiver has already been disabled.

        WriteReg(RXDP, 0);
        WriteReg(CR, CR_RXE);

        // Setup receive filters

        WriteReg(RFCR, RFCR_RFEN | RFCR_AAB | RFCR_APM | RFCR_AAU);

        // Enable Magic Packet as wakeup source
        
        WriteRegFlush(WCSR, WCSR_WKMAG);
        
        DEBUG_LOG("Magic Packet enabled\n");
    }
    else
    {
        WriteReg(RXDP, 0);
        WriteReg(CR, CR_RXE);
        WriteRegFlush(WCSR, 0);
    }
}

#pragma mark -
#pragma mark ••• Receive •••
#pragma mark -

//---------------------------------------------------------------------------

bool CLASS::allocateRxMemory( void )
{
    fRxDescMemory = IOBufferMemoryDescriptor::withOptions(
                    kIOMemoryPhysicallyContiguous,
                    kRxDescCount * sizeof(RxDesc),
                    kDescAlignment );

    if (!fRxDescMemory)
    {
        ERROR_LOG("%s: No memory for receive descriptors\n", getName());
        return false;
    }

    if (fRxDescMemory->prepare() != kIOReturnSuccess)
    {
        fRxDescMemory->release();
        fRxDescMemory = 0;
        return false;
    }

    fRxDescPhysAddr = fRxDescMemory->getPhysicalSegment(0, 0);
    if (fRxDescPhysAddr == 0)
    {
        return false;
    }

    fRxDescBase = (RxDesc *) fRxDescMemory->getBytesNoCopy();
    DEBUG_LOG("RX DESC Base V=%p P=0x%lx, count %lu\n",
              fRxDescBase, fRxDescPhysAddr, kRxDescCount);

    memset(fRxDescBase, 0, kRxDescCount * sizeof(RxDesc));

    return true;
}

//---------------------------------------------------------------------------

void CLASS::releaseRxMemory( void )
{
    if (fRxDescMemory)
    {
        for (UInt32 i = 0; i < kRxDescCount; i++)
        {
            if (fRxDescBase[i].packet)
                freePacket(fRxDescBase[i].packet);
        }

        memset(fRxDescBase, 0, kRxDescCount * sizeof(RxDesc));
        fRxDescMemory->complete();
        fRxDescMemory->release();
        fRxDescMemory   = 0;
        fRxDescBase     = 0;
        fRxDescPhysAddr = 0;
    }
}

//---------------------------------------------------------------------------

bool CLASS::initRxRing( void )
{
    IOPhysicalSegment vector;
    UInt              segCount;    
    IOPhysicalAddress physAddr;
    RxDesc *          descPtr;
    UInt32            cmdStatus;

    // Link the descriptors into a ring.

    for (int i = kRxDescCount - 1 ;i > 0; i--)
    {
        physAddr = fRxDescMemory->getPhysicalSegment(sizeof(RxDesc) * i, 0);
        if (physAddr == 0)
            return false;

        fRxDescBase[i-1].link = physAddr;
    }

    // Wrap the ring - last descriptor points to the first

    fRxDescBase[kRxDescCount - 1].link = fRxDescPhysAddr;

    // Attach cluster mbufs to each receive descriptor.

    for (UInt32 i = 0 ;i < kRxDescCount; i++)
    {
        descPtr = &fRxDescBase[i];

        if (descPtr->packet == 0)
            descPtr->packet = allocatePacket( kRxMaxBufferSize );
        if (descPtr->packet == 0)
            return false;

        segCount = fRxMbufCursor->getPhysicalSegments(
                                  descPtr->packet, &vector, 1);
        if (segCount != 1)
            return false;

        cmdStatus = kDescInterrupt | kDescIncludeCRC |
                    (kRxMaxBufferSize & kDescBufferSizeMask);

        descPtr->bufferPtr = vector.location;
        OSWriteLittleInt32(&descPtr->cmdStatus, 0, cmdStatus);
    }

    fRxHeadIndex = 0;

    return true;
}

//---------------------------------------------------------------------------

#define MBUF_PADDR(m) \
        ((IOPhysicalAddress) mbuf_data_to_physical(mbuf_data(m)))

void CLASS::serviceRxInterrupt(void)
{
    mbuf_t        pkt;
    UInt32        rxStatus;
    UInt32        rxLength;
    UInt32        rxIndex = fRxHeadIndex;
    RxDesc *      descPtr = &fRxDescBase[ rxIndex ];
    bool          pktReplaced;

    rxStatus = OSReadLittleInt32(&descPtr->cmdStatus, 0);

    while (rxStatus & kDescOwn)
    {
        DEBUG_LOG("RX Status = %lx @ index %lu\n", rxStatus, rxIndex);

        // Total packet length, including the 4-byte FCS?

        rxLength = (rxStatus & kDescBufferSizeMask);

        // Packet must be contained within a single descriptor,
        // and must be a good packet.

        if (((rxStatus & (kDescMore | kDescPacketOK)) == kDescPacketOK) &&
            (rxLength <= kIOEthernetMaxPacketSize + 4))
        {
            pkt = replaceOrCopyPacket(
                  &descPtr->packet, rxLength, &pktReplaced);
            if (pkt)
            {
                if (pktReplaced)
                {
                    // New packet added to ring, update descriptor
                    // with new physical address.

                    OSWriteLittleInt32(&descPtr->bufferPtr, 0, 
                                       MBUF_PADDR(descPtr->packet));
                    assert(descPtr->bufferPtr != 0);
                }

                fNetif->inputPacket(
                        pkt, rxLength,
                        IONetworkInterface::kInputOptionQueuePacket );

                NET_STAT(inputPackets, 1);
            }
            else /* !pkt (mbuf shortage) */
            {
                NET_STAT(inputErrors, 1);
                ETH_STAT(dot3RxExtraEntry.resourceErrors, 1);
                DEBUG_LOG("RX RESOURCE ERROR\n");
            }
        }
        else
        {
            recordRxDescriptorErrors(rxStatus);
        }

        // Update the current descriptor and make it owned by NIC.

        rxStatus = kDescInterrupt | kDescIncludeCRC |
                   (kRxMaxBufferSize & kDescBufferSizeMask);

        OSWriteLittleInt32(&descPtr->cmdStatus, 0, rxStatus);

        // Advance to next descriptor.

        rxIndex  = (rxIndex + 1) & (kRxDescCount - 1);
        descPtr  = &fRxDescBase[rxIndex];
        rxStatus = OSReadLittleInt32(&descPtr->cmdStatus, 0);
    }

    fRxHeadIndex = rxIndex;

    // Push up all packets received in the loop above in one shot.

    fNetif->flushInputQueue();
}

//---------------------------------------------------------------------------

void CLASS::recordRxDescriptorErrors( UInt32 rxStatus )
{
    DEBUG_LOG("RX PACKET ERROR %08lx\n", rxStatus);

    NET_STAT(inputErrors, 1);
    if (rxStatus & kDescRxOverrun)
        ETH_STAT(dot3RxExtraEntry.overruns, 1);
    if (rxStatus & kDescTooLong)
        ETH_STAT(dot3StatsEntry.frameTooLongs, 1);
    if (rxStatus & kDescRuntFrame)
        ETH_STAT(dot3RxExtraEntry.frameTooShorts, 1);
    if (rxStatus & kDescSymbolError)
        ETH_STAT(dot3RxExtraEntry.phyErrors, 1);
    if (rxStatus & kDescFCSError)
        ETH_STAT(dot3StatsEntry.fcsErrors, 1);
    if (rxStatus & kDescRxCollision)
        ETH_STAT(dot3RxExtraEntry.collisionErrors, 1);
}

#pragma mark -
#pragma mark ••• Transmit •••
#pragma mark -

//---------------------------------------------------------------------------

bool CLASS::allocateTxMemory( void )
{
    fTxDescMemory = IOBufferMemoryDescriptor::withOptions(
                    kIOMemoryPhysicallyContiguous,
                    kTxDescCount * sizeof(TxDesc),
                    kDescAlignment);

    if (!fTxDescMemory)
    {
        ERROR_LOG("%s: No memory for transmit descriptors\n", getName());
        return false;
    }

    if (fTxDescMemory->prepare() != kIOReturnSuccess)
    {
        fTxDescMemory->release();
        fTxDescMemory = 0;
        return false;
    }

    fTxDescPhysAddr = fTxDescMemory->getPhysicalSegment(0, 0);
    if (fTxDescPhysAddr == 0)
    {
        return false;
    }

    fTxDescBase = (TxDesc *) fTxDescMemory->getBytesNoCopy();
    DEBUG_LOG("TX DESC Base V=%p P=0x%lx, count %lu\n",
              fTxDescBase, fTxDescPhysAddr, kTxDescCount);

    memset(fTxDescBase, 0, kTxDescCount * sizeof(TxDesc));

    return true;
}

//---------------------------------------------------------------------------

void CLASS::releaseTxMemory( void )
{
    if (fTxDescMemory)
    {
        for (UInt32 i = 0; i < kTxDescCount; i++)
        {
            if (fTxDescBase[i].packet)
                freePacket(fTxDescBase[i].packet);
        }

        memset(fTxDescBase, 0, kTxDescCount * sizeof(TxDesc));
        fTxDescMemory->complete();
        fTxDescMemory->release();
        fTxDescMemory = 0;
        fTxDescBase   = 0;
    }
}

//---------------------------------------------------------------------------

bool CLASS::initTxRing( void )
{
    IOPhysicalAddress physAddr;

    // Link the descriptors into a ring

    for (int i = kTxDescCount - 1 ;i > 0; i--)
    {
        physAddr = fTxDescMemory->getPhysicalSegment(sizeof(TxDesc) * i, 0);
        if (physAddr == 0)
            return false;

        fTxDescBase[i-1].link = physAddr;
    }

    // Wrap the ring - last descriptor points to the first

    fTxDescBase[kTxDescCount - 1].link = fTxDescPhysAddr;

    fTxHeadIndex = 0;
    fTxTailIndex = 0;
    fTxInterruptInterval = 0;

    return true;
}

//---------------------------------------------------------------------------

#define TX_RING_FREE(head, tail) \
        (((head) - (tail) - 1) & (kTxDescCount - 1))

#define TX_RING_BUSY(head, tail) \
        (((tail) - (head)) & (kTxDescCount - 1))

UInt32 CLASS::outputPacket( mbuf_t packet, void * param )
{
    TxDesc *            descNext;
    TxDesc *            descHead;
    TxDesc *            descLast;
    UInt                segCount;
    UInt32              cmdStatus;
    UInt32              tailIndex;
    IOPhysicalSegment   vectors[ kTxMaxSegmentCount ];
    IODebuggerLockState state;

    state = IOKernelDebugger::lock(this);   

    tailIndex = fTxTailIndex;

    // Check if there are enough descriptors to describe the packet
    // buffers. kTxMaxSegmentCount should be large enough to reduce
    // the need to coalesce mbufs.

    if (TX_RING_FREE(fTxHeadIndex, tailIndex) < kTxMaxSegmentCount)
    {
        IOKernelDebugger::unlock(state);
        return kIOReturnOutputStall;
    }

    // Get the next transmit descriptor owned by the driver.

    descHead = &fTxDescBase[tailIndex];
    descNext = descHead;

    // Use the mbuf cursor to generate a list of physical address and
    // length vectors for the network buffers.

    segCount = fTxMbufCursor->getPhysicalSegmentsWithCoalesce(
                              packet, vectors, kTxMaxSegmentCount);
    if (segCount == 0)
    {
        DEBUG_LOG("TX Cursor returned 0 segments\n");
        goto drop_packet;
    }
    assert(segCount <= kTxMaxSegmentCount);

    // Update the first (head) descriptor.
    // Do not set the OWN bit until the rest of the descriptors are done.

    OSWriteLittleInt32(&descHead->bufferPtr, 0, vectors[0].location);
    cmdStatus = (vectors[0].length & kDescBufferSizeMask);
    tailIndex = (tailIndex + 1) & (kTxDescCount - 1);
    descLast  = descHead;
    descNext  = &fTxDescBase[tailIndex];

    for (UInt seg = 1; seg < segCount; seg++)
    {
        // Write cmdStatus for previous descriptor with MORE bit set.

        OSWriteLittleInt32(&descLast->cmdStatus, 0, cmdStatus | kDescMore);

        // Update current descriptor.

        OSWriteLittleInt32(&descNext->bufferPtr, 0, vectors[seg].location);
        cmdStatus = (vectors[seg].length & kDescBufferSizeMask) | kDescOwn;

        tailIndex = (tailIndex + 1) & (kTxDescCount - 1);
        descLast  = descNext;
        descNext  = &fTxDescBase[tailIndex];
    }

    // Last descriptor must have MORE bit cleared.

    if (++fTxInterruptInterval >= (kTxDescCount/kTxMaxSegmentCount/4))
    {
        cmdStatus |= kDescInterrupt;
        fTxInterruptInterval = 0;
    }

    OSWriteLittleInt32(&descLast->cmdStatus, 0, cmdStatus);
    
    // Set OWN bit on head descriptor after all descriptors following it
    // have been prepared.

    descHead->cmdStatus |= OSSwapHostToLittleConstInt32(kDescOwn);

    // Update Head Descriptor.

    descHead->packet    = packet;
    descHead->descLast  = descLast;
    descHead->descCount = segCount;
    descHead->nextIndex = tailIndex;

    // Update free descriptor count after completing the descriptor chain.
    // The order is important otherwise we may race with interrupt handler.

    fTxTailIndex = tailIndex;
    DEBUG_LOG("TX DESC:%d-%ld (size %d)\n",
              descHead-fTxDescBase, fTxTailIndex, mbuf_pkthdr_len(packet));

    // Enable transmitter in case its in txIdle state.

    WriteReg(CR, CR_TXE);

    IOKernelDebugger::unlock(state);
    NET_STAT(outputPackets, 1);
    return kIOReturnOutputSuccess;

drop_packet:
    IOKernelDebugger::unlock(state);
    freePacket(packet);
    ETH_STAT(dot3TxExtraEntry.resourceErrors, 1);
    return kIOReturnOutputDropped;
}

//---------------------------------------------------------------------------

void CLASS::serviceTxInterrupt( void )
{
    TxDesc * descPtr;
    UInt32   headIndex = fTxHeadIndex;
    UInt32   busyCount = TX_RING_BUSY(headIndex, fTxTailIndex);
    UInt32   doneCount = 0;
    UInt32   txStatus;

    while (doneCount < busyCount)
    {
        descPtr = &fTxDescBase[ headIndex ];
        assert(descPtr->descLast);
        assert(descPtr->descCount);

        // Examine ownership bit in the last descriptor for this chain.

        txStatus = OSReadLittleInt32(&descPtr->descLast->cmdStatus, 0);
        if (txStatus & kDescOwn)
            break;  // transmit not done yet

        if (txStatus & kDescTxAbnormalMask)
        {
            recordTxDescriptorErrors(txStatus);
        }

        if (descPtr->packet)
        {
            freePacket(descPtr->packet, kDelayFree);
            descPtr->packet = 0;
        }

        // Skip to the start of the next transmit slot.

        headIndex  = descPtr->nextIndex;
        doneCount += descPtr->descCount;
    }

    if (doneCount)
    {
        fTxHeadIndex = headIndex;
        fTransmitQueue->service();
        releaseFreePackets();
        DEBUG_LOG("TX ISR: retired %lu\n", doneCount);
    }
}

//---------------------------------------------------------------------------

void CLASS::recordTxDescriptorErrors( UInt32 txStatus )
{    
    DEBUG_LOG("TX PACKET ERROR %08lx\n", txStatus);

    if (txStatus & kDescTxErrorMask)
        NET_STAT(outputErrors, 1);
    if (txStatus & (kDescTxAbort | kDescExcessiveDeferral))
        ETH_STAT(dot3StatsEntry.internalMacTransmitErrors, 1);
    if (txStatus & kDescTxFIFOUnderrun)
        ETH_STAT(dot3TxExtraEntry.underruns, 1);
    if (txStatus & kDescCarrierSenseLost)
        ETH_STAT(dot3StatsEntry.carrierSenseErrors, 1);
    if (txStatus & kDescTransmitDeferral)
        ETH_STAT(dot3StatsEntry.deferredTransmissions, 1);
    if (txStatus & kDescLateCollision)
        ETH_STAT(dot3StatsEntry.lateCollisions, 1);
    if (txStatus & kDescExcessiveCollisions)
        ETH_STAT(dot3StatsEntry.excessiveCollisions, 1);
    if (txStatus & kDescTxCollisionCountMask)
    {
        UInt32 count = (txStatus & kDescTxCollisionCountMask) >>
                        kDescTxCollisionCountShift;
        ETH_STAT(dot3CollEntry.collFrequencies[count], 1);
        NET_STAT(collisions, count);
    }
}

#pragma mark -
#pragma mark ••• Polled Interface •••
#pragma mark -

//---------------------------------------------------------------------------

void CLASS::waitForFreeTransmitDescriptors( UInt32 freeCount )
{
    TxDesc * descPtr;

    while (TX_RING_FREE(fTxHeadIndex, fTxTailIndex) < freeCount)
    {
        descPtr = &fTxDescBase[ fTxHeadIndex ];

        while (descPtr->descLast->cmdStatus & 
               OSSwapHostToLittleConstInt32(kDescOwn))
        {
            IODelay(20);
        }

        if (descPtr->packet)
        {
            fKDPQueue->enqueue(descPtr->packet);
            descPtr->packet = 0;
        }

        fTxHeadIndex = descPtr->nextIndex;
    }
}

//---------------------------------------------------------------------------

void CLASS::sendPacket( void * pkt_data, UInt32 pkt_size )
{
    TxDesc * descPtr;
    UInt32   cmdStatus;
    UInt32   tailIndex = fTxTailIndex;

    DEBUG_LOG("sendPacket size %ld\n", pkt_size);

    if (!pkt_data || pkt_size > kIOEthernetMaxPacketSize) return;

    waitForFreeTransmitDescriptors( 1 );

    descPtr = &fTxDescBase[ tailIndex ];

    memcpy(mbuf_data(fKDPMbuf), pkt_data, pkt_size);

    // Prepare descriptor.
    // Must enable descriptor interrupt so that the tx interrupt handler
    // will run after returning from kdp, and clean up the ring.

    descPtr->bufferPtr = fKDPMbufSeg.location;
    cmdStatus = kDescOwn | kDescInterrupt | (pkt_size & kDescBufferSizeMask);
    OSWriteLittleInt32(&descPtr->cmdStatus, 0, cmdStatus);

    tailIndex = (tailIndex + 1) & (kTxDescCount - 1);

    descPtr->packet    = 0;
    descPtr->descLast  = descPtr;
    descPtr->descCount = 1;
    descPtr->nextIndex = tailIndex;

    fTxTailIndex = tailIndex;

    // Wait for KDP buffer send completion before returning.

    WriteRegFlush(CR, CR_TXE);

    DEBUG_LOG("sendPacket completion wait...");
    while (descPtr->cmdStatus & OSSwapHostToLittleConstInt32(kDescOwn))
    {
        IODelay(20);
    }
    DEBUG_LOG("done\n");
}

//---------------------------------------------------------------------------

void CLASS::receivePacket( void * pkt_data, UInt32 * pkt_size,
                           UInt32 timeout )
{
    RxDesc * rxDesc;
    UInt32   rxStatus;
    UInt32   rxLength;

    *pkt_size = 0;
    timeout *= 1000;  // from ms to us

    while (timeout && (*pkt_size == 0))
    {
        rxDesc   = &fRxDescBase[fRxHeadIndex];
        rxStatus = OSReadLittleInt32(&rxDesc->cmdStatus, 0);

        if ((rxStatus & kDescOwn) == 0)
        {
            IODelay(20);
            timeout -= 20;
            continue;
        }

        DEBUG_LOG("RX Status = %lx @ index %lu\n", rxStatus, fRxHeadIndex);

        rxLength = (rxStatus & kDescBufferSizeMask);

        if (((rxStatus & (kDescMore | kDescPacketOK)) == kDescPacketOK) &&
            (rxLength >= kIOEthernetMinPacketSize) &&
            (rxLength <= kIOEthernetMaxPacketSize))
        {
            // KDP allocates a buffer to hold 1518 bytes.
            // Driver must never overrun this buffer.

            memcpy(pkt_data,
                   mbuf_data(rxDesc->packet),
                   rxLength);

            *pkt_size = rxLength;

            DEBUG_LOG("DEBUG: RX %lu\n", rxLength);
        }

        // Update the current descriptor and make it owned by NIC.

        rxStatus = kDescInterrupt | kDescIncludeCRC |
                   (kRxMaxBufferSize & kDescBufferSizeMask);

        OSWriteLittleInt32(&rxDesc->cmdStatus, 0, rxStatus);

        // Advance to next descriptor.

        fRxHeadIndex = (fRxHeadIndex + 1) & (kRxDescCount - 1);
    }
}

#pragma mark -
#pragma mark ••• Interrupts •••
#pragma mark -

#define kInterruptSourceMask \
        ( ISR_PHY     | \
          ISR_TXURN   | \
          ISR_TXDESC  | \
          ISR_RXDESC  | \
          ISR_RXERR   | \
          ISR_RXOK    | \
          ISR_DPERR   | \
          ISR_SSERR   | \
          ISR_RMABT   | \
          ISR_RTABT )

//---------------------------------------------------------------------------

void CLASS::enableHardwareInterrupts( void )
{
    WriteRegFlush( IMR, kInterruptSourceMask );
    WriteRegFlush( IER, IER_IE );
}

void CLASS::disableHardwareInterrupts( void )
{
    WriteRegFlush( IER, 0x0 );
}

//---------------------------------------------------------------------------

void CLASS::interruptHandler( OSObject * owner, IOInterruptEventSource *, int )
{
    CLASS * me = (CLASS *) owner;
    me->interruptOccurred();
}

void CLASS::interruptOccurred( void )
{
    UInt32              status;
    IODebuggerLockState state;

    if (fActivationLevel != kActivationLevelBSD)
    {
        return;  // shared or spurious interrupt
    }

    state = IOKernelDebugger::lock(this);        

    while (1)
    {
        status = ReadReg(ISR);  // ack on read
        if ((status & kInterruptSourceMask) == 0)
            break;

        DEBUG_LOG("ISR = 0x%08x\n", status);

        // Receive Interrupt

        if (status & (ISR_RXDESC | ISR_RXOK | ISR_RXERR))
        {
            serviceRxInterrupt();
            ETH_STAT(dot3RxExtraEntry.interrupts, 1);
        }

        // Transmit Interrupt

        if (status & (ISR_TXDESC | ISR_TXURN))
        {
            serviceTxInterrupt();
            ETH_STAT(dot3TxExtraEntry.interrupts, 1);
        }

        // Everything else

        if (status & (ISR_PHY|ISR_DPERR|ISR_SSERR|ISR_RMABT|ISR_RTABT))
        {
            if (status & (ISR_DPERR|ISR_SSERR|ISR_RMABT|ISR_RTABT))
            {
                ERROR_LOG("%s: Fatal PCI Error\n", getName());
            }
            if (status & ISR_PHY)
            {
                phyReportLinkStatus();
                DEBUG_LOG("Link Change interrupt\n");
            }
        }
    }

    IOKernelDebugger::unlock(state);
}
