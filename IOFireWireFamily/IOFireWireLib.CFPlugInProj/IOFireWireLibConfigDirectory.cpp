/*
 *  IOFireWireConfigDirectory.cpp
 *  IOFireWireFamily
 *
 *  Created by NWG on Thu Jan 18 2001.
 *  Copyright (c) 2000 Apple, Inc. All rights reserved.
 *
 */

#import "IOFireWireLibConfigDirectory.h"

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
	
	ConfigDirectory::ConfigDirectory( IUnknownVTbl* interface, Device& userclient, FWKernConfigDirectoryRef inKernConfigDirectoryRef)
	: IOFireWireIUnknown( interface ),
	mUserClient( userclient ),
	mKernConfigDirectoryRef( inKernConfigDirectoryRef )
	{
		mUserClient.AddRef() ;
	}
	
	ConfigDirectory::ConfigDirectory( IUnknownVTbl* interface, Device& userclient )
	: IOFireWireIUnknown( interface ),
	mUserClient( userclient )
	{	
		IOReturn err = IOConnectMethodScalarIScalarO(mUserClient.GetUserClientConnection(), kFWConfigDirectoryCreate, 0, 1, & mKernConfigDirectoryRef) ;
		if (err)
			throw ;//Exception::IOReturn(err) ;	//!!!

		mUserClient.AddRef() ;
	}
		
	ConfigDirectory::~ConfigDirectory()
	{
		IOReturn err = IOConnectMethodScalarIScalarO(
					mUserClient.GetUserClientConnection(),
					kFWConfigDirectoryRelease,
					1,
					0,
					mKernConfigDirectoryRef ) ;
		IOFireWireLibLogIfErr_(err, "%s %u: release config dir failed err=%x\n", __FILE__, __LINE__, err) ;
	
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
		return IOConnectMethodScalarIScalarO(
					mUserClient.GetUserClientConnection(),
					kFWConfigDirectoryGetKeyType,
					1,
					1,
					key,
					& type) ;
	}
	
	IOReturn
	ConfigDirectory::GetKeyValue(int key, UInt32 &value, CFStringRef*& text)
	{
		FWKernOSStringRef	kernelStringRef ;
		UInt32				stringLen ;
		IOReturn result = ::IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(),
				kFWConfigDirectoryGetKeyValue_UInt32, 3, 3, mKernConfigDirectoryRef, key, text != nil,
				& value, & kernelStringRef, & stringLen) ;
	
		if (text && (kIOReturnSuccess == result))
			result = mUserClient.CreateCFStringWithOSStringRef(kernelStringRef, stringLen, text) ;
		
		return result ;
	}
		
	IOReturn
	ConfigDirectory::GetKeyValue(int key, CFDataRef* value, CFStringRef*& text)
	{
		FWGetKeyValueDataResults results ;
		IOByteCount	resultsSize = sizeof(results) ;
		
		IOReturn error = IOConnectMethodScalarIStructureO( mUserClient.GetUserClientConnection(), 
				kFWConfigDirectoryGetKeyValue_Data, 3, & resultsSize, mKernConfigDirectoryRef, key, text != nil,
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
		FWKernOSStringRef			kernelStringRef ;
		FWKernConfigDirectoryRef	kernelDirectoryRef ;
		UInt32						stringLen ;
		IOReturn result = IOConnectMethodScalarIScalarO( mUserClient.GetUserClientConnection(), kFWConfigDirectoryGetKeyValue_ConfigDirectory,
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
	ConfigDirectory::GetKeyOffset(int key, FWAddress& value, CFStringRef*& text)
	{
		FWGetKeyOffsetResults results ;
		IOByteCount resultsSize = sizeof(results) ;

		IOReturn error = IOConnectMethodScalarIStructureO( mUserClient.GetUserClientConnection(), kFWConfigDirectoryGetKeyOffset_FWAddress,
				3, & resultsSize, mKernConfigDirectoryRef, key, text != nil, & results ) ;
		
		value = results.address ;

		if (text && (kIOReturnSuccess == error))
			error = mUserClient.CreateCFStringWithOSStringRef( results.text, results.length, text ) ;
	
		return error ;
	}
	
	IOReturn
	ConfigDirectory::GetKeyValue(int key, FWKernConfigDirectoryRef& value)
	{
		FWKernOSStringRef			kernelStringRef ;
		FWKernConfigDirectoryRef	kernelDirectoryRef ;
		UInt32						stringLen ;
		IOReturn result = IOConnectMethodScalarIScalarO(
								mUserClient.GetUserClientConnection(),
								kFWConfigDirectoryGetKeyValue_ConfigDirectory,
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
		return IOConnectMethodScalarIScalarO(
					mUserClient.GetUserClientConnection(),
					kFWConfigDirectoryGetIndexType,
					2,
					1,
					mKernConfigDirectoryRef,
					index,
					& type) ;
	}
	
	IOReturn
	ConfigDirectory::GetIndexKey(int index, int &key)
	{
		return IOConnectMethodScalarIScalarO(
					mUserClient.GetUserClientConnection(),
					kFWConfigDirectoryGetIndexKey,
					2,
					1,
					mKernConfigDirectoryRef,
					index,
					& key) ;
	}
	
	IOReturn
	ConfigDirectory::GetIndexValue(int index, UInt32& value)
	{
		return IOConnectMethodScalarIScalarO(
					mUserClient.GetUserClientConnection(),
					kFWConfigDirectoryGetIndexValue_UInt32,
					2,
					1,
					mKernConfigDirectoryRef,
					index,
					& value) ;
	}
	
		
	IOReturn
	ConfigDirectory::GetIndexValue(int index, CFDataRef* value)
	{
		FWKernOSDataRef	dataRef ;
		IOByteCount		dataLen ;
		IOReturn result = IOConnectMethodScalarIScalarO(
								mUserClient.GetUserClientConnection(),
								kFWConfigDirectoryGetIndexValue_Data,
								2,
								2,
								mKernConfigDirectoryRef,
								index,
								& dataRef,
								& dataLen) ;
	
		if (kIOReturnSuccess == result)
			result = mUserClient.CreateCFDataWithOSDataRef(dataRef, dataLen, value) ;
	
		return result ;
	}
		
	IOReturn
	ConfigDirectory::GetIndexValue(int index, CFStringRef* value)
	{
		FWKernOSStringRef	stringRef ;
		UInt32				stringLen ;
		IOReturn result = IOConnectMethodScalarIScalarO(
								mUserClient.GetUserClientConnection(),
								kFWConfigDirectoryGetIndexValue_String,
								2,
								2,
								mKernConfigDirectoryRef,
								index,
								& stringRef,
								& stringLen) ;
	
		if (kIOReturnSuccess == result)
			result = mUserClient.CreateCFStringWithOSStringRef(stringRef, stringLen, value) ;
		
		return result ;
	}
	
	IOReturn
	ConfigDirectory::GetIndexValue(int index, FWKernConfigDirectoryRef& value)
	{
		return IOConnectMethodScalarIScalarO(
						mUserClient.GetUserClientConnection(),
						kFWConfigDirectoryGetIndexValue_ConfigDirectory,
						2,
						1,
						mKernConfigDirectoryRef,
						index,
						& value) ;
	}
	
	IOReturn
	ConfigDirectory::GetIndexValue(int index, IOFireWireLibConfigDirectoryRef& value, REFIID iid)
	{
		FWKernConfigDirectoryRef	directoryRef ;
		IOReturn result = IOConnectMethodScalarIScalarO(mUserClient.GetUserClientConnection(), kFWConfigDirectoryGetIndexValue_ConfigDirectory,
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
		UInt32 addressHi ;
		UInt32 addressLo ;
		IOReturn result = IOConnectMethodScalarIScalarO(mUserClient.GetUserClientConnection(), kFWConfigDirectoryGetIndexOffset_FWAddress, 2, 2, 
								mKernConfigDirectoryRef, index, & addressHi, & addressLo) ;
		
		value.nodeID = addressHi>>16 ;
		value.addressHi = addressHi ;
		value.addressLo = addressLo ;
		
		return result ;
	}
		
	IOReturn
	ConfigDirectory::GetIndexOffset(int index, UInt32& value)
	{
		return IOConnectMethodScalarIScalarO(mUserClient.GetUserClientConnection(), kFWConfigDirectoryGetIndexOffset_UInt32, 2, 1,
						mKernConfigDirectoryRef, index, & value) ;
	}
	
	IOReturn
	ConfigDirectory::GetIndexEntry(int index, UInt32 &value)
	{
		return IOConnectMethodScalarIScalarO(mUserClient.GetUserClientConnection(), kFWConfigDirectoryGetIndexEntry, 2, 1, 
						mKernConfigDirectoryRef, index, & value) ;
	}
	
	IOReturn
	ConfigDirectory::GetSubdirectories(io_iterator_t *outIterator)
	{
		return IOConnectMethodScalarIScalarO(mUserClient.GetUserClientConnection(), kFWConfigDirectoryGetSubdirectories, 1, 1,
						mKernConfigDirectoryRef, outIterator) ;
	}
	
	IOReturn
	ConfigDirectory::GetKeySubdirectories(int key, io_iterator_t *outIterator)
	{
		return IOConnectMethodScalarIScalarO(mUserClient.GetUserClientConnection(), kFWConfigDirectoryGetKeySubdirectories, 2, 1,
						mKernConfigDirectoryRef, key, outIterator) ;
	}
	
	IOReturn
	ConfigDirectory::GetType(int *outType)
	{
		return IOConnectMethodScalarIScalarO(mUserClient.GetUserClientConnection(), kFWConfigDirectoryGetType, 1, 1, mKernConfigDirectoryRef,
						outType) ;
	}
	
	IOReturn 
	ConfigDirectory::GetNumEntries(int *outNumEntries)
	{
		return IOConnectMethodScalarIScalarO(mUserClient.GetUserClientConnection(), kFWConfigDirectoryGetNumEntries, 1, 1,
						mKernConfigDirectoryRef, outNumEntries) ;
	}
	
	// ============================================================
	//
	// ConfigDirectoryCOM
	//
	// ============================================================
	
#pragma mark -
	ConfigDirectoryCOM::ConfigDirectoryCOM( Device& inUserClient, FWKernConfigDirectoryRef inDirRef )
	: ConfigDirectory( reinterpret_cast<IUnknownVTbl*>( & sInterface ), inUserClient, inDirRef)
	{
	}
	
	ConfigDirectoryCOM::ConfigDirectoryCOM( Device& inUserClient )
	: ConfigDirectory( reinterpret_cast<IUnknownVTbl*>( & sInterface ), inUserClient)
	{
	}
	
	ConfigDirectoryCOM::~ConfigDirectoryCOM()
	{
	}
	
	IUnknownVTbl**
	ConfigDirectoryCOM::Alloc( Device& inUserClient, FWKernConfigDirectoryRef inDirRef )
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
