/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "3C90x.h"

#define super IOEthernetController
OSDefineMetaClassAndStructors( Apple3Com3C90x, IOEthernetController )

//---------------------------------------------------------------------------
// getAdapterType
//
// Determine the type of controller on the 3Com EtherLink NIC based on
// its 16-bit PCI device ID.

static const AdapterInfo *
getAdapterInfo( UInt16 pciDeviceID )
{
    UInt32 index = adapterInfoTableCount - 1;  // default

    for ( UInt32 i = 0; i < adapterInfoTableCount; i++ )
    {
        if ( adapterInfoTable[i].deviceID == pciDeviceID )
        {
            index = i;
            break;
        }
    }

    return &adapterInfoTable[index];
}

//---------------------------------------------------------------------------
// C to C++ glue.

static void
interruptOccurred( OSObject *               target,
                   IOInterruptEventSource * sender,
                   int                      count )
{
    ((Apple3Com3C90x *) target)->interruptHandler( sender );
}

static void
timeoutOccurred( OSObject *           target,
                 IOTimerEventSource * sender )
{
    ((Apple3Com3C90x *) target)->timeoutHandler( sender );
}

//---------------------------------------------------------------------------
// initPCIConfigSpace
//
// Enable the I/O mapped register range, and enable bus-master.

void Apple3Com3C90x::initPCIConfigSpace()
{
    UInt32  reg;

    // Set the PCI Bus Master bit. Enable I/O space decoding.
    // Although the Cyclone can use memory-mapped IO, we don't
    // take advantage of it.

    reg = _pciDevice->configRead16( kIOPCIConfigCommand );

    reg |= ( kIOPCICommandIOSpace         |
             kIOPCICommandBusMaster       |
             kIOPCICommandMemWrInvalidate |
             kIOPCICommandParityError     |
             kIOPCICommandSERR            );

    reg &= ~kIOPCICommandMemorySpace;

    _pciDevice->configWrite16( kIOPCIConfigCommand, reg );

#if 0
    // Manually set the PCI latency timer (CFLT) to the max value.
    // This is needed to workaround a data corruption issue when
    // the timer expires and a bus master operation is in progress.
    // Documented in the Vortex ERS.
    //
    // According to 3Com, this may still be required for the Boomerang.
    // But dumping the PCI configuration space under Win95, their
    // driver does not do this. Is this still needed?

    reg = _pciDevice->configRead32( kIOPCIConfigCacheLineSize );
    reg |= 0xFF00;    // max the 8-bit latency count
    _pciDevice->configWrite32( kIOPCIConfigCacheLineSize, reg );
#endif
}

//---------------------------------------------------------------------------
// createSupportObjects
//
// Create and initialize driver objects before the hardware is enabled.
// Returns true on sucess, false if allocation/initialization failed.

bool Apple3Com3C90x::createSupportObjects( IOService * provider )
{
	// This driver will allocate and use an IOGatedOutputQueue.

	_transmitQueue = getOutputQueue();
	if ( _transmitQueue == 0 ) return false;

	// Allocate a single IOMbufLittleMemoryCursor for both transmit and
    // receive. Safe since this driver is single-threaded. The maximum
    // number of segments defaults to 1, but can be changed later.

	_mbufCursor = IOMbufLittleMemoryCursor::withSpecification( 1522, 1 );
	if ( _mbufCursor == 0 )
		return false;

	// Get a reference to our own workloop.

	IOWorkLoop * myWorkLoop = (IOWorkLoop *) getWorkLoop();
	if ( myWorkLoop == 0 )
		return false;

	// Create and attach an interrupt event source to the work loop.

	_interruptSrc = IOInterruptEventSource::interruptEventSource(
                    this,
                    (IOInterruptEventAction) interruptOccurred,
                    provider );

	if ( (_interruptSrc == 0 ) ||
		 (myWorkLoop->addEventSource(_interruptSrc) != kIOReturnSuccess) )
    {
		return false;
    }

    // This is important. If the interrupt line is shared with other PCI
    // devices, and the interrupt source is not enabled. Then the interrupt
    // line will be masked, and the other devices on the same line will not
    // get interrupts. To avoid this, the interrupt source is enabled right
    // away.

    _interruptSrc->enable();

	// Register a timer event source. This is used as a periodic timer to
    // monitor transmitter watchdog and link status.

	_timerSrc = IOTimerEventSource::timerEventSource(
                this,
                (IOTimerEventSource::Action) timeoutOccurred );

	if ( (_timerSrc == 0) ||
		 (myWorkLoop->addEventSource(_timerSrc) != kIOReturnSuccess) )
    {
		return false;
    }

	return true;
}

