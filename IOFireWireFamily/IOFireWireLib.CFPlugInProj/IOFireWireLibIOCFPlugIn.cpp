/*
 *  IOFireWireLibIOCFPlugIn.cpp
 *  IOFireWireFamily
 *
 *  Created by Niels on Thu Feb 27 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 *	$ Log:IOFireWireLibIOCFPlugIn.cpp,v $
 */

#import "IOFireWireLibIOCFPlugIn.h"
#import "IOFireWireLibDevice.h"

namespace IOFireWireLib {
	
	const IOCFPlugInInterface IOCFPlugIn::sInterface = 
	{
		INTERFACEIMP_INTERFACE,
		1, 0, // version/revision
		& IOCFPlugIn::SProbe,
		& IOCFPlugIn::SStart,
		& IOCFPlugIn::SStop
	};
	
	IOCFPlugIn::IOCFPlugIn()
	: IOFireWireIUnknown( reinterpret_cast<const IUnknownVTbl &>( sInterface ) ),
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
}
