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
 */

#import "IOFireWireLibIsochPort.h"
#import <IOKit/iokitmig.h>
#import <unistd.h>
#import <pthread.h>
#import <exception>

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

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
				*outDataBuffer		= (IOVirtualAddress)((DCLTransferPacketStruct*)dcl)->buffer ;
				*outDataLength		= ((DCLTransferPacketStruct*)dcl)->size ;
				result = true ;
				break ;
				
			case kDCLSendBufferOp:
			case kDCLReceiveBufferOp:
				//zzz what should I do here?
				break ;
	
			case kDCLPtrTimeStampOp:
				*outDataBuffer		= (IOVirtualAddress)((DCLPtrTimeStampStruct*)dcl)->timeStampPtr ;
				*outDataLength		= sizeof( *( ((DCLPtrTimeStampStruct*)dcl)->timeStampPtr) ) ;
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
				result = sizeof(DCLTransferPacketStruct) ;
				break ;
				
			case kDCLSendBufferOp:
			case kDCLReceiveBufferOp:
				result = sizeof(DCLTransferBufferStruct) ;
				break ;
	
			case kDCLCallProcOp:
				result = sizeof(DCLCallProcStruct) ;
				break ;
				
			case kDCLLabelOp:
				result = sizeof(DCLLabelStruct) ;
				break ;
				
			case kDCLJumpOp:
				result = sizeof(DCLJump) ;
				break ;
				
			case kDCLSetTagSyncBitsOp:
				result = sizeof(DCLSetTagSyncBitsStruct) ;
				break ;
				
			case kDCLUpdateDCLListOp:
				result = sizeof(DCLUpdateDCLListStruct) ;
				break ;
	
			case kDCLPtrTimeStampOp:
				result = sizeof(DCLPtrTimeStampStruct) ;
		}
		
		return result ;
	}
	
#pragma mark -
	// ============================================================
	// CoalesceTree
	// ============================================================
	CoalesceTree::CoalesceTree()
	{
		mTop = nil ;
	}
	
	CoalesceTree::~CoalesceTree()
	{
		DeleteNode(mTop) ;
	}
	
	void
	CoalesceTree::DeleteNode(Node* inNode)
	{
		if (inNode)
		{
			DeleteNode(inNode->left) ;
			DeleteNode(inNode->right) ;
			delete inNode ;
		}
	}
				
	void
	CoalesceTree::CoalesceRange(const IOVirtualRange& inRange)
	{
		if ( inRange.address == NULL or inRange.length == 0)
			return ;
	
		// ranges must be page aligned and have lengths in multiples of the vm page size only:
		IOVirtualRange range = { trunc_page(inRange.address), round_page( (inRange.address & getpagesize() - 1) + inRange.length - 1) } ;
	
		if (mTop)
			CoalesceRange(range, mTop) ;
		else
		{
			mTop					= new Node ;
			mTop->left 				= nil ;
			mTop->right				= nil ;
			mTop->range.address		= range.address ;
			mTop->range.length		= range.length ;
		}
	}
	
	void
	CoalesceTree::CoalesceRange(const IOVirtualRange& inRange, Node* inNode)
	{
		if (inRange.address > inNode->range.address)
		{
			if ( (inRange.address - inNode->range.address) <= inNode->range.length)
			{
				// merge
				inNode->range.length = MAX(inNode->range.length, inRange.address + inRange.length - inNode->range.address) ;
			}
			else
				if (inNode->right)
					CoalesceRange(inRange, inNode->right) ;
				else
				{
					inNode->right 					= new Node ;
					inNode->right->left				= nil ;
					inNode->right->right			= nil ;
					
					inNode->right->range.address	= inRange.address ;
					inNode->right->range.length		= inRange.length ;
				}
		}
		else	
		{
			if ((inNode->range.address - inRange.address) <= inRange.length)
			{
				// merge
				inNode->range.length 	= MAX(inRange.length, inNode->range.address + inNode->range.length - inRange.address) ;
				inNode->range.address 	= inRange.address ;
			}
			else
				if (inNode->left)
					CoalesceRange(inRange, inNode->left) ;
				else
				{
					inNode->left					= new Node ;
					inNode->left->left			= nil ;
					inNode->left->right			= nil ;
					
					inNode->left->range.address	= inRange.address ;
					inNode->left->range.length	= inRange.length ;
				}
		}
	}
	
	const UInt32
	CoalesceTree::GetCount() const
	{
		return GetCount(mTop) ;
	}
	
	const UInt32
	CoalesceTree::GetCount(Node* inNode) const
	{
		if (inNode)
			return 1 + GetCount(inNode->left) + GetCount(inNode->right) ;
		else
			return 0 ;
	}
	
	void
	CoalesceTree::GetCoalesceList(IOVirtualRange* outRanges) const
	{
		UInt32 index = 0 ;
		GetCoalesceList(outRanges, mTop, & index) ;
	}
	
	void
	CoalesceTree::GetCoalesceList(IOVirtualRange* outRanges, Node* inNode, UInt32* pIndex) const
	{
		if (inNode)
		{
			// add ranges less than us first
			GetCoalesceList(outRanges, inNode->left, pIndex) ;
	
			// add us
			outRanges[*pIndex].address	= inNode->range.address ;
			outRanges[*pIndex].length	= inNode->range.length ;
			++(*pIndex) ;
			
			// add ranges to the right of us
			GetCoalesceList(outRanges, inNode->right, pIndex) ;
		}
	}
	
