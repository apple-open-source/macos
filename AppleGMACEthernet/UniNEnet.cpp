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
 * Copyright (c) 1998-2002 Apple Computer
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


#if USE_ELG
void UniNEnet::AllocateEventLog( UInt32 size )
{
	IOPhysicalAddress	phys;
	mach_timespec_t		time;


	fpELG = (elg*)IOMallocContiguous( size, 0x1000, &phys );
	if ( !fpELG )
	{
		kprintf( "AllocateEventLog - UniNEnet evLog allocation failed " );
		return;
	}
#ifdef OPEN_FIRMWARE
///	IOUnmapPages( kernel_map, (vm_offset_t)fpELG, size );
	IOSetProcessorCacheMode(	kernel_task,
								(IOVirtualAddress)fpELG,
								size,
								kIOMapWriteThruCache );
#endif // OPEN_FIRMWARE
	bzero( fpELG, size );

	fpELG->evLogBuf		= (UInt8*)fpELG + sizeof( struct elg );
	fpELG->evLogBufe	= (UInt8*)fpELG + kEvLogSize - 0x20; // ??? overran buffer?
	fpELG->evLogBufp	= fpELG->evLogBuf;
//	fpELG->evLogFlag	 = 0xFEEDBEEF;	// continuous wraparound
	fpELG->evLogFlag	 = 0x03330333;	// > kEvLogSize - don't wrap - stop logging at buffer end
//	fpELG->evLogFlag	 = 0x0099;		// < #elements - count down and stop logging at 0
//	fpELG->evLogFlag	 = 'step';		// stop at each ELG

	IOGetTime( &time );
	fpELG->startTimeSecs	= time.tv_sec;
	fpELG->physAddr			= (UInt32)phys;

	IOLog( "\033[32mUniNEnet::AllocateEventLog - buffer=%8x phys=%8lx \033[0m \n",
							(unsigned int)fpELG, (UInt32)phys );
	return;
}/* end AllocateEventLog */


void UniNEnet::EvLog( UInt32 a, UInt32 b, UInt32 ascii, char* str )
{
	register UInt32		*lp;			/* Long pointer					*/
	register elg		*pe = fpELG;	/* pointer to elg structure		*/
	mach_timespec_t		time;
	UInt32				lefty;


	if ( pe->evLogFlag == 0 )
		return;

	IOGetTime( &time );

	if ( pe->evLogFlag <= kEvLogSize / 0x10 )
		--pe->evLogFlag;
	else if ( pe->evLogFlag == 0xDEBEEFED )
	{
		for ( lp = (UInt32*)pe->evLogBuf; lp < (UInt32*)pe->evLogBufe; lp++ )
			*lp = 0xDEBEEFED;
		pe->evLogBufp	= pe->evLogBuf;		// rewind
		pe->evLogFlag	= 0x03330333;		// stop at end
	}

			/* handle buffer wrap around if any */

	if ( pe->evLogBufp >= pe->evLogBufe )
	{
		pe->evLogBufp = pe->evLogBuf;
		pe->wrapCount++;
		if ( pe->evLogFlag != 0xFEEDBEEF )	// make 0xFEEDBEEF a symbolic ???
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

//	IOFlushProcessorCache( kernel_task, (IOVirtualAddress)(lp - 3), 0x10 );

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
	char		name[] = "UniNEnet: ";
	char		*bp = work;
	UInt8		x;
	int			i;


	EvLog( a, b, ascii, str );
	EvLog( '****', '****', 'Alrt', "*** Alrt" );

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

	fpELG->alertCount++;	// trigger anybody watching

		// The following is ensure viewability with Open Firmware:
	OSSynchronizeIO();
	IOFlushProcessorCache( kernel_task, (IOVirtualAddress)fpELG, kEvLogSize );

//	fpELG->evLogFlag = 0;	// stop logging but alertCount can continue increasing.

///if ( fpELG->evLogFlag == 0xfeedbeef )
///	 fpELG->evLogFlag = 100;	// cruise to see what happens next.

//	kprintf( work );
///	panic( work );
//	Debugger( work );
///	IOLog( work );

	return 0xDEADBEEF;
}/* end Alrt */
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
	ELG( 0, 0, 'eNet', "UniNEnet::init - event logging set up." );
#endif /* USE_ELG */

    if ( super::init( properties ) == false )
        return false;

		/* Initialize some instance variables:	*/

	fCellClockEnabled	= true;
	fMediumType			= kIOMediumEthernetAuto;	// default to autoNegotiation

    return true;
}/* end init */


