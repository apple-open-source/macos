/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").	You may not use this file except in compliance with the
 * License.	 Please obtain a copy of the License at
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
 *	IOFWUserClientPseudoAddressSpace.cpp
 *	IOFireWireFamily
 *
 *	Created by NWG on Wed Dec 06 2000.
 *	Copyright (c) 2000 Apple, Inc. All rights reserved.
 *
 */

#ifndef __IOFWUserClientPseuAddrSpace_H__
#define __IOFWUserClientPseuAddrSpace_H__

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOUserClient.h>

#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFireWireLink.h>

#include "IOFWUserClientPsdoAddrSpace.h"

class IOFireWireUserClient ;

// ============================================================
//
// IOFWPacketHeader
//
// ============================================================

IOFWPacketHeader_t::IOFWPacketHeader_t()
{
	CommonHeader.type			= IOFWPacketHeader::kFree ;
	CommonHeader.next			= this ;
	IOFWPacketHeaderGetSize(this)	= 0 ;
	IOFWPacketHeaderGetOffset(this) = 0 ;
}

inline IOByteCount& IOFWPacketHeaderGetSize(IOFWPacketHeader_t* hdr)
{
	return hdr->CommonHeader.args[1] ;
}

inline IOByteCount& IOFWPacketHeaderGetOffset(IOFWPacketHeader_t* hdr)
{
	return hdr->CommonHeader.args[2] ;
}

inline void InitIncomingPacketHeader(
	IOFWPacketHeader_t*				header,
	IOFWPacketHeader_t*				next,
	const IOByteCount				len,
	const IOByteCount				offset,
	OSAsyncReference*				ref,
	void*							refCon,
	UInt16							nodeID,
	const IOFWSpeed&   				speed,
	const FWAddress&				addr,
	const Boolean					lockWrite)
{
	header->CommonHeader.type				= IOFWPacketHeader::kIncomingPacket ;
	header->CommonHeader.next				= next ;
	IOFWPacketHeaderGetSize(header)			= len ;
	IOFWPacketHeaderGetOffset(header)		= offset ;
	header->CommonHeader.whichAsyncRef		= ref ;
	header->CommonHeader.argCount			= 8;

	header->IncomingPacket.commandID		= (UInt32) header ;
	header->IncomingPacket.nodeID			= nodeID ;
	header->IncomingPacket.speed			= speed ;
	header->IncomingPacket.addrHi			= addr.addressHi;
	header->IncomingPacket.addrLo			= addr.addressLo;
	header->IncomingPacket.lockWrite		= lockWrite ;
}

inline void InitSkippedPacketHeader(
	IOFWPacketHeader*				header,
	IOFWPacketHeader*				next,
	const IOByteCount				offset,
	OSAsyncReference*				ref,
	void*							refCon)
{
	header->CommonHeader.type 				= IOFWPacketHeader::kSkippedPacket ;
	header->CommonHeader.next				= next ;
	IOFWPacketHeaderGetSize(header)			= 0;
	IOFWPacketHeaderGetOffset(header)		= offset ;
	header->CommonHeader.whichAsyncRef		= ref ;
	header->CommonHeader.argCount			= 2;
	
	header->SkippedPacket.commandID			= (UInt32) header ;
	header->SkippedPacket.skippedPacketCount= 1;
}

inline void InitReadPacketHeader(
	IOFWPacketHeader*				header,
	IOFWPacketHeader*				next,
	IOByteCount						len,
	IOByteCount						offset,
	OSAsyncReference*				ref,
	IOFWRequestRefCon				reqrefcon,
	UInt16							nodeID,
	IOFWSpeed&						speed,
	FWAddress						addr,
	UInt32							generation)
{
	header->CommonHeader.type			= IOFWPacketHeader::kReadPacket ;
	header->CommonHeader.next			= next ;
	IOFWPacketHeaderGetSize(header)		= len ;
	IOFWPacketHeaderGetOffset(header)	= offset ;
	header->CommonHeader.whichAsyncRef	= ref ;
	header->CommonHeader.argCount		= 7 ;

	header->ReadPacket.commandID		= (UInt32) header ;
	header->ReadPacket.nodeID			= nodeID ;
	header->ReadPacket.speed			= speed ;
	header->ReadPacket.addrHi   		= addr.addressHi ;
	header->ReadPacket.addrLo			= addr.addressLo ;
	header->ReadPacket.reqrefcon		= reqrefcon ;
	header->ReadPacket.generation		= generation ;
}

