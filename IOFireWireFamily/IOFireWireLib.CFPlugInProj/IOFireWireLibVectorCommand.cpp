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

// public
#import <IOKit/firewire/IOFireWireLib.h>

// private
#include "IOFireWireLibVectorCommand.h"
#include "IOFireWireLibCommand.h"
#import "IOFireWireLibDevice.h"
#import "IOFireWireLibPriv.h"

namespace IOFireWireLib 
{

	IOFireWireLibVectorCommandInterface VectorCommand::sInterface =
	{
		INTERFACEIMP_INTERFACE,
		1, 1, // version/revision

		&VectorCommand::SSubmit,
		&VectorCommand::SSubmitWithRefconAndCallback,
		&VectorCommand::SIsExecuting,
		&VectorCommand::SSetCallback,
		&VectorCommand::SSetRefCon,
		&VectorCommand::SGetRefCon,
		&VectorCommand::SSetFlags,
		&VectorCommand::SGetFlags,
		&VectorCommand::SEnsureCapacity,
		&VectorCommand::SAddCommand,
		&VectorCommand::SRemoveCommand,
		&VectorCommand::SInsertCommandAtIndex,
		&VectorCommand::SGetCommandAtIndex,
		&VectorCommand::SGetIndexOfCommand,
		&VectorCommand::SRemoveCommandAtIndex,
		&VectorCommand::SRemoveAllCommands,
		&VectorCommand::SGetCommandCount
	};

	CFArrayCallBacks VectorCommand::sArrayCallbacks = 
	{
		0,										// version
		&VectorCommand::SRetainCallback,		// retain
		&VectorCommand::SReleaseCallback,		// release
		NULL,									// copyDescription
		NULL,									// equal - NULL means pointer equality will be used 
	};
	
	// Alloc
	//
	//
	
	IUnknownVTbl**	VectorCommand::Alloc(	Device& 						userclient,
											IOFireWireLibCommandCallback	callback,
											void*							inRefCon )
	{
		VectorCommand * me = new VectorCommand( userclient, callback, inRefCon );
		if( !me )
			return nil;

		return reinterpret_cast<IUnknownVTbl**>(&me->GetInterface());
	}
												
	// VectorCommand
	//
	//
	
	VectorCommand::VectorCommand( Device & userClient,
					IOFireWireLibCommandCallback inCallback, void* inRefCon )
	:	IOFireWireIUnknown( reinterpret_cast<const IUnknownVTbl &>( sInterface ) ),
		mUserClient( userClient ),
		mKernCommandRef( 0 ),
		mRefCon( inRefCon ),
		mCallback( inCallback ),
		mFlags( 0 ),
		mInflightCount( 0 ),
		mSubmitBuffer( NULL ),
		mSubmitBufferSize( 0 ),
		mResultBuffer( NULL ),
		mResultBufferSize( 0 )
	{
		mUserClient.AddRef();

		mCommandArray = CFArrayCreateMutable(	kCFAllocatorDefault, 
												0, // unlimited capacity 
												&sArrayCallbacks );
		if( mCommandArray == NULL )
		{
			throw kIOReturnNoMemory;
		}

		// output data
		UserObjectHandle kernel_ref = 0;
		size_t outputStructCnt = sizeof(kernel_ref);

		// send it down
		IOReturn status = IOConnectCallStructMethod(	mUserClient.GetUserClientConnection(),
														kVectorCommandCreate,
														NULL, 0,
														&kernel_ref, &outputStructCnt );
		if( status != kIOReturnSuccess )
		{
			throw status;
		}
		
		mKernCommandRef = kernel_ref;
	}
	
	// ~VectorCommand
	//
	//
	
	VectorCommand::~VectorCommand() 
	{
		if( mKernCommandRef )
		{
			IOReturn result = kIOReturnSuccess;
			
			uint32_t outputCnt = 0;
			const uint64_t inputs[1]={ (const uint64_t)mKernCommandRef };
			result = IOConnectCallScalarMethod(	mUserClient.GetUserClientConnection(),
												kReleaseUserObject,
												inputs, 1,
												NULL, &outputCnt);
			
			DebugLogCond( result, "VectorCommand::~VectorCommand: command release returned 0x%08x\n", result );
		}
		
		// free any current storage
		if( mSubmitBuffer != NULL )
		{
			vm_deallocate( mach_task_self(), (vm_address_t)mSubmitBuffer, mSubmitBufferSize );
			mSubmitBuffer = NULL;
			mSubmitBufferSize = 0;
		}

		if( mResultBuffer != NULL )
		{
			vm_deallocate( mach_task_self(), (vm_address_t)mResultBuffer, mResultBufferSize );
			mResultBuffer = NULL;
			mResultBufferSize = 0;
		}
				
		if( mCommandArray )
		{
			CFRelease( mCommandArray );
			mCommandArray = NULL;
		}
		
		mUserClient.Release();
	}

