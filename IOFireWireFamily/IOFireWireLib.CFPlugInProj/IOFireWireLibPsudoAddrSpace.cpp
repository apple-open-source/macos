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
 *  IOFireWirePseudoAddressSpacePriv.cpp
 *  IOFireWireLib
 *
 *  Created by NWG on Wed Dec 06 2000.
 *  Copyright (c) 2000 Apple, Inc. All rights reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/iokitmig.h>

#include "IOFireWireLibPriv.h"
#include "IOFireWireLibPsudoAddrSpace.h"

// ============================================================
//
// interface table
//
// ============================================================

IOFireWirePseudoAddressSpaceInterface IOFireWirePseudoAddressSpaceImp::sInterface =
{
	INTERFACEIMP_INTERFACE,
	1, 0, // version/revision
	& IOFireWirePseudoAddressSpaceImp::SSetWriteHandler,
	& IOFireWirePseudoAddressSpaceImp::SSetReadHandler,
	& IOFireWirePseudoAddressSpaceImp::SSetSkippedPacketHandler,
	& IOFireWirePseudoAddressSpaceImp::SNotificationIsOn,
	& IOFireWirePseudoAddressSpaceImp::STurnOnNotification,
	& IOFireWirePseudoAddressSpaceImp::STurnOffNotification,
	& IOFireWirePseudoAddressSpaceImp::SClientCommandIsComplete,
	
	& IOFireWirePseudoAddressSpaceImp::SGetFWAddress,
	& IOFireWirePseudoAddressSpaceImp::SGetBuffer,
	& IOFireWirePseudoAddressSpaceImp::SGetBufferSize,
	& IOFireWirePseudoAddressSpaceImp::SGetRefCon
} ;

IUnknownVTbl** 
IOFireWirePseudoAddressSpaceImp::Alloc(
	IOFireWireDeviceInterfaceImp& 	inUserClient, 
	FWKernAddrSpaceRef 				inKernAddrSpaceRef, 
	void* 							inBuffer, 
	UInt32 							inBufferSize, 
	void* 							inBackingStore, 
	void*							inRefCon)
{
    IOFireWirePseudoAddressSpaceImp *	me;
	IUnknownVTbl** 	interface = NULL;
	
    me = new IOFireWirePseudoAddressSpaceImp(inUserClient, inKernAddrSpaceRef, inBuffer, inBufferSize, inBackingStore, inRefCon) ;
    if( me && (kIOReturnSuccess == me->Init()) )
	{
//		me->AddRef();
        interface = & me->mInterface.pseudoVTable;
    }
	else
		delete me ;
	
	return interface;
}

HRESULT STDMETHODCALLTYPE
IOFireWirePseudoAddressSpaceImp::QueryInterface(REFIID iid, LPVOID* ppv)
{
	HRESULT		result = S_OK ;
	*ppv = nil ;

	CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;

	if ( CFEqual(interfaceID, IUnknownUUID) ||  CFEqual(interfaceID, kIOFireWirePseudoAddressSpaceInterfaceID) )
	{
		*ppv = & mInterface ;
		AddRef() ;
	}
	else
	{
		*ppv = nil ;
		result = E_NOINTERFACE ;
	}	
	
	CFRelease(interfaceID) ;
	return result ;
}

// ============================================================
//
// interface table methods
//
// ============================================================

const IOFireWirePseudoAddressSpaceWriteHandler
IOFireWirePseudoAddressSpaceImp::SSetWriteHandler(IOFireWireLibPseudoAddressSpaceRef self, IOFireWirePseudoAddressSpaceWriteHandler inWriter)
{ 
	return GetThis(self)->SetWriteHandler(inWriter); 
}

const IOFireWirePseudoAddressSpaceReadHandler
IOFireWirePseudoAddressSpaceImp::SSetReadHandler(IOFireWireLibPseudoAddressSpaceRef self, IOFireWirePseudoAddressSpaceReadHandler inReader)
{ 
	return GetThis(self)->SetReadHandler(inReader); 
}

const IOFireWirePseudoAddressSpaceSkippedPacketHandler
IOFireWirePseudoAddressSpaceImp::SSetSkippedPacketHandler(IOFireWireLibPseudoAddressSpaceRef self, IOFireWirePseudoAddressSpaceSkippedPacketHandler inHandler)
{ 
	return GetThis(self)->SetSkippedPacketHandler(inHandler); 
}

