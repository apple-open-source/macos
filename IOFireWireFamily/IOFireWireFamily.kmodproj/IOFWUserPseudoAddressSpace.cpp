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
	$Log: IOFWUserPseudoAddressSpace.cpp,v $
	Revision 1.20  2007/02/16 19:03:43  arulchan
	*** empty log message ***
	
	Revision 1.19  2007/02/14 21:58:29  collin
	*** empty log message ***
	
	Revision 1.18  2007/02/07 06:35:20  collin
	*** empty log message ***
	
	Revision 1.17  2007/01/24 04:10:13  collin
	*** empty log message ***
	
	Revision 1.16  2007/01/18 01:07:32  collin
	*** empty log message ***
	
	Revision 1.15  2007/01/17 03:46:26  collin
	*** empty log message ***
	
	Revision 1.14  2006/12/21 21:17:44  ayanowit
	More changes necessary to eventually get support for 64-bit apps working (4222965).
	
	Revision 1.13  2006/12/06 00:01:07  arulchan
	Isoch Channel 31 Generic Receiver
	
	Revision 1.12  2005/02/18 03:19:03  niels
	fix isight
	
	Revision 1.11  2003/09/20 00:54:17  collin
	*** empty log message ***
	
	Revision 1.10  2003/08/30 00:16:44  collin
	*** empty log message ***
	
	Revision 1.9  2003/08/20 23:33:37  niels
	*** empty log message ***
	
	Revision 1.8  2003/08/19 01:48:54  niels
	*** empty log message ***
	
	Revision 1.7  2003/07/24 06:30:58  collin
	*** empty log message ***
	
	Revision 1.6  2003/07/21 06:52:59  niels
	merge isoch to TOT
	
	Revision 1.4.4.2  2003/07/21 06:44:44  niels
	*** empty log message ***
	
	Revision 1.4.4.1  2003/07/01 20:54:07  niels
	isoch merge
	
	Revision 1.4  2003/04/18 20:32:06  collin
	*** empty log message ***
	
	Revision 1.3  2002/10/22 00:34:25  collin
	fix user space lock transactions, publish GUID = 0 property in objects in the IOFireWire plane
	
	Revision 1.2  2002/10/18 23:29:43  collin
	fix includes, fix cast which fails on new compiler
	
	Revision 1.1  2002/09/25 00:27:22  niels
	flip your world upside-down
	
	Revision 1.21  2002/08/06 21:10:06  wgulland
	Add IOfireWireBus::isCompleteRequest(reqrefcon)
	
	Revision 1.20  2002/08/06 19:44:57  niels
	*** empty log message ***
	
	Revision 1.19  2002/08/06 19:42:54  niels
	now send conflict response if user pseudo address space can't receive a write because the queue is full in cases where the hardware has not already responded 'ack complete'
	
*/

#ifndef __IOFWUserClientPseuAddrSpace_H__
#define __IOFWUserClientPseuAddrSpace_H__

#import <IOKit/firewire/IOFireWireNub.h>
#import <IOKit/firewire/IOFireWireController.h>

// protected
#import <IOKit/firewire/IOFireWireLink.h>

// private
#import "IOFireWireUserClient.h"
#import "IOFireWireLib.h"
#import "IOFWUserPseudoAddressSpace.h"

// system
#import <IOKit/assert.h>
#import <IOKit/IOLib.h>
#import <IOKit/IOWorkLoop.h>
#import <IOKit/IOTypes.h>
#import <IOKit/IOMessage.h>
#import <IOKit/IOUserClient.h>

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

io_user_reference_t& IOFWPacketHeaderGetSize(IOFWPacketHeader_t* hdr)
{
	return hdr->CommonHeader.args[1] ;
}

io_user_reference_t& IOFWPacketHeaderGetOffset(IOFWPacketHeader_t* hdr)
{
	return hdr->CommonHeader.args[2] ;
}

