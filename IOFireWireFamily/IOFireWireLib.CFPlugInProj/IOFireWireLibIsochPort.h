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
 *  IOFireWireLibIsochPort.h
 *  IOFireWireFamily
 *
 *  Created by NWG on Mon Mar 12 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef __IOFireWireLibIsochPort_H__
#define __IOFireWireLibIsochPort_H__

#include <IOKit/firewire/IOFireWireLib.h>
#include <IOKit/firewire/IOFireWireLibIsoch.h>
#include "IOFireWireLibPriv.h"

class IOFireWireLibIsochChannelImp ;

// ============================================================
//
// IOFireWireLibCoalesceTree
//
// ============================================================

class IOFireWireLibCoalesceTree
{
	struct Node
	{
		Node*				left ;
		Node*				right ;
		IOVirtualRange		range ;
	} ;

 public:
						IOFireWireLibCoalesceTree() ;
						~IOFireWireLibCoalesceTree() ;
			
	void	 			CoalesceRange(const IOVirtualRange& inRange) ;
	const UInt32	 	GetCount() const ;
	void			 	GetCoalesceList(IOVirtualRange* outRanges) const ;

 protected:
	Node*	mTop ;

	void				DeleteNode(Node* inNode) ;
	void				CoalesceRange(const IOVirtualRange& inRange, Node* inNode) ;
	const UInt32		GetCount(Node* inNode) const ;
	void				GetCoalesceList(IOVirtualRange* outRanges, Node* inNode, UInt32* pIndex) const ;
} ;

class IOFireWireLibIsochPortImp: public IOFireWireIUnknown
{
	friend class IOFireWireLibIsochChannelImp ;
	
 public:
							IOFireWireLibIsochPortImp(
									IOFireWireDeviceInterfaceImp&	inUserClient) ;
	virtual					~IOFireWireLibIsochPortImp() ;
	virtual IOReturn		Init(
									Boolean				inTalking) ;
	
	// --- methods from kernel isoch port ----------
	virtual IOReturn		GetSupported(
									IOFWSpeed& 			maxSpeed, 
									UInt64& 			chanSupported ) ;
	virtual IOReturn		AllocatePort(
									IOFWSpeed 			speed, 
									UInt32 				chan ) ;
	virtual IOReturn		ReleasePort() ;
	virtual IOReturn		Start() ;
	virtual IOReturn		Stop() ;
								
	virtual void			SetRefCon(
									void*				inRefCon) ;
	virtual void*			GetRefCon() const ;
	virtual Boolean			GetTalking() const ;

 protected:
	IOFireWireDeviceInterfaceImp&	mUserClient ;
	FWKernIsochPortRef				mKernPortRef ;
	void*							mRefCon ;
	Boolean							mTalking ;
	
	virtual FWKernIsochPortRef	GetKernPortRef()
										{ return mKernPortRef; }

} ;

class IOFireWireLibIsochPortCOM: public IOFireWireLibIsochPortImp
{
 public:
							IOFireWireLibIsochPortCOM(
									IOFireWireDeviceInterfaceImp&	inUserClient) ;
	virtual					~IOFireWireLibIsochPortCOM() ;

	// --- COM -----------------------------
 	struct InterfaceMap
 	{
 		IUnknownVTbl*				pseudoVTable ;
 		IOFireWireLibIsochPortCOM*	obj ;
 	} ;

 	// --- GetThis() -----------------------
 	inline static IOFireWireLibIsochPortCOM* GetThis(IOFireWireLibIsochPortRef self)
	 	{ return ((InterfaceMap*)self)->obj ;}

	// --- static methods ------------------
	static IOReturn			SGetSupported(
									IOFireWireLibIsochPortRef		self, 
									IOFWSpeed* 						maxSpeed, 
									UInt64* 						chanSupported ) ;
	static IOReturn			SAllocatePort(
									IOFireWireLibIsochPortRef		self, 
									IOFWSpeed 						speed, 
									UInt32 							chan ) ;
	static IOReturn			SReleasePort(
									IOFireWireLibIsochPortRef		self) ;
	static IOReturn			SStart(
									IOFireWireLibIsochPortRef		self) ;
	static IOReturn			SStop(
									IOFireWireLibIsochPortRef		self) ;
	static void				SSetRefCon(
									IOFireWireLibIsochPortRef		self,
									void*				inRefCon) ;
	static void*			SGetRefCon(
									IOFireWireLibIsochPortRef		self) ;
	static Boolean			SGetTalking(
									IOFireWireLibIsochPortRef		self) ;
} ;

