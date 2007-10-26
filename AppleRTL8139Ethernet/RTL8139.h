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
 * Copyright (c) 2001 Realtek Semiconductor Corp.
 *
 * rtl8139.h
 *
 * HISTORY
 *
 * 9-Jul-01	Owen Wei at Realtek Semiconductor Corp. created for Realtek
 *		RTL8139 family NICs.
 *		
 */

#ifndef _RTL8139_H
#define _RTL8139_H

#include <IOKit/assert.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOGatedOutputQueue.h>
#include <IOKit/network/IOMbufMemoryCursor.h>
#include <IOKit/IOUserClient.h>
#include <libkern/OSByteOrder.h>
extern "C" {
#include <sys/kpi_mbuf.h>
}
#include "RTL8139HW.h"


#define RTL8139		com_apple_driver_RTL8139


#ifdef  DEBUG
#warning ***************************************************
#warning ** DEBUG defined - turn off for deployment build **
#warning ***************************************************
#define DEBUG_LOG(args...)  kprintf(args)
#else
#define DEBUG_LOG(args...)
#endif


#define USE_ELG		0				// for debugging

#define kEvLogSize	(4096*16)		// 16 pages = 64K = 4096 events 0x10000
//#define kEvLogSize	(4096*1)		// 1 page = 4K = 256 events

#if USE_ELG /* (( */
#define ELG(A,B,ASCI,STRING)	EvLog( (UInt32)(A), (UInt32)(B), (UInt32)(ASCI), STRING )
#define ALRT(A,B,ASCI,STRING)	Alrt(  (UInt32)(A), (UInt32)(B), (UInt32)(ASCI), STRING )
#define UC_ELG(A,B,ASCI,STRING)	fProvider->EvLog( (UInt32)(A), (UInt32)(B), (UInt32)(ASCI), STRING )
	void   EvLog( UInt32 a, UInt32 b, UInt32 ascii, char* str );
	UInt32 Alrt(  UInt32 a, UInt32 b, UInt32 ascii, char* str );
#warning *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***
#warning     The USE_ELG debugging facility is enabled.      *** *** *** *** *** *** *** *** *** *** ***
#warning *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***
#else /* ) not USE_ELG: (	*/
#define ELG(A,B,ASCI,S)
#define ALRT(A,B,ASCI,STRING)	IOLog( "{ %8x %8x} " STRING "\n", (unsigned int)(A), (unsigned int)(B) )
#define UC_ELG(A,B,ASCI,STRING)
#endif /* USE_ELG )) */

	typedef struct elg		/* Event Log flag and pointers	*/
	{
		UInt32		 evLogFlag; 	// debugging only
		UInt8		*evLogBuf;
		UInt8		*evLogBufe;
		UInt8		*evLogBufp;

		UInt32		startTimeSecs;	// Time buffer wrapped or init'd
		UInt32		lostEvents;		// # events not logged because evLogFlag was 0
		UInt32		wrapCount;		// # of times buffer wrapped around
		UInt32		alertCount;		// # times Alrt invoked - can be used as trigger

		addr64_t	physAddr;		// 64 bit physical address
		UInt32		reserved0x38;	// reserved
		UInt32		lastAlrt;		// reserved
	};


#ifdef CRAP
#if USE_ELG
#define READ_REGISTER( REG )	(UInt32)(fCellClockEnabled ? OSReadLittleInt32( (void*)fpRegs, offsetof( Rltk_Registers, REG ) ) :  Alrt( 0, offsetof( Rltk_Registers, REG ), 'REG-', "RTL8139: regs unavail" ) )
#define WRITE_REGISTER( REG, VAL )	writeRegister( &fpRegs->REG, VAL )
#else
#define READ_REGISTER( REG ) OSReadLittleInt32( (void*)fpRegs, offsetof( Rltk_Registers, REG ) )
#define WRITE_REGISTER( REG, VAL )	OSWriteLittleInt32( (void*)fpRegs, offsetof( Rltk_Registers, REG ), VAL )
#endif // USE_ELG
#endif // CRAP

#define BUMP_NET_COUNTER(x)			do { netStats->x += 1; } while(0)
#define BUMP_ETHER_COUNTER(x)		do { etherStats->dot3StatsEntry.x += 1; } while(0)
#define BUMP_ETHER_RX_COUNTER(x)	do { etherStats->dot3RxExtraEntry.x += 1; } while(0)
#define BUMP_ETHER_TX_COUNTER(x)	do { etherStats->dot3TxExtraEntry.x += 1; } while(0)

