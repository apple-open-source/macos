/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1998-1999 Apple Computer
 *
 * Hardware independent (relatively) code for the Sun GEM Ethernet Controller 
 *
 * HISTORY
 *
 * dd-mmm-yy     
 *  Created.
 *
 */

//void call_kdp(void);

#include "UniNEnet.h"
#include "UniNEnetMII.h"
#include <libkern/OSByteOrder.h>

extern "C"
{
	extern boolean_t	ml_probe_read( vm_offset_t physAddr, unsigned int *val );
}



#define super IOEthernetController

	OSDefineMetaClassAndStructors( UniNEnet, IOEthernetController )		;

	globals		g;	/**** Instantiate the globals ****/


#if USE_ELG
static void AllocateEventLog( UInt32 size )
{
	IOPhysicalAddress	phys;


//	IOSleep( 60000 );
	if ( g.evLogBuf )
		return;

	g.evLogFlag = 0;			/* assume insufficient memory	*/
	g.evLogBuf = (UInt8*)IOMallocContiguous( size, 0x1000, &phys );
	if ( !g.evLogBuf )
	{
		kprintf( "AllocateEventLog - UniNEnet evLog allocation failed " );
		return;
	}

	bzero( g.evLogBuf, size );
	g.evLogBufp = g.evLogBuf;
	g.evLogBufe = g.evLogBufp + kEvLogSize - 0x20; // ??? overran buffer?
	g.evLogFlag	 = 0xFEEDBEEF;	// continuous wraparound
//	g.evLogFlag	 = 0x0333;		// any nonzero - don't wrap - stop logging at buffer end
//	g.evLogFlag	 = 'step';		// stop at each ELG

	IOLog( "UniNEnet - AllocateEventLog - &globals=%8x buffer=%8x phys=%8x\n",
							(unsigned int)&g, (unsigned int)g.evLogBuf, (UInt32)phys );
	return;
}/* end AllocateEventLog */


void EvLog( UInt32 a, UInt32 b, UInt32 ascii, char* str )
{
	register UInt32		*lp;			/* Long pointer		*/
	mach_timespec_t		time;
	UInt32				lefty;


	if ( g.evLogFlag == 0 )
		return;

	IOGetTime( &time );

	if ( g.evLogFlag == 0xDEBEEFED )
	{
		for ( lp = (UInt32*)g.evLogBuf; lp < (UInt32*)g.evLogBufe; lp++ )
			*lp = 0xDEBEEFED;
		g.evLogBufp	= g.evLogBuf;			// rewind
		g.evLogFlag	= 0x333;				// stop at end
	}

	lp = (UInt32*)g.evLogBufp;
	g.evLogBufp += 0x10;

	if ( g.evLogBufp >= g.evLogBufe )		/* handle buffer wrap around if any */
	{	 g.evLogBufp  = g.evLogBuf;
		if ( g.evLogFlag != 0xFEEDBEEF )	// make 0xFEEDBEEF a symbolic ???
			g.evLogFlag = 0;				/* stop tracing if wrap undesired	*/
		IOFlushProcessorCache( kernel_task, (IOVirtualAddress)g.evLogBuf, kEvLogSize );
	}

		/* compose interrupt level with 3 byte time stamp:	*/

	if ( g.pRegs )
		 lefty = OSSwapInt32( ((GMAC_Registers*)g.pRegs)->RxCompletion ) << 24;
	else lefty = 0xFF000000;
	*lp++ = lefty | (time.tv_nsec >> 10);	// ~ 1 microsec resolution
	*lp++ = a;
	*lp++ = b;
	*lp	  = ascii;

	if ( g.evLogFlag == 'step' )
	{	static char code[ 5 ] = {0,0,0,0,0};
		*(UInt32*)&code = ascii;
	//	kprintf( "%8x UniNEnet: %8x %8x %s		   %s\n", time.tv_nsec>>10, a, b, code, str );
	//	kprintf( "%8x UniNEnet: %8x %8x %s\n", time.tv_nsec>>10, a, b, code );
		IOLog( "%8x UniNEnet: %8x %8x\t%s\n",
					 time.tv_nsec>>10, (unsigned int)a, (unsigned int)b, code );
		IOSleep( 2 );
	}

	lp = (UInt32*)g.evLogBufe;	// to find next location
	*lp = (UInt32)g.evLogBufp;	// to log using OF.

	return;
}/* end EvLog */


