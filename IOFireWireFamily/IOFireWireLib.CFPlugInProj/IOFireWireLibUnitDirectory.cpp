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
 *  IOFireWireLibUnitDirectory.cpp
 *  IOFireWireLib
 *
 *  Created by NWG on Thu Apr 27 2000.
 *  Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 */

#import "IOFireWireLibUnitDirectory.h"
#import <CoreFoundation/CoreFoundation.h>

namespace IOFireWireLib {

	// static interface table
	LocalUnitDirectory::Interface LocalUnitDirectory::sInterface =
	{
		INTERFACEIMP_INTERFACE,
		1, 0, // version/revision
		& LocalUnitDirectory::SAddEntry_Ptr,
		& LocalUnitDirectory::SAddEntry_UInt32,
		& LocalUnitDirectory::SAddEntry_FWAddress,
		& LocalUnitDirectory::SPublish,
		& LocalUnitDirectory::SUnpublish
	} ;
	
	// ============================================================
	// LocalUnitDirectory implementation
	// ============================================================
	
	LocalUnitDirectory::LocalUnitDirectory( Device& userclient )
	: IOFireWireIUnknown( reinterpret_cast<IUnknownVTbl*>(& sInterface) ),
	  mUserClient(userclient), 
	  mPublished(false)
	{
		IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kUnitDirCreate, 
				0, 1, & mKernUnitDirRef ) ;
	}
	
	LocalUnitDirectory::~LocalUnitDirectory()
	{
		if (mPublished)
			Unpublish() ;
	}

	LocalUnitDirectory::Interface** 
	LocalUnitDirectory::Alloc( Device& userclient )
	{
		LocalUnitDirectory*	me = new LocalUnitDirectory( userclient ) ;
	
		if( !me )
			return nil ;
			
		return reinterpret_cast<Interface**>(&me->GetInterface()) ;
	}
	
	HRESULT STDMETHODCALLTYPE
	LocalUnitDirectory::QueryInterface(REFIID iid, LPVOID* ppv)
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if (CFEqual(interfaceID, IUnknownUUID) || CFEqual(interfaceID, kIOFireWireLocalUnitDirectoryInterfaceID) )
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

	IOReturn
	LocalUnitDirectory::AddEntry( int key, void* inBuffer, size_t inLen, CFStringRef )
	{
		if (inLen > 0xC00)
		{
			IOFireWireLibLog_(("LocalUnitDirectory::AddEntry: entry too large\n")) ;
			return kIOReturnBadArgument ;
		}
		
		return IOConnectMethodScalarIStructureI( mUserClient.GetUserClientConnection(), kUnitDirAddEntry_Buffer, 2, 
					inLen, mKernUnitDirRef, key, inBuffer ) ;
	}
	
	IOReturn
	LocalUnitDirectory::AddEntry(
		int			key,
		UInt32		value,
		CFStringRef	/*inDesc = NULL*/)	// zzz don't know what to do with this yet...
										// zzz should probably go into kernel
	{
		IOReturn err = IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kUnitDirAddEntry_UInt32, 3, 0, mKernUnitDirRef, key, value ) ;
	
		IOFireWireLibLogIfErr_(err, "LocalUnitDirectory::AddEntry[UInt32](): result = 0x%08x\n", err) ;
	
		return err ;
	}
	
	IOReturn
	LocalUnitDirectory::AddEntry(
		int					key,
		const FWAddress &	value,
		CFStringRef	/*inDesc = NULL*/)	// zzz don't know what to do with this yet...
										// zzz should probably go into kernel
	{
		return IOConnectMethodScalarIStructureI( mUserClient.GetUserClientConnection(), kUnitDirAddEntry_FWAddr, 
					2, sizeof(value), mKernUnitDirRef, key, & value ) ;
	}
	
	IOReturn
	LocalUnitDirectory::Publish()
	{
		if (mPublished)
			return kIOReturnSuccess ;
	
		IOReturn err = IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kUnitDirPublish, 1, 0, mKernUnitDirRef ) ;
	
		mPublished = ( kIOReturnSuccess == err ) ;
	
		return err ;
	}
	
	IOReturn
	LocalUnitDirectory::Unpublish()
	{
		if (!mPublished)
			return kIOReturnSuccess ;
			
		IOReturn err = IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kUnitDirUnpublish, 1, 0, mKernUnitDirRef) ;
			
		if ( kIOReturnSuccess == err )
				mPublished = false ;
		
		return err ;
	}

	// ============================================================
	//
	// static interface methods
	//
	// ============================================================
	
	IOReturn 
	LocalUnitDirectory::SAddEntry_Ptr(
		DirRef self,
		int				key, 
		void* 			inBuffer, 
		size_t 			inLen, 
		CFStringRef 	inDesc)
	{ 
		return IOFireWireIUnknown::InterfaceMap<LocalUnitDirectory>::GetThis(self)->AddEntry(key, inBuffer, inLen, inDesc); 
	}
	
	IOReturn
	LocalUnitDirectory::SAddEntry_UInt32(
		DirRef self,
		int 			key,
		UInt32 			value,
		CFStringRef 	inDesc)
	{ 
		return IOFireWireIUnknown::InterfaceMap<LocalUnitDirectory>::GetThis(self)->AddEntry(key, value, inDesc); 
	}
	
	IOReturn
	LocalUnitDirectory::SAddEntry_FWAddress(
		DirRef self,
		int 				key, 
		const FWAddress*	inAddr, 
		CFStringRef 		inDesc)
	{ 
		return IOFireWireIUnknown::InterfaceMap<LocalUnitDirectory>::GetThis(self)->AddEntry(key, *inAddr, inDesc); 
	}
	
	IOReturn
	LocalUnitDirectory::SPublish(
		DirRef self)
	{ 
		return IOFireWireIUnknown::InterfaceMap<LocalUnitDirectory>::GetThis(self)->Publish();
	}
	
	IOReturn
	LocalUnitDirectory::SUnpublish(
		DirRef self)
	{ 
		return IOFireWireIUnknown::InterfaceMap<LocalUnitDirectory>::GetThis(self)->Unpublish(); 
	}
}
