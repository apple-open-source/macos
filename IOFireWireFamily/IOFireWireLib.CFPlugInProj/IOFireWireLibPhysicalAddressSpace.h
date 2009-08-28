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
*  IOFireWireLibPhysicalAddressSpace.h
*  IOFireWireLib
*
*  Created by NWG on Fri Dec 08 2000.
*  Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
*
*/

#import "IOFireWireLibIUnknown.h"
#import "IOFireWireLibPriv.h"

namespace IOFireWireLib {

	class Device ;
	class PhysicalAddressSpace: IOFireWireIUnknown
	{
		typedef ::IOFireWirePhysicalAddressSpaceInterface	Interface ;
	
		public:
			//
			// === COM =====================================
			//
		
			struct InterfaceMap
			{
				IUnknownVTbl *						pseudoVTable;
				PhysicalAddressSpace*	obj;
			};
		
			// interfaces
			static Interface	sInterface ;
		
			// QueryInterface
			virtual HRESULT				QueryInterface(
												REFIID 				iid, 
												LPVOID*				ppv) ;
			// static allocator
			static IUnknownVTbl**		Alloc(
											Device&	inUserClient,
											UserObjectHandle inKernPhysicalAddrSpaceRef,
											UInt32 					inSize, 
											void* 					inBackingStore, 
											UInt32 					inFlags) ;
		
			//
			// === STATIC METHODS ==========================						
			//
			static void					SGetPhysicalSegments(
												IOFireWireLibPhysicalAddressSpaceRef self,
												UInt32*				ioSegmentCount,
												IOByteCount			outSegments[],
												IOPhysicalAddress	outAddresses[]) ;
			static IOPhysicalAddress	SGetPhysicalSegment(
												IOFireWireLibPhysicalAddressSpaceRef self,
												IOByteCount 		offset,
												IOByteCount*		length) ;
			static IOPhysicalAddress	SGetPhysicalAddress(
												IOFireWireLibPhysicalAddressSpaceRef self) ;
		
			static void					SGetFWAddress(
												IOFireWireLibPhysicalAddressSpaceRef self,
												FWAddress*			outAddr ) ;
			static void*				SGetBuffer(
												IOFireWireLibPhysicalAddressSpaceRef self) ;
			static const UInt32			SGetBufferSize(
												IOFireWireLibPhysicalAddressSpaceRef self) ;
		
			// --- constructor/destructor ------------------
									PhysicalAddressSpace(
											Device& inUserClient,
											UserObjectHandle    inKernPhysicalAddrSpaceRef,
											UInt32 					inSize, 
											void* 					inBackingStore, 
											UInt32 					inFlags) ;
			virtual					~PhysicalAddressSpace() ;
			IOReturn				Init() ;
		
			// --- accessors -------------------------------
			virtual void				GetPhysicalSegments(
												UInt32*				ioSegmentCount,
												IOByteCount			outSegments[],
												IOPhysicalAddress	outAddresses[]) ;
			virtual IOPhysicalAddress	GetPhysicalSegment(
												IOByteCount 		offset,
												IOByteCount*		length) ;
		protected:
			// --- member data -----------------------------
			Device&	mUserClient ;
			UserObjectHandle		mKernPhysicalAddrSpaceRef ;
			UInt32							mSize ;
			void*							mBackingStore ;
			FWAddress						mFWAddress ;
			
			FWPhysicalSegment32 *				mSegments ;
			UInt32							mSegmentCount ;
	} ;
}
