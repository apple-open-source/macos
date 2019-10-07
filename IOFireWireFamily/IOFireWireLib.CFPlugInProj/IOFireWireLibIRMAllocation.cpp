/*
 *  IOFireWireLibIRMAllocation.cpp
 *  IOFireWireFamily
 *
 *  Created by Andy on 02/06/07.
 *  Copyright (c) 2007 Apple Computer, Inc. All rights reserved.
 *
 *	$ Log:  $
 */

#import "IOFireWireLibIRMAllocation.h"
#import "IOFireWireLibDevice.h"
#import "IOFireWireLibPriv.h"

#if !defined(__LP64__)
#import <IOKit/iokitmig.h>
#endif

namespace IOFireWireLib {

#pragma mark IRMAllocation -

	// ============================================================
	// IRMAllocation
	// ============================================================
	IRMAllocation::IRMAllocation( const IUnknownVTbl& interface, 
								 Device& userclient,
								 UserObjectHandle inKernIRMAllocationRef,
								 void* inCallBack,
								 void* inRefCon	)
	: IOFireWireIUnknown( interface ), 
		mNotifyIsOn(false),
		mUserClient(userclient), 
		mKernIRMAllocationRef(inKernIRMAllocationRef),
		mLostHandler( (IOFireWireLibIRMAllocationLostNotificationProc) inCallBack ),
		mUserRefCon(inRefCon),
		mRefInterface( reinterpret_cast<IOFireWireLibIRMAllocationRef>( & GetInterface() ) )
	{
		userclient.AddRef() ;
	}
	

	IRMAllocation::~IRMAllocation()
	{
		uint32_t outputCnt = 0;
		const uint64_t inputs[1]={(const uint64_t)mKernIRMAllocationRef};

		IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
								  kReleaseUserObject,
								  inputs,1,
								  NULL,&outputCnt);
		