class IOFireWireLibRemoteIsochPortImp: public IOFireWireLibIsochPortCOM
{
 public:
							IOFireWireLibRemoteIsochPortImp(
									IOFireWireDeviceInterfaceImp&	inUserClient) ;
	virtual					~IOFireWireLibRemoteIsochPortImp() {}
//	virtual IOReturn		Init() ;
	
	// --- methods from kernel isoch port ----------
	virtual IOReturn		GetSupported(
									IOFWSpeed&			maxSpeed,
									UInt64&				chanSupported) ;
	virtual IOReturn		AllocatePort(
									IOFWSpeed 			speed, 
									UInt32 				chan ) ;
	virtual IOReturn		ReleasePort() ;
	virtual IOReturn		Start() ;
	virtual IOReturn		Stop() ;

	virtual IOFireWireLibIsochPortGetSupportedCallback
							SetGetSupportedHandler(
									IOFireWireLibIsochPortGetSupportedCallback inHandler) ;
	virtual IOFireWireLibIsochPortAllocateCallback
							SetAllocatePortHandler(
									IOFireWireLibIsochPortAllocateCallback	inHandler) ;
	virtual IOFireWireLibIsochPortCallback
							SetReleasePortHandler(
									IOFireWireLibIsochPortCallback	inHandler) ;
	virtual IOFireWireLibIsochPortCallback
							SetStartHandler(
									IOFireWireLibIsochPortCallback	inHandler) ;
	virtual IOFireWireLibIsochPortCallback
							SetStopHandler(
									IOFireWireLibIsochPortCallback	inHandler) ;							

 protected:	
	IOFireWireLibIsochPortGetSupportedCallback	mGetSupportedHandler ;
	IOFireWireLibIsochPortAllocateCallback		mAllocatePortHandler ;
	IOFireWireLibIsochPortCallback				mReleasePortHandler ;
	IOFireWireLibIsochPortCallback				mStartHandler ;
	IOFireWireLibIsochPortCallback				mStopHandler ;
	
	IOFireWireLibIsochPortRef					mRefInterface ;
	
//	virtual void SetRefInterface(IOFireWireLibIsochPortRef inInterface)
//				{ mRefInterface = inInterface; }
} ;

class IOFireWireLibRemoteIsochPortCOM: public IOFireWireLibRemoteIsochPortImp
{
 public:
							IOFireWireLibRemoteIsochPortCOM(
									IOFireWireDeviceInterfaceImp&	inUserClient) ;
	virtual					~IOFireWireLibRemoteIsochPortCOM() ;

	// --- COM -----------------------------
 	struct InterfaceMap
 	{
 		IUnknownVTbl*						pseudoVTable ;
 		IOFireWireLibRemoteIsochPortCOM*	obj ;
 	} ;
 
	static IOFireWireRemoteIsochPortInterface	sInterface ;
 	InterfaceMap								mInterface ;

 	// --- GetThis() -----------------------
 	inline static IOFireWireLibRemoteIsochPortCOM* GetThis(IOFireWireLibRemoteIsochPortRef self)
	 	{ return ((InterfaceMap*)self)->obj ;}

	// --- IUNKNOWN support ----------------
	static IUnknownVTbl**	Alloc(
									IOFireWireDeviceInterfaceImp&	inUserClient,
									Boolean							inTalking) ;
	virtual HRESULT			QueryInterface(
									REFIID 		iid, 
									void** 		ppv ) ;

	// --- static methods ------------------
	static IOFireWireLibIsochPortGetSupportedCallback
							SSetGetSupportedHandler(
									IOFireWireLibRemoteIsochPortRef		self,
									IOFireWireLibIsochPortGetSupportedCallback inHandler) ;
	static IOFireWireLibIsochPortAllocateCallback
							SSetAllocatePortHandler(
									IOFireWireLibRemoteIsochPortRef		self,
									IOFireWireLibIsochPortAllocateCallback	inHandler) ;
	static IOFireWireLibIsochPortCallback
							SSetReleasePortHandler(
									IOFireWireLibRemoteIsochPortRef		self,
									IOFireWireLibIsochPortCallback	inHandler) ;
	static IOFireWireLibIsochPortCallback
							SSetStartHandler(
									IOFireWireLibRemoteIsochPortRef		self,
									IOFireWireLibIsochPortCallback	inHandler) ;
	static IOFireWireLibIsochPortCallback
							SSetStopHandler(
									IOFireWireLibRemoteIsochPortRef		self,
									IOFireWireLibIsochPortCallback	inHandler) ;

} ;

