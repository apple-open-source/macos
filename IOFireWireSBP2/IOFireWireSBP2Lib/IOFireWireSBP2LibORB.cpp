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

#include <CoreFoundation/CoreFoundation.h>

#include "FWDebugging.h"
#include "IOFireWireSBP2LibORB.h"

__BEGIN_DECLS
#include <IOKit/iokitmig.h>
__END_DECLS

//
// static interface table for IOFireWireSBP2LibORBInterface
//

IOFireWireSBP2LibORBInterface IOFireWireSBP2LibORB::sIOFireWireSBP2LibORBInterface =
{
    0,
	&IOFireWireSBP2LibORB::staticQueryInterface,
	&IOFireWireSBP2LibORB::staticAddRef,
	&IOFireWireSBP2LibORB::staticRelease,
	1, 0, // version/revision
	&IOFireWireSBP2LibORB::staticSetRefCon,
	&IOFireWireSBP2LibORB::staticGetRefCon,
	&IOFireWireSBP2LibORB::staticSetCommandFlags,
	&IOFireWireSBP2LibORB::staticSetMaxORBPayloadSize,
	&IOFireWireSBP2LibORB::staticSetCommandTimeout,
	&IOFireWireSBP2LibORB::staticSetCommandGeneration,
	&IOFireWireSBP2LibORB::staticSetCommandBuffersAsRanges,
	&IOFireWireSBP2LibORB::staticReleaseCommandBuffers,
	&IOFireWireSBP2LibORB::staticSetCommandBlock,
	&IOFireWireSBP2LibORB::staticLSIWorkaroundSetCommandBuffersAsRanges,
	&IOFireWireSBP2LibORB::staticLSIWorkaroundSyncBuffersForOutput,
	&IOFireWireSBP2LibORB::staticLSIWorkaroundSyncBuffersForInput
};

// alloc
//
// static allocator, called by factory method

IUnknownVTbl ** IOFireWireSBP2LibORB::alloc( io_connect_t connection,
											   mach_port_t asyncPort )
{
    IOReturn					status = kIOReturnSuccess;
	IOFireWireSBP2LibORB *		me;
	IUnknownVTbl ** 			interface = NULL;
	
	if( status == kIOReturnSuccess )
	{
		me = new IOFireWireSBP2LibORB();
		if( me == NULL )
			status = kIOReturnError;
	}
		
	if( status == kIOReturnSuccess )
	{
		status = me->init( connection, asyncPort );
	}
	
	if( status != kIOReturnSuccess )
		delete me;

	if( status == kIOReturnSuccess )
	{
		// we return an interface here. queryInterface will not be called. call addRef here
		me->addRef();
		interface = (IUnknownVTbl **) &me->fIOFireWireSBP2LibORBInterface.pseudoVTable;
	}
	
	return interface;
}

// ctor
//
//

IOFireWireSBP2LibORB::IOFireWireSBP2LibORB( void )
{	
	// init cf plugin ref counting
	fRefCount = 0;
	fConnection = 0;
	fORBRef = 0;
	fRefCon = 0;

	// create test driver interface map
	fIOFireWireSBP2LibORBInterface.pseudoVTable 
								= (IUnknownVTbl *) &sIOFireWireSBP2LibORBInterface;
	fIOFireWireSBP2LibORBInterface.obj = this;

}

// init
//
//

IOReturn IOFireWireSBP2LibORB::init( io_connect_t connection, mach_port_t asyncPort )
{
	IOReturn status = kIOReturnSuccess;

	fConnection = connection;
	fAsyncPort = asyncPort;
	
	FWLOG(( "IOFireWireSBP2LibORB : fConnection %d, fAsyncPort %d\n", 
												fConnection, fAsyncPort ));
	
	if( !fConnection || !fAsyncPort )
		status = kIOReturnError;
		
	if( status == kIOReturnSuccess )
	{
		mach_msg_type_number_t len = 1;
		status = io_connect_method_scalarI_scalarO( connection, kIOFWSBP2UserClientCreateORB, 
													NULL, 0, (int*)&fORBRef, &len );
		if( status != kIOReturnSuccess )
			fORBRef = 0; // just to make sure
													
		FWLOG(( "IOFireWireSBP2LibORB :  status = 0x%08x = fORBRef 0x%08lx\n",
																	status, fORBRef ));
	}
			
	return status;
}