//---------------------------------------------------------------------------
// start

bool Apple3Com3C90x::start( IOService * provider )
{
    UInt32         pciReg;
    UInt8          irqNumber;

    LOG_DEBUG("%s::%s rev:16\n", getName(), __FUNCTION__);

    if ( super::start(provider) == false )
        return false;

    // Cache our provider.

    _pciDevice = OSDynamicCast( IOPCIDevice, provider );
    if ( _pciDevice == 0 )
        goto fail;

    // Retain provider, released later in free().

    _pciDevice->retain();

    // Open our provider before using its services.

    if ( _pciDevice->open(this) == false )
        goto fail;

    // Initialize the PCI config space.

	initPCIConfigSpace();

    // Determine the type of EtherLinkXL card.

    pciReg = _pciDevice->configRead32( kIOPCIConfigVendorID );

    _adapterInfo = getAdapterInfo( (pciReg >> 16) & 0xffff );

    // For 3C90xB-TX NICs, determine the type of ASIC. This allows
    // us to do the right thing for media auto-negotiation functions.

    _asicType = kASIC_Unknown;

    if ( _adapterInfo->deviceID == 0x9055 )
    {
        UInt8 asicRev;

        pciReg = _pciDevice->configRead32( kIOPCIConfigRevisionID );

        _asicType = ( (pciReg >> 5) & 0x07 );

        switch ( _asicType )
        {
            case kASIC_40_0502_00x:
                asicRev  = pciReg & 0x1f;
                IOLog("(40-0502-00x, Rev:%x)", asicRev);
                break;

            case kASIC_40_0483_00x:
                asicRev  = (pciReg >> 2) & 0x7;
                IOLog("(40-0483-00x, Rev:%x)", asicRev);
                break;

            case kASIC_40_0476_001:
                asicRev  = (pciReg >> 2) & 0x7;
                IOLog("(40-0476-001, Rev:%x)", asicRev);
                break;

            default:
                IOLog("(Unknown ASIC:%02lx)", pciReg);
                _asicType = kASIC_40_0502_00x;  // default assignment
        }
    }

    // Get the I/O space registers at PCI base address register 0.

    pciReg  = _pciDevice->configRead32( kIOPCIConfigBaseAddress0 );
    _ioBase = pciReg & ~0x01;  // mask I/O space indicator at bit 0

    LOG_DEBUG("%s: PCI BAR0 = %x\n", getName(), _ioBase);

    // Initialize instance variables.

    irqNumber      = _pciDevice->configRead32( kIOPCIConfigInterruptLine );
    _window        = ((UInt8) -1);  // invalidate current window
    _rxFilterMask  = kFilterIndividual | kFilterBroadcast;

    // Create supporting objects.

    if ( createSupportObjects( provider ) == false )
    {
        IOLog("%s: createSupportObjects failed\n", getName());
        goto fail;
    }

    // Reset the adapter.

    if ( resetAndEnable( false ) == false )
    {
        IOLog("%s: resetAndEnable failed\n", getName());
        goto fail;
    }

    // Parse the onboard EEPROM
    
    if ( parseEEPROM() == false )
    {
        IOLog("%s: EEPROM parse error\n", getName());
        goto fail; 
    }

    // Get media ports supported by the adapter.

	_mediumDict = OSDictionary::withCapacity( 5 );
	if ( _mediumDict == 0 )
    {
		goto fail;
    }
    probeMediaSupport();
    publishMediaCapability( _mediumDict );

    // Get user configurable settings.

    getDriverSettings();

    // Allocate memory for packet buffers and DMA descriptors.

    if ( allocateMemory() == false )
    {
        goto fail;
    }

    // Allocate a single cluster mbuf for KDB, also used by the media
    // auto-selection logic.

    _kdpMbuf = allocatePacket( 1 );
    if ( _kdpMbuf == 0 )
    {
        IOLog("%s: KDB mbuf allocation failed\n", getName());
        goto fail;
    }

    // Announce the hardware model.

    IOLog("%s: 3Com EtherLink %s Port 0x%0x IRQ %d\n", getName(),
          _adapterInfo->name, _ioBase, irqNumber);

    // Close our provider, it will be re-opened on demand when
    // our enable() method is called.

    if ( _pciDevice ) _pciDevice->close(this);

    // Create and attach nubs for BSD and KDP clients.

    if ( attachInterface((IONetworkInterface **) &_netif) == false )
        goto fail;

	attachDebuggerClient( &_debugger );

    return true;

fail:
    if ( _pciDevice ) _pciDevice->close(this);

    return false;
}
//---------------------------------------------------------------------------
// createWorkLoop
//
// Override IONetworkController::createWorkLoop() method to create and return
// a new work loop object.

