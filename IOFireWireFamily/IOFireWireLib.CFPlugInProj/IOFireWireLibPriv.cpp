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
 *  IOFireWireLibPriv.cpp
 *  IOFireWireLib
 *
 *  Created by NWG on Fri Apr 28 2000.
 *  Copyright (c) 2000-2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include <mach/message.h>
#include <CoreFoundation/CoreFoundation.h>
//#include <Carbon/Carbon.h>
#include <IOKit/IOKitLib.h>

#include "IOFireWireFamilyCommon.h"
#ifdef __cplusplus
extern "C" {
#endif

	#include <IOKit/iokitmig.h>
	
#ifdef __cplusplus
}
#endif

#include "IOFireWireLib.h"
#include "IOFireWireLibPriv.h"
#include "IOFireWireLibCommand.h"
#include "IOFireWireLibUnitDirectory.h"
#include "IOFireWireLibPsudoAddrSpace.h"
#include "IOFireWireLibPhysAddrSpace.h"
#include "IOFireWireLibConfigDirectory.h"
#include "IOFireWireLibIsochChannel.h"
#include "IOFireWireLibIsochPort.h"
#include "IOFireWireLibDCLCommandPool.h"

// ============================================================
//
// interface tables
//
// ============================================================

// Reserved0() was:
//	IOFireWireLibCommandRef	
//						(*CreateReadQuadletCommand)(
//									IOFireWireLibDeviceRef	self,
//									io_object_t			device,
//									const FWAddress *	addr,
//									UInt32				quads[],
//									UInt32				numQuads,
//									IOFireWireLibCommandCallback callback,
//									Boolean				failOnReset,
//									UInt32				generation,
//									void*				inRefCon,
//									REFIID				iid) ;

// Reserved1() was:
//	IOFireWireLibCommandRef
//						(*CreateWriteQuadletCommand)(
//									IOFireWireLibDeviceRef	self,
//									io_object_t			device,
//									const FWAddress *	addr,
//									UInt32				quads[],
//									UInt32				numQuads,
//									IOFireWireLibCommandCallback callback,
//									Boolean				failOnReset,
//									UInt32				generation,
//									void*				inRefCon,
//									REFIID				iid) ;

// static interface table for IOCFPlugInInterface
IOCFPlugInInterface IOFireWireDeviceInterfaceCOM::sIOCFPlugInInterface = 
{
	INTERFACEIMP_INTERFACE,
	1, 0, // version/revision
	& IOFireWireDeviceInterfaceCOM::SProbe,
	& IOFireWireDeviceInterfaceCOM::SStart,
	& IOFireWireDeviceInterfaceCOM::SStop
};

IOFireWireDeviceInterface IOFireWireDeviceInterfaceCOM::sInterface = 
{
	INTERFACEIMP_INTERFACE,
	1, 0, // version/revision
	& IOFireWireDeviceInterfaceCOM::SInterfaceIsInited,
	& IOFireWireDeviceInterfaceCOM::SGetDevice,
	& IOFireWireDeviceInterfaceCOM::SOpen,
	& IOFireWireDeviceInterfaceCOM::SOpenWithSessionRef,
	& IOFireWireDeviceInterfaceCOM::SClose,
	& IOFireWireDeviceInterfaceCOM::SNotificationIsOn,
	& IOFireWireDeviceInterfaceCOM::SAddCallbackDispatcherToRunLoop,
	& IOFireWireDeviceInterfaceCOM::SRemoveCallbackDispatcherFromRunLoop,
	& IOFireWireDeviceInterfaceCOM::STurnOnNotification,
	& IOFireWireDeviceInterfaceCOM::STurnOffNotification,
	& IOFireWireDeviceInterfaceCOM::SSetBusResetHandler,
	& IOFireWireDeviceInterfaceCOM::SSetBusResetDoneHandler,
	& IOFireWireDeviceInterfaceCOM::SClientCommandIsComplete,
	
	// --- FireWire send/recv methods --------------
	& IOFireWireDeviceInterfaceCOM::SRead,
	& IOFireWireDeviceInterfaceCOM::SReadQuadlet,
	& IOFireWireDeviceInterfaceCOM::SWrite,
	& IOFireWireDeviceInterfaceCOM::SWriteQuadlet,
	& IOFireWireDeviceInterfaceCOM::SCompareSwap,
	
	// --- firewire commands -----------------------
	& IOFireWireDeviceInterfaceCOM::SCreateReadCommand,
	& IOFireWireDeviceInterfaceCOM::SCreateReadQuadletCommand,
	& IOFireWireDeviceInterfaceCOM::SCreateWriteCommand,
	& IOFireWireDeviceInterfaceCOM::SCreateWriteQuadletCommand,
	& IOFireWireDeviceInterfaceCOM::SCreateCompareSwapCommand,

		// --- other methods ---------------------------
	& IOFireWireDeviceInterfaceCOM::SBusReset,
	& IOFireWireDeviceInterfaceCOM::SGetCycleTime,
	& IOFireWireDeviceInterfaceCOM::SGetGenerationAndNodeID,
	& IOFireWireDeviceInterfaceCOM::SGetLocalNodeID,
	& IOFireWireDeviceInterfaceCOM::SGetResetTime,

		// --- unit directory support ------------------
	& IOFireWireDeviceInterfaceCOM::SCreateLocalUnitDirectory,

	& IOFireWireDeviceInterfaceCOM::SGetConfigDirectory,
	& IOFireWireDeviceInterfaceCOM::SCreateConfigDirectoryWithIOObject,
		// --- address space support -------------------
	& IOFireWireDeviceInterfaceCOM::SCreatePseudoAddressSpace,
	& IOFireWireDeviceInterfaceCOM::SCreatePhysicalAddressSpace,
		
		// --- debugging -------------------------------
	& IOFireWireDeviceInterfaceCOM::SFireBugMsg,
	
		// --- isoch -----------------------------------
	& IOFireWireDeviceInterfaceCOM::SAddIsochCallbackDispatcherToRunLoop,
	& IOFireWireDeviceInterfaceCOM::SCreateRemoteIsochPort,
	& IOFireWireDeviceInterfaceCOM::SCreateLocalIsochPort,
	& IOFireWireDeviceInterfaceCOM::SCreateIsochChannel,
	& IOFireWireDeviceInterfaceCOM::SCreateDCLCommandPool,

		// --- refcon ----------------------------------
	& IOFireWireDeviceInterfaceCOM::SGetRefCon,
	& IOFireWireDeviceInterfaceCOM::SSetRefCon,

		// --- debugging -------------------------------
	// do not use this function
//	& IOFireWireDeviceInterfaceCOM::SGetDebugProperty,
	nil,

	& IOFireWireDeviceInterfaceCOM::SPrintDCLProgram
} ;

// ============================================================
// IOFireWireIUnknown methods
// ============================================================

// static
HRESULT
IOFireWireIUnknown::SQueryInterface(void* self, REFIID iid, void** ppv)
{
	return GetThis(self)->QueryInterface(iid, ppv) ;
}

UInt32
IOFireWireIUnknown::SAddRef(void* self)
{
	return GetThis(self)->AddRef() ;
}

ULONG
IOFireWireIUnknown::SRelease(void* self)
{
	return GetThis(self)->Release() ;
}

ULONG
IOFireWireIUnknown::AddRef()
{
	return ++mRefCount ;
}

ULONG
IOFireWireIUnknown::Release()
{
	UInt32 newCount = mRefCount;
	
	if (mRefCount == 1)
	{
		mRefCount = 0 ;
		delete this ;
	}
	else
		mRefCount-- ;
	
	return newCount ;
}

#pragma mark -
#pragma mark --IOFireWireDeviceInterfaceImp

IOReturn
IOFireWireDeviceInterfaceImp::Start(CFDictionaryRef propertyTable, io_service_t service )
{
	IOReturn	kr = kIOReturnSuccess ;

	if ( !service )
		kr = kIOReturnBadArgument ;
	
	if ( kIOReturnSuccess == kr )
	{
		mDefaultDevice = service ;

		if ( kIOReturnSuccess == OpenDefaultConnection() )
			mIsInited = true ;
		else
			kr = kIOReturnBadArgument ;
	}
	
	return kr ;
}

IOReturn
IOFireWireDeviceInterfaceImp::Stop()
{
	mIsInited = false ;
	IOReturn	result = kIOReturnSuccess ;
	
	if (mUserClientConnection)
	{
		result = IOServiceClose(mUserClientConnection) ;
		mUserClientConnection = 0 ;
	}
	
	return result ;//IOServiceClose(mDefaultDevice) ;
}