	// QueryInterface
	//
	//
	
	HRESULT
	VectorCommand::QueryInterface( REFIID iid, LPVOID* ppv )
	{
		HRESULT		result = S_OK;
		*ppv = nil;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes( kCFAllocatorDefault, iid );
	
		if( CFEqual(interfaceID, IUnknownUUID) || CFEqual(interfaceID, kIOFireWireVectorCommandInterfaceID) )
		{
			*ppv = &GetInterface();
			AddRef();
		}
		else
		{
			*ppv = nil;
			result = E_NOINTERFACE;
		}
		
		CFRelease( interfaceID );
		return result;
	}

	// EnsureCapacity
	//
	//
	
	IOReturn VectorCommand::SEnsureCapacity( IOFireWireLibVectorCommandRef self, UInt32 capacity )
	{
		return IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis(self)->EnsureCapacity( capacity );
	}
	
	IOReturn VectorCommand::EnsureCapacity( UInt32 capacity )
	{
		IOReturn status = kIOReturnSuccess;
		
		mach_vm_size_t required_submit_size = capacity * sizeof(CommandSubmitParams);
		mach_vm_size_t required_result_size = capacity * sizeof(CommandSubmitResult);
		
		// do we have enough space?
		if( (mSubmitBufferSize < required_submit_size) ||
			(mResultBufferSize < required_result_size) )
		{
			//
			// allocate new buffers
			//

			// allocate the submit buffer
			vm_address_t submit_buffer = NULL;
			status = vm_allocate( mach_task_self(), &submit_buffer, required_submit_size, true /*anywhere*/ ) ;
			if ( !submit_buffer && (status == kIOReturnSuccess) )
			{
				status = kIOReturnNoMemory;
			}

			// allocate the result buffer
			vm_address_t result_buffer = NULL;
			status = vm_allocate( mach_task_self(), &result_buffer, required_result_size, true /*anywhere*/ ) ;
			if ( !result_buffer && (status == kIOReturnSuccess) )
			{
				status = kIOReturnNoMemory;
			}
			
			//
			// send the buffers to the kernel
			//
			
			if( status == kIOReturnSuccess )
			{
				// send the buffer to the kernel

				// inputs
				const uint64_t inputs[4] = { (const uint64_t)submit_buffer,
											 (const uint64_t)required_submit_size,
											 (const uint64_t)result_buffer,
											 (const uint64_t)required_result_size };

				// outputs
				uint32_t output_count = 0;

				status = IOConnectCallScalarMethod(	mUserClient.GetUserClientConnection(),
													mUserClient.MakeSelectorWithObject( kVectorCommandSetBuffers, mKernCommandRef ),
													inputs, 4,
													NULL, &output_count);
			}
			
			//
			// free any prior storage
			//
			
			if( status == kIOReturnSuccess )
			{
				// this must be done after calling the kernel so the kernel can release it's wiring, else we'd hang
				// we also only do this if the above kernel call was successful as we may hang if the memory is still wired
				
				if( mSubmitBuffer != NULL )
				{
					vm_deallocate( mach_task_self(), (vm_address_t)mSubmitBuffer, mSubmitBufferSize );
					mSubmitBuffer = NULL;
					mSubmitBufferSize = 0;
				}
				
				if( mResultBuffer != NULL )
				{
					vm_deallocate( mach_task_self(), (vm_address_t)mResultBuffer, mResultBufferSize );
					mResultBuffer = NULL;
					mResultBufferSize = 0;
				}
			}
			
			//
			// remember our storage
			//
			
			if( status == kIOReturnSuccess )
			{
				mSubmitBuffer = (CommandSubmitParams*)submit_buffer;
				mSubmitBufferSize = required_submit_size;

				mResultBuffer = (CommandSubmitResult*)result_buffer;
				mResultBufferSize = required_result_size;
			}
		}
		
		return status;
	}

	// Submit
	//
	//
	
	IOReturn
	VectorCommand::SSubmit( IOFireWireLibVectorCommandRef self ) 
	{
		return IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis(self)->Submit();
	}
	
