/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
 *  IOFireWireLibConfigDirectory.h
 *  IOFireWireFamily
 *
 *  Created by NWG on Thu Jan 18 2001.
 *  Copyright (c) 2000-2001 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef _IOKIT_IOFireWireLibConfigDirectory_H_
#define _IOKIT_IOFireWireLibConfigDirectory_H_

//#include <Carbon/Carbon.h>
#include <IOKit/IOCFPlugIn.h>

#include "IOFireWireLibPriv.h"

class IOFireWireLibConfigDirectoryImp: public IOFireWireIUnknown
{
 public:
	IOFireWireLibConfigDirectoryImp(IOFireWireDeviceInterfaceImp& inUserClient, FWKernConfigDirectoryRef inDirRef) ;
	IOFireWireLibConfigDirectoryImp(IOFireWireDeviceInterfaceImp& inUserClient) ;
	virtual ~IOFireWireLibConfigDirectoryImp() ;
	
    /*!
        @function update
        makes sure that the ROM has at least the specified capacity,
        and that the ROM is uptodate from its start to at least the
        specified quadlet offset.
        @result kIOReturnSuccess if the specified offset is now
        accessable at romBase[offset].
    */
	virtual IOReturn Update(UInt32 offset) ;

    /*!
        @function getKeyType
        Gets the data type for the specified key
        @param type on return, set to the data type
        @result kIOReturnSuccess if the key exists in the dictionary
    */
    virtual IOReturn GetKeyType(int key, IOConfigKeyType& type);
	
    /*!
        @function getKeyValue
        Gets the value for the specified key, in a variety of forms.
        @param value on return, set to the data type
        @param text if non-zero, on return points to the
        string description of the field, or NULL if no text found.
        @result kIOReturnSuccess if the key exists in the dictionary
        and is of a type appropriate for the value parameter
        @param value reference to variable to store the entry's value
    */
    virtual IOReturn GetKeyValue(int key, UInt32 &value, CFStringRef*& text);
    virtual IOReturn GetKeyValue(int key, CFDataRef* value, CFStringRef*& text);
    virtual IOReturn GetKeyValue(int key, IOFireWireLibConfigDirectoryRef& value, REFIID iid, CFStringRef*& text);
    virtual IOReturn GetKeyOffset(int key, FWAddress& value, CFStringRef*& text);
	virtual IOReturn GetKeyValue(int key, FWKernConfigDirectoryRef& value) ;

    /*!
        @function getIndexType
        Gets the data type for entry at the specified index
        @param type on return, set to the data type
        @result kIOReturnSuccess if the index exists in the dictionary
    */
    virtual IOReturn GetIndexType(int index, IOConfigKeyType &type);
    /*!
        @function getIndexKey
        Gets the key for entry at the specified index
        @param key on return, set to the key
        @result kIOReturnSuccess if the index exists in the dictionary
    */
    virtual IOReturn GetIndexKey(int index, int &key);

    /*!
        @function getIndexValue
        Gets the value at the specified index of the directory,
        in a variety of forms.
        @param type on return, set to the data type
        @result kIOReturnSuccess if the index exists in the dictionary
        and is of a type appropriate for the value parameter
        @param value reference to variable to store the entry's value
    */
    virtual IOReturn GetIndexValue(int index, UInt32& value);
    virtual IOReturn GetIndexValue(int index, CFDataRef* value);
    virtual IOReturn GetIndexValue(int index, CFStringRef* value);
	virtual IOReturn GetIndexValue(int index, FWKernConfigDirectoryRef& value) ;
    virtual IOReturn GetIndexValue(int index, IOFireWireLibConfigDirectoryRef& value, REFIID iid);

    virtual IOReturn GetIndexOffset(int index, FWAddress& value);
    virtual IOReturn GetIndexOffset(int index, UInt32& value);

    /*!
        @function getIndexEntry
        Gets the entry at the specified index of the directory,
        as a raw UInt32.
        @param entry on return, set to the entry value
        @result kIOReturnSuccess if the index exists in the dictionary
        @param value reference to variable to store the entry's value
    */
    virtual IOReturn GetIndexEntry(int index, UInt32 &value);

    /*!
        @function getSubdirectories
        Creates an iterator over the subdirectories of the directory.
        @param iterator on return, set to point to an OSIterator
        @result kIOReturnSuccess if the iterator could be created
    */
    virtual IOReturn GetSubdirectories(io_iterator_t *outIterator);

