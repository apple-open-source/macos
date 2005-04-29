/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#ifndef _BCM440X_H

//#define DEBUG 1
#ifdef  DEBUG
#define IOASSERT 1
#endif

#include <libkern/OSByteOrder.h>
#include <libkern/OSAtomic.h>
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
#include <IOKit/network/IOPacketQueue.h>
#include <IOKit/assert.h>
#include "BCM440XPHY.h"

__BEGIN_DECLS
#include <sys/kpi_mbuf.h>
#include "if_bfereg.h"
__END_DECLS

#ifdef  DEBUG
#define ERROR_LOG(fmt, args...)  IOLog(fmt, ## args)
#define DEBUG_LOG(fmt, args...)  IOLog(fmt, ## args)
#define RX_TX_LOG(fmt, args...)
#else
#define ERROR_LOG(fmt, args...)  IOLog(fmt, ## args)
#define DEBUG_LOG(fmt, args...)
#define RX_TX_LOG(fmt, args...)
#endif

#define NET_STAT(var, cnt) \
        do { (fNetStats->var) += (cnt); } while(0)

#define ETH_STAT(var, cnt) \
        do { (fEtherStats->var) += (cnt); } while(0)

/*
 * Adapter activation levels.
 */
enum {
    kActivationLevelNone = 0,  /* adapter shut off */
    kActivationLevelKDP,       /* adapter partially up to support KDP */
    kActivationLevelBSD        /* adapter fully up to support KDP and BSD */
};

/*
 * Periodic firing interval for the watchdog timer.
 */
enum {
    kWatchdogTimerPeriodMS = 4000
};

#define BCM440X AppleBCM440XEthernet
class BCM440X : public IOEthernetController
{
    OSDeclareDefaultStructors( AppleBCM440XEthernet )

public:
    IOPCIDevice *                fPCIDevice;
    IOWorkLoop *                 fWorkLoop;
    IOEthernetInterface *        fNetif;
    IOMemoryMap *                fRegMap;
    IOInterruptEventSource *     fInterruptSrc;
    IOTimerEventSource *         fWatchdogTimer;
    struct bfe_softc *           sc;
    IONetworkStats *             fNetStats;
    IOEthernetStats *            fEtherStats;
    UInt32                       fActivationLevel;
    IOEthernetAddress            fEnetAddr;
    bool                         fEnabledForBSD;
    bool                         fEnabledForKDP;
    bool                         fSelectMediumOverride;
    bool                         fInterruptEnabled;
    AppleBCM440XPHY *            fPHY;
    OSDictionary *               fMediaDict;
    IOPacketQueue *              fGarbageQueue;

    /* Receive */
    IOBufferMemoryDescriptor *   fRxDescMemory;
    IOPhysicalAddress            fRxDescPhysAddr;
    bfe_desc *                   fRxDescBase;
    mbuf_t *                     fRxPacketArray;
    UInt32                       fRxHeadIndex;

    /* Transmit */
    IOMbufNaturalMemoryCursor *  fTxMbufCursor;
    IOBufferMemoryDescriptor *   fTxDescMemory;
    IOPhysicalAddress            fTxDescPhysAddr;
    bfe_desc *                   fTxDescBase;
    mbuf_t *                     fTxPacketArray;
    UInt32                       fTxDescBusy;
    UInt32                       fTxHeadIndex;
    UInt32                       fTxTailIndex;
    UInt32                       fTxRingDelayIOC;
    IOOutputQueue *              fTransmitQueue;

    /* Kernel Debugging */
    mbuf_t                       fKDPMbuf;
    IOPhysicalSegment            fKDPMbufSeg;
    IOKernelDebugger *           fKDPNub;

    static void watchdogTimeout( OSObject *           owner,
                                 IOTimerEventSource * timer );

    static bool mdiRead( void * owner, UInt32 phyAddr, UInt16 phyReg,
                         UInt16 * phyData );

    static bool mdiWrite( void * owner, UInt32 phyAddr, UInt16 phyReg,
                          UInt16 phyData );

    bool initDriverObjects( IOService * provider );
    void initPCIConfigSpace( IOPCIDevice * pci );
    bool allocateTxMemory( void );
    void releaseTxMemory( void );
    void freeTxRingPackets( void );
    bool initTxRing( void );
    bool allocateRxMemory( void );
    void releaseRxMemory( void );
    void freeRxRingPackets( void );
    bool initRxRing( void );
    void initRxFilter( void );
    bool publishMediaSupport( void );
    void reportLinkStatus( void );
    void interruptOccurred( IOInterruptEventSource * src, int count );
    bool setActivationLevel( UInt32 inLevel );
    bool increaseActivationLevel( UInt32 inLevel );
    bool decreaseActivationLevel( UInt32 inLevel );
    bool resetCurrentActivationLevel( void );
    void serviceRxInterrupt( void );
    bool updateRxDescriptor( UInt32 index );
    void serviceTxInterrupt( void );

public:
    virtual IOOutputQueue * createOutputQueue( void );

    virtual IOWorkLoop *    getWorkLoop( void ) const;

    virtual bool     createWorkLoop( void );

    virtual UInt32   outputPacket( mbuf_t packet, void * param );

    virtual bool     start( IOService * provider );

    virtual void     free( void );

    virtual IOReturn getHardwareAddress( IOEthernetAddress * addr );

    virtual IOReturn enable( IONetworkInterface * netif );

    virtual IOReturn disable( IONetworkInterface * netif );

    virtual IOReturn enable( IOKernelDebugger * debugger );

    virtual IOReturn disable( IOKernelDebugger * debugger );

    virtual bool     configureInterface( IONetworkInterface * interface );

    virtual IOReturn selectMedium( const IONetworkMedium * medium );

    virtual IOReturn setPromiscuousMode( bool active );

    virtual IOReturn setMulticastMode( bool active );

    virtual IOReturn setMulticastList( IOEthernetAddress * mcAddrList,
                                       UInt32              mcAddrCount );

    virtual const OSString * newVendorString( void ) const;

    virtual const OSString * newModelString( void ) const;

    virtual IOReturn registerWithPolicyMaker( IOService * policyMaker );

    virtual IOReturn setPowerState( unsigned long powerStateOrdinal,
                                    IOService *   policyMaker );

    virtual void     receivePacket( void *   pkt_data,
                                    UInt32 * pkt_size,
                                    UInt32   timeoutMS );

    virtual void     sendPacket( void * pkt_data, UInt32 pkt_size );
};

#endif /* !_BCM440X_H */
