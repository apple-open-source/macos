/*
 *  IOFireWireLibDCLCommandPool.cpp
 *  IOFireWireFamily
 *
 *  Created by NWG on Mon Mar 12 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */
/*
	$Log: IOFireWireLibDCLCommandPool.cpp,v $
	Revision 1.8  2002/08/26 20:08:34  niels
	fix user space hang (when devices are unplugged and a DCL program is running)
	
*/
#import "IOFireWireLibDCLCommandPool.h"
#import <pthread.h>

namespace IOFireWireLib {
	
	DCLCommandPoolCOM::Interface	DCLCommandPoolCOM::sInterface = {
		INTERFACEIMP_INTERFACE,
		1, 0, //vers, rev
	
		DCLCommandPoolCOM::SAllocate,
		&DCLCommandPoolCOM::SAllocateWithOpcode,
		
		&DCLCommandPoolCOM::SAllocateTransferPacketDCL,
		&DCLCommandPoolCOM::SAllocateTransferBufferDCL,
	
		&DCLCommandPoolCOM::SAllocateSendPacketStartDCL,
		&DCLCommandPoolCOM::SAllocateSendPacketWithHeaderStartDCL,
		&DCLCommandPoolCOM::SAllocateSendBufferDCL,
		&DCLCommandPoolCOM::SAllocateSendPacketDCL,
	
		&DCLCommandPoolCOM::SAllocateReceivePacketStartDCL,
		&DCLCommandPoolCOM::SAllocateReceivePacketDCL,
		&DCLCommandPoolCOM::SAllocateReceiveBufferDCL,
	
		&DCLCommandPoolCOM::SAllocateCallProcDCL,
		&DCLCommandPoolCOM::SAllocateLabelDCL,
		&DCLCommandPoolCOM::SAllocateJumpDCL,
		&DCLCommandPoolCOM::SAllocateSetTagSyncBitsDCL,
		&DCLCommandPoolCOM::SAllocateUpdateDCLListDCL,
		&DCLCommandPoolCOM::SAllocatePtrTimeStampDCL,
	
		&DCLCommandPoolCOM::SFree,
		
		&DCLCommandPoolCOM::SGetSize,
		&DCLCommandPoolCOM::SSetSize,
		&DCLCommandPoolCOM::SGetBytesRemaining
	} ;
	
	DCLCommandPool::DCLCommandPool( IUnknownVTbl* interface, Device& inUserClient, IOByteCount inSize )
	: IOFireWireIUnknown( interface ),
	mUserClient(inUserClient)
	{
		mUserClient.AddRef() ;
	
		mFreeBlocks				= CFArrayCreateMutable(kCFAllocatorDefault, 0, nil) ;
		mFreeBlockSizes			= CFArrayCreateMutable(kCFAllocatorDefault, 0, nil) ;
		mAllocatedBlocks		= CFArrayCreateMutable(kCFAllocatorDefault, 0, nil) ;
		mAllocatedBlockSizes	= CFArrayCreateMutable(kCFAllocatorDefault, 0, nil) ;
	
		mStorage = new UInt8[inSize] ;
	
		if(mStorage)
		{
			mBytesRemaining = inSize ;
	
			CFArrayAppendValue(mFreeBlocks, mStorage) ;
			CFArrayAppendValue(mFreeBlockSizes, (const void*) inSize) ;
		}
		
		pthread_mutex_init( & mMutex, nil ) ;
	}
		
	DCLCommandPool::~DCLCommandPool()
	{
		Lock() ;
	
		if (mStorage)
		{
			delete[] mStorage ;
			mStorage = nil ;
		}
		
		if (mFreeBlocks)
			CFRelease(mFreeBlocks) ;
		if (mFreeBlockSizes)
			CFRelease(mFreeBlockSizes) ;
		if (mAllocatedBlocks)
			CFRelease(mAllocatedBlocks) ;
		if (mAllocatedBlockSizes)
			CFRelease(mAllocatedBlockSizes) ;
	
		Unlock() ;
		
		pthread_mutex_destroy( & mMutex ) ;
	
		mUserClient.Release() ;
	}
	
