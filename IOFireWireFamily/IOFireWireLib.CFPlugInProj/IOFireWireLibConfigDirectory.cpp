/*
 *  IOFireWireConfigDirectory.cpp
 *  IOFireWireFamily
 *
 *  Created by NWG on Thu Jan 18 2001.
 *  Copyright (c) 2000 Apple, Inc. All rights reserved.
 *
 */
 

#import "IOFireWireLibConfigDirectory.h"
#import "IOFireWireLibDevice.h"

namespace IOFireWireLib {

	ConfigDirectoryCOM::Interface 	ConfigDirectoryCOM::sInterface = {
		INTERFACEIMP_INTERFACE,
		1, 0, //vers, rev
		& ConfigDirectoryCOM::SUpdate,
		& ConfigDirectoryCOM::SGetKeyType,
		& ConfigDirectoryCOM::SGetKeyValue_UInt32,
		& ConfigDirectoryCOM::SGetKeyValue_Data,
		& ConfigDirectoryCOM::SGetKeyValue_ConfigDirectory,
		& ConfigDirectoryCOM::SGetKeyOffset_FWAddress,
		& ConfigDirectoryCOM::SGetIndexType,
		& ConfigDirectoryCOM::SGetIndexKey,
		& ConfigDirectoryCOM::SGetIndexValue_UInt32,
		& ConfigDirectoryCOM::SGetIndexValue_Data,
		& ConfigDirectoryCOM::SGetIndexValue_String,
		& ConfigDirectoryCOM::SGetIndexValue_ConfigDirectory,
		& ConfigDirectoryCOM::SGetIndexOffset_FWAddress,
		& ConfigDirectoryCOM::SGetIndexOffset_UInt32,
		& ConfigDirectoryCOM::SGetIndexEntry,
		& ConfigDirectoryCOM::SGetSubdirectories,
		& ConfigDirectoryCOM::SGetKeySubdirectories,
		& ConfigDirectoryCOM::SGetType,
		& ConfigDirectoryCOM::SGetNumEntries
	} ;				
	
	
	// ============================================================
	//
	// ConfigDirectory
	//
	// ============================================================
	
	ConfigDirectory::ConfigDirectory( const IUnknownVTbl & interface, Device& userclient, UserObjectHandle inKernConfigDirectoryRef)
	: IOFireWireIUnknown( interface ),
	  mUserClient( userclient ),
	  mKernConfigDirectoryRef( inKernConfigDirectoryRef )
	{
		mUserClient.AddRef() ;
	}
	
