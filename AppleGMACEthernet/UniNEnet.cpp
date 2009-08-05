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
 * Copyright (c) 1998-2005 Apple Computer
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


		/* The GMAC registers are grouped in discontiguous sets.		*/
		/* gGMACRegisterTemplate describes the sets with a length		*/
		/* in bytes of a set, and an offset for the start of the set.	*/

	struct LengthOffset
	{
		UInt32	setLength, setOffset;
	};

	LengthOffset	gGMACRegisterTemplate[] =	/* This is a global	*/
	{
		/*	Length	Offset	*/

		{	0x20,	0x0000	},		/* Global Resources			*/
		{	0x14,	0x1000	},
		{	0x38,	0x2000	},		/* Transmit DMA Registers	*/
		{	0x1C,	0x2100	},
		{	0x14,	0x3000	},		/* Wake On Magic Packet		*/
		{	0x2C,	0x4000	},		/* Receive DMA Registers	*/
		{	0x24,	0x4100	},
		{	0x68,	0x6000	},		/* MAC Registers			*/
		{	0xB8,	0x6080	},
		{	0x20,	0x6200	},		/* MIF Registers			*/
		{	0x1C,	0x9000	},		/* PCS/Serialink			*/
		{	0x10,	0x9050	},
		{	0,		0		}
	};


#define super IOEthernetController

	OSDefineMetaClassAndStructors( UniNEnet, IOEthernetController )		;

#pragma mark -
#pragma mark еее Event Logging еее
#pragma mark -

#if USE_ELG
void UniNEnet::AllocateEventLog( UInt32 size )
{
	mach_timespec_t		time;
	IOByteCount			length;


    fpELGMemDesc = IOBufferMemoryDescriptor::withOptions(	kIOMemoryPhysicallyContiguous,
															kEvLogSize,
															PAGE_SIZE );
	if ( !fpELGMemDesc )
	{
		kprintf( "AllocateEventLog - UniNEnet evLog allocation failed " );
		return;
	}

	fpELGMemDesc->prepare( kIODirectionNone );
	fpELG = (elg*)fpELGMemDesc->getBytesNoCopy();
	bzero( fpELG, kEvLogSize );
	fpELG->physAddr	= fpELGMemDesc->getPhysicalSegment64( 0, &length );	// offset: 0; length

//#define OPEN_FIRMWARE
#ifdef OPEN_FIRMWARE
	IOSetProcessorCacheMode(	kernel_task,
								(IOVirtualAddress)fpELG,
								size,
								kIOMapWriteThruCache );
#endif // OPEN_FIRMWARE

	fpELG->evLogBuf		= (UInt8*)fpELG + sizeof( struct elg );
	fpELG->evLogBufe	= (UInt8*)fpELG + kEvLogSize - 0x20; // ??? overran buffer?
	fpELG->evLogBufp	= fpELG->evLogBuf;
//	fpELG->evLogFlag	 = 0xFeedBeef;	// continuous wraparound
	fpELG->evLogFlag	 = 0x03330333;	// > kEvLogSize - don't wrap - stop logging at buffer end
//	fpELG->evLogFlag	 = 0x0099;		// < #elements - count down and stop logging at 0
//	fpELG->evLogFlag	 = 'step';		// stop at each ELG

	IOGetTime( &time );
	fpELG->startTimeSecs	= time.tv_sec;

	IOLog( "\033[32mUniNEnet::AllocateEventLog - buffer=%8x phys=%16llx \033[0m \n",
							(unsigned int)fpELG, fpELG->physAddr );
	IOLog( "UniNEnet::AllocateEventLog - compiled on %s, %s\n", __DATE__, __TIME__ );
	return;
}/* end AllocateEventLog */


void UniNEnet::EvLog( UInt32 a, UInt32 b, UInt32 ascii, char* str )
{
	register UInt32		*lp;			/* Long pointer					*/
	register elg		*pe = fpELG;	/* pointer to elg structure		*/
	mach_timespec_t		time;
	UInt32				lefty;


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
			IOFlushProcessorCache( kernel_task, (IOVirtualAddress)fpELG, kEvLogSize );
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

#ifdef OPEN_FIRMWARE
	IOFlushProcessorCache( kernel_task, (IOVirtualAddress)(lp - 3), 0x10 );
	IOFlushProcessorCache( kernel_task, (IOVirtualAddress)pe, sizeof( elg ) );
#endif // OPEN_FIRMWARE

#ifdef STEPPABLE
	if ( pe->evLogFlag == 'step' )
	{	static char code[ 5 ] = {0,0,0,0,0};
		*(UInt32*)&code = ascii;
	//	kprintf( "%8x UniNEnet: %8x %8x %s		   %s\n", time.tv_nsec>>10, a, b, code, str );
	//	kprintf( "%8x UniNEnet: %8x %8x %s\n", time.tv_nsec>>10, a, b, code );
		IOLog( "%8x UniNEnet: %8x %8x\t%s\n",
					 time.tv_nsec>>10, (unsigned int)a, (unsigned int)b, code );
		IOSleep( 2 );
	}
#endif // STEPPABLE

	return;
}/* end EvLog */


UInt32 UniNEnet::Alrt( UInt32 a, UInt32 b, UInt32 ascii, char* str )
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

#ifdef OPEN_FIRMWARE
		// The following is ensure viewability with Open Firmware:
	OSSynchronizeIO();
	IOFlushProcessorCache( kernel_task, (IOVirtualAddress)fpELG, kEvLogSize );
#endif // OPEN_FIRMWARE

//	fpELG->evLogFlag = 0;	// stop logging but alertCount can continue increasing.

///if ( fpELG->evLogFlag == 0xFeedBeef	 )
///	 fpELG->evLogFlag = 100;	// cruise to see what happens next.

//	kprintf( work );
//	panic( work );
//	Debugger( work );
	IOLog( work );

	return 0xDeadBeef;
}/* end Alrt */
#endif // USE_ELG

#pragma mark -
#pragma mark еее override methods еее
#pragma mark -

bool UniNEnet::init( OSDictionary *properties )
{
    UInt32		rxOff = kMaxFrameSize_default * 2 / kPauseThresholds_Factor;
    UInt32		rxOn  = kMaxFrameSize_default * 1 / kPauseThresholds_Factor;

#if USE_ELG
	AllocateEventLog( kEvLogSize );
	ELG( this, fpELG, 'GMAC', "UniNEnet::init - event logging set up." );
#endif /* USE_ELG */

    if ( super::init( properties ) == false )
        return false;

		/* Initialize some instance variables:	*/

		/* Set receive flow control pause thresholds:									*/
		/* Pause OFF when 2 packets back up in the FIFO.								*/
		/* Pause  ON when the FIFO is down to one packet.								*/
		/* There are 2 reasons for the FIFO to back up:									*/
		/* The controller is vying for bandwidth on the PCI bus with another device or	*/
		/* the interrupt handler, driver, and network stack are not offloading			*/
		/* the Rx ring fast enough.														*/
		/* Bus contention should not happen in most machines. So setting the rxOff to	*/
		/* to cover 2 frames is more than adequate. As far as frames backing up in		*/
		/* the Rx ring, we want to rxOff as soon as possible to minimize dropped		*/
		/* frames.	The rxOn value must, of course, be less than rxOff value so it is	*/
		/* set to the value for one full size frame.									*/
		/* The pause time value shuold be set large enough to cover interrupt latency,	*/
		/* processing of a full Rx ring, and some time for Tx ring processing.			*/
		/* A high value is most often not a problem since the rxOn will override it.	*/
		/* However, if we don't process packets fast enough, they could back up in the	*/
		/* switch which may then drop them.												*/

	fPauseThresholds	= (rxOff << kPauseThresholds_OFF_Threshold_Shift)
						| (rxOn	 << kPauseThresholds_ON_Threshold_Shift);
	fCellClockEnabled	= false;
	fAutoNegotiate		= true;
	fMediumType			= kIOMediumEthernetAuto;	// default to autoNegotiation
	fSendPauseCommand	= kSendPauseCommand_default;
	fTxRingIndexLast	= ~0U;
	fMaxFrameSize		= kIOEthernetMaxPacketSize;
    return true;
}/* end init */