// dtor
//
//

IOFireWireSBP2LibORB::~IOFireWireSBP2LibORB()
{
	if( fORBRef ) 
	{
		IOReturn status = kIOReturnSuccess;
		
		mach_msg_type_number_t len = 0;
		status = io_connect_method_scalarI_scalarO( fConnection, 	
													kIOFWSBP2UserClientReleaseORB, 
													(int*)&fORBRef, 1, NULL, &len );
		FWLOG(( "IOFireWireSBP2LibORB : release orb status = 0x%08x\n", status ));
	}
}


//////////////////////////////////////////////////////////////////
// IUnknown methods
//

// queryInterface
//
//

HRESULT IOFireWireSBP2LibORB::staticQueryInterface( void * self, REFIID iid, void **ppv )
{
	return getThis(self)->queryInterface( iid, ppv );
}

HRESULT IOFireWireSBP2LibORB::queryInterface( REFIID iid, void **ppv )
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT result = S_OK;

	if( CFEqual(uuid, IUnknownUUID) ||  CFEqual(uuid, kIOFireWireSBP2LibORBInterfaceID) ) 
	{
        *ppv = &fIOFireWireSBP2LibORBInterface;
        addRef();
    }
    else
        *ppv = 0;

    if( !*ppv )
        result = E_NOINTERFACE;

    CFRelease( uuid );
	
    return result;
}

// addRef
//
//

UInt32 IOFireWireSBP2LibORB::staticAddRef( void * self )
{
	return getThis(self)->addRef();
}

UInt32 IOFireWireSBP2LibORB::addRef()
{
    fRefCount += 1;
    return fRefCount;
}

// Release
//
//

UInt32 IOFireWireSBP2LibORB::staticRelease( void * self )
{
	return getThis(self)->release();
}

UInt32 IOFireWireSBP2LibORB::release( void )
{
	UInt32 retVal = fRefCount;
	
	if( 1 == fRefCount-- ) 
	{
		delete this;
    }
	
	return retVal;
}

//////////////////////////////////////////////////////////////////
// IOFireWireSBP2LibORB methods

// setRefCon
//
//

void IOFireWireSBP2LibORB::staticSetRefCon( void * self, UInt32 refCon )
{
	getThis(self)->setRefCon( refCon );
}

void IOFireWireSBP2LibORB::setRefCon( UInt32 refCon )
{
	FWLOG(( "IOFireWireSBP2LibORB : setRefCon\n"));

	fRefCon = refCon;

	mach_msg_type_number_t len = 0;
	UInt32 params[2];
	
	params[0] = fORBRef;
	params[1] = refCon;
	
	io_connect_method_scalarI_scalarO( fConnection, 	
									   kIOFWSBP2UserClientSetORBRefCon, 
									   (int*)params, 2, NULL, &len );	
}

// getRefCon
//
//

UInt32 IOFireWireSBP2LibORB::staticGetRefCon( void * self )
{
	return getThis(self)->getRefCon();
}

UInt32 IOFireWireSBP2LibORB::getRefCon( void )
{
	return fRefCon;
}

// setCommandFlags
//
//

void IOFireWireSBP2LibORB::staticSetCommandFlags( void * self, UInt32 flags )
{
	getThis(self)->setCommandFlags( flags );
}

void IOFireWireSBP2LibORB::setCommandFlags( UInt32 flags )
{
	FWLOG(( "IOFireWireSBP2LibORB : setCommandFlags = %ld\n", flags ));
		
	mach_msg_type_number_t len = 0;
	UInt32 params[2];
	
	params[0] = fORBRef;
	params[1] = flags;
	
	io_connect_method_scalarI_scalarO( fConnection, 	
									   kIOFWSBP2UserClientSetCommandFlags, 
									   (int*)params, 2, NULL, &len );
}


// setMaxORBPayloadSize
//
//

void IOFireWireSBP2LibORB::staticSetMaxORBPayloadSize( void * self, UInt32 size )
{
	getThis(self)->setMaxORBPayloadSize( size );
}

