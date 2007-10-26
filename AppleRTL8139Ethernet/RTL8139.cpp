/*
 * Copyright (c) 1998-2003, 2006 Apple Computer, Inc. All rights reserved.
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

	OSDefineMetaClassAndStructors( RTL8139, IOEthernetController )	;

#pragma mark -
#pragma mark еее Event Logging еее
#pragma mark -

#if USE_ELG
void RTL8139::AllocateEventLog( UInt32 size )
{
	mach_timespec_t		time;
	IOByteCount			length;


    fpELGMemDesc = IOBufferMemoryDescriptor::withOptions(	kIOMemoryPhysicallyContiguous,
															kEvLogSize,
															PAGE_SIZE );
	if ( !fpELGMemDesc )
	{
		kprintf( "AllocateEventLog - RTL8139 evLog allocation failed " );
		return;
	}

	fpELGMemDesc->prepare( kIODirectionNone );
	fpELG = (elg*)fpELGMemDesc->getBytesNoCopy();
	bzero( fpELG, kEvLogSize );
	fpELG->physAddr	= fpELGMemDesc->getPhysicalSegment64( 0, &length );	// offset: 0; length

	fpELG->evLogBuf		= (UInt8*)fpELG + sizeof( struct elg );
	fpELG->evLogBufe	= (UInt8*)fpELG + kEvLogSize - 0x20; // ??? overran buffer?
	fpELG->evLogBufp	= fpELG->evLogBuf;
//	fpELG->evLogFlag	 = 0xFeedBeef;	// continuous wraparound
	fpELG->evLogFlag	 = 0x03330333;	// > kEvLogSize - don't wrap - stop logging at buffer end
//	fpELG->evLogFlag	 = 0x0099;		// < #elements - count down and stop logging at 0
//	fpELG->evLogFlag	 = 'step';		// stop at each ELG

	IOGetTime( &time );
	fpELG->startTimeSecs	= time.tv_sec;

	IOLog( "\033[32mRTL8139::AllocateEventLog - buffer=%8x phys=%16llx \033[0m \n",
							(unsigned int)fpELG, fpELG->physAddr );
	return;
}/* end AllocateEventLog */


void RTL8139::EvLog( UInt32 a, UInt32 b, UInt32 ascii, char* str )
{
	register UInt32		*lp;			/* Long pointer					*/
	register elg		*pe = fpELG;	/* pointer to elg structure		*/
	mach_timespec_t		time;
	UInt32				lefty;

//	kprintf( "%08lx  %08lx ", a, b );	kprintf( str ); kprintf( "\n" );

	if ( pe->evLogFlag == 0 )
	{
		pe->lostEvents++;				/* count this as a lost event	*/
		return;
	}

	IOGetTime( &time );

	if ( pe->evLogFlag <= kEvLogSize / 0x10 )
		--pe->evLogFlag;
	else if ( pe->evLogFlag == 0xDebeefed )	/// ??? do this in a separate routine
	{
		for ( lp = (UInt32*)pe->evLogBuf; lp < (UInt32*)pe->evLogBufe; lp++ )
			*lp = 0xDebeefed;
		pe->evLogBufp	= pe->evLogBuf;		// rewind
		pe->evLogFlag	= 0x03330333;		// stop at end
	}

			/* handle buffer wrap around if any */

	if ( pe->evLogBufp >= pe->evLogBufe )
	{
		pe->evLogBufp = pe->evLogBuf;
		pe->wrapCount++;
		if ( pe->evLogFlag != 0xFeedBeef )	// make 0xFeedBeef a symbolic ???
		{
			pe->evLogFlag = 0;				/* stop tracing if wrap undesired	*/
		//	IOFlushProcessorCache( kernel_task, (IOVirtualAddress)fpELG, kEvLogSize );
			return;
		}
		pe->startTimeSecs = time.tv_sec;
	}

	lp = (UInt32*)pe->evLogBufp;
	pe->evLogBufp += 0x10;

		/* compose interrupt level with 3 byte time stamp:	*/

//	if ( fpRegs )		// don't read cell regs if clock disabled
//		 lefty = OSSwapInt32( fpRegs->RxCompletion ) << 24;
//	else lefty = 0xFF000000;
	lefty = time.tv_sec << 24;				// put seconds on left for now.
	*lp++ = lefty | (time.tv_nsec >> 10);	// ~ 1 microsec resolution
	*lp++ = a;
	*lp++ = b;
	*lp	  = ascii;

	return;
}/* end EvLog */


UInt32 RTL8139::Alrt( UInt32 a, UInt32 b, UInt32 ascii, char* str )
{
	char		work [ 256 ];
	char		*bp = work;
	UInt8		x;
	int			i;


	EvLog( a, b, ascii, str );
	EvLog( '****', '****', 'Alrt', "*** Alrt" );

	*bp++ = '{';						// prepend p1 in hex:
	for ( i = 7; i >= 0; --i )
	{
		x = a & 0x0F;
		if ( x < 10 )
			 x += '0';
		else x += 'A' - 10;
		bp[ i ] = x;
		a >>= 4;
	}
	bp += 8;

	*bp++ = ' ';						// prepend p2 in hex:

	for ( i = 7; i >= 0; --i )
	{
		x = b & 0x0F;
		if ( x < 10 )
			 x += '0';
		else x += 'A' - 10;
		bp[ i ] = x;
		b >>= 4;
	}
	bp += 8;
	*bp++ = '}';

	*bp++ = ' ';

	for ( i = sizeof( work ) - (int)(bp - work) - 1; i && (*bp++ = *str++); --i )	;
	bp[ -1 ] = '\n';	// insert new line character
	*bp = 0;			// add C string terminator

	fpELG->alertCount++;	// trigger anybody watching
	fpELG->lastAlrt = ascii;

//	fpELG->evLogFlag = 0;	// stop logging but alertCount can continue increasing.

//	if ( fpELG->evLogFlag == 0xFeedBeef	 )
//		 fpELG->evLogFlag = 333;	// cruise to see what happens next.

//	kprintf( work );
//	panic( work );
//	Debugger( work );
//	IOLog( work );

	return 0xDeadBeef;
}/* end Alrt */
#endif // USE_ELG

