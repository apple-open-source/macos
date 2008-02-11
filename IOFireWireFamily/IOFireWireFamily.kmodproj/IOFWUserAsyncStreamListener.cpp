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
 *	IOFWUserAsyncStreamListener.cpp
 *	IOFireWireFamily
 *
 *	Created by Arul on Wed Nov 08 2006.
 *	Copyright (c) 2006 Apple, Inc. All rights reserved.
 *
 */
/*
	$Log: IOFWUserAsyncStreamListener.cpp,v $
	Revision 1.5.6.1  2007/11/09 19:57:30  arulchan
	.
	
	Revision 1.5  2007/02/16 19:03:43  arulchan
	*** empty log message ***
	
	Revision 1.4  2007/02/07 06:35:20  collin
	*** empty log message ***
	
	Revision 1.3  2007/01/15 23:29:04  arulchan
	Fixed Skipped Packet Handler Notifications
	
	Revision 1.2  2006/12/21 21:17:44  ayanowit
	More changes necessary to eventually get support for 64-bit apps working (4222965).
	
	Revision 1.1  2006/12/06 00:03:28  arulchan
	Isoch Channel 31 Receiver
	
*/

#import <IOKit/firewire/IOFireWireNub.h>
#import <IOKit/firewire/IOFireWireController.h>


// protected
#import <IOKit/firewire/IOFireWireLink.h>

// private
#import "IOFireWireUserClient.h"
#import "IOFireWireLib.h"
#import "IOFWUserPseudoAddressSpace.h"
#import "IOFWUserAsyncStreamListener.h"

// system
#import <IOKit/assert.h>
#import <IOKit/IOLib.h>
#import <IOKit/IOWorkLoop.h>
#import <IOKit/IOTypes.h>
#import <IOKit/IOMessage.h>
#import <IOKit/IOUserClient.h>

extern void InitSkippedPacketHeader(
	IOFWPacketHeader*				header,
	IOFWPacketHeader*				next,
	const IOByteCount				offset,
	OSAsyncReference64*				ref);

Boolean IsAsyncStreamSkippedPacketHeader(
	IOFWPacketHeader*				header)
{
	return header->CommonHeader.type == IOFWPacketHeader::kSkippedPacket ;
}

Boolean IsAsyncStreamFreePacketHeader(
	IOFWPacketHeader*				header)
{
	return header->CommonHeader.type == IOFWPacketHeader::kFree ;
}

// ============================================================
//	IOFWUserAsyncStreamListener methods
// ============================================================

OSDefineMetaClassAndStructors( IOFWUserAsyncStreamListener, IOFWAsyncStreamListener ) ;

#if IOFIREWIREUSERCLIENTDEBUG > 0