void InitIncomingPacketHeader(
	IOFWPacketHeader_t*				header,
	IOFWPacketHeader_t*				next,
	const IOByteCount				len,
	const IOByteCount				offset,
	OSAsyncReference64*				ref,
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

void InitSkippedPacketHeader(
	IOFWPacketHeader*				header,
	IOFWPacketHeader*				next,
	const IOByteCount				offset,
	OSAsyncReference64*				ref)
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

void InitReadPacketHeader(
	IOFWPacketHeader*				header,
	IOFWPacketHeader*				next,
	IOByteCount						len,
	IOByteCount						offset,
	OSAsyncReference64*				ref,
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
	header->ReadPacket.reqrefcon		= (io_user_reference_t)reqrefcon ;
	header->ReadPacket.generation		= generation ;
}

void InitLockPacketHeader(
	IOFWPacketHeader*				header,
	IOFWPacketHeader*				next,
	IOByteCount						len,
	IOByteCount						offset,
	OSAsyncReference64*				ref,
	UInt16							nodeID,
	IOFWSpeed&						speed,
	FWAddress						addr,
	const UInt32					generation,
	IOFWRequestRefCon				reqrefcon)
{
	InitIncomingPacketHeader( header, next, len, offset, ref, nodeID, speed, addr, true ) ;	
	header->IncomingPacket.type			= IOFWPacketHeader::kLockPacket ;
	header->IncomingPacket.generation	= generation ;
	header->IncomingPacket.reqrefcon 	= (io_user_reference_t)reqrefcon;
}

Boolean IsSkippedPacketHeader(
	IOFWPacketHeader*				header)
{
	return header->CommonHeader.type == IOFWPacketHeader::kSkippedPacket ;
}

Boolean IsFreePacketHeader(
	IOFWPacketHeader*				header)
{
	return header->CommonHeader.type == IOFWPacketHeader::kFree ;
}

// ============================================================
//	IOFWUserPseudoAddressSpace methods
// ============================================================

OSDefineMetaClassAndStructors( IOFWUserPseudoAddressSpace, IOFWPseudoAddressSpace ) ;

#if IOFIREWIREUSERCLIENTDEBUG > 0

bool
IOFWUserPseudoAddressSpace::serialize(OSSerialize *s) const
{
	if (s->previouslySerialized(this))
		return true ;
	
	char temp[256] ;
	
	snprintf(temp, sizeof(temp), "addr=%x:%08lx", fAddress.addressHi, fAddress.addressLo) ;
	snprintf(temp+strlen(temp), sizeof(temp), ", backing-store-bytes=%lud",
			fDesc ? fDesc->getLength() : 0) ;
	if ( fFlags )
	{
		snprintf(temp+strlen(temp), sizeof(temp), ", flags:") ;
		if (fFlags & kFWAddressSpaceNoWriteAccess)
			snprintf(temp+strlen(temp), sizeof(temp), " no-write") ;
		if (fFlags & kFWAddressSpaceNoReadAccess)
			snprintf(temp+strlen(temp), sizeof(temp), " no-read") ;
		if (fFlags & kFWAddressSpaceAutoWriteReply)
			snprintf(temp+strlen(temp), sizeof(temp), " auto-write") ;
		if (fFlags & kFWAddressSpaceAutoReadReply)
			snprintf(temp+strlen(temp), sizeof(temp), " auto-read") ;
		if (fFlags & kFWAddressSpaceAutoCopyOnWrite)
			snprintf(temp+strlen(temp), sizeof(temp), " copy-on-write") ;
		if (fFlags & kFWAddressSpaceShareIfExists)
			snprintf(temp+strlen(temp), sizeof(temp), " shared") ;
		if (fFlags & kFWAddressSpaceExclusive)
			snprintf(temp+strlen(temp), sizeof(temp), " exclusive") ;			
	}
	else
	{
		snprintf(temp+strlen(temp), sizeof(temp), ", no flags") ;
	}
	
	OSString*	string = OSString::withCString(temp) ;
	if (!string)
		return false ;
		
	bool result =  string->serialize(s) ;
	string->release() ;
	
	return result ;
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

	delete fLastWrittenHeader ;

	if( fLock )
	{
		IOLockFree( fLock );
		fLock = NULL;
	}
	
	IOFWPseudoAddressSpace::free() ;
}

// exporterCleanup
//
//

void
IOFWUserPseudoAddressSpace::exporterCleanup( const OSObject * self )
{
	IOFWUserPseudoAddressSpace * me = (IOFWUserPseudoAddressSpace*)self;
	
	DebugLog("IOFWUserPseudoAddressSpace::exporterCleanup\n");
	
	me->deactivate();
}

void
IOFWUserPseudoAddressSpace::deactivate()
{
	IOFWPseudoAddressSpace::deactivate() ;
	
	IOLockLock(fLock) ;
	
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
	
	fWaitingForUserCompletion = false ;
	
	IOLockUnlock(fLock) ;
}

bool
IOFWUserPseudoAddressSpace::completeInit( IOFireWireUserClient* userclient, AddressSpaceCreateParams* params )
{
	Boolean	status = true ;

	fUserRefCon					= params->refCon ;
	fFlags						= params->flags ;
	fWaitingForUserCompletion	= false ;

	// set user client
	fUserClient = userclient ;

	// see if user specified a packet queue and queue size
	if ( !params->queueBuffer && ( !(fFlags & kFWAddressSpaceAutoWriteReply) || !(fFlags & kFWAddressSpaceAutoReadReply) ) )
	{
		DebugLog("IOFWUserPseudoAddressSpace::initAll: address space without queue buffer must have both auto-write and auto-read set\n") ;
		status = false ;
	}

	// make memory descriptor around queue
	if ( status )
	{
		if ( params->queueBuffer )
		{
			fPacketQueueBuffer = IOMemoryDescriptor::withAddressRange( params->queueBuffer,
																	params->queueSize,
																	kIODirectionOutIn,
																	fUserClient->getOwningTask() ) ;
			if ( !fPacketQueueBuffer )
			{
				DebugLog("%s %u: couldn't make fPacketQueueBuffer memory descriptor\n", __FILE__, __LINE__) ;
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
			DebugLog("%s %u: couldn't allocate lock\n", __FILE__, __LINE__) ;
			status = false ;
		}
	}
	
	// get a backing store if needed
	if ( status )
		if ( NULL != params->backingStore )
		{
			fDesc = IOMemoryDescriptor::withAddressRange(	params->backingStore,
															params->size,
															kIODirectionOutIn,
															userclient->getOwningTask() ) ;
			if (!fDesc)
			{
				DebugLog("%s %u: failed to make backing store memory descriptor\n", __FILE__, __LINE__) ;
				status = false ;
			}
			
			if ( status )
				status = ( kIOReturnSuccess == fDesc->prepare() ) ;
			
			fBackingStorePrepared = status ;
		}
	
	// set reader and writer callbacks based on access flags and user callback flags
	if (status)
	{		
		if (!(params->flags & kFWAddressSpaceNoWriteAccess))
		{
			if (params->flags & kFWAddressSpaceAutoWriteReply)
			{
				if (params->backingStore)
					fWriter = & IOFWUserPseudoAddressSpace::simpleWriter ;
				else
				{	// this macro needs braces
					DebugLog("IOFireWireUserClient::allocateAddressSpace(): can't create auto-write address space w/o backing store!\n") ;
				}
			}
			else
			{
				fWriter = & IOFWUserPseudoAddressSpace::pseudoAddrSpaceWriter ;
				fUserLocks = true ;
			}
		}

		if (!(params->flags & kFWAddressSpaceNoReadAccess))
		{
			if (params->flags & kFWAddressSpaceAutoReadReply)
			{
				if (params->backingStore)
					fReader = & IOFWUserPseudoAddressSpace::simpleReader ;
				else
				{	// this macro needs braces
					DebugLog("IOFireWireUserClient::allocateAddressSpace(): can't create auto-read address space w/o backing store!\n") ;
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
	IOFireWireUserClient*		userclient, 
	IOFireWireLib::AddressSpaceCreateParams* 	params)
{
	if ( !IOFWPseudoAddressSpace::initAll( userclient->getOwner()->getController(), & fAddress, params->size, NULL, NULL, this ))
	{
		DebugLog("IOFWUserPseudoAddressSpace::initPseudo: IOFWPseudoAddressSpace::initAll failed\n") ;
		return false ;
	}
	
	bool result = completeInit( userclient, params ) ;
	
	return result ;
}

bool
IOFWUserPseudoAddressSpace::initFixed(
	IOFireWireUserClient*		userclient,
	IOFireWireLib::AddressSpaceCreateParams*	params )
{
	IOFWAddressSpace*	addrSpace = userclient->getOwner()->getController()->getAddressSpace( FWAddress( kCSRRegisterSpaceBaseAddressHi, params->addressLo ) ) ;
	
	fAddress = FWAddress( kCSRRegisterSpaceBaseAddressHi, params->addressLo ) ;

	if ( addrSpace && !(params->flags & kFWAddressSpaceShareIfExists ) )
		return false ;

	if ( !IOFWPseudoAddressSpace::initFixed( userclient->getOwner()->getController(), fAddress, params->size, NULL, NULL, this ))
		return false ;
	
	// mark this address space as exclusve
	// it will fail in activate if there's a conflict
	if( params->flags & kFWAddressSpaceExclusive )
	{
		setExclusive( true );
	}
		
	return completeInit( userclient, params ) ;
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

	IOLockLock(fLock) ;
	
	IOFWPacketHeader*	currentHeader = fLastWrittenHeader ;

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
			
			fLastWrittenHeader = currentHeader ;
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
	
	if( currentHeader->CommonHeader.type != IOFWPacketHeader::kFree )
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
	OSAsyncReference64	asyncRef)
{
	bcopy(asyncRef, fPacketAsyncNotificationRef, sizeof(OSAsyncReference64)) ;	
}

void
IOFWUserPseudoAddressSpace::setAsyncRef_SkippedPacket(
	OSAsyncReference64	asyncRef)
{
	bcopy(asyncRef, fSkippedPacketAsyncNotificationRef, sizeof(OSAsyncReference64)) ;
}

void
IOFWUserPseudoAddressSpace::setAsyncRef_Read(
	OSAsyncReference64	asyncRef)
{
	bcopy(asyncRef, fReadAsyncNotificationRef, sizeof(OSAsyncReference64)) ;
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
			case IOFWPacketHeader::kLockPacket:
				{
					fUserClient->getOwner()->getController()->asyncLockResponse( oldHeader->IncomingPacket.generation,
																				oldHeader->IncomingPacket.nodeID, 
																				oldHeader->IncomingPacket.speed,
																				fDesc,//fBackingStore
																				oldHeader->IncomingPacket.addrLo - fAddress.addressLo,
																				oldHeader->IncomingPacket.packetSize >> 1,
																				(void*)oldHeader->IncomingPacket.reqrefcon ) ;
				}
				
				// fall through
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
                                                                                (void*)oldHeader->ReadPacket.reqrefcon ) ;
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
			void * bytes = IOMalloc( len );
		
			fPacketQueueBuffer->readBytes( IOFWPacketHeaderGetOffset( inPacketHeader ), bytes, len );
			
			fDesc->writeBytes(	inPacketHeader->IncomingPacket.addrLo - fAddress.addressLo, 
								bytes,
								len ) ;

			IOFree( bytes, len );
		}
		
		if (inPacketHeader->CommonHeader.whichAsyncRef[0])
		{
			IOFireWireUserClient::sendAsyncResult64(*(inPacketHeader->CommonHeader.whichAsyncRef),
							kIOReturnSuccess,
							(io_user_reference_t*)inPacketHeader->CommonHeader.args,
							inPacketHeader->CommonHeader.argCount) ;
			fWaitingForUserCompletion = true ;
		}
	}
}

#endif //__IOFWUserClientPseuAddrSpace_H__
