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
 *  Created by NWG on Fri Apr 28 2000.
 *  Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef _IOKIT_IOFireWireLibPriv_H_
#define _IOKIT_IOFireWireLibPriv_H_

#include <stdio.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include "IOFireWireLib.h"
#include "IOFireWireFamilyCommon.h"

#define kIOFireWireLibConnection 11

__BEGIN_DECLS
void * IOFireWireLibFactory(CFAllocatorRef allocator, CFUUIDRef typeID) ;
__END_DECLS

#define INTERFACEIMP_INTERFACE \
	0,	\
	& IOFireWireIUnknown::SQueryInterface,	\
	& IOFireWireIUnknown::SAddRef,	\
	& IOFireWireIUnknown::SRelease



// ============================================================
//
// ¥ class IOFireWireIUnknown
//
// ============================================================

class IOFireWireIUnknown: IUnknown
{
	struct InterfaceMap
	{
		IUnknownVTbl*		pseudoVTable ;
		IOFireWireIUnknown*	obj ;
	} ;

 public:
	static HRESULT STDMETHODCALLTYPE	SQueryInterface(void* self, REFIID iid, LPVOID* ppv) ;
	static ULONG STDMETHODCALLTYPE		SAddRef(void* self) ;
	static ULONG STDMETHODCALLTYPE		SRelease(void* 	self) ;

    virtual HRESULT QueryInterface(REFIID iid, LPVOID* ppv) = 0;
    virtual ULONG AddRef(void) ;
    virtual ULONG Release(void) ;
	
 	// GetThis
 	inline static IOFireWireIUnknown* GetThis(void* interface)
		{ return (IOFireWireIUnknown *) ((InterfaceMap *) interface)->obj; };

	IOFireWireIUnknown(): mRefCount(1) {}
	virtual ~IOFireWireIUnknown() {}
	
 protected:
	UInt32				mRefCount ;
} ;



// ============================================================
//
// ¥ class IOFireWireDeviceInterfaceImp
//
// ============================================================

class IOFireWireDeviceInterfaceImp: public IOFireWireIUnknown
{
 public:
	//
	// === VIRTUAL METHODS ======================
	//
							IOFireWireDeviceInterfaceImp() ;
	virtual					~IOFireWireDeviceInterfaceImp() ;
				
	//
	// --- CFPlugin static methods ---------
	//
	IOReturn 				Probe(CFDictionaryRef propertyTable, 
								 io_service_t service, SInt32 *order );
    IOReturn 				Start(CFDictionaryRef propertyTable, 
								 io_service_t service );
	IOReturn		 		Stop();	
	
	//
	// --- FireWire user client support ----------
	//
	virtual Boolean			IsInited() const { return mIsInited; }
	virtual io_object_t		GetDevice() const { return mDefaultDevice; }
	virtual IOReturn		Open() ;
	virtual IOReturn		OpenWithSessionRef(
									IOFireWireSessionRef   session) ;
	virtual void			Close() ;
	
	//
	// --- FireWire notification methods --------------
	// (see also FWUserPseudoAddressSpace, below)
	//
	virtual const IOReturn	AddCallbackDispatcherToRunLoop(CFRunLoopRef inRunLoop) ;
	virtual void			RemoveCallbackDispatcherFromRunLoop() ;	

	// Is notification active? (i.e. Will I be notified of incoming packet writes/reads/drops?)
	virtual const Boolean	NotificationIsOn() { return mNotifyIsOn; }
	
	// Makes notification active. Returns false if notification could not be activated.
	virtual const Boolean	TurnOnNotification(
									void*				callBackRefCon) ;
													
	// Notification callbacks will no longer be called.
	virtual void			TurnOffNotification() ;

	virtual const IOFireWireBusResetHandler	
							SetBusResetHandler(
									IOFireWireBusResetHandler	inBusResetHandler)  ;
	virtual const IOFireWireBusResetDoneHandler
							SetBusResetDoneHandler(
									IOFireWireBusResetDoneHandler	inBusResetDoneHandler)  ;

	static void				BusResetHandler(
									void*				refCon,
									IOReturn			result) ;
	static void				BusResetDoneHandler(
									void*				refCon,
									IOReturn			result) ;
													
		// Call this function when you have completed processing a notification.
	virtual void			ClientCommandIsComplete(
									FWClientCommandID	commandID,
									IOReturn			status) ;