IOReturn
IOFireWireDeviceInterfaceImp::Probe(CFDictionaryRef propertyTable, io_service_t service, SInt32 *order)
{
	// only load against firewire nubs
    if( !service || !IOObjectConformsTo(service, "IOFireWireNub") )
        return kIOReturnBadArgument;
	
	return kIOReturnSuccess;
}

#pragma mark --ctor/dtor
// 
// constructor/destructor
//

IOFireWireDeviceInterfaceImp::IOFireWireDeviceInterfaceImp(): IOFireWireIUnknown()
{
	// mr. safety says, "initialize for safety!"
	mUserClientConnection		= nil ;
	mIsInited					= false ;
	mIsOpen						= false ;
	mNotifyIsOn					= false ;
	mAsyncPort					= 0 ;
	mAsyncCFPort				= 0 ;
	mBusResetAsyncRef[0] 		= 0 ;
	mBusResetDoneAsyncRef[0]	= 0 ;
	mBusResetHandler 			= 0 ;
	mBusResetDoneHandler 		= 0 ;

	mRunLoop					= 0 ;
	mRunLoopSource				= 0 ;

	mPseudoAddressSpaces 		= CFSetCreateMutable(kCFAllocatorDefault, 0, nil) ;
	mUnitDirectories			= CFSetCreateMutable(kCFAllocatorDefault, 0, nil) ;
	mPhysicalAddressSpaces		= CFSetCreateMutable(kCFAllocatorDefault, 0, nil) ;
	mIOFireWireLibCommands		= CFSetCreateMutable(kCFAllocatorDefault, 0, nil) ;
	mConfigDirectories			= CFSetCreateMutable(kCFAllocatorDefault, 0, nil) ;
	
	//
	// isoch related
	//
	mIsochAsyncPort				= 0 ;
	mIsochAsyncCFPort			= 0 ;

}

IOFireWireDeviceInterfaceImp::~IOFireWireDeviceInterfaceImp()
{
	if (mIsOpen)
		Close() ;

	if (mRunLoopSource)
	{
//		RemoveCallbackDispatcherFromRunLoop() ;
		AddCallbackDispatcherToRunLoop( 0 ) ;
		CFRelease(mRunLoopSource) ;
		mRunLoopSource = 0 ;
	}
	
	if ( mIsochRunLoopSource )
	{
		AddIsochCallbackDispatcherToRunLoop( 0 ) ;
		CFRelease( mIsochRunLoopSource ) ;
		mIsochRunLoopSource = 0 ;
	}

	if (mRunLoop)
	{
		CFRelease(mRunLoop) ;
		mRunLoop = 0 ;
	}

	if (mIsochRunLoopSource)
	{
//		RemoveCallbackDispatcherFromRunLoop() ;
		RemoveDispatcherFromRunLoop( mIsochRunLoop, mIsochRunLoopSource, kCFRunLoopDefaultMode ) ;
		CFRelease(mIsochRunLoopSource) ;
		mIsochRunLoopSource = 0 ;
	}

	if (mIsochRunLoop)
	{
		CFRelease(mIsochRunLoop) ;
		mIsochRunLoop = 0 ;
	}

	if (mPseudoAddressSpaces)
	{
		CFRelease(mPseudoAddressSpaces) ;
		mPseudoAddressSpaces = 0 ;
	}
		
	if (mPhysicalAddressSpaces)
	{
		CFRelease(mPhysicalAddressSpaces) ;
		mPhysicalAddressSpaces = 0 ;
	}

	if (mUnitDirectories)
	{
		CFRelease(mUnitDirectories) ;
		mUnitDirectories = 0 ;
	}
		
	if (mIOFireWireLibCommands)
	{
		CFRelease(mIOFireWireLibCommands) ;
		mIOFireWireLibCommands = 0 ;
	}
		
	if (mConfigDirectories)
	{
		CFRelease(mConfigDirectories) ;
		mConfigDirectories = 0 ;
	}

	if (mUserClientConnection)
	{
		IOServiceClose(mUserClientConnection) ;
		mUserClientConnection = 0 ;
	}
	
}

#pragma mark -- static methods

#pragma mark -
#pragma mark --VIRTUAL METHODS
#pragma mark --iokit miscellany

const io_connect_t
IOFireWireDeviceInterfaceImp::OpenDefaultConnection()
{
	io_connect_t	connection	= 0 ;
	IOReturn		kr			= kIOReturnSuccess ;
	
	if ( 0 == mDefaultDevice )
		kr = kIOReturnNoDevice ;
	
	if (kIOReturnSuccess == kr )
		kr = IOServiceOpen(mDefaultDevice, mach_task_self(), kIOFireWireLibConnection, & connection) ;

	if (kIOReturnSuccess == kr )
		mUserClientConnection = connection ;
	
	return kr ;
}

IOReturn
IOFireWireDeviceInterfaceImp::CreateAsyncPorts()
{
	IOReturn result = kIOReturnSuccess ;

	if (! mAsyncPort)
	{
		IOCreateReceivePort(kOSAsyncCompleteMessageID, & mAsyncPort) ;
		
		Boolean shouldFreeInfo ;
		CFMachPortContext cfPortContext	= {1, this, NULL, NULL, NULL} ;
		mAsyncCFPort = CFMachPortCreateWithPort(
							kCFAllocatorDefault,
							mAsyncPort,
							(CFMachPortCallBack) IODispatchCalloutFromMessage,
							& cfPortContext,
							& shouldFreeInfo) ;
		
		if (!mAsyncCFPort)
			result = kIOReturnNoMemory ;
	}
	
	return result ;
}

IOReturn
IOFireWireDeviceInterfaceImp::CreateIsochAsyncPorts()
{
	IOReturn result = kIOReturnSuccess ;

	if (! mIsochAsyncPort)
	{
		IOCreateReceivePort(kOSAsyncCompleteMessageID, & mIsochAsyncPort) ;
		
		Boolean shouldFreeInfo ;
		CFMachPortContext cfPortContext	= {1, this, NULL, NULL, NULL} ;
		mIsochAsyncCFPort = CFMachPortCreateWithPort( kCFAllocatorDefault,
													  mIsochAsyncPort,
													  (CFMachPortCallBack) IODispatchCalloutFromMessage,
													  & cfPortContext,
													  & shouldFreeInfo) ;
		if (!mIsochAsyncCFPort)
			result = kIOReturnNoMemory ;		
	}
	
	return result ;
}

const Boolean
IOFireWireDeviceInterfaceImp::AsyncPortsExist() const
{ 
	return ((mAsyncCFPort != 0) && (mAsyncPort != 0)); 
}

const Boolean
IOFireWireDeviceInterfaceImp::IsochAsyncPortsExist() const
{
	return ((mIsochAsyncCFPort != 0) && (mIsochAsyncPort != 0)); 
}

IOReturn
IOFireWireDeviceInterfaceImp::Open()
{
	IOReturn result = IOConnectMethodScalarIScalarO(
		GetUserClientConnection(),
		kFireWireOpen,
		0,
		0) ;
		
	mIsOpen = (kIOReturnSuccess == result) ;

	return result ;
}

IOReturn
IOFireWireDeviceInterfaceImp::OpenWithSessionRef(IOFireWireSessionRef session)
{
	IOReturn	result = kIOReturnSuccess ;

	if (mIsOpen)
		result = kIOReturnExclusiveAccess ;
	else
	{
		result = IOConnectMethodScalarIScalarO(
			GetUserClientConnection(),
			kFireWireOpenWithSessionRef,
			1,
			0,
			session) ;
		
		mIsOpen = (kIOReturnSuccess == result) ;
	}
	
	return result ;
}

void
IOFireWireDeviceInterfaceImp::Close()
{
	IOReturn result = kIOReturnSuccess ;
	
	if (!mIsOpen)
		result  = kIOReturnNotOpen ;
	else
	{
		result = IOConnectMethodScalarIScalarO(
			GetUserClientConnection(),
			kFireWireClose,
			0,
			0) ;
			
		IOFireWireLibLogIfErr_(result, ("IOFireWireDeviceInterfaceImp::Close(): error %08lX returned from Close()!\n", (UInt32) result ) ) ;

		mIsOpen = false ;
	}
}

#pragma mark --notification
// --- FireWire notification methods --------------
const IOReturn
IOFireWireDeviceInterfaceImp::AddCallbackDispatcherToRunLoop(
	CFRunLoopRef			inRunLoop)
{
	// if the client passes 0 as the runloop, that means
	// we should remove the source instead of adding it.

	if ( !inRunLoop )
	{
		RemoveDispatcherFromRunLoop( mRunLoop, mRunLoopSource, kCFRunLoopDefaultMode ) ;
		return kIOReturnSuccess ;
	}

	IOReturn result = kIOReturnSuccess ;
	
	if (!AsyncPortsExist())
		result = CreateAsyncPorts() ;

	if ( kIOReturnSuccess == result )
	{
		CFRetain(inRunLoop) ;
		
		mRunLoop = inRunLoop ;
		mRunLoopSource	= CFMachPortCreateRunLoopSource(
								kCFAllocatorDefault,
								GetAsyncCFPort(),
								0) ;

		if (!mRunLoopSource)
			result = kIOReturnNoMemory ;
		
		if ((kIOReturnSuccess == result) && mRunLoop && mRunLoopSource)
			CFRunLoopAddSource(mRunLoop, mRunLoopSource, kCFRunLoopDefaultMode) ;
	}
	
	return result ;
}

