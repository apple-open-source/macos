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

#include "BCM440X.h"

#define super IOEthernetController
OSDefineMetaClassAndStructors( AppleBCM440XEthernet, IOEthernetController )

//-------------------------------------------------------------------------

bool BCM440X::start( IOService * provider )
{
    bool  success = false;
    bool  started = false;

    union {
        UInt32  dword[32];
        UInt8   byte[];
    } eeprom;

    do {
        if (super::start(provider) != true)
            break;

        started = true;

        // Allocate memory for bfe driver's softc structure

        sc = IONew(struct bfe_softc, 1);
        if (sc == 0)
        {
            ERROR_LOG("%s: No memory for bfe_softc\n", getName());
            break;
        }
        memset(sc, 0, sizeof(*sc));

        // Allocate transmit and receive memory

        if (allocateTxMemory() == false)
        {
            ERROR_LOG("%s: allocateTxMemory failed\n", getName());
            break;
        }
        if (allocateRxMemory() == false)
        {
            ERROR_LOG("%s: allocateRxMemory failed\n", getName());
            break;
        }

        // Cache provider and attempt to become its exclusive client

        fPCIDevice = OSDynamicCast(IOPCIDevice, provider);
        if (fPCIDevice == 0) break;

        fPCIDevice->retain();
        if (fPCIDevice->open(this) == false)
            break;

        // Allocate and init support objects used by the driver

        if (initDriverObjects(provider) == false)
        {
            ERROR_LOG("%s: initDriverObjects failed\n", getName());
            break;
        }

        // Initialize PCI config space

        initPCIConfigSpace(fPCIDevice);

        // Map hardware registers

        fRegMap = fPCIDevice->mapDeviceMemoryWithRegister(
                      BFE_PCI_MEMLO, kIOMapInhibitCache );
        if (fRegMap == 0)
        {
            ERROR_LOG("%s: PCI BAR@%x (%08lx) mapping error\n",
                      getName(), BFE_PCI_MEMLO,
                      fPCIDevice->configRead32(BFE_PCI_MEMLO));
            break;
        }

        // Initialize bfe_softc

        sc->bfe_dev      = this;
        sc->bfe_unit     = 0;
        sc->bfe_bhandle  = (void *) fRegMap->getVirtualAddress();
        sc->bfe_phyaddr  = (eeprom.byte[90] & 0x1f);
        sc->bfe_mdc_port = (eeprom.byte[90] >> 14) & 0x1;
        sc->bfe_tx_dma   = fTxDescPhysAddr;
        sc->bfe_rx_dma   = fRxDescPhysAddr;

        // Read permanent Ethernet address from EPROM

        for (int i = 0; i < 32; i++)
            eeprom.dword[i] = CSR_READ_4(sc, 0x1000 + 4*i);

        fEnetAddr.bytes[0] = eeprom.byte[79];
        fEnetAddr.bytes[1] = eeprom.byte[78];
        fEnetAddr.bytes[2] = eeprom.byte[81];
        fEnetAddr.bytes[3] = eeprom.byte[80];
        fEnetAddr.bytes[4] = eeprom.byte[83];
        fEnetAddr.bytes[5] = eeprom.byte[82];

        DEBUG_LOG("Enet address = %02x:%02x:%02x:%02x:%02x:%02x\n",
                  fEnetAddr.bytes[0], fEnetAddr.bytes[1],
                  fEnetAddr.bytes[2], fEnetAddr.bytes[3],
                  fEnetAddr.bytes[4], fEnetAddr.bytes[5]);

        // Reset Core

        bfe_chip_reset(sc);

        // Probe PHY

        if (fPHY->probePHY() != kIOReturnSuccess)
        {
            ERROR_LOG("%s: PHY not detected\n", getName());
            break;
        }

        // Report the supported media types

        publishMediaSupport();

        success = true;
    
    } while (0);

    // Close our provider, it will be re-opened on demand when driver
    // is enabled by BSD or KDP.  No hardware access is allowed below
    // this line to guarantee single threaded access to hardware.

    if (fPCIDevice) fPCIDevice->close(this);

    do {
        if (success == false) break;
        success = false;

        // Attach an IOEthernetInterface

        if (attachInterface((IONetworkInterface **) &fNetif, false) == false)
            break;

        // Reserved a copy buffer memory used during kernel debugging,
        // and resolve its physical address. Use mbuf for convenience.

        fKDPMbuf = allocatePacket(BFE_BUFFER_SIZE);
        if (fKDPMbuf &&
            fTxMbufCursor->getPhysicalSegments(fKDPMbuf, &fKDPMbufSeg) == 1)
        {
            attachDebuggerClient(&fKDPNub);
        }

        fNetif->registerService();
        success = true;

    } while (0);

    if (started && !success)
    {
        super::stop(provider);
    }

    return success;
}

//---------------------------------------------------------------------------

void BCM440X::free( void )
{
    #define RELEASE(x) do { if(x) { (x)->release(); (x) = 0; } } while(0)

    DEBUG_LOG("BCM440X::free\n");

    assert(fEnabledForKDP == false);
    assert(fEnabledForBSD == false);

    if (fInterruptSrc && fWorkLoop)
    {
        fWorkLoop->removeEventSource(fInterruptSrc);
    }

    RELEASE( fInterruptSrc  );
    RELEASE( fKDPNub        );
    RELEASE( fNetif         );
    RELEASE( fTxMbufCursor  );
    RELEASE( fPCIDevice     );
    RELEASE( fWorkLoop      );
    RELEASE( fRegMap        );    
    RELEASE( fWatchdogTimer );
    RELEASE( fGarbageQueue  );
    RELEASE( fMediaDict     );
    RELEASE( fPHY           );
    RELEASE( fKDPNub        );

    releaseTxMemory();
    releaseRxMemory();

    if (fKDPMbuf)
    {
        freePacket(fKDPMbuf);
        fKDPMbuf = 0;
    }

    if (sc)
    {
        IODelete(sc, struct bfe_softc, 1);
        sc = 0;
    }

    return super::free();
}

