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

// public
#import <IOKit/firewire/IOFireWireLib.h>

// private
#import "IOFireWireLibCommand.h"
#import "IOFireWireLibDevice.h"
#import "IOFireWireLibPriv.h"

// system
#import <assert.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/iokitmig.h>
#import <System/libkern/OSCrossEndian.h>

#define IOFIREWIRELIBCOMMANDIMP_INTERFACE \
	&Cmd::SGetStatus,	\
	&Cmd::SGetTransferredBytes,	\
	&Cmd::SGetTargetAddress,	\
	&Cmd::SSetTarget,	\
	&Cmd::SSetGeneration,	\
	&Cmd::SSetCallback,	\
	&Cmd::SSetRefCon,	\
	&Cmd::SIsExecuting,	\
	&Cmd::SSubmit,	\
	&Cmd::SSubmitWithRefconAndCallback, \
	&Cmd::SCancel

#define IOFIREWIRELIBCOMMANDIMP_INTERFACE_v2	\
	&Cmd::SSetBuffer,	\
	&Cmd::SGetBuffer,	\
	&Cmd::SSetMaxPacket,	\
	&Cmd::SSetFlags

#define IOFIREWIRELIBCOMMANDIMP_INTERFACE_v3	\
	&Cmd::SSetTimeoutDuration,	\
	&Cmd::SSetMaxRetryCount,	\
	&Cmd::SGetAckCode,		\
	&Cmd::SGetResponseCode,		\
	&Cmd::SSetMaxPacketSpeed,	\
	&Cmd::SGetRefCon
	
namespace IOFireWireLib {

