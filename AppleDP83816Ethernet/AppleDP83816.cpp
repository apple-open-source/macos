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

OSDefineMetaClassAndStructors( AppleDP83816Ethernet, IOEthernetController )

//---------------------------------------------------------------------------

bool CLASS::start( IOService * provider )
{
    bool success    = false;
    bool superStart = false;

    do {
        // Start our superclass first.

        if (false == super::start(provider))
            break;

        superStart = true;

        // Save a reference to our provider.

        fPCINub = OSDynamicCast(IOPCIDevice, provider);
        if (!fPCINub)
            break;

        // Retain provider, released in free().

        fPCINub->retain();

        // Open our provider.

        if (false == fPCINub->open(this))
            break;

        // Allocate mbuf cursors and other support objects.

        if (false == allocateSupportObjects(provider))
            break;

        // Initialize our harwdare's PCI config space.

        initPCIConfigSpace(fPCINub);

        // Get the virtual address mapping of registers located at
        // Base Address Range 1 (offset 0x14 - memory range).

        fRegMap = fPCINub->mapDeviceMemoryWithRegister(
                           kIOPCIConfigBaseAddress1);
        if (0 == fRegMap)
            break;

        fRegBase = (volatile void *) fRegMap->getVirtualAddress();

        // Detect the hardware type.

        if (probeHardware() == false)
        {
            ERROR_LOG("%s: probeHardware() failed\n", getName());
            break;
        }

        // Publish our media capabilities.

        phyProbeCapability();

        success = true;
    }
    while ( false );

    // Stop super on failure.

    if (!success && superStart)
    {
        super::stop(provider);
    }

    // Close our provider, it will be re-opened on demand when
    // our enable() is called by a client.

    if (fPCINub) fPCINub->close(this);
    
    do {
        if (false == success) break;
        success = false;

        // Allocate and attach an IOEthernetInterface instance.

        if (false == attachInterface((IONetworkInterface **)&fNetif, false))
            break;

        // Optional: this driver supports kernel debugging.
        // Reserved a copy buffer memory used during kernel debugging,
        // and resolve its physical address. Use mbuf for convenience.

        fKDPMbuf = allocatePacket(kIOEthernetMaxPacketSize);
        if (fKDPMbuf &&
            fRxMbufCursor->getPhysicalSegments(fKDPMbuf, &fKDPMbufSeg) == 1)
        {
            attachDebuggerClient(&fKDPNub);
        }

        // Start matching clients of netif.

        fNetif->registerService();

        success = true;
    }
    while ( false );
    
    return success;
}

//---------------------------------------------------------------------------

void CLASS::free( void )
{
#define RELEASE(x) do { if(x) { (x)->release(); (x) = 0; } } while(0)

    DEBUG_LOG("%s::free\n", getName());

    assert(fEnabledForKDP == false);
    assert(fEnabledForBSD == false);

    if (fInterruptSource && fWorkLoop)
    {
        fWorkLoop->removeEventSource(fInterruptSource);
    }

    RELEASE( fInterruptSource );
    RELEASE( fKDPNub          );
    RELEASE( fNetif           );
    RELEASE( fRxMbufCursor    );
    RELEASE( fTxMbufCursor    );
    RELEASE( fPCINub          );
    RELEASE( fWorkLoop        );
    RELEASE( fRegMap          );
    RELEASE( fMediumDict      );
    RELEASE( fWatchdogTimer   );
    RELEASE( fKDPQueue        );

    if (fKDPMbuf)
    {
        freePacket(fKDPMbuf);
        fKDPMbuf = 0;
        fKDPMbufSeg.location = 0;
    }

    super::free();
}

//---------------------------------------------------------------------------

void CLASS::initPCIConfigSpace( IOPCIDevice * pci )
{
    UInt16 cmd;
    UInt8  pmCapOffset;

    cmd = pci->configRead16( kIOPCIConfigCommand );

    cmd |= (kIOPCICommandBusMaster | kIOPCICommandMemorySpace);
    cmd &= ~kIOPCICommandIOSpace;

    pci->configWrite16(kIOPCIConfigCommand, cmd);

    // PCI power management (wake from D3 cold for Magic Packet)

    if (pci->findPCICapability( kIOPCIPowerManagementCapability,
                                &pmCapOffset ))
    {
        UInt16 pmc = pci->configRead16(pmCapOffset + 2);
        DEBUG_LOG("PCI PMC 0x%04x\n", pmc);

        if (pmc & kPCIPMCPMESupportFromD3Cold)
        {
            fMagicPacketSupported = true;
            DEBUG_LOG("PME from D3cold is supported\n");
        }
    }

    // Tell PCI family that the card supports D3 but not PME on D3.
    // We will inform PCI later on if PME Enable should be set, but
    // for now clear PME Enable for normal Rx operation and bypass
    // the WCSR wake up filters.

    if (pci->hasPCIPowerManagement(kPCIPMCD3Support))
    {
        pci->enablePCIPowerManagement(kPCIPMCSPowerStateD3);
    }
}

