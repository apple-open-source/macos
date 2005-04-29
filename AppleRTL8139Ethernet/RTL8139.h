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
#include <libkern/OSByteOrder.h>
extern "C" {
#include <sys/kpi_mbuf.h>
}
#include "RTL8139HW.h"

#ifdef  DEBUG
#warning ***************************************************
#warning ** DEBUG defined - turn off for deployment build **
#warning ***************************************************
#define DEBUG_LOG(args...)  kprintf(args)
#else
#define DEBUG_LOG(args...)
#endif

#define BUMP_NET_COUNTER(x) \
        do { netStats->x += 1; } while(0)

#define BUMP_ETHER_COUNTER(x) \
        do { etherStats->dot3StatsEntry.x += 1; } while(0)

#define BUMP_ETHER_RX_COUNTER(x) \
        do { etherStats->dot3RxExtraEntry.x += 1; } while(0)

#define BUMP_ETHER_TX_COUNTER(x) \
        do { etherStats->dot3TxExtraEntry.x += 1; } while(0)

#define kWatchdogTimerPeriod    4000  // milliseconds
#define kTxBufferCount          4     // 4 TX buffers/descriptors
#define kTransmitQueueCapacity  384
#define MINPACK                 60    // minimum output packet length 
#define MAXPACK                 1514  // maximum output packet length
#define RX_BUF_SIZE             (1024 * 16)
#define TX_BUF_SIZE             (1024 * 2)

/*
 * Header prepended to each rx packet.
 */
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

#define RTL8139 com_apple_driver_RTL8139

class RTL8139 : public IOEthernetController
{
	OSDeclareDefaultStructors( com_apple_driver_RTL8139 )

protected:
    IOEthernetInterface	*      netif;
    IOPCIDevice *              pciNub;
    IOWorkLoop *               workLoop;
	IOInterruptEventSource *   interruptSrc;
	IOOutputQueue *            transmitQueue;
	IOTimerEventSource *       timerSrc;
	IOKernelDebugger *         debugger;
	IONetworkStats *           netStats;
	IOEthernetStats *          etherStats;
	IOMemoryMap *              csrMap;
    volatile void *            csrBase;
	OSDictionary *             mediumDict;
	const IONetworkMedium *    mediumTable[MEDIUM_INDEX_COUNT];

    IOBufferMemoryDescriptor * rx_md;
    UInt8 *                    rx_buf_start;
    UInt8 *                    rx_buf_end;
    UInt8 *                    rx_buf_ptr;
    IOPhysicalAddress		   rx_buf_phys;

    IOBufferMemoryDescriptor * tx_md[kTxBufferCount];
    UInt8 *                    tx_buf_ptr[kTxBufferCount];
    IOPhysicalAddress		   tx_buf_phys[kTxBufferCount];
    UInt8			           tx_buf_ownership[4];
    volatile bool              tx_buf_reclaimed;
    UInt8			           tx_send_index;
    UInt8                      tx_send_count;
    UInt8			           tx_ack_index;
 
    enum {
        kOwnedByChip = 0,
        kOwnedByHost = 1
    };
 
    UInt8			           reg_config1;
    UInt32                     reg_rcr;
	UInt32                     currentLevel;
	bool                       enabledByBSD;
    bool                       enabledByKDP;
	bool                       interruptEnabled;
	MediumIndex                currentMediumIndex;
    UInt16                     phyStatusLast;
    UInt16                     reg_bms;
    UInt32                     reg_mar0;
    UInt32                     reg_mar4;

    void restartReceiver( void );

    void transmitterInterrupt( bool * reclaimed );

    void receiverInterrupt( bool * queued );

	void interruptOccurred( IOInterruptEventSource * src, int count );

	bool initEventSources( IOService * provider );

    bool initPCIConfigSpace( IOPCIDevice * provider );

	bool enableAdapter( UInt32 level );

	bool disableAdapter( UInt32 level );

    enum {
        kActivationLevel0 = 0,
        kActivationLevel1,
        kActivationLevel2
    };

