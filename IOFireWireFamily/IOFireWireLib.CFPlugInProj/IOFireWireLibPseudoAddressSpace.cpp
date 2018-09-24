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
 *  IOFireWireLibPseudoAddressSpace.cpp
 *  IOFireWireLib
 *
 *  Created by NWG on Wed Dec 06 2000.
 *  Copyright (c) 2000 Apple, Inc. All rights reserved.
 *
 */

#import "IOFireWireLibPseudoAddressSpace.h"
#import "IOFireWireLibDevice.h"

#import <IOKit/iokitmig.h>
#import <System/libkern/OSCrossEndian.h>

namespace IOFireWireLib {
	
	PseudoAddressSpace::Interface PseudoAddressSpace::sInterface =
	{
		INTERFACEIMP_INTERFACE,
		1, 1, // version/revision
		& PseudoAddressSpace::SSetWriteHandler,
		& PseudoAddressSpace::SSetReadHandler,
		& PseudoAddressSpace::SSetSkippedPacketHandler,
		& PseudoAddressSpace::SNotificationIsOn,
		& PseudoAddressSpace::STurnOnNotification,
		& PseudoAddressSpace::STurnOffNotification,
		& PseudoAddressSpace::SClientCommandIsComplete,		
		& PseudoAddressSpace::SGetFWAddress,
		& PseudoAddressSpace::SGetBuffer,
		& PseudoAddressSpace::SGetBufferSize,
		& PseudoAddressSpace::SGetRefCon
	} ;
	
