/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
 *  Created by NWG on Mon Mar 12 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include "IOFireWireLibIsochPort.h"
#include <IOKit/iokitmig.h>


#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#define IOFIREWIREISOCHPORTIMP_INTERFACE	\
	& IOFireWireLibIsochPortCOM::SGetSupported,	\
	& IOFireWireLibIsochPortCOM::SAllocatePort,	\
	& IOFireWireLibIsochPortCOM::SReleasePort,	\
	& IOFireWireLibIsochPortCOM::SStart,	\
	& IOFireWireLibIsochPortCOM::SStop,	\
	& IOFireWireLibIsochPortCOM::SSetRefCon,	\
	& IOFireWireLibIsochPortCOM::SGetRefCon
	
IOFireWireRemoteIsochPortInterface		IOFireWireLibRemoteIsochPortCOM::sInterface =
{
	INTERFACEIMP_INTERFACE,
	1, 0,
	
	IOFIREWIREISOCHPORTIMP_INTERFACE,
	& IOFireWireLibRemoteIsochPortCOM::SSetGetSupportedHandler,
	& IOFireWireLibRemoteIsochPortCOM::SSetAllocatePortHandler,
	& IOFireWireLibRemoteIsochPortCOM::SSetReleasePortHandler,
	& IOFireWireLibRemoteIsochPortCOM::SSetStartHandler,
	& IOFireWireLibRemoteIsochPortCOM::SSetStopHandler,
} ;

IOFireWireLocalIsochPortInterface	IOFireWireLibLocalIsochPortCOM::sInterface = 
{
	INTERFACEIMP_INTERFACE,
	1, 0,
	
	IOFIREWIREISOCHPORTIMP_INTERFACE,
	& IOFireWireLibLocalIsochPortCOM::SModifyJumpDCL,
	& IOFireWireLibLocalIsochPortCOM::SPrintDCLProgram
} ;


// ============================================================
// utility functions
// ============================================================

Boolean
GetDCLDataBuffer(
	DCLCommandStruct*	inDCL,
	IOVirtualAddress*	outDataBuffer,
	IOByteCount*		outDataLength)
{
	Boolean	result = false ;

	switch(inDCL->opcode & ~kFWDCLOpFlagMask)
	{
		case kDCLSendPacketStartOp:
		case kDCLSendPacketWithHeaderStartOp:
		case kDCLSendPacketOp:
		case kDCLReceivePacketStartOp:
		case kDCLReceivePacketOp:
			*outDataBuffer		= (IOVirtualAddress)((DCLTransferPacketStruct*)inDCL)->buffer ;
			*outDataLength		= ((DCLTransferPacketStruct*)inDCL)->size ;
			result = true ;
			break ;
			
		case kDCLSendBufferOp:
		case kDCLReceiveBufferOp:
			//zzz what should I do here?
			break ;

		case kDCLPtrTimeStampOp:
			*outDataBuffer		= (IOVirtualAddress)((DCLPtrTimeStampStruct*)inDCL)->timeStampPtr ;
			*outDataLength		= sizeof( *( ((DCLPtrTimeStampStruct*)inDCL)->timeStampPtr) ) ;
			result = true ;
			break ;
		
		default:
			break ;
	}
	
	return result ;
}

