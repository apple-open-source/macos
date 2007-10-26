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
 *  IOFireWirePseudoAddressSpacePriv.h
 *  IOFireWireLib
 *
 *  Created  by NWG on Wed Dec 06 2000.
 *  Copyright (c) 2000 Apple, Inc. All rights reserved.
 *
 */

#import "IOFireWireLibIUnknown.h"
#import "IOFireWireLibPriv.h"

namespace IOFireWireLib {

	class Device ;
	class PseudoAddressSpace: public IOFireWireIUnknown
	{
			typedef ::IOFireWirePseudoAddressSpaceInterface 	Interface ;
			typedef ::IOFireWireLibPseudoAddressSpaceRef		AddressSpaceRef ;
			typedef ::IOFireWirePseudoAddressSpaceWriteHandler	WriteHandler ;
			typedef ::IOFireWirePseudoAddressSpaceReadHandler 	ReadHandler ;
			typedef ::IOFireWirePseudoAddressSpaceSkippedPacketHandler SkippedPacketHandler ;
			
			// interfaces
			static Interface sInterface ;
		
		public:
			// static allocator
			static IUnknownVTbl** 	Alloc( Device& userclient, UserObjectHandle inKernAddrSpaceRef, 
											void* inBuffer, UInt32 inBufferSize, void* inBackingStore, 
											void* inRefCon) ;
		
			// QueryInterface
			virtual HRESULT	QueryInterface(REFIID iid, void **ppv );
		
			//
			// === STATIC METHODS ==========================						
			//
		
			static IOReturn							SInit() ;
			
			// callback management
			static const WriteHandler			SSetWriteHandler( AddressSpaceRef interface, WriteHandler inWriter ) ;
			static const ReadHandler			SSetReadHandler( AddressSpaceRef interface, ReadHandler inReader) ;
			static const SkippedPacketHandler	SSetSkippedPacketHandler( AddressSpaceRef interface, SkippedPacketHandler inHandler ) ;
		
			static Boolean			SNotificationIsOn(
											AddressSpaceRef interface) ;
			static Boolean			STurnOnNotification(
											AddressSpaceRef interface) ;
			static void				STurnOffNotification(
											AddressSpaceRef interface) ;	
			static void				SClientCommandIsComplete(
											AddressSpaceRef interface,	
											FWClientCommandID				commandID,
											IOReturn						status) ;
		
			// accessors
			static void				SGetFWAddress(
											AddressSpaceRef	interface,
											FWAddress*						outAddr) ;
			static void*			SGetBuffer(
											AddressSpaceRef	interface) ;
			static const UInt32		SGetBufferSize(
											AddressSpaceRef	interface) ;
			static void*			SGetRefCon(
											AddressSpaceRef	interface) ;
		
			// --- constructor/destructor ----------
									PseudoAddressSpace(
											Device&	userclient,
											UserObjectHandle				inKernAddrSpaceRef,
											void*							inBuffer,
											UInt32							inBufferSize,
											void*							inBackingStore,
											void*							inRefCon = 0) ;
			virtual					~PseudoAddressSpace() ;
					
			// --- callback methods ----------------
			static void				Writer( AddressSpaceRef refcon, IOReturn result, void** args,
											int numArgs) ;
			static void				Reader( AddressSpaceRef refcon, IOReturn result, void** args,
											int numArgs) ;
			static void				SkippedPacket( AddressSpaceRef refCon, IOReturn result, FWClientCommandID commandID,
											UInt32 packetCount) ;

			// --- notification methods ----------
			virtual const WriteHandler			SetWriteHandler( WriteHandler inWriter ) ;
			virtual const ReadHandler	 		SetReadHandler( ReadHandler inReader ) ;
			virtual const SkippedPacketHandler	SetSkippedPacketHandler( SkippedPacketHandler inHandler ) ;
			virtual Boolean						NotificationIsOn() const									{ return mNotifyIsOn ; } 
			virtual Boolean						TurnOnNotification( void* callBackRefCon ) ;
			virtual void						TurnOffNotification() ;
			virtual void						ClientCommandIsComplete( FWClientCommandID commandID, IOReturn status) ;
		
			virtual const FWAddress& 			GetFWAddress() ;
			virtual void*						GetBuffer() ;
			virtual const UInt32				GetBufferSize() ;
			virtual void*						GetRefCon() ;
		
			const ReadHandler					GetReader()	const											{ return mReader ; }
			const WriteHandler					GetWriter() const 											{ return mWriter ; }
			const SkippedPacketHandler			GetSkippedPacketHandler() const								{ return mSkippedPacketHandler ; }
			
		protected:
			// callback mgmt.
			Boolean					mNotifyIsOn ;
			CFRunLoopRef			mNotifyRunLoop ;
			IONotificationPortRef	mNotifyPort ;
			io_object_t				mNotify;		
			WriteHandler			mWriter ;
			ReadHandler				mReader ;
			SkippedPacketHandler	mSkippedPacketHandler ;
			Device&					mUserClient ;
			FWAddress				mFWAddress ;
			UserObjectHandle		mKernAddrSpaceRef ;
			char*					mBuffer ;
			UInt32					mBufferSize ;
		
			void*							mBackingStore ;
			void*							mRefCon ;
			
			CFMutableDictionaryRef			mPendingLocks ;
	} ;	
}
