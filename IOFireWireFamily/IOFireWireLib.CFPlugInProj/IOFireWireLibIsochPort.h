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
 *  IOFireWireLibIsochPort.h
 *  IOFireWireFamily
 *
 *  Created on Mon Mar 12 2001.
 *  Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
 *
 */

// public
#import <IOKit/firewire/IOFireWireLib.h>
#import <IOKit/firewire/IOFireWireLibIsoch.h>

// private
#import "IOFireWireLibPriv.h"

namespace IOFireWireLib {
	
	class IsochChannel ;
	
	// ============================================================
	//
	// CoalesceTree
	//
	// ============================================================
	
	class CoalesceTree
	{
		struct Node
		{
			Node*				left ;
			Node*				right ;
			IOVirtualRange		range ;
		} ;
	
	public:
							CoalesceTree() ;
							~CoalesceTree() ;
				
		void	 			CoalesceRange(const IOVirtualRange& inRange) ;
		const UInt32	 	GetCount() const ;
		void			 	GetCoalesceList(IOVirtualRange* outRanges) const ;

	protected:
		void				DeleteNode(Node* inNode) ;
		void				CoalesceRange(const IOVirtualRange& inRange, Node* inNode) ;
		const UInt32		GetCount(Node* inNode) const ;
		void				GetCoalesceList(IOVirtualRange* outRanges, Node* inNode, UInt32* pIndex) const ;

	protected:
		Node*	mTop ;
	
	} ;
	
#pragma mark -
	class IsochPort: public IOFireWireIUnknown
	{
		friend class IsochChannel ;
		
		public:
									IsochPort( IUnknownVTbl* interface, Device& userclient, bool talking, bool allocateKernPort ) ;
			virtual					~IsochPort() ;
			
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
	
			void					SetRefCon(
											void*				inRefCon)		{ mRefCon = inRefCon ; }
			void*					GetRefCon() const							{ return mRefCon ; }
			Boolean					GetTalking() const							{ return mTalking ; }
			
		protected:
			KernIsochPortRef		GetKernPortRef()							{ return mKernPortRef; }
	
	protected:
		Device&							mUserClient ;
		KernIsochPortRef				mKernPortRef ;
		void*							mRefCon ;
		Boolean							mTalking ;
		
	
	} ;
	
#pragma mark -
	class IsochPortCOM: public IsochPort
	{
		public:
									IsochPortCOM( IUnknownVTbl* interface, Device& inUserClient, bool talking, bool allocateKernPort = true ) ;
			virtual					~IsochPortCOM() ;		
		
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
	
#pragma mark -
	class RemoteIsochPort: public IsochPortCOM
	{
		protected:
			typedef ::IOFireWireRemoteIsochPortInterface Interface ;
			typedef ::IOFireWireLibRemoteIsochPortRef PortRef ;

		public:
									RemoteIsochPort( IUnknownVTbl* interface, Device& userclient, bool talking ) ;
			virtual					~RemoteIsochPort() {}
			
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
		
			IOFireWireLibIsochPortGetSupportedCallback
									SetGetSupportedHandler(
											IOFireWireLibIsochPortGetSupportedCallback inHandler) ;
			IOFireWireLibIsochPortAllocateCallback
									SetAllocatePortHandler(
											IOFireWireLibIsochPortAllocateCallback	inHandler) ;
			IOFireWireLibIsochPortCallback
									SetReleasePortHandler(
											IOFireWireLibIsochPortCallback	inHandler) ;
			IOFireWireLibIsochPortCallback
									SetStartHandler(
											IOFireWireLibIsochPortCallback	inHandler) ;
			IOFireWireLibIsochPortCallback
									SetStopHandler(
											IOFireWireLibIsochPortCallback	inHandler) ;							
		
		protected:	
			IOFireWireLibIsochPortGetSupportedCallback	mGetSupportedHandler ;
			IOFireWireLibIsochPortAllocateCallback		mAllocatePortHandler ;
			IOFireWireLibIsochPortCallback				mReleasePortHandler ;
			IOFireWireLibIsochPortCallback				mStartHandler ;
			IOFireWireLibIsochPortCallback				mStopHandler ;
			
			IOFireWireLibIsochPortRef					mRefInterface ;	
	} ;
	
#pragma mark -
	class RemoteIsochPortCOM: public RemoteIsochPort
	{
		public:
									RemoteIsochPortCOM( Device& userclient, bool talking ) ;
			virtual					~RemoteIsochPortCOM() ;
		
			static Interface	sInterface ;
		