#pragma mark -
#pragma mark еее Override methods еее
#pragma mark -

	//---------------------------------------------------------------------------

bool RTL8139::init( OSDictionary *properties )
{
#if USE_ELG
	AllocateEventLog( kEvLogSize );
	ELG( this, fpELG, 'Rltk', "RTL8139::init - event logging set up." );
#endif /* USE_ELG */

	if ( false == super::init( properties ) )
		return false;

	forceLinkChange		= true;
	phyStatusLast		= 0;
	fSpeed100			= false;
    currentLevel		= kActivationLevel0;
    currentMediumIndex	= MEDIUM_INDEX_NONE;
	fLoopback			= false;
	fLoopbackMode		= kSelectLoopbackPHY;
	fTSD_ERTXTH			= R_TSD_ERTXTH;
    return true;
}/* end init */

	//---------------------------------------------------------------------------

bool RTL8139::start( IOService *provider )
{
    bool success = false;
    
	ELG( IOThreadSelf(), provider, 'Strt', "RTL8139::start - this, provider." );
    DEBUG_LOG( "start() ===>\n" );

    do
	{
        if ( false == super::start( provider ) )	// Start our superclass first
            break;

			// Save a reference to our provider.

        pciNub = OSDynamicCast( IOPCIDevice, provider );
        if ( 0 == pciNub )
            break;

        pciNub->retain();						// Retain provider, released in free().

        if ( false == pciNub->open( this ) )	// Open our provider.
            break;

        if ( false == initEventSources( provider ) )
            break;

			// Allocate memory for descriptors. This function will leak memory
			// if called more than once. So don't do it.

        if ( false == allocateDescriptorMemory() )
            break;

			// Get the virtual address mapping of CSR registers located at
			// Base Address Range 0 (0x10).

        csrMap = pciNub->mapDeviceMemoryWithRegister( kIOPCIConfigBaseAddress1 );
        if ( 0 == csrMap )
            break;

        csrBase = (volatile void*)csrMap->getVirtualAddress();

			// Init PCI config space:

        if ( false == initPCIConfigSpace( pciNub ) )
            break;

			// Reset chip to bring it to a known state.

        if ( initAdapter( kResetChip ) == false )
        {
            IOLog( "%s: initAdapter() failed\n", getName() );
            break;
        }

		registerEEPROM();

			// Publish our media capabilities:

        phyProbeMediaCapability();
        if ( false == publishMediumDictionary( mediumDict ) )
			break;

        success = true;
    }
    while ( false );

		// Close our provider, it will be re-opened on demand when
		// our enable() is called by a client.

    if ( pciNub )
		 pciNub->close( this );
    
    do
	{
        if ( false == success )
            break;

        success = false;

			// Allocate and attach an IOEthernetInterface instance.

        if ( false == attachInterface( (IONetworkInterface**)&netif, false) )
            break;

			// Optional: this driver supports kernel debugging.

        attachDebuggerClient( &debugger );

			// Trigger matching for clients of netif.

        netif->registerService();
        success = true;
    }
    while ( false );

    DEBUG_LOG( "start() <===\n" );
    return success;
}/* end start */

	//---------------------------------------------------------------------------

void RTL8139::stop( IOService *provider )
{
	ELG( 0, provider, 'stop', "RTL8139::stop" );
    DEBUG_LOG( "stop() ===>\n" );
    super::stop( provider );
    DEBUG_LOG( "stop() <===\n" );
	return;
}/* end stop */

	//---------------------------------------------------------------------------

bool RTL8139::initEventSources( IOService *provider )
{
	ELG( 0, 0, 'InES', "RTL8139::initEventSources - " );
    DEBUG_LOG( "initEventSources() ===>\n" );

	IOWorkLoop	*wl = getWorkLoop();
	if ( 0 == wl )
        return false;

	fTransmitQueue = getOutputQueue();
	if ( 0 == fTransmitQueue )
        return false;
	fTransmitQueue->setCapacity( kTransmitQueueCapacity );

		// Create an interrupt event source to handle hardware interrupts.

	interruptSrc = IOInterruptEventSource::interruptEventSource(
						this,
					   OSMemberFunctionCast(	IOInterruptEventAction,
												this,
												&RTL8139::interruptOccurred),
					   provider );

	if ( !interruptSrc || (wl->addEventSource( interruptSrc ) != kIOReturnSuccess) )
		return false;

		// This is important. If the interrupt line is shared with other devices,
		// then the interrupt vector will be enabled only if all corresponding
		// interrupt event sources are enabled. To avoid masking interrupts for
		// other devices that are sharing the interrupt line, the event source
		// is enabled immediately. Hardware interrupt sources remain disabled.

    interruptSrc->enable();

		// Register a timer event source used as a watchdog timer:

	timerSrc = IOTimerEventSource::timerEventSource(
					this,
					OSMemberFunctionCast(	IOTimerEventSource::Action,
											this,
											&RTL8139::timeoutOccurred ) );

	if ( !timerSrc || (wl->addEventSource( timerSrc ) != kIOReturnSuccess) )
		return false;

		// Create a dictionary to hold IONetworkMedium objects:

	mediumDict = OSDictionary::withCapacity( 5 );
	if ( 0 == mediumDict )
		return false;

	DEBUG_LOG( "initEventSources() <===\n" );
	return true;
}/* end initEventSources */

	//--------------------------------------------------------------------------
	// Update PCI command register to enable the IO mapped PCI memory range,
	// and bus-master interface.

