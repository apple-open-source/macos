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
 
#include "IOFireWireAVCLibUnit.h"
#include <IOKit/avc/IOFireWireAVCConsts.h>
#include "IOFireWireAVCLibConsumer.h"

#include <IOKit/IOMessage.h>
#include <unistd.h>

#define FWLOGGING 0
#define FWASSERTING 0

#if FWLOGGING
#define FWLOG(x) printf x
#else
#define FWLOG(x) do {} while (0)
#endif

#if FWASSERTING
#define FWLOGASSERT(a) { if(!(a)) { printf( "File "__FILE__", line %d: assertion '%s' failed.\n", __LINE__, #a); } }
#else
#define FWLOGASSERT(a) do {} while (0)
#endif

#define kProducerHeartbeatTime 	5.0 	// 5.0 seconds
#define kConsumerHeartbeatTime 	3.0 	// 3.0 seconds
#define kReconnectTime 			10.0 	// 10.0 seconds

enum
{
    kFWAVCStatePrivBusSuspended 	= 0,
    kFWAVCStatePrivBusResumed 		= 1,
    kFWAVCStatePrivPlugSuspended 	= 3,
    kFWAVCStatePrivPlugConnected 	= 4
};

enum
{
    kFWAVCConsumerPlugCountMask 	= 0x00ffffff,
    kFWAVCConsumerPlugCountPhase 	= 0,
    kFWAVCConsumerPlugSCMask 		= 0x01000000,
    kFWAVCConsumerPlugSCPhase		= 24,
    kFWAVCConsumerPlugModeMask  	= 0x0E000000,
    kFWAVCConsumerPlugModePhase		= 25,
    kFWAVCConsumerPlugHBMask 		= 0x10000000,
    kFWAVCConsumerPlugHBPhase		= 28
};

enum
{
    kFWAVCConsumerMode_FREE 		= 0,
    kFWAVCConsumerMode_SUSPENDED 	= 2
};

enum
{
    kFWAVCProducerMode_FREE 		= 0,
    kFWAVCProducerMode_SUSPEND		= 2,
    kFWAVCProducerMode_RESUME		= 4
};

enum
{
    kAVCConcurrentWrites 	= 0x1,
    kAVCMulticast 			= 0x2
};

enum
{
    ASYNCHRONOUS_CONNECTION	= 0x26,
    ALLOCATE_ATTACH 		= 0x03,
    DETACH_RELEASE 			= 0x07,
    RESTORE_PORT 			= 0x40
};

#define AddressHi(poa) ((poa) & 0x0000ffff)
#define AddressLo_PortBits(poa,pbits) ( ((poa) & 0xfffffffc) | ((pbits) & 0x00000003) )
#define AddressLoToMaskedPortID(a) (0xffffffc3 | ((a) & 0x0000003c))
#define Ex_ConnectionCount(ex,con) (((ex) << 7) | ((con) & 0x3f))
#define WriteInterval_RetryCount(wi,rc) (((wi) << 4) | ((rc) & 0x0000000f))

enum
{
    kAVCProducerRunBit 	= 0x00000020,
    kAVCProducerHBBit	= 0x10000000,
    kAVCProducerSCBit	= 0x01000000
};

#define oAPR(mode,count,flags,max) ( (flags) | (((mode) & 0x00000007) << 25) | \
                    ((count) & 0x00ffffc0) | ((max) & 0x0000000f) )

typedef struct
{
    UInt8	command_type;
    UInt8	header_address;
    UInt8	opcode;
    UInt8	subfunction;
    UInt8	status;
    UInt8	plug_id;
    UInt16	plug_offset_hi;
    UInt32 	plug_offset_lo;
    UInt16	connected_node_id;
    UInt16	connected_plug_offset_hi;
    UInt32	connected_plug_offset_lo;
    UInt8	connected_plug_id;
    UInt8	connection_count;
    UInt8	write_interval_retry_count;
    UInt8	reserved;
} ACAVCCommand;

//////////////////////////////////////////////////////////////////
// static interface table
//

IOFireWireAVCLibConsumerInterface 
    IOFireWireAVCLibConsumer::sIOFireWireAVCLibConsumerInterface =
{
    0,
	&IOFireWireAVCLibConsumer::queryInterface,
	&IOFireWireAVCLibConsumer::comAddRef,
	&IOFireWireAVCLibConsumer::comRelease,
	1, 0, // version/revision
    &IOFireWireAVCLibConsumer::setSubunit,
    &IOFireWireAVCLibConsumer::setRemotePlug,
    &IOFireWireAVCLibConsumer::connectToRemotePlug,
    &IOFireWireAVCLibConsumer::disconnectFromRemotePlug,
    &IOFireWireAVCLibConsumer::setFrameStatusHandler,
    &IOFireWireAVCLibConsumer::frameProcessed,
    &IOFireWireAVCLibConsumer::setMaxPayloadSize,
    &IOFireWireAVCLibConsumer::setSegmentSize,
    &IOFireWireAVCLibConsumer::getSegmentSize,
    &IOFireWireAVCLibConsumer::getSegmentBuffer,
    &IOFireWireAVCLibConsumer::setPortStateHandler,
    &IOFireWireAVCLibConsumer::setPortFlags,
    &IOFireWireAVCLibConsumer::clearPortFlags,
    &IOFireWireAVCLibConsumer::getPortFlags
};

CFArrayCallBacks IOFireWireAVCLibConsumer::sArrayCallbacks = 
{
	0,										// version
	&IOFireWireAVCLibConsumer::cfAddRef, 	// retain
   	&IOFireWireAVCLibConsumer::cfRelease, 	// release
	NULL, 									// copyDescription
    NULL,									// equal
};

//////////////////////////////////////////////////////////////////
// creation and destruction
//

// ctor
//
//

IOFireWireAVCLibConsumer::IOFireWireAVCLibConsumer( void )
{
	// create driver interface map
	fIOFireWireAVCLibConsumerInterface.pseudoVTable = (IUnknownVTbl *) &sIOFireWireAVCLibConsumerInterface;
	fIOFireWireAVCLibConsumerInterface.obj = this;
	
	// init cf plugin ref counting
	fRefCount = 0;
        
    fAVCUnit 	= NULL;
    fFWUnit 	= NULL;
    fCFRunLoop	= NULL;
    fService 	= NULL;
	
	fGeneration	= 0;
	
	fHeartbeatResponseSource = NULL;
	fHeartbeatResponseSourceInfo = this;
	fHeartbeatResponseScheduled = false;
    fConsumerHeartbeatTimer = NULL;
    fProducerHeartbeatTimer = NULL;
    fReconnectTimer 		= NULL;
    
    fFlags 				= 0;
    fSubunit 			= 0;
    fLocalPlugNumber	= 0;
    
    fRemotePlugNumber 	= 0;
    fRemotePlugAddress 	= 0;
    fRemotePlugOptions 	= 0;
    
    fMaxPayloadLog 		= 0x5; // min allowed by spec
    fSegmentBitState	= true;
    fHeartbeatBitState 	= false;
    fMode				= 0;
	
    fInputPlugRegisterBuffer 	= 0x00000000;
    fOutputPlugRegisterBuffer	= 0x00000000;
    
	fState 				= 0;
    fStateHandlerRefcon	= 0;
    fStateHandler 		= 0;
    
    fSegmentSize 		= 0;
    fSegmentBuffer		= NULL;
    fPlugAddressSpace 	= NULL;

    fFrameStatusSource = NULL;
	fFrameStatusSourceInfo = this;
	fFrameStatusSourceScheduled = false;
    fFrameStatusHandler 		= NULL;
    fFrameStatusHandlerRefcon 	= 0;
	
	fDisconnectResponseSource = NULL;
	fDisconnectResponseSourceInfo = this;
	fDisconnectResponseScheduled = false;
}

// finalize
//
// we do the actual clean up in finalize, not in the destructor
// this gets called before the AVCUnit calls the final CFRelease on us

void IOFireWireAVCLibConsumer::finalize( void )
{   
	//
	// stop timers
	//
	
    stopReconnectTimer();
    stopProducerHeartbeatTimer();
	stopConsumerHeartbeatTimer();
	
	//
	// free the segment buffer and address space
    //
	
	releaseSegment();
	
	//
	// release our runloop sources
	//
	
	if( fHeartbeatResponseSource != NULL )
	{
		CFRunLoopSourceInvalidate( fHeartbeatResponseSource );
		CFRelease( fHeartbeatResponseSource );
		fHeartbeatResponseSource = NULL;
	}
    
	if( fFrameStatusSource != NULL )
	{
		CFRunLoopSourceInvalidate( fFrameStatusSource );
		CFRelease( fFrameStatusSource );
		fFrameStatusSource = NULL;
	}	
		
	if( fDisconnectResponseSource != NULL )
	{
		CFRunLoopSourceInvalidate( fDisconnectResponseSource );
		CFRelease( fDisconnectResponseSource );
		fDisconnectResponseSource = NULL;
	}	
	
	pthread_mutex_destroy( &fLock );

	//
	// release our interfaces
	//
	
	if( fFWUnit )
    {
		(*fFWUnit)->RemoveCallbackDispatcherFromRunLoop( fFWUnit );
		(*fFWUnit)->Close( fFWUnit );
        (*fFWUnit)->Release( fFWUnit );
		fFWUnit = NULL;
    }
}

