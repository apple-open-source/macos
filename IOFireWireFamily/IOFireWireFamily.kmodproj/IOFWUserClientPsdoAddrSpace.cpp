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
 *	IOFWUserClientPsdAddrSpace.cpp
 *	IOFireWireFamily
 *
 *	Created by NWG on Wed Dec 06 2000.
 *	Copyright (c) 2000-2002 Apple, Inc. All rights reserved.
 *
 */
/*
	$Log: IOFWUserClientPsdoAddrSpace.cpp,v $
	Revision 1.21  2002/08/06 21:10:06  wgulland
	Add IOfireWireBus::isCompleteRequest(reqrefcon)
	
	Revision 1.20  2002/08/06 19:44:57  niels
	*** empty log message ***
	
	Revision 1.19  2002/08/06 19:42:54  niels
	now send conflict response if user pseudo address space can't receive a write because the queue is full in cases where the hardware has not already responded 'ack complete'
	
*/

#ifndef __IOFWUserClientPseuAddrSpace_H__
#define __IOFWUserClientPseuAddrSpace_H__

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/firewire/IOFireWireNub.h>
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
	UInt16							nodeID,
	const IOFWSpeed&   				speed,
	const FWAddress&				addr,
	const bool						isLock)
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
	header->IncomingPacket.isLock			= isLock ;
}

inline void InitSkippedPacketHeader(
	IOFWPacketHeader*				header,
	IOFWPacketHeader*				next,
	const IOByteCount				offset,
	OSAsyncReference*				ref)
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

