/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_IOFIREWIREAVCLIBCONSUMER_H_
#define _IOKIT_IOFIREWIREAVCLIBCONSUMER_H_

#include <IOKit/avc/IOFireWireAVCLib.h>

#include <pthread.h>	// for mutexes

#include <IOKit/firewire/IOFireWireLib.h>

class IOFireWireAVCLibConsumer
{

protected:

    typedef struct _InterfaceMap 
    {
        IUnknownVTbl *pseudoVTable;
        IOFireWireAVCLibConsumer *obj;
    } InterfaceMap;

	//////////////////////////////////////
	// cf plugin interfaces
	
	static IOFireWireAVCLibConsumerInterface	sIOFireWireAVCLibConsumerInterface;
	InterfaceMap								fIOFireWireAVCLibConsumerInterface;

	//////////////////////////////////////    
    // CFArray callbacks
    
    static CFArrayCallBacks sArrayCallbacks;

	//////////////////////////////////////
	// cf plugin ref counting
	
	UInt32 		fRefCount;

	//////////////////////////////////////
    
    IOFireWireAVCLibUnitInterface **		fAVCUnit;
    IOFireWireDeviceInterface **			fFWUnit;
    CFRunLoopRef 							fCFRunLoop;
    io_object_t								fService;
	pthread_mutex_t 						fLock;

    UInt32				fGeneration;
    CFRunLoopSourceRef	fHeartbeatResponseSource;
	IOFireWireAVCLibConsumer *	fHeartbeatResponseSourceInfo;
	bool				fHeartbeatResponseScheduled;
    CFRunLoopTimerRef	fConsumerHeartbeatTimer;
    CFRunLoopTimerRef	fProducerHeartbeatTimer;
    CFRunLoopTimerRef	fReconnectTimer;
    
    UInt32 				fFlags;
    UInt8				fSubunit;
    UInt8				fLocalPlugNumber;
    
    UInt8				fRemotePlugNumber;
    FWAddress			fRemotePlugAddress;
    UInt8				fRemotePlugOptions;
    
    UInt32				fMaxPayloadLog;
    bool				fSegmentBitState;
    bool				fHeartbeatBitState;
    UInt8				fMode;
    
    UInt32				fInputPlugRegisterBuffer;
    UInt32				fOutputPlugRegisterBuffer;

    UInt32							fState;    
    void *							fStateHandlerRefcon;
    IOFireWireAVCPortStateHandler 	fStateHandler;
    
    UInt32 								fSegmentSize;
    char *								fSegmentBuffer;
    IOFireWireLibPseudoAddressSpaceRef 	fPlugAddressSpace;

    CFRunLoopSourceRef					fFrameStatusSource;
	IOFireWireAVCLibConsumer *			fFrameStatusSourceInfo;
	bool								fFrameStatusSourceScheduled;
    UInt32								fFrameStatusMode;
    UInt32								fFrameStatusCount;
    IOFireWireAVCFrameStatusHandler  	fFrameStatusHandler;
    void *								fFrameStatusHandlerRefcon;

    CFRunLoopSourceRef	fDisconnectResponseSource;
	IOFireWireAVCLibConsumer *	fDisconnectResponseSourceInfo;
	bool				fDisconnectResponseScheduled;
    
    static inline UInt64 subtractFWAddressFromFWAddress( FWAddress address1, FWAddress address2 )
    {
        UInt64 addr1 = (((UInt64)address1.addressHi) << 32) | address1.addressLo;
        UInt64 addr2 = (((UInt64)address2.addressHi) << 32) | address2.addressLo;
        return (addr2 - addr1);
    }

	static void sendHeartbeatResponse( void * info );
	static void sendDisconnectResponse( void * info );
    static void sendFrameStatusNotification( void * info );
        
    virtual void startConsumerHeartbeatTimer( void );
    static void consumerHeartbeatProc( CFRunLoopTimerRef timer, void * data );
	virtual void stopConsumerHeartbeatTimer( void );

    virtual void startProducerHeartbeatTimer( void );
    static void producerHeartbeatProc( CFRunLoopTimerRef timer, void * data );
	virtual void stopProducerHeartbeatTimer( void );
    