bool
IOFWUserAsyncStreamListener::serialize(OSSerialize *s) const
{
	if (s->previouslySerialized(this))
		return true ;
	
	char temp[256] ;
	
	if ( fFlags )
	{
		snprintf(temp+strlen(temp), sizeof(temp), ", flags:") ;
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
IOFWUserAsyncStreamListener::free()
{
	if ( fPacketQueuePrepared )
		fPacketQueueBuffer->complete() ;

	if ( fPacketQueueBuffer )
		fPacketQueueBuffer->release() ;

	delete fLastWrittenHeader ;

	if( fLock )
	{
		IOLockFree( fLock );
		fLock = NULL;
	}

	IOFWAsyncStreamListener::free() ;
}

// exporterCleanup
//
//

void
IOFWUserAsyncStreamListener::exporterCleanup( OSObject * self, IOFWUserObjectExporter * exporter )
{
	IOFWUserAsyncStreamListener * me = (IOFWUserAsyncStreamListener*)self;
	
	me->deactivate();

	((IOFireWireUserClient*)exporter->getOwner())->getOwner()->getController()->removeAsyncStreamListener(me);
}

void
IOFWUserAsyncStreamListener::deactivate()
{
	IOFWAsyncStreamListener::TurnOffNotification() ;
	
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
		
	fWaitingForUserCompletion = false ;
	
	IOLockUnlock(fLock) ;
}

bool
IOFWUserAsyncStreamListener::completeInit( IOFireWireUserClient* userclient, FWUserAsyncStreamListenerCreateParams* params )
{
	Boolean	status = true ;


	fUserRefCon					= params->refCon ;
	fFlags						= params->flags ;
	fWaitingForUserCompletion	= false ;

	// set user client
	fUserClient = userclient ;

	// see if user specified a packet queue and queue size
	if ( !params->queueBuffer )
	{
		DebugLog("IOFWUserAsyncStreamListener::initAll: async stream listener without queue buffer\n") ;
		status = false ;
	}
	
	// make memory descriptor around queue
	if ( status )
	{
		if ( params->queueBuffer )
		{
			fPacketQueueBuffer = IOMemoryDescriptor::withAddressRange(	params->queueBuffer,
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

		fLastWrittenHeader->CommonHeader.whichAsyncRef = NULL;
		
		// get a lock for the packet queue
		fLock = IOLockAlloc() ;
		
		if ( !fLock )
		{
			DebugLog("%s %u: couldn't allocate lock\n", __FILE__, __LINE__) ;
			status = false ;
		}
	}
	
	
	if (status)
	{		
		fUserLocks = true ;
	}
	
	return status ;
}

bool
IOFWUserAsyncStreamListener::initAsyncStreamListener( 
			IOFireWireUserClient									*userclient, 
			IOFireWireLib::FWUserAsyncStreamListenerCreateParams	*params)
{
	if ( !IOFWUserAsyncStreamListener::initAll( userclient->getOwner()->getController(), 
												params->channel, 
												IOFWUserAsyncStreamListener::asyncStreamListenerHandler, 
												this ))
	{
		DebugLog("IOFWUserAsyncStreamListener::initAsyncStreamListener failed\n") ;
		return false ;
	}
	
	bool result = completeInit( userclient, params ) ;
	
	return result ;
}


void
IOFWUserAsyncStreamListener::doPacket(
	UInt32							len,
	const void*						buf,
	IOFWPacketHeader::QueueTag		tag,
	UInt32*							oldVal)	// oldVal only used in lock case
{
	IOByteCount		destOffset	= 0 ;
	bool			wontFit		= false ;

	IOLockLock(fLock) ;
	
	IOFWPacketHeader*	currentHeader = fLastWrittenHeader ;

	if ( tag == IOFWPacketHeader::kIncomingPacket )
	{
		IOByteCount		spaceAtEnd	= fPacketQueueBuffer->getLength() ;

		spaceAtEnd -= (IOFWPacketHeaderGetOffset(currentHeader) + IOFWPacketHeaderGetSize(currentHeader)) ;
	
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
		if (IsAsyncStreamSkippedPacketHeader(currentHeader))
		{
			++(currentHeader->SkippedPacket.skippedPacketCount) ;
		}
		else
		{
			if (!IsAsyncStreamFreePacketHeader(currentHeader))
			{
				if ( !IsAsyncStreamFreePacketHeader(currentHeader->CommonHeader.next) )
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
	}
	else
	{
		if (!IsAsyncStreamFreePacketHeader(currentHeader))
		{
			if ( !IsAsyncStreamFreePacketHeader(currentHeader->CommonHeader.next) )
			{
				IOFWPacketHeader*	newHeader		= new IOFWPacketHeader ;
				newHeader->CommonHeader.next		= currentHeader->CommonHeader.next ;
				currentHeader->CommonHeader.next	= newHeader ;
			}

		}

		currentHeader = currentHeader->CommonHeader.next ;

		FWAddress addr;
		addr.addressHi = 0; addr.addressLo = 0;
		
		IOFWSpeed speed = kFWSpeedInvalid;

		InitIncomingPacketHeader(
				currentHeader,
				currentHeader->CommonHeader.next,
				len,
				destOffset,
				& fPacketAsyncNotificationRef,
				0,
				speed,
				addr,
				false) ;

		fPacketQueueBuffer->writeBytes(destOffset, buf, len) ;
		
		fBufferAvailable -= len ;

		fLastWrittenHeader = currentHeader ;
	}

	if( currentHeader->CommonHeader.type != IOFWPacketHeader::kFree )
		sendPacketNotification(currentHeader) ;

	IOLockUnlock(fLock) ;
}

void
IOFWUserAsyncStreamListener::asyncStreamListenerHandler(
                                            void*					refCon,
                                            const void*				buf)
{
	IOFWUserAsyncStreamListener*	me = (IOFWUserAsyncStreamListener*)refCon ;
	ISOC_DATA_PKT *pkt = (ISOC_DATA_PKT*)buf;

	me->doPacket( pkt->size+sizeof(ISOC_DATA_PKT), buf, IOFWPacketHeader::kIncomingPacket ) ;
}

void
IOFWUserAsyncStreamListener::setAsyncStreamRef_Packet(
	OSAsyncReference64	asyncRef)
{
	bcopy(asyncRef, fPacketAsyncNotificationRef, sizeof(OSAsyncReference64)) ;	
}

void
IOFWUserAsyncStreamListener::setAsyncStreamRef_SkippedPacket(
	OSAsyncReference64	asyncRef)
{
	bcopy(asyncRef, fSkippedPacketAsyncNotificationRef, sizeof(OSAsyncReference64)) ;
}

void
IOFWUserAsyncStreamListener::clientCommandIsComplete(
	FWClientCommandID 	inCommandID)
{
	IOLockLock(fLock) ;

	if ( fWaitingForUserCompletion )
	{
		IOFWPacketHeader*			oldHeader 	= fLastReadHeader ;
		fLastReadHeader							= fLastReadHeader->CommonHeader.next ;
				
		fBufferAvailable += oldHeader->IncomingPacket.packetSize ;
		
		oldHeader->CommonHeader.type = IOFWPacketHeader::kFree ;
		oldHeader->CommonHeader.whichAsyncRef = 0;
		
		fWaitingForUserCompletion = false ;
	}
	if ( fLastReadHeader->CommonHeader.type != IOFWPacketHeader::kFree )
		sendPacketNotification(fLastReadHeader) ;

	IOLockUnlock(fLock) ;
}

void
IOFWUserAsyncStreamListener::sendPacketNotification(
	IOFWPacketHeader*	inPacketHeader)
{	
	if (!fWaitingForUserCompletion)
	{
		if (inPacketHeader->CommonHeader.whichAsyncRef[0])
		{
			IOReturn ret = IOFireWireUserClient::sendAsyncResult64(*(inPacketHeader->CommonHeader.whichAsyncRef),
																kIOReturnSuccess,
																(io_user_reference_t *)inPacketHeader->CommonHeader.args,
																inPacketHeader->CommonHeader.argCount) ;
			
			if(ret == kIOReturnSuccess)
				fWaitingForUserCompletion = true ;
		}
	}
}