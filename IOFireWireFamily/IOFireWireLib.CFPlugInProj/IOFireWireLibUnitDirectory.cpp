/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
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
#import "IOFireWireLibDevice.h"

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
	: IOFireWireIUnknown( reinterpret_cast<const IUnknownVTbl &>( sInterface) ),
	  mUserClient(userclient), 
	  mPublished(false)
	{
		IOConnectMethodScalarIScalarO(  mUserClient.GetUserClientConnection(), kLocalConfigDirectory_Create, 
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

	inline const char *
	CFStringGetChars( CFStringRef string, unsigned & outLen )
	{
		CFIndex stringLength = ::CFStringGetLength( string ) ;
	
		const char * result = ::CFStringGetCStringPtr( string, kCFStringEncodingUTF8 ) ;
		if ( result )
		{
			outLen = (unsigned)stringLength ;
			return result ;
		}
		
		CFIndex bufferSize = ::CFStringGetBytes( string, ::CFRangeMake( 0, stringLength ), kCFStringEncodingUTF8, 
				0xFF, false, NULL, 0, NULL ) ;
		
		UInt8 inlineBuffer[ bufferSize ] ;
		result = (char*)inlineBuffer ;

		::CFStringGetBytes( string, ::CFRangeMake( 0, stringLength ), kCFStringEncodingUTF8, 0xFF, false, inlineBuffer, bufferSize, NULL ) ;
		
		outLen = bufferSize ;
		return result ;
	}
	
	IOReturn
	LocalUnitDirectory::AddEntry( int key, void* buffer, size_t len, CFStringRef desc )
	{
		unsigned descLen = 0;
		const char * descCString = NULL;
		
		if( desc )
		{
			descCString = CFStringGetChars( desc, descLen ) ;
		}
		
		return ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kLocalConfigDirectory_AddEntry_Buffer, 6, 0,
				mKernUnitDirRef, key, buffer, len, descCString, descLen ) ;
	}
	
	IOReturn
	LocalUnitDirectory::AddEntry(
		int			key,
		UInt32		value,
		CFStringRef desc )
	{
		unsigned descLen = 0;
		const char * descCString = NULL;
		
		if( desc )
		{
			descCString = CFStringGetChars( desc, descLen ) ;
		}
		
		IOReturn err = IOConnectMethodScalarIScalarO(   mUserClient.GetUserClientConnection(), 
														kLocalConfigDirectory_AddEntry_UInt32, 5, 0, mKernUnitDirRef, key, value, descCString, descLen ) ;
	
		DebugLogCond( err, "LocalUnitDirectory::AddEntry[UInt32](): result = 0x%08x\n", err ) ;
	
		return err ;
	}
	
	IOReturn
	LocalUnitDirectory::AddEntry(
		int					key,
		const FWAddress &	value,
		CFStringRef			desc )
	{
		unsigned descLen = 0;
		const char * descCString = NULL;
		
		if( desc )
		{
			descCString = CFStringGetChars( desc, descLen ) ;
		}

		return IOConnectMethodScalarIStructureI(	mUserClient.GetUserClientConnection(), 
													kLocalConfigDirectory_AddEntry_FWAddr, 
													4, sizeof(value), mKernUnitDirRef, key, descCString, descLen, & value) ;
	}
	
	IOReturn
	LocalUnitDirectory::Publish()
	{
		if (mPublished)
			return kIOReturnSuccess ;
	
		IOReturn err = IOConnectMethodScalarIScalarO(   mUserClient.GetUserClientConnection(), 
														kLocalConfigDirectory_Publish, 
														1, 0, mKernUnitDirRef ) ;
	
		mPublished = ( kIOReturnSuccess == err ) ;
	
		return err ;
	}
	
	IOReturn
	LocalUnitDirectory::Unpublish()
	{
		if (!mPublished)
			return kIOReturnSuccess ;
			
		IOReturn err = IOConnectMethodScalarIScalarO(   mUserClient.GetUserClientConnection(), 
														kLocalConfigDirectory_Unpublish, 1, 0, mKernUnitDirRef) ;
			
		mPublished = (!err) ;
		
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
