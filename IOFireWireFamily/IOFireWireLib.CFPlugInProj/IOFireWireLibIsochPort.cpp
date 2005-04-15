/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 *  IOFireWireLibIsochPort.cpp
 *  IOFireWireFamily
 *
 *  Created on Mon Mar 12 2001.
 *  Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
 *
 * $Log: IOFireWireLibIsochPort.cpp,v $
 * Revision 1.33.10.1  2005/03/08 03:48:49  collin
 * *** empty log message ***
 *
 * Revision 1.33  2004/02/17 23:13:23  niels
 * *** empty log message ***
 *
 * Revision 1.32  2004/02/17 23:12:27  niels
 * *** empty log message ***
 *
 * Revision 1.31  2004/02/17 20:23:58  niels
 * keep track if local isoch port has started or not to avoid leaking when stop is called on an unstarted port
 *
 * Revision 1.30  2003/12/19 22:07:46  niels
 * send force stop when channel dies/system sleeps
 *
 * Revision 1.29  2003/08/25 08:39:17  niels
 * *** empty log message ***
 *
 * Revision 1.28  2003/08/20 23:33:37  niels
 * *** empty log message ***
 *
 * Revision 1.27  2003/08/20 18:48:45  niels
 * *** empty log message ***
 *
 * Revision 1.26  2003/08/18 23:18:15  niels
 * *** empty log message ***
 *
 * Revision 1.25  2003/08/14 17:47:33  niels
 * *** empty log message ***
 *
 * Revision 1.24  2003/07/29 22:49:25  niels
 * *** empty log message ***
 *
 * Revision 1.23  2003/07/24 20:49:50  collin
 * *** empty log message ***
 *
 * Revision 1.22  2003/07/24 06:30:59  collin
 * *** empty log message ***
 *
 * Revision 1.21  2003/07/21 06:53:10  niels
 * merge isoch to TOT
 *
 * Revision 1.20.6.6  2003/07/21 06:44:48  niels
 * *** empty log message ***
 *
 * Revision 1.20.6.5  2003/07/18 00:17:47  niels
 * *** empty log message ***
 *
 * Revision 1.20.6.4  2003/07/10 00:11:58  niels
 * *** empty log message ***
 *
 * Revision 1.20.6.3  2003/07/09 21:24:07  niels
 * *** empty log message ***
 *
 * Revision 1.20.6.2  2003/07/03 22:10:26  niels
 * fix iidc/dv rcv
 *
 * Revision 1.20.6.1  2003/07/01 20:54:24  niels
 * isoch merge
 *
 */

#import "IOFireWireLibIsochPort.h"
#import "IOFireWireLibDevice.h"
#import "IOFireWireLibNuDCLPool.h"
#import "IOFireWireLibNuDCL.h"
#import "IOFireWireLibCoalesceTree.h"

#import <IOKit/iokitmig.h>
#import <mach/mach.h>

#define IOFIREWIREISOCHPORTIMP_INTERFACE	\
	& IsochPortCOM::SGetSupported,	\
	& IsochPortCOM::SAllocatePort,	\
	& IsochPortCOM::SReleasePort,	\
	& IsochPortCOM::SStart,	\
	& IsochPortCOM::SStop,	\
	& IsochPortCOM::SSetRefCon,	\
	& IsochPortCOM::SGetRefCon

namespace IOFireWireLib {
	RemoteIsochPort::Interface	RemoteIsochPortCOM::sInterface =
	{
		INTERFACEIMP_INTERFACE,
		1, 0,
		
		IOFIREWIREISOCHPORTIMP_INTERFACE,
		& RemoteIsochPortCOM::SSetGetSupportedHandler,
		& RemoteIsochPortCOM::SSetAllocatePortHandler,
		& RemoteIsochPortCOM::SSetReleasePortHandler,
		& RemoteIsochPortCOM::SSetStartHandler,
		& RemoteIsochPortCOM::SSetStopHandler,
	} ;
	
	// ============================================================
	// utility functions
	// ============================================================
	
	Boolean
	GetDCLDataBuffer(
		DCLCommand*	dcl,
		IOVirtualAddress*	outDataBuffer,
		IOByteCount*		outDataLength)
	{
		Boolean	result = false ;
	
		switch(dcl->opcode & ~kFWDCLOpFlagMask)
		{
			case kDCLSendPacketStartOp:
			case kDCLSendPacketWithHeaderStartOp:
			case kDCLSendPacketOp:
			case kDCLReceivePacketStartOp:
			case kDCLReceivePacketOp:
				*outDataBuffer		= (IOVirtualAddress)((DCLTransferPacket*)dcl)->buffer ;
				*outDataLength		= ((DCLTransferPacket*)dcl)->size ;
				result = true ;
				break ;
				
			case kDCLSendBufferOp:
			case kDCLReceiveBufferOp:
				//zzz what should I do here?
				break ;
	
			case kDCLPtrTimeStampOp:
				*outDataBuffer		= (IOVirtualAddress)((DCLPtrTimeStamp*)dcl)->timeStampPtr ;
				*outDataLength		= sizeof( *( ((DCLPtrTimeStamp*)dcl)->timeStampPtr) ) ;
				result = true ;
				break ;
			
			default:
				break ;
		}
		
		return result ;
	}
	