    /*!
        @function getKeySubdirectories
        Creates an iterator over subdirectories of a given type of the directory.
        @param key type of subdirectory to iterate over
        @param iterator on return, set to point to an io_iterator_t
        @result kIOReturnSuccess if the iterator could be created
    */
    virtual IOReturn GetKeySubdirectories(int key, io_iterator_t *outIterator);
	virtual IOReturn GetType(int *outType) ;
	virtual IOReturn GetNumEntries(int *outNumEntries) ;

	// my bits:
	virtual IOReturn 	Init() ;

//	static IUnknownVTbl**	AllocWithDirectoryRef(IOFireWireDeviceInterfaceImp& inUserClient, FWKernConfigDirectoryRef inDirRef) ;
//	static IUnknownVTbl**	Alloc(IOFireWireDeviceInterfaceImp& inUserClient) ;
	
 protected:
	IOFireWireDeviceInterfaceImp&	mUserClient ;
	FWKernConfigDirectoryRef		mKernConfigDirectoryRef ;
	
} ;

class IOFireWireLibConfigDirectoryCOM: public IOFireWireLibConfigDirectoryImp
{
 public:
	IOFireWireLibConfigDirectoryCOM(IOFireWireDeviceInterfaceImp& inUserClient) ;
	IOFireWireLibConfigDirectoryCOM(IOFireWireDeviceInterfaceImp& inUserClient, FWKernConfigDirectoryRef inDirRef) ;
	virtual ~IOFireWireLibConfigDirectoryCOM() ;

	// --- COM ---------------
 	struct InterfaceMap
 	{
 		IUnknownVTbl*						pseudoVTable ;
 		IOFireWireLibConfigDirectoryCOM*	obj ;
 	} ;
 
	static IOFireWireConfigDirectoryInterface	sInterface ;
 	InterfaceMap								mInterface ;

 	// GetThis()
 	inline static IOFireWireLibConfigDirectoryCOM* GetThis(IOFireWireLibConfigDirectoryRef self)
	 	{ return ((InterfaceMap*)self)->obj ;}

	// --- IUNKNOWN support ----------------
	
	static IUnknownVTbl**	Alloc(IOFireWireDeviceInterfaceImp& inUserClient, FWKernConfigDirectoryRef inDirRef) ;
	static IUnknownVTbl**	Alloc(IOFireWireDeviceInterfaceImp& inUserClient) ;
	virtual HRESULT			QueryInterface(REFIID iid, void ** ppv ) ;

	// --- static methods ------------------
	static IOReturn SUpdate(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							UInt32 								inOffset) ;
    static IOReturn SGetKeyType(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int 								inKey, 
							IOConfigKeyType* 					outType);
    static IOReturn SGetKeyValue_UInt32(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int 								inKey, 
							UInt32*								outValue, 
							CFStringRef*						outText);
    static IOReturn SGetKeyValue_Data(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int 								inKey, 
							CFDataRef*							outValue, 
							CFStringRef*						outText);
    static IOReturn SGetKeyValue_ConfigDirectory(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int 								inKey, 
							IOFireWireLibConfigDirectoryRef *	outValue, 
							REFIID								iid,
							CFStringRef*						outText);
    static IOReturn SGetKeyOffset_FWAddress(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int 								inKey, 
							FWAddress*							outValue, 
							CFStringRef*						text);
    static IOReturn SGetIndexType(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int 								inIndex, 
							IOConfigKeyType*					type);
    static IOReturn SGetIndexKey(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int 								inIndex, 
							int *								key);
    static IOReturn SGetIndexValue_UInt32(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int 								inIndex, 
							UInt32 *							value);
    static IOReturn SGetIndexValue_Data(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int 								inIndex, 
							CFDataRef *							value);
    static IOReturn SGetIndexValue_String(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int 								inIndex, 
							CFStringRef*						outValue);
    static IOReturn SGetIndexValue_ConfigDirectory(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int 								inIndex, 
							IOFireWireLibConfigDirectoryRef *	outValue,
							REFIID								iid);
    static IOReturn SGetIndexOffset_FWAddress(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int 								inIndex, 
							FWAddress*							outValue);
    static IOReturn SGetIndexOffset_UInt32(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int 								inIndex, 
							UInt32*								outValue);
    static IOReturn SGetIndexEntry(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int 								inIndex, 
							UInt32*								outValue);
    static IOReturn SGetSubdirectories(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							io_iterator_t *						outIterator);
    static IOReturn SGetKeySubdirectories(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int 								inKey, 
							io_iterator_t *						outIterator);
	static IOReturn SGetType(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int *								outType) ;
	static IOReturn SGetNumEntries(
							IOFireWireLibConfigDirectoryRef 	inDir, 
							int *								outNumEntries) ;
} ;

#endif // #ifndef _IOKIT_IOFireWireLibConfigDirectory_H_