	// --- FireWire send/recv methods --------------	
	virtual IOReturn 		Read(
									io_object_t			device,
									const FWAddress &	addr,
									void*				buf,
									UInt32*				size,
									Boolean				failOnReset = false,
									UInt32				generation = 0) ;
    virtual IOReturn 		ReadQuadlet(
									io_object_t			device,
									const FWAddress &	addr,
									UInt32*				val,
									Boolean				failOnReset = false,
									UInt32				generation = 0) ;
    virtual IOReturn 		Write(
									io_object_t			device,
									const FWAddress &	addr,
									const void*			buf,
									UInt32*				size,
									Boolean				failOnReset = false,
									UInt32				generation = 0) ;
    virtual IOReturn 		WriteQuadlet(
									io_object_t			device,
									const FWAddress &	addr,
									const UInt32		val,
									Boolean				failOnReset = false,
									UInt32				generation = 0) ;
    virtual IOReturn 		CompareSwap(
									io_object_t			device,
									const FWAddress &	addr,
									UInt32 				cmpVal,
									UInt32 				newVal,
									Boolean				failOnReset = false,
									UInt32				generation = 0) ;
	
	// --- FireWire command object methods ---------

	virtual IOFireWireLibCommandRef	
							CreateReadCommand(
									io_object_t			device,
									const FWAddress &	addr,
									void*				buf,
									UInt32				size,
									IOFireWireLibCommandCallback callback,
									Boolean				failOnReset,
									UInt32				generation,
									void*				inRefcon,
									REFIID				iid) ;
	virtual IOFireWireLibCommandRef	
							CreateReadQuadletCommand(
									io_object_t			device,
									const FWAddress &	addr,
									UInt32				quads[],
									UInt32				numQuads,
									IOFireWireLibCommandCallback callback,
									Boolean				failOnReset,
									UInt32				generation,
									void*				inRefCon,
									REFIID				iid) ;
	virtual IOFireWireLibCommandRef
							CreateWriteCommand(
									io_object_t			device,
									const FWAddress &	addr,
									void*				buf,
									UInt32 				size,
									IOFireWireLibCommandCallback callback,
									Boolean				failOnReset,
									UInt32				generation,
									void*				inRefCon,
									REFIID				iid) ;
	virtual IOFireWireLibCommandRef
							CreateWriteQuadletCommand(
									io_object_t			device,
									const FWAddress &	addr,
									UInt32				quads[],
									UInt32				numQuads,
									IOFireWireLibCommandCallback callback,
									Boolean				failOnReset,
									UInt32				generation,
									void*				inRefCon,
									REFIID				iid) ;
	virtual IOFireWireLibCommandRef
							CreateCompareSwapCommand(
									io_object_t			device,
									const FWAddress &	addr,
									UInt32 				cmpVal,
									UInt32 				newVal,
									IOFireWireLibCommandCallback callback,
									Boolean				failOnReset,
									UInt32				generation,
									void*				inRefCon,
									REFIID				iid) ;
	
	// --- other methods ----------
    virtual IOReturn 		BusReset() ;
	
	virtual IOReturn		GetCycleTime(
									UInt32*				cycleTime) ;
											
	virtual IOReturn		GetGenerationAndNodeID(
									UInt32*				outGeneration,
									UInt16*				outNodeID) ;

	virtual IOReturn		GetLocalNodeID(
									UInt16*				outLocalNodeID) ;

	virtual IOReturn		GetResetTime(
									AbsoluteTime*		resetTime) ;

	// --- address space support ------------------
    virtual IOFireWireLibPseudoAddressSpaceRef		
    						CreatePseudoAddressSpace(
									UInt32 				inLength,
									void*				inRefCon,
									UInt32				inQueueBufferSize,
									void*				inBackingStore,
									UInt32				inFlags,
									REFIID				iid) ;
	virtual IOFireWireLibPhysicalAddressSpaceRef	
							CreatePhysicalAddressSpace(
									UInt32				inLength,
									void*				inBackingStore,
									UInt32				inFlags,
									REFIID				iid) ;
												
	// --- config rom support ----------------------
	virtual IOFireWireLibLocalUnitDirectoryRef		
							CreateLocalUnitDirectory(
									REFIID				iid) ;
	virtual IOFireWireLibConfigDirectoryRef
							GetConfigDirectory(
									REFIID				iid) ;
	virtual IOFireWireLibConfigDirectoryRef
							CreateConfigDirectoryWithIOObject(
									io_object_t			inObject,
									REFIID				iid) ;