	IUnknownVTbl** 
	PseudoAddressSpace::Alloc( Device& userclient, UserObjectHandle inKernAddrSpaceRef, void* inBuffer, UInt32 inBufferSize, 
			void* inBackingStore, void* inRefCon )
	{
		PseudoAddressSpace* me = nil ;
		
		try {
			me = new PseudoAddressSpace(userclient, inKernAddrSpaceRef, inBuffer, inBufferSize, inBackingStore, inRefCon) ;
		} catch (...) {
		}
		
		return (nil == me) ? nil : reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	HRESULT STDMETHODCALLTYPE
	PseudoAddressSpace::QueryInterface(REFIID iid, LPVOID* ppv)
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if ( CFEqual(interfaceID, IUnknownUUID) ||  CFEqual(interfaceID, kIOFireWirePseudoAddressSpaceInterfaceID) )
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
	
	// ============================================================
	//
	// interface table methods
	//
	// ============================================================
	
	const PseudoAddressSpace::WriteHandler
	PseudoAddressSpace::SSetWriteHandler( AddressSpaceRef self, WriteHandler inWriter )
	{ 
		return IOFireWireIUnknown::InterfaceMap<PseudoAddressSpace>::GetThis(self)->SetWriteHandler(inWriter); 
	}
	
	const PseudoAddressSpace::ReadHandler
	PseudoAddressSpace::SSetReadHandler(AddressSpaceRef self, ReadHandler inReader)
	{ 
		return IOFireWireIUnknown::InterfaceMap<PseudoAddressSpace>::GetThis(self)->SetReadHandler(inReader); 
	}
	
	const PseudoAddressSpace::SkippedPacketHandler
	PseudoAddressSpace::SSetSkippedPacketHandler(AddressSpaceRef self, SkippedPacketHandler inHandler)
	{ 
		return IOFireWireIUnknown::InterfaceMap<PseudoAddressSpace>::GetThis(self)->SetSkippedPacketHandler(inHandler); 
	}
	
	Boolean
	PseudoAddressSpace::SNotificationIsOn(AddressSpaceRef self)
	{
		return IOFireWireIUnknown::InterfaceMap<PseudoAddressSpace>::GetThis(self)->mNotifyIsOn; 
	}
	
	Boolean
	PseudoAddressSpace::STurnOnNotification(AddressSpaceRef self)
	{ 
		return IOFireWireIUnknown::InterfaceMap<PseudoAddressSpace>::GetThis(self)->TurnOnNotification(self); 
	}
	
	void
	PseudoAddressSpace::STurnOffNotification(AddressSpaceRef self)
	{ 
		IOFireWireIUnknown::InterfaceMap<PseudoAddressSpace>::GetThis(self)->TurnOffNotification(); 
	}
	
	void
	PseudoAddressSpace::SClientCommandIsComplete(AddressSpaceRef self, FWClientCommandID commandID, IOReturn status)
	{ 
		IOFireWireIUnknown::InterfaceMap<PseudoAddressSpace>::GetThis(self)->ClientCommandIsComplete(commandID, status); 
	}
	
	void
	PseudoAddressSpace::SGetFWAddress(AddressSpaceRef self, FWAddress* outAddr)
	{ 
		bcopy (&IOFireWireIUnknown::InterfaceMap<PseudoAddressSpace>::GetThis(self)->mFWAddress, outAddr, sizeof(FWAddress)); 
	}
	
	void*
	PseudoAddressSpace::SGetBuffer(AddressSpaceRef self)
	{ 
		return IOFireWireIUnknown::InterfaceMap<PseudoAddressSpace>::GetThis(self)->GetBuffer() ; 
	}
	
	const UInt32
	PseudoAddressSpace::SGetBufferSize(AddressSpaceRef self)
	{ 
		return IOFireWireIUnknown::InterfaceMap<PseudoAddressSpace>::GetThis(self)->mBufferSize; 
	}
	
	void*
	PseudoAddressSpace::SGetRefCon(AddressSpaceRef self)
	{
		return IOFireWireIUnknown::InterfaceMap<PseudoAddressSpace>::GetThis(self)->mRefCon; 
	}
	
	#pragma mark -
	// ============================================================
	//
	// class methods
	//
	// ============================================================
	
	PseudoAddressSpace::PseudoAddressSpace( Device& userclient, UserObjectHandle inKernAddrSpaceRef,
												void* inBuffer, UInt32 inBufferSize, void* inBackingStore, void* inRefCon) 
	: IOFireWireIUnknown( reinterpret_cast<const IUnknownVTbl &>( sInterface ) ),
		mNotifyIsOn(false),
		mWriter( nil ),
		mReader( nil ),
		mSkippedPacketHandler( nil ),
		mUserClient(userclient), 
		mKernAddrSpaceRef(inKernAddrSpaceRef),
		mBuffer((char*)inBuffer),
		mBufferSize(inBufferSize),
		mBackingStore(inBackingStore),
		mRefCon(inRefCon)
	{
		userclient.AddRef() ;

		mPendingLocks = ::CFDictionaryCreateMutable( kCFAllocatorDefault, 0, NULL, NULL ) ;
		if (!mPendingLocks)
			throw kIOReturnNoMemory ;
	
		AddressSpaceInfo info ;

		IOReturn error ;
		
		uint32_t outputCnt = 0;
		size_t outputStructSize =  sizeof( info ) ;
		const uint64_t inputs[1]={(const uint64_t)mKernAddrSpaceRef};

		error = IOConnectCallMethod(mUserClient.GetUserClientConnection(), 
									kPseudoAddrSpace_GetFWAddrInfo,
									inputs,1,
									NULL,0,
									NULL,&outputCnt,
									&info,&outputStructSize);
		if (error)
		{
			throw error ;
		}

#ifndef __LP64__		
		ROSETTA_ONLY(
			{
				info.address.nodeID = OSSwapInt16( info.address.nodeID );
				info.address.addressHi = OSSwapInt16( info.address.addressHi );
				info.address.addressLo = OSSwapInt32( info.address.addressLo );
			}
		);
#endif
		
		mFWAddress = info.address ;
	}
	
	PseudoAddressSpace::~PseudoAddressSpace()
	{

		uint32_t outputCnt = 0;
		const uint64_t inputs[1]={(const uint64_t)mKernAddrSpaceRef};

#if IOFIREWIREUSERCLIENTDEBUG > 0
		IOReturn error = 
#endif
	
		IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
								  kReleaseUserObject,
								  inputs,1,NULL,&outputCnt);
		
		DebugLogCond( error, "PseudoAddressSpace::~PseudoAddressSpace: error %x releasing address space!\n", error ) ;
	
		if( mPendingLocks )
		{
			::CFDictionaryRemoveAllValues( mPendingLocks );
			::CFRelease( mPendingLocks );
			mPendingLocks = 0;
		}
			
		if( mBuffer and mBufferSize > 0 )	
		{
			delete[] mBuffer;
			mBuffer		= 0;
			mBufferSize = 0;
		}
			
		mUserClient.Release() ;
	}
	
