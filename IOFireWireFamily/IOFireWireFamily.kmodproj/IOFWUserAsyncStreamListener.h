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
 *  IOFWUserAsyncStreamListener.h
 *  IOFireWireFamily
 *
 *  Created by Arul on Wed Nov 08 2006.
 *  Copyright (c) 2004-2006 Apple Computer, Inc. All rights reserved.
 *
 */
/*
	$Log: IOFWUserAsyncStreamListener.h,v $
	Revision 1.4  2007/02/16 19:03:43  arulchan
	*** empty log message ***

	Revision 1.3  2007/02/07 06:35:20  collin
	*** empty log message ***
	
	Revision 1.2  2006/12/21 21:17:44  ayanowit
	More changes necessary to eventually get support for 64-bit apps working (4222965).
	
	Revision 1.1  2006/12/06 00:03:27  arulchan
	Isoch Channel 31 Receiver
	
*/

#ifndef __IOFWUSERASYNCSTREAMLISTENER_H__
#define __IOFWUSERASYNCSTREAMLISTENER_H__

// public
#import <IOKit/firewire/IOFireWireFamilyCommon.h>
#import <IOKit/firewire/IOFWAsyncStreamListener.h>

// private
#import "IOFireWireLibPriv.h"
#import "IOFWUserPseudoAddressSpace.h"

#if defined(__BIG_ENDIAN__)
typedef struct {
	UInt16 size;
	UInt8  tag:2;
	UInt8  chan:6;
	UInt8  tcode:4;
	UInt8  sy:4;
} ISOC_DATA_PKT;
#elif defined(__LITTLE_ENDIAN__)
typedef struct {    
	UInt8  sy:4;
	UInt8  tcode:4;
	UInt8  chan:6;
	UInt8  tag:2;
	UInt16 size;
} ISOC_DATA_PKT;
#else
#error host endian unknown
#endif

class IOFireWireUserClient ;

class IOFWUserAsyncStreamListener: public IOFWAsyncStreamListener
{
	OSDeclareDefaultStructors(IOFWUserAsyncStreamListener)

public:
	// --- OSObject ----------
#if IOFIREWIREUSERCLIENTDEBUG > 0
    virtual bool 					serialize(OSSerialize *s) const;
#endif
	virtual void					free() ;
	
	static void						exporterCleanup( OSObject * self, IOFWUserObjectExporter * exporter );

	bool							completeInit( IOFireWireUserClient* userclient, FWUserAsyncStreamListenerCreateParams* params ) ;

	void							deactivate();
	
	bool							initAsyncStreamListener( IOFireWireUserClient* userclient, FWUserAsyncStreamListenerCreateParams* params ) ;
	
	void							doPacket(
											UInt32							len,
											const void*						buf,
											IOFWPacketHeader::QueueTag		tag,
											UInt32*							oldVal = NULL) ;

	// --- getters ----------
	const mach_vm_address_t			getUserRefCon() { return fUserRefCon ;}
	
	const IOFireWireUserClient&		getUserClient() { return *fUserClient ;}

	// --- packet handler ----------
    static void						asyncStreamListenerHandler(
                                            void*					refCon,
                                            const void*				buf) ;

	// --- async utility functions ----------
	void							setAsyncStreamRef_Packet(
											OSAsyncReference64		inAsyncRef ) ;
	void							setAsyncStreamRef_SkippedPacket(
											OSAsyncReference64		inAsyncRef ) ;
	void							clientCommandIsComplete(
											FWClientCommandID		inCommandID ) ;
	void							sendPacketNotification(
											IOFWPacketHeader*		inPacketHeader ) ;

private:
    IOMemoryDescriptor*			fPacketQueueBuffer ;			// the queue where incoming packets, etc., go
	IOLock*						fLock ;							// to lock this object

	mach_vm_address_t			fUserRefCon ;
	IOFireWireUserClient*		fUserClient ;
	IOFWPacketHeader*			fLastWrittenHeader ;
	IOFWPacketHeader*			fLastReadHeader;
	UInt32						fBufferAvailable ;				// amount of queue space remaining
	
	OSAsyncReference64			fSkippedPacketAsyncNotificationRef ;
	OSAsyncReference64			fPacketAsyncNotificationRef ;
	bool						fWaitingForUserCompletion ;
	bool						fUserLocks ;					// are we doing locks in user space?
	
	UInt32						fFlags ;
	
	Boolean						fPacketQueuePrepared ;
} ;

#endif // __IOFWUSERASYNCSTREAMLISTENER_H__