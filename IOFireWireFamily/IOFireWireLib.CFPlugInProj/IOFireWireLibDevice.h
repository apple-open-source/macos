/*
 *  IOFireWireLibDevice.h
 *  IOFireWireFamily
 *
 *  Created by Niels on Thu Feb 27 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *	$Log: IOFireWireLibDevice.h,v $
 *	Revision 1.23  2007/10/16 16:50:21  ayanowit
 *	Removed existing "work-in-progress" support for buffer-fill isoch.
 *	
 *	Revision 1.22  2007/06/21 04:08:45  collin
 *	*** empty log message ***
 *	
 *	Revision 1.21  2007/05/12 01:10:45  arulchan
 *	Asyncstream transmit command interface
 *	
 *	Revision 1.20  2007/05/03 01:21:29  arulchan
 *	Asyncstream transmit APIs
 *	
 *	Revision 1.19  2007/04/28 02:54:23  collin
 *	*** empty log message ***
 *	
 *	Revision 1.18  2007/03/23 01:47:17  collin
 *	*** empty log message ***
 *	
 *	Revision 1.17  2007/03/22 00:30:01  collin
 *	*** empty log message ***
 *	
 *	Revision 1.16  2007/03/14 02:29:35  collin
 *	*** empty log message ***
 *	
 *	Revision 1.15  2007/03/06 04:50:21  collin
 *	*** empty log message ***
 *	
 *	Revision 1.14  2007/02/09 20:36:46  ayanowit
 *	More Leopard IRMAllocation changes.
 *	
 *	Revision 1.13  2007/01/02 18:14:12  ayanowit
 *	Enabled building the plug-in lib 4-way FAT. Also, fixed compile problems for 64-bit.
 *	
 *	Revision 1.12  2006/12/13 21:34:24  ayanowit
 *	For 4222965, replaced all io async method calls with new Leopard API version.
 *	
 *	Revision 1.11  2006/09/28 22:31:31  arulchan
 *	New Feature rdar::3413505
 *	
 *	Revision 1.10  2006/09/27 22:42:12  ayanowit
 *	Merged in Leopard changes for new IRMAllocation API.
 *	
 *	Revision 1.9  2006/09/22 06:45:19  collin
 *	*** empty log message ***
 *	
 *	Revision 1.8  2004/05/04 22:52:20  niels
 *	*** empty log message ***
 *	
 *	Revision 1.7  2003/11/07 21:24:28  niels
 *	*** empty log message ***
 *	
 *	Revision 1.6  2003/09/15 22:17:10  niels
 *	*** empty log message ***
 *	
 *	Revision 1.5  2003/09/12 22:35:52  niels
 *	padding for digidesign
 *	
 *	Revision 1.4  2003/08/20 18:48:44  niels
 *	*** empty log message ***
 *	
 *	Revision 1.3  2003/08/08 21:03:47  gecko1
 *	Merge max-rec clipping code into TOT
 *	
 *	Revision 1.2  2003/07/21 06:53:10  niels
 *	merge isoch to TOT
 *	
 *	Revision 1.1.2.2  2003/07/18 00:17:47  niels
 *	*** empty log message ***
 *	
 *	Revision 1.1.2.1  2003/07/01 20:54:23  niels
 *	isoch merge
 *	
 */

#import "IOFireWireLibIUnknown.h"
#import "IOFireWireLibPriv.h"

namespace IOFireWireLib {

	class Device: public IOFireWireIUnknown
	{
		friend class DeviceCOM ;
		
		protected:
		
			bool						mIsInited ;
			bool						mNotifyIsOn ;
			mach_port_t					mMasterDevicePort ;
			mach_port_t					mPort ;
			io_object_t					mDefaultDevice ;
			
			void*						mReserved[4] ;
			
			io_connect_t				mConnection ;
		
			CFMachPortRef				mAsyncCFPort ;
			mach_port_t					mAsyncPort ;
			IOFireWireBusResetHandler	mBusResetHandler ;
			IOFireWireBusResetDoneHandler mBusResetDoneHandler ;
			void*						mUserRefCon ;
			
			CFRunLoopRef				mRunLoop ;
			CFRunLoopSourceRef			mRunLoopSource ;
			CFStringRef					mRunLoopMode ;
		
			bool						mIsOpen ;
			
			// isoch related
			CFMachPortRef				mIsochAsyncCFPort ;
			mach_port_t					mIsochAsyncPort ;
			CFRunLoopRef				mIsochRunLoop ;
			CFRunLoopSourceRef			mIsochRunLoopSource ;
			CFStringRef					mIsochRunLoopMode ;

		public:
									Device( const IUnknownVTbl & interface, CFDictionaryRef propertyTable, io_service_t service ) ;
			virtual					~Device() ;