inline void InitLockPacketHeader(
	IOFWPacketHeader*				header,
	IOFWPacketHeader*				next,
	IOByteCount						len,
	IOByteCount						offset,
	OSAsyncReference*				ref,
	UInt16							nodeID,
	IOFWSpeed&						speed,
	FWAddress						addr,
	const UInt32					generation,
	IOFWRequestRefCon				reqrefcon)
{
	InitIncomingPacketHeader( header, next, len, offset, ref, nodeID, speed, addr, true ) ;	
	header->IncomingPacket.type			= IOFWPacketHeader::kLockPacket ;
	header->IncomingPacket.generation	= generation ;
	header->IncomingPacket.reqrefcon 	= reqrefcon;
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

// ============================================================
//	IOFWUserPseudoAddressSpace methods
// ============================================================

OSDefineMetaClassAndStructors( IOFWUserPseudoAddressSpace, IOFWPseudoAddressSpace ) ;

//#if IOFIREWIREUSERCLIENTDEBUG > 0
#if 0
bool
IOFWUserPseudoAddressSpace::serialize(OSSerialize *s) const
{
	if (s->previouslySerialized(this))
		return true ;
	
	char temp[256] ;
	
	sprintf(temp, "addr=%x:%08lx", fAddress.addressHi, fAddress.addressLo) ;
	sprintf(temp+strlen(temp), ", backing-store-bytes=%lud",
			fDesc ? fDesc->getLength() : 0) ;
	if ( fFlags )
	{
		sprintf(temp+strlen(temp), ", flags:") ;
		if (fFlags & kFWAddressSpaceNoWriteAccess)
			sprintf(temp+strlen(temp), " no-write") ;
		if (fFlags & kFWAddressSpaceNoReadAccess)
			sprintf(temp+strlen(temp), " no-read") ;
		if (fFlags & kFWAddressSpaceAutoWriteReply)
			sprintf(temp+strlen(temp), " auto-write") ;
		if (fFlags & kFWAddressSpaceAutoReadReply)
			sprintf(temp+strlen(temp), " auto-read") ;
		if (fFlags & kFWAddressSpaceAutoCopyOnWrite)
			sprintf(temp+strlen(temp), " copy-on-write") ;
		if (fFlags & kFWAddressSpaceShareIfExists)
			sprintf(temp+strlen(temp), " shared") ;
	}
	else
		sprintf(temp+strlen(temp), ", no flags") ;

	{
		OSString*	string = OSString::withCString(temp) ;
		if (!string)
			return false ;
			
		return string->serialize(s) ;
	}
}
#endif

void
IOFWUserPseudoAddressSpace::free()
{
	if ( fPacketQueuePrepared )
		fPacketQueueBuffer->complete() ;

	if ( fPacketQueueBuffer )
		fPacketQueueBuffer->release() ;

	if ( fBackingStorePrepared )
		fDesc->complete() ;

	if ( fLastWrittenHeader )
		delete fLastWrittenHeader ;

	IOFWPseudoAddressSpace::free() ;
}

void
IOFWUserPseudoAddressSpace::deactivate()
{
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

bool
IOFWUserPseudoAddressSpace::completeInit( 
	IOFireWireUserClient*		inUserClient, 
	FWAddrSpaceCreateParams* 	inParams)
{
	Boolean	status = true ;

	fUserRefCon					= inParams->refCon ;
	fFlags						= inParams->flags ;
	fWaitingForUserCompletion	= false ;

	// set user client
	fUserClient = inUserClient ;

	// see if user specified a packet queue and queue size
	if ( !inParams->queueBuffer && ( !(fFlags & kFWAddressSpaceAutoWriteReply) || !(fFlags & kFWAddressSpaceAutoReadReply) ) )
	{
		IOFireWireUserClientLog_("IOFWUserPseudoAddressSpace::initAll: address space without queue buffer must have both auto-write and auto-read set\n") ;
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
			{
				IOFireWireUserClientLog_("%s %u: couldn't make fPacketQueueBuffer memory descriptor\n", __FILE__, __LINE__) ;
				status = false ;
			}
			
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
		{
			IOFireWireUserClientLog_("%s %u: couldn't allocate lock\n", __FILE__, __LINE__) ;
			status = false ;
		}
	}
	
	// get a backing store if needed
	if ( status )
		if ( NULL != inParams->backingStore )
		{
			fDesc = IOMemoryDescriptor::withAddress( (vm_address_t) inParams->backingStore,
															(IOByteCount) inParams->size,
															kIODirectionOutIn,
															inUserClient->getOwningTask() ) ;
			if (!fDesc)
			{
				IOFireWireUserClientLog_("%s %u: failed to make backing store memory descriptor\n", __FILE__, __LINE__) ;
				status = false ;
			}
			
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
					fWriter = & IOFWUserPseudoAddressSpace::simpleWriter ;
				else
				{	// this macro needs braces
					IOFireWireUserClientLog_("IOFireWireUserClient::allocateAddressSpace(): can't create auto-write address space w/o backing store!\n") ;
				}
			}
			else
			{
				fWriter = & IOFWUserPseudoAddressSpace::pseudoAddrSpaceWriter ;
				fUserLocks = true ;
			}
		}

		if (!(inParams->flags & kFWAddressSpaceNoReadAccess))
		{
			if (inParams->flags & kFWAddressSpaceAutoReadReply)
			{
				if (inParams->backingStore)
					fReader = & IOFWUserPseudoAddressSpace::simpleReader ;
				else
				{	// this macro needs braces
					IOFireWireUserClientLog_("IOFireWireUserClient::allocateAddressSpace(): can't create auto-read address space w/o backing store!\n") ;
				}
			}
			else
			{
				fReader = & IOFWUserPseudoAddressSpace::pseudoAddrSpaceReader ;
				fUserLocks &= true ;	// &=, only set to true if true already
			}
		}

	}
	
	return status ;
}

bool
IOFWUserPseudoAddressSpace::initPseudo( 
	IOFireWireUserClient*		inUserClient, 
	FWAddrSpaceCreateParams* 	inParams)
{
	if ( !IOFWPseudoAddressSpace::initAll( inUserClient->getOwner()->getController(), & fAddress, inParams->size, NULL, NULL, this ))
	{
		IOFireWireUserClientLog_("IOFWUserPseudoAddressSpace::initPseudo: IOFWPseudoAddressSpace::initAll failed\n") ;
		return false ;
	}
	
	bool result = completeInit( inUserClient, inParams ) ;
	
	return result ;
}