void Alert( UInt32 a, UInt32 b, UInt32 ascii, char* str )
{
	char		work [ 256 ];
	char		name[] = "UniNEnet: ";
	char		*bp = work;
	UInt8		x;
	int			i;


	EvLog( a, b, ascii, str );
	EvLog( '****', '** A', 'lert', "*** Alert" );

	bcopy( name, bp, sizeof( name ) );
	bp += sizeof( name ) - 1;

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
	*bp++ = '\n';

//	kprintf( work );
//	panic( work );
///	Debugger( work );
///	IOLog( work );
///	g.evLogFlag = 0;		// prevent the log buffer fm getting wiped.
///	g.evLogFlag = 0x333;	// prevent the log buffer fm getting wiped.

		// The following is ensure viewability with Open Firmware:
	OSSynchronizeIO();
	IOFlushProcessorCache( kernel_task, (IOVirtualAddress)g.evLogBuf, kEvLogSize );

	return;
}/* end Alert */
#endif // USE_ELG


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

bool UniNEnet::init( OSDictionary *properties )
{
#if USE_ELG
	AllocateEventLog( kEvLogSize );
	fpGlobals = &g;				// improves ease of finding globals
	g.UniNEnetInstance = this;
	g.pRegs = 0;
	ELG( &g, g.evLogBufp, 'eNet', "UniNEnet::init - event logging set up." );
#endif /* USE_ELG */

    if ( super::init( properties ) == false )
        return false;

		/* Initialize my instance variables:	*/
    phyId          = 0xFF;
    linkStatusPrev = kLinkStatusUnknown;

    return true;
}/* end init */


bool UniNEnet::start( IOService *provider )
{    
    OSString	*matchEntry;
    IOWorkLoop	*myWorkLoop	= getWorkLoop();


	ELG( IOThreadSelf(), provider, 'Strt', "UniNEnet::start - this, provider." );

    matchEntry = OSDynamicCast( OSString, getProperty( gIONameMatchedKey ) );
    if ( matchEntry == 0 )
    {
        ALERT( 0, 0, 'Mat-', "UniNEnet::start: Cannot obtain matching property." );
        return false;
    }
	fBuiltin = matchEntry->isEqualTo( "gmac" );

    	// ensure that our provider is an IOPCIDevice

    nub = OSDynamicCast( IOPCIDevice, provider );

		// Invoke superclass's start routine

    if ( !nub || !super::start( provider ) )
        return false;

	if ( fBuiltin )
	{
			// Wait for KeyLargo to show up.
			// KeyLargo is used by hardwareResetPHY.
	
		keyLargo = waitForService( serviceMatching( "KeyLargo" ) );
		if ( keyLargo == 0 )	return false;
		keyLargo_resetUniNEthernetPhy = OSSymbol::withCString( "keyLargo_resetUniNEthernetPhy" );
		ELG( IOThreadSelf(), keyLargo, 'KeyL', "UniNEnet::start - KeyLargo" );
	}
	else
	{
		keyLargo = 0;
	}

		// Allocate memory for buffers etc

	transmitQueue = (IOGatedOutputQueue*)getOutputQueue();
    if ( !transmitQueue ) 
    {
        IOLog( "UniNEnet::start - Output queue initialization failed\n" );
        return false;
    }
    transmitQueue->retain();

		/* Allocate debug queue. This stores packets retired from the TX ring
		 * by the polling routine. We cannot call freePacket() or m_free() within
		 * the debugger context.
		 *
		 * The capacity of the queue is set at maximum to prevent the queue from
		 * calling m_free() due to over-capacity. But we don't expect the size
		 * of the queue to grow too large.
		 */
    debugQueue = IOPacketQueue::withCapacity( (UInt)-1 );
    if ( !debugQueue )
        return false;

		/* Allocate a IOMbufBigMemoryCursor instance. Currently, the maximum
		 * number of segments is set to 1. The maximum length for each segment
		 * is set to the maximum ethernet frame size (plus padding).
		 */    
    mbufCursor = IOMbufBigMemoryCursor::withSpecification( NETWORK_BUFSIZE, 1 );
    if ( !mbufCursor ) 
    {
        IOLog( "UniNEnet::start - IOMbufBigMemoryCursor allocation failure\n" );
        return false;
    }

    phyId		= 0xFF;
   
		/* Get a reference to the IOWorkLoop in our superclass.	*/
	myWorkLoop = getWorkLoop();

    	/* Allocate Interrupt source:	*/

    interruptSource = IOInterruptEventSource::interruptEventSource(
                        (OSObject*)this,
                        (IOInterruptEventAction)&UniNEnet::interruptOccurred,
                        (IOService*)provider,
                        (int)0 );

    if ( interruptSource == NULL )
    {	IOLog( "UniNEnet::start: Couldn't allocate Interrupt event source\n" );    
        return false;
    }
    if ( myWorkLoop->addEventSource( interruptSource ) != kIOReturnSuccess )
    {	IOLog( "UniNEnet::start - Couldn't add Interrupt event source\n" );    
        return false;
    }     

    	/* Allocate Timer event source:	*/

    timerSource = IOTimerEventSource::timerEventSource(
						this,
						(IOTimerEventSource::Action)&UniNEnet::timeoutOccurred );
    if ( timerSource == NULL )
    {
        IOLog( "UniNEnet::start - Couldn't allocate timer event source\n" );
        return false;
    }
    if ( myWorkLoop->addEventSource( timerSource ) != kIOReturnSuccess )
    {
		IOLog( "UniNEnet::start - Couldn't add timer event source\n" );        
        return false;
    }     

		/* allocate KDB buffer:	 */

    MGETHDR( txDebuggerPkt, M_DONTWAIT, MT_DATA );
    
    if ( !txDebuggerPkt ) 
    {	IOLog( "UniNEnet::start - Couldn't allocate KDB buffer\n" );
        return false;
    }

		/* Cache my MAC address:	*/

    if ( getHardwareAddress( &myAddress ) != kIOReturnSuccess )
    {	ALERT( 0, 0, 'gha-', "UniNEnet::start - getHardwareAddress failed" );
        return false;
    }

		/* Allocate memory for ring buffers:	*/

    if ( allocateMemory() == false )
    {	ALERT( 0, 0, 'alo-', "UniNEnet::start - allocateMemory failed" );    
        return false;
    }

		/* Attach an IOEthernetInterface client.	*/
		/* But don't register it just yet			*/
	ELG( IOThreadSelf(), 0, 'AttI', "UniNEnet::start - attach interface" );
    if ( !attachInterface( (IONetworkInterface**)&networkInterface, false ) )
    {	ALERT( 0, 0, 'Att-', "UniNEnet::start - attachInterface failed" );      
        return false;
    }

	if ( fBuiltin )
	{
			/* Attach a kernel debugger client:	*/
	
		ELG( IOThreadSelf(), 0, 'AttD', "UniNEnet::start - attach debugger" );
		attachDebuggerClient( &debugger );
	}

		/* If built-in (not card), ask Power Management to turn us on:	*/

	if ( fBuiltin )
	{		/// setPowerState was already called as part of registerWithPolicyMaker
		ELG( IOThreadSelf(), currentPowerState, 'powr', "UniNEnet::start - more power!" );
	}
	else
	{
		currentPowerState = 1;	/// temp - Card is always on.
	}

		/* Ready to service interface requests:	*/

	ELG( IOThreadSelf(), 0, 'RegS', "UniNEnet::start - networkInterface->registerService" );
    networkInterface->registerService();
	ELG( IOThreadSelf(), 0, 'Exit', "UniNEnet::start - exiting" );
    return true;
}/* end start */