	DCLCommandStruct*
	DCLCommandPool::Allocate(
		IOByteCount 					inSize )
	{
		UInt32 				blockSize ;
		UInt32 				remainder ;
		UInt32 				index			= 0 ;
		const UInt8*		foundFreeBlock	= 0 ;
		DCLCommandStruct*	allocatedBlock	= nil ;
		UInt32				freeBlockCount	= CFArrayGetCount(mFreeBlocks) ;
	
		Lock() ;
		
		do
		{
			blockSize	= (UInt32) CFArrayGetValueAtIndex(mFreeBlockSizes, index) ;
			remainder	= blockSize - inSize ;
	
			if ( blockSize >= inSize )
			{
				// found a free block w/ enough space,
				// use it to allocate. we allocate from the end of the free block, not the beginning
				foundFreeBlock	= (const UInt8*) CFArrayGetValueAtIndex(mFreeBlocks, index) ;
				allocatedBlock	= (DCLCommandStruct*) (foundFreeBlock + remainder) ;
				
				CFArrayAppendValue(mAllocatedBlockSizes, (const void*) inSize) ;
				CFArrayAppendValue(mAllocatedBlocks, allocatedBlock ) ;
	
				if (remainder > 0)
				{
					//
					// if we didn't use up all of this free block,
					// resize the block to reflect the new size
	//				CFArraySetValueAtIndex(mFreeBlocks, index, (UInt32) CFArrayGetValueAtIndex(mFreeBlocks, index)) ;
					CFArraySetValueAtIndex(mFreeBlockSizes, index, (const void*) remainder) ;
				}
				else
				{
					//
					// otherwise remove the block from the free list
					CFArrayRemoveValueAtIndex(mFreeBlocks, index) ;
					CFArrayRemoveValueAtIndex(mFreeBlockSizes, index) ;
				}
				
				// update reminaing size to reflect successful allocation
				mBytesRemaining -= inSize ;
			}
		} while( (++index < freeBlockCount) && (foundFreeBlock == nil) ) ;
		
		Unlock() ;
		
		return allocatedBlock ;
	}
	
	IOReturn
	DCLCommandPool::AllocateWithOpcode(
		DCLCommandStruct* 		inDCL, 
		DCLCommandStruct** 		outDCL, 
		UInt32 					opcode, ... )
	{
		return kIOReturnUnsupported ;
	}
	
	DCLCommandStruct*
	DCLCommandPool::AllocateTransferPacketDCL(
		DCLCommandStruct*		inDCL,
		UInt32					inOpcode,
		void*					inBuffer,
		IOByteCount				inSize)
	{
		DCLTransferPacketStruct*	newDCL = (DCLTransferPacketStruct*) Allocate( sizeof(DCLTransferPacketStruct) ) ;
	
		if (!newDCL)
		{
	//		mStatus = kIOReturnNoMemory ;
			return nil ;
		}
		
		newDCL->pNextDCLCommand	= nil ;
		newDCL->compilerData	= 0 ;
		newDCL->opcode 			= inOpcode ;
		newDCL->buffer 			= inBuffer ;
		newDCL->size 			= inSize ;	
		
		if (inDCL)
			inDCL->pNextDCLCommand = (DCLCommandStruct*) newDCL ;
	
		return (DCLCommandStruct*) newDCL ;
	}
	
	DCLCommandStruct*
	DCLCommandPool::AllocateTransferBufferDCL(
		DCLCommandStruct*		inDCL, 
		UInt32 					inOpcode, 
		void* 					inBuffer, 
		IOByteCount 			inSize, 
		IOByteCount 			inPacketSize, 
		UInt32 					inBufferOffset)
	{
		DCLTransferBufferStruct*	newDCL = (DCLTransferBufferStruct*) Allocate( sizeof(DCLTransferBufferStruct) ) ;
	
		if (!newDCL)
			return nil ;
		
		newDCL->pNextDCLCommand	= nil ;
		newDCL->compilerData	= 0 ;
		newDCL->opcode 			= inOpcode ;
		newDCL->buffer 			= inBuffer ;
		newDCL->size 			= inSize ;	
		newDCL->packetSize		= inPacketSize ;
		newDCL->bufferOffset	= inBufferOffset ;
		
		if (inDCL)
			inDCL->pNextDCLCommand = (DCLCommandStruct*) newDCL ;
	
		return (DCLCommandStruct*) newDCL ;
	}
	
	DCLCommandStruct*
	DCLCommandPool::AllocateSendPacketStartDCL(
		DCLCommandStruct* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return AllocateTransferPacketDCL(inDCL, kDCLSendPacketStartOp, inBuffer, inSize) ;
	}
	
	DCLCommandStruct*
	DCLCommandPool::AllocateSendPacketWithHeaderStartDCL(
		DCLCommandStruct* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return AllocateTransferPacketDCL(inDCL, kDCLSendPacketWithHeaderStartOp, inBuffer, inSize) ;
	}
	