#define kWatchdogTimerPeriod    1000  // milliseconds
#define kTxBufferCount          4     // 4 TX buffers/descriptors
#define kTransmitQueueCapacity  256
#define MINPACK                 64    // minimum output packet length with 4 FCS bytes
#define MAXPACK                 1518  // maximum output packet length with 4 FCS bytes
#define RX_BUF_SIZE             (1024 * 64)	// max out the Rx buffer size
#define TX_BUF_SIZE             (1024 * 2)

		/* Header prepended to each rx packet: */
	struct rbuf_hdr_t
	{
#if defined( __BIG_ENDIAN__ ) // FIXME
		UInt16 rb_count;   // Receive byte count = packet length + 14  + 4
		UInt16 rb_status;  // Receive status
#elif defined( __LITTLE_ENDIAN__ )
		UInt16 rb_status;  // Receive status
		UInt16 rb_count;   // Receive byte count = packet length + 14  + 4
#else
#error Unknown machine endianess
#endif
	};


	enum
	{		/* command values to send to the user client:	*/
		kSelectLoopbackMAC			= 0x20,
		kSelectLoopbackPHY			= 0x21,

		kRltkUserCmd_GetLog			= 0x30,		// get entire Rltk ELG buffer
		kRltkUserCmd_GetRegs		= 0x31,		// get all Rltk registers
		kRltkUserCmd_GetOneReg		= 0x32,		// get one particular Rltk register
		kRltkUserCmd_WriteOneReg	= 0x35,		// write one particular Rltk register
	};

	struct UCRequest				/* User Client Request structure:	*/
	{
		UInt32		reqID;			/* one of the kRltkUserCmd_ requests enum'd above	*/
		UInt8*		pLogBuffer;		/* pointer to user's log buffer						*/
		UInt32		bufSize;		/* size of the log buffer							*/
	};


	class RTL8139;


class RTL8139UserClient : public IOUserClient
{
	OSDeclareDefaultStructors( RTL8139UserClient )		;

private:
	RTL8139				*fProvider;
	IOExternalMethod	fMethods[1];		// just one method
	task_t				fTask;
	IOMemoryMap			*fMap;				// memory map of the client's buffer

public:

    static RTL8139UserClient*	withTask( task_t owningTask );	// Constructor

	virtual IOReturn	clientClose();
	virtual IOReturn	clientDied();

	virtual IOReturn 	registerNotificationPort( mach_port_t port, UInt32 type );	// not impl
	virtual IOReturn  	connectClient( IOUserClient *client );
	virtual IOExternalMethod*	getExternalMethodForIndex( UInt32 index );
	virtual bool		start( IOService *provider );

private:

	IOReturn doRequest(			void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize );
	IOReturn getRltkLog(		void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize );
	IOReturn getRltkRegs(		void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize );
	IOReturn writeOneRltkReg(	void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize );

};/* end class RTL8139UserClient */


class RTL8139 : public IOEthernetController
{
	OSDeclareDefaultStructors( RTL8139 )	;

public:

	elg							*fpELG;			// pointer to ELG structure & buffer
	IOBufferMemoryDescriptor	*fpELGMemDesc;	// memory descriptor of ELG structure & buffer

///protected:	User Client doesn't like some protected fields and methods.
    IOEthernetInterface	*      netif;
    IOPCIDevice *              pciNub;
    IOWorkLoop *               workLoop;
	IOInterruptEventSource *   interruptSrc;
	IOOutputQueue *            fTransmitQueue;
	IOTimerEventSource *       timerSrc;
	IOKernelDebugger *         debugger;
	IONetworkStats *           netStats;
	IOEthernetStats *          etherStats;
	IOMemoryMap *              csrMap;
    volatile void *            csrBase;
	OSDictionary *             mediumDict;
	const IONetworkMedium *    mediumTable[ MEDIUM_INDEX_COUNT ];

    IOBufferMemoryDescriptor	*fpTxRxMD;
	IOPhysicalAddress			fTxRxPhysical;
    UInt8						*fpTxRxBuffers;

	UInt8						fTxBufOwnership[ kTxBufferCount ];
    volatile bool				fTxBufReclaimed;
    UInt8						fTxSendIndex, fTxAckIndex;
	UInt8						fTxSendCount;
	UInt32						fTSD_ERTXTH;	// Tx early threshold

	IOPhysicalAddress			fRxBufferPhys;
    UInt8						*fpRxBuffer;
	UInt16						fRxOffset;

    enum
	{
        kOwnedByChip = 0,
        kOwnedByHost = 1
    };

    UInt8		reg_config1;
    UInt32		reg_rcr;
	UInt32		currentLevel;
	bool		enabledByBSD;
    bool		enabledByKDP;
	bool		interruptEnabled;
	bool		forceLinkChange;
	bool		fSpeed100;
	bool		fPauseSupported;
	MediumIndex	currentMediumIndex;
    UInt16		phyStatusLast;
    UInt16		reg_bms;
    UInt32		reg_mar0;
    UInt32		reg_mar4;

    void	restartReceiver();
    void	transmitterInterrupt( bool* reclaimed );
    void	receiverInterrupt( bool* queued );
	void	interruptOccurred( IOInterruptEventSource* src, int count );
	bool	initEventSources( IOService* provider );
    bool	initPCIConfigSpace( IOPCIDevice* provider );
	bool	enableAdapter(  UInt32 level );
	bool	disableAdapter( UInt32 level );