bool RTL8139::initPCIConfigSpace( IOPCIDevice *provider )
{
	UInt16	reg16;
        
	ELG( 0, provider, 'iPCI', "RTL8139::initPCIConfigSpace" );
    DEBUG_LOG( "pciConfigInit() ===>\n" );

    reg16	= provider->configRead16( kIOPCIConfigCommand );
    reg16  &= ~kIOPCICommandIOSpace;

    reg16	|= ( kIOPCICommandBusMaster
			|    kIOPCICommandMemorySpace
			|	 kIOPCICommandMemWrInvalidate );

	provider->configWrite16( kIOPCIConfigCommand, reg16 );

	provider->configWrite8( kIOPCIConfigCacheLineSize, 64 / sizeof( UInt32 ) );
	provider->configWrite8( kIOPCIConfigLatencyTimer, 0xF8 );// max timer - low 3 bits ignored

    DEBUG_LOG( "pciConfigInit() <===\n" );

    return true;
}/* end initPCIConfigSpace */

	//---------------------------------------------------------------------------

bool RTL8139::createWorkLoop()
{
    DEBUG_LOG( "createWorkLoop() ===>\n" );
    workLoop = IOWorkLoop::workLoop();
    DEBUG_LOG( "createWorkLoop() <===\n" );
    return (workLoop != 0);
}/* end createWorkLoop */

//---------------------------------------------------------------------------

IOWorkLoop * RTL8139::getWorkLoop( void ) const
{
		// Override IOService::getWorkLoop() method to return the
		//  work loop we allocated in createWorkLoop().

    DEBUG_LOG( "getWorkLoop() ===>\n" );
    DEBUG_LOG( "getWorkLoop() <===\n" );
	return workLoop;
}/* end getWorkLoop */

	//---------------------------------------------------------------------------

bool RTL8139::configureInterface( IONetworkInterface * netif )
{
    IONetworkData * data;

	ELG( this, netif, 'cfgI', "RTL8139::configureInterface " );
    DEBUG_LOG( "configureInterface() ===>\n" );

    if ( false == super::configureInterface( netif ) )
        return false;
	
		// Get the generic network statistics structure:

    data = netif->getParameter( kIONetworkStatsKey );
    if ( !data || !(netStats = (IONetworkStats*)data->getBuffer()) ) 
        return false;

		// Get the Ethernet statistics structure:

    data = netif->getParameter( kIOEthernetStatsKey );
    if ( !data || !(etherStats = (IOEthernetStats*)data->getBuffer()) ) 
        return false;

    DEBUG_LOG( "configureInterface() <===\n" );
    return true;
}/* end configureInterface */

	//---------------------------------------------------------------------------

void RTL8139::free()
{
#define RELEASE(x) do { if(x) { (x)->release(); (x) = 0; } } while(0)

	ELG( 0, 0, 'free', "RTL8139::free" );
    DEBUG_LOG( "free() ===>\n" );

    if ( interruptSrc && workLoop )
        workLoop->removeEventSource( interruptSrc );

    RELEASE( netif        );
    RELEASE( debugger     );
    RELEASE( interruptSrc );
    RELEASE( timerSrc     );
    RELEASE( csrMap       );
    RELEASE( mediumDict   );
    RELEASE( pciNub       );
    RELEASE( workLoop     );

    if ( fpTxRxMD )
    {
        fpTxRxMD->complete();
        fpTxRxMD->release();
        fpTxRxMD = 0;
    }

    super::free();

    DEBUG_LOG( "free() <===\n" );
	return;
}/* end free */

	//---------------------------------------------------------------------------
	// Function: enableAdapter
	//
	// Enables the adapter & driver to the given level of support.

bool RTL8139::enableAdapter( UInt32 level )
{
	UInt16	isr;
	bool	success = false;

	ELG( 0, level, 'enbA', "RTL8139::enableAdapter" );
    DEBUG_LOG( "enableAdapter() ===>\n" );
    DEBUG_LOG( "enable level %ld\n", level);

    switch ( level ) 
    {
	case kActivationLevel1:

			// Open our provider (IOPCIDevice):

		if ( (0 == pciNub) || (false == pciNub->open( this )) )
			break;

			// Perform a full initialization sequence:

		if ( initAdapter( kFullInitialization ) != true )
			break;

			// Program the physical layer / transceiver:

		if ( selectMedium( getSelectedMedium() ) != kIOReturnSuccess )
			break;

			// Start the periodic timer:

		timerSrc->setTimeoutMS( kWatchdogTimerPeriod );	// ??? do this in Level 2???

			// Unless we wait and ack PUN/LinkChg interrupts, the receiver
			// will not work. This creates a problem when DB_HALT debug
			// flag is set, since we will break into the debugger right
			// away after this function returns. But we won't be able to
			// attach since the receiver is deaf. I have no idea why this
			// workaround (discovered through experimentation) is needed.

		for ( int i = 0; i < 100; i++ )
		{
			isr = csrRead16( RTL_ISR );
			if ( isr & R_ISR_PUN )
			{
				csrWrite16( RTL_ISR, R_ISR_PUN );
				DEBUG_LOG( "cleared PUN interrupt %x in %d\n", isr, i );
				break;
			}
			IOSleep( 10 );
		}/* end FOR */

		success = true;
		break;
		
	case kActivationLevel2:
		workLoop->enableAllInterrupts();
		success = true;
		break;
    }/* end SWITCH */

    if ( false == success )
        IOLog( "enable level %ld failed\n", level );

    DEBUG_LOG( "enableAdapter() <===\n" );
    return success;
}/* end enableAdapter */


	//---------------------------------------------------------------------------
	// Function: disableAdapter
	// Disables the adapter & driver to the given level of support.

