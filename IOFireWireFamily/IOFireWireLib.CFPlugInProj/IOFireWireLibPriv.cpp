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
 *  Created on Fri Apr 28 2000.
 *  Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 */
/*
	$Log: IOFireWireLibPriv.cpp,v $
	Revision 1.37  2002/10/11 23:12:22  collin
	fix broken headerdoc, fix compiler warnings
	
	Revision 1.36  2002/09/25 00:27:34  niels
	flip your world upside-down
	
	Revision 1.35  2002/09/12 22:41:56  niels
	add GetIRMNodeID() to user client
	
*/

#import "IOFireWireLibPriv.h"
#import "IOFireWireLibCommand.h"
#import "IOFireWireLibUnitDirectory.h"
#import "IOFireWireLibConfigDirectory.h"
#import "IOFireWireLibPhysicalAddressSpace.h"
#import "IOFireWireLibPseudoAddressSpace.h"
#import "IOFireWireLibIsochChannel.h"
#import "IOFireWireLibIsochPort.h"
#import "IOFireWireLibDCLCommandPool.h"

#import <mach/mach.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/iokitmig.h>
#import <exception>

// ============================================================
// factory function implementor
// ============================================================

void*
IOFireWireLibFactory( CFAllocatorRef allocator, CFUUIDRef typeID )
{
	void* result	= nil;

	if ( CFEqual( typeID, kIOFireWireLibTypeID ) )
		result	= (void*) IOFireWireLib::IOCFPlugIn::Alloc() ;

	return (void*) result ;
}

namespace IOFireWireLib {
	
	// ============================================================
	//
	// interface tables
	//
	// ============================================================

#pragma mark -
	// static interface table for IOCFPlugInInterface
	IOCFPlugInInterface IOCFPlugIn::sInterface = 
	{
		INTERFACEIMP_INTERFACE,
		1, 0, // version/revision
		& IOCFPlugIn::SProbe,
		& IOCFPlugIn::SStart,
		& IOCFPlugIn::SStop
	};
	
	IOCFPlugIn::IOCFPlugIn()
	: IOFireWireIUnknown(reinterpret_cast<IUnknownVTbl*>( & sInterface ) ),
	  mDevice(0)
	{
		// factory counting
		::CFPlugInAddInstanceForFactory( kIOFireWireLibFactoryID );
	}

	IOCFPlugIn::~IOCFPlugIn()
	{
		if (mDevice)
			(**mDevice).Release(mDevice) ;		

		// cleaning up COM bits
		::CFPlugInRemoveInstanceForFactory( kIOFireWireLibFactoryID );
	}
	
	IOReturn
	IOCFPlugIn::Probe( CFDictionaryRef propertyTable, io_service_t service, SInt32 *order )
	{	
		// only load against firewire nubs
		if( !service || !IOObjectConformsTo(service, "IOFireWireNub") )
			return kIOReturnBadArgument;
		
		return kIOReturnSuccess;
	}
	
	IOReturn
	IOCFPlugIn::Start( CFDictionaryRef propertyTable, io_service_t service )
	{
		mDevice = DeviceCOM::Alloc( propertyTable, service ) ;
		if (!mDevice)
			return kIOReturnError ;

		return kIOReturnSuccess ;
	}
	
	IOReturn
	IOCFPlugIn::Stop()
	{
		return kIOReturnSuccess ;
	}
	
	HRESULT
	IOCFPlugIn::QueryInterface( REFIID iid, LPVOID* ppv )
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if ( CFEqual(interfaceID, IUnknownUUID) ||
			CFEqual(interfaceID, kIOCFPlugInInterfaceID) )
		{
			*ppv = & GetInterface() ;
			AddRef() ;
			::CFRelease(interfaceID) ;
		}
		else 
			// we don't have one of these... let's ask the device interface...
			result = (**mDevice).QueryInterface( mDevice, iid, ppv) ;

		
		return result ;
	}

	IOCFPlugInInterface**
	IOCFPlugIn::Alloc()
	{
		IOCFPlugIn*		me = new IOCFPlugIn ;
		if( !me )
			return nil ;

		return reinterpret_cast<IOCFPlugInInterface **>( & me->GetInterface() );
	}

	IOReturn
	IOCFPlugIn::SProbe(void* self, CFDictionaryRef propertyTable, io_service_t service, SInt32 *order )
	{
		return IOFireWireIUnknown::InterfaceMap<IOCFPlugIn>::GetThis(self)->Probe(propertyTable, service, order) ;
	}
	
	IOReturn
	IOCFPlugIn::SStart(void* self, CFDictionaryRef propertyTable, io_service_t service)
	{
		return IOFireWireIUnknown::InterfaceMap<IOCFPlugIn>::GetThis(self)->Start(propertyTable, service) ;
	}
	
	IOReturn
	IOCFPlugIn::SStop(void* self)
	{
		return IOFireWireIUnknown::InterfaceMap<IOCFPlugIn>::GetThis(self)->Stop() ;
	}