	IOByteCount
	GetDCLSize(
		DCLCommand*	dcl)
	{
		IOByteCount result = 0 ;
	
		switch(dcl->opcode & ~kFWDCLOpFlagMask)
		{
			case kDCLSendPacketStartOp:
			case kDCLSendPacketWithHeaderStartOp:
			case kDCLSendPacketOp:
			case kDCLReceivePacketStartOp:
			case kDCLReceivePacketOp:
				result = sizeof(DCLTransferPacket) ;
				break ;
				
			case kDCLSendBufferOp:
			case kDCLReceiveBufferOp:
				result = sizeof(DCLTransferBuffer) ;
				break ;
	
			case kDCLCallProcOp:
				result = sizeof(DCLCallProc) ;
				break ;
				
			case kDCLLabelOp:
				result = sizeof(DCLLabel) ;
				break ;
				
			case kDCLJumpOp:
				result = sizeof(DCLJump) ;
				break ;
				
			case kDCLSetTagSyncBitsOp:
				result = sizeof(DCLSetTagSyncBits) ;
				break ;
				
			case kDCLUpdateDCLListOp:
				result = sizeof(DCLUpdateDCLList) ;
				break ;
	
			case kDCLPtrTimeStampOp:
				result = sizeof(DCLPtrTimeStamp) ;

			case kDCLSkipCycleOp:
				result = sizeof(DCLCommand) ;
		}
		
		return result ;
	}
	
#pragma mark -
	// ============================================================
	//
	// IsochPort
	//
	// ============================================================
	
	IsochPort::IsochPort( const IUnknownVTbl & interface, Device & device, bool talking, bool allocateKernPort )
	: IOFireWireIUnknown( interface ),
	  mDevice( device ),
	  mKernPortRef( 0 ),
	  mTalking( talking )
	{
		mDevice.AddRef() ;
	}
	
	IsochPort::~IsochPort()
	{
		if ( mKernPortRef )
		{
			IOReturn error = kIOReturnSuccess;
			
			error = IOConnectMethodScalarIScalarO( mDevice.GetUserClientConnection(), 
												   kReleaseUserObject, 1, 0, mKernPortRef ) ;

			DebugLogCond( error, "Couldn't release kernel port" ) ;
		}
		
		mDevice.Release() ;
	}
	
	IOReturn
	IsochPort::GetSupported(
		IOFWSpeed&					maxSpeed, 
		UInt64& 					chanSupported )
	{
		return ::IOConnectMethodScalarIScalarO( mDevice.GetUserClientConnection(), kIsochPort_GetSupported,
						1, 3, mKernPortRef, & maxSpeed, (UInt32*)&chanSupported, (UInt32*)&chanSupported + 1) ;
	}
	
	IOReturn
	IsochPort::AllocatePort( IOFWSpeed speed, UInt32 chan )
	{
		return ::IOConnectMethodScalarIScalarO( mDevice.GetUserClientConnection(), 
												mDevice.MakeSelectorWithObject( kIsochPort_AllocatePort_d, mKernPortRef ),
												2, 0, speed, chan ) ;
	}
	
	IOReturn
	IsochPort::ReleasePort()
	{
		return ::IOConnectMethodScalarIScalarO( mDevice.GetUserClientConnection(),
												mDevice.MakeSelectorWithObject( kIsochPort_ReleasePort_d, mKernPortRef ), 
												0, 0 ) ;
	}
	
	IOReturn
	IsochPort::Start()
	{
		return ::IOConnectMethodScalarIScalarO( mDevice.GetUserClientConnection(),
												mDevice.MakeSelectorWithObject( kIsochPort_Start_d, mKernPortRef ),
												0, 0 ) ;
	}
	
	IOReturn
	IsochPort::Stop()
	{
		return ::IOConnectMethodScalarIScalarO( mDevice.GetUserClientConnection(), 
												mDevice.MakeSelectorWithObject( kIsochPort_Stop_d, mKernPortRef ), 
												0, 0 ) ;
	}
	
#pragma mark -
	// ============================================================
	//
	// IsochPortCOM
	//
	// ============================================================
	
	IsochPortCOM::IsochPortCOM( const IUnknownVTbl & interface, Device& userclient, bool talking, bool allocateKernPort )
	:	IsochPort( interface, userclient, talking, allocateKernPort )
	{
	}
	
	IsochPortCOM::~IsochPortCOM()
	{
	}
	
	IOReturn
	IsochPortCOM::SGetSupported(
		IOFireWireLibIsochPortRef	self, 
		IOFWSpeed* 					maxSpeed, 
		UInt64* 					chanSupported )
	{
		return IOFireWireIUnknown::InterfaceMap<IsochPortCOM>::GetThis(self)->GetSupported(*maxSpeed, *chanSupported) ;
	}
	
	IOReturn
	IsochPortCOM::SAllocatePort(
		IOFireWireLibIsochPortRef	self, 
		IOFWSpeed 					speed, 
		UInt32 						chan )
	{
		return IOFireWireIUnknown::InterfaceMap<IsochPortCOM>::GetThis(self)->AllocatePort(speed, chan) ;
	}
	
	IOReturn
	IsochPortCOM::SReleasePort(
		IOFireWireLibIsochPortRef	self)
	{
		return IOFireWireIUnknown::InterfaceMap<IsochPortCOM>::GetThis(self)->ReleasePort() ;
	}
	
	IOReturn
	IsochPortCOM::SStart(
		IOFireWireLibIsochPortRef	self)
	{
		return IOFireWireIUnknown::InterfaceMap<IsochPortCOM>::GetThis(self)->Start() ;
	}
	
	IOReturn
	IsochPortCOM::SStop(
		IOFireWireLibIsochPortRef	self)
	{
		return IOFireWireIUnknown::InterfaceMap<IsochPortCOM>::GetThis(self)->Stop() ;
	}
	
	void
	IsochPortCOM::SSetRefCon(
		IOFireWireLibIsochPortRef		self,
		void*				inRefCon)
	{
		IOFireWireIUnknown::InterfaceMap<IsochPortCOM>::GetThis(self)->SetRefCon(inRefCon) ;
	}
	