bool RTL8139::disableAdapter( UInt32 currentLevel )
{
    bool success = false;

	ELG( 0, currentLevel, 'disA', "RTL8139::disableAdapter" );
    DEBUG_LOG( "disableAdapter() ===>\n" );
    DEBUG_LOG( "disable currentLevel %ld\n", currentLevel );

    switch ( currentLevel )
    {
	case kActivationLevel1:
		timerSrc->cancelTimeout();		// Stop the timer event source.
		initAdapter( kResetChip );		// Reset the hardware engine.

		phySetMedium( MEDIUM_INDEX_NONE );	// Power down the PHY

		if ( pciNub )
			pciNub->close( this );	// Close our provider.

		success = true;
		break;

	case kActivationLevel2:
		disableHardwareInterrupts();		// KDP doesn't use interrupts.
		workLoop->disableAllInterrupts();

			// Stop the transmit queue. outputPacket() will not get called
			// after this. KDP calls sendPacket() to send a packet in polled
			// mode and that is unaffected by the state of the output queue.

		fTransmitQueue->stop();
		fTransmitQueue->flush();

		setLinkStatus( kIONetworkLinkValid );	// Valid sans kIONetworkLinkActive
		success = true;
		break;
    }/* end SWITCH */

    if ( false == success )
        IOLog( "disable currentLevel %ld failed\n", currentLevel );

    DEBUG_LOG( "disableAdapter() <===\n" );

    return success;
}/* end disableAdapter */

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

	ELG( 0, level, 'sLvl', "RTL8139::setActivationLevel" );
    DEBUG_LOG( "setActivationLevel() ===>\n" );
    DEBUG_LOG( "---> CURRENT LEVEL: %ld DESIRED LEVEL: %ld\n", currentLevel, level );

    if ( currentLevel == level )
        return true;

    for ( ; currentLevel > level; currentLevel-- )
    {
        if ( (success = disableAdapter( currentLevel )) == false )
            break;
    }

    for ( nextLevel = currentLevel + 1; currentLevel < level;
          currentLevel++, nextLevel++ )
    {
        if ( (success = enableAdapter( nextLevel )) == false )
            break;
    }

    DEBUG_LOG( "---> PRESENT LEVEL: %ld\n\n", currentLevel);
    DEBUG_LOG( "setActivationLevel() <===\n" );
    return success;
}/* end setActivationLevel */

	//---------------------------------------------------------------------------

IOReturn RTL8139::enable( IONetworkInterface *netif )
{
	ELG( 0, enabledByBSD, 'enbN', "RTL8139::enable - netif" );
    DEBUG_LOG( "enable(netif) ===>\n" );
    
    if ( true == enabledByBSD )
    {
        DEBUG_LOG( "enable() <===\n" );
        return kIOReturnSuccess;
    }

    enabledByBSD = setActivationLevel( kActivationLevel2 );

    DEBUG_LOG( "enable(netif) <===\n" );

    return enabledByBSD ? kIOReturnSuccess : kIOReturnIOError;
}/* end enable netif */

	//---------------------------------------------------------------------------

IOReturn RTL8139::disable( IONetworkInterface* /*netif*/ )
{
	ELG( enabledByKDP, enabledByBSD, 'disN', "RTL8139::disable - netif" );
    DEBUG_LOG( "disable(netif) ===>\n" );

    enabledByBSD = false;

    setActivationLevel( enabledByKDP ? kActivationLevel1 : kActivationLevel0 );

    DEBUG_LOG( "disable(netif) <===\n" );

#if USE_ELG
fpELG->evLogFlag = 0x03330333;	/// ??? delete this when ifconfig en0 down is fixed
#endif // USE_ELG

	return kIOReturnSuccess;
}/* end disable netif */

	//---------------------------------------------------------------------------

IOReturn RTL8139::enable( IOKernelDebugger* /* debugger */ )
{
	ELG( enabledByKDP, enabledByBSD, 'enbD', "RTL8139::enable - debugger" );

	if ( enabledByKDP || enabledByBSD )
    {
		enabledByKDP = true;
		return kIOReturnSuccess;
	}

	enabledByKDP = setActivationLevel( kActivationLevel1 );

	return enabledByKDP ? kIOReturnSuccess : kIOReturnIOError;
}/* end enable debugger */

	//---------------------------------------------------------------------------

IOReturn RTL8139::disable( IOKernelDebugger* /* debugger */ )
{
	ELG( 0, 0, 'disD', "RTL8139::disable - debugger" );
	enabledByKDP = false;

	if ( enabledByBSD == false )
		setActivationLevel( kActivationLevel0 );

#if USE_ELG
fpELG->evLogFlag = 0x03330333;	/// ??? delete this when ifconfig en0 down is fixed
#endif // USE_ELG

	return kIOReturnSuccess;
}/* end disable debugger */


 bool RTL8139::setLinkStatus(	UInt32					status,
								const IONetworkMedium	*activeMedium,
								UInt64					speed,
								OSData					*data )
{
	ELG( speed / 1000000, status, ' SLS', "setLinkStatus" );
	return super::setLinkStatus( status, activeMedium, speed, data );
}/* end setLinkStatus */

	//---------------------------------------------------------------------------