bool UniNEnet::start( IOService *provider )
{    
    OSString	*matchEntry;
    OSNumber	*numObj;
    IOWorkLoop	*myWorkLoop	= getWorkLoop();
	UInt32		x, xFactor;


	ELG( IOThreadSelf(), provider, 'Strt', "UniNEnet::start - this, provider." );

    matchEntry = OSDynamicCast( OSString, getProperty( gIONameMatchedKey ) );
    if ( matchEntry == 0 )
    {
        ALRT( 0, 0, 'Mat-', "UniNEnet::start: Cannot obtain matching property." );
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

	phyId = 0xFF;
	fLinkStatus = kLinkStatusUnknown;

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

		/// talk to Joe about using allocatePacket()
    MGETHDR( txDebuggerPkt, M_DONTWAIT, MT_DATA );

    if ( !txDebuggerPkt )
    {	IOLog( "UniNEnet::start - Couldn't allocate KDB buffer\n" );
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
    if ( getWorkLoop() )	getWorkLoop()->disableAllEventSources();
    if ( timerSource )		timerSource->release();
    if ( interruptSource )	interruptSource->release();
	if ( txDebuggerPkt )	freePacket( txDebuggerPkt );
	if ( transmitQueue )	transmitQueue->release();
	if ( debugQueue )		debugQueue->release();
	if ( networkInterface )	networkInterface->release();
	if ( fMediumDict )		fMediumDict->release();
	if ( ioMapEnet )		ioMapEnet->release();
	if ( fTxDescriptorRing )IOFreeContiguous( (void*)fTxDescriptorRing, fTxRingElements * sizeof( TxDescriptor ) );
	if ( fRxDescriptorRing )IOFreeContiguous( (void*)fRxDescriptorRing, fRxRingElements * sizeof( RxDescriptor ) );
	if ( fTxMbuf )			IOFree( fTxMbuf, sizeof( mbuf* ) * fTxRingElements );
	if ( fRxMbuf )			IOFree( fRxMbuf, sizeof( mbuf* ) * fRxRingElements );

	if ( workLoop )			workLoop->release();

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
	UInt32				mifStatus;
	bool				doFlushQueue;
	bool				doService;


    if ( fReady == false )
	{
		if ( fCellClockEnabled == false )
	         interruptStatus = 0x8BADF00D;
		else interruptStatus = READ_REGISTER( Status );
		ALRT( 0, interruptStatus, 'int-', "interruptOccurred - not ready" );
		return;
	}

///	do
///	{
		lockState = IODebuggerLock( this );

        interruptStatus = READ_REGISTER( Status );
		ELG( READ_REGISTER( RxCompletion ), interruptStatus, 'Int+', "interruptOccurred - got status" );

#ifdef LOG_RX_BACKUP
{
	UInt32	fifoCtr		= READ_REGISTER( RxFIFOPacketCounter );
	UInt32	rxMACStatus	= READ_REGISTER( RxMACStatus );	// Auto-clear register
	if ( interruptStatus & kStatus_Rx_Buffer_Not_Available
	  || rxMACStatus & kRX_MAC_Status_Rx_Overflow		// see if FIFO overflowed
	  || fifoCtr > 5 )
	{
		ELG( fifoCtr, rxMACStatus, 'Rx--', "interruptOccurred - Rx overflow" );
		fRxMACStatus |= rxMACStatus;					// save for timeout routine
	}
}
#endif // LOG_RX_BACKUP

	///	interruptStatus &= kStatus_TX_INT_ME
	///					 | kStatus_RX_DONE
	///					 | kStatus_MIF_Interrupt;
		fIntStatusForTO |= interruptStatus;	// accumulate Tx & Rx int bits for timer code

        doService  = false;

		if ( interruptStatus & kStatus_TX_INT_ME )
        {
		///	txWDInterrupts++;
            KERNEL_DEBUG( DBG_GEM_TXIRQ | DBG_FUNC_START, 0, 0, 0, 0, 0 );
            doService = transmitInterruptOccurred();
            KERNEL_DEBUG( DBG_GEM_TXIRQ | DBG_FUNC_END,   0, 0, 0, 0, 0 );
			ETHERNET_STAT_ADD( dot3TxExtraEntry.interrupts );
        }

        doFlushQueue = false;

	///	if ( interruptStatus & kStatus_RX_DONE )
        {
		///	rxWDInterrupts++;
            KERNEL_DEBUG( DBG_GEM_RXIRQ | DBG_FUNC_START, 0, 0, 0, 0, 0 );
            doFlushQueue = receivePackets( false );
            KERNEL_DEBUG( DBG_GEM_RXIRQ | DBG_FUNC_END,   0, 0, 0, 0, 0 );
			ETHERNET_STAT_ADD( dot3RxExtraEntry.interrupts );
        }


		if ( interruptStatus & kStatus_MIF_Interrupt )
		{
			mifStatus = READ_REGISTER( MIFStatus );		// clear the interrupt
			ELG( 0, mifStatus, '*MIF', "interruptOccurred - MIF interrupt" );
		}


		IODebuggerUnlock( lockState );

			/* Submit all received packets queued up by receivePackets() to the network stack.	*/
			/* The up call is performed without holding the debugger lock.						*/

		if ( doFlushQueue )
	    	networkInterface->flushInputQueue();

			/* Make sure the output queue is not stalled.	*/
		if ( doService && netifEnabled )
	   	 transmitQueue->service();
///	} while ( interruptStatus );

	return;
}/* end interruptOccurred */


UInt32 UniNEnet::outputPacket( struct mbuf *pkt, void *param )
{
    UInt32		ret = kIOReturnOutputSuccess;

		/*** Caution - this method runs on the client's	***/
		/*** thread not the workloop thread.			***/

    KERNEL_DEBUG( DBG_GEM_TXQUEUE | DBG_FUNC_NONE, (int)pkt, (int)pkt->m_pkthdr.len, 0, 0, 0 );

    reserveDebuggerLock();/* Hold debugger lock so debugger can't interrupt us	*/

	ELG( pkt, fLinkStatus, 'OutP', "outputPacket" );
///	ELG( pkt, READ_REGISTER( StatusAlias ), 'OutP', "outputPacket" );

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

    releaseDebuggerLock();

    return ret;
}/* end outputPacket */


void UniNEnet::putToSleep( bool sleepCellClockOnly )
{
	IOMediumType    mediumType	= kIOMediumEthernetNone;
	IONetworkMedium	*medium;


	ELG( fCellClockEnabled, sleepCellClockOnly, 'Slep', "UniNEnet::putToSleep" );

    if ( fCellClockEnabled == false )
    {		/* See if the everything is shutting down,	*/
			/* or just disabling the clock:				*/
        if ( sleepCellClockOnly )
			return;	// just disable the clock - it's already disabled - so just return

			/* Shutting down the whole thing. The ethernet cell's clock,	*/
			/* is off so we must enable it before continuing				*/
		ELG( 0, 0, '+Clk', "UniNEnet::putToSleep - turning on cell clock!!!" );
	///	OSSynchronizeIO();
		callPlatformFunction( "EnableUniNEthernetClock", true, (void*)true, (void*)nub, 0, 0 );
		OSSynchronizeIO();
		IODelay( 3 );			// Allow the cell some cycles before using it.
		fCellClockEnabled = true;
    }

	reserveDebuggerLock();

	fReady = false;

	if ( timerSource ) 
		 timerSource->cancelTimeout();

	WRITE_REGISTER( InterruptMask, kInterruptMask_None );

	if ( getWorkLoop() )
		 getWorkLoop()->disableAllInterrupts();



medium = IONetworkMedium::getMediumWithType( fMediumDict, mediumType );
setLinkStatus( kIONetworkLinkValid, medium, 0 ); // Link status is Valid and inactive.
	if ( sleepCellClockOnly )
	{
///		setLinkStatus( kIONetworkLinkValid ); // Link status is Valid and inactive.
	}
	else
	{
	///	setLinkStatus( 0, 0 );
		stopChip();					// stop the DMA engines.
		stopPHY();					// Set up for wake on Magic Packet if wanted.
	}

									// Flush all mbufs from TX ring.
	flushRings( true, false );		// Flush the Tx ring.

	currentPowerState = 0;			// No more accesses to chip's registers.

	if ( fBuiltin && (!fWOL || sleepCellClockOnly) )
	{
        fCellClockEnabled = false;
		ELG( 0, 0, '-Clk', "UniNEnet::putToSleep - disabling cell clock!!!" );
		OSSynchronizeIO();
		callPlatformFunction( "EnableUniNEthernetClock", true, (void*)false, (void*)nub, 0, 0 );
		OSSynchronizeIO();
		ELG( 0, 0, '-clk', "UniNEnet::putToSleep - disabled ethernet cell clock." );
	}

    if ( sleepCellClockOnly )
        timerSource->setTimeoutMS( WATCHDOG_TIMER_MS );

    releaseDebuggerLock();

    return;
}/* end putToSleep */


bool UniNEnet::wakeUp( bool wakeCellClockOnly )
{
	bool			rc = false;
	bool			regAvail;
	UInt32			gemReg = 0;
	IOMediumType    mediumType	= kIOMediumEthernetNone;
	IONetworkMedium	*medium;


/// fpELG->evLogFlag = 0xDEBEEFED;		/// clear and reset the log buffer.

	ELG( this, wakeCellClockOnly, 'Wake', "UniNEnet::wakeUp" );

    reserveDebuggerLock();

    fReady = false;
	
	if ( !wakeCellClockOnly )	/// ?Is this necessary?
		phyId = 0xFF;

	if ( timerSource ) 
		 timerSource->cancelTimeout();

	if ( getWorkLoop() )
		 getWorkLoop()->disableAllInterrupts();

///	setLinkStatus( 0, 0 );	    // Initialize the link status.
	medium = IONetworkMedium::getMediumWithType( fMediumDict, mediumType );
	setLinkStatus( kIONetworkLinkValid, medium, 0 ); // Link status is Valid and inactive.

	if ( fBuiltin )
	{
			// Set PHY and/or Cell to full power:

		ELG( 0, 0, '+Clk', "UniNEnet::wakeUp - turning on cell clock!!!" );
		callPlatformFunction( "EnableUniNEthernetClock", true, (void*)true, (void*)nub, 0, 0 );
		IODelay( 3 );

		if ( ioMapEnet )			// Probe register access if able:
		{
			IOSleep( 10 );
	
			regAvail = ml_probe_read(	(vm_offset_t)&fpRegsPhys->Status,
										&(unsigned int)gemReg );

			if ( !regAvail )		// try again if cell clock disabled: 
			{
				ALRT( 0, 0, 'wk1-', "UniNEnet::wakeUp - ethernet cell's clock is disabled." );
				callPlatformFunction( "EnableUniNEthernetClock", true, (void*)true, (void*)nub, 0, 0 );
				IOSleep( 10 );
				regAvail = ml_probe_read(	(vm_offset_t)&fpRegsPhys->Status,
											&(unsigned int)gemReg );
				if ( !regAvail )	// return FALSE if cell clock still disabled. 
				{
					ALRT( 0, 0, 'wk2-', "UniNEnet::wakeUp - ethernet cell's clock is still disabled." );
					goto wakeUp_exit;	
				}/* end IF still disabled */
			}/* end IF need to try again. */ 
		}/* end IF can probe UniN register access */
        fCellClockEnabled = true;
	}/* end IF builtin ethernet */

	if ( !wakeCellClockOnly )
	{
			/* BUS MASTER, MEM I/O Space, MEM WR & INV	*/

		nub->configWrite32( 0x04, 0x16 );		// write to the Config space

		if ( ioMapEnet == NULL )
		{
			ioMapEnet = nub->mapDeviceMemoryWithRegister( 0x10 );
			if ( ioMapEnet == NULL )
				goto wakeUp_exit;

			fpRegs	= (GMAC_Registers*)ioMapEnet->getVirtualAddress();
			ELG( ioMapEnet, fpRegs, 'Adrs', "start - base eNet addr" );
				// for ml_probe_read on Wake:
			fpRegsPhys	= (GMAC_Registers*)ioMapEnet->getPhysicalAddress();
		}
	}

	if ( !initRxRing() || !initTxRing() ) 
		goto wakeUp_exit;

	currentPowerState = 1;		// Allow access to cell's registers.

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
			hardwareResetPHY();			/* Generate a hardware PHY reset.	*/
	
			if ( phyId == 0xFF )
			{
				if ( miiFindPHY( &phyId ) == false )
					goto wakeUp_exit;
			}
		}

		getPhyType();					// Also patches PHYs

		if ( !fMediumDict && createMediumTables() == false )
		{
			ALRT( 0, 0, 'cmt-', "UniNEnet::start - createMediumTables failed" );    
			goto wakeUp_exit;
		}

		startPHY();						// Bring up the PHY and the MAC.

		if ( fBuiltin )
			miiInitializePHY( phyId );
	}

	timerSource->setTimeoutMS( WATCHDOG_TIMER_MS );

	if ( getWorkLoop() )
		 getWorkLoop()->enableAllInterrupts();

	fReady = true;

	if ( !wakeCellClockOnly )
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
        IOLog( "EtherNet(UniN): already enabled\n" );
        return kIOReturnSuccess;
    }

    if ( (fReady == false) && !wakeUp(false) )
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
 