bool
IOFWUserPseudoAddressSpace::initFixed(
	IOFireWireUserClient*		inUserClient,
	FWAddrSpaceCreateParams*	inParams )
{
	IOFWAddressSpace*	addrSpace = inUserClient->getOwner()->getController()->getAddressSpace( FWAddress( kCSRRegisterSpaceBaseAddressHi, inParams->addressLo ) ) ;
	
	fAddress = FWAddress( kCSRRegisterSpaceBaseAddressHi, inParams->addressLo ) ;

	if ( addrSpace && !(inParams->flags & kFWAddressSpaceShareIfExists ) )
		return false ;

	if ( !IOFWPseudoAddressSpace::initFixed( inUserClient->getOwner()->getController(), fAddress, inParams->size, NULL, NULL, this ))
		return false ;
	
	return completeInit( inUserClient, inParams ) ;
}

UInt32 IOFWUserPseudoAddressSpace::doLock(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 inLen,
                        const UInt32 *newVal, UInt32 &outLen, UInt32 *oldVal, UInt32 type,
                          IOFWRequestRefCon refcon)
{
	if ( fUserLocks )
	{
	    if(addr.addressHi != fBase.addressHi)
			return kFWResponseAddressError;
		if(addr.addressLo < fBase.addressLo)
			return kFWResponseAddressError;
		if(addr.addressLo + inLen > fBase.addressLo+fLen)
			return kFWResponseAddressError;
		if(!fReader)
			return kFWResponseTypeError;

		return doPacket( nodeID, speed, addr, inLen, newVal, refcon, IOFWPacketHeader::kLockPacket, oldVal ) ;
	}
	
	return IOFWPseudoAddressSpace::doLock( nodeID, speed, addr, inLen, newVal, outLen, oldVal, type, refcon ) ;
}		

