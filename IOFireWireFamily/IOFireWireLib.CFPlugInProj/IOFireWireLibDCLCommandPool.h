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
 *  IOFireWireLibDCLPool.h
 *  IOFireWireFamily
 *
 *  Created by NWG on Mon Mar 12 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef __IOFireWireLibDCLCommandPool_H__
#define __IOFireWireLibDCLCommandPool_H__

#include "IOFireWireLibPriv.h"
#include <IOKit/firewire/IOFireWireLibIsoch.h>
#include <IOKit/firewire/IOFWIsoch.h>

class IOFireWireLibDCLCommandPoolImp: public IOFireWireIUnknown
{
 public:
								IOFireWireLibDCLCommandPoolImp(
										IOFireWireDeviceInterfaceImp& 	inUserClient,
										IOByteCount						inSize) ;
	virtual						~IOFireWireLibDCLCommandPoolImp() ;
	virtual Boolean				Init() 
										{ return true; }
	
	virtual DCLCommandStruct*	Allocate(
										IOByteCount 			inSize ) ;
	virtual IOReturn			AllocateWithOpcode(
										DCLCommandStruct* 		inDCL, 
										DCLCommandStruct** 		outDCL, 
										UInt32 					opcode, ... ) ;
	virtual DCLCommandStruct*	AllocateTransferPacketDCL(
										DCLCommandStruct*		inDCL,
										UInt32					inOpcode,
										void*					inBuffer,
										IOByteCount				inSize) ;
	virtual DCLCommandStruct*	AllocateTransferBufferDCL(
										DCLCommandStruct* 		inDCL, 
										UInt32 					inOpcode, 
										void* 					inBuffer, 
										IOByteCount 			inSize, 
										IOByteCount 			inPacketSize, 
										UInt32 					inBufferOffset) ;
	virtual DCLCommandStruct*	AllocateSendPacketStartDCL(
										DCLCommandStruct* 		inDCL, 
										void*					inBuffer,
										IOByteCount				inSize) ;
	virtual DCLCommandStruct*	AllocateSendPacketWithHeaderStartDCL(
										DCLCommandStruct* 		inDCL, 
										void*					inBuffer,
										IOByteCount				inSize) ;
	virtual DCLCommandStruct*	AllocateSendBufferDCL(		// currently does nothing
										DCLCommandStruct* 		inDCL, 
										void*					inBuffer,
										IOByteCount				inSize,
										IOByteCount				inPacketSize,
										UInt32					inBufferOffset) ;
	virtual DCLCommandStruct*	AllocateSendPacketDCL(
										DCLCommandStruct* 		inDCL,
										void*					inBuffer,
										IOByteCount				inSize) ;
	virtual DCLCommandStruct*	AllocateReceivePacketStartDCL(
										DCLCommandStruct* 		inDCL, 
										void*					inBuffer,
										IOByteCount				inSize) ;
	virtual DCLCommandStruct*	AllocateReceivePacketDCL(
										DCLCommandStruct* 		inDCL,
										void*					inBuffer,
										IOByteCount				inSize) ;
	virtual DCLCommandStruct*	AllocateReceiveBufferDCL(	// currently does nothing
										DCLCommandStruct* 		inDCL, 
										void*					inBuffer,
										IOByteCount				inSize,
										IOByteCount				inPacketSize,
										UInt32					inBufferOffset) ;
	virtual	DCLCommandStruct*	AllocateCallProcDCL(
										DCLCommandStruct* 		inDCL, 
										DCLCallCommandProcPtr	inProc,
										UInt32					inProcData) ;
	virtual DCLCommandStruct*	AllocateLabelDCL(
										DCLCommandStruct* 		inDCL) ;
	virtual DCLCommandStruct*	AllocateJumpDCL(
										DCLCommandStruct* 		inDCL, 
										DCLLabelPtr				pInJumpDCLLabel) ;
	virtual DCLCommandStruct*	AllocateSetTagSyncBitsDCL(
										DCLCommandStruct* 		inDCL, 
										UInt16					inTagBits,
										UInt16					inSyncBits) ;
	virtual DCLCommandStruct*	AllocateUpdateDCLListDCL(
										DCLCommandStruct* 		inDCL, 
										DCLCommandPtr*			inDCLCommandList,
										UInt32					inNumCommands) ;
	virtual DCLCommandStruct*	AllocatePtrTimeStampDCL(
										DCLCommandStruct* 		inDCL, 
										UInt32*					inTimeStampPtr) ;

	virtual void 				Free(
										DCLCommandStruct*		inDCL ) ;
	virtual IOByteCount			GetSize() ;
	virtual Boolean				SetSize(
										IOByteCount 					inSize ) ;
	virtual IOByteCount			GetBytesRemaining() ;

 protected:
	IOFireWireDeviceInterfaceImp&	mUserClient ;
 
	CFMutableArrayRef	mFreeBlocks ;
	CFMutableArrayRef	mFreeBlockSizes ;
	CFMutableArrayRef	mAllocatedBlocks ;
	CFMutableArrayRef	mAllocatedBlockSizes ;	
	UInt8*				mStorage ;
	IOByteCount			mStorageSize ;
	IOByteCount			mBytesRemaining ;
 