	void*
	IsochPortCOM::SGetRefCon(
		IOFireWireLibIsochPortRef		self)
	{
		return IOFireWireIUnknown::InterfaceMap<IsochPortCOM>::GetThis(self)->GetRefCon() ;
	}
	
	Boolean
	IsochPortCOM::SGetTalking(
		IOFireWireLibIsochPortRef		self)
	{
		return IOFireWireIUnknown::InterfaceMap<IsochPortCOM>::GetThis(self)->GetTalking() ;
	}

#pragma mark -
	// ============================================================
	//
	// RemoteIsochPort
	//
	// ============================================================
	
	RemoteIsochPort::RemoteIsochPort( const IUnknownVTbl & interface, Device& userclient, bool talking )
	:	IsochPortCOM( interface, userclient, talking ),
		mGetSupportedHandler(0),
		mAllocatePortHandler(0),
		mReleasePortHandler(0),
		mStartHandler(0),
		mStopHandler(0),
		mRefInterface( reinterpret_cast<IOFireWireIsochPortInterface**>(& GetInterface()) )
	{
	}
	
	IOReturn
	RemoteIsochPort::GetSupported(
		IOFWSpeed&					maxSpeed,
		UInt64&						chanSupported)
	{
		if (mGetSupportedHandler)
			return (*mGetSupportedHandler)(mRefInterface, & maxSpeed, & chanSupported) ;
		else
			return kIOReturnUnsupported ;	// should we return unsupported if user proc doesn't answer?
	}
	
	IOReturn
	RemoteIsochPort::AllocatePort(
		IOFWSpeed 					speed, 
		UInt32 						chan )
	{
		if (mAllocatePortHandler)
			return (*mAllocatePortHandler)(mRefInterface, speed,  chan) ;
		else
			return kIOReturnSuccess ;
	}
	
	IOReturn
	RemoteIsochPort::ReleasePort()
	{
		if (mReleasePortHandler)
			return (*mReleasePortHandler)(mRefInterface) ;
		else
			return kIOReturnSuccess ;
	}
	
	IOReturn
	RemoteIsochPort::Start()
	{
		if (mStartHandler)
			return (*mStartHandler)(mRefInterface) ;
		else
			return kIOReturnSuccess ;
	}
	
	IOReturn
	RemoteIsochPort::Stop()
	{
		if (mStopHandler)
			return (*mStopHandler)(mRefInterface) ;
		else
			return kIOReturnSuccess ;
	}
	
	
	IOFireWireLibIsochPortGetSupportedCallback
	RemoteIsochPort::SetGetSupportedHandler(
		IOFireWireLibIsochPortGetSupportedCallback	inHandler)
	{
		IOFireWireLibIsochPortGetSupportedCallback oldHandler = mGetSupportedHandler ;
		mGetSupportedHandler = inHandler ;
		
		return oldHandler ;
	}
	
	IOFireWireLibIsochPortAllocateCallback
	RemoteIsochPort::SetAllocatePortHandler(
		IOFireWireLibIsochPortAllocateCallback	inHandler)
	{
		IOFireWireLibIsochPortAllocateCallback	oldHandler	= mAllocatePortHandler ;
		mAllocatePortHandler = inHandler ;
		
		return oldHandler ;
	}
	
	IOFireWireLibIsochPortCallback
	RemoteIsochPort::SetReleasePortHandler(
		IOFireWireLibIsochPortCallback	inHandler)
	{
		IOFireWireLibIsochPortCallback	oldHandler	= mReleasePortHandler ;
		mReleasePortHandler = inHandler ;
		
		return oldHandler ;
	}
	
	IOFireWireLibIsochPortCallback
	RemoteIsochPort::SetStartHandler(
		IOFireWireLibIsochPortCallback	inHandler)
	{
		IOFireWireLibIsochPortCallback	oldHandler	= mStartHandler ;
		mStartHandler = inHandler ;
		
		return oldHandler ;
	}
	
	IOFireWireLibIsochPortCallback
	RemoteIsochPort::SetStopHandler(
		IOFireWireLibIsochPortCallback	inHandler)
	{
		IOFireWireLibIsochPortCallback	oldHandler	= mStopHandler ;
		mStopHandler = inHandler ;
		
		return oldHandler ;
	}
								
#pragma mark -
	// ============================================================
	//
	// RemoteIsochPortCOM
	//
	// ============================================================
	RemoteIsochPortCOM::RemoteIsochPortCOM( Device& userclient, bool talking )
	: RemoteIsochPort( reinterpret_cast<const IUnknownVTbl &>( sInterface ), userclient, talking )
	{
	}
								
	RemoteIsochPortCOM::~RemoteIsochPortCOM()
	{
	}
	