IOReturn UniNEnet::disable( IONetworkInterface* /*netif*/ )
{
#if USE_ELG
///	if ( (fpELG->evLogFlag == 0) || (fpELG->evLogFlag == 0xFEEDBEEF) )
///	fpELG->evLogFlag = 0xDEBEEFED;
#endif // USE_ELG

	ELG( this, debugEnabled, 'NetD', "disable( IONetworkInterface* )" );

    /*
     * Disable our IOOutputQueue object. This will prevent the
     * outputPacket() method from being called.
     */
    transmitQueue->stop();

    /*
     * Flush all packets currently in the output queue.
     */
    transmitQueue->setCapacity( 0 );
    transmitQueue->flush();

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
	UInt32				txRingIndex;
	UInt32				x;


		/*** Caution - this method runs on the workloop thread while	***/
		/*** the outputPacket method runs on the client's thread.		***/ 

	ELG( txCommandHead << 16 | txCommandTail, fCellClockEnabled, 'Time', "UniNEnet::timeoutOccurred" );

		/* If the ethernet cell clock is disabled, monitorLinkStatus	*/
		/* is called, and the rest of this function is skipped.			*/
    if ( fCellClockEnabled == false )
    {
        monitorLinkStatus( false );
        timerSource->setTimeoutMS( WATCHDOG_TIMER_MS );
        return; 
    }    

	x			  = READ_REGISTER( TxMACStatus );
	fRxMACStatus |= READ_REGISTER( RxMACStatus );
	ELG( x, fRxMACStatus, 'MACS', "timeoutOccurred - Tx and Rx MAC Status regs" );
#ifdef JUST_FOR_TESTING
{
	UInt32	interruptStatus, rxKick, rxCompletion;
	interruptStatus	= READ_REGISTER( StatusAlias );
	rxKick			= READ_REGISTER( RxKick );
	rxCompletion	= READ_REGISTER( RxCompletion );
	ELG( rxKick<<16 | rxCompletion, interruptStatus, 'rxIS', "timeoutOccurred" );
}
#endif // JUST_FOR_TESTING


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

	if ( (fIntStatusForTO & (kStatus_TX_DONE |  kStatus_RX_DONE)) == 0 )
		monitorLinkStatus( false );	// Don't do this if neither Tx nor Rx are moving

		// if the link went down (fLinkStatus is updated in monitorLinkStatus),
		// disable the ethernet clock and exit this function.
	if ( fLinkStatus == kLinkStatusDown )
	{ 
		putToSleep( true );
		timerSource->setTimeoutMS( WATCHDOG_TIMER_MS );
		return;
	} 

		/* If there are pending entries on the Tx ring:	*/

///	if ( txCommandHead != txCommandTail )
	if ( (fIntStatusForTO & kStatus_TX_DONE) )
	{
		txWDCount = 0;
	}
	else
	{		/* If the hardware Tx pointer did not move since	*/
			/* the last check, increment the txWDCount.			*/
		txRingIndex = READ_REGISTER( TxCompletion );
		if ( txRingIndex == txRingIndexLast )
		{
			txWDCount++;			/* bump the watchdog count */
		}
		else
        {
            txWDCount = 0;			/* reset the watchdog count */
            txRingIndexLast = txRingIndex;
        }

        if ( txWDCount > 2 )
        {
            /* We take interrupts every 32 or so tx completions, so we may be here just
             * to do normal clean-up of tx packets. We check if the hardware tx pointer
             * points to the next available tx slot. This indicates that we transmitted all
             * packets that were scheduled vs rather than the hardware tx being stalled.
             */
            if ( txRingIndex != txCommandTail )
            {
                UInt32        intStatus, compReg, kickReg;
 
				intStatus		= READ_REGISTER( StatusAlias );	// don't use auto-clear reg
				compReg			= READ_REGISTER( TxCompletion );
				kickReg			= READ_REGISTER( TxKick );
                ALRT( intStatus, kickReg << 16 | compReg, 'Tx--', "UniNEnet::timeoutOccurred - Tx Int Timeout" );
            }

			transmitInterruptOccurred();

            doService = true;

            txRingIndexLast = txRingIndex;
            txWDCount = 0;
        }
    }
    
		/* Check for Rx deafness.										*/
		/* IF no Rx interrupts have occurred in the past few timeouts	*/
		/* AND the FIFO overflowed,										*/
		/* THEN restart the receiver.									*/

    if ( fIntStatusForTO & kStatus_RX_DONE )
	{
		rxWDCount      = 0;			// Reset watchdog timer count
	///	rxWDInterrupts = 0;			// Reset watchdog interrupt count
	}
	else if ( rxWDCount++ >= 2 )	// skip 1st timer period
    {
		if ( (fRxMACStatus & kRX_MAC_Status_Rx_Overflow)	// If FIFO overflow
		  || (rxWDCount > (30000 / WATCHDOG_TIMER_MS)) )	// or 30 seconds max idle
		{
				// Bad news, the receiver may be deaf as a result of this
				// overflow, and if so, a RX MAC reset is needed.

			restartReceiver();

			NETWORK_STAT_ADD( inputErrors );
			ETHERNET_STAT_ADD( dot3RxExtraEntry.watchdogTimeouts );
			fRxMACStatus	= 0;		// reset FIFO overflow indicator
			rxWDCount		= 0;		// reset the watchdog count.
		}
    }

	fIntStatusForTO = 0;				// reset the Tx and Rx accumulated int bits.

		/* Clean-up after the debugger if the debugger was active:	*/

	if ( debugTxPoll )
	{
		debugQueue->flush();
		debugTxPoll	= false;
		doService	= true;
	}
	IODebuggerUnlock( lockState );

		/* Make sure the queue is not stalled.	*/

	if ( doService && netifEnabled )
		transmitQueue->service();

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
	ELG( 0, active, 'SetP', "setPromiscuousMode" );

	reserveDebuggerLock();

	fIsPromiscuous = active;

	if ( fCellClockEnabled )
	{
		fRxMACConfiguration	= READ_REGISTER( RxMACConfiguration );
	
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

	releaseDebuggerLock();

	return kIOReturnSuccess;
}/* end setPromiscuousMode */


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
    
	if ( fCellClockEnabled == false )
		return kIOReturnSuccess;
	
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


bool UniNEnet::createMediumTables()
{
	IONetworkMedium		*medium;
	UInt32				i;


	fMediumDict = OSDictionary::withCapacity( fMediumTableCount );
	ELG( 0, fMediumDict, 'MTbl', "createMediumTables" );
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
	IOMediumType	mType		= medium->getType();
	UInt16			controlReg	= 0;
	IOReturn		ior;
	bool			gotReg;


			/* If the user sets a speed/duplex unsupported by the hub/switch,		*/
			/* link will not be established and the cell clock will be disabled.	*/
			/* Wake it up so the setting can be fixed:								*/
    if ( fCellClockEnabled == false )
		wakeUp( true );

///fpELG->evLogFlag = 0xDEBEEFED;	/// ???

	gotReg = miiReadWord( &controlReg, MII_CONTROL, phyId );
	ALRT( controlReg, mType, 'sMed', "selectMedium" );

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

	ior = negotiateSpeedDuplex( controlReg );		
	if ( ior != kIOReturnSuccess )
	{
			/* Negotiation failed - just force the user's desires on the PHY:	*/
		ior = forceSpeedDuplex( controlReg );
		if ( ior != kIOReturnSuccess )
			return ior;
	}
	
	setSelectedMedium( medium );
	ELG( fXIFConfiguration, controlReg, 'sMe+', "UniNEnet::selectMedium - returning kIOReturnSuccess" );

	monitorLinkStatus( true );				// force Link change notification

	return kIOReturnSuccess;
}/* end selectMedium */


IOReturn UniNEnet::negotiateSpeedDuplex( UInt16 controlReg )
{
	UInt16			anar;		// 04 - AutoNegotiation Advertisement Register
	UInt16			gigReg;		// Vendor specific register
	IOMediumType	mType;
	bool			br;


	mType =  fMediumType & (kIOMediumNetworkTypeMask | kIOMediumSubTypeMask | kIOMediumCommonOptionsMask);

	controlReg |= MII_CONTROL_AUTONEGOTIATION | MII_CONTROL_RESTART_NEGOTIATION;

	ELG( mType, controlReg, 'n SD', "UniNEnet::negotiateSpeedDuplex" );

	br = miiReadWord( &anar, MII_ADVERTISEMENT, phyId );

	anar &= ~(	MII_ANAR_100BASET4			/* turn off all speed/duplex bits	*/
			  | MII_ANAR_100BASETX_FD		/* This register has only  10/100	*/
			  | MII_ANAR_100BASETX			/* Full/Half bits - no gigabit		*/
			  | MII_ANAR_10BASET_FD
			  | MII_ANAR_10BASET );

		/* Set the Speed/Duplex bit that we need:	*/

	switch ( mType )
	{
	case kIOMediumEthernetAuto:
		anar |=	(	MII_ANAR_100BASETX_FD	/* turn on all speed/duplex bits	*/
				  | MII_ANAR_100BASETX
				  | MII_ANAR_10BASET_FD
				  | MII_ANAR_10BASET );
		break;

	case kIOMediumEthernet10BaseT | kIOMediumOptionFullDuplex:		// 10 Full
		anar |= MII_ANAR_10BASET_FD;
		break;

	case kIOMediumEthernet10BaseT | kIOMediumOptionHalfDuplex:		// 10 Half
		anar |= MII_ANAR_10BASET;
		break;

	case kIOMediumEthernet100BaseTX | kIOMediumOptionFullDuplex:	// 100 Full
		anar |= MII_ANAR_100BASETX_FD;
		break;

	case kIOMediumEthernet100BaseTX | kIOMediumOptionHalfDuplex:	// 100 Half
		anar |= MII_ANAR_100BASETX;
		break;

	case kIOMediumEthernet1000BaseTX | kIOMediumOptionFullDuplex:	// 1000 Full
	case kIOMediumEthernet1000BaseTX | kIOMediumOptionHalfDuplex:	// 1000 Half
		break;	//	gigabit is vendor specific - do it there

	default:		/* unknown	*/
		ELG( 0, 0, ' ?sd', "UniNEnet::negotiateSpeedDuplex - not 10 nor 100 combo." );
		break;
	}/* end SWITCH on speed/duplex */

	miiWriteWord( anar, MII_ADVERTISEMENT, phyId );


		/* Do vendor specific stuff:	*/

	switch ( fPHYType )
	{
					/* Non gigabit PHYs:	*/
	case 0x0971:									// Level One LXT971:
	case 0x5201:									// Broadcom 52x1:
	case 0x5221:
		break;
					/* Gigabit PHYs:	*/

	case 0x1011:									// Marvell:
			/* Enable Automatic Crossover:	*/
		br = miiReadWord( &gigReg, MII_MARVELL_PHY_SPECIFIC_CONTROL, phyId );
		gigReg |= MII_MARVELL_PHY_SPECIFIC_CONTROL_AUTOL_MDIX;
		miiWriteWord( gigReg, MII_1000BASETCONTROL, phyId );
		controlReg |= MII_CONTROL_RESET;

		// fall through to generic gigabit code.
	case 0x5400:									// Broadcom 54xx:
	case 0x5401:
	case 0x5411:
	case 0x5421:
		br = miiReadWord( &gigReg, MII_1000BASETCONTROL, phyId );
			// Turn off gig/Half and gig/Full bits:
		gigReg &= ~(MII_1000BASETCONTROL_FULLDUPLEXCAP | MII_1000BASETCONTROL_HALFDUPLEXCAP);

			/* Turn on gig/Full or gig/Half as appropriate:	*/

		switch ( mType )
		{					// gig/Full:
		case kIOMediumEthernetAuto:
		case kIOMediumEthernet1000BaseTX | kIOMediumOptionFullDuplex:
			gigReg |= MII_1000BASETCONTROL_FULLDUPLEXCAP;
			break;
							// gig/Half:
		case kIOMediumEthernet1000BaseTX | kIOMediumOptionHalfDuplex:
			gigReg |= MII_1000BASETCONTROL_HALFDUPLEXCAP;
			break;
		}/* end SWITCH on Marvell gig/Full or gig/Half */

		miiWriteWord( gigReg, MII_1000BASETCONTROL, phyId );

		break;

	case ' GEM':	// GEM card is fiber optic and nonnegotiable
	default:
		break;
	}/* end SWITCH on PHY type */

	miiWriteWord( controlReg, MII_CONTROL, phyId );

	br = miiWaitForAutoNegotiation( phyId );
	if ( br )
		 return kIOReturnSuccess;
	return kIOReturnIOError;
}/* end negotiateSpeedDuplex */


IOReturn UniNEnet::forceSpeedDuplex( UInt16 controlReg )
{
	IOMediumType	mType;
	UInt16			statusReg;
	UInt16			gigReg;		// Vendor specific register
	bool			br;


	mType = fMediumType & (kIOMediumNetworkTypeMask | kIOMediumSubTypeMask | kIOMediumCommonOptionsMask);

	ELG( mType, controlReg, 'f SD', "UniNEnet::forceSpeedDuplex" );

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

	case kIOMediumEthernet1000BaseTX:
		controlReg = MII_CONTROL_SPEED_SELECTION_2;
		break;
	}/* end SWITCH */


	if ( mType & kIOMediumOptionFullDuplex )	controlReg |= MII_CONTROL_FULLDUPLEX;
	if ( mType & kIOMediumOptionLoopback )		controlReg |= MII_CONTROL_LOOPBACK;

	switch ( fPHYType )
	{
	case 0x1011:										// Marvell:
			/* Disable Crossover cable:	*/
		br = miiReadWord( &gigReg, MII_MARVELL_PHY_SPECIFIC_CONTROL, phyId );
		gigReg &= ~(MII_MARVELL_PHY_SPECIFIC_CONTROL_AUTOL_MDIX
				  | MII_MARVELL_PHY_SPECIFIC_CONTROL_MANUAL_MDIX);
		miiWriteWord( gigReg, MII_1000BASETCONTROL, phyId );
		controlReg |= MII_CONTROL_RESET;
		break;

	case 0x0971:										// Level One LXT971:
	case 0x5201:										// Broadcom PHYs:
	case 0x5221:
	case 0x5400:
	case 0x5401:
	case 0x5411:
	case 0x5421:
	case ' GEM':
	default:		// first, reset the PHY:
		miiWriteWord( MII_CONTROL_RESET, MII_CONTROL, phyId );
		IOSleep( 3 );
		break;
	}/* end SWITCH on PHY type */

//	if ( mType & kIOMediumOptionFlowControl )	/// touch up the MAC

	miiWriteWord( controlReg, MII_CONTROL, phyId );

	if ( controlReg & MII_CONTROL_SPEED_SELECTION_2 )
		 fXIFConfiguration |= kXIFConfiguration_GMIIMODE;	// set MAC to GIG:
	else fXIFConfiguration &= kXIFConfiguration_GMIIMODE;	// set MAC to nonGIG:
	WRITE_REGISTER( XIFConfiguration, fXIFConfiguration );	// So order.

	for ( int i = 0; i < 5000; i+= 10 )
	{
		miiReadWord( &statusReg, MII_STATUS, phyId );
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
        ALRT( data, pReg, 'Wrg-', "writeRegister: enet clock is disabled" );
        return;
    }

	if ( pReg != &fpRegs->MIFBitBangFrame_Output )
		ELG( data, (UInt32)pReg - (UInt32)fpRegs, 'wReg', "writeRegister" );

	OSWriteLittleInt32( pReg, 0, data );
	return;
}/* end writeRegister */


