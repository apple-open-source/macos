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

/*
 * Copyright (c) 2001 Realtek Semiconductor Corp.  All rights reserved. 
 *
 * rtl8139.cpp
 *
 * HISTORY
 *
 * 09-Jul-01	Owen Wei at Realtek Semiconductor Corp. created for Realtek 
 *		RTL8139 family NICs.
 *
 */

#include "RTL8139.h"

#define super IOEthernetController

OSDefineMetaClassAndStructors( com_apple_driver_RTL8139,
                               IOEthernetController )

//---------------------------------------------------------------------------

bool RTL8139::init( OSDictionary * properties )
{
    if ( false == super::init(properties) ) return false;

    currentLevel       = kActivationLevel0;
    currentMediumIndex = MEDIUM_INDEX_NONE;

    return true;
}

//---------------------------------------------------------------------------

bool RTL8139::start( IOService * provider )
{
    bool success = false;
    
    DEBUG_LOG("start() ===>\n");

    do {
        // Start our superclass first.

        if ( false == super::start(provider) )
            break;

        // Save a reference to our provider.

        pciNub = OSDynamicCast( IOPCIDevice, provider );
        if ( 0 == pciNub )
            break;

        // Retain provider, released in free().

        pciNub->retain();

        // Open our provider.

        if ( false == pciNub->open(this) )
            break;

        // Initialize the driver's event sources.

        if ( false == initEventSources(provider) )
            break;

        // Allocate memory for descriptors. This function will leak memory
        // if called more than once. So don't do it.

        if ( false == allocateDescriptorMemory() )
            break;

        // Get the virtual address mapping of CSR registers located at
        // Base Address Range 0 (0x10).

        csrMap = pciNub->mapDeviceMemoryWithRegister( kIOPCIConfigBaseAddress0 );
        if ( 0 == csrMap )
            break;

        csrBase = (volatile void *) csrMap->getVirtualAddress();

        // Init PCI config space.

        if ( false == initPCIConfigSpace( pciNub ) )
            break;

        // Reset chip to bring it to a known state.

        if ( initAdapter( kResetChip ) == false )
        {
            IOLog("%s: initAdapter() failed\n", getName());
            break;
        }

        // Publish our media capabilities.

        phyProbeMediaCapability();
        if (false == publishMediumDictionary(mediumDict))
			break;

        success = true;
    }
    while ( false );

    // Close our provider, it will be re-opened on demand when
    // our enable() is called by a client.

    if (pciNub) pciNub->close(this);
    
    do {
        if (false == success)
            break;

        success = false;

        // Allocate and attach an IOEthernetInterface instance.

        if (false == attachInterface((IONetworkInterface **) &netif, false))
            break;

        // Optional: this driver supports kernel debugging.

        attachDebuggerClient( &debugger );

        // Trigger matching for clients of netif.

        netif->registerService();

        success = true;
    }
    while ( false );

    DEBUG_LOG("start() <===\n");
    
    return success;
}

//---------------------------------------------------------------------------

void RTL8139::stop( IOService * provider )
{
    DEBUG_LOG("stop() ===>\n");
    super::stop( provider );
    DEBUG_LOG("stop() <===\n");
}

//---------------------------------------------------------------------------

bool RTL8139::initEventSources( IOService * provider )
{
    DEBUG_LOG("initEventSources() ===>\n");

	IOWorkLoop * wl = getWorkLoop();
	if ( 0 == wl )
        return false;

	transmitQueue = getOutputQueue();
	if ( 0 == transmitQueue )
        return false;

	// Create an interrupt event source to handle hardware interrupts.

	interruptSrc = IOInterruptEventSource::interruptEventSource(this,
                   (IOInterruptEventAction) &RTL8139::interruptOccurred,
                   provider);

	if ( !interruptSrc ||
		 (wl->addEventSource(interruptSrc) != kIOReturnSuccess) )
		return false;

	// This is important. If the interrupt line is shared with other devices,
    // then the interrupt vector will be enabled only if all corresponding
    // interrupt event sources are enabled. To avoid masking interrupts for
    // other devices that are sharing the interrupt line, the event source
    // is enabled immediately. Hardware interrupt sources remain disabled.

    interruptSrc->enable();

	// Register a timer event source used as a watchdog timer.

	timerSrc = IOTimerEventSource::timerEventSource( this,
               (IOTimerEventSource::Action) &RTL8139::timeoutOccurred );

	if ( !timerSrc || (wl->addEventSource(timerSrc) != kIOReturnSuccess) )
		return false;

	// Create a dictionary to hold IONetworkMedium objects.

	mediumDict = OSDictionary::withCapacity(5);
	if ( 0 == mediumDict )
		return false;

	DEBUG_LOG("initEventSources() <===\n");

	return true;
}