	ConfigDirectory::ConfigDirectory( const IUnknownVTbl & interface, Device& userclient )
	: IOFireWireIUnknown( interface ),
	  mUserClient( userclient )
	{	
		IOReturn err = ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), 
														kConfigDirectory_Create, 
														0, 1, & mKernConfigDirectoryRef) ;
		if (err)
			throw err ;

		mUserClient.AddRef() ;
	}
		
	ConfigDirectory::~ConfigDirectory()
	{
		IOReturn error = kIOReturnSuccess;
		
		error = IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(),
											   kReleaseUserObject, 1, 0, mKernConfigDirectoryRef ) ;

		DebugLogCond( error, "release config dir failed err=%x\n", error ) ;
	
		mUserClient.Release() ;
	}
		
	IOReturn
	ConfigDirectory::Update(UInt32 offset)
	{
		return kIOReturnUnsupported ;
	}
	
	IOReturn
	ConfigDirectory::GetKeyType(int key, IOConfigKeyType& type)
	{
		return IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kConfigDirectory_GetKeyType,
					2, 1, mKernConfigDirectoryRef, key, & type) ;
	}
	
	IOReturn
	ConfigDirectory::GetKeyValue(int key, UInt32 &value, CFStringRef*& text)
	{
		UserObjectHandle	kernelStringRef ;
		UInt32				stringLen ;
		IOReturn result = ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(),
				kConfigDirectory_GetKeyValue_UInt32, 3, 3, mKernConfigDirectoryRef, key, text != nil,
				& value, & kernelStringRef, & stringLen) ;
	
		if (text && (kIOReturnSuccess == result))
			result = mUserClient.CreateCFStringWithOSStringRef(kernelStringRef, stringLen, text) ;
		
		return result ;
	}
		
	IOReturn
	ConfigDirectory::GetKeyValue(int key, CFDataRef* value, CFStringRef*& text)
	{
		GetKeyValueDataResults results ;
		IOByteCount	resultsSize = sizeof(results) ;
		
		IOReturn error = IOConnectMethodScalarIStructureO( mUserClient.GetUserClientConnection(), 
				kConfigDirectory_GetKeyValue_Data, 3, & resultsSize, mKernConfigDirectoryRef, key, text != nil,
				& results ) ;
	
		if ( text && (kIOReturnSuccess == error))
			error = mUserClient.CreateCFStringWithOSStringRef( results.text, results.textLength, text ) ;
		if ( kIOReturnSuccess == error )
			error = mUserClient.CreateCFDataWithOSDataRef( results.data, results.dataLength, value ) ;
		
		return error ;
	}
		
	IOReturn
	ConfigDirectory::GetKeyValue(int key, DirRef& value, REFIID iid, CFStringRef*& text)
	{
		UserObjectHandle			kernelStringRef ;
		UserObjectHandle	kernelDirectoryRef ;
		UInt32						stringLen ;
		IOReturn result = IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kConfigDirectory_GetKeyValue_ConfigDirectory,
				3, 3, mKernConfigDirectoryRef, key, text != nil, & kernelDirectoryRef, & kernelStringRef, & stringLen ) ;
	
		IUnknownVTbl**	iUnknown = nil ;
		if (kIOReturnSuccess == result)
		{
			iUnknown = reinterpret_cast<IUnknownVTbl**>(ConfigDirectoryCOM::Alloc(mUserClient, kernelDirectoryRef)) ;
		
			if (!iUnknown)
				result = kIOReturnNoMemory ;
			else
			{
				if (S_OK != (*iUnknown)->QueryInterface(iUnknown, iid, (void**) & value))
					result = kIOReturnError ;
				
				(*iUnknown)->Release(iUnknown) ;
			}
		
			if (text && (kIOReturnSuccess == result))
				result = mUserClient.CreateCFStringWithOSStringRef(kernelStringRef, stringLen, text) ;
		}
		
		return result ;
	}
	
	IOReturn
	ConfigDirectory :: GetKeyOffset (
		int					key, 
		FWAddress &			value, 
		CFStringRef * &		text)
	{
		GetKeyOffsetResults results ;
		IOByteCount resultsSize = sizeof(results) ;

		IOReturn error = ::IOConnectMethodScalarIStructureO(	mUserClient.GetUserClientConnection(), 
																kConfigDirectory_GetKeyOffset_FWAddress,
																3, & resultsSize, mKernConfigDirectoryRef, key, text != nil, & results ) ;
		
		value = results.address ;

		if (text && (kIOReturnSuccess == error))
			error = mUserClient.CreateCFStringWithOSStringRef( results.text, results.length, text ) ;
	
		return error ;
	}
	
	IOReturn
	ConfigDirectory::GetKeyValue(int key, UserObjectHandle& value)
	{
		UserObjectHandle			kernelStringRef ;
		UserObjectHandle	kernelDirectoryRef ;
		UInt32						stringLen ;
		IOReturn result = IOConnectMethodScalarIScalarO (
								mUserClient.GetUserClientConnection(),
								kConfigDirectory_GetKeyValue_ConfigDirectory,
								3,
								3,
								mKernConfigDirectoryRef,
								key,
								(UInt32)false,
								& kernelDirectoryRef,
								& kernelStringRef,
								& stringLen) ;
		
		return result ;
	}
	
	IOReturn
	ConfigDirectory::GetIndexType(int index, IOConfigKeyType &type)
	{
		return ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kConfigDirectory_GetIndexType,
					2, 1, mKernConfigDirectoryRef, index, & type) ;
	}
	
	IOReturn
	ConfigDirectory::GetIndexKey(int index, int &key)
	{
		return ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kConfigDirectory_GetIndexKey,
					2, 1, mKernConfigDirectoryRef, index, & key) ;
	}
	
	IOReturn
	ConfigDirectory::GetIndexValue(int index, UInt32& value)
	{
		return ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), 
				kConfigDirectory_GetIndexValue_UInt32, 2, 1, mKernConfigDirectoryRef, index, & value) ;
	}
	
		
	IOReturn
	ConfigDirectory::GetIndexValue(int index, CFDataRef* value)
	{
		UserObjectHandle	dataRef ;
		IOByteCount		dataLen ;
		IOReturn result = ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(),
								kConfigDirectory_GetIndexValue_Data, 2, 2, mKernConfigDirectoryRef, index, 
								& dataRef, & dataLen) ;
	
		if (kIOReturnSuccess == result)
			result = mUserClient.CreateCFDataWithOSDataRef(dataRef, dataLen, value) ;
	
		return result ;
	}
		
	IOReturn
	ConfigDirectory::GetIndexValue(int index, CFStringRef* value)
	{
		UserObjectHandle		stringRef ;
		UInt32				stringLen ;
		IOReturn result = ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(),
								kConfigDirectory_GetIndexValue_String, 2, 2, mKernConfigDirectoryRef,
								index, & stringRef, & stringLen) ;
	
		if (kIOReturnSuccess == result)
			result = mUserClient.CreateCFStringWithOSStringRef(stringRef, stringLen, value) ;
		
		return result ;
	}
	
	IOReturn
	ConfigDirectory::GetIndexValue(int index, UserObjectHandle& value)
	{
		return ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), 
				kConfigDirectory_GetIndexValue_ConfigDirectory, 2, 1, mKernConfigDirectoryRef, index, & value) ;
	}
	
	IOReturn
	ConfigDirectory :: GetIndexValue (
		int						index, 
		IOFireWireLibConfigDirectoryRef & value, 
		REFIID					iid)
	{
		UserObjectHandle	directoryRef ;
		IOReturn result = IOConnectMethodScalarIScalarO(	mUserClient.GetUserClientConnection(), 
															kConfigDirectory_GetIndexValue_ConfigDirectory, 
															2, 1, mKernConfigDirectoryRef, index, & directoryRef) ;
	
		IUnknownVTbl** iUnknown ;
		if (kIOReturnSuccess == result)
		{
			iUnknown = reinterpret_cast<IUnknownVTbl**>(ConfigDirectoryCOM::Alloc(mUserClient, directoryRef)) ;
	
			if (!iUnknown)
				result = kIOReturnNoMemory ;
			else
			{
				if (S_OK != (*iUnknown)->QueryInterface(iUnknown, iid, (void**) & value))
					result = kIOReturnError ;
				
				(*iUnknown)->Release(iUnknown) ;
			}
		}
		
		return result ;
	}
	
	IOReturn
	ConfigDirectory::GetIndexOffset(int index, FWAddress& value)
	{
		IOByteCount resultsSize = sizeof(value) ;
		IOReturn result = IOConnectMethodScalarIStructureO( mUserClient.GetUserClientConnection(), 
															kConfigDirectory_GetIndexOffset_FWAddress, 
															2, &resultsSize, mKernConfigDirectoryRef, index, &value) ;
		
		return result ;
	}
		
	IOReturn
	ConfigDirectory::GetIndexOffset(int index, UInt32& value)
	{
		return IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kConfigDirectory_GetIndexOffset_UInt32, 2, 1,
						mKernConfigDirectoryRef, index, & value) ;
	}
	
	IOReturn
	ConfigDirectory::GetIndexEntry(int index, UInt32 &value)
	{
		return IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kConfigDirectory_GetIndexEntry, 2, 1, 
						mKernConfigDirectoryRef, index, & value) ;
	}
	
	IOReturn
	ConfigDirectory::GetSubdirectories(io_iterator_t *outIterator)
	{
		return IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kConfigDirectory_GetSubdirectories, 1, 1,
						mKernConfigDirectoryRef, outIterator) ;
	}
	
	IOReturn
	ConfigDirectory::GetKeySubdirectories(int key, io_iterator_t *outIterator)
	{
		return IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kConfigDirectory_GetKeySubdirectories, 2, 1,
						mKernConfigDirectoryRef, key, outIterator) ;
	}
	
	IOReturn
	ConfigDirectory::GetType(int *outType)
	{
		return IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kConfigDirectory_GetType, 1, 1, mKernConfigDirectoryRef,
						outType) ;
	}
	
	IOReturn 
	ConfigDirectory::GetNumEntries(int *outNumEntries)
	{
		return IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kConfigDirectory_GetNumEntries, 1, 1,
						mKernConfigDirectoryRef, outNumEntries) ;
	}
	
	// ============================================================
	//
	// ConfigDirectoryCOM
	//
	// ============================================================
	
