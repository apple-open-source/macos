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
 * Copyright (c) 1998-2002 Apple Software, Inc.
 *
 * Interface definition for the UniN Ethernet Controller 
 *
 * HISTORY
 *
 */

#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOGatedOutputQueue.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/network/IOMbufMemoryCursor.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOLib.h>            /* bcopy */
#include <IOKit/IOUserClient.h>


extern "C" 
{
#include <sys/param.h>
#include <sys/mbuf.h>
}

	typedef volatile UInt32		VU32;
	typedef volatile UInt16		VU16;

	// No kernel tracing support at this time.
#define KERNEL_DEBUG( x, a, b, c, d, e )

#include "UniNEnetRegisters.h"



	/*****	For fans of kprintf, IOLog and debugging infrastructure of the	*****/
	/*****	string ilk, please modify the ELG and ALRT macros or their		*****/
	/*****	associated EvLog and Alrt functions to suit your taste. These	*****/
	/*****	macros currently are set up to log events to a wraparound		*****/
	/*****	buffer with minimal performance impact. They take 2 UInt32		*****/
	/*****	parameters so that when the buffer is dumped 16 bytes per line, *****/
	/*****	time stamps (~1 microsecond) run down the left side while		*****/
	/*****	unique 4-byte ASCII codes can be read down the right side.		*****/
	/*****	Preserving this convention facilitates different maintainers	*****/
	/*****	using different debugging styles with minimal code clutter.		*****/

#define USE_ELG	 	 0				// for debugging
#define kEvLogSize	(4096*16)		// 16 pages = 64K = 4096 events 0x10000
///#define kEvLogSize	(4096*1)		// 1 page = 4K = 256 events

#if USE_ELG /* (( */
#define ELG(A,B,ASCI,STRING)	EvLog( (UInt32)(A), (UInt32)(B), (UInt32)(ASCI), STRING )
#define ALRT(A,B,ASCI,STRING)	Alrt(  (UInt32)(A), (UInt32)(B), (UInt32)(ASCI), STRING )
#define UC_ELG(A,B,ASCI,STRING)	fProvider->EvLog( (UInt32)(A), (UInt32)(B), (UInt32)(ASCI), STRING )
	void   EvLog( UInt32 a, UInt32 b, UInt32 ascii, char* str );
	UInt32 Alrt(  UInt32 a, UInt32 b, UInt32 ascii, char* str );
#else /* ) not USE_ELG: (	*/
#define ELG(A,B,ASCI,S)
#define ALRT(A,B,ASCI,STRING)	IOLog( "UniNEnet: %8x %8x " STRING "\n", (unsigned int)(A), (unsigned int)(B) )
#define UC_ELG(A,B,ASCI,STRING)
#endif /* USE_ELG )) */

	typedef struct elg		/* Event Log flag and pointers	*/
	{
		UInt32		 evLogFlag; 	// debugging only
		UInt8		*evLogBuf;
		UInt8		*evLogBufe;
		UInt8		*evLogBufp;

		UInt32		startTimeSecs;	// Time buffer wrapped or init'd
		UInt32		physAddr;		// Physical address of buffer to dump via OF
		UInt32		alertCount;		// # times Alrt invoked - can be used as trigger
		UInt32		wrapCount;		// # of times buffer wrapped around
	};


#if USE_ELG
///#define READ_REGISTER(  REG )	OSReadLittleInt32(  (void*)&fpRegs->REG, 0 )
#define READ_REGISTER( REG )	(UInt32)(fCellClockEnabled ? OSReadLittleInt32( (void*)&fpRegs->REG, 0 ) :  Alrt( 0, 0, 'REG-', "regs unavail" ) )
#define WRITE_REGISTER( REG, VAL )	writeRegister( &fpRegs->REG, VAL )
#else
#define READ_REGISTER( REG ) OSReadLittleInt32( (void*)&fpRegs->REG, 0 )
#define WRITE_REGISTER( REG, VAL )	OSWriteLittleInt32( (void*)&fpRegs->REG, 0, VAL )
#endif // USE_ELG



#define NETWORK_STAT_ADD(  x )	(fpNetStats->x++)
#define ETHERNET_STAT_ADD( x )	(fpEtherStats->x++)


	typedef void	*IOPPCAddress;


	enum LinkStatus
	{
		kLinkStatusUnknown,
		kLinkStatusDown,
		kLinkStatusUp,
	};

	struct MediumTable
	{
		UInt32	type;
		UInt32	speed;
	};