	ReadCmd::Interface ReadCmd::sInterface =
	{
		INTERFACEIMP_INTERFACE,
		1, 1, // version/revision
		
		IOFIREWIRELIBCOMMANDIMP_INTERFACE,
		IOFIREWIRELIBCOMMANDIMP_INTERFACE_v2,
		IOFIREWIRELIBCOMMANDIMP_INTERFACE_v3
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
		IOFIREWIRELIBCOMMANDIMP_INTERFACE_v2,
		IOFIREWIRELIBCOMMANDIMP_INTERFACE_v3
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

	CompareSwapCmd::Interface_v3 CompareSwapCmd::sInterface_v3 =
	{
		INTERFACEIMP_INTERFACE,
		2, 0, // version/revision
		IOFIREWIRELIBCOMMANDIMP_INTERFACE,
		IOFIREWIRELIBCOMMANDIMP_INTERFACE_v2,
		IOFIREWIRELIBCOMMANDIMP_INTERFACE_v3,
	
		// --- v1
		(void (*)(IOFireWireLibCompareSwapCommandV3Ref, UInt32, UInt32)) &CompareSwapCmd::SSetValues,
	
		// --- v2
		(void (*)(IOFireWireLibCompareSwapCommandV3Ref, UInt64, UInt64)) &CompareSwapCmd::SSetValues64,
		(Boolean (*)(IOFireWireLibCompareSwapCommandV3Ref)) &CompareSwapCmd::SDidLock,
		(IOReturn (*)(IOFireWireLibCompareSwapCommandV3Ref, UInt32 *)) &CompareSwapCmd::SLocked,
		(IOReturn (*)(IOFireWireLibCompareSwapCommandV3Ref, UInt64 *)) &CompareSwapCmd::SLocked64		
	} ;

	PHYCmd::Interface PHYCmd::sInterface =
	{
		INTERFACEIMP_INTERFACE,
		1, 0, // version/revision
		
		IOFIREWIRELIBCOMMANDIMP_INTERFACE,
		IOFIREWIRELIBCOMMANDIMP_INTERFACE_v2,
		IOFIREWIRELIBCOMMANDIMP_INTERFACE_v3,
		&PHYCmd::S_SetDataQuads
	};

	AsyncStreamCmd::Interface AsyncStreamCmd::sInterface =
	{
		INTERFACEIMP_INTERFACE,
		1, 0, // version/revision
		
		IOFIREWIRELIBCOMMANDIMP_INTERFACE,
		IOFIREWIRELIBCOMMANDIMP_INTERFACE_v2,
		IOFIREWIRELIBCOMMANDIMP_INTERFACE_v3,
		&AsyncStreamCmd::S_SetChannel,
		&AsyncStreamCmd::S_SetSyncBits,
		&AsyncStreamCmd::S_SetTagBits
	};
		
	// ==================================
	// virtual members
	// ==================================
#pragma mark -
	IOFireWireCommandInterface 	Cmd::sInterface = 
	{
		INTERFACEIMP_INTERFACE,
		1, 0, 									// version/revision
		IOFIREWIRELIBCOMMANDIMP_INTERFACE,
		0, 0, 0, 0,
		0, 0, 0, 0, 0, 0
	} ;

	Cmd::Cmd( const IUnknownVTbl & vtable, Device& userClient, io_object_t device, 
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

		mParams->callback		= (mach_vm_address_t)&CommandCompletionHandler;
		mParams->refCon			= (mach_vm_address_t)this;
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
				
				uint32_t outputCnt = 0;
				const uint64_t inputs[1]={(const uint64_t)mParams->kernCommandRef};
				result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(),
												   kReleaseUserObject,
												   inputs,1,
												   NULL,&outputCnt);
				
				DebugLogCond( result, "Cmd::~Cmd: command release returned 0x%08x\n", result) ;
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
		mParams->newTarget = addr.addressLo;
		mParams->newTarget |= (((UInt64)addr.addressHi) << 32);
		mParams->newTarget |= (((UInt64)addr.nodeID) << 48);
	
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
	Cmd::PrepareForVectorSubmit( CommandSubmitParams * submit_params )
	{
		IOReturn status = kIOReturnSuccess;
		
		// can't prep the vector if a command is still inflight
		if( mIsExecuting )
		{
			status = kIOReturnBusy;
		}
		
		// we're not supporting synchronous operations 
		// or inline data when using vectors
		
		if( status == kIOReturnSuccess )
		{
			if( (mParams->flags & kFWCommandInterfaceSyncExecute) || 
				(mParams->flags & kFireWireCommandUseCopy) )
			{
				status = kIOReturnBadArgument;
			}
		}

		// we're only supportting read and write operations on vectors
		
		if( status == kIOReturnSuccess )
		{
			if( (mParams->type != kFireWireCommandType_Read) && 
				(mParams->type != kFireWireCommandType_Write) && 
				(mParams->type != kFireWireCommandType_PHY) )
			{
				status = kIOReturnBadArgument;
			}
		}

		if( status == kIOReturnSuccess )
		{
			// if we don't yet have kernel command for this command,  make it now
			if( !mParams->kernCommandRef )
			{
				// async ref data
				uint64_t async_ref[kOSAsyncRef64Count];
				async_ref[kIOAsyncCalloutFuncIndex] = (uint64_t)0;
				async_ref[kIOAsyncCalloutRefconIndex] = (unsigned long)0;
				
				// input data
				CommandSubmitParams create_params;
				bcopy( mParams, &create_params, sizeof(CommandSubmitParams) );
						
				// swap for Rosetta
		#ifndef __LP64__		
				ROSETTA_ONLY(
					{
						//create_params.kernCommandRef = create_params.kernCommandRef;  // no swap
						create_params.type = (IOFireWireCommandType)OSSwapInt32(create_params.type);
						create_params.callback = (mach_vm_address_t)OSSwapInt64( (UInt64)create_params.callback );
						create_params.refCon = (mach_vm_address_t)OSSwapInt64( (UInt64)create_params.refCon );
						create_params.flags = OSSwapInt32( create_params.flags );
						create_params.staleFlags = OSSwapInt32( create_params.staleFlags );
						create_params.newTarget = OSSwapInt64( create_params.newTarget );
						create_params.newBuffer = (mach_vm_address_t)OSSwapInt64( (UInt64)create_params.newBuffer);
						create_params.newBufferSize = OSSwapInt32( create_params.newBufferSize );
						//create_params.newFailOnReset = create_params.newFailOnReset;
						create_params.newGeneration = OSSwapInt32( create_params.newGeneration );
						create_params.newMaxPacket = OSSwapInt32( create_params.newMaxPacket );

						create_params.timeoutDuration = OSSwapInt32( create_params.timeoutDuration );
						create_params.retryCount = OSSwapInt32( create_params.retryCount );
						create_params.maxPacketSpeed = OSSwapInt32( create_params.maxPacketSpeed );
					}
				);
		#endif			
				
				// output data
				UInt32	kernel_ref = 0;
				size_t outputStructCnt = sizeof(kernel_ref);

				// send it down
				status = IOConnectCallAsyncStructMethod(	mUserClient.GetUserClientConnection(),
															kCommandCreateAsync,
															mUserClient.GetAsyncPort(),
															async_ref, kOSAsyncRef64Count,
															&create_params, sizeof(CommandSubmitParams),
															&kernel_ref, &outputStructCnt );
				if( status == kIOReturnSuccess )
				{
					mParams->kernCommandRef = kernel_ref;
				}
			}
		}
		
		if( status == kIOReturnSuccess )
		{
			// staleFlags will be reset to 0 on each Submit() call,
			// so we set kFireWireCommandStale_MaxPacket before we submit
			if (mParams->newMaxPacket > 0)
				mParams->staleFlags |= kFireWireCommandStale_MaxPacket;

			// copy params into vector
			bcopy( mParams, submit_params, sizeof(CommandSubmitParams) );
					
			// swap for Rosetta
	#ifndef __LP64__		
			ROSETTA_ONLY(
				{
					//submit_params->kernCommandRef = submit_params->kernCommandRef;  // no swap
					submit_params->type = (IOFireWireCommandType)OSSwapInt32(submit_params->type);
					submit_params->callback = (mach_vm_address_t)OSSwapInt64( (UInt64)submit_params->callback );
					submit_params->refCon = (mach_vm_address_t)OSSwapInt64( (UInt64)submit_params->refCon );
					submit_params->flags = OSSwapInt32( submit_params->flags );
					submit_params->staleFlags = OSSwapInt32( submit_params->staleFlags );
					submit_params->newTarget = OSSwapInt64( submit_params->newTarget );
					submit_params->newBuffer = (mach_vm_address_t)OSSwapInt64( (UInt64)submit_params->newBuffer);
					submit_params->newBufferSize = OSSwapInt32( submit_params->newBufferSize );
					//submit_params->newFailOnReset = submit_params->newFailOnReset;
					submit_params->newGeneration = OSSwapInt32( submit_params->newGeneration );
					submit_params->newMaxPacket = OSSwapInt32( submit_params->newMaxPacket );

					submit_params->timeoutDuration = OSSwapInt32( submit_params->timeoutDuration );
					submit_params->retryCount = OSSwapInt32( submit_params->retryCount );
					submit_params->maxPacketSpeed = OSSwapInt32( submit_params->maxPacketSpeed );
				}
			);
	#endif
		
		}
	
		//	printf( "Cmd::PrepareForVectorSubmit - status = 0x%08lx\n", status );
	
		return status;
	}

	void
	Cmd::VectorIsExecuting( void )
	{
		mIsExecuting = true ;
		mParams->staleFlags = 0;		
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

		CommandSubmitParams	* submit_params = params;
		
		IOReturn 			err = 0;
		vm_address_t vm_address = 0;
#ifndef __LP64__		
		ROSETTA_ONLY(
			{
				err = vm_allocate( mach_task_self(), &vm_address, paramsSize, true /*anywhere*/ );
				if( vm_address == 0 )
				{
					err = kIOReturnNoMemory;
				}
			
				if( !err )
				{
					submit_params = (CommandSubmitParams*)vm_address;
					bcopy( params, submit_params, paramsSize );
					
					//submit_params->kernCommandRef = submit_params->kernCommandRef;  // no swap
					submit_params->type = (IOFireWireCommandType)OSSwapInt32(submit_params->type);
					submit_params->callback = (mach_vm_address_t)OSSwapInt64( (UInt64)submit_params->callback );
					submit_params->refCon = (mach_vm_address_t)OSSwapInt64( (UInt64)submit_params->refCon );
					submit_params->flags = OSSwapInt32( submit_params->flags );
					submit_params->staleFlags = OSSwapInt32( submit_params->staleFlags );
					submit_params->newTarget = OSSwapInt64( submit_params->newTarget );
					submit_params->newBuffer = (mach_vm_address_t)OSSwapInt64( (UInt64)submit_params->newBuffer);
					submit_params->newBufferSize = OSSwapInt32( submit_params->newBufferSize );
					//submit_params->newFailOnReset = submit_params->newFailOnReset;
					submit_params->newGeneration = OSSwapInt32( submit_params->newGeneration );
					submit_params->newMaxPacket = OSSwapInt32( submit_params->newMaxPacket );

					submit_params->timeoutDuration = OSSwapInt32( submit_params->timeoutDuration );
					submit_params->retryCount = OSSwapInt32( submit_params->retryCount );
					submit_params->maxPacketSpeed = OSSwapInt32( submit_params->maxPacketSpeed );

				}
			}
		);
#endif
	
		if( !err )
		{
			uint64_t refrncData[kOSAsyncRef64Count];
			refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) 0;
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long) 0;
			size_t outputStructCnt = *submitResultSize;

			err = IOConnectCallAsyncStructMethod(mUserClient.GetUserClientConnection(),
												 kCommand_Submit,
												 mUserClient.GetAsyncPort(),
												 refrncData,kOSAsyncRef64Count,
												 submit_params,paramsSize,
												 submitResult,&outputStructCnt);
			*submitResultSize = outputStructCnt;
		}
		