	// --- debugging -------------------------------
	virtual IOReturn		FireBugMsg(
									const char*				msg) ;

	// --- internal use methods --------------------
	const io_connect_t		OpenDefaultConnection() ;
	const io_object_t		GetUserClientConnection() const { return mUserClientConnection; }
	const io_connect_t		GetDefaultDevice() const { return mDefaultDevice; }
	
	const CFMachPortRef		GetAsyncCFPort() const { return mAsyncCFPort; }
	const mach_port_t		GetAsyncPort() const { return mAsyncPort ; }
	IOReturn				CreateAsyncPorts() ;
	const Boolean			AsyncPortsExist() const ;

	const CFMachPortRef		GetIsochAsyncCFPort() const { return mIsochAsyncCFPort ; }
	const mach_port_t		GetIsochAsyncPort() const { return mIsochAsyncPort ; }
	IOReturn				CreateIsochAsyncPorts() ;
	const Boolean			IsochAsyncPortsExist() const ;

	virtual IOReturn		CreateCFStringWithOSStringRef(
									FWKernOSStringRef	inStringRef,
									UInt32				inStringLen,
									CFStringRef*&		text) ;
	virtual IOReturn		CreateCFDataWithOSDataRef(
									FWKernOSDataRef		inDataRef,
									IOByteCount			inDataLen,
									CFDataRef*&			data) ;
	// --- isoch related ----------
	virtual IOReturn		AddIsochCallbackDispatcherToRunLoop(
									CFRunLoopRef		inRunLoop) ;
	virtual IOFireWireLibIsochChannelRef 
							CreateIsochChannel(
									Boolean 				doIrm, 
									UInt32 					packetSize, 
									IOFWSpeed 				prefSpeed,
									REFIID 					iid) ;
	virtual IOFireWireLibRemoteIsochPortRef
							CreateRemoteIsochPort(
									Boolean					inTalking,
									REFIID 					iid) ;
	virtual IOFireWireLibLocalIsochPortRef
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
	virtual IOFireWireLibDCLCommandPoolRef
							CreateDCLCommandPool(
									IOByteCount 			size, 
									REFIID 					iid ) ;
	virtual void			PrintDCLProgram(
									const DCLCommandStruct*		inDCL,
									UInt32						inDCLCount) ;

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
	
	io_connect_t				mUserClientConnection ;

	CFMachPortRef				mAsyncCFPort ;
	mach_port_t					mAsyncPort ;
	io_async_ref_t				mBusResetAsyncRef ;
	io_async_ref_t				mBusResetDoneAsyncRef ;
	IOFireWireBusResetHandler	mBusResetHandler ;
	IOFireWireBusResetDoneHandler mBusResetDoneHandler ;
	
	CFRunLoopRef				mRunLoop ;
	CFRunLoopSourceRef			mRunLoopSource ;

	Boolean						mIsOpen ;
	
	// isoch related
	CFMachPortRef				mIsochAsyncCFPort ;
	mach_port_t					mIsochAsyncPort ;
	CFRunLoopRef				mIsochRunLoop ;
	CFRunLoopSourceRef			mIsochRunLoopSource ;
	
} ;



// ============================================================
//
// ¥ class IOFireWireDeviceInterfaceCOM
//
// ============================================================

class IOFireWireDeviceInterfaceCOM: public IOFireWireDeviceInterfaceImp
{
 public:
								IOFireWireDeviceInterfaceCOM() ;
	virtual 					~IOFireWireDeviceInterfaceCOM() ;
	
	//
	// --- COM ----------------------------
	//
	struct InterfaceMap 
	{
        IUnknownVTbl*					pseudoVTable;
        IOFireWireDeviceInterfaceCOM*	obj;
    };

	//
	// --- interfaces ---------------------
	//
 	static IOCFPlugInInterface			sIOCFPlugInInterface ;
 	InterfaceMap						mIOCFPlugInInterface ;
 	static IOFireWireDeviceInterface	sInterface ;
	InterfaceMap						mInterface ;
 
	//
 	// --- GetThis() ----------------------
	//
 	inline static IOFireWireDeviceInterfaceCOM* GetThis(IOFireWireLibDeviceRef self)
		{ return (IOFireWireDeviceInterfaceCOM *) ((InterfaceMap *) self)->obj; };


	//
	// --- IUnknown -----------------------
	//
	static IOCFPlugInInterface** 	Alloc() ;
	virtual HRESULT					QueryInterface( REFIID iid, LPVOID* ppv );

