/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
#ifndef _IOKIT_IOFWASYNCSTREAMRECEIVER_H
#define _IOKIT_IOFWASYNCSTREAMRECEIVER_H

#include <IOKit/firewire/IOFireWireLink.h>
#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/firewire/IOFWDCLProgram.h>

const int kMaxAsyncStreamReceiveBuffers		= 9;
const int kMaxAsyncStreamReceiveBufferSize	= 4096; // zzz : should determine from maxrec

class IOFWAsyncStreamReceiver;
class IOFWAsyncStreamReceivePort;

/*! @class IOFWAsyncStreamReceiver
*/
class IOFWAsyncStreamReceiver : public OSObject
{
    OSDeclareDefaultStructors(IOFWAsyncStreamReceiver)

friend class IOFWAsyncStreamListener;

public:
	
/*!	function initAll
	abstract Constructs a DCL program to receive Async Stream packets.
	param control IOFireWireController
	param channel Channel to use.
	result true on success, else false.	*/	
	bool initAll( IOFireWireController *control, UInt32 channel );

/*!	function activate
	abstract Activates the DCL program to start receiving Async stream
			  packets.
	param broadcastSpeed Ideal broadcast speed to receive.
	result returns an IOReturn errorcode.	*/	
	IOReturn activate( IOFWSpeed broadcastSpeed );	

/*!	function deactivate
	abstract Stops receiving Async stream  packets.
	result returns an IOReturn errorcode.	*/	
	IOReturn deactivate();

/*!	function getOverrunCounter
	abstract returns Isoch ovverrun counter.
	result returns the counter.	*/	
	inline UInt16 getOverrunCounter() { return fIsoRxOverrun; };

/*!	function listens
	abstract Checks whether DCL program is listening on this channel.
	param channel Channel to verify.
	result returns true on success, else false.	*/	
	inline bool listens ( UInt32 channel ) { return ( fChannel == channel ); };

/*!	function receiverActive
	abstract Verify whether receiver is active.
	result returns true if active,else false.	*/	
	inline bool receiverActive() { return fActive; };
	
/*!	function receiverInitialized
	abstract Verify whether receiver is initialized.
	result returns true if initialized,else false.	*/	
	inline bool receiverInitialized() { return fInitialized; };

/*!	function addListener
	abstract Add a new IOFWAsyncStreamListener for notifications.
	param listener enable notifications for this listener.
	result returns true if success,else false.	*/	
	bool addListener ( IOFWAsyncStreamListener *listener );

/*!	function removeListener
	abstract Removes a IOFWAsyncStreamListener from receiving notifications.
	param  listener disable notifications for this listener.
	result none.	*/	
	void removeListener ( IOFWAsyncStreamListener *listener );

/*!	function restart
	abstract Callback for restarting the DCL program.
	result none.	*/	
    void restart( );

/*!	function receiveAsyncStream
	abstract Callback for receiving Async Stream packets.
	result none.	*/	
	static void receiveAsyncStream( DCLCommandStruct *dclProgram );

/*!	function receiveAsyncStream
	abstract Callback for receiving Async Stream packets.
	result return kIOReturnSuccess after processing the packet.	*/	
	static IOReturn receiveAsyncStream(void *refcon, IOFireWireMultiIsochReceivePacket *pPacket);

	UInt32	getClientsCount();

protected:
	typedef struct
	{
		void	*obj;
		void	*thisObj;
		UInt8	*buffer;
		UInt16	index;
	} FWAsyncStreamReceiveRefCon ;

	// Structure variables for each segment
	typedef struct FWAsyncStreamReceiveSegment
	{
		DCLLabelPtr pSegmentLabelDCL;
		DCLJumpPtr pSegmentJumpDCL;
	}FWAsyncStreamReceiveSegment, *FWAsyncStreamReceiveSegmentPtr;
    
/*! @struct ExpansionData
    @discussion This structure will be used to expand the capablilties of the class in the future.
    */    
    struct ExpansionData { };

/*! @var reserved
    Reserved for future use.  (Internal use only)  */
    ExpansionData *reserved;
    
    virtual void		free();

private :
	
	IOBufferMemoryDescriptor	*fBufDesc;
	DCLCommandStruct			*fdclProgram;
	IOFWAsyncStreamReceivePort	*fAsynStreamPort;
	IOFWIsochChannel			*fAsyncStreamChan;
    IODCLProgram 				*fIODclProgram;
    IOFireWireController		*fControl;
	IOFireWireLink				*fFWIM;
	
	bool						fActive;
    bool						fInitialized;
	UInt32						fChannel;
    IOFWSpeed					fSpeed;
    
    UInt32						fSegment;
    FWAsyncStreamReceiveSegment	*fReceiveSegmentInfoPtr;
	DCLLabel					*fDCLOverrunLabelPtr;
	UInt16 						fIsoRxOverrun;
	UInt16 						fIsoRxCallbacks;
    IORecursiveLock				*rxCommandLock;
	OSSet						*fAsyncStreamClients;
	OSIterator					*fAsyncStreamClientIterator;
	
	IOFireWireMultiIsochReceiveListener *fListener;
	
	DCLCommandStruct *CreateAsyncStreamRxDCLProgram(	DCLCallCommandProc* proc, 
														void *callbackObject);
	
	IOFWAsyncStreamReceivePort *CreateAsyncStreamPort(	bool talking, 
														DCLCommandStruct *opcodes, 
														void *info,
														UInt32 startEvent, 
														UInt32 startState, 
														UInt32 startMask,
														UInt32 channel);

    static void overrunNotification( DCLCommandStruct *callProc );
	
	static IOReturn forceStopNotification( void* refCon, IOFWIsochChannel* channel, UInt32 stopCondition );
									
	void FreeAsyncStreamRxDCLProgram( DCLCommandStruct *dclProgram );

    void fixDCLJumps( bool restart );

	void removeAllListeners();

	void indicateListeners ( UInt8 *buffer );

    IOReturn modifyDCLJumps( DCLCommandStruct *callProc );
	
    OSMetaClassDeclareReservedUnused(IOFWAsyncStreamReceiver, 0);
    OSMetaClassDeclareReservedUnused(IOFWAsyncStreamReceiver, 1);
};

#endif // _IOKIT_IOFWASYNCSTREAMRECEIVER_H