bool UniNEnet::start( IOService *provider )
{    
    OSString	*matchEntry;
    OSNumber	*numObj;
	UInt32		x, xFactor;


	ELG( IOThreadSelf(), provider, 'Strt', "UniNEnet::start - this, provider." );

    matchEntry = OSDynamicCast( OSString, getProperty( gIONameMatchedKey ) );
    if ( matchEntry == 0 )
    {
        ALRT( 0, 0, 'Mat-', "UniNEnet::start: Cannot obtain matching property." );
        return false;
    }
	fBuiltin	= matchEntry->isEqualTo( "gmac" ) || matchEntry->isEqualTo( "K2-GMAC" );
    fK2			= matchEntry->isEqualTo( "K2-GMAC" );

    	// ensure that our provider is an IOPCIDevice

    nub = OSDynamicCast( IOPCIDevice, provider );

		// Invoke superclass's start routine

    if ( !nub || !super::start( provider ) )	/* calls createWorkLoop & getWorkLoop	*/
        return false;

	if ( fBuiltin )
	{
			// Wait for KeyLargo to show up.
			// KeyLargo is used to hardware reset the PHY.
	
		if ( matchEntry->isEqualTo( "gmac" ) )
			 keyLargo = waitForService( serviceMatching( "KeyLargo" ) );
		else keyLargo = waitForService( serviceMatching( "AppleK2" ) );
		if ( keyLargo == 0 )
				return false;
		keyLargo_resetUniNEthernetPhy = OSSymbol::withCString( "keyLargo_resetUniNEthernetPhy" );
		ELG( IOThreadSelf(), keyLargo, 'KeyL', "UniNEnet::start - KeyLargo" );
	}
	else
	{
		keyLargo = 0;
	}

		// Allocate memory for buffers etc

	transmitQueue = (IOBasicOutputQueue*)getOutputQueue();
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

		/* Allocate IOMbufBigMemoryCursor instances:	*/

	fTxMbufCursor = IOMbufBigMemoryCursor::withSpecification( NETWORK_BUFSIZE, MAX_SEGS_PER_TX_MBUF );
    if ( !fTxMbufCursor ) 
    {
        ALRT( 0, 0, 'tMC-', "UniNEnet::start - Tx IOMbufBigMemoryCursor allocation failure" );
        return false;
    }

	phyId = 0xFF;
	fLinkStatus = kLinkStatusUnknown;

	if ( !fWorkLoop )
		return false;

    	/* Allocate Interrupt source:	*/

    interruptSource = IOInterruptEventSource::interruptEventSource(
							(OSObject*)this,
							OSMemberFunctionCast(	IOInterruptEventAction,
													this,
													&UniNEnet::interruptOccurred ),
							(IOService*)provider,
							(int)0 );

    if ( interruptSource == NULL )
    {	IOLog( "UniNEnet::start: Couldn't allocate Interrupt event source\n" );    
        return false;
    }
    if ( fWorkLoop->addEventSource( interruptSource ) != kIOReturnSuccess )
    {	IOLog( "UniNEnet::start - Couldn't add Interrupt event source\n" );    
        return false;
    }     

    	/* Allocate Timer event source:	*/

    timerSource = IOTimerEventSource::timerEventSource(
						this,
						OSMemberFunctionCast(	IOTimerEventSource::Action,
												this,
												&UniNEnet::timeoutOccurred ) );
    if ( timerSource == NULL )
    {
        IOLog( "UniNEnet::start - Couldn't allocate timer event source\n" );
        return false;
    }
    if ( fWorkLoop->addEventSource( timerSource ) != kIOReturnSuccess )
    {
		IOLog( "UniNEnet::start - Couldn't add timer event source\n" );        
        return false;
    }     

		/* Allocate a KDB buffer (also used by tx timeout code):	*/

	txDebuggerPkt = allocatePacket( NETWORK_BUFSIZE );
    if ( !txDebuggerPkt )
    {	ALRT( 0, NETWORK_BUFSIZE, 'KDP-', "UniNEnet::start - Couldn't allocate KDB buffer." );
        return false;
    }

		/* Cache my MAC address:	*/

    if ( getHardwareAddress( &myAddress ) != kIOReturnSuccess )
    {	ALRT( 0, 0, 'gha-', "UniNEnet::start - getHardwareAddress failed" );
        return false;
    }

		/* set defaults:	*/

	fTxQueueSize	= TRANSMIT_QUEUE_SIZE;
	fTxRingElements	= TX_RING_LENGTH;	fTxRingLengthFactor = TX_RING_LENGTH_FACTOR;
	fRxRingElements	= RX_RING_LENGTH;	fRxRingLengthFactor = RX_RING_LENGTH_FACTOR;

		/* Override with values from the IORegistry:	*/

    numObj = OSDynamicCast( OSNumber, getProperty( kTxQueueSize ) );
    if ( numObj )
    {
		x = numObj->unsigned32BitValue();
		if ( x >= 32 && x <= 9999 )
			fTxQueueSize = x;
		ELG( x, fTxQueueSize, '=txq', "UniNEnet::start - TxQueueSize" );
	}

    numObj = OSDynamicCast( OSNumber, getProperty( kTxRingElements ) );
    if ( numObj )
    {
		xFactor = 0;
		x = numObj->unsigned32BitValue();
		switch ( x )
		{
		case 8192:	++xFactor;
		case 4096:	++xFactor;
		case 2048:	++xFactor;
		case 1024:	++xFactor;
		case 512:	++xFactor;
		case 256:	++xFactor;
		case 128:	++xFactor;
		case 64:	++xFactor;
		case 32:
			fTxRingElements		= x;
			fTxRingLengthFactor	= xFactor;
		}
		ELG( x, fTxRingElements, '=txe', "UniNEnet::start - TxRingElements" );
	}

    numObj = OSDynamicCast( OSNumber, getProperty( kRxRingElements ) );
    if ( numObj )
    {
		xFactor = 0;
		x = numObj->unsigned32BitValue();
		switch ( x )
		{
		case 8192:	++xFactor;
		case 4096:	++xFactor;
		case 2048:	++xFactor;
		case 1024:	++xFactor;
		case 512:	++xFactor;
		case 256:	++xFactor;
		case 128:	++xFactor;
		case 64:	++xFactor;
		case 32:
			fRxRingElements		= x;
			fRxRingLengthFactor	= xFactor;
		}
		ELG( x, fRxRingElements, '=rxe', "UniNEnet::start - TxRingElements" );
	}

	ELG( fTxQueueSize, fTxRingElements << 16 | fRxRingElements, 'parm', "UniNEnet::start - config parms" );

		/* Allocate memory for ring buffers:	*/

    if ( allocateMemory() == false )
    {	ALRT( 0, 0, 'alo-', "UniNEnet::start - allocateMemory failed" );    
        return false;
    }

		/* BUS MASTER, MEM I/O Space, MEM WR & INV	*/

	nub->configWrite32( 0x04, 0x16 );		// write to the Config space

	ioMapEnet = nub->mapDeviceMemoryWithRegister( 0x10 );
	if ( ioMapEnet == NULL )
		return false;

	fpRegs = (GMAC_Registers*)ioMapEnet->getVirtualAddress();
	ELG( ioMapEnet, fpRegs, 'Adrs', "UniNEnet::start - base eNet addr" );

		/* Initialize instance variables to key register values:	*/

	fXIFConfiguration			= kXIFConfiguration_Tx_MII_OE;
	fMACControlConfiguration	= 0;
	fConfiguration				= kConfiguration_TX_DMA_Limit
								| kConfiguration_RX_DMA_Limit
								| kConfiguration_Infinite_Burst
								| kConfiguration_RonPaulBit
								| kConfiguration_EnableBug2Fix;
	fTxConfiguration			= kTxConfiguration_TxFIFO_Threshold
								| fTxRingLengthFactor << kTxConfiguration_Tx_Desc_Ring_Size_Shift;
	fTxMACConfiguration			= 0;
	fRxConfiguration			= kRxConfiguration_RX_DMA_Threshold
						//		| kRxConfiguration_Batch_Disable	may cause 4x primary interrupts
								| fRxRingLengthFactor << kRxConfiguration_Rx_Desc_Ring_Size_Shift
								| kRxConfiguration_Checksum_Start_Offset;
	fRxMACConfiguration			= 0;

		/* Attach an IOEthernetInterface client.	*/
		/* But don't register it just yet			*/
	ELG( IOThreadSelf(), 0, 'AttI', "UniNEnet::start - attach interface" );
    if ( !attachInterface( (IONetworkInterface**)&networkInterface, false ) )
    {	ALRT( 0, 0, 'Att-', "UniNEnet::start - attachInterface failed" );      
        return false;
    }

	if ( fBuiltin )
	{
			/* Attach a kernel debugger client:	*/
	
		ELG( IOThreadSelf(), 0, 'AttD', "UniNEnet::start - attach debugger" );
		attachDebuggerClient( &debugger );
	}

		/* Ready to service interface requests:	*/

	ELG( IOThreadSelf(), 0, 'RegS', "UniNEnet::start - networkInterface->registerService" );
    networkInterface->registerService();
    return true;
}/* end start */


bool UniNEnet::configureInterface( IONetworkInterface *netif )
{
    IONetworkData	 *nd;


	ELG( IOThreadSelf(), netif, 'cfig', "UniNEnet::configureInterface" );

    if ( super::configureInterface( netif ) == false )
        return false;

		/* Grab a pointer to the statistics structure in the interface:	*/

    nd = netif->getNetworkData( kIONetworkStatsKey );
    if ( !nd || !(fpNetStats = (IONetworkStats*)nd->getBuffer()) )
    {
        IOLog( "EtherNet(UniN): invalid network statistics\n" );
        return false;
    }

		// Get the Ethernet statistics structure:

	nd = netif->getParameter( kIOEthernetStatsKey );
	if ( !nd || !(fpEtherStats = (IOEthernetStats*)nd->getBuffer()) )
	{
		IOLog( "UniNEnet::configureInterface - invalid ethernet statistics\n" );
        return false;
	}

    return true;
}/* end configureInterface */


void UniNEnet::free()
{
	ELG( this, 0, 'Free', "UniNEnet::free" );

	flushRings( true, true );	// Flush both Tx and Rx rings.

	if ( debugger )			debugger->release();
    if ( fWorkLoop )		fWorkLoop->disableAllEventSources();
    if ( timerSource )		timerSource->release();
    if ( interruptSource )	interruptSource->release();
	if ( txDebuggerPkt )	freePacket( txDebuggerPkt );
	if ( transmitQueue )	transmitQueue->release();
	if ( debugQueue )		debugQueue->release();
	if ( networkInterface )	networkInterface->release();
	if ( fTxMbufCursor )	fTxMbufCursor->release();
	if ( fMediumDict )		fMediumDict->release();
	if ( ioMapEnet )		ioMapEnet->release();

	if ( fTxRingMemDesc )
	{	fTxRingMemDesc->complete(  kIODirectionOutIn );
		fTxRingMemDesc->release();
	}
	if ( fRxRingMemDesc )
	{	fRxRingMemDesc->complete(  kIODirectionOutIn );
		fRxRingMemDesc->release();
	}
	if ( fTxMbuf )			IOFree( fTxMbuf, sizeof( mbuf_t ) * fTxRingElements );
	if ( fRxMbuf )			IOFree( fRxMbuf, sizeof( mbuf_t ) * fRxRingElements );

	if ( fWorkLoop )			fWorkLoop->release();

	if ( keyLargo_resetUniNEthernetPhy )
	{
		keyLargo_resetUniNEthernetPhy->release();
		keyLargo_resetUniNEthernetPhy = 0;
	}

	if ( fpELGMemDesc )
	{	fpELGMemDesc->complete(  kIODirectionNone );
		fpELGMemDesc->release();
	}

    super::free();
	return;
}/* end free */


	/*-------------------------------------------------------------------------
	 * Override IONetworkController::createWorkLoop() method and create
	 * a workloop so that we don't use the global workLoop.
	 * Called by IONetworkController during start.
	 *-------------------------------------------------------------------------*/

bool UniNEnet::createWorkLoop()
{
    fWorkLoop = IOWorkLoop::workLoop();		/* Get our own workloop	*/
	ELG( 0, fWorkLoop, 'c WL', "UniNEnet::createWorkLoop" );
    return (fWorkLoop != 0);
}/* end createWorkLoop */


	/* Override IOService::getWorkLoop() method to return our workloop.	*/

IOWorkLoop* UniNEnet::getWorkLoop() const
{
	return fWorkLoop;
}/* end getWorkLoop */


