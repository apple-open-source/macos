/*
 *  IOFireWireConfigDirectory.cpp
 *  IOFireWireFamily
 *
 *  Created by NWG on Thu Jan 18 2001.
 *  Copyright (c) 2000 Apple, Inc. All rights reserved.
 *
 */

//#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>

#include "IOFireWireLibPriv.h"
#include "IOFireWireLibConfigDirectory.h"

IOFireWireConfigDirectoryInterface 
IOFireWireLibConfigDirectoryCOM::sInterface = {
	INTERFACEIMP_INTERFACE,
	1, 0, //vers, rev
	& IOFireWireLibConfigDirectoryCOM::SUpdate,
	& IOFireWireLibConfigDirectoryCOM::SGetKeyType,
	& IOFireWireLibConfigDirectoryCOM::SGetKeyValue_UInt32,
	& IOFireWireLibConfigDirectoryCOM::SGetKeyValue_Data,
	& IOFireWireLibConfigDirectoryCOM::SGetKeyValue_ConfigDirectory,
	& IOFireWireLibConfigDirectoryCOM::SGetKeyOffset_FWAddress,
	& IOFireWireLibConfigDirectoryCOM::SGetIndexType,
	& IOFireWireLibConfigDirectoryCOM::SGetIndexKey,
	& IOFireWireLibConfigDirectoryCOM::SGetIndexValue_UInt32,
	& IOFireWireLibConfigDirectoryCOM::SGetIndexValue_Data,
	& IOFireWireLibConfigDirectoryCOM::SGetIndexValue_String,
	& IOFireWireLibConfigDirectoryCOM::SGetIndexValue_ConfigDirectory,
	& IOFireWireLibConfigDirectoryCOM::SGetIndexOffset_FWAddress,
	& IOFireWireLibConfigDirectoryCOM::SGetIndexOffset_UInt32,
	& IOFireWireLibConfigDirectoryCOM::SGetIndexEntry,
	& IOFireWireLibConfigDirectoryCOM::SGetSubdirectories,
	& IOFireWireLibConfigDirectoryCOM::SGetKeySubdirectories,
	& IOFireWireLibConfigDirectoryCOM::SGetType,
	& IOFireWireLibConfigDirectoryCOM::SGetNumEntries
} ;				


// ============================================================
//
// IOFireWireLibConfigDirectoryImp
//
// ============================================================

IOFireWireLibConfigDirectoryImp::IOFireWireLibConfigDirectoryImp(
	IOFireWireDeviceInterfaceImp& 	inUserClient,
	FWKernConfigDirectoryRef	  	inKernConfigDirectoryRef): IOFireWireIUnknown(),
															   mUserClient(inUserClient),
															   mKernConfigDirectoryRef(inKernConfigDirectoryRef)
{
	mUserClient.AddRef() ;
}

IOFireWireLibConfigDirectoryImp::IOFireWireLibConfigDirectoryImp(
	IOFireWireDeviceInterfaceImp& inUserClient): IOFireWireIUnknown(),
												 mUserClient(inUserClient),
												 mKernConfigDirectoryRef(0)
{	
	mUserClient.AddRef() ;
}
	
IOFireWireLibConfigDirectoryImp::~IOFireWireLibConfigDirectoryImp()
{
	mUserClient.Release() ;
}
	
IOReturn
IOFireWireLibConfigDirectoryImp::Update(UInt32 offset)
{
	return kIOReturnUnsupported ;
}

IOReturn
IOFireWireLibConfigDirectoryImp::GetKeyType(int key, IOConfigKeyType& type)
{
	io_connect_t	connection = mUserClient.GetUserClientConnection() ;

	if (!connection)
		return kIOReturnError ;
		
	return IOConnectMethodScalarIScalarO(
				connection,
				kFWConfigDirectoryGetKeyType,
				1,
				1,
				key,
				& type) ;
}

IOReturn
IOFireWireLibConfigDirectoryImp::GetKeyValue(int key, UInt32 &value, CFStringRef*& text)
{
	io_connect_t		connection ;
	
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;
	
	FWKernOSStringRef	kernelStringRef ;
	UInt32				stringLen ;
	IOReturn result = IOConnectMethodScalarIScalarO(
							connection,
							kFWConfigDirectoryGetKeyValue_UInt32,
							2,
							3,
							mKernConfigDirectoryRef,
							key,
							& value,
							& kernelStringRef,
							& stringLen) ;

	if (text && (kIOReturnSuccess == result))
		result = mUserClient.CreateCFStringWithOSStringRef(kernelStringRef, stringLen, text) ;
	
	return result ;
}
	
