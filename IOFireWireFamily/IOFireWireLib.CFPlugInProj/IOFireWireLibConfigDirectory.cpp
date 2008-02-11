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

#import <System/libkern/OSCrossEndian.h>

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
		uint32_t outputCnt = 1;
		uint64_t outputVal = 0;
		IOReturn err = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
												 kConfigDirectory_Create,
												 NULL,0,
												 &outputVal,&outputCnt);
		mKernConfigDirectoryRef = (UserObjectHandle) outputVal;
		if (err)
			throw err ;

		mUserClient.AddRef() ;
	}
		
	ConfigDirectory::~ConfigDirectory()
	{
		IOReturn error = kIOReturnSuccess;
		
		uint32_t outputCnt = 0;
		const uint64_t inputs[1]={(const uint64_t)mKernConfigDirectoryRef};
		error = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(),
													kReleaseUserObject,
													inputs,1,
													NULL,&outputCnt);
		
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
		uint32_t outputCnt = 1;
		uint64_t outputVal = 0;
		const uint64_t inputs[2] = {(const uint64_t)mKernConfigDirectoryRef, key};
		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetKeyType,
													inputs,2,
													&outputVal,&outputCnt);
		type = (IOConfigKeyType) (outputCnt & 0xFFFFFFFF);
		return result;
	}
	
	IOReturn
	ConfigDirectory::GetKeyValue(int key, UInt32 &value, CFStringRef*& text)
	{
		UserObjectHandle	kernelStringRef ;
		UInt32				stringLen ;
		
		uint32_t outputCnt = 3;
		uint64_t outputVal[3];
		const uint64_t inputs[3] = {(const uint64_t)mKernConfigDirectoryRef, key,(text != nil)};
		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetKeyValue_UInt32,
													inputs,3,
													outputVal,&outputCnt);
		value = outputVal[0] & 0xFFFFFFFF;
		kernelStringRef = (UserObjectHandle) outputVal[1];
		stringLen = outputVal[2] & 0xFFFFFFFF;
		
		if (text && (kIOReturnSuccess == result))
			result = mUserClient.CreateCFStringWithOSStringRef(kernelStringRef, stringLen, text) ;
		
		return result ;
	}
		
	IOReturn
	ConfigDirectory::GetKeyValue(int key, CFDataRef* value, CFStringRef*& text)
	{
		GetKeyValueDataResults results ;
		
		uint32_t outputCnt = 0;
		size_t outputStructSize = sizeof(results) ;
		const uint64_t inputs[3] = {(const uint64_t)mKernConfigDirectoryRef, key, (text != nil)};
		IOReturn error = IOConnectCallMethod(mUserClient.GetUserClientConnection(), 
											 kConfigDirectory_GetKeyValue_Data,
											 inputs,3,
											 NULL,0,
											 NULL,&outputCnt,
											 & results,&outputStructSize);
		
		ROSETTA_ONLY(
			{
				results.data = (UserObjectHandle)OSSwapInt32( (UInt32)results.data );
				results.dataLength = OSSwapInt32( results.dataLength );
				results.textLength = OSSwapInt32( results.textLength );
			}
		);
			
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
		
		
		uint32_t outputCnt = 3;
		uint64_t outputVal[3];
		const uint64_t inputs[3] = {(const uint64_t)mKernConfigDirectoryRef, key,(text != nil)};
		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetKeyValue_ConfigDirectory,
													inputs,3,
													outputVal,&outputCnt);
		kernelDirectoryRef =(UserObjectHandle) outputVal[0];
		kernelStringRef = (UserObjectHandle) outputVal[1];
		stringLen = outputVal[2] & 0xFFFFFFFF;
	
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
	ConfigDirectory::GetKeyOffset (
		int					key, 
		FWAddress &			value, 
		CFStringRef * &		text)
	{
		GetKeyOffsetResults results ;
		uint32_t outputCnt = 0;
		size_t outputStructSize = sizeof(results) ;
		const uint64_t inputs[3] = {(const uint64_t)mKernConfigDirectoryRef, key, (text != nil)};
		IOReturn error = IOConnectCallMethod(mUserClient.GetUserClientConnection(), 
											 kConfigDirectory_GetKeyOffset_FWAddress,
											 inputs,3,
											 NULL,0,
											 NULL,&outputCnt,
											 & results,&outputStructSize);
		ROSETTA_ONLY(
			{
				results.address.nodeID = OSSwapInt16( results.address.nodeID );
				results.address.addressHi = OSSwapInt16( results.address.addressHi );
				results.address.addressLo = OSSwapInt32( results.address.addressLo );
				results.length = OSSwapInt32( results.length );
			}
		);
		
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
		
		
		uint32_t outputCnt = 3;
		uint64_t outputVal[3];
		const uint64_t inputs[3] = {(const uint64_t)mKernConfigDirectoryRef, key,false};
		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetKeyValue_ConfigDirectory,
													inputs,3,
													outputVal,&outputCnt);
		kernelDirectoryRef =(UserObjectHandle) outputVal[0];
		kernelStringRef = (UserObjectHandle) outputVal[1];
		stringLen = outputVal[2] & 0xFFFFFFFF;
		return result ;
	}
	
	IOReturn
	ConfigDirectory::GetIndexType(int index, IOConfigKeyType &type)
	{
		uint32_t outputCnt = 1;
		uint64_t outputVal;
		const uint64_t inputs[2] = {(const uint64_t)mKernConfigDirectoryRef, index};
		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetIndexType,
													inputs,2,
													&outputVal,&outputCnt);
		type = (IOConfigKeyType)(outputVal & 0xFFFFFFFF);
		return result;
	}
	
	IOReturn
	ConfigDirectory::GetIndexKey(int index, int &key)
	{
		uint32_t outputCnt = 1;
		uint64_t outputVal;
		const uint64_t inputs[2] = {(const uint64_t)mKernConfigDirectoryRef, index};
		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetIndexKey,
													inputs,2,
													&outputVal,&outputCnt);
		key = outputVal & 0xFFFFFFFF;
		return result;
	}
	
	IOReturn
	ConfigDirectory::GetIndexValue(int index, UInt32& value)
	{
		uint32_t outputCnt = 1;
		uint64_t outputVal;
		const uint64_t inputs[2] = {(const uint64_t)mKernConfigDirectoryRef, index};
		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetIndexValue_UInt32,
													inputs,2,
													&outputVal,&outputCnt);
		value = outputVal & 0xFFFFFFFF;
		return result;
	}
	
		
	IOReturn
	ConfigDirectory::GetIndexValue(int index, CFDataRef* value)
	{
		UserObjectHandle	dataRef ;
		IOByteCount		dataLen ;
		
		uint32_t outputCnt = 2;
		uint64_t outputVal[2];
		const uint64_t inputs[2] = {(const uint64_t)mKernConfigDirectoryRef, index};
		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetIndexValue_Data,
													inputs,2,
													outputVal,&outputCnt);
		dataRef = (UserObjectHandle) outputVal[0];
		dataLen = outputVal[1] & 0xFFFFFFFF;
		
		if (kIOReturnSuccess == result)
			result = mUserClient.CreateCFDataWithOSDataRef(dataRef, dataLen, value) ;
	
		return result ;
	}
		
	IOReturn
	ConfigDirectory::GetIndexValue(int index, CFStringRef* value)
	{
		UserObjectHandle		stringRef ;
		UInt32				stringLen ;
		
		uint32_t outputCnt = 2;
		uint64_t outputVal[2];
		const uint64_t inputs[2] = {(const uint64_t)mKernConfigDirectoryRef, index};
		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetIndexValue_String,
													inputs,2,
													outputVal,&outputCnt);
		stringRef = (UserObjectHandle) outputVal[0];
		stringLen = outputVal[1] & 0xFFFFFFFF;
		
		if (kIOReturnSuccess == result)
			result = mUserClient.CreateCFStringWithOSStringRef(stringRef, stringLen, value) ;
		
		return result ;
	}

