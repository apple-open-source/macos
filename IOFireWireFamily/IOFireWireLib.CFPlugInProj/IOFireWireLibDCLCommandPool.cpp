/*
 *  IOFireWireLibDCLCommandPool.cpp
 *  IOFireWireFamily
 *
 *  Created by NWG on Mon Mar 12 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */
/*
	$Log: not supported by cvs2svn $
	Revision 1.16  2007/02/07 06:35:22  collin
	*** empty log message ***
	
	Revision 1.15  2007/01/08 18:47:20  ayanowit
	More 64-bit changes for isoch.
	
	Revision 1.14  2007/01/02 18:14:12  ayanowit
	Enabled building the plug-in lib 4-way FAT. Also, fixed compile problems for 64-bit.
	
	Revision 1.13  2006/02/09 00:21:55  niels
	merge chardonnay branch to tot
	
	Revision 1.12  2005/09/24 00:55:28  niels
	*** empty log message ***
	
	Revision 1.11.20.2  2006/01/31 04:49:57  collin
	*** empty log message ***
	
	Revision 1.11  2003/07/21 10:01:29  niels
	*** empty log message ***
	
	Revision 1.10  2003/07/21 06:53:10  niels
	merge isoch to TOT
	
	Revision 1.9.14.1  2003/07/01 20:54:23  niels
	isoch merge
	
	Revision 1.9  2002/09/25 00:27:33  niels
	flip your world upside-down
	
	Revision 1.8  2002/08/26 20:08:34  niels
	fix user space hang (when devices are unplugged and a DCL program is running)
	
*/

#import "IOFireWireLibDCLCommandPool.h"
#import "IOFireWireLibDevice.h"
#import <mach/mach.h>
#import <pthread.h>

namespace IOFireWireLib {
	
	TraditionalDCLCommandPoolCOM::Interface	TraditionalDCLCommandPoolCOM::sInterface = {
		INTERFACEIMP_INTERFACE,
		1, 0, //vers, rev
	
		SAllocate,
		SAllocateWithOpcode,
		SAllocateTransferPacketDCL,
		SAllocateTransferBufferDCL,
		SAllocateSendPacketStartDCL,
		SAllocateSendPacketWithHeaderStartDCL,
		SAllocateSendBufferDCL,
		SAllocateSendPacketDCL,
		SAllocateReceivePacketStartDCL,
		SAllocateReceivePacketDCL,
		SAllocateReceiveBufferDCL,
		SAllocateCallProcDCL,
		SAllocateLabelDCL,
		SAllocateJumpDCL,
		SAllocateSetTagSyncBitsDCL,
		SAllocateUpdateDCLListDCL,
		SAllocatePtrTimeStampDCL,
		SFree,
		SGetSize,
		SSetSize,
		SGetBytesRemaining
	} ;
	
	TraditionalDCLCommandPool::TraditionalDCLCommandPool( const IUnknownVTbl & interface, Device& inUserClient, IOByteCount inSize )
	: IOFireWireIUnknown( interface ),
	mUserClient(inUserClient)
	{
		mUserClient.AddRef() ;
	
		mFreeBlocks				= CFArrayCreateMutable(kCFAllocatorDefault, 0, nil) ;
		mFreeBlockSizes			= CFArrayCreateMutable(kCFAllocatorDefault, 0, nil) ;
		mAllocatedBlocks		= CFArrayCreateMutable(kCFAllocatorDefault, 0, nil) ;
		mAllocatedBlockSizes	= CFArrayCreateMutable(kCFAllocatorDefault, 0, nil) ;
	
//		mStorage = new UInt8[inSize] ;
		IOReturn		error = vm_allocate ( mach_task_self (), (vm_address_t *) & mStorage, inSize, true /*anywhere*/ ) ;
		if ( error )
			throw error ;
			
		if ( ! mStorage )
			throw kIOReturnVMError ;
				
		mBytesRemaining = inSize ;
		mStorageSize = inSize ;

#ifdef __LP64__
		DebugLog( "TraditionalDCLCommandPool::TraditionalDCLCommandPool mStorage=%p, mStorageSize=%u\n", mStorage, (UInt32)mStorageSize ) ;
#else
		DebugLog( "TraditionalDCLCommandPool::TraditionalDCLCommandPool mStorage=%p, mStorageSize=%lu\n", mStorage, (UInt32)mStorageSize ) ;
#endif
		::CFArrayAppendValue ( mFreeBlocks, mStorage ) ;
		::CFArrayAppendValue ( mFreeBlockSizes, (const void *) inSize ) ;
		
		pthread_mutex_init( & mMutex, nil ) ;
	}
		
