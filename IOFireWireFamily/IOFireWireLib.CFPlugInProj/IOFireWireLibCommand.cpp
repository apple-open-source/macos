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
 *  IOFireWireLibCommand.cpp
 *  IOFireWireLib
 *
 *  Created by gabel on Fri Dec 15 2000.
 *  Copyright (c) 2000 Apple, Inc. All rights reserved.
 *
 */

//#include <Carbon/Carbon.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/iokitmig.h>

#include "IOFireWireLib.h"
#include "IOFireWireLibPriv.h"
#include "IOFireWireLibCommand.h"

#define IOFIREWIRELIBCOMMANDIMP_INTERFACE \
	& IOFireWireLibCommandImp::SGetStatus,	\
	& IOFireWireLibCommandImp::SGetTransferredBytes,	\
	& IOFireWireLibCommandImp::SGetTargetAddress,	\
	& IOFireWireLibCommandImp::SSetTarget,	\
	& IOFireWireLibCommandImp::SSetGeneration,	\
	& IOFireWireLibCommandImp::SSetCallback,	\
	& IOFireWireLibCommandImp::SSetRefCon,	\
	& IOFireWireLibCommandImp::SIsExecuting,	\
	& IOFireWireLibCommandImp::SSubmit,	\
	& IOFireWireLibCommandImp::SSubmitWithRefconAndCallback, \
	& IOFireWireLibCommandImp::SCancel

#define IOFIREWIRELIBCOMMANDIMP_INTERFACE_v2	\
	& IOFireWireLibCommandImp::SSetBuffer,	\
	& IOFireWireLibCommandImp::SGetBuffer,	\
	& IOFireWireLibCommandImp::SSetMaxPacket,	\
	& IOFireWireLibCommandImp::SSetFlags

IOFireWireCommandInterface IOFireWireLibCommandImp::sInterface = 
{
	INTERFACEIMP_INTERFACE,
	1, 0, // version/revision
	
	IOFIREWIRELIBCOMMANDIMP_INTERFACE
} ;

IOFireWireReadCommandInterface IOFireWireLibReadCommandImp::sInterface =
{
	INTERFACEIMP_INTERFACE,
	1, 1, // version/revision
	
	IOFIREWIRELIBCOMMANDIMP_INTERFACE,
	IOFIREWIRELIBCOMMANDIMP_INTERFACE_v2
} ;

IOFireWireReadQuadletCommandInterface IOFireWireLibReadQuadletCommandImp::sInterface =
{
	INTERFACEIMP_INTERFACE,
	1, 0, // version/revision
	
	IOFIREWIRELIBCOMMANDIMP_INTERFACE,

	& IOFireWireLibReadQuadletCommandImp::SSetQuads
} ;

IOFireWireWriteCommandInterface IOFireWireLibWriteCommandImp::sInterface =
{
	INTERFACEIMP_INTERFACE,
	1, 1, // version/revision
	
	IOFIREWIRELIBCOMMANDIMP_INTERFACE,
	IOFIREWIRELIBCOMMANDIMP_INTERFACE_v2
} ;

IOFireWireWriteQuadletCommandInterface IOFireWireLibWriteQuadletCommandImp::sInterface =
{
	INTERFACEIMP_INTERFACE,
	1, 0, // version/revision
	IOFIREWIRELIBCOMMANDIMP_INTERFACE,

	& IOFireWireLibWriteQuadletCommandImp::SSetQuads
} ;

IOFireWireCompareSwapCommandInterface IOFireWireLibCompareSwapCommandImp::sInterface =
{
	INTERFACEIMP_INTERFACE,
	1, 0, // version/revision
	IOFIREWIRELIBCOMMANDIMP_INTERFACE,

	& IOFireWireLibCompareSwapCommandImp::SSetValues
} ;