			virtual HRESULT			QueryInterface( REFIID iid, LPVOID* ppv ) ;

			//
			// general user client support
			//
			Boolean					IsInited() const { return mIsInited; }
			io_object_t				GetDevice() const { return mDefaultDevice; }
			IOReturn				Open() ;
			IOReturn				OpenWithSessionRef(
											IOFireWireSessionRef   session) ;
			IOReturn				Seize(		// v3+
											IOOptionBits			inFlags ) ;
			void					Close() ;
			
			//
			// notification methods
			//
			const IOReturn			AddCallbackDispatcherToRunLoop(CFRunLoopRef inRunLoop)	{ return AddCallbackDispatcherToRunLoopForMode( inRunLoop, kCFRunLoopCommonModes ) ; }
			IOReturn				AddCallbackDispatcherToRunLoopForMode(	// v3+
											CFRunLoopRef 				inRunLoop,
											CFStringRef					inRunLoopMode ) ;
			void					RemoveDispatcherFromRunLoop( 
											CFRunLoopRef		inRunLoop,
											CFRunLoopSourceRef	inRunLoopSource,
											CFStringRef			inMode) ;	
			const Boolean			TurnOnNotification(
											void*				callBackRefCon) ;
			void					TurnOffNotification() ;
		
			const IOFireWireBusResetHandler	
									SetBusResetHandler(
											IOFireWireBusResetHandler	inBusResetHandler)  ;
			const IOFireWireBusResetDoneHandler
									SetBusResetDoneHandler(
											IOFireWireBusResetDoneHandler	inBusResetDoneHandler)  ;
		
			static void				BusResetHandler(
											void*				refCon,
											IOReturn			result) ;
			static void				BusResetDoneHandler(
											void*				refCon,
											IOReturn			result) ;
															
			// Call this function when you have completed processing a notification.
			void					ClientCommandIsComplete(
											FWClientCommandID	commandID,
											IOReturn			status)						{}
		
			// --- FireWire send/recv methods --------------	
			IOReturn 				Read(
											io_object_t			device,
											const FWAddress &	addr,
											void*				buf,
											UInt32*				size,
											Boolean				failOnReset = false,
											UInt32				generation = 0) ;
			IOReturn 				ReadQuadlet(
											io_object_t			device,
											const FWAddress &	addr,
											UInt32*				val,
											Boolean				failOnReset = false,
											UInt32				generation = 0) ;
			IOReturn 				Write(
											io_object_t			device,
											const FWAddress &	addr,
											const void*			buf,
											UInt32*				size,
											Boolean				failOnReset = false,
											UInt32				generation = 0) ;
			IOReturn 				WriteQuadlet(
											io_object_t			device,
											const FWAddress &	addr,
											const UInt32		val,
											Boolean				failOnReset = false,
											UInt32				generation = 0) ;
			IOReturn 				CompareSwap(
											io_object_t			device,
											const FWAddress &	addr,
											UInt32 				cmpVal,
											UInt32 				newVal,
											Boolean				failOnReset = false,
											UInt32				generation = 0) ;
			IOReturn				CompareSwap64(
											io_object_t				device,
											const FWAddress &		addr,
											UInt32*					expectedVal,
											UInt32*					newVal,
											UInt32*					oldVal,
											IOByteCount				size,
											Boolean 				failOnReset,
											UInt32					generation = 0) ;
			//
			// command object methods
			//
			IOFireWireLibCommandRef	CreateReadCommand(
											io_object_t			device,
											const FWAddress &	addr,
											void*				buf,
											UInt32				size,
											IOFireWireLibCommandCallback callback,
											Boolean				failOnReset,
											UInt32				generation,
											void*				inRefcon,
											REFIID				iid) ;
			IOFireWireLibCommandRef	CreateReadQuadletCommand(
											io_object_t			device,
											const FWAddress &	addr,
											UInt32				quads[],
											UInt32				numQuads,
											IOFireWireLibCommandCallback callback,
											Boolean				failOnReset,
											UInt32				generation,
											void*				inRefCon,
											REFIID				iid) ;
			IOFireWireLibCommandRef	CreateWriteCommand( io_object_t device, const FWAddress& addr, void* buf, UInt32 size,
											IOFireWireLibCommandCallback callback, Boolean failOnReset, UInt32 generation, 
											void* inRefCon, REFIID iid) ;
			IOFireWireLibCommandRef	CreateWriteQuadletCommand( io_object_t device, const FWAddress& addr, UInt32 quads[], 
											UInt32 numQuads, IOFireWireLibCommandCallback callback, Boolean failOnReset, 
											UInt32 generation, void* inRefCon, REFIID iid) ;
			IOFireWireLibCommandRef	CreateCompareSwapCommand( io_object_t device, const FWAddress& addr, UInt64 cmpVal, UInt64 newVal,
											unsigned int quads, IOFireWireLibCommandCallback callback, Boolean failOnReset,
											UInt32 generation, void* inRefCon, REFIID iid) ;
			
