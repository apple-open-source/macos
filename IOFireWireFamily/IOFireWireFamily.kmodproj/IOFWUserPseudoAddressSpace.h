/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  IOFWUserClientPsdoAddrSpace.h
 *  IOFireWireFamily
 *
 *  Created by NWG on Fri Dec 08 2000.
 *  Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 */
/*
	$Log: IOFWUserPseudoAddressSpace.h,v $
	Revision 1.4  2003/07/24 06:30:58  collin
	*** empty log message ***
	
	Revision 1.3  2003/07/21 06:52:59  niels
	merge isoch to TOT
	
	Revision 1.2.14.1  2003/07/01 20:54:07  niels
	isoch merge
	
	Revision 1.2  2002/10/18 23:29:44  collin
	fix includes, fix cast which fails on new compiler
	
	Revision 1.1  2002/09/25 00:27:22  niels
	flip your world upside-down
	
	Revision 1.11  2002/08/06 19:42:54  niels
	now send conflict response if user pseudo address space can't receive a write because the queue is full in cases where the hardware has not already responded 'ack complete'
	
*/

#ifndef __IOFWUserClientPsduAddrSpace_H__
#define __IOFWUserClientPsduAddrSpace_H__

// public
#import <IOKit/firewire/IOFireWireFamilyCommon.h>
#import <IOKit/firewire/IOFWAddressSpace.h>

// private
#import "IOFireWireLibPriv.h"

using namespace IOFireWireLib ;

typedef union IOFWPacketHeader_t
{
    typedef enum
    {
        kFree = 0,
        kStopPacket   	  	= 'stop',
        kBadPacket	   		= ' bad',
        kIncomingPacket	 	= 'pckt',
        kSkippedPacket		= 'skip',
		kReadPacket			= 'read',
		kLockPacket			= 'lock'
    } QueueTag ;

    struct
    {
        QueueTag 					type ;
        IOFWPacketHeader_t*			next ;
        OSAsyncReference*			whichAsyncRef ;
        UInt32						argCount ;
        
        UInt32						args[9] ;
    } CommonHeader ;

    struct
    {
        // -----------------------------------------------
        QueueTag					type ;
        IOFWPacketHeader_t*			next ;
        OSAsyncReference*			whichAsyncRef ;
        UInt32						argCount ;
        // -----------------------------------------------
        
        UInt32						commandID ;			//	0
        UInt32						packetSize ;		//	1
        UInt32						packetOffset ;		//	2
        UInt32						nodeID ;
        UInt32						speed ;
        UInt32						addrHi ;
        UInt32						addrLo ;
        UInt32						isLock ;
		UInt32						generation ;
		IOFWRequestRefCon			reqrefcon ;

    } IncomingPacket ;

    struct SkippedPacket_t
    {
        // -----------------------------------------------
        QueueTag 					type ;
        IOFWPacketHeader_t*			next ;
        OSAsyncReference*			whichAsyncRef ;
        UInt32						argCount ;
        // -----------------------------------------------

        UInt32						commandID ;			//	0
        UInt32						skippedPacketCount ;
    } SkippedPacket ;

	struct ReadPacket_t
	{
	    // -----------------------------------------------
	    QueueTag 					type ;
		IOFWPacketHeader_t*			next ;
		OSAsyncReference*			whichAsyncRef ;
		UInt32						argCount ;
		// -----------------------------------------------

        UInt32						commandID ;			//	0
        UInt32						packetSize ;		//	1
        UInt32						packetOffset ;		//	2
        UInt32						nodeID ;
        UInt32						speed ;
        UInt32						addrHi ;
        UInt32						addrLo ;
		IOFWRequestRefCon			reqrefcon ;
		UInt32						generation ;
	} ReadPacket ;

public:
    IOFWPacketHeader_t() ;
    
} IOFWPacketHeader ;

inline IOByteCount& IOFWPacketHeaderGetSize(IOFWPacketHeader_t* hdr) ;
inline IOByteCount& IOFWPacketHeaderGetOffset(IOFWPacketHeader_t* hdr) ;
inline void InitIncomingPacketHeader(
	IOFWPacketHeader_t*				header,
	IOFWPacketHeader_t*				next,
	const IOByteCount				len,
	const IOByteCount				offset,
	OSAsyncReference*				ref,
	UInt16							nodeID,
	const IOFWSpeed&   				speed,
	const FWAddress&				addr,
	const bool						isLock = false) ;	// generation used only for lock
inline void InitSkippedPacketHeader(
	IOFWPacketHeader*				header,
	const union IOFWPacketHeader_t* next,
	const IOByteCount				offset,
	OSAsyncReference*				ref) ;
inline void InitReadPacketHeader(
	IOFWPacketHeader*				header,
	IOFWPacketHeader*				next,
	UInt32							len,
	UInt32							offset,
	OSAsyncReference*				ref,
	void*							refCon,
	UInt16							nodeID,
	IOFWSpeed&						speed,
	FWAddress						addr,
	IOFWRequestRefCon				reqrefcon) ;