IOReturn UniNEnet::newUserClient(	task_t			owningTask,
									void*,						// Security id (?!)
									UInt32			type,		// Lucky number
									IOUserClient	**handler )	// returned handler
{
	IOReturn			ior		= kIOReturnSuccess;
	UniNEnetUserClient	*client	= NULL;

	
	ELG( type, type, 'Usr+', "UniNEnet::newUserClient" );

		// Check that this is a user client type that we support.
		// type is known only to this driver's user and kernel
		// classes. It could be used, for example, to define
		// read or write privileges. In this case, we look for
		// a private value.
	if ( type != 'GMAC' )
	{		/// ??? don't return error - call superclass and return its code.
		ELG( 0, type, 'Usr-', "UniNEnet::newUserClient - unlucky." );
		return 0x333;
	}

		// Instantiate a new client for the requesting task:

	client = UniNEnetUserClient::withTask( owningTask );
	if ( !client )
	{
		ELG( 0, 0, 'usr-', "UniNEnet::newUserClient: Can't create user client" );
		return 0x334;
	}

	if ( ior == kIOReturnSuccess )
	{		// Attach ourself to the client so that this client instance can call us.
		if ( client->attach( this ) == false )
		{
			ior = 0x335;
			ELG( 0, 0, 'USR-', "UniNEnet::newUserClient: Can't attach user client" );
		}
	}

	if ( ior == kIOReturnSuccess )
	{		// Start the client so it can accept requests.
		if ( client->start( this ) == false )
		{
			ior = 0x336;
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


bool UniNEnetUserClient::start( IOService * provider )
{
	fProvider = (UniNEnet*)provider;
	UC_ELG( 0, 0, 'UC S', "UniNEnetUserClient::start" );

	if ( super::start( provider ) == false )	return false;
	if ( provider->open( this )   == false )	return false;

	fProvider = (UniNEnet*)provider;

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
		UC_ELG( 0, 0, 'UC D', "UniNEnetUserClient::clientDied" );
	
	return clientClose();
}/* end UniNEnetUserClient::clientDied */


IOReturn UniNEnetUserClient::connectClient( IOUserClient *client )
{
	UC_ELG( 0, 0, 'uCon', "connectClient - connect client" );
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


	UC_ELG( 0, index, 'uXMi', "getExternalMethodForIndex - get external method" );

    if ( index == 0 )
        result = &fMethods[0];

    return result;
}/* end getExternalMethodForIndex */


IOReturn UniNEnetUserClient::doRequest(
								void		*pIn,		void		*pOut,
								IOByteCount	inputSize,	IOByteCount	*pOutPutSize )
{
	UInt8	*input;


	UC_ELG( *pOutPutSize, (UInt32)pIn, 'uReq', "doRequest - get external method" );

		// check first byte of input data for a command code
	if ( pIn && pOut && (inputSize > 0) )
	{
		input = (UInt8*)pIn;

		switch( *input )	// 1st byte of input has request ID
		{
		case kGMACUserCmd_GetLog:	return getGMACLog(		pIn, pOut, inputSize, pOutPutSize );
		case kGMACUserCmd_GetRegs:	return getGMACRegs(		pIn, pOut, inputSize, pOutPutSize );

		case kGMACUserCmd_GetTxRing:	return getGMACTxRing(	pIn, pOut, inputSize, pOutPutSize );
		case kGMACUserCmd_GetRxRing:	return getGMACRxRing(	pIn, pOut, inputSize, pOutPutSize );

		case kGMACUserCmd_ReadAllMII:return readAllMII(	pIn, pOut, inputSize, pOutPutSize );
		case kGMACUserCmd_ReadMII:	return readMII(		pIn, pOut, inputSize, pOutPutSize );
		case kGMACUserCmd_WriteMII:	return writeMII(    pIn, pOut, inputSize, pOutPutSize );

		default:
			IOLog( "UniNEnetUserClient - Bad command to doRequest, %x\n", *input );
		}
	}
	else IOLog( "UniNEnetUserClient - pin/pout,size error\n" );

	return kIOReturnBadArgument;
}/* end doRequest */


#if USE_ELG
	/* getGMACLog - Get UniNEnet event log.		*/
	/*											*/
	/* input is 9 bytes:						*/
	/*		command code (kGMACUserCmd_GetLog)	*/
	/*		four bytes of buffer address		*/
	/*		four bytes of buffer size			*/
	/*											*/
	/* output set to GMACLogInfo record			*/
	/*		and buffer filled with log data		*/

IOReturn UniNEnetUserClient::getGMACLog(
							void		*pIn,		void		*pOut,
							IOByteCount	inputSize,	IOByteCount	*pOutPutSize )
{
	IOMemoryDescriptor	*md;	// make a memory descriptor for the client's big buffer
	UInt8				*input = (UInt8*)pIn;
	vm_address_t		bigaddr;
	IOByteCount			biglen;
	IOByteCount			bc;
	IOReturn 			ior;


	UC_ELG( (UInt32)pIn,  inputSize,    'UgLg', "UniNEnetUserClient::getGMACLog" );
	UC_ELG( (UInt32)pOut, *pOutPutSize, 'UgL2', "UniNEnetUserClient::getGMACLog" );

//	require( inputSize == 9, Fail );
//	require( pOutPutSize, Fail );
//	require( *pOutPutSize == sizeof( GMACLogInfo ), Fail );

		// Skip Req ID and get following buffer addr and buffer size:

	bigaddr = input[1] << 24 | input[2] << 16 | input[3] << 8 | input[4];
	biglen	= input[5] << 24 | input[6] << 16 | input[7] << 8 | input[8];

	UC_ELG( bigaddr, biglen, '=uBf', "UniNEnetUserClient::getGMACLog - user buffer" );

		// Allocate and init the memory descriptor:

	md = IOMemoryDescriptor::withAddress( bigaddr, biglen, kIODirectionOutIn, fTask );	// REVIEW direction
	if ( !md )	goto Fail;

		// copy the buffer over now:

	ior = md->prepare( kIODirectionNone );
	if ( ior )  {	UC_ELG( -1, ior, 'prp-', "UniNEnetUserClient::getGMACLog - prepare failed" ); }

#ifdef JUST_FOR_TESTING	/// don't execute until Alrt is called.
	while( fProvider->fpELG->alertCount == 0 )	IOSleep( 100 );
#endif // JUST_FOR_TESTING

	bc = md->writeBytes( 0, fProvider->fpELG, kEvLogSize );
	if ( bc != kEvLogSize )
		UC_ELG( 0, bc, 'Ubc-', "UniNEnetUserClient::getGMACLog - write failed" );

	ior = md->complete( kIODirectionNone );
	if ( ior )	{ UC_ELG( 0, ior, 'gLg-', "UniNEnetUserClient::getGMACLog - complete failed" ); }
	else   		{ UC_ELG( 0,   0, 'gLg+', "UniNEnetUserClient::getGMACLog - complete worked" ); }

	md->release();			// free it
	fProvider->fpELG->evLogFlag = 0xFEEDBEEF;	/// Let 'er rip again.

    return kIOReturnSuccess;

Fail:

	return kIOReturnBadArgument;
}/* end getGMACLog */

#else // no event logging buffer:
IOReturn UniNEnetUserClient::getGMACLog( void *, void *, IOByteCount, IOByteCount* )
{
	return kIOReturnBadArgument;
}/* end getGMACLog */
#endif // USE_ELG


	/* getGMACRegs - Get UniNEnet registers.	*/
	/*											*/
	/* input is 9 bytes:						*/
	/*		command code (kGMACUserCmd_GetRegs)	*/
	/*		four bytes of buffer address		*/
	/*		four bytes of buffer size			*/
	/*											*/
	/* output set to Length/Type/Value records	*/

IOReturn UniNEnetUserClient::getGMACRegs(
							void		*pIn,		void		*pOut,
							IOByteCount	inputSize,	IOByteCount	*pOutPutSize )
{
	IOMemoryDescriptor	*md;	// make a memory descriptor for the client's big buffer
	UInt8				*input = (UInt8*)pIn;
	vm_address_t		bigaddr;
	IOByteCount			biglen;
	LengthOffset		*pTemplate;
	UInt8				*src;
	UInt32				dest = 0;
	UInt32				len;
	IOByteCount			bc;
	UInt32				lowRegs[ 8 ];
	UInt32				*pl;	// pointer to a Long
	IOReturn 			ior;

//	require( inputSize == 9, Fail );
//	require( pOutPutSize, Fail );
//	require( *pOutPutSize == sizeof( GMACLogInfo ), Fail );

		// Skip Req ID and get following buffer addr and buffer size:

	bigaddr = input[1] << 24 | input[2] << 16 | input[3] << 8 | input[4];
	biglen	= input[5] << 24 | input[6] << 16 | input[7] << 8 | input[8];

		// Allocate and init the memory descriptor:

	md = IOMemoryDescriptor::withAddress( bigaddr, biglen, kIODirectionOutIn, fTask );	// REVIEW direction
	if ( !md )	goto Fail;

		// copy the buffer over now:

	ior = md->prepare( kIODirectionNone );
	if ( ior )  {	UC_ELG( -1, ior, 'prp-', "UniNEnetUserClient::getGMACRegs - prepare failed" ); }

	for ( pTemplate = gGMACRegisterTemplate; ; pTemplate++ )
	{
		bc = md->writeBytes( dest, pTemplate, sizeof( LengthOffset ) );
		dest += sizeof( LengthOffset );

		len = pTemplate->setLength;
		if ( len == 0 )
			break;

			/* 0x000C Status Register autoclears and	*/
			/* must be special cased:					*/

		if ( pTemplate->setOffset == 0 )
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
			bc	= md->writeBytes( dest, src, len );
		}
		else
		{
			src	= (UInt8*)fProvider->fpRegs + pTemplate->setOffset;		
			bc	= md->writeBytes( dest, src, len );
		}

		if ( bc != len )
		{
			UC_ELG( len, bc, 'Ubc-', "UniNEnetUserClient::getGMACRegs - write failed" );
			break;
		}
		dest += len;
	}/* end FOR */

	ior = md->complete( kIODirectionNone );
	if ( ior )	{ UC_ELG( 0, ior, 'gLg-', "UniNEnetUserClient::getGMACRegs - complete failed" ); }
	else   		{ UC_ELG( 0,   0, 'gLg+', "UniNEnetUserClient::getGMACRegs - complete worked" ); }
	md->release();			// free it
    return kIOReturnSuccess;

Fail:

	return kIOReturnBadArgument;
}/* end getGMACRegs */


	/* getGMACTxRing - Get Tx ring elements.		*/
	/*												*/
	/* input is 9 bytes:							*/
	/*		command code (kGMACUserCmd_GetTxRing)	*/
	/*		four bytes of buffer address			*/
	/*		four bytes of buffer size				*/
	/*												*/
	/* output set to Length/Type/Value records		*/

IOReturn UniNEnetUserClient::getGMACTxRing(
							void		*pIn,		void		*pOut,
							IOByteCount	inputSize,	IOByteCount	*pOutPutSize )
{
	IOMemoryDescriptor	*md;	// make a memory descriptor for the client's big buffer
	UInt8				*input = (UInt8*)pIn;
	vm_address_t		bigaddr;
	IOByteCount			biglen;
	UInt8				*src;
	UInt32				dest;
	UInt32				len;
	IOByteCount			bc;
	IOReturn 			ior;


		/* Skip Req ID and get following buffer addr and buffer size:	*/

	bigaddr = input[1] << 24 | input[2] << 16 | input[3] << 8 | input[4];
	biglen	= input[5] << 24 | input[6] << 16 | input[7] << 8 | input[8];

		/* Allocate and init the memory descriptor:	*/

	md = IOMemoryDescriptor::withAddress( bigaddr, biglen, kIODirectionOutIn, fTask );	// REVIEW direction
	if ( !md )	return kIOReturnBadArgument;

		/* copy the Tx ring elements over now:	*/

	ior = md->prepare( kIODirectionNone );
	if ( ior )  {	UC_ELG( -1, ior, 'prp-', "UniNEnetUserClient::getGMACTxRing - prepare failed" ); }

	dest	= 0;
	src		= (UInt8*)fProvider->fTxDescriptorRing;
	len		= fProvider->fTxRingElements * sizeof( TxDescriptor );

	bc = md->writeBytes( dest, src, len );

	ior = md->complete( kIODirectionNone );
	if ( ior )	{ UC_ELG( 0, ior, 'gLg-', "UniNEnetUserClient::getGMACTxRing - complete failed" ); }
	else   		{ UC_ELG( 0, 0,  'gLg+', "UniNEnetUserClient::getGMACTxRing - complete worked" ); }
	md->release();			// free it
    return kIOReturnSuccess;
}/* end getGMACTxRing */


	/* getGMACRxRing - Get Rx ring elements.		*/
	/*												*/
	/* input is 9 bytes:							*/
	/*		command code (kGMACUserCmd_GetRxRing)	*/
	/*		four bytes of buffer address			*/
	/*		four bytes of buffer size				*/
	/*												*/
	/* output set to Length/Type/Value records		*/

IOReturn UniNEnetUserClient::getGMACRxRing(
							void		*pIn,		void		*pOut,
							IOByteCount	inputSize,	IOByteCount	*pOutPutSize )
{
	IOMemoryDescriptor	*md;	// make a memory descriptor for the client's big buffer
	UInt8				*input = (UInt8*)pIn;
	vm_address_t		bigaddr;
	IOByteCount			biglen;
	UInt8				*src;
	UInt32				dest;
	UInt32				len;
	IOByteCount			bc;
	IOReturn 			ior;


		// Skip Req ID and get following buffer addr and buffer size:

	bigaddr = input[1] << 24 | input[2] << 16 | input[3] << 8 | input[4];
	biglen	= input[5] << 24 | input[6] << 16 | input[7] << 8 | input[8];

		// Allocate and init the memory descriptor:

	md = IOMemoryDescriptor::withAddress( bigaddr, biglen, kIODirectionOutIn, fTask );	// REVIEW direction
	if ( !md )	return kIOReturnBadArgument;

		// copy the Rx ring elements over now:

	ior = md->prepare( kIODirectionNone );
	if ( ior )  {	UC_ELG( -1, ior, 'prp-', "UniNEnetUserClient::getGMACRxRing - prepare failed" ); }

	dest	= 0;
	src		= (UInt8*)fProvider->fRxDescriptorRing;
	len		= fProvider->fRxRingElements * sizeof( RxDescriptor );
	bc = md->writeBytes( dest, src, len );

	ior = md->complete( kIODirectionNone );
	if ( ior )	{ UC_ELG( 0, ior, 'gLg-', "UniNEnetUserClient::getGMACRxRing - complete failed" ); }
	else   		{ UC_ELG( 0, 0,  'gLg+', "UniNEnetUserClient::getGMACRxRing - complete worked" ); }
	md->release();			// free it
    return kIOReturnSuccess;
}/* end getGMACRxRing */


	// readAllMII - return all 32 MII registers
	//  (todo: check to see if all HW works with this)

IOReturn UniNEnetUserClient::readAllMII(	void		*pIn,
											void		*pOut,
											IOByteCount	inputSize,
											IOByteCount	*outPutSize )
{
	bool		result;
	UInt16		*reg_value;		// 32 shorts are small enough to go directly out to user
	UInt16		i;


	IOLog( "Hello from readAllMII\n" );
	
	if ( pOut && outPutSize && *outPutSize >= (32 * sizeof( UInt16 )) )
	{
		reg_value	= (UInt16*)pOut;
		*outPutSize	= 0;						// init returned byte count

		for ( i = 0 ; i < 32; i++ )
		{
			result = fProvider->miiReadWord( reg_value, i, kPHYAddr0 );
			if ( result )
			{
				IOLog( "read mii %d, 0x%x\n",	i, *reg_value );
				reg_value++;					// incr to next short in the output buffer
				*outPutSize += sizeof( UInt16 );// incr returned byte count
			}
			else
			{
				IOLog( "read of mii %d failed\n", i );
				return kIOReturnError;			// todo - see if more robust 'read all' is in order
			}
		}/* end FOR */
		return kIOReturnSuccess;
	}
	return kIOReturnBadArgument;
}/* end readAllMII */


IOReturn UniNEnetUserClient::readMII(	void		*pIn,
										void		*pOut,
										IOByteCount	inputSize,
										IOByteCount	*outPutSize )
{
	bool		result;
	UInt16		*reg_value	= (UInt16*)pOut;	// 32 shorts are small enough to go directly out to user
	UInt16 		reg_num		= *((UInt8*)pIn+1);	// single byte of register number after command byte


	IOLog( "hello from readMII\n" );
	if ( pIn && inputSize == 2 && pOut && outPutSize && *outPutSize >= (1 * sizeof(UInt16))) {
		
		if ( reg_num < 32 )
		{
			*outPutSize	= 0;						// init returned byte count
			result		= fProvider->miiReadWord( reg_value, reg_num, kPHYAddr0 );
			if ( result )
			{
				IOLog( "read mii %d, 0x%x\n", reg_num, *reg_value );
				*outPutSize += sizeof( UInt16 );	// incr returned byte count
			}
			else
			{
				IOLog( "read of mii %d failed\n", reg_num );
				return kIOReturnError;			// todo - see if more robust 'read all' is in order
			}
			return kIOReturnSuccess;
		}
	}
	return kIOReturnBadArgument;
}/* end readMII */


IOReturn UniNEnetUserClient::writeMII(	void		*pIn,
										void		*pOut,
										IOByteCount	inputSize,
										IOByteCount	*outPutSize )
{
	UInt8		*input_bytes = (UInt8*)pIn;
	UInt16		reg_num = input_bytes[1];
	UInt16		reg_val = input_bytes[2] << 8 | input_bytes[3];
	bool		result;


	IOLog( "hello from writeMII\n" );
	
	if ( outPutSize )
		*outPutSize = 0;		// not returning any data, zero the byte count
	
		// input: command byte, byte of register number, two bytes of value to write
		// output: ignored
	if ( pIn && inputSize == 4 )
	{	
		if ( reg_num < 32 )
		{
			result = fProvider->miiWriteWord( reg_val, reg_num, kPHYAddr0 );
			if ( result )
			{
				IOLog( "wrote mii %d with 0x%x\n", reg_num, reg_val );
			}
			else
			{
				IOLog( "write of mii %d failed\n", reg_num );
				return kIOReturnError;			// todo - see if more robust 'read all' is in order
			}
			return kIOReturnSuccess;
		}
	}
	return kIOReturnBadArgument;
}/* end writeMII */