//---------------------------------------------------------------------------

bool BCM440X::initDriverObjects( IOService * provider )
{
    // Create the MII PHY

    fPHY = AppleBCM440XPHY::BCM440XPHY(this, mdiRead, mdiWrite);
    if (fPHY == 0)
        return false;

    // When transmit ring is full, packets are queued here

    fTransmitQueue = getOutputQueue();
    if (fTransmitQueue == 0)
        return false;

    // Queue freed packets on TX ring while debugging

    fGarbageQueue = IOPacketQueue::withCapacity(~0);
    if (fGarbageQueue == 0)
        return false;

    // Get our work loop

    IOWorkLoop * workLoop = (IOWorkLoop *) getWorkLoop();
    if (!workLoop)
    {
        DEBUG_LOG("No work loop\n");
        return false;
    }

    // Create a mbuf cursor for transmit

    fTxMbufCursor = IOMbufNaturalMemoryCursor::withSpecification(
                                                   BFE_BUFFER_SIZE, 1);
    if (!fTxMbufCursor)
    {
        DEBUG_LOG("TX mbuf cursor allocation error\n");
        return false;
    }

    // Attach an interrupt event source to our work loop

    fInterruptSrc = IOInterruptEventSource::interruptEventSource( this,
                    (IOInterruptEventAction) &BCM440X::interruptOccurred,
                    provider);
    if (!fInterruptSrc ||
        (workLoop->addEventSource(fInterruptSrc) != kIOReturnSuccess))
    {
        DEBUG_LOG("IOInterruptEventSource error\n");
        return false;
    }

    fWatchdogTimer = IOTimerEventSource::timerEventSource(this,
                (IOTimerEventSource::Action) &BCM440X::watchdogTimeout);
    if (!fWatchdogTimer ||
        (workLoop->addEventSource(fWatchdogTimer) != kIOReturnSuccess))
    {
        DEBUG_LOG("IOTimerEventSource error\n");
        return false;
    }

    fInterruptSrc->enable();

    return true;
}

//---------------------------------------------------------------------------

void BCM440X::initPCIConfigSpace( IOPCIDevice * pci )
{
    UInt16 cmd = pci->configRead16(kIOPCIConfigCommand);

    cmd |= ( kIOPCICommandBusMaster       |
             kIOPCICommandMemorySpace     |
             kIOPCICommandMemWrInvalidate );
    cmd &= ~kIOPCICommandIOSpace;

    pci->configWrite16(kIOPCIConfigCommand, cmd);
    DEBUG_LOG("PCI CMD = %04x\n", pci->configRead16(kIOPCIConfigCommand));
}

//---------------------------------------------------------------------------