#define kNumOfPowerStates		2
#define kUniNsettle_time		500		// guess 500 microseconds for settling

	/* Items in the IORegistry:	*/

#define kTxQueueSize		"TxPacketQueueSize"
#define kTxRingElements		"TxRingElements: 32,64,128,256,512,1024,2048,4096,8192"
#define kRxRingElements		"RxRingElements: 32,64,128,256,512,1024,2048,4096,8192"

	/* Performance tracepoints
	 *
	 * DBG_UniN_RXIRQ       - Receive  ISR run time
	 * DBG_UniN_TXIRQ       - Transmit ISR run time
	 * DBG_UniN_TXQUEUE     - Transmit packet passed from network stack
	 * DBG_UniN_TXCOMPLETE  - Transmit packet sent
	 * DBG_UniN_RXCOMPLETE  - Receive packet passed to network stack
	 */
#define DBG_UniN_ENET          0x0900
#define DBG_UniN_RXIRQ         DRVDBG_CODE( DBG_DRVNETWORK, (DBG_UniN_ENET+1) )
#define DBG_UniN_TXIRQ         DRVDBG_CODE( DBG_DRVNETWORK, (DBG_UniN_ENET+2) )
#define DBG_UniN_TXQUEUE       DRVDBG_CODE( DBG_DRVNETWORK, (DBG_UniN_ENET+3) )
#define DBG_UniN_TXCOMPLETE    DRVDBG_CODE( DBG_DRVNETWORK, (DBG_UniN_ENET+4) )
#define DBG_UniN_RXCOMPLETE    DRVDBG_CODE( DBG_DRVNETWORK, (DBG_UniN_ENET+5) )

	enum
	{
		kGMACUserClientCookie	= 123		// pass to IOServiceOpen
	};

	enum
	{		/* command bytes to send to the user client:	*/
		kGMACUserCmd_GetLog		= 0x30,		// get entire GMAC ELG buffer
		kGMACUserCmd_GetRegs	= 0x31,		// get GMAC registers
		kGMACUserCmd_GetPHY		= 0x32,		// get PHY  registers
		kGMACUserCmd_GetTxRing	= 0x33,		// get Tx DMA elements
		kGMACUserCmd_GetRxRing	= 0x34,		// get Rx DMA elements

		kGMACUserCmd_ReadAllMII	= 0x50,		// read MII registers 0 thru 31
		kGMACUserCmd_ReadMII	= 0x51,		// read one MII register
		kGMACUserCmd_WriteMII	= 0x52		// write one MII register
	};


class UniNEnet;


class UniNEnetUserClient : public IOUserClient
{
	OSDeclareDefaultStructors( UniNEnetUserClient )		;

private:
	UniNEnet			*fProvider;
	IOExternalMethod	fMethods[1];		// just one method
	task_t				fTask;

public:
    static UniNEnetUserClient*	withTask( task_t owningTask );	// Constructor

	virtual IOReturn	clientClose();
	virtual IOReturn	clientDied();

	virtual IOReturn 	registerNotificationPort( mach_port_t port, UInt32 type );	// not impl
	virtual IOReturn  	connectClient( IOUserClient *client );
	virtual IOExternalMethod*	getExternalMethodForIndex( UInt32 index );
	virtual bool		start( IOService *provider );

	IOReturn doRequest(		void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize );
	IOReturn getGMACLog(	void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize );
	IOReturn getGMACRegs(	void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize );
	IOReturn getGMACTxRing(	void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize );
	IOReturn getGMACRxRing(	void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize );

	IOReturn readAllMII(	void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize );
	IOReturn readMII(		void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize );
	IOReturn writeMII(		void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize );

};/* end class UniNEnetUserClient */



class UniNEnet: public IOEthernetController
{
	OSDeclareDefaultStructors( UniNEnet )

	/* Instance variables are public for UserClient availability:	*/

public:

	elg						*fpELG;			// pointer to ELG structure & buffer
	volatile GMAC_Registers	*fpRegs;		// pointer to GMAC's registers
	GMAC_Registers			*fpRegsPhys;	// for ml_probe_read
	IOPCIDevice				*nub;
	IOMemoryMap				*ioMapEnet;
	volatile IOPPCAddress	ioBaseEnet;