void IOFireWireSBP2LibORB::setMaxORBPayloadSize( UInt32 size )
{
	FWLOG(( "IOFireWireSBP2LibORB : setMaxORBPayloadSize = %ld\n", size ));
		
	mach_msg_type_number_t len = 0;
	UInt32 params[2];
	
	params[0] = fORBRef;
	params[1] = size;
	
	io_connect_method_scalarI_scalarO( fConnection, 	
									   kIOFWSBP2UserClientSetMaxORBPayloadSize, 
									   (int*)params, 2, NULL, &len );
}

// setCommandTimeout
//
//

void IOFireWireSBP2LibORB::staticSetCommandTimeout( void * self, UInt32 timeout )
{
	getThis(self)->setCommandTimeout( timeout );
}

void IOFireWireSBP2LibORB::setCommandTimeout( UInt32 timeout )
{
	FWLOG(( "IOFireWireSBP2LibORB : setCommandTimeout = %ld\n", timeout ));
		
	mach_msg_type_number_t len = 0;
	UInt32 params[2];
	
	params[0] = fORBRef;
	params[1] = timeout;
	
	io_connect_method_scalarI_scalarO( fConnection, 	
									   kIOFWSBP2UserClientSetCommandTimeout, 
									   (int*)params, 2, NULL, &len );
}

// setCommandGeneration
//
//

void IOFireWireSBP2LibORB::staticSetCommandGeneration( void * self, UInt32 generation )
{
	getThis(self)->setCommandGeneration( generation );
}

void IOFireWireSBP2LibORB::setCommandGeneration( UInt32 generation )
{
	FWLOG(( "IOFireWireSBP2LibORB : setCommandGeneration = %ld\n", generation ));
		
	mach_msg_type_number_t len = 0;
	UInt32 params[2];
	
	params[0] = fORBRef;
	params[1] = generation;
	
	io_connect_method_scalarI_scalarO( fConnection, 	
									   kIOFWSBP2UserClientSetCommandGeneration, 
									   (int*)params, 2, NULL, &len );
}

// setToDummy
//
//

void IOFireWireSBP2LibORB::staticSetToDummy( void * self )
{
	getThis(self)->setToDummy();
}

void IOFireWireSBP2LibORB::setToDummy( void )
{
	IOReturn status = kIOReturnSuccess;
	
	FWLOG(( "IOFireWireSBP2LibORB : setToDummy\n" ));
		
	mach_msg_type_number_t len = 0;
	status = io_connect_method_scalarI_scalarO( fConnection, 	
												kIOFWSBP2UserClientSetToDummy, 
												(int*)&fORBRef, 1, NULL, &len );
}

// setCommandBuffersAsRanges
//
//

IOReturn IOFireWireSBP2LibORB::staticSetCommandBuffersAsRanges( void * self, 
										FWSBP2VirtualRange * ranges, UInt32 withCount,
										UInt32 withDirection, 
										UInt32 offset, UInt32 length )
{
	return getThis(self)->setCommandBuffersAsRanges( ranges, withCount, withDirection, 
																	offset, length);
}

IOReturn IOFireWireSBP2LibORB::setCommandBuffersAsRanges( FWSBP2VirtualRange * ranges, 
										UInt32 withCount, UInt32 withDirection, 
										UInt32 offset, 
										UInt32 length )
{
	IOReturn status = kIOReturnSuccess;

	FWLOG(( "IOFireWireSBP2LibORB : setCommandBuffersAsRanges\n" ));
		
	mach_msg_type_number_t len = 0;
	UInt32 params[6];
	
	params[0] = fORBRef;
	params[1] = (UInt32)ranges;
	params[2] = withCount;
	params[3] = withDirection;
	params[4] = offset;
	params[5] = length;

	status = io_connect_method_scalarI_scalarO( fConnection, 	
									   kIOFWSBP2UserClientSetCommandBuffersAsRanges, 
									   (int*)params, 6, NULL, &len );
	return status;
}

// releaseCommandBuffers
//
//

IOReturn IOFireWireSBP2LibORB::staticReleaseCommandBuffers( void * self )
{
	return getThis(self)->releaseCommandBuffers();
}

IOReturn IOFireWireSBP2LibORB::releaseCommandBuffers( void )
{
	IOReturn status = kIOReturnSuccess;
	
	FWLOG(( "IOFireWireSBP2LibORB : releaseCommandBuffers\n" ));
		
	mach_msg_type_number_t len = 0;
	status = io_connect_method_scalarI_scalarO( fConnection, 	
												kIOFWSBP2UserClientReleaseCommandBuffers, 
												(int*)&fORBRef, 1, NULL, &len );
	return status;
}