			//
			// other methods
			//
			IOReturn 				BusReset() ;			
			IOReturn				GetCycleTime(
											UInt32*				cycleTime) ;
			IOReturn				GetBusCycleTime(
											UInt32*				busTime,
											UInt32 *			cycleTime) ;
			IOReturn				GetGenerationAndNodeID(
											UInt32*				outGeneration,
											UInt16*				outNodeID) ;
			IOReturn				GetLocalNodeID(
											UInt16*				outLocalNodeID) ;
			IOReturn				GetResetTime(
											AbsoluteTime*		resetTime) ;
		
			// address space support
			IOFireWireLibPseudoAddressSpaceRef		
									CreatePseudoAddressSpace(
											UInt32 				inSize,
											void*				inRefCon,
											UInt32				inQueueBufferSize,
											void*				inBackingStore,
											UInt32				inFlags,
											REFIID				iid)						{ return CreateAddressSpace( inSize, inRefCon, inQueueBufferSize, inBackingStore, inFlags, iid ) ; }
			IOFireWireLibPhysicalAddressSpaceRef	
									CreatePhysicalAddressSpace(
											UInt32				inLength,
											void*				inBackingStore,
											UInt32				inFlags,
											REFIID				iid) ;
			IOFireWireLibPseudoAddressSpaceRef
									CreateInitialUnitsPseudoAddressSpace( 
											UInt32						inAddressLo,
											UInt32  					inSize, 
											void*  						inRefCon, 
											UInt32  					inQueueBufferSize, 
											void*  						inBackingStore, 
											UInt32  					inFlags,
											REFIID  					iid)				{ return CreateAddressSpace( inSize, inRefCon, inQueueBufferSize, inBackingStore, inFlags, iid, true, inAddressLo ) ; }
			IOFireWireLibPseudoAddressSpaceRef
									CreateAddressSpace(
											UInt32						inSize,
											void*						inRefCon,
											UInt32						inQueueBufferSize,
											void*						inBackingStore,
											UInt32						inFlags,
											REFIID						iid,
											Boolean						isInitialUnits = false,
											UInt32						inAddressLo = 0 ) ;
														
			// --- config ROM support ----------------------
			IOFireWireLibLocalUnitDirectoryRef		
									CreateLocalUnitDirectory(
											REFIID				iid) ;
			IOFireWireLibConfigDirectoryRef
									GetConfigDirectory(
											REFIID				iid) ;
			IOFireWireLibConfigDirectoryRef
									CreateConfigDirectoryWithIOObject(
											io_object_t			inObject,
											REFIID				iid) ;
		
			// --- debugging -------------------------------
			IOReturn				FireBugMsg(
											const char*			msg) ;
			IOReturn				FireLog( const char * format, va_list ap ) ;
			IOReturn				FireLog( const char* string, ... ) ;
		
			// --- internal use methods --------------------
			IOReturn				OpenDefaultConnection() ;
			const io_object_t		GetUserClientConnection() const 	{ return mConnection; }
			const io_connect_t		GetDefaultDevice() const 			{ return mDefaultDevice; }
			
			const CFMachPortRef		GetAsyncCFPort() const 				{ return mAsyncCFPort; }
			const mach_port_t		GetAsyncPort() const 				{ return mAsyncPort ; }
			IOReturn				CreateAsyncPorts() ;
			Boolean					AsyncPortsExist() const 			{ return ((mAsyncCFPort != 0) && (mAsyncPort != 0)); }
			const CFMachPortRef		GetIsochAsyncCFPort() const 		{ return mIsochAsyncCFPort ; }
			const mach_port_t		GetIsochAsyncPort() const 			{ return mIsochAsyncPort ; }
			IOReturn				CreateIsochAsyncPorts() ;
			const Boolean			IsochAsyncPortsExist() const		{ return ((mIsochAsyncCFPort != 0) && (mIsochAsyncPort != 0)); }
		