bool Apple3Com3C90x::createWorkLoop()
{
    _workLoop = IOWorkLoop::workLoop();

    return ( _workLoop != 0 );
}

//---------------------------------------------------------------------------
// getWorkLoop
//
// Override IOService::getWorkLoop() method and return a reference to our
// work loop.

IOWorkLoop * Apple3Com3C90x::getWorkLoop() const
{
    return _workLoop;
}

//---------------------------------------------------------------------------
// getDriverSettings
//
// Parses the driver settings from the config table.

void Apple3Com3C90x::getDriverSettings()
{
    // MacControl settings.

    _flowControlEnabled   = true;
    _extendAfterCollision = false;

    // Set threshold defaults.

    _dnBurstThresh     = 256;
    _dnPriorityThresh  = 128;
    _txReclaimThresh   = 128;
    _txStartThresh_10  = 128;
    _txStartThresh_100 = 512;
    _upBurstThresh     = 256;
    _upPriorityThresh  = 128;

    // Get storeAndForward value. This overrides the setting for
    // txStartThresh.

    _storeAndForward = false;

    // Get the size of the transmit/receive descriptor rings.

    _txRingSize     = kTxRingSize;
    _rxRingSize     = kRxRingSize;
    _txIntThreshold = _txRingSize / 4;

    LOG_DEBUG("%s: Ring sizes: transmit=%ld(%ld), receive=%ld\n",
              getName(), _txRingSize, _txIntThreshold, _rxRingSize);
}

//---------------------------------------------------------------------------
// checkEEPROMChecksum
//
// Scan the EEPROM and compute the checksum to match against the
// checksum byte store in the last word (lower byte).
//
// Returns true on success. (Always returns YES)

bool Apple3Com3C90x::checkEEPROMChecksum()
{
    UInt16  data;
    int     csum = 0;
    const   UInt32 wordCount = (_adapterInfo->type == kAdapterType3C90x) ?
                                kEEPROMSizeBoomerang : kEEPROMSizeCyclone;

    for ( UInt32 i = 0; i < wordCount; i++ )
    { 
        data = readEEPROM(i);
        if (i == (wordCount - 1)) data &= 0xff;  // use lower byte only
        csum ^= data;
#ifdef DUMP_EEPROM
        IOLog("%02x: 0x%04x\n", i, data);
#endif DUMP_EEPROM
    }

    // compute the byte-wide XOR checksum. We XOR all the EEPROM bytes and
    // the checksum byte itself. This should result in zero for a good
    // checksum.

    csum = (csum ^ (csum >> 8)) & 0xff;

    // For bad checksum, we print a warning but allow the driver to proceed.
    // FIXME: For 3C90xC cards, the checksum location has changed.

    if ( csum != 0 && _adapterInfo->type != kAdapterType3C90xC )
        IOLog("%s: WARNING: EEPROM checksum mismatch\n", getName());

    return true;
}