inline Boolean IsSkippedPacketHeader(
	IOFWPacketHeader*				header)
{
	return header->CommonHeader.type == IOFWPacketHeader::kSkippedPacket ;
}

inline Boolean IsFreePacketHeader(
	IOFWPacketHeader*				header)
{
	return header->CommonHeader.type == IOFWPacketHeader::kFree ;
}

inline Boolean IsReadPacketHeader(
	IOFWPacketHeader*				header)
{
	return header->CommonHeader.type == IOFWPacketHeader::kReadPacket ;
}

//////////////////////////////////////////////////////////////
//
//	IOFWUserClientPseudoAddrSpace methods
//
//////////////////////////////////////////////////////////////

OSDefineMetaClassAndStructors(IOFWUserClientPseudoAddrSpace, IOFWPseudoAddressSpace) ;

bool
IOFWUserClientPseudoAddrSpace::initAll( 
	IOFireWireUserClient*		inUserClient, 
	FWAddrSpaceCreateParams* 	inParams)
{
	Boolean	status = true ;

	if (!IOFWPseudoAddressSpace::initAll(inUserClient->getOwner()->getBus(), & fAddress, 
				inParams->size, NULL, NULL, this))
		return false ;

	fUserRefCon					= inParams->refCon ;
	fFlags						= inParams->flags ;
	fWaitingForUserCompletion	= false ;

	// set user client
	fUserClient = inUserClient ;
	fUserClient->retain() ;

	// see if user specified a packet queue and queue size
	if ( !inParams->queueBuffer && !( (fFlags & kFWAddressSpaceAutoWriteReply) && (fFlags & kFWAddressSpaceAutoReadReply) ) )
	{
		IOFireWireUserClientLog_(("IOFWUserClientPseudoAddrSpace::initAll: address space without queue buffer must have both auto-write and auto-read set\n")) ;
		status = false ;
	}

	// make memory descriptor around queue
	if ( status )
	{
		if ( inParams->queueBuffer )
		{
			fPacketQueueBuffer = IOMemoryDescriptor::withAddress( (vm_address_t) inParams->queueBuffer,
																	(IOByteCount) inParams->queueSize,
																	kIODirectionOutIn,
																	fUserClient->getOwningTask() ) ;
			if ( !fPacketQueueBuffer )
				status = false ;
		
			if ( status )
			{
				status =  ( kIOReturnSuccess == fPacketQueueBuffer->prepare() ) ;
		
				fPacketQueuePrepared = status ;
			}

			if ( status )
				fBufferAvailable = fPacketQueueBuffer->getLength() ;
			
		}
	}
	
	if ( status )
	{		
		// init the easy vars
		fLastReadHeader 			= new IOFWPacketHeader ;
		fLastWrittenHeader			= fLastReadHeader ;
		
		// get a lock for the packet queue
		fLock = IOLockAlloc() ;
		
		if ( !fLock )
			status = false ;
	}
	
	// get a backing store if needed
	if ( status )
		if ( NULL != inParams->backingStore )
		{
			fDesc = IOMemoryDescriptor::withAddress( (vm_address_t) inParams->backingStore,
															(IOByteCount) inParams->size,
															kIODirectionOutIn,
															inUserClient->getOwningTask()) ;
			if (!fDesc)
				status = false ;
			
			if ( status )
				status = ( kIOReturnSuccess == fDesc->prepare() ) ;
			
			fBackingStorePrepared = status ;
		}
	
	// set reader and writer callbacks based on access flags and user callback flags
	if (status)
	{		
		if (!(inParams->flags & kFWAddressSpaceNoWriteAccess))
		{
			if (inParams->flags & kFWAddressSpaceAutoWriteReply)
			{
				if (inParams->backingStore)
					fWriter = & IOFWUserClientPseudoAddrSpace::simpleWriter ;
				else
				{	// this macro needs braces
					IOFireWireUserClientLog_(("IOFireWireUserClient::allocateAddressSpace(): can't create auto-write address space w/o backing store!\n")) ;
				}
			}
			else
				fWriter = & IOFWUserClientPseudoAddrSpace::pseudoAddrSpaceWriter ;
		}

		if (!(inParams->flags & kFWAddressSpaceNoReadAccess))
		{
			if (inParams->flags & kFWAddressSpaceAutoReadReply)
			{
				if (inParams->backingStore)
					fReader = & IOFWUserClientPseudoAddrSpace::simpleReader ;
				else
				{	// this macro needs braces
					IOFireWireUserClientLog_(("IOFireWireUserClient::allocateAddressSpace(): can't create auto-read address space w/o backing store!\n")) ;
				}
			}
			else
				fReader = & IOFWUserClientPseudoAddrSpace::pseudoAddrSpaceReader ;
		}

	}
	
	return status ;
}