	DCLCommandStruct*
	DCLCommandPool::AllocateSendBufferDCL(	// not implemented
		DCLCommandStruct* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize,
		IOByteCount				inPacketSize,
		UInt32					inBufferOffset)
	{
		return nil ;
	}
	
	DCLCommandStruct*
	DCLCommandPool::AllocateSendPacketDCL(
		DCLCommandStruct* 		inDCL,
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return AllocateTransferPacketDCL(inDCL, kDCLSendPacketOp, inBuffer, inSize) ;
	}
	
	DCLCommandStruct*
	DCLCommandPool::AllocateReceivePacketStartDCL(
		DCLCommandStruct* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return AllocateTransferPacketDCL(inDCL, kDCLReceivePacketStartOp, inBuffer, inSize) ;
	}
	
	DCLCommandStruct*
	DCLCommandPool::AllocateReceivePacketDCL(
		DCLCommandStruct* 		inDCL,
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return AllocateTransferPacketDCL(inDCL, kDCLReceivePacketOp, inBuffer, inSize) ;
	}
	
	DCLCommandStruct*
	DCLCommandPool::AllocateReceiveBufferDCL( // not implemented
		DCLCommandStruct* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize,
		IOByteCount				inPacketSize,
		UInt32					inBufferOffset)
	{
		return nil ;
	}
	
	DCLCommandStruct*
	DCLCommandPool::AllocateCallProcDCL(
		DCLCommandStruct* 		inDCL, 
		DCLCallCommandProcPtr	inProc,
		UInt32					inProcData)
	{
		DCLCallProcStruct*	newDCL = (DCLCallProcStruct*) Allocate(sizeof(DCLCallProcStruct)) ;
		
		if (!newDCL)
			return nil ;
		
		newDCL->pNextDCLCommand	= nil ;
		newDCL->opcode 			= kDCLCallProcOp ;
		newDCL->proc			= inProc ;
		newDCL->procData		= inProcData ;
		
		if (inDCL)
			inDCL->pNextDCLCommand = (DCLCommandStruct*) newDCL ;
		
		return (DCLCommandStruct*) newDCL ;
	}
	
	DCLCommandStruct*
	DCLCommandPool::AllocateLabelDCL(
		DCLCommandStruct* 		inDCL)
	{
		DCLLabelStruct*	newDCL = (DCLLabelStruct*) Allocate(sizeof(DCLLabelStruct)) ;
		
		if (newDCL)
		{
			newDCL->pNextDCLCommand = nil ;
			newDCL->opcode			= kDCLLabelOp ;
			
			if (inDCL)
				inDCL->pNextDCLCommand = (DCLCommandStruct*) newDCL ;
		}
		
		return (DCLCommandStruct*) newDCL ;
	}
	
	DCLCommandStruct*
	DCLCommandPool::AllocateJumpDCL(
		DCLCommandStruct* 		inDCL, 
		DCLLabelPtr				pInJumpDCLLabel)
	{
		DCLJumpStruct*	newDCL = (DCLJumpStruct*) Allocate( sizeof(DCLJumpStruct)) ;
	
		if (newDCL)
		{
			newDCL->pNextDCLCommand = nil ;
			newDCL->opcode			= kDCLJumpOp ;
			newDCL->pJumpDCLLabel	= pInJumpDCLLabel ;
			
			if (inDCL)
				inDCL->pNextDCLCommand = (DCLCommandStruct*) newDCL ;
		}
		
		return (DCLCommandStruct*) newDCL ;
	}
	
	DCLCommandStruct*
	DCLCommandPool::AllocateSetTagSyncBitsDCL(
		DCLCommandStruct* 		inDCL, 
		UInt16					inTagBits,
		UInt16					inSyncBits)
	{
		DCLSetTagSyncBitsStruct*	newDCL = (DCLSetTagSyncBitsStruct*) Allocate(sizeof(DCLSetTagSyncBitsStruct)) ;
		
		if (newDCL)
		{
			newDCL->pNextDCLCommand = nil ;
			newDCL->opcode			= kDCLSetTagSyncBitsOp ;
			newDCL->tagBits			= inTagBits ;
			newDCL->syncBits		= inSyncBits ;
			
			if (inDCL)
				inDCL->pNextDCLCommand = (DCLCommandStruct*) newDCL ;
		}
		
		return (DCLCommandStruct*) newDCL ;
	}
	