//---------------------------------------------------------------------------
// parseEEPROM
//
// Parses the EEPROM. Verify its checksum, and extract the station's
// ethernet address. Returns true on success.

bool Apple3Com3C90x::parseEEPROM()
{
    // Verify EEPROM checksum.

    if ( checkEEPROMChecksum() == false )
    {
        IOLog("%s: Invalid EEPROM checksum\n", getName());
        return false;
    }

    // Get station address.

    getStationAddress( &_etherAddress );

    LOG_DEBUG("%s: Station Address = %02x:%02x:%02x:%02x:%02x:%02x\n",
              getName(),
              _etherAddress.bytes[0],
              _etherAddress.bytes[1],
              _etherAddress.bytes[2],
              _etherAddress.bytes[3],
              _etherAddress.bytes[4],
              _etherAddress.bytes[5]);

    // Get available media from MediaOptions register.
    // Note that MediaOptions was called ResetOptions in the Boomerang
    // documentation. Now they added a new register called ResetOptions.
    // Confusing enough?

    _mediaOptions = getMediaOptions();
    LOG_DEBUG("%s: Media options: 0x%04x\n", getName(), _mediaOptions);

    // Get adapter's default medium from the xcvrSelect field in the
    // InternalConfig register.

    _defaultPort = (MediaPort) ReadBitField( InternalConfig, XcvrSelect );
    LOG_DEBUG("%s: Default media port: %s\n", getName(), 
              mediaPortTable[_defaultPort].name);

    return true;
}

//---------------------------------------------------------------------------
// resetAndEnable
//
// Resets the adapter. If enable is true, the adapter will be enabled
// to transmit and receive packets.

bool Apple3Com3C90x::resetAndEnable( bool enable )
{
    disableAdapterInterrupts();

    stopPeriodicTimer();

    sendCommandWait( GlobalReset );

	resetAdapter();

    setLinkStatus( kIONetworkLinkValid );

    if ( enable )
    {
        LOG_DEBUG("%s: enabling adapter\n", getName());

        // Enable NIC, but no interrupt sources are enabled.

        resetAndEnableAdapter( false );

        // Init PHY and media registers.

        resetMedia( getSelectedMedium() );

        // Initialize watchdog counters.

        bzero( _oldWDCounters, sizeof(_oldWDCounters) );
        bzero( _newWDCounters, sizeof(_newWDCounters) );
        _oldWDCounters[ kWDInterruptsRetired ] = 1;

        _watchdogTimerEnabled    = true;
        _linkMonitorTimerEnabled = true;

        // Wait a bit and see if the link will come up before the
        // driver is in active use.

        for ( int loops = 10;
              loops && ( (_media.linkStatus & kIONetworkLinkActive) == 0 );
              loops-- )
        {
            if ( _media.mediaPort == kMediaPortAuto ) break;
            monitorLinkStatus();
            IOSleep( 100 );
        }

        // Start the periodic timer.

        startPeriodicTimer();

        // Enable NIC interrupt sources.
        
        enableAdapterInterrupts();
    }

    return true;
}

//---------------------------------------------------------------------------
// free
//
// Release any resource that may have been allocated by the driver.
// No need to shut down the hardware, the family must have called
// the driver's disable() method beforehand.

void Apple3Com3C90x::free()
{
#define RELEASE(x) do { if(x) { (x)->release(); (x) = 0; } } while(0)

	RELEASE( _debugger     );
    RELEASE( _netif        );
	RELEASE( _interruptSrc );
	RELEASE( _timerSrc     );
	RELEASE( _mbufCursor   );
    RELEASE( _pciDevice    );
    RELEASE( _workLoop     );
    RELEASE( _mediumDict   );
    
    if ( _txRing )
    {
        for ( UInt32 i = 0; i < _txRingSize; i++ )
        {
            if ( _txRing[i].drvMbuf )
            {
                freePacket( _txRing[i].drvMbuf );
                _txRing[i].drvMbuf = 0;
            }
        }
    }

    if ( _rxRing )
    {
        for ( UInt32 i = 0; i < _rxRingSize; i++ )
        {
            if ( _rxRing[i].drvMbuf )
            {
                freePacket( _rxRing[i].drvMbuf );
                _rxRing[i].drvMbuf = 0;
            }
        }
    }

    if ( _kdpMbuf )
    {
        freePacket( _kdpMbuf );
        _kdpMbuf = 0;
    }

    freeDescMemory( &_txRingMem );
    freeDescMemory( &_rxRingMem );

	super::free();
}