//---------------------------------------------------------------------------
// Update PCI command register to enable the I/O mapped PCI memory range,
// and bus-master interface.

bool RTL8139::initPCIConfigSpace( IOPCIDevice * provider )
{
    UInt16 reg;
        
    DEBUG_LOG("pciConfigInit() ===>\n");

    reg = provider->configRead16( kIOPCIConfigCommand );

    reg |= ( kIOPCICommandBusMaster |
             kIOPCICommandIOSpace   |
             kIOPCICommandMemWrInvalidate );

    reg &= ~kIOPCICommandMemorySpace;

    provider->configWrite16( kIOPCIConfigCommand, reg );

    DEBUG_LOG("pciConfigInit() <===\n");

    return true;
}

//---------------------------------------------------------------------------

bool RTL8139::createWorkLoop( void )
{
    DEBUG_LOG("createWorkLoop() ===>\n");
    workLoop = IOWorkLoop::workLoop();
    DEBUG_LOG("createWorkLoop() <===\n");
    return ( workLoop != 0 );
}

//---------------------------------------------------------------------------

IOWorkLoop * RTL8139::getWorkLoop( void ) const
{
    // Override IOService::getWorkLoop() method to return the work loop
    // we allocated in createWorkLoop().

    DEBUG_LOG("getWorkLoop() ===>\n");
    DEBUG_LOG("getWorkLoop() <===\n");
	return workLoop;
}

//---------------------------------------------------------------------------

bool RTL8139::configureInterface( IONetworkInterface * netif )
{
    IONetworkData * data;

    DEBUG_LOG("configureInterface() ===>\n");

    if ( false == super::configureInterface(netif) )
        return false;
	
    // Get the generic network statistics structure.

    data = netif->getParameter( kIONetworkStatsKey );
    if ( !data || !(netStats = (IONetworkStats *) data->getBuffer()) ) 
    {
        return false;
    }

    // Get the Ethernet statistics structure.

    data = netif->getParameter( kIOEthernetStatsKey );
    if ( !data || !(etherStats = (IOEthernetStats *) data->getBuffer()) ) 
    {
        return false;
    }

    DEBUG_LOG("configureInterface() <===\n");
    return true;
}

//---------------------------------------------------------------------------

void RTL8139::free( void )
{
#define RELEASE(x) do { if(x) { (x)->release(); (x) = 0; } } while(0)

    DEBUG_LOG("free() ===>\n");

    if ( interruptSrc && workLoop )
    {
        workLoop->removeEventSource( interruptSrc );
    }

    RELEASE( netif        );
    RELEASE( debugger     );
    RELEASE( interruptSrc );
    RELEASE( timerSrc     );
    RELEASE( csrMap       );
    RELEASE( mediumDict   );
    RELEASE( pciNub       );
    RELEASE( workLoop     );

    if ( rx_md )
    {
        rx_md->complete();
        rx_md->release();
        rx_md = 0;
    }

    for ( int i = 0; i < kTxBufferCount; i++ )
    {
        if ( tx_md[i] )
        {
            tx_md[i]->complete();
            tx_md[i]->release();
            tx_md[i] = 0;
        }
    }

    super::free();

    DEBUG_LOG("free() <===\n");
}

//---------------------------------------------------------------------------
// Function: enableAdapter
//
// Enables the adapter & driver to the given level of support.

