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

#import "IOFireWireLibIUnknown.h"
#import "IOFireWireLibPriv.h"

namespace IOFireWireLib {

	class Device ;
	class LocalUnitDirectory: public IOFireWireIUnknown
	{
		typedef ::IOFireWireLocalUnitDirectoryInterface 	Interface ;
		typedef ::IOFireWireLibLocalUnitDirectoryRef		DirRef ;

		protected:
		
			static Interface		sInterface ;		
			UserObjectHandle mKernUnitDirRef ;
			Device&					mUserClient ;
			bool					mPublished ;

			HRESULT					QueryInterface(
											REFIID				iid, 
											void **				ppv ) ;		
		public:
			// --- constructor/destructor ----------
									LocalUnitDirectory( Device& userclient ) ;
			virtual					~LocalUnitDirectory() ;
		
			// --- adding to ROM -------------------
			IOReturn				AddEntry(
											int 				key,
											void*				inBuffer,
											size_t				inLen,
											CFStringRef			inDesc = NULL) ;
			IOReturn				AddEntry(
											int					key,
											UInt32				value,
											CFStringRef			inDesc = NULL) ;
			IOReturn				AddEntry(
											int					key,
											const FWAddress &	value,
											CFStringRef			inDesc = NULL) ;
											
			IOReturn				Publish() ;
			IOReturn				Unpublish() ;

			// --- IUNKNOWN support ----------------
			static Interface**		Alloc( Device& userclient ) ;

			// --- adding to ROM -------------------
			static IOReturn			SAddEntry_Ptr(
											DirRef self,
											int 				key,
											void*				inBuffer,
											size_t				inLen,
											CFStringRef			inDesc = NULL) ;
			static IOReturn			SAddEntry_UInt32(
											DirRef self,
											int					key,
											UInt32				value,
											CFStringRef			inDesc = NULL) ;
			static IOReturn			SAddEntry_FWAddress(
											DirRef self,
											int					key,
											const FWAddress*	value,
											CFStringRef			inDesc = NULL) ;
		
			// Use this function to cause your unit directory to appear in the Mac's config ROM.
			static IOReturn			SPublish( DirRef self ) ;
			static IOReturn			SUnpublish( DirRef self ) ;		
	} ;
}