    virtual void startReconnectTimer( void );
    static void reconnectTimeoutProc( CFRunLoopTimerRef timer, void * data );
	virtual void stopReconnectTimer( void );

    IOReturn getNodeIDsAndGeneration( UInt16 * local, UInt16 * remote, UInt32 * gen );
	virtual void forciblyDisconnected( void );
	
	virtual void releaseSegment( void );
	
	virtual IOReturn doConnectToRemotePlug( void );

	virtual IOReturn updateProducerRegister( UInt32 newVal, UInt32 generation );

public:

	IOFireWireAVCLibConsumer( void );
	virtual ~IOFireWireAVCLibConsumer();

    static IUnknownVTbl ** alloc( IOFireWireAVCLibUnitInterface ** avcUnit, 
                                  CFRunLoopRef cfRunLoop,
                                  UInt8 plugNumber );

    virtual void finalize( void );
    
	// utility function to get "this" pointer from interface
	static inline IOFireWireAVCLibConsumer * getThis( void * self )
        { return (IOFireWireAVCLibConsumer *)((InterfaceMap *)self)->obj; }
    
	//////////////////////////////////////	
	// IUnknown static methods
	
	static HRESULT queryInterface( void * self, REFIID iid, void **ppv );
	static UInt32 comAddRef( void * self );
	static UInt32 comRelease( void * self );

	//////////////////////////////////////	
	// CFArray static methods

	// callbacks for CFArray
	static CFArrayCallBacks * getCFArrayCallbacks( void )
		{ return &sArrayCallbacks; }
        
	static const void *	cfAddRef( CFAllocatorRef allocator, const void *value );
	static void	cfRelease( CFAllocatorRef allocator, const void *value );

    //////////////////////////////////////
    // core addRef and release
    
	static UInt32 addRef( IOFireWireAVCLibConsumer * me );
    static UInt32 release( IOFireWireAVCLibConsumer * me );

    //////////////////////////////////////
   
    virtual IOReturn init( IOFireWireAVCLibUnitInterface ** avcUnit, 
                           CFRunLoopRef cfRunLoop,
                           UInt8 plugNumber );

    virtual void deviceInterestCallback( natural_t type, void * arg );
    static void setPortStateHandler( void * self, void * refcon, IOFireWireAVCPortStateHandler handler );

    static void setSubunit( void * self, UInt8 subunit );
    static void setRemotePlug( void * self, UInt8 plugNumber );
    static void setMaxPayloadSize( void * self, UInt32 size );

    static IOReturn connectToRemotePlug( void * self );
    static IOReturn disconnectFromRemotePlug( void * self );

    static IOReturn setSegmentSize( void * self, UInt32 size );
    static UInt32 getSegmentSize( void * self );
    static char * getSegmentBuffer( void * self );

    static void setFrameStatusHandler( void * self, void * refcon, IOFireWireAVCFrameStatusHandler handler );

    static UInt32 packetWriteHandler( IOFireWireLibPseudoAddressSpaceRef addressSpace,
                                      FWClientCommandID commandID, UInt32 packetLen,
                                      void* packet, UInt16 srcNodeID,		// nodeID of sender
                                      UInt32 destAddressHi, UInt32 destAddressLo,
                                      void* refCon );
                                      
    static UInt32 packetReadHandler( IOFireWireLibPseudoAddressSpaceRef	addressSpace, 
                                     FWClientCommandID commandID, UInt32 packetLen,
                                     UInt32 packetOffset, UInt16 nodeID,	// nodeID of requester
                                     UInt32 destAddressHi, UInt32 destAddressLo,
                                     void* refcon);
                                     
    static void skippedPacketHandler( IOFireWireLibPseudoAddressSpaceRef addressSpace,
                                      FWClientCommandID commandID, UInt32 skippedPacketCount);

    static void frameProcessed( void * self, UInt32 mode );
            
    static void setPortFlags( void * self, UInt32 flags );
    static void clearPortFlags( void * self, UInt32 flags );
    static UInt32 getPortFlags( void * self );
   
};

#endif