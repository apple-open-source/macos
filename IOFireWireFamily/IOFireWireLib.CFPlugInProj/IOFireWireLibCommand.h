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
 *  IOFireWireLibCommand.h
 *  IOFireWireLib
 *
 *  Created by NWG on Tue Dec 12 2000.
 *  Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef _IOKIT_IOFireWireLibCommand_H_
#define _IOKIT_IOFireWireLibCommand_H_

#include <CoreFoundation/CoreFoundation.h>

#include "IOFireWireLib.h"
#include "IOFireWireLibPriv.h"


class IOFireWireLibCommandImp: public IOFireWireIUnknown
{
	// ==================================
	// COM members
	// ==================================
	
	struct InterfaceMap 
	{
		IUnknownVTbl*				pseudoVTable ;
		IOFireWireLibCommandImp*	obj ;
	} ;

	static IOFireWireCommandInterface	sInterface ;
	InterfaceMap						mInterface ;

	virtual HRESULT QueryInterface(REFIID iid, LPVOID* ppv) ;
 
	// GetThis()
	inline static IOFireWireLibCommandImp* GetThis(IOFireWireLibCommandRef	self)
		{ return ((InterfaceMap*)self)->obj; }

 public:
	// --- getters -----------------------
	static IOReturn			SGetStatus(
								IOFireWireLibCommandRef	self) ;
	static UInt32			SGetTransferredBytes(
								IOFireWireLibCommandRef	self) ;
	static void				SGetTargetAddress(
								IOFireWireLibCommandRef	self,
								FWAddress*				outAddr) ;
	// --- setters -----------------------
	static void				SSetTarget(
									IOFireWireLibCommandRef	self,
									const FWAddress*	addr) ;
	static void				SSetGeneration(
									IOFireWireLibCommandRef	self,
									UInt32					generation) ;
	static void				SSetCallback(
									IOFireWireLibCommandRef	self,
									IOFireWireLibCommandCallback	inCallback) ;
	static void				SSetRefCon(
									IOFireWireLibCommandRef	self,
									void*					refCon) ;

	static const Boolean	SIsExecuting(
									IOFireWireLibCommandRef	self) ;
	static IOReturn			SSubmit(
									IOFireWireLibCommandRef	self) ;
	static IOReturn			SSubmitWithRefconAndCallback(
									IOFireWireLibCommandRef	self,
									void*	refCon,
									IOFireWireLibCommandCallback	inCallback) ;
	static IOReturn			SCancel(
									IOFireWireLibCommandRef self,
									IOReturn				reason) ;
	static void				SSetBuffer(
									IOFireWireLibCommandRef self,
									UInt32					size,
									void*					buf) ;
	static void				SGetBuffer(
									IOFireWireLibCommandRef	self,
									UInt32*					outSize,
									void**					outBuf) ;
	static IOReturn			SSetMaxPacket(
									IOFireWireLibCommandRef self,
									IOByteCount				inMaxBytes) ;
	static void				SSetFlags(
									IOFireWireLibCommandRef	self,
									UInt32					inFlags) ;

	// ==================================
	// virtual members
	// ==================================

	// --- ctor/dtor ---------------------
							IOFireWireLibCommandImp(
									IOFireWireDeviceInterfaceImp&	userClient,
									io_object_t						inDevice) ;
	virtual					~IOFireWireLibCommandImp() ;
	
	virtual Boolean			Init(	
									const FWAddress&				inAddr,
									IOFireWireLibCommandCallback	inCallback,
									const Boolean					inFailOnReset,
									const UInt32					inGeneration,
									void*							inRefCon) ;
	
	// --- getters -----------------------
	virtual const IOReturn	GetCompletionStatus() const ;
	virtual const UInt32	GetTransferredBytes() const ;
	virtual const FWAddress& GetTargetAddress() const ;

	// --- setters -----------------------
	virtual void			SetTarget(
									const FWAddress&	addr) ;
	virtual void			SetGeneration(
									UInt32				generation) ;
	virtual void			SetCallback(
									IOFireWireLibCommandCallback inCallback) ;
	virtual void			SetRefCon(
									void*				refCon) ;
	virtual const Boolean	IsExecuting() const ;
	virtual IOReturn		Submit() = 0 ;
	virtual IOReturn		Submit(
									FWUserCommandSubmitParams*	params,
									mach_msg_type_number_t		paramsSize,
									FWUserCommandSubmitResult*	ioResult,
									mach_msg_type_number_t*		ioResultSize ) ;
	virtual IOReturn		SubmitWithRefconAndCallback(
									void*				refCon,
									IOFireWireLibCommandCallback inCallback) ;
	virtual IOReturn		Cancel(
									IOReturn			reason) ;
	virtual void			SetBuffer(
									UInt32				size,
									void*				buf) ;
	virtual void			GetBuffer(
									UInt32*				outSize,
									void**				outBuf) ;
	virtual IOReturn		SetMaxPacket(
									IOByteCount				inMaxBytes) ;
	virtual void			SetFlags(
									UInt32					inFlags) ;
	static void				CommandCompletionHandler(
									void*				refcon,
									IOReturn			result,
									IOByteCount			bytesTransferred) ;									
 protected:
	IOFireWireDeviceInterfaceImp&	mUserClient ;
	io_object_t						mDevice ;
	io_async_ref_t					mAsyncRef ;
	IOByteCount						mBytesTransferred ;	
 	Boolean							mIsExecuting ;
	IOReturn						mStatus ;
	void*							mRefCon ;
	IOFireWireLibCommandCallback	mCallback ;
	