	virtual void				Lock() ;
	virtual void				Unlock() ;
	virtual void				CoalesceFreeBlocks() ;
} ;

class IOFireWireLibDCLCommandPoolCOM: public IOFireWireLibDCLCommandPoolImp
{
 public:
	//
	// --- ctor/dtor
	//
							IOFireWireLibDCLCommandPoolCOM(
									IOFireWireDeviceInterfaceImp&	inUserClient, 
									IOByteCount						inSize) ;
	virtual 				~IOFireWireLibDCLCommandPoolCOM() ;

	//
	// --- COM ---------------
	//
 	struct InterfaceMap
 	{
 		IUnknownVTbl*					pseudoVTable ;
 		IOFireWireLibDCLCommandPoolCOM*	obj ;
 	} ;
 
	static IOFireWireDCLCommandPoolInterface	sInterface ;
 	InterfaceMap								mInterface ;

	//
 	// GetThis()
	//
 	inline static IOFireWireLibDCLCommandPoolCOM* GetThis(IOFireWireLibDCLCommandPoolRef self)
	 	{ return ((InterfaceMap*)self)->obj ;}

	//
	// --- IUNKNOWN support ----------------
	//
	static IUnknownVTbl**		Alloc(
										IOFireWireDeviceInterfaceImp&	inUserClient, 
										IOByteCount						inSize) ;
	virtual HRESULT				QueryInterface(REFIID iid, void ** ppv ) ;

	//
	// --- static methods ------------------
	//
	static DCLCommandStruct*	SAllocate(
										IOFireWireLibDCLCommandPoolRef	self, 
										IOByteCount 					inSize ) ;
	static IOReturn				SAllocateWithOpcode(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct*				inDCL,
										DCLCommandStruct**				outDCL,
										UInt32			 				opcode, ... ) ;
	static DCLCommandStruct*	SAllocateTransferPacketDCL(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct*				inDCL,
										UInt32							inOpcode,
										void*							inBuffer,
										IOByteCount						inSize) ;
	static DCLCommandStruct*	SAllocateTransferBufferDCL(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct* 				inDCL, 
										UInt32 							inOpcode, 
										void* 							inBuffer, 
										IOByteCount 					inSize, 
										IOByteCount 					inPacketSize, 
										UInt32 							inBufferOffset) ;
	static DCLCommandStruct*	SAllocateSendPacketStartDCL(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct* 				inDCL, 
										void*							inBuffer,
										IOByteCount						inSize) ;
	static DCLCommandStruct*	SAllocateSendPacketWithHeaderStartDCL(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct* 				inDCL, 
										void*							inBuffer,
										IOByteCount						inSize) ;
	static DCLCommandStruct*	SAllocateSendBufferDCL(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct* 				inDCL, 
										void*							inBuffer,
										IOByteCount						inSize,
										IOByteCount						inPacketSize,
										UInt32							inBufferOffset) ;
	static DCLCommandStruct*	SAllocateSendPacketDCL(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct* 				inDCL,
										void*							inBuffer,
										IOByteCount						inSize) ;
	static DCLCommandStruct*	SAllocateReceivePacketStartDCL(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct* 				inDCL, 
										void*							inBuffer,
										IOByteCount						inSize) ;
	static DCLCommandStruct*	SAllocateReceivePacketDCL(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct* 				inDCL,
										void*							inBuffer,
										IOByteCount						inSize) ;
	static DCLCommandStruct*	SAllocateReceiveBufferDCL(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct* 				inDCL, 
										void*							inBuffer,
										IOByteCount						inSize,
										IOByteCount						inPacketSize,
										UInt32							inBufferOffset) ;
	static	DCLCommandStruct*	SAllocateCallProcDCL(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct* 				inDCL, 
										DCLCallCommandProcPtr			inProc,
										UInt32							inProcData) ;
	static DCLCommandStruct*	SAllocateLabelDCL(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct* 				inDCL) ;
	static DCLCommandStruct*	SAllocateJumpDCL(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct* 				inDCL, 
										DCLLabelPtr						pInJumpDCLLabel) ;
	static DCLCommandStruct*	SAllocateSetTagSyncBitsDCL(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct* 				inDCL, 
										UInt16							inTagBits,
										UInt16							inSyncBits) ;
	static DCLCommandStruct*	SAllocateUpdateDCLListDCL(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct* 				inDCL, 
										DCLCommandPtr*					inDCLCommandList,
										UInt32							inNumCommands) ;
	static DCLCommandStruct*	SAllocatePtrTimeStampDCL(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct* 				inDCL, 
										UInt32*							inTimeStampPtr) ;
	static void 				SFree(
										IOFireWireLibDCLCommandPoolRef 	self, 
										DCLCommandStruct* 				inDCL ) ;
	static IOByteCount			SGetSize(
										IOFireWireLibDCLCommandPoolRef 	self ) ;
	static Boolean				SSetSize(
										IOFireWireLibDCLCommandPoolRef 	self, 
										IOByteCount 					inSize ) ;
	static IOByteCount			SGetBytesRemaining(
										IOFireWireLibDCLCommandPoolRef 	self ) ;
} ;

#endif //__IOFireWireLibDCLCommandPool_H__
