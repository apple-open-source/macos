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

#import "IOFireWireLibIUnknown.h"
#import "IOFireWireLibPriv.h"

namespace IOFireWireLib {

	class Device ;
	class ConfigDirectory: public IOFireWireIUnknown
	{
		protected:
			typedef ::IOFireWireLibConfigDirectoryRef 	DirRef ;
	
		public:
			ConfigDirectory( const IUnknownVTbl & interface, Device& inUserClient, UserObjectHandle inDirRef ) ;
			ConfigDirectory( const IUnknownVTbl & interface, Device& inUserClient ) ;
			virtual ~ConfigDirectory() ;
			
			/*!
				@function update
				makes sure that the ROM has at least the specified capacity,
				and that the ROM is uptodate from its start to at least the
				specified quadlet offset.
				@result kIOReturnSuccess if the specified offset is now
				accessable at romBase[offset].
			*/
			IOReturn Update(UInt32 offset) ;
		
			/*!
				@function getKeyType
				Gets the data type for the specified key
				@param type on return, set to the data type
				@result kIOReturnSuccess if the key exists in the dictionary
			*/
			IOReturn GetKeyType(int key, IOConfigKeyType& type);
			
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
			IOReturn GetKeyValue(int key, UInt32 &value, CFStringRef*& text);
			IOReturn GetKeyValue(int key, CFDataRef* value, CFStringRef*& text);
			IOReturn GetKeyValue(int key, DirRef& value, REFIID iid, CFStringRef*& text);
			IOReturn GetKeyOffset(int key, FWAddress& value, CFStringRef*& text);
			IOReturn GetKeyValue(int key, UserObjectHandle& value) ;
		
			/*!
				@function getIndexType
				Gets the data type for entry at the specified index
				@param type on return, set to the data type
				@result kIOReturnSuccess if the index exists in the dictionary
			*/
			IOReturn GetIndexType(int index, IOConfigKeyType &type);
			/*!
				@function getIndexKey
				Gets the key for entry at the specified index
				@param key on return, set to the key
				@result kIOReturnSuccess if the index exists in the dictionary
			*/
			IOReturn GetIndexKey(int index, int &key);
		
			/*!
				@function getIndexValue
				Gets the value at the specified index of the directory,
				in a variety of forms.
				@param type on return, set to the data type
				@result kIOReturnSuccess if the index exists in the dictionary
				and is of a type appropriate for the value parameter
				@param value reference to variable to store the entry's value
			*/
			IOReturn GetIndexValue(int index, UInt32& value);
			IOReturn GetIndexValue(int index, CFDataRef* value);
			IOReturn GetIndexValue(int index, CFStringRef* value);
//			IOReturn GetIndexValue(int index, UserObjectHandle& value) ;
			IOReturn GetIndexValue(int index, DirRef& value, REFIID iid);
		
			IOReturn GetIndexOffset(int index, FWAddress& value);
			IOReturn GetIndexOffset(int index, UInt32& value);
		
			/*!
				@function getIndexEntry
				Gets the entry at the specified index of the directory,
				as a raw UInt32.
				@param entry on return, set to the entry value
				@result kIOReturnSuccess if the index exists in the dictionary
				@param value reference to variable to store the entry's value
			*/
			IOReturn GetIndexEntry(int index, UInt32 &value);
		
			/*!
				@function getSubdirectories
				Creates an iterator over the subdirectories of the directory.
				@param iterator on return, set to point to an OSIterator
				@result kIOReturnSuccess if the iterator could be created
			*/
			IOReturn GetSubdirectories(io_iterator_t *outIterator);
		
			/*!
				@function getKeySubdirectories
				Creates an iterator over subdirectories of a given type of the directory.
				@param key type of subdirectory to iterate over
				@param iterator on return, set to point to an io_iterator_t
				@result kIOReturnSuccess if the iterator could be created
			*/
			IOReturn GetKeySubdirectories(int key, io_iterator_t *outIterator);
			IOReturn GetType(int *outType) ;
			IOReturn GetNumEntries(int *outNumEntries) ;
		