IOByteCount
GetDCLSize(
	DCLCommandStruct*	inDCL)
{
	IOByteCount result ;

	switch(inDCL->opcode & ~kFWDCLOpFlagMask)
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
			result = sizeof(DCLJumpStruct) ;
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

// ============================================================
// IOFireWireLibCoalesceTree
// ============================================================
IOFireWireLibCoalesceTree::IOFireWireLibCoalesceTree()
{
	mTop = nil ;
}

IOFireWireLibCoalesceTree::~IOFireWireLibCoalesceTree()
{
	DeleteNode(mTop) ;
}

void
IOFireWireLibCoalesceTree::DeleteNode(Node* inNode)
{
	if (inNode)
	{
		DeleteNode(inNode->left) ;
		DeleteNode(inNode->right) ;
		delete inNode ;
	}
}
			
void
IOFireWireLibCoalesceTree::CoalesceRange(const IOVirtualRange& inRange)
{
	if (mTop)
		CoalesceRange(inRange, mTop) ;
	else
	{
		mTop					= new Node ;
		mTop->left 				= nil ;
		mTop->right				= nil ;
		mTop->range.address		= inRange.address ;
		mTop->range.length		= inRange.length ;
	}
}

//inRange			|xxxxxxxxxxxxx|
//inNode->range				|xxxxxxxxxxxxxxxxxxxx|
void
IOFireWireLibCoalesceTree::CoalesceRange(const IOVirtualRange& inRange, Node* inNode)
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
IOFireWireLibCoalesceTree::GetCount() const
{
	return GetCount(mTop) ;
}

const UInt32
IOFireWireLibCoalesceTree::GetCount(Node* inNode) const
{
	if (inNode)
		return 1 + GetCount(inNode->left) + GetCount(inNode->right) ;
	else
		return 0 ;
}

void
IOFireWireLibCoalesceTree::GetCoalesceList(IOVirtualRange* outRanges) const
{
	UInt32 index = 0 ;
	GetCoalesceList(outRanges, mTop, & index) ;
}

void
IOFireWireLibCoalesceTree::GetCoalesceList(IOVirtualRange* outRanges, Node* inNode, UInt32* pIndex) const
{
	if (inNode)
	{
		// add ranges less than us first
		GetCoalesceList(outRanges, inNode->left, pIndex) ;

		// add us
		outRanges[*pIndex].address	= inNode->range.address ;
		outRanges[*pIndex].length	= inNode->range.length ;
		(*pIndex)++ ;
		
		// add ranges to the right of us
		GetCoalesceList(outRanges, inNode->right, pIndex) ;
	}
}

// ============================================================
// IOFireWireLibIsochPortImp
// ============================================================

IOFireWireLibIsochPortImp::IOFireWireLibIsochPortImp(
	IOFireWireDeviceInterfaceImp&	inUserClient): IOFireWireIUnknown(),
												   mUserClient(inUserClient),
												   mKernPortRef(0)
{
	mUserClient.AddRef() ;
}

IOFireWireLibIsochPortImp::~IOFireWireLibIsochPortImp()
{
	mUserClient.Release() ;
}

IOReturn
IOFireWireLibIsochPortImp::Init(
	Boolean		inTalking)
{
	mTalking = inTalking ;

	FWIsochPortAllocateParams	params ;
	IOByteCount					outputSize = sizeof(FWKernIsochPortRef) ;
	
	IOReturn result = IOConnectMethodStructureIStructureO( mUserClient.GetUserClientConnection(),
														   kFWIsochPort_Allocate,
														   sizeof(FWIsochPortAllocateParams),
														   & outputSize,
														   & params,
														   & mKernPortRef) ;

	if (kIOReturnSuccess != result)
		fprintf(stderr, "IOFireWireLibIsochPortImp::Init: result=0x%08lX\n", (UInt32) result) ;
	return result ;
}

IOReturn
IOFireWireLibIsochPortImp::GetSupported(
	IOFWSpeed&					maxSpeed, 
	UInt64& 					chanSupported )
{
	return IOConnectMethodScalarIScalarO(
				mUserClient.GetUserClientConnection(),
				kFWIsochPort_GetSupported,
				1,
				3,
				mKernPortRef,
				& maxSpeed,
				(UInt32*)&chanSupported,
				(UInt32*)&chanSupported + 1) ;
}

IOReturn
IOFireWireLibIsochPortImp::AllocatePort(
	IOFWSpeed 					speed, 
	UInt32 						chan )
{
	return IOConnectMethodScalarIScalarO(
						mUserClient.GetUserClientConnection(),
						kFWIsochPort_AllocatePort,
						3,
						0,
						mKernPortRef,
						speed,
						chan) ;
}

IOReturn
IOFireWireLibIsochPortImp::ReleasePort()
{
	return IOConnectMethodScalarIScalarO(
					mUserClient.GetUserClientConnection(),
					kFWIsochPort_ReleasePort,
					1,
					0,
					mKernPortRef) ;
}

IOReturn
IOFireWireLibIsochPortImp::Start()
{
	return IOConnectMethodScalarIScalarO(
					mUserClient.GetUserClientConnection(),
					kFWIsochPort_Start,
					1,
					0,
					mKernPortRef) ;
}

IOReturn
IOFireWireLibIsochPortImp::Stop()
{
	return IOConnectMethodScalarIScalarO(
					mUserClient.GetUserClientConnection(),
					kFWIsochPort_Stop,
					1,
					0,
					mKernPortRef) ;
}

// ============================================================
//
// IOFireWireLibIsochPortCOM
//
// ============================================================

IOFireWireLibIsochPortCOM::IOFireWireLibIsochPortCOM(
	IOFireWireDeviceInterfaceImp&	inUserClient): IOFireWireLibIsochPortImp(inUserClient)
{
}

IOFireWireLibIsochPortCOM::~IOFireWireLibIsochPortCOM()
{
}

IOReturn
IOFireWireLibIsochPortCOM::SGetSupported(
	IOFireWireLibIsochPortRef	self, 
	IOFWSpeed* 					maxSpeed, 
	UInt64* 					chanSupported )
{
	return GetThis(self)->GetSupported(*maxSpeed, *chanSupported) ;
}

IOReturn
IOFireWireLibIsochPortCOM::SAllocatePort(
	IOFireWireLibIsochPortRef	self, 
	IOFWSpeed 					speed, 
	UInt32 						chan )
{
	return GetThis(self)->AllocatePort(speed, chan) ;
}

IOReturn
IOFireWireLibIsochPortCOM::SReleasePort(
	IOFireWireLibIsochPortRef	self)
{
	return GetThis(self)->ReleasePort() ;
}

IOReturn
IOFireWireLibIsochPortCOM::SStart(
	IOFireWireLibIsochPortRef	self)
{
	return GetThis(self)->Start() ;
}

IOReturn
IOFireWireLibIsochPortCOM::SStop(
	IOFireWireLibIsochPortRef	self)
{
	return GetThis(self)->Stop() ;
}


// ============================================================
//
// IOFireWireLibRemoteIsochPortImp
//
// ============================================================

IOFireWireLibRemoteIsochPortImp::IOFireWireLibRemoteIsochPortImp(
	IOFireWireDeviceInterfaceImp&	inUserClient): IOFireWireLibIsochPortCOM(inUserClient),
												   mGetSupportedHandler(0),
												   mAllocatePortHandler(0),
												   mReleasePortHandler(0),
												   mStartHandler(0),
												   mStopHandler(0),
												   mRefInterface(0)
{
}

IOReturn
IOFireWireLibRemoteIsochPortImp::GetSupported(
	IOFWSpeed&					maxSpeed,
	UInt64&						chanSupported)
{
	if (mGetSupportedHandler)
		return (*mGetSupportedHandler)(mRefInterface, & maxSpeed, & chanSupported) ;
	else
		return kIOReturnUnsupported ;	// should we return unsupported if user proc doesn't answer?
}

IOReturn
IOFireWireLibRemoteIsochPortImp::AllocatePort(
	IOFWSpeed 					speed, 
	UInt32 						chan )
{
	if (mAllocatePortHandler)
		return (*mAllocatePortHandler)(mRefInterface, speed,  chan) ;
	else
		return kIOReturnSuccess ;
}

IOReturn
IOFireWireLibRemoteIsochPortImp::ReleasePort()
{
	if (mReleasePortHandler)
		return (*mReleasePortHandler)(mRefInterface) ;
	else
		return kIOReturnSuccess ;
}

IOReturn
IOFireWireLibRemoteIsochPortImp::Start()
{
	if (mStartHandler)
		return (*mStartHandler)(mRefInterface) ;
	else
		return kIOReturnSuccess ;
}

IOReturn
IOFireWireLibRemoteIsochPortImp::Stop()
{
	if (mStopHandler)
		return (*mStopHandler)(mRefInterface) ;
	else
		return kIOReturnSuccess ;
}


IOFireWireLibIsochPortGetSupportedCallback
IOFireWireLibRemoteIsochPortImp::SetGetSupportedHandler(
	IOFireWireLibIsochPortGetSupportedCallback	inHandler)
{
	IOFireWireLibIsochPortGetSupportedCallback oldHandler = mGetSupportedHandler ;
	mGetSupportedHandler = inHandler ;
	
	return oldHandler ;
}

IOFireWireLibIsochPortAllocateCallback
IOFireWireLibRemoteIsochPortImp::SetAllocatePortHandler(
	IOFireWireLibIsochPortAllocateCallback	inHandler)
{
	IOFireWireLibIsochPortAllocateCallback	oldHandler	= mAllocatePortHandler ;
	mAllocatePortHandler = inHandler ;
	
	return oldHandler ;
}

IOFireWireLibIsochPortCallback
IOFireWireLibRemoteIsochPortImp::SetReleasePortHandler(
	IOFireWireLibIsochPortCallback	inHandler)
{
	IOFireWireLibIsochPortCallback	oldHandler	= mReleasePortHandler ;
	mReleasePortHandler = inHandler ;
	
	return oldHandler ;
}

IOFireWireLibIsochPortCallback
IOFireWireLibRemoteIsochPortImp::SetStartHandler(
	IOFireWireLibIsochPortCallback	inHandler)
{
	IOFireWireLibIsochPortCallback	oldHandler	= mStartHandler ;
	mStartHandler = inHandler ;
	
	return oldHandler ;
}

IOFireWireLibIsochPortCallback
IOFireWireLibRemoteIsochPortImp::SetStopHandler(
	IOFireWireLibIsochPortCallback	inHandler)
{
	IOFireWireLibIsochPortCallback	oldHandler	= mStopHandler ;
	mStopHandler = inHandler ;
	
	return oldHandler ;
}
							
void
IOFireWireLibIsochPortImp::SetRefCon(
	void*							inRefCon)
{
	mRefCon = inRefCon ;
}

void*
IOFireWireLibIsochPortImp::GetRefCon() const
{
	return mRefCon ;
}

Boolean
IOFireWireLibIsochPortImp::GetTalking() const
{
	return mTalking ;
}

// ============================================================
//
// IOFireWireLibRemoteIsochPortCOM
//
// ============================================================
IOFireWireLibRemoteIsochPortCOM::IOFireWireLibRemoteIsochPortCOM(
	IOFireWireDeviceInterfaceImp&	inUserClient): IOFireWireLibRemoteIsochPortImp(inUserClient)
{
	mInterface.pseudoVTable = (IUnknownVTbl*) & IOFireWireLibRemoteIsochPortCOM::sInterface ;
	mInterface.obj = this ;
	
	mRefInterface = (IOFireWireLibIsochPortRef) & mInterface.pseudoVTable ;
}
							
IOFireWireLibRemoteIsochPortCOM::~IOFireWireLibRemoteIsochPortCOM()
{
}

IUnknownVTbl**
IOFireWireLibRemoteIsochPortCOM::Alloc(
	IOFireWireDeviceInterfaceImp&	inUserClient,
	Boolean							inTalking)
{
	IOFireWireLibRemoteIsochPortCOM*	me ;
	IUnknownVTbl** interface = NULL ;
	
	me = new IOFireWireLibRemoteIsochPortCOM(inUserClient) ;
	if ( me && kIOReturnSuccess == me->Init(inTalking) )
	{
		interface = & me->mInterface.pseudoVTable ;
	}
	else
		delete me ;
		
	return interface ;
}


HRESULT
IOFireWireLibRemoteIsochPortCOM::QueryInterface(REFIID iid, void ** ppv )
{
	HRESULT		result = S_OK ;
	*ppv = nil ;

	CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;

	if ( CFEqual(interfaceID, IUnknownUUID) ||  CFEqual(interfaceID, kIOFireWireRemoteIsochPortInterfaceID) )
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

IOFireWireLibIsochPortGetSupportedCallback
IOFireWireLibRemoteIsochPortCOM::SSetGetSupportedHandler(
	IOFireWireLibRemoteIsochPortRef				self,
	IOFireWireLibIsochPortGetSupportedCallback	inHandler)
{
//	GetThis(self)->SetRefInterface(self) ;
	return GetThis(self)->SetGetSupportedHandler(inHandler) ;
}

IOFireWireLibIsochPortAllocateCallback
IOFireWireLibRemoteIsochPortCOM::SSetAllocatePortHandler(
	IOFireWireLibRemoteIsochPortRef		self,
	IOFireWireLibIsochPortAllocateCallback		inHandler)
{
//	GetThis(self)->SetRefInterface(self) ;
	return GetThis(self)->SetAllocatePortHandler(inHandler) ;
}

IOFireWireLibIsochPortCallback
IOFireWireLibRemoteIsochPortCOM::SSetReleasePortHandler(
	IOFireWireLibRemoteIsochPortRef		self,
	IOFireWireLibIsochPortCallback		inHandler)
{
//	GetThis(self)->SetRefInterface(self) ;
	return GetThis(self)->SetReleasePortHandler(inHandler) ;
}

IOFireWireLibIsochPortCallback
IOFireWireLibRemoteIsochPortCOM::SSetStartHandler(
	IOFireWireLibRemoteIsochPortRef		self,
	IOFireWireLibIsochPortCallback		inHandler)
{
//	GetThis(self)->SetRefInterface(self) ;
	return GetThis(self)->SetStartHandler(inHandler) ;
}

IOFireWireLibIsochPortCallback
IOFireWireLibRemoteIsochPortCOM::SSetStopHandler(
	IOFireWireLibRemoteIsochPortRef		self,
	IOFireWireLibIsochPortCallback		inHandler)
{
//	GetThis(self)->SetRefInterface(self) ;
	return GetThis(self)->SetStopHandler(inHandler) ;
}
							
void
IOFireWireLibIsochPortCOM::SSetRefCon(
	IOFireWireLibIsochPortRef		self,
	void*				inRefCon)
{
	GetThis(self)->SetRefCon(inRefCon) ;
}

void*
IOFireWireLibIsochPortCOM::SGetRefCon(
	IOFireWireLibIsochPortRef		self)
{
	return GetThis(self)->GetRefCon() ;
}

Boolean
IOFireWireLibIsochPortCOM::SGetTalking(
	IOFireWireLibIsochPortRef		self)
{
	return GetThis(self)->GetTalking() ;
}

// ============================================================
// IOFireWireLibLocalIsochPortImp
// ============================================================
IOFireWireLibLocalIsochPortImp::IOFireWireLibLocalIsochPortImp(
	IOFireWireDeviceInterfaceImp&	inUserClient): IOFireWireLibIsochPortCOM(inUserClient)
{
}

IOFireWireLibLocalIsochPortImp::~IOFireWireLibLocalIsochPortImp()
{
}

IOReturn
IOFireWireLibLocalIsochPortImp::Init(
	Boolean				inTalking,
	DCLCommandStruct*	inDCLProgram,
	UInt32				inStartEvent,
	UInt32				inStartState,
	UInt32				inStartMask,
	IOVirtualRange		inDCLProgramRanges[],			// optional optimization parameters
	UInt32				inDCLProgramRangeCount,
	IOVirtualRange		inBufferRanges[],
	UInt32				inBufferRangeCount)
{
	// init the easy parameters
	mTalking		= inTalking ;
	mDCLProgram		= inDCLProgram ;
	mStartEvent		= inStartEvent ;
	mStartState		= inStartState ;
	mStartMask		= inStartMask ;

	// trees used to coalesce program/data ranges
	IOFireWireLibCoalesceTree		programTree ;
	IOFireWireLibCoalesceTree		bufferTree ;
	
	// check if user passed in any virtual memory ranges to start with...
	//zzz maybe we should try to validate that the ranges are actually in
	//zzz client memory?
	if (inDCLProgramRanges)
		for(UInt32 index=0; index < inDCLProgramRangeCount; index++)
			programTree.CoalesceRange(inDCLProgramRanges[index]) ;

	if (inBufferRanges)
		for(UInt32 index=0; index < inBufferRangeCount; index++)
			bufferTree.CoalesceRange(inBufferRanges[index]) ;

	// point to beginning of DCL program
	DCLCommandStruct*			pCurrentDCLStruct = mDCLProgram ;
	IOVirtualRange				tempRange ;
	FWLocalIsochPortAllocateParams	params ;

	params.userDCLProgramDCLCount = 0 ;
	while (pCurrentDCLStruct)
	{
		params.userDCLProgramDCLCount++ ;
	
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

	IOReturn 					result 				= kIOReturnSuccess ;
	
	// allocate lists to store coalesced ranges
	if ( nil == (programRanges = new IOVirtualRange[programRangeCount] ))
		result = kIOReturnNoMemory ;
		
	if ( kIOReturnSuccess == result )
		if (nil == (bufferRanges = new IOVirtualRange[bufferRangeCount] ))
			result = kIOReturnNoMemory ;

	// get trees' coalesced buffer lists into the buffers
	if (kIOReturnSuccess == result)
	{
		programTree.GetCoalesceList(programRanges) ;
		bufferTree.GetCoalesceList(bufferRanges) ;

//		fprintf(stderr, "programRanges: ") ;
//		for(UInt32 index=0; index < programRangeCount; index++)
//		{
//			fprintf(stderr, "[0x%08lX, %u] ", programRanges[index].address, programRanges[index].length) ;
//		}
//		fprintf(stderr, "\n") ;

//		fprintf(stderr, "bufferRanges: ") ;
//		for(UInt32 index=0; index < bufferRangeCount; index++)
//		{
//			fprintf(stderr, "[0x%08lX, %u] ", bufferRanges[index].address, bufferRanges[index].length) ;
//		}
//		fprintf(stderr, "\n") ;
	}
	
	// fill out param struct and submit to kernel
	if ( kIOReturnSuccess == result )
	{
		params.userDCLProgram				= mDCLProgram ;
	
		params.userDCLProgramRanges 		= programRanges ;
		params.userDCLProgramRangeCount		= programRangeCount ;
		params.userDCLBufferRanges			= bufferRanges ;
		params.userDCLBufferRangeCount		= bufferRangeCount ;
		
		params.talking						= mTalking ;
	
		params.startEvent					= mStartEvent ;
		params.startState					= mStartState ;
		params.startMask					= mStartMask ;
		
		IOByteCount	outputSize = sizeof(FWKernIsochPortRef) ;
		result = IOConnectMethodStructureIStructureO(
						mUserClient.GetUserClientConnection(),
						kFWLocalIsochPort_Allocate,
						sizeof(params),
						& outputSize,
						& params,
						& mKernPortRef) ;

//		PrintDCLProgram(params.userDCLProgram, params.userDCLProgramDCLCount) ;
		
	}
	
	if (programRanges)
		delete[] programRanges ;
	if (bufferRanges)
		delete[] bufferRanges ;
	
	if (kIOReturnSuccess == result)
	{
		mach_msg_type_number_t 	outputSize = 0 ;
		io_scalar_inband_t		params ;
		
		params[0]	= (UInt32) mKernPortRef ;
		params[1]	= (UInt32) & IOFireWireLibLocalIsochPortImp::DCLCallProcHandler ;
		
		result = io_async_method_scalarI_scalarO( mUserClient.GetUserClientConnection(),
												  mUserClient.GetIsochAsyncPort(),
												  mAsyncRef,
												  1,
												  kFWSetAsyncRef_DCLCallProc,
												  params,
												  2,
												  NULL,
												  & outputSize) ;
	}											  
												
	return result ;
}

IOReturn
IOFireWireLibLocalIsochPortImp::ModifyJumpDCL(
	DCLJumpStruct* 						inJump, 
	DCLLabelStruct* 					inLabel)
{
	inJump->pJumpDCLLabel = inLabel ;

	IOReturn result = IOConnectMethodScalarIScalarO(
				mUserClient.GetUserClientConnection(),
				kFWLocalIsochPort_ModifyJumpDCL,
				3,
				0,
				mKernPortRef, inJump->compilerData, inLabel->compilerData) ;
	
	return result ;
}

void
IOFireWireLibLocalIsochPortImp::DCLCallProcHandler(
	void*				inRefCon,
	IOReturn			result)
{
//	fprintf(stderr, "+ IOFireWireLibLocalIsochPortImp::DCLCallProcHandler\n") ;
	
	DCLCallProcStruct*	callProcDCL = (DCLCallProcStruct*)inRefCon ;
	
	(*callProcDCL->proc)((DCLCommandStruct*)inRefCon) ;
}

void
IOFireWireLibLocalIsochPortImp::PrintDCLProgram(
	const DCLCommandStruct*		inDCL,
	UInt32						inDCLCount) 
{
	mUserClient.PrintDCLProgram(inDCL, inDCLCount) ;
}

// ============================================================
// IOFireWireLibLocalIsochPortCOM
// ============================================================

IOFireWireLibLocalIsochPortCOM::IOFireWireLibLocalIsochPortCOM(
	IOFireWireDeviceInterfaceImp&	inUserClient): IOFireWireLibLocalIsochPortImp(inUserClient)
{
	mInterface.pseudoVTable = (IUnknownVTbl*) & IOFireWireLibLocalIsochPortCOM::sInterface ;
	mInterface.obj = this ;
}

IOFireWireLibLocalIsochPortCOM::~IOFireWireLibLocalIsochPortCOM()
{
}

IUnknownVTbl**
IOFireWireLibLocalIsochPortCOM::Alloc(
	IOFireWireDeviceInterfaceImp&	inUserClient, 
	Boolean							inTalking,
	DCLCommandStruct*				inDCLProgram,
	UInt32							inStartEvent,
	UInt32							inStartState,
	UInt32							inStartMask,
	IOVirtualRange					inDCLProgramRanges[],			// optional optimization parameters
	UInt32							inDCLProgramRangeCount,
	IOVirtualRange					inBufferRanges[],
	UInt32							inBufferRangeCount)
{
	IOFireWireLibLocalIsochPortCOM*	me ;
	IUnknownVTbl** interface = NULL ;
	
	me = new IOFireWireLibLocalIsochPortCOM(inUserClient) ;
	if ( me && (kIOReturnSuccess == me->Init(inTalking,
											 inDCLProgram,
											 inStartEvent,
											 inStartState,
											 inStartMask,
											 inDCLProgramRanges,
											 inDCLProgramRangeCount,
											 inBufferRanges,
											 inBufferRangeCount)) )
	{
		interface = & me->mInterface.pseudoVTable ;
	}
	else
		delete me ;
		
	return interface ;
}

HRESULT
IOFireWireLibLocalIsochPortCOM::QueryInterface(REFIID iid, void ** ppv )
{
	HRESULT		result = S_OK ;
	*ppv = nil ;

	CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;

	if ( CFEqual(interfaceID, IUnknownUUID) ||  CFEqual(interfaceID, kIOFireWireLocalIsochPortInterfaceID) )
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

IOReturn
IOFireWireLibLocalIsochPortCOM::SModifyJumpDCL(
	IOFireWireLibLocalIsochPortRef 	self, 
	DCLJumpStruct* 					inJump, 
	DCLLabelStruct* 				inLabel)
{
	return GetThis(self)->ModifyJumpDCL(inJump, inLabel) ;
}

// --- utility functions ----------
void
IOFireWireLibLocalIsochPortCOM::SPrintDCLProgram(
	IOFireWireLibLocalIsochPortRef 	self, 
	const DCLCommandStruct*			inProgram,
	UInt32							inLength)
{
	GetThis(self)->PrintDCLProgram(inProgram, inLength) ;
}