	FWUserCommandSubmitParams* 		mParams ;
} ;

class IOFireWireLibReadCommandImp: public IOFireWireLibCommandImp
{
	struct InterfaceMap
	{
		IUnknownVTbl*					pseudoVTable ;
		IOFireWireLibReadCommandImp*	obj ;
	} ;

	static IOFireWireReadCommandInterface 	sInterface ;
	InterfaceMap							mInterface ;

	virtual HRESULT QueryInterface(REFIID iid, LPVOID* ppv) ;

	// GetThis()
	inline static IOFireWireLibReadCommandImp* GetThis(IOFireWireLibReadCommandRef	self)
		{ return ((InterfaceMap*)self)->obj; }

 public:
	virtual Boolean			Init(	
									const FWAddress&				inAddr,
									void*							buf,
									UInt32							size,
									IOFireWireLibCommandCallback	inCallback,
									const Boolean					inFailOnReset,
									const UInt32					inGeneration,
									void*							inRefCon) ;
	static IUnknownVTbl**	Alloc(
									IOFireWireDeviceInterfaceImp& inUserClient,
									io_object_t			device,
									const FWAddress&	addr,
									void*				buf,
									UInt32				size,
									IOFireWireLibCommandCallback callback,
									Boolean				failOnReset,
									UInt32				generation,
									void*				inRefCon) ;

	// --- ctor/dtor ----------------
							IOFireWireLibReadCommandImp(
									IOFireWireDeviceInterfaceImp& inUserClient,
									io_object_t			device,
									const FWAddress&	addr,
									void*				buf,
									UInt32				size,
									IOFireWireLibCommandCallback callback,
									Boolean				failOnReset,
									UInt32				generation,
									void*				inRefCon) ;
	virtual					~IOFireWireLibReadCommandImp() {}

	// required Submit() method
	virtual IOReturn		Submit() ;

 protected:
//	void*			mBuffer ;
//	IOByteCount		mSize ;
} ;

class IOFireWireLibReadQuadletCommandImp: public IOFireWireLibCommandImp
{
	struct InterfaceMap
	{
		IUnknownVTbl*					pseudoVTable ;
		IOFireWireLibReadQuadletCommandImp*	obj ;
	} ;

	static IOFireWireReadQuadletCommandInterface	sInterface ;
	InterfaceMap									mInterface ;

	virtual HRESULT QueryInterface(REFIID iid, LPVOID* ppv) ;

	// GetThis()
	inline static IOFireWireLibReadQuadletCommandImp* GetThis(IOFireWireLibReadQuadletCommandRef self)
		{ return ((InterfaceMap*)self)->obj; }

 public:
	virtual Boolean			Init(	
									const FWAddress &	addr,
									UInt32				quads[],
									UInt32				numQuads,
									IOFireWireLibCommandCallback callback,
									Boolean				failOnReset,
									UInt32				generation,
									void*				inRefCon) ;
	static IUnknownVTbl**	Alloc(
									IOFireWireDeviceInterfaceImp& inUserClient,
									io_object_t			device,
									const FWAddress &	addr,
									UInt32				quads[],
									UInt32				numQuads,
									IOFireWireLibCommandCallback callback,
									Boolean				failOnReset,
									UInt32				generation,
									void*				inRefCon) ;

	static void				SSetQuads(
									IOFireWireLibReadQuadletCommandRef self,
									UInt32				inQuads[],
									UInt32				inNumQuads) ;
	// --- ctor/dtor ----------------

							IOFireWireLibReadQuadletCommandImp(
									IOFireWireDeviceInterfaceImp& inUserClient,
									io_object_t			device) ;
	virtual					~IOFireWireLibReadQuadletCommandImp() {}
							
	virtual void			SetQuads(
									UInt32				quads[],
									UInt32				numQuads) ;
	// required Submit() method
	virtual IOReturn		Submit() ;

	static void				CommandCompletionHandler(
									void*				refcon,
									IOReturn			result,
									void*				quads[],
									UInt32				numQuads) ;

 protected:
//	UInt32	mQuads[] ;
	UInt32	mNumQuads ;
} ;

class IOFireWireLibWriteCommandImp: public IOFireWireLibCommandImp
{
	struct InterfaceMap
	{
		IUnknownVTbl*					pseudoVTable ;
		IOFireWireLibWriteCommandImp*	obj ;
	} ;

	virtual HRESULT QueryInterface(REFIID iid, LPVOID* ppv) ;

	static IOFireWireWriteCommandInterface		sInterface ;
	InterfaceMap								mInterface ;

	// GetThis()
	inline static IOFireWireLibWriteCommandImp* GetThis(IOFireWireLibWriteCommandRef self)
		{ return ((InterfaceMap*)self)->obj; }