bool RTL8139::enableAdapter( UInt32 level )
{
    bool success = false;

    DEBUG_LOG("enableAdapter() ===>\n");
    DEBUG_LOG("enable level %ld\n", level);

    switch ( level ) 
    {
        case kActivationLevel1:

            // Open our provider (IOPCIDevice).

            if ( (0 == pciNub) || (false == pciNub->open(this)) )
                break;

            // Perform a full initialization sequence.

            if ( initAdapter( kFullInitialization ) != true )
                break;

            // Program the physical layer / transceiver.

            if ( selectMedium( getSelectedMedium() ) != kIOReturnSuccess )
                break;

            // Start the periodic timer.

            timerSrc->setTimeoutMS( kWatchdogTimerPeriod );

            // Unless we wait and ack PUN/LinkChg interrupts, the receiver
            // will not work. This creates a problem when DB_HALT debug
            // flag is set, since we will break into the debugger right
            // away after this function returns. But we won't be able to
            // attach since the receiver is deaf. I have no idea why this
            // workaround (discovered through experimentation) is needed.

            for ( int i = 0; i < 100; i++ )
            {
                UInt16 isr = csrRead16( RTL_ISR );
                if ( isr & R_ISR_PUN )
                {
                    csrWrite16( RTL_ISR, R_ISR_PUN );
                    DEBUG_LOG("cleared PUN interrupt %x in %d\n", isr, i);
                    break;
                }
                IOSleep(10);
            }

            success = true;
            break;
		
        case kActivationLevel2:

            // Start our transmit queue. Limit its capacity to
            // kTransmitQueueCapacity mbufs.

            transmitQueue->setCapacity( kTransmitQueueCapacity );
            transmitQueue->start();

            // Enable hardware interrupts.

            enableHardwareInterrupts();
            
            success = true;
            break;
    }

    if ( false == success )
        IOLog("enable level %ld failed\n", level);

    DEBUG_LOG("enableAdapter() <===\n");
    return success;
}

//---------------------------------------------------------------------------
// Function: disableAdapter
//
// Disables the adapter & driver to the given level of support.

bool RTL8139::disableAdapter( UInt32 level )
{
    bool success = false;

    DEBUG_LOG("disableAdapter() ===>\n");
    DEBUG_LOG("disable level %ld\n", level);

    switch ( level )
    {
        case kActivationLevel1:

            // Stop the timer event source.

            timerSrc->cancelTimeout();

            // Reset the hardware engine.

            initAdapter( kResetChip );

            // Report link status: unknown.

            phySetMedium( MEDIUM_INDEX_NONE );
            setLinkStatus( 0 );

            // Flush all packets held in the queue and prevent it
            // from accumulating any additional packets.

            transmitQueue->setCapacity( 0 );
            transmitQueue->flush();

            // Close our provider.

            if ( pciNub ) pciNub->close(this);

            success = true;
            break;

        case kActivationLevel2:

            // Disable hardware interrupt sources.

            disableHardwareInterrupts();

            // Stop the transmit queue. outputPacket() will not get called
            // after this. KDP calls sendPacket() to send a packet in polled
            // mode and that is unaffected by the state of the output queue.

            transmitQueue->stop();

            success = true;
            break;
    }

    if ( false == success )
        IOLog("disable level %ld failed\n", level);

    DEBUG_LOG("disableAdapter() <===\n");

    return success;
}

//---------------------------------------------------------------------------
// Function: setActivationLevel
//
// Sets the adapter's activation level.
//
// kActivationLevel0 : Adapter disabled.
// kActivationLevel1 : Adapter partially enabled to support KDP.
// kActivationLevel2 : Adapter completely enabled for KDP and BSD.

bool RTL8139::setActivationLevel( UInt32 level )
{
    bool    success = false;
    UInt32  nextLevel;

    DEBUG_LOG("setActivationLevel() ===>\n");
    DEBUG_LOG("---> CURRENT LEVEL: %ld DESIRED LEVEL: %ld\n",
              currentLevel, level);

    if (currentLevel == level) 
        return true;

    for ( ; currentLevel > level; currentLevel--) 
    {
        if ( (success = disableAdapter(currentLevel)) == false )
            break;
    }

    for ( nextLevel = currentLevel + 1; currentLevel < level;
          currentLevel++, nextLevel++ ) 
    {
        if ( (success = enableAdapter(nextLevel)) == false )
            break;
    }

    DEBUG_LOG("---> PRESENT LEVEL: %ld\n\n", currentLevel);
    DEBUG_LOG("setActivationLevel() <===\n");
    return success;
}

