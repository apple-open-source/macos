/*
 * Copyright (c) 1998-2007 Apple Computer, Inc. All rights reserved.
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

#import <IOKit/firewire/IOFireWireFamilyCommon.h>

#import "IOFireWireLibIUnknown.h"
#import "IOFireWireLib.h"
#import "IOFireWireLibPriv.h"

namespace IOFireWireLib 
{
	class Device;

	class PHYPacketListener : public IOFireWireIUnknown
	{		
		protected:
		
			static IOFireWireLibPHYPacketListenerInterface	sInterface;

			Device &						mUserClient;
			UserObjectHandle				mKernelRef;
			UInt32							mQueueCount;
			void *							mRefCon;
			
			IOFireWireLibPHYPacketCallback			mCallback;
			IOFireWireLibPHYPacketSkippedCallback	mSkippedCallback;
			UInt32									mFlags;
			Boolean									mNotifyIsOn;
			
		public:
			PHYPacketListener( Device& userClient, UInt32 queue_count );
			
			virtual ~PHYPacketListener( );

			static IUnknownVTbl**	Alloc(	Device& userclient, UInt32 queue_count );			
	
			virtual HRESULT				QueryInterface( REFIID iid, LPVOID* ppv );	
		
		protected:
			inline PHYPacketListener *	GetThis( IOFireWireLibPHYPacketListenerRef self )		
					{ return IOFireWireIUnknown::InterfaceMap<PHYPacketListener>::GetThis( self ); }
	
			static void SSetRefCon( IOFireWireLibPHYPacketListenerRef self, void * refCon );

			static void * SGetRefCon( IOFireWireLibPHYPacketListenerRef self );

			static void SSetListenerCallback(	IOFireWireLibPHYPacketListenerRef self, 
												IOFireWireLibPHYPacketCallback	callback );

			static void SSetSkippedPacketCallback(	IOFireWireLibPHYPacketListenerRef	self, 
													IOFireWireLibPHYPacketSkippedCallback	callback );

			static Boolean SNotificationIsOn( IOFireWireLibPHYPacketListenerRef self );

			static IOReturn STurnOnNotification( IOFireWireLibPHYPacketListenerRef self );
			IOReturn TurnOnNotification( IOFireWireLibPHYPacketListenerRef self );
	
			static void STurnOffNotification( IOFireWireLibPHYPacketListenerRef self );
			void TurnOffNotification( IOFireWireLibPHYPacketListenerRef self );
	
			static void SClientCommandIsComplete(	IOFireWireLibPHYPacketListenerRef		self,
													FWClientCommandID			commandID );

			static void SSetFlags( IOFireWireLibPHYPacketListenerRef	self,
								   UInt32								flags );
		
			static UInt32 SGetFlags( IOFireWireLibPHYPacketListenerRef self );
		
			static void SListenerCallback( IOFireWireLibPHYPacketListenerRef self, IOReturn result, void ** args, int numArgs );
			static void SSkippedCallback( IOFireWireLibPHYPacketListenerRef self, IOReturn result, void ** args, int numArgs );

		};

}