	IOReturn
	VectorCommand::Submit()
	{
		IOReturn status = kIOReturnSuccess;
		
		if( mInflightCount != 0 )
		{ 
			status = kIOReturnNotReady;
		}
		
		CFIndex count = 0;
		if( status == kIOReturnSuccess )
		{
			count = CFArrayGetCount( mCommandArray );
			status = EnsureCapacity( count );
		}

		if( status == kIOReturnSuccess )
		{
			// reset vector status
			mStatus = kIOReturnSuccess;
			
			for( CFIndex index = 0; (status == kIOReturnSuccess) && (index < count); index++ )
			{
				IOFireWireLibCommandRef command = (IOFireWireLibCommandRef)CFArrayGetValueAtIndex( mCommandArray, index );
			
				Cmd * cmd = IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(command);
				
				status = cmd->PrepareForVectorSubmit( &mSubmitBuffer[index] );
			}
		}

		if( status == kIOReturnSuccess )
		{
			// send the requests to the kernel

			// async ref
			uint64_t async_ref[kOSAsyncRef64Count];
			async_ref[kIOAsyncCalloutFuncIndex] = (uint64_t) 0;
			async_ref[kIOAsyncCalloutRefconIndex] = (unsigned long) 0;

			// inputs
			const uint64_t inputs[2] = { (const uint64_t)&SVectorCompletionHandler,
										 (const uint64_t)this };
			// outputs
			uint32_t output_count = 0;

			status = IOConnectCallAsyncScalarMethod( mUserClient.GetUserClientConnection(),
												  mUserClient.MakeSelectorWithObject( kVectorCommandSubmit, mKernCommandRef ),
												  mUserClient.GetAsyncPort(),
												  async_ref, kOSAsyncRef64Count,
												  inputs, 2,
												  NULL, &output_count);

			mInflightCount = count;
			
//			printf( "VectorCommand::Submit - IOConnectCallAsyncStructMethod status = 0x%08lx\n", status );
		}

		if( status == kIOReturnSuccess )
		{
			for( CFIndex index = 0; (status == kIOReturnSuccess) && (index < count); index++ )
			{
				IOFireWireLibCommandRef command = (IOFireWireLibCommandRef)CFArrayGetValueAtIndex( mCommandArray, index );
			
				Cmd * cmd = IOFireWireIUnknown::InterfaceMap<Cmd>::GetThis(command);
				
				cmd->VectorIsExecuting();				
			}			
		}
		
#if 0						
		if( status == kIOReturnSuccess )
		{
			// reset vector status
			mStatus = kIOReturnSuccess;
			
			for( CFIndex index = 0; index < count; index++ )
			{
				IOFireWireLibCommandRef command = (IOFireWireLibCommandRef)CFArrayGetValueAtIndex( mCommandArray, index );
				mInflightCount++;
				(*command)->Submit( command );
				// our commands seem to call the completion handler on error
				// so pretend all was success
			}
		}
#endif
		
//		printf( "VectorCommand::Submit - status = 0x%08lx\n", status );
		
		return status;
	}

	// SubmitWithRefconAndCallback
	//
	//
	
	IOReturn VectorCommand::SSubmitWithRefconAndCallback( IOFireWireLibVectorCommandRef self, void* refCon, IOFireWireLibCommandCallback inCallback )
	{
		(*self)->SetRefCon( self, refCon );
		(*self)->SetCallback( self, inCallback );
		return (*self)->Submit( self );
	}
	
	// IsExecuting
	//
	//
	
	Boolean VectorCommand::SIsExecuting( IOFireWireLibVectorCommandRef self )
	{
		VectorCommand * me = IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis(self);
		return (me->mInflightCount != 0);
	}

	// VectorCompletionHandler
	//
	//
	
	void
	VectorCommand::SVectorCompletionHandler(
		void*				refcon,
		IOReturn			result,
		void*				quads[],
		UInt32				numQuads )
	{
		VectorCommand *	me = (VectorCommand*)refcon;
		me->VectorCompletionHandler( result, quads, numQuads );		
	}

	void
	VectorCommand::VectorCompletionHandler(
		IOReturn			result,
		void*				quads[],
		UInt32				numQuads )
	{
		//printf( "VectorCommand::VectorCompletionHandler - status = 0x%08lx\n", result );

		CFIndex count = CFArrayGetCount( mCommandArray );
			
		for( CFIndex index = 0; (index < count); index++ )
		{
			Cmd * cmd = (Cmd*)mResultBuffer[index].refCon;
			
			IOReturn status = mResultBuffer[index].result;
			
			// pack 'em up like the kernel would
			void * args[3];
			args[0] = (void*)mResultBuffer[index].bytesTransferred;
			args[1] = (void*)mResultBuffer[index].ackCode;
			args[2] = (void*)mResultBuffer[index].responseCode;
		
			// call the completion routine
			cmd->CommandCompletionHandler( cmd, status, args, 3 );
			
			if( mInflightCount > 0 )
			{
				mInflightCount--;
			}
		}
		
		(*mCallback)( mRefCon, mStatus );
	}
		
	// SetCallback
	//
	//
	
	void VectorCommand::SSetCallback( IOFireWireLibVectorCommandRef self, IOFireWireLibCommandCallback inCallback )
	{
		VectorCommand * me = IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis(self);
		me->mCallback = inCallback;
	}
	
