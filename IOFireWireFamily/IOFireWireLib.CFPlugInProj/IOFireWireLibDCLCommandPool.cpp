/*
 *  IOFireWireLibDCLPool.cpp
 *  IOFireWireFamily
 *
 *  Created by NWG on Mon Mar 12 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include "IOFireWireLibDCLCommandPool.h"

IOFireWireDCLCommandPoolInterface
IOFireWireLibDCLCommandPoolCOM::sInterface = {
	INTERFACEIMP_INTERFACE,
	1, 0, //vers, rev

	&IOFireWireLibDCLCommandPoolCOM::SAllocate,
	&IOFireWireLibDCLCommandPoolCOM::SAllocateWithOpcode,
	
	&IOFireWireLibDCLCommandPoolCOM::SAllocateTransferPacketDCL,
	&IOFireWireLibDCLCommandPoolCOM::SAllocateTransferBufferDCL,

	&IOFireWireLibDCLCommandPoolCOM::SAllocateSendPacketStartDCL,
	&IOFireWireLibDCLCommandPoolCOM::SAllocateSendPacketWithHeaderStartDCL,
	&IOFireWireLibDCLCommandPoolCOM::SAllocateSendBufferDCL,
	&IOFireWireLibDCLCommandPoolCOM::SAllocateSendPacketDCL,

	&IOFireWireLibDCLCommandPoolCOM::SAllocateReceivePacketStartDCL,
	&IOFireWireLibDCLCommandPoolCOM::SAllocateReceivePacketDCL,
	&IOFireWireLibDCLCommandPoolCOM::SAllocateReceiveBufferDCL,

	&IOFireWireLibDCLCommandPoolCOM::SAllocateCallProcDCL,
	&IOFireWireLibDCLCommandPoolCOM::SAllocateLabelDCL,
	&IOFireWireLibDCLCommandPoolCOM::SAllocateJumpDCL,
	&IOFireWireLibDCLCommandPoolCOM::SAllocateSetTagSyncBitsDCL,
	&IOFireWireLibDCLCommandPoolCOM::SAllocateUpdateDCLListDCL,
	&IOFireWireLibDCLCommandPoolCOM::SAllocatePtrTimeStampDCL,

	&IOFireWireLibDCLCommandPoolCOM::SFree,
	
	&IOFireWireLibDCLCommandPoolCOM::SGetSize,
	&IOFireWireLibDCLCommandPoolCOM::SSetSize,
	&IOFireWireLibDCLCommandPoolCOM::SGetBytesRemaining
} ;

IOFireWireLibDCLCommandPoolImp::IOFireWireLibDCLCommandPoolImp(
	IOFireWireDeviceInterfaceImp&	inUserClient,
	IOByteCount						inSize): IOFireWireIUnknown(),
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
}
	
IOFireWireLibDCLCommandPoolImp::~IOFireWireLibDCLCommandPoolImp()
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
	
	mUserClient.Release() ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolImp::Allocate(
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
IOFireWireLibDCLCommandPoolImp::AllocateWithOpcode(
	DCLCommandStruct* 		inDCL, 
	DCLCommandStruct** 		outDCL, 
	UInt32 					opcode, ... )
{
	return kIOReturnUnsupported ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolImp::AllocateTransferPacketDCL(
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
IOFireWireLibDCLCommandPoolImp::AllocateTransferBufferDCL(
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
IOFireWireLibDCLCommandPoolImp::AllocateSendPacketStartDCL(
	DCLCommandStruct* 		inDCL, 
	void*					inBuffer,
	IOByteCount				inSize)
{
	return AllocateTransferPacketDCL(inDCL, kDCLSendPacketStartOp, inBuffer, inSize) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolImp::AllocateSendPacketWithHeaderStartDCL(
	DCLCommandStruct* 		inDCL, 
	void*					inBuffer,
	IOByteCount				inSize)
{
	return AllocateTransferPacketDCL(inDCL, kDCLSendPacketWithHeaderStartOp, inBuffer, inSize) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolImp::AllocateSendBufferDCL(	// not implemented
	DCLCommandStruct* 		inDCL, 
	void*					inBuffer,
	IOByteCount				inSize,
	IOByteCount				inPacketSize,
	UInt32					inBufferOffset)
{
	return nil ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolImp::AllocateSendPacketDCL(
	DCLCommandStruct* 		inDCL,
	void*					inBuffer,
	IOByteCount				inSize)
{
	return AllocateTransferPacketDCL(inDCL, kDCLSendPacketOp, inBuffer, inSize) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolImp::AllocateReceivePacketStartDCL(
	DCLCommandStruct* 		inDCL, 
	void*					inBuffer,
	IOByteCount				inSize)
{
	return AllocateTransferPacketDCL(inDCL, kDCLReceivePacketStartOp, inBuffer, inSize) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolImp::AllocateReceivePacketDCL(
	DCLCommandStruct* 		inDCL,
	void*					inBuffer,
	IOByteCount				inSize)
{
	return AllocateTransferPacketDCL(inDCL, kDCLReceivePacketOp, inBuffer, inSize) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolImp::AllocateReceiveBufferDCL( // not implemented
	DCLCommandStruct* 		inDCL, 
	void*					inBuffer,
	IOByteCount				inSize,
	IOByteCount				inPacketSize,
	UInt32					inBufferOffset)
{
	return nil ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolImp::AllocateCallProcDCL(
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
IOFireWireLibDCLCommandPoolImp::AllocateLabelDCL(
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
IOFireWireLibDCLCommandPoolImp::AllocateJumpDCL(
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
IOFireWireLibDCLCommandPoolImp::AllocateSetTagSyncBitsDCL(
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
IOFireWireLibDCLCommandPoolImp::AllocateUpdateDCLListDCL(
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
IOFireWireLibDCLCommandPoolImp::AllocatePtrTimeStampDCL(
	DCLCommandStruct* 		inDCL, 
	UInt32*					inTimeStampPtr)
{
	DCLPtrTimeStampStruct*	newDCL = (DCLPtrTimeStampStruct*) Allocate(sizeof(DCLPtrTimeStampStruct)) ;
	
	if (newDCL)
	{
		newDCL->pNextDCLCommand	= nil ;
		newDCL->opcode			= kDCLTimeStampOp ;
		newDCL->timeStampPtr	= inTimeStampPtr ;
	}
	
	return (DCLCommandStruct*) newDCL ;
}

void
IOFireWireLibDCLCommandPoolImp::Free(
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
			index++ ;

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

IOByteCount
IOFireWireLibDCLCommandPoolImp::GetSize()
{
	return mStorageSize ;
}

Boolean
IOFireWireLibDCLCommandPoolImp::SetSize(
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

IOByteCount
IOFireWireLibDCLCommandPoolImp::GetBytesRemaining()
{
	return mBytesRemaining ;
}

void
IOFireWireLibDCLCommandPoolImp::Lock()
{
}

void
IOFireWireLibDCLCommandPoolImp::Unlock()
{
}

void
IOFireWireLibDCLCommandPoolImp::CoalesceFreeBlocks()
{
	Lock() ;
	
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
			index++ ;
			preceedingBlockSize = blockSize ;
		}
	}
	
	Unlock() ;
}

// ============================================================
// IOFireWireLibDCLCommandPoolCOM
// ============================================================

IOFireWireLibDCLCommandPoolCOM::IOFireWireLibDCLCommandPoolCOM(
	IOFireWireDeviceInterfaceImp&	inUserClient, 
	IOByteCount						inSize): IOFireWireLibDCLCommandPoolImp(inUserClient, inSize)
{
	mInterface.pseudoVTable	= (IUnknownVTbl*) & IOFireWireLibDCLCommandPoolCOM::sInterface ;
	mInterface.obj			= this ;
}

IOFireWireLibDCLCommandPoolCOM::~IOFireWireLibDCLCommandPoolCOM()
{
}

IUnknownVTbl**
IOFireWireLibDCLCommandPoolCOM::Alloc(
	IOFireWireDeviceInterfaceImp&	inUserClient, 
	IOByteCount			 			inSize)
{
    IOFireWireLibDCLCommandPoolCOM *	me;
	IUnknownVTbl** 	interface = NULL;
	
    me = new IOFireWireLibDCLCommandPoolCOM(inUserClient, inSize) ;
    if( me && me->Init() )
	{
		// whoops -- no need to call addref. all these objects derive from IOFireWireIUnknown
		// which automatically sets the ref count to 1 on creation.
//		me->AddRef();
        interface = & me->mInterface.pseudoVTable;
    }
	else
		delete me ;
	
	return interface;
}

HRESULT
IOFireWireLibDCLCommandPoolCOM::QueryInterface(REFIID iid, void ** ppv )
{
	HRESULT		result = S_OK ;
	*ppv = nil ;

	CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;

	if ( CFEqual(interfaceID, IUnknownUUID) ||  CFEqual(interfaceID, kIOFireWireDCLCommandPoolInterfaceID) )
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

//
// --- static methods ------------------
//
DCLCommandStruct*
IOFireWireLibDCLCommandPoolCOM::SAllocate(
	IOFireWireLibDCLCommandPoolRef	self, 
	IOByteCount 					inSize )
{
	return GetThis(self)->Allocate(inSize) ;
}


IOReturn
IOFireWireLibDCLCommandPoolCOM::SAllocateWithOpcode(
	IOFireWireLibDCLCommandPoolRef	self, 
	DCLCommandStruct* 				inDCL, 
	DCLCommandStruct** 				outDCL, 
	UInt32 							opcode, ... )
{
	IOReturn	result = kIOReturnSuccess ;
	va_list 	va ;

	va_start(va, opcode) ;
	result = GetThis(self)->AllocateWithOpcode(inDCL, outDCL, opcode, va) ;
	va_end(va) ;

	return result ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolCOM::SAllocateTransferPacketDCL(
	IOFireWireLibDCLCommandPoolRef	self, 
	DCLCommandStruct*				inDCL,
	UInt32							inOpcode,
	void*							inBuffer,
	IOByteCount						inSize)
{
	return GetThis(self)->AllocateTransferPacketDCL(inDCL, inOpcode, inBuffer, inSize) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolCOM::SAllocateTransferBufferDCL(
	IOFireWireLibDCLCommandPoolRef 	self, 
	DCLCommandStruct* 				inDCL, 
	UInt32 							inOpcode, 
	void* 							inBuffer, 
	IOByteCount 					inSize, 
	IOByteCount 					inPacketSize, 
	UInt32 							inBufferOffset)
{
	return GetThis(self)->AllocateTransferBufferDCL(inDCL, inOpcode, inBuffer, inSize, inPacketSize, inBufferOffset) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolCOM::SAllocateSendPacketStartDCL(
	IOFireWireLibDCLCommandPoolRef	self, 
	DCLCommandStruct* 		inDCL, 
	void*					inBuffer,
	IOByteCount				inSize)
{
	return GetThis(self)->AllocateSendPacketStartDCL(inDCL, inBuffer, inSize) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolCOM::SAllocateSendPacketWithHeaderStartDCL(
	IOFireWireLibDCLCommandPoolRef	self, 
	DCLCommandStruct* 		inDCL, 
	void*					inBuffer,
	IOByteCount				inSize)
{
	return GetThis(self)->AllocateSendPacketWithHeaderStartDCL(inDCL, inBuffer, inSize) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolCOM::SAllocateSendBufferDCL(		// currently does nothing
	IOFireWireLibDCLCommandPoolRef	self, 
	DCLCommandStruct* 		inDCL, 
	void*					inBuffer,
	IOByteCount				inSize,
	IOByteCount				inPacketSize,
	UInt32					inBufferOffset)
{
	return GetThis(self)->AllocateSendBufferDCL(inDCL, inBuffer, inSize, inPacketSize, inBufferOffset) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolCOM::SAllocateSendPacketDCL(
	IOFireWireLibDCLCommandPoolRef	self, 
	DCLCommandStruct* 		inDCL,
	void*					inBuffer,
	IOByteCount				inSize)
{
	return GetThis(self)->AllocateSendPacketDCL(inDCL, inBuffer, inSize) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolCOM::SAllocateReceivePacketStartDCL(
	IOFireWireLibDCLCommandPoolRef	self, 
	DCLCommandStruct* 		inDCL, 
	void*					inBuffer,
	IOByteCount				inSize)
{
	return GetThis(self)->AllocateReceivePacketStartDCL(inDCL, inBuffer, inSize) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolCOM::SAllocateReceivePacketDCL(
	IOFireWireLibDCLCommandPoolRef	self, 
	DCLCommandStruct* 		inDCL,
	void*					inBuffer,
	IOByteCount				inSize)
{
	return GetThis(self)->AllocateReceivePacketDCL(inDCL, inBuffer, inSize) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolCOM::SAllocateReceiveBufferDCL(	// currently does nothing
	IOFireWireLibDCLCommandPoolRef	self, 
	DCLCommandStruct* 		inDCL, 
	void*					inBuffer,
	IOByteCount				inSize,
	IOByteCount				inPacketSize,
	UInt32					inBufferOffset)
{
	return GetThis(self)->AllocateReceiveBufferDCL(inDCL, inBuffer, inSize, inPacketSize, inBufferOffset) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolCOM::SAllocateCallProcDCL(
	IOFireWireLibDCLCommandPoolRef	self, 
	DCLCommandStruct* 		inDCL, 
	DCLCallCommandProcPtr	inProc,
	UInt32					inProcData)
{
	return GetThis(self)->AllocateCallProcDCL(inDCL, inProc, inProcData) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolCOM::SAllocateLabelDCL(
	IOFireWireLibDCLCommandPoolRef	self, 
	DCLCommandStruct* 		inDCL)
{
	return GetThis(self)->AllocateLabelDCL(inDCL) ;
}
	
DCLCommandStruct*
IOFireWireLibDCLCommandPoolCOM::SAllocateJumpDCL(
	IOFireWireLibDCLCommandPoolRef	self, 
	DCLCommandStruct* 		inDCL, 
	DCLLabelPtr				pInJumpDCLLabel)
{
	return GetThis(self)->AllocateJumpDCL(inDCL, pInJumpDCLLabel) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolCOM::SAllocateSetTagSyncBitsDCL(
	IOFireWireLibDCLCommandPoolRef	self, 
	DCLCommandStruct* 		inDCL, 
	UInt16					inTagBits,
	UInt16					inSyncBits)
{
	return GetThis(self)->AllocateSetTagSyncBitsDCL(inDCL, inTagBits, inSyncBits) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolCOM::SAllocateUpdateDCLListDCL(
	IOFireWireLibDCLCommandPoolRef	self, 
	DCLCommandStruct* 		inDCL, 
	DCLCommandPtr*			inDCLCommandList,
	UInt32					inNumCommands)
{
	return GetThis(self)->AllocateUpdateDCLListDCL(inDCL, inDCLCommandList, inNumCommands) ;
}

DCLCommandStruct*
IOFireWireLibDCLCommandPoolCOM::SAllocatePtrTimeStampDCL(
	IOFireWireLibDCLCommandPoolRef	self, 
	DCLCommandStruct* 		inDCL, 
	UInt32*					inTimeStampPtr)
{
	return GetThis(self)->AllocatePtrTimeStampDCL(inDCL, inTimeStampPtr) ;
}

void
IOFireWireLibDCLCommandPoolCOM::SFree(
	IOFireWireLibDCLCommandPoolRef 	self, 
	DCLCommandStruct* 				inDCL )
{
	GetThis(self)->Free(inDCL) ;
}

IOByteCount
IOFireWireLibDCLCommandPoolCOM::SGetSize(
	IOFireWireLibDCLCommandPoolRef 	self )
{
	return GetThis(self)->GetSize() ;
}

Boolean
IOFireWireLibDCLCommandPoolCOM::SSetSize(
	IOFireWireLibDCLCommandPoolRef 	self, 
	IOByteCount 					inSize )
{
	return GetThis(self)->SetSize(inSize) ;
}

IOByteCount
IOFireWireLibDCLCommandPoolCOM::SGetBytesRemaining(
	IOFireWireLibDCLCommandPoolRef 	self )
{
	return GetThis(self)->GetBytesRemaining() ;
}