void RTL8139::timeoutOccurred( IOTimerEventSource *timer )
{
	UInt32	u32;

	timerSrc->setTimeoutMS( kWatchdogTimerPeriod );
	ELG( 0, 0, 'time', "RTL8139::timeoutOccurred" );

	u32 = csrRead32( RTL_MPC );	// get the 24-bit Missed Packet Counter
	if ( u32 )
	{
		etherStats->dot3StatsEntry.missedFrames += u32;
		csrWrite32( RTL_MPC, 0 );
	}

	phyReportLinkStatus();
	return;
}/* end timeoutOccurred */

	//---------------------------------------------------------------------------

IOReturn RTL8139::setPromiscuousMode( bool enabled )
{
	ELG( 0, enabled, 'setP', "RTL8139::setPromiscuousMode" );
    DEBUG_LOG( "setPromiscuousMode() ===>\n" );

    if ( enabled )
    {
        reg_rcr |= R_RCR_AAP;					// allow all physical

        csrWrite32( RTL_MAR0, 0xffffffff );		// Accept all multicast
        csrWrite32( RTL_MAR4, 0xffffffff );
    }
    else
    {
        reg_rcr &= ~R_RCR_AAP;

        csrWrite32( RTL_MAR0, reg_mar0 );		// Restore multicast hash filter.
        csrWrite32( RTL_MAR4, reg_mar4 );
    }
    
    csrWrite32( RTL_RCR, reg_rcr );

    DEBUG_LOG( "setPromiscuousMode RTL_RCR = 0x%lx\n", reg_rcr );
    DEBUG_LOG( "setPromiscuousMode() <===\n" );

    return kIOReturnSuccess;
}/* end setPromiscuousMode */

	//---------------------------------------------------------------------------

IOReturn RTL8139::setMulticastMode( bool enabled )
{
	ELG( 0, enabled, 'setM', "RTL8139::setMulticastMode" );
    DEBUG_LOG( "setMulticastMode() ===>\n" );

		// Always accept multicast packets. The R_RCR_AM flag is always set
		// whenever the receiver is enabled. Nothing else is needed here.

    DEBUG_LOG( "setMulticastMode RTL_RCR = 0x%lx\n", reg_rcr );
    DEBUG_LOG( "setMulticastMode() <===\n" );

    return kIOReturnSuccess;
}/* end setMulticastMode */

	//---------------------------------------------------------------------------

static inline UInt32 rtl_ether_crc( int length, const unsigned char *data )
{
    static unsigned const	ethernet_polynomial = 0x04c11db7U;
	unsigned char			current_octet;
    int						crc = -1;

	while ( --length >= 0 )
	{
		current_octet = *data++;
		for ( int bit = 0; bit < 8; bit++, current_octet >>= 1 )
			crc = (crc << 1) ^
				((crc < 0) ^ (current_octet & 1) ? ethernet_polynomial : 0);
	}
	return crc;
}/* end rtl_ether_crc */


IOReturn RTL8139::setMulticastList( IOEthernetAddress *addrs, UInt32 count )
{
	ELG( addrs, count, 'setL', "RTL8139::setMulticastList" );
    DEBUG_LOG( "setMulticastList() ===>\n" );

	for ( UInt32 i = 0; i < count; i++, addrs++ )
    {
        int bit = rtl_ether_crc( 6, (const UInt8*)addrs ) >> 26;
        if ( bit < 32 )
            reg_mar0 |= (1 << bit);
        else reg_mar4 |= (1 << (bit - 32));
    }

    csrWrite32( RTL_MAR0, reg_mar0 );
    csrWrite32( RTL_MAR4, reg_mar4 );

    DEBUG_LOG( "setMulticastList() <===\n" );

    return kIOReturnSuccess;
}/* end setMulticastList */

	//---------------------------------------------------------------------------

void RTL8139::getPacketBufferConstraints(
								IOPacketBufferConstraints *constraints ) const
{
//	ELG( 0, kIOPacketBufferAlign1, 'gPBC', "RTL8139::getPacketBufferConstraints" );
    DEBUG_LOG( "getPacketBufferConstraints() ===>\n" );

    constraints->alignStart  = kIOPacketBufferAlign1; // no restriction
    constraints->alignLength = kIOPacketBufferAlign1; // no restriction

    DEBUG_LOG( "getPacketBufferConstraints() <===\n" );
	return;
}/* end getPacketBufferConstraints */

	//---------------------------------------------------------------------------

IOReturn RTL8139::getHardwareAddress( IOEthernetAddress *address )
{
    union
	{
        UInt8  bytes[4];
        UInt32 int32;
    } idr;

	ELG( 0, 0, 'gHWA', "RTL8139::getHardwareAddress" );
    DEBUG_LOG( "getHardwareAddress() ===>\n" );

		// Fetch the hardware address bootstrapped from EEPROM.

    idr.int32 = OSSwapLittleToHostInt32( csrRead32( RTL_IDR0 ) );
    address->bytes[0] = idr.bytes[0];
    address->bytes[1] = idr.bytes[1];
    address->bytes[2] = idr.bytes[2];
    address->bytes[3] = idr.bytes[3];

    idr.int32 = OSSwapLittleToHostInt32( csrRead32( RTL_IDR4 ) );
    address->bytes[4] = idr.bytes[0];
    address->bytes[5] = idr.bytes[1];

    DEBUG_LOG( "getHardwareAddress() <===\n" );
	return kIOReturnSuccess;
}/* end getHardwareAddress */

	//---------------------------------------------------------------------------