IOReturn BCM440X::getHardwareAddress( IOEthernetAddress * addr )
{
    memcpy(addr, &fEnetAddr, sizeof(*addr));
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

bool BCM440X::createWorkLoop( void )
{
    fWorkLoop = IOWorkLoop::workLoop();
    return (fWorkLoop != 0);
}

IOWorkLoop * BCM440X::getWorkLoop( void ) const
{
    return fWorkLoop;
}

//---------------------------------------------------------------------------

const OSString * BCM440X::newVendorString( void ) const
{
    return OSString::withCString("Broadcom");
}

const OSString * BCM440X::newModelString( void ) const
{
    return OSString::withCString("4401");
}

//---------------------------------------------------------------------------

bool BCM440X::configureInterface( IONetworkInterface * netif )
{
    IONetworkData * data;

    if (super::configureInterface(netif) == false)
        return false;

    // Get the generic network statistics structure
    data = netif->getParameter(kIONetworkStatsKey);
    if (!data || !(fNetStats = (IONetworkStats *) data->getBuffer()))
    {
        DEBUG_LOG("No network statistics\n");
        return false;
    }

    // Get the Ethernet statistics structure
    data = netif->getParameter(kIOEthernetStatsKey);
    if (!data || !(fEtherStats = (IOEthernetStats *) data->getBuffer()))
    {
        DEBUG_LOG("No Ethernet statistics\n");
        return false;
    }

    return true;
}

#pragma mark -
#pragma mark ••• Enable and Disable •••
#pragma mark -

//---------------------------------------------------------------------------

bool BCM440X::increaseActivationLevel( UInt32 newLevel )
{
    bool success = false;
    
    switch (newLevel)
    {
        case kActivationLevelKDP:

            if ((fPCIDevice == 0) || (fPCIDevice->open(this) == false))
                break;

            //bfe_chip_halt(sc);
            //IOSleep(10);
            bfe_chip_reset(sc);

            if (initTxRing() == false || initRxRing() == false)
            {
                ERROR_LOG("%s: TX/RX ring init failed\n", getName());
                break;
            }

            // Program PHY
            fSelectMediumOverride = true;
            selectMedium(getSelectedMedium());
            fSelectMediumOverride = false;

            initRxFilter();
            bfe_chip_enable(sc);

            fWatchdogTimer->setTimeoutMS(kWatchdogTimerPeriodMS);
            success = true;
            break;

        case kActivationLevelBSD:

            bfe_enable_interrupts(sc);
            fInterruptEnabled = true;

            fTransmitQueue->setCapacity(1024);
            fTransmitQueue->start();
            success = true;
            break;
    }

    return success;
}

//---------------------------------------------------------------------------

bool BCM440X::decreaseActivationLevel( UInt32 newLevel )
{
    bool success = true;

    switch (newLevel)
    {
        case kActivationLevelKDP:

            bfe_chip_halt(sc);
            IOSleep(10);
            bfe_chip_reset(sc);

            fWatchdogTimer->cancelTimeout();

            setLinkStatus( kIONetworkLinkValid );
            IOSleep(20);

            freeTxRingPackets();
            freeRxRingPackets();

            if (fPCIDevice)
                fPCIDevice->close(this);

            break;

        case kActivationLevelBSD:

            bfe_disable_interrupts(sc);
            fInterruptEnabled = false;

            fTransmitQueue->stop();
            fTransmitQueue->setCapacity(0);
            fTransmitQueue->flush();

            break;
    }

    return success;
}

//---------------------------------------------------------------------------

bool BCM440X::resetCurrentActivationLevel( void )
{
    bool success = false;

    do {
        if (fActivationLevel != kActivationLevelKDP &&
            fActivationLevel != kActivationLevelBSD)
        {
            success = true;
            break;
        }

        if (fActivationLevel == kActivationLevelBSD)
            fTransmitQueue->stop();

        IODebuggerLockState state = IOKernelDebugger::lock(this);

        bfe_chip_halt(sc);
        IOSleep(10);
        bfe_chip_reset(sc);
        fInterruptEnabled = false;

        if (initTxRing() == false || initRxRing() == false)
        {
            ERROR_LOG("%s: TX/RX ring init failed\n", getName());
            IOKernelDebugger::unlock(state);
            break;
        }

        initRxFilter();
        bfe_chip_enable(sc);

        if (fActivationLevel == kActivationLevelBSD)
        {
            bfe_enable_interrupts(sc);
            fInterruptEnabled = true;
            IOKernelDebugger::unlock(state);
            fTransmitQueue->start();
        }
        else
        {
            IOKernelDebugger::unlock(state);
        }

        success = true;
    } while (0);

    return success;
}

//---------------------------------------------------------------------------

bool BCM440X::setActivationLevel( UInt32 inLevel )
{
    bool success = false;

    DEBUG_LOG("setActivationLevel %ld\n", inLevel);

    if (fActivationLevel == inLevel) return true;

    for ( ; fActivationLevel > inLevel; fActivationLevel--) 
    {
        if ((success = decreaseActivationLevel(fActivationLevel)) == false)
            break;
    }

    for ( ; fActivationLevel < inLevel; fActivationLevel++ ) 
    {
        if ((success = increaseActivationLevel(fActivationLevel+1)) == false)
            break;
    }

    return success;
}

//---------------------------------------------------------------------------

IOReturn BCM440X::enable( IONetworkInterface * netif )
{
    if (fEnabledForBSD) return kIOReturnSuccess;

    fEnabledForBSD = setActivationLevel(kActivationLevelBSD);

    return fEnabledForBSD ? kIOReturnSuccess : kIOReturnIOError;
}

//---------------------------------------------------------------------------

IOReturn BCM440X::disable( IONetworkInterface * netif )
{
    fEnabledForBSD = false;

    setActivationLevel(fEnabledForKDP ?
                       kActivationLevelKDP :
                       kActivationLevelNone);

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn BCM440X::enable( IOKernelDebugger * debugger )
{    
    if (fEnabledForKDP || fEnabledForBSD)
    {
        fEnabledForKDP = true;
        return kIOReturnSuccess;
    }

    fEnabledForKDP = setActivationLevel(kActivationLevelKDP);

    return (fEnabledForKDP ? kIOReturnSuccess : kIOReturnIOError);
}

//---------------------------------------------------------------------------

IOReturn BCM440X::disable( IOKernelDebugger * debugger )
{
    fEnabledForKDP = false;

    if (fEnabledForBSD == false)
        setActivationLevel(kActivationLevelNone);

    return kIOReturnSuccess;
}

#pragma mark -
#pragma mark ••• Transmit •••
#pragma mark -

//---------------------------------------------------------------------------

bool BCM440X::allocateTxMemory( void )
{
    fTxDescMemory = IOBufferMemoryDescriptor::withOptions(
                            kIOMemoryPhysicallyContiguous,
                            BFE_TX_LIST_SIZE, PAGE_SIZE);
    if (fTxDescMemory == 0)
    {
        ERROR_LOG("%s: No memory for transmit descriptors\n", getName());
        return false;
    }

    if (fTxDescMemory->prepare() != kIOReturnSuccess)
    {
        ERROR_LOG("%s: TX memory prepare failed\n", getName());
        fTxDescMemory->release();
        fTxDescMemory = 0;
        return false;
    }

    // Resolve descriptor base physical address

    fTxDescPhysAddr = fTxDescMemory->getPhysicalSegment(0, 0);
    if (fTxDescPhysAddr == 0)
    {
        ERROR_LOG("%s: TX memory getPhysicalSegment failed\n", getName());
        return false;
    }

    fTxDescBase = (bfe_desc *) fTxDescMemory->getBytesNoCopy();
    DEBUG_LOG("TX DESC Base V=%p P=0x%lx\n", fTxDescBase, fTxDescPhysAddr);
    memset(fTxDescBase, 0, BFE_TX_LIST_SIZE);

    // Reserve memory for an array of mbufs

    fTxPacketArray = IONew(mbuf_t, BFE_TX_LIST_CNT);
    if (fTxPacketArray == 0)
    {
        ERROR_LOG("%s: No memory for transmit packet list\n", getName());
        return false;
    }
    memset(fTxPacketArray, 0, BFE_TX_LIST_CNT * sizeof(mbuf_t));

    return true;
}

//---------------------------------------------------------------------------

void BCM440X::releaseTxMemory( void )
{
    if (fTxDescMemory)
    {
        fTxDescMemory->complete();
        fTxDescMemory->release();
        fTxDescMemory = 0;
        fTxDescBase   = 0;
    }

    if (fTxPacketArray)
    {
        freeTxRingPackets();
        IODelete(fTxPacketArray, mbuf_t, BFE_TX_LIST_CNT);
        fTxPacketArray = 0;
    }
}

//---------------------------------------------------------------------------

bool BCM440X::initTxRing( void )
{
    freeTxRingPackets();

    memset(fTxDescBase, 0, BFE_TX_LIST_SIZE);
    memset(fTxPacketArray, 0, BFE_TX_LIST_CNT * sizeof(mbuf_t));

    fTxHeadIndex    = 0;
    fTxTailIndex    = 0;
    fTxDescBusy     = 0;
    fTxRingDelayIOC = 0;

    return true;
}

//---------------------------------------------------------------------------

void BCM440X::freeTxRingPackets( void )
{
    if (fTxPacketArray)
    {
        for (int i = 0; i < BFE_TX_LIST_CNT; i++)
        {
            if (fTxPacketArray[i])
            {
                freePacket(fTxPacketArray[i]);
                fTxPacketArray[i] = 0;
            }
        }
    }
}

//---------------------------------------------------------------------------

UInt32 BCM440X::outputPacket( mbuf_t packet, void * param )
{
    enum {
        kTxMaxSegmentCount = 8,
        kTxDescLowWater    = kTxMaxSegmentCount + 2
    };

    UInt32              ctrl = 0;
    int                 curFrag, lastFrag, fragCount;
    IOPhysicalSegment   vectors[kTxMaxSegmentCount];  // FIXME

    IODebuggerLockState state = IOKernelDebugger::lock(this);        

    // Stall output if ring is almost full

    if (BFE_TX_LIST_CNT - fTxDescBusy < kTxDescLowWater)
    {
        IOKernelDebugger::unlock(state);
        return kIOReturnOutputStall;
    }

    // Extract physical address and length pairs from the packet

    fragCount = fTxMbufCursor->getPhysicalSegmentsWithCoalesce(
                                  packet, vectors,
                                  kTxMaxSegmentCount);
    if (fragCount == 0)
    {
        DEBUG_LOG("TX Cursor returned 0 segments\n");
        goto drop_packet;
    }

    // Add memory fragments to the ring

    curFrag = lastFrag = fTxTailIndex;
    for (int i = 0; i < fragCount; i++)
    {
        ctrl = vectors[i].length & BFE_DESC_LEN;
        if (i == 0)
            ctrl |= BFE_DESC_SOF;
        if (curFrag == BFE_TX_LIST_CNT - 1)
            ctrl |= BFE_DESC_EOT;

        OSWriteLittleInt32(&fTxDescBase[curFrag].bfe_ctrl, 0, ctrl);
        OSWriteLittleInt32(&fTxDescBase[curFrag].bfe_addr, 0,
                           vectors[i].location + BFE_PCI_DMA);

        lastFrag = curFrag;
        BFE_INC(curFrag, BFE_TX_LIST_CNT);
    }
    
    // Set end-of-fragment flag on last descriptor

    ctrl |= BFE_DESC_EOF;

    // Limit the number of TX interrupts

    fTxRingDelayIOC += fragCount;
    if (fTxRingDelayIOC >= min(32, BFE_TX_LIST_CNT/4))
    {
        fTxRingDelayIOC = 0;
        ctrl |= BFE_DESC_IOC;
    }
    OSWriteLittleInt32(&fTxDescBase[lastFrag].bfe_ctrl, 0, ctrl);

    // Attach mbuf packet to ring until transmission is complete

    fTxPacketArray[lastFrag] = packet;
    fTxDescBusy += fragCount;
    fTxTailIndex = curFrag;

    // Update hardware pointer

    CSR_WRITE_4(sc, BFE_DMATX_PTR, curFrag * sizeof(struct bfe_desc));

    IOKernelDebugger::unlock(state);
    NET_STAT(outputPackets, 1);
    RX_TX_LOG("TX index %d\n", lastFrag);
    return kIOReturnOutputSuccess;

drop_packet:
    IOKernelDebugger::unlock(state);
    freePacket(packet);
    NET_STAT(outputErrors, 1);
    return kIOReturnOutputDropped;
}

//---------------------------------------------------------------------------

void BCM440X::serviceTxInterrupt( void )
{
    UInt32  index;
    UInt32  chipIndex;

    chipIndex  = CSR_READ_4(sc, BFE_DMATX_STAT) & BFE_STAT_CDMASK;
    chipIndex /= sizeof(struct bfe_desc);

    for ( index = fTxHeadIndex; fTxDescBusy > 0; )
    {
        RX_TX_LOG("TX ISR index %ld chipIndex %ld\n", index, chipIndex);
        if (index == chipIndex)
            break;

        if (fTxPacketArray[index] != 0)
        {
            freePacket(fTxPacketArray[index]);
            fTxPacketArray[index] = 0;
        }

        fTxDescBusy--;
        BFE_INC(index, BFE_TX_LIST_CNT);
    }

    fTxHeadIndex = index;
    fTransmitQueue->service();
}

//---------------------------------------------------------------------------

IOOutputQueue * BCM440X::createOutputQueue( void )
{
    /* Output is synchronized with work loop (makes things simple) */
    return IOGatedOutputQueue::withTarget(this, getWorkLoop());
}

#pragma mark -
#pragma mark ••• Receive •••
#pragma mark -

//---------------------------------------------------------------------------

bool BCM440X::allocateRxMemory( void )
{
    fRxDescMemory = IOBufferMemoryDescriptor::withOptions(
                            kIOMemoryPhysicallyContiguous,
                            BFE_RX_LIST_SIZE, PAGE_SIZE);
    if (fRxDescMemory == 0)
    {
        ERROR_LOG("%s: No memory for receive descriptors\n", getName());
        return false;
    }

    if (fRxDescMemory->prepare() != kIOReturnSuccess)
    {
        ERROR_LOG("%s: RX memory prepare failed\n", getName());
        fRxDescMemory->release();
        fRxDescMemory = 0;
        return false;
    }

    // Resolve descriptor base physical address

    fRxDescPhysAddr = fRxDescMemory->getPhysicalSegment(0, 0);
    if (fRxDescPhysAddr == 0)
    {
        ERROR_LOG("%s: RX memory getPhysicalSegment failed\n", getName());
        return false;
    }

    fRxDescBase = (bfe_desc *) fRxDescMemory->getBytesNoCopy();
    DEBUG_LOG("RX DESC Base V=%p P=0x%lx\n", fRxDescBase, fRxDescPhysAddr);
    memset(fRxDescBase, 0, BFE_RX_LIST_SIZE);

    // Reserve memory for an array of mbufs

    fRxPacketArray = IONew(mbuf_t, BFE_RX_LIST_CNT);
    if (fRxPacketArray == 0)
    {
        ERROR_LOG("%s: No memory for receive packet list\n", getName());
        return false;
    }
    memset(fRxPacketArray, 0, BFE_RX_LIST_CNT * sizeof(mbuf_t));

    return true;
}

//---------------------------------------------------------------------------

void BCM440X::releaseRxMemory( void )
{
    if (fRxDescMemory)
    {
        fRxDescMemory->complete();
        fRxDescMemory->release();
        fRxDescMemory = 0;
        fRxDescBase   = 0;
    }

    if (fRxPacketArray)
    {
        freeRxRingPackets();
        IODelete(fRxPacketArray, mbuf_t, BFE_RX_LIST_CNT);
        fRxPacketArray = 0;
    }
}

//---------------------------------------------------------------------------

bool BCM440X::initRxRing( void )
{
    int i;

    memset(fRxDescBase, 0, BFE_RX_LIST_SIZE);

    for (i = 0; i < BFE_RX_LIST_CNT; i++)
    {
        if (fRxPacketArray[i] == 0)
            fRxPacketArray[i] = allocatePacket(BFE_BUFFER_SIZE);
        if (fRxPacketArray[i] == 0)
            return false;
        if (updateRxDescriptor(i) == false)
            return false;
    }
    CSR_WRITE_4(sc, BFE_DMARX_PTR, i * sizeof(bfe_desc));

    fRxHeadIndex = 0;

    return true;
}

//---------------------------------------------------------------------------

void BCM440X::freeRxRingPackets( void )
{
    if (fRxPacketArray)
    {
        for (int i = 0; i < BFE_RX_LIST_CNT; i++)
        {
            if (fRxPacketArray[i])
            {
                freePacket(fRxPacketArray[i]);
                fRxPacketArray[i] = 0;
            }
        }
    }
}

//---------------------------------------------------------------------------

#define MBUF_PHYS_ADDR(m) \
        ((IOPhysicalAddress)mbuf_data_to_physical(mbuf_data(m)))

bool BCM440X::updateRxDescriptor( UInt32 index )
{
    bfe_rxheader * rx_header;
    UInt32         ctrl;
    mbuf_t         packet;

    assert(index <= BFE_RX_LIST_CNT);

    // Clear the receive header in the mbuf

    packet = fRxPacketArray[index];
    rx_header = (struct bfe_rxheader *) mbuf_data(packet);
    rx_header->len   = 0;
    rx_header->flags = 0;

    // Attach the mbuf into the DMA ring

    ctrl = BFE_BUFFER_SIZE;
    if (index == BFE_RX_LIST_CNT - 1)
        ctrl |= BFE_DESC_EOT;

    OSWriteLittleInt32(&fRxDescBase[index].bfe_ctrl, 0, ctrl);
    OSWriteLittleInt32(&fRxDescBase[index].bfe_addr, 0,
                       MBUF_PHYS_ADDR(packet) + BFE_PCI_DMA);

    return true;
}

//---------------------------------------------------------------------------

void BCM440X::serviceRxInterrupt( void )
{
    UInt32         index;
    UInt32         chipIndex;
    bool           replaced;
    bfe_rxheader * rxh;
    UInt16         flags, len;
    mbuf_t         inputPkt;
    UInt8 *        inputData;

    chipIndex = (CSR_READ_4(sc, BFE_DMARX_STAT) & BFE_STAT_CDMASK) /
                sizeof(struct bfe_desc);

    for (index = fRxHeadIndex; index != chipIndex; )
    {
        rxh = (bfe_rxheader *) mbuf_data(fRxPacketArray[index]);
        len = OSSwapLittleToHostInt16(rxh->len);
        if (len == 0)
        {
            // It is conceivable that the memory write from the chip got
            // posted.  Why DMA into the mbuf memory in the first place?
            // Driver must never pass up a packet in this state to avoid
            // the dreaded "mbuf corruption" problem.

            for (int retry = 0; retry < 10; retry++)
            {
                len = OSSwapLittleToHostInt16(rxh->len);
                if (len) break;
                DEBUG_LOG("RX zero header len: chip %ld index %ld retry %d\n",
                          chipIndex, index, retry);
                IODelay(1);
            }
            if (len == 0)
            {
                resetCurrentActivationLevel();
                ETH_STAT(dot3RxExtraEntry.resets, 1);
                return;
            }
        }
        flags = OSSwapLittleToHostInt16(rxh->flags);

        // Check for over-sized frames and receive errors. len reported does
        // not include the bfe_rxheader, but does include the 4 byte FCS.

        if ((len > kIOEthernetMaxPacketSize) ||
            (len < kIOEthernetMinPacketSize) ||
            ((flags & (BFE_RX_FLAG_ERRORS|BFE_RX_FLAG_LAST)) != BFE_RX_FLAG_LAST))
        {
            DEBUG_LOG("RX error len %d flags 0x%x\n", len, flags);
            NET_STAT(inputErrors, 1);
            goto next;
        }

        // Copy or replace the packet in the ring depending on the size
        // of the received frame

        inputPkt = replaceOrCopyPacket(&fRxPacketArray[index],
                                       len + BFE_RX_OFFSET,
                                       &replaced);
        if (inputPkt == 0)
        {
            DEBUG_LOG("RX replaceOrCopy error, len %d flags 0x%x\n",
                      len + BFE_RX_OFFSET, flags);
            NET_STAT(inputErrors, 1);
            goto next;
        }
        
        inputData = (UInt8 *) mbuf_data(inputPkt);
        inputData += BFE_RX_OFFSET;
        mbuf_setdata(inputPkt, inputData, len);
        fNetif->inputPacket( inputPkt, len,
                             IONetworkInterface::kInputOptionQueuePacket );
        NET_STAT(inputPackets, 1);
        RX_TX_LOG("RX %d bytes at index %ld\n", len, index);

next:
        updateRxDescriptor(index);
        BFE_INC(index, BFE_RX_LIST_CNT);
    }

    fRxHeadIndex = index;
    fNetif->flushInputQueue();
}

//---------------------------------------------------------------------------

IOReturn BCM440X::setPromiscuousMode( bool active )
{
    UInt32 rxc = CSR_READ_4(sc, BFE_RXCONF);

    DEBUG_LOG("setPromiscuousMode %d\n", active);

    if (active)
        rxc |= BFE_RXCONF_PROMISC;
    else
        rxc &= ~BFE_RXCONF_PROMISC;

    CSR_WRITE_4(sc, BFE_RXCONF, rxc);
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn BCM440X::setMulticastMode( bool active )
{
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn BCM440X::setMulticastList( IOEthernetAddress * mcAddrList,
                                    UInt32              mcAddrCount )
{
    UInt32 index = 0;

    DEBUG_LOG("setMulticastList cnt=%ld\n", mcAddrCount);

    // Clear CAM
    CSR_WRITE_4(sc, BFE_CAM_CTRL, 0);

    // Write our local address
    bfe_cam_write(sc, fEnetAddr.bytes, index++);

    // Is there a limit?
    for ( UInt32 i = 0; i < mcAddrCount; i++ )
    {
        bfe_cam_write(sc, mcAddrList[i].bytes, index++);
    }

    // Enable CAM
    BFE_OR(sc, BFE_CAM_CTRL, BFE_CAM_ENABLE);
    
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void BCM440X::initRxFilter( void )
{
    UInt32 rxc = CSR_READ_4(sc, BFE_RXCONF);
    DEBUG_LOG("initRxFilter: rxconfig was 0x%lx\n", rxc);

    rxc &= ~(BFE_RXCONF_DBCAST   |
             BFE_RXCONF_ALLMULTI |
             BFE_RXCONF_PROMISC  |
             BFE_RXCONF_LPBACK   |
             BFE_RXCONF_FLOW     |
             BFE_RXCONF_ACCEPT   |
             BFE_RXCONF_RFILT    );

    // Clear CAM
    CSR_WRITE_4(sc, BFE_CAM_CTRL, 0);

    // Write our local address
    bfe_cam_write(sc, fEnetAddr.bytes, 0);

    // Enable CAM
    CSR_WRITE_4(sc, BFE_RXCONF, rxc);
    BFE_OR(sc, BFE_CAM_CTRL, BFE_CAM_ENABLE);
}

#pragma mark -
#pragma mark ••• Interrupt and Timer •••
#pragma mark -

//---------------------------------------------------------------------------

void BCM440X::interruptOccurred( IOInterruptEventSource * src, int count )
{
    IODebuggerLockState state;
    UInt32              istat;

    if (fInterruptEnabled == false)
        return;

    state = IOKernelDebugger::lock(this);        

    while (1)
    {
        istat = CSR_READ_4(sc, BFE_ISTAT) & BFE_IMASK_DEF;
        if (istat == 0)
            break;

        CSR_WRITE_4(sc, BFE_ISTAT, istat);

        if (istat & BFE_ISTAT_ERRORS)
        {
            UInt32 flag = CSR_READ_4(sc, BFE_DMATX_STAT);
            if (flag & BFE_STAT_EMASK)
                NET_STAT(inputErrors, 1);
    
            flag = CSR_READ_4(sc, BFE_DMARX_STAT);
            if (flag & BFE_RX_FLAG_ERRORS)
                NET_STAT(inputErrors, 1);

            resetCurrentActivationLevel();
            ETH_STAT(dot3RxExtraEntry.resets, 1);
        }

        if (istat & BFE_ISTAT_RX)
        {
            serviceRxInterrupt();
            ETH_STAT(dot3RxExtraEntry.interrupts, 1);
        }

        if (istat & BFE_ISTAT_TX)
        {
            serviceTxInterrupt();
            ETH_STAT(dot3TxExtraEntry.interrupts, 1);
        }
    }

    IOKernelDebugger::unlock(state);
}

//---------------------------------------------------------------------------

void BCM440X::watchdogTimeout( OSObject *           owner,
                               IOTimerEventSource * timer )
{
    BCM440X * me = (BCM440X *) owner;

    // Poll for link change, does this thing generate link change interrupts?

    if (me->fActivationLevel >= kActivationLevelKDP)
    {
        bool linkChanged;

        me->fPHY->checkForLinkChange(&linkChanged, kPHYLinkChangePoll);
        if (linkChanged)
            me->reportLinkStatus();

        // Update collision count (half duplex)

        if (me->fActivationLevel >= kActivationLevelBSD)
            me->fNetStats->collisions += CSR_READ_4(me->sc, BFE_TX_TCOLS);
    }

    if (me->fGarbageQueue->getSize())
    {
        IODebuggerLockState state = IOKernelDebugger::lock(me);
        me->fGarbageQueue->flush();
        IOKernelDebugger::unlock(state);
    }

    timer->setTimeoutMS(kWatchdogTimerPeriodMS);
}

#pragma mark -
#pragma mark ••• MDIO Interface •••
#pragma mark -

//---------------------------------------------------------------------------

bool BCM440X::mdiRead( void * owner, UInt32 phyAddr, UInt16 phyReg,
                       UInt16 * phyData )
{
    u_int32_t value;
    struct bfe_softc * sc = ((BCM440X *)owner)->sc;
    sc->bfe_phyaddr = phyAddr;
    if (bfe_readphy(sc, phyReg, &value) == 0)
    {
        *phyData = (PHYWord) value;
        return true;
    }
    else
    {
        *phyData = 0;
        return false;
    }
}

bool BCM440X::mdiWrite( void * owner, UInt32 phyAddr, UInt16 phyReg,
                        UInt16 phyData )
{
    struct bfe_softc * sc = ((BCM440X *)owner)->sc;
    sc->bfe_phyaddr = phyAddr;
    return (bfe_writephy(sc, phyReg, phyData) == 0);
}

#pragma mark -
#pragma mark ••• Media •••
#pragma mark -

//---------------------------------------------------------------------------

static bool addMedium( OSDictionary * table, 
                       UInt32 type, UInt32 speed, UInt32 refcon )
{
    IONetworkMedium * medium;
    bool              success = false;

    medium = IONetworkMedium::medium(type, speed, 0, refcon);
    if (medium)
    {
        success = IONetworkMedium::addMedium(table, medium);
        medium->release();
    }
    return success;
}

//---------------------------------------------------------------------------

bool BCM440X::publishMediaSupport( void )
{
    UInt32 supportMask;

    if (fPHY == 0) return false;
    if (fMediaDict) return true;

    fMediaDict = OSDictionary::withCapacity(5);
    if (!fMediaDict)
        return false;

    supportMask = fPHY->getLocalLinkSupportMask();

    addMedium(fMediaDict, kIOMediumEthernetAuto, 0, kMIILinkAutoNeg);

    if (supportMask & kMIILink10BASET)
        addMedium(fMediaDict,
                  kIOMediumEthernet10BaseT | kIOMediumOptionHalfDuplex,
                  10, kMIILink10BASET);

    if (supportMask & kMIILink10BASET_FD)
        addMedium(fMediaDict,
                  kIOMediumEthernet10BaseT | kIOMediumOptionFullDuplex,
                  10, kMIILink10BASET_FD);

    if (supportMask & kMIILink100BASETX)
        addMedium(fMediaDict,
                  kIOMediumEthernet100BaseTX | kIOMediumOptionHalfDuplex,
                  100, kMIILink100BASETX);

    if (supportMask & kMIILink100BASETX_FD)
        addMedium(fMediaDict,
                  kIOMediumEthernet100BaseTX | kIOMediumOptionFullDuplex,
                  100, kMIILink100BASETX_FD);

    return publishMediumDictionary(fMediaDict);
}

//---------------------------------------------------------------------------

void BCM440X::reportLinkStatus( void )
{
    IONetworkMedium * medium;
    UInt32            miiLink;

    miiLink = fPHY->getActiveLink();
    if (miiLink == kMIILinkNone)
    {
        // link down
        setLinkStatus(kIONetworkLinkValid, getSelectedMedium());
    }
    else
    {
        // link up
        medium = IONetworkMedium::getMediumWithIndex(fMediaDict, miiLink);
        setLinkStatus( kIONetworkLinkValid | kIONetworkLinkActive, medium);
    }
}

//---------------------------------------------------------------------------

IOReturn BCM440X::selectMedium( const IONetworkMedium * medium )
{
    IOReturn ior = kIOReturnSuccess;
    UInt32   miiLink;

    if (OSDynamicCast(IONetworkMedium, medium) == 0)
    {
        medium = IONetworkMedium::getMediumWithIndex(fMediaDict, kMIILinkAutoNeg);
        if (medium == 0) return kIOReturnBadArgument;
    }

    setSelectedMedium(medium);

    if (fSelectMediumOverride || fActivationLevel > kActivationLevelNone)
    {
        miiLink = medium->getIndex();
        ior = fPHY->programLink(miiLink, 5000);
        bfe_setupphy(sc);
        fPHY->checkForLinkChange();
        reportLinkStatus();
    }

    return ior;
}

#pragma mark -
#pragma mark ••• Power Management •••
#pragma mark -

//---------------------------------------------------------------------------

enum {
    kPowerStateOff = 0,
    kPowerStateOn,
    kPowerStateCount
};

IOReturn BCM440X::registerWithPolicyMaker( IOService * policyMaker )
{
    static IOPMPowerState powerStateArray[ kPowerStateCount ] =
    {
        { 1,0,0,0,0,0,0,0,0,0,0,0 },
        { 1,kIOPMDeviceUsable,kIOPMPowerOn,kIOPMPowerOn,0,0,0,0,0,0,0,0 }
    };

    return policyMaker->registerPowerDriver( this, powerStateArray,
                                                   kPowerStateCount );
}

//---------------------------------------------------------------------------

IOReturn BCM440X::setPowerState( unsigned long powerStateOrdinal,
                                 IOService *   policyMaker )
{
    return IOPMAckImplied;
}

#pragma mark -
#pragma mark ••• KDP (Polled Mode) Interface •••
#pragma mark -

enum { kDebuggerPollDelayUS = 10 };

//---------------------------------------------------------------------------

void BCM440X::receivePacket( void *   pkt_data,
                             UInt32 * pkt_size,
                             UInt32   timeoutMS )
{
    bfe_rxheader * rxh;
    UInt32         chipIndex;
    UInt16         flags, len;
    int            timeoutUS = timeoutMS * 1000;

    *pkt_size = 0;

    // Poll for a new packet arrival

    while (1)
    {
        chipIndex = (CSR_READ_4(sc, BFE_DMARX_STAT) & BFE_STAT_CDMASK) /
                    sizeof(struct bfe_desc);

        if (chipIndex != fRxHeadIndex)
        {
            rxh = (bfe_rxheader *) mbuf_data(fRxPacketArray[fRxHeadIndex]);
            len = OSSwapLittleToHostInt16(rxh->len);
            flags = OSSwapLittleToHostInt16(rxh->flags);
            if (len) break;  // packet arrived
        }

        if (timeoutUS <= 0)
            return;  // timed out, return to KDP

        IODelay(kDebuggerPollDelayUS);
        timeoutUS -= kDebuggerPollDelayUS;
    }

    if ((len >= kIOEthernetMinPacketSize) &&
        (len <= kIOEthernetMaxPacketSize) &&
        ((flags & (BFE_RX_FLAG_ERRORS|BFE_RX_FLAG_LAST)) == BFE_RX_FLAG_LAST))
    {
        const UInt8 * frameData;

        // Copy frame data from ring buffer to KDP buffer (1518 bytes max)

        frameData = (const UInt8 *) mbuf_data(fRxPacketArray[fRxHeadIndex]) +
                    BFE_RX_OFFSET;
        memcpy(pkt_data, frameData, len);
        *pkt_size = len;
    }

    updateRxDescriptor(fRxHeadIndex);
    BFE_INC(fRxHeadIndex, BFE_RX_LIST_CNT);
}

//---------------------------------------------------------------------------

void BCM440X::sendPacket( void * pkt_data, UInt32 pkt_size )
{
    UInt32     ctrl;
    bfe_desc * desc;

    if (!pkt_data || pkt_size > kIOEthernetMaxPacketSize) return;

    // We should always be able to grab a free descriptor since
    // outputPacket() will never fill up the ring.

    if (fTxDescBusy >= BFE_TX_LIST_CNT)
    {
        DEBUG_LOG("sendPacket: no TX descriptor\n");
        return;
    }

    // Copy the KDP data to our send buffer dedicated to debugger function

    desc = &fTxDescBase[fTxTailIndex];
    memcpy(mbuf_data(fKDPMbuf), pkt_data, pkt_size);

    // KDP packet will always consume a single fragment

    ctrl  = pkt_size & BFE_DESC_LEN;
    ctrl |= BFE_DESC_SOF | BFE_DESC_EOF | BFE_DESC_IOC;
    if (fTxTailIndex == BFE_TX_LIST_CNT - 1)
        ctrl |= BFE_DESC_EOT;

    OSWriteLittleInt32(&desc->bfe_ctrl, 0, ctrl);
    OSWriteLittleInt32(&desc->bfe_addr, 0,
                       fKDPMbufSeg.location + BFE_PCI_DMA);    

    fTxDescBusy++;
    BFE_INC(fTxTailIndex, BFE_TX_LIST_CNT);

    // Update hardware tail pointer

    CSR_WRITE_4(sc, BFE_DMATX_PTR, fTxTailIndex * sizeof(struct bfe_desc));

    // Wait until ring is completely drained

    do {
        UInt32  chipIndex;
        chipIndex  = CSR_READ_4(sc, BFE_DMATX_STAT) & BFE_STAT_CDMASK;
        chipIndex /= sizeof(struct bfe_desc);

        while ((fTxHeadIndex != chipIndex) && (fTxDescBusy > 0))
        {
            if (fTxPacketArray[fTxHeadIndex] != 0)
            {
                fGarbageQueue->enqueue(fTxPacketArray[fTxHeadIndex]);
                fTxPacketArray[fTxHeadIndex] = 0;
            }

            fTxDescBusy--;
            BFE_INC(fTxHeadIndex, BFE_TX_LIST_CNT);
        }

        IODelay(kDebuggerPollDelayUS);

    } while (fTxDescBusy > 0);
}

#pragma mark -
#pragma mark ••• BFE Driver Shim •••
#pragma mark -

//---------------------------------------------------------------------------

extern "C" {

__private_extern__
int pci_read_config( void * device, unsigned offset, unsigned size )
{
    assert(size == 4);
    BCM440X * me = (BCM440X *) device;
    return me->fPCIDevice->configRead32( offset );
}

__private_extern__
void pci_write_config( void * device, unsigned offset, unsigned value,
                       unsigned size )
{
    assert(size == 4);
    BCM440X * me = (BCM440X *) device;
    return me->fPCIDevice->configWrite32( offset, value );
}

} /* extern "C" */