		mUserClient.Release() ;
	}

	Boolean 
	IRMAllocation::NotificationIsOn ( IOFireWireLibIRMAllocationRef self ) 
	{
		return mNotifyIsOn ;
	}
	
	Boolean 
	IRMAllocation::TurnOnNotification ( IOFireWireLibIRMAllocationRef self )
	{
		IOReturn				err					= kIOReturnSuccess ;
		io_connect_t			connection			= mUserClient.GetUserClientConnection() ;
		
		// if notification is already on, skip out.
		if (mNotifyIsOn)
			return true ;
		
		if (!connection)
			err = kIOReturnNoDevice ;
		
		if ( kIOReturnSuccess == err )
		{
			uint64_t refrncData[kOSAsyncRef64Count];
			refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) 0;
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long) 0;
			const uint64_t inputs[3] = {(const uint64_t)mKernIRMAllocationRef,(const uint64_t)&IRMAllocation::LostProc,(const uint64_t)self};
			uint32_t outputCnt = 0;
			err = IOConnectCallAsyncScalarMethod(connection,
												 kIRMAllocation_SetRef,
												 mUserClient.GetAsyncPort(),
												 refrncData,kOSAsyncRef64Count,
												 inputs,3,
												 NULL,&outputCnt);
		}
		
		if ( kIOReturnSuccess == err )
			mNotifyIsOn = true ;
		
		return ( kIOReturnSuccess == err ) ;
	}
	
	void 
	IRMAllocation::TurnOffNotification ( IOFireWireLibIRMAllocationRef self )
	{
		IOReturn				err			= kIOReturnSuccess ;
		io_connect_t			connection	= mUserClient.GetUserClientConnection() ;
		
		// if notification isn't on, skip out.
		if (!mNotifyIsOn)
			return ;
		
		if (!connection)
			err = kIOReturnNoDevice ;
		
		if ( kIOReturnSuccess == err )
		{
			uint64_t refrncData[kOSAsyncRef64Count];
			refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) 0;
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long) 0;
			const uint64_t inputs[3] = {(const uint64_t)mKernIRMAllocationRef,0,(const uint64_t)self};
			uint32_t outputCnt = 0;
			
			// set callback for writes to 0
			err = IOConnectCallAsyncScalarMethod(connection,
												 kIRMAllocation_SetRef,
												 mUserClient.GetIsochAsyncPort(),
												 refrncData,kOSAsyncRef64Count,
												 inputs,3,
												 NULL,&outputCnt);
			
		}
		
		mNotifyIsOn = false ;
	}
	
	void IRMAllocation::SetReleaseIRMResourcesOnFree ( IOFireWireLibIRMAllocationRef self,  Boolean doRelease)
	{
		uint32_t outputCnt = 0;
		const uint64_t inputs[2]={(const uint64_t)mKernIRMAllocationRef,doRelease};
		
		IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
								  kIRMAllocation_setDeallocateOnRelease,
								  inputs,2,
								  NULL,&outputCnt);
	}
	
	IOReturn IRMAllocation::AllocateIsochResources(IOFireWireLibIRMAllocationRef self, UInt8 isochChannel, UInt32 bandwidthUnits)
	{
		uint32_t outputCnt = 0;
		const uint64_t inputs[3]={(const uint64_t)mKernIRMAllocationRef,isochChannel,bandwidthUnits};
		
		return IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
										 kIRMAllocation_AllocateResources,
										 inputs,3,
										 NULL,&outputCnt);
	}
	
	IOReturn IRMAllocation::DeallocateIsochResources(IOFireWireLibIRMAllocationRef self)
	{
		uint32_t outputCnt = 0;
		const uint64_t inputs[1]={(const uint64_t)mKernIRMAllocationRef};
		
		return IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
										 kIRMAllocation_DeallocateResources,
										 inputs,1,
										 NULL,&outputCnt);
	}
	
	Boolean IRMAllocation::AreIsochResourcesAllocated(IOFireWireLibIRMAllocationRef self, UInt8 *pAllocatedIsochChannel, UInt32 *pAllocatedBandwidthUnits)
	{
		uint32_t outputCnt = 2;
		uint64_t outputVal[2];
		const uint64_t inputs[1]={(const uint64_t)mKernIRMAllocationRef};
		Boolean result;
		
		result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
										   kIRMAllocation_areResourcesAllocated,
										   inputs,1,
										   outputVal,&outputCnt);

		if (result)
		{
			*pAllocatedIsochChannel = outputVal[0];
			*pAllocatedBandwidthUnits = outputVal[1];
		}
		return result;
	}
	
	void IRMAllocation::SetRefCon(IOFireWireLibIRMAllocationRef self, void* refCon)
	{
		mUserRefCon = refCon;
	}

	void* IRMAllocation::GetRefCon(IOFireWireLibIRMAllocationRef self)
	{
		return mUserRefCon;
	}
	
	void IRMAllocation::LostProc( IOFireWireLibIRMAllocationRef refcon, IOReturn result, void** args, int numArgs)
	{
		IRMAllocation* me = IOFireWireIUnknown::InterfaceMap<IRMAllocation>::GetThis(refcon) ;
		if (me->mLostHandler)
			me->mLostHandler(refcon,me->mUserRefCon);
	}