//---------------------------------------------------------------------------

IOReturn RTL8139::enable( IONetworkInterface * netif )
{
    DEBUG_LOG("enable(netif) ===>\n");
    
    if ( true == enabledByBSD )
    {
        DEBUG_LOG("enable() <===\n");
        return kIOReturnSuccess;
    }

    enabledByBSD = setActivationLevel( kActivationLevel2 );

    DEBUG_LOG("enable(netif) <===\n");

    return enabledByBSD ? kIOReturnSuccess : kIOReturnIOError;
}

//---------------------------------------------------------------------------

IOReturn RTL8139::disable( IONetworkInterface * /*netif*/ )
{
    DEBUG_LOG("disable(netif) ===>\n");

    enabledByBSD = false;

    setActivationLevel( enabledByKDP ?
                        kActivationLevel1 : kActivationLevel0 );

    DEBUG_LOG("disable(netif) <===\n");

	return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn RTL8139::enable( IOKernelDebugger * /*debugger*/ )
{
	if ( enabledByKDP || enabledByBSD )
    {
		enabledByKDP = true;
		return kIOReturnSuccess;
	}

	enabledByKDP = setActivationLevel( kActivationLevel1 );

	return enabledByKDP ? kIOReturnSuccess : kIOReturnIOError;
}

//---------------------------------------------------------------------------

IOReturn RTL8139::disable( IOKernelDebugger * /*debugger*/ )
{
	enabledByKDP = false;

	if ( enabledByBSD == false )
		setActivationLevel( kActivationLevel0 );

	return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void RTL8139::timeoutOccurred( IOTimerEventSource * timer )
{
    phyReportLinkStatus();
    timerSrc->setTimeoutMS( kWatchdogTimerPeriod );
}

//---------------------------------------------------------------------------

IOReturn RTL8139::setPromiscuousMode( bool enabled )
{
    DEBUG_LOG("setPromiscuousMode() ===>\n");

    if ( enabled )
    {
        reg_rcr |= R_RCR_AAP;  // allow all physical
        
        // Accept all multicast.
        csrWrite32( RTL_MAR0, 0xffffffff );
        csrWrite32( RTL_MAR4, 0xffffffff );
    }
    else
    {
        reg_rcr &= ~R_RCR_AAP;

        // Restore multicast hash filter.
        csrWrite32( RTL_MAR0, reg_mar0 );
        csrWrite32( RTL_MAR4, reg_mar4 );
    }
    
    csrWrite32( RTL_RCR, reg_rcr );

    DEBUG_LOG("setPromiscuousMode RTL_RCR = 0x%lx\n", reg_rcr );
    DEBUG_LOG("setPromiscuousMode() <===\n");

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn RTL8139::setMulticastMode( bool enabled )
{
    DEBUG_LOG("setMulticastMode() ===>\n");

    // Always accept multicast packets. The R_RCR_AM flag is always set
    // whenever the receiver is enabled. Nothing else is needed here.

    DEBUG_LOG("setMulticastMode RTL_RCR = 0x%lx\n", reg_rcr );
    DEBUG_LOG("setMulticastMode() <===\n");

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

static inline UInt32 rtl_ether_crc( int length, const unsigned char * data )
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

IOReturn RTL8139::setMulticastList( IOEthernetAddress * addrs, UInt32 count )
{
    DEBUG_LOG("setMulticastList() ===>\n");

	for ( UInt32 i = 0; i < count; i++, addrs++ )
    {
        int bit = rtl_ether_crc(6, (const UInt8 *) addrs) >> 26;
        if (bit < 32)
            reg_mar0 |= (1 << bit);
        else
            reg_mar4 |= (1 << (bit - 32));
    }

    csrWrite32( RTL_MAR0, reg_mar0 );
    csrWrite32( RTL_MAR4, reg_mar4 );

    DEBUG_LOG("setMulticastList() <===\n");

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void RTL8139::getPacketBufferConstraints(
                       IOPacketBufferConstraints * constraints ) const
{
    DEBUG_LOG("getPacketBufferConstraints() ===>\n");

    constraints->alignStart  = kIOPacketBufferAlign1; // no restriction
    constraints->alignLength = kIOPacketBufferAlign1; // no restriction

    DEBUG_LOG("getPacketBufferConstraints() <===\n");
}

//---------------------------------------------------------------------------

IOReturn RTL8139::getHardwareAddress( IOEthernetAddress * address )
{
    union {
        UInt8  bytes[4];
        UInt32 int32;
    } idr;

    DEBUG_LOG("getHardwareAddress() ===>\n");

    // Fetch the hardware address bootstrapped from EEPROM.

    idr.int32 = OSSwapLittleToHostInt32(csrRead32( RTL_IDR0 ));
    address->bytes[0] = idr.bytes[0];
    address->bytes[1] = idr.bytes[1];
    address->bytes[2] = idr.bytes[2];
    address->bytes[3] = idr.bytes[3];

    idr.int32 = OSSwapLittleToHostInt32(csrRead32( RTL_IDR4 ));
    address->bytes[4] = idr.bytes[0];
    address->bytes[5] = idr.bytes[1];

    DEBUG_LOG("getHardwareAddress() <===\n");
	return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOOutputQueue * RTL8139::createOutputQueue( void )
{
    DEBUG_LOG("createOutputQueue() ===>\n");
    DEBUG_LOG("createOutputQueue() <===\n");

    // An IOGatedOutputQueue will serialize all calls to the driver's
    // outputPacket() function with its work loop. This essentially
    // serializes all access to the driver and the hardware through
    // the driver's work loop, which simplifies the driver but also
    // carries a small performance cost (relatively for 10/100 Mb).

    return IOGatedOutputQueue::withTarget( this, getWorkLoop() );
}

//---------------------------------------------------------------------------

IOReturn RTL8139::selectMedium( const IONetworkMedium * medium )
{
    bool success;

    if ( medium == 0 )
        medium = phyGetMediumWithIndex( MEDIUM_INDEX_AUTO );
    if ( medium == 0 )
        return kIOReturnUnsupported;

	success = phySetMedium( medium );
    if (success)
    {
        setCurrentMedium( medium );
        setLinkStatus( kIONetworkLinkValid );
        phyReportLinkStatus( true );
    }

    return success ? kIOReturnSuccess : kIOReturnIOError;
}

//---------------------------------------------------------------------------
// Report human readable hardware information strings.

const OSString * RTL8139::newVendorString( void ) const
{
    DEBUG_LOG("newVendorString() ===>\n");
    DEBUG_LOG("newVendorString() <===\n");
    return OSString::withCString("Realtek");
}

const OSString * RTL8139::newModelString( void ) const
{
    const char * model = "8139";

    // FIXME: should do a better job of identifying the device type.

    DEBUG_LOG("newModelString() ===>\n");
    DEBUG_LOG("newModelString() <===\n");
    return OSString::withCString(model);
}

//---------------------------------------------------------------------------

IOReturn RTL8139::registerWithPolicyMaker( IOService * policyMaker )
{
    enum {
        kPowerStateOff = 0,
        kPowerStateOn,
        kPowerStateCount
    };

    static IOPMPowerState powerStateArray[ kPowerStateCount ] =
    {
        { 1,0,0,0,0,0,0,0,0,0,0,0 },
        { 1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0 }
    };

    IOReturn ret;

    ret = policyMaker->registerPowerDriver( this, powerStateArray,
                                            kPowerStateCount );
    
    return ret;
}

//---------------------------------------------------------------------------

IOReturn RTL8139::setPowerState( unsigned long powerStateOrdinal,
                                 IOService *   policyMaker )
{
    // Rely exclusively on enable() and disable() calls from our clients
    // who are power savvy, and will turn the controller off and back on
    // across system sleep. We just have to re-initialize the chip every
    // time the controller is enabled.
    //
    // FIXME: add support for wake on magic packet

    DEBUG_LOG("setPowerState state %d\n", powerStateOrdinal);

    return IOPMAckImplied;
}
