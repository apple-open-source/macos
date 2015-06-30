/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 *  IOFireWireLibIRMAllocation.h
 *  IOFireWireFamily
 *
 *  Created by Andy on 02/06/07.
 *  Copyright (c) 2007 Apple Computer, Inc. All rights reserved.
 *
 *	$Log: not supported by cvs2svn $
 *	Revision 1.2  2007/02/15 22:02:38  ayanowit
 *	More fixes for new IRMAllocation stuff.
 *	
 *	Revision 1.1  2007/02/09 20:38:00  ayanowit
 *	New IRMAllocation files for user-space lib.
 *	
 *	
 */

#import "IOFireWireLibIUnknown.h"
#import "IOFireWireLibPriv.h"
#import "IOFireWireLib.h"

namespace IOFireWireLib {

	class Device ;
	class IRMAllocation: public IOFireWireIUnknown
	{
		public:
			IRMAllocation( const IUnknownVTbl& interface, 
						  Device& userclient, 
						  UserObjectHandle inKernIRMAllocationRef,
						  void* inCallBack = 0,
						  void* inRefCon = 0) ;
										
			virtual ~IRMAllocation() ;

		public:

			static void	LostProc( IOFireWireLibIRMAllocationRef refcon, IOReturn result, void** args, int numArgs) ;
		
			Boolean NotificationIsOn(IOFireWireLibIRMAllocationRef self ) ;
		
			Boolean TurnOnNotification(IOFireWireLibIRMAllocationRef self ) ;
		
			void TurnOffNotification(IOFireWireLibIRMAllocationRef self ) ;	
		
			void SetReleaseIRMResourcesOnFree (IOFireWireLibIRMAllocationRef self, Boolean doRelease ) ;
			IOReturn AllocateIsochResources(IOFireWireLibIRMAllocationRef self, UInt8 isochChannel, UInt32 bandwidthUnits);
			IOReturn DeallocateIsochResources(IOFireWireLibIRMAllocationRef self);
			Boolean AreIsochResourcesAllocated(IOFireWireLibIRMAllocationRef self, UInt8 *pAllocatedIsochChannel, UInt32 *pAllocatedBandwidthUnits);
		
			void SetRefCon(IOFireWireLibIRMAllocationRef self, void* refCon) ;
			void* GetRefCon(IOFireWireLibIRMAllocationRef self) ;
		
		protected:
			Boolean mNotifyIsOn ;
			Device& mUserClient ;
			UserObjectHandle mKernIRMAllocationRef ;
			IOFireWireLibIRMAllocationLostNotificationProc mLostHandler ;
			void* mUserRefCon ;
			IOFireWireLibIRMAllocationRef mRefInterface ;
	} ;
	
	class IRMAllocationCOM: public IRMAllocation
	{
			typedef ::IOFireWireLibIRMAllocationInterface	Interface ;
	
		public:
			IRMAllocationCOM( Device&					userclient,
									UserObjectHandle	inKernIRMAllocationRef,
									void*				inCallBack,
									void*				inRefCon ) ;
			
			virtual ~IRMAllocationCOM() ;
		
		private:
			static Interface sInterface ;

		public:
			static IUnknownVTbl**	Alloc(	Device&				inUserClient, 
											UserObjectHandle	inKernIRMAllocationRef,
											void*				inCallBack,
											void*				inRefCon );
											
			virtual HRESULT			QueryInterface( REFIID iid, void ** ppv ) ;
			
		protected:
		
			static Boolean SNotificationIsOn (IOFireWireLibIRMAllocationRef self ) ;
		
			static Boolean STurnOnNotification (IOFireWireLibIRMAllocationRef self ) ;
		
			static void STurnOffNotification (IOFireWireLibIRMAllocationRef self ) ;	
		
			static const void SSetReleaseIRMResourcesOnFree (IOFireWireLibIRMAllocationRef self, Boolean doRelease ) ;
		
			static IOReturn SAllocateIsochResources(IOFireWireLibIRMAllocationRef self, UInt8 isochChannel, UInt32 bandwidthUnits);
			static IOReturn SDeallocateIsochResources(IOFireWireLibIRMAllocationRef self);
			static Boolean SAreIsochResourcesAllocated(IOFireWireLibIRMAllocationRef self, UInt8 *pAllocatedIsochChannel, UInt32 *pAllocatedBandwidthUnits);
		
			static void SSetRefCon(IOFireWireLibIRMAllocationRef self, void* refCon) ;
			static void* SGetRefCon(IOFireWireLibIRMAllocationRef self) ;

	};
}