void
IOFWUserClientPseudoAddrSpace::free()
{
//	IOLog("+IOFWUserClientPseudoAddrSpace::free %p\n", this) ;

	if ( fPacketQueuePrepared )
		fPacketQueueBuffer->complete() ;

	if ( fPacketQueueBuffer )
		fPacketQueueBuffer->release() ;

	if ( fBackingStorePrepared )
		fDesc->complete() ;

//		if (fDesc)						// initting the super class takes care of this for us...
//			fBackingStore->release() ;

	if ( fLastWrittenHeader )
		delete fLastWrittenHeader ;

	if ( fUserClient )
		fUserClient->release() ;		// we keep a reference to the user client which must be released.
	
	IOFWPseudoAddressSpace::free() ;
}

void
IOFWUserClientPseudoAddrSpace::deactivate()
{
	// zzz - this should clean up this address space, however there may be issues with this
	// code.

	fBufferAvailable = 0 ;	// zzz do we need locking here to protect our data?
	fLastReadHeader = NULL ;	
	
	IOFWPacketHeader*	firstHeader = fLastWrittenHeader ;
	IOFWPacketHeader*	tempHeader ;

	if (fLastWrittenHeader)
	{
		while (fLastWrittenHeader->CommonHeader.next != firstHeader)
		{
			tempHeader = fLastWrittenHeader->CommonHeader.next ;
			delete fLastWrittenHeader ;
			fLastWrittenHeader = tempHeader ;	
		}
	
	}
	
	if ( fBackingStorePrepared )
	{
		fDesc->complete() ;
		fBackingStorePrepared = false ;
	}

	IOFWPseudoAddressSpace::deactivate() ;
}

UInt32
IOFWUserClientPseudoAddrSpace::pseudoAddrSpaceReader(
	void*					refCon,
	UInt16					nodeID,
	IOFWSpeed&				speed,
	FWAddress				addr,
	UInt32					len,
	IOMemoryDescriptor**	buf,
	IOByteCount*			outOffset,
	IOFWRequestRefCon		reqrefcon)
{
	IOFWUserClientPseudoAddrSpace*	me = (IOFWUserClientPseudoAddrSpace*)refCon ;

	if ( 0 == me->fReadAsyncNotificationRef[0] )
		return kFWResponseTypeError ;
		
	IOLockLock( me->fLock ) ;

	IOFWPacketHeader*	currentHeader = me->fLastWrittenHeader ;

	if ( !IsFreePacketHeader(currentHeader) )
	{
		if ( !IsFreePacketHeader(currentHeader->CommonHeader.next) )
		{
			IOFWPacketHeader*	newHeader		= new IOFWPacketHeader ;
			newHeader->CommonHeader.next		= currentHeader->CommonHeader.next ;
			currentHeader->CommonHeader.next	= newHeader ;
		}
	
	}
	
	currentHeader = currentHeader->CommonHeader.next ;	

	UInt32 generation = me->fUserClient->getOwner()->getController()->getGeneration() ;

	// save info in header
	InitReadPacketHeader(
			currentHeader,
			currentHeader->CommonHeader.next,
			len,
			addr.addressLo - me->fAddress.addressLo,
			& (me->fReadAsyncNotificationRef),
			reqrefcon,
			nodeID,
			speed,
			addr,
			generation) ;
	
	me->fLastWrittenHeader = currentHeader ;

	IOLockUnlock( me->fLock ) ;
	
//	IOLog("sending notification for header %p, handling incoming packet\n", currentHeader) ;
	me->sendPacketNotification(currentHeader) ;

	return kFWResponsePending ;
}