inline void	InitLockPacketHeader(
	IOFWPacketHeader*				header,
	IOFWPacketHeader*				next,
	IOByteCount						len,
	IOByteCount						offset,
	OSAsyncReference*				ref,
	UInt16							nodeID,
	IOFWSpeed&						speed,
	FWAddress						addr,
	const UInt32					generation,
	IOFWRequestRefCon				reqrefcon) ;
inline Boolean IsSkippedPacketHeader(const union IOFWPacketHeader_t* header) ;
inline Boolean IsFreePacketHeader(const union IOFWPacketHeader_t* header) ;
//inline Boolean IsReadPacketHeader(const union IOFWPacketHeader_t* header) ;

class IOFireWireUserClient ;

// To support mapping the memory descriptor within
// a pseudo address space to user space, we need to add
// accessors to IOFWPseudoAddressSpace. This class
// implements the additional functionality.
class IOFWUserPseudoAddressSpace: public IOFWPseudoAddressSpace
{
	OSDeclareDefaultStructors(IOFWUserPseudoAddressSpace)

public:
	// --- OSObject ----------
#if IOFIREWIREUSERCLIENTDEBUG > 0
    virtual bool 						serialize(OSSerialize *s) const;
#endif
	virtual void						free() ;
	void								exporterCleanup ();

	// --- IOFWPseudoAddressSpace ----------
	// override deactivate so we can delete any notification related structures...
	virtual IOReturn					activate() ;
	virtual void						deactivate() ;
	
	bool							completeInit( IOFireWireUserClient* userclient, AddressSpaceCreateParams* params ) ;
	bool							initPseudo( IOFireWireUserClient* userclient, AddressSpaceCreateParams* params ) ;
	bool							initFixed( IOFireWireUserClient* userclient, AddressSpaceCreateParams* params ) ;
	virtual UInt32 					doLock(
											UInt16 						nodeID, 
											IOFWSpeed &					speed, 
											FWAddress 					addr, 
											UInt32 						inLen,
											const UInt32 *				newVal, 
											UInt32 &					outLen, 
											UInt32 *					oldVal, 
											UInt32 						type,
											IOFWRequestRefCon 			refcon) ;

	UInt32							doPacket(
											UInt16							nodeID,
											IOFWSpeed&						speed,
											FWAddress						addr,
											UInt32							len,
											const void*						buf,
											IOFWRequestRefCon				reqrefcon,
											IOFWPacketHeader::QueueTag		tag,
											UInt32*							oldVal = NULL) ;

	// --- getters ----------
    const FWAddress& 				getBase() { return fAddress ; }
	const UInt32					getUserRefCon() { return fUserRefCon ;}
	const IOFireWireUserClient&				getUserClient() { return *fUserClient ;}

	// --- readers/writers ----------
    static UInt32					pseudoAddrSpaceReader(
                                            void*					refCon,
                                            UInt16					nodeID,
                                            IOFWSpeed& 				speed,
                                            FWAddress 				addr,
                                            UInt32		 			len,
                                            IOMemoryDescriptor**	buf,
                                            IOByteCount* 			offset,
                                            IOFWRequestRefCon		reqrefcon) ;
    static UInt32					pseudoAddrSpaceWriter(
                                            void*					refCon,
                                            UInt16					nodeID,
                                            IOFWSpeed&				speed,
                                            FWAddress				addr,
                                            UInt32					len,
                                            const void*				buf,
                                            IOFWRequestRefCon		reqrefcon) ;

	// --- async utility functions ----------
	void							setAsyncRef_Packet(
											OSAsyncReference		inAsyncRef) ;
	void							setAsyncRef_SkippedPacket(
											OSAsyncReference		inAsyncRef) ;
	void							setAsyncRef_Read(
											OSAsyncReference		inAsyncRef) ;
	void							clientCommandIsComplete(
											FWClientCommandID		inCommandID,
											IOReturn				inResult ) ;
	void							sendPacketNotification(
											IOFWPacketHeader*		inPacketHeader) ;
private:
    IOMemoryDescriptor*			fPacketQueueBuffer ;			// the queue where incoming packets, etc., go
	IOLock*						fLock ;							// to lock this object

	UInt32						fUserRefCon ;
	IOFireWireUserClient*					fUserClient ;
	IOFWPacketHeader*			fLastWrittenHeader ;
	IOFWPacketHeader*			fLastReadHeader ;
	UInt32						fBufferAvailable ;				// amount of queue space remaining
	FWAddress					fAddress ;						// where we are
	
	OSAsyncReference			fSkippedPacketAsyncNotificationRef ;
	OSAsyncReference			fPacketAsyncNotificationRef ;
	OSAsyncReference			fReadAsyncNotificationRef ;
	bool						fWaitingForUserCompletion ;
	bool						fUserLocks ;					// are we doing locks in user space?
	
	UInt32						fFlags ;
	
	Boolean						fPacketQueuePrepared ;
	Boolean						fBackingStorePrepared ;
} ;

#endif //__IOFWUserClientPsduAddrSpace_H__
