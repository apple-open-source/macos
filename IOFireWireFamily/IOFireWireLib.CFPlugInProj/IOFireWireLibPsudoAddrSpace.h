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

#ifndef _IOKIT_IOFireWireLibPsudoAddrSpace_H__
#define _IOKIT_IOFireWireLibPsudoAddrSpace_H__

#include "IOFireWireLib.h"

class IOFireWirePseudoAddressSpaceImp: public IOFireWireIUnknown
{
 public:

	struct InterfaceMap 
	{
        IUnknownVTbl*						pseudoVTable;
        IOFireWirePseudoAddressSpaceImp*	obj;
    };

 	// interfaces
 	static IOFireWirePseudoAddressSpaceInterface	sInterface ;
	InterfaceMap									mInterface ;
 
 	// GetThis()
 	inline static IOFireWirePseudoAddressSpaceImp* GetThis(IOFireWireLibPseudoAddressSpaceRef interface)
	 	{ return (IOFireWirePseudoAddressSpaceImp*) ((InterfaceMap*)interface)->obj ;}
 
	// static allocator
	static IUnknownVTbl** 	Alloc(
									IOFireWireDeviceInterfaceImp& inUserClient, 
									FWKernAddrSpaceRef 		inKernAddrSpaceRef, 
									void* 					inBuffer,
									UInt32 					inBufferSize,
									void* 					inBackingStore,
									void*					inRefCon) ;

	// QueryInterface
	virtual HRESULT	QueryInterface(REFIID iid, void **ppv );

	//
	// === STATIC METHODS ==========================						
	//

	static IOReturn							SInit() ;
	
	// callback management
	static const IOFireWirePseudoAddressSpaceWriteHandler	
											SSetWriteHandler(
													IOFireWireLibPseudoAddressSpaceRef			interface,
													IOFireWirePseudoAddressSpaceWriteHandler	inWriter) ;
	static const IOFireWirePseudoAddressSpaceReadHandler	
											SSetReadHandler(
													IOFireWireLibPseudoAddressSpaceRef			interface,
													IOFireWirePseudoAddressSpaceReadHandler		inReader) ;
	static const IOFireWirePseudoAddressSpaceSkippedPacketHandler
											SSetSkippedPacketHandler(
													IOFireWireLibPseudoAddressSpaceRef					interface,
													IOFireWirePseudoAddressSpaceSkippedPacketHandler	inHandler) ;

	static Boolean			SNotificationIsOn(
									IOFireWireLibPseudoAddressSpaceRef interface) ;
	static Boolean			STurnOnNotification(
									IOFireWireLibPseudoAddressSpaceRef interface) ;
	static void				STurnOffNotification(
									IOFireWireLibPseudoAddressSpaceRef interface) ;	
	static void				SClientCommandIsComplete(
									IOFireWireLibPseudoAddressSpaceRef interface,	
									FWClientCommandID				commandID,
									IOReturn						status) ;

	// accessors
	static void				SGetFWAddress(
									IOFireWireLibPseudoAddressSpaceRef	interface,
									FWAddress*						outAddr) ;
	static void*			SGetBuffer(
									IOFireWireLibPseudoAddressSpaceRef	interface) ;
	static const UInt32		SGetBufferSize(
									IOFireWireLibPseudoAddressSpaceRef	interface) ;
	static void*			SGetRefCon(
									IOFireWireLibPseudoAddressSpaceRef	interface) ;

	// --- constructor/destructor ----------
							IOFireWirePseudoAddressSpaceImp(
									IOFireWireDeviceInterfaceImp&	inUserClient,
									FWKernAddrSpaceRef				inKernAddrSpaceRef,
									void*							inBuffer,
									UInt32							inBufferSize,
									void*							inBackingStore,
									void*							inRefCon = 0) ;
	virtual					~IOFireWirePseudoAddressSpaceImp() ;
	
    IOReturn				Init() ;

	// --- callback methods ----------------
	static void				Writer(
									IOFireWireLibPseudoAddressSpaceRef refCon,
									IOReturn						result,
									void**							args,
									int								numArgs) ;
	static void				SkippedPacketHandler(
									IOFireWireLibPseudoAddressSpaceRef refCon,
									IOReturn						result,
									FWClientCommandID				commandID,
									UInt32							packetCount) ;
	static void				Reader(
									IOFireWireLibPseudoAddressSpaceRef refCon,
									IOReturn						result,
									void**							args,
									int								numArgs) ;
	
	// NotificationHandler()
	// This routine receives all messages from the kernel user client's 
	// pseudo address space read and write handlers and then calls the 
	// appropriate user callback.
//	static void				NotificationHandler(
//									IOFireWirePseudoAddressSpaceImp* that,
//									io_service_t					service,
//									natural_t						messageType,
//									void*							messageArgument) ;
//	static void				MatchingHandler(
//									void *			refcon,
//									io_iterator_t	iterator ) ;
	
	// --- notification methods ----------
	virtual const IOFireWirePseudoAddressSpaceWriteHandler	
							SetWriteHandler(
									IOFireWirePseudoAddressSpaceWriteHandler	inWriter) ;
	virtual const IOFireWirePseudoAddressSpaceReadHandler	
							SetReadHandler(
									IOFireWirePseudoAddressSpaceReadHandler	inReader) ;
	virtual const IOFireWirePseudoAddressSpaceSkippedPacketHandler
							SetSkippedPacketHandler(
									IOFireWirePseudoAddressSpaceSkippedPacketHandler inHandler) ;
	virtual Boolean			NotificationIsOn() { return mNotifyIsOn; } 
	virtual Boolean			TurnOnNotification(
//									CFRunLoopRef					inRunLoop,
									void*							callBackRefCon ) ;
	virtual void			TurnOffNotification() ;
	virtual void			ClientCommandIsComplete(
									FWClientCommandID				commandID,
									IOReturn						status) ;

	virtual const FWAddress& GetFWAddress() ;
	virtual void*			GetBuffer() ;
	virtual const UInt32	GetBufferSize() ;
	virtual void*			GetRefCon() ;

	const IOFireWirePseudoAddressSpaceReadHandler	
							GetReader() {return mReader;}
	const IOFireWirePseudoAddressSpaceWriteHandler	
							GetWriter() {return mWriter;}
	const IOFireWirePseudoAddressSpaceSkippedPacketHandler
							GetSkippedPacketHandler() {return mSkippedPacketHandler;}
	
 protected:
	// callback mgmt.
	Boolean						mNotifyIsOn ;
	CFRunLoopRef				mNotifyRunLoop ;
	IONotificationPortRef		mNotifyPort ;
	io_object_t					mNotify;

	IOFireWirePseudoAddressSpaceWriteHandler			mWriter ;
	IOFireWirePseudoAddressSpaceReadHandler				mReader ;
	IOFireWirePseudoAddressSpaceSkippedPacketHandler	mSkippedPacketHandler ;

	IOFireWireDeviceInterfaceImp&	mUserClient ;
	FWAddress						mFWAddress ;
	FWKernAddrSpaceRef				mKernAddrSpaceRef ;
	void*							mBuffer ;
	UInt32							mBufferSize ;

	void*							mBackingStore ;
	void*							mRefCon ;
	
	io_async_ref_t					mPacketAsyncRef ;
	io_async_ref_t					mSkippedPacketAsyncRef ;
	io_async_ref_t					mReadPacketAsyncRef ;
	
} ;

#endif //_IOKIT_IOFireWireLibPsudoAddrSpace_H__
