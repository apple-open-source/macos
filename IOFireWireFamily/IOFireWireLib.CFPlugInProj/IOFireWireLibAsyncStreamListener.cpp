/*
 *  IOFireWireLibAsyncStreamListener.cpp
 *  IOFireWireFamily
 *
 *  Created by Arul on Thu Sep 28 2006.
 *  Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
 *
 *	$ Log:IOFireWireLibAsyncStreamListener.cpp,v $
 */

#import "IOFireWireLibAsyncStreamListener.h"
#import "IOFireWireLibDevice.h"
#import "IOFireWireLibPriv.h"

#import <IOKit/iokitmig.h>

namespace IOFireWireLib {

#pragma mark AsyncStreamListener -

	// ============================================================
	// AsyncStreamListener
	// ============================================================
	AsyncStreamListener::AsyncStreamListener( const IUnknownVTbl&		interface, 
													Device&				userclient,
													UserObjectHandle	inKernAddrSpaceRef,
													void*				inBuffer,
													UInt32				inBufferSize,
													void*				inCallBack,
													void*				inRefCon	)
	: IOFireWireIUnknown( interface ), 
		mUserClient(userclient), 
		mKernAsyncStreamListenerRef(inKernAddrSpaceRef),
		mBuffer((char*)inBuffer),
		mNotifyIsOn(false),
		mUserRefCon(inRefCon),
		mBufferSize(inBufferSize),
		mListener( (AsyncStreamListenerHandler) inCallBack ),
		mSkippedPacketHandler( nil ),
		mRefInterface( reinterpret_cast<AsyncStreamListenerRef>( & GetInterface() ) )
	{
		userclient.AddRef() ;
	}
	

	AsyncStreamListener::~AsyncStreamListener()
	{
		uint32_t outputCnt = 0;
		const uint64_t inputs[1]={(const uint64_t)mKernAsyncStreamListenerRef};

		IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
								  kReleaseUserObject,
								  inputs,1,
								  NULL,&outputCnt);
		
		if( mBuffer and mBufferSize > 0 )	
		{
			delete[] mBuffer;
			mBuffer		= 0;
			mBufferSize = 0;
		}
			