void UniNEnet::interruptOccurred( IOInterruptEventSource *src, int /*count*/ )
{
    IODebuggerLockState	lockState = kIODebuggerLockTaken;
	UInt32				interruptStatus;
	UInt32				rxMACStatus;
	bool				doFlushQueue;
	bool				doService;


    if ( fReady == false )
	{
		if ( fCellClockEnabled == false )
	         interruptStatus = 0x8BadF00d;
		else interruptStatus = READ_REGISTER( Status );
		ELG( this, interruptStatus, 'int-', "UniNEnet::interruptOccurred - not ready" );
		return;
	}

	if ( fBuiltin )	lockState = IODebuggerLock( this );

	interruptStatus = READ_REGISTER( Status );	// auto-clear register
	ELG( READ_REGISTER( RxCompletion ), interruptStatus, 'Int+', "UniNEnet::interruptOccurred - got status" );
	fTxCompletion = interruptStatus >> 19;
		/* Bump statistics if either the Rx ring or the Rx FIFO overflowed:	*/

	rxMACStatus	= READ_REGISTER( RxMACStatus );				/* NB: Auto-clear register	*/
	fRxMACStatus |= rxMACStatus;							/* save for timeout routine	*/
	if ( interruptStatus & kStatus_Rx_Buffer_Not_Available
		|| rxMACStatus & kRX_MAC_Status_Rx_Overflow )
	{		/* If either overflowed:	*/
	///	if ( fMACControlConfiguration & kMACControlConfiguration_Send_Pause_Enable )
	///		WRITE_REGISTER( SendPauseCommand, fSendPauseCommand | 0x10000 );
		ELG( 0, rxMACStatus, 'Rx--', "UniNEnet::interruptOccurred - Rx overflow" );
		ETHERNET_STAT_ADD( dot3RxExtraEntry.overruns );
		NETWORK_STAT_ADD( inputErrors );
	}

	fIntStatusForTO |= interruptStatus;	// accumulate Tx & Rx int bits for timer code

	doService  = false;

	if ( interruptStatus & (kStatus_TX_INT_ME | kStatus_TX_ALL) )
	{
		KERNEL_DEBUG( DBG_GEM_TXIRQ | DBG_FUNC_START, 0, 0, 0, 0, 0 );
		doService = transmitInterruptOccurred();
		KERNEL_DEBUG( DBG_GEM_TXIRQ | DBG_FUNC_END,   0, 0, 0, 0, 0 );
		ETHERNET_STAT_ADD( dot3TxExtraEntry.interrupts );
	}

	doFlushQueue = false;

	if ( interruptStatus & kStatus_RX_DONE )
	{
		KERNEL_DEBUG( DBG_GEM_RXIRQ | DBG_FUNC_START, 0, 0, 0, 0, 0 );
		doFlushQueue	= receivePackets( false );
		fRxMACStatus	= 0;	// Rx not hung - clear any FIFO overflow condition for timeout
		KERNEL_DEBUG( DBG_GEM_RXIRQ | DBG_FUNC_END,   0, 0, 0, 0, 0 );
		ETHERNET_STAT_ADD( dot3RxExtraEntry.interrupts );
	}

#ifdef LATER
	UInt32				mifStatus;
	if ( interruptStatus & kStatus_MIF_Interrupt )
	{
		mifStatus = READ_REGISTER( MIFStatus );		// clear the interrupt
		ELG( 0, mifStatus, '*MIF', "UniNEnet::interruptOccurred - MIF interrupt" );
	}
#endif // LATER


	if ( fBuiltin )	IODebuggerUnlock( lockState );

		/* Submit all received packets queued up by receivePackets() to the network stack.	*/
		/* The up call is performed without holding the debugger lock.						*/

	if ( doFlushQueue )
		networkInterface->flushInputQueue();

	if ( doService && netifEnabled )	/* Ensure output queue is not stalled.	*/
		transmitQueue->service( IOBasicOutputQueue::kServiceAsync );

	return;
}/* end interruptOccurred */


UInt32 UniNEnet::outputPacket( mbuf_t pkt, void *param )
{
	IODebuggerLockState	lockState = kIODebuggerLockTaken;
	UInt32				ret = kIOReturnOutputSuccess;

		/*** Caution - this method runs on the client's	***/
		/*** thread not the workloop thread.			***/

    KERNEL_DEBUG( DBG_GEM_TXQUEUE | DBG_FUNC_NONE, (int)pkt, (int)mbuf_len(), 0, 0, 0 );

	ELG( READ_REGISTER( TxFIFOPacketCounter ) << 16 | READ_REGISTER( TxCompletion ),
						mbuf_len( pkt ),
						'OutP', "outputPacket" );
///	ELG( pkt, READ_REGISTER( StatusAlias ), 'OutP', "outputPacket" );

		/* Hold debugger lock so debugger can't interrupt us:	*/
	if ( fBuiltin )	lockState = IODebuggerLock( this );

    if ( fLinkStatus != kLinkStatusUp )
    {
		ELG( pkt, fLinkStatus, 'Out-', "UniNEnet::outputPacket - link is down" );
        freePacket( pkt );
    }
    else if ( transmitPacket( pkt ) == false )
    {		/// caution: interrupt window here. If Tx interrupt
			/// occurs and packets get freed, we will now stall
			/// and nothing will unstall (except timeout?).
        ret = kIOReturnOutputStall;
    }

	if ( fBuiltin )		IODebuggerUnlock( lockState );
    return ret;
}/* end outputPacket */


void UniNEnet::putToSleep( bool sleepCellClockOnly )
{
	IOMediumType		mediumType	= kIOMediumEthernetNone;
	IONetworkMedium		*medium;
	IODebuggerLockState	lockState = kIODebuggerLockTaken;


	ELG( fCellClockEnabled, sleepCellClockOnly, 'Slep', "UniNEnet::putToSleep" );

    if ( fCellClockEnabled == false )
    {		/* See if the everything is shutting down,	*/
			/* or just disabling the clock:				*/
        if ( sleepCellClockOnly )
			return;	// just disable the clock - it's already disabled - so just return

			/* Shutting down the whole thing. The ethernet cell's clock,	*/
			/* is off so we must enable it before continuing				*/
		enableCellClock();
    }

	if ( fBuiltin )	lockState = IODebuggerLock( this );

	fReady = false;

	if ( timerSource )
    {	fTimerRunning = false;
        timerSource->cancelTimeout();
    }

	WRITE_REGISTER( InterruptMask, kInterruptMask_None );

	fWorkLoop->disableAllInterrupts();

	if ( sleepCellClockOnly == false )
	{
		nub->saveDeviceState();
		stopChip();					// stop the DMA engines.
		stopPHY();					// Set up for wake on Magic Packet if wanted.
		medium = IONetworkMedium::getMediumWithType( fMediumDict, mediumType );
		setLinkStatus( kIONetworkLinkValid );	// Link status is valid/not active. Was "unknown" until Radar 3872249.
		ELG( medium, 0, 'sls-', "UniNEnet::wakeUp -  setLinkStatus valid/not active." );
	}

	flushRings( true, false );		// Flush all mbufs from TX ring.

	if ( fBuiltin && (!fWOL || sleepCellClockOnly) )
		disableCellClock();

    if ( sleepCellClockOnly )
    {	fTimerRunning = true;
        timerSource->setTimeoutMS( WATCHDOG_TIMER_MS );
    }

	if ( fBuiltin )		IODebuggerUnlock( lockState );
    return;
}/* end putToSleep */


bool UniNEnet::wakeUp( bool wakeCellClockOnly )
{
	IOMediumType		mediumType	= kIOMediumEthernetNone;
	IONetworkMedium		*medium;
	IODebuggerLockState	lockState = kIODebuggerLockTaken;
	UInt16				status, control;	/* PHY registers	*/
	UInt32				gemReg;


	ELG( this, wakeCellClockOnly, 'Wake', "UniNEnet::wakeUp" );

	if ( fBuiltin )	lockState = IODebuggerLock( this );

    fReady = false;

	medium = IONetworkMedium::getMediumWithType( fMediumDict, mediumType );
	setLinkStatus( 0 ); // Link status is unknown - not kIONetworkLinkValid.
	ELG( medium, 0, 'slsU', "UniNEnet::wakeUp -  setLinkStatus unknown." );

	if ( fBuiltin )
		enableCellClock();

	if ( !wakeCellClockOnly )
	{
		nub->restoreDeviceState();
	/// nub->configWrite32( 0x04, 0x16 );	// BUS MASTER, MEM I/O Space, MEM WR & INV --> Config space
	}

	if ( !initRxRing() || !initTxRing() ) 
		goto wakeUp_exit;

	WRITE_REGISTER( SoftwareReset, kSoftwareReset_TX | kSoftwareReset_RX );
    do
    {		/// ??? put a time limit here.
		gemReg = READ_REGISTER( SoftwareReset );
    } 
	while( gemReg & (kSoftwareReset_TX | kSoftwareReset_RX) );

	initChip();					// set up the important registers in the cell

	if ( !wakeCellClockOnly )
	{
		if ( fBuiltin )
		{
				/* Assert reset pin on the PHY momentarily to bring	*/
				/* older PHYs out of low-power mode:				*/
			keyLargo->callPlatformFunction( keyLargo_resetUniNEthernetPhy, false, 0, 0, 0, 0 );
			ELG( keyLargo, phyId, 'hwRP', "UniNEnet::wakeUp - hardware reset the PHY." );

			phyId = 0;
			if ( fK2 )	phyId = 1;		// K2 has something at 00 (0x10001000) and the PHY at 1

			miiResetPHY();				// Software reset the PHY.

				/* The first two PHY registers are required:	*/
			miiReadWord( &status,	MII_STATUS  );
			miiReadWord( &control,	MII_CONTROL );

			if ( status == 0xFFFF && control == 0xFFFF )
			{	phyId = 0xFF;
				goto wakeUp_exit;
			}
			getPhyType();					// getPhyType also patches PHYs.
		}/* end IF builtin */

		if ( !fMediumDict && createMediumTables() == false )
		{
			ALRT( 0, 0, 'cmt-', "UniNEnet::wakeup - createMediumTables failed" );    
			goto wakeUp_exit;
		}

		startPHY();	// sets enables in cell config regs - not PHY related.		/// ??? move actual code here.
	
		fRxMACStatus	= 0;
		fReady			= true;			// set fReady before selectMedium for MLS startChip

		medium = IONetworkMedium::getMediumWithType( fMediumDict, fMediumType );
		selectMedium( medium );			// Restore any manual speed/duplex after sleep

		fTimerRunning = true;
		timerSource->setTimeoutMS( WATCHDOG_TIMER_MS );
	}/* end IF full wake up	*/
	else
	{
		fRxMACStatus	= 0;
		fReady			= true;
	}

	fWorkLoop->enableAllInterrupts();
	
wakeUp_exit:

	if ( fBuiltin )		IODebuggerUnlock( lockState );
    return true;
}/* end wakeUp */


	/*-------------------------------------------------------------------------
	 * Called by IOEthernetInterface client to enable the controller.
	 * This method is always called while running on the default workloop thread.
	 *-------------------------------------------------------------------------*/