#if 0	
	IOReturn
	ConfigDirectory::GetIndexValue(int index, UserObjectHandle& value)
	{
		uint32_t outputCnt = 1;
		uint64_t outputVal;
		const uint64_t inputs[2] = {(const uint64_t)mKernConfigDirectoryRef, index};
		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetIndexValue_ConfigDirectory,
													inputs,2,
													&outputVal,&outputCnt);
		value = (UserObjectHandle) outputVal;
		return result;
	}
#endif
	
	IOReturn
	ConfigDirectory::GetIndexValue (
		int						index, 
		IOFireWireLibConfigDirectoryRef & value, 
		REFIID					iid)
	{
		UserObjectHandle	directoryRef ;
		
		uint32_t outputCnt = 1;
		uint64_t outputVal;
		const uint64_t inputs[2] = {(const uint64_t)mKernConfigDirectoryRef, index};
		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetIndexValue_ConfigDirectory,
													inputs,2,
													&outputVal,&outputCnt);
		directoryRef = (UserObjectHandle) outputVal;
		
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
		
		uint32_t outputCnt = 0;
		size_t outputStructSize = sizeof(value) ;
		const uint64_t inputs[2] = {(const uint64_t)mKernConfigDirectoryRef, index};
		IOReturn result = IOConnectCallMethod(mUserClient.GetUserClientConnection(), 
											 kConfigDirectory_GetIndexOffset_FWAddress,
											 inputs,2,
											 NULL,0,
											 NULL,&outputCnt,
											 & value,&outputStructSize);
		ROSETTA_ONLY(
			{
				value.nodeID = OSSwapInt16( value.nodeID );
				value.addressHi = OSSwapInt16( value.addressHi );
				value.addressLo = OSSwapInt32( value.addressLo );
			}
		);
		
		return result ;
	}
		
	IOReturn
	ConfigDirectory::GetIndexOffset(int index, UInt32& value)
	{
		uint32_t outputCnt = 1;
		uint64_t outputVal;
		const uint64_t inputs[2] = {(const uint64_t)mKernConfigDirectoryRef, index};
		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetIndexOffset_UInt32,
													inputs,2,
													&outputVal,&outputCnt);
		value = outputVal & 0xFFFFFFFF;
		return result;
	}
	
	IOReturn
	ConfigDirectory::GetIndexEntry(int index, UInt32 &value)
	{
		uint32_t outputCnt = 1;
		uint64_t outputVal;
		const uint64_t inputs[2] = {(const uint64_t)mKernConfigDirectoryRef, index};
		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetIndexEntry,
													inputs,2,
													&outputVal,&outputCnt);
		value = outputVal & 0xFFFFFFFF;
		return result;
	}
	
	IOReturn
	ConfigDirectory::GetSubdirectories(io_iterator_t *outIterator)
	{
		uint32_t outputCnt = 1;
		uint64_t outputVal;
		const uint64_t inputs[1]={(const uint64_t)mKernConfigDirectoryRef};

		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetSubdirectories,
													inputs,1,
													&outputVal,&outputCnt);
		*outIterator = (io_iterator_t) outputVal;
		return result;
	}
	
	IOReturn
	ConfigDirectory::GetKeySubdirectories(int key, io_iterator_t *outIterator)
	{
		uint32_t outputCnt = 1;
		uint64_t outputVal;
		const uint64_t inputs[2] = {(const uint64_t)mKernConfigDirectoryRef, key};
		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetKeySubdirectories,
													inputs,2,
													&outputVal,&outputCnt);
		*outIterator = (io_iterator_t) outputVal;
		return result;
	}
	
	IOReturn
	ConfigDirectory::GetType(int *outType)
	{
		uint32_t outputCnt = 1;
		uint64_t outputVal;
		const uint64_t inputs[1]={(const uint64_t)mKernConfigDirectoryRef};
		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetType,
													inputs,1,
													&outputVal,&outputCnt);
		*outType = outputVal & 0xFFFFFFFF;
		return result;
	}
	
	IOReturn 
	ConfigDirectory::GetNumEntries(int *outNumEntries)
	{
		uint32_t outputCnt = 1;
		uint64_t outputVal;
		const uint64_t inputs[1]={(const uint64_t)mKernConfigDirectoryRef};
		IOReturn result = IOConnectCallScalarMethod(mUserClient.GetUserClientConnection(), 
													kConfigDirectory_GetNumEntries,
													inputs,1,
													&outputVal,&outputCnt);
		*outNumEntries = outputVal & 0xFFFFFFFF;
		return result;
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