// dtor
//
// clean up has already been done by finalize

IOFireWireAVCLibConsumer::~IOFireWireAVCLibConsumer()
{
    if( fAVCUnit )
    {
        (*fAVCUnit)->Release( fAVCUnit );
		fAVCUnit = NULL;		
    }
}

// alloc
//
// static allocator, called by factory method

IUnknownVTbl ** IOFireWireAVCLibConsumer::alloc( IOFireWireAVCLibUnitInterface ** avcUnit,
                                                 CFRunLoopRef cfRunLoop,
                                                 UInt8 plugNumber )
{
    IOReturn					status = kIOReturnSuccess;
	IOFireWireAVCLibConsumer *	me;
	IUnknownVTbl ** 			interface = NULL;
	
	if( status == kIOReturnSuccess )
	{
		me = new IOFireWireAVCLibConsumer();
		if( me == NULL )
			status = kIOReturnError;
	}
		
	if( status == kIOReturnSuccess )
	{
		status = me->init( avcUnit, cfRunLoop, plugNumber );
	}
	
	if( status != kIOReturnSuccess )
		delete me;

	if( status == kIOReturnSuccess )
	{
		// we return an interface here. 
        // queryInterface is not called in this case, so we call addRef here
		IOFireWireAVCLibConsumer::addRef(me);
		interface = (IUnknownVTbl **) &me->fIOFireWireAVCLibConsumerInterface.pseudoVTable;
	}
	
	return interface;
}

// init
//
// initialize our object, called by alloc()