#pragma mark -
	IOFireWireIUnknown::IOFireWireIUnknown( IUnknownVTbl* interface )
	: mInterface( interface, this ),
	  mRefCount(1) 
	{
	}

	// static
	HRESULT
	IOFireWireIUnknown::SQueryInterface(void* self, REFIID iid, void** ppv)
	{
		return IOFireWireIUnknown::InterfaceMap<IOFireWireIUnknown>::GetThis(self)->QueryInterface(iid, ppv) ;
	}
	
	UInt32
	IOFireWireIUnknown::SAddRef(void* self)
	{
		return IOFireWireIUnknown::InterfaceMap<IOFireWireIUnknown>::GetThis(self)->AddRef() ;
	}
	
	ULONG
	IOFireWireIUnknown::SRelease(void* self)
	{
		return IOFireWireIUnknown::InterfaceMap<IOFireWireIUnknown>::GetThis(self)->Release() ;
	}
	
	ULONG
	IOFireWireIUnknown::AddRef()
	{
		return ++mRefCount ;
	}
	
	ULONG
	IOFireWireIUnknown::Release()
	{
		assert( mRefCount > 0) ;
	
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
	Device::Device( IUnknownVTbl* interface, CFDictionaryRef /*propertyTable*/, io_service_t service )
	: IOFireWireIUnknown( interface )
	{	
		if ( !service )
			throw std::exception() ;

		// mr. safety says, "initialize for safety!"
		mConnection					= nil ;
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
		mRunLoopMode				= 0 ;
	
		mIsochRunLoop				= 0 ;
		mIsochRunLoopSource			= 0 ;
		mIsochRunLoopMode			= 0 ;
	
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

		mDefaultDevice = service ;

		if ( kIOReturnSuccess != OpenDefaultConnection() )
			throw std::exception() ;
		
		// factory counting
		::CFPlugInAddInstanceForFactory( kIOFireWireLibFactoryID );

		mIsInited = true ;
	}

	Device::~Device()
	{
		if (mIsOpen)
			Close() ;
	
		if (mRunLoopSource)
		{
			RemoveDispatcherFromRunLoop( mRunLoop, mRunLoopSource, mRunLoopMode ) ;
			
			CFRelease( mRunLoopSource ) ;
			CFRelease( mRunLoop ) ;
			CFRelease( mRunLoopMode ) ;		
		}
		
		if ( mIsochRunLoopSource )
		{
			RemoveDispatcherFromRunLoop( mIsochRunLoop, mIsochRunLoopSource, mIsochRunLoopMode ) ;
	
			CFRelease( mIsochRunLoopSource ) ;
			CFRelease( mIsochRunLoop ) ;
			CFRelease( mIsochRunLoopMode ) ;		
		}
	
		// club ports to death
		if ( mIsochAsyncCFPort )
		{
			CFMachPortInvalidate( mIsochAsyncCFPort ) ;
			CFRelease( mIsochAsyncCFPort ) ;
			mach_port_destroy( mach_task_self(), mIsochAsyncPort ) ;
		}
		
		// club ports to death
		if ( mAsyncCFPort )
		{
			CFMachPortInvalidate( mAsyncCFPort ) ;
			CFRelease( mAsyncCFPort ) ;
			mach_port_destroy( mach_task_self(), mAsyncPort ) ;
		}
	
		if (mPseudoAddressSpaces)
			CFRelease(mPseudoAddressSpaces) ;
			
		if (mPhysicalAddressSpaces)
			CFRelease(mPhysicalAddressSpaces) ;
	
		if (mUnitDirectories)
			CFRelease(mUnitDirectories) ;
			
		if (mIOFireWireLibCommands)
			CFRelease(mIOFireWireLibCommands) ;
			
		if (mConfigDirectories)
			CFRelease(mConfigDirectories) ;
	
		if ( mConnection )
			IOServiceClose( mConnection ) ;

		if (mIsInited)
		{
			// factory counting
			::CFPlugInRemoveInstanceForFactory( kIOFireWireLibFactoryID );
		}
	}
	
	HRESULT STDMETHODCALLTYPE
	Device::QueryInterface(REFIID iid, LPVOID* ppv)
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if (	CFEqual(interfaceID, kIOFireWireDeviceInterfaceID )
	
				// v2 interfaces...
				||	CFEqual(interfaceID, kIOFireWireDeviceInterfaceID_v2)
				||	CFEqual(interfaceID, kIOFireWireNubInterfaceID)
				||	CFEqual(interfaceID, kIOFireWireUnitInterfaceID)
			
				// v3 interfaces...
				||	CFEqual( interfaceID, kIOFireWireNubInterfaceID_v3 )
				||	CFEqual( interfaceID, kIOFireWireDeviceInterfaceID_v3 )
				||	CFEqual( interfaceID, kIOFireWireUnitInterfaceID_v3 )

				// v4 interfaces...
				||	CFEqual( interfaceID, kIOFireWireNubInterfaceID_v4 )
				||	CFEqual( interfaceID, kIOFireWireDeviceInterfaceID_v4 )
				||	CFEqual( interfaceID, kIOFireWireUnitInterfaceID_v4 )

				// v5 interfaces...
				|| CFEqual( interfaceID, kIOFireWireNubInterfaceID_v5 )
				|| CFEqual( interfaceID, kIOFireWireDeviceInterfaceID_v5 )
				|| CFEqual( interfaceID, kIOFireWireUnitInterfaceID_v5 )
			)
		{
			*ppv = & GetInterface() ;
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

#pragma mark -
	IOReturn
	Device::OpenDefaultConnection()
	{
		io_connect_t	connection	= 0 ;
		IOReturn		kr			= kIOReturnSuccess ;
		
		if ( 0 == mDefaultDevice )
			kr = kIOReturnNoDevice ;
		
		if (kIOReturnSuccess == kr )
			kr = IOServiceOpen(mDefaultDevice, mach_task_self(), kIOFireWireLibConnection, & connection) ;
	
		if (kIOReturnSuccess == kr )
			mConnection = connection ;
		
		return kr ;
	}
	
	IOReturn
	Device::CreateAsyncPorts()
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
	Device::CreateIsochAsyncPorts()
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
	
	IOReturn
	Device::Open()
	{
		IOReturn result = ::IOConnectMethodScalarIScalarO( GetUserClientConnection(), kOpen, 0, 0 ) ;
			
		mIsOpen = (kIOReturnSuccess == result) ;
	
		return result ;
	}
	
	IOReturn
	Device::OpenWithSessionRef(IOFireWireSessionRef session)
	{
		IOReturn	result = kIOReturnSuccess ;
	
		if (mIsOpen)
			result = kIOReturnExclusiveAccess ;
		else
		{
			result = ::IOConnectMethodScalarIScalarO( GetUserClientConnection(), kOpenWithSessionRef, 1, 0, session ) ;
			
			mIsOpen = (kIOReturnSuccess == result) ;
		}
		
		return result ;
	}
	
	IOReturn
	Device::Seize( IOOptionBits flags )	// v3
	{
		if ( mIsOpen )
		{
			IOFireWireLibLog_("Device::Seize: Can't call while interface is open\n") ;
			return kIOReturnError ;
		}
		
		if ( nil == mConnection )
			return kIOReturnNoDevice ;
	
		IOReturn err = ::IOConnectMethodScalarIScalarO( GetUserClientConnection(), kSeize, 1, 0, flags ) ;
		IOFireWireLibLogIfErr_(err, "Could not seize service! err=%x\n", err) ;
		
		return err ;
	}
	
	void
	Device::Close()
	{	
		if (!mIsOpen)
		{
			IOFireWireLibLog_("Device::Close: interface not open\n") ;
			return ;
		}
	
		IOReturn err = kIOReturnSuccess;
		err = IOConnectMethodScalarIScalarO( GetUserClientConnection(), kClose, 0, 0 ) ;
			
		IOFireWireLibLogIfErr_( err, "Device::Close(): error %x returned from Close()!\n", err ) ;
	
		mIsOpen = false ;
	}
	
	#pragma mark -
	IOReturn
	Device::AddCallbackDispatcherToRunLoopForMode(	// v3+
		CFRunLoopRef 				inRunLoop,
		CFStringRef					inRunLoopMode )
	{
		// if the client passes 0 as the runloop, that means
		// we should remove the source instead of adding it.
	
		if ( !inRunLoop || !inRunLoopMode )
		{
			IOFireWireLibLog_("IOFireWireLibDeviceInterfaceImp::AddCallbackDispatcherToRunLoopForMode: inRunLoop == 0 || inRunLoopMode == 0\n") ;
			return kIOReturnBadArgument ;
		}
	
		IOReturn result = kIOReturnSuccess ;
		
		if (!AsyncPortsExist())
			result = CreateAsyncPorts() ;
	
		if ( kIOReturnSuccess == result )
		{
			CFRetain( inRunLoop ) ;
			CFRetain( inRunLoopMode ) ;
			
			mRunLoop 		= inRunLoop ;
			mRunLoopSource	= CFMachPortCreateRunLoopSource(
									kCFAllocatorDefault,
									GetAsyncCFPort(),
									0) ;
			mRunLoopMode	= inRunLoopMode ;
	
			if (!mRunLoopSource)
			{
				CFRelease( mRunLoop ) ;
				mRunLoop = 0 ;
				
				CFRelease( mRunLoopMode ) ;
				mRunLoopMode = 0 ;
				
				result = kIOReturnNoMemory ;
			}
			
			if ( kIOReturnSuccess == result )
				CFRunLoopAddSource(mRunLoop, mRunLoopSource, mRunLoopMode ) ;
		}
		
		return result ;
	}
	
	void
	Device::RemoveDispatcherFromRunLoop(
		CFRunLoopRef			runLoop,
		CFRunLoopSourceRef		runLoopSource,
		CFStringRef				mode)
	{
		if ( runLoop && runLoopSource )
			if (CFRunLoopContainsSource( runLoop, runLoopSource, mode ))
				CFRunLoopRemoveSource( runLoop, runLoopSource, mode );
	}
	
	const Boolean
	Device::TurnOnNotification(
		void*					callBackRefCon)
	{
		IOReturn				result					= kIOReturnSuccess ;
		io_scalar_inband_t		params ;
		mach_msg_type_number_t	size = 0 ;
		
		if ( nil == mConnection )
			result = kIOReturnNoDevice ;
		
		if (!AsyncPortsExist())
			result = kIOReturnError ;	// zzz  need a new error type meaning "you forgot to call AddDispatcherToRunLoop"
	
		if ( kIOReturnSuccess == result )
		{
			params[0]	= (UInt32)(IOAsyncCallback) & Device::BusResetHandler ;
			params[1]	= (UInt32) callBackRefCon; //(UInt32) this ;
		
			result = ::io_async_method_scalarI_scalarO( mConnection, mAsyncPort, mBusResetAsyncRef, 1, 
					kSetAsyncRef_BusReset, params, 2, params, & size ) ;
			
		}
	
		if ( kIOReturnSuccess == result )
		{
			params[0]	= (UInt32)(IOAsyncCallback) & Device::BusResetDoneHandler ;
			params[1]	= (UInt32) callBackRefCon; //(UInt32) this ;
			size = 0 ;
		
			result = ::io_async_method_scalarI_scalarO( mConnection, mAsyncPort, mBusResetDoneAsyncRef, 1, 
					kSetAsyncRef_BusResetDone, params, 2, params, & size) ;
			
		}
		
		if ( kIOReturnSuccess == result )
			mNotifyIsOn = true ;
			
		return ( kIOReturnSuccess == result ) ;
	}
	
	void
	Device::TurnOffNotification()
	{
		IOReturn				result			= kIOReturnSuccess ;
		io_scalar_inband_t		params ;
		mach_msg_type_number_t	size 		= 0 ;
		
		// if notification isn't on, skip out.
		if (!mNotifyIsOn)
			return ;
	
		if (!mConnection)
			result = kIOReturnNoDevice ;
		
		if ( kIOReturnSuccess == result )
		{
			params[0]	= (UInt32)(IOAsyncCallback) 0 ;
			params[1]	= 0 ;
		
			result = ::io_async_method_scalarI_scalarO( mConnection, mAsyncPort, mBusResetAsyncRef, 1, 
					kSetAsyncRef_BusReset, params, 2, params, & size) ;
			
			params[0]	= (UInt32)(IOAsyncCallback) 0 ;
			params[1]	= 0 ;
			size = 0 ;
		
			result = ::io_async_method_scalarI_scalarO( mConnection, mAsyncPort, mBusResetDoneAsyncRef, 1,
					kSetAsyncRef_BusResetDone, params, 2, params, & size) ;
			
		}
		
		mNotifyIsOn = false ;
	}
	
	
	const IOFireWireBusResetHandler
	Device::SetBusResetHandler(
		IOFireWireBusResetHandler			inBusResetHandler)
	{
		IOFireWireBusResetHandler	result = mBusResetHandler ;
		mBusResetHandler = inBusResetHandler ;
		
		return result ;
	}
	
	const IOFireWireBusResetDoneHandler
	Device::SetBusResetDoneHandler(
		IOFireWireBusResetDoneHandler		inBusResetDoneHandler)
	{
		IOFireWireBusResetDoneHandler	result = mBusResetDoneHandler ;
		mBusResetDoneHandler = inBusResetDoneHandler ;
		
		return result ;
	}
	
	void
	Device::BusResetHandler(
		void*							refCon,
		IOReturn						result)
	{
		Device*	me = IOFireWireIUnknown::InterfaceMap<Device>::GetThis(refCon) ;
	
		if (me->mBusResetHandler)
			(me->mBusResetHandler)( (IOFireWireLibDeviceRef)refCon, (FWClientCommandID) me) ;
	}
	
	void
	Device::BusResetDoneHandler(
		void*							refCon,
		IOReturn						result)
	{
		Device*	me = IOFireWireIUnknown::InterfaceMap<Device>::GetThis(refCon) ;
	
		if (me->mBusResetDoneHandler)
			(me->mBusResetDoneHandler)( (IOFireWireLibDeviceRef)refCon, (FWClientCommandID) me) ;
	}
	
#pragma mark -
	IOReturn
	Device::Read(
		io_object_t				device,
		const FWAddress &		addr,
		void*					buf,
		UInt32*					size,
		Boolean					failOnReset,
		UInt32					generation)
	{
		if (!mIsOpen)
			return kIOReturnNotOpen ;
		if ( device != mDefaultDevice && device != 0 )
			return kIOReturnBadArgument ;
	
		IOByteCount 			resultSize	 	= sizeof( *size ) ;
		ReadParams	 			params 			= { addr, buf, *size, failOnReset, generation, device == 0 /*isAbs*/ } ;
	
		return IOConnectMethodStructureIStructureO( mConnection, kRead, sizeof(params), & resultSize, & params, size ) ;
	}
	
	IOReturn
	Device::ReadQuadlet( io_object_t device, const FWAddress & addr, UInt32* val, Boolean failOnReset, 
		UInt32 generation )
	{
		if (!mIsOpen)
			return kIOReturnNotOpen ;
		if ( device != mDefaultDevice && device != 0 )
			return kIOReturnBadArgument ;
	
		IOByteCount 				resultSize 		= sizeof( *val ) ;
		ReadQuadParams	 			params 			= { addr, val, 1, failOnReset, generation, device == 0 /*isAbs*/ } ;
	
		return IOConnectMethodStructureIStructureO( mConnection, kReadQuad, sizeof(params), & resultSize, & params, val ) ;
	}
	
	IOReturn
	Device::Write(
		io_object_t				device,
		const FWAddress &		addr,
		const void*				buf,
		UInt32* 				size,
		Boolean					failOnReset,
		UInt32					generation)
	{
		if (!mIsOpen)
			return kIOReturnNotOpen ;
		if ( device != mDefaultDevice && device != 0 )
			return kIOReturnBadArgument ;
	
		IOByteCount 				resultSize 		= sizeof( *size ) ;
		WriteParams		 			params 			= { addr, buf, *size, failOnReset, generation, device == 0 /*isAbs*/ } ;
	
		return IOConnectMethodStructureIStructureO( mConnection, kWrite, sizeof(params), & resultSize, & params, size ) ;
	}
	
	IOReturn
	Device::WriteQuadlet(
		io_object_t				device,
		const FWAddress &		addr,
		const UInt32			val,
		Boolean 				failOnReset,
		UInt32					generation)
	{
		if (!mIsOpen)
			return kIOReturnNotOpen ;
		if ( device != mDefaultDevice && device != 0 )
			return kIOReturnBadArgument ;
	
		WriteQuadParams 			params 			= { addr, val, failOnReset, generation, device == 0 } ;
		IOByteCount 				resultSize 		= 0 ;
	
		return IOConnectMethodStructureIStructureO( mConnection, kWriteQuad, sizeof(params), & resultSize, & params, nil ) ;
	}
	
	IOReturn
	Device::CompareSwap(
		io_object_t				device,
		const FWAddress &		addr,
		UInt32 					cmpVal,
		UInt32 					newVal,
		Boolean 				failOnReset,
		UInt32					generation)
	{
		if (!mIsOpen)
			return kIOReturnNotOpen ;
		if ( device != mDefaultDevice && device != 0 )
			return kIOReturnBadArgument ;
	
		IOByteCount 				resultSize 		= sizeof(UInt64) ;
		UInt64						result ;
		CompareSwapParams			params ;
		
		params.addr					= addr ;
		*(UInt32*)&params.cmpVal	= cmpVal ;
		*(UInt32*)&params.swapVal	= newVal ;
		params.size					= 1 ;
		params.failOnReset			= failOnReset ;
		params.generation			= generation ;
		params.isAbs				= device == 0 ;
	
		return IOConnectMethodStructureIStructureO( mConnection, kCompareSwap, sizeof(params), & resultSize, & params, & result ) ;
	}
	
	IOReturn
	Device::CompareSwap64(
		io_object_t				device,
		const FWAddress &		addr,
		UInt32*					expectedVal,
		UInt32*					newVal,
		UInt32*					oldVal,
		IOByteCount				size,
		Boolean 				failOnReset,
		UInt32					generation)
	{
		if (!mIsOpen)
			return kIOReturnNotOpen ;
		if ( device != mDefaultDevice && device != 0 )
			return kIOReturnBadArgument ;
	
		CompareSwapParams			params ;
		
		// config params
		params.addr				= addr ;
		if ( size==4 )
		{
			*(UInt32*)&params.cmpVal	= expectedVal[0] ;
			*(UInt32*)&params.swapVal	= newVal[0] ;
		}
		else
		{
			params.cmpVal			= *(UInt64*)expectedVal ;
			params.swapVal			= *(UInt64*)newVal ;
		}
	
		params.size				= size >> 2 ;
		params.failOnReset		= failOnReset ;
		params.generation		= generation ;
		params.isAbs			= device == 0 ;
	
		UInt64			result ;
		IOByteCount 	resultSize 	= sizeof(UInt64) ;
		IOReturn error = ::IOConnectMethodStructureIStructureO( mConnection, kCompareSwap, sizeof(params), 
				& resultSize, & params, & result ) ;
		
		if (size==4)
		{
			oldVal[0] = *(UInt32*)&result ;
			if ( oldVal[0] != expectedVal[0] )
				error = kIOReturnCannotLock ;
		}
		else
		{
			*(UInt64*)oldVal = result ;
			if ( *(UInt64*)expectedVal != result )
				error = kIOReturnCannotLock ;
		}
		
		return error ;
	}
	
	#pragma mark -
	#pragma mark --- command objects ----------
	
	IOFireWireLibCommandRef	
	Device::CreateReadCommand(
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
		
		IUnknownVTbl** iUnknown = ReadCmd::Alloc(*this, device, addr, buf, size, callback, failOnReset, generation, inRefCon) ;
		if (iUnknown)
		{
			(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
			(*iUnknown)->Release(iUnknown) ;
		}
	
		return result ;
	}
	
	IOFireWireLibCommandRef	
	Device::CreateReadQuadletCommand(
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
		
		IUnknownVTbl** iUnknown = ReadQuadCmd::Alloc(*this, device, addr, quads, numQuads, callback, failOnReset, generation, inRefCon) ;
		if (iUnknown)
		{
			(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
			(*iUnknown)->Release(iUnknown) ;
		}
		
		return result ;
	}
	
	IOFireWireLibCommandRef
	Device::CreateWriteCommand(
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
		
		IUnknownVTbl** iUnknown = WriteCmd::Alloc(*this, device, addr, buf, size, callback, failOnReset, generation, inRefCon) ;
		if (iUnknown)
		{
			(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
			(*iUnknown)->Release(iUnknown) ;
		}
		
		return result ;
		
	}
	
	IOFireWireLibCommandRef
	Device::CreateWriteQuadletCommand(
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
		
		IUnknownVTbl** iUnknown = WriteQuadCmd::Alloc(*this, device, addr, quads, numQuads, callback, failOnReset, generation, inRefCon) ;
		if (iUnknown)
		{
			(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
			(*iUnknown)->Release(iUnknown) ;
		}
		
		return result ;
	}
	
	IOFireWireLibCommandRef
	Device::CreateCompareSwapCommand( io_object_t device, const FWAddress & addr, UInt64 cmpVal, UInt64 newVal, 
		unsigned int quads, IOFireWireLibCommandCallback callback, Boolean failOnReset, UInt32 generation,
		void* inRefCon, REFIID iid)
	{
		IOFireWireLibCommandRef	result = 0 ;
		
		IUnknownVTbl** iUnknown = CompareSwapCmd::Alloc( *this, device, addr, cmpVal, newVal, quads, callback, failOnReset, generation, inRefCon) ;
		if (iUnknown)
		{
			(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
			(*iUnknown)->Release(iUnknown) ;
		}
		
		return result ;
	}
	
	#pragma mark -
	IOReturn
	Device::BusReset()
	{
		if (!mIsOpen)
			return kIOReturnNotOpen ;
	
		return IOConnectMethodScalarIScalarO( mConnection, kBusReset, 0, 0 ) ;
	}
	
	IOReturn
	Device::GetCycleTime(
		UInt32*		outCycleTime)
	{
		return IOConnectMethodScalarIScalarO( mConnection, kCycleTime, 0, 1, outCycleTime ) ;
	}
	
	IOReturn
	Device::GetBusCycleTime(
		UInt32*		outBusTime,
		UInt32*		outCycleTime)
	{
		return IOConnectMethodScalarIScalarO( mConnection, kGetBusCycleTime, 0, 2, outBusTime, outCycleTime ) ;
	}
	
	IOReturn
	Device::GetGenerationAndNodeID(
		UInt32*		outGeneration,
		UInt16*		outNodeID)
	{
		UInt32	nodeID ;
		IOReturn result = IOConnectMethodScalarIScalarO( mConnection, kGetGenerationAndNodeID, 0, 2, outGeneration, & nodeID ) ;
		
		*outNodeID = (UInt16)nodeID ;
	
		return result ;
	}
	
	IOReturn
	Device::GetLocalNodeID(
		UInt16*		outLocalNodeID)
	{
		UInt32	localNodeID ;
	
		IOReturn result = IOConnectMethodScalarIScalarO( mConnection, kGetLocalNodeID, 0, 1, & localNodeID ) ;
		*outLocalNodeID = (UInt16)localNodeID ;
		
		return result ;
	}
	
	IOReturn
	Device::GetResetTime(
		AbsoluteTime*			resetTime)
	{
		IOByteCount		size = sizeof(*resetTime) ;
		return IOConnectMethodStructureIStructureO( mConnection, kGetResetTime, 0, & size, NULL, resetTime ) ;
	}
	
	#pragma mark -
	IOFireWireLibLocalUnitDirectoryRef
	Device::CreateLocalUnitDirectory( REFIID iid )
	{
		IOFireWireLibLocalUnitDirectoryRef	result = 0 ;
	
		if (mIsOpen)
		{
				// we allocate a user space pseudo address space with the reference we
				// got from the kernel
			IUnknownVTbl**	iUnknown = reinterpret_cast<IUnknownVTbl**>(LocalUnitDirectory::Alloc(*this)) ;
						
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
	Device::GetConfigDirectory(
		REFIID				iid)
	{
		IOFireWireLibConfigDirectoryRef	result = 0 ;
		
		// we allocate a user space config directory space with the reference we
		// got from the kernel
		IUnknownVTbl**	iUnknown	= reinterpret_cast<IUnknownVTbl**>(ConfigDirectoryCOM::Alloc(*this)) ;

		// we got a new iUnknown from the object. Query it for the interface
		// requested in iid...
		if (iUnknown)
		{
			(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
		
			// we got the interface we wanted (or at least another ref to iUnknown),
			// so we call iUnknown.Release()
			(*iUnknown)->Release(iUnknown) ;
		}
	
		return result ;
	}
	
	IOFireWireLibConfigDirectoryRef
	Device::CreateConfigDirectoryWithIOObject(
		io_object_t			inObject,
		REFIID				iid)
	{
		IOFireWireLibConfigDirectoryRef	result = 0 ;
		
		// we allocate a user space pseudo address space with the reference we
		// got from the kernel
		IUnknownVTbl**	iUnknown	= reinterpret_cast<IUnknownVTbl**>(ConfigDirectoryCOM::Alloc(*this, (KernConfigDirectoryRef)inObject)) ;
	
		// we got a new iUnknown from the object. Query it for the interface
		// requested in iid...
		(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
			
		// we got the interface we wanted (or at least another ref to iUnknown),
		// so we call iUnknown.Release()
		(*iUnknown)->Release(iUnknown) ;
	
		return result ;
	}
	
	IOFireWireLibPhysicalAddressSpaceRef
	Device::CreatePhysicalAddressSpace(
		UInt32						inSize,
		void*						inBackingStore,
		UInt32						inFlags,
		REFIID						iid)	// flags unused
	{
		IOFireWireLibPhysicalAddressSpaceRef	result = 0 ;
		
		if ( mIsOpen )
		{
			PhysicalAddressSpaceCreateParams	params ;
			params.size				= inSize ;
			params.backingStore		= inBackingStore ;
			params.flags			= inFlags ;
			
			KernPhysicalAddrSpaceRef	output ;
			IOByteCount	size = sizeof(output) ;
			
			if (kIOReturnSuccess  == IOConnectMethodStructureIStructureO(
								mConnection,
								kPhysicalAddrSpace_Allocate,
								sizeof(params),
								& size,
								(void*) & params,
								(void*) & output ))
			{
				if (output)
				{
					// we allocate a user space pseudo address space with the reference we
					// got from the kernel
					IUnknownVTbl**	iUnknown = PhysicalAddressSpace::Alloc(*this, output, inSize, inBackingStore, inFlags) ;
					
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
	
	IOFireWireLibPseudoAddressSpaceRef
	Device::CreateAddressSpace(
		UInt32						inSize,
		void*						inRefCon,
		UInt32						inQueueBufferSize,
		void*						inBackingStore,
		UInt32						inFlags,
		REFIID						iid,
		Boolean						isInitialUnits,
		UInt32						inAddressLo )
	{
		if (!mIsOpen)
		{
			IOFireWireLibLog_("Device::CreatePseudoAddressSpace: no connection or device is not open\n") ;
			return 0 ;
		}
		
		if ( !inBackingStore && ( (inFlags & kFWAddressSpaceAutoWriteReply != 0) || (inFlags & kFWAddressSpaceAutoReadReply != 0) || (inFlags & kFWAddressSpaceAutoCopyOnWrite != 0) ) )
		{
			IOFireWireLibLog_("%s %u:Can't create address space with nil backing store!\n", __FILE__, __LINE__) ;
			return 0 ;
		}
		
		IOFireWireLibPseudoAddressSpaceRef				result = 0 ;
	
		void*	queueBuffer = nil ;
		if ( inQueueBufferSize > 0 )
			queueBuffer	= new Byte[inQueueBufferSize] ;
	
		AddressSpaceCreateParams	params ;
		params.size 			= inSize ;
		params.queueBuffer 		= (Byte*) queueBuffer ;
		params.queueSize		= (UInt32) inQueueBufferSize ;
		params.backingStore 	= (Byte*) inBackingStore ;
		params.refCon			= this ;
		params.flags			= inFlags ;
		params.isInitialUnits	= isInitialUnits ;
		params.addressLo		= inAddressLo ;
		
		KernAddrSpaceRef	addrSpaceRef ;
		IOByteCount			size	= sizeof(addrSpaceRef) ;
		
		// call the routine which creates a pseudo address space in the kernel.
		IOReturn err = ::IOConnectMethodStructureIStructureO( mConnection, kPseudoAddrSpace_Allocate,
										sizeof(params), & size, & params, & addrSpaceRef ) ;
	
		if ( !err )
		{
			// we allocate a user space pseudo address space with the reference we
			// got from the kernel
			IUnknownVTbl**	iUnknown = PseudoAddressSpace::Alloc(*this, addrSpaceRef, queueBuffer, inQueueBufferSize, inBackingStore, inRefCon) ;
			
			// we got a new iUnknown from the object. Query it for the interface
			// requested in iid...
			(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
			
			// we got the interface we wanted (or at least another ref to iUnknown),
			// so we call iUnknown.Release()
			(*iUnknown)->Release(iUnknown) ;
			
		}
		
		return result ;
	}
	
	
	#pragma mark -
	IOReturn
	Device::FireBugMsg(
		const char *	inMsg)
	{
		IOReturn result = kIOReturnSuccess ;
		UInt32 size = strlen(inMsg) + 1 ;
		
		if (!mIsOpen)
			result = kIOReturnNotOpen ;
	
		{
			UInt32		generation = 0 ;
			UInt16		nodeID ;
	
			if ( kIOReturnSuccess == result )
				result = GetGenerationAndNodeID( & generation, & nodeID ) ;
	
			if (kIOReturnSuccess == result)
			{
				result = Write(0, FWAddress(0x4242, 0x42424242, 0x4242), inMsg, & size, true, generation) ;
				
				// firebug never responds, so just change a timeout err into success
				if ( kIOReturnTimeout == result )
					result = kIOReturnSuccess ;
			}
		}
		
		return result ;
	}
	
	IOReturn
	Device::FireLog(
		const char *	format, 
		va_list			ap )
	{
		char		msg[128] ;
		vsnprintf( msg, 127, format, ap ) ;
	
		IOByteCount outSize = 0 ;
		
		IOReturn	err = ::IOConnectMethodStructureIStructureO( GetUserClientConnection(), kFireLog, strlen( msg )+1, & outSize, msg, nil ) ;
		return err ;
	}
	
	IOReturn
	Device::FireLog( const char* format, ... )
	{
		va_list		ap ;
		va_start( ap, format ) ;
		
		IOReturn error = FireLog( format, ap ) ;
		
		va_end( ap ) ;	
		
		return error ;	
	}
	
	IOReturn
	Device::CreateCFStringWithOSStringRef(
		KernOSStringRef	inStringRef,
		UInt32				inStringLen,
		CFStringRef*&		text)
	{
		char*				textBuffer = new char[inStringLen+1] ;
		io_connect_t		connection = GetUserClientConnection() ;
		UInt32				stringSize ;
		
		if (!textBuffer)
			return kIOReturnNoMemory ;
		
		IOReturn result = ::IOConnectMethodScalarIScalarO( connection, kGetOSStringData, 3, 1, inStringRef, 
				inStringLen, textBuffer, & stringSize) ;
	
		textBuffer[inStringLen] = 0 ;
		
		if (text && (kIOReturnSuccess == result))
			*text = CFStringCreateWithCString(kCFAllocatorDefault, textBuffer, kCFStringEncodingASCII) ;
		
		delete textBuffer ;
	
		return result ;
	}
	
	IOReturn
	Device::CreateCFDataWithOSDataRef(
		KernOSDataRef		inDataRef,
		IOByteCount			inDataSize,
		CFDataRef*&			data)
	{
		UInt8*			buffer = new UInt8[inDataSize] ;
		IOByteCount		dataSize ;
		
		if (!buffer)
			return kIOReturnNoMemory ;
			
		if (!mConnection)
			return kIOReturnError ;
		
		IOReturn result = IOConnectMethodScalarIScalarO( mConnection, kGetOSDataData, 3, 1,
									inDataRef, inDataSize, buffer, & dataSize) ;
	
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
	Device::AddIsochCallbackDispatcherToRunLoopForMode( // v3+
		CFRunLoopRef			inRunLoop,
		CFStringRef 			inRunLoopMode )
	{
		IOReturn result = kIOReturnSuccess ;
		
		if ( !inRunLoop || !inRunLoopMode )
		{
			IOFireWireLibLog_("Device::AddIsochCallbackDispatcherToRunLoopForMode: inRunLoop==0 || inRunLoopMode==0\n") ;
			return kIOReturnBadArgument ;
		}
	
		if (!IsochAsyncPortsExist())
			result = CreateIsochAsyncPorts() ;
	
		if ( kIOReturnSuccess == result )
		{
			CFRetain(inRunLoop) ;
			CFRetain(inRunLoopMode) ;
			
			mIsochRunLoop 			= inRunLoop ;
			mIsochRunLoopSource		= CFMachPortCreateRunLoopSource( kCFAllocatorDefault,
																	GetIsochAsyncCFPort() ,
																	0) ;
			mIsochRunLoopMode		= inRunLoopMode ;
	
			if (!mIsochRunLoopSource)
			{
				CFRelease( mIsochRunLoop ) ;
				CFRelease( mIsochRunLoopMode ) ;
				result = kIOReturnNoMemory ;
			}
			
			if ( kIOReturnSuccess == result )
				CFRunLoopAddSource(mIsochRunLoop, mIsochRunLoopSource, kCFRunLoopDefaultMode) ;
		}
		
		return result ;
	}
	
	void
	Device::RemoveIsochCallbackDispatcherFromRunLoop()
	{
		RemoveDispatcherFromRunLoop( mIsochRunLoop, mIsochRunLoopSource, mIsochRunLoopMode ) ;
		
		CFRelease(mIsochRunLoop) ;
		mIsochRunLoop = 0 ;
		
		CFRelease( mIsochRunLoopSource ) ;
		mIsochRunLoopSource = 0 ;
		
		CFRelease( mIsochRunLoopMode );
		mIsochRunLoopMode = 0 ;
	}
	
	IOFireWireLibIsochChannelRef 
	Device::CreateIsochChannel(
		Boolean 				inDoIRM, 
		UInt32 					inPacketSize, 
		IOFWSpeed 				inPrefSpeed,
		REFIID 					iid)
	{
		IOFireWireLibIsochChannelRef	result = 0 ;
		
		IUnknownVTbl** iUnknown = reinterpret_cast<IUnknownVTbl**>(IsochChannelCOM::Alloc(*this, inDoIRM, inPacketSize, inPrefSpeed)) ;
		if (iUnknown)
		{
			(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
			(*iUnknown)->Release(iUnknown) ;
		}
		
		return result ;
	}
	
	IOFireWireLibRemoteIsochPortRef
	Device::CreateRemoteIsochPort(
		Boolean					inTalking,
		REFIID 					iid)
	{
		IOFireWireLibRemoteIsochPortRef		result = 0 ;
		
		IUnknownVTbl** iUnknown = RemoteIsochPortCOM::Alloc(*this, inTalking) ;
		if (iUnknown)
		{
			(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
			(*iUnknown)->Release(iUnknown) ;
		}
		
		return result ;
	}
	
	IOFireWireLibLocalIsochPortRef
	Device::CreateLocalIsochPort(
		Boolean					inTalking,
		DCLCommand*		inDCLProgram,
		UInt32					inStartEvent,
		UInt32					inStartState,
		UInt32					inStartMask,
		IOVirtualRange			inDCLProgramRanges[],			// optional optimization parameters
		UInt32					inDCLProgramRangeCount,
		IOVirtualRange			inBufferRanges[],
		UInt32					inBufferRangeCount,
		REFIID 					iid)
	{
		if ( !inDCLProgram )
		{
			IOFireWireLibLog_("%s %u: can't create local isoch port with nil DCL program!\n", __FILE__, __LINE__) ;
			return 0 ;
		}
		
		IOFireWireLibLocalIsochPortRef	result = 0 ;
		
		IUnknownVTbl** iUnknown = LocalIsochPortCOM::Alloc(*this, inTalking, inDCLProgram, inStartEvent, inStartState, 
															inStartMask, inDCLProgramRanges, inDCLProgramRangeCount, inBufferRanges, inBufferRangeCount) ;
		if (iUnknown)
		{
			(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
			(*iUnknown)->Release(iUnknown) ;
		}
		
		return result ;
	}
	
	IOFireWireLibDCLCommandPoolRef
	Device::CreateDCLCommandPool(
		IOByteCount 			inSize, 
		REFIID 					iid )
	{
		IOFireWireLibDCLCommandPoolRef	result = 0 ;
		
		IUnknownVTbl** iUnknown = DCLCommandPoolCOM::Alloc(*this, inSize) ;
		if (iUnknown)
		{
			(*iUnknown)->QueryInterface(iUnknown, iid, (void**) & result) ;
			(*iUnknown)->Release(iUnknown) ;
		}
		
		return result ;
	}
	
	void
	Device::PrintDCLProgram( const DCLCommand* dcl, UInt32 inDCLCount ) 
	{
		const DCLCommand*		currentDCL	= dcl ;
		UInt32						index		= 0 ;
		
		fprintf(stderr, "IsochPort::printDCLProgram: dcl=0x%08lX, inDCLCount=%lud\n", (UInt32) dcl, inDCLCount) ;
		
		while ( (index < inDCLCount) && currentDCL )
		{
			fprintf(stderr, "\n#0x%04lX  @0x%08lX   next=0x%08lX, cmplrData=0x%08lX, op=%lu ", 
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
					fprintf(stderr, "(DCLTransferPacketStruct) buffer=%08lX, size=%lu",
						(UInt32) ((DCLTransferPacketStruct*)currentDCL)->buffer,
						((DCLTransferPacketStruct*)currentDCL)->size) ;
					break ;
					
				case kDCLSendBufferOp:
				case kDCLReceiveBufferOp:
					fprintf(stderr, "(DCLTransferBufferStruct) buffer=%p, size=%lu, packetSize=0x%x, bufferOffset=%08lX",
						((DCLTransferBufferStruct*)currentDCL)->buffer,
						((DCLTransferBufferStruct*)currentDCL)->size,
						((DCLTransferBufferStruct*)currentDCL)->packetSize,
						((DCLTransferBufferStruct*)currentDCL)->bufferOffset) ;
					break ;
		
				case kDCLCallProcOp:
					fprintf(stderr, "(DCLCallProcStruct) proc=%p, procData=%08lX",
						((DCLCallProcStruct*)currentDCL)->proc,
						((DCLCallProcStruct*)currentDCL)->procData) ;
					break ;
					
				case kDCLLabelOp:
					fprintf(stderr, "(DCLLabelStruct)") ;
					break ;
					
				case kDCLJumpOp:
					fprintf(stderr, "(DCLJump) pJumpDCLLabel=%p",
						((DCLJump*)currentDCL)->pJumpDCLLabel) ;
					break ;
					
				case kDCLSetTagSyncBitsOp:
					fprintf(stderr, "(DCLSetTagSyncBitsStruct) tagBits=%04x, syncBits=%04x",
						((DCLSetTagSyncBitsStruct*)currentDCL)->tagBits,
						((DCLSetTagSyncBitsStruct*)currentDCL)->syncBits) ;
					break ;
					
				case kDCLUpdateDCLListOp:
					fprintf(stderr, "(DCLUpdateDCLListStruct) dclCommandList=%p, numDCLCommands=%lud \n",
						((DCLUpdateDCLListStruct*)currentDCL)->dclCommandList,
						((DCLUpdateDCLListStruct*)currentDCL)->numDCLCommands) ;
					
					for(UInt32 listIndex=0; listIndex < ((DCLUpdateDCLListStruct*)currentDCL)->numDCLCommands; ++listIndex)
					{
						fprintf(stderr, "%p ", (((DCLUpdateDCLListStruct*)currentDCL)->dclCommandList)[listIndex]) ;
					}
					
					break ;
		
				case kDCLPtrTimeStampOp:
					fprintf(stderr, "(DCLPtrTimeStampStruct) timeStampPtr=%p",
						((DCLPtrTimeStampStruct*)currentDCL)->timeStampPtr) ;
			}
			
			currentDCL = currentDCL->pNextDCLCommand ;
			++index ;
		}
		
		fprintf(stderr, "\n") ;
	
		if (index != inDCLCount)
			fprintf(stderr, "unexpected end of program\n") ;
		
		if (currentDCL != NULL)
			fprintf(stderr, "program too long for count\n") ;
	}
	
	IOReturn
	Device::GetBusGeneration( UInt32* outGeneration )
	{
		return IOConnectMethodScalarIScalarO( mConnection, kGetBusGeneration, 0, 1, outGeneration ) ;
	}
	
	IOReturn
	Device::GetLocalNodeIDWithGeneration( UInt32 checkGeneration, UInt16* outLocalNodeID )
	{
		UInt32	nodeID ;
		IOReturn error = IOConnectMethodScalarIScalarO( mConnection, kGetLocalNodeIDWithGeneration, 1, 1, checkGeneration, & nodeID ) ;
		
		*outLocalNodeID = (UInt16)nodeID ;
		
		return error ;
	}
	
	IOReturn
	Device::GetRemoteNodeID( UInt32 checkGeneration, UInt16* outRemoteNodeID )
	{
		UInt32	nodeID ;
		IOReturn error = IOConnectMethodScalarIScalarO( mConnection, kGetRemoteNodeID, 1, 1, checkGeneration, & nodeID ) ;
		
		*outRemoteNodeID = (UInt16)nodeID ;
		
		return error ;
	}
	
	IOReturn
	Device::GetSpeedToNode( UInt32 checkGeneration, IOFWSpeed* outSpeed)
	{
		UInt32	speed ;
		IOReturn error = IOConnectMethodScalarIScalarO( mConnection, kGetSpeedToNode, 1, 1, checkGeneration, & speed ) ;

		*outSpeed = (IOFWSpeed)speed ;
		
		return error ;
	}
	
	IOReturn
	Device::GetSpeedBetweenNodes( UInt32 checkGeneration, UInt16 srcNodeID, UInt16 destNodeID, IOFWSpeed* outSpeed)
	{
		UInt32 speed ;
		
		IOReturn error = IOConnectMethodScalarIScalarO( mConnection, kGetSpeedBetweenNodes, 3, 1, checkGeneration, (UInt32)srcNodeID, (UInt32)destNodeID, & speed ) ;
		
		*outSpeed = (IOFWSpeed)speed ;
		
		return error ;
	}

	IOReturn
	Device::GetIRMNodeID( UInt32 checkGeneration, UInt16* irmNodeID )
	{
		UInt32 tempNodeID ;
		
		IOReturn error = ::IOConnectMethodScalarIScalarO( mConnection, kGetIRMNodeID, 1, 1, checkGeneration, & tempNodeID ) ;

		*irmNodeID = (UInt16)tempNodeID ;
		
		return error ;
	}
	
#pragma mark -
	IOFireWireDeviceInterface DeviceCOM::sInterface = 
	{
		INTERFACEIMP_INTERFACE,
		4, 0, // version/revision
		& DeviceCOM::SInterfaceIsInited,
		& DeviceCOM::SGetDevice,
		& DeviceCOM::SOpen,
		& DeviceCOM::SOpenWithSessionRef,
		& DeviceCOM::SClose,
		& DeviceCOM::SNotificationIsOn,
		& DeviceCOM::SAddCallbackDispatcherToRunLoop,
		& DeviceCOM::SRemoveCallbackDispatcherFromRunLoop,
		& DeviceCOM::STurnOnNotification,
		& DeviceCOM::STurnOffNotification,
		& DeviceCOM::SSetBusResetHandler,
		& DeviceCOM::SSetBusResetDoneHandler,
		& DeviceCOM::SClientCommandIsComplete,
		
		// --- FireWire send/recv methods --------------
		& DeviceCOM::SRead,
		& DeviceCOM::SReadQuadlet,
		& DeviceCOM::SWrite,
		& DeviceCOM::SWriteQuadlet,
		& DeviceCOM::SCompareSwap,
		
		// --- firewire commands -----------------------
		& DeviceCOM::SCreateReadCommand,
		& DeviceCOM::SCreateReadQuadletCommand,
		& DeviceCOM::SCreateWriteCommand,
		& DeviceCOM::SCreateWriteQuadletCommand,
		& DeviceCOM::SCreateCompareSwapCommand,
	
			// --- other methods ---------------------------
		& DeviceCOM::SBusReset,
		& DeviceCOM::SGetCycleTime,
		& DeviceCOM::SGetGenerationAndNodeID,
		& DeviceCOM::SGetLocalNodeID,
		& DeviceCOM::SGetResetTime,
	
			// --- unit directory support ------------------
		& DeviceCOM::SCreateLocalUnitDirectory,
	
		& DeviceCOM::SGetConfigDirectory,
		& DeviceCOM::SCreateConfigDirectoryWithIOObject,
			// --- address space support -------------------
		& DeviceCOM::SCreatePseudoAddressSpace,
		& DeviceCOM::SCreatePhysicalAddressSpace,
			
			// --- debugging -------------------------------
		& DeviceCOM::SFireBugMsg,
		
			// --- isoch -----------------------------------
		& DeviceCOM::SAddIsochCallbackDispatcherToRunLoop,
		& DeviceCOM::SCreateRemoteIsochPort,
		& DeviceCOM::SCreateLocalIsochPort,
		& DeviceCOM::SCreateIsochChannel,
		& DeviceCOM::SCreateDCLCommandPool,
	
			// --- refcon ----------------------------------
		& DeviceCOM::SGetRefCon,
		& DeviceCOM::SSetRefCon,
	
			// --- debugging -------------------------------
		// do not use this function
	//	& DeviceCOM::SGetDebugProperty,
		NULL,
		& DeviceCOM::SPrintDCLProgram,
	
			// --- initial units address space -------------	
		& DeviceCOM::SCreateInitialUnitsPseudoAddressSpace,
	
			// --- callback dispatcher utils (cont.) -------
		& DeviceCOM::SAddCallbackDispatcherToRunLoopForMode,
		& DeviceCOM::SAddIsochCallbackDispatcherToRunLoopForMode,
		& DeviceCOM::SRemoveIsochCallbackDispatcherFromRunLoop,
		
			// --- seize service ---------------------------
		& DeviceCOM::SSeize,
		
			// --- more debugging --------------------------
		& DeviceCOM::SFireLog,
	
			// --- other methods --- new in v3
		& DeviceCOM::SGetBusCycleTime,
		
		//
		// v4
		//
		& DeviceCOM::SCreateCompareSwapCommand64,
		& DeviceCOM::SCompareSwap64,
		& DeviceCOM::SGetBusGeneration,
		& DeviceCOM::SGetLocalNodeIDWithGeneration,
		& DeviceCOM::SGetRemoteNodeID,
		& DeviceCOM::SGetSpeedToNode,
		& DeviceCOM::SGetSpeedBetweenNodes,
		
		//
		// v5/
		//
		
		& DeviceCOM::S_GetIRMNodeID
	} ;
	
	DeviceCOM::DeviceCOM( CFDictionaryRef propertyTable, io_service_t service )
	: Device( reinterpret_cast<IUnknownVTbl*>( &sInterface ), propertyTable, service )
	{
	}
	
	DeviceCOM::~DeviceCOM()
	{
	}
	
	// ============================================================
	// static allocator
	// ============================================================
	
	IOFireWireDeviceInterface** 
	DeviceCOM::Alloc( CFDictionaryRef propertyTable, io_service_t service )
	{
		DeviceCOM*	me = nil ;
		
		try {
			me = new DeviceCOM( propertyTable, service ) ;
		} catch ( ... ) {
		}

		if( !me )
			return nil ;

		return reinterpret_cast<IOFireWireDeviceInterface**>(&me->GetInterface()) ;
	}

	Boolean
	DeviceCOM::SInterfaceIsInited(IOFireWireLibDeviceRef self) 
	{ 
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->mIsInited;
	}
	
	io_object_t
	DeviceCOM::SGetDevice(IOFireWireLibDeviceRef self)
	{ 
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->mDefaultDevice; 
	}
	
	IOReturn
	DeviceCOM::SOpen(IOFireWireLibDeviceRef self) 
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->Open() ;
	}
	
	IOReturn
	DeviceCOM::SOpenWithSessionRef(IOFireWireLibDeviceRef self, IOFireWireSessionRef session) 
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->OpenWithSessionRef(session) ;
	}
	
	IOReturn
	DeviceCOM::SSeize(		// v3+
		IOFireWireLibDeviceRef	 	self,
		IOOptionBits				flags,
		... )
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->Seize( flags ) ;
	}
	
	void
	DeviceCOM::SClose(IOFireWireLibDeviceRef self)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->Close() ;
	}
		
	// --- FireWire notification methods --------------
	const Boolean
	DeviceCOM::SNotificationIsOn(IOFireWireLibDeviceRef self)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->mNotifyIsOn; 
	}
	
	const IOReturn
	DeviceCOM::SAddCallbackDispatcherToRunLoop(IOFireWireLibDeviceRef self, CFRunLoopRef inRunLoop)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->AddCallbackDispatcherToRunLoop(inRunLoop) ;
	}
	
	IOReturn
	DeviceCOM::SAddCallbackDispatcherToRunLoopForMode(	// v3+
		IOFireWireLibDeviceRef 		self, 
		CFRunLoopRef 				inRunLoop,
		CFStringRef					inRunLoopMode )
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->AddCallbackDispatcherToRunLoopForMode( inRunLoop, inRunLoopMode ) ;
	}
	
	const void
	DeviceCOM::SRemoveCallbackDispatcherFromRunLoop(IOFireWireLibDeviceRef self)
	{
		DeviceCOM*	me = IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis( self ) ;
		me->RemoveDispatcherFromRunLoop( me->mRunLoop, me->mRunLoopSource, me->mRunLoopMode ) ;
		
		if ( me->mRunLoop )
		{
			CFRelease( me->mRunLoop ) ;
			me->mRunLoop = 0 ;
		}
		
		if ( me->mRunLoopSource )
		{
			CFRelease( me->mRunLoopSource ) ;
			me->mRunLoopSource = 0 ;
		}
		
		if ( me->mRunLoopMode )
		{
			CFRelease( me->mRunLoopMode) ;
			me->mRunLoopMode = 0 ;
		}
	}
	
	// Makes notification active. Returns false if notification could not be activated.
	const Boolean
	DeviceCOM::STurnOnNotification(IOFireWireLibDeviceRef self)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->TurnOnNotification( self ) ;
	}
	
	// Notification callbacks will no longer be called.
	void
	DeviceCOM::STurnOffNotification(IOFireWireLibDeviceRef self)
	{
		IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->TurnOffNotification() ;
	}
	
	const IOFireWireBusResetHandler
	DeviceCOM::SSetBusResetHandler(IOFireWireLibDeviceRef self, IOFireWireBusResetHandler inBusResetHandler)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->SetBusResetHandler(inBusResetHandler) ;
	}
	
	const IOFireWireBusResetDoneHandler
	DeviceCOM::SSetBusResetDoneHandler(IOFireWireLibDeviceRef self, IOFireWireBusResetDoneHandler inBusResetDoneHandler) 
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->SetBusResetDoneHandler(inBusResetDoneHandler) ;
	}
	
	void
	DeviceCOM::SClientCommandIsComplete(IOFireWireLibDeviceRef self, FWClientCommandID commandID, IOReturn status)
	{ 
		IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->ClientCommandIsComplete(commandID, status); 
	}
	
	IOReturn
	DeviceCOM::SRead(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress * addr, void* buf, 
		UInt32* size, Boolean failOnReset, UInt32 generation)
	{ 
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->Read(device, *addr, buf, size, failOnReset, generation) ; 
	}
	
	IOReturn
	DeviceCOM::SReadQuadlet(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress * addr,
		UInt32* val, Boolean failOnReset, UInt32 generation)
	{ 
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->ReadQuadlet(device, *addr, val, failOnReset, generation); 
	}
	
	IOReturn
	DeviceCOM::SWrite(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress * addr, const void* buf, UInt32* size,
		Boolean failOnReset, UInt32 generation)
	{ 
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->Write(device, *addr, buf, size, failOnReset, generation) ;
	}
	
	IOReturn
	DeviceCOM::SWriteQuadlet(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress* addr, const UInt32 val, 
		Boolean failOnReset, UInt32 generation)
	{ 
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->WriteQuadlet(device, *addr, val, failOnReset, generation) ;
	}
	
	IOReturn
	DeviceCOM::SCompareSwap(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress* addr, UInt32 cmpVal, UInt32 newVal, Boolean failOnReset, UInt32 generation)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CompareSwap(device, *addr, cmpVal, newVal, failOnReset, generation) ;
	}
	
	//
	// v4
	//
	IOReturn
	DeviceCOM::SCompareSwap64( IOFireWireLibDeviceRef self, io_object_t device, const FWAddress* addr, 
			UInt32* expectedVal, UInt32* newVal, UInt32* oldVal, IOByteCount size, Boolean failOnReset,
			UInt32 generation)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CompareSwap64( device, *addr, expectedVal, 
				newVal, oldVal, size, failOnReset, generation ) ;
	}

	//
	// v4
	//
	IOFireWireLibCommandRef
	DeviceCOM::SCreateCompareSwapCommand64(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress* addr, UInt64 cmpVal, 
		UInt64 newVal, IOFireWireLibCommandCallback callback, Boolean failOnReset, UInt32 generation, void* inRefCon,
		REFIID iid )
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CreateCompareSwapCommand(device, 
				addr ? *addr : FWAddress(), cmpVal, newVal, 2, callback, failOnReset, generation, inRefCon, iid) ;
	}

	IOReturn
	DeviceCOM::SGetBusGeneration( IOFireWireLibDeviceRef self, UInt32* outGeneration )
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->GetBusGeneration( outGeneration ) ;
	}
	
	IOReturn
	DeviceCOM::SGetLocalNodeIDWithGeneration( IOFireWireLibDeviceRef self, UInt32 checkGeneration, UInt16* outLocalNodeID )
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->GetLocalNodeIDWithGeneration( checkGeneration, outLocalNodeID ) ;
	}
	
	IOReturn
	DeviceCOM::SGetRemoteNodeID( IOFireWireLibDeviceRef self, UInt32 checkGeneration, UInt16* outRemoteNodeID )
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->GetRemoteNodeID( checkGeneration, outRemoteNodeID ) ;
	}
	
	IOReturn
	DeviceCOM::SGetSpeedToNode( IOFireWireLibDeviceRef self, UInt32 checkGeneration, IOFWSpeed* outSpeed)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->GetSpeedToNode( checkGeneration, outSpeed ) ;
	}
	
	IOReturn
	DeviceCOM::SGetSpeedBetweenNodes( IOFireWireLibDeviceRef self, UInt32 checkGeneration, UInt16 srcNodeID, UInt16 destNodeID,  IOFWSpeed* outSpeed)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->GetSpeedBetweenNodes( checkGeneration, srcNodeID, destNodeID, outSpeed ) ;
	}

#pragma mark -
	IOFireWireLibCommandRef	
	DeviceCOM::SCreateReadCommand(IOFireWireLibDeviceRef self,
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
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CreateReadCommand(device, addr ? *addr : FWAddress(), buf, size, callback, failOnReset, generation, inRefCon, iid) ;
	}
	
	IOFireWireLibCommandRef	
	DeviceCOM::SCreateReadQuadletCommand(IOFireWireLibDeviceRef self, io_object_t device, 
		const FWAddress* addr, UInt32 val[], const UInt32 numQuads, IOFireWireLibCommandCallback callback, 
		Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CreateReadQuadletCommand(device, addr ? *addr : FWAddress(), val, numQuads, callback, failOnReset, generation, inRefCon, iid) ;
	}
	
	IOFireWireLibCommandRef
	DeviceCOM::SCreateWriteCommand(IOFireWireLibDeviceRef self, io_object_t device, 
		const FWAddress* addr, void* buf, UInt32 size, IOFireWireLibCommandCallback callback, 
		Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CreateWriteCommand( device, addr ? *addr : FWAddress(), buf, size, callback, failOnReset, generation, inRefCon, iid) ;
	}
	
	IOFireWireLibCommandRef
	DeviceCOM::SCreateWriteQuadletCommand(IOFireWireLibDeviceRef self, io_object_t device, 
		const FWAddress* addr, UInt32 quads[], const UInt32 numQuads, IOFireWireLibCommandCallback callback,
		Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CreateWriteQuadletCommand(device, addr ? *addr : FWAddress(), quads, numQuads, callback, failOnReset, generation, inRefCon, iid) ;
	}
	
	IOFireWireLibCommandRef
	DeviceCOM::SCreateCompareSwapCommand(IOFireWireLibDeviceRef self, io_object_t device, const FWAddress* addr, UInt32 cmpVal, UInt32 newVal, 
		IOFireWireLibCommandCallback callback, Boolean failOnReset, UInt32 generation, void* inRefCon, REFIID iid)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CreateCompareSwapCommand(device, 
				addr ? *addr : FWAddress(), cmpVal, newVal, 1, callback, failOnReset, generation, inRefCon, iid) ;
	}
	
	IOReturn
	DeviceCOM::SBusReset(IOFireWireLibDeviceRef self)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->BusReset() ;
	}
	

