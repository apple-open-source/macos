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
 *  IOFireWireLibDCLPool.h
 *  IOFireWireFamily
 *
 *  Created on Mon Mar 12 2001.
 *  Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
 *
 */

#import "IOFireWireLibPriv.h"
#import "IOFireWireLibIsoch.h"
#import "IOFWIsoch.h"

namespace IOFireWireLib {

	class DCLCommandPool: public IOFireWireIUnknown
	{
		public:
										DCLCommandPool( IUnknownVTbl* interface, Device& inUserClient, IOByteCount inSize ) ;
			virtual						~DCLCommandPool() ;
			Boolean				Init() 
												{ return true; }
			
			DCLCommandStruct*	Allocate(
												IOByteCount 			inSize ) ;
			IOReturn			AllocateWithOpcode(
												DCLCommandStruct* 		inDCL, 
												DCLCommandStruct** 		outDCL, 
												UInt32 					opcode, ... ) ;
			DCLCommandStruct*	AllocateTransferPacketDCL(
												DCLCommandStruct*		inDCL,
												UInt32					inOpcode,
												void*					inBuffer,
												IOByteCount				inSize) ;
			DCLCommandStruct*	AllocateTransferBufferDCL(
												DCLCommandStruct* 		inDCL, 
												UInt32 					inOpcode, 
												void* 					inBuffer, 
												IOByteCount 			inSize, 
												IOByteCount 			inPacketSize, 
												UInt32 					inBufferOffset) ;
			DCLCommandStruct*	AllocateSendPacketStartDCL(
												DCLCommandStruct* 		inDCL, 
												void*					inBuffer,
												IOByteCount				inSize) ;
			DCLCommandStruct*	AllocateSendPacketWithHeaderStartDCL(
												DCLCommandStruct* 		inDCL, 
												void*					inBuffer,
												IOByteCount				inSize) ;
			DCLCommandStruct*	AllocateSendBufferDCL(		// currently does nothing
												DCLCommandStruct* 		inDCL, 
												void*					inBuffer,
												IOByteCount				inSize,
												IOByteCount				inPacketSize,
												UInt32					inBufferOffset) ;
			DCLCommandStruct*	AllocateSendPacketDCL(
												DCLCommandStruct* 		inDCL,
												void*					inBuffer,
												IOByteCount				inSize) ;
			DCLCommandStruct*	AllocateReceivePacketStartDCL(
												DCLCommandStruct* 		inDCL, 
												void*					inBuffer,
												IOByteCount				inSize) ;
			DCLCommandStruct*	AllocateReceivePacketDCL(
												DCLCommandStruct* 		inDCL,
												void*					inBuffer,
												IOByteCount				inSize) ;
			DCLCommandStruct*	AllocateReceiveBufferDCL(	// currently does nothing
												DCLCommandStruct* 		inDCL, 
												void*					inBuffer,
												IOByteCount				inSize,
												IOByteCount				inPacketSize,
												UInt32					inBufferOffset) ;
			DCLCommandStruct*	AllocateCallProcDCL(
												DCLCommandStruct* 		inDCL, 
												DCLCallCommandProcPtr	inProc,
												UInt32					inProcData) ;
			DCLCommandStruct*	AllocateLabelDCL(
												DCLCommandStruct* 		inDCL) ;
			DCLCommandStruct*	AllocateJumpDCL(
												DCLCommandStruct* 		inDCL, 
												DCLLabelPtr				pInJumpDCLLabel) ;
			DCLCommandStruct*	AllocateSetTagSyncBitsDCL(
												DCLCommandStruct* 		inDCL, 
												UInt16					inTagBits,
												UInt16					inSyncBits) ;
			DCLCommandStruct*	AllocateUpdateDCLListDCL(
												DCLCommandStruct* 		inDCL, 
												DCLCommandPtr*			inDCLCommandList,
												UInt32					inNumCommands) ;
			DCLCommandStruct*	AllocatePtrTimeStampDCL(
												DCLCommandStruct* 		inDCL, 
												UInt32*					inTimeStampPtr) ;
		
			void 				Free(
												DCLCommandStruct*		inDCL ) ;
			IOByteCount			GetSize() ;
			Boolean				SetSize(
												IOByteCount 					inSize ) ;
			IOByteCount			GetBytesRemaining() ;

		protected:
			void				Lock() ;
			void				Unlock() ;
			void				CoalesceFreeBlocks() ;

		protected:
			Device&				mUserClient ;
			CFMutableArrayRef	mFreeBlocks ;
			CFMutableArrayRef	mFreeBlockSizes ;
			CFMutableArrayRef	mAllocatedBlocks ;
			CFMutableArrayRef	mAllocatedBlockSizes ;	
			UInt8*				mStorage ;
			IOByteCount			mStorageSize ;
			IOByteCount			mBytesRemaining ;
			pthread_mutex_t		mMutex ;
		
	} ;
	
	class DCLCommandPoolCOM: public DCLCommandPool
	{
		typedef IOFireWireLibDCLCommandPoolRef 		Ref ;
		typedef IOFireWireDCLCommandPoolInterface	Interface ;

		public:
			DCLCommandPoolCOM( Device& inUserClient, IOByteCount inSize ) ;
			virtual ~DCLCommandPoolCOM() ;
		
			//
			// --- COM ---------------
			//		
			static Interface			sInterface ;
		
			//
			// --- IUNKNOWN support ----------------
			//
			static IUnknownVTbl**		Alloc(
												Device&	inUserClient, 
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
}