IOReturn UniNEnet::enable( IONetworkInterface* netif )
{
	ELG( this, netif, 'NetE', "UniNEnet::enable( netInterface* )" );

		/* If an interface client has previously enabled us,	*/
		/* and we know there can only be one interface client	*/
		/* for this driver, then simply return true.			*/

    if ( netifEnabled )
    {
        IOLog( "EtherNet(UniN): already enabled\n" );
        return kIOReturnSuccess;
    }

    if ( (fReady == false) && !wakeUp( false ) )
        return kIOReturnIOError;

    netifEnabled = true;	/* Mark the controller as enabled by the interface.	*/

		/* Start our IOOutputQueue object:	*/

    transmitQueue->setCapacity( fTxQueueSize );
    transmitQueue->start();

    return kIOReturnSuccess;
}/* end enable netif */


	/*-------------------------------------------------------------------------
	 * Called by IOEthernetInterface client to disable the controller.
	 * This method is always called while running on the default workloop
	 * thread.
	 *-------------------------------------------------------------------------*/
 
IOReturn UniNEnet::disable( IONetworkInterface* /*netif*/ )
{
#if USE_ELG
///	if ( (fpELG->evLogFlag == 0) || (fpELG->evLogFlag == 0xFEEDBEEF) )
///	fpELG->evLogFlag = 0xDEBEEFED;
#endif // USE_ELG

	ELG( this, debugEnabled, 'NetD', "UniNEnet::disable( IONetworkInterface* )" );

		/* Disable our IOOutputQueue object. This will prevent the
		 * outputPacket() method from being called.
		 */
    transmitQueue->stop();

    transmitQueue->setCapacity( 0 );
    transmitQueue->flush();	/* Flush all packets currently in the output queue.	*/

    	/* If we have no active clients, then disable the controller.	*/

	if ( debugEnabled == false )
		putToSleep( false );

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

IOReturn UniNEnet::enable( IOKernelDebugger* /*debugger*/ )
{
	ELG( this, fReady, 'DbgE', "UniNEnet::enable( IOKernelDebugger* )" );

    	/* Enable hardware and make it ready to support the debugger client:	*/

    if ( (fReady == false) && !wakeUp( false ) )
        return kIOReturnIOError;

    debugEnabled = true;    /* Mark the controller as enabled by the debugger.	*/

		/* Returning true will allow the kdp registration to continue.
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

IOReturn UniNEnet::disable( IOKernelDebugger* /*debugger*/ )
{
	ELG( this, netifEnabled, 'DbgD', "UniNEnet::disable( IOKernelDebugger* )" );
    debugEnabled = false;

    /*
     * If we have no active clients, then disable the controller.
     */
	if ( netifEnabled == false )
		putToSleep( false );

    return kIOReturnSuccess;
}/* end disable debugger */


IOReturn UniNEnet::getMaxPacketSize( UInt32 *maxSize ) const
{
	*maxSize = kIOEthernetMaxPacketSize + 4;	// bump max for VLAN
///	ELG( 0, kIOEthernetMaxPacketSize + 4, 'gMPz', "UniNEnet::getMaxPacketSize" );
    return kIOReturnSuccess;
}/* end getMaxPacketSize */


IOReturn UniNEnet::setMaxPacketSize( UInt32 maxSize )
{
	if ( maxSize > kIOEthernetMaxPacketSize + 4 )	// sanity check. The family might do this too
		return kIOReturnBadArgument;

	fMaxFrameSize = maxSize;
	return kIOReturnSuccess;
}/* end setMaxPacketSize */


UInt32 UniNEnet::getFeatures() const
{
	return kIONetworkFeatureSoftwareVlan;
}/* end getFeatures */


IOReturn UniNEnet::getPacketFilters( const OSSymbol	*group, UInt32 *filters ) const
{
//	ELG( 0, 0, 'G PF', "UniNEnet::getPacketFilters" );	// can't cuz const issue

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


void UniNEnet::timeoutOccurred( IOTimerEventSource* /*timer*/ )
{
    IODebuggerLockState	lockState;
	bool  				doService = false;
	UInt32				x;


		/*** Caution - this method runs on the workloop thread while		***/
		/*** the outputPacket method usually runs on the client's thread.	***/

		/*** Caution - this method runs at a higher priority than			***/
		/*** interruptOccurred. Don't waste time in here since it impacts	***/
		/*** not just our interrupts but those of other drivers too.		***/

	ELG( txCommandHead << 16 | txCommandTail, fCellClockEnabled, 'Time', "UniNEnet::timeoutOccurred" );

	if ( !fTimerRunning || fLoopback )	/* cancelTimeout() can let one more event to occur - check fReady	*/
		return;

		/* If the ethernet cell clock is disabled, monitorLinkStatus	*/
		/* is called, and the rest of this function is skipped.			*/
    if ( fCellClockEnabled == false )
    {
        monitorLinkStatus( false );
        timerSource->setTimeoutMS( WATCHDOG_TIMER_MS );
        return; 
    }    

	fRxMACStatus |= READ_REGISTER( RxMACStatus );	// both are auto-clear registers
	ELG( READ_REGISTER( TxMACStatus ), fRxMACStatus, 'MACS', "UniNEnet::timeoutOccurred - Tx and Rx MAC Status regs" );

		/* Update statistics from the GMAC statistics registers:	*/

	x = READ_REGISTER( LengthErrorCounter );
	if ( x )
	{	WRITE_REGISTER( LengthErrorCounter, 0 );
		fpEtherStats->dot3StatsEntry.frameTooLongs += x;
	}

	x = READ_REGISTER( AlignmentErrorCounter );
	if ( x )
	{	WRITE_REGISTER( AlignmentErrorCounter, 0 );
		fpEtherStats->dot3StatsEntry.alignmentErrors += x;
	}

	x = READ_REGISTER( FCSErrorCounter );
	if ( x )
	{	WRITE_REGISTER( FCSErrorCounter, 0 );
		fpEtherStats->dot3StatsEntry.fcsErrors += x;
	}

	x = READ_REGISTER( RxCodeViolationErrorCounter );
	if ( x )
	{	WRITE_REGISTER( RxCodeViolationErrorCounter, 0 );
		fpEtherStats->dot3StatsEntry.internalMacTransmitErrors += x;
	}

	x = READ_REGISTER( NormalCollisionCounter );
	if ( x )
	{	WRITE_REGISTER( NormalCollisionCounter, 0 );
		fpNetStats->collisions += x;
	}

	x = READ_REGISTER( FirstAttemptSuccessfulCollisionCounter );
	if ( x )
	{	WRITE_REGISTER( FirstAttemptSuccessfulCollisionCounter, 0 );
		fpEtherStats->dot3StatsEntry.singleCollisionFrames += x;
	}

	x = READ_REGISTER( ExcessiveCollisionCounter );
	if ( x )
	{	WRITE_REGISTER( ExcessiveCollisionCounter, 0 );
		fpEtherStats->dot3StatsEntry.excessiveCollisions += x;
	}

	x = READ_REGISTER( LateCollisionCounter );
	if ( x )
	{	WRITE_REGISTER( LateCollisionCounter, 0 );
		fpEtherStats->dot3StatsEntry.lateCollisions += x;
	}

	lockState = IODebuggerLock( this );

	fIntStatusForTO |= READ_REGISTER( StatusAlias );
	if ( (fIntStatusForTO & (kStatus_TX_INT_ME | kStatus_RX_DONE)) == 0 )
		monitorLinkStatus( false );	// Don't do this if neither Tx nor Rx are moving

		// if the link went down (fLinkStatus is updated in monitorLinkStatus),
		// disable the ethernet clock and exit this function.
	if ( fLinkStatus == kLinkStatusDown )
	{
		putToSleep( true );
		timerSource->setTimeoutMS( WATCHDOG_TIMER_MS );
		IODebuggerUnlock( lockState );
		fIntStatusForTO = 0;
		return;
	} 

		/* See if the transmitter is hung:	*/

	fTxCompletion = READ_REGISTER( TxCompletion );

	if ( (fIntStatusForTO & kStatus_TX_DONE) || (txCommandHead == txCommandTail) )
	{							/* If Tx interrupt occurred or nothing to Tx,	*/
		txWDCount = 0;			/* reset the watchdog count.					*/
	}
	else
	{		/* If the hardware Tx pointer did not move since	*/
			/* the last check, increment the txWDCount.			*/
		if ( fTxCompletion == fTxRingIndexLast )
		{							/* Transmitter may be stuck.	*/
			txWDCount++;			/* bump the watchdog count.		*/
		}
		else
        {							/* Transmitter moved; it's ok.	*/
            txWDCount = 0;			/* reset the watchdog count.	*/
        }

		if ( txWDCount >= 8 )			/* The transmitter hasn't moved in a while:	*/
		{
			transmitInterruptOccurred();/* Fake Tx int; can affect txCommandHead.	*/
			doService = true;			/* make sure network stack not blocked.		*/

				/* We take interrupts every 32 or so Tx descriptor completions, so	*/
				/* we may be here just to do clean-up of Tx packets. We check		*/
				/* if the hardware Tx pointer points to the next available Tx slot.	*/
				/* This indicates that we transmitted all packets that were			*/
				/* scheduled vs rather than the hardware Tx being stalled.			*/

			if ( txCommandHead == txCommandTail )
				txWDCount = 0;			/* All is now ok - reset watchdog count.	*/
			else
            {
				UInt32		intStatus, compReg, kickReg, macControlStatus;
 
					/* This may be bad - the transmitter has work to do but		*/
					/* is not progressing. We may be getting flow controlled.	*/
 
				intStatus			= READ_REGISTER( StatusAlias );	// don't use auto-clear reg
				compReg				= READ_REGISTER( TxCompletion );
				kickReg				= READ_REGISTER( TxKick );
				macControlStatus	= READ_REGISTER( MACControlStatus );// Pause interrupts

				if ( compReg != kickReg )
				{
					ELG( txWDCount, kickReg << 16 | compReg, 'Tx--', "UniNEnet::timeoutOccurred - Tx Int Timeout" );
	
					restartTransmitter();
					txWDCount = 0;
				}
            }
        }/* end IF txWDCount > 8 periods	*/
    }/* end ELSE no transmissions completed this period	*/

	fTxRingIndexLast = fTxCompletion;

		/* Check for Rx deafness:	*/

    if ( fIntStatusForTO & kStatus_RX_DONE )
	{
		rxWDCount = 0;			// Reset watchdog timer count
	}
	else if ( (rxWDCount++ > (33 * 1000 / WATCHDOG_TIMER_MS))	// give 33 secs max,
		  || ((rxWDCount   > 2)									// or 600-900 ms
			 && (fRxMACStatus & kRX_MAC_Status_Rx_Overflow)) )	// with FIFO overflow
    {
			/* There are 3  possibilities here:									*/
			/* 1) There really was no traffic on the link.						*/
			/* 2) The receiver is deaf, necessitating a receiver reset.			*/
			/* 3) Sometimes some errant kernel code (names withheld to protect	*/
			/*    the guilty) can take excessive time at a priority above our	*/
			/*    workloop interrupt code but below our workloop timer code		*/
			/*    misleading the timer code into believing there has been		*/
			/*    no link activity. This timer code needs to be revamped		*/
			/*    to better detect link activity. NB that if this 3rd condition	*/
			/*    occurs, it is quite likely that the Rx ring and Rx FIFO		*/
			/*    have overflowed.												*/

			/* Radar 5477907 "networking disappeared while running network		*/
			/* file system test" - Rx was deaf without FIFO overflow requiring	*/
			/* restart after some time period such as 33 seconds.				*/

		restartReceiver();

		NETWORK_STAT_ADD( inputErrors );
		ETHERNET_STAT_ADD( dot3RxExtraEntry.watchdogTimeouts );
		fRxMACStatus	= 0;		// reset FIFO overflow indicator
		rxWDCount		= 0;		// reset the watchdog count.
    }/* end IF no Rx activity in a while */

	fIntStatusForTO = 0;				// reset the Tx and Rx accumulated int bits.

		/* Clean-up after the debugger if the debugger was active:	*/

	if ( debugTxPoll )
	{
		debugQueue->flush();	///??? fix this
		debugTxPoll	= false;
		doService	= true;
	}
	IODebuggerUnlock( lockState );

		/* Make sure the queue is not stalled.	*/

	if ( doService && netifEnabled )
		transmitQueue->service( IOBasicOutputQueue::kServiceAsync );

    timerSource->setTimeoutMS( WATCHDOG_TIMER_MS );	/* Restart watchdog timer	*/
	return;
}/* end timeoutOccurred */


const OSString* UniNEnet::newVendorString() const
{
    return OSString::withCString( "Apple" );
}/* end newVendorString */


const OSString* UniNEnet::newModelString() const
{
    return OSString::withCString( "gmac+" );
}/* end newModelString */



const OSString* UniNEnet::newRevisionString() const
{
    return OSString::withCString( "" );
}/* end newRevisionString */


IOReturn UniNEnet::setPromiscuousMode( bool active )
{
	IODebuggerLockState		lockState = kIODebuggerLockTaken;


	ELG( 0, active, 'SetP', "UniNEnet::setPromiscuousMode" );

	if ( fBuiltin )	lockState = IODebuggerLock( this );

	fIsPromiscuous = active;

	if ( fCellClockEnabled )
	{
			if ( active )
		{
			fRxMACConfiguration |=  kRxMACConfiguration_Promiscuous;
			fRxMACConfiguration &= ~kRxMACConfiguration_Strip_FCS;
		}
		else
		{
			fRxMACConfiguration &= ~kRxMACConfiguration_Promiscuous;
			fRxMACConfiguration |=  kRxMACConfiguration_Strip_FCS;
		}
	
		WRITE_REGISTER( RxMACConfiguration, fRxMACConfiguration );
	}

	if ( fBuiltin )		IODebuggerUnlock( lockState );
	return kIOReturnSuccess;
}/* end setPromiscuousMode */


IOReturn UniNEnet::setMulticastMode( bool active )
{
	ELG( this, active, 'SetM', "UniNEnet::setMulticastMode" );
	multicastEnabled = active;

	return kIOReturnSuccess;
}/* end setMulticastMode */


IOReturn UniNEnet::setMulticastList( IOEthernetAddress *addrs, UInt32 count )
{
	IODebuggerLockState		lockState = kIODebuggerLockTaken;


	ELG( addrs, count, 'SetL', "UniNEnet::setMulticastList" );

	if ( fCellClockEnabled == false )
	///	enableCellClock();				// Leave the cell clocked when done.
			// setting fCellClockEnabled causes wakeUp to not be called in monitorLinkStatus 
		return kIOReturnSuccess;

	if ( fBuiltin )	lockState = IODebuggerLock( this );

    resetHashTableMask();							// bzero the tables
    for ( UInt32 i = 0; i < count; i++ ) 
    {
        addToHashTableMask( addrs->bytes );			// add a MAC address
        addrs++;									// point to next address
    }
    updateHashTableMask();							// fill in 16 HashTable registers 

	if ( fBuiltin )		IODebuggerUnlock( lockState );
    return kIOReturnSuccess;
}/* end setMulticastList */


IOOutputQueue* UniNEnet::createOutputQueue()
{
	return IOBasicOutputQueue::withTarget( this, TRANSMIT_QUEUE_SIZE );
}/* end createOutputQueue */


bool UniNEnet::createMediumTables()
{
	IONetworkMedium		*medium;
	UInt32				i;


	fMediumDict = OSDictionary::withCapacity( fMediumTableCount );
	ELG( 0, fMediumDict, 'MTbl', "UniNEnet::createMediumTables" );
	if ( fMediumDict == 0 )
		return false;

	for ( i = 0; i < fMediumTableCount; i++ )
	{
		medium = IONetworkMedium::medium( fpgMediumTable[i].type, fpgMediumTable[i].speed );
		IONetworkMedium::addMedium( fMediumDict, medium );
		medium->release();
	}/* end FOR */

	if ( publishMediumDictionary( fMediumDict ) != true )
		return false;

	medium = IONetworkMedium::getMediumWithType( fMediumDict, kIOMediumEthernetAuto );

    setCurrentMedium( medium );

    return true;
}/* end createMediumTables */


IOReturn UniNEnet::selectMedium( const IONetworkMedium *medium )
{
	IOMediumType	mType = medium->getType();
	UInt16			controlReg;						// 00 - PHY control register
	IOReturn		ior;
	bool			gotReg;


			/* If the user sets a speed/duplex unsupported by the hub/switch,		*/
			/* link will not be established and the cell clock will be disabled.	*/
			/* Wake it up so the setting can be fixed:								*/
    if ( fCellClockEnabled == false )
		wakeUp( true );

	gotReg = miiReadWord( &controlReg, MII_CONTROL );
	ELG( controlReg, mType, 'sMed', "UniNEnet::selectMedium" );

	if ( !gotReg || controlReg == 0xFFFF )
	{
		ALRT( fPHYType, controlReg, 'Pnr-', "UniNEnet::selectMedium - PHY not responding" );
		return kIOReturnIOError;
	}

	if ( (mType & kIOMediumNetworkTypeMask) != kIOMediumEthernet )
	{
		ALRT( fPHYType, controlReg, 'sMe-', "UniNEnet::selectMedium - not ethernet medium" );
		return kIOReturnBadArgument;
	}

	fMediumType = mType;

		/* Kill network if Loopback is changing:	*/

	if ( (mType & kIOMediumOptionLoopback) ^ (controlReg & MII_CONTROL_LOOPBACK) )
	{
		setLinkStatus( 0 );	/* Link status is unknown - not kIONetworkLinkValid	*/
		ELG( medium, mType, 'SLSu', "UniNEnet::selectMedium -  setLinkStatus." );
		fPHYStatus	= 0;
		fLinkStatus	= kLinkStatusUnknown;
		ELG( 0, mType & kIOMediumOptionLoopback, 'LpBk', "UniNEnet::selectMedium - changing loopback mode." );
	}

	if ( mType & kIOMediumOptionLoopback )
	{	fLoopback = true;
		ior = kIOReturnIOError;		/* bypass negotiateSpeedDuplex and fake an error	*/
	}
	else
	{	fLoopback = false;
		ior = negotiateSpeedDuplex();		
	}

	if ( ior != kIOReturnSuccess && mType != kIOMediumEthernetAuto )
	{
			/* Negotiation failed or loopback - just force the user's desires on the PHY:	*/
		ior = forceSpeedDuplex();
	///	if ( ior != kIOReturnSuccess )	// Radar 3610003 - ignore error if force fails
	///		return ior;					// user may move cable
		fAutoNegotiate = false;
	}

	setSelectedMedium( medium );
	ELG( fMediumType, fXIFConfiguration, 'sMe+', "UniNEnet::selectMedium - returning kIOReturnSuccess" );

	monitorLinkStatus( true );				/* force Link change notification	*/

	return kIOReturnSuccess;
}/* end selectMedium */


IOReturn UniNEnet::negotiateSpeedDuplex()
{
	UInt16			controlReg;			// 00 - control register
	UInt16			statusReg;			// 01 - status register
	UInt16			anar;				// 04 - AutoNegotiation Advertisement Register
	UInt16			gigReg;				// Vendor specific register
	IOMediumType	mType, mtyp;		// mtyp is mType without loopback
	bool			br;					// boolean return value


	mType	=  fMediumType & (kIOMediumNetworkTypeMask | kIOMediumSubTypeMask | kIOMediumCommonOptionsMask);
	mtyp	= mType & ~(kIOMediumOptionLoopback);

	controlReg = MII_CONTROL_AUTONEGOTIATION | MII_CONTROL_RESTART_NEGOTIATION;

	br = miiReadWord( &anar, MII_ADVERTISEMENT );

	ELG( anar, mType, 'n SD', "UniNEnet::negotiateSpeedDuplex" );

	anar &= ~(	MII_ANAR_ASYM_PAUSE
			  | MII_ANAR_PAUSE
			  | MII_ANAR_100BASET4			/* turn off all speed/duplex bits	*/
			  | MII_ANAR_100BASETX_FD		/* This register has only  10/100	*/
			  | MII_ANAR_100BASETX			/* Full/Half bits - no gigabit		*/
			  | MII_ANAR_10BASET_FD
			  | MII_ANAR_10BASET );

		/* Set the Speed/Duplex bit that we need:	*/

	switch ( mtyp )
	{
	case kIOMediumEthernetAuto:
		anar |=	(	MII_ANAR_100BASETX_FD	/* turn on all speed/duplex bits	*/
				  | MII_ANAR_100BASETX
				  | MII_ANAR_10BASET_FD
				  | MII_ANAR_10BASET
				  | MII_ANAR_PAUSE );
		break;

	case kIOMediumEthernet10BaseT | kIOMediumOptionHalfDuplex:	// 10/Half
		anar |= MII_ANAR_10BASET;
		break;

	case kIOMediumEthernet10BaseT | kIOMediumOptionFullDuplex:	// 10/Full
		anar |= MII_ANAR_10BASET_FD;
		break;
																// 10/Full/Flow Control:
	case kIOMediumEthernet10BaseT | kIOMediumOptionFullDuplex | kIOMediumOptionFlowControl:
		anar |= MII_ANAR_10BASET_FD | MII_ANAR_PAUSE;
		break;

	case kIOMediumEthernet100BaseTX | kIOMediumOptionHalfDuplex:// 100/Half
		anar |= MII_ANAR_100BASETX;
		break;

	case kIOMediumEthernet100BaseTX | kIOMediumOptionFullDuplex:// 100/Full
		anar |= MII_ANAR_100BASETX_FD;
		break;
																// 100/Full/Flow Control:
	case kIOMediumEthernet100BaseTX | kIOMediumOptionFullDuplex | kIOMediumOptionFlowControl:
		anar |= MII_ANAR_100BASETX_FD | MII_ANAR_PAUSE;
		break;
																// gig/Full/Flow Control:
	case kIOMediumEthernet1000BaseT | kIOMediumOptionFullDuplex | kIOMediumOptionFlowControl:
		anar |= MII_ANAR_PAUSE;
		break;	//	gigabit is vendor specific - do it there

	case kIOMediumEthernet1000BaseT | kIOMediumOptionFullDuplex:// gig/Full
		break;	//	gigabit is vendor specific - do it there

	default:		/* unknown - maybe NONE	*/
		ELG( 0, 0, ' ?sd', "UniNEnet::negotiateSpeedDuplex - unknown combo." );
		break;
	}/* end SWITCH on speed/duplex */

	miiWriteWord( anar, MII_ADVERTISEMENT );


		/* Do vendor specific stuff:	*/

	switch ( fPHYType )
	{
					/* Non gigabit PHYs:	*/
	case 0x0971:									// Level One LXT971:
	case 0x5201:									// Broadcom 52x1:
	case 0x5221:
	case 0x5241:
		break;
					/* Gigabit PHYs:	*/

	case 0x1011:									// Marvell:
			/* Enable Automatic Crossover:	*/
		br = miiReadWord( &gigReg, MII_MARVELL_PHY_SPECIFIC_CONTROL );
		gigReg |= MII_MARVELL_PHY_SPECIFIC_CONTROL_AUTOL_MDIX;
		miiWriteWord( gigReg, MII_MARVELL_PHY_SPECIFIC_CONTROL );
		controlReg |= MII_CONTROL_RESET;

		// fall through to generic gigabit code.
	case 0x5400:									// Broadcom 54xx:
	case 0x5401:
	case 0x5411:
	case 0x5421:
		br = miiReadWord( &gigReg, MII_1000BASETCONTROL );
			// Turn off gig/Half and gig/Full bits:
		gigReg &= ~(MII_1000BASETCONTROL_FULLDUPLEXCAP | MII_1000BASETCONTROL_HALFDUPLEXCAP);

			/* Optionally turn on gig/Full (we don't allow gig/half):	*/

		switch ( mtyp )
		{
		case kIOMediumEthernetAuto:
		case kIOMediumEthernet1000BaseT | kIOMediumOptionFullDuplex:
		case kIOMediumEthernet1000BaseT | kIOMediumOptionFullDuplex | kIOMediumOptionFlowControl:
			gigReg |= MII_1000BASETCONTROL_FULLDUPLEXCAP;
			break;
		}/* end SWITCH on Marvell gig/Full or gig/Half */

		miiWriteWord( gigReg, MII_1000BASETCONTROL );

		break;

	case ' GEM':	// GEM card is fiber optic and nonnegotiable
	default:
		break;
	}/* end SWITCH on PHY type */

	miiWriteWord( controlReg, MII_CONTROL );

	br = miiWaitForAutoNegotiation();
	if ( br == false )
		return kIOReturnIOError;

	miiReadWord( &statusReg, MII_STATUS );
	if ( !(statusReg & MII_STATUS_LINK_STATUS) )
		return kIOReturnIOError;

	return kIOReturnSuccess;
}/* end negotiateSpeedDuplex */


IOReturn UniNEnet::forceSpeedDuplex()
{
	IOMediumType	mType;
	UInt16			controlReg	= 0;	// 00 - control register
	UInt16			statusReg;			// 01 - status register
	UInt16			gigReg;				// Vendor specific register
	bool			br;


	mType = fMediumType & (kIOMediumNetworkTypeMask | kIOMediumSubTypeMask | kIOMediumCommonOptionsMask);

	ELG( 0, mType, 'f SD', "UniNEnet::forceSpeedDuplex" );

	switch ( mType & (kIOMediumNetworkTypeMask | kIOMediumSubTypeMask) )
	{
	case kIOMediumEthernetAuto:
		return kIOReturnIOError;	// negotiation already attempted. Don't force it.

	case kIOMediumEthernet10BaseT:
	//	controlReg &= ~MII_CONTROL_SPEED_SELECTION_2;
		break;

	case kIOMediumEthernet100BaseTX:
		controlReg = MII_CONTROL_SPEED_SELECTION;
		break;

	case kIOMediumEthernet1000BaseT:
		controlReg = MII_CONTROL_SPEED_SELECTION_2;
		break;
	}/* end SWITCH */


	if ( mType & kIOMediumOptionFullDuplex )	controlReg |= MII_CONTROL_FULLDUPLEX;
	if ( mType & kIOMediumOptionLoopback )		controlReg |= MII_CONTROL_LOOPBACK;

	switch ( fPHYType )
	{
	case 0x1011:										// Marvell:
			/* Disable auto crossover:	*/
		br = miiReadWord( &gigReg, MII_MARVELL_PHY_SPECIFIC_CONTROL );
		gigReg &= ~(MII_MARVELL_PHY_SPECIFIC_CONTROL_AUTOL_MDIX
				  | MII_MARVELL_PHY_SPECIFIC_CONTROL_MANUAL_MDIX);
		miiWriteWord( gigReg, MII_MARVELL_PHY_SPECIFIC_CONTROL );
		controlReg |= MII_CONTROL_RESET;
		break;

	case 0x0971:										// Level One LXT971:
	case 0x5201:										// Broadcom PHYs:
	case 0x5221:
	case 0x5241:
	case 0x5400:
	case 0x5401:
	case 0x5411:
	case 0x5421:
	case ' GEM':
	default:		// first, reset the PHY:
		miiWriteWord( MII_CONTROL_RESET, MII_CONTROL );
		IOSleep( 3 );
		break;
	}/* end SWITCH on PHY type */

		// monitorLinkStatus will touch up the MAC wrt Pause flow control.

	miiWriteWord( controlReg, MII_CONTROL );

	if ( controlReg & MII_CONTROL_SPEED_SELECTION_2 )
		 fXIFConfiguration |= kXIFConfiguration_GMIIMODE;	// set MAC to GIG:
	else fXIFConfiguration &= kXIFConfiguration_GMIIMODE;	// set MAC to nonGIG:
	WRITE_REGISTER( XIFConfiguration, fXIFConfiguration );	// So order.

	if ( fLoopback )
	{
		IOSleep( 5 );				/* PHY status won't show link UP	*/
		return kIOReturnSuccess;	/* So, just pretend it is.			*/
	}

	for ( int i = 0; i < 5000; i += 10 )	/* 5 seconds in 10 ms chunks	*/
	{
		miiReadWord( &statusReg, MII_STATUS );
		if ( statusReg & MII_STATUS_LINK_STATUS )
			return kIOReturnSuccess;		// Link is UP, return
		IOSleep( 10 );
	}/* end FOR */

	return kIOReturnIOError;
}/* end forceSpeedDuplex */


IOReturn UniNEnet::getChecksumSupport(	UInt32	*checksumMask,
										UInt32	checksumFamily,
										bool	/*isOutput*/ )
{
	if ( checksumFamily != kChecksumFamilyTCPIP )
		return kIOReturnUnsupported;

	*checksumMask	= kChecksumTCPSum16;

	return kIOReturnSuccess;
}/* end getChecksumSupport */


void UniNEnet::writeRegister( volatile UInt32 *pReg, UInt32 data )
{
    if ( fCellClockEnabled == false )
    {
        ALRT( data, pReg, 'Wrg-', "UniNEnet::writeRegister: enet clock is disabled" );
        return;
    }

	if ( pReg != &fpRegs->MIFBitBangFrame_Output )
		ELG( data, (UInt32)pReg - (UInt32)fpRegs, 'wReg', "UniNEnet::writeRegister" );

	OSWriteLittleInt32( pReg, 0, data );
	return;
}/* end writeRegister */


void UniNEnet::enableCellClock()
{
	ELG( fCellClockEnabled, 0, '+Clk', "UniNEnet::enableCellClock" );
	callPlatformFunction( "EnableUniNEthernetClock", true, (void*)true, (void*)nub, 0, 0 );
	OSSynchronizeIO();
	IODelay( 3 );			// Allow the cell some cycles before using it.
	fCellClockEnabled = true;
	return;
}/* end enableCellClock */


void UniNEnet::disableCellClock()
{
	fCellClockEnabled = false;
	ELG( 0, 0, '-Clk', "UniNEnet::disableCellClock - disabling cell clock!!!" );
	OSSynchronizeIO();
	callPlatformFunction( "EnableUniNEthernetClock", true, (void*)false, (void*)nub, 0, 0 );
	OSSynchronizeIO();
//	ELG( 0, 0, '-clk', "UniNEnet::putToSleep - disabled ethernet cell clock." );
	return;
}/* end disableCellClock */


IOReturn UniNEnet::newUserClient(	task_t			owningTask,
									void*,						// Security id (?!)
									UInt32			type,		// Lucky number
									IOUserClient	**handler )	// returned handler
{
	UniNEnetUserClient	*client	= NULL;
    bool				privileged;
	IOReturn			ior		= kIOReturnSuccess;


	ELG( type, type, 'Usr+', "UniNEnet::newUserClient" );
    
    privileged = IOUserClient::clientHasPrivilege( current_task(), kIOClientPrivilegeAdministrator ) == kIOReturnSuccess;
	if ( !privileged )
	{
		ELG( 0, 0, 'Prv-', "UniNEnet::newUserClient - task is not privileged." );
		return kIOReturnNotPrivileged;
	}
		// Check that this is a user client type that we support.
		// type is known only to this driver's user and kernel
		// classes. It could be used, for example, to define
		// read or write privileges. In this case, we look for
		// a private value.
	if ( type != 'GMAC' )
	{		/// ??? don't return error - call superclass and return its code.
		ELG( 0, type, 'Usr-', "UniNEnet::newUserClient - unlucky." );
		return kIOReturnError;
	}

		// Instantiate a new client for the requesting task:

	client = UniNEnetUserClient::withTask( owningTask );
	if ( !client )
	{
		ELG( 0, 0, 'usr-', "UniNEnet::newUserClient: Can't create user client" );
		return kIOReturnError;
	}

	if ( ior == kIOReturnSuccess )
	{		// Attach ourself to the client so that this client instance can call us.
		if ( client->attach( this ) == false )
		{
			ior = kIOReturnError;
			ELG( 0, 0, 'USR-', "UniNEnet::newUserClient: Can't attach user client" );
		}
	}

	if ( ior == kIOReturnSuccess )
	{		// Start the client so it can accept requests.
		if ( client->start( this ) == false )
		{
			ior = kIOReturnError;
			ELG( 0, 0, 'USR-', "UniNEnet::newUserClient: Can't start user client" );
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

	OSDefineMetaClassAndStructors( UniNEnetUserClient, IOUserClient )	;


UniNEnetUserClient* UniNEnetUserClient::withTask( task_t owningTask )
{
	UniNEnetUserClient*		me = new UniNEnetUserClient;


//	ELG( 0, me, 'UC++', "UniNEnetUserClient::withTask" );

	if ( me && me->init() == false )
    {
        me->release();
        return 0;
    }

	me->fTask = owningTask;

    return me;
}/* end UniNEnetUserClient::withTask */


bool UniNEnetUserClient::start( IOService *provider )
{
	if ( super::start( provider ) == false )	return false;
	if ( provider->open( this )   == false )	return false;

	fProvider = (UniNEnet*)provider;

	UC_ELG( 0, provider, 'UC S', "UniNEnetUserClient::start" );

		/* Initialize the call structure:	*/

	fMethods[0].object = this;
	fMethods[0].func   = (IOMethod)&UniNEnetUserClient::doRequest;
	fMethods[0].count0 = 0xFFFFFFFF;			/* One input  as big as I need */
	fMethods[0].count1 = 0xFFFFFFFF;			/* One output as big as I need */
	fMethods[0].flags  = kIOUCStructIStructO;

	return true;
}/* end UniNEnetUserClient::start */


IOReturn UniNEnetUserClient::clientClose()
{

	if ( fProvider )
	{
		UC_ELG( 0, 0, 'UC C', "UniNEnetUserClient::clientClose" );

		if ( fmap )	{ fmap-> release(); fmap = 0; }

		if ( fProvider->isOpen( this ) )
			fProvider->close( this );

		detach( fProvider );
		fProvider = 0;
	}
	return kIOReturnSuccess;
}/* end UniNEnetUserClient::clientClose */


IOReturn UniNEnetUserClient::clientDied()
{
	if ( fProvider )
	{
		UC_ELG( 0, fProvider, 'UC D', "UniNEnetUserClient::clientDied" );
		if ( fmap )	{ fmap-> release(); fmap = 0; }
	}

	return clientClose();
}/* end UniNEnetUserClient::clientDied */


IOReturn UniNEnetUserClient::connectClient( IOUserClient *client )
{
	UC_ELG( 0, 0, 'uCon', "UniNEnetUserClient::connectClient - connect client" );
    return kIOReturnSuccess;
}/* end connectClient */


IOReturn UniNEnetUserClient::registerNotificationPort( mach_port_t port, UInt32 type )
{
	UC_ELG( 0, 0, 'uRNP', "UniNEnetUserClient - register notification ignored" );
	return kIOReturnUnsupported;
}/* end registerNotificationPort */


IOExternalMethod* UniNEnetUserClient::getExternalMethodForIndex( UInt32 index )
{
    IOExternalMethod	*result = NULL;


	if ( index == 0 )
		result = &fMethods[0];

	UC_ELG( result, index, 'uXMi', "UniNEnetUserClient::getExternalMethodForIndex - get external method" );

    return result;
}/* end getExternalMethodForIndex */


IOReturn UniNEnetUserClient::doRequest(	void		*pIn,		void		*pOut,
										IOByteCount	inputSize,	IOByteCount	*pOutPutSize )
{
	UInt32		reqID;
	IOReturn	ior;
	bool		cellClockEnabled;


	UC_ELG( inputSize,   (UInt32)pIn,  'uRqI', "UniNEnetUserClient::doRequest - input parameters" );
	UC_ELG( pOutPutSize ? *pOutPutSize : 0xDeadBeef, (UInt32)pOut, 'uRqO', "UniNEnetUserClient::doRequest - output parameters" );

		/* validate parameters:	*/

	if ( !pIn  || (inputSize < sizeof( UCRequest )) )
	{
		IOLog( "UniNEnetUserClient::doRequest - bad argument(s) - pIn = %lx, inputSize = %ld.\n",
				(UInt32)pIn, inputSize );
		return kIOReturnBadArgument;
	}

	cellClockEnabled = fProvider->fCellClockEnabled;
	if ( cellClockEnabled == false )
		fProvider->enableCellClock();

	reqID = *(UInt32*)pIn;

	switch ( reqID )					/* switch on request ID	*/
	{
	case kGMACUserCmd_GetLog:		ior = getGMACLog(		pIn, pOut, inputSize, pOutPutSize );	break;
	case kGMACUserCmd_GetRegs:		ior = getGMACRegs(		pIn, pOut, inputSize, pOutPutSize );	break;
	case kGMACUserCmd_GetOneReg:	ior = getOneGMACReg(	pIn, pOut, inputSize, pOutPutSize );	break;

	case kGMACUserCmd_GetTxRing:	ior = getGMACTxRing(	pIn, pOut, inputSize, pOutPutSize );	break;
	case kGMACUserCmd_GetRxRing:	ior = getGMACRxRing(	pIn, pOut, inputSize, pOutPutSize );	break;

	case kGMACUserCmd_WriteOneReg:	ior = writeOneGMACReg(	pIn, pOut, inputSize, pOutPutSize );	break;

	case kGMACUserCmd_ReadAllMII:	ior = readAllMII(	pIn, pOut, inputSize, pOutPutSize );	break;
	case kGMACUserCmd_ReadMII:		ior = readMII(		pIn, pOut, inputSize, pOutPutSize );	break;
	case kGMACUserCmd_WriteMII:		ior = writeMII(		pIn, pOut, inputSize, pOutPutSize );	break;

	default:
		IOLog( "UniNEnetUserClient::doRequest - Bad command, %lx\n", reqID );
		UC_ELG( 0, reqID, 'uRq?', "UniNEnetUserClient::doRequest - unknown" );
		ior = kIOReturnBadArgument;
		break;
	}

	if ( cellClockEnabled == false )
		fProvider->disableCellClock();

	return ior;
}/* end doRequest */


#if USE_ELG
	/* getGMACLog - Get UniNEnet event log.			*/
	/*												*/
	/* input is struct:								*/
	/*		command code (kGMACUserCmd_GetLog)		*/
	/*		buffer address (unused)					*/
	/*		buffer size    (unused)					*/
	/* output is struct:							*/
	/*		command code (copied)					*/
	/*		address of buffer mapped in user space	*/
	/*		buffer size								*/

IOReturn UniNEnetUserClient::getGMACLog(	void		*pIn,		void		*pOut,
											IOByteCount	inputSize,	IOByteCount	*pOutPutSize )
{
	UCRequest			*req = (UCRequest*)pIn;
	IOVirtualAddress	clientAddr;


	UC_ELG( (UInt32)pIn,  inputSize,    'UgLg', "UniNEnetUserClient::getGMACLog" );

		/* map in the buffer to kernel space:	*/

	fmap = fProvider->fpELGMemDesc->map( fTask, 0, kIOMapAnywhere );

	clientAddr = fmap->getVirtualAddress();
	req->pLogBuffer	= (UInt8*)clientAddr;
	req->bufSize	= (fProvider->fpELG->evLogBufe - fProvider->fpELG->evLogBuf + PAGE_SIZE) & ~(PAGE_SIZE - 1);

	UC_ELG( req->bufSize, req->pLogBuffer, '=uBf', "UniNEnetUserClient::getGMACLog - user buffer" );

	bcopy( req, pOut, sizeof( UCRequest ) );
	*pOutPutSize = sizeof( UCRequest );

    return kIOReturnSuccess;
}/* end getGMACLog */

#else // Production driver - no event logging buffer:
IOReturn UniNEnetUserClient::getGMACLog( void*, void*, IOByteCount, IOByteCount* )
{
	return kIOReturnBadArgument;
}/* end getGMACLog */
#endif // USE_ELG


	/* getGMACRegs - Get UniNEnet registers.	*/
	/*											*/
	/* input is bytes:							*/
	/*		command code (kGMACUserCmd_GetRegs)	*/
	/*		buffer address						*/
	/*		buffer size							*/
	/*											*/
	/* output set to Length/Type/Value records	*/

IOReturn UniNEnetUserClient::getGMACRegs(	void		*pIn,		void		*pOut,
											IOByteCount	inputSize,	IOByteCount	*pOutPutSize )
{
	LengthOffset	*pTemplate;
	UInt8			*src;
	UInt8			*dest = (UInt8*)pOut;
	UInt32			len;
	UInt32			lowRegs[ 8 ];
	UInt32			*pl;			// pointer to a Long


	if ( !pOut || (*pOutPutSize < PAGE_SIZE) )	// actual size unknown
	{
		IOLog( "UniNEnetUserClient::getGMACRegs - bad argument(s) - pOut = %lx, *pOutPutSize = %ld.\n",
				(UInt32)pOut, (UInt32)*pOutPutSize );
		return kIOReturnBadArgument;
	}

	if ( !fProvider->fCellClockEnabled )
		return kIOReturnNoPower;

	for ( pTemplate = gGMACRegisterTemplate; true; pTemplate++ )
	{
		bcopy( pTemplate, dest, sizeof( LengthOffset ) );
		dest += sizeof( LengthOffset );

		len = pTemplate->setLength;
		if ( len == 0 )
			break;

		if ( pTemplate->setOffset != 0 )
		{
			src	= (UInt8*)fProvider->fpRegs + pTemplate->setOffset;
		}
		else		/* 0x000C Status Register autoclears and must be special cased.	*/
		{	pl = (UInt32*)fProvider->fpRegs;
			lowRegs[ 0 ] = pl[ 0 ];
			lowRegs[ 1 ] = pl[ 1 ];
			lowRegs[ 2 ] = pl[ 2 ];
			lowRegs[ 3 ] = pl[ 7 ];	/*** This autoclears - read its shadow	***/
			lowRegs[ 4 ] = pl[ 4 ];
			lowRegs[ 5 ] = pl[ 5 ];
			lowRegs[ 6 ] = pl[ 6 ];
			lowRegs[ 7 ] = pl[ 7 ];
			src	= (UInt8*)lowRegs;
		}

		UInt32		i;
		UInt32* 	d32 = (UInt32*)dest;
		UInt32*		s32 = (UInt32*)src;
		for ( i = 0; i < len / 4; i++ )
		{
			*d32++ = OSReadLittleInt32( s32, 0 );
			s32++;
		}

		dest += len;
	}/* end FOR */

	*pOutPutSize = dest - (UInt8*)pOut;
	UC_ELG( 0, *pOutPutSize, 'gReg', "UniNEnetUserClient::getGMACRegs - done" );
    return kIOReturnSuccess;
}/* end getGMACRegs */


IOReturn UniNEnetUserClient::getOneGMACReg(	void		*pIn,		void		*pOut,
											IOByteCount	inputSize,	IOByteCount	*outPutSize )
{
	UCRequest	*req = (UCRequest*)pIn;
	UInt32		*reg_value = (UInt32*)pOut;
	UInt32 		reg;


	*outPutSize	= 0;							// init returned byte count
	reg			= (UInt32)req->pLogBuffer;		// use the input struct's bufSize for the reg #
	if ( (reg > 0x905C) || (reg & 3) )
		return kIOReturnError;

	if ( !fProvider->fCellClockEnabled )
		return kIOReturnNoPower;

	*reg_value	= OSReadLittleInt32(  (UInt32*)fProvider->fpRegs, reg );
	*outPutSize = sizeof( UInt32 );		// returned byte count

///	IOLog( "UniNEnetUserClient::getOneGMACReg %d, 0x%x\n", reg_num, *reg_value );

	return kIOReturnSuccess;
}/* end getOneGMACReg */


IOReturn UniNEnetUserClient::writeOneGMACReg(	void		*pIn,		void		*pOut,
												IOByteCount	inputSize,	IOByteCount	*outPutSize )
{
	UCRequest	*req = (UCRequest*)pIn;
	UInt32		regValue;
	UInt32 		reg;


	*outPutSize	= 0;							// init returned byte count
	reg			= (UInt32)req->pLogBuffer;		// use the input struct's bufSize for the reg #
	if ( (reg > 0x905C) || (reg & 3) )
	{
		IOLog( "UniNEnetUserClient::writeOneGMACReg - invalid register number: 0x%lx \n",
				reg );
		return kIOReturnError;
	}

	if ( !fProvider->fCellClockEnabled )
		return kIOReturnNoPower;

	regValue = req->bufSize;
	OSWriteLittleInt32(  (UInt32*)fProvider->fpRegs, reg, regValue );

	if ( reg == offsetof( GMAC_Registers, SendPauseCommand ) )
		fProvider->fSendPauseCommand = regValue & 0x0000FFFF;

///	IOLog( "UniNEnetUserClient::writeOneGMACReg %d, 0x%x\n", reg, regValue );

	return kIOReturnSuccess;
}/* end writeOneGMACReg */


	/* getGMACTxRing - Get Tx ring elements.		*/
	/*												*/
	/* input is:									*/
	/*		command code (kGMACUserCmd_GetTxRing)	*/
	/*		buffer address							*/
	/*		buffer size								*/
	/*												*/
	/* output set to ring elements					*/

IOReturn UniNEnetUserClient::getGMACTxRing(	void		*pIn,		void		*pOut,
											IOByteCount	inputSize,	IOByteCount	*pOutPutSize )
{
	UInt64		*src;
	UInt64		*dst;
	UInt32		len;


	src	= (UInt64*)fProvider->fTxDescriptorRing;
	dst	= (UInt64*)pOut;
	len	= fProvider->fTxRingElements * sizeof( TxDescriptor );

	*pOutPutSize = len;

	while ( len )
	{
		*dst++ = OSReadLittleInt64( src, 0 );
		src += 1;
		len -= sizeof( UInt64 );
	}

	UC_ELG( fProvider->fTxRingElements, fProvider->fTxDescriptorRing, 'gTxR', "UniNEnetUserClient::getGMACTxRing - done" );
    return kIOReturnSuccess;
}/* end getGMACTxRing */


	/* getGMACRxRing - Get Rx ring elements.		*/
	/*												*/
	/* input is bytes:								*/
	/*		command code (kGMACUserCmd_GetRxRing)	*/
	/*		buffer address							*/
	/*		buffer size								*/
	/*												*/
	/* output set to ring elements					*/

IOReturn UniNEnetUserClient::getGMACRxRing(	void		*pIn,		void		*pOut,
											IOByteCount	inputSize,	IOByteCount	*pOutPutSize )
{
	UInt64		*src	= (UInt64*)fProvider->fRxDescriptorRing;
	UInt64		*dst	= (UInt64*)pOut;
	UInt32		len		= fProvider->fRxRingElements * sizeof( RxDescriptor );


	*pOutPutSize = len;

	while ( len )
	{
		*dst++ = OSReadLittleInt64( src, 0 );
		src++;
		len -= sizeof( UInt64 );
	}

	UC_ELG( fProvider->fRxRingElements, fProvider->fRxDescriptorRing, 'gRxR', "UniNEnetUserClient::getGMACRxRing - done" );
    return kIOReturnSuccess;
}/* end getGMACRxRing */


IOReturn UniNEnetUserClient::readAllMII(	void		*pIn,
											void		*pOut,
											IOByteCount	inputSize,
											IOByteCount	*outPutSize )
{
	UCRequest	*req = (UCRequest*)pIn;
	UInt32 		phyAddr, origPHYaddr = 0;
	UInt16		*reg_value;		// 32 shorts are small enough to go directly out to user
	UInt16		i;
	bool		result;
	bool		cellClockEnabled;
	IOReturn	ior = kIOReturnSuccess;

	if ( !(pOut && outPutSize && *outPutSize >= (32 * sizeof( UInt16 ))) )
	{
		UC_ELG( pOut,  outPutSize, 'rAll', "UniNEnetUserClient::readAllMII - bad argument" );
		IOLog( "UniNEnetUserClient::readAllMII - bad argument(s) pOut = %08lx, outPutSize = %08lx, ", (UInt32)pOut, (UInt32)outPutSize );
		if ( outPutSize )
			 IOLog( "*outPutSize = %ld.", (UInt32)*outPutSize );
		IOLog( "\n" );
		return kIOReturnBadArgument;
	}

	phyAddr = (UInt32)req->pLogBuffer >> 16;
	if ( phyAddr > 31 && phyAddr != 0xFF )
	{
		IOLog( "UniNEnetUserClient::readAllMII - bad argument PHY address = %lx\n", phyAddr );
		return kIOReturnBadArgument;
	}

	cellClockEnabled = fProvider->fCellClockEnabled;
	if ( cellClockEnabled == false )
		fProvider->enableCellClock();

	if ( phyAddr != 0xFF )
	{
		origPHYaddr			= fProvider->phyId;
		fProvider->phyId	= phyAddr;
	}

	reg_value	= (UInt16*)pOut;
	*outPutSize	= 0;						// init returned byte count

	for ( i = 0 ; i < 32; i++ )
	{
		result = fProvider->miiReadWord( reg_value, i );
		if ( result )
		{
			reg_value++;					// incr to next short in the output buffer
			*outPutSize += sizeof( UInt16 );// incr returned byte count
		}
		else
		{
			IOLog( "Read of PHY register %d failed.\n", i );
			ior = kIOReturnError;
			break;
		}
	}/* end FOR */

	if ( phyAddr != 0xFF )
		fProvider->phyId = origPHYaddr;

	if ( cellClockEnabled == false )
		fProvider->disableCellClock();

	return ior;
}/* end readAllMII */


IOReturn UniNEnetUserClient::readMII(	void		*pIn,		void		*pOut,
										IOByteCount	inputSize,	IOByteCount	*outPutSize )
{
	UCRequest	*req = (UCRequest*)pIn;
	UInt16		*reg_value = (UInt16*)pOut;
	UInt32 		reg_num, phyAddr, origPHYaddr = 0;
	bool		result;
	bool		cellClockEnabled;


	*outPutSize	= 0;									// init returned byte count
	phyAddr		= (UInt32)req->pLogBuffer >> 16;
	reg_num		= (UInt32)req->pLogBuffer & 0xFFFF;		// use the input struct's bufSize for the reg #
	if ( reg_num > 31 )
		return kIOReturnError;


	cellClockEnabled = fProvider->fCellClockEnabled;
	if ( cellClockEnabled == false )
		fProvider->enableCellClock();

	if ( phyAddr != 0xFF )
	{
		origPHYaddr			= fProvider->phyId;
		fProvider->phyId	= phyAddr;
	}

	result = fProvider->miiReadWord( reg_value, reg_num );

	if ( phyAddr != 0xFF )
		fProvider->phyId = origPHYaddr;

	if ( cellClockEnabled == false )
		fProvider->disableCellClock();

	if ( !result )
	{
		IOLog( "Read of PHY register %ld failed.\n", reg_num );
		return kIOReturnError;			// todo - see if more robust 'read all' is in order
	}

///	IOLog( "Read of PHY register %d, 0x%x\n", reg_num, *reg_value );
	*outPutSize = sizeof( UInt16 );		// returned byte count

	return kIOReturnSuccess;
}/* end readMII */


IOReturn UniNEnetUserClient::writeMII(	void		*pIn,		void		*pOut,
										IOByteCount	inputSize,	IOByteCount	*outPutSize )
{
	UCRequest	*req = (UCRequest*)pIn;
	UInt32		reg_num, reg_val, phyAddr, origPHYaddr = 0;
	bool		result;
	bool		cellClockEnabled;


	phyAddr	= (UInt32)req->pLogBuffer >> 16;
	reg_num = (UInt32)req->pLogBuffer & 0xFFFF;
	reg_val = req->bufSize;

	if ( reg_num > 31 || reg_val > 0xFFFF || (phyAddr > 31 && phyAddr != 0xFF) )
		return kIOReturnBadArgument;

	cellClockEnabled = fProvider->fCellClockEnabled;
	if ( cellClockEnabled == false )
		fProvider->enableCellClock();

	if ( phyAddr != 0xFF )
	{
		origPHYaddr			= fProvider->phyId;
		fProvider->phyId	= phyAddr;
	}

	result = fProvider->miiWriteWord( reg_val, reg_num );

	if ( phyAddr != 0xFF )
		fProvider->phyId = origPHYaddr;

	if ( cellClockEnabled == false )
		fProvider->disableCellClock();

	if ( !result )
	{
		IOLog( "Write of PHY register %ld failed.\n", reg_num );
		return kIOReturnError;			// todo - see if more robust 'read all' is in order
	}

	return kIOReturnSuccess;
}/* end writeMII */