			IOReturn				CreateCFStringWithOSStringRef(
											UserObjectHandle	inStringRef,
											UInt32				inStringLen,
											CFStringRef*&		text) ;
			IOReturn				CreateCFDataWithOSDataRef(
											UserObjectHandle		inDataRef,
											IOByteCount			inDataLen,
											CFDataRef*&			data) ;
			// --- isoch related ----------
			IOReturn				AddIsochCallbackDispatcherToRunLoop(
											CFRunLoopRef		inRunLoop)			{ return AddIsochCallbackDispatcherToRunLoopForMode( inRunLoop, kCFRunLoopCommonModes ) ; }
			IOReturn				AddIsochCallbackDispatcherToRunLoopForMode( // v3+
											CFRunLoopRef			inRunLoop,
											CFStringRef 			inRunLoopMode ) ;
			void					RemoveIsochCallbackDispatcherFromRunLoop() ;	// v3+
			IOFireWireLibIsochChannelRef 
									CreateIsochChannel(
											Boolean 				doIrm, 
											UInt32 					packetSize, 
											IOFWSpeed 				prefSpeed,
											REFIID 					iid) ;
			IOFireWireLibRemoteIsochPortRef
									CreateRemoteIsochPort(
											Boolean					inTalking,
											REFIID 					iid) ;
			IOFireWireLibLocalIsochPortRef
									CreateLocalIsochPortWithOptions(
											Boolean					talking,
											DCLCommand*				dclProgram,
											UInt32					startEvent,
											UInt32					startState,
											UInt32					startMask,
											IOVirtualRange			dclProgramRanges[],			// optional optimization parameters
											UInt32					dclProgramRangeCount,
											IOVirtualRange			bufferRanges[],
											UInt32					bufferRangeCount,
											IOFWIsochPortOptions	options,
											REFIID 					iid) ;
			IOFireWireLibDCLCommandPoolRef
									CreateDCLCommandPool(
											IOByteCount 			size, 
											REFIID 					iid ) ;
			IOReturn 				GetBusGeneration( UInt32* outGeneration ) ;
			IOReturn 				GetLocalNodeIDWithGeneration( UInt32 checkGeneration, UInt16* outLocalNodeID ) ;
			IOReturn 				GetRemoteNodeID( UInt32 checkGeneration, UInt16* outRemoteNodeID ) ;
			IOReturn 				GetSpeedToNode( UInt32 checkGeneration, IOFWSpeed* outSpeed) ;
			IOReturn 				GetSpeedBetweenNodes( UInt32 checkGeneration, UInt16 srcNodeID, UInt16 destNodeID,  IOFWSpeed* outSpeed) ;
			IOReturn				GetIRMNodeID( UInt32 checkGeneration, UInt16* irmNodeID ) ;
			IOReturn				ClipMaxRec2K( Boolean clipMaxRec ) ;
			IOFireWireSessionRef	GetSessionRef() ;

			static inline MethodSelector	MakeSelectorWithObject( MethodSelector selector, UserObjectHandle obj )		{ return (MethodSelector)( (unsigned long)obj << 16 | selector & 0xFFFF ) ; }		
			
			IOFireWireLibVectorCommandRef CreateVectorCommand( IOFireWireLibCommandCallback callback, void* inRefCon,  REFIID iid );
			
			IOReturn AllocateIRMBandwidthInGeneration(UInt32 bandwidthUnits, UInt32 generation) ;

			IOReturn ReleaseIRMBandwidthInGeneration(UInt32 bandwidthUnits, UInt32 generation) ;

			IOReturn AllocateIRMChannelInGeneration(UInt8 isochChannel, UInt32 generation) ;

			IOReturn ReleaseIRMChannelInGeneration(UInt8 isochChannel, UInt32 generation) ;

			IOFireWireLibIRMAllocationRef CreateIRMAllocation(Boolean releaseIRMResourcesOnFree, 
															  IOFireWireLibIRMAllocationLostNotificationProc callback,
															  void *pLostNotificationProcRefCon,
															  REFIID iid) ; 

			IOFWAsyncStreamListenerInterfaceRef	CreateAsyncStreamListener(  UInt32		channel,
																			IOFWAsyncStreamListenerHandler	callback,																			
																			void*  		inRefCon, 
																			UInt32  	inQueueBufferSize, 
																			REFIID  	iid ) ;
	
			IOFireWireLibCommandRef		CreatePHYCommand(	UInt32							data1,
															UInt32							data2,
															IOFireWireLibCommandCallback	callback, 
															Boolean							failOnReset, 
															UInt32							generation,
															void*							inRefCon, 
															REFIID							iid );
																													   
			IOFireWireLibPHYPacketListenerRef	CreatePHYPacketListener( UInt32 queueCount, REFIID iid );

			IOFireWireLibCommandRef	CreateAsyncStreamCommand(	UInt32							channel,
																UInt32							sync,
																UInt32							tag,
																void*							buf, 
																UInt32							size,
																IOFireWireLibCommandCallback	callback, 
																Boolean							failOnReset,
																UInt32							generation,
																void*							inRefCon,
																REFIID							iid);