void
IOFireWireDeviceInterfaceImp::RemoveDispatcherFromRunLoop(
	CFRunLoopRef			runLoop,
	CFRunLoopSourceRef		runLoopSource,
	CFStringRef				mode)
{
	if ( runLoop && runLoopSource )
		if (CFRunLoopContainsSource( runLoop, runLoopSource, mode ))
			CFRunLoopRemoveSource( runLoop, runLoopSource, mode );
}

const Boolean
IOFireWireDeviceInterfaceImp::TurnOnNotification(
//	CFRunLoopRef 			inRunLoop,
	void*					callBackRefCon)
{
	IOReturn				result					= kIOReturnSuccess ;
	io_scalar_inband_t		params ;
	mach_msg_type_number_t	size = 0 ;
	
	if (!mUserClientConnection)
		result = kIOReturnNoDevice ;

	if (!AsyncPortsExist())
		result = kIOReturnError ;	// zzz  need a new error type meaning "you forgot to call AddDispatcherToRunLoop"
//		result = CreateAsyncPorts() ;

/*	if ( kIOReturnSuccess == result )
	{
		CFRunLoopSourceRef	runLoopSource	= CFMachPortCreateRunLoopSource(
													kCFAllocatorDefault,
													GetAsyncCFPort(),
													0) ;

		if (!runLoopSource)
			result = kIOReturnNoMemory ;
		
		if ((kIOReturnSuccess == result) && inRunLoop && runLoopSource)
		{
			CFRunLoopAddSource(inRunLoop, runLoopSource, kCFRunLoopDefaultMode) ;
			CFRelease(runLoopSource) ;
		}														
	} */

	if ( kIOReturnSuccess == result )
	{
		params[0]	= (UInt32)(IOAsyncCallback) & IOFireWireDeviceInterfaceImp::BusResetHandler ;
		params[1]	= (UInt32) callBackRefCon; //(UInt32) this ;
	
		result = io_async_method_scalarI_scalarO(
				mUserClientConnection,
				mAsyncPort,
				mBusResetAsyncRef,
				1,
				kFWSetAsyncRef_BusReset,
				params,
				2,
				params,
				& size) ;
		
	}

	if ( kIOReturnSuccess == result )
	{
		params[0]	= (UInt32)(IOAsyncCallback) & IOFireWireDeviceInterfaceImp::BusResetDoneHandler ;
		params[1]	= (UInt32) callBackRefCon; //(UInt32) this ;
		size = 0 ;
	
		result = io_async_method_scalarI_scalarO(
				mUserClientConnection,
				mAsyncPort,
				mBusResetDoneAsyncRef,
				1,
				kFWSetAsyncRef_BusResetDone,
				params,
				2,
				params,
				& size) ;
		
	}
	
	if ( kIOReturnSuccess == result )
		mNotifyIsOn = true ;
		
	return ( kIOReturnSuccess == result ) ;
}

void
IOFireWireDeviceInterfaceImp::TurnOffNotification()
{
	IOReturn				result			= kIOReturnSuccess ;
	io_scalar_inband_t		params ;
	mach_msg_type_number_t	size 		= 0 ;
	
	// if notification isn't on, skip out.
	if (!mNotifyIsOn)
		return ;

	if (!mUserClientConnection)
		result = kIOReturnNoDevice ;
	
	if ( kIOReturnSuccess == result )
	{
		params[0]	= (UInt32)(IOAsyncCallback) 0 ;
		params[1]	= 0 ;
	
		result = io_async_method_scalarI_scalarO(
				mUserClientConnection,
				mAsyncPort,
				mBusResetAsyncRef,
				1,
				kFWSetAsyncRef_BusReset,
				params,
				2,
				params,
				& size) ;
		
		params[0]	= (UInt32)(IOAsyncCallback) 0 ;
		params[1]	= 0 ;
		size = 0 ;
	
		result = io_async_method_scalarI_scalarO(
				mUserClientConnection,
				mAsyncPort,
				mBusResetDoneAsyncRef,
				1,
				kFWSetAsyncRef_BusResetDone,
				params,
				2,
				params,
				& size) ;
		
	}
	
	mNotifyIsOn = false ;
}


const IOFireWireBusResetHandler
IOFireWireDeviceInterfaceImp::SetBusResetHandler(
	IOFireWireBusResetHandler			inBusResetHandler)
{
	IOFireWireBusResetHandler	result = mBusResetHandler ;
	mBusResetHandler = inBusResetHandler ;
	
	return result ;
}

const IOFireWireBusResetDoneHandler
IOFireWireDeviceInterfaceImp::SetBusResetDoneHandler(
	IOFireWireBusResetDoneHandler		inBusResetDoneHandler)
{
	IOFireWireBusResetDoneHandler	result = mBusResetDoneHandler ;
	mBusResetDoneHandler = inBusResetDoneHandler ;
	
	return result ;
}

void
IOFireWireDeviceInterfaceImp::BusResetHandler(
	void*							refCon,
	IOReturn						result)
{
	IOFireWireDeviceInterfaceImp*	me = IOFireWireDeviceInterfaceCOM::GetThis((IOFireWireLibDeviceRef)refCon) ;

	if (me->mBusResetHandler)
		(me->mBusResetHandler)( (IOFireWireLibDeviceRef)refCon, (FWClientCommandID) me) ;
}

void
IOFireWireDeviceInterfaceImp::BusResetDoneHandler(
	void*							refCon,
	IOReturn						result)
{
	IOFireWireDeviceInterfaceImp*	me = IOFireWireDeviceInterfaceCOM::GetThis((IOFireWireLibDeviceRef) refCon) ;

	if (me->mBusResetDoneHandler)
		(me->mBusResetDoneHandler)( (IOFireWireLibDeviceRef)refCon, (FWClientCommandID) me) ;
}

void
IOFireWireDeviceInterfaceImp::ClientCommandIsComplete(
	FWClientCommandID				commandID,
	IOReturn						status)
{
}

#pragma mark --firewire read/write/lock

IOReturn
IOFireWireDeviceInterfaceImp::Read(
	io_object_t				device,
	const FWAddress &		addr,
	void*					buf,
	UInt32*					size,
	Boolean					failOnReset,
	UInt32					generation)
{
	IOReturn	kr = kIOReturnSuccess ;

	if (!mIsOpen || !mUserClientConnection)
		kr = kIOReturnNotOpen ;

	if (kIOReturnSuccess == kr)
	{
		mach_msg_type_number_t	outputParamSize = sizeof(*size) ;
		FWReadWriteParams	params = {addr, buf, *size, failOnReset, generation} ;

		if (device == mDefaultDevice)
		{
			kr = io_connect_method_structureI_structureO(
				mUserClientConnection,
				kFireWireRead,
				(char*) & params,
				sizeof(params),
				(char*) size,
				& outputParamSize) ;
		}
		else if (device == 0)
		{
			kr = io_connect_method_structureI_structureO(
				mUserClientConnection,
				kFireWireReadAbsolute,
				(char*) & params,
				sizeof(params),
				(char*) size,
				& outputParamSize) ;
		}
		else
			kr = kIOReturnBadArgument ;
	}

	return kr ;
}

IOReturn
IOFireWireDeviceInterfaceImp::ReadQuadlet(
	io_object_t				device,
	const FWAddress &		addr,
	UInt32*					val,
	Boolean					failOnReset,
	UInt32					generation)
{
	IOReturn				kr = kIOReturnSuccess ;
	
	if (!mIsOpen || !mUserClientConnection)
		kr = kIOReturnNotOpen ;
		
	if (kIOReturnSuccess == kr)
	{
		io_scalar_inband_t		params ;
		mach_msg_type_number_t	size = 1 ;
				
		params[0] = *((UInt32*) & addr) ;
		params[1] = addr.addressLo ;
		params[2] = failOnReset ;
		params[3] = generation ;

		if (device == mDefaultDevice)
		{
			kr = io_connect_method_scalarI_scalarO(
				mUserClientConnection,
				kFireWireReadQuad,
				params,
				4,
				(int*) val,
				& size) ;
				
		}
		else if (device == 0)
		{
			kr = io_connect_method_scalarI_scalarO(
				mUserClientConnection,
				kFireWireReadQuadAbsolute,
				params,
				4,
				(int*) val,
				& size) ;
		}
		else
			kr = kIOReturnBadArgument ;
	}
		
	return kr ;
}