IOOutputQueue* RTL8139::createOutputQueue()
{
	ELG( 0, 0, 'crOQ', "RTL8139::createOutputQueue" );
    DEBUG_LOG( "createOutputQueue() ===>\n" );
    DEBUG_LOG( "createOutputQueue() <===\n" );

		// An IOGatedOutputQueue will serialize all calls to the driver's
		// outputPacket() function with its work loop. This essentially
		// serializes all access to the driver and the hardware through
		// the driver's work loop, which simplifies the driver but also
		// carries a small performance cost (relatively for 10/100 Mb).

    return IOGatedOutputQueue::withTarget( this, getWorkLoop() );
}/* end createOutputQueue */

	//---------------------------------------------------------------------------

IOReturn RTL8139::selectMedium( const IONetworkMedium *medium )
{
	int		index;
    bool	success;

#if USE_ELG
if ( fpELG->evLogFlag == 0xFeedBeef )	fpELG->evLogFlag = 0xDebeefed;	/// ??? delete this
#endif // USE_ELG

	ELG( 0, medium, 'sMed', "RTL8139::selectMedium" );
    if ( medium == 0 )
        medium = phyGetMediumWithIndex( MEDIUM_INDEX_AUTO );

    if ( medium == 0 )
        return kIOReturnUnsupported;

	success = phySetMedium( medium );
    if ( success )
    {
		setCurrentMedium( medium );
		forceLinkChange = true;			// force link change
		phyReportLinkStatus();
    }

    return success ? kIOReturnSuccess : kIOReturnIOError;
}/* end selectMedium */

	//---------------------------------------------------------------------------
	// Report human readable hardware information strings.

const OSString* RTL8139::newVendorString() const
{
    DEBUG_LOG( "newVendorString() ===>\n" );
    DEBUG_LOG( "newVendorString() <===\n" );
    return OSString::withCString( "Realtek" );
}/* end newVendorString */


const OSString* RTL8139::newModelString() const
{
    const char	*model = "8139";

		// FIXME: should do a better job of identifying the device type.

    DEBUG_LOG( "newModelString() ===>\n" );
    DEBUG_LOG( "newModelString() <===\n" );
    return OSString::withCString(model);
}/* end newModelString */

	//---------------------------------------------------------------------------

IOReturn RTL8139::registerWithPolicyMaker( IOService *policyMaker )
{
    enum
	{
        kPowerStateOff = 0,
        kPowerStateOn,
        kPowerStateCount
    };

    static IOPMPowerState powerStateArray[ kPowerStateCount ] =
    {
        { 1,0,0,0,0,0,0,0,0,0,0,0 },
        { 1, IOPMDeviceUsable, IOPMPowerOn, IOPMPowerOn, 0,0,0,0,0,0,0,0 }
    };

    IOReturn ret;

	ELG( 0, 0, 'RwPM', "RTL8139::registerWithPolicyMaker" );
    ret = policyMaker->registerPowerDriver( this, powerStateArray,
                                            kPowerStateCount );
    
    return ret;
}/* end registerWithPolicyMaker */

	//---------------------------------------------------------------------------

IOReturn RTL8139::setPowerState( unsigned long powerStateOrdinal,
                                 IOService *policyMaker )
{
    // Rely exclusively on enable() and disable() calls from our clients
    // who are power savvy, and will turn the controller off and back on
    // across system sleep. We just have to re-initialize the chip every
    // time the controller is enabled.
    //
    // FIXME: add support for wake on magic packet

	ELG( 0, powerStateOrdinal, 's PS', "RTL8139::setPowerState" );
    DEBUG_LOG( "setPowerState state %d\n", powerStateOrdinal);

    return IOPMAckImplied;
}/* end setPowerState */

	//---------------------------------------------------------------------------


IOReturn RTL8139::newUserClient(	task_t			owningTask,
									void*,						// Security id (?!)
									UInt32			type,		// Lucky number
									IOUserClient	**handler )	// returned handler
{
	IOReturn			ior		= kIOReturnSuccess;
	RTL8139UserClient	*client	= NULL;
    bool				privileged;


	ELG( type, type, 'Usr+', "RTL8139::newUserClient" );
    
    privileged = IOUserClient::clientHasPrivilege( current_task(), kIOClientPrivilegeAdministrator ) == kIOReturnSuccess;
	if ( !privileged )
	{
		ELG( 0, 0, 'nuc-', "RTL8139::newUserClient - task is not privileged." );
		return kIOReturnNotPrivileged;
	}
		// Check that this is a user client type that we support.
		// type is known only to this driver's user and kernel
		// classes. It could be used, for example, to define
		// read or write privileges. In this case, we look for
		// a private value.
	if ( type != 'Rltk' )
	{		/// ??? don't return error - call superclass and return its code.
		ELG( 0, type, 'Usr-', "RTL8139::newUserClient - unlucky." );
		return kIOReturnError;
	}

		// Instantiate a new client for the requesting task:

	client = RTL8139UserClient::withTask( owningTask );
	if ( !client )
	{
		ELG( 0, 0, 'usr-', "Realtek::newUserClient: Can't create user client" );
		return kIOReturnError;
	}

	if ( ior == kIOReturnSuccess )
	{		// Attach ourself to the client so that this client instance can call us.
		if ( client->attach( this ) == false )
		{
			ior = kIOReturnError;
			ELG( 0, 0, 'USR-', "Realtek::newUserClient: Can't attach user client" );
		}
	}

	if ( ior == kIOReturnSuccess )
	{		// Start the client so it can accept requests.
		if ( client->start( this ) == false )
		{
			ior = kIOReturnError;
			ELG( 0, 0, 'USR-', "Realtek::newUserClient: Can't start user client" );
		}
	}

	if ( client && (ior != kIOReturnSuccess) )
	{
		client->detach( this );
		client->release();
		client = 0;
	}

	*handler = client;
	return ior;
}/* end newUserClient */