#pragma mark -
	// ============================================================
	//
	// IsochPort
	//
	// ============================================================
	
	IsochPort::IsochPort( IUnknownVTbl* interface, Device& userclient, bool talking, bool allocateKernPort )
	: IOFireWireIUnknown( interface ),
	  mUserClient( userclient ),
	  mKernPortRef( 0 ),
	  mTalking( talking )
	{
		mUserClient.AddRef() ;

		if (allocateKernPort)
		{
			IsochPortAllocateParams	params ;
			IOByteCount					outputSize = sizeof(KernIsochPortRef) ;
			
			IOReturn err = ::IOConnectMethodStructureIStructureO( mUserClient.GetUserClientConnection(), 
								kIsochPort_Allocate, sizeof(IsochPortAllocateParams), & outputSize, & params, 
								& mKernPortRef) ;
			if(err)
				throw std::exception() ;
		}
	}
	
	IsochPort::~IsochPort()
	{
		if ( mKernPortRef )
		{
			IOReturn	err = kIOReturnSuccess;
			
			err = ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(),
									               kIsochPort_Release, 1, 0, mKernPortRef ) ;
		
			IOFireWireLibLogIfErr_( err, "%s %u: Couldn't release kernel port", __FILE__, __LINE__) ;
		}
		
		mUserClient.Release() ;
	}
	
	IOReturn
	IsochPort::GetSupported(
		IOFWSpeed&					maxSpeed, 
		UInt64& 					chanSupported )
	{
		return ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kIsochPort_GetSupported,
						1, 3, mKernPortRef, & maxSpeed, (UInt32*)&chanSupported, (UInt32*)&chanSupported + 1) ;
	}
	
	IOReturn
	IsochPort::AllocatePort( IOFWSpeed speed, UInt32 chan )
	{
		return ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kIsochPort_AllocatePort,
						3, 0, mKernPortRef, speed, chan ) ;
	}
	
	IOReturn
	IsochPort::ReleasePort()
	{
		return ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(),
						kIsochPort_ReleasePort, 1, 0, mKernPortRef ) ;
	}
	
	IOReturn
	IsochPort::Start()
	{
		return ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(),
						kIsochPort_Start, 1, 0, mKernPortRef ) ;
	}
	
	IOReturn
	IsochPort::Stop()
	{
		return ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kIsochPort_Stop, 1, 0, 
						mKernPortRef ) ;
	}
	