UInt32
IOFWUserPseudoAddressSpace::doPacket(
	UInt16							nodeID,
	IOFWSpeed&						speed,
	FWAddress						addr,
	UInt32							len,
	const void*						buf,
	IOFWRequestRefCon				reqrefcon,
	IOFWPacketHeader::QueueTag		tag,
	UInt32*							oldVal)	// oldVal only used in lock case
{
	IOByteCount		destOffset	= 0 ;
	bool			wontFit		= false ;
	UInt32			response	= kFWResponseComplete ;
	IOFWPacketHeader*	currentHeader = fLastWrittenHeader ;

	IOLockLock(fLock) ;
	
	if ( tag == IOFWPacketHeader::kIncomingPacket || tag == IOFWPacketHeader::kLockPacket )
	{
		IOByteCount		spaceAtEnd	= fPacketQueueBuffer->getLength() ;

		spaceAtEnd -= (IOFWPacketHeaderGetOffset(currentHeader)
						+ IOFWPacketHeaderGetSize(currentHeader)) ;
	
		if ( fBufferAvailable < len )
        {
			wontFit = true ;
        }
		else
		{
			if (len <= spaceAtEnd)
				destOffset = IOFWPacketHeaderGetOffset(currentHeader) + IOFWPacketHeaderGetSize(currentHeader) ;
			else
			{
				if ( (len + spaceAtEnd) <= fBufferAvailable )
					destOffset = 0 ;
				else
				{
					destOffset = IOFWPacketHeaderGetOffset(currentHeader) ;
					wontFit = true ;
				}
			}
		}
	}
	
	if (wontFit)
	{
		if (IsSkippedPacketHeader(currentHeader))
			++(currentHeader->SkippedPacket.skippedPacketCount) ;
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
					& fSkippedPacketAsyncNotificationRef ) ;
	
		}

		// if we can't handle the packet, and the hardware hasn't already responded,
		// send kFWResponseConflictError
		if ( ! fUserClient->getOwner()->getController()->isCompleteRequest( reqrefcon ) )
			response = kFWResponseConflictError ;
	}
	else
	{
		if (!IsFreePacketHeader(currentHeader))
		{
			if ( !IsFreePacketHeader(currentHeader->CommonHeader.next) )
			{
				IOFWPacketHeader*	newHeader		= new IOFWPacketHeader ;
				newHeader->CommonHeader.next		= currentHeader->CommonHeader.next ;
				currentHeader->CommonHeader.next	= newHeader ;
			}

		}

		currentHeader = currentHeader->CommonHeader.next ;

		switch(tag)
		{
			case IOFWPacketHeader::kIncomingPacket:
				// save info in header
				InitIncomingPacketHeader(
						currentHeader,
						currentHeader->CommonHeader.next,
						len,
						destOffset,
						& fPacketAsyncNotificationRef,
						nodeID,
						speed,
						addr) ;

				// zzz this write should probably be eliminated when kFWAddressSpaceAutoCopyOnWrite is set..
				fPacketQueueBuffer->writeBytes(destOffset, buf, len) ;
				
				// write packet to backing store
//				if ( (fFlags & kFWAddressSpaceAutoCopyOnWrite) != 0 )
//					fDesc->writeBytes( addr.addressLo - fAddress.addressLo, buf, len ) ;
		
				fBufferAvailable -= len ;
					
				break ;
				
			case IOFWPacketHeader::kLockPacket:
				{
					// save info in header
					InitLockPacketHeader(
							currentHeader,
							currentHeader->CommonHeader.next,
							len,
							destOffset,
							& fPacketAsyncNotificationRef,
							nodeID,
							speed,
							addr,
							fUserClient->getOwner()->getController()->getGeneration(),
							reqrefcon ) ;
	
					// copy data to queue
					fPacketQueueBuffer->writeBytes(destOffset, buf, len) ;	
					fBufferAvailable -= len ;
					response = kFWResponsePending ;
				}
				break ;

			case IOFWPacketHeader::kReadPacket:
				InitReadPacketHeader(
						currentHeader,
						currentHeader->CommonHeader.next,
						len,
						addr.addressLo - fAddress.addressLo,
						& fReadAsyncNotificationRef,
						reqrefcon,
						nodeID,
						speed,
						addr,
						fUserClient->getOwner()->getController()->getGeneration() ) ;

				response = kFWResponsePending ;
			
				break ;
			
			default:
				IOLog("%s %u: internal error: doPacket called with improper type\n", __FILE__, __LINE__) ;
				break ;
		}


		fLastWrittenHeader = currentHeader ;
	}
	
	sendPacketNotification(currentHeader) ;

	IOLockUnlock(fLock) ;

	return response ;
}

UInt32
IOFWUserPseudoAddressSpace::pseudoAddrSpaceReader(
	void*					refCon,
	UInt16					nodeID,
	IOFWSpeed&				speed,
	FWAddress				addr,
	UInt32					len,
	IOMemoryDescriptor**	buf,
	IOByteCount*			outOffset,
	IOFWRequestRefCon		reqrefcon)
{
	IOFWUserPseudoAddressSpace*	me = (IOFWUserPseudoAddressSpace*)refCon ;

	if ( 0 == me->fReadAsyncNotificationRef[0] )
		return kFWResponseTypeError ;
		
	return me->doPacket( nodeID, speed, addr, len, buf, reqrefcon, IOFWPacketHeader::kReadPacket) ;
}

UInt32
IOFWUserPseudoAddressSpace::pseudoAddrSpaceWriter(
	void*					refCon,
	UInt16					nodeID,
	IOFWSpeed&				speed,
	FWAddress				addr,
	UInt32					len,
	const void*				buf,
	IOFWRequestRefCon		reqrefcon)
{
	IOFWUserPseudoAddressSpace*	me = (IOFWUserPseudoAddressSpace*)refCon ;
	
	return me->doPacket( nodeID, speed, addr, len, buf, reqrefcon, IOFWPacketHeader::kIncomingPacket ) ;
}

void
IOFWUserPseudoAddressSpace::setAsyncRef_Packet(
	OSAsyncReference	asyncRef)
{
	bcopy(asyncRef, fPacketAsyncNotificationRef, sizeof(OSAsyncReference)) ;	
}

