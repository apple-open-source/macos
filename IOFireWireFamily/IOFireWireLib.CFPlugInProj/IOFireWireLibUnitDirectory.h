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
 *  IOFireWireLibUnitDirectory.h
 *  IOFireWireLib
 *
 *  Created by NWG on Thu Apr 27 2000.
 *  Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef _IOKIT_IOFireWireLibUnitDirectory_H_
#define _IOKIT_IOFireWireLibUnitDirectory_H_

#include "IOFireWireLib.h"
#include "IOFireWireLibPriv.h"

class IOFireWireLocalUnitDirectoryImp: public IOFireWireIUnknown
{
 public:
 	struct InterfaceMap
 	{
 		IUnknownVTbl*						pseudoVTable ;
 		IOFireWireLocalUnitDirectoryImp*	obj ;
 	} ;
 
	static IOFireWireLocalUnitDirectoryInterface	sInterface ;
 	InterfaceMap									mInterface ;

 	// GetThis()
 	inline static IOFireWireLocalUnitDirectoryImp* GetThis(IOFireWireLibLocalUnitDirectoryRef self)
	 	{ return ((InterfaceMap*)self)->obj ;}

	// --- IUNKNOWN support ----------------
	static IUnknownVTbl**	Alloc(
									IOFireWireDeviceInterfaceImp& inUserClient) ;

	HRESULT					QueryInterface(
									REFIID				iid, 
									void **				ppv ) ;

	// --- adding to ROM -------------------
	static IOReturn			SAddEntry_Ptr(
									IOFireWireLibLocalUnitDirectoryRef self,
									int 				key,
									void*				inBuffer,
									size_t				inLen,
									CFStringRef			inDesc = NULL) ;
	static IOReturn			SAddEntry_UInt32(
									IOFireWireLibLocalUnitDirectoryRef self,
									int					key,
									UInt32				value,
									CFStringRef			inDesc = NULL) ;
	static IOReturn			SAddEntry_FWAddress(
									IOFireWireLibLocalUnitDirectoryRef self,
									int					key,
									const FWAddress*	value,
									CFStringRef			inDesc = NULL) ;

	// Use this function to cause your unit directory to appear in the Mac's config ROM.
	static	IOReturn		SPublish(IOFireWireLibLocalUnitDirectoryRef	self) ;
	static	IOReturn		SUnpublish(IOFireWireLibLocalUnitDirectoryRef self) ;

 public:
	// --- constructor/destructor ----------
							IOFireWireLocalUnitDirectoryImp(IOFireWireDeviceInterfaceImp& inUserClient) ;
	virtual					~IOFireWireLocalUnitDirectoryImp() ;

	IOFireWireLibLocalUnitDirectoryRef	CreateRef() ;
	
	// --- adding to ROM -------------------
	virtual IOReturn		AddEntry(
									int 				key,
									void*				inBuffer,
									size_t				inLen,
									CFStringRef			inDesc = NULL) ;
	virtual IOReturn		AddEntry(
									int					key,
									UInt32				value,
									CFStringRef			inDesc = NULL) ;
	virtual IOReturn		AddEntry(
									int					key,
									const FWAddress &	value,
									CFStringRef			inDesc = NULL) ;
									
	virtual IOReturn		Publish() ;
	virtual IOReturn		Unpublish() ;

	// callback management
	virtual IOFireWireDeviceAddedCallback		SetDeviceAddedCallback(
													IOFireWireDeviceAddedCallback	 /*inDeviceAddedHandler*/) {return mDeviceAddedCallback; }
	virtual IOFireWireDeviceRemovedCallback		SetDeviceRemovedCallback(
													IOFireWireDeviceRemovedCallback	/*inDeviceRemovedHandler*/) { return mDeviceRemovedCallback; }

 protected:
	FWKernUnitDirRef					mKernUnitDirRef ;
	IOFireWireDeviceInterfaceImp&		mUserClient ;
	IOFireWireDeviceAddedCallback		mDeviceAddedCallback ;
	IOFireWireDeviceRemovedCallback		mDeviceRemovedCallback ;
	Boolean								mPublished ;
} ;
#endif //_IOKIT_IOFireWireLibUnitDirectory_H_