IOReturn
IOFireWireDeviceInterfaceImp::Write(
	io_object_t				device,
	const FWAddress &		addr,
	const void*				buf,
	UInt32* 				size,
	Boolean					failOnReset,
	UInt32					generation)
{
	IOReturn	kr = kIOReturnSuccess ;
	
	if (!mIsOpen || !mUserClientConnection)
		kr = kIOReturnExclusiveAccess ;

	if (kIOReturnSuccess == kr)
	{
		FWReadWriteParams params = {addr, buf, *size, failOnReset, generation} ;
		IOByteCount bytes ;
		IOByteCount size = sizeof(IOByteCount) ;

		if (device == mDefaultDevice)
		{
			kr = IOConnectMethodStructureIStructureO(
				mUserClientConnection,
				kFireWireWrite,
				sizeof(params),
				& size,
				& params,
				& bytes) ;
		}
		else if (device == 0)
		{
			kr = IOConnectMethodStructureIStructureO(
				mUserClientConnection,
				kFireWireWriteAbsolute,
				sizeof(params),
				& size,
				& params,
				& bytes) ;
		}
		else
			kr = kIOReturnBadArgument ;
	}
		
	return kr ;
}

IOReturn
IOFireWireDeviceInterfaceImp::WriteQuadlet(
	io_object_t				device,
	const FWAddress &		addr,
	const UInt32			val,
	Boolean 				failOnReset,
	UInt32					generation)
{
	IOReturn				kr = kIOReturnSuccess ;
	
	if (!mIsOpen || !mUserClientConnection)
		kr = kIOReturnNotOpen ;
	
	if (kIOReturnSuccess == kr)
	{
		io_scalar_inband_t		params ;
		mach_msg_type_number_t	size = 0 ;
					
		if ( mUserClientConnection )
		{
			params[0] = *((UInt32*) & addr) ;
			params[1] = addr.addressLo ;
			params[2] = val ;
			params[3] = failOnReset ;
			params[4] = generation ;
	
			if (device == mDefaultDevice)
			{
				kr = io_connect_method_scalarI_scalarO(
					mUserClientConnection,
					kFireWireWriteQuad,
					params,
					5,
					(int*) val,
					& size) ;
					
			}
			else if (device == 0)
			{
				kr = io_connect_method_scalarI_scalarO(
					mUserClientConnection,
					kFireWireWriteQuadAbsolute,
					params,
					5,
					(int*) val,
					& size) ;
			}
			else
				kr = kIOReturnBadArgument ;
		}
		else
			kr = kIOReturnNotOpen ;
	}
	
	return kr ;
}


IOReturn
IOFireWireDeviceInterfaceImp::CompareSwap(
	io_object_t				device,
	const FWAddress &		addr,
	UInt32 					cmpVal,
	UInt32 					newVal,
	Boolean 				failOnReset,
	UInt32					generation)
{
	IOReturn	kr = kIOReturnSuccess ;

	if (!mIsOpen || !mUserClientConnection)
		kr = kIOReturnNotOpen ;

	if (kIOReturnSuccess == kr)
	{
		io_scalar_inband_t		params ;

		params[0] = *((UInt32*) & addr) ;
		params[1] = addr.addressLo ;
		params[2] = cmpVal ;
		params[3] = newVal ;
		params[4] = failOnReset ;
		params[5] = generation ;
		
		mach_msg_type_number_t size = 0 ;
			
		if (device == mDefaultDevice)
		{
			kr = io_connect_method_scalarI_scalarO(
				mUserClientConnection,
				kFireWireCompareSwap,
				params,
				6,
				params,
				& size) ;
		}
		else if (device == 0)
		{
			kr = io_connect_method_scalarI_scalarO(
				mUserClientConnection,
				kFireWireCompareSwapAbsolute,
				params,
				6,
				params,
				& size) ;
		}
		else
			kr = kIOReturnBadArgument ;
	}
		
	return kr ;
}

#pragma mark --FireWire command object methods
// --- FireWire command object methods ---------