	IUnknownVTbl**
	RemoteIsochPortCOM::Alloc( Device& userclient, bool talking )
	{
		RemoteIsochPortCOM*	me = nil ;

		try {
			me = new RemoteIsochPortCOM( userclient, talking ) ;
		} catch(...) {
		}
			
		return (nil==me) ? nil : reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	
	HRESULT
	RemoteIsochPortCOM::QueryInterface(REFIID iid, void ** ppv )
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if ( CFEqual(interfaceID, IUnknownUUID) ||  CFEqual(interfaceID, kIOFireWireRemoteIsochPortInterfaceID) )
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
	
	IOFireWireLibIsochPortGetSupportedCallback
	RemoteIsochPortCOM::SSetGetSupportedHandler(
		PortRef				self,
		IOFireWireLibIsochPortGetSupportedCallback	inHandler)
	{
		return IOFireWireIUnknown::InterfaceMap<RemoteIsochPortCOM>::GetThis(self)->SetGetSupportedHandler(inHandler) ;
	}
	
	IOFireWireLibIsochPortAllocateCallback
	RemoteIsochPortCOM::SSetAllocatePortHandler(
		PortRef		self,
		IOFireWireLibIsochPortAllocateCallback		inHandler)
	{
		return IOFireWireIUnknown::InterfaceMap<RemoteIsochPortCOM>::GetThis(self)->SetAllocatePortHandler(inHandler) ;
	}
	
	IOFireWireLibIsochPortCallback
	RemoteIsochPortCOM::SSetReleasePortHandler(
		PortRef		self,
		IOFireWireLibIsochPortCallback		inHandler)
	{
		return IOFireWireIUnknown::InterfaceMap<RemoteIsochPortCOM>::GetThis(self)->SetReleasePortHandler(inHandler) ;
	}
	
	IOFireWireLibIsochPortCallback
	RemoteIsochPortCOM::SSetStartHandler(
		PortRef		self,
		IOFireWireLibIsochPortCallback		inHandler)
	{
		return IOFireWireIUnknown::InterfaceMap<RemoteIsochPortCOM>::GetThis(self)->SetStartHandler(inHandler) ;
	}
	
	IOFireWireLibIsochPortCallback
	RemoteIsochPortCOM::SSetStopHandler(
		PortRef		self,
		IOFireWireLibIsochPortCallback		inHandler)
	{
		return IOFireWireIUnknown::InterfaceMap<RemoteIsochPortCOM>::GetThis(self)->SetStopHandler(inHandler) ;
	}
								
#pragma mark -
	LocalIsochPortCOM::Interface	LocalIsochPortCOM::sInterface = 
	{
		INTERFACEIMP_INTERFACE
		,4,0
		
		,IOFIREWIREISOCHPORTIMP_INTERFACE
		,& LocalIsochPortCOM::SModifyJumpDCL
		,& LocalIsochPortCOM::SPrintDCLProgram
		,& LocalIsochPortCOM::SModifyTransferPacketDCLSize
		,& LocalIsochPortCOM::SModifyTransferPacketDCLBuffer
		,& LocalIsochPortCOM::SModifyTransferPacketDCL
		,& LocalIsochPortCOM::S_SetFinalizeCallback
		, & LocalIsochPortCOM::S_SetResourceUsageFlags
		, & LocalIsochPortCOM::S_Notify
	} ;

	LocalIsochPort::LocalIsochPort( const IUnknownVTbl & interface, Device & userclient, bool talking,
			DCLCommand* program, UInt32 startEvent, UInt32 startState, UInt32 startMask,
			IOVirtualRange userProgramRanges[], UInt32 userProgramRangeCount, 
			IOVirtualRange userBufferRanges[], UInt32 userBufferRangeCount, IOFWIsochPortOptions options )
	: IsochPortCOM( interface, userclient, talking, false )
	, mDCLProgram( program )
	, mExpectedStopTokens(0)
	, mDeferredReleaseCount(0)
	, mFinalizeCallback(nil)
	, mBufferRanges( nil )
	, mStarted( false )
	{
		// sorry about the spaghetti.. hope you're hungry:
		
		if ( !program )
		{
			DebugLog( "no DCL program!\n" ) ;
			throw kIOReturnBadArgument ;
		}

//		DeviceCOM :: SPrintDCLProgram ( nil, program, 0 ) ;

		IOReturn	error = kIOReturnSuccess ;

		// trees used to coalesce program/data ranges
		CoalesceTree		bufferTree ;

		// check if user passed in any virtual memory ranges to start with...
		if ( userBufferRanges )
		{
			for( unsigned index=0; index < userBufferRangeCount; ++index )
				bufferTree.CoalesceRange( userBufferRanges[index]) ;
		}		

		LocalIsochPortAllocateParams params ;
		{
			params.programExportBytes = 0 ;
			params.programData = NULL ;
		}

		if ( program->opcode == kDCLNuDCLLeaderOp )
		{
			NuDCLPool*	pool	= reinterpret_cast<NuDCLPool*>( reinterpret_cast< DCLNuDCLLeader* >( program )->program ) ;

			params.version 				= 1 ;		// new-style DCL program

			pool->CoalesceBuffers( bufferTree ) ;
			
			mBufferRangeCount = bufferTree.GetCount() ;
			mBufferRanges = new IOVirtualRange[ mBufferRangeCount ] ;
			if ( !mBufferRanges )
			{
				error = kIOReturnNoMemory ;
			}
			else
			{
				bufferTree.GetCoalesceList( mBufferRanges ) ;
			
				params.programExportBytes 	= pool->Export( & params.programData, mBufferRanges, mBufferRangeCount ) ;
			}
		}
		else
		{	
			unsigned programCount = 0 ;
			
			params.version = 0 ;					// old-style DCL program
			
			// count DCLs in program and coalesce buffers:
			for( DCLCommand * dcl = mDCLProgram; dcl != nil; dcl = dcl->pNextDCLCommand )
			{
				IOVirtualRange tempRange ;
				if ( GetDCLDataBuffer ( dcl, & tempRange.address, & tempRange.length ) )
				{
					bufferTree.CoalesceRange ( tempRange ) ;
				}

				++programCount ;
			}
			
			InfoLog("program count is %d\n", programCount) ;
			
			if ( !error )
			{
				error = ExportDCLs( & params.programData, & params.programExportBytes ) ;
			}

			if ( !error )
			{
				mBufferRangeCount = bufferTree.GetCount() ;
				mBufferRanges = new IOVirtualRange[ mBufferRangeCount ] ;
	
				if ( !mBufferRanges )
				{
					error = kIOReturnNoMemory ;
				}
				else
				{
					bufferTree.GetCoalesceList( mBufferRanges ) ;
				}
			}
		}
		
		if ( error )
		{
			throw error ;
		}
	
//		// allocate lists to store buffer ranges
//		// and get coalesced buffer lists
//
//		UInt32			bufferRangeCount					= bufferTree.GetCount() ;
//		IOVirtualRange	bufferRanges[ bufferRangeCount ] ;
//
//		bufferTree.GetCoalesceList ( bufferRanges ) ;
	
		// fill out param struct and submit to kernel
		params.bufferRanges					= mBufferRanges ;
		params.bufferRangeCount				= mBufferRangeCount ;
		params.talking						= mTalking ;	
		params.startEvent					= startEvent ;
		params.startState					= startState ;
		params.startMask					= startMask ;
		params.userObj						= this ;
		params.options						= options;

#if 0
		params.options |= kFWIsochEnableRobustness;
#endif
	
		InfoLog("startEvent=%x, startState=%x, startMask=%x\n", params.startEvent, params.startState, params.startMask) ;

		IOByteCount	outputSize = sizeof( UserObjectHandle ) ;
		error = :: IOConnectMethodStructureIStructureO (	mDevice.GetUserClientConnection(),
															kLocalIsochPort_Allocate, sizeof ( params ), 
															& outputSize, & params, & mKernPortRef ) ;
		if (error)
		{
			DebugLog ( "Couldn't create local isoch port (error=%x)\nCheck your buffers!\n", error ) ;
			DebugLog ( "Found buffers:\n" ) ;

#if IOFIREWIRELIBDEBUG
			for( unsigned index=0; index < mBufferRangeCount; ++index )
			{
				DebugLog (	"\%u: <0x%x>-<0x%lx>\n", index, mBufferRanges[index].address, 
							(unsigned)mBufferRanges[index].address + mBufferRanges[index].length ) ;
			}
#endif

			throw error ;
		}
		
		{
			mach_msg_type_number_t 	outputSize = 0 ;
			io_scalar_inband_t		params = { (int) mKernPortRef } ;

			// nnn a bit skanky: turn a function pointer to member function into an integer:
			OSAsyncReference asyncRef ;
			asyncRef[ kIOAsyncCalloutFuncIndex ] =  (natural_t) (void (*)(IOReturn)) & LocalIsochPort::DCLStopTokenCallProcHandler ;
			asyncRef[ kIOAsyncCalloutRefconIndex ] = (natural_t) this ;

			error = :: io_async_method_scalarI_scalarO (	mDevice.GetUserClientConnection(), 
															mDevice.GetIsochAsyncPort(), 
															asyncRef, kOSAsyncRefCount, kSetAsyncRef_DCLCallProc, 
															params, 1, nil, & outputSize) ;

			if( error )
			{
				throw error ;
			}
		}
		
		if ( params.programData )
			vm_deallocate( mach_task_self (), (vm_address_t) params.programData, params.programExportBytes ) ;		// this is temporary storage
						
		// make our mutex
		pthread_mutex_init ( & mMutex, nil ) ;	
	}
	
	LocalIsochPort :: ~LocalIsochPort ()
	{
		delete[] mBufferRanges ;
		
		pthread_mutex_destroy( & mMutex ) ;
	}
	
	ULONG
	LocalIsochPort :: Release ()
	{
		Lock () ;
		
		if ( mExpectedStopTokens > 0 )
		{
			Unlock () ;
			++ mDeferredReleaseCount ;
			return mRefCount ;
		}
	
		Unlock () ;

//		while( true )
//		{
//			Lock() ;
//			bool run = ( mExpectedStopTokens > 0 ) ;
//			Unlock() ;
//			
//			if ( !run )
//			{
//				break ;
//			}
//			
//			::CFRunLoopRunInMode( kCFRunLoopDefaultMode, 1, true ) ;
//		}

		return IsochPortCOM::Release() ;
	}

	IOReturn
	LocalIsochPort :: Start()
	{
		IOReturn error = IsochPort::Start() ;
		if ( !error )
		{
			Lock() ;
			mStarted = true ;
			Unlock() ;
		}
		
		return error ;
	}
	
	IOReturn
	LocalIsochPort :: Stop ()
	{
		Lock() ;
		if ( mStarted )
		{
			mStarted = false ;
			++mExpectedStopTokens ;
			InfoLog("waiting for %lu stop tokens\n", mExpectedStopTokens) ;
		}
		Unlock() ;
						
		return IsochPortCOM::Stop() ;	// call superclass Stop()
	}
	
	IOReturn
	LocalIsochPort :: ModifyJumpDCL ( DCLJump* inJump, DCLLabel* inLabel )
	{		
		inJump->pJumpDCLLabel = inLabel ;
	
		IOReturn result = ::IOConnectMethodScalarIScalarO(  mDevice.GetUserClientConnection(), 
															mDevice.MakeSelectorWithObject( kLocalIsochPort_ModifyJumpDCL_d, 
																mKernPortRef ), 
															2, 0, inJump->compilerData, 
															inLabel->compilerData ) ;
		return result ;
	}
	
	IOReturn
	LocalIsochPort :: ModifyTransferPacketDCLSize ( DCLTransferPacket* dcl, IOByteCount newSize )
	{
//		return :: IOConnectMethodScalarIScalarO (	mDevice.GetUserClientConnection(), kLocalIsochPort_ModifyTransferPacketDCLSize, 
//													3, 0, mKernPortRef, dcl->compilerData, newSize ) ;
		return kIOReturnUnsupported ;
	}

	void
	LocalIsochPort :: DCLStopTokenCallProcHandler( IOReturn )
	{
		if ( mExpectedStopTokens > 0 )
		{
			Lock() ;
			mExpectedStopTokens-- ;
			Unlock() ;

			if ( mExpectedStopTokens == 0 )
			{
				if ( mFinalizeCallback )
					(*mFinalizeCallback)(mRefCon) ;
				while ( mDeferredReleaseCount > 0 )
				{
					Release() ;
					--mDeferredReleaseCount ;
				}
			}
		}		
	}
#if 0	
	void
	LocalIsochPort :: S_DCLKernelCallout( DCLCallProc * dcl )
	{
		(*dcl->proc)(dcl->procData) ;
		
		::IOConnectMethodScalarIScalarO( port->mDevice.GetUserClientConnection(), 
				Device::MakeSelectorWithObject( kLocalIsochPort_RunDCLUpdateList_d, port->mKernPortRef ), 1, 0, dcl->compilerData ) ;
	}

	void
	LocalIsochPort :: S_NuDCLKernelCallout ( NuDCL * dcl )
	{
		(*dcl->fData.callback)(dcl->fData.refcon, (NuDCLRef)dcl) ;
		
		::IOConnectMethodScalarIScalarO( 
				mDevice.GetUserClientConnection(), 
				Device::MakeSelectorWithObject( kLocalIsochPort_RunNuDCLUpdateList_d, mKernPortRef ), 1, 0, dcl->fExportIndex ) ;
	}
#endif	
	
	IOReturn
	LocalIsochPort :: SetResourceUsageFlags (
				IOFWIsochResourceFlags 			flags )
	{
		return ::IOConnectMethodScalarIScalarO(	mDevice.GetUserClientConnection(), 
												mDevice.MakeSelectorWithObject( kIsochPort_SetIsochResourceFlags_d, mKernPortRef ), 
												1, 0, flags ) ;
	}

	IOReturn
	LocalIsochPort :: ExportDCLs( IOVirtualAddress * exportBuffer, IOByteCount * exportBytes )
	{
		IOReturn error = kIOReturnSuccess ;
	
		// see how much space we need for serialization...
		
//		unsigned byteCount = 0 ;
		*exportBytes = 0 ;
		for( DCLCommand * dcl = mDCLProgram; dcl != NULL; dcl = dcl->pNextDCLCommand )
		{
			*exportBytes += GetDCLSize( dcl ) ;
			
			switch ( dcl->opcode & ~kFWDCLOpFlagMask )
			{
				case kDCLUpdateDCLListOp :
				{
					// update DCLs store a copy of their update list in the export buffer
					*exportBytes += sizeof( DCLCommand* ) * ((DCLUpdateDCLList*)dcl)->numDCLCommands ;
					break ;
				}
				case kDCLCallProcOp :
				{
					*exportBytes += sizeof( OSAsyncReference ) ;
					break ;
				}
			}
		}

		// buffer to hold copy of DCLs in program
		error = vm_allocate( mach_task_self (), exportBuffer, *exportBytes, true /*anywhere*/ ) ;

		if ( !*exportBuffer && !error )
		{
			error = kIOReturnNoMemory ;
		}

		// start from beginning
		{
			unsigned offset = 0 ;
			IOVirtualAddress buffer = *exportBuffer ;
			
			InfoLog("exporting DCLs, pass 1...\n") ;

			for( DCLCommand * dcl = mDCLProgram; dcl != NULL ; dcl = dcl->pNextDCLCommand )
			{

				unsigned size = GetDCLSize( dcl ) ;
		
				bcopy( dcl, (void*)buffer, size ) ;
				dcl->compilerData = offset ;		// save for later.

				switch ( dcl->opcode & ~kFWDCLOpFlagMask )
				{
					case kDCLUpdateDCLListOp :
					
						size += ( sizeof( DCLCommand* ) * ((DCLUpdateDCLList*)dcl)->numDCLCommands ) ;
						
						break ;

					case kDCLCallProcOp :

						// if this is a callproc DCL, fill in the procData
						// field in our exported copy of the DCL with a pointer
						// to the original DCL. This is for the kernel use...
						// We also leave room in the buffer for an OSAsyncReference..
						// This means the kernel can avoid allocating it...

						((DCLCallProc*)buffer)->procData = (UInt32)dcl ;
						size += sizeof( OSAsyncReference ) ;
						
						break ;

//					// nnn debug only:
//					case kDCLJumpOp :
//					{
//						if ( ((DCLJump*)dcl)->pJumpDCLLabel && ( ( ((DCLJump*)dcl)->pJumpDCLLabel->opcode & ~kFWDCLOpFlagMask ) != kDCLLabelOp ) )
//						{
//							DebugLog("dcl=%p, dcl->pJumpDCLLabel=%p\n", dcl, ((DCLJump*)dcl)->pJumpDCLLabel) ;
//							assert( false ) ;
//						}
//						break ;
//					}
							
					default :
						break ;
				}
				
				buffer += size ;
				offset += size ;
			}

			InfoLog("...done\n") ;
		}
		
		// some DCLs (jumps and update list DCLs) refer to other DCLs with 
		// user space pointers which makes translation a bit harder.
		// The 'compilerData' field of all the DCLs in our export data block 
		// contain an offset in bytes from the beginning of the exported data
		// block..
		// We now replace any user space pointers with offsets for kernel use..
		{
			unsigned offset = 0 ;

			InfoLog("exporting DCLs, pass 2... export size=%d bytes\n", (int)*exportBytes ) ;

			while( offset < *exportBytes )
			{
				DCLCommand * dcl = (DCLCommand*)(*exportBuffer + offset ) ;
				
//				DebugLog("DCL=%p, offset=%x, opcode=%x\n", dcl, offset, dcl->opcode) ;
	
				{
					unsigned opcode = dcl->opcode & ~kFWDCLOpFlagMask ;
					assert( opcode <= 15 || opcode == 20 ) ;
				}
	
				unsigned size = GetDCLSize( dcl ) ;
				
				switch ( dcl->opcode & ~kFWDCLOpFlagMask )
				{
					case kDCLUpdateDCLListOp :
					{
						// make list of offsets from list of user space DCL pointers
						// List starts after DCL in question on export buffer
						
						DCLCommand ** list = (DCLCommand**)( ((DCLUpdateDCLList*)dcl) + 1 )  ;
						for( unsigned index=0; index < ((DCLUpdateDCLList*)dcl)->numDCLCommands; ++index )
						{
							list[ index ] = (DCLCommand*) ((DCLUpdateDCLList*)dcl)->dclCommandList[ index ]->compilerData ;
						}
	
						size += sizeof( DCLCommand* ) * ((DCLUpdateDCLList*)dcl)->numDCLCommands ;
						
						break ;
					}
					
					case kDCLJumpOp :
					{
						((DCLJump*)dcl)->pJumpDCLLabel = (DCLLabel*) ((DCLJump*)dcl)->pJumpDCLLabel->compilerData ;
						break ;
					}
					
					case kDCLCallProcOp :
					{
						size += sizeof( OSAsyncReference ) ;
						break ;
					}
						
					default :
					
						break ;
				}
				
				offset += size ;
				
			}

			InfoLog("...done\n") ;
			
		}
		
		// fill in DCL compiler data fields with ( program index + 1 ) ;
		{
			unsigned count = 0 ;
			for( DCLCommand * dcl = mDCLProgram; dcl != nil; dcl = dcl->pNextDCLCommand )
			{
				dcl->compilerData = ++count ;		// index incremented here..
													// compiler data for DCL should be index + 1
			}
		}
		
		return error ;
	}
	
	IOReturn
	LocalIsochPort :: Notify (
		IOFWDCLNotificationType 	notificationType,
		void ** 					inDCLList, 
		UInt32 						numDCLs )
	{
		IOReturn error = kIOReturnSuccess ;
	
		switch( notificationType )
		{
			case kFWNuDCLModifyNotification:
			{
				IOByteCount dataSize = 0 ;
				for( unsigned index=0; index < numDCLs; ++index )
				{
					dataSize += 4 + ((NuDCL**)inDCLList)[ index ]->Export( NULL, NULL, NULL ) ;
				}
				
				UInt8 data[ dataSize ] ;

				{
					UInt8 * exportCursor = data ;
					for( unsigned index=0; index < numDCLs; ++index )				
					{
						NuDCL * dcl = ((NuDCL**)inDCLList)[ index ] ;
						*(UInt32*)exportCursor = dcl->GetExportIndex() ;
						exportCursor += sizeof( UInt32 ) ;
						
						dcl->Export( (IOVirtualAddress*) & exportCursor, mBufferRanges, mBufferRangeCount ) ;
					}
				}
				
				error = ::IOConnectMethodScalarIStructureI( 
						mDevice.GetUserClientConnection(),
						Device::MakeSelectorWithObject( kLocalIsochPort_Notify_d, mKernPortRef ), 
						2, dataSize, (UInt32)notificationType, numDCLs, data ) ;
				
				break ;
			}
			
			case kFWNuDCLModifyJumpNotification:
			{
				unsigned pairCount = numDCLs << 1 ;
				
				unsigned dcls[ pairCount ] ;
				
				{
					unsigned index = 0 ;
					unsigned pairIndex=0; 
					
					while( pairIndex < pairCount )
					{
						NuDCL * theDCL = ((NuDCL**)inDCLList)[ index++ ] ;
						dcls[ pairIndex++ ] = theDCL->GetExportIndex() ;
						dcls[ pairIndex++ ] = theDCL->GetBranch()->GetExportIndex() ;
					}
				}

				error = ::IOConnectMethodScalarIStructureI( 
						mDevice.GetUserClientConnection(),
						Device::MakeSelectorWithObject( kLocalIsochPort_Notify_d, mKernPortRef ), 
						2, sizeof( dcls ), (UInt32)notificationType, numDCLs, dcls ) ;
				break ;
			}
			
			case kFWNuDCLUpdateNotification:
			{
				unsigned dcls[ numDCLs ] ;

				for( unsigned index=0; index < numDCLs; ++index )
				{
					dcls[ index ] = ((NuDCL*)inDCLList[ index ])->GetExportIndex() ;
				}

				error = ::IOConnectMethodScalarIStructureI( 
						mDevice.GetUserClientConnection(),
						Device::MakeSelectorWithObject( kLocalIsochPort_Notify_d, mKernPortRef ), 
						2, sizeof( dcls ), (UInt32)notificationType, numDCLs, dcls ) ;
				break ;
			}
			
			case kFWDCLUpdateNotification:
			case kFWDCLModifyNotification:
			{
				error = kIOReturnUnsupported ;
			}
				
			default:
				error = kIOReturnBadArgument ;
		}		
		
		return error ;
	}

#pragma mark -
	// ============================================================
	//
	// LocalIsochPortCOM
	//
	// ============================================================
	
	LocalIsochPortCOM::LocalIsochPortCOM( Device& userclient, bool talking, DCLCommand* program, UInt32 startEvent,
			UInt32 startState, UInt32 startMask, IOVirtualRange programRanges[], UInt32 programRangeCount,
			IOVirtualRange bufferRanges[], UInt32 bufferRangeCount, IOFWIsochPortOptions options )
	: LocalIsochPort( reinterpret_cast<const IUnknownVTbl &>( sInterface ), userclient, talking, program, 
			startEvent, startState, startMask, programRanges, 
			programRangeCount, bufferRanges, bufferRangeCount, options )
	{
	}
	
	LocalIsochPortCOM::~LocalIsochPortCOM()
	{
	}
	
	IUnknownVTbl**
	LocalIsochPortCOM::Alloc( Device & userclient, Boolean talking, DCLCommand * program,
			UInt32 startEvent, UInt32 startState, UInt32 startMask,
			IOVirtualRange programRanges[], UInt32 programRangeCount, 
			IOVirtualRange bufferRanges[], UInt32 bufferRangeCount,
			IOFWIsochPortOptions options )
	{
		LocalIsochPortCOM*	me = nil ;
		
		try
		{
			me = new LocalIsochPortCOM (	userclient, (bool)talking, program, startEvent, startState, startMask,
											programRanges, programRangeCount, bufferRanges, bufferRangeCount, options ) ;
		}
		catch(...)
		{
		}

		return ( nil == me ) ? nil : reinterpret_cast < IUnknownVTbl ** > ( & me->GetInterface () ) ;
	}
	
	HRESULT
	LocalIsochPortCOM :: QueryInterface (	REFIID iid, void ** ppv )
	{
		HRESULT		result			= S_OK ;
		CFUUIDRef	interfaceID		= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;

		*ppv = nil ;
	
		if ( CFEqual(interfaceID, IUnknownUUID) 
				|| CFEqual(interfaceID, kIOFireWireLocalIsochPortInterfaceID ) 
				|| CFEqual( interfaceID, kIOFireWireLocalIsochPortInterfaceID_v2 )
#if 0
				|| CFEqual( interfaceID, kIOFireWireLocalIsochPortInterfaceID_v3 )	// don't support this yet...
#endif
				|| CFEqual( interfaceID, kIOFireWireLocalIsochPortInterfaceID_v4 )
				|| CFEqual( interfaceID, kIOFireWireLocalIsochPortInterfaceID_v5 )
			)
		{
			* ppv = & GetInterface () ;
			AddRef () ;
		}
		else
		{
			DebugLog("unknown local isoch port interface UUID\n") ;
			
			* ppv = nil ;
			result = E_NOINTERFACE ;
		}	
		
		:: CFRelease ( interfaceID ) ;
		return result ;
	}
	
	IOReturn
	LocalIsochPortCOM :: SModifyJumpDCL(
				IOFireWireLibLocalIsochPortRef 	self, 
				DCLJump *	 					jump, 
				DCLLabel *		 				label)
	{
		return IOFireWireIUnknown :: InterfaceMap< LocalIsochPortCOM > :: GetThis ( self )->ModifyJumpDCL ( jump, label ) ;
	}
	
	//
	// utility functions
	//
	
	void
	LocalIsochPortCOM :: SPrintDCLProgram (
				IOFireWireLibLocalIsochPortRef 	self ,
				const DCLCommand *				program ,
				UInt32							length )
	{
		DeviceCOM :: SPrintDCLProgram ( nil, program, length ) ;
	}	

	IOReturn
	LocalIsochPortCOM :: SModifyTransferPacketDCLSize ( 
				PortRef 				self, 
				DCLTransferPacket * 	dcl, 
				IOByteCount 			newSize )
	{
		IOReturn error = kIOReturnBadArgument ;
	
		switch ( dcl->opcode )
		{
			case kDCLSendPacketStartOp:
			case kDCLSendPacketWithHeaderStartOp:
			case kDCLSendPacketOp:
			case kDCLReceivePacketStartOp:
			case kDCLReceivePacketOp:
			case kDCLReceiveBufferOp:			
				error = IOFireWireIUnknown::InterfaceMap<LocalIsochPortCOM>::GetThis( self )->ModifyTransferPacketDCLSize( dcl, newSize ) ;
		}

		return error ;
	}

	IOReturn
	LocalIsochPortCOM :: SModifyTransferPacketDCLBuffer (
				PortRef 				self, 
				DCLTransferPacket * 	dcl, 
				void * 					newBuffer )
	{
		return kIOReturnUnsupported ;
	}
	
	IOReturn
	LocalIsochPortCOM :: SModifyTransferPacketDCL ( PortRef self, DCLTransferPacket * dcl, void * newBuffer, IOByteCount newSize )
	{
		return kIOReturnUnsupported ;
	}

	//
	// v4
	//
	
	IOReturn
	LocalIsochPortCOM :: S_SetFinalizeCallback( 
				IOFireWireLibLocalIsochPortRef 				self, 
				IOFireWireLibIsochPortFinalizeCallback	 	finalizeCallback )
	{
		LocalIsochPortCOM * 	me = IOFireWireIUnknown :: InterfaceMap< LocalIsochPortCOM > :: GetThis( self ) ;

		me->mFinalizeCallback = finalizeCallback ;
		
		return kIOReturnSuccess ;
	}

	//
	// v5 (panther)
	//
	
	IOReturn
	LocalIsochPortCOM :: S_SetResourceUsageFlags (
				IOFireWireLibLocalIsochPortRef	self, 
				IOFWIsochResourceFlags 			flags )
	{
		return IOFireWireIUnknown::InterfaceMap< LocalIsochPortCOM >::GetThis( self )->SetResourceUsageFlags( flags ) ;
	}

	IOReturn
	LocalIsochPortCOM :: S_Notify( 
				IOFireWireLibLocalIsochPortRef self, 
				IOFWDCLNotificationType notificationType, 
				void ** inDCLList, 
				UInt32 numDCLs )
	{
		return  IOFireWireIUnknown::InterfaceMap< LocalIsochPortCOM >::GetThis( self )->Notify( notificationType, inDCLList, numDCLs ) ;
	}
}
