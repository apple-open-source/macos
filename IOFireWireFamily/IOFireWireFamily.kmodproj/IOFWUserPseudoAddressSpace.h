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
 *  IOFWUserClientPsdoAddrSpace.h
 *  IOFireWireFamily
 *
 *  Created by NWG on Fri Dec 08 2000.
 *  Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 */
/*
	$Log: not supported by cvs2svn $
	Revision 1.11  2008/09/12 23:44:05  calderon
	<rdar://5971979/> PseudoAddressSpace skips/mangles packets
	<rdar://5708169/> FireWire synchronous commands' headerdoc missing callback info
	
	Revision 1.10  2008/07/04 00:09:14  arulchan
	fix for rdar://6035774
	
	Revision 1.9  2007/02/16 19:03:44  arulchan
	*** empty log message ***
	
	Revision 1.8  2007/02/14 21:58:29  collin
	*** empty log message ***
	
	Revision 1.7  2007/02/07 06:35:20  collin
	*** empty log message ***
	
	Revision 1.6  2006/12/21 21:17:44  ayanowit
	More changes necessary to eventually get support for 64-bit apps working (4222965).
	
	Revision 1.5  2006/12/06 19:21:49  arulchan
	*** empty log message ***
	
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
#import "IOFWRingBufferQ.h"

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
        OSAsyncReference64*			whichAsyncRef ;
        UInt32						argCount ;
        io_user_reference_t			headerSize ;		// only valid for skipped packets
		io_user_reference_t			headerOffset ;		// only valid for skipped packets
        
        io_user_reference_t			args[9] ;
    } CommonHeader ;

    struct
    {
        // -----------------------------------------------
        QueueTag					type ;
        IOFWPacketHeader_t*			next ;
        OSAsyncReference64*			whichAsyncRef ;
        UInt32						argCount ;
		io_user_reference_t			headerSize ;		// only valid for skipped packets
		io_user_reference_t			headerOffset ;		// only valid for skipped packets
        // -----------------------------------------------
        
        io_user_reference_t			commandID ;			//	0
        io_user_reference_t			packetSize ;		//	1
        io_user_reference_t			packetOffset ;		//	2
        io_user_reference_t			nodeID ;
        io_user_reference_t			speed ;
        io_user_reference_t			addrHi ;
        io_user_reference_t			addrLo ;
        io_user_reference_t			isLock ;
		io_user_reference_t			generation ;
		io_user_reference_t			reqrefcon ;

    } IncomingPacket ;

    struct SkippedPacket_t
    {
        // -----------------------------------------------
        QueueTag 					type ;
        IOFWPacketHeader_t*			next ;
        OSAsyncReference64*			whichAsyncRef ;
        UInt32						argCount ;
		io_user_reference_t			headerSize ;		// only valid for skipped packets
		io_user_reference_t			headerOffset ;		// only valid for skipped packets
        // -----------------------------------------------

        io_user_reference_t					commandID ;			//	0
        io_user_reference_t					skippedPacketCount ;
    } SkippedPacket ;

	struct ReadPacket_t
	{
	    // -----------------------------------------------
	    QueueTag 					type ;
		IOFWPacketHeader_t*			next ;
		OSAsyncReference64*			whichAsyncRef ;
		UInt32						argCount ;
		io_user_reference_t			headerSize ;		// only valid for skipped packets
		io_user_reference_t			headerOffset ;		// only valid for skipped packets
		// -----------------------------------------------

        io_user_reference_t			commandID ;			//	0
        io_user_reference_t			packetSize ;		//	1
        io_user_reference_t			packetOffset ;		//	2
        io_user_reference_t			nodeID ;
        io_user_reference_t			speed ;
        io_user_reference_t			addrHi ;
        io_user_reference_t			addrLo ;
		io_user_reference_t			reqrefcon ;
		io_user_reference_t			generation ;
	} ReadPacket ;

public:
    IOFWPacketHeader_t() ;
    
} IOFWPacketHeader ;

io_user_reference_t& IOFWPacketHeaderGetSize(IOFWPacketHeader_t* hdr) ;
io_user_reference_t& IOFWPacketHeaderGetOffset(IOFWPacketHeader_t* hdr) ;
void InitIncomingPacketHeader(
	IOFWPacketHeader_t*				header,
	IOFWPacketHeader_t*				next,
	const IOByteCount				len,
	const IOByteCount				offset,
	OSAsyncReference64*				ref,
	UInt16							nodeID,
	const IOFWSpeed&   				speed,
	const FWAddress&				addr,
	const bool						isLock = false) ;	// generation used only for lock
inline void InitSkippedPacketHeader(
	IOFWPacketHeader*				header,
	const union IOFWPacketHeader_t* next,
	const IOByteCount				offset,
	OSAsyncReference64*				ref) ;
inline void InitReadPacketHeader(
	IOFWPacketHeader*				header,
	IOFWPacketHeader*				next,
	UInt32							len,
	UInt32							offset,
	OSAsyncReference64*				ref,
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
	OSAsyncReference64*				ref,
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
	static void							exporterCleanup( const OSObject * self );

	// --- IOFWPseudoAddressSpace ----------
	// override deactivate so we can delete any notification related structures...
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
											OSAsyncReference64		inAsyncRef) ;
	void							setAsyncRef_SkippedPacket(
											OSAsyncReference64		inAsyncRef) ;
	void							setAsyncRef_Read(
											OSAsyncReference64		inAsyncRef) ;
	void							clientCommandIsComplete(
											FWClientCommandID		inCommandID,
											IOReturn				inResult ) ;
	void							sendPacketNotification(
											IOFWPacketHeader*		inPacketHeader) ;
private:
	IOFWRingBufferQ *			fPacketQueue;					// the queue where incoming packets go before being written to the backingstore
	IOLock*						fLock ;							// to lock this object

	mach_vm_address_t			fUserRefCon ;
	IOFireWireUserClient*		fUserClient ;
	IOFWPacketHeader*			fLastWrittenHeader ;
	IOFWPacketHeader*			fLastReadHeader ;
	FWAddress					fAddress ;						// where we are
	
	OSAsyncReference64			fSkippedPacketAsyncNotificationRef ;
	OSAsyncReference64			fPacketAsyncNotificationRef ;
	OSAsyncReference64			fReadAsyncNotificationRef ;
	bool						fWaitingForUserCompletion ;
	bool						fUserLocks ;					// are we doing locks in user space?
	
	UInt32						fFlags ;
	
	Boolean						fPacketQueuePrepared ;
	Boolean						fBackingStorePrepared ;
} ;

#endif //__IOFWUserClientPsduAddrSpace_H__