			IOReturn GetCycleTimeAndUpTime(	UInt32*		outCycleTime,
											UInt64*		outUpTime );
	} ;
	
	
#pragma mark -
	// ============================================================
	//
	// DeviceCOM
	//
	// ============================================================
	
	class DeviceCOM: public Device
	{
		protected:
		
			static const IOFireWireDeviceInterface sInterface ;
			
		public:
		
			DeviceCOM( CFDictionaryRef propertyTable, io_service_t service ) ;
//			virtual ~DeviceCOM() ;

			//
			// --- IUnknown -----------------------
			//
			static IOFireWireDeviceInterface**	Alloc( CFDictionaryRef propertyTable, io_service_t service ) ;
		
			//
			// === STATIC METHODS ==========================						
			//
					
			//
			// --- FireWire user client maintenance -----------
			//
			static Boolean			SInterfaceIsInited(
											IOFireWireLibDeviceRef self)  ;
			static io_object_t		SGetDevice(
											IOFireWireLibDeviceRef self) ;
			static IOReturn			SOpen(
											IOFireWireLibDeviceRef self) ;
			static IOReturn			SOpenWithSessionRef(
											IOFireWireLibDeviceRef self,
											IOFireWireSessionRef   session) ;
			static IOReturn			SSeize(		// v3+
											IOFireWireLibDeviceRef 	self,
											IOOptionBits			inFlags,
											... ) ;
			static void				SClose(
											IOFireWireLibDeviceRef self) ;
			
			// --- FireWire notification methods --------------
			static const Boolean	SNotificationIsOn(
											IOFireWireLibDeviceRef self) ;
		
			// This function adds the proper event source to the passed in CFRunLoop. Call this
			// before trying to use any IOFireWireLib notifications.
			static const IOReturn	SAddCallbackDispatcherToRunLoop(
											IOFireWireLibDeviceRef 		self, 
											CFRunLoopRef 				inRunLoop) ;
			static IOReturn			SAddCallbackDispatcherToRunLoopForMode(	// v3+
											IOFireWireLibDeviceRef 		self, 
											CFRunLoopRef 				inRunLoop,
											CFStringRef					inRunLoopMode ) ;
			static const void		SRemoveCallbackDispatcherFromRunLoop(
											IOFireWireLibDeviceRef 		self) ;
		
			// Makes notification active. Returns false if notification could not be activated.
			static const Boolean	STurnOnNotification(
											IOFireWireLibDeviceRef		self) ;
		
			// Notification callbacks will no longer be called.
			static void				STurnOffNotification(
											IOFireWireLibDeviceRef self) ;
			static const IOFireWireBusResetHandler	
									SSetBusResetHandler(
											IOFireWireLibDeviceRef		self,
											IOFireWireBusResetHandler	inBusResetHandler)  ;
			static const IOFireWireBusResetDoneHandler
									SSetBusResetDoneHandler(
											IOFireWireLibDeviceRef		self,
											IOFireWireBusResetDoneHandler	inBusResetDoneHandler)  ;
											
			// Call this function when you have completed processing a notification.
			static void				SClientCommandIsComplete(
											IOFireWireLibDeviceRef		self,
											FWClientCommandID			commandID,
											IOReturn					status) ;
		
			// --- FireWire send/recv methods --------------	
			static IOReturn 		SRead(
											IOFireWireLibDeviceRef	self,
											io_object_t 		device,
											const FWAddress* addr, 
											void*				buf,
											UInt32*				size,
											Boolean				failOnReset = false,
											UInt32				generation = 0) ;
			static IOReturn 		SReadQuadlet(
											IOFireWireLibDeviceRef 	self,
											io_object_t 			device,
											const FWAddress* 		addr, 
											UInt32*					val,
											Boolean					failOnReset = false,
											UInt32					generation = 0) ;
			static IOReturn 		SWrite(
											IOFireWireLibDeviceRef 	self,
											io_object_t 			device,
											const FWAddress* 		addr, 
											const void*				buf,
											UInt32*					size,
											Boolean					failOnReset = false,
											UInt32					generation = 0) ;
			static IOReturn 		SWriteQuadlet(
											IOFireWireLibDeviceRef 	self,
											io_object_t 			device,
											const FWAddress* 		addr, 
											const UInt32			val,
											Boolean					failOnReset = false,
											UInt32					generation = 0) ;
			static IOReturn 		SCompareSwap(
											IOFireWireLibDeviceRef 	self,
											io_object_t				device,
											const FWAddress* 		addr, 
											UInt32 					cmpVal,
											UInt32 					newVal,
											Boolean					failOnReset = false,
											UInt32					generation = 0) ;
			static IOReturn			SCompareSwap64( IOFireWireLibDeviceRef self, io_object_t device, 
											const FWAddress* addr, UInt32* expectedVal, UInt32* newVal, UInt32* oldVal, 
											IOByteCount size, Boolean failOnReset, UInt32 generation) ;
		