#pragma mark -
#pragma mark еее User Client еее
#pragma mark -


#undef  super
#define super IOUserClient

	OSDefineMetaClassAndStructors( RTL8139UserClient, IOUserClient )	;



RTL8139UserClient* RTL8139UserClient::withTask( task_t owningTask )
{
	RTL8139UserClient*		me = new RTL8139UserClient;

	if ( me && me->init() == false )
    {
        me->release();
        return 0;
    }

	me->fTask = owningTask;

    return me;
}/* end RTL8139UserClient::withTask */


bool RTL8139UserClient::start( IOService *provider )
{
	if ( super::start( provider ) == false )	return false;
	if ( provider->open( this )   == false )	return false;

	fProvider = (RTL8139*)provider;

		/* Initialize the call structure:	*/

	fMethods[0].object = this;
	fMethods[0].func   = (IOMethod)&RTL8139UserClient::doRequest;
	fMethods[0].count0 = 0xFFFFFFFF;			/* One input  as big as I need */
	fMethods[0].count1 = 0xFFFFFFFF;			/* One output as big as I need */
	fMethods[0].flags  = kIOUCStructIStructO;

	return true;
}/* end RTL8139UserClient::start */


IOReturn RTL8139UserClient::clientClose()
{

	if ( fProvider )
	{
		if ( fProvider->isOpen( this ) )
			fProvider->close( this );

		detach( fProvider );
		fProvider = 0;
	}
	return kIOReturnSuccess;
}/* end RTL8139UserClient::clientClose */


IOReturn RTL8139UserClient::clientDied()
{
	return clientClose();
}/* end RTL8139UserClient::clientDied */


IOReturn RTL8139UserClient::connectClient( IOUserClient *client )
{
    return kIOReturnSuccess;
}/* end connectClient */


IOReturn RTL8139UserClient::registerNotificationPort( mach_port_t port, UInt32 type )
{
	return kIOReturnUnsupported;
}/* end registerNotificationPort */


IOExternalMethod* RTL8139UserClient::getExternalMethodForIndex( UInt32 index )
{
    IOExternalMethod	*result = NULL;


	if ( index == 0 )
		result = &fMethods[0];

    return result;
}/* end getExternalMethodForIndex */


IOReturn RTL8139UserClient::doRequest(	void		*pIn,		void		*pOut,
										IOByteCount	inputSize,	IOByteCount	*pOutPutSize )
{
	UInt32		reqID;
    bool		privileged;
	IOReturn	ior = kIOReturnSuccess;


//	UC_ELG( inputSize,   (UInt32)pIn,  'uRqI', "RTL8139UserClient::doRequest - input parameters" );
//	UC_ELG( pOutPutSize ? *pOutPutSize : 0xDeadBeef, (UInt32)pOut, 'uRqO', "RTL8139UserClient::doRequest - output parameters" );

    privileged = (IOUserClient::clientHasPrivilege(
		current_task(), kIOClientPrivilegeAdministrator ) == kIOReturnSuccess);

	if ( !privileged )
		return kIOReturnNotPrivileged;

		/* validate parameters:	*/

	if ( !pIn  || (inputSize < sizeof( UCRequest )) )
	{
		IOLog( "RTL8139UserClient::doRequest - bad argument(s) - pIn = %lx, inputSize = %ld.\n",
				(UInt32)pIn, inputSize );
		return kIOReturnBadArgument;
	}

	reqID = *(UInt32*)pIn;

	switch ( reqID )					/* switch on request ID	*/
	{
	case kSelectLoopbackMAC:
	case kSelectLoopbackPHY:
		fProvider->fLoopbackMode = reqID;
		break;

	case kRltkUserCmd_GetLog:		ior = getRltkLog(		pIn, pOut, inputSize, pOutPutSize );	break;
	case kRltkUserCmd_GetRegs:		ior = getRltkRegs(		pIn, pOut, inputSize, pOutPutSize );	break;
//	case kRltkUserCmd_GetOneReg:	ior = getOneRltkReg(	pIn, pOut, inputSize, pOutPutSize );	break;
	case kRltkUserCmd_WriteOneReg:	ior = writeOneRltkReg(	pIn, pOut, inputSize, pOutPutSize );	break;

	default:
		IOLog( "RTL8139UserClient::doRequest - Bad command, %lx\n", reqID );
		ior = kIOReturnBadArgument;
		break;
	}

	return ior;
}/* end doRequest */


#if USE_ELG
	/* getRltkLog - Get Realtek event log.			*/
	/*												*/
	/* input is struct:								*/
	/*		command code (kRltkUserCmd_GetLog)		*/
	/*		buffer address (unused)					*/
	/*		buffer size    (unused)					*/
	/* output is struct:							*/
	/*		command code (copied)					*/
	/*		address of buffer mapped in user space	*/
	/*		buffer size								*/