	//
	// === STATIC METHODS ==========================						
	//
	
	//
	// --- CFPlugin static methods ---------
	//
	static IOReturn 		SProbe(IOFireWireLibDeviceRef self, CFDictionaryRef propertyTable, 
								 io_service_t service, SInt32 *order );
    static IOReturn 		SStart(IOFireWireLibDeviceRef self, CFDictionaryRef propertyTable, 
								 io_service_t service );
	static IOReturn 		SStop( IOFireWireLibDeviceRef self );	

	//
	// --- FireWire user client maintenance -----------
	//
	static const Boolean	SInterfaceIsInited(
									IOFireWireLibDeviceRef self)  ;
	static io_object_t		SGetDevice(
									IOFireWireLibDeviceRef self) ;
	static IOReturn			SOpen(
									IOFireWireLibDeviceRef self) ;
	static IOReturn			SOpenWithSessionRef(
									IOFireWireLibDeviceRef self,
									IOFireWireSessionRef   session) ;
	static void				SClose(
									IOFireWireLibDeviceRef self) ;
	
	// --- FireWire notification methods --------------
	static const Boolean	SNotificationIsOn(
									IOFireWireLibDeviceRef self) ;

	// This function adds the proper event source to the passed in CFRunLoop. Call this
	// before trying to use any IOFireWireLib notifications.
	static const IOReturn	SAddCallbackDispatcherToRunLoop(IOFireWireLibDeviceRef self, CFRunLoopRef inRunLoop) ;
	static void				SRemoveCallbackDispatcherFromRunLoop(IOFireWireLibDeviceRef self) ;

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
									IOFireWireLibDeviceRef self,
									io_object_t 		device,
									const FWAddress* addr, 
									UInt32*				val,
									Boolean				failOnReset = false,
									UInt32				generation = 0) ;
    static IOReturn 		SWrite(
									IOFireWireLibDeviceRef self,
									io_object_t 		device,
									const FWAddress* addr, 
									const void*			buf,
									UInt32*				size,
									Boolean				failOnReset = false,
									UInt32				generation = 0) ;
    static IOReturn 		SWriteQuadlet(
									IOFireWireLibDeviceRef self,
									io_object_t 		device,
									const FWAddress* addr, 
									const UInt32		val,
									Boolean				failOnReset = false,
									UInt32				generation = 0) ;
    static IOReturn 		SCompareSwap(
									IOFireWireLibDeviceRef self,
									io_object_t			device,
									const FWAddress* addr, 
									UInt32 				cmpVal,
									UInt32 				newVal,
									Boolean				failOnReset = false,
									UInt32				generation = 0) ;

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
	
	// --- other methods ----------
	static IOReturn			SBusReset(
									IOFireWireLibDeviceRef			self) ;
	static IOReturn			SGetCycleTime(
									IOFireWireLibDeviceRef			self,
									UInt32*					outCycleTime)
									{ return GetThis(self)->GetCycleTime(outCycleTime); }
	static IOReturn			SGetGenerationAndNodeID(
									IOFireWireLibDeviceRef self,
									UInt32*				outGeneration,
									UInt16*				outNodeID)
									{ return GetThis(self)->GetGenerationAndNodeID(outGeneration, outNodeID); }
	static IOReturn			SGetLocalNodeID(
									IOFireWireLibDeviceRef	self,
									UInt16*					outLocalNodeID)
									{ return GetThis(self)->GetLocalNodeID(outLocalNodeID); }
	static IOReturn			SGetResetTime(
									IOFireWireLibDeviceRef	self,
									AbsoluteTime*			outResetTime)
									{ return GetThis(self)->GetResetTime(outResetTime); }

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

	// --- debugging -------------------------------
	static IOReturn			SFireBugMsg(
									IOFireWireLibDeviceRef self,
									const char *			msg)
									{ return GetThis(self)->FireBugMsg(msg); }
	
	// --- isoch -----------------------------------
	static IOReturn			SAddIsochCallbackDispatcherToRunLoop(
									IOFireWireLibDeviceRef 	self, 
									CFRunLoopRef 			inRunLoop) ;
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
	static void				SPrintDCLProgram(
									IOFireWireLibDeviceRef 	self, 
									const DCLCommandStruct*	inDCL,
									UInt32					inDCLCount)  ;
} ;


#endif //_IOKIT_IOFireWireLibPriv_H_