bool UniNEnet::configureInterface( IONetworkInterface *netif )
{
    IONetworkData	 *nd;


	ELG( IOThreadSelf(), netif, 'cfig', "configureInterface" );

    if ( super::configureInterface( netif ) == false )
        return false;

		/* Grab a pointer to the statistics structure in the interface:	*/

    nd = netif->getNetworkData( kIONetworkStatsKey );
    if (!nd || !(fpNetStats = (IONetworkStats *) nd->getBuffer()))
    {
        IOLog("EtherNet(UniN): invalid network statistics\n");
        return false;
    }

		// Get the Ethernet statistics structure:

	nd = netif->getParameter( kIOEthernetStatsKey );
	if ( !nd || !(fpEtherStats = (IOEthernetStats*)nd->getBuffer()) )
	{
		IOLog( "UniNEnet::configureInterface - invalid ethernet statistics\n" );
        return false;
	}

    /*
     * Set the driver/stack reentrancy flag. This is meant to reduce
     * context switches. May become irrelevant in the future.
     */
    return true;
}/* end configureInterface */


void UniNEnet::free()
{
	ELG( this, 0, 'Free', "UniNEnet::free" );

	putToSleep();

	flushRings( true, true );	// Flush both Tx and Rx rings.

	if ( debugger )			debugger->release();
    if ( getWorkLoop() )	getWorkLoop()->disableAllEventSources();
    if ( timerSource )	  { timerSource->release(); timerSource = 0;	}
    if ( interruptSource )	interruptSource->release();
	if ( txDebuggerPkt )	freePacket( txDebuggerPkt );
	if ( transmitQueue )	transmitQueue->release();
	if ( debugQueue )		debugQueue->release();
	if ( networkInterface )	networkInterface->release();
	if ( mbufCursor )		mbufCursor->release();
	if ( mediumDict )		mediumDict->release();
	if ( ioMapEnet )		ioMapEnet->release();
	if ( dmaCommands )		IOFreeContiguous( (void*)dmaCommands, dmaCommandsSize );
	if ( workLoop )		  {	workLoop->release(); workLoop = 0;	}

	if ( keyLargo_resetUniNEthernetPhy )
	{
		keyLargo_resetUniNEthernetPhy->release();
		keyLargo_resetUniNEthernetPhy = 0;
	}

    super::free();
	return;
}/* end free */


/*-------------------------------------------------------------------------
 * Override IONetworkController::createWorkLoop() method and create
 * a workloop.
 *
 *-------------------------------------------------------------------------*/