#pragma mark -
	// ============================================================
	//
	// IsochPortCOM
	//
	// ============================================================
	
	IsochPortCOM::IsochPortCOM( IUnknownVTbl* interface, Device& inUserClient, bool talking, bool allocateKernPort )
	:	IsochPort( interface, inUserClient, talking, allocateKernPort )
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
	
	RemoteIsochPort::RemoteIsochPort( IUnknownVTbl* interface, Device& userclient, bool talking )
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
	: RemoteIsochPort( reinterpret_cast<IUnknownVTbl*>(& sInterface), userclient, talking )
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
		INTERFACEIMP_INTERFACE,
		1, 0,
		
		IOFIREWIREISOCHPORTIMP_INTERFACE,
		& LocalIsochPortCOM::SModifyJumpDCL,
		& LocalIsochPortCOM::SPrintDCLProgram,
		& LocalIsochPortCOM::SModifyTransferPacketDCLSize,
		& LocalIsochPortCOM::SModifyTransferPacketDCLBuffer,
		& LocalIsochPortCOM::SModifyTransferPacketDCL
	} ;
	
	LocalIsochPort::LocalIsochPort( IUnknownVTbl* interface, Device& userclient, bool talking, DCLCommand* inDCLProgram, 
										UInt32 inStartEvent, UInt32 inStartState, UInt32 inStartMask, IOVirtualRange inDCLProgramRanges[], 
										UInt32 inDCLProgramRangeCount, IOVirtualRange inBufferRanges[], UInt32 inBufferRangeCount )
	: IsochPortCOM( interface, userclient, talking, false ), 
	  mDCLProgram(inDCLProgram),
	  mStartEvent(inStartEvent),
	  mStartState(inStartState),
	  mStartMask(inStartMask),
	  mExpectedStopTokens(0),
	  mDeferredRelease(false)
	{
		if ( !inDCLProgram )
		{
			IOFireWireLibLog_("%s %u:no DCL program!\n", __FILE__, __LINE__) ;
			throw;//return kIOReturnBadArgument ;
		}

		// trees used to coalesce program/data ranges
		CoalesceTree		programTree ;
		CoalesceTree		bufferTree ;
		
		// check if user passed in any virtual memory ranges to start with...
		//zzz maybe we should try to validate that the ranges are actually in
		//zzz client memory?
		if (inDCLProgramRanges)
			for(UInt32 index=0; index < inDCLProgramRangeCount; ++index)
				programTree.CoalesceRange(inDCLProgramRanges[index]) ;
	
		if (inBufferRanges)
			for(UInt32 index=0; index < inBufferRangeCount; ++index)
				bufferTree.CoalesceRange(inBufferRanges[index]) ;
	
		// point to beginning of DCL program
		DCLCommand*							pCurrentDCLStruct = mDCLProgram ;
		IOVirtualRange						tempRange ;
		LocalIsochPortAllocateParams		params ;
	
		params.userDCLProgramDCLCount = 0 ;
		while (pCurrentDCLStruct)
		{
			++params.userDCLProgramDCLCount ;
		
			tempRange.address	= (IOVirtualAddress) pCurrentDCLStruct ;
			tempRange.length	= GetDCLSize(pCurrentDCLStruct) ;
			
			programTree.CoalesceRange(tempRange) ;
			
			if ( GetDCLDataBuffer(pCurrentDCLStruct, & tempRange.address, & tempRange.length) )
			{
				bufferTree.CoalesceRange(tempRange) ;
			}
			
			pCurrentDCLStruct = pCurrentDCLStruct->pNextDCLCommand ;
		}
	
		IOVirtualRange*				programRanges 		= nil ;
		IOVirtualRange*				bufferRanges		= nil ;
		UInt32						programRangeCount	= programTree.GetCount() ;
		UInt32						bufferRangeCount	= bufferTree.GetCount() ;
	
		// allocate lists to store coalesced ranges
		// and get trees' coalesced buffer lists into the buffers
		if ( nil == (programRanges = new IOVirtualRange[programRangeCount] ))
			throw;//result = kIOReturnNoMemory ;
		else
			programTree.GetCoalesceList(programRanges) ;
		
			
		if ( bufferRangeCount > 0)
		{
			if (nil == (bufferRanges = new IOVirtualRange[bufferRangeCount] ))
				throw;//result = kIOReturnNoMemory ;
			else
				bufferTree.GetCoalesceList(bufferRanges) ;
		}
	
		// fill out param struct and submit to kernel
		params.userDCLProgram				= mDCLProgram ;
		params.userDCLProgramRanges 		= programRanges ;
		params.userDCLProgramRangeCount		= programRangeCount ;
		params.userDCLBufferRanges			= bufferRanges ;
		params.userDCLBufferRangeCount		= bufferRangeCount ;
		params.talking						= mTalking ;	
		params.startEvent					= mStartEvent ;
		params.startState					= mStartState ;
		params.startMask					= mStartMask ;
		params.userObj						= this ;
		
		IOByteCount	outputSize = sizeof(KernIsochPortRef) ;
		IOReturn err = ::IOConnectMethodStructureIStructureO( mUserClient.GetUserClientConnection(), 
							kLocalIsochPort_Allocate, sizeof(params), & outputSize, & params, & mKernPortRef) ;
		if (err)
		{
			IOFireWireLibLog_("Couldn't create local isoch port\nCheck your buffers!\n") ;
			IOFireWireLibLog_("Found buffers:\n") ;

#if IOFIREWIRELIBDEBUG
			for( unsigned index=0; index < bufferRangeCount; ++index )
			{
				IOFireWireLibLog_("\%u: <0x%lx>-<0x%lx>\n", index, bufferRanges[index].address, 
						(unsigned)bufferRanges[index].address + bufferRanges[index].length ) ;
			}
#endif
			
			throw std::exception() ;
		}
		
		// done with these:
		delete[] programRanges ;
		delete[] bufferRanges ;
		
		{
			mach_msg_type_number_t 	outputSize = 0 ;
			io_scalar_inband_t		params = { (int)mKernPortRef, (int)& DCLCallProcHandler, (int)this } ;
			
	//		params[0]	= (UInt32) mKernPortRef ;
	//		params[1]	= (UInt32) & DCLCallProcHandler ;
	//		params[2]	= (UInt32) this ;
			
			err = ::io_async_method_scalarI_scalarO( mUserClient.GetUserClientConnection(), 
							mUserClient.GetIsochAsyncPort(), mAsyncRef, 1, kSetAsyncRef_DCLCallProc, params, 2, 
							NULL, & outputSize) ;
			if(err)
				throw std::exception() ;
		}
		
		// make our mutex
		pthread_mutex_init( & mMutex, nil ) ;	
	}
	
	LocalIsochPort::~LocalIsochPort()
	{
		pthread_mutex_destroy( & mMutex ) ;
	}
	
	ULONG
	LocalIsochPort::Release()
	{
		Lock() ;
		if ( mExpectedStopTokens > 0 )
		{
			Unlock() ;
			mDeferredRelease = true ;
			return mRefCount ;
		}
	
		Unlock() ;
		
		return IsochPortCOM::Release() ;
	}

	IOReturn
	LocalIsochPort::Stop()
	{
		Lock() ;
		++mExpectedStopTokens ;
		IOFireWireLibLog_("waiting for %lu stop tokens\n", mExpectedStopTokens) ;
		Unlock() ;
		
		return IsochPortCOM::Stop() ;	// call superclass Stop()
	}
	
	IOReturn
	LocalIsochPort::ModifyJumpDCL( DCLJump* inJump, DCLLabelStruct* inLabel )
	{		
		inJump->pJumpDCLLabel = inLabel ;
	
		IOReturn result = ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), 
						kLocalIsochPort_ModifyJumpDCL, 3, 0, mKernPortRef, inJump->compilerData, 
						inLabel->compilerData ) ;
		
		return result ;
	}
	
	IOReturn
	LocalIsochPort::ModifyTransferPacketDCLSize( DCLTransferPacket* dcl, IOByteCount newSize )
	{
		return ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kLocalIsochPort_ModifyTransferPacketDCLSize, 3, 0,
						mKernPortRef, dcl->compilerData, newSize ) ;
	}

	void
	LocalIsochPort::DCLCallProcHandler(
		void*				inRefCon,
		IOReturn			result,
		LocalIsochPort*		me )
	{
		if ( me->mExpectedStopTokens > 0 )
		{
			if ( result == kIOFireWireLastDCLToken && inRefCon==(void*)0xFFFFFFFF )
			{			
				me->Lock() ;
				me->mExpectedStopTokens-- ;
				me->Unlock() ;
	
				if ( me->mExpectedStopTokens == 0 && me->mDeferredRelease )
					me->Release() ;
			}
			return ;		
		}
		
		if ( result == kIOReturnSuccess )
		{
			DCLCallProcStruct*	callProcDCL = (DCLCallProcStruct*)inRefCon ;
			
			(*callProcDCL->proc)((DCLCommand*)inRefCon) ;
		}
	}
	
	void
	LocalIsochPort::Lock()
	{
		pthread_mutex_lock( & mMutex ) ;
	}
	
	void
	LocalIsochPort::Unlock()
	{
		pthread_mutex_unlock( & mMutex ) ;
	}
	
	void
	LocalIsochPort::PrintDCLProgram(
		const DCLCommand*		dcl,
		UInt32						inDCLCount) 
	{
		mUserClient.PrintDCLProgram(dcl, inDCLCount) ;
	}
	
