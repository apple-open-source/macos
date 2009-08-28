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
 * Copyright (c) 2000 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 */


#ifndef _IOKIT_IOFireWireLink_H
#define _IOKIT_IOFireWireLink_H

#ifndef FIREWIREPRIVATE
#warning Please do not include this file. Include IOFireWireBus.h instead.
#endif

#import <IOKit/firewire/IOFireWireFamilyCommon.h>

#import <IOKit/IOService.h>
#import <IOKit/IOFilterInterruptEventSource.h>
#import <IOKit/firewire/IOFireWireController.h>

enum
{
	kIOFWAllPhysicalFilters = 64
};

// These are used by SetLinkMode, arg1 parameter
enum
{
	kIOFWSetDSLimit = 0,
	kIOFWSetForceLinkReset = 1
};

enum
{
	kIOFWNodeFlagRetryOnAckD	= (1 << 0)
};

struct IOFWNodeScan;
class IOFWDCLPool ;
class IODCLProgram ;

/*! @class IOFireWireLink
*/
class IOFireWireLink : public IOService
{
    OSDeclareAbstractStructors(IOFireWireLink)

	protected:
	
		IOFireWireController *fControl;
		IOFWWorkLoop *	fWorkLoop;
	
	/*! @struct ExpansionData
		@discussion This structure will be used to expand the capablilties of the class in the future.
		*/    
		struct ExpansionData { };
	
	/*! @var reserved
		Reserved for future use.  (Internal use only)  */
		ExpansionData *reserved;
	
		// calls to protected methods of controller
		void processBusReset()
		{ fControl->processBusReset(); };
		void processSelfIDs(UInt32 *IDs, int numIDs, UInt32 *ownIDs, int numOwnIDs)
		{ fControl->processSelfIDs(IDs, numIDs, ownIDs, numOwnIDs); };
		void processRcvPacket(UInt32 *data, int numQuads, IOFWSpeed speed )
			{ fControl->processRcvPacket(data, numQuads, speed ); };
		void processCycle64Int()
		{ fControl->processCycle64Int(); };
		virtual IOFireWireController * createController();
		virtual IOFWWorkLoop* createWorkLoop();
	
		
		// Public methods are usually only called from the firewire controller object.
	public:
	
		// Create a device nub
		virtual IOFireWireDevice *createDeviceNub(CSRNodeUniqueID guid, const IOFWNodeScan *deviceInfo);
	
		// Set power state
		virtual IOReturn setLinkPowerState ( unsigned long powerStateOrdinal) = 0;
	
		// Bus management stuff
		virtual IOReturn 				setContender(bool state) = 0;
		virtual IOReturn 				setRootHoldOff(bool state) = 0;
		virtual IOReturn 				setCycleMaster(bool state) = 0;
		
		virtual IODCLProgram*			createDCLProgram(bool talking, DCLCommand *opcodes,
												IOFireWireController::DCLTaskInfo *info, UInt32 startEvent, 
												UInt32 startState, UInt32 startMask) = 0;
	
		// Send a PHY packet
		virtual IOReturn 				sendPHYPacket(UInt32 quad) = 0;
	
		// Check for hardware interrupts (typically from a timeout call)
		virtual void 					handleInterrupts( IOInterruptEventSource *, int count ) = 0;
	
		virtual IOReturn 				resetBus( bool useIBR = false ) = 0;
	
		virtual IOReturn 				asyncRead(UInt16 nodeID, UInt16 addrHi, UInt32 addrLo, 
												int speed, int label, int size, IOFWAsyncCommand *cmd,
												IOFWReadFlags 			flags) = 0;
	
		virtual IOReturn 				asyncReadQuadResponse(UInt16 nodeID, int speed, 
												int label, int rcode, UInt32 data) = 0;
	
		virtual IOReturn				asyncReadResponse(UInt16 nodeID, int speed,
											int label, int rcode, IOMemoryDescriptor *buf,
											IOByteCount offset, int len, IODMACommand * in_dma_command ) = 0;
	
		virtual IOReturn				asyncWrite(	UInt16 					nodeID, 
													UInt16 					addrHi, 
													UInt32 					addrLo,
													int 					speed, 
													int 					label, 
													IOMemoryDescriptor *	buf, 
													IOByteCount 			offset,
													int 					size, 
													IOFWAsyncCommand *		cmd,
													IOFWWriteFlags 			flags ) = 0;
		
		virtual IOReturn 				asyncWriteResponse(UInt16 nodeID, int speed, 
												int label, int rcode, UInt16 addrHi) = 0;
											
		virtual IOReturn asyncLock(	UInt16					destID, 
									UInt16 					addrHi, 
									UInt32 					addrLo, 
									int 					speed, 
									int 					label, 
									int 					type, 
									IOMemoryDescriptor *	buf, 
									IOByteCount 			offset, 
									int 					length, 
									IOFWAsyncCommand *		cmd ) = 0;
									
		virtual IOReturn 				asyncLockResponse(UInt16 nodeID, int speed, 
												int label, int rcode, int type, void *data, int len) = 0;
	
		// Try to fix whatever might have caused the other device to not respond
		virtual IOReturn 				handleAsyncTimeout(IOFWAsyncCommand *cmd) = 0;
	
