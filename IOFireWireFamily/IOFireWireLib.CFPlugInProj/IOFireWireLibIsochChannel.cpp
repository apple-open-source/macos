/*
 *  IOFireWireLibIsochChannel.cpp
 *  IOFireWireFamily
 *
 *  Created by NWG on Mon Mar 12 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include "IOFireWireLibIsochChannel.h"
#include "IOFireWireLibIsochPort.h"
#include <IOKit/iokitmig.h>

// ============================================================
// IOFireWireLibIsochChannelImp
// ============================================================

IOFireWireLibIsochChannelImp::IOFireWireLibIsochChannelImp(
	IOFireWireDeviceInterfaceImp&	inUserClient): IOFireWireIUnknown(), 
												   mUserClient(inUserClient),
												   mKernChannelRef(0),
												   mNotifyIsOn(false),
												   mForceStopHandler(0),
												   mUserRefCon(0),
												   mTalker(0),
												   mListeners(0),
												   mRefInterface(0)
{
	mUserClient.AddRef() ;
}

IOFireWireLibIsochChannelImp::~IOFireWireLibIsochChannelImp()
{
	if(NotificationIsOn())
		TurnOffNotification() ;
	
	if (mKernChannelRef)
	{
		IOConnectMethodScalarIScalarO(
				mUserClient.GetUserClientConnection(),
				kFWIsochChannel_Release,
				1,
				0,
				mKernChannelRef) ;
		
		mKernChannelRef = 0 ;
	}

	if (mListeners)
		CFRelease(mListeners) ;
		
	mUserClient.Release() ;
}

IOReturn
IOFireWireLibIsochChannelImp::Init(
	Boolean							inDoIRM,
	IOByteCount						inPacketSize,
	IOFWSpeed						inPrefSpeed)
{
	mListeners = CFArrayCreateMutable( kCFAllocatorDefault, NULL, 0) ;
	if (!mListeners)
		return kIOReturnNoMemory ;
	
	mPrefSpeed = inPrefSpeed ;
	
	IOReturn	result		= kIOReturnSuccess ;
	result = IOConnectMethodScalarIScalarO(
				mUserClient.GetUserClientConnection(),
				kFWIsochChannel_Allocate,
				3,
				1,
				inDoIRM,
				inPacketSize,
				inPrefSpeed,
				& mKernChannelRef) ;
	
	return result ;
}

void
IOFireWireLibIsochChannelImp::ForceStopHandler(
	IOFireWireLibPseudoAddressSpaceRef refCon,
	IOReturn						result,
	void**							args,
	int								numArgs)
{
	IOFireWireLibIsochChannelImp*	me = (IOFireWireLibIsochChannelImp*) args[0] ;

	if (me->mForceStopHandler)
		(me->mForceStopHandler)(me->mRefInterface, (UInt32)args[1]) ;	// reason
	
}

IOReturn
IOFireWireLibIsochChannelImp::SetTalker(
	IOFireWireLibIsochPortRef	 		inTalker )
{
	mTalker	= IOFireWireLibIsochPortCOM::GetThis(inTalker) ;
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireLibIsochChannelImp::AddListener(
	IOFireWireLibIsochPortRef	inListener )
{
	CFArrayAppendValue(mListeners, IOFireWireLibIsochPortCOM::GetThis(inListener)) ;
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireLibIsochChannelImp::AllocateChannel()
{
	fprintf(stderr, "+ IOFireWireLibIsochChannelImp::AllocateChannel\n") ;

	IOReturn	result		= kIOReturnSuccess ;

	IOFWSpeed	portSpeed ;
	UInt64		portChans ;

	// Get best speed, minimum of requested speed and paths from talker to each listener
	mSpeed = mPrefSpeed ;
	
	// reduce speed to minimum of so far and what all ports can do,
	// and find valid channels
	UInt64	allowedChans = ~(UInt64)0 ;
	
	if(mTalker) {
		mTalker->GetSupported( portSpeed, portChans);
		if(portSpeed < mSpeed)
			mSpeed = portSpeed;
		allowedChans &= portChans;
	}
	
	UInt32						listenCount = CFArrayGetCount(mListeners) ;
	IOFireWireLibIsochPortImp*	listen ;
	for (UInt32 listenIndex=0; listenIndex < listenCount; listenIndex++)
	{
		listen = (IOFireWireLibIsochPortImp*)CFArrayGetValueAtIndex(mListeners, listenIndex) ;
		listen->GetSupported( portSpeed, portChans );
		if(portSpeed < mSpeed)
			mSpeed = portSpeed;
		allowedChans &= portChans;
	}

	// call the kernel middle bits
	result = IOConnectMethodScalarIScalarO(
						mUserClient.GetUserClientConnection(),
						kFWIsochChannel_UserAllocateChannelBegin,
						4,
						2,
						mKernChannelRef,
						mSpeed,
//							(UInt32)(allowedChans >> 32),
//							(UInt32)(0xFFFFFFFF & allowedChans),
						allowedChans,
						& mSpeed,
						& mChannel) ;

	if (kIOReturnSuccess == result)
	{
		// complete in user space
		UInt32 listenIndex = 0 ;
		while (kIOReturnSuccess == result && listenIndex < listenCount)
		{
			result = ((IOFireWireLibIsochPortImp*)CFArrayGetValueAtIndex(mListeners, listenIndex))->AllocatePort(mSpeed, mChannel) ;
			listenIndex++ ;
		}
		
		if (kIOReturnSuccess == result && mTalker)
			result = mTalker->AllocatePort(mSpeed, mChannel) ;
	}
		
	return result ;
}

IOReturn
IOFireWireLibIsochChannelImp::ReleaseChannel()
{
	fprintf(stderr, "+ IOFireWireLibIsochChannelImp::ReleaseChannel\n") ;
//	return IOConnectMethodScalarIScalarO(
//						mUserClient.GetUserClientConnection(),
//						kFWIsochChannel_ReleaseChannel,
//						1,
//						0,
//						mKernChannelRef) ;

	IOReturn 	result = kIOReturnSuccess ;
	
	if(mTalker) {
		result = mTalker->ReleasePort();
	}

	if (kIOReturnSuccess != result)
		fprintf(stderr, "IOFireWireLibIsochChannelImp::ReleaseChannel: error 0x%08lX calling ReleasePort() on talker\n", result) ;

	UInt32							listenCount	= CFArrayGetCount(mListeners) ;
	IOFireWireLibIsochPortImp*		listen ;

	UInt32	index=0 ;
	while (kIOReturnSuccess == result && index < listenCount)
	{
		listen = (IOFireWireLibIsochPortImp*)CFArrayGetValueAtIndex(mListeners, index) ;
		result = listen->ReleasePort();

		if (kIOReturnSuccess != result)
			fprintf(stderr, "IOFireWireLibIsochChannelImp::ReleaseChannel: error 0x%08lX calling ReleasePort() on listener\n", listen) ;
		
		index++ ;
	}

	result = IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(),
											kFWIsochChannel_UserReleaseChannelComplete,
											1,
											0,
											mKernChannelRef ) ;
	
	return result ;
}

IOReturn
IOFireWireLibIsochChannelImp::Start()
{
	fprintf(stderr, "+ IOFireWireLibIsochChannelImp::Start\n") ;
//	return IOConnectMethodScalarIScalarO(
//						mUserClient.GetUserClientConnection(),
//						kFWIsochChannel_Start,
//						1,
//						0,
//						mKernChannelRef) ;
//    OSIterator *listenIterator;
//    IOFWIsochPort *listen;

    // Start all listeners, then start the talker
//    listenIterator = OSCollectionIterator::withCollection(fListeners);
	UInt32 						listenCount = CFArrayGetCount( mListeners ) ;
	IOFireWireLibIsochPortImp*	listen ;
	UInt32						listenIndex = 0 ;
	IOReturn					result = kIOReturnSuccess ;
	
	while (kIOReturnSuccess == result && listenIndex < listenCount)
	{
		listen = (IOFireWireLibIsochPortImp*) CFArrayGetValueAtIndex( mListeners, listenIndex) ;
		result = listen->Start() ;
		if (kIOReturnSuccess != result)
			fprintf(stderr, "IOFireWireLibIsochChannelImp::Start: error 0x%08lX starting channel\n", result) ;
			
		listenIndex++ ;
	}
	
	if (mTalker)
		mTalker->Start() ;

    return kIOReturnSuccess;
}

IOReturn
IOFireWireLibIsochChannelImp::Stop()
{
	fprintf(stderr, "+ IOFireWireLibIsochChannelImp::Stop\n") ;
//	return IOConnectMethodScalarIScalarO(
//						mUserClient.GetUserClientConnection(),
//						kFWIsochChannel_Stop,
//						1,
//						0,
//						mKernChannelRef) ;
	if (mTalker)
		mTalker->Stop() ;

	UInt32 						listenCount = CFArrayGetCount( mListeners ) ;
	IOFireWireLibIsochPortImp* 	listen ;
	for (UInt32 listenIndex=0; listenIndex < listenCount; listenIndex++)
	{
		listen = (IOFireWireLibIsochPortImp*)CFArrayGetValueAtIndex( mListeners, listenIndex) ;
		listen->Stop() ;
	}

    return kIOReturnSuccess;
}

IOFireWireIsochChannelForceStopHandler
IOFireWireLibIsochChannelImp::SetChannelForceStopHandler(
	IOFireWireIsochChannelForceStopHandler stopProc)
{
	IOFireWireIsochChannelForceStopHandler oldHandler = mForceStopHandler ;
	mForceStopHandler = stopProc ;
	
	return oldHandler ;
}

void
IOFireWireLibIsochChannelImp::SetRefCon(
	void*			stopProcRefCon)
{
	mUserRefCon = stopProcRefCon ;
}

void*
IOFireWireLibIsochChannelImp::GetRefCon()
{
	return mUserRefCon ;
}

Boolean
IOFireWireLibIsochChannelImp::NotificationIsOn()
{
	return mNotifyIsOn ;
}

Boolean
IOFireWireLibIsochChannelImp::TurnOnNotification()
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

	if ( kIOReturnSuccess == err )
	{
		params[0]	= (UInt32) mKernChannelRef ;
		params[1]	= (UInt32)(IOAsyncCallback) & IOFireWireLibIsochChannelImp::ForceStopHandler ;
		params[2]	= (UInt32) this ;
	
		err = io_async_method_scalarI_scalarO(
				connection,
				mUserClient.GetAsyncPort(),
				mAsyncRef,
				1,
				kFWSetAsyncRef_IsochChannelForceStop,
				params,
				3,
				output,
				& size) ;
		
	}

	if ( kIOReturnSuccess == err )
		mNotifyIsOn = true ;
		
	return ( kIOReturnSuccess == err ) ;
}

void
IOFireWireLibIsochChannelImp::TurnOffNotification()
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
		params[0]	= (UInt32) mKernChannelRef ;
		params[1]	= (UInt32)(IOAsyncCallback) 0 ;
		params[2]	= (UInt32) this ;
	
		err = io_async_method_scalarI_scalarO(
				connection,
				mUserClient.GetAsyncPort(),
				mAsyncRef,
				1,
				kFWSetAsyncRef_Packet,
				params,
				3,
				params,
				& size) ;
	}
	
	mNotifyIsOn = false ;
}

void
IOFireWireLibIsochChannelImp::ClientCommandIsComplete(
	FWClientCommandID 				commandID, 
	IOReturn 						status)
{
}

// ============================================================
// IOFireWireLibIsochChannelCOM
// ============================================================

//
// --- COM ---------------
//
 
IOFireWireIsochChannelInterface	IOFireWireLibIsochChannelCOM::sInterface = 
{
	INTERFACEIMP_INTERFACE,
	1, 0,
	
	& IOFireWireLibIsochChannelCOM::SSetTalker,
	& IOFireWireLibIsochChannelCOM::SAddListener,
	& IOFireWireLibIsochChannelCOM::SAllocateChannel,
	& IOFireWireLibIsochChannelCOM::SReleaseChannel,
	& IOFireWireLibIsochChannelCOM::SStart,
	& IOFireWireLibIsochChannelCOM::SStop,

	& IOFireWireLibIsochChannelCOM::SSetChannelForceStopHandler,
	& IOFireWireLibIsochChannelCOM::SSetRefCon,
	& IOFireWireLibIsochChannelCOM::SGetRefCon,
	& IOFireWireLibIsochChannelCOM::SNotificationIsOn,
	& IOFireWireLibIsochChannelCOM::STurnOnNotification,
	& IOFireWireLibIsochChannelCOM::STurnOffNotification,
	& IOFireWireLibIsochChannelCOM::SClientCommandIsComplete
} ;

//
// --- ctor/dtor -----------------------
//

IOFireWireLibIsochChannelCOM::IOFireWireLibIsochChannelCOM(
	IOFireWireDeviceInterfaceImp&	inUserClient): IOFireWireLibIsochChannelImp(inUserClient)
{
	mInterface.pseudoVTable = (IUnknownVTbl*) & sInterface ;
	mInterface.obj = this ;
	
	mRefInterface = (IOFireWireLibIsochChannelRef) & mInterface.pseudoVTable ;
}

IOFireWireLibIsochChannelCOM::~IOFireWireLibIsochChannelCOM()
{
}

//
// --- IUNKNOWN support ----------------
//
	
IUnknownVTbl**
IOFireWireLibIsochChannelCOM::Alloc(
	IOFireWireDeviceInterfaceImp&	inUserClient,
	Boolean							inDoIRM,
	IOByteCount						inPacketSize,
	IOFWSpeed						inPrefSpeed)
{
	IOFireWireLibIsochChannelCOM*	me ;
	IUnknownVTbl** 					interface 	= NULL ;
	
	me = new IOFireWireLibIsochChannelCOM(inUserClient) ;
	if ( me && (kIOReturnSuccess == me->Init(inDoIRM, inPacketSize, inPrefSpeed)) )
	{
		interface = & me->mInterface.pseudoVTable ;
	}
	else
		delete me ;
		
	return interface ;
}

HRESULT
IOFireWireLibIsochChannelCOM::QueryInterface(REFIID iid, void ** ppv )
{
	HRESULT		result = S_OK ;
	*ppv = nil ;

	CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;

	if ( CFEqual(interfaceID, IUnknownUUID) ||  CFEqual(interfaceID, kIOFireWireIsochChannelInterfaceID) )
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

//
// --- static methods ------------------
//

IOReturn
IOFireWireLibIsochChannelCOM::SSetTalker(
	IOFireWireLibIsochChannelRef 	self, 
	IOFireWireLibIsochPortRef 		inTalker)
{
	return GetThis(self)->SetTalker(inTalker) ;
}

IOReturn
IOFireWireLibIsochChannelCOM::SAddListener(
	IOFireWireLibIsochChannelRef 	self, 
	IOFireWireLibIsochPortRef 		inListener)
{
	return GetThis(self)->AddListener(inListener) ;
}

IOReturn
IOFireWireLibIsochChannelCOM::SAllocateChannel(
	IOFireWireLibIsochChannelRef 	self)
{
	return GetThis(self)->AllocateChannel() ;
}

IOReturn
IOFireWireLibIsochChannelCOM::SReleaseChannel(
	IOFireWireLibIsochChannelRef 	self)
{
	return GetThis(self)->ReleaseChannel() ;
}

IOReturn
IOFireWireLibIsochChannelCOM::SStart(
	IOFireWireLibIsochChannelRef 	self)
{
	return GetThis(self)->Start() ;
}

IOReturn
IOFireWireLibIsochChannelCOM::SStop(
	IOFireWireLibIsochChannelRef 	self)
{
	return GetThis(self)->Stop() ;
}

IOFireWireIsochChannelForceStopHandler
IOFireWireLibIsochChannelCOM::SSetChannelForceStopHandler(
	IOFireWireLibIsochChannelRef 	self, 
	IOFireWireIsochChannelForceStopHandler stopProc)
{
	return GetThis(self)->SetChannelForceStopHandler(stopProc) ;
}

void
IOFireWireLibIsochChannelCOM::SSetRefCon(
	IOFireWireLibIsochChannelRef 	self, 
	void* 							stopProcRefCon)
{
	return GetThis(self)->SetRefCon(stopProcRefCon) ;
}

void*
IOFireWireLibIsochChannelCOM::SGetRefCon(
	IOFireWireLibIsochChannelRef	self)
{
	return GetThis(self)->GetRefCon() ;
}

Boolean
IOFireWireLibIsochChannelCOM::SNotificationIsOn(
	IOFireWireLibIsochChannelRef 	self)
{
	return GetThis(self)->NotificationIsOn() ;
}

Boolean
IOFireWireLibIsochChannelCOM::STurnOnNotification(
	IOFireWireLibIsochChannelRef 	self)
{
	return GetThis(self)->TurnOnNotification() ;
}

void
IOFireWireLibIsochChannelCOM::STurnOffNotification(
	IOFireWireLibIsochChannelRef	self)
{
	GetThis(self)->TurnOffNotification() ;
}

void
IOFireWireLibIsochChannelCOM::SClientCommandIsComplete(
	IOFireWireLibIsochChannelRef 	self, 
	FWClientCommandID 				commandID, 
	IOReturn 						status)
{
	GetThis(self)->ClientCommandIsComplete(commandID, status) ;
}