#pragma mark -
	// ============================================================
	//
	// LocalIsochPortCOM
	//
	// ============================================================
	
	LocalIsochPortCOM::LocalIsochPortCOM( Device& userclient, bool inTalking, DCLCommand* inDCLProgram, UInt32 inStartEvent,
			UInt32 inStartState, UInt32 inStartMask, IOVirtualRange inDCLProgramRanges[], UInt32 inDCLProgramRangeCount,
			IOVirtualRange inBufferRanges[], UInt32 inBufferRangeCount)
	: LocalIsochPort( reinterpret_cast<IUnknownVTbl*>(& sInterface), userclient, inTalking, inDCLProgram, inStartEvent, 
			inStartState, inStartMask, inDCLProgramRanges, inDCLProgramRangeCount, inBufferRanges, inBufferRangeCount )
	{
	}
	
	LocalIsochPortCOM::~LocalIsochPortCOM()
	{
	}
	
	IUnknownVTbl**
	LocalIsochPortCOM::Alloc(
		Device&							inUserClient, 
		Boolean							inTalking,
		DCLCommand*				inDCLProgram,
		UInt32							inStartEvent,
		UInt32							inStartState,
		UInt32							inStartMask,
		IOVirtualRange					inDCLProgramRanges[],			// optional optimization parameters
		UInt32							inDCLProgramRangeCount,
		IOVirtualRange					inBufferRanges[],
		UInt32							inBufferRangeCount)
	{
		LocalIsochPortCOM*	me = nil ;
		
		try {
			me = new LocalIsochPortCOM(inUserClient, inTalking, inDCLProgram, inStartEvent, inStartState, inStartMask,
							inDCLProgramRanges, inDCLProgramRangeCount, inBufferRanges, inBufferRangeCount ) ;
		} catch(...) {
		}

		return (nil == me) ? nil : reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	HRESULT
	LocalIsochPortCOM::QueryInterface(REFIID iid, void ** ppv )
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if ( 	CFEqual(interfaceID, IUnknownUUID) 
				|| CFEqual(interfaceID, kIOFireWireLocalIsochPortInterfaceID ) 
				|| CFEqual( interfaceID, kIOFireWireLocalIsochPortInterfaceID_v2 )
#if 0
				|| CFEqual( interfaceID, kIOFireWireLocalIsochPortInterfaceID_v3 )	// don't support this yet...
#endif
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
	LocalIsochPortCOM::SModifyJumpDCL(
		IOFireWireLibLocalIsochPortRef 	self, 
		DCLJump* 					inJump, 
		DCLLabelStruct* 				inLabel)
	{
		return IOFireWireIUnknown::InterfaceMap<LocalIsochPortCOM>::GetThis(self)->ModifyJumpDCL(inJump, inLabel) ;
	}
	
	// --- utility functions ----------
	void
	LocalIsochPortCOM::SPrintDCLProgram(
		IOFireWireLibLocalIsochPortRef 	self, 
		const DCLCommand*			inProgram,
		UInt32							inLength)
	{
		IOFireWireIUnknown::InterfaceMap<LocalIsochPortCOM>::GetThis(self)->PrintDCLProgram(inProgram, inLength) ;
	}	

	IOReturn
	LocalIsochPortCOM::SModifyTransferPacketDCLSize( PortRef self, DCLTransferPacket* dcl, IOByteCount newSize )
	{
		IOReturn err = kIOReturnBadArgument ;
	
		switch(dcl->opcode)
		{
			case kDCLSendPacketStartOp:
			case kDCLSendPacketWithHeaderStartOp:
			case kDCLSendPacketOp:
			case kDCLReceivePacketStartOp:
			case kDCLReceivePacketOp:
			case kDCLReceiveBufferOp:			
				err = IOFireWireIUnknown::InterfaceMap<LocalIsochPortCOM>::GetThis(self)->ModifyTransferPacketDCLSize(dcl, newSize) ;
		}

		return err ;
	}

	IOReturn
	LocalIsochPortCOM::SModifyTransferPacketDCLBuffer( PortRef self, DCLTransferPacket* dcl, void* newBuffer )
	{
		return kIOReturnUnsupported ;
	}
	
	IOReturn
	LocalIsochPortCOM::SModifyTransferPacketDCL( PortRef self, DCLTransferPacket* dcl, void* newBuffer, IOByteCount newSize )
	{
		return kIOReturnUnsupported ;
	}
}
