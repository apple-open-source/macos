/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
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

#import "IOFireWireLibPriv.h"
#import "IOFireWireLibPseudoAddressSpace.h"
#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/iokitmig.h>
#import <exception>


namespace IOFireWireLib {
	
	PseudoAddressSpace::Interface PseudoAddressSpace::sInterface =
	{
		INTERFACEIMP_INTERFACE,
		1, 0, // version/revision
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
	PseudoAddressSpace::Alloc( Device& userclient, KernAddrSpaceRef inKernAddrSpaceRef, void* inBuffer, UInt32 inBufferSize, 
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
	
	PseudoAddressSpace::PseudoAddressSpace( Device& userclient, KernAddrSpaceRef inKernAddrSpaceRef,
												void* inBuffer, UInt32 inBufferSize, void* inBackingStore, void* inRefCon) 
	: IOFireWireIUnknown( reinterpret_cast<IUnknownVTbl*>(& sInterface) ),
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
			throw std::exception() ;
	
		UInt32 nodeID ;
		UInt32 addressHi ;
		UInt32 addressLo ;

		IOReturn error ;
		error = ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kPseudoAddrSpace_GetFWAddrInfo, 1, 3, mKernAddrSpaceRef,
				& nodeID, & addressHi, & addressLo ) ;
		if (error)
			throw std::exception() ;

		mFWAddress = FWAddress( (UInt16)addressHi, addressLo, (UInt16)nodeID ) ;
	}
	
	PseudoAddressSpace::~PseudoAddressSpace()
	{
		#if IOFIREWIREUSERCLIENTDEBUG > 0
		IOReturn result = 
		#endif
		IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kPseudoAddrSpace_Release, 1, 0, mKernAddrSpaceRef ) ;
		IOFireWireLibLogIfErr_(result, "PseudoAddressSpace::~PseudoAddressSpace: error %x releasing address space!\n", result) ;
	
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
			params[0]	= (UInt32)mKernAddrSpaceRef ;
			params[1]	= (UInt32)(IOAsyncCallback) & PseudoAddressSpace::Writer ;
			params[2]	= (UInt32) callBackRefCon;
		