//---------------------------------------------------------------------------

bool CLASS::allocateSupportObjects( IOService * provider )
{
    fTransmitQueue = OSDynamicCast(IOOutputQueue, getOutputQueue());
    if (fTransmitQueue == 0)
        return false;

    fKDPQueue = IOPacketQueue::withCapacity(~0);
    if (!fKDPQueue)
        return false;

    // Allocate two Mbuf Cursors. One dedicated for transmit and
    // the other for receive. NOTE types are different.

    fRxMbufCursor = IOMbufLittleMemoryCursor::withSpecification(
                    kRxMaxBufferSize, 1);

    fTxMbufCursor = IOMbufNaturalMemoryCursor::withSpecification(
                    kTxMaxBufferSize, 1);

    if (!fRxMbufCursor || !fTxMbufCursor)
    {
        ERROR_LOG("%s: mbuf cursor allocation error\n", getName());
        return false;
    }

    IOWorkLoop * workLoop = (IOWorkLoop *) getWorkLoop();
    if (!workLoop)
    {
        ERROR_LOG("%s: no work loop\n", getName());
        return false;
    }

    // Create an interrupt event source to dispatch interrupts.
    // FIXME: Use interrupt filter event source

    fInterruptSource = IOInterruptEventSource::interruptEventSource(
                       this,
                       &AppleDP83816Ethernet::interruptHandler,
                       provider);

    if (!fInterruptSource ||
        (workLoop->addEventSource(fInterruptSource) != kIOReturnSuccess))
    {
        ERROR_LOG("%s: IOInterruptEventSource error\n", getName());
        return false;
    }

    fWatchdogTimer = IOTimerEventSource::timerEventSource(
                     this,
                     &AppleDP83816Ethernet::timeoutHandler);
    if (!fWatchdogTimer ||
        (workLoop->addEventSource(fWatchdogTimer) != kIOReturnSuccess))
    {
        ERROR_LOG("%s: IOTimerEventSource error\n", getName());
        return false;
    }

    // Very important. If the interrupt line is shared with other devices,
    // then the interrupt vector will be enabled only if all corresponding
    // interrupt event sources are enabled. To avoid masking interrupts for
    // other devices that are sharing the interrupt line, the event source
    // is enabled immediately.

    fInterruptSource->enable();

    return true;
}

//---------------------------------------------------------------------------

bool CLASS::createWorkLoop( void )
{
    fWorkLoop = IOWorkLoop::workLoop();
    return (fWorkLoop != 0);
}

IOWorkLoop * CLASS::getWorkLoop( void ) const
{
    return fWorkLoop;
}

IOOutputQueue * CLASS::createOutputQueue()
{
    return IOBasicOutputQueue::withTarget( this );
}

//---------------------------------------------------------------------------

const OSString * CLASS::newVendorString( void ) const
{
    return OSString::withCString("National Semiconductor");
}

const OSString * CLASS::newModelString( void ) const
{
    return OSString::withCString("DP83816");
}

//---------------------------------------------------------------------------

bool CLASS::configureInterface( IONetworkInterface * netif )
{
    IONetworkData * data;

    if (super::configureInterface(netif) == false)
        return false;

    // Get the generic network statistics structure

    data = netif->getParameter(kIONetworkStatsKey);
    if (!data || !(fNetStats = (IONetworkStats *) data->getBuffer()))
    {
        ERROR_LOG("%s: no network statistics\n", getName());
        return false;
    }
    bzero(fNetStats, sizeof(*fNetStats));

    // Get the Ethernet statistics structure

    data = netif->getParameter(kIOEthernetStatsKey);
    if (!data || !(fEtherStats = (IOEthernetStats *) data->getBuffer()))
    {
        ERROR_LOG("%s: no Ethernet statistics\n", getName());
        return false;
    }

    return true;
}

//---------------------------------------------------------------------------

bool CLASS::increaseActivationLevel( UInt32 newLevel )
{
    bool success = false;

    switch (newLevel)
    {
        case kActivationLevelKDP:

            if ((fPCINub == 0) || (fPCINub->open(this) == false))
            {
                ERROR_LOG("%s: open provider failed\n", getName());
                break;
            }

            // Allocate software runtime resources.

            if (swInit() == false)
            {
                ERROR_LOG("%s: swInit failed\n", getName());
                break;
            }

            // Issue hardware soft reset.

            if (hwReset() == false)
            {
                ERROR_LOG("%s: hwReset failed\n", getName());
                break;
            }

            // Configure hardware but keep it disabled.

            hwInit();

            // PHY and link layer programming.

            if (phySelectMedium(getSelectedMedium(), true) !=
                kIOReturnSuccess)
            {
                ERROR_LOG("%s: phySelectMedium error\n", getName());
            }

            // Enable transmit + receive.

            WriteReg(CR, CR_TXE | CR_RXE);

            fWatchdogTimer->setTimeoutMS(kWatchdogTimerPeriodMS);
            success = true;
            break;

        case kActivationLevelBSD:

            fTransmitQueue->setCapacity(kTransmitQueueCapacity);
            fTransmitQueue->start();

            enableHardwareInterrupts();

            success = true;
            break;
    }

    return success;
}