Boolean
IOFireWirePseudoAddressSpaceImp::SNotificationIsOn(IOFireWireLibPseudoAddressSpaceRef self)
{
	return GetThis(self)->mNotifyIsOn; 
}

Boolean
IOFireWirePseudoAddressSpaceImp::STurnOnNotification(IOFireWireLibPseudoAddressSpaceRef self)
{ 
	return GetThis(self)->TurnOnNotification(self); 
}

void
IOFireWirePseudoAddressSpaceImp::STurnOffNotification(IOFireWireLibPseudoAddressSpaceRef self)
{ 
	GetThis(self)->TurnOffNotification(); 
}

void
IOFireWirePseudoAddressSpaceImp::SClientCommandIsComplete(IOFireWireLibPseudoAddressSpaceRef self, FWClientCommandID commandID, IOReturn status)
{ 
	GetThis(self)->ClientCommandIsComplete(commandID, status); 
}

void
IOFireWirePseudoAddressSpaceImp::SGetFWAddress(IOFireWireLibPseudoAddressSpaceRef self, FWAddress* outAddr)
{ 
	bcopy (&GetThis(self)->mFWAddress, outAddr, sizeof(FWAddress)); 
}

void*
IOFireWirePseudoAddressSpaceImp::SGetBuffer(IOFireWireLibPseudoAddressSpaceRef self)
{ 
	return GetThis(self)->mBuffer; 
}

const UInt32
IOFireWirePseudoAddressSpaceImp::SGetBufferSize(IOFireWireLibPseudoAddressSpaceRef self)
{ 
	return GetThis(self)->mBufferSize; 
}

void*
IOFireWirePseudoAddressSpaceImp::SGetRefCon(IOFireWireLibPseudoAddressSpaceRef self)
{
	return GetThis(self)->mRefCon; 
}

#pragma mark -
// ============================================================
//
// class methods
//
// ============================================================

IOFireWirePseudoAddressSpaceImp::IOFireWirePseudoAddressSpaceImp(
	IOFireWireDeviceInterfaceImp&	inUserClient,
	FWKernAddrSpaceRef				inKernAddrSpaceRef,
	void*							inBuffer,
	UInt32							inBufferSize,
	void*							inBackingStore,
	void*							inRefCon) : IOFireWireIUnknown(), // COM fixup
												mNotifyIsOn(false),
												mUserClient(inUserClient), 
												mKernAddrSpaceRef(inKernAddrSpaceRef),
												mBuffer(inBuffer),
												mBufferSize(inBufferSize),
												mBackingStore(inBackingStore),
												mRefCon(inRefCon)
{
	inUserClient.AddRef() ;
	
	mInterface.pseudoVTable = (IUnknownVTbl*) & sInterface ;
	mInterface.obj = this ;
}

IOFireWirePseudoAddressSpaceImp::~IOFireWirePseudoAddressSpaceImp()
{
	IOReturn result = IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(),
													 kFWPseudoAddrSpace_Release,
													 1,
													 0,
													 mKernAddrSpaceRef) ;
	if (kIOReturnSuccess != result)
		fprintf(stderr, "IOFireWirePseudoAddressSpaceImp::~IOFireWirePseudoAddressSpaceImp: error releasing address space!\n") ;

	mUserClient.Release() ;
}

IOReturn
IOFireWirePseudoAddressSpaceImp::Init()
{
    IOReturn	err = kIOReturnSuccess ;
    
    io_scalar_inband_t	params = {(int)mKernAddrSpaceRef} ;
    io_scalar_inband_t	output;
    mach_msg_type_number_t size = 3 ;

    err = io_connect_method_scalarI_scalarO(
                mUserClient.GetUserClientConnection(),
                kFWPseudoAddrSpace_GetFWAddrInfo,
                params,
                1,
                output,
                & size) ;
    
	#if __IOFireWireClientDebug__
    fprintf(stderr, "IOFireWirePseudoAddressSpaceImp::Init(): kr = %08lX\n",(UInt32) err) ;
	#endif

    if ( kIOReturnSuccess == err )
    {
        mFWAddress 		= FWAddress(output[1], output[2], output[0]) ;
    }
    
	#if __IOFireWireClientDebug__
	fprintf(stderr, "mFWAddress = %04lX:%08lX\n", (UInt16) mFWAddress.addressHi, (UInt32) mFWAddress.addressLo) ;
	#endif

    return err ;
}

// callback management

#pragma mark -
#pragma mark --callback management