			// --- FireWire command object methods ---------
		
			static IOFireWireLibCommandRef	
									SCreateReadCommand(
											IOFireWireLibDeviceRef	self,
											io_object_t			device,
											const FWAddress*	addr,
											void*				buf,
											UInt32				size,
											IOFireWireLibCommandCallback callback,
											Boolean				failOnReset,
											UInt32				generation,
											void*				inRefcon,
											REFIID				iid) ;
			static IOFireWireLibCommandRef	
									SCreateReadQuadletCommand(
											IOFireWireLibDeviceRef	self,
											io_object_t			device,
											const FWAddress *	addr,
											UInt32				quads[],
											UInt32				numQuads,
											IOFireWireLibCommandCallback callback,									
											Boolean				failOnReset,
											UInt32				generation,
											void*				inRefCon,
											REFIID				iid) ;
			static IOFireWireLibCommandRef
									SCreateWriteCommand(
											IOFireWireLibDeviceRef	self,
											io_object_t			device,
											const FWAddress*	addr,
											void*				buf,
											UInt32 				size,
											IOFireWireLibCommandCallback callback,
											Boolean				failOnReset,
											UInt32				generation,
											void*				inRefCon,
											REFIID				iid) ;
			static IOFireWireLibCommandRef
									SCreateWriteQuadletCommand(
											IOFireWireLibDeviceRef	self,
											io_object_t			device,
											const FWAddress *	addr,
											UInt32				quads[],
											UInt32				numQuads,
											IOFireWireLibCommandCallback callback,
											Boolean				failOnReset,
											UInt32				generation,
											void*				inRefCon,
											REFIID				iid) ;
			static IOFireWireLibCommandRef
									SCreateCompareSwapCommand(
											IOFireWireLibDeviceRef	self,
											io_object_t			device,
											const FWAddress *	addr,
											UInt32 				cmpVal,
											UInt32 				newVal,
											IOFireWireLibCommandCallback callback,
											Boolean				failOnReset,
											UInt32				generation,
											void*				inRefCon,
											REFIID				iid) ;
			static IOFireWireLibCommandRef
									SCreateCompareSwapCommand64(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress* addr, 
											UInt64 cmpVal, UInt64 newVal, IOFireWireLibCommandCallback callback, Boolean failOnReset, 
											UInt32 generation, void* inRefCon, REFIID iid ) ;
			
			// --- other methods ----------
			static IOReturn			SBusReset(
											IOFireWireLibDeviceRef			self) ;
			static IOReturn			SGetCycleTime(
											IOFireWireLibDeviceRef			self,
											UInt32*					outCycleTime)
											{ return IOFireWireIUnknown::InterfaceMap<Device>::GetThis(self)->GetCycleTime(outCycleTime); }

			static IOReturn			SGetCycleTimeAndUpTime(
											IOFireWireLibDeviceRef			self,
											UInt32*					outCycleTime,
											UInt64*		outUpTime )
											{ return IOFireWireIUnknown::InterfaceMap<Device>::GetThis(self)->GetCycleTimeAndUpTime(outCycleTime, outUpTime); }
											
			static IOReturn			SGetBusCycleTime(
											IOFireWireLibDeviceRef			self,
											UInt32*					outBusTime,
											UInt32*					outCycleTime)
											{ return IOFireWireIUnknown::InterfaceMap<Device>::GetThis(self)->GetBusCycleTime(outBusTime, outCycleTime); }
			static IOReturn			SGetGenerationAndNodeID(
											IOFireWireLibDeviceRef self,
											UInt32*				outGeneration,
											UInt16*				outNodeID)
											{ return IOFireWireIUnknown::InterfaceMap<Device>::GetThis(self)->GetGenerationAndNodeID(outGeneration, outNodeID); }
			static IOReturn			SGetLocalNodeID(
											IOFireWireLibDeviceRef	self,
											UInt16*					outLocalNodeID)
											{ return IOFireWireIUnknown::InterfaceMap<Device>::GetThis(self)->GetLocalNodeID(outLocalNodeID); }
			static IOReturn			SGetResetTime(
											IOFireWireLibDeviceRef	self,
											AbsoluteTime*			outResetTime)
											{ return IOFireWireIUnknown::InterfaceMap<Device>::GetThis(self)->GetResetTime(outResetTime); }
		
			// --- config rom support ----------------------
			static IOFireWireLibLocalUnitDirectoryRef			
									SCreateLocalUnitDirectory(
											IOFireWireLibDeviceRef	self,
											REFIID					iid) ;
		
