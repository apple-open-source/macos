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

#ifndef __APPLE_3COM_3C90X_H
#define __APPLE_3COM_3C90X_H

#include <IOKit/assert.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOOutputQueue.h>
#include <IOKit/network/IOMbufMemoryCursor.h>
#include <IOKit/network/IOPacketQueue.h>
#include <IOKit/network/IOGatedOutputQueue.h>
#include <IOKit/network/IONetworkMedium.h>

#include "3C90xDefines.h"
#include "3C90xInline.h"
#include "3C90xDebug.h"

extern "C" {
#include <sys/param.h>
#include <sys/mbuf.h>
#include <string.h>
}

class Apple3Com3C90x : public IOEthernetController
{
    OSDeclareDefaultStructors( Apple3Com3C90x )

public:
    IOEthernetInterface *       _netif;
    IOEthernetAddress           _etherAddress;
	IOInterruptEventSource *    _interruptSrc;
	IOTimerEventSource *        _timerSrc;
	IOOutputQueue *             _transmitQueue;
    IOPCIDevice *               _pciDevice;
    IOWorkLoop *                _workLoop;
    IOMbufLittleMemoryCursor *  _mbufCursor;
	IOKernelDebugger *          _debugger;

    UInt16                      _ioBase;
    UInt8                       _rxFilterMask;
    bool                        _netifEnabled;
    SInt32                      _driverEnableCount;
    volatile UInt16             _interruptMask;
    const AdapterInfo *         _adapterInfo;
	IONetworkStats *            _netStats;
	IOEthernetStats *           _etherStats;
    OSDictionary *              _mediumDict;
    UInt8                       _window;
    UInt32                      _asicType;

    // User configurable settings.

    UInt32                      _txIntThreshold;
    bool                        _flowControlEnabled;
    bool                        _extendAfterCollision;
    bool                        _storeAndForward;
    bool                        _linkMonitorTimerEnabled;
    bool                        _watchdogTimerEnabled;

    // Transmit and receive thresholds.

    UInt16                      _dnBurstThresh;
    UInt16                      _dnPriorityThresh;
    UInt16                      _txReclaimThresh;
    UInt16                      _txStartThresh_10;
    UInt16                      _txStartThresh_100;
    UInt16                      _upBurstThresh;
    UInt16                      _upPriorityThresh;

    // Transmit watchdog facility.

    enum {
        kWDInterruptsPending,
        kWDInterruptsRetired,
        kWDCounters
    };

    UInt32                      _oldWDCounters[kWDCounters];
    UInt32                      _newWDCounters[kWDCounters];

    // Descriptor rings.

    UInt32                      _rxRingSize;
    MemoryRange                 _rxRingMem;
    RxDescriptor *              _rxRing;
    RxDescriptor *              _rxRingTail;
    bool                        _rxRingInited;

    UInt32                      _txRingSize;
    MemoryRange                 _txRingMem;
    TxDescriptor *              _txRing;
    TxDescriptor *              _txRingHead;
    TxDescriptor *              _txRingTail; 
    UInt32                      _txRingFree;
    UInt32                      _txRingInt;
    bool                        _txRingInited;

    // Media support.

    MediaPort                   _defaultPort;
    MediaState                  _media;
    bool                        _linkTest;
    UInt16                      _mediaOptions;

    // Kernel debugger support.

    struct mbuf *               _kdpMbuf;

    // Superclass overrides.

    virtual bool start( IOService * provider );

    virtual void free();

    virtual bool createWorkLoop();

    virtual IOWorkLoop * getWorkLoop() const;

	virtual IOReturn getHardwareAddress( IOEthernetAddress * addr );

	virtual IOReturn setPromiscuousMode( bool enable );

	virtual IOReturn setMulticastMode( bool enable );

	virtual IOReturn setMulticastList( IOEthernetAddress * addrs,
                                       UInt32              count );

    virtual UInt32 outputPacket( struct mbuf * m, void * param );

    virtual IOOutputQueue * createOutputQueue();

    virtual IOReturn enable( IONetworkInterface * netif );

    virtual IOReturn disable( IONetworkInterface * netif );

    virtual IOReturn enable( IOKernelDebugger * debugger );

    virtual IOReturn disable( IOKernelDebugger * debugger );

    virtual IOReturn selectMedium(const IONetworkMedium * medium);

    virtual bool configureInterface( IONetworkInterface * netif );