	TraditionalDCLCommandPool::~TraditionalDCLCommandPool()
	{
		Lock() ;
	
		if (mStorage)
		{
//			delete[] mStorage ;
			vm_deallocate ( mach_task_self (), (vm_address_t) mStorage, mStorageSize ) ;
			mStorage = nil ;
			mStorageSize = 0 ;
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
	
	DCLCommand*
	TraditionalDCLCommandPool::Allocate(
		IOByteCount 					inSize )
	{
		unsigned long		blockSize ;
		UInt32 				remainder ;
		UInt32 				index			= 0 ;
		const UInt8*		foundFreeBlock	= 0 ;
		DCLCommand*	allocatedBlock	= nil ;
		UInt32				freeBlockCount	= CFArrayGetCount(mFreeBlocks) ;
	
		Lock() ;
		
		do
		{
			blockSize	= (unsigned long) CFArrayGetValueAtIndex(mFreeBlockSizes, index) ;
			remainder	= blockSize - inSize ;
	
			if ( blockSize >= inSize )
			{
				// found a free block w/ enough space,
				// use it to allocate. we allocate from the end of the free block, not the beginning
				foundFreeBlock	= (const UInt8*) CFArrayGetValueAtIndex(mFreeBlocks, index) ;
				allocatedBlock	= (DCLCommand*) (foundFreeBlock + remainder) ;
				
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
	TraditionalDCLCommandPool::AllocateWithOpcode(
		DCLCommand* 		inDCL, 
		DCLCommand** 		outDCL, 
		UInt32 					opcode, ... )
	{
		return kIOReturnUnsupported ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPool::AllocateTransferPacketDCL(
		DCLCommand*		inDCL,
		UInt32					inOpcode,
		void*					inBuffer,
		IOByteCount				inSize)
	{
		DCLTransferPacket*	newDCL = (DCLTransferPacket*) Allocate( sizeof(DCLTransferPacket) ) ;
	
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
			inDCL->pNextDCLCommand = (DCLCommand*) newDCL ;
	
		return (DCLCommand*) newDCL ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPool::AllocateTransferBufferDCL(
		DCLCommand*		inDCL, 
		UInt32 					inOpcode, 
		void* 					inBuffer, 
		IOByteCount 			inSize, 
		IOByteCount 			inPacketSize, 
		UInt32 					inBufferOffset)
	{
		DCLTransferBuffer*	newDCL = (DCLTransferBuffer*) Allocate( sizeof(DCLTransferBuffer) ) ;
	
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
			inDCL->pNextDCLCommand = (DCLCommand*) newDCL ;
	
		return (DCLCommand*) newDCL ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPool::AllocateSendPacketStartDCL(
		DCLCommand* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return AllocateTransferPacketDCL(inDCL, kDCLSendPacketStartOp, inBuffer, inSize) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPool::AllocateSendPacketWithHeaderStartDCL(
		DCLCommand* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return nil;	// Deprecated 
		//return AllocateTransferPacketDCL(inDCL, kDCLSendPacketWithHeaderStartOp, inBuffer, inSize) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPool::AllocateSendBufferDCL(	// not implemented
		DCLCommand* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize,
		IOByteCount				inPacketSize,
		UInt32					inBufferOffset)
	{
		return nil ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPool::AllocateSendPacketDCL(
		DCLCommand* 		inDCL,
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return AllocateTransferPacketDCL(inDCL, kDCLSendPacketOp, inBuffer, inSize) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPool::AllocateReceivePacketStartDCL(
		DCLCommand* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return AllocateTransferPacketDCL(inDCL, kDCLReceivePacketStartOp, inBuffer, inSize) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPool::AllocateReceivePacketDCL(
		DCLCommand* 		inDCL,
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return AllocateTransferPacketDCL(inDCL, kDCLReceivePacketOp, inBuffer, inSize) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPool::AllocateReceiveBufferDCL( // not implemented
		DCLCommand* 			inDCL, 
		void*					inBuffer,
		IOByteCount				inSize,
		IOByteCount				inPacketSize,
		UInt32					inBufferOffset)
	{
		return nil ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPool::AllocateCallProcDCL(
		DCLCommand* 			inDCL, 
		DCLCallCommandProc*		inProc,
		DCLCallProcDataType		inProcData)
	{
		DCLCallProc*	newDCL = (DCLCallProc*) Allocate(sizeof(DCLCallProc)) ;
		
		if (!newDCL)
			return nil ;
		
		newDCL->pNextDCLCommand	= nil ;
		newDCL->opcode 			= kDCLCallProcOp ;
		newDCL->proc			= inProc ;
		newDCL->procData		= inProcData ;
		
		if (inDCL)
			inDCL->pNextDCLCommand = (DCLCommand*) newDCL ;
		
		return (DCLCommand*) newDCL ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPool::AllocateLabelDCL(
		DCLCommand* 		inDCL)
	{
		DCLLabel*	newDCL = (DCLLabel*) Allocate(sizeof(DCLLabel)) ;
		
		if (newDCL)
		{
			newDCL->pNextDCLCommand = nil ;
			newDCL->opcode			= kDCLLabelOp ;
			
			if (inDCL)
				inDCL->pNextDCLCommand = (DCLCommand*) newDCL ;
		}
		
		return (DCLCommand*) newDCL ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPool::AllocateJumpDCL(
		DCLCommand* 		inDCL, 
		DCLLabel*			pInJumpDCLLabel)
	{
		DCLJump*	newDCL = (DCLJump*) Allocate( sizeof(DCLJump)) ;
	
		if (newDCL)
		{
			newDCL->pNextDCLCommand = nil ;
			newDCL->opcode			= kDCLJumpOp ;
			newDCL->pJumpDCLLabel	= pInJumpDCLLabel ;
			
			if (inDCL)
				inDCL->pNextDCLCommand = (DCLCommand*) newDCL ;
		}
		
		return (DCLCommand*) newDCL ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPool::AllocateSetTagSyncBitsDCL(
		DCLCommand* 		inDCL, 
		UInt16					inTagBits,
		UInt16					inSyncBits)
	{
		DCLSetTagSyncBits*	newDCL = (DCLSetTagSyncBits*) Allocate(sizeof(DCLSetTagSyncBits)) ;
		
		if (newDCL)
		{
			newDCL->pNextDCLCommand = nil ;
			newDCL->opcode			= kDCLSetTagSyncBitsOp ;
			newDCL->tagBits			= inTagBits ;
			newDCL->syncBits		= inSyncBits ;
			
			if (inDCL)
				inDCL->pNextDCLCommand = (DCLCommand*) newDCL ;
		}
		
		return (DCLCommand*) newDCL ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPool::AllocateUpdateDCLListDCL(
		DCLCommand* 		inDCL, 
		DCLCommand**			inDCLCommandList,
		UInt32					inNumDCLCommands)
	{
		DCLUpdateDCLList*	newDCL = (DCLUpdateDCLList*) Allocate(sizeof(DCLUpdateDCLList)) ;
		
		if (newDCL)
		{
			newDCL->pNextDCLCommand	= nil ;
			newDCL->opcode 			= kDCLUpdateDCLListOp ;
			newDCL->dclCommandList	= inDCLCommandList ;
			newDCL->numDCLCommands	= inNumDCLCommands ;
			
			if (inDCL)
				inDCL->pNextDCLCommand = (DCLCommand*) newDCL ;
		}
		
		return (DCLCommand*) newDCL ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPool::AllocatePtrTimeStampDCL(
		DCLCommand* 		inDCL, 
		UInt32*					inTimeStampPtr)
	{
		DCLPtrTimeStamp*	newDCL = (DCLPtrTimeStamp*) Allocate(sizeof(DCLPtrTimeStamp)) ;
		
		if (newDCL)
		{
			newDCL->pNextDCLCommand	= nil ;
			newDCL->opcode			= kDCLPtrTimeStampOp ;
			newDCL->timeStampPtr	= inTimeStampPtr ;
	
			if (inDCL)
				inDCL->pNextDCLCommand = (DCLCommand*) newDCL ;
		}
		
		return (DCLCommand*) newDCL ;
	}
	
	void
	TraditionalDCLCommandPool::Free(
		DCLCommand* 				inDCL )
	{
		Lock() ;
		
		// 1. find this block in allocated list
		CFRange searchRange = {0, CFArrayGetCount(mAllocatedBlocks) } ;
		CFIndex	foundIndex = CFArrayGetFirstIndexOfValue(mAllocatedBlocks, searchRange, (const void*) inDCL);
		if (foundIndex >= 0)
		{
			unsigned long foundBlockSize = (unsigned long) CFArrayGetValueAtIndex(mAllocatedBlockSizes, foundIndex) ;
			
			// 2. if found, return block to free list
			
			CFIndex index = 0 ;
			{
				CFIndex count = ::CFArrayGetCount( mFreeBlocks ) ;
				while ( (index < count) && (CFArrayGetValueAtIndex(mFreeBlocks, index) <= inDCL) )
					++index ;
			}
			
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
	TraditionalDCLCommandPool::SetSize(
		IOByteCount 					inSize )
	{
		// trying to make buffer smaller than space we've already allocated
		if (inSize < mStorageSize )
			return false ;
		
		if (inSize > mStorageSize)
		{
			UInt8*	newStorage = 0 ;// = new UInt8[inSize] ;
			IOReturn error = vm_allocate ( mach_task_self (), (vm_address_t *) & newStorage, inSize, true /*anywhere*/ ) ;
			if ( error )
				return false ;
				
			if ( ! newStorage )
				return false ;
			
			Lock() ;
			
			::CFArrayAppendValue ( mFreeBlocks, mStorage + mStorageSize ) ;
			::CFArrayAppendValue ( mFreeBlockSizes, (const void *)( inSize - mStorageSize ) ) ;
			
			CoalesceFreeBlocks() ;			
	
			mBytesRemaining += inSize - mStorageSize ;
			
			bcopy ( mStorage, newStorage, mStorageSize ) ;

//			delete[] mStorage ;
			vm_deallocate ( mach_task_self (), (vm_address_t) mStorage, mStorageSize ) ;
				
			mStorage = newStorage ;
			mStorageSize = inSize ;
			
			Unlock() ;
		}
		
		return true ;
	}

	void
	TraditionalDCLCommandPool::Lock()
	{
		pthread_mutex_lock( & mMutex ) ;
	}
	
	void
	TraditionalDCLCommandPool::Unlock()
	{
		pthread_mutex_unlock( & mMutex ) ;
	}
	
	void
	TraditionalDCLCommandPool::CoalesceFreeBlocks()
	{		
		UInt32			freeBlockCount	 	= CFArrayGetCount(mFreeBlocks) ;
		UInt32			index				= 1 ;
		unsigned long		preceedingBlockSize = (unsigned long) CFArrayGetValueAtIndex(mFreeBlockSizes, 0) ;
		unsigned long		blockSize ;
	
		while (index < freeBlockCount)
		{
			blockSize = (unsigned long) CFArrayGetValueAtIndex(mFreeBlockSizes, index) ;
			
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
	// TraditionalDCLCommandPoolCOM
	// ============================================================
	
	TraditionalDCLCommandPoolCOM::TraditionalDCLCommandPoolCOM( Device& inUserClient, IOByteCount inSize)
	: TraditionalDCLCommandPool( reinterpret_cast<const IUnknownVTbl &>( sInterface ), inUserClient, inSize )
	{
	}
	
	TraditionalDCLCommandPoolCOM::~TraditionalDCLCommandPoolCOM()
	{
	}
	
	IUnknownVTbl**
	TraditionalDCLCommandPoolCOM::Alloc(
		Device&	inUserClient, 
		IOByteCount			 			inSize)
	{
		TraditionalDCLCommandPoolCOM *	me = nil;
		try {
			me = new TraditionalDCLCommandPoolCOM(inUserClient, inSize) ;
		} catch(...) {
		}

		return (nil == me) ? nil : reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	HRESULT
	TraditionalDCLCommandPoolCOM::QueryInterface(REFIID iid, void ** ppv )
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
	DCLCommand*
	TraditionalDCLCommandPoolCOM::SAllocate(
		Ref						self, 
		IOByteCount 			inSize )
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->Allocate(inSize) ;
	}
	
	
	IOReturn
	TraditionalDCLCommandPoolCOM::SAllocateWithOpcode(
		Ref						self, 
		DCLCommand* 		inDCL, 
		DCLCommand** 		outDCL, 
		UInt32 					opcode, ... )
	{
		IOReturn	result = kIOReturnSuccess ;
		va_list 	va ;
	
		va_start(va, opcode) ;
		result = IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->AllocateWithOpcode(inDCL, outDCL, opcode, va) ;
		va_end(va) ;
	
		return result ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPoolCOM::SAllocateTransferPacketDCL(
		Ref						self, 
		DCLCommand*		inDCL,
		UInt32					inOpcode,
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->AllocateTransferPacketDCL(inDCL, inOpcode, inBuffer, inSize) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPoolCOM::SAllocateTransferBufferDCL(
		Ref 	self, 
		DCLCommand* 				inDCL, 
		UInt32 							inOpcode, 
		void* 							inBuffer, 
		IOByteCount 					inSize, 
		IOByteCount 					inPacketSize, 
		UInt32 							inBufferOffset)
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->AllocateTransferBufferDCL(inDCL, inOpcode, inBuffer, inSize, inPacketSize, inBufferOffset) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPoolCOM::SAllocateSendPacketStartDCL(
		Ref						self, 
		DCLCommand* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->AllocateSendPacketStartDCL(inDCL, inBuffer, inSize) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPoolCOM::SAllocateSendPacketWithHeaderStartDCL(
		Ref						self, 
		DCLCommand* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->AllocateSendPacketWithHeaderStartDCL(inDCL, inBuffer, inSize) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPoolCOM::SAllocateSendBufferDCL(		// currently does nothing
		Ref						self, 
		DCLCommand* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize,
		IOByteCount				inPacketSize,
		UInt32					inBufferOffset)
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->AllocateSendBufferDCL(inDCL, inBuffer, inSize, inPacketSize, inBufferOffset) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPoolCOM::SAllocateSendPacketDCL(
		Ref						self, 
		DCLCommand* 		inDCL,
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->AllocateSendPacketDCL(inDCL, inBuffer, inSize) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPoolCOM::SAllocateReceivePacketStartDCL(
		Ref						self, 
		DCLCommand* 		inDCL, 
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->AllocateReceivePacketStartDCL(inDCL, inBuffer, inSize) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPoolCOM::SAllocateReceivePacketDCL(
		Ref						self, 
		DCLCommand* 		inDCL,
		void*					inBuffer,
		IOByteCount				inSize)
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->AllocateReceivePacketDCL(inDCL, inBuffer, inSize) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPoolCOM::SAllocateReceiveBufferDCL(	// currently does nothing
		Ref						self, 
		DCLCommand* 			inDCL, 
		void*					inBuffer,
		IOByteCount				inSize,
		IOByteCount				inPacketSize,
		UInt32					inBufferOffset)
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->AllocateReceiveBufferDCL(inDCL, inBuffer, inSize, inPacketSize, inBufferOffset) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPoolCOM::SAllocateCallProcDCL(
		Ref						self, 
		DCLCommand* 			inDCL, 
		DCLCallCommandProc*		inProc,
		DCLCallProcDataType		inProcData)
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->AllocateCallProcDCL(inDCL, inProc, inProcData) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPoolCOM::SAllocateLabelDCL(
		Ref						self, 
		DCLCommand* 			inDCL)
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->AllocateLabelDCL(inDCL) ;
	}
		
	DCLCommand*
	TraditionalDCLCommandPoolCOM::SAllocateJumpDCL(
		Ref						self, 
		DCLCommand* 			inDCL, 
		DCLLabel*				pInJumpDCLLabel)
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->AllocateJumpDCL(inDCL, pInJumpDCLLabel) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPoolCOM::SAllocateSetTagSyncBitsDCL(
		Ref						self, 
		DCLCommand* 		inDCL, 
		UInt16					inTagBits,
		UInt16					inSyncBits)
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->AllocateSetTagSyncBitsDCL(inDCL, inTagBits, inSyncBits) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPoolCOM::SAllocateUpdateDCLListDCL(
		Ref						self, 
		DCLCommand* 		inDCL, 
		DCLCommand**			inDCLCommandList,
		UInt32					inNumCommands)
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->AllocateUpdateDCLListDCL(inDCL, inDCLCommandList, inNumCommands) ;
	}
	
	DCLCommand*
	TraditionalDCLCommandPoolCOM::SAllocatePtrTimeStampDCL(
		Ref						self, 
		DCLCommand* 		inDCL, 
		UInt32*					inTimeStampPtr)
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->AllocatePtrTimeStampDCL(inDCL, inTimeStampPtr) ;
	}
	
	void
	TraditionalDCLCommandPoolCOM::SFree(
		Ref 	self, 
		DCLCommand* 				inDCL )
	{
		IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->Free(inDCL) ;
	}
	
	IOByteCount
	TraditionalDCLCommandPoolCOM::SGetSize(
		Ref 	self )
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->mStorageSize ;
	}
	
	Boolean
	TraditionalDCLCommandPoolCOM::SSetSize(
		Ref 	self, 
		IOByteCount 					inSize )
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->SetSize(inSize) ;
	}
	
	IOByteCount
	TraditionalDCLCommandPoolCOM::SGetBytesRemaining(
		Ref 	self )
	{
		return IOFireWireIUnknown::InterfaceMap<TraditionalDCLCommandPoolCOM>::GetThis(self)->mBytesRemaining ;
	}
}