void
IOFWUserPseudoAddressSpace::setAsyncRef_SkippedPacket(
	OSAsyncReference	asyncRef)
{
	bcopy(asyncRef, fSkippedPacketAsyncNotificationRef, sizeof(OSAsyncReference)) ;
}

void
IOFWUserPseudoAddressSpace::setAsyncRef_Read(
	OSAsyncReference	asyncRef)
{
	bcopy(asyncRef, fReadAsyncNotificationRef, sizeof(OSAsyncReference)) ;
}

void
IOFWUserPseudoAddressSpace::clientCommandIsComplete(
	FWClientCommandID 	inCommandID,
	IOReturn			inResult)
{
	IOLockLock(fLock) ;

	if ( fWaitingForUserCompletion )
	{
		IOFWPacketHeader*			oldHeader 	= fLastReadHeader ;
		IOFWPacketHeader::QueueTag	type 		= oldHeader->CommonHeader.type ;
		fLastReadHeader = fLastReadHeader->CommonHeader.next ;
				
		switch(type)
		{
			case IOFWPacketHeader::kIncomingPacket:
				fBufferAvailable += oldHeader->IncomingPacket.packetSize ;
				break ;
				
			case IOFWPacketHeader::kReadPacket:
                {
                    fUserClient->getOwner()->getController()->asyncReadResponse( oldHeader->ReadPacket.generation,
                                                                                oldHeader->ReadPacket.nodeID, 
                                                                                oldHeader->ReadPacket.speed,
                                                                                fDesc,//fBackingStore
                                                                                oldHeader->ReadPacket.addrLo - fAddress.addressLo,
                                                                                oldHeader->ReadPacket.packetSize,
                                                                                oldHeader->ReadPacket.reqrefcon ) ;
				}
                break ;

			case IOFWPacketHeader::kLockPacket:
				{
					fUserClient->getOwner()->getController()->asyncLockResponse( oldHeader->IncomingPacket.generation,
																				oldHeader->IncomingPacket.nodeID, 
																				oldHeader->IncomingPacket.speed,
																				fDesc,//fBackingStore
																				oldHeader->IncomingPacket.addrLo - fAddress.addressLo,
																				oldHeader->IncomingPacket.packetSize >> 1,
																				oldHeader->IncomingPacket.reqrefcon ) ;
				}
				break ;
				
			default:
				// nothing...
				break ;
		}
		
		oldHeader->CommonHeader.type = IOFWPacketHeader::kFree ;
		fWaitingForUserCompletion = false ;

		if ( fLastReadHeader->CommonHeader.type != IOFWPacketHeader::kFree )
			sendPacketNotification(fLastReadHeader) ;
		
	}

	IOLockUnlock(fLock) ;
}

void
IOFWUserPseudoAddressSpace::sendPacketNotification(
	IOFWPacketHeader*	inPacketHeader)
{	
	if (!fWaitingForUserCompletion)
	{
		if ( inPacketHeader->CommonHeader.type == IOFWPacketHeader::kIncomingPacket and (fFlags & kFWAddressSpaceAutoCopyOnWrite) != 0 )
		{
			IOByteCount len = IOFWPacketHeaderGetSize(inPacketHeader) ;
			fDesc->writeBytes(	inPacketHeader->IncomingPacket.addrLo - fAddress.addressLo, 
								fPacketQueueBuffer->getVirtualSegment( IOFWPacketHeaderGetOffset( inPacketHeader ), &len ),
								IOFWPacketHeaderGetSize( inPacketHeader ) ) ;
		}
		
		if (inPacketHeader->CommonHeader.whichAsyncRef[0])
		{
			IOFireWireUserClient::sendAsyncResult(*(inPacketHeader->CommonHeader.whichAsyncRef),
							kIOReturnSuccess,
							(void**)inPacketHeader->CommonHeader.args,
							inPacketHeader->CommonHeader.argCount) ;
			fWaitingForUserCompletion = true ;
		}
	}
}

#endif //__IOFWUserClientPseuAddrSpace_H__
