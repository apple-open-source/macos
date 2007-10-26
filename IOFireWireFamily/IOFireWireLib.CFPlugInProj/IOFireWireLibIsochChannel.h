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
 *  IOFireWireLibIsochChannel.h
 *  IOFireWireFamily
 *
 *  Created on Mon Mar 12 2001.
 *  Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
 *
 */

#import "IOFireWireLibIUnknown.h"
#import "IOFireWireLibPriv.h"
#import "IOFireWireLibIsoch.h"

namespace IOFireWireLib {

	class Device ;
	class IsochChannel: public IOFireWireIUnknown
	{
		protected:
			typedef ::IOFireWireLibIsochChannelRef 				ChannelRef ;
			typedef ::IOFireWireIsochChannelForceStopHandler	ForceStopHandler ;

		public:
			IsochChannel( const IUnknownVTbl & interface, Device& userclient, bool inDoIRM, IOByteCount inPacketSize, IOFWSpeed inPrefSpeed) ;
			virtual ~IsochChannel() ;

		public:
			// --- other methods
			virtual IOReturn 			SetTalker( IOFireWireLibIsochPortRef talker ) ;
			virtual IOReturn			AddListener( IOFireWireLibIsochPortRef listener ) ;

			virtual IOReturn			AllocateChannel() ;
			virtual IOReturn 			ReleaseChannel() ;
			virtual IOReturn			Start() ;
			virtual IOReturn			Stop() ;
					
			virtual ForceStopHandler	SetChannelForceStopHandler( ForceStopHandler stopProc, IOFireWireLibIsochChannelRef interface ) ;
			virtual void	 			SetRefCon( void* stopProcRefCon ) ;
			virtual void*				GetRefCon() ;

			virtual Boolean				NotificationIsOn() ;
			virtual Boolean				TurnOnNotification( IOFireWireLibIsochChannelRef interface ) ;
			virtual void				TurnOffNotification() ;

			virtual void				ClientCommandIsComplete( FWClientCommandID commandID, IOReturn status ) ;
		
		protected:
//			static void					ForceStop( ChannelRef refcon, IOReturn result, void** args, int numArgs ) ;
		
		protected:
			Device&						mUserClient ;
			UserObjectHandle			mKernChannelRef ;
			Boolean						mNotifyIsOn ;
			ForceStopHandler			mForceStopHandler ;
			void*						mUserRefCon ;
			IOFireWireLibIsochPortRef	mTalker ;
			CFMutableArrayRef			mListeners ;
			ChannelRef					mRefInterface ;
			IOFWSpeed					mSpeed ;
			IOFWSpeed					mPrefSpeed ;
			UInt32						mChannel ;
	} ;
	
	class IsochChannelCOM: public IsochChannel
	{
			typedef ::IOFireWireIsochChannelInterface		Interface ;
	
		public:
			IsochChannelCOM( Device& userclient, bool inDoIRM, IOByteCount inPacketSize, IOFWSpeed inPrefSpeed ) ;
			virtual ~IsochChannelCOM() ;
		
		private:
			static Interface sInterface ;

		public:
			static IUnknownVTbl**	Alloc( Device&	inUserClient, Boolean inDoIRM, IOByteCount inPacketSize, IOFWSpeed inPrefSpeed) ;
			virtual HRESULT			QueryInterface( REFIID iid, void ** ppv ) ;
		
		protected:
			static IOReturn			SSetTalker(
											ChannelRef 	self, 
											IOFireWireLibIsochPortRef 		talker) ;
			static IOReturn 		SAddListener(
											ChannelRef 	self, 
											IOFireWireLibIsochPortRef 		listener) ;
			static IOReturn 		SAllocateChannel(
											ChannelRef 	self) ;
			static IOReturn			SReleaseChannel(
											ChannelRef 	self) ;
			static IOReturn 		SStart(
											ChannelRef 	self) ;
			static IOReturn			SStop(
											ChannelRef 	self) ;
			static ForceStopHandler
									SSetChannelForceStopHandler(
											ChannelRef 	self, 
											ForceStopHandler stopProc) ;
			static void		 		SSetRefCon(
											ChannelRef 	self, 
											void* 							stopProcRefCon) ;
			static void*			SGetRefCon(
											ChannelRef 	self) ;
			static Boolean			SNotificationIsOn(
											ChannelRef 	self) ;
			static Boolean			STurnOnNotification(
											ChannelRef 	self) ;
			static void				STurnOffNotification(
											ChannelRef	self) ;	
			static void				SClientCommandIsComplete(
											ChannelRef 	self, 
											FWClientCommandID 				commandID, 
											IOReturn 						status) ;
	} ;	
}