// setCommandBlock
//
//

IOReturn IOFireWireSBP2LibORB::staticSetCommandBlock( void * self, void * buffer, 
																	UInt32 length )
{
	return getThis(self)->setCommandBlock( buffer, length );
}

IOReturn IOFireWireSBP2LibORB::setCommandBlock( void * buffer, UInt32 length )
{
	IOReturn status = kIOReturnSuccess;

	FWLOG(( "IOFireWireSBP2LibORB : setCommandBlock\n" ));
		
	mach_msg_type_number_t len = 0;
	UInt32 params[3];
	
	params[0] = fORBRef;
	params[1] = (UInt32)buffer;
	params[2] = length;

	status = io_connect_method_scalarI_scalarO( fConnection, 	
									   kIOFWSBP2UserClientSetCommandBlock, 
									   (int*)params, 3, NULL, &len );
	return status;
}

// LSIWorkaroundSetCommandBuffersAsRanges
//
//

IOReturn IOFireWireSBP2LibORB::staticLSIWorkaroundSetCommandBuffersAsRanges
							( void * self, FWSBP2VirtualRange * ranges, UInt32 withCount,
									UInt32 withDirection, UInt32 offset, UInt32 length )
{
	return getThis( self )->LSIWorkaroundSetCommandBuffersAsRanges( ranges, withCount, withDirection,
																	offset, length );
}

IOReturn IOFireWireSBP2LibORB::LSIWorkaroundSetCommandBuffersAsRanges
								( FWSBP2VirtualRange * ranges, UInt32 withCount,
									UInt32 withDirection, UInt32 offset, UInt32 length )
{
	IOReturn status = kIOReturnSuccess;

	FWLOG(( "IOFireWireSBP2LibORB : LSIWorkaroundSetCommandBuffersAsRanges\n" ));
		
	mach_msg_type_number_t len = 0;
	UInt32 params[6];
	
	params[0] = fORBRef;
	params[1] = (UInt32)ranges;
	params[2] = withCount;
	params[3] = withDirection;
	params[4] = offset;
	params[5] = length;

	status = io_connect_method_scalarI_scalarO( fConnection, 	
									   kIOFWSBP2UserClientLSIWorkaroundSetCommandBuffersAsRanges, 
									   (int*)params, 6, NULL, &len );
	return status;

}

// LSIWorkaroundSyncBuffersForOutput
//
//

IOReturn IOFireWireSBP2LibORB::staticLSIWorkaroundSyncBuffersForOutput( void * self )
{
	return getThis( self )->LSIWorkaroundSyncBuffersForOutput();
}

IOReturn IOFireWireSBP2LibORB::LSIWorkaroundSyncBuffersForOutput( void )
{
	IOReturn status = kIOReturnSuccess;

	FWLOG(( "IOFireWireSBP2LibORB : LSIWorkaroundSyncBuffersForOutput\n" ));
		
	mach_msg_type_number_t len = 0;

	status = io_connect_method_scalarI_scalarO( fConnection, 	
									   kIOFWSBP2UserClientMgmtORBLSIWorkaroundSyncBuffersForOutput, 
									   (int*)&fORBRef, 1, NULL, &len );
	return status;
}

// LSIWorkaroundSyncBuffersForInput
//
//

IOReturn IOFireWireSBP2LibORB::staticLSIWorkaroundSyncBuffersForInput( void * self )
{
	return getThis( self )->LSIWorkaroundSyncBuffersForInput();
}

IOReturn IOFireWireSBP2LibORB::LSIWorkaroundSyncBuffersForInput( void )
{
	IOReturn status = kIOReturnSuccess;

	FWLOG(( "IOFireWireSBP2LibORB : LSIWorkaroundSyncBuffersForInput\n" ));
		
	mach_msg_type_number_t len = 0;

	status = io_connect_method_scalarI_scalarO( fConnection, 	
									   kIOFWSBP2UserClientMgmtORBLSIWorkaroundSyncBuffersForInput, 
									   (int*)&fORBRef, 1, NULL, &len );
	return status;
}

// getORBRef
//
//

UInt32 IOFireWireSBP2LibORB::getORBRef( void )
{
	return fORBRef;
}