    enum
	{
        kActivationLevel0 = 0,
        kActivationLevel1,
        kActivationLevel2
    };

	bool setActivationLevel( UInt32 newLevel );
	void timeoutOccurred( IOTimerEventSource * timer );
    bool allocateDescriptorMemory();

    enum
	{
        kFullInitialization = 0,
        kResetChip          = 1
    };

	bool initAdapter( IOOptionBits options );
	void disableHardwareInterrupts();
	void enableHardwareInterrupts();
    void reclaimTransmitBuffer();

		// PHY functions

	bool phyAddMediumType( IOMediumType type, UInt32 bps, MediumIndex index );
	void phyProbeMediaCapability();
	bool phyReset();
	bool phyWaitForAutoNegotiation();
	bool phySetMedium( MediumIndex mediumIndex );
    bool phySetMedium( const IONetworkMedium * medium );
    void phyReportLinkStatus();
	const IONetworkMedium * phyGetMediumWithIndex( MediumIndex index ) const;

	inline void csrWrite32( UInt16 offset, UInt32 value )
    {
		ELG( value, offset, 'Wr32', "csrWrite32" );
		OSWriteLittleInt32( csrBase, offset, value );
	}/* end csrWrite32 */

    inline void csrWrite16( UInt16 offset, UInt16 value )
    {
		ELG( value, offset, 'Wr16', "csrWrite16" );
		OSWriteLittleInt16( csrBase, offset, value );
	}/* end csrWrite16 */

    inline void csrWrite8(  UInt16 offset, UInt8 value )
	{
		ELG( value, offset, 'Wr08', "csrWrite8" );
		*((UInt8*)csrBase + offset) = value;
	}/* end csrWrite8 */

    inline UInt32 csrRead32( UInt16 offset )
    { return OSReadLittleInt32( csrBase, offset ); }

    inline UInt16 csrRead16( UInt16 offset )
    { return OSReadLittleInt16( csrBase, offset ); }

    inline UInt8 csrRead8( UInt16 offset )
    {  return *((UInt8*)csrBase + offset); }

public:
	UInt32		fLoopback;		// TRUE if in loopback
	UInt32		fLoopbackMode;	// kSelectLoopbackMAC, or kSelectLoopbackPHY

#ifdef USE_ELG
	void		AllocateEventLog( UInt32 size );
	void		EvLog( UInt32 a, UInt32 b, UInt32 ascii, char* str );
	UInt32		Alrt(  UInt32 a, UInt32 b, UInt32 ascii, char* str );
#endif // USE_ELG

    virtual bool init( OSDictionary *properties );
	virtual bool start( IOService *provider );
	virtual void stop(  IOService *provider );
	virtual void free();
	virtual IOReturn	enable(  IONetworkInterface *netif );
	virtual IOReturn	disable( IONetworkInterface *netif );
	virtual IOReturn	enable(  IOKernelDebugger   *netif );
	virtual IOReturn	disable( IOKernelDebugger   *netif );
    virtual bool		setLinkStatus(	UInt32					status,
										const IONetworkMedium	*activeMedium = 0,
										UInt64					speed = 0,
										OSData					*data = 0 );
	virtual UInt32	outputPacket( mbuf_t m, void *param );
	virtual void	getPacketBufferConstraints(
							IOPacketBufferConstraints *constraints ) const;
	virtual IOOutputQueue* createOutputQueue();
	virtual const OSString* newVendorString() const;
	virtual const OSString* newModelString() const;
	virtual IOReturn	selectMedium( const IONetworkMedium *medium );
	virtual bool		configureInterface( IONetworkInterface *interface );
    virtual bool		createWorkLoop();
    virtual IOWorkLoop*	getWorkLoop() const;
	virtual IOReturn	getHardwareAddress( IOEthernetAddress *addr );
	virtual IOReturn	setPromiscuousMode( bool enabled );
	virtual IOReturn	setMulticastMode( bool enabled );
	virtual IOReturn	setMulticastList( IOEthernetAddress *addrs, UInt32 count );
    virtual void		sendPacket( void *pkt, UInt32 pktLen );
    virtual void		receivePacket( void * pkt, UInt32 * pktLen, UInt32 timeout );
    virtual IOReturn	registerWithPolicyMaker( IOService *policyMaker );
    virtual IOReturn	setPowerState( unsigned long powerStateOrdinal,
                                    IOService *policyMaker );
	virtual IOReturn	newUserClient( task_t, void*, UInt32, IOUserClient** );

		/*	EEPROM stuff:	*/

	void	registerEEPROM();
	void	outCommand( UInt16 cmd );
//	void	outEEPROM16( int location, UInt16 value );
	int		inEEPROM16( int location );
	void	outBit( UInt16 bit );
	int		inBit();
	void	write9346CR( UInt8 value );

};/* end class RTL8139 */

#endif /* !_RTL8139_H */