//---------------------------------------------------------------------------
// timeoutHandler

void Apple3Com3C90x::timeoutHandler( IOTimerEventSource * src )
{
    if ( _driverEnableCount == 0 ) return;

    reserveDebuggerLock();

    if ( _linkMonitorTimerEnabled )
    {
        monitorLinkStatus();
    }

    if ( _watchdogTimerEnabled )
    {
        // If no interrupts were retired over two watchdog intervals when
        // there are interrupts pending, then something is wrong.

        if ( _oldWDCounters[ kWDInterruptsPending ] &&
          (( _oldWDCounters[ kWDInterruptsRetired ] +
             _newWDCounters[ kWDInterruptsRetired ] ) == 0 ) )
        {
            IOLog("%s: Watchdog timer expired\n", getName());

            // Global Reset
            //
            // This is a very drastic measure, but it may be the only
            // way to recover from a hung engine. TxReset alone is not
            // quite enough.
            //
            // Observed timeouts when using the 3C905-TX cards
            // at either 10 or 100 Mbps.
            //
            // Re-initialize the card and driver.

            if ( resetAndEnable(true) == false )
            {
                IOLog("%s: WatchDog: resetAndEnable failed\n", getName());
            }

            _netStats->outputErrors++;
            _etherStats->dot3TxExtraEntry.timeouts++;
            _etherStats->dot3TxExtraEntry.resets++;
        }
        else
        {
            bcopy( _newWDCounters, _oldWDCounters, sizeof(_newWDCounters) );
            bzero( _newWDCounters, sizeof(_newWDCounters) );
        }
    }

    releaseDebuggerLock();

    // Re-arm the timer for the next interval.

	src->setTimeoutMS( kPeriodicTimerMSInterval );
}

//---------------------------------------------------------------------------
// setMulticastMode.