UInt32
IOFWUserClientPseudoAddrSpace::pseudoAddrSpaceWriter(
	void*					refCon,
	UInt16					nodeID,
	IOFWSpeed&				speed,
	FWAddress				addr,
	UInt32					len,
	const void*				buf,
	IOFWRequestRefCon		reqrefcon)
{
	static UInt32 packetsWritten = 0 ;
	
	packetsWritten++ ;
	
	IOFWUserClientPseudoAddrSpace*	me = (IOFWUserClientPseudoAddrSpace*)refCon ;
	
//	IOLog("IOFWUserClientPseudoAddrSpace::pseudoAddrSpaceWriter: packets written=%u, addrSpace=%p\n", packetsWritten, me ) ;

	IOByteCount		destOffset	= 0 ;
	bool			wontFit		= false ;

	IOLockLock(me->fLock) ;		
	IOByteCount		spaceAtEnd	= me->fPacketQueueBuffer->getLength() ;
	
//	// create next header if it doesn't exist...

	IOFWPacketHeader*	currentHeader = me->fLastWrittenHeader ;

	spaceAtEnd -= (IOFWPacketHeaderGetOffset(currentHeader)
					 + IOFWPacketHeaderGetSize(currentHeader)) ;

	if (me->fBufferAvailable < len)
		wontFit = true ;
	else
	{
		if (len <= spaceAtEnd)
			destOffset = IOFWPacketHeaderGetOffset(currentHeader) + IOFWPacketHeaderGetSize(currentHeader) ;
		else
		{
			if ( (len + spaceAtEnd) <= me->fBufferAvailable )
				destOffset = 0 ;
			else
			{
				destOffset = IOFWPacketHeaderGetOffset(currentHeader) ;
			 wontFit = true ;
			}
		}
	}
	
	if (wontFit)
	{
		if (IsSkippedPacketHeader(currentHeader))
			currentHeader->SkippedPacket.skippedPacketCount++ ;
		else
		{
			if (!IsFreePacketHeader(currentHeader))
			{
				if ( !IsFreePacketHeader(currentHeader->CommonHeader.next) )
				{
					IOFWPacketHeader*	newHeader = new IOFWPacketHeader ;
					newHeader->CommonHeader.next = currentHeader->CommonHeader.next ;
					currentHeader->CommonHeader.next = newHeader ;
				}

				currentHeader = currentHeader->CommonHeader.next ;

			}

			InitSkippedPacketHeader(
					currentHeader,
					currentHeader->CommonHeader.next,
					destOffset,
					& (me->fSkippedPacketAsyncNotificationRef),
					(void*) currentHeader ) ;

	   }

	}
	else
	{
//		IOLog("currentHeader=%p, isfree=%u, next=%p, next->isfree=%u\n", currentHeader, IsFreePacketHeader(currentHeader), currentHeader->CommonHeader.next, IsFreePacketHeader(currentHeader->CommonHeader.next)) ;

		if (!IsFreePacketHeader(currentHeader))
		{
			if ( !IsFreePacketHeader(currentHeader->CommonHeader.next) )
			{
				IOFWPacketHeader*	newHeader		= new IOFWPacketHeader ;
				newHeader->CommonHeader.next		= currentHeader->CommonHeader.next ;
				currentHeader->CommonHeader.next	= newHeader ;

//				IOLog("creating new header %p, next=%p, insert after %p\n", newHeader, newHeader->CommonHeader.next, currentHeader) ;
			}

		}

		currentHeader = currentHeader->CommonHeader.next ;

		// save info in header
		InitIncomingPacketHeader(
				currentHeader,
				currentHeader->CommonHeader.next,
				len,
				destOffset,
				& (me->fPacketAsyncNotificationRef),
				(void*) currentHeader,//me->fUserRefCon,	// zzz don't know what this refcon was used for
				nodeID,
				speed,
				addr,
				me->fControl->isLockRequest(reqrefcon)) ;

//		IOLog("putting packet in header %p\n", currentHeader) ;

		// write packet to backing store
					
		// zzz this write should probably be eliminated when kFWAddressSpaceAutoCopyOnWrite is set..
		me->fPacketQueueBuffer->writeBytes(destOffset, buf, len) ;
		
		if ( me->fFlags & kFWAddressSpaceAutoCopyOnWrite )
			me->fDesc->writeBytes( addr.addressLo - me->fAddress.addressLo, buf, len ) ;

		me->fBufferAvailable -= len ;
		me->fLastWrittenHeader = currentHeader ;
		
	}
	
//	IOLog("sending packet notification header=%p, written packet\n", currentHeader) ;
	me->sendPacketNotification(currentHeader) ;

	IOLockUnlock(me->fLock) ;

	return kFWResponseComplete ;
}