const IOFireWirePseudoAddressSpaceWriteHandler
IOFireWirePseudoAddressSpaceImp::SetWriteHandler(
	IOFireWirePseudoAddressSpaceWriteHandler		inWriter)
{
	IOFireWirePseudoAddressSpaceWriteHandler		oldWriter = mWriter ;
	mWriter = inWriter ;
	
	return oldWriter ;
}


const IOFireWirePseudoAddressSpaceReadHandler
IOFireWirePseudoAddressSpaceImp::SetReadHandler(
	IOFireWirePseudoAddressSpaceReadHandler		inReader)
{
	IOFireWirePseudoAddressSpaceReadHandler		oldReader = mReader ;
	mReader = inReader ;
	
	return oldReader ;
}

const IOFireWirePseudoAddressSpaceSkippedPacketHandler
IOFireWirePseudoAddressSpaceImp::SetSkippedPacketHandler(
	IOFireWirePseudoAddressSpaceSkippedPacketHandler			inHandler)
{
	IOFireWirePseudoAddressSpaceSkippedPacketHandler result = mSkippedPacketHandler ;
	mSkippedPacketHandler = inHandler ;

	return result ;
}

Boolean
IOFireWirePseudoAddressSpaceImp::TurnOnNotification(
//	CFRunLoopRef 			inRunLoop, 
	void*					callBackRefCon)
{
	IOReturn				err					= kIOReturnSuccess ;
	io_connect_t			connection			= mUserClient.GetUserClientConnection() ;
	io_scalar_inband_t		params ;
	io_scalar_inband_t		output ;
	mach_msg_type_number_t	size = 0 ;

	// if notification is already on, skip out.
	if (mNotifyIsOn)
		return kIOReturnSuccess ;
	
	if (!connection)
		err = kIOReturnNoDevice ;

/*	if (mUserClient.AsyncPortsExist()) ;
		err = mUserClient.CreateAsyncPorts() ;
		
	if ( kIOReturnSuccess == err )
	{
		CFRunLoopSourceRef	runLoopSource	= CFMachPortCreateRunLoopSource(
													kCFAllocatorDefault,
													mUserClient.GetAsyncCFPort(),
													0) ;

		if (!runLoopSource)
			err = kIOReturnNoMemory ;
		
		if ((kIOReturnSuccess == err) && inRunLoop && runLoopSource)
		{
			CFRunLoopAddSource(inRunLoop, runLoopSource, kCFRunLoopDefaultMode) ;
			CFRelease(runLoopSource) ;
		}														
	} */
	
	if ( kIOReturnSuccess == err )
	{
		params[0]	= (UInt32)mKernAddrSpaceRef ;
		params[1]	= (UInt32)(IOAsyncCallback) & IOFireWirePseudoAddressSpaceImp::Writer ;
		params[2]	= (UInt32) callBackRefCon;
	
		err = io_async_method_scalarI_scalarO(
				connection,
				mUserClient.GetAsyncPort(),
				mPacketAsyncRef,
				1,
				kFWSetAsyncRef_Packet,
				params,
				3,
				output,
				& size) ;
		
	}
	
	if ( kIOReturnSuccess == err)
	{
		size=0 ;
		params[0]	= (UInt32) mKernAddrSpaceRef ;
		params[1]	= (UInt32)(IOAsyncCallback2) & IOFireWirePseudoAddressSpaceImp::SkippedPacketHandler ;
		params[2]	= (UInt32) callBackRefCon;
		
		err = io_async_method_scalarI_scalarO(
				connection,
				mUserClient.GetAsyncPort(),
				mSkippedPacketAsyncRef,
				1,
				kFWSetAsyncRef_SkippedPacket,
				params,
				3,
				output,
				& size) ;
	}
	
	if ( kIOReturnSuccess == err)
	{
		params[0]	= (UInt32) mKernAddrSpaceRef ;
		params[1]	= (UInt32) & IOFireWirePseudoAddressSpaceImp::ReadHandler ;
		params[2]	= (UInt32) this ;
		
		err = io_async_method_scalarI_scalarO(
				connection,
				mUserClient.GetAsyncPort(),
				mSkippedPacketAsyncRef,
				1,
				kFWSetAsyncRef_Read,
				params,
				3,
				params,
				& size) ;
	}

	if ( kIOReturnSuccess == err )
		mNotifyIsOn = true ;
		
	return ( kIOReturnSuccess == err ) ;
}

