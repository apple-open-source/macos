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
 *  IOFireWireLibIsochChannel.h
 *  IOFireWireFamily
 *
 *  Created by NWG on Mon Mar 12 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef __IOFireWireLibIsochChannel_H__
#define __IOFireWireLibIsochChannel_H__

#include "IOFireWireLibPriv.h"
#include "IOFireWireLibIsochPort.h"
#include <IOKit/firewire/IOFireWireLibIsoch.h>

class IOFireWireLibIsochChannelImp: public IOFireWireIUnknown
{
 public:
	// --- ctor/dtor
							IOFireWireLibIsochChannelImp(
									IOFireWireDeviceInterfaceImp&	inUserClient) ;
							~IOFireWireLibIsochChannelImp() ;
	virtual IOReturn		Init(
									Boolean							inDoIRM,
									IOByteCount						inPacketSize,
									IOFWSpeed						inPrefSpeed) ;

	static void				ForceStopHandler(
									IOFireWireLibPseudoAddressSpaceRef refCon,
									IOReturn						result,
									void**							args,
									int								numArgs) ;

	// --- other methods
	virtual IOReturn 		SetTalker(
									IOFireWireLibIsochPortRef	 		talker ) ;
	virtual IOReturn		AddListener(
									IOFireWireLibIsochPortRef	 		listener ) ;
	virtual IOReturn		AllocateChannel() ;
	virtual IOReturn 		ReleaseChannel() ;
	virtual IOReturn		Start() ;
	virtual IOReturn		Stop() ;

	virtual IOFireWireIsochChannelForceStopHandler
							SetChannelForceStopHandler(
									IOFireWireIsochChannelForceStopHandler stopProc) ;
	virtual void	 		SetRefCon(
									void* 							stopProcRefCon) ;
	virtual void*			GetRefCon() ;
	virtual Boolean			NotificationIsOn() ;
	virtual Boolean			TurnOnNotification() ;
	virtual void			TurnOffNotification() ;
	virtual void			ClientCommandIsComplete(
									FWClientCommandID 				commandID, 
									IOReturn 						status) ;

 protected:
	IOFireWireDeviceInterfaceImp&			mUserClient ;
	FWKernIsochChannelRef					mKernChannelRef ;
	Boolean									mNotifyIsOn ;
	IOFireWireIsochChannelForceStopHandler	mForceStopHandler ;
	void*									mUserRefCon ;
	
	io_async_ref_t							mAsyncRef ;

	IOFireWireLibIsochPortImp*				mTalker ;
	CFMutableArrayRef						mListeners ;
	IOFireWireLibIsochChannelRef			mRefInterface ;
	
	IOFWSpeed								mSpeed ;
	IOFWSpeed								mPrefSpeed ;
	UInt32									mChannel ;
} ;

class IOFireWireLibIsochChannelCOM: public IOFireWireLibIsochChannelImp
{
 public:
							IOFireWireLibIsochChannelCOM(
									IOFireWireDeviceInterfaceImp&	inUserClient) ;
	virtual 				~IOFireWireLibIsochChannelCOM() ;

	// --- COM ---------------
 	struct InterfaceMap
 	{
 		IUnknownVTbl*					pseudoVTable ;
 		IOFireWireLibIsochChannelCOM*	obj ;
 	} ;
 
	static IOFireWireIsochChannelInterface	sInterface ;
 	InterfaceMap							mInterface ;

 	// GetThis()
 	inline static IOFireWireLibIsochChannelCOM* GetThis(IOFireWireLibIsochChannelRef self)
	 	{ return ((InterfaceMap*)self)->obj ;}

	// --- IUNKNOWN support ----------------
	
	static IUnknownVTbl**	Alloc(
									IOFireWireDeviceInterfaceImp&	inUserClient,
									Boolean							inDoIRM,
									IOByteCount						inPacketSize,
									IOFWSpeed						inPrefSpeed) ;
	virtual HRESULT			QueryInterface(REFIID iid, void ** ppv ) ;

	// --- static methods ------------------
	static IOReturn			SSetTalker(
									IOFireWireLibIsochChannelRef 	self, 
									IOFireWireLibIsochPortRef 		talker) ;
	static IOReturn 		SAddListener(
									IOFireWireLibIsochChannelRef 	self, 
									IOFireWireLibIsochPortRef 		listener) ;
	static IOReturn 		SAllocateChannel(
									IOFireWireLibIsochChannelRef 	self) ;
	static IOReturn			SReleaseChannel(
									IOFireWireLibIsochChannelRef 	self) ;
	static IOReturn 		SStart(
									IOFireWireLibIsochChannelRef 	self) ;
	static IOReturn			SStop(
									IOFireWireLibIsochChannelRef 	self) ;
	static IOFireWireIsochChannelForceStopHandler
							SSetChannelForceStopHandler(
									IOFireWireLibIsochChannelRef 	self, 
									IOFireWireIsochChannelForceStopHandler stopProc) ;
	static void		 		SSetRefCon(
									IOFireWireLibIsochChannelRef 	self, 
									void* 							stopProcRefCon) ;
	static void*			SGetRefCon(
									IOFireWireLibIsochChannelRef 	self) ;
	static Boolean			SNotificationIsOn(
									IOFireWireLibIsochChannelRef 	self) ;
	static Boolean			STurnOnNotification(
									IOFireWireLibIsochChannelRef 	self) ;
	static void				STurnOffNotification(
									IOFireWireLibIsochChannelRef	self) ;	
	static void				SClientCommandIsComplete(
									IOFireWireLibIsochChannelRef 	self, 
									FWClientCommandID 				commandID, 
									IOReturn 						status) ;
} ;

#endif __IOFireWireLibIsochChannel_H__
