/*
 *  IOFireWireLibIsochChannel.cpp
 *  IOFireWireFamily
 *
 *  Created by NWG on Mon Mar 12 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#import "IOFireWireLibIsochChannel.h"
#import "IOFireWireLibIsochPort.h"
#import "IOFireWireLibDevice.h"
#import "IOFireWireLibPriv.h"

#import <IOKit/iokitmig.h>

namespace IOFireWireLib {

	// ============================================================
	// IsochChannel
	// ============================================================
	
	IsochChannel::IsochChannel( const IUnknownVTbl & interface, Device& userclient, bool inDoIRM, IOByteCount inPacketSize, IOFWSpeed inPrefSpeed)
	: IOFireWireIUnknown( interface ), 
	  mUserClient( userclient ),
	  mKernChannelRef(0),
	  mNotifyIsOn(false),
	  mForceStopHandler(0),
	  mUserRefCon(0),
	  mTalker(0),
	  mListeners( ::CFArrayCreateMutable( kCFAllocatorDefault, 0, NULL ) ),
	  mRefInterface( reinterpret_cast<ChannelRef>( & GetInterface() ) ),
	  mPrefSpeed( inPrefSpeed )
	{
		if (!mListeners)
			throw kIOReturnNoMemory ;

		mUserClient.AddRef() ;

		uint32_t outputCnt = 1;
		uint64_t outputVal = 0;
		const uint64_t inputs[3] = {inDoIRM, inPacketSize, inPrefSpeed};
		IOReturn err = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(),
												 kIsochChannel_Allocate,
												 inputs,3,
												 &outputVal,&outputCnt);
		mKernChannelRef = (UserObjectHandle) outputVal;
		
		if(err)
		{
			throw err ;
		}
	}
	

	IsochChannel::~IsochChannel()
	{
		if(NotificationIsOn())
			TurnOffNotification() ;
		
		if (mKernChannelRef)
		{
			uint32_t outputCnt = 0;
			const uint64_t inputs[1]={(const uint64_t)mKernChannelRef};

			IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(),
									  kReleaseUserObject,
									  inputs,1,
									  NULL,&outputCnt);
			
			mKernChannelRef = 0 ;
		}
	
		if ( mTalker )
			(**mTalker).Release( mTalker ) ;
	
		if (mListeners)
		{
			for( CFIndex index = 0, count = ::CFArrayGetCount( mListeners ); index < count; ++index )
			{
				IOFireWireLibIsochPortRef	port = (IOFireWireLibIsochPortRef) CFArrayGetValueAtIndex(mListeners, index) ;
				(**port).Release( port ) ;
			}
			CFRelease(mListeners) ;
		}
		
		mUserClient.Release() ;
	}
	
//	void
//	IsochChannel::ForceStop( ChannelRef refCon, IOReturn result, void** args, int numArgs )
//	{
//		IsochChannel*	me = (IsochChannel*) args[0] ;
//	
//		if (me->mForceStopHandler)
//			(me->mForceStopHandler)(me->mRefInterface, (UInt32)args[1]) ;	// reason
//		
//	}
	
	IOReturn
	IsochChannel::SetTalker(
		IOFireWireLibIsochPortRef	 		inTalker )
	{
		(**inTalker).AddRef( inTalker ) ;
		mTalker	= inTalker ;
		
		IsochPort * port = dynamic_cast<LocalIsochPort*>( IOFireWireIUnknown::InterfaceMap<IsochPort>::GetThis( inTalker ) ) ;
		
		if ( port )
		{
			uint32_t outputCnt = 0;
			const uint64_t inputs[2] = {(const uint64_t)port->mKernPortRef, (const uint64_t)mKernChannelRef};
			IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(),
									  kLocalIsochPort_SetChannel,
									  inputs,2,
									  NULL,&outputCnt);
		}

		return kIOReturnSuccess ;
	}
	
	IOReturn
	IsochChannel::AddListener(
		IOFireWireLibIsochPortRef	inListener )
	{
		CFArrayAppendValue(mListeners, inListener) ;
		(**inListener).AddRef( inListener ) ;

		IsochPort * port = dynamic_cast<LocalIsochPort*>( IOFireWireIUnknown::InterfaceMap<IsochPort>::GetThis( inListener ) ) ;
		
		if ( port )
		{
			uint32_t outputCnt = 0;
			const uint64_t inputs[2] = {(const uint64_t)port->mKernPortRef, (const uint64_t)mKernChannelRef};
			IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(),
									  kLocalIsochPort_SetChannel,
									  inputs,2,
									  NULL,&outputCnt);
		}
		
		return kIOReturnSuccess ;
	}
	
	IOReturn
	IsochChannel::AllocateChannel()
	{
		DebugLog( "+ IsochChannel::AllocateChannel\n" ) ;
	
		IOReturn	result		= kIOReturnSuccess ;
	
		IOFWSpeed	portSpeed ;
		UInt64		portChans ;
	
		// Get best speed, minimum of requested speed and paths from talker to each listener
		mSpeed = mPrefSpeed ;
		
		// reduce speed to minimum of so far and what all ports can do,
		// and find valid channels
		UInt64	allowedChans = ~(UInt64)0 ;
		
		if(mTalker) {
			(**mTalker).GetSupported( mTalker, & portSpeed, & portChans);
			if(portSpeed < mSpeed)
				mSpeed = portSpeed;
			allowedChans &= portChans;
		}
		
		UInt32						listenCount = CFArrayGetCount(mListeners) ;
		IOFireWireLibIsochPortRef	listen ;
		for (UInt32 listenIndex=0; listenIndex < listenCount; ++listenIndex)
		{
			listen = (IOFireWireLibIsochPortRef)CFArrayGetValueAtIndex( mListeners, listenIndex) ;
			(**listen).GetSupported( listen, & portSpeed, & portChans );
	
			if(portSpeed < mSpeed)
				mSpeed = portSpeed;
			allowedChans &= portChans;
		}
	
		// call the kernel middle bits
		uint32_t outputCnt = 2;
		uint64_t outputVal[2];
		const uint64_t inputs[4] = {(const uint64_t)mKernChannelRef, (const uint64_t)mSpeed, (allowedChans >> 32),(0xFFFFFFFF & allowedChans)};
		result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(),
										   kIsochChannel_UserAllocateChannelBegin,
												 inputs,4,
												 outputVal,&outputCnt);
		mSpeed = (IOFWSpeed) (outputVal[0] & 0xFFFFFFFF);
		mChannel =  outputVal[1] & 0xFFFFFFFF;
		
		if (kIOReturnSuccess == result)
		{
			// complete in user space
			UInt32 listenIndex = 0 ;
			while (kIOReturnSuccess == result && listenIndex < listenCount)
			{
				IOFireWireLibIsochPortRef port = (IOFireWireLibIsochPortRef)CFArrayGetValueAtIndex(mListeners, listenIndex) ;
				result = (**port).AllocatePort( port, mSpeed, mChannel ) ;
				++listenIndex ;
			}
			
			if (kIOReturnSuccess == result && mTalker)
				result = (**mTalker).AllocatePort( mTalker, mSpeed, mChannel) ;
		}
			
		return result ;
	}
	
	IOReturn
	IsochChannel::ReleaseChannel()
	{
		DebugLog("+ IsochChannel::ReleaseChannel\n") ;
	
		IOReturn 	result = kIOReturnSuccess ;
		
		if(mTalker) {
			result = (**mTalker).ReleasePort( mTalker );
		}
	
		DebugLogCond( result, "IsochChannel::ReleaseChannel: error 0x%08x calling ReleasePort() on talker\n", result) ;
	
		UInt32							listenCount	= CFArrayGetCount(mListeners) ;
		IOFireWireLibIsochPortRef		listen ;
	
		UInt32	index=0 ;
		while (kIOReturnSuccess == result && index < listenCount)
		{
			listen = (IOFireWireLibIsochPortRef)CFArrayGetValueAtIndex(mListeners, index) ;
			result = (**listen).ReleasePort( listen );
	
			DebugLogCond(result, "IsochChannel::ReleaseChannel: error %p calling ReleasePort() on listener\n", listen) ;
			
			++index ;
		}
	
		uint32_t outputCnt = 0;
		result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(),
										   mUserClient.MakeSelectorWithObject( kIsochChannel_UserReleaseChannelComplete_d, mKernChannelRef ),
										   NULL,0,
										   NULL,&outputCnt);
		
		return result ;
	}
	
	IOReturn
	IsochChannel::Start()
	{
		DebugLog("+ IsochChannel::Start\n") ;
	
		// Start all listeners, then start the talker
		UInt32 						listenCount = CFArrayGetCount( mListeners ) ;
		IOFireWireLibIsochPortRef	listen ;
		UInt32						listenIndex = 0 ;
		IOReturn					error = kIOReturnSuccess ;
		
		while ( !error && listenIndex < listenCount )
		{
			listen = (IOFireWireLibIsochPortRef) CFArrayGetValueAtIndex( mListeners, listenIndex) ;
			error = (**listen).Start( listen ) ;
	
			DebugLogCond( error, "IsochChannel::Start: error 0x%x starting channel\n", error ) ;
				
			++listenIndex ;
		}
		
		if ( mTalker && !error )
			error = (**mTalker).Start( mTalker ) ;
	
		if ( error )
			Stop() ;
	
		DebugLog("-IsochChannel::Start error=%x\n", error) ;
		
		return error ;
	}
	
	IOReturn
	IsochChannel::Stop()
	{
		DebugLog( "+ IsochChannel::Stop\n" ) ;
	
		if (mTalker)
			(**mTalker).Stop( mTalker ) ;
	
		UInt32 						listenCount = CFArrayGetCount( mListeners ) ;
		IOFireWireLibIsochPortRef 	listen ;
		for (UInt32 listenIndex=0; listenIndex < listenCount; ++listenIndex)
		{
			listen = (IOFireWireLibIsochPortRef)CFArrayGetValueAtIndex( mListeners, listenIndex ) ;
			(**listen).Stop( listen ) ;
		}
	
		return kIOReturnSuccess;
	}

	IOFireWireIsochChannelForceStopHandler
	IsochChannel::SetChannelForceStopHandler (
		IOFireWireIsochChannelForceStopHandler stopProc,
		IOFireWireLibIsochChannelRef interface )
	{
		DebugLog( "+IsochChannel::SetChannelForceStopHandler this=%p, proc=%p\n", this, stopProc ) ;
	
		IOFireWireIsochChannelForceStopHandler oldHandler = mForceStopHandler ;
		mForceStopHandler = stopProc ;

		io_connect_t connection = mUserClient.GetUserClientConnection() ;

		if ( mNotifyIsOn && connection )
		{
			uint64_t refrncData[kOSAsyncRef64Count];
			refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) mForceStopHandler;
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long)interface;
			uint32_t outputCnt = 0;
			const uint64_t inputs[1]={(const uint64_t)mKernChannelRef};

			IOReturn error = IOConnectCallAsyncScalarMethod(connection,
															kSetAsyncRef_IsochChannelForceStop,
															mUserClient.GetAsyncPort(), 
															refrncData,kOSAsyncRef64Count,
															inputs,1,
															NULL,&outputCnt);
			
			#pragma unused( error )
			DebugLogCond( error, "Error setting isoch channel force stop handler\n") ;
		}
		
		return oldHandler ;
	}
	
	void
	IsochChannel::SetRefCon(
		void*			stopProcRefCon)
	{
		mUserRefCon = stopProcRefCon ;
	}
	
	void*
	IsochChannel::GetRefCon()
	{
		return mUserRefCon ;
	}
	
	Boolean
	IsochChannel::NotificationIsOn()
	{
		return mNotifyIsOn ;
	}
	
	Boolean
	IsochChannel::TurnOnNotification( IOFireWireLibIsochChannelRef interface )
	{
		// if notification is already on, skip out.
		if (mNotifyIsOn)
		{
			return true ;
		}
		
		io_connect_t			connection			= mUserClient.GetUserClientConnection() ;	
		
		if (!connection)
		{
			DebugLog("IsochChannel::TurnOnNotification: user client not open!\n") ;
			return false ;
		}
		
		IOReturn error = kIOReturnSuccess ;
		{
			uint64_t refrncData[kOSAsyncRef64Count];
			refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) mForceStopHandler;
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long)interface;
			uint32_t outputCnt = 0;
			const uint64_t inputs[1]={(const uint64_t)mKernChannelRef};

			error = IOConnectCallAsyncScalarMethod(connection,
												   kSetAsyncRef_IsochChannelForceStop,
												   mUserClient.GetAsyncPort(), 
												   refrncData,kOSAsyncRef64Count,
												   inputs,1,
												   NULL,&outputCnt);
		}

		{
			unsigned count = ::CFArrayGetCount( mListeners ) ;
			unsigned index = 0 ;
			while( index < count && !error )
			{
				// have kernel channel force stop proc call user dcl program force stop proc

				LocalIsochPort * port = dynamic_cast<LocalIsochPort*>(IOFireWireIUnknown::InterfaceMap<IsochPort>::GetThis( (IsochPort*) ::CFArrayGetValueAtIndex( mListeners, index ) ) ) ;
				if ( port )
				{
					
					uint32_t outputCnt = 0;
					const uint64_t inputs[2] = {(const uint64_t)port->mKernPortRef, (const uint64_t)mKernChannelRef};
					error = IOConnectCallScalarMethod(connection,
													  kLocalIsochPort_SetChannel,
													  inputs,2,
													  NULL,&outputCnt);
				}
				
				++index ;
			}
		}
		
		if ( !error && mTalker )
		{
			LocalIsochPort * port = dynamic_cast<LocalIsochPort*>( IOFireWireIUnknown::InterfaceMap<IsochPort>::GetThis( mTalker ) ) ;
			if ( port )
			{
				uint32_t outputCnt = 0;
				const uint64_t inputs[2] = {(const uint64_t)port->mKernPortRef, (const uint64_t)mKernChannelRef};
				error = IOConnectCallScalarMethod(connection,
												  kLocalIsochPort_SetChannel,
												  inputs,2,
												  NULL,&outputCnt);
			}
		}
		
		if ( !error )
		{
			mNotifyIsOn = true ;
		}
			
		return ( error == kIOReturnSuccess ) ;
	}
	
	void
	IsochChannel::TurnOffNotification()
	{
		io_connect_t			connection			= mUserClient.GetUserClientConnection() ;
		
		// if notification isn't on, skip out.
		if ( !mNotifyIsOn || !connection )
		{
			return ;
		}
		
		uint64_t refrncData[kOSAsyncRef64Count];
		refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) 0;
		refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long) 0;
		const uint64_t inputs[3] = {(const uint64_t)mKernChannelRef,0,(const uint64_t)this};
		uint32_t outputCnt = 0;
		IOConnectCallAsyncScalarMethod(connection,
									   kSetAsyncRef_IsochChannelForceStop,
									   mUserClient.GetAsyncPort(), 
									   refrncData,kOSAsyncRef64Count,
									   inputs,3,
									   NULL,&outputCnt);
		mNotifyIsOn = false ;
	}
	
	void
	IsochChannel::ClientCommandIsComplete(
		FWClientCommandID 				commandID, 
		IOReturn 						status)
	{
	}
	
#pragma mark -
	IsochChannelCOM::Interface IsochChannelCOM::sInterface = 
	{
		INTERFACEIMP_INTERFACE,
		1, 0,
		
		& IsochChannelCOM::SSetTalker,
		& IsochChannelCOM::SAddListener,
		& IsochChannelCOM::SAllocateChannel,
		& IsochChannelCOM::SReleaseChannel,
		& IsochChannelCOM::SStart,
		& IsochChannelCOM::SStop,
	
		& IsochChannelCOM::SSetChannelForceStopHandler,
		& IsochChannelCOM::SSetRefCon,
		& IsochChannelCOM::SGetRefCon,
		& IsochChannelCOM::SNotificationIsOn,
		& IsochChannelCOM::STurnOnNotification,
		& IsochChannelCOM::STurnOffNotification,
		& IsochChannelCOM::SClientCommandIsComplete
	} ;
	
	//
	// --- ctor/dtor -----------------------
	//
	
	IsochChannelCOM::IsochChannelCOM( Device& userclient, bool inDoIRM, IOByteCount inPacketSize, IOFWSpeed inPrefSpeed )
	: IsochChannel( reinterpret_cast<const IUnknownVTbl &>( sInterface ), userclient, inDoIRM, inPacketSize, inPrefSpeed )
	{
	}
	
	IsochChannelCOM::~IsochChannelCOM()
	{
	}
	
	//
	// --- IUNKNOWN support ----------------
	//
		
	IUnknownVTbl**
	IsochChannelCOM::Alloc( Device& userclient, Boolean inDoIRM, IOByteCount inPacketSize, IOFWSpeed inPrefSpeed )
	{
		IsochChannelCOM*	me = nil ;
		
		try {
			me = new IsochChannelCOM( userclient, inDoIRM, inPacketSize, inPrefSpeed ) ;
		} catch(...) {
		}

		return ( nil == me ) ? nil : reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	HRESULT
	IsochChannelCOM::QueryInterface(REFIID iid, void ** ppv )
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if ( CFEqual(interfaceID, IUnknownUUID) ||  CFEqual(interfaceID, kIOFireWireIsochChannelInterfaceID) )
		{
			*ppv = & GetInterface() ;
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
	IsochChannelCOM::SSetTalker( ChannelRef self, 
		IOFireWireLibIsochPortRef 		inTalker)
	{
		return IOFireWireIUnknown::InterfaceMap<IsochChannelCOM>::GetThis(self)->SetTalker(inTalker) ;
	}
	
	IOReturn
	IsochChannelCOM::SAddListener(
		ChannelRef 	self, 
		IOFireWireLibIsochPortRef 		inListener)
	{
		return IOFireWireIUnknown::InterfaceMap<IsochChannelCOM>::GetThis(self)->AddListener(inListener) ;
	}
	
	IOReturn
	IsochChannelCOM::SAllocateChannel(
		ChannelRef 	self)
	{
		return IOFireWireIUnknown::InterfaceMap<IsochChannelCOM>::GetThis(self)->AllocateChannel() ;
	}
	
	IOReturn
	IsochChannelCOM::SReleaseChannel(
		ChannelRef 	self)
	{
		return IOFireWireIUnknown::InterfaceMap<IsochChannelCOM>::GetThis(self)->ReleaseChannel() ;
	}
	
	IOReturn
	IsochChannelCOM::SStart(
		ChannelRef 	self)
	{
		return IOFireWireIUnknown::InterfaceMap<IsochChannelCOM>::GetThis(self)->Start() ;
	}
	
	IOReturn
	IsochChannelCOM::SStop(
		ChannelRef 	self)
	{
		return IOFireWireIUnknown::InterfaceMap<IsochChannelCOM>::GetThis(self)->Stop() ;
	}
	
	IOFireWireIsochChannelForceStopHandler
	IsochChannelCOM::SSetChannelForceStopHandler( ChannelRef self, ForceStopHandler stopProc )
	{
		return IOFireWireIUnknown::InterfaceMap<IsochChannelCOM>::GetThis(self)->SetChannelForceStopHandler( stopProc, self ) ;
	}
	
	void
	IsochChannelCOM::SSetRefCon( ChannelRef self, void* refcon )
	{
		return IOFireWireIUnknown::InterfaceMap<IsochChannelCOM>::GetThis(self)->SetRefCon( refcon ) ;
	}
	
	void*
	IsochChannelCOM::SGetRefCon( ChannelRef self )
	{
		return IOFireWireIUnknown::InterfaceMap<IsochChannelCOM>::GetThis(self)->GetRefCon() ;
	}
	
	Boolean
	IsochChannelCOM::SNotificationIsOn( ChannelRef self )
	{
		return IOFireWireIUnknown::InterfaceMap<IsochChannelCOM>::GetThis(self)->NotificationIsOn() ;
	}
	
	Boolean
	IsochChannelCOM::STurnOnNotification( ChannelRef self )
	{
		return IOFireWireIUnknown::InterfaceMap<IsochChannelCOM>::GetThis(self)->TurnOnNotification( self ) ;
	}
	
	void
	IsochChannelCOM::STurnOffNotification( ChannelRef self)
	{
		IOFireWireIUnknown::InterfaceMap<IsochChannelCOM>::GetThis(self)->TurnOffNotification() ;
	}
	
	void
	IsochChannelCOM::SClientCommandIsComplete( ChannelRef self, FWClientCommandID commandID, IOReturn status )
	{
		IOFireWireIUnknown::InterfaceMap<IsochChannelCOM>::GetThis(self)->ClientCommandIsComplete(commandID, status) ;
	}
}