IOReturn
IOFireWireLibConfigDirectoryImp::GetKeyValue(int key, CFDataRef* value, CFStringRef*& text)
{
	io_connect_t		connection ;
	
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;
	
	FWKernOSStringRef	kernelStringRef ;
	UInt32				stringLen ;
	FWKernOSDataRef		kernelDataRef ;
	IOByteCount			dataSize ;
	
	IOReturn result = IOConnectMethodScalarIScalarO(
							connection,
							kFWConfigDirectoryGetKeyValue_Data,
							2,
							4,
							mKernConfigDirectoryRef,
							key,
							& kernelStringRef,
							& stringLen,
							& kernelDataRef,
							& dataSize) ;

	if (text && (kIOReturnSuccess == result))
		result = mUserClient.CreateCFStringWithOSStringRef(kernelStringRef, stringLen, text) ;
	if (kIOReturnSuccess == result)
		result = mUserClient.CreateCFDataWithOSDataRef(kernelDataRef, dataSize, value) ;
	
	return result ;
}
	
IOReturn
IOFireWireLibConfigDirectoryImp::GetKeyValue(int key, IOFireWireLibConfigDirectoryRef& value, REFIID iid, CFStringRef*& text)
{
	io_connect_t		connection ;
	
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;
	
	FWKernOSStringRef			kernelStringRef ;
	FWKernConfigDirectoryRef	kernelDirectoryRef ;
	UInt32						stringLen ;
	IOReturn result = IOConnectMethodScalarIScalarO(
							connection,
							kFWConfigDirectoryGetKeyValue_ConfigDirectory,
							2,
							3,
							mKernConfigDirectoryRef,
							key,
							& kernelDirectoryRef,
							& kernelStringRef,
							& stringLen) ;

//	fprintf(stderr, "IOFireWireLibConfigDirectoryImp::GetKeyValue: kernelDirectoryRef=0x%08lX\n", kernelDirectoryRef) ;

	IUnknownVTbl**	iUnknown = nil ;
	if (kIOReturnSuccess == result)
	{
		iUnknown = IOFireWireLibConfigDirectoryCOM::Alloc(mUserClient, kernelDirectoryRef) ;
	
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
IOFireWireLibConfigDirectoryImp::GetKeyOffset(int key, FWAddress& value, CFStringRef*& text)
{
	io_connect_t		connection ;
	
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;

	FWKernOSStringRef	kernelStringRef ;
	UInt32				stringLen ;
	UInt32				addressHi ;
	UInt32				addressLo ;
	
	IOReturn result = IOConnectMethodScalarIScalarO(
								connection,
								kFWConfigDirectoryGetKeyOffset_FWAddress,
								2,
								4,
								mKernConfigDirectoryRef,
								key,
								& addressHi,
								& addressLo,
								& kernelStringRef,
								& stringLen) ;
	
	value.nodeID = addressHi >> 16 ;
	value.addressHi = addressHi & 0xFFFF ;
	value.addressLo = addressLo ;

	if (text && (kIOReturnSuccess == result))
		result = mUserClient.CreateCFStringWithOSStringRef(kernelStringRef, stringLen, text) ;

	return result ;
}

IOReturn
IOFireWireLibConfigDirectoryImp::GetKeyValue(int key, FWKernConfigDirectoryRef& value)
{
	io_connect_t		connection ;
	
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;
	
	FWKernOSStringRef			kernelStringRef ;
	FWKernConfigDirectoryRef	kernelDirectoryRef ;
	UInt32						stringLen ;
	IOReturn result = IOConnectMethodScalarIScalarO(
							connection,
							kFWConfigDirectoryGetKeyValue_ConfigDirectory,
							2,
							3,
							mKernConfigDirectoryRef,
							key,
							& kernelDirectoryRef,
							& kernelStringRef,
							& stringLen) ;
	
	return result ;
}

IOReturn
IOFireWireLibConfigDirectoryImp::GetIndexType(int index, IOConfigKeyType &type)
{
	io_connect_t		connection ;
	
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;

	return IOConnectMethodScalarIScalarO(
				connection,
				kFWConfigDirectoryGetIndexType,
				2,
				1,
				mKernConfigDirectoryRef,
				index,
				& type) ;
}

IOReturn
IOFireWireLibConfigDirectoryImp::GetIndexKey(int index, int &key)
{
	io_connect_t		connection ;
	
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;

	return IOConnectMethodScalarIScalarO(
				connection,
				kFWConfigDirectoryGetIndexKey,
				2,
				1,
				mKernConfigDirectoryRef,
				index,
				& key) ;
}

IOReturn
IOFireWireLibConfigDirectoryImp::GetIndexValue(int index, UInt32& value)
{
	io_connect_t		connection ;
	
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;

	return IOConnectMethodScalarIScalarO(
				connection,
				kFWConfigDirectoryGetIndexValue_UInt32,
				2,
				1,
				mKernConfigDirectoryRef,
				index,
				& value) ;
}

	
IOReturn
IOFireWireLibConfigDirectoryImp::GetIndexValue(int index, CFDataRef* value)
{
	io_connect_t		connection ;
	
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;

	FWKernOSDataRef	dataRef ;
	IOByteCount		dataLen ;
	IOReturn result = IOConnectMethodScalarIScalarO(
							connection,
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
IOFireWireLibConfigDirectoryImp::GetIndexValue(int index, CFStringRef* value)
{
	io_connect_t		connection ;
	
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;

	FWKernOSStringRef	stringRef ;
	UInt32				stringLen ;
	IOReturn result = IOConnectMethodScalarIScalarO(
							connection,
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
IOFireWireLibConfigDirectoryImp::GetIndexValue(int index, FWKernConfigDirectoryRef& value)
{
	io_connect_t		connection ;
	
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;
	
	return IOConnectMethodScalarIScalarO(
					connection,
					kFWConfigDirectoryGetIndexValue_ConfigDirectory,
					2,
					1,
					mKernConfigDirectoryRef,
					index,
					& value) ;
}

IOReturn
IOFireWireLibConfigDirectoryImp::GetIndexValue(int index, IOFireWireLibConfigDirectoryRef& value, REFIID iid)
{
	io_connect_t		connection ;
	
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;
	
	FWKernConfigDirectoryRef	directoryRef ;
	IOReturn result = IOConnectMethodScalarIScalarO(connection, kFWConfigDirectoryGetIndexValue_ConfigDirectory,
								2, 1, mKernConfigDirectoryRef, index, & directoryRef) ;

	IUnknownVTbl** iUnknown ;
	if (kIOReturnSuccess == result)
	{
		iUnknown = IOFireWireLibConfigDirectoryCOM::Alloc(mUserClient, directoryRef) ;

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
IOFireWireLibConfigDirectoryImp::GetIndexOffset(int index, FWAddress& value)
{
	io_connect_t		connection ;	
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;
	
	UInt32 addressHi ;
	UInt32 addressLo ;
	IOReturn result = IOConnectMethodScalarIScalarO(connection, kFWConfigDirectoryGetIndexOffset_FWAddress, 2, 2, 
							mKernConfigDirectoryRef, index, & addressHi, & addressLo) ;
	
	value.nodeID = addressHi>>16 ;
	value.addressHi = addressHi ;
	value.addressLo = addressLo ;
	
	return result ;
}
	
IOReturn
IOFireWireLibConfigDirectoryImp::GetIndexOffset(int index, UInt32& value)
{
	io_connect_t		connection ;
	
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;

	return IOConnectMethodScalarIScalarO(connection, kFWConfigDirectoryGetIndexOffset_UInt32, 2, 1,
					mKernConfigDirectoryRef, index, & value) ;
}

IOReturn
IOFireWireLibConfigDirectoryImp::GetIndexEntry(int index, UInt32 &value)
{
	io_connect_t		connection ;

	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;

	return IOConnectMethodScalarIScalarO(connection, kFWConfigDirectoryGetIndexEntry, 2, 1, 
					mKernConfigDirectoryRef, index, & value) ;
}

IOReturn
IOFireWireLibConfigDirectoryImp::GetSubdirectories(io_iterator_t *outIterator)
{
	io_connect_t	connection ;
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;
	
	return IOConnectMethodScalarIScalarO(connection, kFWConfigDirectoryGetSubdirectories, 1, 1,
					mKernConfigDirectoryRef, outIterator) ;
}

IOReturn
IOFireWireLibConfigDirectoryImp::GetKeySubdirectories(int key, io_iterator_t *outIterator)
{
	io_connect_t	connection ;
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;

	return IOConnectMethodScalarIScalarO(connection, kFWConfigDirectoryGetKeySubdirectories, 2, 1,
					mKernConfigDirectoryRef, key, outIterator) ;
}

IOReturn
IOFireWireLibConfigDirectoryImp::GetType(int *outType)
{
	io_connect_t	connection ;
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;

	return IOConnectMethodScalarIScalarO(connection, kFWConfigDirectoryGetType, 1, 1, mKernConfigDirectoryRef,
					outType) ;
}

IOReturn 
IOFireWireLibConfigDirectoryImp::GetNumEntries(int *outNumEntries)
{
	io_connect_t	connection ;
	if (0 == (connection = mUserClient.GetUserClientConnection()))
		return kIOReturnError ;
	
	return IOConnectMethodScalarIScalarO(connection, kFWConfigDirectoryGetNumEntries, 1, 1,
					mKernConfigDirectoryRef, outNumEntries) ;
}

IOReturn
IOFireWireLibConfigDirectoryImp::Init()
{
	IOReturn result = kIOReturnSuccess ;

	if (0 == mKernConfigDirectoryRef)
	{
		io_connect_t	connection ;
		if (0 == (connection = mUserClient.GetUserClientConnection()))
			result = kIOReturnError ;
		else
			result = IOConnectMethodScalarIScalarO(connection, kFWConfigDirectoryCreate, 0, 1, & mKernConfigDirectoryRef) ;
	}	
	
	return result ;
}

//IOReturn
//IOFireWireLibConfigDirectoryImp::GetSubdirectoryRef(int inKey, FWKernConfigDirectoryRef* outDirRef, CFStringRef*& outText) ;

// ============================================================
//
// IOFireWireLibConfigDirectoryCOM
//
// ============================================================

IOFireWireLibConfigDirectoryCOM::IOFireWireLibConfigDirectoryCOM(
	IOFireWireDeviceInterfaceImp& 	inUserClient,
	FWKernConfigDirectoryRef 		inDirRef): IOFireWireLibConfigDirectoryImp(inUserClient, inDirRef)
{
	mInterface.pseudoVTable = (IUnknownVTbl*) & sInterface ;
	mInterface.obj = this ;
}

IOFireWireLibConfigDirectoryCOM::IOFireWireLibConfigDirectoryCOM(IOFireWireDeviceInterfaceImp& inUserClient): IOFireWireLibConfigDirectoryImp(inUserClient)
{
	mInterface.pseudoVTable = (IUnknownVTbl*) & sInterface ;
	mInterface.obj = this ;
}

IOFireWireLibConfigDirectoryCOM::~IOFireWireLibConfigDirectoryCOM()
{
}

IUnknownVTbl**
IOFireWireLibConfigDirectoryCOM::Alloc(IOFireWireDeviceInterfaceImp& inUserClient, FWKernConfigDirectoryRef inDirRef)
{
	IOFireWireLibConfigDirectoryCOM*	me ;
	IUnknownVTbl** interface = NULL ;
	
	me = new IOFireWireLibConfigDirectoryCOM(inUserClient, inDirRef) ;
	if ( me && (kIOReturnSuccess == me->Init()) )
	{
//		me->AddRef() ;
		interface = & me->mInterface.pseudoVTable ;
	}
	else
		delete me ;
		
	return interface ;
}

IUnknownVTbl** 
IOFireWireLibConfigDirectoryCOM::Alloc(IOFireWireDeviceInterfaceImp& inUserClient)
{
    IOFireWireLibConfigDirectoryCOM *	me;
	IUnknownVTbl** 	interface = NULL;
	
    me = new IOFireWireLibConfigDirectoryCOM(inUserClient) ;
    if( me && (kIOReturnSuccess == me->Init()) )
	{
		// whoops -- no need to call addref. all these objects derive from IOFireWireIUnknown
		// which automatically sets the ref count to 1 on creation.
//		me->AddRef();
        interface = & me->mInterface.pseudoVTable;
    }
	else
		delete me ;
	
	return interface;
}

HRESULT STDMETHODCALLTYPE
IOFireWireLibConfigDirectoryCOM::QueryInterface(REFIID iid, LPVOID* ppv)
{
	HRESULT		result = S_OK ;
	*ppv = nil ;

	CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;

	if ( CFEqual(interfaceID, IUnknownUUID) ||  CFEqual(interfaceID, kIOFireWireConfigDirectoryInterfaceID) )
	{
		*ppv = & mInterface ;
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
IOFireWireLibConfigDirectoryCOM::SUpdate(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	UInt32 								inOffset)
{
	return kIOReturnUnsupported ;
}

IOReturn
IOFireWireLibConfigDirectoryCOM::SGetKeyType(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int 								inKey, 
	IOConfigKeyType* 					outType)
{
	return GetThis(inDir)->GetKeyType(inKey, *outType) ;
}

IOReturn
IOFireWireLibConfigDirectoryCOM::SGetKeyValue_UInt32(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int 								inKey, 
	UInt32*								outValue, 
	CFStringRef*						outText)
{
	return GetThis(inDir)->GetKeyValue(inKey, * outValue, outText) ;
}
	
IOReturn
IOFireWireLibConfigDirectoryCOM::SGetKeyValue_Data(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int 								inKey, 
	CFDataRef *							outValue, 
	CFStringRef*						outText)
{
	return GetThis(inDir)->GetKeyValue(inKey, outValue, outText) ;
}
	
IOReturn
IOFireWireLibConfigDirectoryCOM::SGetKeyValue_ConfigDirectory(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int 								inKey, 
	IOFireWireLibConfigDirectoryRef*	outValue,
	REFIID								iid,
	CFStringRef*						outText)
{
	return GetThis(inDir)->GetKeyValue(inKey, * outValue, iid, outText) ;
}
	
IOReturn
IOFireWireLibConfigDirectoryCOM::SGetKeyOffset_FWAddress(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int 								inKey, 
	FWAddress*							outValue, 
	CFStringRef* 						text)
{
	return GetThis(inDir)->GetKeyOffset(inKey, *outValue, text) ;
}
	
IOReturn
IOFireWireLibConfigDirectoryCOM::SGetIndexType(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int 								inIndex, 
	IOConfigKeyType*					outType)
{
	return GetThis(inDir)->GetIndexType(inIndex, *outType) ;
}

IOReturn
IOFireWireLibConfigDirectoryCOM::SGetIndexKey(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int 								inIndex, 
	int *								outKey)
{
	return GetThis(inDir)->GetIndexKey(inIndex, *outKey) ;
}

IOReturn
IOFireWireLibConfigDirectoryCOM::SGetIndexValue_UInt32(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int 								inIndex, 
	UInt32 *							outValue)
{
	return GetThis(inDir)->GetIndexValue(inIndex, *outValue) ;
}

IOReturn
IOFireWireLibConfigDirectoryCOM::SGetIndexValue_Data(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int 								inIndex, 
	CFDataRef *							outValue)
{
	return GetThis(inDir)->GetIndexValue(inIndex, outValue) ;
}
	
IOReturn
IOFireWireLibConfigDirectoryCOM::SGetIndexValue_String(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int 								inIndex, 
	CFStringRef*						outValue)
{
	return GetThis(inDir)->GetIndexValue(inIndex, outValue) ;
}
	
IOReturn
IOFireWireLibConfigDirectoryCOM::SGetIndexValue_ConfigDirectory(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int 								inIndex, 
	IOFireWireLibConfigDirectoryRef*	outValue,
	REFIID								iid)
{
	return GetThis(inDir)->GetIndexValue(inIndex, *outValue, iid) ;
}
	
IOReturn
IOFireWireLibConfigDirectoryCOM::SGetIndexOffset_FWAddress(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int 								inIndex, 
	FWAddress*							outValue)
{
	return GetThis(inDir)->GetIndexOffset(inIndex, *outValue) ;
}

IOReturn
IOFireWireLibConfigDirectoryCOM::SGetIndexOffset_UInt32(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int 								inIndex, 
	UInt32*								outValue)
{
	return GetThis(inDir)->GetIndexOffset(inIndex, *outValue) ;
}

IOReturn
IOFireWireLibConfigDirectoryCOM::SGetIndexEntry(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int 								inIndex, 
	UInt32*								outValue)
{
	return GetThis(inDir)->GetIndexEntry(inIndex, *outValue) ;
}

IOReturn
IOFireWireLibConfigDirectoryCOM::SGetSubdirectories(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	io_iterator_t*						outIterator)
{
	return GetThis(inDir)->GetSubdirectories(outIterator) ;
}

IOReturn
IOFireWireLibConfigDirectoryCOM::SGetKeySubdirectories(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int 								inKey, 
	io_iterator_t *						outIterator)
{
	return GetThis(inDir)->GetKeySubdirectories(inKey, outIterator) ;
}

IOReturn
IOFireWireLibConfigDirectoryCOM::SGetType(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int *								outType)
{
	return GetThis(inDir)->GetType(outType) ;
}

IOReturn
IOFireWireLibConfigDirectoryCOM::SGetNumEntries(
	IOFireWireLibConfigDirectoryRef 	inDir, 
	int *								outNumEntries)
{
	return GetThis(inDir)->GetNumEntries(outNumEntries) ;
}