IOFireWireLibCommandRef	
IOFireWireDeviceInterfaceImp::CreateReadCommand(
	io_object_t 		device, 
	const FWAddress&	addr, 
	void* 				buf, 
	UInt32 				size, 
	IOFireWireLibCommandCallback callback,
	Boolean 			failOnReset, 
	UInt32 				generation,
	void*				inRefCon,
	REFIID				iid)
{
	IOFireWireLibCommandRef	result = 0 ;
	
	IUnknownVTbl** iUnknown = IOFireWireLibReadCommandImp::Alloc(*this, device, addr, buf, size, callback, failOnReset, generation, inRefCon) ;
	if (iUnknown)
	{
		(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
		(*iUnknown)->Release(iUnknown) ;
	}

	return result ;
}

IOFireWireLibCommandRef	
IOFireWireDeviceInterfaceImp::CreateReadQuadletCommand(
	io_object_t 		device, 
	const FWAddress & 	addr, 
	UInt32	 			quads[], 
	UInt32				numQuads,
	IOFireWireLibCommandCallback callback,
	Boolean 			failOnReset, 
	UInt32				generation,
	void*				inRefCon,
	REFIID				iid)
{
	IOFireWireLibCommandRef	result = 0 ;
	
	IUnknownVTbl** iUnknown = IOFireWireLibReadQuadletCommandImp::Alloc(*this, device, addr, quads, numQuads, callback, failOnReset, generation, inRefCon) ;
	if (iUnknown)
	{
		(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
		(*iUnknown)->Release(iUnknown) ;
	}
	
	return result ;
}

IOFireWireLibCommandRef
IOFireWireDeviceInterfaceImp::CreateWriteCommand(
	io_object_t 		device, 
	const FWAddress & 	addr, 
	void*		 		buf, 
	UInt32 				size, 
	IOFireWireLibCommandCallback callback,
	Boolean 			failOnReset, 
	UInt32 				generation,
	void*				inRefCon,
	REFIID				iid)
{
	IOFireWireLibCommandRef	result = 0 ;
	
	IUnknownVTbl** iUnknown = IOFireWireLibWriteCommandImp::Alloc(*this, device, addr, buf, size, callback, failOnReset, generation, inRefCon) ;
	if (iUnknown)
	{
		(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
		(*iUnknown)->Release(iUnknown) ;
	}
	
	return result ;
	
}

IOFireWireLibCommandRef
IOFireWireDeviceInterfaceImp::CreateWriteQuadletCommand(
	io_object_t 		device, 
	const FWAddress & 	addr, 
	UInt32		 		quads[], 
	UInt32				numQuads,
	IOFireWireLibCommandCallback callback,
	Boolean 			failOnReset, 
	UInt32 				generation,
	void*				inRefCon,
	REFIID				iid)
{
	IOFireWireLibCommandRef	result = 0 ;
	
	IUnknownVTbl** iUnknown = IOFireWireLibWriteQuadletCommandImp::Alloc(*this, device, addr, quads, numQuads, callback, failOnReset, generation, inRefCon) ;
	if (iUnknown)
	{
		(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
		(*iUnknown)->Release(iUnknown) ;
	}
	
	return result ;
}

IOFireWireLibCommandRef
IOFireWireDeviceInterfaceImp::CreateCompareSwapCommand(
	io_object_t 		device, 
	const FWAddress & 	addr, 
	UInt32 				cmpVal, 
	UInt32				newVal, 
	IOFireWireLibCommandCallback callback,
	Boolean 			failOnReset, 
	UInt32 				generation,
	void*				inRefCon,
	REFIID				iid)
{
	IOFireWireLibCommandRef	result = 0 ;
	
	IUnknownVTbl** iUnknown = IOFireWireLibCompareSwapCommandImp::Alloc(*this, device, addr, cmpVal, newVal, callback, failOnReset, generation, inRefCon) ;
	if (iUnknown)
	{
		(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
		(*iUnknown)->Release(iUnknown) ;
	}
	
	return result ;
}
 
#pragma mark --other bus actions

IOReturn
IOFireWireDeviceInterfaceImp::BusReset()
{
	IOReturn result = kIOReturnSuccess ;
	
	if (!mIsOpen || !mUserClientConnection)
		result = kIOReturnNotOpen ;

	if (kIOReturnSuccess == result)
	{
		mach_msg_type_number_t size = 0 ;
		io_scalar_inband_t params ;
	
		result = io_connect_method_scalarI_scalarO(
						mUserClientConnection,
						kFireWireBusReset,
						params,
						0,
						params,
						& size) ;
	}
	
	return result ;
}

IOReturn
IOFireWireDeviceInterfaceImp::GetCycleTime(
	UInt32*		outCycleTime)
{
	mach_msg_type_number_t	size = 1 ;
	io_scalar_inband_t	params ;
	
	return io_connect_method_scalarI_scalarO(
				mUserClientConnection,
				kFireWireCycleTime,
				params,
				0,
				(io_scalar_inband_t) outCycleTime,
				& size) ;
}

IOReturn
IOFireWireDeviceInterfaceImp::GetGenerationAndNodeID(
	UInt32*		outGeneration,
	UInt16*		outNodeID)
{
	io_scalar_inband_t params ;
	mach_msg_type_number_t	size = 2 ;
	
	IOReturn result = io_connect_method_scalarI_scalarO(
							mUserClientConnection,
							kFireWireGetGenerationAndNodeID,
							params,
							0,
							params,
							& size) ;
	
	*outGeneration = params[0] ;
	*outNodeID = params[1] >> 16 ;

	return result ;
}

IOReturn
IOFireWireDeviceInterfaceImp::GetLocalNodeID(
	UInt16*		outLocalNodeID)
{
	io_scalar_inband_t params ;
	mach_msg_type_number_t	size = 1 ;

	return io_connect_method_scalarI_scalarO(
				mUserClientConnection,
				kFireWireGetLocalNodeID,
				params,
				0,
				(int*) outLocalNodeID,
				& size) ;
	
}

IOReturn
IOFireWireDeviceInterfaceImp::GetResetTime(
	AbsoluteTime*			resetTime)
{
	// zzz this code works if AbsoluteTime is a UInt64 (should be)
	
	mach_msg_type_number_t	size = sizeof(*resetTime) ;
	io_scalar_inband_t		params ;

	return io_connect_method_scalarI_scalarO(
				mUserClientConnection,
				kFireWireGetResetTime,
				params,
				0,
				(io_scalar_inband_t) resetTime,
				& size) ;
}

#pragma mark --iofirewiredevice and friends

IOFireWireLibLocalUnitDirectoryRef
IOFireWireDeviceInterfaceImp::CreateLocalUnitDirectory(
	REFIID					iid)
{
	IOFireWireLibLocalUnitDirectoryRef	result = 0 ;

	if (mIsOpen && mUserClientConnection)
	{
			// we allocate a user space pseudo address space with the reference we
			// got from the kernel
		IUnknownVTbl**	iUnknown = IOFireWireLocalUnitDirectoryImp::Alloc(*this) ;
					
			// we got a new iUnknown from the object. Query it for the interface
			// requested in iid...
		(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
					
			// we got the interface we wanted (or at least another ref to iUnknown),
			// so we call iUnknown.Release()
		(*iUnknown)->Release(iUnknown) ;
	}
		
	return result ;
}

IOFireWireLibConfigDirectoryRef
IOFireWireDeviceInterfaceImp::GetConfigDirectory(
	REFIID				iid)
{
	IOFireWireLibConfigDirectoryRef	result = 0 ;
	
	if (mIsOpen && mUserClientConnection)
	{
			// we allocate a user space config directory space with the reference we
			// got from the kernel
		IUnknownVTbl**	iUnknown	= IOFireWireLibConfigDirectoryCOM::Alloc(*this) ;

			// we got a new iUnknown from the object. Query it for the interface
			// requested in iid...
		if (iUnknown)
		{
			(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
		
			// we got the interface we wanted (or at least another ref to iUnknown),
			// so we call iUnknown.Release()
			(*iUnknown)->Release(iUnknown) ;
		}
	}

	return result ;
}

IOFireWireLibConfigDirectoryRef
IOFireWireDeviceInterfaceImp::CreateConfigDirectoryWithIOObject(
	io_object_t			inObject,
	REFIID				iid)
{
	IOFireWireLibConfigDirectoryRef	result = 0 ;
	
	if (mIsOpen && mUserClientConnection)
	{
			// we allocate a user space pseudo address space with the reference we
			// got from the kernel
		IUnknownVTbl**	iUnknown	= IOFireWireLibConfigDirectoryCOM::Alloc(*this, (FWKernConfigDirectoryRef)inObject) ;

			// we got a new iUnknown from the object. Query it for the interface
			// requested in iid...
		(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
		
			// we got the interface we wanted (or at least another ref to iUnknown),
			// so we call iUnknown.Release()
		(*iUnknown)->Release(iUnknown) ;
	}

	return result ;
}

IOFireWireLibPseudoAddressSpaceRef
IOFireWireDeviceInterfaceImp::CreatePseudoAddressSpace(
	UInt32						inSize,
	void*						inRefCon,
	UInt32						inQueueBufferSize,
	void*						inBackingStore,
	UInt32						inFlags,
	REFIID						iid)	// flags unused
{
	IOFireWireLibPseudoAddressSpaceRef				result = 0 ;

	if (!mUserClientConnection || !mIsOpen)
	{
		IOFireWireLibLog_(("IOFireWireDeviceInterfaceImp::CreatePseudoAddressSpace: no connection or device is not open\n")) ;
	}
	else
	{
		void*	queueBuffer = nil ;
		if ( inQueueBufferSize > 0 )
			queueBuffer	= new Byte[inQueueBufferSize] ;

		FWAddrSpaceCreateParams	params ;
		params.size 			= inSize ;
		params.queueBuffer 		= (Byte*) queueBuffer ;
		params.queueSize		= (UInt32) inQueueBufferSize ;
		params.backingStore 	= (Byte*) inBackingStore ;
		params.refCon			= (UInt32) this ;
		params.flags			= inFlags ;
		
		FWKernAddrSpaceRef	addrSpaceRef ;
		mach_msg_type_number_t	size	= sizeof(addrSpaceRef) ;
		
		// We call the routine which creates a pseudo address 
		// space in the kernel.
		if( kIOReturnSuccess == io_connect_method_structureI_structureO(
										mUserClientConnection,
										kFWPseudoAddrSpace_Allocate,
										(char*) & params,
										sizeof(params),
										(char*) & addrSpaceRef,
										& size) )
		{
			if (addrSpaceRef)
			{
				// we allocate a user space pseudo address space with the reference we
				// got from the kernel
				IUnknownVTbl**	iUnknown = IOFireWirePseudoAddressSpaceImp::Alloc(*this, addrSpaceRef, queueBuffer, inQueueBufferSize, inBackingStore, inRefCon) ;
				
				// we got a new iUnknown from the object. Query it for the interface
				// requested in iid...
				(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
				
				// we got the interface we wanted (or at least another ref to iUnknown),
				// so we call iUnknown.Release()
				(*iUnknown)->Release(iUnknown) ;
				
			}
		}
	}
	
	return result ;
}

IOFireWireLibPhysicalAddressSpaceRef
IOFireWireDeviceInterfaceImp::CreatePhysicalAddressSpace(
	UInt32						inSize,
	void*						inBackingStore,
	UInt32						inFlags,
	REFIID						iid)	// flags unused
{
	IOFireWireLibPhysicalAddressSpaceRef	result = 0 ;
	
	if ( mUserClientConnection && mIsOpen )
	{
		FWPhysicalAddrSpaceCreateParams	params ;
		params.size				= inSize ;
		params.backingStore		= inBackingStore ;
		params.flags			= inFlags ;
		
		FWKernPhysicalAddrSpaceRef	output ;
		IOByteCount	size = sizeof(output) ;
		
		if (kIOReturnSuccess  == IOConnectMethodStructureIStructureO(
							mUserClientConnection,
							kFWPhysicalAddrSpace_Allocate,
							sizeof(params),
							& size,
							(void*) & params,
							(void*) & output ))
		{
			if (output)
			{
				// we allocate a user space pseudo address space with the reference we
				// got from the kernel
				IUnknownVTbl**	iUnknown = IOFireWirePhysicalAddressSpaceImp::Alloc(*this, output, inSize, inBackingStore, inFlags) ;
				
				// we got a new iUnknown from the object. Query it for the interface
				// requested in iid...
				if (iUnknown)
				{
					(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
					
					// we got the interface we wanted (or at least another ref to iUnknown),
					// so we call iUnknown.Release()
					(*iUnknown)->Release(iUnknown) ;
				}
			}
		}			

	}
	
	return result ;
}

#pragma mark --debugging

IOReturn
IOFireWireDeviceInterfaceImp::FireBugMsg(
	const char *	inMsg)
{
	IOReturn result = kIOReturnSuccess ;
	UInt32 size = strlen(inMsg) + 1 ;
	
	if (!mIsOpen || !mUserClientConnection)
		result = kIOReturnNotOpen ;

	if (kIOReturnSuccess == result)
		result = Write(0, FWAddress(0, 0, 0x4242), inMsg, & size) ;

	return result ;
}

IOReturn
IOFireWireDeviceInterfaceImp::CreateCFStringWithOSStringRef(
	FWKernOSStringRef	inStringRef,
	UInt32				inStringLen,
	CFStringRef*&		text)
{
	char*				textBuffer = new char[inStringLen] ;
	io_connect_t		connection = GetUserClientConnection() ;
	UInt32				stringSize ;
	
	if (!textBuffer)
		return kIOReturnNoMemory ;
	
	IOReturn result = IOConnectMethodScalarIScalarO(
							connection,
							kFWGetOSStringData,
							3,
							1,
							inStringRef,
							inStringLen,
							textBuffer,
							& stringSize) ;

	if (text && (kIOReturnSuccess == result))
		*text = CFStringCreateWithCString(kCFAllocatorDefault, textBuffer, kCFStringEncodingASCII) ;
	
	delete textBuffer ;

	return result ;
}

IOReturn
IOFireWireDeviceInterfaceImp::CreateCFDataWithOSDataRef(
	FWKernOSDataRef		inDataRef,
	IOByteCount			inDataSize,
	CFDataRef*&			data)
{
	UInt8*			buffer = new UInt8[inDataSize] ;
	IOByteCount		dataSize ;
	
	if (!buffer)
		return kIOReturnNoMemory ;
		
	if (!mUserClientConnection)
		return kIOReturnError ;
	
	IOReturn result = IOConnectMethodScalarIScalarO(
							mUserClientConnection,
							kFWGetOSDataData,
							2,
							2,
							inDataRef,
							inDataSize,
							buffer,
							& dataSize) ;

	if (data && (kIOReturnSuccess == result))
		*data = CFDataCreate(kCFAllocatorDefault, buffer, inDataSize) ;

	if (!data)
		result = kIOReturnNoMemory ;
		
	delete buffer ;
	
	return result ;
}

// 
// --- isoch related
//
IOReturn
IOFireWireDeviceInterfaceImp::AddIsochCallbackDispatcherToRunLoop(
	CFRunLoopRef		inRunLoop)
{
	IOReturn result = kIOReturnSuccess ;
	
	if ( !inRunLoop )
	{
		RemoveDispatcherFromRunLoop( mIsochRunLoop, mIsochRunLoopSource, kCFRunLoopDefaultMode ) ;
		return result ;
	}

	if (!IsochAsyncPortsExist())
		result = CreateIsochAsyncPorts() ;

	if ( kIOReturnSuccess == result )
	{
		CFRetain(inRunLoop) ;
		
		mIsochRunLoop 			= inRunLoop ;
		mIsochRunLoopSource		= CFMachPortCreateRunLoopSource( kCFAllocatorDefault,
																 GetIsochAsyncCFPort() ,
																 0) ;

		if (!mIsochRunLoopSource)
			result = kIOReturnNoMemory ;
		
		if ((kIOReturnSuccess == result) && mIsochRunLoop && mIsochRunLoopSource)
			CFRunLoopAddSource(mIsochRunLoop, mIsochRunLoopSource, kCFRunLoopDefaultMode) ;
	}
	
	return result ;
}

IOFireWireLibIsochChannelRef 
IOFireWireDeviceInterfaceImp::CreateIsochChannel(
	Boolean 				inDoIRM, 
	UInt32 					inPacketSize, 
	IOFWSpeed 				inPrefSpeed,
	REFIID 					iid)
{
	IOFireWireLibIsochChannelRef	result = 0 ;
	
	IUnknownVTbl** iUnknown = IOFireWireLibIsochChannelCOM::Alloc(*this, inDoIRM, inPacketSize, inPrefSpeed) ;
	if (iUnknown)
	{
		(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
		(*iUnknown)->Release(iUnknown) ;
	}
	
	return result ;
}

IOFireWireLibRemoteIsochPortRef
IOFireWireDeviceInterfaceImp::CreateRemoteIsochPort(
	Boolean					inTalking,
	REFIID 					iid)
{
	IOFireWireLibRemoteIsochPortRef		result = 0 ;
	
	IUnknownVTbl** iUnknown = IOFireWireLibRemoteIsochPortCOM::Alloc(*this, inTalking) ;
	if (iUnknown)
	{
		(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
		(*iUnknown)->Release(iUnknown) ;
	}
	
	return result ;
}

IOFireWireLibLocalIsochPortRef
IOFireWireDeviceInterfaceImp::CreateLocalIsochPort(
	Boolean					inTalking,
	DCLCommandStruct*		inDCLProgram,
	UInt32					inStartEvent,
	UInt32					inStartState,
	UInt32					inStartMask,
	IOVirtualRange			inDCLProgramRanges[],			// optional optimization parameters
	UInt32					inDCLProgramRangeCount,
	IOVirtualRange			inBufferRanges[],
	UInt32					inBufferRangeCount,
	REFIID 					iid)
{
	IOFireWireLibLocalIsochPortRef	result = 0 ;
	
	IUnknownVTbl** iUnknown = IOFireWireLibLocalIsochPortCOM::Alloc(*this, inTalking, inDCLProgram, inStartEvent,
																	inStartState, inStartMask, inDCLProgramRanges,
																	inDCLProgramRangeCount, inBufferRanges,
																	inBufferRangeCount) ;
	if (iUnknown)
	{
		(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
		(*iUnknown)->Release(iUnknown) ;
	}
	return result ;
}

IOFireWireLibDCLCommandPoolRef
IOFireWireDeviceInterfaceImp::CreateDCLCommandPool(
	IOByteCount 			inSize, 
	REFIID 					iid )
{
	IOFireWireLibDCLCommandPoolRef	result = 0 ;
	
	IUnknownVTbl** iUnknown = IOFireWireLibDCLCommandPoolCOM::Alloc(*this, inSize) ;
	(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
	(*iUnknown)->Release(iUnknown) ;
	
	return result ;
}

void
IOFireWireDeviceInterfaceImp::PrintDCLProgram(
	const DCLCommandStruct*		inDCL,
	UInt32						inDCLCount) 
{
	const DCLCommandStruct*		currentDCL	= inDCL ;
	UInt32						index		= 0 ;
	
	fprintf(stderr, "IOFireWireLibIsochPortImp::printDCLProgram: inDCL=0x%08lX, inDCLCount=%ud\n", (UInt32) inDCL, inDCLCount) ;
	
	while ( (index < inDCLCount) && currentDCL )
	{
		fprintf(stderr, "\n#0x%04lX  @0x%08lX   next=0x%08lX, cmplrData=0x%08lX, op=%u ", 
			  index, 
			  (UInt32) currentDCL,
			  (UInt32) currentDCL->pNextDCLCommand,
			  currentDCL->compilerData,
			  currentDCL->opcode) ;

		switch(currentDCL->opcode & ~kFWDCLOpFlagMask)
		{
			case kDCLSendPacketStartOp:
				//		 |
				//		\|/
				//		 V
			case kDCLSendPacketWithHeaderStartOp:
				//		 |
				//		\|/
				//		 V
			case kDCLSendPacketOp:
				//		 |
				//		\|/
				//		 V
			case kDCLReceivePacketStartOp:
				//		 |
				//		\|/
				//		 V
			case kDCLReceivePacketOp:
				fprintf(stderr, "(DCLTransferPacketStruct) buffer=%08lX, size=%u",
					  (UInt32) ((DCLTransferPacketStruct*)currentDCL)->buffer,
					  ((DCLTransferPacketStruct*)currentDCL)->size) ;
				break ;
				
			case kDCLSendBufferOp:
			case kDCLReceiveBufferOp:
				fprintf(stderr, "(DCLTransferBufferStruct) buffer=%08lX, size=%u, packetSize=%08lX, bufferOffset=%08lX",
					  ((DCLTransferBufferStruct*)currentDCL)->buffer,
					  ((DCLTransferBufferStruct*)currentDCL)->size,
					  ((DCLTransferBufferStruct*)currentDCL)->packetSize,
					  ((DCLTransferBufferStruct*)currentDCL)->bufferOffset) ;
				break ;
	
			case kDCLCallProcOp:
				fprintf(stderr, "(DCLCallProcStruct) proc=%08lX, procData=%08lX",
					  ((DCLCallProcStruct*)currentDCL)->proc,
					  ((DCLCallProcStruct*)currentDCL)->procData) ;
				break ;
				
			case kDCLLabelOp:
				fprintf(stderr, "(DCLLabelStruct)") ;
				break ;
				
			case kDCLJumpOp:
				fprintf(stderr, "(DCLJumpStruct) pJumpDCLLabel=%08lX",
					  ((DCLJumpStruct*)currentDCL)->pJumpDCLLabel) ;
				break ;
				
			case kDCLSetTagSyncBitsOp:
				fprintf(stderr, "(DCLSetTagSyncBitsStruct) tagBits=%04lX, syncBits=%04lX",
					  ((DCLSetTagSyncBitsStruct*)currentDCL)->tagBits,
					  ((DCLSetTagSyncBitsStruct*)currentDCL)->syncBits) ;
				break ;
				
			case kDCLUpdateDCLListOp:
				fprintf(stderr, "(DCLUpdateDCLListStruct) dclCommandList=%08lX, numDCLCommands=%ud \n",
					  ((DCLUpdateDCLListStruct*)currentDCL)->dclCommandList,
					  ((DCLUpdateDCLListStruct*)currentDCL)->numDCLCommands) ;
				
				for(UInt32 listIndex=0; listIndex < ((DCLUpdateDCLListStruct*)currentDCL)->numDCLCommands; listIndex++)
				{
					fprintf(stderr, "%08lX ", (((DCLUpdateDCLListStruct*)currentDCL)->dclCommandList)[listIndex]) ;
				}
				
				break ;
	
			case kDCLPtrTimeStampOp:
				fprintf(stderr, "(DCLPtrTimeStampStruct) timeStampPtr=%08lX",
					  ((DCLPtrTimeStampStruct*)currentDCL)->timeStampPtr) ;
		}
		
		currentDCL = currentDCL->pNextDCLCommand ;
		index++ ;
	}
	
	fprintf(stderr, "\n") ;

	if (index != inDCLCount)
		fprintf(stderr, "unexpected end of program\n") ;
	
	if (currentDCL != NULL)
		fprintf(stderr, "program too long for count\n") ;
}

#pragma mark -
#pragma mark --- IUnknown ----------

//### ============================================================
//###
//### ¥ IOFireWireDeviceInterfaceCOM
//###
//### ============================================================

// ============================================================
// factory function implementor
// ============================================================

void*
IOFireWireLibFactory(
	CFAllocatorRef				allocator,
	CFUUIDRef					typeID)
{
	void* result	= nil;

	if (CFEqual(typeID, kIOFireWireLibTypeID))
		result	= (void*) IOFireWireDeviceInterfaceCOM::Alloc() ;

	return (void*) result ;
}

// ============================================================
// constructor/destructor
// ============================================================
IOFireWireDeviceInterfaceCOM::IOFireWireDeviceInterfaceCOM(): IOFireWireDeviceInterfaceImp()
{
	// COM bits
	mIOCFPlugInInterface.pseudoVTable = (IUnknownVTbl*) & sIOCFPlugInInterface ;
	mIOCFPlugInInterface.obj = this ;
	
	mInterface.pseudoVTable = (IUnknownVTbl*) & sInterface ;
	mInterface.obj = this ;

	// factory counting
	CFPlugInAddInstanceForFactory( kIOFireWireLibFactoryID );
}

IOFireWireDeviceInterfaceCOM::~IOFireWireDeviceInterfaceCOM()
{
	// cleaning up COM bits
	CFPlugInRemoveInstanceForFactory( kIOFireWireLibFactoryID );
}

// ============================================================
// static allocator
// ============================================================

IOCFPlugInInterface** 
IOFireWireDeviceInterfaceCOM::Alloc()
{
    IOFireWireDeviceInterfaceCOM*	me;
	IOCFPlugInInterface ** 	interface = NULL;
	
    me = new IOFireWireDeviceInterfaceCOM ;
    if( me )
	{
		// we return an interface here. queryInterface will not be called. call addRef here
		me->AddRef();
        interface = (IOCFPlugInInterface **) &me->mIOCFPlugInInterface.pseudoVTable;
    }
	
	return interface;
}

// ============================================================
// QueryInterface (includes IOCFPlugIn support)
// ============================================================

HRESULT STDMETHODCALLTYPE
IOFireWireDeviceInterfaceCOM::QueryInterface(REFIID iid, LPVOID* ppv)
{
	HRESULT		result = S_OK ;
	*ppv = nil ;

	CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;

	if ( CFEqual(interfaceID, IUnknownUUID) ||
		 CFEqual(interfaceID, kIOCFPlugInInterfaceID) )
	{
		*ppv = & mIOCFPlugInInterface ;
		AddRef() ;
	}
	else if ( CFEqual(interfaceID, kIOFireWireDeviceInterfaceID) || 
			  CFEqual(interfaceID, kIOFireWireDeviceInterfaceID_v2) ||
			  CFEqual(interfaceID, kIOFireWireNubInterfaceID) ||
			  CFEqual(interfaceID, kIOFireWireUnitInterfaceID) )
	{
		*ppv = & mInterface ;
		AddRef() ;
	}
	else
	{
		*ppv = nil ;
		result = E_NOINTERFACE ;
	}	
	
	CFRelease(interfaceID) ;
	
	return result ;
}

//
// IOCFPlugIn support
//

IOReturn
IOFireWireDeviceInterfaceCOM::SProbe(IOFireWireLibDeviceRef self, CFDictionaryRef propertyTable, io_service_t service, SInt32 *order )
{
	return GetThis(self)->Probe(propertyTable, service, order) ;
}

IOReturn
IOFireWireDeviceInterfaceCOM::SStart(IOFireWireLibDeviceRef self, CFDictionaryRef propertyTable, io_service_t service)
{
	return GetThis(self)->Start(propertyTable, service) ;
}

IOReturn
IOFireWireDeviceInterfaceCOM::SStop(IOFireWireLibDeviceRef self)
{
	return GetThis(self)->Stop() ;
}

// ============================================================
//
// static methods
//
// ============================================================

// --- FireWire user client maintenance -----------
const Boolean
IOFireWireDeviceInterfaceCOM::SInterfaceIsInited(IOFireWireLibDeviceRef self) 
{ 
	return GetThis(self)->mIsInited;
}

io_object_t
IOFireWireDeviceInterfaceCOM::SGetDevice(IOFireWireLibDeviceRef self)
{ 
	return GetThis(self)->mDefaultDevice; 
}

IOReturn
IOFireWireDeviceInterfaceCOM::SOpen(IOFireWireLibDeviceRef self) 
{
	return GetThis(self)->Open() ;
}

IOReturn
IOFireWireDeviceInterfaceCOM::SOpenWithSessionRef(IOFireWireLibDeviceRef self, IOFireWireSessionRef session) 
{
	return GetThis(self)->OpenWithSessionRef(session) ;
}

void
IOFireWireDeviceInterfaceCOM::SClose(IOFireWireLibDeviceRef self)
{
	return GetThis(self)->Close() ;
}
	
// --- FireWire notification methods --------------
const Boolean
IOFireWireDeviceInterfaceCOM::SNotificationIsOn(IOFireWireLibDeviceRef self)
{
	return GetThis(self)->mNotifyIsOn; 
}

const IOReturn
IOFireWireDeviceInterfaceCOM::SAddCallbackDispatcherToRunLoop(IOFireWireLibDeviceRef self, CFRunLoopRef inRunLoop)
{
	return GetThis(self)->AddCallbackDispatcherToRunLoop(inRunLoop) ;
}

void
IOFireWireDeviceInterfaceCOM::SRemoveCallbackDispatcherFromRunLoop(IOFireWireLibDeviceRef self)
{
//	GetThis(self)->RemoveCallbackDispatcherFromRunLoop() ;
	IOFireWireDeviceInterfaceImp*	me = GetThis( self ) ;
	me->RemoveDispatcherFromRunLoop( me->GetRunLoop(), me->GetRunLoopSource(), kCFRunLoopDefaultMode ) ;
}

// Makes notification active. Returns false if notification could not be activated.
const Boolean
IOFireWireDeviceInterfaceCOM::STurnOnNotification(IOFireWireLibDeviceRef self)
{
	return GetThis(self)->TurnOnNotification(self) ;
}

// Notification callbacks will no longer be called.
void
IOFireWireDeviceInterfaceCOM::STurnOffNotification(IOFireWireLibDeviceRef self)
{
	GetThis(self)->TurnOffNotification() ;
}

const IOFireWireBusResetHandler
IOFireWireDeviceInterfaceCOM::SSetBusResetHandler(IOFireWireLibDeviceRef self, IOFireWireBusResetHandler inBusResetHandler)
{
	return GetThis(self)->SetBusResetHandler(inBusResetHandler) ;
}

const IOFireWireBusResetDoneHandler
IOFireWireDeviceInterfaceCOM::SSetBusResetDoneHandler(IOFireWireLibDeviceRef self, IOFireWireBusResetDoneHandler inBusResetDoneHandler) 
{
	return GetThis(self)->SetBusResetDoneHandler(inBusResetDoneHandler) ;
}

void
IOFireWireDeviceInterfaceCOM::SClientCommandIsComplete(IOFireWireLibDeviceRef self, FWClientCommandID commandID, IOReturn status)
{ 
	GetThis(self)->ClientCommandIsComplete(commandID, status); 
}

IOReturn
IOFireWireDeviceInterfaceCOM::SRead(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress * addr, void* buf, 
	UInt32* size, Boolean failOnReset, UInt32 generation)
{ 
	return GetThis(self)->Read(device, *addr, buf, size, failOnReset, generation) ; 
}

IOReturn
IOFireWireDeviceInterfaceCOM::SReadQuadlet(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress * addr,
	UInt32* val, Boolean failOnReset, UInt32 generation)
{ 
	return GetThis(self)->ReadQuadlet(device, *addr, val, failOnReset, generation); 
}

IOReturn
IOFireWireDeviceInterfaceCOM::SWrite(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress * addr, const void* buf, UInt32* size,
	Boolean failOnReset, UInt32 generation)
{ 
	return GetThis(self)->Write(device, *addr, buf, size, failOnReset, generation) ;
}

IOReturn
IOFireWireDeviceInterfaceCOM::SWriteQuadlet(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress* addr, const UInt32 val, 
	Boolean failOnReset, UInt32 generation)
{ 
	return GetThis(self)->WriteQuadlet(device, *addr, val, failOnReset, generation) ;
}

IOReturn
IOFireWireDeviceInterfaceCOM::SCompareSwap(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress* addr, UInt32 cmpVal, UInt32 newVal, Boolean failOnReset, UInt32 generation)
{
	return GetThis(self)->CompareSwap(device, *addr, cmpVal, newVal, failOnReset, generation) ;
}

#pragma mark --FireWire command object methods
IOFireWireLibCommandRef	
IOFireWireDeviceInterfaceCOM::SCreateReadCommand(IOFireWireLibDeviceRef self,
	io_object_t 		device, 
	const FWAddress*	addr, 
	void* 				buf, 
	UInt32 				size, 
	IOFireWireLibCommandCallback callback,
	Boolean 			failOnReset, 
	UInt32 				generation,
	void*				inRefCon,
	REFIID				iid)
{
	return GetThis(self)->CreateReadCommand(device, addr ? *addr : FWAddress(), buf, size, callback, failOnReset, generation, inRefCon, iid) ;
}

IOFireWireLibCommandRef	
IOFireWireDeviceInterfaceCOM::SCreateReadQuadletCommand(IOFireWireLibDeviceRef self, io_object_t device, 
	const FWAddress* addr, UInt32 val[], const UInt32 numQuads, IOFireWireLibCommandCallback callback, 
	Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid)
{
	return GetThis(self)->CreateReadQuadletCommand(device, addr ? *addr : FWAddress(), val, numQuads, callback, failOnReset, generation, inRefCon, iid) ;
}

IOFireWireLibCommandRef
IOFireWireDeviceInterfaceCOM::SCreateWriteCommand(IOFireWireLibDeviceRef self, io_object_t device, 
	const FWAddress* addr, void* buf, UInt32 size, IOFireWireLibCommandCallback callback, 
	Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid)
{
	return GetThis(self)->CreateWriteCommand( device, addr ? *addr : FWAddress(), buf, size, callback, failOnReset, generation, inRefCon, iid) ;
}

IOFireWireLibCommandRef
IOFireWireDeviceInterfaceCOM::SCreateWriteQuadletCommand(IOFireWireLibDeviceRef self, io_object_t device, 
	const FWAddress* addr, UInt32 quads[], const UInt32 numQuads, IOFireWireLibCommandCallback callback,
	Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid)
{
	return GetThis(self)->CreateWriteQuadletCommand(device, addr ? *addr : FWAddress(), quads, numQuads, callback, failOnReset, generation, inRefCon, iid) ;
}

IOFireWireLibCommandRef
IOFireWireDeviceInterfaceCOM::SCreateCompareSwapCommand(IOFireWireLibDeviceRef self,
	io_object_t 		device, 
	const FWAddress* 	addr, 
	UInt32 				cmpVal, 
	UInt32				newVal, 
	IOFireWireLibCommandCallback callback,
	Boolean 			failOnReset, 
	UInt32 				generation,
	void*				inRefCon,
	REFIID				iid)
{
	return GetThis(self)->CreateCompareSwapCommand(device, addr ? *addr : FWAddress(), cmpVal, newVal, callback, failOnReset, generation, inRefCon, iid) ;
}

IOReturn
IOFireWireDeviceInterfaceCOM::SBusReset(IOFireWireLibDeviceRef self)
{
	return GetThis(self)->BusReset() ;
}

#pragma mark --local unit directory support

//
// --- local unit directory support ------------------
//
IOFireWireLibLocalUnitDirectoryRef
IOFireWireDeviceInterfaceCOM::SCreateLocalUnitDirectory(IOFireWireLibDeviceRef self, REFIID iid)
{ 
	return GetThis(self)->CreateLocalUnitDirectory(iid); 
}

IOFireWireLibConfigDirectoryRef
IOFireWireDeviceInterfaceCOM::SGetConfigDirectory(IOFireWireLibDeviceRef self, REFIID iid)
{
	return GetThis(self)->GetConfigDirectory(iid) ;
}

IOFireWireLibConfigDirectoryRef
IOFireWireDeviceInterfaceCOM::SCreateConfigDirectoryWithIOObject(IOFireWireLibDeviceRef self, io_object_t inObject, REFIID iid)
{
	return GetThis(self)->CreateConfigDirectoryWithIOObject(inObject, iid) ;
}

#pragma mark --address space support
//
// --- address space support ------------------
//
IOFireWireLibPseudoAddressSpaceRef
IOFireWireDeviceInterfaceCOM::SCreatePseudoAddressSpace(IOFireWireLibDeviceRef self, UInt32 inLength, void* inRefCon, UInt32 inQueueBufferSize, 
	void* inBackingStore, UInt32 inFlags, REFIID iid)
{ 
	return GetThis(self)->CreatePseudoAddressSpace(inLength, inRefCon, inQueueBufferSize, inBackingStore, inFlags, iid); 
}

IOFireWireLibPhysicalAddressSpaceRef
IOFireWireDeviceInterfaceCOM::SCreatePhysicalAddressSpace(IOFireWireLibDeviceRef self, UInt32 inLength, void* inBackingStore, UInt32 flags, REFIID iid)
{ 
	return GetThis(self)->CreatePhysicalAddressSpace(inLength, inBackingStore, flags, iid); 
}

//
// --- isoch -----------------------------------
//
IOReturn
IOFireWireDeviceInterfaceCOM::SAddIsochCallbackDispatcherToRunLoop(
	IOFireWireLibDeviceRef	self,
	CFRunLoopRef			inRunLoop)
{
	return GetThis(self)->AddIsochCallbackDispatcherToRunLoop(inRunLoop) ;
}

IOFireWireLibRemoteIsochPortRef
IOFireWireDeviceInterfaceCOM::SCreateRemoteIsochPort(
	IOFireWireLibDeviceRef 	self,
	Boolean					inTalking,
	REFIID					iid)
{
	return GetThis(self)->CreateRemoteIsochPort(inTalking, iid) ;
}

IOFireWireLibLocalIsochPortRef
IOFireWireDeviceInterfaceCOM::SCreateLocalIsochPort(
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
	REFIID 					iid)
{
	return GetThis(self)->CreateLocalIsochPort(inTalking, inDCLProgram, inStartEvent, inStartState, inStartMask,
											   inDCLProgramRanges, inDCLProgramRangeCount, inBufferRanges, 
											   inBufferRangeCount, iid) ;
}

IOFireWireLibIsochChannelRef
IOFireWireDeviceInterfaceCOM::SCreateIsochChannel(
	IOFireWireLibDeviceRef 	self, 
	Boolean 				inDoIRM, 
	UInt32 					inPacketSize, 
	IOFWSpeed 				inPrefSpeed,
//	IOFireWireIsochChannelForceStopHandler stopProc, 
//	void* 					stopProcRefCon, 
	REFIID 					iid)
{
	return GetThis(self)->CreateIsochChannel(inDoIRM, inPacketSize, inPrefSpeed, iid) ;
}

IOFireWireLibDCLCommandPoolRef
IOFireWireDeviceInterfaceCOM::SCreateDCLCommandPool(
	IOFireWireLibDeviceRef 	self, 
	IOByteCount 			size, 
	REFIID 					iid )
{
	return GetThis(self)->CreateDCLCommandPool(size, iid) ;
}

void
IOFireWireDeviceInterfaceCOM::SPrintDCLProgram(
	IOFireWireLibDeviceRef 		self, 
	const DCLCommandStruct*		inDCL,
	UInt32						inDCLCount) 
{
	GetThis(self)->PrintDCLProgram(inDCL, inDCLCount) ;
}