bool UniNEnet::createWorkLoop()
{
    workLoop = IOWorkLoop::workLoop();

    return ( workLoop != 0 );
}/* end createWorkLoop */


	/* Override IOService::getWorkLoop() method to return our workloop.	*/

IOWorkLoop* UniNEnet::getWorkLoop() const
{
    return workLoop;
}/* end getWorkLoop */



void UniNEnet::interruptOccurred( IOInterruptEventSource *src, int /*count*/ )
{
    IODebuggerLockState	lockState;
	UInt32				interruptStatus;
	bool				doFlushQueue;
	bool				doService;


    if ( ready == false )
	{
		ALERT( 0, READ_REGISTER( Status ), 'int-', "interruptOccurred - not ready" );
		return;
	}

    do 
    {
		lockState = IODebuggerLock( this );

        interruptStatus = READ_REGISTER( Status )
						& ( kStatus_TX_INT_ME | kStatus_RX_DONE );
		ELG( READ_REGISTER( RxCompletion ), interruptStatus, 'Int+', "interruptOccurred - got status" );

        doService  = false;

        if ( interruptStatus & kStatus_TX_INT_ME )
        {
            txWDInterrupts++;
            KERNEL_DEBUG(DBG_GEM_TXIRQ | DBG_FUNC_START, 0, 0, 0, 0, 0 );
            doService = transmitInterruptOccurred();
            KERNEL_DEBUG(DBG_GEM_TXIRQ | DBG_FUNC_END,   0, 0, 0, 0, 0 );
			ETHERNET_STAT_ADD( dot3TxExtraEntry.interrupts );
        }

        doFlushQueue = false;

        if ( interruptStatus & kStatus_RX_DONE )
        {
            rxWDInterrupts++;
            KERNEL_DEBUG(DBG_GEM_RXIRQ | DBG_FUNC_START, 0, 0, 0, 0, 0 );
            doFlushQueue = receiveInterruptOccurred();
            KERNEL_DEBUG(DBG_GEM_RXIRQ | DBG_FUNC_END,   0, 0, 0, 0, 0 );
			ETHERNET_STAT_ADD( dot3RxExtraEntry.interrupts );
        }

		IODebuggerUnlock( lockState );

			/* Submit all received packets queued up						*/
			/* by _receiveInterruptOccurred() to the network stack.			*/
			/* The up call is performed without holding the debugger lock.	*/

		if ( doFlushQueue )
	    	networkInterface->flushInputQueue();

			/* Make sure the output queue is not stalled.	*/
		if ( doService && netifEnabled )
	   	 transmitQueue->service();
    } while ( interruptStatus );

	return;
}/* end interruptOccurred */


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

UInt32 UniNEnet::outputPacket(struct mbuf * pkt, void * param)
{
    UInt32 ret = kIOReturnOutputSuccess;

    KERNEL_DEBUG( DBG_GEM_TXQUEUE | DBG_FUNC_NONE,
                  (int) pkt, (int) pkt->m_pkthdr.len, 0, 0, 0 );

    /*
     * Hold the debugger lock so the debugger can't interrupt us
     */
    reserveDebuggerLock();
 	ELG( pkt, linkStatusPrev, 'OutP', "outputPacket" );

    if ( linkStatusPrev != kLinkStatusUp )
    {
		ELG( pkt, linkStatusPrev, 'Out-', "UniNEnet::outputPacket - link is down" );
        freePacket( pkt );
    }
    else if ( transmitPacket(pkt) == false )
    {
        ret = kIOReturnOutputStall;
    }
      
    releaseDebuggerLock();

    return ret;
}/* end outputPacket */


void UniNEnet::putToSleep()
{
	ELG( this, 0, 'Slep', "UniNEnet::putToSleep" );

	reserveDebuggerLock();

	ready = false;

	if ( timerSource ) 
		 timerSource->cancelTimeout();

	WRITE_REGISTER( InterruptMask, kInterruptMask_None );

	if ( getWorkLoop() )
		 getWorkLoop()->disableAllInterrupts();

    setLinkStatus( 0, 0 );

	stopChip();					// stop the DMA engines.
								// Flush all mbufs from TX ring.
	flushRings( true, false );	// Flush the Tx ring.

	stopPHY();					// Set up for wake on Magic Packet if wanted.

	currentPowerState = 0;		// No more accesses to chip's registers.

	if ( fBuiltin && !fWOL )
	{
		ALERT( 0, 0, '-Clk', "UniNEnet::setPowerState - turning off cell clock!!!" );
		OSSynchronizeIO();
		callPlatformFunction( "EnableUniNEthernetClock", true, (void*)false, 0, 0, 0 );
		OSSynchronizeIO();
		ALERT( 0, 0, '-clk', "UniNEnet::setPowerState - turned off cell clock!!!" );
	}

    releaseDebuggerLock();

    return;
}/* end putToSleep */