		// Local ConfigROM changed
		// Bus will be reset (via resetBus()) if this returns kIOReturnSuccess
		virtual IOReturn 				updateROM(const OSData *rom) = 0;
		
		// Read Cycle time register. safe to call at any time.
		virtual IOReturn 				getCycleTime(UInt32 &cycleTime) = 0;
		virtual IOReturn 				getBusCycleTime(UInt32 &busTime, UInt32 &cycleTime) = 0;
		virtual IOReturn 				setBusTime(UInt32 busTime) = 0;
	
		virtual CSRNodeUniqueID 		getGUID() = 0;
		virtual UInt32 					getBusCharacteristics() = 0;
		virtual UInt32 					getMaxSendLog() = 0;
		virtual UInt16 					getNodeID() = 0;
	
		virtual IOFireWireController * 	getController() const;
		
		// Implement IOService::getWorkLoop
		virtual IOWorkLoop *			getWorkLoop () const;
		
		// FireWire wants an IOFWWorkLoop
		virtual IOFWWorkLoop *			getFireWireWorkLoop () const;
		
		virtual void 					setNodeIDPhysicalFilter ( UInt16 nodeID, bool state ) = 0;
		virtual void 					setNodeFlags ( UInt16 nodeID, UInt32 flags ) = 0;
		
//		// don't forget about the isoch workloop:
//		virtual void					closeIsochGate () ;
//		virtual void					openIsochGate () ;
	
		virtual	IOReturn					asyncStreamTransmit (
															UInt32					channel,
															int						speed,		
															UInt32					sync,
															UInt32					tag,
															IOMemoryDescriptor		*pmd,
															IOByteCount				offset,
															int						length,
															IOFWAsyncStreamCommand	* cmd ) = 0;
		virtual void						setSecurityMode (
															IOFWSecurityMode mode ) = 0;
		void								handleARxReqIntComplete ( void )							{ fControl->handleARxReqIntComplete(); };
		virtual void						flushWaitingPackets ( void ) = 0;
		virtual IOFWDCLPool*				createDCLPool ( UInt32 capacity ) ;
		inline IOWorkLoop *					getIsochWorkloop ()											{ return fIsocWorkloop ; }
		inline IOWorkLoop *					getWorkloop()											{ return (IOWorkLoop*)fWorkLoop; }

		virtual IOReturn					clipMaxRec2K( bool clipMaxRec ) = 0;
		virtual IOFWSpeed					getPhySpeed() = 0 ;
		
		virtual void disablePHYPortOnSleep( UInt32 mask );
		
		virtual	UInt32 *					getPingTimes ();
		
		virtual IOReturn					handleAsyncCompletion( IOFWCommand *cmd, IOReturn status );

		virtual void handleSystemShutDown( UInt32 messageType );

		virtual void configureAsyncRobustness( bool enabled );

		virtual bool isPhysicalAccessEnabledForNodeID( UInt16 nodeID );	

		virtual void notifyInvalidSelfIDs ();

		virtual IOReturn asyncPHYPacket( UInt32 data, UInt32 data2, IOFWAsyncPHYCommand * cmd );

		virtual bool enterLoggingMode( void );

		virtual IOReturn getCycleTimeAndUpTime( UInt32 &cycleTime, UInt64 &uptime );

		virtual UInt32 setLinkMode( UInt32 arg1, UInt32 arg2 );
		
		void requestBusReset()
			{ fControl->resetBus(); };
		
		virtual IOReturn activateMultiIsochReceiveListener(IOFireWireMultiIsochReceiveListener *pListener) = 0;
		virtual IOReturn deactivateMultiIsochReceiveListener(IOFireWireMultiIsochReceiveListener *pListener) = 0;
		virtual void clientDoneWithMultiIsochReceivePacket(IOFireWireMultiIsochReceivePacket *pPacket) = 0;
		
		inline void setMultiIsochReceiveListenerActivatedState(IOFireWireMultiIsochReceiveListener *pListener, bool active) {pListener->fActivated = active;};

		virtual void enableAllInterrupts( void ) = 0;

		virtual IOPMPowerState * getPowerStateTable( unsigned long * numberOfStates ) = 0;
	
	private:
	
		OSMetaClassDeclareReservedUnused(IOFireWireLink, 0);
		OSMetaClassDeclareReservedUnused(IOFireWireLink, 1);
		OSMetaClassDeclareReservedUnused(IOFireWireLink, 2);
		OSMetaClassDeclareReservedUnused(IOFireWireLink, 3);
		OSMetaClassDeclareReservedUnused(IOFireWireLink, 4);
		OSMetaClassDeclareReservedUnused(IOFireWireLink, 5);
		OSMetaClassDeclareReservedUnused(IOFireWireLink, 6);
		OSMetaClassDeclareReservedUnused(IOFireWireLink, 7);
		OSMetaClassDeclareReservedUnused(IOFireWireLink, 8);
	
	protected:
	
//		FWOHCIIsocInterruptEventSource*	fIsocInterruptEventSource;
//		IOEventSource *					
		IOWorkLoop *					fIsocWorkloop;
};

#endif /* ! _IOKIT_IOFireWireLink_H */