		mUserClient.Release() ;
	}

	const IOFWAsyncStreamListenerHandler 
	AsyncStreamListener::SetListenerHandler ( AsyncStreamListenerRef		self, 
											  AsyncStreamListenerHandler	inReceiver )
	{
		AsyncStreamListenerHandler oldListener = mListener ;
		mListener = inReceiver ;
		
		return oldListener ;
	}

	const IOFWAsyncStreamListenerSkippedPacketHandler	
	AsyncStreamListener::SetSkippedPacketHandler( AsyncStreamListenerRef	self, 
												  AsyncStreamSkippedPacketHandler		inHandler )
	{
		AsyncStreamSkippedPacketHandler oldHandler = mSkippedPacketHandler;
		mSkippedPacketHandler = inHandler;
		
		return oldHandler;
	}

	Boolean 
	AsyncStreamListener::NotificationIsOn ( AsyncStreamListenerRef self ) 
	{
		return mNotifyIsOn ;
	}

	Boolean 
	AsyncStreamListener::TurnOnNotification ( AsyncStreamListenerRef self )
	{
		IOReturn				err					= kIOReturnSuccess ;
		io_connect_t			connection			= mUserClient.GetUserClientConnection() ;
	
		// if notification is already on, skip out.
		if (mNotifyIsOn)
			return true ;
		
		if (!connection)
			err = kIOReturnNoDevice ;

		if ( kIOReturnSuccess == err )
		{
			uint64_t refrncData[kOSAsyncRef64Count];
			refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) 0;
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long) 0;
			const uint64_t inputs[3] = {(const uint64_t)mKernAsyncStreamListenerRef,(const uint64_t)& Listener,(const uint64_t)self};
			uint32_t outputCnt = 0;
			err = IOConnectCallAsyncScalarMethod(connection,
												 kSetAsyncStreamRef_Packet,
												 mUserClient.GetIsochAsyncPort(),
												 refrncData,kOSAsyncRef64Count,
												 inputs,3,
												 NULL,&outputCnt);
		}
		
		if ( kIOReturnSuccess == err)
		{
	
			uint64_t refrncData[kOSAsyncRef64Count];
			refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) 0;
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long) 0;
			const uint64_t inputs[3] = {(const uint64_t)mKernAsyncStreamListenerRef,(const uint64_t)& SkippedPacket,(const uint64_t)self};
			uint32_t outputCnt = 0;
			err = IOConnectCallAsyncScalarMethod(connection,
												 kSetAsyncStreamRef_SkippedPacket,
												 mUserClient.GetIsochAsyncPort(),
												 refrncData,kOSAsyncRef64Count,
												 inputs,3,
												 NULL,&outputCnt);
		}

		if ( kIOReturnSuccess == err )
		{
			uint32_t outputCnt = 0;
			const uint64_t inputs[1]={(const uint64_t)mKernAsyncStreamListenerRef};

			err = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
											kAsyncStreamListener_TurnOnNotification,
											inputs,1,
											NULL,&outputCnt);
		}

		
		if ( kIOReturnSuccess == err )
			mNotifyIsOn = true ;
			
		return ( kIOReturnSuccess == err ) ;
	}

	void 
	AsyncStreamListener::TurnOffNotification ( AsyncStreamListenerRef self )
	{
		IOReturn				err			= kIOReturnSuccess ;
		io_connect_t			connection	= mUserClient.GetUserClientConnection() ;

		// if notification isn't on, skip out.
		if (!mNotifyIsOn)
			return ;
	
		if (!connection)
			err = kIOReturnNoDevice ;
	
		if ( kIOReturnSuccess == err )
		{
			uint64_t refrncData[kOSAsyncRef64Count];
			refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) 0;
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long) 0;
			const uint64_t inputs[3] = {(const uint64_t)mKernAsyncStreamListenerRef,0,(const uint64_t)self};
			uint32_t outputCnt = 0;

			// set callback for writes to 0
			err = IOConnectCallAsyncScalarMethod(connection,
												 kSetAsyncStreamRef_Packet,
												 mUserClient.GetIsochAsyncPort(),
												 refrncData,kOSAsyncRef64Count,
												 inputs,3,
												 NULL,&outputCnt);

			outputCnt = 0;
			// set callback for skipped packets to 0
			err = IOConnectCallAsyncScalarMethod(connection,
												 kSetAsyncStreamRef_SkippedPacket,
												 mUserClient.GetIsochAsyncPort(),
												 refrncData,kOSAsyncRef64Count,
												 inputs,3,
												 NULL,&outputCnt);
		}

		if ( kIOReturnSuccess == err )
		{
			uint32_t outputCnt = 0;
			const uint64_t inputs[1]={(const uint64_t)mKernAsyncStreamListenerRef};

			err = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
											kAsyncStreamListener_TurnOffNotification,
											inputs,1,
											NULL,&outputCnt);
		}
		
		mNotifyIsOn = false ;
	}

	void 
	AsyncStreamListener::ClientCommandIsComplete ( AsyncStreamListenerRef	self,
												   FWClientCommandID		commandID )
	{
		uint32_t		outputCnt = 0;
		const uint64_t	inputs[2] = {(const uint64_t)mKernAsyncStreamListenerRef, (const uint64_t)commandID};

		#if IOFIREWIREUSERCLIENTDEBUG > 0
		OSStatus err = 
		#endif

		IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kAsyncStreamListener_ClientCommandIsComplete,
													inputs,2,
													NULL,&outputCnt);
		
#ifdef __LP64__
		DebugLogCond( err, "AsyncStreamListener::ClientCommandIsComplete: err=0x%08X\n", err ) ;
#else
		DebugLogCond( err, "AsyncStreamListener::ClientCommandIsComplete: err=0x%08lX\n", err ) ;
