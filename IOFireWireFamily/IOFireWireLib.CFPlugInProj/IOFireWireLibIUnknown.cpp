/*
 *  IOFireWireLibIUnknown.cpp
 *  IOFireWireFamily
 *
 *  Created by Niels on Thu Feb 27 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 *	$ Log:IOFireWireLibIUnknown.cpp,v $
 */

#import "IOFireWireLibIUnknown.h"
#import <assert.h>
#import <string.h>		// bzero

namespace IOFireWireLib {

	IOFireWireIUnknown::IOFireWireIUnknown( const IUnknownVTbl & interface )
	: mInterface( interface, this ),
	  mRefCount(1) 
	{
	}
#if IOFIREWIRELIBDEBUG
	IOFireWireIUnknown::~IOFireWireIUnknown()
	{
	}	
#endif

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
} // namespace