	IOEthernetInterface		*networkInterface;
	IOBasicOutputQueue		*transmitQueue;
	IOPacketQueue			*debugQueue;
	IOKernelDebugger		*debugger;

	IOWorkLoop				*workLoop;
	IOInterruptEventSource	*interruptSource;
	IOTimerEventSource		*timerSource;

	IONetworkStats			*fpNetStats;
	IOEthernetStats			*fpEtherStats;
	IOMbufBigMemoryCursor	*mbufCursor;

    OSDictionary			*fMediumDict;
	MediumTable				*fpgMediumTable;
	UInt32					fMediumTableCount;
	IOMediumType			fMediumType;	// current medium speed/duplex/...

	IOEthernetAddress	 	myAddress;

	bool		fBuiltin;			// UniN builtin = 1; PCI card = 0
	bool		fReady;
	bool		fWOL;				// WakeOnLAN desired.
	bool		fCellClockEnabled;
	bool		netifEnabled;
	bool		debugEnabled;
	bool		debugTxPoll;
	bool		fIsPromiscuous;
	bool		multicastEnabled;
	bool		isFullDuplex;

	UInt32 		phyType;			// misnomer - really both PHY ID registers
	UInt8		phyId;				// misnomer - really PHY address 00-1F or FF

	UInt32		fLinkStatus;
	UInt16		fPHYStatus;

	UInt16		fPHYControl;		// 16 bit PHY Control register.
	UInt32		fPHYType;			// 5400, 5401 or 5201 for PM

	UInt32		fTxQueueSize;				// size of the Tx queue
	UInt32		fTxRingElements;			// number of Tx ring elements
	UInt32		fRxRingElements;			// number of Rx ring elements

	mbuf		**fTxMbuf;	// array of Tx mBuf pointers; index of last segment of pkt -> mbuf
	mbuf		**fRxMbuf;	// array of Rx mBuf pointers
	mbuf		*txDebuggerPkt;

///	mbuf		*rxMbufBucket;
///	UInt32		*fRxBucketPhysAddr;			// Physical addr of bit bucket

	void		*debuggerPkt;
	UInt32		debuggerPktSize;

	UInt32		txCommandHead;  			// TX ring descriptor index
	UInt32		txCommandTail;
	UInt32		rxCommandHead;				// RX ring descriptor index
	UInt32		rxCommandTail;				/// rxCommandTail is not used anywhere

	TxDescriptor	*fTxDescriptorRing; 	// TX descriptor ring ptr
	UInt32			fTxDescriptorRingPhys;
	UInt32			fTxElementsAvail;
	UInt32			fTxRingLengthFactor;

	RxDescriptor	*fRxDescriptorRing;  	// Rx descriptor ring ptr
	UInt32			fRxDescriptorRingPhys;
	UInt32			fRxRingLengthFactor;

	UInt32		txIntCnt;
	UInt32		txRingIndexLast;
	UInt32		txWDInterrupts;
	UInt32		txWDCount;
    
	UInt32		rxWDInterrupts;
	UInt32		rxWDCount;

	UInt16		hashTableUseCount[ 256 ];
	UInt16		hashTableMask[ 16 ];

	UInt32		currentPowerState;	/* must be 0 or 1		*/

		/* Local copies of certain key registers:	*/

	UInt32		fConfiguration;
	UInt32		fXIFConfiguration;
	UInt32		fRxMACConfiguration;
	UInt32		fMACControlConfiguration;
	UInt32		fRxMACStatus;				// preserve auto-clear register.

private:			// Instance methods:
	bool		allocateMemory();
	bool		initTxRing();
	bool		initRxRing();
	void		flushRings( bool, bool );	// Flush Tx and Rx rings
	bool		initChip();
	void		setDuplexMode( bool duplexMode );
	void		startChip();
	void		stopChip();
	bool		genRxDescriptor( UInt32 index );
	void		monitorLinkStatus( bool firstPoll = false );
	bool		transmitPacket( struct mbuf *packet );
	bool		transmitInterruptOccurred();
	void		debugTransmitInterruptOccurred();
	void		debugTransmitCleanup();
	bool		receiveInterruptOccurred();
	bool		receivePackets( bool fDebugger );
	void		packetToDebugger( struct mbuf *packet, u_int size );
	void		restartReceiver();
	void		putToSleep( bool pangeaClockOnly );
	bool		wakeUp( bool pangeaClockOnly );
	void		sendDummyPacket();
	void		resetHashTableMask();
	void		addToHashTableMask( UInt8 *addr );
	void		removeFromHashTableMask( UInt8 *addr );
	void		updateHashTableMask();