#endif
	}

	void* 
	AsyncStreamListener::GetRefCon	( AsyncStreamListenerRef self )	
	{
		return mUserRefCon;
	}

	void 
	AsyncStreamListener::SetFlags ( AsyncStreamListenerRef		self,
									UInt32						flags )
	{
		uint32_t outputCnt = 0;
		const uint64_t inputs[2] = {(const uint64_t)mKernAsyncStreamListenerRef, flags};
		IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
								  kAsyncStreamListener_SetFlags,
								  inputs,2,
								  NULL,&outputCnt);
		
		mFlags = flags;
	}
		
	UInt32 
	AsyncStreamListener::GetFlags ( AsyncStreamListenerRef	self )	
	{
		uint32_t outputCnt = 1;
		uint64_t outputVal = 0;
		const uint64_t inputs[1]={(const uint64_t)mKernAsyncStreamListenerRef};

		IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
								  kAsyncStreamListener_GetFlags,
								  inputs,1,
								  &outputVal,&outputCnt);
		mFlags = outputVal & 0xFFFFFFFF;
		
		return mFlags;
	}

	UInt32
	AsyncStreamListener::GetOverrunCounter ( AsyncStreamListenerRef	self )
	{
		UInt32 counter = 0;
		uint32_t outputCnt = 1;
		uint64_t outputVal = 0;
		const uint64_t inputs[1]={(const uint64_t)mKernAsyncStreamListenerRef};

		IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
								  kAsyncStreamListener_GetOverrunCounter,
								  inputs,1,
								  &outputVal,&outputCnt);
		counter = outputVal & 0xFFFFFFFF;
		return counter;
	}

	void
	AsyncStreamListener::Listener( AsyncStreamListenerRef refcon, IOReturn result, void** args, int numArgs)
	{
		AsyncStreamListener* me = IOFireWireIUnknown::InterfaceMap<AsyncStreamListener>::GetThis(refcon) ;
		
		if ( ! me->mListener )
		{
			me->ClientCommandIsComplete( (AsyncStreamListenerRef) refcon, args[0] ) ;
			return ;
		}
		else
		{
			(me->mListener)(
				(AsyncStreamListenerRef) refcon,
				(FWClientCommandID) args[0],						// commandID,
				(unsigned long)(args[1]),									// size
				me->mBuffer + (unsigned long)(args[2]),					// packet
				(void*) me->mUserRefCon) ;								// refcon
		}
	}
	
	void
	AsyncStreamListener::SkippedPacket( AsyncStreamListenerRef refcon, IOReturn result, FWClientCommandID commandID, UInt32 packetCount)
	{
		AsyncStreamListener* me = IOFireWireIUnknown::InterfaceMap<AsyncStreamListener>::GetThis(refcon) ;
	
		if (me->mSkippedPacketHandler)
			(me->mSkippedPacketHandler)( refcon, commandID, packetCount) ;
		else
			me->ClientCommandIsComplete( refcon, commandID ) ;
	}