#pragma mark IRMAllocationCOM -

	IRMAllocationCOM::Interface IRMAllocationCOM::sInterface = 
	{
		INTERFACEIMP_INTERFACE,
		1, 0,
		&IRMAllocationCOM::SSetReleaseIRMResourcesOnFree,
		&IRMAllocationCOM::SAllocateIsochResources,
		&IRMAllocationCOM::SDeallocateIsochResources,
		&IRMAllocationCOM::SAreIsochResourcesAllocated,
		&IRMAllocationCOM::SNotificationIsOn,
		&IRMAllocationCOM::STurnOnNotification,
		&IRMAllocationCOM::STurnOffNotification,
		&IRMAllocationCOM::SSetRefCon,
		&IRMAllocationCOM::SGetRefCon
	} ;
	
	//
	// --- ctor/dtor -----------------------
	//
	
	IRMAllocationCOM::IRMAllocationCOM( Device& userclient,
									   UserObjectHandle inKernIRMAllocationRef,
									   void* inCallBack,
									   void* inRefCon )
	: IRMAllocation( reinterpret_cast<const IUnknownVTbl &>( sInterface ), userclient, inKernIRMAllocationRef, inCallBack, inRefCon )
	{
	}
	
	IRMAllocationCOM::~IRMAllocationCOM()
	{
	}
	
	//
	// --- IUNKNOWN support ----------------
	//
		
	IUnknownVTbl**
	IRMAllocationCOM::Alloc(Device& userclient,
							UserObjectHandle inKernIRMAllocationRef,
							void* inCallBack,
							void* inRefCon )
	{
		IRMAllocationCOM*	me = nil ;
		
		try {
			me = new IRMAllocationCOM( userclient, inKernIRMAllocationRef, inCallBack, inRefCon ) ;
		} catch(...) {
		}

		return ( nil == me ) ? nil : reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	HRESULT
	IRMAllocationCOM::QueryInterface(REFIID iid, void ** ppv )
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if ( CFEqual(interfaceID, IUnknownUUID) ||  CFEqual(interfaceID, kIOFireWireIRMAllocationInterfaceID) )
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
	
	//
	// --- static methods ------------------
	//
	const void IRMAllocationCOM::SSetReleaseIRMResourcesOnFree (IOFireWireLibIRMAllocationRef self, Boolean doRelease )
	{
		return IOFireWireIUnknown::InterfaceMap<IRMAllocationCOM>::GetThis(self)->SetReleaseIRMResourcesOnFree( self, doRelease ) ;
	}

	IOReturn IRMAllocationCOM::SAllocateIsochResources(IOFireWireLibIRMAllocationRef self, UInt8 isochChannel, UInt32 bandwidthUnits)
	{
		return IOFireWireIUnknown::InterfaceMap<IRMAllocationCOM>::GetThis(self)->AllocateIsochResources( self, isochChannel, bandwidthUnits) ;
	}
	
	IOReturn IRMAllocationCOM::SDeallocateIsochResources(IOFireWireLibIRMAllocationRef self)
	{
		return IOFireWireIUnknown::InterfaceMap<IRMAllocationCOM>::GetThis(self)->DeallocateIsochResources(self) ;
	}
	
	Boolean IRMAllocationCOM::SAreIsochResourcesAllocated(IOFireWireLibIRMAllocationRef self, UInt8 *pAllocatedIsochChannel, UInt32 *pAllocatedBandwidthUnits)
	{
		return IOFireWireIUnknown::InterfaceMap<IRMAllocationCOM>::GetThis(self)->AreIsochResourcesAllocated(self, pAllocatedIsochChannel, pAllocatedBandwidthUnits) ;
	}
	
	Boolean IRMAllocationCOM::SNotificationIsOn(IOFireWireLibIRMAllocationRef self)
	{
		return IOFireWireIUnknown::InterfaceMap<IRMAllocationCOM>::GetThis(self)->NotificationIsOn(self) ;
	}
	
	Boolean IRMAllocationCOM::STurnOnNotification(IOFireWireLibIRMAllocationRef self)
	{
		return IOFireWireIUnknown::InterfaceMap<IRMAllocationCOM>::GetThis(self)->TurnOnNotification(self) ;
	}
	
	void IRMAllocationCOM::STurnOffNotification(IOFireWireLibIRMAllocationRef self)
	{
		return IOFireWireIUnknown::InterfaceMap<IRMAllocationCOM>::GetThis(self)->TurnOffNotification(self);
	}
	
	void  IRMAllocationCOM::SSetRefCon(IOFireWireLibIRMAllocationRef self, void* refCon)
	{
		return IOFireWireIUnknown::InterfaceMap<IRMAllocationCOM>::GetThis(self)->SetRefCon(self,refCon);
	}
	
	void*  IRMAllocationCOM::SGetRefCon(IOFireWireLibIRMAllocationRef self)
	{
		return IOFireWireIUnknown::InterfaceMap<IRMAllocationCOM>::GetThis(self)->GetRefCon(self);
	}
}