IOReturn RTL8139UserClient::getRltkLog(	void		*pIn,		void		*pOut,
											IOByteCount	inputSize,	IOByteCount	*pOutPutSize )
{
	UCRequest			*req = (UCRequest*)pIn;
	IOVirtualAddress	clientAddr;


	UC_ELG( (UInt32)pIn,  inputSize,    'UgLg', "RTL8139UserClient::getRltkLog" );

		/* map in the buffer to kernel space:	*/

	fMap = fProvider->fpELGMemDesc->map( fTask, 0, kIOMapAnywhere );

	clientAddr = fMap->getVirtualAddress();
	req->pLogBuffer	= (UInt8*)clientAddr;
	req->bufSize	= (fProvider->fpELG->evLogBufe - fProvider->fpELG->evLogBuf + PAGE_SIZE) & ~(PAGE_SIZE - 1);

//	UC_ELG( req->bufSize, req->pLogBuffer, '=uBf', "RTL8139UserClient::getRltkLog - user buffer" );

	bcopy( req, pOut, sizeof( UCRequest ) );
	*pOutPutSize = sizeof( UCRequest );

    return kIOReturnSuccess;
}/* end getRltkLog */

#else // Production driver - no event logging buffer:
IOReturn RTL8139UserClient::getRltkLog( void*, void*, IOByteCount, IOByteCount* )
{
	return kIOReturnBadArgument;
}/* end getRltkLog */
#endif // USE_ELG


	/* getRltkRegs - Get Realtek registers.	*/
	/*											*/
	/* input is bytes:							*/
	/*		command code (kRltkUserCmd_GetRegs)	*/
	/*		buffer address						*/
	/*		buffer size							*/
	/*											*/
	/* output set to Length/Type/Value records	*/

IOReturn RTL8139UserClient::getRltkRegs(	void		*pIn,		void		*pOut,
											IOByteCount	inputSize,	IOByteCount	*pOutPutSize )
{
	UInt8			*dest = (UInt8*)pOut;
	UInt32			i;


	if ( !pOut || (*pOutPutSize < PAGE_SIZE) )	// actual size unknown
	{
		IOLog( "RTL8139UserClient::getRltkRegs - bad argument(s) - pOut = %lx, *pOutPutSize = %ld.\n",
				(UInt32)pOut, (UInt32)*pOutPutSize );
		return kIOReturnBadArgument;
	}

	for ( i = 0; i < 0x80; i++ )
		*dest++ = fProvider->csrRead8( i );

	*pOutPutSize = 0x80;
	UC_ELG( 0, *pOutPutSize, 'gReg', "RTL8139UserClient::getRltkRegs - done" );
    return kIOReturnSuccess;
}/* end getRltkRegs */


	static UInt8	regSize[ 0x80 ] =
	{
		4, 0, 0, 0,  4, 0, 0, 0,   4, 0, 0, 0,  4, 0, 0, 0,
		4, 0, 0, 0,  4, 0, 0, 0,   4, 0, 0, 0,  4, 0, 0, 0,
		4, 0, 0, 0,  4, 0, 0, 0,   4, 0, 0, 0,  4, 0, 0, 0,
		4, 0, 0, 0,  2, 0, 1, 1,   2, 0, 2, 0,  2, 0, 2, 0,

		4, 0, 0, 0,  4, 0, 0, 0,   4, 0, 0, 0,  4, 0, 0, 0,
		1, 1, 1, 0,  4, 0, 0, 0,   1, 1, 1, 1,  2, 0, 0, 0,
		2, 0, 2, 0,  2, 0, 2, 0,   2, 0, 2, 0,  2, 0, 2, 0,
		2, 0, 2, 0,  2, 0, 0, 0,   4, 0, 0, 0,  4, 0, 0, 0
	};

	/* writeOneRltkReg - Write a register		*/
	/*											*/
	/* input is bytes:							*/
	/*		command code (kRltkUserCmd_GetRegs)	*/
	/*		buffer address						*/
	/*		buffer size							*/
	/*											*/
	/* output set to Length/Type/Value records	*/

IOReturn RTL8139UserClient::writeOneRltkReg(
							void		*pIn,		void		*pOut,
							IOByteCount	inputSize,	IOByteCount	*pOutPutSize )
{
	UCRequest	*req = (UCRequest*)pIn;
	UInt32		reg, value;
	UInt8		size = 0;		// default to bad argument

	UC_ELG( (UInt32)pIn,  inputSize,    'UgWR', "RTL8139UserClient::writeOneRltkReg" );

	*pOutPutSize = 0;
	reg		= (UInt32)req->pLogBuffer;
	value	= req->bufSize;

	if ( reg == 0x100 )		// adjust the Tx early threshold being used.
	{
		fProvider->fTSD_ERTXTH = (value & 0x3F) << 16;
		return kIOReturnSuccess;
	}

	if ( reg < 0x80 )
		size = regSize[ reg ];

	if ( size == 0 )
	{
		IOLog( "RTL8139UserClient::writeOneRltkReg - bad argument(s) - reg = %lx; size = %d.\n", reg, size );
		return kIOReturnBadArgument;
	}

	switch ( size )
	{
	case 1:
		if ( value >= 256 )		{ size = 0; break; }
		fProvider->csrWrite8( reg, (UInt8)value );
	case 2:
		if ( value >= 0x10000 )	{ size = 0; break; }
			fProvider->csrWrite16( reg, (UInt16)value );
	case 4:
		fProvider->csrWrite32( reg, (UInt32)value );
	}

	if ( size == 0 )
	{
		IOLog( "RTL8139UserClient::writeOneRltkReg - value too large for register - reg = %lx; value = %lx.\n", reg, value );
		return kIOReturnBadArgument;
	}

    return kIOReturnSuccess;
}/* end writeOneRltkReg */