			// --- IUNKNOWN support ----------------
			static IUnknownVTbl**	Alloc( Device& inUserClient, bool inTalking ) ;
			virtual HRESULT			QueryInterface( REFIID iid, void** ppv ) ;
		
			// --- static methods ------------------
			static IOFireWireLibIsochPortGetSupportedCallback
									SSetGetSupportedHandler(
											PortRef		self,
											IOFireWireLibIsochPortGetSupportedCallback inHandler) ;
			static IOFireWireLibIsochPortAllocateCallback
									SSetAllocatePortHandler(
											PortRef		self,
											IOFireWireLibIsochPortAllocateCallback	inHandler) ;
			static IOFireWireLibIsochPortCallback
									SSetReleasePortHandler(
											PortRef		self,
											IOFireWireLibIsochPortCallback	inHandler) ;
			static IOFireWireLibIsochPortCallback
									SSetStartHandler(
											PortRef		self,
											IOFireWireLibIsochPortCallback	inHandler) ;
			static IOFireWireLibIsochPortCallback
									SSetStopHandler(
											PortRef		self,
											IOFireWireLibIsochPortCallback	inHandler) ;
	} ;
	
	// ============================================================
	//
	// LocalIsochPort
	//
	// ============================================================
	
#pragma mark -
	class LocalIsochPort: public IsochPortCOM
	{
		protected:
			typedef ::IOFireWireLibLocalIsochPortRef	PortRef ;
		
		public:
									LocalIsochPort( IUnknownVTbl* interface, Device& inUserClient, bool inTalking,
														DCLCommand* inDCLProgram, UInt32 inStartEvent, UInt32 inStartState,
														UInt32 inStartMask, IOVirtualRange inDCLProgramRanges[], UInt32 inDCLProgramRangeCount,
														IOVirtualRange inBufferRanges[], UInt32 inBufferRangeCount ) ;
			virtual					~LocalIsochPort() ;
			virtual ULONG			Release() ;

			// port overrides:
			virtual IOReturn		Stop() ;
		
			// local port methods:
			IOReturn				ModifyJumpDCL( DCLJump* inJump, DCLLabelStruct* inLabel ) ;
			IOReturn				ModifyTransferPacketDCLSize( DCLTransferPacket* dcl, IOByteCount newSize ) ;			
			static void				DCLCallProcHandler( void* inRefCon, IOReturn result, LocalIsochPort* me) ;
			void					Lock() ;
			void					Unlock() ;
		
			// utility functions:
			void					PrintDCLProgram( const DCLCommand* inProgram, UInt32 inLength ) ;
		
		protected:
			DCLCommand*				mDCLProgram ;
			UInt32							mStartEvent ;
			UInt32							mStartState ;
			UInt32							mStartMask ;
			UInt32							mExpectedStopTokens ;
			Boolean							mDeferredRelease ;
			
			io_async_ref_t					mAsyncRef ;
		
			pthread_mutex_t					mMutex ;		
	} ;
	
	// ============================================================
	//
	// LocalIsochPortCOM
	//
	// ============================================================
	
#pragma mark -
	class LocalIsochPortCOM: public LocalIsochPort
	{
		typedef ::IOFireWireLocalIsochPortInterface Interface ;
		typedef ::IOFireWireLibLocalIsochPortRef PortRef ;
		
		public:
			LocalIsochPortCOM( Device& userclient, bool inTalking, DCLCommand* inDCLProgram, UInt32 inStartEvent,
					UInt32 inStartState, UInt32 inStartMask, IOVirtualRange inDCLProgramRanges[], 
					UInt32 inDCLProgramRangeCount, IOVirtualRange inBufferRanges[], UInt32 inBufferRangeCount) ;
			virtual ~LocalIsochPortCOM() ;	
			
			// --- IUNKNOWN support ----------------
			static IUnknownVTbl**	Alloc(
											Device&	inUserClient, 
											Boolean							inTalking,
											DCLCommand*				inDCLProgram,
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
											PortRef 	self, 
											DCLJump* 					inJump, 
											DCLLabelStruct* 				inLabel) ;
			static void				SPrintDCLProgram(
											PortRef						 	self, 
											const DCLCommand*			inProgram,
											UInt32							inLength) ;
			static IOReturn			SModifyTransferPacketDCLSize( PortRef self, DCLTransferPacket* dcl, IOByteCount newSize ) ;
			static IOReturn			SModifyTransferPacketDCLBuffer( PortRef self, DCLTransferPacket* dcl, void* newBuffer ) ;
			static IOReturn			SModifyTransferPacketDCL( PortRef self, DCLTransferPacket* dcl, void* newBuffer, IOByteCount newSize ) ;

		protected:
			static Interface	sInterface ;
	} ;
	
}