	DCLCommandStruct*
	DCLCommandPool::AllocateUpdateDCLListDCL(
		DCLCommandStruct* 		inDCL, 
		DCLCommandPtr*			inDCLCommandList,
		UInt32					inNumDCLCommands)
	{
		DCLUpdateDCLListStruct*	newDCL = (DCLUpdateDCLListStruct*) Allocate(sizeof(DCLUpdateDCLListStruct)) ;
		
		if (newDCL)
		{
			newDCL->pNextDCLCommand	= nil ;
			newDCL->opcode 			= kDCLUpdateDCLListOp ;
			newDCL->dclCommandList	= inDCLCommandList ;
			newDCL->numDCLCommands	= inNumDCLCommands ;
			
			if (inDCL)
				inDCL->pNextDCLCommand = (DCLCommandStruct*) newDCL ;
		}
		
		return (DCLCommandStruct*) newDCL ;
	}
	
	DCLCommandStruct*
	DCLCommandPool::AllocatePtrTimeStampDCL(
		DCLCommandStruct* 		inDCL, 
		UInt32*					inTimeStampPtr)
	{
		DCLPtrTimeStampStruct*	newDCL = (DCLPtrTimeStampStruct*) Allocate(sizeof(DCLPtrTimeStampStruct)) ;
		
		if (newDCL)
		{
			newDCL->pNextDCLCommand	= nil ;
			newDCL->opcode			= kDCLPtrTimeStampOp ;
			newDCL->timeStampPtr	= inTimeStampPtr ;
	
			if (inDCL)
				inDCL->pNextDCLCommand = (DCLCommandStruct*) newDCL ;
		}
		
		return (DCLCommandStruct*) newDCL ;
	}
	
	void
	DCLCommandPool::Free(
		DCLCommandStruct* 				inDCL )
	{
		Lock() ;
		
		// 1. find this block in allocated list
		CFRange searchRange = {0, CFArrayGetCount(mAllocatedBlocks) } ;
		CFIndex	foundIndex = CFArrayGetFirstIndexOfValue(mAllocatedBlocks, searchRange, (const void*) inDCL);
		if (foundIndex >= 0)
		{
			IOByteCount	foundBlockSize = (IOByteCount) CFArrayGetValueAtIndex(mAllocatedBlockSizes, foundIndex) ;
			
			// 2. if found, return block to free list
			
			CFIndex index = 0 ;
			while ( (index < CFArrayGetCount(mFreeBlocks)) && (CFArrayGetValueAtIndex(mFreeBlocks, index) <= inDCL) )
				++index ;
	
			// update free space counter to reflect returning block to free list
			mBytesRemaining += foundBlockSize ;
	
			CFArrayRemoveValueAtIndex(mAllocatedBlocks, foundIndex) ;
			CFArrayRemoveValueAtIndex(mAllocatedBlockSizes, foundIndex) ;
			
			CFArrayInsertValueAtIndex(mFreeBlocks, index, inDCL) ;
			CFArrayInsertValueAtIndex(mFreeBlockSizes, index, (const void*) foundBlockSize) ;
			
			CoalesceFreeBlocks() ;
		}
		
		Unlock() ;
	}
	
	Boolean
	DCLCommandPool::SetSize(
		IOByteCount 					inSize )
	{
		// trying to make buffer smaller than space we've already allocated
		if (inSize < mStorageSize )
			return false ;
		
		if (inSize > mStorageSize)
		{
			UInt8*	newStorage = new UInt8[inSize] ;
			if (!newStorage)
				return false ;
			
			Lock() ;
			
			CFArrayAppendValue(mFreeBlocks, mStorage + mStorageSize) ;
			CFArrayAppendValue(mFreeBlockSizes, (const void*)(inSize - mStorageSize) ) ;
			CoalesceFreeBlocks() ;			
	
			mBytesRemaining += inSize - mStorageSize ;
			
			bcopy(mStorage, newStorage, mStorageSize) ;
			
			mStorageSize = inSize ;
	
			delete[] mStorage ;
			mStorage = newStorage ;
			
			Unlock() ;
		}
		
		return true ;
	}

	void
	DCLCommandPool::Lock()
	{
		pthread_mutex_lock( & mMutex ) ;
	}
	
	void
	DCLCommandPool::Unlock()
	{
		pthread_mutex_unlock( & mMutex ) ;
	}
	