			// --- config directory support ----------------
			static IOFireWireLibConfigDirectoryRef
									SGetConfigDirectory(
											IOFireWireLibDeviceRef	self,
											REFIID					iid) ;
			static IOFireWireLibConfigDirectoryRef
									SCreateConfigDirectoryWithIOObject(
											IOFireWireLibDeviceRef	self,
											io_object_t				inObject,
											REFIID					iid) ;
											
			// --- pseudo address space support ------------
			static IOFireWireLibPseudoAddressSpaceRef		
									SCreatePseudoAddressSpace(
											IOFireWireLibDeviceRef self,
											UInt32 			inLength, 
											void*			inRefCon,
											UInt32			inQueueBufferSize,
											void*			inBackingStore,
											UInt32			inFlags,
											REFIID			iid) ;
			static IOFireWireLibPhysicalAddressSpaceRef
									SCreatePhysicalAddressSpace(
											IOFireWireLibDeviceRef self,
											UInt32			inLength,
											void*			inBackingStore,
											UInt32			inFlags,
											REFIID			iid) ;
			static IOFireWireLibPseudoAddressSpaceRef
									SCreateInitialUnitsPseudoAddressSpace( 
											IOFireWireLibDeviceRef  	self,
											UInt32						inAddressLo,
											UInt32  					inSize, 
											void*  						inRefCon, 
											UInt32  					inQueueBufferSize, 
											void*  						inBackingStore, 
											UInt32  					inFlags,
											REFIID  					iid) ;
		
			// --- debugging -------------------------------
			static IOReturn			SFireBugMsg(
											IOFireWireLibDeviceRef self,
											const char *			msg)
											{ return IOFireWireIUnknown::InterfaceMap<Device>::GetThis(self)->FireBugMsg(msg); }
			static IOReturn			SFireLog(
											IOFireWireLibDeviceRef self,
											const char *		format, 
											... ) ;
			
			// --- isoch -----------------------------------
			static IOReturn			SAddIsochCallbackDispatcherToRunLoop(
											IOFireWireLibDeviceRef 	self, 
											CFRunLoopRef 			inRunLoop) ;
			static IOReturn			SAddIsochCallbackDispatcherToRunLoopForMode( // v3+
											IOFireWireLibDeviceRef 	self, 
											CFRunLoopRef			inRunLoop,
											CFStringRef 			inRunLoopMode ) ;
			static void				SRemoveIsochCallbackDispatcherFromRunLoop(	// v3+
											IOFireWireLibDeviceRef 	self) ;
			static IOFireWireLibIsochChannelRef 
									SCreateIsochChannel(
											IOFireWireLibDeviceRef	self, 
											Boolean 				doIrm, 
											UInt32 					packetSize, 
											IOFWSpeed 				prefSpeed,
											REFIID 					iid) ;
			static IOFireWireLibRemoteIsochPortRef
									SCreateRemoteIsochPort(
											IOFireWireLibDeviceRef 	self, 
											Boolean					inTalking,
											REFIID 					iid) ;
			static IOFireWireLibLocalIsochPortRef
									S_CreateLocalIsochPort(
											IOFireWireLibDeviceRef 	self, 
											Boolean					inTalking,
											DCLCommand*		inDCLProgram,
											UInt32					inStartEvent,
											UInt32					inStartState,
											UInt32					inStartMask,
											IOVirtualRange			inDCLProgramRanges[],			// optional optimization parameters
											UInt32					inDCLProgramRangeCount,
											IOVirtualRange			inBufferRanges[],
											UInt32					inBufferRangeCount,
											REFIID 					iid) ;
			static IOFireWireLibDCLCommandPoolRef
									SCreateDCLCommandPool(
											IOFireWireLibDeviceRef 	self, 
											IOByteCount 			size, 
											REFIID 					iid ) ;
			static void*			SGetRefCon(
											IOFireWireLibDeviceRef 	self)		{ return IOFireWireIUnknown::InterfaceMap<Device>::GetThis(self)->mUserRefCon ; }
			static void				SSetRefCon(
											IOFireWireLibDeviceRef 	self,
											const void*				inRefCon )	{ IOFireWireIUnknown::InterfaceMap<Device>::GetThis(self)->mUserRefCon = const_cast<void*>(inRefCon) ; }
			static void				SPrintDCLProgram( IOFireWireLibDeviceRef self, const DCLCommand* dcl, UInt32 dclCount )  ;
			//
			// v4
			//
			static IOReturn SGetBusGeneration( IOFireWireLibDeviceRef self, UInt32* outGeneration ) ;
			static IOReturn SGetLocalNodeIDWithGeneration( IOFireWireLibDeviceRef self, UInt32 checkGeneration, UInt16* outLocalNodeID ) ;
			static IOReturn SGetRemoteNodeID( IOFireWireLibDeviceRef self, UInt32 checkGeneration, UInt16* outRemoteNodeID ) ;
			static IOReturn SGetSpeedToNode( IOFireWireLibDeviceRef self, UInt32 checkGeneration, IOFWSpeed* outSpeed) ;
			static IOReturn SGetSpeedBetweenNodes( IOFireWireLibDeviceRef self, UInt32 checkGeneration, UInt16 srcNodeID, UInt16 destNodeID,  IOFWSpeed* outSpeed) ;