	// SetRefCon
	//
	//
	
	void VectorCommand::SSetRefCon( IOFireWireLibVectorCommandRef self, void* refCon )
	{
		VectorCommand * me = IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis(self);
		me->mRefCon = refCon;
	}

	// GetRefCon
	//
	//
	
	void * VectorCommand::SGetRefCon( IOFireWireLibVectorCommandRef self )
	{
		VectorCommand * me = IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis(self);
		return me->mRefCon;
	}
	
	// SetFlags
	//
	//

	void VectorCommand::SSetFlags( IOFireWireLibVectorCommandRef self, UInt32 inFlags )
	{
		VectorCommand * me = IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis(self);
		me->mFlags = inFlags;
	}
	
	// GetFlags
	//
	//
	
	UInt32 VectorCommand::SGetFlags( IOFireWireLibVectorCommandRef self )
	{
		VectorCommand * me = IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis(self);
		return me->mFlags;
	}
		
	// AddCommand
	//
	//
	
	void VectorCommand::SAddCommand( IOFireWireLibVectorCommandRef self, IOFireWireLibCommandRef command )
	{
		VectorCommand * me = IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis(self);
		
		CFArrayAppendValue( me->mCommandArray, command );
	}
		
	// RemoveCommand
	//
	//

	void VectorCommand::SRemoveCommand( IOFireWireLibVectorCommandRef self, IOFireWireLibCommandRef command )
	{
		VectorCommand * me = IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis(self);
		
		CFMutableArrayRef array = me->mCommandArray;
		
		CFRange search_range = CFRangeMake( 0, CFArrayGetCount( array ) );
		CFIndex index = kCFNotFound;
		
		// search for all instances of a given command
		while( (index = CFArrayGetFirstIndexOfValue( array, search_range, command)) != kCFNotFound )
		{
			// remove the index
			CFArrayRemoveValueAtIndex( array, index );
			
			// recalc the search range
			search_range = CFRangeMake( 0, CFArrayGetCount( array ) );
		}
	}

	// InsertCommandAtIndex
	//
	//
	
	void VectorCommand::SInsertCommandAtIndex( IOFireWireLibVectorCommandRef self, IOFireWireLibCommandRef command, UInt32 index )
	{
		VectorCommand * me = IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis(self);
	
		CFArrayInsertValueAtIndex( me->mCommandArray, index, command );
	}
	
	// CommandAtIndex
	//
	//
	
	IOFireWireLibCommandRef VectorCommand::SGetCommandAtIndex( IOFireWireLibVectorCommandRef self, UInt32 index )
	{
		VectorCommand * me = IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis(self);
		
		return (IOFireWireLibCommandRef)CFArrayGetValueAtIndex( me->mCommandArray, index );
	}
	
	// GetIndexOfCommand
	//
	//

	UInt32 VectorCommand::SGetIndexOfCommand( IOFireWireLibVectorCommandRef self, IOFireWireLibCommandRef command )
	{
		VectorCommand * me = IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis(self);
		
		CFRange search_range = CFRangeMake( 0, CFArrayGetCount( me->mCommandArray ) );
		
		return CFArrayGetFirstIndexOfValue( me->mCommandArray, search_range, command );
	}
	
	// RemoveCommandAtIndex
	//
	//

	void VectorCommand::SRemoveCommandAtIndex( IOFireWireLibVectorCommandRef self, UInt32 index )
	{
		VectorCommand * me = IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis(self);
		
		CFArrayRemoveValueAtIndex( me->mCommandArray, index );
	}
	
	// RemoveAllCommands
	//
	//
	
	void VectorCommand::SRemoveAllCommands( IOFireWireLibVectorCommandRef self )
	{
		VectorCommand * me = IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis(self);
		
		CFArrayRemoveAllValues( me->mCommandArray );
	}

	// GetCommandCount
	//
	//
	
	UInt32 VectorCommand::SGetCommandCount( IOFireWireLibVectorCommandRef self )
	{
		VectorCommand * me = IOFireWireIUnknown::InterfaceMap<VectorCommand>::GetThis(self);
		
		return CFArrayGetCount( me->mCommandArray );
	}

	// RetainCallback
	//
	//

	const void * VectorCommand::SRetainCallback( CFAllocatorRef allocator, const void * value )
	{
		IUnknownVTbl** iUnknown = (IUnknownVTbl**)value;
		(*iUnknown)->AddRef( iUnknown );
		
		return value;
	}

	// ReleaseCallback
	//
	//

	void VectorCommand::SReleaseCallback( CFAllocatorRef allocator, const void * value )
	{
		IUnknownVTbl** iUnknown = (IUnknownVTbl**)value;
		(*iUnknown)->Release( iUnknown );
	}

}