	void
	DCLCommandPool::CoalesceFreeBlocks()
	{		
		UInt32			freeBlockCount	 	= CFArrayGetCount(mFreeBlocks) ;
		UInt32			index				= 1 ;
		IOByteCount		preceedingBlockSize = (IOByteCount) CFArrayGetValueAtIndex(mFreeBlockSizes, 0) ;
		IOByteCount		blockSize ;
	
		while (index < freeBlockCount)
		{
			blockSize = (IOByteCount) CFArrayGetValueAtIndex(mFreeBlockSizes, index) ;
			
			if ( ((UInt8*)CFArrayGetValueAtIndex(mFreeBlocks, index - 1) + preceedingBlockSize) == (UInt8*)CFArrayGetValueAtIndex(mFreeBlocks, index) )
			{
				// resize preceeding block to include current block
				CFArraySetValueAtIndex(mFreeBlockSizes, index-1, (const void*)(preceedingBlockSize + blockSize) ) ;
				
				// remove current block since preceeding block now includes it.
				CFArrayRemoveValueAtIndex(mFreeBlocks, index) ;
				CFArrayRemoveValueAtIndex(mFreeBlockSizes, index) ;
	
				preceedingBlockSize += blockSize ;
			}
			else
			{
				++index ;
				preceedingBlockSize = blockSize ;
			}
		}
	}
	
	// ============================================================
	// DCLCommandPoolCOM
	// ============================================================
	
	DCLCommandPoolCOM::DCLCommandPoolCOM( Device& inUserClient, IOByteCount inSize)
	: DCLCommandPool( reinterpret_cast<IUnknownVTbl*>( & sInterface ), inUserClient, inSize )
	{
	}
	
	DCLCommandPoolCOM::~DCLCommandPoolCOM()
	{
	}
	
	IUnknownVTbl**
	DCLCommandPoolCOM::Alloc(
		Device&	inUserClient, 
		IOByteCount			 			inSize)
	{
		DCLCommandPoolCOM *	me = nil;
		try {
			me = new DCLCommandPoolCOM(inUserClient, inSize) ;
		} catch(...) {
		}

		return (nil == me) ? nil : reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	HRESULT
	DCLCommandPoolCOM::QueryInterface(REFIID iid, void ** ppv )
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if ( CFEqual(interfaceID, IUnknownUUID) ||  CFEqual(interfaceID, kIOFireWireDCLCommandPoolInterfaceID) )
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
	
	//
	// --- static methods ------------------
	//
	DCLCommandStruct*
	DCLCommandPoolCOM::SAllocate(
		Ref						self, 
		IOByteCount 			inSize )
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->Allocate(inSize) ;
	}
	
	
	IOReturn
	DCLCommandPoolCOM::SAllocateWithOpcode(
		Ref						self, 
		DCLCommandStruct* 		inDCL, 
		DCLCommandStruct** 		outDCL, 
		UInt32 					opcode, ... )
	{
		IOReturn	result = kIOReturnSuccess ;
		va_list 	va ;
	
		va_start(va, opcode) ;
		result = IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->AllocateWithOpcode(inDCL, outDCL, opcode, va) ;
		va_end(va) ;
	
		return result ;
	}
	
	DCLCommandStruct*
	DCLCommandPoolCOM::SAllocateTransferPacketDCL(
		Ref						self, 
		DCLCommandStruct*		inDCL,
		UInt32					inOpcode,
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->AllocateTransferPacketDCL(inDCL, inOpcode, inBuffer, inSize) ;
	}
	