		protected:
			Device&						mUserClient ;
			UserObjectHandle	mKernConfigDirectoryRef ;			
	} ;
	
	class ConfigDirectoryCOM: public ConfigDirectory
	{
		protected:
			typedef ::IOFireWireConfigDirectoryInterface	Interface ;
	
		public:
			ConfigDirectoryCOM(Device& inUserClient) ;
			ConfigDirectoryCOM(Device& inUserClient, UserObjectHandle inDirRef) ;
			virtual ~ConfigDirectoryCOM() ;

		private:
			static Interface sInterface ;

		public:
			// --- IUNKNOWN support ----------------
			static IUnknownVTbl**	Alloc(Device& inUserClient, UserObjectHandle inDirRef) ;
			static IUnknownVTbl**	Alloc(Device& inUserClient) ;
			virtual HRESULT			QueryInterface(REFIID iid, void ** ppv ) ;
		
		protected:
			// --- static methods ------------------
			static IOReturn SUpdate(
									DirRef 	inDir, 
									UInt32 								inOffset) ;
			static IOReturn SGetKeyType(
									DirRef 	inDir, 
									int 								inKey, 
									IOConfigKeyType* 					outType);
			static IOReturn SGetKeyValue_UInt32(
									DirRef 	inDir, 
									int 								inKey, 
									UInt32*								outValue, 
									CFStringRef*						outText);
			static IOReturn SGetKeyValue_Data(
									DirRef 	inDir, 
									int 								inKey, 
									CFDataRef*							outValue, 
									CFStringRef*						outText);
			static IOReturn SGetKeyValue_ConfigDirectory(
									DirRef 	inDir, 
									int 								inKey, 
									DirRef *	outValue, 
									REFIID								iid,
									CFStringRef*						outText);
			static IOReturn SGetKeyOffset_FWAddress(
									DirRef 				inDir, 
									int 				inKey, 
									FWAddress*			outValue, 
									CFStringRef*		text);
			static IOReturn SGetIndexType(
									DirRef 				inDir, 
									int 				inIndex, 
									IOConfigKeyType*	type);
			static IOReturn SGetIndexKey(
									DirRef 				inDir, 
									int 				inIndex, 
									int *				key);
			static IOReturn SGetIndexValue_UInt32(
									DirRef 				inDir, 
									int 				inIndex, 
									UInt32 *			value);
			static IOReturn SGetIndexValue_Data(
									DirRef 				inDir, 
									int 				inIndex, 
									CFDataRef *			value);
			static IOReturn SGetIndexValue_String(
									DirRef 				inDir, 
									int 				inIndex, 
									CFStringRef*		outValue);
			static IOReturn SGetIndexValue_ConfigDirectory(
									DirRef 				inDir, 
									int 				inIndex, 
									DirRef *			outValue,
									REFIID				iid);
			static IOReturn SGetIndexOffset_FWAddress(
									DirRef 				inDir, 
									int 				inIndex, 
									FWAddress*			outValue);
			static IOReturn SGetIndexOffset_UInt32(
									DirRef 				inDir, 
									int 				inIndex, 
									UInt32*				outValue);
			static IOReturn SGetIndexEntry(
									DirRef 				inDir, 
									int 				inIndex, 
									UInt32*				outValue);
			static IOReturn SGetSubdirectories(
									DirRef 				inDir, 
									io_iterator_t*		outIterator);
			static IOReturn SGetKeySubdirectories(
									DirRef 				inDir,
									int 				inKey, 
									io_iterator_t *		outIterator);
			static IOReturn SGetType(
									DirRef 				inDir, 
									int *				outType) ;
			static IOReturn SGetNumEntries(
									DirRef		 		inDir, 
									int *				outNumEntries) ;
	} ;
}