		if ( !err )
		{
#ifndef __LP64__		
			ROSETTA_ONLY(
				{
			//		submitResult->kernCommandRef = submitResult->kernCommandRef;
					submitResult->result = OSSwapInt32( submitResult->result );
					submitResult->bytesTransferred = OSSwapInt32( submitResult->bytesTransferred );
					submitResult->ackCode = OSSwapInt32( submitResult->ackCode );
					submitResult->responseCode = OSSwapInt32( submitResult->responseCode );
				}
			);
#endif
			if (mParams->flags & kFWCommandInterfaceSyncExecute)
			{
				mStatus = submitResult->result ;
				mBytesTransferred = submitResult->bytesTransferred ;
				mAckCode = submitResult->ackCode;
				mResponseCode = submitResult->responseCode;
				
				// the kernel has to pass success to get submit results, set err to the proper err here
				err = mStatus;
			}
			else	
				mIsExecuting = true ;
	
			mParams->staleFlags = 0 ;
			if (!mParams->kernCommandRef)
				mParams->kernCommandRef = submitResult->kernCommandRef ;
		}

		if( vm_address )
		{
			vm_deallocate( mach_task_self(), vm_address, paramsSize );
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
		
		uint32_t outputCnt = 0;
		return IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(),
										 mUserClient.MakeSelectorWithObject( kCommand_Cancel_d, mParams->kernCommandRef ),
										 NULL,0,
										 NULL,&outputCnt);
	}
	
	void
	Cmd::SetBuffer(
		UInt32				inSize,
		void*				inBuffer)
	{
		mParams->newBufferSize = inSize ;
		mParams->newBuffer = (mach_vm_address_t)inBuffer ;
		mParams->staleFlags |= kFireWireCommandStale_Buffer ;
	}
	
	void
	Cmd::GetBuffer(
		UInt32*				outSize,
		void**				outBuf)
	{
		*outSize = mParams->newBufferSize ;
		*outBuf	= (void*)mParams->newBuffer ;
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
		{
#ifdef __LP64__
			DebugLog("Invalid flags %x passed to SetFlags!\n", inFlags) ;
#else
			DebugLog("Invalid flags %lx passed to SetFlags!\n", inFlags) ;
#endif
		}
		
		mParams->flags &= ~kFireWireCommandUserFlagsMask ;
		mParams->flags |= (inFlags & kFireWireCommandUserFlagsMask) ;
	
		if (mParams->flags & kFWCommandInterfaceForceCopyAlways)
			mParams->flags |= kFireWireCommandUseCopy ;
		if (mParams->flags & kFWCommandInterfaceForceNoCopy)
			mParams->flags &= ~kFireWireCommandUseCopy ;
		if (mParams->flags & kFWCommandInterfaceAbsolute )
			mParams->flags |= kFireWireCommandAbsolute ;
	}
		
	void Cmd::SetTimeoutDuration( 
		UInt32 duration )
	{
		mParams->timeoutDuration = duration;
		mParams->staleFlags |= kFireWireCommandStale_Timeout;
	}
	
	void 
	Cmd::SetMaxRetryCount( 
		UInt32 count )
	{
		mParams->retryCount = count;
		mParams->staleFlags |= kFireWireCommandStale_Retries;
	}
	
	UInt32
	Cmd::GetAckCode()
	{
		return mAckCode;
	}
	
	UInt32
	Cmd::GetResponseCode()
	{
		return mResponseCode;
	}
	
	void
	Cmd::SetMaxPacketSpeed( IOFWSpeed speed )
	{
		mParams->maxPacketSpeed = speed;
		mParams->staleFlags |= kFireWireCommandStale_Speed;
	}
	
	void
	Cmd::CommandCompletionHandler(
		void*				refcon,
		IOReturn			result,
		void*				quads[],
		UInt32				numQuads )
	{
		Cmd*	me = (Cmd*)refcon ;
	
		me->mStatus 			= result ;
		me->mBytesTransferred 	= (unsigned long)quads[0] ;
		me->mAckCode			= (unsigned long)quads[1];
		me->mResponseCode		= (unsigned long)quads[2];
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

	void *
	Cmd::SGetRefCon(
		IOFireWireLibCommandRef	self )
	{
		return IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->mRefCon;
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

	void
	Cmd::SSetTimeoutDuration( 
		IOFireWireLibCommandRef self, 
		UInt32 duration )
	{
		IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->SetTimeoutDuration( duration );
	}
	
	void 
	Cmd::SSetMaxRetryCount( 
		IOFireWireLibCommandRef self, 
		UInt32 count )
	{
		IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->SetMaxRetryCount( count );
	}
		
	UInt32
	Cmd::SGetAckCode( IOFireWireLibCommandRef self )
	{
		return IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->GetAckCode();
	}
	
	UInt32
	Cmd::SGetResponseCode( IOFireWireLibCommandRef self )
	{
		return IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->GetResponseCode();
	}
	
	void
	Cmd::SSetMaxPacketSpeed( IOFireWireLibCommandRef self, IOFWSpeed speed )
	{
		IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->SetMaxPacketSpeed( speed );
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
	: Cmd( reinterpret_cast<const IUnknownVTbl &>( sInterface ), userclient, device, addr, callback, 
				failOnReset, generation, refcon, 
				reinterpret_cast<CommandSubmitParams*>(new UInt8[sizeof(CommandSubmitParams)]) )
	{
		mParams->type			 = kFireWireCommandType_Read ;
		mParams->newBuffer		 = (mach_vm_address_t)buf ;
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
			|| CFEqual( interfaceID, kIOFireWireReadCommandInterfaceID_v2)
			|| CFEqual( interfaceID, kIOFireWireReadCommandInterfaceID_v3) )
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
			bcopy(submitResult + 1, (void*)mParams->newBuffer, mBytesTransferred) ;
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
	: Cmd( reinterpret_cast<const IUnknownVTbl &>( sInterface ), userclient, device, addr, callback, 
				failOnReset, generation, refcon, 
				reinterpret_cast<CommandSubmitParams*>(new UInt8[sizeof(CommandSubmitParams)]) )
	{		
		mParams->callback		= (mach_vm_address_t)&CommandCompletionHandler;
		mParams->type			= kFireWireCommandType_ReadQuadlet ;
		mParams->newBuffer		= (mach_vm_address_t)quads ;
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
		mParams->newBufferSize = inNumQuads << 2; // * 4
		mParams->newBuffer = (mach_vm_address_t) inQuads ;
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
				
			bcopy(submitResult + 1, (void*)mParams->newBuffer, mBytesTransferred) ;
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
						// sendAsyncResult64 always send args as a pointer
						// instead of inline...
	
		ReadQuadCmd*	me = (ReadQuadCmd*)refcon ;
	
		me->mStatus 			= result ;
		me->mBytesTransferred 	= (numQuads *4) + 2;
		me->mAckCode			= (unsigned long)quads[2];
		me->mResponseCode		= (unsigned long)quads[1];
		me->mIsExecuting 		= false;
		
		//zzz this copy is going to have to change for 64 bit		
		bcopy( quads + 2, (void*)me->mParams->newBuffer, me->mBytesTransferred );

#ifndef __LP64__		
		ROSETTA_ONLY(
			{
				// async results get auto swapped on 32 bit boundaries
				// unswap what shouldn't have been swapped
				
				UInt32 * buffer_quads = (UInt32*)me->mParams->newBuffer;

				for( unsigned i = 0; i < numQuads; i++ )
				{
					buffer_quads[i] = OSSwapInt32( buffer_quads[i] );
				}
			}
		);
#endif
		
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
	: Cmd( reinterpret_cast<const IUnknownVTbl &>( sInterface), userclient, device, addr, callback, 
				failOnReset, generation, refcon, 
				reinterpret_cast<CommandSubmitParams*>(new UInt8[sizeof(CommandSubmitParams)]) )
	{
		mParams->type			= kFireWireCommandType_Write ;
		mParams->newBuffer		= (mach_vm_address_t)buf ;
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
			|| CFEqual(interfaceID, kIOFireWireCommandInterfaceID) 
			|| CFEqual(interfaceID, kIOFireWireWriteCommandInterfaceID)
			|| CFEqual(interfaceID, kIOFireWireWriteCommandInterfaceID_v2)
			|| CFEqual(interfaceID, kIOFireWireWriteCommandInterfaceID_v3) )
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
	// PHYCmd methods
	//
	// ============================================================
#pragma mark -
	PHYCmd::PHYCmd( Device& userclient, UInt32 data1, UInt32 data2, 
								CommandCallback callback, bool failOnReset, UInt32 generation, void* refcon )
	: Cmd( reinterpret_cast<const IUnknownVTbl &>( sInterface), userclient, NULL, FWAddress(), callback, 
				failOnReset, generation, refcon, 
				reinterpret_cast<CommandSubmitParams*>(new UInt8[sizeof(CommandSubmitParams)]) )
	{
		mParams->type			= kFireWireCommandType_PHY ;
		mParams->newBuffer		= 0;
		mParams->newBufferSize	= 0;
	//	mParams->staleFlags |= kFireWireCommandStale_Buffer ;
		mParams->data1			= data1;
		mParams->data2			= data2;
	}

	IUnknownVTbl**
	PHYCmd::Alloc( Device& userclient, UInt32 data1, UInt32 data2, 
							CommandCallback callback, bool failOnReset, UInt32 generation, void* refcon)
	{
		PHYCmd*	me = new PHYCmd( userclient, data1, data2, callback, failOnReset, generation, refcon );

		if (!me)
			return nil;
		
		return reinterpret_cast<IUnknownVTbl**>(& me->GetInterface());
	}
	
	HRESULT
	PHYCmd::QueryInterface( REFIID iid, LPVOID* ppv )
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes( kCFAllocatorDefault, iid );
	
		if ( CFEqual(interfaceID, IUnknownUUID) 
			|| CFEqual(interfaceID, kIOFireWireCommandInterfaceID) 
			|| CFEqual(interfaceID, kIOFireWirePHYCommandInterfaceID) 
			)
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
	PHYCmd::Submit()
	{
		if (mIsExecuting)
			return kIOReturnBusy ;
	
		CommandSubmitResult			submitResult ;
		mach_msg_type_number_t		submitResultSize = sizeof(submitResult) ;
		UInt32						paramsSize ;
	
		paramsSize = sizeof(*mParams) ;
	
		return Cmd::Submit(mParams, paramsSize, & submitResult, & submitResultSize) ;
	}

	void PHYCmd::S_SetDataQuads(	IOFireWireLibPHYCommandRef	self,
									UInt32						data1, 
									UInt32						data2 )
	{
		PHYCmd * phy_cmd = IOFireWireIUnknown::InterfaceMap<PHYCmd>::GetThis(self);
		
		phy_cmd->mParams->data1 = data1;
		phy_cmd->mParams->data2 = data2;	
	}
		
	// ============================================================
	//
	// WriteQuadCmd methods
	//
	// ============================================================
#pragma mark -
	WriteQuadCmd::WriteQuadCmd( Device& userclient, io_object_t device, const FWAddress& addr, UInt32 quads[], UInt32 numQuads,
										CommandCallback callback, bool failOnReset, UInt32 generation, void* refcon )
	: Cmd( reinterpret_cast<const IUnknownVTbl &>( sInterface ), userclient, device, addr, callback, failOnReset, generation, refcon, 
				reinterpret_cast<CommandSubmitParams*>(new UInt8[sizeof(CommandSubmitParams) + numQuads << 2]) ),
	mParamsExtra( reinterpret_cast<UInt8*>(mParams) )
	{
		mParams->type			= kFireWireCommandType_WriteQuadlet ;
		mParams->newBuffer 		= (mach_vm_address_t)(mParams+1);//(void*) quads ;
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
	
			DebugLogCond( !newParamsExtra, "warning: WriteQuadCmd::SetQuads: out of memory!\n" ) ;
	
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
	: Cmd( reinterpret_cast<const IUnknownVTbl &>( sInterface ), userclient, device, addr, callback, 
			failOnReset, generation, refcon, 
			reinterpret_cast<CommandSubmitParams*>( new UInt8[sizeof(CommandSubmitParams) + sizeof(UInt64) * 2] ) ),	// 8 bytes/UInt16 * 2 values/cmd
	mParamsExtra( reinterpret_cast<UInt8*>(mParams) ),
	mInterface_v3( reinterpret_cast<const IUnknownVTbl &>( sInterface_v3 ), this )
	{
		mParams->callback		= (mach_vm_address_t)&CommandCompletionHandler;
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
		else if ( CFEqual(interfaceID, kIOFireWireCompareSwapCommandInterfaceID_v3) )
		{
			*ppv = &mInterface_v3;
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
		((UInt32*)(mParams+1))[2] 	= newVal ;
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
			if( mParams->flags & kFWCommandInterfaceSyncExecute )
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
		
		*oldValue = mSubmitResult.lockInfo.value[0];
		return kIOReturnSuccess ;
	}
	
	IOReturn
	CompareSwapCmd::Locked64(
		UInt64* 			oldValue)
	{
		if (mIsExecuting)
			return kIOReturnBusy ;
		if (!mParams->kernCommandRef)
			return kIOReturnError ;
		if (mParams->newBufferSize != sizeof(UInt64))
			return kIOReturnBadArgument ;

		*oldValue = *(UInt64*)mSubmitResult.lockInfo.value;
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
		return GetThis(self)->Locked64(oldValue) ;
	}

	void
	CompareSwapCmd::SSetFlags( CmdRef self, UInt32 inFlags )
	{
		IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(self)->SetFlags(inFlags) ;
	}

	void
	CompareSwapCmd::CommandCompletionHandler(
		void*					refcon,
		IOReturn				result,
		io_user_reference_t		quads[],
		UInt32					numQuads)
	{
		CompareSwapCmd * me = reinterpret_cast<CompareSwapCmd*>(refcon) ;
	
	#if 0	
		int i = 0;
		for( i = 0; i < 7; i++ )
		{
		#ifdef __LP64__			
			printf( "CompareSwapCmd::CommandCompletionHandler - quads[%d] - %llx\n", i, quads[i] );
		#else
			printf( "CompareSwapCmd::CommandCompletionHandler - quads[%d] - %x\n", i, quads[i] );
		#endif
		}
	#endif
		
		IF_ROSETTA()
		{
			#ifndef __LP64__
				me->mSubmitResult.result = (UserObjectHandle)OSSwapInt32((UInt32)quads[0]);
				me->mSubmitResult.bytesTransferred = (IOByteCount)OSSwapInt32((UInt32)quads[1]);
				me->mSubmitResult.ackCode = (UInt32)OSSwapInt32((UInt32)quads[2]);
				me->mSubmitResult.responseCode = (UserObjectHandle)OSSwapInt32((UInt32)quads[3]);
				me->mSubmitResult.lockInfo.didLock = (UserObjectHandle)OSSwapInt32((UInt32)quads[4]);
				me->mSubmitResult.lockInfo.value[0] = OSSwapInt32((UInt32)quads[5]);
				me->mSubmitResult.lockInfo.value[1] = OSSwapInt32((UInt32)quads[6]);
			#endif
		}
		else
		{
			me->mSubmitResult.result = (UserObjectHandle)quads[0];
			me->mSubmitResult.bytesTransferred = (IOByteCount)quads[1];
			me->mSubmitResult.ackCode = (UInt32)quads[2];
			me->mSubmitResult.responseCode = (UserObjectHandle)quads[3];
			me->mSubmitResult.lockInfo.didLock = (UserObjectHandle)quads[4];
			me->mSubmitResult.lockInfo.value[0] = (UInt32)quads[5];
			me->mSubmitResult.lockInfo.value[1] = (UInt32)quads[6];
		}
		
#if 0				
			bcopy((CompareSwapSubmitResult*)quads, & me->mSubmitResult, sizeof(me->mSubmitResult)) ;

#ifndef __LP64__		
		ROSETTA_ONLY(
			{
				// async results get auto swapped on 32 bit boundaries
				// unswap what shouldn't have been swapped
				
				// no one uses kernCommandRef currently
				me->mSubmitResult.kernCommandRef = (UserObjectHandle)OSSwapInt32( (UInt32)me->mSubmitResult.kernCommandRef );
				me->mSubmitResult.lockInfo.didLock = OSSwapInt32( me->mSubmitResult.lockInfo.didLock );
				
				UInt32 * value_quads = (UInt32*)&me->mSubmitResult.lockInfo.value;
				value_quads[0] = OSSwapInt32( value_quads[0] );
				value_quads[1] = OSSwapInt32( value_quads[1] );
			}
		);
#endif		
#endif
		
		me->mStatus 			= result ;
		me->mBytesTransferred	= me->mSubmitResult.bytesTransferred ;
		me->mAckCode			= me->mSubmitResult.ackCode;
		me->mResponseCode		= me->mSubmitResult.responseCode;
		me->mIsExecuting 		= false ;		
	
		if (me->mCallback)
			(*(me->mCallback))(me->mRefCon, me->mStatus) ;
		
	}
	
	// ============================================================
	//
	// AsyncStreamCmd methods
	//
	// ============================================================
#pragma mark -
	AsyncStreamCmd::AsyncStreamCmd(Device& 	userclient, UInt32 channel, UInt32 sync, UInt32 tag, void*	buf, UInt32	size, CommandCallback callback, Boolean	failOnReset, UInt32 generation, void*	inRefCon )
				: Cmd( reinterpret_cast<const IUnknownVTbl &>( sInterface), userclient, NULL, FWAddress(), callback, failOnReset, generation, inRefCon, 
				reinterpret_cast<CommandSubmitParams*>(new UInt8[sizeof(CommandSubmitParams)]) )				
	{
		mParams->type			= kFireWireCommandType_AsyncStream ;
		mParams->newBuffer		= (mach_vm_address_t)buf ;
		mParams->newBufferSize	= size ;
		mParams->staleFlags		|= kFireWireCommandStale_Buffer ;
		mParams->data1			= channel;
		mParams->tag			= tag;
		mParams->sync			= sync;
		mParams->newFailOnReset	= failOnReset;
	}

	IUnknownVTbl**
	AsyncStreamCmd::Alloc( Device& 	userclient, UInt32 channel, UInt32 sync, UInt32 tag, void*	buf, UInt32	size, CommandCallback callback, Boolean	failOnReset, UInt32 generation, void*	inRefCon)
	{
		AsyncStreamCmd*	me = new AsyncStreamCmd( userclient, channel, sync, tag, buf, size, callback, failOnReset, generation, inRefCon ) ;

		if (!me)
			return nil ;
		
		return reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	HRESULT
	AsyncStreamCmd::QueryInterface(REFIID iid, LPVOID* ppv)
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if ( CFEqual(interfaceID, IUnknownUUID) 
			|| CFEqual(interfaceID, kIOFireWireCommandInterfaceID) 
			|| CFEqual(interfaceID, kIOFireWireAsyncStreamCommandInterfaceID) )
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
	AsyncStreamCmd::Submit()
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
	
	void
	AsyncStreamCmd::S_SetChannel( IOFireWireLibAsyncStreamCommandRef self, UInt32 channel )
	{
		AsyncStreamCmd * asyncStream_cmd = IOFireWireIUnknown::InterfaceMap<AsyncStreamCmd>::GetThis(self);
		
		asyncStream_cmd->mParams->data1 = channel;
	}

	void
	AsyncStreamCmd::S_SetSyncBits( IOFireWireLibAsyncStreamCommandRef self, UInt16 sync )
	{
		AsyncStreamCmd * asyncStream_cmd = IOFireWireIUnknown::InterfaceMap<AsyncStreamCmd>::GetThis(self);
		
		asyncStream_cmd->mParams->sync = sync;
	}

	void
	AsyncStreamCmd::S_SetTagBits( IOFireWireLibAsyncStreamCommandRef self, UInt16 tag )
	{
		AsyncStreamCmd * asyncStream_cmd = IOFireWireIUnknown::InterfaceMap<AsyncStreamCmd>::GetThis(self);
		
		asyncStream_cmd->mParams->tag = tag;
	}
	
}	// namespace IOFireWireLib