#pragma mark -
	//
	// --- local unit directory support ------------------
	//
	IOFireWireLibLocalUnitDirectoryRef
	DeviceCOM::SCreateLocalUnitDirectory(IOFireWireLibDeviceRef self, REFIID iid)
	{ 
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CreateLocalUnitDirectory(iid); 
	}
	
	IOFireWireLibConfigDirectoryRef
	DeviceCOM::SGetConfigDirectory(IOFireWireLibDeviceRef self, REFIID iid)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->GetConfigDirectory(iid) ;
	}
	
	IOFireWireLibConfigDirectoryRef
	DeviceCOM::SCreateConfigDirectoryWithIOObject(IOFireWireLibDeviceRef self, io_object_t inObject, REFIID iid)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CreateConfigDirectoryWithIOObject(inObject, iid) ;
	}
	
#pragma mark -
	//
	// --- address space support ------------------
	//
	IOFireWireLibPseudoAddressSpaceRef
	DeviceCOM::SCreatePseudoAddressSpace(IOFireWireLibDeviceRef self, UInt32 inLength, void* inRefCon, UInt32 inQueueBufferSize, 
		void* inBackingStore, UInt32 inFlags, REFIID iid)
	{ 
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CreatePseudoAddressSpace(inLength, inRefCon, inQueueBufferSize, inBackingStore, inFlags, iid); 
	}
	
	IOFireWireLibPhysicalAddressSpaceRef
	DeviceCOM::SCreatePhysicalAddressSpace(IOFireWireLibDeviceRef self, UInt32 inLength, void* inBackingStore, UInt32 flags, REFIID iid)
	{ 
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CreatePhysicalAddressSpace(inLength, inBackingStore, flags, iid); 
	}
	
	IOFireWireLibPseudoAddressSpaceRef
	DeviceCOM::SCreateInitialUnitsPseudoAddressSpace( 
		IOFireWireLibDeviceRef  	self,
		UInt32						inAddressLo,
		UInt32  					inSize, 
		void*  						inRefCon, 
		UInt32  					inQueueBufferSize, 
		void*  						inBackingStore, 
		UInt32  					inFlags,
		REFIID  					iid)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CreateInitialUnitsPseudoAddressSpace( inAddressLo, inSize, inRefCon, inQueueBufferSize, inBackingStore, inFlags, iid ) ;
	}
	
	//
	// --- FireLog -----------------------------------
	//
	IOReturn
	DeviceCOM::SFireLog(
		IOFireWireLibDeviceRef self,
		const char *		format, 
		... )
	{
		va_list		ap ;
		va_start( ap, format ) ;
		
		IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->FireLog( format, ap ) ;
		
		va_end( ap ) ;
		
		return kIOReturnSuccess ;
	}
	
	//
	// --- isoch -----------------------------------
	//
	IOReturn
	DeviceCOM::SAddIsochCallbackDispatcherToRunLoop(
		IOFireWireLibDeviceRef	self,
		CFRunLoopRef			inRunLoop)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->AddIsochCallbackDispatcherToRunLoopForMode( inRunLoop, kCFRunLoopDefaultMode ) ;
	}
	
	IOReturn
	DeviceCOM::SAddIsochCallbackDispatcherToRunLoopForMode( // v3+
		IOFireWireLibDeviceRef 	self, 
		CFRunLoopRef			inRunLoop,
		CFStringRef 			inRunLoopMode )
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->AddIsochCallbackDispatcherToRunLoopForMode( inRunLoop, inRunLoopMode ) ;
	}
	
	void
	DeviceCOM::SRemoveIsochCallbackDispatcherFromRunLoop(	// v3+
		IOFireWireLibDeviceRef 	self)
	{
		IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->RemoveIsochCallbackDispatcherFromRunLoop() ;
	}
	
	IOFireWireLibRemoteIsochPortRef
	DeviceCOM::SCreateRemoteIsochPort(
		IOFireWireLibDeviceRef 	self,
		Boolean					inTalking,
		REFIID					iid)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CreateRemoteIsochPort(inTalking, iid) ;
	}
	
	IOFireWireLibLocalIsochPortRef
	DeviceCOM::SCreateLocalIsochPort(
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
		REFIID 					iid)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CreateLocalIsochPort(inTalking, inDCLProgram, inStartEvent, inStartState, inStartMask,
												inDCLProgramRanges, inDCLProgramRangeCount, inBufferRanges, 
												inBufferRangeCount, iid) ;
	}
	
	IOFireWireLibIsochChannelRef
	DeviceCOM::SCreateIsochChannel(
		IOFireWireLibDeviceRef 	self, 
		Boolean 				inDoIRM, 
		UInt32 					inPacketSize, 
		IOFWSpeed 				inPrefSpeed,
		REFIID 					iid)
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CreateIsochChannel(inDoIRM, inPacketSize, inPrefSpeed, iid) ;
	}
	
	IOFireWireLibDCLCommandPoolRef
	DeviceCOM::SCreateDCLCommandPool(
		IOFireWireLibDeviceRef 	self, 
		IOByteCount 			size, 
		REFIID 					iid )
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->CreateDCLCommandPool(size, iid) ;
	}
	
	void
	DeviceCOM::SPrintDCLProgram( IOFireWireLibDeviceRef self, const DCLCommand* dcl, UInt32 inDCLCount ) 
	{
		IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->PrintDCLProgram(dcl, inDCLCount) ;
	}

	IOReturn
	DeviceCOM::S_GetIRMNodeID( IOFireWireLibDeviceRef self, UInt32 checkGeneration, UInt16* outIRMNodeID )
	{
		return IOFireWireIUnknown::InterfaceMap<DeviceCOM>::GetThis(self)->GetIRMNodeID( checkGeneration, outIRMNodeID) ;		
	}
}