	// callback management
	
	#pragma mark -
	#pragma mark --callback management
	
	const PseudoAddressSpace::WriteHandler
	PseudoAddressSpace::SetWriteHandler( WriteHandler inWriter )
	{
		WriteHandler oldWriter = mWriter ;
		mWriter = inWriter ;
		
		return oldWriter ;
	}
	
	
	const PseudoAddressSpace::ReadHandler
	PseudoAddressSpace::SetReadHandler(
		ReadHandler		inReader)
	{
		ReadHandler oldReader = mReader ;
		mReader = inReader ;
		
		return oldReader ;
	}
	
	const PseudoAddressSpace::SkippedPacketHandler
	PseudoAddressSpace::SetSkippedPacketHandler(
		SkippedPacketHandler			inHandler)
	{
		SkippedPacketHandler result = mSkippedPacketHandler ;
		mSkippedPacketHandler = inHandler ;
	
		return result ;
	}
	
	Boolean
	PseudoAddressSpace::TurnOnNotification( void* callBackRefCon )
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
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long)0;
			const uint64_t inputs[3] = {(const uint64_t)mKernAddrSpaceRef, (const uint64_t)&PseudoAddressSpace::Writer, (const uint64_t)callBackRefCon};
			uint32_t outputCnt = 0;
			err = IOConnectCallAsyncScalarMethod(connection,
												 kSetAsyncRef_Packet,
												 mUserClient.GetAsyncPort(), 
												 refrncData,kOSAsyncRef64Count,
												 inputs,3,
												 NULL,&outputCnt);
		}
		
		if ( kIOReturnSuccess == err)
		{
			uint64_t refrncData[kOSAsyncRef64Count];
			refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) 0;
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long)0;
			const uint64_t inputs[3] = {(const uint64_t)mKernAddrSpaceRef, (const uint64_t)& SkippedPacket, (const uint64_t)callBackRefCon};
			uint32_t outputCnt = 0;
			err = IOConnectCallAsyncScalarMethod(connection,
												 kSetAsyncRef_SkippedPacket,
												 mUserClient.GetAsyncPort(), 
												 refrncData,kOSAsyncRef64Count,
												 inputs,3,
												 NULL,&outputCnt);
		}
		
		if ( kIOReturnSuccess == err)
		{
			uint64_t refrncData[kOSAsyncRef64Count];
			refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) 0;
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long)0;
			const uint64_t inputs[3] = {(const uint64_t)mKernAddrSpaceRef, (const uint64_t)& Reader, (const uint64_t)callBackRefCon};
			uint32_t outputCnt = 0;
			err = IOConnectCallAsyncScalarMethod(connection,
												 kSetAsyncRef_Read,
												 mUserClient.GetAsyncPort(), 
												 refrncData,kOSAsyncRef64Count,
												 inputs,3,
												 NULL,&outputCnt);
		}
	
		if ( kIOReturnSuccess == err )
			mNotifyIsOn = true ;
			
		return ( kIOReturnSuccess == err ) ;
	}
	
	void
	PseudoAddressSpace::TurnOffNotification()
	{
		IOReturn				err					= kIOReturnSuccess ;
		io_connect_t			connection			= mUserClient.GetUserClientConnection() ;
		
		// if notification isn't on, skip out.
		if (!mNotifyIsOn)
			return ;
	
		if (!connection)
			err = kIOReturnNoDevice ;
	
		if ( kIOReturnSuccess == err )
		{
	
			uint64_t refrncData[kOSAsyncRef64Count];
			refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) 0;
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long)0;
			const uint64_t inputs[3] = {(const uint64_t)mKernAddrSpaceRef, (const uint64_t)0, (const uint64_t)this};
			uint32_t outputCnt = 0;
			err = IOConnectCallAsyncScalarMethod(connection,
												 kSetAsyncRef_Packet,
												 mUserClient.GetAsyncPort(), 
												 refrncData,kOSAsyncRef64Count,
												 inputs,3,
												 NULL,&outputCnt);

			outputCnt = 0;
			err = IOConnectCallAsyncScalarMethod(connection,
												 kSetAsyncRef_SkippedPacket,
												 mUserClient.GetAsyncPort(), 
												 refrncData,kOSAsyncRef64Count,
												 inputs,3,
												 NULL,&outputCnt);

			outputCnt = 0;
			err = IOConnectCallAsyncScalarMethod(connection,
												 kSetAsyncRef_Read,
												 mUserClient.GetAsyncPort(), 
												 refrncData,kOSAsyncRef64Count,
												 inputs,3,
												 NULL,&outputCnt);
		}
		
		mNotifyIsOn = false ;
	}
	
	void
	PseudoAddressSpace::ClientCommandIsComplete(
		FWClientCommandID				commandID,
		IOReturn						status)
	{
		void**		args ;
		
		if (::CFDictionaryGetValueIfPresent( mPendingLocks, commandID, (const void**) &args ) && (status == kIOReturnSuccess) )
		{
			::CFDictionaryRemoveValue( mPendingLocks, commandID ) ;
			AddressSpaceRef 	addressSpaceRef = (AddressSpaceRef) args[0] ;
	
			++args ;	// we tacked on an extra arg at the beginning, so we undo that.
			
			bool	equal ;
			UInt32	offset = (unsigned long)args[6] ;
			
			if ( (unsigned long) args[1] == 8 )
				// 32-bit compare
				equal = *(UInt32*)((char*)mBackingStore + offset) == *(UInt32*)(mBuffer + (unsigned long)args[2]) ;
			else
				// 64-bit compare
				equal = *(UInt64*)((char*)mBackingStore + offset) == *(UInt64*)(mBuffer + (unsigned long)args[2]) ;
	
			if ( equal )
			{
				mWriter(
					addressSpaceRef,
					(FWClientCommandID)(args[0]),						// commandID,
					(unsigned long)(args[1]) >> 1,								// packetSize
					mBuffer + (unsigned long)args[2] + ( (unsigned long) args[1] == 8 ? 4 : 8),// packet
					(UInt16)(unsigned long)args[3],							// nodeID
					(unsigned long)(args[5]),									// addr.nodeID, addr.addressHi,
					(unsigned long)(args[6]),
					(void*) mRefCon) ;									// refcon
			}
			else
				status = kFWResponseAddressError ;
				
			delete[] (args-1) ;
		}
	
		uint32_t outputCnt = 0;		
		const uint64_t inputs[3] = {(const uint64_t)mKernAddrSpaceRef, (const uint64_t)commandID, (const uint64_t)status};

		#if IOFIREWIREUSERCLIENTDEBUG > 0
		OSStatus err = 
		#endif
		
		IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
								  kPseudoAddrSpace_ClientCommandIsComplete,
								  inputs,3,
								  NULL,&outputCnt);

