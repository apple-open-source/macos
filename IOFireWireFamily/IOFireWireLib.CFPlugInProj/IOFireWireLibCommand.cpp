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
*  Created on Fri Dec 15 2000.
*  Copyright (c) 2000-2002 Apple, Inc. All rights reserved.
*
*/

#import <assert.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/iokitmig.h>

#import "IOFireWireLib.h"
#import "IOFireWireLibPriv.h"
#import "IOFireWireLibCommand.h"

#define IOFIREWIRELIBCOMMANDIMP_INTERFACE \
	& Cmd::SGetStatus,	\
	& Cmd::SGetTransferredBytes,	\
	& Cmd::SGetTargetAddress,	\
	& Cmd::SSetTarget,	\
	& Cmd::SSetGeneration,	\
	& Cmd::SSetCallback,	\
	& Cmd::SSetRefCon,	\
	& Cmd::SIsExecuting,	\
	& Cmd::SSubmit,	\
	& Cmd::SSubmitWithRefconAndCallback, \
	& Cmd::SCancel

#define IOFIREWIRELIBCOMMANDIMP_INTERFACE_v2	\
	& Cmd::SSetBuffer,	\
	& Cmd::SGetBuffer,	\
	& Cmd::SSetMaxPacket,	\
	& Cmd::SSetFlags

namespace IOFireWireLib {

	ReadCmd::Interface ReadCmd::sInterface =
	{
		INTERFACEIMP_INTERFACE,
		1, 1, // version/revision
		
		IOFIREWIRELIBCOMMANDIMP_INTERFACE,
		IOFIREWIRELIBCOMMANDIMP_INTERFACE_v2
	} ;
	
	ReadQuadCmd::Interface ReadQuadCmd::sInterface =
	{
		INTERFACEIMP_INTERFACE,
		1, 0, // version/revision
		
		IOFIREWIRELIBCOMMANDIMP_INTERFACE,
	
		& ReadQuadCmd::SSetQuads
	} ;
	
	WriteCmd::Interface WriteCmd::sInterface =
	{
		INTERFACEIMP_INTERFACE,
		1, 1, // version/revision
		
		IOFIREWIRELIBCOMMANDIMP_INTERFACE,
		IOFIREWIRELIBCOMMANDIMP_INTERFACE_v2
	} ;
	
	WriteQuadCmd::Interface WriteQuadCmd::sInterface =
	{
		INTERFACEIMP_INTERFACE,
		1, 0, // version/revision
		IOFIREWIRELIBCOMMANDIMP_INTERFACE,
	
		& WriteQuadCmd::SSetQuads
	} ;
	
	CompareSwapCmd::Interface CompareSwapCmd::sInterface =
	{
		INTERFACEIMP_INTERFACE,
		2, 0, // version/revision
		IOFIREWIRELIBCOMMANDIMP_INTERFACE,
	
		// --- v1
		& CompareSwapCmd::SSetValues,
	
		// --- v2
		& CompareSwapCmd::SSetValues64,
		& CompareSwapCmd::SDidLock,
		& CompareSwapCmd::SLocked,
		& CompareSwapCmd::SLocked64,
		& CompareSwapCmd::SSetFlags
		
	} ;
	
	// ==================================
	// virtual members
	// ==================================
#pragma mark -
	IOFireWireCommandInterface 	Cmd::sInterface = 
	{
		INTERFACEIMP_INTERFACE,
		1, 0, 									// version/revision
		IOFIREWIRELIBCOMMANDIMP_INTERFACE
	} ;

	Cmd::Cmd( IUnknownVTbl* vtable, Device& userClient, io_object_t device, 
					const FWAddress& inAddr, CommandCallback inCallback, 
					const bool inFailOnReset, const UInt32 inGeneration, void* inRefCon,
					CommandSubmitParams* params )
	:	IOFireWireIUnknown( vtable ),
		mUserClient( userClient ),
		mDevice( device ),
		mBytesTransferred( 0 ),
		mIsExecuting( false ),
		mStatus( kIOReturnSuccess ),
		mRefCon( inRefCon ),
		mCallback( 0 ),
		mParams(params)
	{
		mUserClient.AddRef() ;
		bzero(mParams, sizeof(*mParams)) ;

		mParams->callback		= (void*)& CommandCompletionHandler ;
		mParams->refCon			= this ;
		SetTarget(inAddr) ;
		SetGeneration(inGeneration) ;
		mParams->staleFlags 	= kFireWireCommandStale + kFireWireCommandStale_Buffer ;
		if (0==device)
			mParams->flags |= kFireWireCommandAbsolute ;
		mParams->newFailOnReset = inFailOnReset ;
		SetCallback(inCallback) ;
	}
	