bool UniNEnet::wakeUp()
{
	bool	rc = false;
	bool	regAvail;
	UInt32	gemReg = 0;


	ELG( this, 0, 'Wake', "UniNEnet::wakeUp" );

    reserveDebuggerLock();

    ready = false;
	phyId = 0xFF;

	if ( timerSource ) 
		 timerSource->cancelTimeout();

	if ( getWorkLoop() )
		 getWorkLoop()->disableAllInterrupts();

    setLinkStatus( 0, 0 );	    // Initialize the link status.

	if ( fBuiltin )
	{
			// Set PHY and Cell to full power:

		callPlatformFunction( "EnableUniNEthernetClock", true, (void*)true, 0, 0, 0 );

		if ( ioMapEnet )			// Probe register access if able:
		{
			IOSleep( 10 );
	
			regAvail = ml_probe_read(	(vm_offset_t)&fpRegsPhys->Status,
										&(unsigned int)gemReg );

			if ( !regAvail )		// try again if cell clock disabled: 
			{
				IOLog( "UniNEnet::wakeUp - ethernet cell's clock is disabled.\n" );
				callPlatformFunction( "EnableUniNEthernetClock", true, (void*)true, 0, 0, 0 );
				IOSleep( 10 );
				regAvail = ml_probe_read(	(vm_offset_t)&fpRegsPhys->Status,
											&(unsigned int)gemReg );
				if ( !regAvail )	// return FALSE if cell clock still disabled. 
				{
					IOLog( "UniNEnet::wakeUp - ethernet cell's clock is still disabled.\n" );
					goto wakeUp_exit;	
				}/* end IF still disabled */
			}/* end IF need to try again. */ 
		}/* end IF can probe UniN register access */
	}/* end IF builtin ethernet */

		/* BUS MASTER, MEM I/O Space, MEM WR & INV	*/

	nub->configWrite32( 0x04, 0x16 );		// write to the Config space

		/* set Latency to Max , cache 32	*/

	nub->configWrite32( 0x0C, ((2 + (kGEMBurstSize * (0+1)))<< 8) | (CACHE_LINE_SIZE >> 2) );

	if ( ioMapEnet == NULL )
	{
		ioMapEnet = nub->mapDeviceMemoryWithRegister( 0x10 );
		if ( ioMapEnet == NULL )
			goto wakeUp_exit;

		fpRegs	= (GMAC_Registers*)ioMapEnet->getVirtualAddress();
		g.pRegs	= (UInt32)fpRegs;
		ELG( ioMapEnet, fpRegs, 'Adrs', "start - base eNet addr" );
			// for ml_probe_read on Wake:
		fpRegsPhys	= (GMAC_Registers*)ioMapEnet->getPhysicalAddress();
	}

	if ( !initRxRing() || !initTxRing() ) 
		goto wakeUp_exit;

	currentPowerState = 1;		// Allow access to cell's registers.

	WRITE_REGISTER( SoftwareReset, kSoftwareReset_TX | kSoftwareReset_RX );
    do
    {
		gemReg = READ_REGISTER( SoftwareReset );
    } 
	while( gemReg & (kSoftwareReset_TX | kSoftwareReset_RX) );

	initChip();					// set up the important registers in the cell

	if ( !mediumDict && createMediumTables() == false )
	{	ALERT( 0, 0, 'cmt-', "UniNEnet::start - createMediumTables failed" );    
		goto wakeUp_exit;
	}

	if ( fBuiltin )
	{
		hardwareResetPHY();			/* Generate a hardware PHY reset.	*/
	
		if ( phyId == 0xFF)
		{
			if ( miiFindPHY( &phyId ) == false )
				goto wakeUp_exit;
		}
	
		if ( getPhyType() == false )	// Also patches PHYs
			goto wakeUp_exit;
	}

	startPHY();						// Bring up the PHY and the MAC.

	if ( fBuiltin )
		miiInitializePHY( phyId );

	timerSource->setTimeoutMS( WATCHDOG_TIMER_MS );

	if ( getWorkLoop() )
		 getWorkLoop()->enableAllInterrupts();

	ready = true;

	monitorLinkStatus( true );		// startChip is done here.
	rc = true;

wakeUp_exit:
    
    releaseDebuggerLock();

    return rc;
}/* end wakeUp */


	/*-------------------------------------------------------------------------
	 * Called by IOEthernetInterface client to enable the controller.
	 * This method is always called while running on the default workloop
	 * thread.
	 *-------------------------------------------------------------------------*/