	// Driver defined.

    bool createSupportObjects( IOService * provider );

    void initPCIConfigSpace();

	void getDriverSettings();

	bool checkEEPROMChecksum();

	bool parseEEPROM();

	bool resetAndEnable( bool enable );

	void setRunning( bool running );

    void resetAdapter();

    bool resetAndEnableAdapter( bool enableIRQ = true );

    bool initRxRing();

    bool initTxRing();

    bool initAdapter();

    void initTransmitterParameters();

    void initReceiverParameters();

    void enableTransmitter();

    void enableReceiver();

    void enableAdapter( bool enableIRQ = true );

    void interruptHandler( IOInterruptEventSource * src );

    void transmitInterruptHandler();

    void receiveInterruptHandler();

    void transmitErrorInterruptHandler();

    void hostErrorInterruptHandler();

    void linkEventInterruptHandler();

    void updateStatsInterruptHandler();

    void enableAdapterInterrupts();

    void disableAdapterInterrupts();

    void disableLinkEventInterrupt();

    void enableLinkEventInterrupt();

    void startPeriodicTimer();

    void stopPeriodicTimer();

    void timeoutHandler( IOTimerEventSource * src );

    bool allocateDescMemory( MemoryRange * mem, UInt32 size );

    void freeDescMemory( MemoryRange * mem );

	bool allocateMemory();

    void setupMulticastHashFilter( IOEthernetAddress * addrs,
                                   UInt32              count );

	bool updateRxDescriptor( RxDescriptor * descriptor,
                             struct mbuf *  pkt );

	bool updateTxDescriptor( TxDescriptor * descriptor,
                             struct mbuf *  pkt );

    // MII/PHY support.

    bool       phyProbe( PHYAddress addr );

    bool       miiReadWord( PHYRegAddr  reg,
                            PHYAddress   addr,
                            PHYWord *    data = 0 );

	void       miiWriteWord( PHYAddress  phy,
                             PHYRegAddr reg,
                             PHYWord     data );

	void       physicalMgmtWriteWord( UInt32 word, UInt8 bits );

    UInt8      physicalMgmtReadBit();

    bool       phyReset( PHYAddress phy );
    
    bool       phyWaitForValidLink( PHYAddress phy );
    
    bool       phyWaitForNegotiation( PHYAddress phy );

    UInt32     phyGetSupportedLinks( PHYAddress phy );

    MIILink    phyGetBestNegotiatedLink( PHYAddress phy,
                                         MIILink    phyLink,
                                         MIILink    reportedLink,
                                         PHYWord *  reportedStatus );

	UInt32     phyGetIdentifier( PHYAddress phy );

    bool       phyForceMIILink( PHYAddress phy, MIILink link );

    bool       phyStartNegotiation( PHYAddress phy );

	// Kernel debugger support.

    virtual void sendPacket( void * pkt, UInt32 pkt_len );

    virtual void receivePacket( void *   pkt,
                                UInt32 * pkt_len,
                                UInt32   timeout );

	// MAC controller.

    UInt8 	setRegisterWindow( UInt8 newWindow );

    UInt8   hashMulticastAddress( UInt8 * Address );
    
    void    setStationAddress( const IOEthernetAddress * addr );

    void    sendCommand( UInt16 cmd, UInt16 arg = 0 );

    void    sendCommandWait( UInt16 cmd, UInt16 arg = 0 );

	void    selectTransceiverPort( MediaPort port );

    void    waitForTransmitterIdle();
    
    UInt16  readEEPROM( UInt8 offset );
    
    void    getStationAddress( IOEthernetAddress * addr );

    // Media support.

    void       resetMedia( const IONetworkMedium * medium = 0 );

    void       probeMediaSupport();

    bool       autoSelectMediaPort();

    void       monitorLinkStatus();

    bool       testLoopBack( MediaPort port );

    bool       testLinkStatus();

    bool       testNwayMIIPort();

    void       transmitTestFrame();

    bool       receiveTestFrame( UInt32 timeoutMS );

    void       flushReceiveRing();

    bool       checkMediaPortSupport( MediaPort  mediaPort,
                                      DuplexMode duplexMode );

    bool       mapSelectionToMIIMediaPort();

    bool       configurePHY();
    
    bool       setMediaRegisters();
    
    MIILink    getMIILinkFromMediaPort( MediaPort  mediaPort,
                                        DuplexMode duplexMode );