	bool setActivationLevel( UInt32 newLevel );

	void timeoutOccurred( IOTimerEventSource * timer );

    bool allocateDescriptorMemory( void );

    enum {
        kFullInitialization = 0,
        kResetChip          = 1
    };

	bool initAdapter( IOOptionBits options );

	void disableHardwareInterrupts( void );

	void enableHardwareInterrupts( void );

    void reclaimTransmitBuffer( void );

    // PHY functions

	bool phyAddMediumType( IOMediumType type, UInt32 bps, MediumIndex index );

	void phyProbeMediaCapability( void );

	bool phyReset( void );

	bool phyWaitForAutoNegotiation( void );

	bool phySetMedium( MediumIndex mediumIndex );

    bool phySetMedium( const IONetworkMedium * medium );

    void phyReportLinkStatus( bool forceStatusReport = false );

	const IONetworkMedium * phyGetMediumWithIndex( MediumIndex index ) const;

    // Access hardware registers

	inline void csrWrite32( UInt16 offset, UInt32 value )
    { pciNub->ioWrite32(offset, value, csrMap); }

    inline void csrWrite16( UInt16 offset, UInt16 value )
    { pciNub->ioWrite16(offset, value, csrMap); }

    inline void csrWrite8(  UInt16 offset, UInt8 value )
	{ pciNub->ioWrite8(offset, value, csrMap); }

	inline void csrWrite32Slow( UInt16 offset, UInt32 value )
    { pciNub->ioWrite32(offset, value, csrMap); pciNub->ioRead32(offset, csrMap); }

    inline void csrWrite16Slow( UInt16 offset, UInt16 value )
    { pciNub->ioWrite16(offset, value, csrMap); pciNub->ioRead16(offset, csrMap); }

    inline void csrWrite8Slow(  UInt16 offset, UInt8 value )
	{ pciNub->ioWrite8(offset, value, csrMap); pciNub->ioRead8(offset, csrMap); }

    inline UInt32 csrRead32( UInt16 offset )
    { return pciNub->ioRead32(offset, csrMap); }

    inline UInt16 csrRead16( UInt16 offset )
    { return pciNub->ioRead16(offset, csrMap); }

    inline UInt8  csrRead8(  UInt16 offset )
    { return pciNub->ioRead8(offset, csrMap); }

public:
    virtual bool init( OSDictionary * properties );

	virtual bool start( IOService * provider );

	virtual void stop( IOService * provider );

	virtual void free( void );

	virtual IOReturn enable( IONetworkInterface * netif );

	virtual IOReturn disable( IONetworkInterface * netif );

	virtual IOReturn enable( IOKernelDebugger * netif );

	virtual IOReturn disable( IOKernelDebugger * netif );

	virtual UInt32 outputPacket( mbuf_t m, void * param );

	virtual void getPacketBufferConstraints(
                 IOPacketBufferConstraints * constraints ) const;

	virtual IOOutputQueue * createOutputQueue( void );

	virtual const OSString * newVendorString( void ) const;

	virtual const OSString * newModelString( void ) const;

	virtual IOReturn selectMedium( const IONetworkMedium * medium );

	virtual bool configureInterface( IONetworkInterface * interface );

    virtual bool createWorkLoop( void );

    virtual IOWorkLoop * getWorkLoop( void ) const;

	virtual IOReturn getHardwareAddress( IOEthernetAddress * addr );

	virtual IOReturn setPromiscuousMode( bool enabled );

	virtual IOReturn setMulticastMode( bool enabled );

	virtual IOReturn setMulticastList( IOEthernetAddress * addrs,
                                       UInt32              count );

    virtual void sendPacket( void * pkt, UInt32 pkt_len );

    virtual void receivePacket( void * pkt, UInt32 * pkt_len, UInt32 timeout );

    virtual IOReturn registerWithPolicyMaker( IOService * policyMaker );

    virtual IOReturn setPowerState( unsigned long powerStateOrdinal,
                                    IOService *   policyMaker );
};

#endif /* !_RTL8139_H */