IOReturn Apple3Com3C90x::setMulticastMode( bool enable )
{
    reserveDebuggerLock();

    if ( enable )
    {
        if ( _adapterInfo->type >= kAdapterType3C90xB )
            _rxFilterMask |= kFilterMulticastHash;
        else
            _rxFilterMask |= kFilterMulticast;
    }
    else
    {
        _rxFilterMask &= ~( kFilterMulticast | kFilterMulticastHash );
    }

    sendCommand( SetRxFilter, _rxFilterMask );

	releaseDebuggerLock();

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// setMulticastList.

IOReturn
Apple3Com3C90x::setMulticastList( IOEthernetAddress * addrs, UInt32 count )
{
    reserveDebuggerLock();

	setupMulticastHashFilter( addrs, count );

	releaseDebuggerLock();

	return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// setPromiscuousMode.

IOReturn Apple3Com3C90x::setPromiscuousMode( bool enable )
{
    reserveDebuggerLock();

    if ( enable )
        _rxFilterMask |= kFilterPromiscuous;
    else
        _rxFilterMask &= ~kFilterPromiscuous;

    sendCommand( SetRxFilter, _rxFilterMask );

    releaseDebuggerLock();

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// getHardwareAddress
//
// Return the Ethernet address burned in ROM.

IOReturn Apple3Com3C90x::getHardwareAddress( IOEthernetAddress * addr )
{
	bcopy( &_etherAddress, addr, sizeof(*addr) );
	return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// createOutputQueue
//
// Allocate an IOGatedOutputQueue instance.

IOOutputQueue * Apple3Com3C90x::createOutputQueue()
{
    // Return a gated queue. This will serialize the transmission with
    // the work loop thread. The superclass is responsible for releasing
    // this object.

	return IOGatedOutputQueue::withTarget( this, getWorkLoop() );
}

//---------------------------------------------------------------------------
// enable/disable (IONetworkInterface)

IOReturn Apple3Com3C90x::enable( IONetworkInterface * netif )
{
    LOG_DEBUG("%s::%s(netif)\n", getName(), __FUNCTION__);

    if ( _driverEnableCount == 0 )
    {
        if ( _pciDevice->open(this) != true )
        {
            return kIOReturnError;
        }

        if ( resetAndEnable( true ) == false )
        {
            _pciDevice->close(this);
            return kIOReturnIOError;
        }
    }

    _driverEnableCount++;

    // Enable hardware interrupts.

	enableAdapterInterrupts();

	_transmitQueue->setCapacity( 512 );
	_transmitQueue->start();

    _netifEnabled = true;

    return kIOReturnSuccess;
}

IOReturn Apple3Com3C90x::disable( IONetworkInterface * netif )
{
    LOG_DEBUG("%s::%s(netif)\n", getName(), __FUNCTION__);

    _netifEnabled = false;
    
    _transmitQueue->stop();
    _transmitQueue->setCapacity( 0 );
    _transmitQueue->flush();

	disableAdapterInterrupts();

    if ( _driverEnableCount && ( --_driverEnableCount == 0 ) )
    {
        resetAndEnable( false );

        if ( _pciDevice )
        {
            _pciDevice->close( this );
        }
    }

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// enable/disable (IOKernelDebugger)

IOReturn Apple3Com3C90x::enable( IOKernelDebugger * /*debugger*/ )
{
    LOG_DEBUG("%s::%s(debugger)\n", getName(), __FUNCTION__);

    if ( _driverEnableCount == 0 )
    {
        if ( _pciDevice->open( this ) != true )
        {
            return kIOReturnError;
        }
    
        if ( resetAndEnable( true ) == false )
        {
            _pciDevice->close(this);
            return kIOReturnIOError;
        }
    }
    _driverEnableCount++;
    return kIOReturnSuccess;
}

IOReturn Apple3Com3C90x::disable( IOKernelDebugger * /*debugger*/ )
{
    LOG_DEBUG("%s::%s(debugger)\n", getName(), __FUNCTION__);
    
    if ( _driverEnableCount && ( --_driverEnableCount == 0 ) )
    {
        resetAndEnable( false );

        if ( _pciDevice )
        {
            _pciDevice->close( this );
        }
    }
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// startPeriodicTimer

void Apple3Com3C90x::startPeriodicTimer()
{
    if ( _linkMonitorTimerEnabled || _watchdogTimerEnabled )
    {
        if ( _timerSrc ) _timerSrc->setTimeoutMS( kPeriodicTimerMSInterval );
    }
}

//---------------------------------------------------------------------------
// stopPeriodicTimer

void Apple3Com3C90x::stopPeriodicTimer()
{
	if ( _timerSrc ) _timerSrc->cancelTimeout();
}

//---------------------------------------------------------------------------
// configureInterface

bool Apple3Com3C90x::configureInterface( IONetworkInterface * netif )
{
	IONetworkData * data;

	if ( super::configureInterface(netif) == false )
		return false;

	// Get the generic network statistics structure.

	data = netif->getNetworkData( kIONetworkStatsKey );
	if (!data || !(_netStats = (IONetworkStats *) data->getBuffer()))
    {
		return false;
	}

	// Get the Ethernet statistics structure.

	data = netif->getNetworkData( kIOEthernetStatsKey );
	if (!data || !(_etherStats = (IOEthernetStats *) data->getBuffer()))
    {
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------
// selectMedium

IOReturn
Apple3Com3C90x::selectMedium(const IONetworkMedium * medium)
{
	resetMedia( medium );
    
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn Apple3Com3C90x::registerWithPolicyMaker( IOService * policyMaker )
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

    ret = policyMaker->registerPowerDriver( this,
                                            powerStateArray,
                                            kPowerStateCount );
    
    return ret;
}

//---------------------------------------------------------------------------

IOReturn Apple3Com3C90x::setPowerState( unsigned long powerStateOrdinal,
                                        IOService *   policyMaker )
{
    return IOPMAckImplied;
}
                                    