//---------------------------------------------------------------------------

bool CLASS::decreaseActivationLevel( UInt32 currentLevel )
{
    bool success = true;

    switch (currentLevel)
    {
        case kActivationLevelKDP:

            hwStop();

            fWatchdogTimer->cancelTimeout();

            // Report link valid and down.

            setLinkStatus( kIONetworkLinkValid );

            // Flush all packets held in the queue and prevent it
            // from accumulating any additional packets.

            fTransmitQueue->setCapacity(0);
            fTransmitQueue->flush();

            if (fPCINub) fPCINub->close(this);

            // Free all runtime resources. Must make sure DMA
            // engine is stopped before this to avoid hard to
            // locate mbuf/memory corruptions.

            swFree();

            break;

        case kActivationLevelBSD:

            fTransmitQueue->stop();

            // Don't need interrupts for KDP, mask all sources.

            disableHardwareInterrupts();

            break;
    }

    return success;
}

//---------------------------------------------------------------------------

bool CLASS::setActivationLevel( UInt32 level )
{
    bool success = false;

    DEBUG_LOG("setActivationLevel %lu\n", level);

    if (fActivationLevel == level) return true;

    for ( ; fActivationLevel > level; fActivationLevel--) 
    {
        if ((success = decreaseActivationLevel(fActivationLevel)) == false)
            break;
    }

    for ( ; fActivationLevel < level; fActivationLevel++) 
    {
        if ((success = increaseActivationLevel(fActivationLevel+1)) == false)
            break;
    }

    return success;
}

//---------------------------------------------------------------------------

IOReturn CLASS::enable( IONetworkInterface * netif )
{
    if (fEnabledForBSD) return kIOReturnSuccess;

    fEnabledForBSD = setActivationLevel(kActivationLevelBSD);

    return fEnabledForBSD ? kIOReturnSuccess : kIOReturnIOError;
}

//---------------------------------------------------------------------------

