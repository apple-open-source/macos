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

	class VectorCommand : public IOFireWireIUnknown
	{		
		protected:
		
			static IOFireWireLibVectorCommandInterface	sInterface;
			static CFArrayCallBacks						sArrayCallbacks;

			Device &						mUserClient;
			UserObjectHandle				mKernCommandRef;
			void*							mRefCon;
			IOFireWireLibCommandCallback	mCallback;
			CFMutableArrayRef				mCommandArray;
			UInt32							mFlags;
			UInt32							mInflightCount;
			IOReturn						mStatus;
			
			CommandSubmitParams *			mSubmitBuffer;
			vm_size_t						mSubmitBufferSize;

			CommandSubmitResult *			mResultBuffer;
			vm_size_t						mResultBufferSize;
						
		public:
			VectorCommand(	Device &						userClient,
							IOFireWireLibCommandCallback	callback, 
							void *							refCon );
			
			virtual ~VectorCommand();

			static IUnknownVTbl**	Alloc(	Device& 						userclient,
											IOFireWireLibCommandCallback	callback,
											void*							inRefCon );			
	
			virtual HRESULT				QueryInterface( REFIID iid, LPVOID* ppv );	
		
		protected:
			inline VectorCommand *		GetThis( IOFireWireLibVectorCommandRef self )		
					{ return IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis( self ); }

			static IOReturn SSubmit( IOFireWireLibVectorCommandRef self );
			virtual IOReturn Submit();

			static IOReturn SSubmitWithRefconAndCallback( IOFireWireLibVectorCommandRef self, void* refCon, IOFireWireLibCommandCallback inCallback );

			static Boolean SIsExecuting( IOFireWireLibVectorCommandRef self );

			static void SSetCallback( IOFireWireLibVectorCommandRef self, IOFireWireLibCommandCallback inCallback );			

			static void SVectorCompletionHandler(	void*				refcon,
													IOReturn			result,
													void*				quads[],
													UInt32				numQuads );

			virtual void VectorCompletionHandler(	IOReturn			result,
													void*				quads[],
													UInt32				numQuads );
																										
			static void SSetRefCon( IOFireWireLibVectorCommandRef self, void* refCon );

			static void * SGetRefCon( IOFireWireLibVectorCommandRef self );

			static void SSetFlags( IOFireWireLibVectorCommandRef self, UInt32 inFlags );

			static UInt32 SGetFlags( IOFireWireLibVectorCommandRef self );

			static IOReturn SEnsureCapacity( IOFireWireLibVectorCommandRef self, UInt32 capacity );
			virtual IOReturn EnsureCapacity( UInt32 capacity );

			static void SAddCommand( IOFireWireLibVectorCommandRef self, IOFireWireLibCommandRef command );
			
			static void SRemoveCommand( IOFireWireLibVectorCommandRef self, IOFireWireLibCommandRef command );
			
			static void SInsertCommandAtIndex( IOFireWireLibVectorCommandRef self, IOFireWireLibCommandRef command, UInt32 index );

			static IOFireWireLibCommandRef SGetCommandAtIndex( IOFireWireLibVectorCommandRef self, UInt32 index );

			static UInt32 SGetIndexOfCommand( IOFireWireLibVectorCommandRef self, IOFireWireLibCommandRef command );

			static void SRemoveCommandAtIndex( IOFireWireLibVectorCommandRef self, UInt32 index );

			static void SRemoveAllCommands( IOFireWireLibVectorCommandRef self );

			static UInt32 SGetCommandCount( IOFireWireLibVectorCommandRef self );

			static const void * SRetainCallback( CFAllocatorRef allocator, const void * value );
			static void SReleaseCallback( CFAllocatorRef allocator, const void * value );

	};

}