IOReturn UniNEnet::enable(IONetworkInterface * netif)
{
	ELG( this, netif, 'NetE', "UniNEnet::enable( netInterface* )" );

    /*
     * If an interface client has previously enabled us,
     * and we know there can only be one interface client
     * for this driver, then simply return true.
     */
    if ( netifEnabled )
    {
        IOLog("EtherNet(UniN): already enabled\n");
        return kIOReturnSuccess;
    }

    if ( (ready == false) && !wakeUp() )
        return kIOReturnIOError;

    /*
     * Mark the controller as enabled by the interface.
     */
    netifEnabled = true;

    /*
     * Start our IOOutputQueue object.
     */
    transmitQueue->setCapacity( TRANSMIT_QUEUE_SIZE );
    transmitQueue->start();

    return kIOReturnSuccess;
}/* end enable netif */


	/*-------------------------------------------------------------------------
	 * Called by IOEthernetInterface client to disable the controller.
	 * This method is always called while running on the default workloop
	 * thread.
	 *-------------------------------------------------------------------------*/
 
IOReturn UniNEnet::disable(IONetworkInterface * /*netif*/)
{
	ELG( this, debugEnabled, 'NetD', "disable( IONetworkInterface* )" );

    /*
     * Disable our IOOutputQueue object. This will prevent the
     * outputPacket() method from being called.
     */
    transmitQueue->stop();

    /*
     * Flush all packets currently in the output queue.
     */
    transmitQueue->setCapacity(0);
    transmitQueue->flush();

    	/* If we have no active clients, then disable the controller.	*/

	if ( debugEnabled == false )
		putToSleep();

    netifEnabled = false;

    return kIOReturnSuccess;
}/* end disable netif */


	/*-------------------------------------------------------------------------
	 * This method is called by our debugger client to bring up the controller
	 * just before the controller is registered as the debugger device. The
	 * debugger client is attached in response to the attachDebuggerClient()
	 * call.
	 *
	 * This method is always called while running on the default workloop
	 * thread.
	 *-------------------------------------------------------------------------*/

IOReturn UniNEnet::enable(IOKernelDebugger * /*debugger*/)
{
	ELG( this, ready, 'DbgE', "UniNEnet::enable( IOKernelDebugger* )" );

    	/* Enable hardware and make it ready to support the debugger client:	*/

    if ( (ready == false) && !wakeUp() )
        return kIOReturnIOError;

    /*
     * Mark the controller as enabled by the debugger.
     */
    debugEnabled = true;

    /*
     * Returning true will allow the kdp registration to continue.
     * If we return false, then we will not be registered as the
     * debugger device, and the attachDebuggerClient() call will
     * return NULL.
     */
    return kIOReturnSuccess;
}/* end enable debugger */


	/*-------------------------------------------------------------------------
	 * This method is called by our debugger client to stop the controller.
	 * The debugger will call this method when we issue a detachDebuggerClient().
	 *
	 * This method is always called while running on the default workloop
	 * thread.
	 *-------------------------------------------------------------------------*/

IOReturn UniNEnet::disable(IOKernelDebugger * /*debugger*/)
{
	ELG( this, netifEnabled, 'DbgD', "UniNEnet::disable( IOKernelDebugger* )" );
    debugEnabled = false;

    /*
     * If we have no active clients, then disable the controller.
     */
	if ( netifEnabled == false )
		putToSleep();

    return kIOReturnSuccess;
}/* end disable debugger */


IOReturn UniNEnet::getPacketFilters( const OSSymbol	*group, UInt32 *filters ) const
{
	ELG( group, filters, 'G PF', "UniNEnet::getPacketFilters" );

	if ( group == gIOEthernetWakeOnLANFilterGroup )
	{
		if ( fBuiltin )
			 *filters = kIOEthernetWakeOnMagicPacket;
		else *filters = 0;
		return kIOReturnSuccess;
	}

	return super::getPacketFilters( group, filters );
}/* end getPacketFilters */


IOReturn UniNEnet::setWakeOnMagicPacket( bool active )
{
	ELG( this, active, 'WoMP', "UniNEnet::setWakeOnMagicPacket" );
	fWOL = active;
	return kIOReturnSuccess;
}/* end setWakeOnMagicPacket */


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