// ============================================================
//
// IOFireWireLibLocalIsochPortImp
//
// ============================================================

class IOFireWireLibLocalIsochPortImp: public IOFireWireLibIsochPortCOM
{
//	friend class IOFireWireLibIsochChannelImp ;
	
 public:
							IOFireWireLibLocalIsochPortImp(
									IOFireWireDeviceInterfaceImp&	inUserClient) ;
	virtual					~IOFireWireLibLocalIsochPortImp() ;
	virtual IOReturn		Init(
									Boolean				inTalking,
									DCLCommandStruct*	inDCLProgram,
									UInt32				inStartEvent,
									UInt32				inStartState,
									UInt32				inStartMask,
									IOVirtualRange		inDCLProgramRanges[],			// optional optimization parameters
									UInt32				inDCLProgramRangeCount,
									IOVirtualRange		inBufferRanges[],
									UInt32				inBufferRangeCount) ;

	// --- local port methods ------------
	virtual IOReturn		ModifyJumpDCL(
									DCLJumpStruct* 						inJump, 
									DCLLabelStruct* 					inLabel) ;
	static void				DCLCallProcHandler(
									void*				inRefCon,
									IOReturn			result) ;

	// --- utility functions -------------
	virtual void			PrintDCLProgram(
									const DCLCommandStruct*	inProgram,
									UInt32					inLength) ;

 protected:
	DCLCommandStruct*				mDCLProgram ;
	UInt32							mStartEvent ;
	UInt32							mStartState ;
	UInt32							mStartMask ;

	io_async_ref_t					mAsyncRef ;
} ;

// ============================================================
//
// IOFireWireLibLocalIsochPortCOM
//
// ============================================================

class IOFireWireLibLocalIsochPortCOM: public IOFireWireLibLocalIsochPortImp
{
 public:
							IOFireWireLibLocalIsochPortCOM(
									IOFireWireDeviceInterfaceImp&	inUserClient) ;
	virtual					~IOFireWireLibLocalIsochPortCOM() ;

	// --- COM -----------------------------
 	struct InterfaceMap
 	{
 		IUnknownVTbl*				pseudoVTable ;
 		IOFireWireLibLocalIsochPortCOM*	obj ;
 	} ;
 
	static IOFireWireLocalIsochPortInterface	sInterface ;
 	InterfaceMap								mInterface ;

	// --- GetThis() -----------------------
	inline static IOFireWireLibLocalIsochPortCOM* GetThis(IOFireWireLibLocalIsochPortRef self)
		{ return ((InterfaceMap*)self)->obj ;}

	// --- IUNKNOWN support ----------------
	static IUnknownVTbl**	Alloc(
									IOFireWireDeviceInterfaceImp&	inUserClient, 
									Boolean							inTalking,
									DCLCommandStruct*				inDCLProgram,
									UInt32							inStartEvent,
									UInt32							inStartState,
									UInt32							inStartMask,
									IOVirtualRange					inDCLProgramRanges[],			// optional optimization parameters
									UInt32							inDCLProgramRangeCount,
									IOVirtualRange					inBufferRanges[],
									UInt32							inBufferRangeCount) ;
	virtual HRESULT			QueryInterface(
									REFIID 		iid, 
									void** 		ppv ) ;

	// --- static methods ------------------
	static IOReturn			SModifyJumpDCL(
									IOFireWireLibLocalIsochPortRef 	self, 
									DCLJumpStruct* 					inJump, 
									DCLLabelStruct* 				inLabel) ;

	// --- utility functions ----------
	static void				SPrintDCLProgram(
									IOFireWireLibLocalIsochPortRef 	self, 
									const DCLCommandStruct*			inProgram,
									UInt32							inLength) ;
									
} ;

#endif //__IOFireWireLibIsochPort_H__
