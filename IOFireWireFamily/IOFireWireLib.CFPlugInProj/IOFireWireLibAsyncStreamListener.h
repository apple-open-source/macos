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
 *  IOFireWireLibAsyncStreamListener.h
 *  IOFireWireFamily
 *
 *  Created by Arul on Thu Sep 28 2006.
 *  Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
 *
 *	$Log: not supported by cvs2svn $
 *	Revision 1.2  2006/12/06 00:01:10  arulchan
 *	Isoch Channel 31 Generic Receiver
 *	
 *	Revision 1.1  2006/09/28 22:31:31  arulchan
 *	New Feature rdar::3413505
 *	
 */

#import "IOFireWireLibIUnknown.h"
#import "IOFireWireLibPriv.h"
#import "IOFireWireLibIsoch.h"
#import "IOFireWireLib.h"

namespace IOFireWireLib {

	class Device ;
	class AsyncStreamListener: public IOFireWireIUnknown
	{
		protected:
		
		typedef ::IOFWAsyncStreamListenerInterfaceRef			AsyncStreamListenerRef ;
		typedef ::IOFWAsyncStreamListenerHandler				AsyncStreamListenerHandler ;
		typedef ::IOFWAsyncStreamListenerSkippedPacketHandler	AsyncStreamSkippedPacketHandler ;

		public:
			AsyncStreamListener( const IUnknownVTbl&		interface, 
										Device&				userclient, 
										UserObjectHandle	inKernAddrSpaceRef,
										void*				inBuffer,
										UInt32				inBufferSize,
										void*				inCallBack = 0,
										void*				inRefCon = 0) ;
										
			virtual ~AsyncStreamListener() ;

		public:
			const AsyncStreamListenerHandler SetListenerHandler ( 
											AsyncStreamListenerRef		self, 
											AsyncStreamListenerHandler	inReceiver ) ;

			const AsyncStreamSkippedPacketHandler	SetSkippedPacketHandler( AsyncStreamListenerRef	self, 
																			 AsyncStreamSkippedPacketHandler	inHandler ) ;

			Boolean NotificationIsOn (
											AsyncStreamListenerRef		self ) ;

			Boolean TurnOnNotification (
											AsyncStreamListenerRef		self ) ;

			void TurnOffNotification (
											AsyncStreamListenerRef		self ) ;	

			void ClientCommandIsComplete (
											AsyncStreamListenerRef		self,
											FWClientCommandID			commandID ) ;

			void* GetRefCon	(
											AsyncStreamListenerRef		self ) ;	

			void SetFlags ( 
											AsyncStreamListenerRef		self,
											UInt32						flags );
		
			UInt32 GetFlags (
											AsyncStreamListenerRef		self ) ;	

			UInt32 GetOverrunCounter (
											AsyncStreamListenerRef		self ) ;	

			AsyncStreamListenerHandler	GetListenerHandler( 
											AsyncStreamListenerRef		self ) { return mListener; } ;	
			
			AsyncStreamSkippedPacketHandler GetSkippedPacketHandler(
											AsyncStreamListenerRef		self ) { return mSkippedPacketHandler;} ;	

			static void	Listener( AsyncStreamListenerRef refcon, IOReturn result, void** args, int numArgs) ;

			static void	SkippedPacket( AsyncStreamListenerRef refCon, IOReturn result, FWClientCommandID commandID, UInt32 packetCount) ;
			
			void*			GetBuffer(
											AsyncStreamListenerRef		self ) ;
											
			const UInt32		GetBufferSize(
											AsyncStreamListenerRef		self ) ;
											
		protected:
			Device&						mUserClient ;
			UserObjectHandle			mKernAsyncStreamListenerRef ;
			
			UInt32						mChannel ;
			char*						mBuffer ;
			Boolean						mNotifyIsOn ;
			void*						mUserRefCon ;
			UInt32						mBufferSize ;
			CFMutableDictionaryRef		mPendingLocks ;
			UInt32						mFlags;

			AsyncStreamListenerHandler			mListener ;
			AsyncStreamSkippedPacketHandler		mSkippedPacketHandler ;
			AsyncStreamListenerRef				mRefInterface ;
	} ;
	
	class AsyncStreamListenerCOM: public AsyncStreamListener
	{
			typedef ::IOFWAsyncStreamListenerInterface	Interface ;
	
		public:
			AsyncStreamListenerCOM( Device&				userclient,
									UserObjectHandle	inKernAddrSpaceRef,
									void*				inBuffer,
									UInt32				inBufferSize,
									void*				inCallBack,
									void*				inRefCon ) ;
			
			virtual ~AsyncStreamListenerCOM() ;
		
		private:
			static Interface sInterface ;

		public:
			static IUnknownVTbl**	Alloc(	Device&				inUserClient, 
											UserObjectHandle	inKernAddrSpaceRef,
											void*				inBuffer, 
											UInt32				inBufferSize,
											void*				inCallBack,
											void*				inRefCon );
											
			virtual HRESULT			QueryInterface( REFIID iid, void ** ppv ) ;
			
		protected:
			static const AsyncStreamListenerHandler SSetListenerHandler ( 
											AsyncStreamListenerRef		self, 
											AsyncStreamListenerHandler	inReceiver ) ;

			static const AsyncStreamSkippedPacketHandler	SSetSkippedPacketHandler( AsyncStreamListenerRef	self, 
																		  AsyncStreamSkippedPacketHandler		inHandler ) ;

			static Boolean SNotificationIsOn (
											AsyncStreamListenerRef		self ) ;

			static Boolean STurnOnNotification (
											AsyncStreamListenerRef		self ) ;

			static void STurnOffNotification (
											AsyncStreamListenerRef		self ) ;	

			static void SClientCommandIsComplete (
											AsyncStreamListenerRef		self,
											FWClientCommandID			commandID,
											IOReturn					status ) ;

			static void* SGetRefCon	(
											AsyncStreamListenerRef		self ) ;	

			static void SSetFlags ( 
											AsyncStreamListenerRef		self,
											UInt32						flags );
		
			static UInt32 SGetFlags (
											AsyncStreamListenerRef		self ) ;	

			static UInt32 SGetOverrunCounter (
											AsyncStreamListenerRef		self ) ;	

	};
}