    MediaPort  getMediaPortFromMIILink( MIILink miiLink );

    DuplexMode getDuplexModeFromMIILink( MIILink miiLink );

    void       setLinkSpeed( LinkSpeed speed );
    
    void       setDuplexMode( DuplexMode mode );

    UInt32     getIOMediumType( MediaPort  mediaPort,
                                DuplexMode duplexMode );

    bool       addMediaPort( OSDictionary * mediaDict,
                             MediaPort      mediaPort,
                             DuplexMode     duplexMode );

    bool       publishMediaCapability( OSDictionary * mediaDict );

    /*
     * Inline hardware register access functions.
     */
    DefineGlobalRegisterAccessors( 16, w,    0x0e, CommandStatus )
    DefineWindowRegisterAccessors( 16, w, 0, 0x0a, EEPROMCommand )
    DefineWindowRegisterAccessors( 16, w, 0, 0x0c, EEPROMData )
    DefineWindowRegisterAccessors( 32, l, 3, 0x00, InternalConfig )
    DefineWindowRegisterAccessors( 16, w, 3, 0x08, MediaOptions )
    DefineWindowRegisterAccessors( 16, w, 2, 0x0c, ResetOptions )
    DefineWindowRegisterAccessors( 16, w, 4, 0x06, NetworkDiagnostic )
    DefineWindowRegisterAccessors( 16, w, 3, 0x06, MacControl )
    DefineWindowRegisterAccessors( 16, w, 4, 0x0a, MediaStatus )
    DefineWindowRegisterAccessors( 16, w, 4, 0x08, PhysicalMgmt )
    DefineGlobalRegisterAccessors(  8, b,    0x1b, TxStatus )
    DefineGlobalRegisterAccessors( 32, l,    0x38, UpListPtr )
    DefineGlobalRegisterAccessors( 32, l,    0x24, DnListPtr )
    DefineGlobalRegisterAccessors(  8, b,    0x2f, TxFreeThresh )
    DefineGlobalRegisterAccessors( 32, l,    0x20, DMACtrl )    
    DefineWindowRegisterAccessors( 16, w, 5, 0x00, TxStartThresh )

    // 3C90xB only
    
    DefineGlobalRegisterAccessors(  8, b,    0x2a, DnBurstThresh )
    DefineGlobalRegisterAccessors(  8, b,    0x2c, DnPriorityThresh )
    DefineWindowRegisterAccessors(  8, b, 5, 0x09, TxReclaimThresh )
    DefineGlobalRegisterAccessors( 16, w,    0x78, DnMaxBurst )
    DefineGlobalRegisterAccessors(  8, b,    0x3e, UpBurstThresh )
    DefineGlobalRegisterAccessors( 16, w,    0x7a, UpMaxBurst )
    DefineGlobalRegisterAccessors(  8, b,    0x3c, UpPriorityThresh )

    // Statistics Registers

    DefineWindowRegisterAccessors(  8, b, 6, 0x00, CarrierLost )
    DefineWindowRegisterAccessors(  8, b, 6, 0x01, SqeErrors )
    DefineWindowRegisterAccessors(  8, b, 6, 0x02, MultipleCollisions )
    DefineWindowRegisterAccessors(  8, b, 6, 0x03, SingleCollisions )
    DefineWindowRegisterAccessors(  8, b, 6, 0x04, LateCollisions )
    DefineWindowRegisterAccessors(  8, b, 6, 0x05, RxOverruns )
    DefineWindowRegisterAccessors(  8, b, 6, 0x06, FramesXmittedOk )
    DefineWindowRegisterAccessors(  8, b, 6, 0x07, FramesRcvdOk )
    DefineWindowRegisterAccessors(  8, b, 6, 0x08, FramesDeferred )
    DefineWindowRegisterAccessors(  8, b, 6, 0x09, UpperFramesOk )
    DefineWindowRegisterAccessors( 16, w, 6, 0x0a, BytesRcvdOk )
    DefineWindowRegisterAccessors( 16, w, 6, 0x0c, BytesXmittedOk )
    DefineWindowRegisterAccessors(  8, w, 4, 0x0c, BadSSD )
    DefineWindowRegisterAccessors(  8, w, 4, 0x0d, UpperBytesOk )
};

#endif /* !__APPLE_3COM_3C90X_H */
