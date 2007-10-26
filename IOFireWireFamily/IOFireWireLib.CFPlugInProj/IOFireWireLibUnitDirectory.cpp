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
#import "IOFireWireLibDevice.h"

#import <System/libkern/OSCrossEndian.h>

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
		uint32_t outputCnt = 1;
		uint64_t outputVal = 0;
		IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
								  kLocalConfigDirectory_Create,
								  NULL,0,&outputVal,&outputCnt);
		mKernUnitDirRef = (UserObjectHandle) outputVal;
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
		
		uint32_t outputCnt = 0;
		const uint64_t inputs[6] = {(const uint64_t)mKernUnitDirRef, key, (const uint64_t)buffer,len, (const uint64_t)descCString, descLen};
		return IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), kLocalConfigDirectory_AddEntry_Buffer,
										 inputs,6,
										 NULL,&outputCnt);
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
		
		uint32_t outputCnt = 0;
		const uint64_t inputs[5] = {(const uint64_t)mKernUnitDirRef, key, value, (const uint64_t)descCString, descLen};
		IOReturn err = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), kLocalConfigDirectory_AddEntry_UInt32,
												 inputs,5,
												 NULL,&outputCnt);
		
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

		FWAddress host_value(value);
		
#ifndef __LP64__		
		ROSETTA_ONLY(
			{
				host_value.nodeID = OSSwapInt16( value.nodeID );
				host_value.addressHi = OSSwapInt16( value.addressHi );
				host_value.addressLo = OSSwapInt32( value.addressLo );
			}
		);
#endif		
		uint32_t outputCnt = 0;
		size_t outputStructSize =  0 ;
		const uint64_t inputs[4] = {(const uint64_t)mKernUnitDirRef, key, (const uint64_t)descCString,descLen};
		return IOConnectCallMethod(mUserClient.GetUserClientConnection(), 
								   kLocalConfigDirectory_AddEntry_FWAddr,
								   inputs,4,
								   &host_value,sizeof(value),
								   NULL,&outputCnt,
								   NULL,&outputStructSize);
	}
	
	IOReturn
	LocalUnitDirectory::Publish()
	{
		if (mPublished)
			return kIOReturnSuccess ;
	
		uint32_t outputCnt = 0;
		const uint64_t inputs[1]={(const uint64_t)mKernUnitDirRef};

		IOReturn err = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
												 kLocalConfigDirectory_Publish,
												 inputs,1,
												 NULL,&outputCnt);
		
		mPublished = ( kIOReturnSuccess == err ) ;
	
		return err ;
	}
	
	IOReturn
	LocalUnitDirectory::Unpublish()
	{
		if (!mPublished)
			return kIOReturnSuccess ;

		uint32_t outputCnt = 0;
		const uint64_t inputs[1]={(const uint64_t)mKernUnitDirRef};
		IOReturn err = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
												 kLocalConfigDirectory_Unpublish,
												 inputs,1,
												 NULL,&outputCnt);
		
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
