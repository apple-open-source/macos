/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 *  IOFireWireLibPriv.h
 *  IOFireWireLib
 *
 *  Created on Fri Apr 28 2000.
 *  Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 */

#import <IOKit/firewire/IOFireWireLib.h>
#import <IOKit/firewire/IOFireWireFamilyCommon.h>
#import <stdio.h>
#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/IOKitLib.h>

#if IOFIREWIRELIBDEBUG
	#include <syslog.h>

	#define IOFireWireLibLog_(x...) syslog(LOG_DEBUG, x)
	#define IOFireWireLibLogIfNil_(x, y...) \
	{ if ((void*)(x) == NULL) { IOFireWireLibLog_(y); } }
	#define IOFireWireLibLogIfErr_(x, y...) \
	{ if ((x) != 0) { IOFireWireLibLog_(y); } }
	#define IOFireWireLibLogIfFalse_(x, y...) \
	{ if (!(x)) { IOFireWireLibLog_(y); } }
#else
	#define IOFireWireLibLog_(x...)
	#define IOFireWireLibLogIfNil_(x, y...)
	#define IOFireWireLibLogIfErr_(x, y...)
	#define IOFireWireLibLogIfFalse_(x, y...)
#endif

#define kIOFireWireLibConnection 11

__BEGIN_DECLS
void * IOFireWireLibFactory(CFAllocatorRef allocator, CFUUIDRef typeID) ;
__END_DECLS

#define INTERFACEIMP_INTERFACE \
	0,	\
	& IOFireWireIUnknown::SQueryInterface,	\
	& IOFireWireIUnknown::SAddRef,	\
	& IOFireWireIUnknown::SRelease

namespace IOFireWireLib {

	// ============================================================
	//
	// IOFireWireIUnknown
	//
	// ============================================================

#pragma mark -
	class IOFireWireIUnknown: IUnknown
	{
		protected:
			template<class T>
			struct InterfaceMap
			{
				public:
					InterfaceMap( IUnknownVTbl* vTable, T* inObj ): pseudoVTable(vTable), obj(inObj)		{}
					inline static T* GetThis( void* map )		{ return reinterpret_cast<T*>(reinterpret_cast<InterfaceMap*>(map)->obj) ; }

				private:
					IUnknownVTbl*		pseudoVTable ;
					T*					obj ;
			} ;
	
		public:
			IOFireWireIUnknown( IUnknownVTbl* interface ) ;
			virtual ~IOFireWireIUnknown() {}
			
			virtual HRESULT 					QueryInterface( REFIID iid, LPVOID* ppv ) = 0;
			virtual ULONG 						AddRef() ;
			virtual ULONG 						Release() ;
			
			InterfaceMap<IOFireWireIUnknown>&	GetInterface() const		{ return mInterface ; }
			
			static HRESULT STDMETHODCALLTYPE	SQueryInterface(void* self, REFIID iid, LPVOID* ppv) ;
			static ULONG STDMETHODCALLTYPE		SAddRef(void* self) ;
			static ULONG STDMETHODCALLTYPE		SRelease(void* 	self) ;
		
		private:
			mutable InterfaceMap<IOFireWireIUnknown>	mInterface ;

		protected:
			UInt32										mRefCount ;
	} ;
	
#pragma mark -
	class IOCFPlugIn: public IOFireWireIUnknown
	{
		public:
			IOCFPlugIn() ;
			virtual ~IOCFPlugIn() ;
	
			virtual HRESULT					QueryInterface( REFIID iid, LPVOID* ppv ) ;
			static IOCFPlugInInterface**	Alloc() ;
			
		private:
			// IOCFPlugin methods
			IOReturn 				Probe(CFDictionaryRef propertyTable, io_service_t service, SInt32 *order );
			IOReturn 				Start(CFDictionaryRef propertyTable, io_service_t service );
			IOReturn		 		Stop();	

			//
			// --- CFPlugin static methods ---------
			//
			static IOReturn 		SProbe( void* self, CFDictionaryRef propertyTable, io_service_t service, SInt32 *order );
			static IOReturn 		SStart( void* self, CFDictionaryRef propertyTable, io_service_t service );
			static IOReturn 		SStop( void* self );	
		
		private:
			static IOCFPlugInInterface	sInterface ;
			IOFireWireLibDeviceRef		mDevice ;
	} ;
	
	// ============================================================
	//
	// ¥ class Device
	//
	// ============================================================
	
#pragma mark -
	class Device: public IOFireWireIUnknown
	{
		friend class DeviceCOM ;
		