	void		sendPacket( void *pkt, UInt32 pkt_len );
	void		receivePacket( void *pkt, UInt32 *pkt_len, UInt32 timeout );
	void		miiWrite( UInt32 miiData, UInt32 dataSize );
	bool		miiResetPHY( UInt8 phy );
	bool		miiWaitForAutoNegotiation( UInt8 phy );
//	void		miiRestartAutoNegotiation( UInt8 phy );
	bool		miiFindPHY( UInt8 *phy_num );
	bool		miiInitializePHY( UInt8 phy );

	UInt32		outputPacket( struct mbuf *m, void *param );

	void		interruptOccurred( IOInterruptEventSource *src, int count );
	void		timeoutOccurred( IOTimerEventSource *timer );
	bool		createMediumTables();

	void		writeRegister( volatile UInt32 *pReg, UInt32 data );

	void		getPhyType();
	void		stopPHY(); 
	void		startPHY();
	bool		hardwareResetPHY();

		// callPlatformFunction symbols
	const OSSymbol 	*keyLargo_resetUniNEthernetPhy;

	IOService	*keyLargo;

public:		// Override methods:

#ifdef USE_ELG
	void		AllocateEventLog( UInt32 size );
	void		EvLog( UInt32 a, UInt32 b, UInt32 ascii, char* str );
	UInt32		Alrt(  UInt32 a, UInt32 b, UInt32 ascii, char* str );

#endif // USE_ELG

	virtual bool		init( OSDictionary *properties = 0 );
	virtual bool		start(IOService *provider);
	virtual void		free();

	virtual bool 		createWorkLoop();
	virtual IOWorkLoop*	getWorkLoop() const;

	virtual IOReturn 	enable(  IONetworkInterface *netif );
	virtual IOReturn	disable( IONetworkInterface *netif );

	virtual IOReturn	setWakeOnMagicPacket( bool active );
	virtual IOReturn	getPacketFilters(	const OSSymbol	*group,
											UInt32			*filters ) const;

	virtual IOReturn	getHardwareAddress( IOEthernetAddress *addr );
	virtual IOReturn	setHardwareAddress( const IOEthernetAddress *addr );

	virtual IOReturn	setMulticastMode( IOEnetMulticastMode mode );
	virtual IOReturn	setMulticastList( IOEthernetAddress  *addrs, UInt32 count );

	virtual IOReturn         setPromiscuousMode( IOEnetPromiscuousMode mode );

	virtual IOOutputQueue*  createOutputQueue();
    
	virtual const OSString*	newVendorString()   const;
	virtual const OSString*	newModelString()    const;
	virtual const OSString* newRevisionString() const;

	virtual IOReturn	enable(  IOKernelDebugger *debugger );
	virtual IOReturn	disable( IOKernelDebugger *debugger );

	virtual bool		configureInterface( IONetworkInterface *netif );
	virtual IOReturn	getChecksumSupport(	UInt32				*checksumMask,
											UInt32				checksumFamily,
											bool				isOutput );

	virtual IOReturn 	selectMedium( const IONetworkMedium *medium );

	    // Power management methods:

	virtual IOReturn   registerWithPolicyMaker( IOService *policyMaker );
	virtual UInt32     maxCapabilityForDomainState(		IOPMPowerFlags state);
	virtual UInt32     initialPowerStateForDomainState(	IOPMPowerFlags state );
	virtual UInt32     powerStateForDomainState(		IOPMPowerFlags state );
	virtual IOReturn   setPowerState(	UInt32		powerStateOrdinal,
										IOService	*whatDevice );
										
		// UserClient public access methods:

	virtual IOReturn	newUserClient( task_t, void*, UInt32, IOUserClient** );
	bool				miiReadWord(  UInt16 *dataPtr, UInt16 reg, UInt8 phy );
    bool				miiWriteWord( UInt16 data,     UInt16 reg, UInt8 phy );

	IOReturn			negotiateSpeedDuplex( UInt16 controlReg );
	IOReturn			forceSpeedDuplex(     UInt16 controlReg );

};/* end class UniNEnet */