void
IOFireWirePseudoAddressSpaceImp::TurnOffNotification()
{
	IOReturn				err					= kIOReturnSuccess ;
	io_connect_t			connection			= mUserClient.GetUserClientConnection() ;
	io_scalar_inband_t		params ;
	mach_msg_type_number_t	size = 0 ;
	
	// if notification isn't on, skip out.
	if (!mNotifyIsOn)
		return ;

	if (!connection)
		err = kIOReturnNoDevice ;

	if ( kIOReturnSuccess == err )
	{
		// set callback for writes to 0
		params[0]	= (UInt32) mKernAddrSpaceRef ;
		params[1]	= (UInt32)(IOAsyncCallback) 0 ;
		params[2]	= (UInt32) this ;
	
		err = io_async_method_scalarI_scalarO(
				connection,
				mUserClient.GetAsyncPort(),
				mPacketAsyncRef,
				1,
				kFWSetAsyncRef_Packet,
				params,
				3,
				params,
				& size) ;
		

		// set callback for skipped packets to 0
		params[0]	= (UInt32) mKernAddrSpaceRef ;
		params[1]	= (UInt32)(IOAsyncCallback) 0 ;
		params[2]	= (UInt32) this ;
		
		err = io_async_method_scalarI_scalarO(
				connection,
				mUserClient.GetAsyncPort(),
				mSkippedPacketAsyncRef,
				1,
				kFWSetAsyncRef_SkippedPacket,
				params,
				3,
				params,
				& size) ;
	}
	
	mNotifyIsOn = false ;
}

void
IOFireWirePseudoAddressSpaceImp::ClientCommandIsComplete(
	FWClientCommandID				commandID,
	IOReturn						status)
{
	io_scalar_inband_t		params	= {(int)mKernAddrSpaceRef, (int)commandID} ;
	mach_msg_type_number_t	size 	= 0 ;
	
	OSStatus err = io_connect_method_scalarI_scalarO(
				mUserClient.GetUserClientConnection(),
				kFWPseudoAddrSpace_ClientCommandIsComplete,
				params,
				2,
				params,
				& size) ;
				
	assert( kIOReturnSuccess == err ) ;
}

void
IOFireWirePseudoAddressSpaceImp::Writer(
	IOFireWireLibPseudoAddressSpaceRef refCon,
	IOReturn						result,
	void**							args,
	int								numArgs)
{
	IOFireWirePseudoAddressSpaceImp* me =  GetThis(refCon);

	if (me->mWriter)
		(me->mWriter)(
			(IOFireWireLibPseudoAddressSpaceRef) refCon,
			(FWClientCommandID) args[0],						// commandID,
			(UInt32) args[1],									// packetSize
			(char*)me->mBuffer + (UInt32)args[2],				// packet
			(UInt16) args[3],									// nodeID
			(UInt32)args[5],									// addr.nodeID, addr.addressHi,
			(UInt32)args[6],
			(UInt32) me->mRefCon) ;								// refCon
}

void
IOFireWirePseudoAddressSpaceImp::SkippedPacketHandler(
	IOFireWireLibPseudoAddressSpaceRef refCon,
	IOReturn						result,
	FWClientCommandID				commandID,
	UInt32							packetCount)
{
	
	IOFireWirePseudoAddressSpaceImp* me = GetThis(refCon) ;

	if (me->mSkippedPacketHandler)
		(me->mSkippedPacketHandler)(
			(IOFireWireLibPseudoAddressSpaceRef) refCon,
			commandID,
			packetCount) ;
}
/*
void
IOFireWirePseudoAddressSpaceImp::NotificationHandler(
	IOFireWirePseudoAddressSpaceImp* that,
	io_service_t					service,
	natural_t						messageType,
	void*							messageArgument)
{
	#if __IOFireWireClientDebug__
	fprintf(stderr, "IOFireWirePseudoAddressSpaceImp::NotificationHandler: that = %08lX, service=%08lX, messageType=%08lX, messageArgument=%08lX\n",
			(UInt32) that,
			(UInt32) service,
			(UInt32) messageType,
			(UInt32) messageArgument) ;
	#endif
	
} */

#pragma mark -
#pragma mark --accessors

const FWAddress&
IOFireWirePseudoAddressSpaceImp::GetFWAddress()
{
	return mFWAddress ;
}

void*
IOFireWirePseudoAddressSpaceImp::GetBuffer()
{
	return mBuffer ;
}

const UInt32
IOFireWirePseudoAddressSpaceImp::GetBufferSize()
{
	return mBufferSize ;
}

void*
IOFireWirePseudoAddressSpaceImp::GetRefCon()
{
	return mRefCon ;
}