	Cmd::~Cmd() 
	{
		if (mParams)
		{
			if (mParams->kernCommandRef)
			{
				IOReturn result = kIOReturnSuccess;
				result = IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kCommand_Release,
																		1, 0, mParams->kernCommandRef ) ;
				IOFireWireLibLogIfErr_( result, "Cmd::~Cmd: command release returned 0x%08x\n", result) ;
			}
		
			delete mParams ;
		}
		
		mUserClient.Release() ;
	}

	HRESULT
	Cmd::QueryInterface(REFIID iid, LPVOID* ppv)
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if (CFEqual(interfaceID, IUnknownUUID) || CFEqual(interfaceID, kIOFireWireCommandInterfaceID) )
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

	// --- setters -----------------------
	void
	Cmd::SetTarget(
		const FWAddress&	addr)
	{
		mParams->newTarget = addr ;
//		mParams->staleFlags |= kFireWireCommandStale ;
	}
	
	void
	Cmd::SetGeneration(
		UInt32				inGeneration)
	{
		mParams->newGeneration = inGeneration ;
//		mParams->staleFlags |= kFireWireCommandStale ;
	}
	
	void
	Cmd::SetCallback(
		CommandCallback inCallback)
	{
		mCallback = inCallback ;
	
		if (!mCallback)
			mParams->flags |= kFWCommandInterfaceSyncExecute ;
		else
			mParams->flags &= ~kFWCommandInterfaceSyncExecute ;
	}

	IOReturn
	Cmd::Submit(
		CommandSubmitParams*		params,
		mach_msg_type_number_t		paramsSize,
		CommandSubmitResult*		submitResult,
		mach_msg_type_number_t*		submitResultSize)
	{
		// staleFlags will be reset to 0 on each Submit() call,
		// so we set kFireWireCommandStale_MaxPacket before we submit
		if (mParams->newMaxPacket > 0)
			mParams->staleFlags |= kFireWireCommandStale_MaxPacket;
		
		IOReturn 			err ;
		err = io_async_method_structureI_structureO( mUserClient.GetUserClientConnection(),
														mUserClient.GetAsyncPort(),
														mAsyncRef,
														1,
														kCommand_Submit,
														(char*) params,
														paramsSize,
														(char*) submitResult,
														submitResultSize ) ;
		
		if ( !err )
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
	
		return err ;
	}
	
	IOReturn
	Cmd::SubmitWithRefconAndCallback(
		void*	inRefCon,
		CommandCallback	inCallback)
	{
		if (mIsExecuting)
			return kIOReturnBusy ;
		
		mRefCon = inRefCon ;
		SetCallback(inCallback) ;
	
		return Submit() ;
	}
	
	IOReturn
	Cmd::Cancel(
		IOReturn	reason)
	{
		if (!mIsExecuting)
			return kIOReturnSuccess ;
		
		return IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(),
											kCommand_Cancel,
											1,
											0,
											mParams->kernCommandRef) ;
	}
	
	void
	Cmd::SetBuffer(
		UInt32				inSize,
		void*				inBuffer)
	{
		mParams->newBufferSize = inSize ;
		mParams->newBuffer = inBuffer ;
		mParams->staleFlags |= kFireWireCommandStale_Buffer ;
	}
	
	void
	Cmd::GetBuffer(
		UInt32*				outSize,
		void**				outBuf)
	{
		*outSize = mParams->newBufferSize ;
		*outBuf	= mParams->newBuffer ;
	}
	
	IOReturn
	Cmd::SetMaxPacket(
		IOByteCount				inMaxBytes)
	{
		mParams->newMaxPacket = inMaxBytes ;
		// staleFlags will be reset to 0 on each Submit() call,
		// so we set kFireWireCommandStale_MaxPacket before we submit

//		mParams->staleFlags |= kFireWireCommandStale_MaxPacket;
		return kIOReturnSuccess ;
	}
	
	void
	Cmd::SetFlags(
		UInt32					inFlags)
	{
		if (mParams->flags & ~kFireWireCommandUserFlagsMask)
			IOFireWireLibLog_("Invalid flags %p passed to SetFlags!\n", inFlags) ;
	
		mParams->flags &= ~kFireWireCommandUserFlagsMask ;
		mParams->flags |= (inFlags & kFireWireCommandUserFlagsMask) ;
	
		if (mParams->flags & kFWCommandInterfaceForceCopyAlways)
			mParams->flags |= kFireWireCommandUseCopy ;
		if (mParams->flags & kFWCommandInterfaceForceNoCopy)
			mParams->flags &= ~kFireWireCommandUseCopy ;
		if (mParams->flags & kFWCommandInterfaceAbsolute )
			mParams->flags |= kFireWireCommandAbsolute ;
	}
	
	void
	Cmd::CommandCompletionHandler(
		void*				refcon,
		IOReturn			result,
		IOByteCount			bytesTransferred)
	{
		Cmd*	me = (Cmd*)refcon ;
	
		me->mStatus 			= result ;
		me->mBytesTransferred 	= bytesTransferred ;
		me->mIsExecuting 		= false ;
		
		if (me->mCallback)
			(*(me->mCallback))(me->mRefCon, me->mStatus) ;
	}
	
	// --- getters -----------------------
	IOReturn
	Cmd::SGetStatus(
		IOFireWireLibCommandRef	self)
	{
		return IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->mStatus ;
	}
	
	UInt32
	Cmd::SGetTransferredBytes(
		IOFireWireLibCommandRef	self)
	{
		return IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->mBytesTransferred ;
	}
	
	void
	Cmd::SGetTargetAddress(
		IOFireWireLibCommandRef	self,
		FWAddress*				outAddr)
	{
		*outAddr = IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->mParams->newTarget ;
	}
	
	// --- setters -----------------------
	void
	Cmd::SSetTarget(
		IOFireWireLibCommandRef	self,
		const FWAddress*	inAddr)
	{	
		IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->SetTarget(*inAddr) ;
	}
	
	void
	Cmd::SSetGeneration(
		IOFireWireLibCommandRef	self,
		UInt32					inGeneration)
	{
		IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->SetGeneration(inGeneration) ;
	}
	
	void
	Cmd::SSetCallback(
		IOFireWireLibCommandRef			self,
		CommandCallback	inCallback)
	{
		IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->SetCallback(inCallback) ;
	}
	
	void
	Cmd::SSetRefCon(
		IOFireWireLibCommandRef	self,
		void*					refCon)
	{
		IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->mRefCon = refCon ;
	}
	
	const Boolean
	Cmd::SIsExecuting(
		IOFireWireLibCommandRef	self)
	{ 
		return IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->mIsExecuting; 
	}
	
	IOReturn
	Cmd::SSubmit(
		IOFireWireLibCommandRef	self) 
	{
		return IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->Submit() ;
	}
	
	IOReturn
	Cmd::SSubmitWithRefconAndCallback(
		IOFireWireLibCommandRef	self,
		void*					inRefCon,
		CommandCallback inCallback)
	{
		return IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->SubmitWithRefconAndCallback(inRefCon, inCallback) ;
	}
	
	IOReturn
	Cmd::SCancel(
		IOFireWireLibCommandRef	self,
		IOReturn				reason)
	{
		return IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->Cancel(reason) ;
	}
	
	void
	Cmd::SSetBuffer(
		IOFireWireLibCommandRef		self,
		UInt32						inSize,
		void*						inBuf)
	{
		IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->SetBuffer(inSize, inBuf) ;
	}
	
	void
	Cmd::SGetBuffer(
		IOFireWireLibCommandRef		self,
		UInt32*						outSize,
		void**						outBuf)
	{
		IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->GetBuffer(outSize, outBuf) ;
	}
	
	IOReturn
	Cmd::SSetMaxPacket(
		IOFireWireLibCommandRef self,
		IOByteCount				inMaxBytes)
	{
		return IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->SetMaxPacket(inMaxBytes) ;
	}
	
	void
	Cmd::SSetFlags(
		IOFireWireLibCommandRef	self,
		UInt32					inFlags)
	{
		IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->SetFlags(inFlags) ;
	}
	
	// ============================================================
	//
	// ReadCmd methods
	//
	// ============================================================