			//
			// v5
			//
			
			static IOReturn	S_GetIRMNodeID( IOFireWireLibDeviceRef self, UInt32 checkGeneration, UInt16* outIRMNodeID ) ;

			//
			// v6
			//
			
			static IOReturn	S_ClipMaxRec2K( IOFireWireLibDeviceRef self, Boolean clipMaxRec ) ;
			static IOFireWireLibNuDCLPoolRef				S_CreateNuDCLPool( IOFireWireLibDeviceRef self, UInt32 capacity, REFIID iid ) ;

			//
			// v7
			//
			
			static IOFireWireSessionRef						S_GetSessionRef( IOFireWireLibDeviceRef self ) ;

			//
			// v8
			//
			
			static IOFireWireLibLocalIsochPortRef
									S_CreateLocalIsochPortWithOptions(
											IOFireWireLibDeviceRef 	self, 
											Boolean					inTalking,
											DCLCommand*		inDCLProgram,
											UInt32					inStartEvent,
											UInt32					inStartState,
											UInt32					inStartMask,
											IOVirtualRange			inDCLProgramRanges[],			// optional optimization parameters
											UInt32					inDCLProgramRangeCount,
											IOVirtualRange			inBufferRanges[],
											UInt32					inBufferRangeCount,
											IOFWIsochPortOptions	options,
											REFIID 					iid) ;
											
			//
			// v9
			//

			static IOFireWireLibVectorCommandRef S_CreateVectorCommand( 
												IOFireWireLibDeviceRef self,
												IOFireWireLibCommandCallback callback, 
												void * inRefCon, 
												REFIID iid);	
			
			static IOReturn S_AllocateIRMBandwidthInGeneration(IOFireWireLibDeviceRef self, UInt32 bandwidthUnits, UInt32 generation) ;
			
			static IOReturn S_ReleaseIRMBandwidthInGeneration(IOFireWireLibDeviceRef self, UInt32 bandwidthUnits, UInt32 generation) ;
			
			static IOReturn S_AllocateIRMChannelInGeneration(IOFireWireLibDeviceRef self, UInt8 isochChannel, UInt32 generation) ;
			
			static IOReturn S_ReleaseIRMChannelInGeneration(IOFireWireLibDeviceRef self, UInt8 isochChannel, UInt32 generation) ;
			
			static IOFireWireLibIRMAllocationRef S_CreateIRMAllocation( IOFireWireLibDeviceRef self, 
																		Boolean releaseIRMResourcesOnFree, 
																		IOFireWireLibIRMAllocationLostNotificationProc callback,
																		void *pLostNotificationProcRefCon,
																		REFIID iid) ; 
																		
			static IOFWAsyncStreamListenerInterfaceRef S_CreateAsyncStreamListener( IOFireWireLibDeviceRef			self, 
																					UInt32							channel,
																					IOFWAsyncStreamListenerHandler	callback,																			
																					void*							inRefCon, 
																					UInt32							inQueueBufferSize, 
																					REFIID							iid ) ;
																					
			static mach_port_t S_GetIsochAsyncPort( IOFireWireLibDeviceRef	self );
		
			static IOFireWireLibCommandRef	S_CreatePHYCommand(	IOFireWireLibDeviceRef			self, 
																	UInt32							data1,
																	UInt32							data2,
																	IOFireWireLibCommandCallback	callback, 
																	Boolean							failOnReset, 
																	UInt32							generation,
																	void *							inRefCon, 
																	REFIID							iid );		
																	
			static IOFireWireLibPHYPacketListenerRef S_CreatePHYPacketListener(	IOFireWireLibDeviceRef self,
																				UInt32	queueCount,  
																				REFIID iid );
																				
			static	IOFireWireLibCommandRef	S_CreateAsyncStreamCommand(	IOFireWireLibDeviceRef			self, 
																		UInt32							channel,
																		UInt32							sync,
																		UInt32							tag,
																		void*							buf, 
																		UInt32							size,
																		IOFireWireLibCommandCallback	callback, 
																		Boolean							failOnReset,
																		UInt32							generation,
																		void*							inRefCon,
																		REFIID							iid);
																				
	} ;
} // namespace
