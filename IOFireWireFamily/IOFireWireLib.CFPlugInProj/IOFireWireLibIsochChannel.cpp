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
#import <IOKit/iokitmig.h>
#import <exception>

namespace IOFireWireLib {

	// ============================================================
	// IsochChannel
	// ============================================================
	
	IsochChannel::IsochChannel( IUnknownVTbl* interface, Device& userclient, bool inDoIRM, IOByteCount inPacketSize, IOFWSpeed inPrefSpeed)
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
			throw;//return kIOReturnNoMemory ;

		mUserClient.AddRef() ;

		IOReturn	err = ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), 
								kIsochChannel_Allocate, 3, 1, inDoIRM, inPacketSize, inPrefSpeed, 
								& mKernChannelRef) ;
		if(err)
			throw ;//err !!!
	}
	

	IsochChannel::~IsochChannel()
	{
		if(NotificationIsOn())
			TurnOffNotification() ;
		
		if (mKernChannelRef)
		{
			::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kIsochChannel_Release, 1, 0, 
					mKernChannelRef ) ;
			mKernChannelRef = 0 ;
		}
	
		if ( mTalker )
			(**mTalker).Release( mTalker ) ;
	
		if (mListeners)
		{
			for( CFIndex index = 0; index < ::CFArrayGetCount( mListeners ); ++index )
			{
				IOFireWireLibIsochPortRef	port = (IOFireWireLibIsochPortRef) CFArrayGetValueAtIndex(mListeners, index) ;
				(**port).Release( port ) ;
			}
			CFRelease(mListeners) ;
		}
		
		mUserClient.Release() ;
	}
	
	void
	IsochChannel::ForceStop( ChannelRef refCon, IOReturn result, void** args, int numArgs )
	{
		IsochChannel*	me = (IsochChannel*) args[0] ;
	
		if (me->mForceStopHandler)
			(me->mForceStopHandler)(me->mRefInterface, (UInt32)args[1]) ;	// reason
		
	}
	
	IOReturn
	IsochChannel::SetTalker(
		IOFireWireLibIsochPortRef	 		inTalker )
	{
		(**inTalker).AddRef( inTalker ) ;
		mTalker	= inTalker ;
		
		return kIOReturnSuccess ;
	}
	
	IOReturn
	IsochChannel::AddListener(
		IOFireWireLibIsochPortRef	inListener )
	{
		CFArrayAppendValue(mListeners, inListener) ;
		(**inListener).AddRef( inListener ) ;
		
		return kIOReturnSuccess ;
	}
	
	IOReturn
	IsochChannel::AllocateChannel()
	{
		IOFireWireLibLog_(("+ IsochChannel::AllocateChannel\n")) ;
	
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
		result = IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), 
						kIsochChannel_UserAllocateChannelBegin, 4, 2, mKernChannelRef, mSpeed,
						(UInt32)(allowedChans >> 32), (UInt32)(0xFFFFFFFF & allowedChans), & mSpeed,
						& mChannel) ;
	
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
		IOFireWireLibLog_("+ IsochChannel::ReleaseChannel\n") ;
	
		IOReturn 	result = kIOReturnSuccess ;
		
		if(mTalker) {
			result = (**mTalker).ReleasePort( mTalker );
		}
	
		IOFireWireLibLogIfErr_(result, "IsochChannel::ReleaseChannel: error 0x%08x calling ReleasePort() on talker\n", result) ;
	
		UInt32							listenCount	= CFArrayGetCount(mListeners) ;
		IOFireWireLibIsochPortRef		listen ;
	
		UInt32	index=0 ;
		while (kIOReturnSuccess == result && index < listenCount)
		{
			listen = (IOFireWireLibIsochPortRef)CFArrayGetValueAtIndex(mListeners, index) ;
			result = (**listen).ReleasePort( listen );
	
			IOFireWireLibLogIfErr_(result, "IsochChannel::ReleaseChannel: error %p calling ReleasePort() on listener\n", listen) ;
			
			++index ;
		}
	
		result = ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(),
						kIsochChannel_UserReleaseChannelComplete, 1, 0, mKernChannelRef ) ;
		
		return result ;
	}
	
	IOReturn
	IsochChannel::Start()
	{
		IOFireWireLibLog_("+ IsochChannel::Start\n") ;
	
		// Start all listeners, then start the talker
		UInt32 						listenCount = CFArrayGetCount( mListeners ) ;
		IOFireWireLibIsochPortRef	listen ;
		UInt32						listenIndex = 0 ;
		IOReturn					result = kIOReturnSuccess ;
		
		while (kIOReturnSuccess == result && listenIndex < listenCount)
		{
			listen = (IOFireWireLibIsochPortRef) CFArrayGetValueAtIndex( mListeners, listenIndex) ;
			result = (**listen).Start( listen ) ;
	
			IOFireWireLibLogIfErr_(result, "IsochChannel::Start: error 0x%x starting channel\n", result) ;
				
			++listenIndex ;
		}
		
		if (mTalker)
			(**mTalker).Start( mTalker ) ;
	
		return kIOReturnSuccess;
	}
	
	IOReturn
	IsochChannel::Stop()
	{
		IOFireWireLibLog_(("+ IsochChannel::Stop\n")) ;
	
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
	IsochChannel::SetChannelForceStopHandler(
		IOFireWireIsochChannelForceStopHandler stopProc)
	{
		IOFireWireIsochChannelForceStopHandler oldHandler = mForceStopHandler ;
		mForceStopHandler = stopProc ;
		
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
	IsochChannel::TurnOnNotification()
	{
		IOReturn				err					= kIOReturnSuccess ;
		io_connect_t			connection			= mUserClient.GetUserClientConnection() ;
		io_scalar_inband_t		params ;
		io_scalar_inband_t		output ;
		mach_msg_type_number_t	size = 0 ;
	
		// if notification is already on, skip out.
		if (mNotifyIsOn)
			return true ;
		
		if (!connection)
			err = kIOReturnNoDevice ;
		
		if ( kIOReturnSuccess == err )
		{
			params[0]	= (UInt32) mKernChannelRef ;
			params[1]	= (UInt32)(IOAsyncCallback) & ForceStop ;
			params[2]	= (UInt32) this ;
		
			err = ::io_async_method_scalarI_scalarO( connection, mUserClient.GetAsyncPort(), mAsyncRef,
					1, kSetAsyncRef_IsochChannelForceStop, params, 3, output, & size) ;
			
		}
	
		if ( kIOReturnSuccess == err )
			mNotifyIsOn = true ;
			
		return ( kIOReturnSuccess == err ) ;
	}
	
	void
	IsochChannel::TurnOffNotification()
	{
		io_connect_t			connection			= mUserClient.GetUserClientConnection() ;
		
		// if notification isn't on, skip out.
		if (!mNotifyIsOn || !connection)
			return ;
		
		io_scalar_inband_t		params = { (UInt32) mKernChannelRef, (UInt32)(IOAsyncCallback) 0, (UInt32) this } ;
		mach_msg_type_number_t	size = 0 ;
	
		::io_async_method_scalarI_scalarO( connection, mUserClient.GetAsyncPort(), mAsyncRef, 1, 
				kSetAsyncRef_Packet, params, 3, params, & size) ;
		
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
	: IsochChannel( reinterpret_cast<IUnknownVTbl*>(& sInterface), userclient, inDoIRM, inPacketSize, inPrefSpeed )
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
		return IOFireWireIUnknown::InterfaceMap<IsochChannelCOM>::GetThis(self)->SetChannelForceStopHandler(stopProc) ;
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
		return IOFireWireIUnknown::InterfaceMap<IsochChannelCOM>::GetThis(self)->TurnOnNotification() ;
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