IOReturn CLASS::disable( IONetworkInterface * netif )
{
    fEnabledForBSD = false;

    setActivationLevel( fEnabledForKDP ?
                        kActivationLevelKDP :
                        kActivationLevelNone );

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn CLASS::enable( IOKernelDebugger * debugger )
{    
    if (fEnabledForKDP || fEnabledForBSD)
    {
        fEnabledForKDP = true;
        return kIOReturnSuccess;
    }

    fEnabledForKDP = setActivationLevel(kActivationLevelKDP);

    return fEnabledForKDP ? kIOReturnSuccess : kIOReturnIOError;
}

//---------------------------------------------------------------------------

IOReturn CLASS::disable( IOKernelDebugger * debugger )
{
    fEnabledForKDP = false;

    if (fEnabledForBSD == false)
        setActivationLevel(kActivationLevelNone);

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void CLASS::timeoutHandler( OSObject * owner, IOTimerEventSource * timer )
{
    CLASS * me = (CLASS *) owner;

    if (me->fKDPQueue->getSize())
    {
        IODebuggerLockState state = IOKernelDebugger::lock(me);
        me->fKDPQueue->flush();
        IOKernelDebugger::unlock(state);
    }

    if (me->fActivationLevel > kActivationLevelNone)
    {
        //me->fetchAndClearStatistics();
        me->phyPollLinkStatus();
        timer->setTimeoutMS( kWatchdogTimerPeriodMS );
    }
}

//---------------------------------------------------------------------------

IOReturn CLASS::getHardwareAddress( IOEthernetAddress * outAddr )
{
    bcopy(&fMacAddress, outAddr, kIOEthernetAddressSize);
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn CLASS::setPromiscuousMode( bool enabled )
{
    if ( enabled )
    {
        fRegRFCR |= (RFCR_AAM | RFCR_AAU);
    }
    else
    {
        fRegRFCR &= ~(RFCR_AAM | RFCR_AAU);
    }

    WriteReg(RFCR, fRegRFCR);

    DEBUG_LOG("%s: RFCR = 0x%08lx\n", __FUNCTION__, fRegRFCR);
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn CLASS::setMulticastMode( bool enabled )
{
    if ( enabled )
    {
        fRegRFCR |= RFCR_MHEN;
    }
    else
    {
        fRegRFCR &= ~RFCR_MHEN;
    }

    WriteReg(RFCR, fRegRFCR);

    DEBUG_LOG("%s: RFCR = 0x%08lx\n", __FUNCTION__, fRegRFCR);
    return kIOReturnSuccess;
}

static inline UInt32 drv_ether_crc( int length, const unsigned char * data )
{
    static unsigned const ethernet_polynomial = 0x04c11db7U;
    int crc = -1;

    while (--length >= 0) {
        unsigned char current_octet = *data++;
        for (int bit = 0; bit < 8; bit++, current_octet >>= 1)
            crc = (crc << 1) ^
                ((crc < 0) ^ (current_octet & 1) ? ethernet_polynomial : 0);
    }
    return crc;
}

IOReturn CLASS::setMulticastList( IOEthernetAddress * addrs, UInt32 count )
{
    UInt8 filterBytes[64];

    memset(filterBytes, 0, sizeof(filterBytes));

    for (UInt32 i = 0; i < count; i++, addrs++)
    {
        int bit = (drv_ether_crc(6, (const UInt8 *)addrs) >> 23) & 0x1FF;
        filterBytes[bit/8] |= (1 << (bit & 0x7));
    }

    for (int index = 0; index < 64; index += 2)
    {
        WriteRegFlush(RFCR, fRegRFCR | (index + 0x200));
        WriteRegFlush(RFDR, (filterBytes[index+1] << 8) | filterBytes[index]);
    }

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void CLASS::getPacketBufferConstraints(
            IOPacketBufferConstraints * constraints ) const
{
    // RX memory buffer should be 4 byte aligned
    constraints->alignStart  = kIOPacketBufferAlign4;
    constraints->alignLength = kIOPacketBufferAlign1;
}

//---------------------------------------------------------------------------

IOReturn CLASS::selectMedium( const IONetworkMedium * medium )
{
    IOReturn ret;

    if (fActivationLevel == kActivationLevelBSD)
    {
        fTransmitQueue->stop();
    }

    ret = phySelectMedium( medium, true );
    setSelectedMedium(medium);

    if (fActivationLevel == kActivationLevelBSD)
    {
        fTransmitQueue->start();
    }

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

enum {
    kPowerStateOff = 0,
    kPowerStateDoze,
    kPowerStateOn,
    kPowerStateCount
};

IOReturn CLASS::registerWithPolicyMaker( IOService * policyMaker )
{
    IOReturn ior;

    static IOPMPowerState powerStateArray[ kPowerStateCount ] =
    {
        { 1,0,0,0,0,0,0,0,0,0,0,0 },
        { 1,0,kIOPMDoze,kIOPMDoze,0,0,0,0,0,0,0,0 },
        { 1,kIOPMDeviceUsable,kIOPMPowerOn,kIOPMPowerOn,0,0,0,0,0,0,0,0 }
    };

    fCurrentPowerState = kPowerStateOn;

    ior = policyMaker->registerPowerDriver( this, powerStateArray,
                                                  kPowerStateCount );

    return ior;
}

//---------------------------------------------------------------------------

IOReturn CLASS::setPowerState( unsigned long powerStateOrdinal,
                               IOService *   policyMaker )
{
    if (!fPCINub || (powerStateOrdinal == fCurrentPowerState))
        return IOPMAckImplied;

    switch (powerStateOrdinal)
    {
        case kPowerStateOff:
            // Now that the driver knows if Magic Packet support was enabled,
            // tell PCI Family whether PME_EN should be set or not.

            hwSetMagicPacketEnable( fMagicPacketEnabled );

            fPCINub->hasPCIPowerManagement( fMagicPacketEnabled ?
                     kPCIPMCPMESupportFromD3Cold : kPCIPMCD3Support );
            break;

        case kPowerStateDoze:
            break;

        case kPowerStateOn:
            if (fCurrentPowerState == kPowerStateOff)
                initPCIConfigSpace(fPCINub);
            break;
    }

    fCurrentPowerState = powerStateOrdinal;
    return IOPMAckImplied;
}

//---------------------------------------------------------------------------

IOReturn CLASS::getPacketFilters( const OSSymbol * group,
                                  UInt32 *         filters ) const
{
    if ((group == gIOEthernetWakeOnLANFilterGroup) && fMagicPacketSupported)
    {
        *filters = kIOEthernetWakeOnMagicPacket;
        return kIOReturnSuccess;
    }
    return IOEthernetController::getPacketFilters( group, filters );
}

//---------------------------------------------------------------------------

IOReturn CLASS::setWakeOnMagicPacket( bool active )
{
    if (fMagicPacketSupported)
    {
        fMagicPacketEnabled = active;
        return kIOReturnSuccess;
    }
    return kIOReturnUnsupported;
}