		public:
									Device( IUnknownVTbl* interface, CFDictionaryRef propertyTable, io_service_t service ) ;
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
			const IOReturn			AddCallbackDispatcherToRunLoop(CFRunLoopRef inRunLoop)	{ return AddCallbackDispatcherToRunLoopForMode( inRunLoop, kCFRunLoopDefaultMode ) ; }
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
											FWKernOSStringRef	inStringRef,
											UInt32				inStringLen,
											CFStringRef*&		text) ;
			IOReturn				CreateCFDataWithOSDataRef(
											FWKernOSDataRef		inDataRef,
											IOByteCount			inDataLen,
											CFDataRef*&			data) ;
			// --- isoch related ----------
			IOReturn				AddIsochCallbackDispatcherToRunLoop(
											CFRunLoopRef		inRunLoop)			{ return AddIsochCallbackDispatcherToRunLoopForMode( inRunLoop, kCFRunLoopDefaultMode ) ; }
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
									CreateLocalIsochPort(
											Boolean					inTalking,
											DCLCommandStruct*		inDCLProgram,
											UInt32					inStartEvent,
											UInt32					inStartState,
											UInt32					inStartMask,
											IOVirtualRange			inDCLProgramRanges[],			// optional optimization parameters
											UInt32					inDCLProgramRangeCount,
											IOVirtualRange			inBufferRanges[],
											UInt32					inBufferRangeCount,
											REFIID 					iid) ;
			IOFireWireLibDCLCommandPoolRef
									CreateDCLCommandPool(
											IOByteCount 			size, 
											REFIID 					iid ) ;
			void					PrintDCLProgram(
											const DCLCommandStruct*		inDCL,
											UInt32						inDCLCount) ;
			IOReturn 				GetBusGeneration( UInt32* outGeneration ) ;
			IOReturn 				GetLocalNodeIDWithGeneration( UInt32 checkGeneration, UInt16* outLocalNodeID ) ;
			IOReturn 				GetRemoteNodeID( UInt32 checkGeneration, UInt16* outRemoteNodeID ) ;
			IOReturn 				GetSpeedToNode( UInt32 checkGeneration, IOFWSpeed* outSpeed) ;
			IOReturn 				GetSpeedBetweenNodes( UInt32 checkGeneration, UInt16 srcNodeID, UInt16 destNodeID,  IOFWSpeed* outSpeed) ;
		
		protected:
			Boolean						mIsInited ;
			Boolean						mNotifyIsOn ;
			mach_port_t					mMasterDevicePort ;
			mach_port_t					mPort ;
			io_object_t					mDefaultDevice ;
			
			// keep track of objects we've dished out
			CFMutableSetRef				mPseudoAddressSpaces ;
			CFMutableSetRef				mPhysicalAddressSpaces ;
			CFMutableSetRef				mUnitDirectories ;
			CFMutableSetRef				mIOFireWireLibCommands ;
			CFMutableSetRef				mConfigDirectories ;
			
			io_connect_t				mConnection ;
		
			CFMachPortRef				mAsyncCFPort ;
			mach_port_t					mAsyncPort ;
			io_async_ref_t				mBusResetAsyncRef ;
			io_async_ref_t				mBusResetDoneAsyncRef ;
			IOFireWireBusResetHandler	mBusResetHandler ;
			IOFireWireBusResetDoneHandler mBusResetDoneHandler ;
			void*						mUserRefCon ;
			
			CFRunLoopRef				mRunLoop ;
			CFRunLoopSourceRef			mRunLoopSource ;
			CFStringRef					mRunLoopMode ;
		
			Boolean						mIsOpen ;
			
			// isoch related
			CFMachPortRef				mIsochAsyncCFPort ;
			mach_port_t					mIsochAsyncPort ;
			CFRunLoopRef				mIsochRunLoop ;
			CFRunLoopSourceRef			mIsochRunLoopSource ;
			CFStringRef					mIsochRunLoopMode ;
			
	} ;
	
	
#pragma mark -
	// ============================================================
	//
	// DeviceCOM
	//
	// ============================================================
	
	class DeviceCOM: public Device
	{
		public:
			DeviceCOM( CFDictionaryRef propertyTable, io_service_t service ) ;
			virtual ~DeviceCOM() ;

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
									SCreateLocalIsochPort(
											IOFireWireLibDeviceRef 	self, 
											Boolean					inTalking,
											DCLCommandStruct*		inDCLProgram,
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
			static void				SPrintDCLProgram(
											IOFireWireLibDeviceRef 	self, 
											DCLCommandStruct*		inDCL,
											UInt32					inDCLCount)  ;
			//
			// v4
			//
			static IOReturn SGetBusGeneration( IOFireWireLibDeviceRef self, UInt32* outGeneration ) ;
			static IOReturn SGetLocalNodeIDWithGeneration( IOFireWireLibDeviceRef self, UInt32 checkGeneration, UInt16* outLocalNodeID ) ;
			static IOReturn SGetRemoteNodeID( IOFireWireLibDeviceRef self, UInt32 checkGeneration, UInt16* outRemoteNodeID ) ;
			static IOReturn SGetSpeedToNode( IOFireWireLibDeviceRef self, UInt32 checkGeneration, IOFWSpeed* outSpeed) ;
			static IOReturn SGetSpeedBetweenNodes( IOFireWireLibDeviceRef self, UInt32 checkGeneration, UInt16 srcNodeID, UInt16 destNodeID,  IOFWSpeed* outSpeed) ;

			protected:
				static IOFireWireDeviceInterface	sInterface ;
	} ;
	
}	// namespace IOFireWireLib