#ifdef __LP64__		
		DebugLogCond( err, "PseudoAddressSpace::ClientCommandIsComplete: err=0x%08X\n", (UInt32)err ) ;
#else
		DebugLogCond( err, "PseudoAddressSpace::ClientCommandIsComplete: err=0x%08lX\n", (UInt32)err ) ;
#endif
	}
	
	void
	PseudoAddressSpace::Writer( AddressSpaceRef refcon, IOReturn result, void** args, int numArgs)
	{
		PseudoAddressSpace* me = IOFireWireIUnknown::InterfaceMap<PseudoAddressSpace>::GetThis(refcon) ;
	
		if ( !me->mWriter || ( (bool)args[7] && !me->mReader) )
		{
			me->ClientCommandIsComplete( args[0], kFWResponseTypeError) ;
			return ;
		}
		else if ( (bool)args[7] )
		{
//			void** lockValues 	= new (void*)[numArgs+1] ;
			void** lockValues 	= (void**) new UInt32 *[numArgs+1] ;
	
			bcopy( args, & lockValues[1], sizeof(void*) * numArgs ) ;
			lockValues[0] = refcon ;
		
			::CFDictionaryAddValue( me->mPendingLocks, args[0], lockValues ) ;
	
			UInt32 offset = (unsigned long)args[6] ;	// !!! hack - all address spaces have 0 for addressLo
	
			(me->mReader)( (AddressSpaceRef) refcon,
							(FWClientCommandID)(args[0]),					// commandID,
							(unsigned long)(args[1]),								// packetSize
							offset,											// packetOffset
							(UInt16)(unsigned long)(args[3]),						// nodeID; double cast avoids compiler warning
							(unsigned long)(args[5]),								// addr.addressHi,
							(unsigned long)(args[6]),								// addr.addressLo
							(void*) me->mRefCon) ;							// refcon
	
		}
		else
		{
			(me->mWriter)(
				(AddressSpaceRef) refcon,
				(FWClientCommandID) args[0],						// commandID,
				(unsigned long)(args[1]),									// packetSize
				me->mBuffer + (unsigned long)(args[2]),					// packet
				(UInt16)(unsigned long)(args[3]),							// nodeID
				(unsigned long)(args[5]),									// addr.addressHi, addr.addressLo
				(unsigned long)(args[6]),
				(void*) me->mRefCon) ;								// refcon
	
		}
	}
	
	void
	PseudoAddressSpace::SkippedPacket( AddressSpaceRef refcon, IOReturn result, FWClientCommandID commandID, UInt32 packetCount)
	{
		PseudoAddressSpace* me = IOFireWireIUnknown::InterfaceMap<PseudoAddressSpace>::GetThis(refcon) ;
	
		if (me->mSkippedPacketHandler)
			(me->mSkippedPacketHandler)( refcon, commandID, packetCount) ;
	}
	
	void
	PseudoAddressSpace::Reader( AddressSpaceRef	refcon, IOReturn result, void** args, int numArgs )
	{
		PseudoAddressSpace* me = IOFireWireIUnknown::InterfaceMap<PseudoAddressSpace>::GetThis(refcon) ;

	
		if (me->mReader)
		{
			(me->mReader)( (AddressSpaceRef) refcon,
						(FWClientCommandID) args[0],					// commandID,
						(unsigned long)(args[1]),								// packetSize
						(unsigned long)(args[2]),								// packetOffset
						(UInt16)(unsigned long)(args[3]),						// nodeID
						(unsigned long)(args[5]),								// addr.nodeID, addr.addressHi,
						(unsigned long)(args[6]),
						(void*) me->mRefCon) ;							// refcon
		}
		else
			me->ClientCommandIsComplete( args[0], //commandID
									kFWResponseTypeError) ;
	}
	
	
	#pragma mark -
	#pragma mark --accessors
	
	const FWAddress&
	PseudoAddressSpace::GetFWAddress()
	{
		return mFWAddress ;
	}
	
	void*
	PseudoAddressSpace::GetBuffer()
	{
		return mBackingStore ;	// I assume this is what the user wants instead of 
								// the queue buffer stored in mBuffer.
	}
	
	const UInt32
	PseudoAddressSpace::GetBufferSize()
	{
		return mBufferSize ;
	}
	
	void*
	PseudoAddressSpace::GetRefCon()
	{
		return mRefCon ;
	}
}