HRESULT
IOFireWireLibReadCommandImp::QueryInterface(REFIID iid, LPVOID* ppv)
{
	HRESULT		result = S_OK ;
	*ppv = nil ;

	CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;

	if ( CFEqual(interfaceID, IUnknownUUID)
		 || CFEqual(interfaceID, kIOFireWireCommandInterfaceID)
		 || CFEqual(interfaceID, kIOFireWireReadCommandInterfaceID)
		 || CFEqual( interfaceID, kIOFireWireReadCommandInterfaceID_v2 ) )
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

HRESULT
IOFireWireLibReadQuadletCommandImp::QueryInterface(REFIID iid, LPVOID* ppv)
{
	HRESULT		result = S_OK ;
	*ppv = nil ;

	CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;

	if (CFEqual(interfaceID, IUnknownUUID) || CFEqual(interfaceID, kIOFireWireCommandInterfaceID) || CFEqual(interfaceID, kIOFireWireReadQuadletCommandInterfaceID) )
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

HRESULT
IOFireWireLibWriteCommandImp::QueryInterface(REFIID iid, LPVOID* ppv)
{
	HRESULT		result = S_OK ;
	*ppv = nil ;

	CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;

	if ( CFEqual(interfaceID, IUnknownUUID) 
		 || CFEqual( interfaceID, kIOFireWireCommandInterfaceID ) 
		 || CFEqual( interfaceID, kIOFireWireWriteCommandInterfaceID )
		 || CFEqual( interfaceID, kIOFireWireWriteCommandInterfaceID_v2 ) )
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

HRESULT
IOFireWireLibWriteQuadletCommandImp::QueryInterface(REFIID iid, LPVOID* ppv)
{
	HRESULT		result = S_OK ;
	*ppv = nil ;

	CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;

	if (CFEqual(interfaceID, IUnknownUUID) || CFEqual(interfaceID, kIOFireWireCommandInterfaceID) || CFEqual(interfaceID, kIOFireWireWriteQuadletCommandInterfaceID) )
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

HRESULT
IOFireWireLibCompareSwapCommandImp::QueryInterface(REFIID iid, LPVOID* ppv)
{
	HRESULT		result = S_OK ;
	*ppv = nil ;

	CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;

	if (CFEqual(interfaceID, IUnknownUUID) || CFEqual(interfaceID, kIOFireWireCommandInterfaceID) || CFEqual(interfaceID, kIOFireWireCompareSwapCommandInterfaceID) )
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

HRESULT
IOFireWireLibCommandImp::QueryInterface(REFIID iid, LPVOID* ppv)
{
	HRESULT		result = S_OK ;
	*ppv = nil ;

	CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;

	if (CFEqual(interfaceID, IUnknownUUID) || CFEqual(interfaceID, kIOFireWireCommandInterfaceID) )
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

// --- getters -----------------------
IOReturn
IOFireWireLibCommandImp::SGetStatus(
	IOFireWireLibCommandRef	self)
{
	return GetThis(self)->GetCompletionStatus() ;
}

UInt32
IOFireWireLibCommandImp::SGetTransferredBytes(
	IOFireWireLibCommandRef	self)
{
	return GetThis(self)->GetTransferredBytes() ;
}

void
IOFireWireLibCommandImp::SGetTargetAddress(
	IOFireWireLibCommandRef	self,
	FWAddress*				outAddr)
{
	bcopy(& GetThis(self)->GetTargetAddress(), outAddr, sizeof(*outAddr)) ;
}

// --- setters -----------------------
void
IOFireWireLibCommandImp::SSetTarget(
	IOFireWireLibCommandRef	self,
	const FWAddress*	inAddr)
{	
	GetThis(self)->SetTarget(*inAddr) ;
}

void
IOFireWireLibCommandImp::SSetGeneration(
	IOFireWireLibCommandRef	self,
	UInt32					inGeneration)
{
	GetThis(self)->SetGeneration(inGeneration) ;
}

void
IOFireWireLibCommandImp::SSetCallback(
	IOFireWireLibCommandRef			self,
	IOFireWireLibCommandCallback	inCallback)
{
	GetThis(self)->SetCallback(inCallback) ;
}

void
IOFireWireLibCommandImp::SSetRefCon(
	IOFireWireLibCommandRef	self,
	void*					refCon)
{
	GetThis(self)->SetRefCon(refCon) ;
}

const Boolean
IOFireWireLibCommandImp::SIsExecuting(
	IOFireWireLibCommandRef	self)
{ 
	return GetThis(self)->mIsExecuting; 
}

IOReturn
IOFireWireLibCommandImp::SSubmit(
	IOFireWireLibCommandRef	self) 
{
	return GetThis(self)->Submit() ;
}

IOReturn
IOFireWireLibCommandImp::SSubmitWithRefconAndCallback(
	IOFireWireLibCommandRef	self,
	void*					inRefCon,
	IOFireWireLibCommandCallback inCallback)
{
	return GetThis(self)->SubmitWithRefconAndCallback(inRefCon, inCallback) ;
}

IOReturn
IOFireWireLibCommandImp::SCancel(
	IOFireWireLibCommandRef	self,
	IOReturn				reason)
{
	return GetThis(self)->Cancel(reason) ;
}

void
IOFireWireLibCommandImp::SSetBuffer(
	IOFireWireLibCommandRef		self,
	UInt32						inSize,
	void*						inBuf)
{
	GetThis(self)->SetBuffer(inSize, inBuf) ;
}

void
IOFireWireLibCommandImp::SGetBuffer(
	IOFireWireLibCommandRef		self,
	UInt32*						outSize,
	void**						outBuf)
{
	GetThis(self)->GetBuffer(outSize, outBuf) ;
}

IOReturn
IOFireWireLibCommandImp::SSetMaxPacket(
	IOFireWireLibCommandRef self,
	IOByteCount				inMaxBytes)
{
	return GetThis(self)->SetMaxPacket(inMaxBytes) ;
}

void
IOFireWireLibCommandImp::SSetFlags(
	IOFireWireLibCommandRef	self,
	UInt32					inFlags)
{
	GetThis(self)->SetFlags(inFlags) ;
}


// ==================================
// virtual members
// ==================================
IOFireWireLibCommandImp::IOFireWireLibCommandImp(
	IOFireWireDeviceInterfaceImp&	userClient,
	io_object_t						inDevice):	IOFireWireIUnknown(),
												mUserClient	(userClient),
												mDevice		(inDevice),
//												mAddress	(inAddr),
//												mFailOnReset(inFailOnReset),
//												mGeneration	(inGeneration),
												mBytesTransferred(0),
												mIsExecuting(false),
												mStatus		(kIOReturnSuccess),
												mRefCon		(0),
												mCallback	(0)
												   
{
	mUserClient.AddRef() ;
		
	mInterface.pseudoVTable	= (IUnknownVTbl*) & sInterface ;
	mInterface.obj			= this ;
}

IOFireWireLibCommandImp::~IOFireWireLibCommandImp() 
{
	if (mParams)
	{
		if (mParams->kernCommandRef)
		{
			IOReturn result = IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(),
														kFWCommand_Release,
														1,
														0,
														mParams->kernCommandRef) ;
			IOFireWireLibLogIfErr_( result, ("IOFireWireLibCommandImp::~IOFireWireLibCommandImp: command release returned 0x%08lX\n", result)) ;
		}
	
		delete mParams ;
	}
	
	mUserClient.Release() ;
	
}

Boolean
IOFireWireLibCommandImp::Init(	
	const FWAddress&				inAddr,
	IOFireWireLibCommandCallback	inCallback,
	const Boolean					inFailOnReset,
	const UInt32					inGeneration,
	void*							inRefCon)
{
	mRefCon		= inRefCon ;
	mCallback	= inCallback ;

	// parameter block setup
	mParams->callback		= & IOFireWireLibCommandImp::CommandCompletionHandler ;
	mParams->refCon			= this ;
	mParams->newTarget		= inAddr ;
	mParams->newFailOnReset	= inFailOnReset ;
	mParams->newGeneration	= inGeneration ;
	mParams->staleFlags 	= kFireWireCommandStale ;
	mParams->flags			= (inCallback == nil) ? kFWCommandInterfaceSyncExecute : kFWCommandNoFlags ;
	mParams->kernCommandRef	= 0 ;
	
	return true ;
}
// --- getters -----------------------
const IOReturn
IOFireWireLibCommandImp::GetCompletionStatus() const
{
	return mStatus ;
}

const UInt32
IOFireWireLibCommandImp::GetTransferredBytes() const
{
	return mBytesTransferred ;
}

const FWAddress&
IOFireWireLibCommandImp::GetTargetAddress() const
{
//	return mAddress ;
	return mParams->newTarget ;
}

// --- setters -----------------------
void
IOFireWireLibCommandImp::SetTarget(
	const FWAddress&	addr)
{
	mParams->newTarget = addr ;
	mParams->staleFlags |= kFireWireCommandStale ;
}

void
IOFireWireLibCommandImp::SetGeneration(
	UInt32				inGeneration)
{
	mParams->newGeneration = inGeneration ;
	mParams->staleFlags |= kFireWireCommandStale ;
}

void
IOFireWireLibCommandImp::SetCallback(
	IOFireWireLibCommandCallback inCallback)
{
	mCallback = inCallback ;

	if (mCallback)
		mParams->flags |= kFWCommandInterfaceSyncExecute ;
	else
		mParams->flags &= ~kFWCommandInterfaceSyncExecute ;
}

void
IOFireWireLibCommandImp::SetRefCon(
	void*				inRefCon)
{
	mRefCon = inRefCon ;
}

const Boolean
IOFireWireLibCommandImp::IsExecuting() const 
{
	return mIsExecuting; 
}


IOReturn
IOFireWireLibCommandImp::Submit(
	FWUserCommandSubmitParams*	params,
	mach_msg_type_number_t		paramsSize,
	FWUserCommandSubmitResult*	submitResult,
	mach_msg_type_number_t*		submitResultSize)
{
	assert( !mIsExecuting ) ;

	IOReturn 				result 			= kIOReturnSuccess ;
		
	if (mDevice == mUserClient.GetDevice() )
	{
		result = io_async_method_structureI_structureO( mUserClient.GetUserClientConnection(),
														mUserClient.GetAsyncPort(),
														mAsyncRef,
														1,
														kFWCommand_Submit,
														(io_struct_inband_t) params,
														paramsSize,
														(io_struct_inband_t) submitResult,
														submitResultSize ) ;
	}
	else if (mDevice == 0)
	{
		result = io_async_method_structureI_structureO( mUserClient.GetUserClientConnection(),
														mUserClient.GetAsyncPort(),
														mAsyncRef,
														1,
														kFWCommand_SubmitAbsolute,
														(io_struct_inband_t) params,
														paramsSize,
														(io_struct_inband_t) submitResult,
														submitResultSize ) ;
	}
	else
		result = kIOReturnNoDevice ;
	
	if (kIOReturnSuccess == result)
	{
		if (mParams->flags & kFWCommandInterfaceSyncExecute)
		{
			mStatus = submitResult->result ;
			mBytesTransferred = submitResult->bytesTransferred ;
		}
		else	
			mIsExecuting = true ;

		mParams->staleFlags = 0 ;
		if (!mParams->kernCommandRef)
			mParams->kernCommandRef = submitResult->kernCommandRef ;
 	}

	return result ;
}

IOReturn
IOFireWireLibCommandImp::SubmitWithRefconAndCallback(
	void*	inRefCon,
	IOFireWireLibCommandCallback	inCallback)
{
	if (mIsExecuting)
		return kIOReturnBusy ;
	
	SetRefCon(inRefCon) ;
	SetCallback(inCallback) ;

	return Submit() ;
}

IOReturn
IOFireWireLibCommandImp::Cancel(
	IOReturn	reason)
{
	if (!mIsExecuting)
		return kIOReturnSuccess ;
	
	return IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(),
										  kFWCommand_Cancel,
										  1,
										  0,
										  mParams->kernCommandRef) ;
}

void
IOFireWireLibCommandImp::SetBuffer(
	UInt32				inSize,
	void*				inBuffer)
{
	mParams->newBufferSize = inSize ;
	mParams->newBuffer = inBuffer ;
	mParams->staleFlags |= kFireWireCommandStale_Buffer ;
}

void
IOFireWireLibCommandImp::GetBuffer(
	UInt32*				outSize,
	void**				outBuf)
{
	*outSize = mParams->newBufferSize ;
	*outBuf	= mParams->newBuffer ;
}

IOReturn
IOFireWireLibCommandImp::SetMaxPacket(
	IOByteCount				inMaxBytes)
{
	mParams->newMaxPacket = inMaxBytes ;
	return kIOReturnSuccess ;
}

void
IOFireWireLibCommandImp::SetFlags(
	UInt32					inFlags)
{
	mParams->flags &= ~kFireWireCommandUserFlagsMask ;
	mParams->flags |= (inFlags | kFireWireCommandUserFlagsMask) ;

	if (mParams->flags & kFWCommandInterfaceForceCopyAlways)
		mParams->flags |= kFireWireCommandUseCopy ;
	if (mParams->flags & kFWCommandInterfaceForceNoCopy)
		mParams->flags &= ~kFireWireCommandUseCopy ;
}

void
IOFireWireLibCommandImp::CommandCompletionHandler(
	void*				refcon,
	IOReturn			result,
	IOByteCount			bytesTransferred)
{
	IOFireWireLibCommandImp*	me = (IOFireWireLibCommandImp*)refcon ;

	me->mStatus 			= result ;
	me->mBytesTransferred 	= bytesTransferred ;
	me->mIsExecuting 		= false ;
	
	if (me->mCallback)
		(*(me->mCallback))(me->mRefCon, me->mStatus) ;
}

// ============================================================
//
// IOFireWireLibReadCommandImp methods
//
// ============================================================
IUnknownVTbl**
IOFireWireLibReadCommandImp::Alloc(
	IOFireWireDeviceInterfaceImp& inUserClient,
	io_object_t			device,
	const FWAddress&	addr,
	void*				buf,
	UInt32				size,
	IOFireWireLibCommandCallback callback,
	Boolean				failOnReset,
	UInt32				generation,
	void*				inRefCon)
{
	IUnknownVTbl** interface = 0 ;
	IOFireWireLibReadCommandImp*	me = new IOFireWireLibReadCommandImp(inUserClient, device, addr, buf, size, callback, failOnReset, generation, inRefCon) ;

	if (me)
	{
		if ( !me->Init(addr, buf, size, callback, failOnReset, generation, inRefCon) )
		{
			delete me ;
			me = nil ;
		}
		else
			interface = (IUnknownVTbl**) & me->mInterface ;
	}
	
	return interface ;
}

Boolean
IOFireWireLibReadCommandImp::Init(
//	IOFireWireDeviceInterfaceImp& inUserClient,
//	io_object_t			device,
	const FWAddress&	addr,
	void*				buf,
	UInt32				size,
	IOFireWireLibCommandCallback callback,
	Boolean				failOnReset,
	UInt32				generation,
	void*				inRefCon)
{
	if (NULL == (mParams = new FWUserCommandSubmitParams))
		return false ;
	if (!IOFireWireLibCommandImp::Init(addr, callback, failOnReset, generation, inRefCon))
		return false ;

	mParams->type			 = kFireWireCommandType_Read ;
	mParams->newBuffer		 = buf ;
	mParams->newBufferSize	 = size ;
	mParams->staleFlags 	|= kFireWireCommandStale_Buffer ;

//	mSubmitSelector 			= kFWCommand_SubmitRead ;
//	mSubmitAbsoluteSelector 	= kFWCommand_SubmitReadAbsolute ;

	return true ;
}

IOFireWireLibReadCommandImp::IOFireWireLibReadCommandImp(
	IOFireWireDeviceInterfaceImp& inUserClient,
	io_object_t			device,
	const FWAddress&	addr,
	void*				buf,
	UInt32				size,
	IOFireWireLibCommandCallback callback,
	Boolean				failOnReset,
	UInt32				generation,
	void*				inRefCon): IOFireWireLibCommandImp(inUserClient, device)
//									 mBuffer(buf),
//									 mSize(size)
{
	// COM support
	mInterface.pseudoVTable = (IUnknownVTbl*) & IOFireWireLibReadCommandImp::sInterface ;
	mInterface.obj = this ;

}

IOReturn
IOFireWireLibReadCommandImp::Submit()
{
	if ( mIsExecuting )
		return kIOReturnBusy ;

	IOReturn 				result 			= kIOReturnSuccess ;

	UInt8						submitResultExtra[sizeof(FWUserCommandSubmitResult) + kFWUserCommandSubmitWithCopyMaxBufferBytes] ;
	FWUserCommandSubmitResult*	submitResult = (FWUserCommandSubmitResult*) submitResultExtra ;
	mach_msg_type_number_t		submitResultSize ;
	
	if (mParams->flags & kFireWireCommandUseCopy)
		submitResultSize = sizeof(*submitResult) + mParams->newBufferSize ;
	else
		submitResultSize = sizeof(*submitResult) ;

	result = IOFireWireLibCommandImp::Submit(mParams, sizeof(*mParams), submitResult, & submitResultSize) ;
	
	if ((mParams->flags & kFWCommandInterfaceSyncExecute) && 
		(mParams->flags & kFireWireCommandUseCopy) &&
		(kIOReturnSuccess == result))
	{
		bcopy(submitResult + 1, mParams->newBuffer, mBytesTransferred) ;
	}
		
	return result ;
}

// ============================================================
//
// IOFireWireLibReadQuadletCommandImp methods
//
// ============================================================
IUnknownVTbl**
IOFireWireLibReadQuadletCommandImp::Alloc(
	IOFireWireDeviceInterfaceImp& inUserClient,
	io_object_t			device,
	const FWAddress &	addr,
	UInt32				quads[],
	UInt32				numQuads,
	IOFireWireLibCommandCallback callback,
	Boolean				failOnReset,
	UInt32				generation,
	void*				inRefCon)
{
	IUnknownVTbl** interface = 0 ;
	IOFireWireLibReadQuadletCommandImp*	me = 
			new IOFireWireLibReadQuadletCommandImp(inUserClient, device) ;

	if (me)
	{
		if ( !me->Init(addr, quads, numQuads, callback, failOnReset, generation, inRefCon) )
		{
			delete me ;
			me = nil ;
		}
		else
			interface = (IUnknownVTbl**) & me->mInterface ;
	}
	
	return interface ;
}

Boolean
IOFireWireLibReadQuadletCommandImp::Init(
	const FWAddress &	addr,
	UInt32				quads[],
	UInt32				numQuads,
	IOFireWireLibCommandCallback callback,
	Boolean				failOnReset,
	UInt32				generation,
	void*				inRefCon)
{
	if (NULL == (mParams = new FWUserCommandSubmitParams))
		return false ;

	if (!IOFireWireLibCommandImp::Init(addr, callback, failOnReset, generation, inRefCon))
		return false ;
	
	mParams->callback		= & IOFireWireLibReadQuadletCommandImp::CommandCompletionHandler ;
	mParams->type			= kFireWireCommandType_ReadQuadlet ;
	mParams->newBuffer		= quads ;
	mParams->newBufferSize	= numQuads << 2 ;	// x * 4
	mParams->staleFlags 	|= kFireWireCommandStale_Buffer ;
	mParams->flags			|= kFireWireCommandUseCopy ;

	return true ;
}

void
IOFireWireLibReadQuadletCommandImp::SSetQuads(
	IOFireWireLibReadQuadletCommandRef self,
	UInt32				inQuads[],
	UInt32				inNumQuads)
{
	GetThis(self)->SetQuads(inQuads, inNumQuads) ;
}

IOFireWireLibReadQuadletCommandImp::IOFireWireLibReadQuadletCommandImp(
	IOFireWireDeviceInterfaceImp& inUserClient,
	io_object_t			device): IOFireWireLibCommandImp(inUserClient, device)
{
	// COM
	mInterface.pseudoVTable = (IUnknownVTbl*) & sInterface ;
	mInterface.obj = this ;
}

void
IOFireWireLibReadQuadletCommandImp::SetQuads(
	UInt32				inQuads[],
	UInt32				inNumQuads)
{
	mParams->newBufferSize = inNumQuads << 2 ; // * 4
	mParams->newBuffer = (void*) inQuads ;
	mNumQuads = inNumQuads ;
	
	mParams->staleFlags |= kFireWireCommandStale_Buffer ;
}

IOReturn
IOFireWireLibReadQuadletCommandImp::Submit()
{
	if (mIsExecuting)
		return kIOReturnBusy ;

	IOReturn 				result 			= kIOReturnSuccess ;

	Boolean							syncFlag = mParams->flags & kFWCommandInterfaceSyncExecute ;
	UInt8							submitResultExtra[sizeof(FWUserCommandSubmitResult) + (syncFlag ? mParams->newBufferSize : 0)] ;
	mach_msg_type_number_t			submitResultSize = sizeof(submitResultExtra) ;
	FWUserCommandSubmitResult*		submitResult = (FWUserCommandSubmitResult*) submitResultExtra ;

/*	if (mDevice == mUserClient.GetDevice() )
	{
		result = io_async_method_structureI_structureO( mUserClient.GetUserClientConnection(),
														mUserClient.GetAsyncPort(),
														mAsyncRef,
														1,
														kFWCommand_Submit,
														(char*) mParams,
														sizeof(*mParams),
														(char*) submitResult,
														& submitResultSize ) ;
	}
	else if (mDevice == 0)
	{
		result = io_async_method_structureI_structureO( mUserClient.GetUserClientConnection(),
														mUserClient.GetAsyncPort(),
														mAsyncRef,
														1,
														kFWCommand_SubmitAbsolute,
														(char*) mParams,
														sizeof(*mParams),
														(char*) submitResult,
														& submitResultSize ) ;
	}
	else
		result = kIOReturnNoDevice ; */
	
	result = IOFireWireLibCommandImp::Submit(mParams, sizeof(*mParams), submitResult, & submitResultSize) ;
	
	if (kIOReturnSuccess == result)
	{
		if ( syncFlag )
		{
//			mStatus = submitResult->result ;
//			mBytesTransferred = submitResult->bytesTransferred ;
			bcopy(submitResult + 1, mParams->newBuffer, mBytesTransferred) ;
		}
//		else	
//			mIsExecuting = true ;

//		mParams->staleFlags = 0 ;
//		if (!mParams->kernCommandRef)
//			mParams->kernCommandRef = submitResult->kernCommandRef ;
 	}

	return result ;
}

void
IOFireWireLibReadQuadletCommandImp::CommandCompletionHandler(
	void*				refcon,
	IOReturn			result,
	void*				quads[],
	UInt32				numQuads)
{
	numQuads -= 2 ;	// number increased by 2 to make
					// sendAsyncResult always send args as a pointer
					// instead of inline...

	IOFireWireLibReadQuadletCommandImp*	me = (IOFireWireLibReadQuadletCommandImp*)refcon ;

	me->mStatus 			= result ;
	me->mBytesTransferred 	= numQuads << 2 ;
	me->mIsExecuting 		= false ;
	
	bcopy(quads, me->mParams->newBuffer, me->mBytesTransferred) ;

	if (me->mCallback)
		(*(me->mCallback))(me->mRefCon, me->mStatus) ;
}

// ============================================================
//
// IOFireWireLibWriteCommandImp methods
//
// ============================================================
IUnknownVTbl**
IOFireWireLibWriteCommandImp::Alloc(
	IOFireWireDeviceInterfaceImp& inUserClient,
	io_object_t			device,
	const FWAddress &	addr,
	void*				buf,
	UInt32				size,
	IOFireWireLibCommandCallback callback,
	Boolean				failOnReset,
	UInt32				generation,
	void*				inRefCon)
{
	IUnknownVTbl** interface = nil ;
	IOFireWireLibWriteCommandImp*	me = new IOFireWireLibWriteCommandImp(inUserClient, device) ;
	
	if (me)
	{
		if ( !me->Init(addr, buf, size, callback, failOnReset, generation, inRefCon) )
		{
			delete me ;
			me = nil ;
		}
		else
			interface = (IUnknownVTbl**) & me->mInterface ;
	}
	
	return interface;
}

Boolean
IOFireWireLibWriteCommandImp::Init(
	const FWAddress &	addr,
	void*				buf,
	UInt32				size,
	IOFireWireLibCommandCallback callback,
	Boolean				failOnReset,
	UInt32				generation,
	void*				inRefCon)
{
	if (NULL == (mParams = new FWUserCommandSubmitParams))
		return false ;
	if (!IOFireWireLibCommandImp::Init(addr, callback, failOnReset, generation, inRefCon))
		return false ;

	mParams->type			= kFireWireCommandType_Write ;
	mParams->newBuffer		= buf ;
	mParams->newBufferSize	= size ;
	mParams->staleFlags |= kFireWireCommandStale_Buffer ;

//	mSubmitSelector 			= kFWCommand_SubmitWrite ;
//	mSubmitAbsoluteSelector 	= kFWCommand_SubmitWriteAbsolute ;

	return true ;
}

IOFireWireLibWriteCommandImp::IOFireWireLibWriteCommandImp(
	IOFireWireDeviceInterfaceImp& 	inUserClient,
	io_object_t						device): IOFireWireLibCommandImp(inUserClient, device)
{
	// COM support
	mInterface.pseudoVTable = (IUnknownVTbl*) & IOFireWireLibWriteCommandImp::sInterface ;
	mInterface.obj = this ;
	
}

IOReturn
IOFireWireLibWriteCommandImp::Submit()
{
	if (mIsExecuting)
		return kIOReturnBusy ;

	IOReturn 				result 			= kIOReturnSuccess ;

	FWUserCommandSubmitResult	submitResult ;
	mach_msg_type_number_t		submitResultSize = sizeof(submitResult) ;
	UInt32						paramsSize ;

	if (mParams->flags & kFireWireCommandUseCopy)
		paramsSize = sizeof(*mParams) + mParams->newBufferSize ;
	else
		paramsSize = sizeof(*mParams) ;

	result = IOFireWireLibCommandImp::Submit(mParams, paramsSize, & submitResult, & submitResultSize) ;

	return result ;
}

// ============================================================
//
// IOFireWireLibWriteQuadletCommandImp methods
//
// ============================================================
IUnknownVTbl**
IOFireWireLibWriteQuadletCommandImp::Alloc(
	IOFireWireDeviceInterfaceImp& inUserClient,
	io_object_t			device,
	const FWAddress &	addr,
	UInt32				quads[],
	UInt32				numQuads,
	IOFireWireLibCommandCallback callback,
	Boolean				failOnReset,
	UInt32				generation,
	void*				inRefCon)
{
	IUnknownVTbl** interface = nil ;
	IOFireWireLibWriteQuadletCommandImp*	me = 
			new IOFireWireLibWriteQuadletCommandImp(inUserClient, device) ;

	if (me)
	{
		if ( !me->Init(addr, quads, numQuads, callback, failOnReset, generation, inRefCon) )
		{
			delete me ;
			me = nil ;
		}
		else
			interface = (IUnknownVTbl**) & me->mInterface ;
	}
	
	return interface;
}

Boolean
IOFireWireLibWriteQuadletCommandImp::Init(
	const FWAddress &	addr,
	UInt32				quads[],
	UInt32				numQuads,
	IOFireWireLibCommandCallback callback,
	Boolean				failOnReset,
	UInt32				generation,
	void*				inRefCon)
{
	if (NULL == (mParamsExtra = new UInt8[sizeof(FWUserCommandSubmitParams) + numQuads << 2]))
		return false ;
	mParams = (FWUserCommandSubmitParams*) mParamsExtra ;
	if (!IOFireWireLibCommandImp::Init(addr, callback, failOnReset, generation, inRefCon))
		return false ;

	mParams = (FWUserCommandSubmitParams*) mParamsExtra ;
	mParams->type			= kFireWireCommandType_WriteQuadlet ;
	mParams->newBuffer 		= mParams+1;//(void*) quads ;
	mParams->newBufferSize 	= numQuads << 2 ; // * 4
	mParams->staleFlags 	|= kFireWireCommandStale_Buffer ;
	mParams->flags			|= kFireWireCommandUseCopy ;

//	mSubmitSelector 			= kFWCommand_SubmitWrite ;
//	mSubmitAbsoluteSelector 	= kFWCommand_SubmitWriteAbsolute ;

	bcopy(quads, mParams+1, mParams->newBufferSize) ;

	return true;
}

void
IOFireWireLibWriteQuadletCommandImp::SSetQuads(
	IOFireWireLibWriteQuadletCommandRef self,
	UInt32				inQuads[],
	UInt32				inNumQuads)
{
	GetThis(self)->SetQuads(inQuads, inNumQuads) ;
}
									
IOFireWireLibWriteQuadletCommandImp::IOFireWireLibWriteQuadletCommandImp(
	IOFireWireDeviceInterfaceImp& inUserClient,
	io_object_t					device): IOFireWireLibCommandImp(inUserClient, device)
{
	// COM
	mInterface.pseudoVTable = (IUnknownVTbl*) & sInterface ;
	mInterface.obj = this ;
	
}

IOFireWireLibWriteQuadletCommandImp::~IOFireWireLibWriteQuadletCommandImp()
{
	delete[] mParamsExtra ;
	mParamsExtra = nil ;
	mParams = nil ;
}

void
IOFireWireLibWriteQuadletCommandImp::SetQuads(
	UInt32				inQuads[],
	UInt32				inNumQuads)
{
	UInt32	newSize = inNumQuads << 2 ;
	if (newSize > mParams->newBufferSize)
	{
		// we're going to need more room to hold user's quads:
		// set our size to the new size
		mParams->newBufferSize = newSize ;

		// allocate a new submit params + quad storage area:
		UInt8* newParamsExtra = new UInt8[sizeof(FWUserCommandSubmitParams) + newSize] ;

		IOFireWireLibLogIfNil_(newParamsExtra, ("warning: IOFireWireLibWriteQuadletCommandImp::SetQuads: out of memory!\n")) ;

		// copy the old params to the new param block (which is at the beginning of ParamsExtra):
		bcopy(mParams, newParamsExtra+0, sizeof(*mParams)) ;
		
		// delete the old storage
		delete[] mParamsExtra ;
		
		// assign the new storage to the command object:
		mParams			= (FWUserCommandSubmitParams*) newParamsExtra ;
		mParamsExtra 	= newParamsExtra ;
	}

	// copy users quads to storage area (just past end of params...)
	// this allows us to submit the params and quads to the kernel in one
	// operation
	bcopy(inQuads, mParams + 1, mParams->newBufferSize) ;	
	
	// let kernel know that buffer has changed. (but may not be strictly
	// necessary for write quad commands...
	mParams->staleFlags |= kFireWireCommandStale_Buffer ;
}

IOReturn
IOFireWireLibWriteQuadletCommandImp::Submit()
{
	if (mIsExecuting)
		return kIOReturnBusy ;

	IOReturn 				result 			= kIOReturnSuccess ;

//	mParams->syncFlag	= (mParams.callback == NULL) ? kFireWireCommandExecute_Sync : kFireWireCommandExecute_Async ;

	FWUserCommandSubmitResult		submitResult ;
	mach_msg_type_number_t			submitResultSize = sizeof(result) ;

/*	if (mDevice == mUserClient.GetDevice() )
	{
		result = io_async_method_structureI_structureO( mUserClient.GetUserClientConnection(),
														mUserClient.GetAsyncPort(),
														mAsyncRef,
														1,
														kFWCommand_Submit,
														(char*) mParamsExtra,
														sizeof(*mParams) + mParams->newBufferSize,
														(char*) & submitResult,
														& submitResultSize ) ;
	}
	else if (mDevice == 0)
	{
		result = io_async_method_structureI_structureO( mUserClient.GetUserClientConnection(),
														mUserClient.GetAsyncPort(),
														mAsyncRef,
														1,
														kFWCommand_SubmitAbsolute,
														(char*) mParamsExtra,
														sizeof(*mParams) + mParams->newBufferSize,
														(char*) & submitResult,
														& submitResultSize ) ;
	}
	else
		result = kIOReturnNoDevice ; */
	
	IOFireWireLibCommandImp::Submit(mParams, sizeof(*mParams)+mParams->newBufferSize, & submitResult, & submitResultSize) ;

/*	if (kIOReturnSuccess == result)
	{
		if (mParams->flags & kFireWireCommandUseCopy)
		{
			mStatus = submitResult.result ;
			mBytesTransferred = submitResult.bytesTransferred ;
		}
		else	
			mIsExecuting = true ;

		mParams->staleFlags = 0 ;
		if (!mParams->kernCommandRef)
			mParams->kernCommandRef = submitResult.kernCommandRef ;
 	} */

	return result ;
}

// ============================================================
//
// IOFireWireLibCompareSwapCommandImp methods
//
// ============================================================

IUnknownVTbl**	
IOFireWireLibCompareSwapCommandImp::Alloc(
	IOFireWireDeviceInterfaceImp& inUserClient,
	io_object_t			device,
	const FWAddress &	addr,
	UInt32				cmpVal,
	UInt32				newVal,
	IOFireWireLibCommandCallback callback,
	Boolean				failOnReset,
	UInt32				generation,
	void*				inRefCon)
{
	IUnknownVTbl**	interface = nil ;
	IOFireWireLibCompareSwapCommandImp*	me = new IOFireWireLibCompareSwapCommandImp(inUserClient, device) ;

	if (me)
	{
		if ( !me->Init(addr, cmpVal, newVal, callback, failOnReset, generation, inRefCon) )
		{
			delete me ;
			me = nil ;
		}
		else
			interface = (IUnknownVTbl**) & me->mInterface ;
	}
	
	return interface ;
}

Boolean
IOFireWireLibCompareSwapCommandImp::Init(
	const FWAddress &	addr,
	UInt32				cmpVal,
	UInt32				newVal,
	IOFireWireLibCommandCallback callback,
	Boolean				failOnReset,
	UInt32				generation,
	void*				inRefCon)
{
	UInt32	numQuads = 1 ;

	if (NULL == (mParamsExtra = new UInt8[sizeof(FWUserCommandSubmitParams) + numQuads << 3]))
		return false ;
	if (!IOFireWireLibCommandImp::Init(addr, callback, failOnReset, generation, inRefCon))
		return false ;
	
	mParams->type			= kFireWireCommandType_CompareSwap ;
	mParams->newBufferSize 	= numQuads << 2 ;
	mParams->flags			|= kFireWireCommandUseCopy ;	// compare swap always does in-line submits

	if (cmpVal && newVal)
	{
		bcopy(& newVal, (UInt32*)(mParams + 1), mParams->newBufferSize) ;
		bcopy(& cmpVal, (UInt32*)(mParams + 1) + numQuads, mParams->newBufferSize) ;
	}


	return true ;
}

void
IOFireWireLibCompareSwapCommandImp::SSetValues(
	IOFireWireLibCompareSwapCommandRef self,
	UInt32				cmpVal,
	UInt32				newVal)
{
	GetThis(self)->SetValues(cmpVal, newVal) ;
}

// --- ctor/dtor ----------------
IOFireWireLibCompareSwapCommandImp::IOFireWireLibCompareSwapCommandImp(
		IOFireWireDeviceInterfaceImp& inUserClient,
		io_object_t			device): IOFireWireLibCommandImp(inUserClient, device)
{
	// COM support
	mInterface.pseudoVTable = (IUnknownVTbl*) & sInterface ;
	mInterface.obj = this ;
}

IOFireWireLibCompareSwapCommandImp::~IOFireWireLibCompareSwapCommandImp()
{
	delete[] mParamsExtra ;
	mParamsExtra = nil ;
	mParams = nil ;
}

void
IOFireWireLibCompareSwapCommandImp::SetValues(
	UInt32				cmpVal,
	UInt32				newVal)
{
	if (mParams->newBufferSize != 4)
	{
		UInt8*	newParamsExtra = new UInt8[sizeof(FWUserCommandSubmitParams) + 8] ;

		bcopy(mParams, newParamsExtra, sizeof(FWUserCommandSubmitParams)) ;
		mParams = (FWUserCommandSubmitParams*) newParamsExtra ;
	}
	
	mParams->newBufferSize = 4 ;	// 1 quadlet
	((UInt32*)(mParams+1))[0] 	= newVal ;
	((UInt32*)(mParams+1))[1] 	= cmpVal ;
	mParams->staleFlags |= kFireWireCommandStale ;
}

IOReturn
IOFireWireLibCompareSwapCommandImp::Submit()
{
	if (mIsExecuting)
		return kIOReturnBusy ;

	IOReturn 				result 			= kIOReturnSuccess ;

//	mParams.syncFlag	= (mParams.callback == NULL) ? kFireWireCommandExecute_Sync : kFireWireCommandExecute_Async ;

	FWUserCommandSubmitResult		submitResult ;
	mach_msg_type_number_t			submitResultSize = sizeof(result) ;

	result = IOFireWireLibCommandImp::Submit(mParams, (mach_msg_type_number_t)(sizeof(*mParams)+mParams->newBufferSize << 1), 
											 & submitResult, & submitResultSize) ;

/*	if (mDevice == mUserClient.GetDevice() )
	{
		result = io_async_method_structureI_structureO( mUserClient.GetUserClientConnection(),
														mUserClient.GetAsyncPort(),
														mAsyncRef,
														1,
														kFWCommand_Submit,
														(char*) & mParamsExtra,
														sizeof(*mParams) + mParams->newBufferSize << 1,//room for new and cmp val quads
														(char*) & submitResult,
														& submitResultSize ) ;
	}
	else if (mDevice == 0)
	{
		result = io_async_method_structureI_structureO( mUserClient.GetUserClientConnection(),
														mUserClient.GetAsyncPort(),
														mAsyncRef,
														1,
														kFWCommand_SubmitAbsolute,
														(char*) & mParamsExtra,
														sizeof(*mParams) + mParams->newBufferSize << 1,//room for new and cmp val quads
														(char*) & submitResult,
														& submitResultSize ) ;
	}
	else
		result = kIOReturnNoDevice ;
	
	if (kIOReturnSuccess == result)
	{
		if (mParams->syncFlag)
		{
			mStatus = submitResult.result ;
			mBytesTransferred = submitResult.bytesTransferred ;
		}
		else	
			mIsExecuting = true ;

		mParams->staleFlags = 0 ;
		if (!mParams->kernCommandRef)
			mParams->kernCommandRef = submitResult.kernCommandRef ;
 	}*/

	return result ;
}