#pragma mark -
	ConfigDirectoryCOM::ConfigDirectoryCOM( Device& inUserClient, UserObjectHandle inDirRef )
	: ConfigDirectory( reinterpret_cast<const IUnknownVTbl &>( sInterface ), inUserClient, inDirRef)
	{
	}
	
	ConfigDirectoryCOM::ConfigDirectoryCOM( Device& inUserClient )
	: ConfigDirectory( reinterpret_cast<const IUnknownVTbl &>( sInterface ), inUserClient)
	{
	}
	
	ConfigDirectoryCOM::~ConfigDirectoryCOM()
	{
	}
	
	IUnknownVTbl**
	ConfigDirectoryCOM::Alloc( Device& inUserClient, UserObjectHandle inDirRef )
	{
		ConfigDirectoryCOM*	me = nil ;
		
		try {
			me = new ConfigDirectoryCOM(inUserClient, inDirRef) ;
		} catch(...) {
		}
		
		return ( nil == me ) ? nil : reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	IUnknownVTbl**
	ConfigDirectoryCOM::Alloc(Device& inUserClient)
	{
		ConfigDirectoryCOM *	me = nil ;

		try {
			me = new ConfigDirectoryCOM(inUserClient) ;
		} catch (...) {
		}

		return ( nil == me ) ? nil : reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	HRESULT STDMETHODCALLTYPE
	ConfigDirectoryCOM::QueryInterface(REFIID iid, LPVOID* ppv)
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if ( CFEqual(interfaceID, IUnknownUUID) ||  CFEqual(interfaceID, kIOFireWireConfigDirectoryInterfaceID) )
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
	
	// static methods:
	
	IOReturn
	ConfigDirectoryCOM::SUpdate(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		UInt32 								inOffset)
	{
		return kIOReturnUnsupported ;
	}
	
	IOReturn
	ConfigDirectoryCOM::SGetKeyType(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int 								inKey, 
		IOConfigKeyType* 					outType)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetKeyType(inKey, *outType) ;
	}
	
	IOReturn
	ConfigDirectoryCOM::SGetKeyValue_UInt32(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int 								inKey, 
		UInt32*								outValue, 
		CFStringRef*						outText)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetKeyValue(inKey, * outValue, outText) ;
	}
		
	IOReturn
	ConfigDirectoryCOM::SGetKeyValue_Data(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int 								inKey, 
		CFDataRef *							outValue, 
		CFStringRef*						outText)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetKeyValue(inKey, outValue, outText) ;
	}
		
	IOReturn
	ConfigDirectoryCOM::SGetKeyValue_ConfigDirectory(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int 								inKey, 
		IOFireWireLibConfigDirectoryRef*	outValue,
		REFIID								iid,
		CFStringRef*						outText)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetKeyValue(inKey, * outValue, iid, outText) ;
	}
		
	IOReturn
	ConfigDirectoryCOM::SGetKeyOffset_FWAddress(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int 								inKey, 
		FWAddress*							outValue, 
		CFStringRef* 						text)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetKeyOffset(inKey, *outValue, text) ;
	}
		
	IOReturn
	ConfigDirectoryCOM::SGetIndexType(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int 								inIndex, 
		IOConfigKeyType*					outType)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetIndexType(inIndex, *outType) ;
	}
	
	IOReturn
	ConfigDirectoryCOM::SGetIndexKey(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int 								inIndex, 
		int *								outKey)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetIndexKey(inIndex, *outKey) ;
	}
	
	IOReturn
	ConfigDirectoryCOM::SGetIndexValue_UInt32(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int 								inIndex, 
		UInt32 *							outValue)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetIndexValue(inIndex, *outValue) ;
	}
	
	IOReturn
	ConfigDirectoryCOM::SGetIndexValue_Data(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int 								inIndex, 
		CFDataRef *							outValue)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetIndexValue(inIndex, outValue) ;
	}
		
	IOReturn
	ConfigDirectoryCOM::SGetIndexValue_String(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int 								inIndex, 
		CFStringRef*						outValue)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetIndexValue(inIndex, outValue) ;
	}
		
	IOReturn
	ConfigDirectoryCOM::SGetIndexValue_ConfigDirectory(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int 								inIndex, 
		IOFireWireLibConfigDirectoryRef*	outValue,
		REFIID								iid)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetIndexValue(inIndex, *outValue, iid) ;
	}
		
	IOReturn
	ConfigDirectoryCOM::SGetIndexOffset_FWAddress(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int 								inIndex, 
		FWAddress*							outValue)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetIndexOffset(inIndex, *outValue) ;
	}
	
	IOReturn
	ConfigDirectoryCOM::SGetIndexOffset_UInt32(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int 								inIndex, 
		UInt32*								outValue)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetIndexOffset(inIndex, *outValue) ;
	}
	
	IOReturn
	ConfigDirectoryCOM::SGetIndexEntry(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int 								inIndex, 
		UInt32*								outValue)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetIndexEntry(inIndex, *outValue) ;
	}
	
	IOReturn
	ConfigDirectoryCOM::SGetSubdirectories(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		io_iterator_t*						outIterator)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetSubdirectories(outIterator) ;
	}
	
	IOReturn
	ConfigDirectoryCOM::SGetKeySubdirectories(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int 								inKey, 
		io_iterator_t *						outIterator)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetKeySubdirectories(inKey, outIterator) ;
	}
	
	IOReturn
	ConfigDirectoryCOM::SGetType(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int *								outType)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetType(outType) ;
	}
	
	IOReturn
	ConfigDirectoryCOM::SGetNumEntries(
		IOFireWireLibConfigDirectoryRef 	inDir, 
		int *								outNumEntries)
	{
		return IOFireWireIUnknown::InterfaceMap<ConfigDirectoryCOM>::GetThis(inDir)->GetNumEntries(outNumEntries) ;
	}
}