#pragma mark -
	ReadCmd::ReadCmd( Device& userclient, io_object_t device, const FWAddress& addr, void* buf,
							UInt32 size, CommandCallback callback, bool failOnReset, 
							UInt32 generation, void* refcon )
	: Cmd( reinterpret_cast<IUnknownVTbl*>(& sInterface), userclient, device, addr, callback, 
				failOnReset, generation, refcon, 
				reinterpret_cast<CommandSubmitParams*>(new UInt8[sizeof(CommandSubmitParams)]) )
	{
		mParams->type			 = kFireWireCommandType_Read ;
		mParams->newBuffer		 = buf ;
		mParams->newBufferSize	 = size ;
		mParams->staleFlags 	|= kFireWireCommandStale_Buffer ;
	}
	
	HRESULT
	ReadCmd::QueryInterface(REFIID iid, LPVOID* ppv)
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if ( CFEqual( interfaceID, IUnknownUUID )
			|| CFEqual( interfaceID, kIOFireWireCommandInterfaceID)
			|| CFEqual( interfaceID, kIOFireWireReadCommandInterfaceID)
			|| CFEqual( interfaceID, kIOFireWireReadCommandInterfaceID_v2 ) )
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
	
	IUnknownVTbl**
	ReadCmd::Alloc(
		Device& 			userclient,
		io_object_t			device,
		const FWAddress&	addr,
		void*				buf,
		UInt32				size,
		CommandCallback callback,
		bool				failOnReset,
		UInt32				generation,
		void*				inRefCon)
	{
		ReadCmd*	me = new ReadCmd( userclient, device, addr, buf, size, callback, failOnReset, generation, inRefCon ) ;
		if (!me)
			return nil ;

		return reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	IOReturn
	ReadCmd::Submit()
	{
		if ( mIsExecuting )
			return kIOReturnBusy ;
	
		IOReturn					result 				= kIOReturnSuccess ;
		UInt8						submitResultExtra[ sizeof(CommandSubmitResult) + kFWUserCommandSubmitWithCopyMaxBufferBytes ] ;
		CommandSubmitResult*		submitResult 		= (CommandSubmitResult*) submitResultExtra ;
		mach_msg_type_number_t		submitResultSize ;
		
		if (mParams->flags & kFireWireCommandUseCopy)
			submitResultSize = sizeof(*submitResult) + mParams->newBufferSize ;
		else
			submitResultSize = sizeof(*submitResult) ;
	
		result = Cmd::Submit(mParams, sizeof(*mParams), submitResult, & submitResultSize) ;
		
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
	// ReadQuadCmd methods
	//
	// ============================================================
#pragma mark -
	ReadQuadCmd::ReadQuadCmd(	Device& 						userclient,
								io_object_t						device,
								const FWAddress &				addr,
								UInt32							quads[],
								UInt32							numQuads,
								CommandCallback 	callback,
								Boolean							failOnReset,
								UInt32							generation,
								void*							refcon)
	: Cmd( reinterpret_cast<IUnknownVTbl*>(&sInterface), userclient, device, addr, callback, 
				failOnReset, generation, refcon, 
				reinterpret_cast<CommandSubmitParams*>(new UInt8[sizeof(CommandSubmitParams)]) )
	{		
		mParams->callback		= (void*)& CommandCompletionHandler ;
		mParams->type			= kFireWireCommandType_ReadQuadlet ;
		mParams->newBuffer		= quads ;
		mParams->newBufferSize	= numQuads << 2 ;	// x * 4
		mParams->staleFlags 	|= kFireWireCommandStale_Buffer ;
		mParams->flags			|= kFireWireCommandUseCopy ;
	}
	
	IUnknownVTbl**
	ReadQuadCmd::Alloc( Device& inUserClient, io_object_t device, const FWAddress& addr, UInt32 quads[], UInt32 numQuads, 
								CommandCallback callback, Boolean failOnReset, UInt32 generation, void* refcon)
	{
		ReadQuadCmd*	me = new ReadQuadCmd(inUserClient, device, addr, quads, numQuads, callback, failOnReset, generation, refcon) ;
		if (!me)
			return nil ;

		return reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	HRESULT
	ReadQuadCmd::QueryInterface(REFIID iid, LPVOID* ppv)
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if (CFEqual(interfaceID, IUnknownUUID) || CFEqual(interfaceID, kIOFireWireCommandInterfaceID) || CFEqual(interfaceID, kIOFireWireReadQuadletCommandInterfaceID) )
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

	void
	ReadQuadCmd::SetFlags(
		UInt32					inFlags)
	{
		Cmd::SetFlags(inFlags) ;
		
		// force always copy:
		mParams->flags |= kFireWireCommandUseCopy ;
	}

	void
	ReadQuadCmd::SetQuads(
		UInt32				inQuads[],
		UInt32				inNumQuads)
	{
		mParams->newBufferSize = inNumQuads << 2 ; // * 4
		mParams->newBuffer = (void*) inQuads ;
		mNumQuads = inNumQuads ;
		
		mParams->staleFlags |= kFireWireCommandStale_Buffer ;
	}
	
	IOReturn
	ReadQuadCmd::Submit()
	{
		if (mIsExecuting)
			return kIOReturnBusy ;
	
		IOReturn 					result 			= kIOReturnSuccess ;
		Boolean						syncFlag = mParams->flags & kFWCommandInterfaceSyncExecute ;
		UInt8						submitResultExtra[sizeof(CommandSubmitResult) + (syncFlag ? mParams->newBufferSize : 0)] ;
		mach_msg_type_number_t		submitResultSize = sizeof(submitResultExtra) ;
		CommandSubmitResult*		submitResult = reinterpret_cast<CommandSubmitResult*>(submitResultExtra) ;
		
		result = Cmd::Submit(mParams, sizeof(*mParams), submitResult, & submitResultSize) ;
		
		if (kIOReturnSuccess == result && syncFlag)
		{
			bcopy(submitResult + 1, mParams->newBuffer, mBytesTransferred) ;
		}
	
		return result ;
	}
	
	void
	ReadQuadCmd::CommandCompletionHandler(
		void*				refcon,
		IOReturn			result,
		void*				quads[],
		UInt32				numQuads)
	{
		numQuads -= 2 ;	// number increased by 2 to make
						// sendAsyncResult always send args as a pointer
						// instead of inline...
	
		ReadQuadCmd*	me = (ReadQuadCmd*)refcon ;
	
		me->mStatus 			= result ;
		me->mBytesTransferred 	= numQuads << 2 ;
		me->mIsExecuting 		= false ;
		
		bcopy(quads, me->mParams->newBuffer, me->mBytesTransferred) ;
	
		if (me->mCallback)
			(*(me->mCallback))(me->mRefCon, me->mStatus) ;
	}
	
	void
	ReadQuadCmd::SSetQuads(
		IOFireWireLibReadQuadletCommandRef self,
		UInt32				inQuads[],
		UInt32				inNumQuads)
	{
		IOFireWireIUnknown::InterfaceMap<ReadQuadCmd>::GetThis(self)->SetQuads(inQuads, inNumQuads) ;
	}

	// ============================================================
	//
	// WriteCmd methods
	//
	// ============================================================
#pragma mark -
	WriteCmd::WriteCmd( Device& userclient, io_object_t device, const FWAddress& addr, void* buf, UInt32 size, 
								CommandCallback callback, bool failOnReset, UInt32 generation, void* refcon )
	: Cmd( reinterpret_cast<IUnknownVTbl*>(& sInterface), userclient, device, addr, callback, 
				failOnReset, generation, refcon, 
				reinterpret_cast<CommandSubmitParams*>(new UInt8[sizeof(CommandSubmitParams)]) )
	{
		mParams->type			= kFireWireCommandType_Write ;
		mParams->newBuffer		= buf ;
		mParams->newBufferSize	= size ;
		mParams->staleFlags |= kFireWireCommandStale_Buffer ;
	}

	IUnknownVTbl**
	WriteCmd::Alloc( Device& userclient, io_object_t device, const FWAddress& addr, void* buf, UInt32 size, 
							CommandCallback callback, bool failOnReset, UInt32 generation, void* refcon)
	{
		WriteCmd*	me = new WriteCmd( userclient, device, addr, buf, size, callback, failOnReset, generation, refcon ) ;

		if (!me)
			return nil ;
		
		return reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	HRESULT
	WriteCmd::QueryInterface(REFIID iid, LPVOID* ppv)
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if ( CFEqual(interfaceID, IUnknownUUID) 
			|| CFEqual( interfaceID, kIOFireWireCommandInterfaceID ) 
			|| CFEqual( interfaceID, kIOFireWireWriteCommandInterfaceID )
			|| CFEqual( interfaceID, kIOFireWireWriteCommandInterfaceID_v2 ) )
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
	
	IOReturn
	WriteCmd::Submit()
	{
		if (mIsExecuting)
			return kIOReturnBusy ;
	
		CommandSubmitResult			submitResult ;
		mach_msg_type_number_t		submitResultSize = sizeof(submitResult) ;
		UInt32						paramsSize ;
	
		if (mParams->flags & kFireWireCommandUseCopy)
			paramsSize = sizeof(*mParams) + mParams->newBufferSize ;
		else
			paramsSize = sizeof(*mParams) ;
	
		return Cmd::Submit(mParams, paramsSize, & submitResult, & submitResultSize) ;
	}
	
	// ============================================================
	//
	// WriteQuadCmd methods
	//
	// ============================================================
#pragma mark -
	WriteQuadCmd::WriteQuadCmd( Device& userclient, io_object_t device, const FWAddress& addr, UInt32 quads[], UInt32 numQuads,
										CommandCallback callback, bool failOnReset, UInt32 generation, void* refcon )
	: Cmd( reinterpret_cast<IUnknownVTbl*>(& sInterface), userclient, device, addr, callback, failOnReset, generation, refcon, 
				reinterpret_cast<CommandSubmitParams*>(new UInt8[sizeof(CommandSubmitParams) + numQuads << 2]) ),
	mParamsExtra( reinterpret_cast<UInt8*>(mParams) )
	{
		mParams->type			= kFireWireCommandType_WriteQuadlet ;
		mParams->newBuffer 		= mParams+1;//(void*) quads ;
		mParams->newBufferSize 	= numQuads << 2 ; // * 4
		mParams->staleFlags 	|= kFireWireCommandStale_Buffer ;
		mParams->flags			|= kFireWireCommandUseCopy ;
	
		bcopy(quads, mParams+1, mParams->newBufferSize) ;	
	}
	
	WriteQuadCmd::~WriteQuadCmd()
	{
		delete[] mParamsExtra ;
		mParamsExtra = nil ;
		mParams = nil ;
	}
	
	HRESULT
	WriteQuadCmd::QueryInterface(REFIID iid, LPVOID* ppv)
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if (CFEqual(interfaceID, IUnknownUUID) || CFEqual(interfaceID, kIOFireWireCommandInterfaceID) || CFEqual(interfaceID, kIOFireWireWriteQuadletCommandInterfaceID) )
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

	IUnknownVTbl**
	WriteQuadCmd::Alloc(
		Device& 			userclient,
		io_object_t			device,
		const FWAddress &	addr,
		UInt32				quads[],
		UInt32				numQuads,
		CommandCallback callback,
		bool				failOnReset,
		UInt32				generation,
		void*				refcon)
	{
		WriteQuadCmd*	me = new WriteQuadCmd( userclient, device, addr, quads, numQuads, callback, failOnReset, generation, refcon ) ;
		if (!me)
			return nil ;
		
		return reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	void
	WriteQuadCmd::SetFlags(
		UInt32					inFlags)
	{
		Cmd::SetFlags(inFlags) ;
		
		// force always copy:
		mParams->flags |= kFireWireCommandUseCopy ;
	}

	void
	WriteQuadCmd::SetQuads(
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
			UInt8* newParamsExtra = new UInt8[sizeof(CommandSubmitParams) + newSize] ;
	
			IOFireWireLibLogIfNil_(newParamsExtra, ("warning: WriteQuadCmd::SetQuads: out of memory!\n")) ;
	
			// copy the old params to the new param block (which is at the beginning of ParamsExtra):
			bcopy(mParams, newParamsExtra+0, sizeof(*mParams)) ;
			
			// delete the old storage
			delete[] mParamsExtra ;
			
			// assign the new storage to the command object:
			mParams			= (CommandSubmitParams*) newParamsExtra ;
			mParamsExtra 	= newParamsExtra ;
		}
	
		// copy users quads to storage area (just past end of params...)
		// this allows us to submit the params and quads to the kernel in one
		// operation
		bcopy(inQuads, mParams + 1, mParams->newBufferSize) ;	
		
		// let kernel know that buffer has changed. (but may not be strictly
		// necessary for write quad commands...)
		mParams->staleFlags |= kFireWireCommandStale_Buffer ;
	}
	
	IOReturn
	WriteQuadCmd::Submit()
	{
		if (mIsExecuting)
			return kIOReturnBusy ;
	
		CommandSubmitResult			submitResult ;
		mach_msg_type_number_t		submitResultSize = sizeof(submitResult) ;

		return Cmd::Submit(mParams, sizeof(*mParams)+mParams->newBufferSize, & submitResult, & submitResultSize) ;
	}
	
	void
	WriteQuadCmd::SSetQuads(
		CmdRef self,
		UInt32				inQuads[],
		UInt32				inNumQuads)
	{
		IOFireWireIUnknown::InterfaceMap<WriteQuadCmd>::GetThis(self)->SetQuads(inQuads, inNumQuads) ;
	}
										
	// ============================================================
	//
	// CompareSwapCmd methods
	//
	// ============================================================
#pragma mark -
	CompareSwapCmd::CompareSwapCmd(	Device& userclient, io_object_t device, const FWAddress& addr, UInt64 cmpVal, UInt64 newVal,
											unsigned int quads, CommandCallback callback, bool failOnReset,
											UInt32 generation, void* refcon )
	: Cmd( reinterpret_cast<IUnknownVTbl*>(& sInterface), userclient, device, addr, callback, failOnReset, generation, refcon, 
				reinterpret_cast<CommandSubmitParams*>( new UInt8[sizeof(CommandSubmitParams) + sizeof(UInt64) * 2] ) ),	// 8 bytes/UInt16 * 2 values/cmd
	mParamsExtra( reinterpret_cast<UInt8*>(mParams) )
	{
		mParams->callback		= (void*)& CommandCompletionHandler ;
		mParams->type			= kFireWireCommandType_CompareSwap ;
		mParams->newBufferSize 	= quads * sizeof(UInt32) ;
		mParams->flags			|= kFireWireCommandUseCopy ;	// compare swap always does in-line submits

		if (quads == 1)
		{
			((UInt32*)(mParams+1))[0] = cmpVal ;
			((UInt32*)(mParams+1))[2] = newVal ;
		}
		else
		{
			((UInt64*)(mParams+1))[0] = cmpVal ;
			((UInt64*)(mParams+1))[1] = newVal ;
		}
	}	

	CompareSwapCmd::~CompareSwapCmd()
	{
		delete[] mParamsExtra ;
		mParamsExtra = nil ;
		mParams = nil ;
	}
	
	IUnknownVTbl**
	CompareSwapCmd::Alloc(
		Device& 			userclient,
		io_object_t			device,
		const FWAddress &	addr,
		UInt64				cmpVal,
		UInt64				newVal,
		unsigned int		quads,
		CommandCallback		callback,
		bool				failOnReset,
		UInt32				generation,
		void*				refcon)
	{
		CompareSwapCmd*	me = new CompareSwapCmd( userclient, device, addr, cmpVal, newVal, quads, 
														callback, failOnReset, generation, refcon ) ;
		if (!me)
			return nil ;
		
		return reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	HRESULT
	CompareSwapCmd::QueryInterface(REFIID iid, LPVOID* ppv)
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if ( CFEqual(interfaceID, IUnknownUUID) 
				|| CFEqual(interfaceID, kIOFireWireCommandInterfaceID) 
				|| CFEqual(interfaceID, kIOFireWireCompareSwapCommandInterfaceID) 
			// v2
				|| CFEqual(interfaceID, kIOFireWireCompareSwapCommandInterfaceID_v2 ) )
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
	
	void
	CompareSwapCmd::SetFlags(
		UInt32					inFlags)
	{
		Cmd::SetFlags(inFlags) ;
		
		// force always copy:
		mParams->flags |= kFireWireCommandUseCopy ;
	}

	IOReturn
	CompareSwapCmd::SetMaxPacket(
		IOByteCount				inMaxBytes)
	{
		return kIOReturnUnsupported ;
	}
	
	void
	CompareSwapCmd::SetValues(
		UInt32				cmpVal,
		UInt32				newVal)
	{
		mParams->newBufferSize = sizeof(UInt32) ;	// 1 quadlet
		((UInt32*)(mParams+1))[0] 	= cmpVal ;
		((UInt32*)(mParams+1))[1] 	= newVal ;
		mParams->staleFlags |= kFireWireCommandStale ;
	}
	
	void
	CompareSwapCmd::SetValues(
		UInt64 				cmpVal, 
		UInt64 				newVal)
	{
		mParams->newBufferSize = sizeof(UInt64) ;	// 1 quadlet
		((UInt64*)(mParams+1))[0] 	= cmpVal ;
		((UInt64*)(mParams+1))[1] 	= newVal ;
		mParams->staleFlags |= kFireWireCommandStale ;		
	}

	IOReturn
	CompareSwapCmd::Submit()
	{
		if (mIsExecuting)
			return kIOReturnBusy ;
	
		IOReturn error = kIOReturnSuccess ;
	
		CompareSwapSubmitResult			submitResult ;
		mach_msg_type_number_t			submitResultSize = sizeof(submitResult) ;

		error = Cmd::Submit(mParams, (mach_msg_type_number_t)(sizeof(*mParams)+2*sizeof(UInt64)), 
												reinterpret_cast<CommandSubmitResult*>(& submitResult), & submitResultSize) ;
	
		if ( not error )
		{
			if (mParams->flags & kFWCommandInterfaceSyncExecute)
			{
				mStatus = submitResult.result ;
				mBytesTransferred = submitResult.bytesTransferred ;
				bcopy( & submitResult, & mSubmitResult, sizeof(mSubmitResult)) ;
			}
			else	
				mIsExecuting = true ;
	
			mParams->staleFlags = 0 ;
			if (!mParams->kernCommandRef)
				mParams->kernCommandRef = submitResult.kernCommandRef ;
		}

		return error ;
	}

	Boolean
	CompareSwapCmd::DidLock()
	{
		return mSubmitResult.lockInfo.didLock ;
	}
	
	IOReturn
	CompareSwapCmd::Locked(
		UInt32* 			oldValue)
	{
		if (mIsExecuting)
			return kIOReturnBusy ;
		if (!mParams->kernCommandRef)
			return kIOReturnError ;
		if (mParams->newBufferSize != sizeof(UInt32))
			return kIOReturnBadArgument ;

		*oldValue = *(UInt32*)&mSubmitResult.lockInfo.value ;
		return kIOReturnSuccess ;
	}
	
	IOReturn
	CompareSwapCmd::Locked(
		UInt64* 			oldValue)
	{
		if (mIsExecuting)
			return kIOReturnBusy ;
		if (!mParams->kernCommandRef)
			return kIOReturnError ;
		if (mParams->newBufferSize != sizeof(UInt64))
			return kIOReturnBadArgument ;

		*oldValue = mSubmitResult.lockInfo.value ;
		return kIOReturnSuccess ;
	}

	void
	CompareSwapCmd::SSetValues(
		CmdRef				self,
		UInt32				cmpVal,
		UInt32				newVal)
	{
		IOFireWireIUnknown::InterfaceMap<CompareSwapCmd>::GetThis(self)->SetValues(cmpVal, newVal ) ;
	}

	void
	CompareSwapCmd::SSetValues64(
		CmdRef			 	self, 
		UInt64 				cmpVal, 
		UInt64 				newVal)
	{
		IOFireWireIUnknown::InterfaceMap<CompareSwapCmd>::GetThis(self)->SetValues( cmpVal, newVal ) ;
	}
	
	Boolean
	CompareSwapCmd::SDidLock(
		CmdRef				self)
	{
		return GetThis(self)->DidLock() ;
	}
	
	IOReturn
	CompareSwapCmd::SLocked(
		CmdRef 				self, 
		UInt32* 			oldValue)
	{
		return GetThis(self)->Locked(oldValue) ;
	}
	
	IOReturn
	CompareSwapCmd::SLocked64(
		CmdRef				self, 
		UInt64* 			oldValue)
	{
		return GetThis(self)->Locked(oldValue) ;
	}

	void
	CompareSwapCmd::SSetFlags( CmdRef self, UInt32 inFlags )
	{
		IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->SetFlags(inFlags) ;
	}

	void
	CompareSwapCmd::CommandCompletionHandler(
		void*				refcon,
		IOReturn			result,
		void*				quads[],
		UInt32				numQuads)
	{
		CompareSwapCmd*	me = reinterpret_cast<CompareSwapCmd*>(refcon) ;
	
		bcopy((CompareSwapSubmitResult*)quads, & me->mSubmitResult, sizeof(me->mSubmitResult)) ;

		me->mStatus 			= result ;
		me->mBytesTransferred	= me->mSubmitResult.bytesTransferred ;
		me->mIsExecuting 		= false ;		
	
		if (me->mCallback)
			(*(me->mCallback))(me->mRefCon, me->mStatus) ;
	}
}	// namespace IOFireWireLib