void
IOFWUserClientPseudoAddrSpace::setAsyncRef_Packet(
	OSAsyncReference	asyncRef)
{
	bcopy(asyncRef, fPacketAsyncNotificationRef, sizeof(OSAsyncReference)) ;	
}

void
IOFWUserClientPseudoAddrSpace::setAsyncRef_SkippedPacket(
	OSAsyncReference	asyncRef)
{
	bcopy(asyncRef, fSkippedPacketAsyncNotificationRef, sizeof(OSAsyncReference)) ;
}

void
IOFWUserClientPseudoAddrSpace::setAsyncRef_Read(
	OSAsyncReference	asyncRef)
{
	bcopy(asyncRef, fReadAsyncNotificationRef, sizeof(OSAsyncReference)) ;
}

void
IOFWUserClientPseudoAddrSpace::clientCommandIsComplete(
	FWClientCommandID 	inCommandID,
	IOReturn			inResult)
{
	IOLockLock(fLock) ;

//	IOLog("IOFWUserClientPseudoAddrSpace::clientCommandIsComplete: inCommandID=%p\n", (void*)inCommandID) ;

	if ( fWaitingForUserCompletion )
	{
		IOFWPacketHeader*			oldHeader 	= fLastReadHeader ;
		IOFWPacketHeader::QueueTag	type 		= oldHeader->CommonHeader.type ;

//		IOLog("IOFWUserClientPseudoAddrSpace::clientCommandIsComplete: fLastReadHeader=%p, next=%p\n", fLastReadHeader, fLastReadHeader->CommonHeader.next) ;
		fLastReadHeader = fLastReadHeader->CommonHeader.next ;
				
		switch(type)
		{
			case IOFWPacketHeader::kIncomingPacket:
				fBufferAvailable += oldHeader->IncomingPacket.packetSize ;
				break ;
				
			case IOFWPacketHeader::kReadPacket:
				fUserClient->getOwner()->getController()->asyncReadResponse( oldHeader->ReadPacket.generation,
																			 oldHeader->ReadPacket.nodeID, 
																			 oldHeader->ReadPacket.speed,
																			 fDesc,//fBackingStore
																			 oldHeader->ReadPacket.addrLo - fAddress.addressLo,
																			 oldHeader->ReadPacket.packetSize,
																			 oldHeader->ReadPacket.reqrefcon ) ;
				break ;
				
			default:
				// nothing...
				break ;
		}
		
		oldHeader->CommonHeader.type = IOFWPacketHeader::kFree ;
		fWaitingForUserCompletion = false ;

		if ( fLastReadHeader->CommonHeader.type != IOFWPacketHeader::kFree )
		{
//			IOLog("IOFireWireUserClientPseudoAddrSpace::clientCommandIsComplete: send notification fLastReadHeader=%p\n", fLastReadHeader) ;
			sendPacketNotification(fLastReadHeader) ;
		}
		
	}

	IOLockUnlock(fLock) ;
}

void
IOFWUserClientPseudoAddrSpace::sendPacketNotification(
	IOFWPacketHeader*	inPacketHeader)
{
	static UInt32 notificationsSent = 0 ;
	
	if (!fWaitingForUserCompletion)
		if (inPacketHeader->CommonHeader.whichAsyncRef[0] != 0)
		{
//			IOLog("IOFWUserClientPseudoAddrSpace::sendPacketNotification: notificationsSent=%u, header=%p\n", ++notificationsSent, inPacketHeader) ;

			IOFireWireUserClient::sendAsyncResult(*(inPacketHeader->CommonHeader.whichAsyncRef),
							kIOReturnSuccess,
							(void**)inPacketHeader->CommonHeader.args,
							inPacketHeader->CommonHeader.argCount) ;
			fWaitingForUserCompletion = true ;
		}
}

#endif //__IOFWUserClientPseuAddrSpace_H__