#pragma mark AsyncStreamListenerCOM -

	AsyncStreamListenerCOM::Interface AsyncStreamListenerCOM::sInterface = 
	{
		INTERFACEIMP_INTERFACE,
		1, 0,
		
		&AsyncStreamListenerCOM::SSetListenerHandler,
		&AsyncStreamListenerCOM::SSetSkippedPacketHandler,
		&AsyncStreamListenerCOM::SNotificationIsOn,
		&AsyncStreamListenerCOM::STurnOnNotification,
		&AsyncStreamListenerCOM::STurnOffNotification, 
		&AsyncStreamListenerCOM::SClientCommandIsComplete,
		&AsyncStreamListenerCOM::SGetRefCon,
		&AsyncStreamListenerCOM::SSetFlags,
		&AsyncStreamListenerCOM::SGetFlags,
		&AsyncStreamListenerCOM::SGetOverrunCounter
	} ;
	
	//
	// --- ctor/dtor -----------------------
	//
	
	AsyncStreamListenerCOM::AsyncStreamListenerCOM( Device&				userclient,
													UserObjectHandle	inKernAddrSpaceRef,
													void*				inBuffer,
													UInt32				inBufferSize,
													void*				inCallBack,
													void*				inRefCon )
	: AsyncStreamListener( reinterpret_cast<const IUnknownVTbl &>( sInterface ), userclient, inKernAddrSpaceRef, inBuffer, 
										inBufferSize, inCallBack, inRefCon )
	{
	}
	
	AsyncStreamListenerCOM::~AsyncStreamListenerCOM()
	{
	}
	
	//
	// --- IUNKNOWN support ----------------
	//
		
	IUnknownVTbl**
	AsyncStreamListenerCOM::Alloc(	Device&				userclient,
									UserObjectHandle	inKernAddrSpaceRef,
									void*				inBuffer,
									UInt32				inBufferSize,
									void*				inCallBack,
									void*				inRefCon )
	{
		AsyncStreamListenerCOM*	me = nil ;
		
		try {
			me = new AsyncStreamListenerCOM( userclient, inKernAddrSpaceRef, inBuffer, inBufferSize, inCallBack, inRefCon ) ;
		} catch(...) {
		}

		return ( nil == me ) ? nil : reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	HRESULT
	AsyncStreamListenerCOM::QueryInterface(REFIID iid, void ** ppv )
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if ( CFEqual(interfaceID, IUnknownUUID) ||  CFEqual(interfaceID, kIOFireWireAsyncStreamListenerInterfaceID) )
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
	const IOFWAsyncStreamListenerHandler 
	AsyncStreamListenerCOM::SSetListenerHandler ( AsyncStreamListenerRef		self, 
												  AsyncStreamListenerHandler	inReceiver )
	{
		return IOFireWireIUnknown::InterfaceMap<AsyncStreamListenerCOM>::GetThis(self)->SetListenerHandler( self, inReceiver ) ;
	}

	const IOFWAsyncStreamListenerSkippedPacketHandler	
	AsyncStreamListenerCOM::SSetSkippedPacketHandler( AsyncStreamListenerRef	self, 
													  AsyncStreamSkippedPacketHandler		inHandler )
	{
		return IOFireWireIUnknown::InterfaceMap<AsyncStreamListenerCOM>::GetThis(self)->SetSkippedPacketHandler( self, inHandler ) ;
	}

	Boolean 
	AsyncStreamListenerCOM::SNotificationIsOn ( AsyncStreamListenerRef self ) 
	{
		return IOFireWireIUnknown::InterfaceMap<AsyncStreamListenerCOM>::GetThis(self)->NotificationIsOn( self ) ;
	}

	Boolean 
	AsyncStreamListenerCOM::STurnOnNotification ( AsyncStreamListenerRef self )
	{
		return IOFireWireIUnknown::InterfaceMap<AsyncStreamListenerCOM>::GetThis(self)->TurnOnNotification( self ) ;
	}

	void 
	AsyncStreamListenerCOM::STurnOffNotification ( AsyncStreamListenerRef self )
	{
		return IOFireWireIUnknown::InterfaceMap<AsyncStreamListenerCOM>::GetThis(self)->TurnOffNotification( self ) ;
	}

	void 
	AsyncStreamListenerCOM::SClientCommandIsComplete ( AsyncStreamListenerRef	self,
													   FWClientCommandID		commandID,
													   IOReturn					status )
	{
		IOFireWireIUnknown::InterfaceMap<AsyncStreamListenerCOM>::GetThis(self)->ClientCommandIsComplete( self, commandID ) ;
	}

	void* 
	AsyncStreamListenerCOM::SGetRefCon	( AsyncStreamListenerRef self )	
	{
		return IOFireWireIUnknown::InterfaceMap<AsyncStreamListenerCOM>::GetThis(self)->GetRefCon( self );
	}

	void
	AsyncStreamListenerCOM::SSetFlags ( AsyncStreamListenerRef		self,
										 UInt32						flags )
	{
		IOFireWireIUnknown::InterfaceMap<AsyncStreamListenerCOM>::GetThis(self)->SetFlags( self, flags );
	}
		
	UInt32
	AsyncStreamListenerCOM::SGetFlags ( AsyncStreamListenerRef	self )	
	{
		return IOFireWireIUnknown::InterfaceMap<AsyncStreamListenerCOM>::GetThis(self)->GetFlags( self );
	}

	UInt32
	AsyncStreamListenerCOM::SGetOverrunCounter ( AsyncStreamListenerRef		self )
	{
		return IOFireWireIUnknown::InterfaceMap<AsyncStreamListenerCOM>::GetThis(self)->GetOverrunCounter( self );
	}
}