void UniNEnet::timeoutOccurred(IOTimerEventSource * /*timer*/)
{
    IODebuggerLockState	lockState;
	bool  				doService = false;
	UInt32				txRingIndex;
	UInt32				x;


    if ( ready == false )
    {
		ALERT( txCommandHead, txCommandTail, 'TIM-', "UniNEnet::timeoutOccurred - spurious" );
        return;
    }
	ELG( txCommandHead, txCommandTail, 'Time', "UniNEnet::timeoutOccurred" );


		/* Update statistics from the GMAC statistics registers:	*/

	x = READ_REGISTER( LengthErrorCounter );
	if ( x )	WRITE_REGISTER( LengthErrorCounter, 0 );
	fpEtherStats->dot3StatsEntry.frameTooLongs += x;

	x = READ_REGISTER( AlignmentErrorCounter );
	if ( x )	WRITE_REGISTER( AlignmentErrorCounter, 0 );
	fpEtherStats->dot3StatsEntry.alignmentErrors += x;

	x = READ_REGISTER( FCSErrorCounter );
	if ( x )	WRITE_REGISTER( FCSErrorCounter, 0 );
	fpEtherStats->dot3StatsEntry.fcsErrors += x;

	x = READ_REGISTER( RxCodeViolationErrorCounter );
	if ( x )	WRITE_REGISTER( RxCodeViolationErrorCounter, 0 );
	fpEtherStats->dot3StatsEntry.internalMacTransmitErrors += x;

	x = READ_REGISTER( FirstAttemptSuccessfulCollisionCounter );
	if ( x )	WRITE_REGISTER( FirstAttemptSuccessfulCollisionCounter, 0 );
	fpEtherStats->dot3StatsEntry.singleCollisionFrames += x;

	x = READ_REGISTER( ExcessiveCollisionCounter );
	if ( x )	WRITE_REGISTER( ExcessiveCollisionCounter, 0 );
	fpEtherStats->dot3StatsEntry.excessiveCollisions += x;

	x = READ_REGISTER( LateCollisionCounter );
	if ( x )	WRITE_REGISTER( LateCollisionCounter, 0 );
	fpEtherStats->dot3StatsEntry.lateCollisions += x;

	lockState = IODebuggerLock( this );

    monitorLinkStatus( false );	/// ??? don't do this if Tx and Rx are moving

    /*
     * If there are pending entries on the Tx ring
     */
    if ( txCommandHead != txCommandTail )
    {
        /* 
         * If the hardware tx pointer did not move since the last
         * check, increment the txWDCount.
         */
		txRingIndex = READ_REGISTER( TxCompletion );
        if ( txRingIndex == txRingIndexLast )
        {
            txWDCount++;         
        }
        else
        {
            txWDCount = 0;
            txRingIndexLast = txRingIndex;
        }
   
        if ( txWDCount > 2 )
        {
            /* 
             * We only take interrupts every 64 tx completions, so we may be here just
             * to do normal clean-up of tx packets. We check if the hardware tx pointer
             * points to the next available tx slot. This indicates that we transmitted all
             * packets that were scheduled vs rather than the hardware tx being stalled.
             */
            if ( txRingIndex != txCommandTail )
            {
                UInt32        interruptStatus, compReg, kickReg;
 
				interruptStatus = READ_REGISTER( Status );
				compReg			= READ_REGISTER( TxCompletion );
				kickReg			= READ_REGISTER( TxKick );

                IOLog( "Tx Int Timeout - Comp = %04x Kick = %04x Int = %08x\n\r", (int)compReg, (int)kickReg, (int)interruptStatus ); 
            }

//          dumpRegisters();

            transmitInterruptOccurred();

            doService = true;

            txRingIndexLast = txRingIndex;
            txWDCount = 0;
        }
    }
    else
    {
        txWDCount        = 0;
    }
    
    // Monitor receiver's health.
    
    if ( rxWDInterrupts == 0 )
    {
        UInt32 rxMACStatus;

        switch ( rxWDCount )
        {
            case 0:
            case 1:
                rxWDCount++;	// Extend timeout
                break;

            default:
                // We could be less conservative here and restart the
                // receiver unconditionally.

                rxMACStatus = READ_REGISTER( RxMACStatus );

                if ( rxMACStatus & kRX_MAC_Status_Rx_Overflow )
                {
                    // Bad news, the receiver may be deaf as a result of this
                    // condition, and if so, a RX MAC reset is needed. Note
                    // that reading this register will clear all bits.

                    restartReceiver();

					NETWORK_STAT_ADD( inputErrors );
					ETHERNET_STAT_ADD( dot3RxExtraEntry.watchdogTimeouts );
                }
                rxWDCount = 0;
                break;
        }
    }
    else
    {
        // Reset watchdog

        rxWDCount      = 0;
        rxWDInterrupts = 0;
    }

		/* Clean-up after the debugger if the debugger was active:	*/

	if ( debugTxPoll )
	{
		debugQueue->flush();
		debugTxPoll	= false;
		doService	= true;
	}
	IODebuggerUnlock( lockState );

	/*
	 * Make sure the queue is not stalled.
	 */
	if (doService && netifEnabled)
	{
		transmitQueue->service();
	}

    /*
     * Restart the watchdog timer
     */
    timerSource->setTimeoutMS(WATCHDOG_TIMER_MS);
	return;
}/* end timeoutOccurred */


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

const OSString * UniNEnet::newVendorString() const
{
    return OSString::withCString("Apple");
}

const OSString * UniNEnet::newModelString() const
{
    return OSString::withCString("gmac+");
}

const OSString * UniNEnet::newRevisionString() const
{
    return OSString::withCString("");
}


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

