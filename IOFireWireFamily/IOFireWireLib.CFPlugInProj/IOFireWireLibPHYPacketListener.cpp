
/*
 * Copyright (c) 1998-2007 Apple Computer, Inc. All rights reserved.
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

// public
#import <IOKit/firewire/IOFireWireLib.h>

// private
#include "IOFireWireLibPHYPacketListener.h"
#include "IOFireWireLibCommand.h"
#import "IOFireWireLibDevice.h"
#import "IOFireWireLibPriv.h"

namespace IOFireWireLib 
{

	IOFireWireLibPHYPacketListenerInterface PHYPacketListener::sInterface =
	{
		INTERFACEIMP_INTERFACE,
		1, 1, // version/revision

		&PHYPacketListener::SSetListenerCallback,
		&PHYPacketListener::SSetSkippedPacketCallback,
		&PHYPacketListener::SNotificationIsOn,
		&PHYPacketListener::STurnOnNotification,
		&PHYPacketListener::STurnOffNotification,
		&PHYPacketListener::SClientCommandIsComplete,
		&PHYPacketListener::SSetRefCon,
		&PHYPacketListener::SGetRefCon,
		&PHYPacketListener::SSetFlags,
		&PHYPacketListener::SGetFlags
	};
		
	// Alloc
	//
	//
	
	IUnknownVTbl**	PHYPacketListener::Alloc( Device& userclient, UInt32 queue_count )
	{
		PHYPacketListener * me = new PHYPacketListener( userclient, queue_count );
		if( !me )
			return nil;

		return reinterpret_cast<IUnknownVTbl**>(&me->GetInterface());
	}
												
	// PHYPacketListener
	//
	//
	
	PHYPacketListener::PHYPacketListener( Device & userClient, UInt32 queue_count )
	:	IOFireWireIUnknown( reinterpret_cast<const IUnknownVTbl &>( sInterface ) ),
		mUserClient( userClient ),
		mKernelRef( 0 ),
		mQueueCount( queue_count ),
		mRefCon( NULL ),
		mCallback( NULL ),
		mSkippedCallback( NULL ),
		mFlags( 0 ),
		mNotifyIsOn( false )
	{
		mUserClient.AddRef();

		// input data
		const uint64_t inputs[1]={ (const uint64_t)mQueueCount };
					
		// output data
		uint64_t kernel_ref = 0;
		uint32_t outputCnt = 1;
		
		// send it down
		IOReturn status = IOConnectCallScalarMethod(	mUserClient.GetUserClientConnection(),
														kPHYPacketListenerCreate,
														inputs, 1,
														&kernel_ref, &outputCnt );
		if( status != kIOReturnSuccess )
		{
			throw status;
		}
		
		mKernelRef = (UserObjectHandle)kernel_ref;
	}
	
	// ~PHYPacketListener
	//
	//
	
	PHYPacketListener::~PHYPacketListener() 
	{
		if( mKernelRef )
		{
			IOReturn result = kIOReturnSuccess;
			
			uint32_t outputCnt = 0;
			const uint64_t inputs[1]={ (const uint64_t)mKernelRef };
			result = IOConnectCallScalarMethod(	mUserClient.GetUserClientConnection(),
												kReleaseUserObject,
												inputs, 1,
												NULL, &outputCnt);
			
			DebugLogCond( result, "VectorCommand::~VectorCommand: command release returned 0x%08x\n", result );
		}
		
		mUserClient.Release();
	}

	// QueryInterface
	//
	//
	
	HRESULT
	PHYPacketListener::QueryInterface( REFIID iid, LPVOID* ppv )
	{
		HRESULT		result = S_OK;
		*ppv = nil;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes( kCFAllocatorDefault, iid );
	
		if( CFEqual(interfaceID, IUnknownUUID) || CFEqual(interfaceID, kIOFireWirePHYPacketListenerInterfaceID) )
		{
			*ppv = &GetInterface();
			AddRef();
		}
		else
		{
			*ppv = nil;
			result = E_NOINTERFACE;
		}
		
		CFRelease( interfaceID );
		return result;
	}

	////////////////////////////////////////////////////////////////////////
	#pragma mark -

	// SetRefCon
	//
	//
	
	void PHYPacketListener::SSetRefCon( IOFireWireLibPHYPacketListenerRef self, void* refCon )
	{
		PHYPacketListener * me = IOFireWireIUnknown::InterfaceMap<PHYPacketListener>::GetThis(self);
		me->mRefCon = refCon;
	}

	// GetRefCon
	//
	//
	
	void * PHYPacketListener::SGetRefCon( IOFireWireLibPHYPacketListenerRef self )
	{
		PHYPacketListener * me = IOFireWireIUnknown::InterfaceMap<PHYPacketListener>::GetThis(self);
		return me->mRefCon;
	}
	
	// SetListenerCallback
	//
	//
	
	void PHYPacketListener::SSetListenerCallback(	IOFireWireLibPHYPacketListenerRef self, 
													IOFireWireLibPHYPacketCallback	inCallback )
	{
		PHYPacketListener * me = IOFireWireIUnknown::InterfaceMap<PHYPacketListener>::GetThis(self);
		me->mCallback = inCallback;
	}

	// SetSkippedPacketCallback
	//
	//
	
	void PHYPacketListener::SSetSkippedPacketCallback(	IOFireWireLibPHYPacketListenerRef self, 
														IOFireWireLibPHYPacketSkippedCallback	inCallback )
	{
		PHYPacketListener * me = IOFireWireIUnknown::InterfaceMap<PHYPacketListener>::GetThis(self);
		me->mSkippedCallback = inCallback;
	}

	// NotificationIsOn
	//
	//
	
	Boolean PHYPacketListener::SNotificationIsOn( IOFireWireLibPHYPacketListenerRef self )
	{
		PHYPacketListener * me = IOFireWireIUnknown::InterfaceMap<PHYPacketListener>::GetThis(self);
		return me->mNotifyIsOn;
	}
	
	// TurnOnNotification
	//
	//
	
	IOReturn PHYPacketListener::STurnOnNotification( IOFireWireLibPHYPacketListenerRef self )
	{
		PHYPacketListener * me = IOFireWireIUnknown::InterfaceMap<PHYPacketListener>::GetThis(self);
		return me->TurnOnNotification( self );
	}
	
	IOReturn PHYPacketListener::TurnOnNotification( IOFireWireLibPHYPacketListenerRef self )
	{
		IOReturn status = kIOReturnSuccess;
		
		if( mNotifyIsOn )
			status = kIOReturnNotPermitted;

		io_connect_t connection = NULL;
		if( status == kIOReturnSuccess )
		{
			connection = mUserClient.GetUserClientConnection();
			if( connection == 0 )
			{
				status = kIOReturnNoDevice;
			}
		}
	
		if( status == kIOReturnSuccess )
		{
			uint64_t refrncData[kOSAsyncRef64Count];
			refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) 0;
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long) 0;
			const uint64_t inputs[2] = {
											(const uint64_t)&SListenerCallback,
											(const uint64_t)self
										};
			uint32_t outputCnt = 0;
			status = IOConnectCallAsyncScalarMethod(	connection,
														mUserClient.MakeSelectorWithObject( kPHYPacketListenerSetPacketCallback, mKernelRef ),
														mUserClient.GetAsyncPort(),
														refrncData, kOSAsyncRef64Count,
														inputs, 2,
														NULL, &outputCnt);
		}

		if( status == kIOReturnSuccess )
		{
			uint64_t refrncData[kOSAsyncRef64Count];
			refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) 0;
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long) 0;
			const uint64_t inputs[2] = {	
											(const uint64_t)&SSkippedCallback,
											(const uint64_t)self
										};
			uint32_t outputCnt = 0;
			status = IOConnectCallAsyncScalarMethod(	connection,
														mUserClient.MakeSelectorWithObject( kPHYPacketListenerSetSkippedCallback, mKernelRef ),
														mUserClient.GetAsyncPort(),
														refrncData, kOSAsyncRef64Count,
														inputs, 2,
														NULL, &outputCnt);
		}

		if( status == kIOReturnSuccess )
		{
			uint32_t outputCnt = 0;
			status = IOConnectCallScalarMethod(	connection, 
												mUserClient.MakeSelectorWithObject( kPHYPacketListenerActivate, mKernelRef ),
												NULL, 0,
												NULL, &outputCnt );
		}

		
		if( status == kIOReturnSuccess )
		{
			mNotifyIsOn = true;
		}
					
		return status;
	}
	
	// TurnOffNotification
	//
	//
	
	void PHYPacketListener::STurnOffNotification( IOFireWireLibPHYPacketListenerRef self )
	{
		PHYPacketListener * me = IOFireWireIUnknown::InterfaceMap<PHYPacketListener>::GetThis(self);
		me->TurnOffNotification( self );
	}

	void PHYPacketListener::TurnOffNotification( IOFireWireLibPHYPacketListenerRef self )
	{
		IOReturn status = kIOReturnSuccess;
		
		if( !mNotifyIsOn )
			status = kIOReturnNotPermitted;

		io_connect_t connection = NULL;
		if( status == kIOReturnSuccess )
		{
			connection = mUserClient.GetUserClientConnection();
			if( connection == 0 )
			{
				status = kIOReturnNoDevice;
			}
		}

		if( status == kIOReturnSuccess )
		{
			uint64_t refrncData[kOSAsyncRef64Count];
			refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) 0;
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long) 0;
			const uint64_t inputs[2] = {
											(const uint64_t)0,
											(const uint64_t)self
										};
			uint32_t outputCnt = 0;
			status = IOConnectCallAsyncScalarMethod(	connection,
													mUserClient.MakeSelectorWithObject( kPHYPacketListenerSetPacketCallback, mKernelRef ),
													mUserClient.GetAsyncPort(),
													refrncData, kOSAsyncRef64Count,
													inputs, 2,
													NULL, &outputCnt);
		}

		if( status == kIOReturnSuccess )
		{
			uint64_t refrncData[kOSAsyncRef64Count];
			refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) 0;
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long) 0;
			const uint64_t inputs[2] = {	
											(const uint64_t)0,
											(const uint64_t)self
										};
			uint32_t outputCnt = 0;
			status = IOConnectCallAsyncScalarMethod(	connection,
													mUserClient.MakeSelectorWithObject( kPHYPacketListenerSetSkippedCallback, mKernelRef ),
													mUserClient.GetAsyncPort(),
													refrncData, kOSAsyncRef64Count,
													inputs, 2,
													NULL, &outputCnt);
		}

		if( status == kIOReturnSuccess )
		{
			uint32_t outputCnt = 0;
			status = IOConnectCallScalarMethod(	connection, 
												mUserClient.MakeSelectorWithObject( kPHYPacketListenerDeactivate, mKernelRef ),
												NULL, 0,
												NULL, &outputCnt );
		}

		mNotifyIsOn = false;
	}
	
	// ClientCommandIsComplete
	//
	//
	
	void PHYPacketListener::SClientCommandIsComplete(	IOFireWireLibPHYPacketListenerRef self,
														FWClientCommandID			commandID )
	{
		PHYPacketListener * me = IOFireWireIUnknown::InterfaceMap<PHYPacketListener>::GetThis(self);

		uint32_t		outputCnt = 0;
		const uint64_t	inputs[2] = { 
										(const uint64_t)commandID 
									};

		IOConnectCallScalarMethod(	me->mUserClient.GetUserClientConnection(), 
									me->mUserClient.MakeSelectorWithObject( kPHYPacketListenerClientCommandIsComplete, me->mKernelRef ),
									inputs, 1,
									NULL, &outputCnt );
	}

	// SetFlags
	//
	//

	void PHYPacketListener::SSetFlags( IOFireWireLibPHYPacketListenerRef self, UInt32 inFlags )
	{
		PHYPacketListener * me = IOFireWireIUnknown::InterfaceMap<PHYPacketListener>::GetThis(self);
		me->mFlags = inFlags;
	}
	
	// GetFlags
	//
	//
	
	UInt32 PHYPacketListener::SGetFlags( IOFireWireLibPHYPacketListenerRef self )
	{
		PHYPacketListener * me = IOFireWireIUnknown::InterfaceMap<PHYPacketListener>::GetThis(self);
		return me->mFlags;
	}

	// ListenerCallback
	//
	//
	
	void PHYPacketListener::SListenerCallback( IOFireWireLibPHYPacketListenerRef self, IOReturn result, void ** args, int numArgs)
	{
		PHYPacketListener * me = IOFireWireIUnknown::InterfaceMap<PHYPacketListener>::GetThis(self);
		
		if( me->mCallback )
		{
			(me->mCallback)(
				self,
				(FWClientCommandID)args[0],								// commandID,
				(UInt32)((unsigned long)args[1]),									// data1
				(UInt32)((unsigned long)args[2]),									// data2
				(void*) me->mRefCon);									// refcon
		}
		else
		{
			me->SClientCommandIsComplete( self, args[0] );
		}

	}

	// SkippedPacket
	//
	//
	
	void PHYPacketListener::SSkippedCallback( IOFireWireLibPHYPacketListenerRef self, IOReturn result, void ** args, int numArgs )
	{
		PHYPacketListener * me = IOFireWireIUnknown::InterfaceMap<PHYPacketListener>::GetThis(self);
		
		// printf( "PHYPacketListener::SSkippedCallback - arg0 0x%08lx arg1 0x%08lx\n", args[0], args[1] );
		
		if( me->mSkippedCallback )
		{
			(me->mSkippedCallback)( self, 
									(FWClientCommandID)args[0],								// commandID,
									(UInt32)((unsigned long)args[1]),									// count,
									(void*)me->mRefCon );									// refcon
		}
		else
		{
			me->SClientCommandIsComplete( self, (FWClientCommandID)args[0] );
		}
	}	
}