 public:
	virtual Boolean			Init(	
									const FWAddress&	inAddr,
									 void*				buf,
									UInt32				size,
									IOFireWireLibCommandCallback	inCallback,
									const Boolean		inFailOnReset,
									const UInt32		inGeneration,
									void*				inRefCon) ;
	static IUnknownVTbl**	Alloc(
									IOFireWireDeviceInterfaceImp& inUserClient,
									io_object_t			device,
									const FWAddress &	addr,
									 void*				buf,
									UInt32				size,
									IOFireWireLibCommandCallback callback,
									Boolean				failOnReset,
									UInt32				generation,
									void*				inRefCon) ;
	// --- ctor/dtor ----------------

							IOFireWireLibWriteCommandImp(
									IOFireWireDeviceInterfaceImp& inUserClient,
									io_object_t			device) ;
	virtual					~IOFireWireLibWriteCommandImp() {}
							
	// required Submit() method
	virtual IOReturn		Submit() ;

 protected:
//	void*		mBuffer ;
//	IOByteCount		mSize ;
	
} ;

class IOFireWireLibWriteQuadletCommandImp: public IOFireWireLibCommandImp
{
 protected:
	struct InterfaceMap
	{
		IUnknownVTbl*							pseudoVTable ;
		IOFireWireLibWriteQuadletCommandImp*	obj ;
	} ;

	static IOFireWireWriteQuadletCommandInterface	sInterface ;
	InterfaceMap									mInterface ;

	virtual HRESULT QueryInterface(REFIID iid, LPVOID* ppv) ;

	// GetThis()
	inline static IOFireWireLibWriteQuadletCommandImp* GetThis(IOFireWireLibWriteQuadletCommandRef self)
		{ return ((InterfaceMap*)self)->obj; }

 public:
	virtual Boolean			Init(	
									const FWAddress&				inAddr,
									UInt32							quads[],
									UInt32							numQuads,
									IOFireWireLibCommandCallback	inCallback,
									const Boolean					inFailOnReset,
									const UInt32					inGeneration,
									void*							inRefCon) ;
 	static IUnknownVTbl**	Alloc(
									IOFireWireDeviceInterfaceImp& inUserClient,									
									io_object_t			device,
									const FWAddress &	addr,
									UInt32				quads[],
									UInt32				numQuads,
									IOFireWireLibCommandCallback callback,
									Boolean				failOnReset,
									UInt32				generation,
									void*				inRefCon) ;

	static void				SSetQuads(
									IOFireWireLibWriteQuadletCommandRef self,
									UInt32				inQuads[],
									UInt32				inNumQuads) ;
	// --- ctor/dtor ----------------

							IOFireWireLibWriteQuadletCommandImp(
									IOFireWireDeviceInterfaceImp& inUserClient,									
									io_object_t			device) ;
	virtual					~IOFireWireLibWriteQuadletCommandImp() ;
							
	virtual void			SetQuads(
									UInt32				inQuads[],
									UInt32				inNumQuads) ;
	virtual IOReturn 		Submit() ;

 protected:
	UInt8*	mParamsExtra ;
} ;

class IOFireWireLibCompareSwapCommandImp: public IOFireWireLibCommandImp
{
	struct InterfaceMap
	{
		IUnknownVTbl*						pseudoVTable ;
		IOFireWireLibCompareSwapCommandImp*	obj ;
	} ;

	static IOFireWireCompareSwapCommandInterface	sInterface ;
	InterfaceMap									mInterface ;

	virtual HRESULT QueryInterface(REFIID iid, LPVOID* ppv) ;

	// GetThis()
	inline static IOFireWireLibCompareSwapCommandImp* GetThis(IOFireWireLibCompareSwapCommandRef self)
		{ return ((InterfaceMap*)self)->obj; }

 public:
	virtual Boolean			Init(	
									const FWAddress &	addr,
									UInt32				cmpVal,
									UInt32				newVal,
									IOFireWireLibCommandCallback callback,
									Boolean				failOnReset,
									UInt32				generation,
									void*				inRefCon) ;
	static IUnknownVTbl**	Alloc(
									IOFireWireDeviceInterfaceImp& inUserClient,
									io_object_t			device,
									const FWAddress &	addr,
									UInt32				cmpVal,
									UInt32				newVal,
									IOFireWireLibCommandCallback callback,
									Boolean				failOnReset,
									UInt32				generation,
									void*				inRefCon) ;

	static void				SSetValues(
									IOFireWireLibCompareSwapCommandRef self,
									UInt32				cmpVal,
									UInt32				newVal) ;
									
	// --- ctor/dtor ----------------
							IOFireWireLibCompareSwapCommandImp(
									IOFireWireDeviceInterfaceImp& inUserClient,
									io_object_t			device) ;
	virtual					~IOFireWireLibCompareSwapCommandImp() ;
							
	virtual void			SetValues(
									UInt32				cmpVal,
									UInt32				newVal) ;
	virtual IOReturn 		Submit() ;

 protected:
	UInt8*		mParamsExtra ;
} ;

#endif //_IOKIT_IOFireWireLibCommand_H_