	DCLCommandStruct*
	DCLCommandPoolCOM::SAllocateTransferBufferDCL(
		Ref 	self, 
		DCLCommandStruct* 				inDCL, 
		UInt32 							inOpcode, 
		void* 							inBuffer, 
		IOByteCount 					inSize, 
		IOByteCount 					inPacketSize, 
		UInt32 							inBufferOffset)
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->AllocateTransferBufferDCL(inDCL, inOpcode, inBuffer, inSize, inPacketSize, inBufferOffset) ;
	}
	
	DCLCommandStruct*
	DCLCommandPoolCOM::SAllocateSendPacketStartDCL(
		Ref						self, 
		DCLCommandStruct* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->AllocateSendPacketStartDCL(inDCL, inBuffer, inSize) ;
	}
	
	DCLCommandStruct*
	DCLCommandPoolCOM::SAllocateSendPacketWithHeaderStartDCL(
		Ref						self, 
		DCLCommandStruct* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->AllocateSendPacketWithHeaderStartDCL(inDCL, inBuffer, inSize) ;
	}
	
	DCLCommandStruct*
	DCLCommandPoolCOM::SAllocateSendBufferDCL(		// currently does nothing
		Ref						self, 
		DCLCommandStruct* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize,
		IOByteCount				inPacketSize,
		UInt32					inBufferOffset)
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->AllocateSendBufferDCL(inDCL, inBuffer, inSize, inPacketSize, inBufferOffset) ;
	}
	
	DCLCommandStruct*
	DCLCommandPoolCOM::SAllocateSendPacketDCL(
		Ref						self, 
		DCLCommandStruct* 		inDCL,
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->AllocateSendPacketDCL(inDCL, inBuffer, inSize) ;
	}
	
	DCLCommandStruct*
	DCLCommandPoolCOM::SAllocateReceivePacketStartDCL(
		Ref						self, 
		DCLCommandStruct* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->AllocateReceivePacketStartDCL(inDCL, inBuffer, inSize) ;
	}
	
	DCLCommandStruct*
	DCLCommandPoolCOM::SAllocateReceivePacketDCL(
		Ref						self, 
		DCLCommandStruct* 		inDCL,
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->AllocateReceivePacketDCL(inDCL, inBuffer, inSize) ;
	}
	
	DCLCommandStruct*
	DCLCommandPoolCOM::SAllocateReceiveBufferDCL(	// currently does nothing
		Ref						self, 
		DCLCommandStruct* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize,
		IOByteCount				inPacketSize,
		UInt32					inBufferOffset)
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->AllocateReceiveBufferDCL(inDCL, inBuffer, inSize, inPacketSize, inBufferOffset) ;
	}
	
	DCLCommandStruct*
	DCLCommandPoolCOM::SAllocateCallProcDCL(
		Ref						self, 
		DCLCommandStruct* 		inDCL, 
		DCLCallCommandProcPtr	inProc,
		UInt32					inProcData)
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->AllocateCallProcDCL(inDCL, inProc, inProcData) ;
	}
	
	DCLCommandStruct*
	DCLCommandPoolCOM::SAllocateLabelDCL(
		Ref						self, 
		DCLCommandStruct* 		inDCL)
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->AllocateLabelDCL(inDCL) ;
	}
		
	DCLCommandStruct*
	DCLCommandPoolCOM::SAllocateJumpDCL(
		Ref						self, 
		DCLCommandStruct* 		inDCL, 
		DCLLabelPtr				pInJumpDCLLabel)
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->AllocateJumpDCL(inDCL, pInJumpDCLLabel) ;
	}
	
	DCLCommandStruct*
	DCLCommandPoolCOM::SAllocateSetTagSyncBitsDCL(
		Ref						self, 
		DCLCommandStruct* 		inDCL, 
		UInt16					inTagBits,
		UInt16					inSyncBits)
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->AllocateSetTagSyncBitsDCL(inDCL, inTagBits, inSyncBits) ;
	}
	
	DCLCommandStruct*
	DCLCommandPoolCOM::SAllocateUpdateDCLListDCL(
		Ref						self, 
		DCLCommandStruct* 		inDCL, 
		DCLCommandPtr*			inDCLCommandList,
		UInt32					inNumCommands)
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->AllocateUpdateDCLListDCL(inDCL, inDCLCommandList, inNumCommands) ;
	}
	
	DCLCommandStruct*
	DCLCommandPoolCOM::SAllocatePtrTimeStampDCL(
		Ref						self, 
		DCLCommandStruct* 		inDCL, 
		UInt32*					inTimeStampPtr)
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->AllocatePtrTimeStampDCL(inDCL, inTimeStampPtr) ;
	}
	
	void
	DCLCommandPoolCOM::SFree(
		Ref 	self, 
		DCLCommandStruct* 				inDCL )
	{
		IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->Free(inDCL) ;
	}
	
	IOByteCount
	DCLCommandPoolCOM::SGetSize(
		Ref 	self )
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->mStorageSize ;
	}
	
	Boolean
	DCLCommandPoolCOM::SSetSize(
		Ref 	self, 
		IOByteCount 					inSize )
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->SetSize(inSize) ;
	}
	
	IOByteCount
	DCLCommandPoolCOM::SGetBytesRemaining(
		Ref 	self )
	{
		return IOFireWireIUnknown::InterfaceMap<DCLCommandPoolCOM>::GetThis(self)->mBytesRemaining ;
	}
}