IOReturn UniNEnet::setPromiscuousMode( bool active )
{
	ELG( 0, active, 'SetP', "setPromiscuousMode" );

	reserveDebuggerLock();

	isPromiscuous	= active;

	rxMacConfigReg	= READ_REGISTER( RxMACConfiguration );

	if ( active )
		 rxMacConfigReg |=  kRxMACConfiguration_Promiscuous;
	else rxMacConfigReg &= ~kRxMACConfiguration_Promiscuous;

	WRITE_REGISTER( RxMACConfiguration, rxMacConfigReg );

	releaseDebuggerLock();

	return kIOReturnSuccess;
}/* end setPromiscuousMode */


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

IOReturn UniNEnet::setMulticastMode( bool active )
{
	ELG( this, active, 'SetM', "setMulticastMode" );
	multicastEnabled = active;

	return kIOReturnSuccess;
}/* end setMulticastMode */


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

IOReturn UniNEnet::setMulticastList(IOEthernetAddress *addrs, UInt32 count)
{
	ELG( addrs, count, 'SetL', "setMulticastList" );
    reserveDebuggerLock();
    
    resetHashTableMask();
    for (UInt32 i = 0; i < count; i++) 
    {
        addToHashTableMask(addrs->bytes);
        addrs++;
    }
    updateHashTableMask();
    
    releaseDebuggerLock();
    return kIOReturnSuccess;
}/* end setMulticastList */


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

IOOutputQueue* UniNEnet::createOutputQueue()
{
	return IOBasicOutputQueue::withTarget( this, TRANSMIT_QUEUE_SIZE );
}/* end createOutputQueue */


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

	static struct MediumTable
	{
		UInt32	type;
		UInt32	speed;
	} 
	mediumTable[] =
	{
		{ kIOMediumEthernetNone                                  ,   0   },
		{ kIOMediumEthernetAuto                                  ,   0   },
		{ kIOMediumEthernet10BaseT    | kIOMediumOptionHalfDuplex,	10   },
		{ kIOMediumEthernet10BaseT    | kIOMediumOptionFullDuplex,	10   },
		{ kIOMediumEthernet100BaseTX  | kIOMediumOptionHalfDuplex,	100  },
		{ kIOMediumEthernet100BaseTX  | kIOMediumOptionFullDuplex,	100  },
		{ kIOMediumEthernet1000BaseSX | kIOMediumOptionFullDuplex,	1000 },
		{ kIOMediumEthernet1000BaseTX | kIOMediumOptionFullDuplex,	1000 }
	};


bool UniNEnet::createMediumTables()
{
	IONetworkMedium		*medium;
	UInt64				maxSpeed;
	UInt16				phyWord;
	UInt32				i;


	if ( fBuiltin )
	{
		miiReadWord( &phyWord, MII_CONTROL, phyId );

		if ( phyWord & MII_CONTROL_SPEED_SELECTION_2 )
			 maxSpeed = 1000;
		else maxSpeed = 100;
	}
	else
	{
		maxSpeed = 1000;	// GEM card.
	}

	mediumDict = OSDictionary::withCapacity( sizeof( mediumTable ) / sizeof( mediumTable[0] ) );
	ELG( 0, mediumDict, 'MTbl', "createMediumTables" );
	if ( mediumDict == 0 )
		return false;

	for ( i = 0; i < sizeof( mediumTable ) / sizeof( mediumTable[0] ); i++ )
	{
		medium = IONetworkMedium::medium( mediumTable[i].type, mediumTable[i].speed );
		if ( medium && (medium->getSpeed() <= maxSpeed) )
		{
			IONetworkMedium::addMedium( mediumDict, medium );
			medium->release();
		}
	}/* end FOR */

	if ( publishMediumDictionary( mediumDict ) != true )
		return false;

	medium = IONetworkMedium::getMediumWithType( mediumDict, kIOMediumEthernetAuto );

    setCurrentMedium( medium );

    return true;
}/* end createMediumTables */


#ifdef HDW_CHECKSUM
IOReturn UniNEnet::getChecksumSupport(	UInt32	*checksumMask,
										UInt32	checksumFamily,
										bool	/*isOutput*/ )
{
	if ( checksumFamily != kChecksumFamilyTCPIP )
		return kIOReturnUnsupported;

	*checksumMask	= kChecksumTCPSum16;

	return kIOReturnSuccess;
}/* end getChecksumSupport */
#endif // HDW_CHECKSUM


void UniNEnet::writeRegister( volatile UInt32 *pReg, UInt32 data )
{
	if ( pReg != &fpRegs->MIFBitBangFrame_Output )
		ELG( data, (UInt32)pReg - (UInt32)fpRegs, 'wReg', "writeRegister" );

	OSWriteLittleInt32( pReg, 0, data );
///	OSSynchronizeIO();	// this may be needed only sometimes.
	return;
}/* end writeRegister */