IOReturn IOFireWireAVCLibConsumer::init( IOFireWireAVCLibUnitInterface ** avcUnit,
                                         CFRunLoopRef cfRunLoop,
                                         UInt8 plugNumber )
{
	IOReturn status = kIOReturnSuccess;
    
	if( avcUnit == NULL || cfRunLoop == NULL || 
		plugNumber < kFWAVCAsyncPlug0 || plugNumber > kFWAVCAsyncPlug30 )
	{
		status = kIOReturnBadArgument;
	}

	if( status == kIOReturnSuccess )
	{
		fAVCUnit = avcUnit;
		(*fAVCUnit)->AddRef( fAVCUnit );		
		
		fCFRunLoop = cfRunLoop;
		fLocalPlugNumber = plugNumber;
		
		pthread_mutex_init( &fLock, NULL );
	}

	if( status == kIOReturnSuccess )
	{
		// fDisconnectResponseSourceInfo points to "this" because CF uses the info pointer 
		// to determine if CF runloop sources are the same
		CFRunLoopSourceContext	context = { 0, &fDisconnectResponseSourceInfo, nil, nil, nil, nil, nil, nil, nil, sendDisconnectResponse };
		fDisconnectResponseSource = CFRunLoopSourceCreate( kCFAllocatorDefault, 0, &context );
		if( fDisconnectResponseSource == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		CFRunLoopAddSource( fCFRunLoop, fDisconnectResponseSource, kCFRunLoopDefaultMode );
	}

    if( status == kIOReturnSuccess )
	{
		// fFrameStatusSourceInfo points to "this" because CF uses the info pointer 
		// to determine if CF runloop sources are the same
		CFRunLoopSourceContext	context = { 0, &fFrameStatusSourceInfo, nil, nil, nil, nil, nil, nil, nil, sendFrameStatusNotification };
		fFrameStatusSource = CFRunLoopSourceCreate( kCFAllocatorDefault, 0, &context );
		if( fFrameStatusSource == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		CFRunLoopAddSource( fCFRunLoop, fFrameStatusSource, kCFRunLoopDefaultMode );
	}

	if( status == kIOReturnSuccess )
	{
		// fHeartbeatResponseSourceInfo points to "this" because CF uses the info pointer 
		// to determine if CF runloop sources are the same
		CFRunLoopSourceContext	context = { 0, &fHeartbeatResponseSourceInfo, nil, nil, nil, nil, nil, nil, nil, sendHeartbeatResponse };
		fHeartbeatResponseSource = CFRunLoopSourceCreate( kCFAllocatorDefault, 0, &context );
		if( fHeartbeatResponseSource == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		CFRunLoopAddSource( fCFRunLoop, fHeartbeatResponseSource, kCFRunLoopDefaultMode );
	}
				
	if( status == kIOReturnSuccess )
	{
		fFWUnit = (IOFireWireDeviceInterface**)(*fAVCUnit)->getAncestorInterface( fAVCUnit, 
									(char*)"IOFireWireUnit", 
									CFUUIDGetUUIDBytes(kIOFireWireLibTypeID), 
									CFUUIDGetUUIDBytes(kIOFireWireDeviceInterfaceID) );
		if( fFWUnit == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		(*fFWUnit)->AddRef( fFWUnit );
	}
	
	IOFireWireSessionRef sessionRef = 0;
		
	if( status == kIOReturnSuccess )
	{
        sessionRef = (*fAVCUnit)->getSessionRef( fAVCUnit );
        if( sessionRef == 0 )
            status = kIOReturnError;
    }
            
    // open
    if( status == kIOReturnSuccess )
    {
		status = (*fFWUnit)->OpenWithSessionRef( fFWUnit, sessionRef );
	}

	FWLOG(( "IOFireWireAVCLibConsumer::init OpenWithSessionRef return status = 0x%08x\n", status ));

	if( status == kIOReturnSuccess )
	{
		status = (*fFWUnit)->AddCallbackDispatcherToRunLoop( fFWUnit, fCFRunLoop );
	}
	
	if( status == kIOReturnSuccess )
	{
		fService = (*fFWUnit)->GetDevice( fFWUnit );
		if( fService == (io_object_t)NULL )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		if( isDeviceSuspended( fAVCUnit ) )
		{
			fState = kFWAVCStatePrivBusSuspended;
		}
		else
		{
			fState = kFWAVCStatePrivBusResumed;
		}
    }
	
	FWLOG(( "IOFireWireAVCLibConsumer::init return status = 0x%08lx\n", (UInt32)status ));
	
    return status;
}

// queryInterface
//
//

HRESULT IOFireWireAVCLibConsumer::queryInterface( void * self, REFIID iid, void **ppv )
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT result = S_OK;
    IOFireWireAVCLibConsumer * me = getThis(self);

//    FWLOG(( "IOFireWireAVCLibConsumer::queryInterface start\n" ));
    
	if( CFEqual(uuid, IUnknownUUID) ||  CFEqual(uuid, kIOFireWireAVCLibConsumerInterfaceID) ) 
	{
        *ppv = &me->fIOFireWireAVCLibConsumerInterface;
        comAddRef(self);
    }
    else
        *ppv = 0;

    if( !*ppv )
        result = E_NOINTERFACE;

    CFRelease( uuid );
	
 //   FWLOG(( "IOFireWireAVCLibConsumer::queryInterface stop\n" ));
    
    return result;
}

//////////////////////////////////////////////////////////////////
// reference counting
// 
// cf and com have different reference counting methods
// we call a shared method to actually do the work

// addRef
//
//

const void * IOFireWireAVCLibConsumer::cfAddRef( CFAllocatorRef allocator, const void *value )
{
    IOFireWireAVCLibConsumer * me = (IOFireWireAVCLibConsumer*)value;
    IOFireWireAVCLibConsumer::addRef( me );
    return value;
}

UInt32 IOFireWireAVCLibConsumer::comAddRef( void * self )
{
    IOFireWireAVCLibConsumer * me = getThis(self);
    return IOFireWireAVCLibConsumer::addRef( me );
}

UInt32 IOFireWireAVCLibConsumer::addRef( IOFireWireAVCLibConsumer * me )
{
	me->fRefCount++;
	return me->fRefCount;
}

// release
//
//

void IOFireWireAVCLibConsumer::cfRelease( CFAllocatorRef allocator, const void *value )
{
    IOFireWireAVCLibConsumer * me = (IOFireWireAVCLibConsumer*)value;
    IOFireWireAVCLibConsumer::release( me );
}

UInt32 IOFireWireAVCLibConsumer::comRelease( void * self )
{
    IOFireWireAVCLibConsumer * me = getThis(self);
    return IOFireWireAVCLibConsumer::release( me );
}

UInt32 IOFireWireAVCLibConsumer::release( IOFireWireAVCLibConsumer * me )
{
	UInt32 retVal = me->fRefCount;
    	
    me->fRefCount--;
    
    if( 0 == me->fRefCount  ) 
	{
        delete me;
    }
	else if( 1 == me->fRefCount ) 
	{
        // clean up before we tell the AVCUnit to let us go
		me->finalize();
        
        // unit is last reference, inform it of this fact
        consumerPlugDestroyed( me->fAVCUnit, me );
    }
    else if( me->fRefCount < 0 )
	{
        me->fRefCount = 0;
	}
	
	return retVal;
}

//////////////////////////////////////////////////////////////////
// accessors
//

// getNodeIDsAndGeneration
//
// returns the local and remote node ids and the generation for which they are valid

IOReturn IOFireWireAVCLibConsumer::getNodeIDsAndGeneration( UInt16 * local, UInt16 * remote, UInt32 * gen )
{
    IOReturn status = kIOReturnSuccess;
    
    UInt16 localNodeID;
    UInt16 remoteNodeID;
	UInt32 firstGen, secondGen;
    
    // these routines only return errors if there is something wrong with the user client's
    // connection to the kernel
    
	// 1. get generation and remote nodeID
    // 2. get local nodeID
	// 3. get generation again
	// 4. repeat if the generation has changed
	
	do
    {
        if( status == kIOReturnSuccess )
        {
            status = (*fFWUnit)->GetGenerationAndNodeID( fFWUnit, &firstGen, &remoteNodeID );
        }
        
        if( status == kIOReturnSuccess )
        {
            status = (*fFWUnit)->GetLocalNodeID( fFWUnit, &localNodeID );
        }

        if( status == kIOReturnSuccess )
        {
            status = (*fFWUnit)->GetGenerationAndNodeID( fFWUnit, &secondGen, &remoteNodeID );
        }
        
    } while( firstGen != secondGen && status == kIOReturnSuccess );
    
    if( status == kIOReturnSuccess )
    {
        *remote = remoteNodeID;
        *local = localNodeID;
        *gen = firstGen;
    }
    
    return status;
}

// setSubunit
//
//

void IOFireWireAVCLibConsumer::setSubunit( void * self, UInt8 subunit )
{
    IOFireWireAVCLibConsumer * me = getThis(self);
    
    me->fSubunit = subunit;
}

// setRemotePlug
//
//

void IOFireWireAVCLibConsumer::setRemotePlug( void * self, UInt8 plugNumber )
{
    IOFireWireAVCLibConsumer * me = getThis(self);
    
    me->fRemotePlugNumber = plugNumber;
}

// setMaxPayloadSize
//
//

void IOFireWireAVCLibConsumer::setMaxPayloadSize( void * self, UInt32 size )
{
    IOFireWireAVCLibConsumer * me = getThis(self);
    UInt32 sizeBytes = size;
    
    me->fMaxPayloadLog = 0x0;
    while( (sizeBytes >= 0x4) && (me->fMaxPayloadLog < 0xa) )
    {
        sizeBytes >>= 1;
        me->fMaxPayloadLog++;
    }
    
    // spec says maxPayloadLog shall not be less than 5
    if( me->fMaxPayloadLog < 0x5 )
        me->fMaxPayloadLog = 0x5;
}

//////////////////////////////////////////////////////////////////
// connection management
//

// connectToRemotePlug
//
// public connection method

IOReturn IOFireWireAVCLibConsumer::connectToRemotePlug( void * self )
{
    IOFireWireAVCLibConsumer * me = getThis(self);
    IOReturn status = kIOReturnSuccess;
    
	pthread_mutex_lock( &me->fLock );
    
    FWLOG(( "IOFireWireAVCLibConsumer::connectToRemotePlug\n" ));
    
    if( me->fState != kFWAVCStatePrivBusResumed )
    {
        status = kIOReturnNotReady;
    }
    
	if( me->fRemotePlugNumber < kFWAVCAsyncPlug0 || me->fRemotePlugNumber > kFWAVCAsyncPlugAny )
	{
		status = kIOReturnNoResources;
	}
	
    if( me->fPlugAddressSpace == NULL )
    {
        status = kIOReturnNoResources;
    }

	if( status == kIOReturnSuccess )
    {
		FWLOG(( "IOFireWireAVCLibConsumer::connectToRemotePlug attempt to connect to device...\n" ));
		UInt32 tries = 11;  // reconnect timeout is 10 seconds
		do
		{
			status = me->doConnectToRemotePlug();
			if( status != kIOReturnSuccess )
			{
				FWLOG(( "IOFireWireLibAVCConsumer::connectToRemotePlug connect to device failed with status = 0x%08lx\n", (UInt32)status ));
				if( tries > 1 )
				{
					// sleep for 1 second before retrying
					usleep( 1 * 1000000 );
					FWLOG(( "IOFireWireLibAVCConsumer::connectToRemotePlug retrying connect to device...\n" ));
				}
			}
		}
		while( status != kIOReturnSuccess && --tries );
	}
	
	FWLOG(( "IOFireWireLibAVCConsumer::connectToRemotePlug return status = 0x%08lx\n", (UInt32)status ));
	
	pthread_mutex_unlock( &me->fLock );
			
	return status;
}

// doConnectToRemotePlug
//
// internal connection routine

IOReturn IOFireWireAVCLibConsumer::doConnectToRemotePlug( void )
{
    IOReturn status = kIOReturnSuccess;
        
    FWLOG(( "IOFireWireAVCLibConsumer::doConnectToRemotePlug\n" ));
    
    FWAddress		address;
           
    if( status == kIOReturnSuccess )
    {
        (*fPlugAddressSpace)->GetFWAddress( fPlugAddressSpace, &address );
    }

    UInt16 			localNodeID;
    UInt16			remoteNodeID;
    UInt32			generation;
    
    if( status == kIOReturnSuccess )
    {
        status = getNodeIDsAndGeneration( &localNodeID, &remoteNodeID, &generation );
    }    

    ACAVCCommand	cmd;
    ACAVCCommand	response;
    
    if( status == kIOReturnSuccess )
    {
        // send ALLOCATE_ATTACH to remote subunit
        
        cmd.command_type 				= 0x00; 					// CT/RC = Control command
        cmd.header_address 				= kAVCUnitAddress;  		// HA = unit
        cmd.opcode 						= ASYNCHRONOUS_CONNECTION;	// Opcode = ASYNCHRONOUS_CONNECTION
        cmd.subfunction 				= ALLOCATE_ATTACH;			// Subfunction = ALLOCATE_ATTACH
        cmd.status 						= 0xff;						// status = N/A
        cmd.plug_id 					= fRemotePlugNumber;		// plugID = plug number
        cmd.plug_offset_hi 				= OSSwapHostToBigInt16(0xffff);
        cmd.plug_offset_lo 				= OSSwapHostToBigInt32(0xffffffff);		// any producer port	
        cmd.connected_node_id 			= OSSwapHostToBigInt16(0xffc0 | localNodeID);
        cmd.connected_plug_offset_hi	= OSSwapHostToBigInt16(AddressHi(address.addressHi));
        cmd.connected_plug_offset_lo	= OSSwapHostToBigInt32(AddressLo_PortBits(address.addressLo, kAVCConcurrentWrites | kAVCMulticast ));
        cmd.connected_plug_id			= fLocalPlugNumber;
        cmd.connection_count			= Ex_ConnectionCount(0x1,0x3f);
        cmd.write_interval_retry_count 	= WriteInterval_RetryCount(0x00,0x00);
        cmd.reserved				 	= 0x00;
    }

    FWLOG(( "IOFireWireAVCLibConsumer::doConnectToRemotePlug avc command\n" ));
   
    UInt32 responseLength = sizeof(response);
    if( status == kIOReturnSuccess )
    {
        status = (*fAVCUnit)->AVCCommandInGeneration( fAVCUnit, generation, (UInt8*)&cmd, sizeof(cmd), (UInt8*)&response, &responseLength );
		
		// Fix up endian issues here!
		if (responseLength >= sizeof(ACAVCCommand))
		{
			response.plug_offset_hi = OSSwapBigToHostInt16(response.plug_offset_hi);
			response.plug_offset_lo = OSSwapBigToHostInt32(response.plug_offset_lo);
			response.connected_node_id = OSSwapBigToHostInt16(response.connected_node_id);
			response.connected_plug_offset_hi = OSSwapBigToHostInt16(response.connected_plug_offset_hi);
			response.connected_plug_offset_lo = OSSwapBigToHostInt32(response.connected_plug_offset_lo);
		}
    }
    
    if( status == kIOReturnSuccess )
    {
        if( response.status != 0x03 )
            status = kIOReturnDeviceError;
    }
    
    if( status == kIOReturnSuccess )
    {
        fRemotePlugNumber = response.plug_id;
        fRemotePlugAddress.addressHi = response.plug_offset_hi;
        fRemotePlugAddress.addressLo = response.plug_offset_lo & 0xfffffffc;
        fRemotePlugOptions = response.plug_offset_lo & 0x00000003;
    }
    
    // set run bit in producer plug
    UInt32 newVal = 0;
    
    if( status == kIOReturnSuccess )
    {
		fOutputPlugRegisterBuffer = 0x00000000;
		fInputPlugRegisterBuffer = 0x00000000;
		fSegmentBitState = true;
		fHeartbeatBitState = false;
    }
    	
    if( status == kIOReturnSuccess )
    {
		FWLOG(( "IOFireWireAVCLibConsumer::doConnectToRemotePlug run bit\n" ));
		// oAPR( mode, count, flags, maxLoad )
		UInt32 flags = 	kAVCProducerRunBit | 
						(fHeartbeatBitState ? kAVCProducerHBBit : 0x00000000) | 
						(fSegmentBitState ? kAVCProducerSCBit : 0x00000000);
		newVal = oAPR( kFWAVCProducerMode_SEND, fSegmentSize, flags, fMaxPayloadLog );
		status = updateProducerRegister( newVal, generation );
	}
    
    if( status == kIOReturnSuccess )
    {
        fState = kFWAVCStatePrivPlugConnected;
        fGeneration = generation;
        fMode = kFWAVCProducerMode_SEND;
        startProducerHeartbeatTimer();
    }
    
    FWLOG(( "IOFireWireAVCLibConsumer::doConnectToRemotePlug status = 0x%08lx\n", (UInt32)status ));
    
    return status;
}

// disconnectFromRemotePlug
//
//

IOReturn IOFireWireAVCLibConsumer::disconnectFromRemotePlug( void * self )
{
    IOFireWireAVCLibConsumer * me = getThis(self);
    IOReturn status = kIOReturnSuccess;
    
	pthread_mutex_lock( &me->fLock );

    switch( me->fState )
    {
        case kFWAVCStatePrivPlugSuspended:
            
            // don't reconnect if we get a resume message
            
            me->stopReconnectTimer();
            
			// neither of the heartbeat timers should be running
			
			if( me->fProducerHeartbeatTimer != NULL || me->fConsumerHeartbeatTimer != NULL )
			{
				printf( "IOFireWireAVCLibConsumer::disconnectFromRemotePlug heartbeat timers set while plug is suspended!\n" );
			}
			
            me->fState = kFWAVCStatePrivBusSuspended;
            
            break;
            
        case kFWAVCStatePrivPlugConnected:
            
            ACAVCCommand	cmd;
            ACAVCCommand	response;
            UInt32 			responseLength;
            
			// the reconnect timer should not be running
			
			if( me->fReconnectTimer != NULL )
			{
				printf( "IOFireWireAVCLibConsumer::disconnectFromRemotePlug reconnect timer set while plug is resumed!\n" );
			}
			
            me->stopProducerHeartbeatTimer();
            me->stopConsumerHeartbeatTimer();

            if( status == kIOReturnSuccess )
            {
                // send DETACH_RELEASE to remote unit
                
                cmd.command_type 				= 0x00; 					// CT/RC = Control command
                cmd.header_address 				= kAVCUnitAddress;  		// HA = unit
                cmd.opcode 						= ASYNCHRONOUS_CONNECTION;	// Opcode = ASYNCHRONOUS_CONNECTION
                cmd.subfunction 				= DETACH_RELEASE;			// Subfunction = DETACH_RELEASE
                cmd.status 						= 0xff;						// status = N/A
                cmd.plug_id 					= me->fRemotePlugNumber;	// plugID = plug number
                cmd.plug_offset_hi 				= OSSwapHostToBigInt16(0xffff);
                cmd.plug_offset_lo 				= OSSwapHostToBigInt32(AddressLoToMaskedPortID(me->fRemotePlugAddress.addressLo));
                cmd.connected_node_id 			= OSSwapHostToBigInt16(0xffff);
                cmd.connected_plug_offset_hi	= OSSwapHostToBigInt16(0xffff);
                cmd.connected_plug_offset_lo	= OSSwapHostToBigInt32(0xffffffff);
                cmd.connected_plug_id			= 0xff;
                cmd.connection_count			= Ex_ConnectionCount(0x1,0x3f);
                cmd.write_interval_retry_count 	= WriteInterval_RetryCount(0xf,0xf);
                cmd.reserved				 	= 0x00;
            }
        
            if( status == kIOReturnSuccess )
            {
                responseLength = sizeof(response);
				status = (*me->fAVCUnit)->AVCCommandInGeneration( me->fAVCUnit, me->fGeneration, (UInt8*)&cmd, sizeof(cmd), (UInt8*)&response, &responseLength );
				
				// Fix up endian issues here!
				if (responseLength >= sizeof(ACAVCCommand))
				{
					response.plug_offset_hi = OSSwapBigToHostInt16(response.plug_offset_hi);
					response.plug_offset_lo = OSSwapBigToHostInt32(response.plug_offset_lo);
					response.connected_node_id = OSSwapBigToHostInt16(response.connected_node_id);
					response.connected_plug_offset_hi = OSSwapBigToHostInt16(response.connected_plug_offset_hi);
					response.connected_plug_offset_lo = OSSwapBigToHostInt32(response.connected_plug_offset_lo);
				}
            }
            
			if( status == kIOFireWireBusReset )
			{
				// bus reset occured while disconnecting.
				// as long as we don't reconnect we will be disconnected, so ignore the error
				
				// this also means we cannot connect until the reconnect period is over
				
				status = kIOReturnSuccess;
			}
			else if( status == kIOReturnSuccess )
            {
                if( response.status != 0x01 )
                {
					// if the device gave us an error on disconnect, it probably
					// already thinks we're disconnected, so ignore the error
					
					FWLOG(( "IOFireWireAVCLibConsumer::disconnectFromRemotePlug disconnect AVCCommand returned response.status = 0x%02x\n", response.status ));
				}
			}
			else
			{
				FWLOG(( "IOFireWireAVCLibConsumer::disconnectFromRemotePlug disconnect AVCCommand status = 0x%08lx\n", (UInt32)status ));
				
				// what do we do about this?
				// figure we're disconnected even if we got an error!?
				
				status = kIOReturnSuccess;
			}
        
			if( status == kIOReturnSuccess )
			{
				me->fState = kFWAVCStatePrivBusResumed;
				me->fInputPlugRegisterBuffer = 0x00000000;
			}
			
            break;
        
        default:
    
            // we're in a state we can't disconnect from
            status = kIOReturnNotPermitted;
            
            break;
    
    }

	FWLOG(( "IOFireWireAVCLibConsumer::disconnectFromRemotePlug status = 0x%08lx\n", (UInt32)status ));
	
	pthread_mutex_unlock( &me->fLock );

    return status;
}

// forciblyDisconnected
//
// notifies clients of a forced disconnection

void IOFireWireAVCLibConsumer::forciblyDisconnected( void )
{
	fInputPlugRegisterBuffer = 0x00000000;
	
    switch( fState )
    {
		case kFWAVCStatePrivPlugSuspended:
			
			fState = kFWAVCStatePrivBusSuspended;
			
            pthread_mutex_unlock( &fLock );
                    
			if( fStateHandler )
            {
                (fStateHandler)(fStateHandlerRefcon, kFWAVCStatePlugDisconnected );
            }
            
			break;
							
        case kFWAVCStatePrivPlugConnected:
            
			fState = kFWAVCStatePrivBusResumed;
            
            pthread_mutex_unlock( &fLock );
                    
            if( fStateHandler )
            {
                (fStateHandler)(fStateHandlerRefcon, kFWAVCStatePlugDisconnected );
            }
            
			break;
        
        default:
        
            pthread_mutex_unlock( &fLock );
                    
            FWLOG(( "IOFireWireAVCLibConsumer::forciblyDisconnected we're in an unexpected state = 0x%08lx.\n", fState ));
            break;
    }
}

// setPortStateHandler
//
//

void IOFireWireAVCLibConsumer::setPortStateHandler( void * self, void * refcon, IOFireWireAVCPortStateHandler handler )
{
    IOFireWireAVCLibConsumer * me = getThis(self);
    
    me->fStateHandlerRefcon = refcon;
    me->fStateHandler = handler;
}

// deviceInterestCallback
//
//

void IOFireWireAVCLibConsumer::deviceInterestCallback( natural_t type, void * arg )
{
    IOReturn status = kIOReturnSuccess;
    
	switch( type )
	{        
        case kIOMessageServiceIsSuspended:
		
            FWLOG(( "IOFireWireAVCLibConsumer::kIOMessageServiceIsSuspended bus reset start\n" ));
            
            pthread_mutex_lock( &fLock );
   
            switch ( fState )
            {
				case kFWAVCStatePrivBusResumed:
                    
                    fState = kFWAVCStatePrivBusSuspended;
                    
                    pthread_mutex_unlock( &fLock );
   
                    if( fStateHandler )
                    {
                        (fStateHandler)( fStateHandlerRefcon, kFWAVCStateBusSuspended );
                    }
                    
                    break;
				
				case kFWAVCStatePrivPlugSuspended:
					
					// restart reconnect timer
					startReconnectTimer();
					
                    pthread_mutex_unlock( &fLock );
   
					break;
					
                case kFWAVCStatePrivPlugConnected:
                    
                    fState = kFWAVCStatePrivPlugSuspended;
                    
					// these values reset on bus resets
					fOutputPlugRegisterBuffer &= ~(kAVCProducerRunBit | kAVCProducerHBBit);
                    fInputPlugRegisterBuffer &= ~kFWAVCConsumerPlugHBMask;
					fHeartbeatBitState = false;
								
                    stopProducerHeartbeatTimer();
                    stopConsumerHeartbeatTimer();
                    startReconnectTimer();
                    
                    pthread_mutex_unlock( &fLock );
   
                    if( fStateHandler )
                    {
                        (fStateHandler)( fStateHandlerRefcon, kFWAVCStateBusSuspended );
                    }
                    
                    break;
                
				case kFWAVCStatePrivBusSuspended:
                default:
                    pthread_mutex_unlock( &fLock );
					// do nothing
                    break;
            
			}
            
            break;
            
        case kIOMessageServiceIsResumed:
        
            FWLOG(( "IOFireWireAVCLibConsumer::kIOMessageServiceIsResumed bus reset complete\n" ));
   
            pthread_mutex_lock( &fLock );
   
            switch ( fState )
            {
                case kFWAVCStatePrivBusSuspended:
                    
                    fState = kFWAVCStatePrivBusResumed;
                    
                    pthread_mutex_unlock( &fLock );
   
                    if( fStateHandler )
                    {
                        (fStateHandler)( fStateHandlerRefcon, kFWAVCStateBusResumed );
                    }
                    
                    break;
                
                case kFWAVCStatePrivPlugSuspended:
     
                    pthread_mutex_unlock( &fLock );
   
                    if( fStateHandler )
                    {
                        (fStateHandler)( fStateHandlerRefcon, kFWAVCStateBusResumed );
                    }
                
					pthread_mutex_lock( &fLock );

					stopReconnectTimer();
				
                    UInt16 			localNodeID;
                    UInt16			remoteNodeID;
                    UInt32			generation;
                    
                    if( status == kIOReturnSuccess )
                    {
                        status = getNodeIDsAndGeneration( &localNodeID, &remoteNodeID, &generation );
                    }
                
                    if( status == kIOReturnSuccess )
                    {
                        if( generation == fGeneration )
                        {
                            // we've already connected on this generation, no need to reconnect
                            // this can happen if we get two (or more) bus rests in a row and
                            // while handling the first one we latched the generation of the second
                        }
                        else
                        {
                            ACAVCCommand	cmd;
                            ACAVCCommand	response;
            
                            if( status == kIOReturnSuccess )
                            {
                                // send RESTORE_PORT to remote unit
                                
                                cmd.command_type 				= 0x00; 					// CT/RC = Control command
                                cmd.header_address 				= kAVCUnitAddress;  		// HA = unit
                                cmd.opcode 						= ASYNCHRONOUS_CONNECTION;	// Opcode = ASYNCHRONOUS_CONNECTION
                                cmd.subfunction 				= RESTORE_PORT;				// Subfunction = RESTORE_PORT
                                cmd.status 						= 0xff;						// status = N/A
                                cmd.plug_id 					= fRemotePlugNumber;		// plugID = plug number
                                cmd.plug_offset_hi 				= OSSwapHostToBigInt16(0xffff);
                                cmd.plug_offset_lo 				= OSSwapHostToBigInt32(AddressLoToMaskedPortID(fRemotePlugAddress.addressLo));			
                                cmd.connected_node_id 			= OSSwapHostToBigInt16(0xffc0 | localNodeID);
                                cmd.connected_plug_offset_hi	= OSSwapHostToBigInt16(0xffff);
                                cmd.connected_plug_offset_lo	= OSSwapHostToBigInt32(AddressLoToMaskedPortID(0x00000000));
                                cmd.connected_plug_id			= 0xff;
                                cmd.connection_count			= Ex_ConnectionCount(0x1,0x3f);
                                cmd.write_interval_retry_count 	= WriteInterval_RetryCount(0xf,0xf);
                                cmd.reserved				 	= 0x00;
                            }
        
                            UInt32 responseLength = sizeof(response);
                            if( status == kIOReturnSuccess )
                            {
                                status = (*fAVCUnit)->AVCCommandInGeneration( fAVCUnit, generation, (UInt8*)&cmd, sizeof(cmd), (UInt8*)&response, &responseLength );
								
								// Fix up endian issues here!
								if (responseLength >= sizeof(ACAVCCommand))
								{
									response.plug_offset_hi = OSSwapBigToHostInt16(response.plug_offset_hi);
									response.plug_offset_lo = OSSwapBigToHostInt32(response.plug_offset_lo);
									response.connected_node_id = OSSwapBigToHostInt16(response.connected_node_id);
									response.connected_plug_offset_hi = OSSwapBigToHostInt16(response.connected_plug_offset_hi);
									response.connected_plug_offset_lo = OSSwapBigToHostInt32(response.connected_plug_offset_lo);
								}
                            }
                            
							if( status == kIOReturnSuccess )
                            {
                                if( response.status != 0x03 )
                                    status = kIOReturnDeviceError;
                            }
                    
                            if( status == kIOReturnSuccess )
                            {
                                fRemotePlugNumber = response.plug_id;
                                fRemotePlugAddress.addressHi = response.plug_offset_hi;
                                fRemotePlugAddress.addressLo = response.plug_offset_lo & 0xfffffffc;
                                fRemotePlugOptions = response.plug_offset_lo & 0x00000003;
							}
							
							if( fHeartbeatBitState || 
								(fOutputPlugRegisterBuffer & (kAVCProducerRunBit | kAVCProducerHBBit)) ||
								(fInputPlugRegisterBuffer & kFWAVCConsumerPlugHBMask) )
							{
								printf( "IOFireWireAVCLibConsumer::deviceInterestCallback register values not reset!\n" );
							}

                            // set run bit in producer register
                            
                            UInt32 newVal = 0;
                           
							if( status == kIOReturnSuccess )
							{
								// oAPR( mode, count, flags, maxLoad )
								UInt32 flags = 	kAVCProducerRunBit | 
												(fHeartbeatBitState ? kAVCProducerHBBit : 0x00000000) | 
												(fSegmentBitState ? kAVCProducerSCBit : 0x00000000);
								newVal = oAPR( kFWAVCProducerMode_SEND, fSegmentSize, flags, fMaxPayloadLog );
								status = updateProducerRegister( newVal, generation );
							}
 
                            if( status == kIOReturnSuccess )
                            {
                                fMode = kFWAVCProducerMode_SEND;
                                 startProducerHeartbeatTimer();
                            }
                    
                        } // generation if
                    
                    } // status if
                    
                    if( status == kIOReturnSuccess )
                    {
                        fState = kFWAVCStatePrivPlugConnected;
                        fGeneration = generation;
                    
					}
					
					if( status == kIOReturnSuccess )
					{
                        pthread_mutex_unlock( &fLock );
                    
                        if( fStateHandler )
                        {
                            (fStateHandler)( fStateHandlerRefcon, kFWAVCStatePlugReconnected );
                        }
                    }
					else if( status == kIOFireWireBusReset )
					{
						// this means our attempt to reconnect was interupted by a bus reset
						// that means we will start the reconnect process over and don't need to do anything here					
                        pthread_mutex_unlock( &fLock );
                    
					}
                    else
                    {
						FWLOG(( "IOFireWireAVCLibConsumer::deviceInterestCallback failed to reconnect - forciblyDisconnecting\n" ));
						
						forciblyDisconnected();
                    }
					
                    break;
                
                case kIOFWMessageServiceIsRequestingClose:
                    FWLOG(( "IOFireWireAVCLibConsumer::kIOFWMessageServiceIsRequestingClose (device removed)\n" ));
                    
                    pthread_mutex_lock( &fLock );
   
                    switch ( fState )
                    {
                        case kFWAVCStatePrivBusSuspended:
                            
                            pthread_mutex_unlock( &fLock );
   
                            if( fStateHandler )
                            {
                                (fStateHandler)( fStateHandlerRefcon, kFWAVCStateDeviceRemoved );
                            }
                            
                            break;
                        
                        case kFWAVCStatePrivPlugSuspended:
                        
							FWLOG(( "IOFireWireAVCLibConsumer::deviceInterestCallback device unplugged - forciblyDisconnecting\n" ));
							
							forciblyDisconnected();
							
                            if( fStateHandler )
                            {
                                (fStateHandler)( fStateHandlerRefcon, kFWAVCStateDeviceRemoved );
                            }
                            
                            break;
                            
                        default:
                            FWLOG(( "IOFireWireAVCLibConsumer::deviceInterestCallback kIOFWMessageServiceIsRequestingClose in odd state, fState = 0x%08lx/n", fState ));
                            break;
                    }
                    
                    
                    break;

                case kIOMessageServiceIsTerminated:
                    FWLOG(( "IOFireWireAVCLibConsumer::kIOMessageServiceIsTerminated (device removed)\n" ));
                    break;
                        
                default:
                    FWLOG(( "IOFireWireAVCLibConsumer::deviceInterestCallback kIOMessageServiceIsResumed in odd state, fState = 0x%08lx/n", fState ));
                    break;
            }
            break;
            
        default:
			break;
	}
}

// startReconnectTimer
//
//

void IOFireWireAVCLibConsumer::startReconnectTimer( void  )
{
	CFRunLoopTimerContext		context;
	CFAbsoluteTime				time;
	
    // stop if necessary
    stopReconnectTimer();
                    
    context.version             = 0;
    context.info                = this;
    context.retain              = NULL;
    context.release             = NULL;
    context.copyDescription     = NULL;

    time = CFAbsoluteTimeGetCurrent() + kReconnectTime;

    fReconnectTimer = CFRunLoopTimerCreate(NULL, time,
                                    0,
                                    0,
                                    0,
                                    (CFRunLoopTimerCallBack)&IOFireWireAVCLibConsumer::reconnectTimeoutProc,
                                    &context);
	
	if ( fReconnectTimer )
	{
		CFRunLoopAddTimer( fCFRunLoop, fReconnectTimer, kCFRunLoopDefaultMode );
	}
}

// reconnectTimeoutProc
//
//

void IOFireWireAVCLibConsumer::reconnectTimeoutProc( CFRunLoopTimerRef timer, void *data )
{
	IOFireWireAVCLibConsumer * me = (IOFireWireAVCLibConsumer*)data;
    
    pthread_mutex_lock( &me->fLock );
   
    me->stopReconnectTimer();
    
	FWLOG(( "IOFireWireAVCLibConsumer::reconnectTimeoutProc reconnect timed out - forciblyDisconnecting\n" ));
						
	me->forciblyDisconnected();
}

// stopReconnectTimer
//
//

void IOFireWireAVCLibConsumer::stopReconnectTimer( void )
{
	if ( fReconnectTimer )
	{
		CFRunLoopTimerInvalidate( fReconnectTimer );
		CFRelease( fReconnectTimer );
		fReconnectTimer = NULL;
	}
}

//////////////////////////////////////////////////////////////////
// segment buffer allocation
//

// setSegmentSize
//
//

IOReturn IOFireWireAVCLibConsumer::setSegmentSize( void * self, UInt32 size )
{
    IOFireWireAVCLibConsumer * me = getThis(self);
    IOReturn status = kIOReturnSuccess;
    
	if( size > 0x00ffffff || (size & 0x0000003f) != 0 )
	{
		return kIOReturnBadArgument;
	}
	
    // free previous address space
	me->releaseSegment();
	    
    // new segment buffer
    me->fSegmentBuffer = (char*)malloc( size + 64 );
    if( me->fSegmentBuffer == NULL )
        status = kIOReturnNoMemory;
        
    if( status == kIOReturnSuccess )
    {
        me->fSegmentSize = size;
    
        // new psuedo address space
        me->fPlugAddressSpace = (*me->fFWUnit)->CreatePseudoAddressSpace( me->fFWUnit, me->fSegmentSize + 64, 
                                                                          me, (me->fSegmentSize + 64) * 2,
                                                                          me->fSegmentBuffer, 
                                                                          kFWAddressSpaceNoFlags,
											CFUUIDGetUUIDBytes(kIOFireWirePseudoAddressSpaceInterfaceID));
        if( me->fPlugAddressSpace == NULL )
            status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
        FWAddress	address;
        (*me->fPlugAddressSpace)->GetFWAddress(me->fPlugAddressSpace, &address);
        
        FWLOG(("IOFireWireAVCLibConsumer::allocated address space at %04X:%08lX\n", address.addressHi, address.addressLo));
        
        (*me->fPlugAddressSpace)->SetWriteHandler( me->fPlugAddressSpace,
                                                   &IOFireWireAVCLibConsumer::packetWriteHandler );
        (*me->fPlugAddressSpace)->SetSkippedPacketHandler( me->fPlugAddressSpace,
                                                           &IOFireWireAVCLibConsumer::skippedPacketHandler );
        (*me->fPlugAddressSpace)->SetReadHandler( me->fPlugAddressSpace, &IOFireWireAVCLibConsumer::packetReadHandler );
        
        if( !(*me->fPlugAddressSpace)->TurnOnNotification( me->fPlugAddressSpace ) )
            status = kIOReturnError ;

    }
    
    return status;
}

// releaseSegment
//
//

void IOFireWireAVCLibConsumer::releaseSegment( void )
{
    // free previous address space
    if( fPlugAddressSpace != NULL )
    {
        (*fPlugAddressSpace)->TurnOffNotification( fPlugAddressSpace );
        (*fPlugAddressSpace)->Release( fPlugAddressSpace );
		fPlugAddressSpace = NULL;
    }
    
    // free previous buffer
    if( fSegmentBuffer != NULL )
    {
        free( fSegmentBuffer );
		fSegmentBuffer= NULL;
    }
}

// getSegmentSize
//
//

UInt32 IOFireWireAVCLibConsumer::getSegmentSize( void * self )
{
    IOFireWireAVCLibConsumer * me = getThis(self);
    
    return me->fSegmentSize;
}

// getSegmentBuffer
//
//

char * IOFireWireAVCLibConsumer::getSegmentBuffer( void * self )
{
    IOFireWireAVCLibConsumer * me = getThis(self);
    
    return (me->fSegmentBuffer + 64);
}

//////////////////////////////////////////////////////////////////
// segment buffer transactions
//

// setFrameStatusHandler
//
//

void IOFireWireAVCLibConsumer::setFrameStatusHandler( void * self, 
                    void * refcon, IOFireWireAVCFrameStatusHandler handler )
{
    IOFireWireAVCLibConsumer * me = getThis(self);
    
    me->fFrameStatusHandler = handler;
    me->fFrameStatusHandlerRefcon = refcon;
}

// updateProducerRegister
//
//

IOReturn IOFireWireAVCLibConsumer::updateProducerRegister( UInt32 newVal, UInt32 generation )
{
	IOReturn status = kIOReturnSuccess;
	
	UInt32 tries = 6;
	bool done = false;
	do
	{
		FWLOG(( "IOFireWireAVCLibConsumer::updateProducerRegister sending CompareAndSwap(0x%08lx,0x%08lx) to producer\n", fOutputPlugRegisterBuffer, newVal ));
		status = (*fFWUnit)->CompareSwap( fFWUnit, fService, &fRemotePlugAddress, OSSwapHostToBigInt32(fOutputPlugRegisterBuffer), OSSwapHostToBigInt32(newVal), true, generation );
		if( status == kIOFireWireBusReset || status == kIOReturnSuccess)
		{
			done = true;
		}
		else
		{
			FWLOG(( "IOFireWireLibAVCConsumer::updateProducerRegister register update failed with status = 0x%08lx\n", (UInt32)status ));
			
			if( tries > 1 )
			{
				// sleep for 25 milliseconds before reading
				usleep( 25 * 1000 );
				
				// perhaps producer register is other than we think
				// try reading it
				UInt32 producerRegister = 0;
				status = (*fFWUnit)->ReadQuadlet( fFWUnit, fService, &fRemotePlugAddress, &producerRegister, true, generation );
				if( status == kIOReturnSuccess )
				{
					fOutputPlugRegisterBuffer = OSSwapBigToHostInt32(producerRegister);
				}
				else if( status == kIOFireWireBusReset )
				{
					done = true;
				}
				
				if( !done )
				{
					// sleep for 25 milliseconds before retrying
					usleep( 25 * 1000 );
					FWLOG(( "IOFireWireLibAVCConsumer::updateProducerRegister retrying register update.\n" ));
				}
			}
		}
	}
	while( !done && --tries );

	if( status == kIOReturnSuccess )
	{
		// if it worked store the value away the value for next time.
		fOutputPlugRegisterBuffer = newVal;
	}
	
	return status;
}

// packetReadHandler
//
//

UInt32
IOFireWireAVCLibConsumer::packetReadHandler(
	IOFireWireLibPseudoAddressSpaceRef	addressSpace,
	FWClientCommandID					commandID,
	UInt32								packetLen,
	UInt32								packetOffset,
	UInt16								nodeID,			// nodeID of requester
	UInt32								destAddressHi,	// destination on this node
	UInt32								destAddressLo,
	void*								refcon)
{

	IOFireWireAVCLibConsumer * me = (IOFireWireAVCLibConsumer*)refcon;
	
	UInt32 endianFixedPlugVal;
	
	FWLOG(( "IOFireWireAVCLibConsumer::packetReadHandler called - destAddressLo = 0x%08lx\n", destAddressLo ));
    
    UInt8 * buffer = (UInt8 *)(*addressSpace)->GetBuffer( addressSpace );
    
    if( packetOffset < 4 )
    {
		endianFixedPlugVal = OSSwapHostToBigInt32(me->fInputPlugRegisterBuffer);
        bcopy( (void*)&endianFixedPlugVal, buffer + packetOffset, sizeof(UInt32));
    }
    else if( packetOffset < 64 )
    {
        bzero( buffer + packetOffset, packetLen );
    }
    else
    {
        bcopy( me->fSegmentBuffer + packetOffset - 64, buffer + packetOffset, packetLen);
	}
    
	(*addressSpace)->ClientCommandIsComplete( addressSpace, commandID, kFWResponseComplete ) ;

	return 0;
}

// packetWriteHandler
//
//

UInt32 IOFireWireAVCLibConsumer::packetWriteHandler(
                    IOFireWireLibPseudoAddressSpaceRef	addressSpace,
					FWClientCommandID					commandID,
					UInt32								packetLen,
					void*								packet,
					UInt16								srcNodeID,		// nodeID of sender
					UInt32								destAddressHi,	// destination on this node
					UInt32								destAddressLo,
					void*								refcon)
{
    IOFireWireAVCLibConsumer * me = (IOFireWireAVCLibConsumer*)refcon;
    IOReturn 	status = kIOReturnSuccess;
 	
    FWAddress	plugAddress;
    (*addressSpace)->GetFWAddress(addressSpace, &plugAddress);
    
    FWAddress 	offsetAddress;
    offsetAddress.addressHi = destAddressHi;
    offsetAddress.addressLo = destAddressLo;
    
    UInt32 offset = (UInt32)subtractFWAddressFromFWAddress( plugAddress, offsetAddress );
	
    if( offset < 64 )
    {
		//
        // in register space
        //
		
        if( packetLen > 4 )
        {
            FWLOG(( "IOFireWireAVCLibConsumer::packetWriteHandler wrote to register > 4 bytes\n" ));
            status = kIOReturnError;
        }
        
        if( !((offsetAddress.addressHi == plugAddress.addressHi) && 
              (offsetAddress.addressLo == plugAddress.addressLo)) )
        {
            FWLOG(( "IOFireWireAVCLibConsumer::packetWriteHandler offsetAddress != plugAddress\n" ));
            status = kIOReturnError;
        }
        
		pthread_mutex_lock( &me->fLock );
		bool workScheduled = false;
		
        if( status == kIOReturnSuccess )
        {        
            me->fInputPlugRegisterBuffer =  OSSwapBigToHostInt32(*((UInt32*)packet));
			FWLOG(( "IOFireWireAVCLibConsumer::packetWriteHandler received packet = 0x%08lx\n", me->fInputPlugRegisterBuffer ));
		}
		
		if( status == kIOReturnSuccess && me->fState == kFWAVCStatePrivPlugConnected )
		{
            me->stopProducerHeartbeatTimer();
            
            UInt32 count	= (me->fInputPlugRegisterBuffer & kFWAVCConsumerPlugCountMask) >> kFWAVCConsumerPlugCountPhase;
            UInt32 mode 	= (me->fInputPlugRegisterBuffer & kFWAVCConsumerPlugModeMask) >> kFWAVCConsumerPlugModePhase;
            
            bool newHeartbeatState = (me->fInputPlugRegisterBuffer & kFWAVCConsumerPlugHBMask) >> kFWAVCConsumerPlugHBPhase;
            
            if( me->fHeartbeatBitState != newHeartbeatState )
            {
				//
				// schedule the heartbeat response
				//
				
				if( !me->fHeartbeatResponseScheduled )
				{
					FWLOG(( "IOFireWireAVCLibConsumer::packetWriteHandler schedule heartbeat response\n" ));
					me->fHeartbeatResponseScheduled = true;
					workScheduled = true;
					CFRunLoopSourceSignal( me->fHeartbeatResponseSource );
					CFRunLoopWakeUp( me->fCFRunLoop );
				}
				else
				{
					FWLOG(( "IOFireWireAVCLibConsumer::packetWriteHandler frame heartbeat response already scheduled!\n" ));
				}
			}
            else
            {
                switch( mode )
                {
                    case kFWAVCConsumerMode_FREE:
                    
                        // producer initiated shutdown request.
    
                        switch( me->fState )
                        {
                            case kFWAVCStatePrivPlugConnected:
                                
								//
								// schedule disconnect response
								//
								
								if( !me->fDisconnectResponseScheduled )
								{
									FWLOG(( "IOFireWireAVCLibConsumer::packetWriteHandler schedule disconnect response\n" ));
									me->fDisconnectResponseScheduled = true;
									workScheduled = true;
									CFRunLoopSourceSignal( me->fDisconnectResponseSource );
									CFRunLoopWakeUp( me->fCFRunLoop );
								}
								else
								{
									FWLOG(( "IOFireWireAVCLibConsumer::packetWriteHandler disconnect response already scheduled!\n" ));
								}
                                
                                break;
                            
                            default:
                                FWLOG(( "IOFireWireAVCLibConsumer::packetWriteHandler FREE received, but we're in an unexpected state = 0x%08lx.\n", me->fState ));
                                break;
                        }
                        
                        break;
                        
                    case kFWAVCConsumerMode_MORE:
                    case kFWAVCConsumerMode_LAST:
                    case kFWAVCConsumerMode_LESS:
                    case kFWAVCConsumerMode_JUNK:
                    case kFWAVCConsumerMode_LOST:
                        
						//
						// schedule frame status handler
						//
						
                        me->startConsumerHeartbeatTimer();
                        
						me->fFrameStatusMode = mode;
                        me->fFrameStatusCount = count;
						
						if( !me->fFrameStatusSourceScheduled )
						{
							FWLOG(( "IOFireWireAVCLibConsumer::packetWriteHandler schedule frame status handler\n" ));
							me->fFrameStatusSourceScheduled = true;
							workScheduled = true;
							CFRunLoopSourceSignal( me->fFrameStatusSource );
							CFRunLoopWakeUp( me->fCFRunLoop );
                        }
						else
						{
							FWLOG(( "IOFireWireAVCLibConsumer::packetWriteHandler frame heartbeat response already scheduled!\n" ));
						}
                        
						break;
                        
                    default:
                        break;
                }
            }
        }
		
		if( !workScheduled )
		{
			pthread_mutex_unlock( &me->fLock );
		}
    }
    else
    {
		//
        // in segment buffer space
        //
		
        if( offset + packetLen - 64 > me->fSegmentSize )
        {
            FWLOG(( "IOFireWireAVCLibConsumer::packetWriteHandler written packet size > than segment size\n" ));
            status = kIOReturnError;
        }
        
        if( status == kIOReturnSuccess )
        {
            // copy the packet into the segment buffer
            bcopy( packet, me->fSegmentBuffer + offset, packetLen );
        }
    }
    
    (*addressSpace)->ClientCommandIsComplete(addressSpace, commandID, kIOReturnSuccess) ;
	
	return 0;
}

// skippedPacketHandler
//
//

void IOFireWireAVCLibConsumer::skippedPacketHandler(
        IOFireWireLibPseudoAddressSpaceRef	addressSpace,
        FWClientCommandID			commandID,
        UInt32						skippedPacketCount)
{
	FWLOG(("IOFireWireAVCLibConsumer::skippedPacketHandler skippedPacketCount = %ld\n", skippedPacketCount));

	//zzz what to do ?
	
	(*addressSpace)->ClientCommandIsComplete(addressSpace, commandID, kIOReturnSuccess); 
}


// sendDisconnectResponse
//
//

void IOFireWireAVCLibConsumer::sendDisconnectResponse( void * info )
{
	IOReturn status = kIOReturnSuccess;
	IOFireWireAVCLibConsumer ** meRef = (IOFireWireAVCLibConsumer **)info;
	IOFireWireAVCLibConsumer * me = *meRef;
	UInt32 newVal = 0;
	
	me->fDisconnectResponseScheduled = false;

	FWLOG(( "IOFireWireLibAVCConsumer::sendDisconnectResponse disconnect response handler called\n" ));

	UInt32 flags = 	kAVCProducerRunBit | 
				(me->fHeartbeatBitState ? kAVCProducerHBBit : 0x00000000) | 
				(!me->fSegmentBitState ? kAVCProducerSCBit : 0x00000000);
	newVal = oAPR( kFWAVCProducerMode_FREE, me->fSegmentSize, flags, me->fMaxPayloadLog );
	status = me->updateProducerRegister( newVal, me->fGeneration );
	if( status == kIOReturnSuccess )
	{
		// I guess this is unneeded as we are disconnected
		me->fMode = kFWAVCProducerMode_FREE;
		me->fState = kFWAVCStatePrivBusResumed;
		me->fSegmentBitState = !me->fSegmentBitState;
		
		FWLOG(( "IOFireWireAVCLibConsumer::packetWriteHandler FREE received - forciblyDisconnecting\n" ));
		me->forciblyDisconnected();
	}
	else
	{
        pthread_mutex_unlock( &me->fLock );
    }
}

// sendHeartbeatResponse
//
//

void IOFireWireAVCLibConsumer::sendHeartbeatResponse( void * info )
{
	IOReturn status = kIOReturnSuccess;
	IOFireWireAVCLibConsumer ** meRef = (IOFireWireAVCLibConsumer **)info;
	IOFireWireAVCLibConsumer * me = *meRef;
	UInt32 newVal = 0;
	
	me->fHeartbeatResponseScheduled = false;
	FWLOG(( "IOFireWireLibAVCConsumer::sendHeartbeatResponse heartbeat response handler called\n" ));
	UInt32 flags = 	kAVCProducerRunBit | 
					(!me->fHeartbeatBitState ? kAVCProducerHBBit : 0x00000000) | 
					(me->fSegmentBitState ? kAVCProducerSCBit : 0x00000000);
	newVal = oAPR( kFWAVCProducerMode_SEND, me->fSegmentSize, flags, me->fMaxPayloadLog );
	status = me->updateProducerRegister( newVal, me->fGeneration );
	if( status == kIOReturnSuccess )
	{
		me->fHeartbeatBitState = !me->fHeartbeatBitState;
		me->startProducerHeartbeatTimer();
        pthread_mutex_unlock( &me->fLock );
    }
	else if( status != kIOFireWireBusReset )
	{
		FWLOG(( "IOFireWireLibAVCConsumer::sendHeartbeatResponse heartbeat handshake failed with status = 0x%08lx\n", (UInt32)status ));
		me->forciblyDisconnected();
	}
	else
    {
        pthread_mutex_unlock( &me->fLock );
    }
	
}

// startConsumerHeartbeatTimer
//
//

void IOFireWireAVCLibConsumer::startConsumerHeartbeatTimer( void  )
{
	CFRunLoopTimerContext		context;
	CFAbsoluteTime				time;
	
	stopConsumerHeartbeatTimer(); // just in case
	
    context.version             = 0;
    context.info                = this;
    context.retain              = NULL;
    context.release             = NULL;
    context.copyDescription     = NULL;

    time = CFAbsoluteTimeGetCurrent() + kConsumerHeartbeatTime;

    fConsumerHeartbeatTimer = CFRunLoopTimerCreate(NULL, time,
                                    kConsumerHeartbeatTime,
                                    0,
                                    0,
                                    (CFRunLoopTimerCallBack)&IOFireWireAVCLibConsumer::consumerHeartbeatProc,
                                    &context);
	
	if ( fConsumerHeartbeatTimer )
	{
		CFRunLoopAddTimer( fCFRunLoop, fConsumerHeartbeatTimer, kCFRunLoopDefaultMode );
	}
}

// consumerHeartbeatProc
//
//

void IOFireWireAVCLibConsumer::consumerHeartbeatProc( CFRunLoopTimerRef timer, void *data )
{
    IOFireWireAVCLibConsumer * me = (IOFireWireAVCLibConsumer*)data;
    IOReturn status = kIOReturnSuccess;
	UInt32 newVal = 0;
	
	pthread_mutex_lock( &me->fLock );

	me->stopConsumerHeartbeatTimer(); // necessary?

	// oAPR( mode, count, flags, maxLoad )
	UInt32 flags = 	kAVCProducerRunBit | 
					(!me->fHeartbeatBitState ? kAVCProducerHBBit : 0x00000000) | 
					(me->fSegmentBitState ? kAVCProducerSCBit : 0x00000000);
	newVal = oAPR( me->fMode, me->fSegmentSize, flags, me->fMaxPayloadLog );
	status = me->updateProducerRegister( newVal, me->fGeneration );
	if( status == kIOReturnSuccess )
	{
		me->fHeartbeatBitState = !me->fHeartbeatBitState;
	}
	
	pthread_mutex_unlock( &me->fLock );
}

// stopConsumerHeartbeatTimer
//
//

void IOFireWireAVCLibConsumer::stopConsumerHeartbeatTimer( void )
{
	if ( fConsumerHeartbeatTimer )
	{
		CFRunLoopTimerInvalidate( fConsumerHeartbeatTimer );
		CFRelease( fConsumerHeartbeatTimer );
		fConsumerHeartbeatTimer = NULL;
	}
}

// sendFrameStatusNotification
//
//

void IOFireWireAVCLibConsumer::sendFrameStatusNotification( void * info )
{
	IOFireWireAVCLibConsumer ** meRef = (IOFireWireAVCLibConsumer **)info;
	IOFireWireAVCLibConsumer * me = *meRef;
	me->fFrameStatusSourceScheduled = false;

	FWLOG(( "IOFireWireLibAVCConsumer::sendFrameStatusNotification frame status handler called\n" ));
	
	pthread_mutex_unlock( &me->fLock );

    if( me->fFrameStatusHandler )
    {
        (me->fFrameStatusHandler)(me->fFrameStatusHandlerRefcon, me->fFrameStatusMode, me->fFrameStatusCount);
    }
}

// frameProcessed
//
//

void IOFireWireAVCLibConsumer::frameProcessed( void * self, UInt32 mode )
{
    IOFireWireAVCLibConsumer * me = getThis(self);
    IOReturn status = kIOReturnSuccess;

	pthread_mutex_lock( &me->fLock );
	
	me->stopConsumerHeartbeatTimer();
	
	UInt32 flags = 	kAVCProducerRunBit | 
					(me->fHeartbeatBitState ? kAVCProducerHBBit : 0x00000000) | 
					(!me->fSegmentBitState ? kAVCProducerSCBit : 0x00000000);
	
	// oAPR( mode, count, flags, maxLoad )
	UInt32 newVal = oAPR( mode, me->fSegmentSize, flags, me->fMaxPayloadLog );
	
	status = me->updateProducerRegister( newVal, me->fGeneration );
	
	if( status == kIOReturnSuccess )
	{
		me->fMode = mode;
		me->fSegmentBitState = !me->fSegmentBitState;
		me->startProducerHeartbeatTimer();
	}

	pthread_mutex_unlock( &me->fLock );
}

// startProducerHeartbeatTimer
//
//

void IOFireWireAVCLibConsumer::startProducerHeartbeatTimer( void  )
{
	CFRunLoopTimerContext		context;
	CFAbsoluteTime				time;
	
	stopProducerHeartbeatTimer(); // just in case
 
    context.version             = 0;
    context.info                = this;
    context.retain              = NULL;
    context.release             = NULL;
    context.copyDescription     = NULL;

    time = CFAbsoluteTimeGetCurrent() + kProducerHeartbeatTime;

    fProducerHeartbeatTimer = CFRunLoopTimerCreate(	NULL, time,
                                                    0,
                                                    0,
                                                    0,
                                                    (CFRunLoopTimerCallBack)&IOFireWireAVCLibConsumer::producerHeartbeatProc,
                                                    &context);
	
	if ( fProducerHeartbeatTimer )
	{
		CFRunLoopAddTimer( fCFRunLoop, fProducerHeartbeatTimer, kCFRunLoopDefaultMode );
	}
}

// producerHeartbeatProc
//
//

void IOFireWireAVCLibConsumer::producerHeartbeatProc( CFRunLoopTimerRef timer, void *data )
{
	IOFireWireAVCLibConsumer * me = (IOFireWireAVCLibConsumer*)data;
	
    pthread_mutex_lock( &me->fLock );
   
    me->stopProducerHeartbeatTimer(); // necessary?
 
	FWLOG(( "IOFireWireAVCLibConsumer::producerHeartbeatProc producer heatbeat timeout - forciblyDisconnecting\n" ));
    
	me->forciblyDisconnected();
}

// stopProducerHeartbeatTimer
//
//

void IOFireWireAVCLibConsumer::stopProducerHeartbeatTimer( void )
{
	if( fProducerHeartbeatTimer )
	{
		CFRunLoopTimerInvalidate( fProducerHeartbeatTimer );
		CFRelease( fProducerHeartbeatTimer );
		fProducerHeartbeatTimer = NULL;
	}
}

//////////////////////////////////////////////////////////////////
// flags
//

void IOFireWireAVCLibConsumer::setPortFlags( void * self, UInt32 flags )
{
    IOFireWireAVCLibConsumer * me = getThis(self);

    me->fFlags |= flags;
}

void IOFireWireAVCLibConsumer::clearPortFlags( void * self, UInt32 flags )
{
    IOFireWireAVCLibConsumer * me = getThis(self);

    me->fFlags &= !flags;
}

UInt32 IOFireWireAVCLibConsumer::getPortFlags( void * self )
{
    IOFireWireAVCLibConsumer * me = getThis(self);

    return me->fFlags;
}