			err = io_async_method_scalarI_scalarO(
					connection,
					mUserClient.GetAsyncPort(),
					mPacketAsyncRef,
					1,
					kSetAsyncRef_Packet,
					params,
					3,
					output,
					& size) ;
			
		}
		
		if ( kIOReturnSuccess == err)
		{
			size=0 ;
			params[0]	= (UInt32) mKernAddrSpaceRef ;
			params[1]	= (UInt32)(IOAsyncCallback2) & SkippedPacket ;
			params[2]	= (UInt32) callBackRefCon;
			
			err = io_async_method_scalarI_scalarO( connection, mUserClient.GetAsyncPort(), mSkippedPacketAsyncRef, 1,
					kSetAsyncRef_SkippedPacket, params, 3, output, & size) ;
		}
		
		if ( kIOReturnSuccess == err)
		{
			params[0]	= (UInt32) mKernAddrSpaceRef ;
			params[1]	= (UInt32)(IOAsyncCallback) & Reader ;
			params[2]	= (UInt32) callBackRefCon ;
			
			err = io_async_method_scalarI_scalarO( connection, mUserClient.GetAsyncPort(), mReadPacketAsyncRef, 1,
					kSetAsyncRef_Read, params, 3, params, & size ) ;
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
		
			err = ::io_async_method_scalarI_scalarO( connection, mUserClient.GetAsyncPort(), mPacketAsyncRef,
							1, kSetAsyncRef_Packet, params, 3, params, & size) ;
			
	
			// set callback for skipped packets to 0
			params[0]	= (UInt32) mKernAddrSpaceRef ;
			params[1]	= (UInt32)(IOAsyncCallback) 0 ;
			params[2]	= (UInt32) this ;
			
			err = io_async_method_scalarI_scalarO( connection, mUserClient.GetAsyncPort(), mSkippedPacketAsyncRef, 
							1, kSetAsyncRef_SkippedPacket, params, 3, params, & size) ;
	
			// set callback for skipped packets to 0
			params[0]	= (UInt32) mKernAddrSpaceRef ;
			params[1]	= (UInt32)(IOAsyncCallback) 0 ;
			params[2]	= (UInt32) this ;
			
			err = io_async_method_scalarI_scalarO( connection, mUserClient.GetAsyncPort(),
					mReadPacketAsyncRef, 1, kSetAsyncRef_Read, params, 3, params, & size) ;
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
			UInt32	offset = (UInt32)args[6] ;
			
			if ( (UInt32) args[1] == 8 )
				// 32-bit compare
				equal = *(UInt32*)((char*)mBackingStore + offset) == *(UInt32*)(mBuffer + (UInt32)args[2]) ;
			else
				// 64-bit compare
				equal = *(UInt64*)((char*)mBackingStore + offset) == *(UInt64*)(mBuffer + (UInt32)args[2]) ;
	
			if ( equal )
			{
				mWriter(
					addressSpaceRef,
					(FWClientCommandID)(args[0]),						// commandID,
					(UInt32)(args[1]) >> 1,								// packetSize
					mBuffer + (UInt32)args[2] + ( (UInt32) args[1] == 8 ? 4 : 8),// packet
					(UInt16)(UInt32)args[3],							// nodeID
					(UInt32)(args[5]),									// addr.nodeID, addr.addressHi,
					(UInt32)(args[6]),
					(UInt32) mRefCon) ;									// refcon
			}
			else
				status = kFWResponseAddressError ;
				
			delete[] (args-1) ;
		}
	
		#if IOFIREWIREUSERCLIENTDEBUG > 0
		OSStatus err = 
		#endif
		::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kPseudoAddrSpace_ClientCommandIsComplete,
				3, 0, mKernAddrSpaceRef, commandID, status) ;
								
		IOFireWireLibLogIfErr_(err, "PseudoAddressSpace::ClientCommandIsComplete: err=0x%08lX\n", err) ;
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
			void** lockValues 	= new (void*)[numArgs+1] ;
	
			bcopy( args, & lockValues[1], sizeof(void*) * numArgs ) ;
			lockValues[0] = refcon ;
		
			::CFDictionaryAddValue( me->mPendingLocks, args[0], lockValues ) ;
	
			UInt32 offset = (UInt32)args[6] ;	// !!! hack - all address spaces have 0 for addressLo
	
			(me->mReader)( (AddressSpaceRef) refcon,
							(FWClientCommandID)(args[0]),					// commandID,
							(UInt32)(args[1]),								// packetSize
							offset,											// packetOffset
							(UInt16)(UInt32)(args[3]),						// nodeID; double cast avoids compiler warning
							(UInt32)(args[5]),								// addr.addressHi,
							(UInt32)(args[6]),								// addr.addressLo
							(UInt32) me->mRefCon) ;							// refcon
	
		}
		else
		{
			(me->mWriter)(
				(AddressSpaceRef) refcon,
				(FWClientCommandID) args[0],						// commandID,
				(UInt32)(args[1]),									// packetSize
				me->mBuffer + (UInt32)(args[2]),					// packet
				(UInt16)(UInt32)(args[3]),							// nodeID
				(UInt32)(args[5]),									// addr.addressHi, addr.addressLo
				(UInt32)(args[6]),
				(UInt32) me->mRefCon) ;								// refcon
	
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
						(UInt32)(args[1]),								// packetSize
						(UInt32)(args[2]),								// packetOffset
						(UInt16)(UInt32)(args[3]),						// nodeID
						(UInt32)(args[5]),								// addr.nodeID, addr.addressHi,
						(UInt32)(args[6]),
						(UInt32) me->mRefCon) ;							// refcon
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
