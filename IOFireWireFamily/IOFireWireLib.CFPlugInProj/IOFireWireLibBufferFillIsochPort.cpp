/*
 *  IOFireWireLibBufferFillIsochPort.cpp
 *  IOFireWireFamily
 *
 *  Created by Niels on Fri Feb 21 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 *	$ Log:IOFireWireLibBufferFillIsochPort.cpp,v $
 */

#import "IOFireWireLibBufferFillIsochPort.h"
#import "IOFireWireLibIUnknown.h"

// this is to be made public later:

//
// buffer fill isoch port
//

//	uuid string: 9ABAFE8E-541E-11D7-9669-000393470256
#define kIOFireWireBufferFillIsochPortInterfaceID CFUUIDGetConstantUUIDWithBytes( kCFAllocatorDefault \
											,0x9A, 0xBA, 0xFE, 0x8E, 0x54, 0x1E, 0x11, 0xD7\
											,0x96, 0x69, 0x00, 0x03, 0x93, 0x47, 0x02, 0x56)



namespace IOFireWireLib {

	BufferFillIsochPort::BufferFillIsochPort( const IUnknownVTbl & vtable, Device& device, 
			UInt32 interruptMicroseconds, UInt32 numRanges, IOVirtualRange ranges[] )
	: IOFireWireIUnknown( vtable )
	{
	}

#pragma mark -	
	const IOFireWireBufferFillIsochPortInterface BufferFillIsochPortCOM::sInterface = {
		INTERFACEIMP_INTERFACE,
		1, 0, //vers, rev
		
	} ;

	BufferFillIsochPortCOM::BufferFillIsochPortCOM( 
			Device& device, UInt32 interruptMicroseconds, UInt32 numRanges, IOVirtualRange ranges[] )
	: BufferFillIsochPort( reinterpret_cast<const IUnknownVTbl &>( sInterface ), device, interruptMicroseconds, numRanges, ranges )
	{
	}
	
	IUnknownVTbl**
	BufferFillIsochPortCOM::Alloc( 
			Device& device, UInt32 interruptMicroseconds, UInt32 numRanges, IOVirtualRange ranges[] )
	{
		BufferFillIsochPortCOM*	me = new BufferFillIsochPortCOM( device, interruptMicroseconds, numRanges, ranges ) ;
		if (!me)
			return nil ;

		return reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}

	HRESULT
	BufferFillIsochPortCOM::QueryInterface( REFIID iid, LPVOID* ppv )
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if ( CFEqual( interfaceID, IUnknownUUID ) ||  CFEqual( interfaceID, kIOFireWireBufferFillIsochPortInterfaceID ) )
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
	
} // namespace
